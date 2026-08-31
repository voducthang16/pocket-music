#pragma once

#include <filesystem>
#include <string>

class MpvPlayer {
   public:
    MpvPlayer();
    ~MpvPlayer();
    MpvPlayer(const MpvPlayer&) = delete;
    MpvPlayer& operator=(const MpvPlayer&) = delete;

    bool start();
    bool load(const std::filesystem::path& path, int startSeconds = 0);
    void togglePause();
    void seek(int seconds);
    void stop();
    bool paused() const { return paused_; }
    int elapsedSeconds() const;
    const std::string& error() const { return error_; }

   private:
    bool command(const std::string& json);
    std::string socketPath_;
    int socket_ = -1;
    int process_ = -1;
    bool paused_ = false;
    int baseSeconds_ = 0;
    uint64_t startedAtMs_ = 0;
    uint64_t pausedAtMs_ = 0;
    std::string error_;
};
