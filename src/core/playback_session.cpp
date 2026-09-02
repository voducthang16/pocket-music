#include "core/playback_session.hpp"

#include <algorithm>
#include <unordered_map>

std::optional<ResolvedPlaybackSession> resolvePlaybackSession(const PlaybackSession& session,
                                                              const MusicLibrary& library) {
    std::unordered_map<std::string, size_t> indexes;
    for (size_t index = 0; index < library.tracks().size(); ++index)
        indexes[library.tracks()[index].path.string()] = index;
    const auto resolve = [&](const std::vector<std::string>& paths) {
        std::vector<size_t> result;
        for (const auto& path : paths)
            if (const auto found = indexes.find(path); found != indexes.end())
                result.push_back(found->second);
        return result;
    };

    ResolvedPlaybackSession resolved{resolve(session.sourcePaths), resolve(session.orderPaths),
                                     resolve(session.historyPaths), 0};
    if (resolved.source.empty() || resolved.order.empty()) return std::nullopt;
    resolved.cursor = std::min(session.cursor, resolved.order.size() - 1);
    if (const auto current = indexes.find(session.currentTrackPath); current != indexes.end()) {
        const auto position =
            std::find(resolved.order.begin(), resolved.order.end(), current->second);
        if (position != resolved.order.end())
            resolved.cursor = static_cast<size_t>(position - resolved.order.begin());
    }
    return resolved;
}

PlaybackSession capturePlaybackSession(const PlaybackController& playback,
                                       const MusicLibrary& library, bool nowPlaying) {
    const auto snapshot = playback.snapshot();
    PlaybackSession session;
    session.sourceTitle = playback.sourceTitle();
    const auto pathFor = [&](size_t index) { return library.tracks()[index].path.string(); };
    for (size_t index : playback.queue().source()) session.sourcePaths.push_back(pathFor(index));
    for (size_t index : playback.queue().order()) session.orderPaths.push_back(pathFor(index));
    for (size_t index : playback.queue().history()) session.historyPaths.push_back(pathFor(index));
    session.cursor = playback.queue().cursor();
    if (!playback.queue().empty()) {
        const size_t currentTrack = playback.queue().current();
        session.currentTrackPath = pathFor(currentTrack);
        if (snapshot.trackIndex == currentTrack) session.positionSeconds = snapshot.positionSeconds;
    }
    session.paused = snapshot.phase != PlaybackPhase::Playing;
    session.shuffle = playback.shuffle();
    session.repeatMode = static_cast<int>(playback.repeatMode());
    session.screen = nowPlaying ? "now-playing" : "home";
    return session;
}
