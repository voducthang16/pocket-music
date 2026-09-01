#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

enum class PlayerEventType {
    FileLoaded,
    PositionChanged,
    DurationChanged,
    PauseChanged,
    SeekableChanged,
    Ended,
    Failed,
    Disconnected,
};

struct PlayerEvent {
    PlayerEventType type;
    uint64_t generation = 0;
    double number = 0;
    bool flag = false;
    std::string message;

    PlayerEvent(PlayerEventType eventType, uint64_t eventGeneration = 0, double eventNumber = 0,
                bool eventFlag = false, std::string eventMessage = {})
        : type(eventType),
          generation(eventGeneration),
          number(eventNumber),
          flag(eventFlag),
          message(std::move(eventMessage)) {}
};

class AudioPlayer {
   public:
    virtual ~AudioPlayer() = default;
    virtual uint64_t load(const std::filesystem::path& path) = 0;
    virtual bool togglePause() = 0;
    virtual bool setPaused(bool paused) = 0;
    virtual bool seekRelative(int seconds) = 0;
    virtual bool seekAbsolute(double seconds) = 0;
    virtual std::vector<PlayerEvent> drainEvents() = 0;
    virtual std::string error() const = 0;
};

class MpvPlayer final : public AudioPlayer {
   public:
    MpvPlayer() = default;
    ~MpvPlayer() override;
    MpvPlayer(const MpvPlayer&) = delete;
    MpvPlayer& operator=(const MpvPlayer&) = delete;

    uint64_t load(const std::filesystem::path& path) override;
    bool togglePause() override;
    bool setPaused(bool paused) override;
    bool seekRelative(int seconds) override;
    bool seekAbsolute(double seconds) override;
    std::vector<PlayerEvent> drainEvents() override;
    std::string error() const override;

   private:
    enum class PendingKind { LoadCommand, FileLoadedPath };
    struct PendingRequest {
        PendingKind kind;
        uint64_t generation;
    };

    bool start();
    uint64_t sendCommand(const std::string& command, PendingRequest* pending = nullptr);
    void readEvents();
    void handleMessage(const std::string& line);
    void emit(PlayerEvent event);
    void stop();
    void setError(std::string message);

    std::string socketPath_;
    int socket_ = -1;
    int process_ = -1;
    std::thread reader_;
    mutable std::mutex writeMutex_;
    mutable std::mutex errorMutex_;
    std::mutex pendingMutex_;
    std::mutex eventMutex_;
    std::map<uint64_t, PendingRequest> pending_;
    std::vector<PlayerEvent> events_;
    std::atomic<uint64_t> nextRequestId_{1};
    std::atomic<uint64_t> nextGeneration_{1};
    std::atomic<uint64_t> activeGeneration_{0};
    std::atomic<bool> healthy_{false};
    std::string error_;
};

std::string jsonString(const std::string& value);
std::vector<PlayerEvent> decodePlayerMessage(const std::string& line, uint64_t generation);
