#include "app/navigation.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>

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

std::optional<std::filesystem::path> dataDirectory() {
    const char* rawDataDir = std::getenv("POCKET_MUSIC_DATA_DIR");
    if (!rawDataDir || !*rawDataDir) return std::nullopt;
    return std::filesystem::path(rawDataDir);
}

std::optional<std::filesystem::path> updateDirectory() {
    const auto dataDir = dataDirectory();
    if (!dataDir) return std::nullopt;
    return *dataDir / "update";
}

std::optional<std::filesystem::path> appDirectory() {
    const char* rawAppDir = std::getenv("POCKET_MUSIC_APP_DIR");
    if (!rawAppDir || !*rawAppDir) return std::nullopt;
    return std::filesystem::path(rawAppDir);
}

std::optional<std::filesystem::path> updateChecker() {
    if (const char* rawChecker = std::getenv("POCKET_MUSIC_UPDATE_CHECKER"))
        if (*rawChecker) return std::filesystem::path(rawChecker);
    const auto appDir = appDirectory();
    if (!appDir) return std::nullopt;
    return *appDir / "update" / "check-update.sh";
}

std::optional<std::string> pendingUpdateVersion() {
    const auto updateDir = updateDirectory();
    if (!updateDir) return std::nullopt;

    std::ifstream pending(*updateDir / "pending-update");
    if (!pending) return std::nullopt;

    std::string line;
    while (std::getline(pending, line)) {
        constexpr const char* prefix = "version=";
        if (line.rfind(prefix, 0) == 0 && line.size() > 8) return line.substr(8);
    }
    return std::nullopt;
}

bool writeUpdateMarker(AppState& app, const char* filename) {
    const auto updateDir = updateDirectory();
    if (!updateDir) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Remote updates are available on TrimUI";
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(*updateDir, error);
    if (error) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Could not prepare update directory";
        return false;
    }

    std::ofstream request(*updateDir / filename, std::ios::trunc);
    if (!request) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Could not request update action";
        return false;
    }
    request << "requested\n";
    request.close();
    if (!request) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Could not save update request";
        return false;
    }
    return true;
}

ViewState homeView(const AppState& app) {
    ViewState view{Screen::Home, "Home", {}, 0, 0};
    view.items = {
        {"Songs", std::to_string(app.library.tracks().size()), ViewAction::OpenSongs, std::nullopt},
        {"Now Playing", "", ViewAction::OpenNowPlaying, std::nullopt},
        {"About", "", ViewAction::OpenAbout, std::nullopt},
        {"Check for Updates", "", ViewAction::CheckForUpdates, std::nullopt}};
    if (const auto version = pendingUpdateVersion())
        view.items.push_back(
            {"Install Update", "v" + *version, ViewAction::InstallUpdate, std::nullopt});
    return view;
}

void refreshHomeView(AppState& app) {
    if (app.view.screen != Screen::Home) return;
    const int selected = app.view.selected;
    app.view = homeView(app);
    app.view.selected = std::clamp(selected, 0, static_cast<int>(app.view.items.size()) - 1);
}

void readCheckPhase(AppState& app) {
    const auto updateDir = updateDirectory();
    if (!updateDir) return;

    std::ifstream phaseFile(*updateDir / "check-phase");
    if (!phaseFile) return;

    std::string phase;
    std::string version;
    std::string line;
    while (std::getline(phaseFile, line)) {
        if (line.rfind("phase=", 0) == 0)
            phase = line.substr(6);
        else if (line.rfind("version=", 0) == 0)
            version = line.substr(8);
    }

    if (!version.empty()) app.update.version = version;
    if (phase == "downloading") {
        app.update.phase = UpdatePhase::Downloading;
        app.update.detail = app.update.version.empty()
                                ? "Downloading update..."
                                : "Downloading v" + app.update.version + "...";
    } else if (phase == "verifying") {
        app.update.phase = UpdatePhase::Verifying;
        app.update.detail = "Verifying update...";
    } else if (phase == "checking") {
        app.update.phase = UpdatePhase::Checking;
        app.update.detail = "Looking for updates...";
    }
}

void clearCheckPhaseFile() {
    const auto updateDir = updateDirectory();
    if (!updateDir) return;
    std::error_code error;
    std::filesystem::remove(*updateDir / "check-phase", error);
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

bool requestUpdateCheck(AppState& app) {
    if (app.update.checking() || app.update.preparingInstall()) return false;

    const auto appDir = appDirectory();
    const auto dataDir = dataDirectory();
    const auto updateDir = updateDirectory();
    const auto checker = updateChecker();
    if (!appDir || !dataDir || !updateDir || !checker) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Remote updates are available on TrimUI";
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(*updateDir, error);
    if (error || access(checker->c_str(), X_OK) != 0) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Pocket Music updater is unavailable";
        return false;
    }
    clearCheckPhaseFile();

    const auto logPath = *updateDir / "update.log";
    const pid_t pid = fork();
    if (pid < 0) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Could not start update check";
        return false;
    }

    if (pid == 0) {
        setpgid(0, 0);
        const int logFd = open(logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logFd >= 0) {
            dup2(logFd, STDOUT_FILENO);
            dup2(logFd, STDERR_FILENO);
            close(logFd);
        }
        execl(checker->c_str(), checker->c_str(), POCKET_MUSIC_VERSION, appDir->c_str(),
              dataDir->c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    setpgid(pid, pid);
    app.message.clear();
    app.update.phase = UpdatePhase::Checking;
    app.update.processId = static_cast<int>(pid);
    app.update.version.clear();
    app.update.detail = "Looking for updates...";
    return true;
}

void pollUpdateCheck(AppState& app) {
    if (!app.update.checking() || app.update.processId <= 0) return;

    readCheckPhase(app);

    int status = 0;
    const pid_t pid = static_cast<pid_t>(app.update.processId);
    const pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) return;

    app.update.processId = -1;
    clearCheckPhaseFile();

    if (result < 0) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Couldn't check for updates. Try again.";
        refreshHomeView(app);
        return;
    }

    const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 2;
    if (exitCode == 0) {
        app.update.phase = UpdatePhase::UpToDate;
        app.update.version.clear();
        app.update.detail = "Pocket Music is up to date";
    } else if (exitCode == 10) {
        app.update.phase = UpdatePhase::Ready;
        if (const auto version = pendingUpdateVersion()) app.update.version = *version;
        app.update.detail = app.update.version.empty()
                                ? "Update is ready to install"
                                : "v" + app.update.version + " is ready to install";
    } else {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Couldn't check for updates. Check Wi-Fi and try again.";
    }
    refreshHomeView(app);
}

void cancelUpdateCheck(AppState& app) {
    if (app.update.processId > 0 && app.update.checking()) {
        const pid_t pid = static_cast<pid_t>(app.update.processId);
        kill(-pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }
    app.update.processId = -1;
    if (app.update.checking()) app.update.phase = UpdatePhase::Idle;
    clearCheckPhaseFile();
}

bool requestUpdateInstall(AppState& app) {
    if (app.update.checking() || app.update.preparingInstall()) return false;

    const auto updateDir = updateDirectory();
    if (!updateDir) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "Remote updates are available on TrimUI";
        return false;
    }
    if (!std::filesystem::exists(*updateDir / "pending-update")) {
        app.update.phase = UpdatePhase::Error;
        app.update.detail = "No update is ready to install";
        return false;
    }

    const auto version = pendingUpdateVersion();
    if (!writeUpdateMarker(app, "install-requested")) return false;

    app.message.clear();
    app.update.phase = UpdatePhase::PreparingInstall;
    app.update.processId = -1;
    app.update.version = version.value_or("");
    app.update.detail = app.update.version.empty() ? "Pocket Music will restart..."
                                                   : "Installing v" + app.update.version + "...";
    return true;
}

void finishDeferredUpdateHandoff(AppState& app) {
    if (app.update.preparingInstall()) app.running = false;
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
        case ViewAction::OpenAbout:
            pushView(app, {Screen::About, "About", {}, 0, 0});
            return;
        case ViewAction::CheckForUpdates:
            requestUpdateCheck(app);
            return;
        case ViewAction::InstallUpdate:
            requestUpdateInstall(app);
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
