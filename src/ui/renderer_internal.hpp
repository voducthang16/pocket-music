#pragma once

#include <SDL.h>

#include <string>

#include "app/app_state.hpp"

std::string formatDuration(int seconds);
std::string repeatLabel(const AppState& app);
void drawButtonHints(AppState& app);
void drawExitConfirmation(AppState& app);
void drawHomeRow(AppState& app, const ViewItem& item, int index, int y, bool active,
                 const std::string& trailing = {}, bool chevron = false);
void drawTrackRow(AppState& app, const ViewItem& item, int index, int y, bool active,
                  const std::string& duration);
void drawPlaybackSurface(AppState& app);
void drawPlaybackProgress(AppState& app, const PlaybackSnapshot& playback, int fallbackDuration,
                          const SDL_Rect& bar, int labelsY);
SDL_Texture* albumCover(AppState& app, const Track& track);
void drawCover(AppState& app, const Track* track, const SDL_Rect& bounds);
const Track* currentTrack(const AppState& app);
void drawNowPlayingBand(AppState& app);
void drawHomeScreen(AppState& app);
void drawSongsScreen(AppState& app);
void drawLinerNotesScreen(AppState& app);
void drawNowPlayingScreen(AppState& app);
