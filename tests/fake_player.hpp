#pragma once

#include <utility>

#include "core/player.hpp"

class FakePlayer final : public AudioPlayer {
   public:
    uint64_t load(const std::filesystem::path& path) override {
        loadedPath = path;
        ++loadCount;
        return loadSucceeds ? ++generation : 0;
    }
    bool togglePause() override {
        ++toggleCount;
        return true;
    }
    bool setPaused(bool paused) override {
        pausedValue = paused;
        return true;
    }
    bool seekRelative(int seconds) override {
        relativeSeek = seconds;
        relativeSeeks.push_back(seconds);
        return true;
    }
    bool seekAbsolute(double seconds) override {
        absoluteSeek = seconds;
        return true;
    }
    std::vector<PlayerEvent> drainEvents() override {
        std::vector<PlayerEvent> result;
        result.swap(events);
        return result;
    }
    std::string error() const override { return "fake player failure"; }

    void emit(PlayerEvent event) { events.push_back(std::move(event)); }
    std::filesystem::path loadedPath;
    int loadCount = 0;
    int toggleCount = 0;
    int relativeSeek = 0;
    std::vector<int> relativeSeeks;
    double absoluteSeek = -1;
    uint64_t generation = 0;
    bool loadSucceeds = true;
    bool pausedValue = false;
    std::vector<PlayerEvent> events;
};
