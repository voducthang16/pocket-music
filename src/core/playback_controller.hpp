#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/library.hpp"
#include "core/playback_queue.hpp"
#include "core/player.hpp"

enum class PlaybackPhase { Idle, Loading, Playing, Paused, Finished, Error };
enum class RepeatMode { Off, One, All };

struct PlaybackSnapshot {
    PlaybackPhase phase = PlaybackPhase::Idle;
    std::optional<size_t> trackIndex;
    double positionSeconds = 0;
    double durationSeconds = 0;
    bool seekable = false;
    std::string errorMessage;
};

class PlaybackController {
   public:
    PlaybackController(const MusicLibrary& library, std::unique_ptr<AudioPlayer> player);

    bool play(size_t trackIndex, const std::vector<size_t>& source,
              std::optional<size_t> sourcePosition = std::nullopt, std::string sourceTitle = {});
    bool restore(std::vector<size_t> source, std::vector<size_t> order, std::vector<size_t> history,
                 size_t cursor, bool shuffle, double resumeSeconds, std::string sourceTitle);
    void update();
    void togglePause();
    void seekRelative(int seconds);
    void next();
    void previous();
    void retry();
    void setShuffle(bool shuffle, uint32_t seed = std::random_device{}());
    void setRepeatMode(RepeatMode mode);

    const PlaybackSnapshot& snapshot() const { return snapshot_; }
    std::optional<size_t> displayTrackIndex() const;
    const PlaybackQueue& queue() const { return queue_; }
    RepeatMode repeatMode() const { return repeatMode_; }
    bool shuffle() const { return queue_.shuffle(); }
    uint64_t revision() const { return revision_; }
    const std::string& sourceTitle() const { return sourceTitle_; }

   private:
    enum class LoadOrigin { User, Restore, Automatic, Retry };

    struct PendingLoad {
        uint64_t generation;
        size_t trackIndex;
        double resumeSeconds;
        bool startPaused;
        bool seekable = false;
        LoadOrigin origin;
    };

    struct FailedLoad {
        size_t trackIndex;
        double resumeSeconds;
        bool startPaused;
        std::vector<size_t> source;
        size_t sourcePosition = 0;
        std::string sourceTitle;
    };

    bool requestLoad(size_t trackIndex, double resumeSeconds, bool startPaused, LoadOrigin origin);
    bool cyclesQueue() const;
    void handle(const PlayerEvent& event);
    void advanceAfterEnd();

    const MusicLibrary& library_;
    std::unique_ptr<AudioPlayer> player_;
    PlaybackQueue queue_;
    PlaybackSnapshot snapshot_;
    RepeatMode repeatMode_ = RepeatMode::Off;
    uint64_t currentGeneration_ = 0;
    std::optional<PendingLoad> pendingLoad_;
    std::optional<FailedLoad> failedLoad_;
    uint64_t revision_ = 0;
    size_t automaticFailures_ = 0;
    std::string sourceTitle_;
};
