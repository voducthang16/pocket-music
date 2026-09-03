#include <cstdlib>
#include <fstream>
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
    buildHomeView(app);
    selectCurrentItem(app);
    require(app.view.screen == Screen::Songs && app.history.size() == 1,
            "Songs must push the Songs view");
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
    require(app.view.screen == Screen::Songs, "first Back must restore Songs");
    navigateBack(app);
    require(app.view.screen == Screen::Home, "second Back must restore Home");
}
void failedLoad() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    auto player = std::make_unique<FakePlayer>();
    player->loadSucceeds = false;
    AppState app(music, std::move(player));
    require(app.library.scan(), "failure fixture must scan");
    buildHomeView(app);
    require(!playTrack(app, 0, app.library.allTrackIndexes()), "load failure must propagate");
    require(
        !app.playback.snapshot().trackIndex && app.playback.queue().empty() && app.history.empty(),
        "load failure must not mutate navigation or queue state");
    require(!app.message.empty(), "load failure must be visible to the UI state");
}

void homeRowsExposePrimaryDestinations() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Album" / "One.mp3");
    touch(music / "Album" / "Two.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "Home fixture must scan");
    unsetenv("POCKET_MUSIC_DATA_DIR");

    buildHomeView(app);

    require(app.view.items[0].subtitle == "2", "Songs must expose a compact numeric count");
    require(app.view.items.size() == 4, "Home must contain the update check destination");
    require(app.view.items[1].title == "Now Playing" && app.view.items[1].subtitle.empty(),
            "Now Playing must be a single-line destination");
    require(app.view.items[2].title == "Liner Notes" && app.view.items[2].subtitle.empty(),
            "Liner Notes must be a single-line destination");
    require(app.view.items[3].title == "Check for Updates" && app.view.items[3].subtitle.empty(),
            "Home must expose remote update checks");
}

void updateCheckRequestsLauncherWork() {
    TemporaryDirectory temporary;
    const auto dataDir = temporary.path / "data";
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    buildHomeView(app);

    setenv("POCKET_MUSIC_DATA_DIR", dataDir.c_str(), 1);
    require(requestUpdateCheck(app), "TrimUI update check must create a launcher request");
    unsetenv("POCKET_MUSIC_DATA_DIR");

    require(!app.running, "update checks must shut the app down cleanly before network work");
    require(fs::exists(dataDir / "update" / "check-requested"),
            "update check request marker must be persisted");
}

void pendingUpdateRequiresExplicitInstallAction() {
    TemporaryDirectory temporary;
    const auto dataDir = temporary.path / "data";
    fs::create_directories(dataDir / "update");
    {
        std::ofstream pending(dataDir / "update" / "pending-update");
        pending << "version=0.2.0\nasset=test.tar.gz\nsha256=abc\n";
    }

    setenv("POCKET_MUSIC_DATA_DIR", dataDir.c_str(), 1);
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    buildHomeView(app);

    require(app.view.items.size() == 5, "pending update must add an explicit install destination");
    require(app.view.items[4].title == "Install Update" && app.view.items[4].subtitle == "v0.2.0",
            "pending update must show the version before installation");
    require(requestUpdateInstall(app), "install action must create a launcher request");
    unsetenv("POCKET_MUSIC_DATA_DIR");

    require(!app.running, "install action must shut the app down before replacing files");
    require(fs::exists(dataDir / "update" / "install-requested"),
            "install request marker must be persisted");
}

void updateInstallRequiresPendingUpdate() {
    TemporaryDirectory temporary;
    const auto dataDir = temporary.path / "data";
    setenv("POCKET_MUSIC_DATA_DIR", dataDir.c_str(), 1);
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());

    require(!requestUpdateInstall(app), "install must refuse when no verified update is pending");
    unsetenv("POCKET_MUSIC_DATA_DIR");
    require(app.running, "refused install must keep the app running");
    require(!app.message.empty(), "refused install must explain why it cannot proceed");
}

void updateCheckIsTrimuiOnly() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    unsetenv("POCKET_MUSIC_DATA_DIR");

    require(!requestUpdateCheck(app), "desktop update check must not attempt TrimUI update flow");
    require(app.running, "unsupported update check must keep the app running");
    require(!app.message.empty(), "unsupported update check must explain why it is unavailable");
}

void trimuiFaceButtonMapping() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "controller fixture must scan");
    buildHomeView(app);

    handleControllerButton(app, SDL_CONTROLLER_BUTTON_B);
    require(app.view.screen == Screen::Songs, "TrimUI A button must select Songs");

    handleControllerButton(app, SDL_CONTROLLER_BUTTON_A);
    require(app.view.screen == Screen::Home, "TrimUI B button must navigate back");

    require(app.playback.play(0, app.library.allTrackIndexes()),
            "controller fixture must start playback");
    handleControllerButton(app, SDL_CONTROLLER_BUTTON_X);
    require(app.playback.shuffle(), "TrimUI Y button must toggle shuffle");
    require(app.view.screen == Screen::Home, "TrimUI Y button must not open Now Playing");

    handleControllerButton(app, SDL_CONTROLLER_BUTTON_Y);
    require(app.view.screen == Screen::NowPlaying, "TrimUI X button must open Now Playing");
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

void restoredNowPlayingBackReturnsToHome() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    app.view = nowPlayingView();

    navigateBack(app);

    require(app.running, "Back from restored Now Playing must keep the app running");
    require(app.view.screen == Screen::Home, "Back from restored Now Playing must return to Home");
    require(app.history.empty(), "restored fallback must not create synthetic history");

    navigateBack(app);
    require(app.running && app.exitConfirmationOpen,
            "Back from Home must request confirmation without exiting");
}

void homeExitRequiresConfirmation() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    buildHomeView(app);

    handleKey(app, SDLK_b);
    require(app.running && app.exitConfirmationOpen && app.exitConfirmationSelection == 0,
            "Home Back must open exit confirmation on Stay");

    handleKey(app, SDLK_RETURN);
    require(app.running && !app.exitConfirmationOpen,
            "confirming Stay must close the dialog and keep running");

    handleKey(app, SDLK_b);
    handleKey(app, SDLK_RIGHT);
    handleKey(app, SDLK_RETURN);
    require(!app.running, "confirming Exit must stop the app");
}

void linerNotesReturnsToHome() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    buildHomeView(app);
    app.view.selected = 2;

    selectCurrentItem(app);
    require(app.view.screen == Screen::LinerNotes, "Home must open Liner Notes");

    navigateBack(app);
    require(app.view.screen == Screen::Home, "Back from Liner Notes must restore Home");
}

void openingNowPlayingUsesOneNavigationPath() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "navigation fixture must scan");
    buildHomeView(app);
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
    tests.emplace_back("Home primary destinations", homeRowsExposePrimaryDestinations);
    tests.emplace_back("update check requests launcher work", updateCheckRequestsLauncherWork);
    tests.emplace_back("pending update requires explicit install",
                       pendingUpdateRequiresExplicitInstallAction);
    tests.emplace_back("update install requires pending update", updateInstallRequiresPendingUpdate);
    tests.emplace_back("update check is TrimUI only", updateCheckIsTrimuiOnly);
    tests.emplace_back("TrimUI face button mapping", trimuiFaceButtonMapping);
    tests.emplace_back("TrimUI Select repeat mapping", trimuiSelectCyclesRepeatMode);
    tests.emplace_back("restored Now Playing back returns to Home",
                       restoredNowPlayingBackReturnsToHome);
    tests.emplace_back("Home exit requires confirmation", homeExitRequiresConfirmation);
    tests.emplace_back("Liner Notes returns to Home", linerNotesReturnsToHome);
    tests.emplace_back("Now Playing uses one navigation path",
                       openingNowPlayingUsesOneNavigationPath);
}
