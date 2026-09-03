#pragma once

#include <optional>

#include "app/app_state.hpp"

enum class InputAction {
    Up,
    Down,
    SeekBack,
    SeekForward,
    Confirm,
    Back,
    PlayPause,
    Previous,
    Next,
    NowPlaying,
    Shuffle,
    Repeat,
};

std::optional<InputAction> keyboardInputAction(SDL_Keycode code);
std::optional<InputAction> controllerInputAction(Uint8 button);
bool isRepeatable(InputAction action);
void handleInputAction(AppState& app, InputAction action);
