#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Track {
    std::filesystem::path path;
    std::string title;
    std::string artist;
    std::string album;
    int durationSeconds = 0;
    int trackNumber = 0;
    std::filesystem::path coverPath;
};

struct Playlist {
    std::string name;
    std::filesystem::path path;
    std::vector<size_t> trackIndexes;
};

struct TrackGroup {
    std::string title;
    std::string subtitle;
    std::vector<size_t> trackIndexes;
};
