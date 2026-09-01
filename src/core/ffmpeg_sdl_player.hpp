#pragma once

#include <SDL.h>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/player.hpp"

class FfmpegSdlPlayer final : public AudioPlayer {
   public:
    FfmpegSdlPlayer() = default;
    ~FfmpegSdlPlayer() override;
    FfmpegSdlPlayer(const FfmpegSdlPlayer&) = delete;
    FfmpegSdlPlayer& operator=(const FfmpegSdlPlayer&) = delete;

    uint64_t load(const std::filesystem::path& path) override;
    bool togglePause() override;
    bool setPaused(bool paused) override;
    bool seekRelative(int seconds) override;
    bool seekAbsolute(double seconds) override;
    std::vector<PlayerEvent> drainEvents() override;
    std::string error() const override;

   private:
    void decode(std::filesystem::path path, uint64_t generation);
    void emit(PlayerEvent event);
    void fail(uint64_t generation, std::string message);
    void stop();

    std::thread worker_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> paused_{false};
    std::atomic<SDL_AudioDeviceID> device_{0};
    std::atomic<uint64_t> generation_{0};
    std::atomic<double> positionSeconds_{0};
    std::mutex seekMutex_;
    std::optional<double> pendingSeek_;
    mutable std::mutex errorMutex_;
    std::string error_;
    std::mutex eventMutex_;
    std::vector<PlayerEvent> events_;
};
