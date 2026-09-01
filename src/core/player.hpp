#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

struct PlayerSnapshot {
    double positionSeconds = 0;
    double durationSeconds = 0;
    bool paused = false;
    bool healthy = false;
};

class AudioPlayer {
   public:
    virtual ~AudioPlayer() = default;
    virtual bool load(const std::filesystem::path& path, int startSeconds = 0) = 0;
    virtual void togglePause() = 0;
    virtual void seek(int seconds) = 0;
    virtual PlayerSnapshot snapshot() const = 0;
    virtual bool consumeEnded() = 0;
    virtual std::string error() const = 0;
};

class MpvPlayer final : public AudioPlayer {
   public:
    MpvPlayer() = default;
    ~MpvPlayer() override;
    MpvPlayer(const MpvPlayer&) = delete;
    MpvPlayer& operator=(const MpvPlayer&) = delete;

    bool load(const std::filesystem::path& path, int startSeconds = 0) override;
    void togglePause() override;
    void seek(int seconds) override;
    PlayerSnapshot snapshot() const override;
    bool consumeEnded() override;
    std::string error() const override;

   private:
    bool start();
    bool sendCommand(const std::string& json);
    void readEvents();
    void handleEvent(const std::string& json);
    void stop();
    void setError(std::string message);

    std::string socketPath_;
    int socket_ = -1;
    int process_ = -1;
    std::thread reader_;
    mutable std::mutex writeMutex_;
    mutable std::mutex errorMutex_;
    std::atomic<double> position_{0};
    std::atomic<double> duration_{0};
    std::atomic<int> pendingResume_{0};
    std::atomic<bool> paused_{false};
    std::atomic<bool> healthy_{false};
    std::atomic<bool> ended_{false};
    std::string error_;
};

std::string jsonString(const std::string& value);
