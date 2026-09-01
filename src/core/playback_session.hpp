#pragma once

#include <optional>
#include <vector>

#include "core/library.hpp"
#include "core/playback_controller.hpp"
#include "core/state.hpp"

struct ResolvedPlaybackSession {
    std::vector<size_t> source;
    std::vector<size_t> order;
    std::vector<size_t> history;
    size_t cursor = 0;
};

std::optional<ResolvedPlaybackSession> resolvePlaybackSession(const PlaybackSession& session,
                                                              const MusicLibrary& library);
PlaybackSession capturePlaybackSession(const PlaybackController& playback,
                                       const MusicLibrary& library, bool nowPlaying);
