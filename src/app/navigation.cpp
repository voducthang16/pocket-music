#include "app/navigation.hpp"

#include <algorithm>
#include <random>

namespace {
std::string countLabel(size_t count, const char* singular, const char* plural) {
    return std::to_string(count) + ' ' + (count == 1 ? singular : plural);
}

ViewItem trackItem(const Track& track, size_t index) {
    return {track.title, track.artist, {index}};
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
        view.items.push_back({group.title, group.subtitle, group.trackIndexes});
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
        {"Songs", countLabel(app.library.tracks().size(), "track", "tracks"),
         app.library.allTrackIndexes()},
        {"Albums", countLabel(app.library.albums().size(), "album", "albums"), {}},
        {"Artists", countLabel(app.library.artists().size(), "artist", "artists"), {}},
        {"Playlists", countLabel(app.library.playlists().size(), "playlist", "playlists"), {}},
        {"Now Playing", app.currentTrack >= 0 ? "Open current track" : "Nothing playing", {}}};
    app.history.clear();
}

bool playTrack(AppState& app, size_t trackIndex, const std::vector<size_t>& queue, int resume) {
    if (trackIndex >= app.library.tracks().size() || queue.empty()) return false;
    const auto position = std::find(queue.begin(), queue.end(), trackIndex);
    if (position == queue.end()) return false;
    if (!app.player->load(app.library.tracks()[trackIndex].path, resume)) {
        app.message = app.player->error();
        return false;
    }
    app.queue = queue;
    app.queuePosition = static_cast<size_t>(position - queue.begin());
    app.currentTrack = static_cast<int>(trackIndex);
    app.message.clear();
    if (app.view.screen != Screen::NowPlaying)
        pushView(app, {Screen::NowPlaying, "Now Playing", "POCKET MUSIC", {}, 0, 0});
    return true;
}

void playAdjacentTrack(AppState& app, int direction) {
    if (app.queue.empty()) return;
    size_t next = app.queuePosition;
    if (app.shuffle && app.queue.size() > 1) {
        static std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<size_t> distribution(0, app.queue.size() - 2);
        next = distribution(generator);
        if (next >= app.queuePosition) ++next;
    } else if (direction > 0) {
        if (next + 1 >= app.queue.size()) {
            if (app.repeatMode != 2) return;
            next = 0;
        } else {
            ++next;
        }
    } else if (next == 0) {
        next = app.repeatMode == 2 ? app.queue.size() - 1 : 0;
    } else {
        --next;
    }
    playTrack(app, app.queue[next], app.queue);
}

void selectCurrentItem(AppState& app) {
    if (app.view.screen == Screen::NowPlaying || app.view.selected < 0 ||
        app.view.selected >= static_cast<int>(app.view.items.size()))
        return;
    const auto selected = static_cast<size_t>(app.view.selected);
    if (app.view.screen == Screen::Library) {
        switch (selected) {
            case 0:
                pushView(app, tracksView(app, "Songs", app.library.allTrackIndexes()));
                break;
            case 1:
                showGroups(app, Screen::Albums, "Albums", app.library.albums());
                break;
            case 2:
                showGroups(app, Screen::Artists, "Artists", app.library.artists());
                break;
            case 3: {
                ViewState view{Screen::Playlists, "Playlists", "LIBRARY", {}, 0, 0};
                for (const auto& playlist : app.library.playlists())
                    view.items.push_back(
                        {playlist.name, countLabel(playlist.trackIndexes.size(), "track", "tracks"),
                         playlist.trackIndexes});
                pushView(app, std::move(view));
                break;
            }
            case 4:
                if (app.currentTrack >= 0)
                    pushView(app, {Screen::NowPlaying, "Now Playing", "POCKET MUSIC", {}, 0, 0});
                break;
        }
        return;
    }
    const auto& item = app.view.items[selected];
    if (app.view.screen == Screen::Tracks) {
        std::vector<size_t> queue;
        queue.reserve(app.view.items.size());
        for (const auto& row : app.view.items)
            if (!row.trackIndexes.empty()) queue.push_back(row.trackIndexes.front());
        playTrack(app, item.trackIndexes.front(), queue);
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
    if (!app.player->consumeEnded() || app.currentTrack < 0) return;
    if (app.repeatMode == 1)
        playTrack(app, static_cast<size_t>(app.currentTrack), app.queue);
    else
        playAdjacentTrack(app, 1);
}
