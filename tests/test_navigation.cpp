#include <chrono>
#include <fstream>
#include <memory>
#include <thread>

#include "app/app_state.hpp"
#include "app/navigation.hpp"
#include "fake_player.hpp"
#include "test_suites.hpp"
#include "ui/input.hpp"
#include "ui/presentation.hpp"
namespace fs = std::filesystem;
namespace {
UpdateRuntimePaths updatePaths(const fs::path& appDir, const fs::path& dataDir,
                               const fs::path& preparer) {
    return {appDir, dataDir, preparer};
}

void makeExecutable(const fs::path& path) {
    fs::permissions(path, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
}

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
    require(app.notice && app.notice->source == NoticeSource::Playback && !app.notice->text.empty(),
            "load failure must be visible through the application notice model");
}

void homeRowsExposePrimaryDestinations() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Album" / "One.mp3");
    touch(music / "Album" / "Two.mp3");
    AppState app(music, std::make_unique<FakePlayer>());
    require(app.library.scan(), "Home fixture must scan");

    buildHomeView(app);

    require(app.view.items[0].subtitle == "2", "Songs must expose a compact numeric count");
    require(app.view.items.size() == 4, "Home must contain the update check destination");
    require(app.view.items[1].title == "Now Playing" && app.view.items[1].subtitle.empty(),
            "Now Playing must be a single-line destination");
    require(app.view.items[2].title == "About" && app.view.items[2].subtitle.empty(),
            "About must be a single-line destination");
    require(app.view.items[3].title == "Check for Updates" && app.view.items[3].subtitle.empty(),
            "Home must expose remote update checks");
}

void presentationMappingsAreSemantic() {
    ViewItem install{"Install Update", "v0.2.7", ViewAction::InstallUpdate, std::nullopt};
    const auto installRow = homeRowPresentation(install);
    require(installRow.trailing == "v0.2.7" && !installRow.chevron,
            "Home rows with details must render the actual item detail instead of a chevron");

    ViewItem destination{"About", "", ViewAction::OpenAbout, std::nullopt};
    require(homeRowPresentation(destination).chevron,
            "Home destinations without details must render a chevron");

    const auto loading = playbackPresentation(PlaybackPhase::Loading);
    const auto playing = playbackPresentation(PlaybackPhase::Playing);
    const auto paused = playbackPresentation(PlaybackPhase::Paused);
    const auto finished = playbackPresentation(PlaybackPhase::Finished);
    const auto error = playbackPresentation(PlaybackPhase::Error);
    require(loading.status == "LOADING" && loading.primaryAction.empty(),
            "loading must not look like active playback");
    require(
        playing.status == "PLAYING" && playing.primaryAction == "PAUSE" && playing.showPauseIcon,
        "playing must expose the pause action");
    require(paused.status == "PAUSED" && paused.primaryAction == "PLAY" && !paused.showPauseIcon,
            "paused must expose the play action");
    require(finished.status == "FINISHED" && finished.primaryAction.empty(),
            "finished playback must not imply an active transport action");
    require(error.status == "PLAYBACK ERROR" && error.primaryAction == "RETRY",
            "playback errors must expose Retry instead of Pause");
}

void updateCheckRunsInsideApp() {
    TemporaryDirectory temporary;
    const auto appDir = temporary.path / "PocketMusic";
    const auto dataDir = appDir / "data";
    const auto preparer = appDir / "update" / "prepare-update.sh";
    fs::create_directories(preparer.parent_path());
    {
        std::ofstream script(preparer);
        script << "#!/bin/sh\n"
                  "mkdir -p \"$3/update\"\n"
                  "printf 'phase=downloading\\nversion=0.2.5\\n' > \"$3/update/check-phase\"\n"
                  "sleep 0.05\n"
                  "printf 'phase=verifying\\nversion=0.2.5\\n' > \"$3/update/check-phase\"\n"
                  "printf 'version=0.2.5\\nasset=test.tar.gz\\nsha256=abc\\nsize=123\\n' > "
                  "\"$3/update/pending-update\"\n"
                  "exit 10\n";
    }
    makeExecutable(preparer);

    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>(),
                 updatePaths(appDir, dataDir, preparer));
    buildHomeView(app);
    app.view.selected = 3;

    require(app.updates.check(), "TrimUI update check must start in the background");
    require(app.running, "update checks must keep the app running during network work");
    require(app.updates.state().checking(),
            "update check must expose semantic active state without a process id");

    handleKey(app, SDLK_DOWN);
    require(app.view.selected == 3, "update loading modal must block duplicate navigation input");

    for (int attempt = 0; attempt < 100 && app.updates.state().checking(); ++attempt) {
        if (app.updates.poll()) refreshHomeView(app);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (app.updates.poll()) refreshHomeView(app);

    require(app.running, "completed update check must leave Pocket Music open");
    require(app.updates.state().phase == UpdatePhase::Idle,
            "completed update checks must return the lifecycle state to idle");
    const auto notice = app.updates.takeNotice();
    require(notice && *notice == "v0.2.5 is ready to install",
            "ready update must be emitted as a transient notice");
    require(app.view.items.size() == 5 && app.view.items[4].title == "Install Update" &&
                app.view.items[4].subtitle == "v0.2.5",
            "completed check must refresh Home from the pending manifest");
    const auto installRow = homeRowPresentation(app.view.items[4]);
    require(installRow.trailing == "v0.2.5" && !installRow.chevron,
            "Install Update must present its pending version as trailing text");
}

void updateCancellationIsNonBlocking() {
    TemporaryDirectory temporary;
    const auto appDir = temporary.path / "PocketMusic";
    const auto dataDir = appDir / "data";
    const auto preparer = appDir / "update" / "prepare-update.sh";
    fs::create_directories(preparer.parent_path());
    {
        std::ofstream script(preparer);
        script << "#!/bin/sh\n"
                  "trap '' TERM\n"
                  "mkdir -p \"$3/update\"\n"
                  "printf 'phase=checking\\n' > \"$3/update/check-phase\"\n"
                  "while :; do sleep 1; done\n";
    }
    makeExecutable(preparer);

    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>(),
                 updatePaths(appDir, dataDir, preparer));
    buildHomeView(app);
    app.view.selected = 3;
    require(app.updates.check(), "cancellation fixture must start preparer");

    const auto started = std::chrono::steady_clock::now();
    handleKey(app, SDLK_b);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    require(elapsed < std::chrono::milliseconds(200),
            "cancel input must not block waiting for the update preparer");
    require(app.updates.state().cancelling() && !app.updates.state().cancellable(),
            "cancel must enter a non-cancellable semantic cancelling state");

    for (int attempt = 0; attempt < 150 && app.updates.state().cancelling(); ++attempt) {
        app.updates.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    app.updates.poll();
    require(app.updates.state().phase == UpdatePhase::Idle,
            "poll must reap a cancelled preparer and return to idle");
    require(!fs::exists(dataDir / "update" / "check-phase"),
            "cancelled preparer phase file must be cleared after reap");
}

void pendingUpdateRequiresExplicitInstallAction() {
    TemporaryDirectory temporary;
    const auto appDir = temporary.path / "PocketMusic";
    const auto dataDir = appDir / "data";
    const auto preparer = appDir / "update" / "prepare-update.sh";
    fs::create_directories(dataDir / "update");
    {
        std::ofstream pending(dataDir / "update" / "pending-update");
        pending << "version=0.2.5\nasset=test.tar.gz\nsha256=abc\nsize=123\n";
    }

    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>(),
                 updatePaths(appDir, dataDir, preparer));
    buildHomeView(app);

    require(app.view.items.size() == 5, "pending update must add an explicit install destination");
    require(app.view.items[4].title == "Install Update" && app.view.items[4].subtitle == "v0.2.5",
            "pending update must show the version before installation");
    app.view.selected = 4;
    require(app.updates.requestInstall(), "install action must create a launcher request");
    require(app.running, "install request must stay alive until the install modal is presented");
    const auto& update = app.updates.state();
    require(update.phase == UpdatePhase::PreparingInstall && update.modalVisible(),
            "install request must enter the modal handoff state");
    require(update.version == "0.2.5", "install handoff must preserve the pending version");
    require(fs::exists(dataDir / "update" / "install-requested"),
            "install request marker must be persisted before shutdown");

    handleKey(app, SDLK_DOWN);
    require(app.view.selected == 4, "install handoff modal must block navigation input");

    finishDeferredUpdateHandoff(app);
    require(!app.running, "install handoff must stop the app only after the render boundary");
}

void updateInstallRequiresPendingUpdate() {
    TemporaryDirectory temporary;
    const auto appDir = temporary.path / "PocketMusic";
    const auto dataDir = appDir / "data";
    const auto preparer = appDir / "update" / "prepare-update.sh";
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>(),
                 updatePaths(appDir, dataDir, preparer));

    require(!app.updates.requestInstall(), "install must refuse when no verified update is pending");
    require(app.running, "refused install must keep the app running");
    require(app.updates.state().phase == UpdatePhase::Idle,
            "refused install must not invent a persistent updater error state");
    const auto notice = app.updates.takeNotice();
    require(notice && !notice->empty(), "refused install must emit an application notice");
}

void updateStatusUsesNoticeChannel() {
    TemporaryDirectory temporary;
    const auto appDir = temporary.path / "PocketMusic";
    const auto dataDir = appDir / "data";
    const auto preparer = appDir / "update" / "prepare-update.sh";
    fs::create_directories(dataDir / "update");
    std::ofstream(dataDir / "update" / "last-status") << "Pocket Music updated to 0.2.5\n";

    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>(),
                 updatePaths(appDir, dataDir, preparer));
    require(app.updates.consumeLastStatus(), "update status fixture must be consumed");
    const auto updateNotice = app.updates.takeNotice();
    require(updateNotice && *updateNotice == "Pocket Music updated to 0.2.5",
            "launcher result must be emitted by the update subsystem");
    app.notice = AppNotice{NoticeSource::Update, *updateNotice};

    advanceWhenFinished(app);

    require(app.notice && app.notice->source == NoticeSource::Update &&
                app.notice->text == "Pocket Music updated to 0.2.5",
            "playback polling must not overwrite an unrelated update notice");
}

void updateCheckIsTrimuiOnly() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());

    require(!app.updates.check(), "desktop update check must not attempt TrimUI update flow");
    require(app.running, "unsupported update check must keep the app running");
    require(app.updates.state().phase == UpdatePhase::Idle,
            "unsupported update check must leave the updater lifecycle idle");
    const auto notice = app.updates.takeNotice();
    require(notice && !notice->empty(), "unsupported update check must emit a user notice");
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

void aboutReturnsToHome() {
    TemporaryDirectory temporary;
    AppState app(temporary.path / "Music", std::make_unique<FakePlayer>());
    buildHomeView(app);
    app.view.selected = 2;

    selectCurrentItem(app);
    require(app.view.screen == Screen::About, "Home must open About");

    navigateBack(app);
    require(app.view.screen == Screen::Home, "Back from About must restore Home");
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
    tests.emplace_back("semantic presentation mappings", presentationMappingsAreSemantic);
    tests.emplace_back("update check runs inside app", updateCheckRunsInsideApp);
    tests.emplace_back("update cancellation is nonblocking", updateCancellationIsNonBlocking);
    tests.emplace_back("pending update requires explicit install",
                       pendingUpdateRequiresExplicitInstallAction);
    tests.emplace_back("update install requires pending update",
                       updateInstallRequiresPendingUpdate);
    tests.emplace_back("update status uses notice channel", updateStatusUsesNoticeChannel);
    tests.emplace_back("update check is TrimUI only", updateCheckIsTrimuiOnly);
    tests.emplace_back("TrimUI face button mapping", trimuiFaceButtonMapping);
    tests.emplace_back("TrimUI Select repeat mapping", trimuiSelectCyclesRepeatMode);
    tests.emplace_back("restored Now Playing back returns to Home",
                       restoredNowPlayingBackReturnsToHome);
    tests.emplace_back("Home exit requires confirmation", homeExitRequiresConfirmation);
    tests.emplace_back("About returns to Home", aboutReturnsToHome);
    tests.emplace_back("Now Playing uses one navigation path",
                       openingNowPlayingUsesOneNavigationPath);
}
