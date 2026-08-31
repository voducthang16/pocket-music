#pragma once
#include <string>
#include <vector>

#include "app/app_state.hpp"
std::vector<std::string> visibleLabels(const AppState& app);
std::string screenHeading(const AppState& app);
void playTrack(AppState& app, size_t index, int resumeSeconds = 0);
void playAdjacentTrack(AppState& app, int direction);
void selectCurrentItem(AppState& app);
void navigateBack(AppState& app);
void advanceWhenFinished(AppState& app);
