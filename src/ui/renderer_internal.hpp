#pragma once

#include <SDL.h>

#include <string>

#include "app/app_state.hpp"

std::string formatDuration(int seconds);
std::string repeatLabel(const AppState& app);
void drawButtonHints(AppState& app);
void drawCompactRow(AppState& app, const ViewItem& item, int index, int y, bool active,
                    int textWidth, const std::string& trailing = {}, bool chevron = false);
void drawPlaybackSurface(AppState& app);
void drawPlaybackProgress(AppState& app, const PlaybackSnapshot& playback, int fallbackDuration,
                          const SDL_Rect& bar, int labelsY);
SDL_Texture* albumCover(AppState& app, const Track& track);
void drawCover(AppState& app, const Track* track, const SDL_Rect& bounds);
const Track* currentTrack(const AppState& app);
void drawNowPlayingBand(AppState& app);
void drawLibraryScreen(AppState& app);
void drawListScreen(AppState& app);
void drawNowPlayingScreen(AppState& app);
