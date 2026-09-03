#include "update/update_controller.hpp"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <system_error>
#include <utility>

extern char** environ;

namespace {
constexpr auto cancelGracePeriod = std::chrono::milliseconds(500);
}

UpdateController::UpdateController(UpdateRuntimePaths paths) : paths_(std::move(paths)) {}

UpdateController::~UpdateController() { terminateForDestruction(); }

std::filesystem::path UpdateController::updateDirectory() const {
    return paths_.dataDir.empty() ? std::filesystem::path{} : paths_.dataDir / "update";
}

void UpdateController::setError(std::string detail) {
    state_.phase = UpdatePhase::Error;
    state_.version.clear();
    state_.detail = std::move(detail);
}

std::optional<std::string> UpdateController::pendingVersion() const {
    const auto updateDir = updateDirectory();
    if (updateDir.empty()) return std::nullopt;

    std::ifstream pending(updateDir / "pending-update");
    if (!pending) return std::nullopt;

    std::string line;
    while (std::getline(pending, line)) {
        constexpr const char* prefix = "version=";
        if (line.rfind(prefix, 0) == 0 && line.size() > 8) return line.substr(8);
    }
    return std::nullopt;
}

void UpdateController::readCheckPhase() {
    if (!state_.checking()) return;
    const auto updateDir = updateDirectory();
    if (updateDir.empty()) return;

    std::ifstream phaseFile(updateDir / "check-phase");
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

    if (!version.empty()) state_.version = version;
    if (phase == "downloading") {
        state_.phase = UpdatePhase::Downloading;
        state_.detail = state_.version.empty() ? "Downloading update..."
                                               : "Downloading v" + state_.version + "...";
    } else if (phase == "verifying") {
        state_.phase = UpdatePhase::Verifying;
        state_.detail = "Verifying update...";
    } else if (phase == "checking") {
        state_.phase = UpdatePhase::Checking;
        state_.detail = "Looking for updates...";
    }
}

void UpdateController::clearCheckPhaseFile() const {
    const auto updateDir = updateDirectory();
    if (updateDir.empty()) return;
    std::error_code error;
    std::filesystem::remove(updateDir / "check-phase", error);
}

bool UpdateController::check() {
    if (state_.checking() || state_.cancelling() || state_.preparingInstall()) return false;
    if (!paths_.available()) {
        setError("Remote updates are available on TrimUI");
        return false;
    }

    const auto updateDir = updateDirectory();
    std::error_code filesystemError;
    std::filesystem::create_directories(updateDir, filesystemError);
    if (filesystemError || access(paths_.checker.c_str(), X_OK) != 0) {
        setError("Pocket Music updater is unavailable");
        return false;
    }
    clearCheckPhaseFile();

    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    bool actionsReady = false;
    bool attributesReady = false;
    int error = posix_spawn_file_actions_init(&actions);
    if (error == 0) {
        actionsReady = true;
        const auto logPath = updateDir / "update.log";
        error = posix_spawn_file_actions_addopen(
            &actions, STDOUT_FILENO, logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (error == 0)
            error = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    }
    if (error == 0) {
        error = posix_spawnattr_init(&attributes);
        if (error == 0) {
            attributesReady = true;
            error = posix_spawnattr_setpgroup(&attributes, 0);
            if (error == 0)
                error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        }
    }
    if (error != 0) {
        if (attributesReady) posix_spawnattr_destroy(&attributes);
        if (actionsReady) posix_spawn_file_actions_destroy(&actions);
        setError("Could not prepare update check");
        return false;
    }

    const std::string checker = paths_.checker.string();
    const std::string appDir = paths_.appDir.string();
    const std::string dataDir = paths_.dataDir.string();
    char* arguments[] = {const_cast<char*>(checker.c_str()),
                         const_cast<char*>(POCKET_MUSIC_VERSION),
                         const_cast<char*>(appDir.c_str()),
                         const_cast<char*>(dataDir.c_str()), nullptr};

    pid_t pid = -1;
    error = posix_spawn(&pid, checker.c_str(), &actions, &attributes, arguments, environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    if (error != 0 || pid <= 0) {
        setError("Could not start update check");
        return false;
    }

    processId_ = static_cast<int>(pid);
    resetCancellation();
    state_.phase = UpdatePhase::Checking;
    state_.version.clear();
    state_.detail = "Looking for updates...";
    return true;
}

bool UpdateController::poll() {
    if (processId_ <= 0) return false;

    if (state_.checking()) readCheckPhase();

    int status = 0;
    const pid_t pid = static_cast<pid_t>(processId_);
    const pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) {
        if (state_.cancelling() && !killSent_ &&
            std::chrono::steady_clock::now() >= cancelDeadline_) {
            kill(-pid, SIGKILL);
            killSent_ = true;
        }
        return false;
    }

    processId_ = -1;
    clearCheckPhaseFile();

    if (state_.cancelling()) {
        resetCancellation();
        state_.phase = UpdatePhase::Idle;
        state_.version.clear();
        state_.detail.clear();
        return true;
    }

    resetCancellation();
    if (result < 0) {
        setError("Couldn't check for updates. Try again.");
        return true;
    }

    const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 2;
    if (exitCode == 0) {
        state_.phase = UpdatePhase::UpToDate;
        state_.version.clear();
        state_.detail = "Pocket Music is up to date";
    } else if (exitCode == 10) {
        state_.phase = UpdatePhase::Ready;
        if (const auto version = pendingVersion()) state_.version = *version;
        state_.detail = state_.version.empty() ? "Update is ready to install"
                                               : "v" + state_.version + " is ready to install";
    } else {
        setError("Couldn't check for updates. Check Wi-Fi and try again.");
    }
    return true;
}

void UpdateController::cancel() {
    if (state_.cancelling()) return;
    if (processId_ <= 0 || !state_.checking()) {
        if (state_.checking()) {
            state_.phase = UpdatePhase::Idle;
            state_.version.clear();
            state_.detail.clear();
        }
        return;
    }

    const pid_t pid = static_cast<pid_t>(processId_);
    kill(-pid, SIGTERM);
    state_.phase = UpdatePhase::Cancelling;
    state_.detail = "Cancelling...";
    killSent_ = false;
    cancelDeadline_ = std::chrono::steady_clock::now() + cancelGracePeriod;
}

bool UpdateController::requestInstall() {
    if (state_.checking() || state_.cancelling() || state_.preparingInstall()) return false;
    const auto updateDir = updateDirectory();
    if (updateDir.empty()) {
        setError("Remote updates are available on TrimUI");
        return false;
    }
    if (!std::filesystem::exists(updateDir / "pending-update")) {
        setError("No update is ready to install");
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(updateDir, error);
    if (error) {
        setError("Could not prepare update directory");
        return false;
    }

    std::ofstream request(updateDir / "install-requested", std::ios::trunc);
    if (!request) {
        setError("Could not request update action");
        return false;
    }
    request << "requested\n";
    request.close();
    if (!request) {
        setError("Could not save update request");
        return false;
    }

    state_.phase = UpdatePhase::PreparingInstall;
    state_.version = pendingVersion().value_or("");
    state_.detail = state_.version.empty() ? "Pocket Music will restart..."
                                           : "Installing v" + state_.version + "...";
    return true;
}

bool UpdateController::consumeLastStatus() {
    const auto updateDir = updateDirectory();
    if (updateDir.empty()) return false;

    const auto statusPath = updateDir / "last-status";
    std::ifstream status(statusPath);
    if (!status) return false;

    std::string line;
    std::getline(status, line);
    std::error_code error;
    std::filesystem::remove(statusPath, error);
    if (line.empty()) return false;

    state_.phase = UpdatePhase::Result;
    state_.version.clear();
    state_.detail = std::move(line);
    return true;
}

void UpdateController::resetCancellation() {
    killSent_ = false;
    cancelDeadline_ = {};
}

void UpdateController::terminateForDestruction() {
    if (processId_ <= 0) return;
    const pid_t pid = static_cast<pid_t>(processId_);
    kill(-pid, SIGKILL);
    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
    }
    processId_ = -1;
    clearCheckPhaseFile();
}
