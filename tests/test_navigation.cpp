#include <memory>

#include "app/app_state.hpp"
#include "app/navigation.hpp"
#include "fake_player.hpp"
#include "test_suites.hpp"
#include "ui/input.hpp"
namespace fs = std::filesystem;
namespace {
void navigationQueue() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Album" / "One.mp3");
    touch(music / "Album" / "Two.mp3");
    auto player = std::make_unique<FakePlayer>();
    auto* fake = player.get();
    AppState app(music, std::move(player));
    require(app.library.scan(), "navigation fixture must scan");
    buildLibraryView(app);
    selectCurrentItem(app);
    require(app.view.screen == Screen::Tracks && app.history.size() == 1,
            "Songs must push a track view");
    app.view.selected = 1;
    selectCurrentItem(app);
    require(app.view.screen == Screen::NowPlaying && app.history.size() == 2,
            "playing must push Now Playing without losing history");
    require(app.playback.queue().source().size() == 2 && app.playback.queue().current() == 1,
            "playback queue must match the selected track view");
    playAdjacentTrack(app, -1);
    require(app.playback.queue().current() == 0 && fake->loadCount == 2,
            "previous must move inside the active queue");
    navigateBack(app);
    require(app.view.screen == Screen::Tracks, "first Back must restore the track view");
    navigateBack(app);
    require(app.view.screen == Screen::Library, "second Back must restore Library");
}
void failedLoad() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    auto player = std::make_unique<FakePlayer>();
    player->loadSucceeds = false;
    AppState app(music, std::move(player));
    require(app.library.scan(), "failure fixture must scan");
    buildLibraryView(app);
    require(!playTrack(app, 0, app.library.allTrackIndexes()), "load failure must propagate");
    require(
        !app.playback.snapshot().trackIndex && app.playback.queue().empty() && app.history.empty(),
        "load failure must not mutate navigation or queue state");
    require(!app.message.empty(), "load failure must be visible to the UI state");
}

void trimuiFaceButtonMapping() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "controller fixture must scan");
    buildLibraryView(app);

    handleControllerButton(app, SDL_CONTROLLER_BUTTON_B);
    require(app.view.screen == Screen::Tracks, "TrimUI A button must select the current item");

    handleControllerButton(app, SDL_CONTROLLER_BUTTON_A);
    require(app.view.screen == Screen::Library, "TrimUI B button must navigate back");
}

void trimuiSelectCyclesRepeatMode() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());

    handleControllerButton(app, SDL_CONTROLLER_BUTTON_BACK);
    require(app.playback.repeatMode() == RepeatMode::One, "TrimUI Select must enable repeat one");
    handleControllerButton(app, SDL_CONTROLLER_BUTTON_BACK);
    require(app.playback.repeatMode() == RepeatMode::All,
            "TrimUI Select must advance to repeat all");
    handleControllerButton(app, SDL_CONTROLLER_BUTTON_BACK);
    require(app.playback.repeatMode() == RepeatMode::Off,
            "TrimUI Select must cycle repeat back to off");
}

void restoredNowPlayingBackReturnsToLibrary() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    app.view = nowPlayingView();

    navigateBack(app);

    require(app.running, "Back from restored Now Playing must keep the app running");
    require(app.view.screen == Screen::Library,
            "Back from restored Now Playing must return to Library");
    require(app.history.empty(), "restored fallback must not create synthetic history");

    navigateBack(app);
    require(!app.running, "Back from root Library must still exit the app");
}

void libraryNowPlayingStatusRefreshesAfterPlaybackStarts() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "navigation fixture must scan");
    buildLibraryView(app);
    require(app.view.items[4].subtitle == "Nothing playing",
            "Library must begin without a current track");

    selectCurrentItem(app);
    selectCurrentItem(app);
    navigateBack(app);
    navigateBack(app);

    require(app.view.screen == Screen::Library,
            "Back from a playing track must eventually restore Library");
    require(app.view.items[4].subtitle == "Open current track",
            "Library must refresh the Now Playing status after playback starts");
}

void openingNowPlayingUsesOneNavigationPath() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "navigation fixture must scan");
    buildLibraryView(app);
    require(app.playback.play(0, app.library.allTrackIndexes()), "track load must begin");

    openNowPlaying(app);
    require(app.view.screen == Screen::NowPlaying && app.history.size() == 1,
            "opening Now Playing must preserve the source view once");
    openNowPlaying(app);
    require(app.history.size() == 1, "opening Now Playing twice must not duplicate history");
}
}  // namespace
void addNavigationTests(TestCases& tests) {
    tests.emplace_back("navigation queue", navigationQueue);
    tests.emplace_back("failed load", failedLoad);
    tests.emplace_back("TrimUI face button mapping", trimuiFaceButtonMapping);
    tests.emplace_back("TrimUI Select repeat mapping", trimuiSelectCyclesRepeatMode);
    tests.emplace_back("restored Now Playing back returns to Library",
                       restoredNowPlayingBackReturnsToLibrary);
    tests.emplace_back("Library Now Playing status refreshes",
                       libraryNowPlayingStatusRefreshesAfterPlaybackStarts);
    tests.emplace_back("Now Playing uses one navigation path",
                       openingNowPlayingUsesOneNavigationPath);
}
