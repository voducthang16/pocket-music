#include "app/navigation.hpp"

#include <algorithm>
#include <random>

std::vector<std::string> visibleLabels(const AppState& app) {
    switch (app.screen) {
        case Screen::Menu:
            return {"Songs", "Albums", "Artists", "Playlists", "Now Playing"};
        case Screen::Songs: {
            std::vector<std::string> out;
            for (const auto& t : app.library.tracks()) out.push_back(t.title);
            return out;
        }
        case Screen::Albums:
            return app.library.albums();
        case Screen::Artists:
            return app.library.artists();
        case Screen::Playlists: {
            std::vector<std::string> out;
            for (const auto& p : app.library.playlists()) out.push_back(p.name);
            return out;
        }
        case Screen::Filtered: {
            std::vector<std::string> out;
            for (size_t i : app.filtered) out.push_back(app.library.tracks()[i].title);
            return out;
        }
        case Screen::NowPlaying:
            return {};
    }
    return {};
}

std::string screenHeading(const AppState& app) {
    switch (app.screen) {
        case Screen::Menu:
            return "iPod";
        case Screen::Songs:
            return "Songs";
        case Screen::Albums:
            return "Albums";
        case Screen::Artists:
            return "Artists";
        case Screen::Playlists:
            return "Playlists";
        case Screen::Filtered:
            return app.filteredTitle;
        case Screen::NowPlaying:
            return "Now Playing";
    }
    return "iPod";
}

void playTrack(AppState& app, size_t index, int resume) {
    if (index >= app.library.tracks().size()) return;
    app.currentTrack = static_cast<int>(index);
    app.trackEnded = false;
    app.player.load(app.library.tracks()[index].path, resume);
    app.previous = app.screen;
    app.screen = Screen::NowPlaying;
    app.selected = app.scroll = 0;
}

void playAdjacentTrack(AppState& app, int direction) {
    if (app.library.tracks().empty()) return;
    int next = app.currentTrack;
    if (app.shuffle) {
        static std::mt19937 generator(std::random_device{}());
        next = std::uniform_int_distribution<int>(
            0, static_cast<int>(app.library.tracks().size() - 1))(generator);
    } else {
        next = (std::max(0, next) + direction + static_cast<int>(app.library.tracks().size())) %
               static_cast<int>(app.library.tracks().size());
    }
    playTrack(app, static_cast<size_t>(next));
}

void selectCurrentItem(AppState& app) {
    const auto items = visibleLabels(app);
    if (app.screen == Screen::NowPlaying || app.selected < 0 ||
        app.selected >= static_cast<int>(items.size()))
        return;
    switch (app.screen) {
        case Screen::Menu: {
            static const Screen screens[] = {Screen::Songs, Screen::Albums, Screen::Artists,
                                             Screen::Playlists, Screen::NowPlaying};
            if (app.selected == 4 && app.currentTrack < 0) return;
            app.screen = screens[app.selected];
            app.selected = app.scroll = 0;
            break;
        }
        case Screen::Songs:
            playTrack(app, static_cast<size_t>(app.selected));
            break;
        case Screen::Albums:
            app.filteredTitle = items[app.selected];
            app.filtered = app.library.byAlbum(app.filteredTitle);
            app.previous = Screen::Albums;
            app.screen = Screen::Filtered;
            app.selected = app.scroll = 0;
            break;
        case Screen::Artists:
            app.filteredTitle = items[app.selected];
            app.filtered = app.library.byArtist(app.filteredTitle);
            app.previous = Screen::Artists;
            app.screen = Screen::Filtered;
            app.selected = app.scroll = 0;
            break;
        case Screen::Playlists:
            app.filteredTitle = items[app.selected];
            app.filtered = app.library.fromPlaylist(app.library.playlists()[app.selected]);
            app.previous = Screen::Playlists;
            app.screen = Screen::Filtered;
            app.selected = app.scroll = 0;
            break;
        case Screen::Filtered:
            if (app.selected < static_cast<int>(app.filtered.size()))
                playTrack(app, app.filtered[app.selected]);
            break;
        case Screen::NowPlaying:
            break;
    }
}

void navigateBack(AppState& app) {
    if (app.screen == Screen::Menu)
        app.running = false;
    else if (app.screen == Screen::Filtered || app.screen == Screen::NowPlaying) {
        app.screen = app.previous;
        app.selected = app.scroll = 0;
    } else {
        app.screen = Screen::Menu;
        app.selected = app.scroll = 0;
    }
}

void advanceWhenFinished(AppState& app) {
    if (app.currentTrack < 0 || app.trackEnded || app.player.paused()) return;
    const auto& track = app.library.tracks()[app.currentTrack];
    if (track.durationSeconds <= 0 || app.player.elapsedSeconds() < track.durationSeconds) return;
    app.trackEnded = true;
    if (app.repeatMode == 1)
        playTrack(app, static_cast<size_t>(app.currentTrack));
    else if (app.currentTrack + 1 < static_cast<int>(app.library.tracks().size()))
        playAdjacentTrack(app, 1);
    else if (app.repeatMode == 2 && !app.library.tracks().empty())
        playTrack(app, 0);
}
