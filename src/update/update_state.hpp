#pragma once

#include <string>

enum class UpdatePhase {
    Idle,
    Checking,
    Downloading,
    Verifying,
    Cancelling,
    UpToDate,
    Ready,
    PreparingInstall,
    Result,
    Error
};

struct UpdateState {
    UpdatePhase phase = UpdatePhase::Idle;
    std::string version;
    std::string detail;

    bool checking() const {
        return phase == UpdatePhase::Checking || phase == UpdatePhase::Downloading ||
               phase == UpdatePhase::Verifying;
    }

    bool cancelling() const { return phase == UpdatePhase::Cancelling; }
    bool preparingInstall() const { return phase == UpdatePhase::PreparingInstall; }
    bool modalVisible() const { return checking() || cancelling() || preparingInstall(); }
    bool cancellable() const { return checking(); }
};
