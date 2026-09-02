#include "app/navigation.hpp"

#include <algorithm>

namespace {
std::string countLabel(size_t count, const char* singular, const char* plural) {
    return std::to_string(count) + ' ' + (count == 1 ? singular : plural);
}

ViewItem trackItem(const Track& track, size_t index) {
    return {track.title, track.artist, ViewAction::None, {index}};
}

ViewState tracksView(const AppState& app, std::string title, std::vector<size_t> indexes) {
    ViewState view{Screen::Tracks, std::move(title), {}, 0, 0};
    for (size_t index : indexes)
        view.items.push_back(trackItem(app.library.tracks()[index], index));
    return view;
}

void pushView(AppState& app, ViewState next) {
    app.history.push_back(std::move(app.view));
    app.view = std::move(next);
}

ViewState libraryView(const AppState& app) {
    ViewState view{Screen::Library,
                   "Library",
                   {},
                   0,
                   0};
    view.items = {
        {"Songs", countLabel(app.library.tracks().size(), "track", "tracks"),
         ViewAction::OpenSongs, app.library.allTrackIndexes()},
        {"Albums", countLabel(app.library.albums().size(), "album", "albums"),
         ViewAction::OpenAlbums, {}},
        {"Artists", countLabel(app.library.artists().size(), "artist", "artists"),
         ViewAction::OpenArtists, {}},
        {"Playlists", countLabel(app.library.playlists().size(), "playlist", "playlists"),
         ViewAction::OpenPlaylists, {}},
        {"Now Playing",
         app.playback.displayTrackIndex() ? "Open current track" : "Nothing playing",
         ViewAction::OpenNowPlaying,
         {}}};
    return view;
}

void refreshLibraryView(AppState& app) {
    const int selected = app.view.selected;
    app.view = libraryView(app);
    app.view.selected = std::clamp(selected, 0, static_cast<int>(app.view.items.size()) - 1);
}

void showGroups(AppState& app, Screen screen, const std::string& title,
                const std::vector<TrackGroup>& groups) {
    ViewState view{screen, title, {}, 0, 0};
    for (const auto& group : groups)
        view.items.push_back({group.title, group.subtitle, ViewAction::None, group.trackIndexes});
    pushView(app, std::move(view));
}
}  // namespace

void buildLibraryView(AppState& app) {
    app.view = libraryView(app);
    app.history.clear();
}

ViewState nowPlayingView() { return {Screen::NowPlaying, "Now Playing", {}, 0, 0}; }

void openNowPlaying(AppState& app) {
    if (app.view.screen == Screen::NowPlaying || !app.playback.displayTrackIndex()) return;
    pushView(app, nowPlayingView());
}

bool playTrack(AppState& app, size_t trackIndex, const std::vector<size_t>& queue,
               std::optional<size_t> sourcePosition) {
    if (!app.playback.play(trackIndex, queue, sourcePosition, app.view.title)) {
        app.message = app.playback.snapshot().errorMessage;
        return false;
    }
    app.message.clear();
    openNowPlaying(app);
    return true;
}

void playAdjacentTrack(AppState& app, int direction) {
    if (direction > 0)
        app.playback.next();
    else
        app.playback.previous();
}

void selectCurrentItem(AppState& app) {
    if (app.view.screen == Screen::NowPlaying || app.view.selected < 0 ||
        app.view.selected >= static_cast<int>(app.view.items.size()))
        return;
    const auto selected = static_cast<size_t>(app.view.selected);
    const auto& item = app.view.items[selected];
    switch (item.action) {
        case ViewAction::OpenSongs:
            pushView(app, tracksView(app, "Songs", app.library.allTrackIndexes()));
            return;
        case ViewAction::OpenAlbums:
            showGroups(app, Screen::Albums, "Albums", app.library.albums());
            return;
        case ViewAction::OpenArtists:
            showGroups(app, Screen::Artists, "Artists", app.library.artists());
            return;
        case ViewAction::OpenPlaylists: {
            ViewState view{Screen::Playlists, "Playlists", {}, 0, 0};
            for (const auto& playlist : app.library.playlists())
                view.items.push_back({playlist.name,
                                      countLabel(playlist.trackIndexes.size(), "track", "tracks"),
                                      ViewAction::None, playlist.trackIndexes});
            pushView(app, std::move(view));
            return;
        }
        case ViewAction::OpenNowPlaying:
            openNowPlaying(app);
            return;
        case ViewAction::None:
            break;
    }
    if (app.view.screen == Screen::Tracks) {
        std::vector<size_t> queue;
        queue.reserve(app.view.items.size());
        for (const auto& row : app.view.items)
            if (!row.trackIndexes.empty()) queue.push_back(row.trackIndexes.front());
        playTrack(app, item.trackIndexes.front(), queue, selected);
    } else {
        pushView(app, tracksView(app, item.title, item.trackIndexes));
    }
}

void navigateBack(AppState& app) {
    if (app.history.empty()) {
        if (app.view.screen != Screen::Library) {
            buildLibraryView(app);
            return;
        }
        app.running = false;
        return;
    }
    app.view = std::move(app.history.back());
    app.history.pop_back();
    if (app.view.screen == Screen::Library) refreshLibraryView(app);
}

void advanceWhenFinished(AppState& app) {
    app.playback.update();
    const auto& playback = app.playback.snapshot();
    if (playback.phase == PlaybackPhase::Error)
        app.message = playback.errorMessage;
    else if (playback.phase == PlaybackPhase::Loading ||
             playback.phase == PlaybackPhase::Playing || playback.phase == PlaybackPhase::Paused)
        app.message.clear();
}
