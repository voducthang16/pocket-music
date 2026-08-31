#include "core/player.hpp"

#include <SDL.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

namespace {
std::string escapeJson(const std::string& input) {
    std::string output;
    for (const char c : input) {
        if (c == '\\' || c == '"') output.push_back('\\');
        output.push_back(c);
    }
    return output;
}
}  // namespace

MpvPlayer::MpvPlayer() = default;
MpvPlayer::~MpvPlayer() { stop(); }

bool MpvPlayer::start() {
    if (process_ > 0) return true;
    socketPath_ = "/tmp/pocket-music-" + std::to_string(getpid()) + ".sock";
    unlink(socketPath_.c_str());
    process_ = fork();
    if (process_ == 0) {
        const std::string ipc = "--input-ipc-server=" + socketPath_;
        execlp("mpv", "mpv", "--idle=yes", "--no-video", "--really-quiet", ipc.c_str(), nullptr);
        _exit(127);
    }
    if (process_ < 0) {
        error_ = "Could not start mpv";
        return false;
    }

    for (int attempt = 0; attempt < 50; ++attempt) {
        socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, socketPath_.c_str(), sizeof(address.sun_path) - 1);
        if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0)
            return true;
        close(socket_);
        socket_ = -1;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    error_ = "mpv IPC did not become ready";
    stop();
    return false;
}

bool MpvPlayer::command(const std::string& json) {
    if (socket_ < 0 && !start()) return false;
    const std::string payload = json + "\n";
    return write(socket_, payload.data(), payload.size()) == static_cast<ssize_t>(payload.size());
}

bool MpvPlayer::load(const std::filesystem::path& path, int startSeconds) {
    if (!command("{\"command\":[\"loadfile\",\"" + escapeJson(path.string()) + "\",\"replace\"]}"))
        return false;
    paused_ = false;
    baseSeconds_ = startSeconds;
    startedAtMs_ = SDL_GetTicks64();
    pausedAtMs_ = 0;
    if (startSeconds > 0) {
        command("{\"command\":[\"seek\"," + std::to_string(startSeconds) + ",\"absolute\"]}");
    }
    return true;
}

void MpvPlayer::togglePause() {
    if (socket_ < 0) return;
    paused_ = !paused_;
    command(std::string("{\"command\":[\"set_property\",\"pause\",") +
            (paused_ ? "true" : "false") + "]}");
    if (paused_) {
        pausedAtMs_ = SDL_GetTicks64();
    } else {
        startedAtMs_ += SDL_GetTicks64() - pausedAtMs_;
    }
}

void MpvPlayer::seek(int seconds) {
    const int target = std::max(0, elapsedSeconds() + seconds);
    command("{\"command\":[\"seek\"," + std::to_string(target) + ",\"absolute\"]}");
    baseSeconds_ = target;
    startedAtMs_ = SDL_GetTicks64();
    if (paused_) pausedAtMs_ = startedAtMs_;
}

int MpvPlayer::elapsedSeconds() const {
    if (startedAtMs_ == 0) return baseSeconds_;
    const uint64_t now = paused_ ? pausedAtMs_ : SDL_GetTicks64();
    return baseSeconds_ + static_cast<int>((now - startedAtMs_) / 1000);
}

void MpvPlayer::stop() {
    if (socket_ >= 0) {
        command("{\"command\":[\"quit\"]}");
        close(socket_);
        socket_ = -1;
    }
    if (process_ > 0) {
        int status = 0;
        waitpid(process_, &status, WNOHANG);
        process_ = -1;
    }
    if (!socketPath_.empty()) unlink(socketPath_.c_str());
}
