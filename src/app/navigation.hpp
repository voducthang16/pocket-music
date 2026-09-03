#pragma once

#include "app/app_state.hpp"

void buildHomeView(AppState& app);
void refreshHomeView(AppState& app);
ViewState nowPlayingView();
void openNowPlaying(AppState& app);
bool playTrack(AppState& app, size_t trackIndex, const std::vector<size_t>& queue,
               std::optional<size_t> sourcePosition = std::nullopt);
void playAdjacentTrack(AppState& app, int direction);
void finishDeferredUpdateHandoff(AppState& app);
void selectCurrentItem(AppState& app);
void navigateBack(AppState& app);
void advanceWhenFinished(AppState& app);
