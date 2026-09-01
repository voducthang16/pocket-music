#include "app/navigation.hpp"

#include <algorithm>
#include <random>

#include "ui/primitives.hpp"
#include "ui/theme.hpp"

namespace {
std::string countLabel(size_t count, const char* singular, const char* plural) {
    return std::to_string(count) + ' ' + (count == 1 ? singular : plural);
}

ViewItem trackItem(const Track& track, size_t index) {
    return {track.title, track.artist, ViewAction::None, {index}};
}

ViewState tracksView(const AppState& app, std::string title, std::vector<size_t> indexes) {
    ViewState view{Screen::Tracks, std::move(title), "LIBRARY", {}, 0, 0};
    for (size_t index : indexes)
        view.items.push_back(trackItem(app.library.tracks()[index], index));
    return view;
}

void pushView(AppState& app, ViewState next) {
    app.history.push_back(std::move(app.view));
    app.view = std::move(next);
}

void showGroups(AppState& app, Screen screen, const std::string& title,
                const std::vector<TrackGroup>& groups) {
    ViewState view{screen, title, "LIBRARY", {}, 0, 0};
    for (const auto& group : groups)
        view.items.push_back({group.title, group.subtitle, ViewAction::None, group.trackIndexes});
    pushView(app, std::move(view));
}
}  // namespace

void buildLibraryView(AppState& app) {
    app.view = {Screen::Library,
                "Library",
                countLabel(app.library.tracks().size(), "song", "songs") + " available offline",
                {},
                0,
                0};
    app.view.items = {
        {"Songs", countLabel(app.library.tracks().size(), "track", "tracks"), ViewAction::OpenSongs,
         app.library.allTrackIndexes()},
        {"Albums",
         countLabel(app.library.albums().size(), "album", "albums"),
         ViewAction::OpenAlbums,
         {}},
        {"Artists",
         countLabel(app.library.artists().size(), "artist", "artists"),
         ViewAction::OpenArtists,
         {}},
        {"Playlists",
         countLabel(app.library.playlists().size(), "playlist", "playlists"),
         ViewAction::OpenPlaylists,
         {}},
        {"Now Playing",
         app.playback.snapshot().trackIndex ? "Open current track" : "Nothing playing",
         ViewAction::OpenNowPlaying,
         {}},
        {"Settings", "Appearance", ViewAction::OpenSettings, {}}};
    app.history.clear();
}

bool playTrack(AppState& app, size_t trackIndex, const std::vector<size_t>& queue,
               std::optional<size_t> sourcePosition) {
    if (!app.playback.play(trackIndex, queue, sourcePosition, app.view.title)) {
        app.message = app.playback.snapshot().errorMessage;
        return false;
    }
    app.message.clear();
    if (app.view.screen != Screen::NowPlaying)
        pushView(app, {Screen::NowPlaying, "Now Playing", "POCKET MUSIC", {}, 0, 0});
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
            ViewState view{Screen::Playlists, "Playlists", "LIBRARY", {}, 0, 0};
            for (const auto& playlist : app.library.playlists())
                view.items.push_back({playlist.name,
                                      countLabel(playlist.trackIndexes.size(), "track", "tracks"),
                                      ViewAction::None, playlist.trackIndexes});
            pushView(app, std::move(view));
            return;
        }
        case ViewAction::OpenNowPlaying:
            if (app.playback.snapshot().trackIndex)
                pushView(app, {Screen::NowPlaying, "Now Playing", "POCKET MUSIC", {}, 0, 0});
            return;
        case ViewAction::OpenSettings: {
            const auto selectedTheme = app.preferences.theme == ThemeMode::Dark ? "Dark" : "Light";
            ViewState settings{Screen::Settings, "Settings", "APPEARANCE", {}, 0, 0};
            settings.items.push_back({"Theme", selectedTheme, ViewAction::ToggleTheme, {}});
            pushView(app, std::move(settings));
            return;
        }
        case ViewAction::ToggleTheme:
            app.preferences.theme =
                app.preferences.theme == ThemeMode::Dark ? ThemeMode::Light : ThemeMode::Dark;
            app.theme = resolveTheme(app.preferences.theme);
            app.view.items[selected].subtitle =
                app.preferences.theme == ThemeMode::Dark ? "Dark" : "Light";
            clearTextCache();
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
        app.running = false;
        return;
    }
    app.view = std::move(app.history.back());
    app.history.pop_back();
}

void advanceWhenFinished(AppState& app) {
    app.playback.update();
    if (app.playback.snapshot().phase == PlaybackPhase::Error)
        app.message = app.playback.snapshot().errorMessage;
    else if (app.playback.snapshot().phase == PlaybackPhase::Loading ||
             app.playback.snapshot().phase == PlaybackPhase::Playing ||
             app.playback.snapshot().phase == PlaybackPhase::Paused)
        app.message.clear();
}
