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

void themeSettingsUseNavigationStack() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "settings fixture must scan");
    buildLibraryView(app);
    app.view.selected = 5;
    selectCurrentItem(app);
    require(app.view.screen == Screen::Settings && app.history.size() == 1,
            "Settings must open through the navigation stack");
    require(app.view.items.front().subtitle == "Dark", "Settings must show the active theme");
    selectCurrentItem(app);
    require(app.preferences.theme == ThemeMode::Light,
            "selecting Theme must update preferences immediately");
    require(app.view.items.front().subtitle == "Light",
            "theme value must update without reopening Settings");
    navigateBack(app);
    require(app.view.screen == Screen::Library && app.view.selected == 5,
            "Back must restore the selected Settings row in Library");
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
}  // namespace
void addNavigationTests(TestCases& tests) {
    tests.emplace_back("navigation queue", navigationQueue);
    tests.emplace_back("failed load", failedLoad);
    tests.emplace_back("theme settings", themeSettingsUseNavigationStack);
    tests.emplace_back("TrimUI face button mapping", trimuiFaceButtonMapping);
}
