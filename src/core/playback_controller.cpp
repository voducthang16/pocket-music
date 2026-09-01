#include "core/playback_controller.hpp"

#include <algorithm>

PlaybackController::PlaybackController(const MusicLibrary& library,
                                       std::unique_ptr<AudioPlayer> player)
    : library_(library), player_(std::move(player)) {}

bool PlaybackController::play(size_t trackIndex, const std::vector<size_t>& source,
                              std::optional<size_t> sourcePosition, std::string sourceTitle) {
    if (trackIndex >= library_.tracks().size() || source.empty() ||
        std::find(source.begin(), source.end(), trackIndex) == source.end())
        return false;
    const auto selected =
        sourcePosition && *sourcePosition < source.size() && source[*sourcePosition] == trackIndex
            ? *sourcePosition
            : static_cast<size_t>(std::find(source.begin(), source.end(), trackIndex) -
                                  source.begin());
    if (!requestLoad(trackIndex, 0, false, LoadOrigin::User)) {
        failedLoad_->source = source;
        failedLoad_->sourcePosition = selected;
        failedLoad_->sourceTitle = std::move(sourceTitle);
        return false;
    }
    queue_.reset(source, selected, queue_.shuffle());
    sourceTitle_ = std::move(sourceTitle);
    return true;
}

bool PlaybackController::restore(std::vector<size_t> source, std::vector<size_t> order,
                                 std::vector<size_t> history, size_t cursor, bool shuffle,
                                 double resumeSeconds, std::string sourceTitle) {
    if (!queue_.restore(std::move(source), std::move(order), std::move(history), cursor, shuffle))
        return false;
    sourceTitle_ = std::move(sourceTitle);
    return requestLoad(queue_.current(), resumeSeconds, true, LoadOrigin::Restore);
}

bool PlaybackController::requestLoad(size_t trackIndex, double resumeSeconds, bool startPaused,
                                     LoadOrigin origin) {
    if (trackIndex >= library_.tracks().size()) return false;
    const uint64_t generation = player_->load(library_.tracks()[trackIndex].path);
    if (generation == 0) {
        failedLoad_ = FailedLoad{.trackIndex = trackIndex,
                                 .resumeSeconds = std::max(0.0, resumeSeconds),
                                 .startPaused = startPaused,
                                 .source = {},
                                 .sourcePosition = 0,
                                 .sourceTitle = {}};
        snapshot_.phase = PlaybackPhase::Error;
        snapshot_.errorMessage = player_->error();
        ++revision_;
        return false;
    }
    pendingLoad_ = PendingLoad{.generation = generation,
                               .trackIndex = trackIndex,
                               .resumeSeconds = std::max(0.0, resumeSeconds),
                               .startPaused = startPaused,
                               .seekable = false,
                               .origin = origin};
    failedLoad_.reset();
    if (origin != LoadOrigin::Automatic) automaticFailures_ = 0;
    snapshot_.phase = PlaybackPhase::Loading;
    snapshot_.positionSeconds = 0;
    snapshot_.durationSeconds = 0;
    snapshot_.seekable = false;
    snapshot_.errorMessage.clear();
    ++revision_;
    return true;
}

void PlaybackController::update() {
    for (const auto& event : player_->drainEvents()) handle(event);
}

void PlaybackController::handle(const PlayerEvent& event) {
    const uint64_t expectedGeneration =
        pendingLoad_ ? pendingLoad_->generation : currentGeneration_;
    if (event.generation != 0 && event.generation != expectedGeneration) return;
    switch (event.type) {
        case PlayerEventType::FileLoaded:
            if (!pendingLoad_) break;
            currentGeneration_ = pendingLoad_->generation;
            snapshot_.trackIndex = pendingLoad_->trackIndex;
            snapshot_.positionSeconds = pendingLoad_->resumeSeconds;
            snapshot_.phase =
                pendingLoad_->startPaused ? PlaybackPhase::Paused : PlaybackPhase::Playing;
            snapshot_.seekable = pendingLoad_->seekable;
            if (pendingLoad_->resumeSeconds > 0) player_->seekAbsolute(pendingLoad_->resumeSeconds);
            player_->setPaused(pendingLoad_->startPaused);
            pendingLoad_.reset();
            automaticFailures_ = 0;
            ++revision_;
            break;
        case PlayerEventType::PositionChanged:
            if (snapshot_.phase != PlaybackPhase::Loading)
                snapshot_.positionSeconds = std::max(0.0, event.number);
            break;
        case PlayerEventType::DurationChanged:
            if (snapshot_.phase != PlaybackPhase::Loading) {
                snapshot_.durationSeconds = std::max(0.0, event.number);
                if (snapshot_.durationSeconds > 0 &&
                    snapshot_.positionSeconds >= snapshot_.durationSeconds) {
                    snapshot_.positionSeconds = std::max(0.0, snapshot_.durationSeconds - 1.0);
                    player_->seekAbsolute(snapshot_.positionSeconds);
                }
            }
            break;
        case PlayerEventType::PauseChanged:
            if (snapshot_.phase != PlaybackPhase::Loading &&
                snapshot_.phase != PlaybackPhase::Error &&
                snapshot_.phase != PlaybackPhase::Finished)
                snapshot_.phase = event.flag ? PlaybackPhase::Paused : PlaybackPhase::Playing;
            ++revision_;
            break;
        case PlayerEventType::SeekableChanged:
            if (pendingLoad_) pendingLoad_->seekable = event.flag;
            if (snapshot_.phase != PlaybackPhase::Loading) snapshot_.seekable = event.flag;
            break;
        case PlayerEventType::Ended:
            advanceAfterEnd();
            break;
        case PlayerEventType::Failed:
        case PlayerEventType::Disconnected:
            if (event.type == PlayerEventType::Failed && pendingLoad_ &&
                pendingLoad_->origin == LoadOrigin::Automatic &&
                ++automaticFailures_ < queue_.source().size()) {
                if (const auto nextTrack = queue_.next(repeatMode_ == RepeatMode::All)) {
                    requestLoad(*nextTrack, 0, false, LoadOrigin::Automatic);
                    break;
                }
            }
            if (pendingLoad_)
                failedLoad_ = FailedLoad{.trackIndex = pendingLoad_->trackIndex,
                                         .resumeSeconds = pendingLoad_->resumeSeconds,
                                         .startPaused = pendingLoad_->startPaused,
                                         .source = {},
                                         .sourcePosition = 0,
                                         .sourceTitle = {}};
            else if (snapshot_.trackIndex)
                failedLoad_ = FailedLoad{.trackIndex = *snapshot_.trackIndex,
                                         .resumeSeconds = snapshot_.positionSeconds,
                                         .startPaused = true,
                                         .source = {},
                                         .sourcePosition = 0,
                                         .sourceTitle = {}};
            pendingLoad_.reset();
            snapshot_.phase = PlaybackPhase::Error;
            snapshot_.errorMessage = event.message.empty() ? "Playback failed" : event.message;
            ++revision_;
            break;
    }
}

void PlaybackController::advanceAfterEnd() {
    if (!snapshot_.trackIndex) return;
    if (repeatMode_ == RepeatMode::One) {
        requestLoad(*snapshot_.trackIndex, 0, false, LoadOrigin::Automatic);
        return;
    }
    const auto track = queue_.next(repeatMode_ == RepeatMode::All);
    if (track)
        requestLoad(*track, 0, false, LoadOrigin::Automatic);
    else {
        snapshot_.phase = PlaybackPhase::Finished;
        snapshot_.positionSeconds = snapshot_.durationSeconds;
        ++revision_;
    }
}

void PlaybackController::togglePause() {
    if (snapshot_.phase == PlaybackPhase::Playing || snapshot_.phase == PlaybackPhase::Paused)
        player_->togglePause();
}

void PlaybackController::seekRelative(int seconds) {
    if (!snapshot_.seekable ||
        (snapshot_.phase != PlaybackPhase::Playing && snapshot_.phase != PlaybackPhase::Paused))
        return;
    player_->seekRelative(seconds);
}

void PlaybackController::next() {
    if (queue_.empty()) return;
    if (const auto track = queue_.next(repeatMode_ == RepeatMode::All))
        requestLoad(*track, 0, false, LoadOrigin::User);
    else
        snapshot_.phase = PlaybackPhase::Finished;
    ++revision_;
}

void PlaybackController::previous() {
    if (!snapshot_.trackIndex && !pendingLoad_) return;
    if (snapshot_.trackIndex && snapshot_.phase != PlaybackPhase::Loading &&
        snapshot_.positionSeconds > 3.0) {
        player_->seekAbsolute(0);
        return;
    }
    if (const auto track = queue_.previous()) requestLoad(*track, 0, false, LoadOrigin::User);
}

void PlaybackController::retry() {
    if (snapshot_.phase != PlaybackPhase::Error || !failedLoad_) return;
    const auto failed = *failedLoad_;
    if (!requestLoad(failed.trackIndex, failed.resumeSeconds, failed.startPaused,
                     LoadOrigin::Retry))
        return;
    if (!failed.source.empty()) {
        queue_.reset(failed.source, failed.sourcePosition, queue_.shuffle());
        sourceTitle_ = failed.sourceTitle;
    }
}

std::optional<size_t> PlaybackController::displayTrackIndex() const {
    if (pendingLoad_) return pendingLoad_->trackIndex;
    if (snapshot_.phase == PlaybackPhase::Error && failedLoad_) return failedLoad_->trackIndex;
    return snapshot_.trackIndex;
}

void PlaybackController::setShuffle(bool shuffle, uint32_t seed) {
    queue_.setShuffle(shuffle, seed);
    ++revision_;
}

void PlaybackController::setRepeatMode(RepeatMode mode) {
    if (repeatMode_ == mode) return;
    repeatMode_ = mode;
    ++revision_;
}
