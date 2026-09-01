#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct PlaybackSession {
    static constexpr int currentVersion = 1;
    int version = currentVersion;
    std::string currentTrackPath;
    std::string sourceTitle;
    std::vector<std::string> sourcePaths;
    std::vector<std::string> orderPaths;
    std::vector<std::string> historyPaths;
    size_t cursor = 0;
    double positionSeconds = 0;
    bool paused = true;
    bool shuffle = false;
    int repeatMode = 0;
    std::string screen = "library";
};

PlaybackSession loadSession(const std::filesystem::path& path);
bool saveSession(const std::filesystem::path& path, const PlaybackSession& session);
