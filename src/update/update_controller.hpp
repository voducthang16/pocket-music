#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

#include "update/update_state.hpp"

struct UpdateRuntimePaths {
    std::filesystem::path appDir;
    std::filesystem::path dataDir;
    std::filesystem::path checker;

    bool available() const { return !appDir.empty() && !dataDir.empty() && !checker.empty(); }
};

class UpdateController {
   public:
    explicit UpdateController(UpdateRuntimePaths paths = {});
    ~UpdateController();

    UpdateController(const UpdateController&) = delete;
    UpdateController& operator=(const UpdateController&) = delete;

    const UpdateState& state() const { return state_; }
    const UpdateRuntimePaths& paths() const { return paths_; }

    bool check();
    bool poll();
    void cancel();
    bool requestInstall();
    bool consumeLastStatus();
    std::optional<std::string> pendingVersion() const;

   private:
    std::filesystem::path updateDirectory() const;
    void setError(std::string detail);
    void readCheckPhase();
    void clearCheckPhaseFile() const;
    void resetCancellation();
    void terminateForDestruction();

    UpdateRuntimePaths paths_;
    UpdateState state_;
    int processId_ = -1;
    bool killSent_ = false;
    std::chrono::steady_clock::time_point cancelDeadline_{};
};
