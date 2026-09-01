#include "core/player.hpp"

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace {
bool contains(const std::string& text, const char* value) {
    return text.find(value) != std::string::npos;
}

double numberAfter(const std::string& text, const char* key, double fallback = 0) {
    const auto position = text.find(key);
    if (position == std::string::npos) return fallback;
    try {
        return std::stod(text.substr(position + std::strlen(key)));
    } catch (...) {
        return fallback;
    }
}
}  // namespace

std::string jsonString(const std::string& value) {
    std::string result = "\"";
    for (unsigned char character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (character < 0x20) {
                    static constexpr char hex[] = "0123456789abcdef";
                    result += "\\u00";
                    result += hex[character >> 4];
                    result += hex[character & 15];
                } else {
                    result += static_cast<char>(character);
                }
        }
    }
    return result + '"';
}

MpvPlayer::~MpvPlayer() { stop(); }

void MpvPlayer::setError(std::string message) {
    std::lock_guard lock(errorMutex_);
    error_ = std::move(message);
}

std::string MpvPlayer::error() const {
    std::lock_guard lock(errorMutex_);
    return error_;
}

bool MpvPlayer::start() {
    if (healthy_) return true;
    stop();
    socketPath_ = "/tmp/pocket-music-" + std::to_string(getpid()) + ".sock";
    unlink(socketPath_.c_str());
    process_ = fork();
    if (process_ == 0) {
        const std::string ipc = "--input-ipc-server=" + socketPath_;
        execlp("mpv", "mpv", "--idle=yes", "--no-video", "--no-config", "--input-terminal=no",
               "--really-quiet", ipc.c_str(), nullptr);
        _exit(127);
    }
    if (process_ < 0) {
        setError("Could not start mpv");
        return false;
    }
    for (int attempt = 0; attempt < 75; ++attempt) {
        socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, socketPath_.c_str(), sizeof(address.sun_path) - 1);
        if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
#ifdef SO_NOSIGPIPE
            int enabled = 1;
            setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
            healthy_ = true;
            reader_ = std::thread(&MpvPlayer::readEvents, this);
            sendCommand("{\"command\":[\"observe_property\",1,\"time-pos\"]}");
            sendCommand("{\"command\":[\"observe_property\",2,\"duration\"]}");
            sendCommand("{\"command\":[\"observe_property\",3,\"pause\"]}");
            return true;
        }
        close(socket_);
        socket_ = -1;
        int status = 0;
        if (waitpid(process_, &status, WNOHANG) == process_) {
            process_ = -1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    setError("mpv IPC did not become ready");
    stop();
    return false;
}

bool MpvPlayer::sendCommand(const std::string& json) {
    std::lock_guard lock(writeMutex_);
    if (socket_ < 0 || !healthy_) return false;
    const std::string payload = json + '\n';
    size_t sent = 0;
    while (sent < payload.size()) {
#ifdef MSG_NOSIGNAL
        const auto count =
            send(socket_, payload.data() + sent, payload.size() - sent, MSG_NOSIGNAL);
#else
        const auto count = send(socket_, payload.data() + sent, payload.size() - sent, 0);
#endif
        if (count > 0) {
            sent += static_cast<size_t>(count);
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        healthy_ = false;
        setError("Lost connection to mpv");
        return false;
    }
    return true;
}

bool MpvPlayer::load(const std::filesystem::path& path, int startSeconds) {
    if (!healthy_ && !start()) return false;
    pendingResume_ = std::max(0, startSeconds);
    position_ = 0;
    duration_ = 0;
    paused_ = false;
    ended_ = false;
    return sendCommand("{\"command\":[\"loadfile\"," + jsonString(path.string()) +
                       ",\"replace\"]}");
}

void MpvPlayer::togglePause() {
    if (!healthy_) return;
    sendCommand(std::string("{\"command\":[\"set_property\",\"pause\",") +
                (paused_ ? "false" : "true") + "]}");
}

void MpvPlayer::seek(int seconds) {
    if (!healthy_) return;
    const double target = std::max(0.0, position_.load() + seconds);
    sendCommand("{\"command\":[\"seek\"," + std::to_string(target) + ",\"absolute\",\"exact\"]}");
}

PlayerSnapshot MpvPlayer::snapshot() const { return {position_, duration_, paused_, healthy_}; }

bool MpvPlayer::consumeEnded() { return ended_.exchange(false); }

void MpvPlayer::handleEvent(const std::string& json) {
    if (contains(json, "\"event\":\"file-loaded\"")) {
        const int resume = pendingResume_.exchange(0);
        if (resume > 0)
            sendCommand("{\"command\":[\"seek\"," + std::to_string(resume) +
                        ",\"absolute\",\"exact\"]}");
        return;
    }
    if (contains(json, "\"event\":\"end-file\"")) {
        if (contains(json, "\"reason\":\"eof\"")) ended_ = true;
        if (contains(json, "\"reason\":\"error\"")) setError("mpv could not play this track");
        return;
    }
    if (!contains(json, "\"event\":\"property-change\"")) return;
    if (contains(json, "\"name\":\"time-pos\""))
        position_ = numberAfter(json, "\"data\":", position_);
    else if (contains(json, "\"name\":\"duration\""))
        duration_ = numberAfter(json, "\"data\":", duration_);
    else if (contains(json, "\"name\":\"pause\""))
        paused_ = contains(json, "\"data\":true");
}

void MpvPlayer::readEvents() {
    std::string buffer;
    char chunk[4096];
    while (healthy_) {
        const auto count = recv(socket_, chunk, sizeof(chunk), 0);
        if (count > 0) {
            buffer.append(chunk, static_cast<size_t>(count));
            size_t newline = 0;
            while ((newline = buffer.find('\n')) != std::string::npos) {
                handleEvent(buffer.substr(0, newline));
                buffer.erase(0, newline + 1);
            }
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        break;
    }
    healthy_ = false;
}

void MpvPlayer::stop() {
    if (socket_ >= 0) {
        sendCommand("{\"command\":[\"quit\"]}");
        shutdown(socket_, SHUT_RDWR);
        close(socket_);
        socket_ = -1;
    }
    healthy_ = false;
    if (reader_.joinable()) reader_.join();
    if (process_ > 0) {
        int status = 0;
        bool reaped = false;
        for (int attempt = 0; attempt < 25; ++attempt) {
            if (waitpid(process_, &status, WNOHANG) == process_) {
                reaped = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!reaped) {
            kill(process_, SIGTERM);
            while (waitpid(process_, &status, 0) < 0 && errno == EINTR) {
            }
        }
        process_ = -1;
    }
    if (!socketPath_.empty()) unlink(socketPath_.c_str());
}
