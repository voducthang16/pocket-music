#pragma once
#include "app/app_state.hpp"
void handleKey(AppState& app, SDL_Keycode code);
void handleMouse(AppState& app, const SDL_MouseButtonEvent& event);
void handleControllerButton(AppState& app, Uint8 button);
