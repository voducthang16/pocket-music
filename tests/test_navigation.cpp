#include <chrono>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <thread>

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

void updateCheckRunsInsideApp() {
    TemporaryDirectory temporary;
    const auto appDir = temporary.path / "PocketMusic";
    const auto dataDir = appDir / "data";
    const auto checker = appDir / "update" / "check-update.sh";
    fs::create_directories(checker.parent_path());
    {
        std::ofstream script(checker);
        script << "#!/bin/sh\n"
                  "mkdir -p \"$3/update\"\n"
                  "printf 'phase=downloading\\nversion=0.2.5\\n' > \"$3/update/check-phase\"\n"
                  "sleep 0.05\n"
                  "printf 'phase=verifying\\nversion=0.2.5\\n' > \"$3/update/check-phase\"\n"
                  "printf 'version=0.2.5\\nasset=test.tar.gz\\nsha256=abc\\n' > "
                  "\"$3/update/pending-update\"\n"
                  "exit 10\n";
    }
    fs::permissions(checker,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);

    setenv("POCKET_MUSIC_APP_DIR", appDir.c_str(), 1);
    setenv("POCKET_MUSIC_DATA_DIR", dataDir.c_str(), 1);
    setenv("POCKET_MUSIC_UPDATE_CHECKER", checker.c_str(), 1);

    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    buildHomeView(app);
    app.view.selected = 3;

    require(requestUpdateCheck(app), "TrimUI update check must start in the background");
    require(app.running, "update checks must keep the app running during network work");
    require(app.update.checking() && app.update.processId > 0,
            "update check must expose an active process state");

    handleKey(app, SDLK_DOWN);
    require(app.view.selected == 3, "update loading modal must block duplicate navigation input");

    for (int attempt = 0; attempt < 100 && app.update.checking(); ++attempt) {
        pollUpdateCheck(app);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    pollUpdateCheck(app);

    require(app.running, "completed update check must leave Pocket Music open");
    require(!app.update.checking() && app.update.phase == UpdatePhase::Ready,
            "ready checker exit must become an in-app ready state");
    require(app.update.version == "0.2.5", "ready state must expose the update version");
    require(app.view.items.size() == 5 && app.view.items[4].title == "Install Update" &&
                app.view.items[4].subtitle == "v0.2.5",
            "completed check must refresh Home with the install action");

    cancelUpdateCheck(app);
    unsetenv("POCKET_MUSIC_UPDATE_CHECKER");
    unsetenv("POCKET_MUSIC_DATA_DIR");
    unsetenv("POCKET_MUSIC_APP_DIR");
}

void pendingUpdateRequiresExplicitInstallAction() {
    TemporaryDirectory temporary;
    const auto dataDir = temporary.path / "data";
    fs::create_directories(dataDir / "update");
    {
        std::ofstream pending(dataDir / "update" / "pending-update");
        pending << "version=0.2.5\nasset=test.tar.gz\nsha256=abc\n";
    }

    setenv("POCKET_MUSIC_DATA_DIR", dataDir.c_str(), 1);
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    buildHomeView(app);

    require(app.view.items.size() == 5, "pending update must add an explicit install destination");
    require(app.view.items[4].title == "Install Update" && app.view.items[4].subtitle == "v0.2.5",
            "pending update must show the version before installation");
    app.view.selected = 4;
    require(requestUpdateInstall(app), "install action must create a launcher request");
    require(app.running, "install request must stay alive until the install modal is presented");
    require(app.update.phase == UpdatePhase::PreparingInstall && app.update.modalVisible(),
            "install request must enter the modal handoff state");
    require(app.update.version == "0.2.5", "install handoff must preserve the pending version");
    require(fs::exists(dataDir / "update" / "install-requested"),
            "install request marker must be persisted before shutdown");

    handleKey(app, SDLK_DOWN);
    require(app.view.selected == 4, "install handoff modal must block navigation input");

    finishDeferredUpdateHandoff(app);
    unsetenv("POCKET_MUSIC_DATA_DIR");
    require(!app.running, "install handoff must stop the app only after the render boundary");
}

void updateInstallRequiresPendingUpdate() {
    TemporaryDirectory temporary;
    const auto dataDir = temporary.path / "data";
    setenv("POCKET_MUSIC_DATA_DIR", dataDir.c_str(), 1);
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());

    require(!requestUpdateInstall(app), "install must refuse when no verified update is pending");
    unsetenv("POCKET_MUSIC_DATA_DIR");
    require(app.running, "refused install must keep the app running");
    require(app.update.phase == UpdatePhase::Error && !app.update.detail.empty(),
            "refused install must explain failure through update state");
    require(app.message.empty(), "update failures must not share the playback message channel");
}

void updateStatusIsIndependentFromPlaybackMessages() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    app.update.phase = UpdatePhase::Result;
    app.update.detail = "Pocket Music updated to 0.2.5";
    app.message = "temporary playback message";

    advanceWhenFinished(app);

    require(app.update.phase == UpdatePhase::Result,
            "playback updates must not mutate the updater lifecycle");
    require(app.update.detail == "Pocket Music updated to 0.2.5",
            "playback updates must not clear updater results");
}

void updateCheckIsTrimuiOnly() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    unsetenv("POCKET_MUSIC_APP_DIR");
    unsetenv("POCKET_MUSIC_DATA_DIR");
    unsetenv("POCKET_MUSIC_UPDATE_CHECKER");

    require(!requestUpdateCheck(app), "desktop update check must not attempt TrimUI update flow");
    require(app.running, "unsupported update check must keep the app running");
    require(app.update.phase == UpdatePhase::Error && !app.update.detail.empty(),
            "unsupported update check must report through update state");
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
    tests.emplace_back("update check runs inside app", updateCheckRunsInsideApp);
    tests.emplace_back("pending update requires explicit install",
                       pendingUpdateRequiresExplicitInstallAction);
    tests.emplace_back("update install requires pending update", updateInstallRequiresPendingUpdate);
    tests.emplace_back("update status is independent from playback",
                       updateStatusIsIndependentFromPlaybackMessages);
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
