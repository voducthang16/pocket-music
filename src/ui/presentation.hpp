#pragma once

#include <string_view>

#include "app/app_state.hpp"

struct HomeRowPresentation {
    std::string_view trailing;
    bool chevron = false;
};

inline HomeRowPresentation homeRowPresentation(const MenuItem& item) {
    return {item.trailing, item.trailing.empty()};
}

struct PlaybackPresentation {
    std::string_view status;
    std::string_view primaryAction;
    bool showPauseIcon = false;
};

inline PlaybackPresentation playbackPresentation(PlaybackPhase phase) {
    switch (phase) {
        case PlaybackPhase::Idle:
            return {"READY", "", false};
        case PlaybackPhase::Loading:
            return {"LOADING", "", false};
        case PlaybackPhase::Playing:
            return {"PLAYING", "PAUSE", true};
        case PlaybackPhase::Paused:
            return {"PAUSED", "PLAY", false};
        case PlaybackPhase::Finished:
            return {"FINISHED", "", false};
        case PlaybackPhase::Error:
            return {"PLAYBACK ERROR", "RETRY", false};
    }
    return {"", "", false};
}
