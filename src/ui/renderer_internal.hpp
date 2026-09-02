#pragma once

#include <SDL.h>

#include <string>

#include "app/app_state.hpp"

std::string formatDuration(int seconds);
std::string repeatLabel(const AppState& app);
void drawHeader(AppState& app, const std::string& title, const std::string& eyebrow = {});
void drawButtonHints(AppState& app);
SDL_Texture* albumCover(AppState& app, const Track& track);
void drawCover(AppState& app, const Track* track, const SDL_Rect& bounds);
const Track* currentTrack(const AppState& app);
void drawNowPlayingBand(AppState& app);
void drawLibraryScreen(AppState& app);
void drawListScreen(AppState& app);
void drawNowPlayingScreen(AppState& app);
