#include "app/navigation.hpp"

#include <algorithm>
#include <numeric>

namespace {
std::vector<size_t> trackIndexSequence(size_t count) {
    std::vector<size_t> indexes(count);
    std::iota(indexes.begin(), indexes.end(), 0);
    return indexes;
}

ViewState songsView(const AppState& app) {
    return {Screen::Songs, "Songs", TrackListView{trackIndexSequence(app.library.tracks().size())},
            0, 0};
}

void pushView(AppState& app, ViewState next) {
    app.history.push_back(std::move(app.view));
    app.view = std::move(next);
}

ViewState homeView(const AppState& app) {
    MenuView menu{{
        {"Songs", std::to_string(app.library.tracks().size()), NavigationIntent::OpenSongs},
        {"Now Playing", "", NavigationIntent::OpenNowPlaying},
        {"About", "", NavigationIntent::OpenAbout},
        {"Check for Updates", "", NavigationIntent::CheckForUpdates},
    }};
    if (const auto version = app.updates.pendingVersion())
        menu.items.push_back({"Install Update", "v" + *version, NavigationIntent::InstallUpdate});
    return {Screen::Home, "Home", std::move(menu), 0, 0};
}
}  // namespace

void buildHomeView(AppState& app) {
    app.view = homeView(app);
    app.history.clear();
}

void refreshHomeView(AppState& app) {
    if (app.view.screen != Screen::Home) return;
    const int selected = app.view.selected;
    app.view = homeView(app);
    app.view.selected = std::clamp(selected, 0, static_cast<int>(app.view.itemCount()) - 1);
}

ViewState nowPlayingView() { return {Screen::NowPlaying, "Now Playing", std::monostate{}, 0, 0}; }

void openNowPlaying(AppState& app) {
    if (app.view.screen == Screen::NowPlaying || !app.playback.displayTrackIndex()) return;
    pushView(app, nowPlayingView());
}

bool playTrack(AppState& app, size_t trackIndex, const std::vector<size_t>& queue,
               std::optional<size_t> sourcePosition) {
    if (!app.playback.play(trackIndex, queue, sourcePosition, app.view.title)) {
        app.notice = AppNotice{NoticeSource::Playback, app.playback.snapshot().errorMessage};
        return false;
    }
    app.notice.reset();
    openNowPlaying(app);
    return true;
}

void playAdjacentTrack(AppState& app, int direction) {
    if (direction > 0)
        app.playback.next();
    else
        app.playback.previous();
}

void finishDeferredUpdateHandoff(AppState& app) {
    if (app.updates.state().preparingInstall()) app.running = false;
}

void selectCurrentItem(AppState& app) {
    if (app.view.selected < 0 || app.view.selected >= static_cast<int>(app.view.itemCount()))
        return;
    const auto selected = static_cast<size_t>(app.view.selected);
    if (const auto* tracks = std::get_if<TrackListView>(&app.view.content)) {
        const size_t trackIndex = tracks->trackIndexes[selected];
        playTrack(app, trackIndex, tracks->trackIndexes, selected);
        return;
    }
    const auto* menu = std::get_if<MenuView>(&app.view.content);
    if (!menu) return;
    switch (menu->items[selected].intent) {
        case NavigationIntent::OpenSongs:
            pushView(app, songsView(app));
            return;
        case NavigationIntent::OpenNowPlaying:
            openNowPlaying(app);
            return;
        case NavigationIntent::OpenAbout:
            pushView(app, {Screen::About, "About", std::monostate{}, 0, 0});
            return;
        case NavigationIntent::CheckForUpdates:
            app.notice.reset();
            app.updates.check();
            return;
        case NavigationIntent::InstallUpdate:
            app.notice.reset();
            app.updates.requestInstall();
            return;
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
    if (playback.phase == PlaybackPhase::Error) {
        if (!app.notice || app.notice->source == NoticeSource::Playback)
            app.notice = AppNotice{NoticeSource::Playback, playback.errorMessage};
    } else if ((playback.phase == PlaybackPhase::Loading ||
                playback.phase == PlaybackPhase::Playing ||
                playback.phase == PlaybackPhase::Paused) &&
               app.notice && app.notice->source == NoticeSource::Playback) {
        app.notice.reset();
    }
}
