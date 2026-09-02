#include "app/navigation.hpp"

#include <algorithm>

namespace {
ViewItem trackItem(const Track& track, size_t index) {
    return {track.title, track.artist, ViewAction::None, index};
}

ViewState songsView(const AppState& app) {
    ViewState view{Screen::Songs, "Songs", {}, 0, 0};
    for (size_t index : app.library.allTrackIndexes())
        view.items.push_back(trackItem(app.library.tracks()[index], index));
    return view;
}

void pushView(AppState& app, ViewState next) {
    app.history.push_back(std::move(app.view));
    app.view = std::move(next);
}

ViewState homeView(const AppState& app) {
    ViewState view{Screen::Home, "Home", {}, 0, 0};
    view.items = {
        {"Songs", std::to_string(app.library.tracks().size()), ViewAction::OpenSongs, std::nullopt},
        {"Now Playing", "", ViewAction::OpenNowPlaying, std::nullopt},
        {"Liner Notes", "", ViewAction::OpenLinerNotes, std::nullopt}};
    return view;
}

void refreshHomeView(AppState& app) {
    const int selected = app.view.selected;
    app.view = homeView(app);
    app.view.selected = std::clamp(selected, 0, static_cast<int>(app.view.items.size()) - 1);
}
}  // namespace

void buildHomeView(AppState& app) {
    app.view = homeView(app);
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
            pushView(app, songsView(app));
            return;
        case ViewAction::OpenNowPlaying:
            openNowPlaying(app);
            return;
        case ViewAction::OpenLinerNotes:
            pushView(app, {Screen::LinerNotes, "Liner Notes", {}, 0, 0});
            return;
        case ViewAction::None:
            break;
    }
    if (app.view.screen == Screen::Songs) {
        if (item.trackIndex)
            playTrack(app, *item.trackIndex, app.library.allTrackIndexes(), selected);
    }
}

void navigateBack(AppState& app) {
    if (app.history.empty()) {
        if (app.view.screen != Screen::Home) {
            buildHomeView(app);
            return;
        }
        app.exitConfirmationOpen = true;
        app.exitConfirmationSelection = 0;
        return;
    }
    app.view = std::move(app.history.back());
    app.history.pop_back();
    if (app.view.screen == Screen::Home) refreshHomeView(app);
}

void advanceWhenFinished(AppState& app) {
    app.playback.update();
    const auto& playback = app.playback.snapshot();
    if (playback.phase == PlaybackPhase::Error)
        app.message = playback.errorMessage;
    else if (playback.phase == PlaybackPhase::Loading || playback.phase == PlaybackPhase::Playing ||
             playback.phase == PlaybackPhase::Paused)
        app.message.clear();
}
