#include "core/player.hpp"

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <nlohmann/json.hpp>
#include <thread>

using Json = nlohmann::json;

namespace {
constexpr size_t kMaximumMessageSize = 1024 * 1024;
constexpr const char* kGenerationPrefix = "pocket-music-request-";

std::string messageForMpvError(const Json& message) {
    if (message.contains("file_error") && message["file_error"].is_string())
        return message["file_error"].get<std::string>();
    if (message.contains("error") && message["error"].is_string())
        return message["error"].get<std::string>();
    return "mpv could not play this track";
}

uint64_t generationFromTitle(const Json& data) {
    if (!data.is_string()) return 0;
    const auto title = data.get<std::string>();
    if (!title.starts_with(kGenerationPrefix)) return 0;
    try {
        return std::stoull(title.substr(std::strlen(kGenerationPrefix)));
    } catch (...) {
        return 0;
    }
}
}  // namespace

std::string jsonString(const std::string& value) { return Json(value).dump(); }

std::vector<PlayerEvent> decodePlayerMessage(const std::string& line, uint64_t generation) {
    std::vector<PlayerEvent> events;
    const auto message = Json::parse(line, nullptr, false);
    if (message.is_discarded() || !message.is_object())
        return {{PlayerEventType::Failed, generation, 0, false, "Malformed mpv response"}};
    const auto event = message.value("event", std::string{});
    if (event == "property-change") {
        const auto name = message.value("name", std::string{});
        const auto data = message.find("data");
        if (data == message.end() || data->is_null()) return events;
        if (name == "time-pos" && data->is_number())
            events.push_back({PlayerEventType::PositionChanged, generation, data->get<double>()});
        else if (name == "duration" && data->is_number())
            events.push_back({PlayerEventType::DurationChanged, generation, data->get<double>()});
        else if (name == "pause" && data->is_boolean())
            events.push_back({PlayerEventType::PauseChanged, generation, 0, data->get<bool>()});
        else if (name == "seekable" && data->is_boolean())
            events.push_back({PlayerEventType::SeekableChanged, generation, 0, data->get<bool>()});
    } else if (event == "end-file") {
        const auto reason = message.value("reason", std::string{});
        if (reason == "eof")
            events.push_back({PlayerEventType::Ended, generation});
        else if (reason == "error")
            events.push_back(
                {PlayerEventType::Failed, generation, 0, false, messageForMpvError(message)});
    }
    return events;
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

void MpvPlayer::emit(PlayerEvent event) {
    std::lock_guard lock(eventMutex_);
    events_.push_back(std::move(event));
}

std::vector<PlayerEvent> MpvPlayer::drainEvents() {
    std::lock_guard lock(eventMutex_);
    std::vector<PlayerEvent> result;
    result.swap(events_);
    return result;
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
            sendCommand(R"({"command":["observe_property",1,"time-pos"]})");
            sendCommand(R"({"command":["observe_property",2,"duration"]})");
            sendCommand(R"({"command":["observe_property",3,"pause"]})");
            sendCommand(R"({"command":["observe_property",4,"seekable"]})");
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

uint64_t MpvPlayer::sendCommand(const std::string& command, PendingRequest* pending) {
    std::lock_guard lock(writeMutex_);
    if (socket_ < 0 || !healthy_) return 0;
    auto payloadJson = Json::parse(command, nullptr, false);
    if (payloadJson.is_discarded() || !payloadJson.is_object()) return 0;
    const uint64_t requestId = nextRequestId_++;
    payloadJson["request_id"] = requestId;
    if (pending) {
        std::lock_guard pendingLock(pendingMutex_);
        pending_[requestId] = *pending;
    }
    const std::string payload = payloadJson.dump() + '\n';
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
        return 0;
    }
    return requestId;
}

uint64_t MpvPlayer::load(const std::filesystem::path& path) {
    if (!healthy_ && !start()) return 0;
    const uint64_t generation = nextGeneration_++;
    activeGeneration_ = generation;
    const Json options = {
        {"force-media-title", std::string(kGenerationPrefix) + std::to_string(generation)}};
    const Json command = {{"command", {"loadfile", path.string(), "replace", -1, options}}};
    PendingRequest pending{PendingKind::LoadCommand, generation};
    if (!sendCommand(command.dump(), &pending)) return 0;
    return generation;
}

bool MpvPlayer::togglePause() { return sendCommand(R"({"command":["cycle","pause"]})") != 0; }

bool MpvPlayer::setPaused(bool paused) {
    const Json command = {{"command", {"set_property", "pause", paused}}};
    return sendCommand(command.dump()) != 0;
}

bool MpvPlayer::seekRelative(int seconds) {
    const Json command = {{"command", {"seek", seconds, "relative+exact"}}};
    return sendCommand(command.dump()) != 0;
}

bool MpvPlayer::seekAbsolute(double seconds) {
    const Json command = {{"command", {"seek", std::max(0.0, seconds), "absolute+exact"}}};
    return sendCommand(command.dump()) != 0;
}

void MpvPlayer::handleMessage(const std::string& line) {
    const auto message = Json::parse(line, nullptr, false);
    if (message.is_discarded() || !message.is_object()) {
        emit({PlayerEventType::Failed, activeGeneration_, 0, false, "Malformed mpv response"});
        return;
    }
    if (message.value("event", std::string{}) == "file-loaded") {
        PendingRequest pending{PendingKind::FileLoadedPath, activeGeneration_};
        sendCommand(R"({"command":["get_property","media-title"]})", &pending);
        return;
    }
    if (message.contains("request_id") && message["request_id"].is_number_integer()) {
        const auto requestId = message["request_id"].get<uint64_t>();
        PendingRequest pending{PendingKind::LoadCommand, 0};
        bool found = false;
        {
            std::lock_guard lock(pendingMutex_);
            if (const auto item = pending_.find(requestId); item != pending_.end()) {
                pending = item->second;
                pending_.erase(item);
                found = true;
            }
        }
        if (!found) return;
        const auto commandError = message.value("error", std::string{"success"});
        if (commandError != "success") {
            emit({PlayerEventType::Failed, pending.generation, 0, false, commandError});
        } else if (pending.kind == PendingKind::FileLoadedPath && message.contains("data")) {
            const auto generation = generationFromTitle(message["data"]);
            if (generation > 0) emit({PlayerEventType::FileLoaded, generation});
        }
        return;
    }
    for (auto& event : decodePlayerMessage(line, activeGeneration_)) emit(std::move(event));
}

void MpvPlayer::readEvents() {
    std::string buffer;
    char chunk[4096];
    while (healthy_) {
        const auto count = recv(socket_, chunk, sizeof(chunk), 0);
        if (count > 0) {
            buffer.append(chunk, static_cast<size_t>(count));
            if (buffer.size() > kMaximumMessageSize) {
                emit({PlayerEventType::Failed, activeGeneration_, 0, false,
                      "mpv response exceeded the size limit"});
                buffer.clear();
            }
            size_t newline = 0;
            while ((newline = buffer.find('\n')) != std::string::npos) {
                handleMessage(buffer.substr(0, newline));
                buffer.erase(0, newline + 1);
            }
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        break;
    }
    if (healthy_.exchange(false))
        emit(
            {PlayerEventType::Disconnected, activeGeneration_, 0, false, "Lost connection to mpv"});
}

void MpvPlayer::stop() {
    if (socket_ >= 0) {
        sendCommand(R"({"command":["quit"]})");
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
    {
        std::lock_guard lock(pendingMutex_);
        pending_.clear();
    }
    if (!socketPath_.empty()) unlink(socketPath_.c_str());
}
