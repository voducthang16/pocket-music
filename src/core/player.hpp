#pragma once

#include <filesystem>
#include <string>
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
