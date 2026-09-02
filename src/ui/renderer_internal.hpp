#pragma once

#include <SDL.h>

#include <string>

#include "app/app_state.hpp"

std::string formatDuration(int seconds);
std::string countLabel(size_t count, const char* singular, const char* plural);
void drawHeader(AppState& app, const std::string& title, const std::string& eyebrow = {});
void drawButtonHints(AppState& app);
SDL_Texture* albumCover(AppState& app, const Track& track);
void drawCover(AppState& app, const Track* track, const SDL_Rect& bounds);
void drawLibraryScreen(AppState& app);
void drawListScreen(AppState& app);
void drawNowPlayingScreen(AppState& app);
