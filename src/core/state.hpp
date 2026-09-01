#pragma once

#include <filesystem>
#include <string>

struct SavedState {
    std::string trackPath;
    int positionSeconds = 0;
    bool shuffle = false;
    int repeatMode = 0;
    std::string screen = "library";
};

SavedState loadState(const std::filesystem::path& path);
bool saveState(const std::filesystem::path& path, const SavedState& state);
