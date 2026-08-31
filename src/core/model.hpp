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
    std::filesystem::path coverPath;
};

struct Playlist {
    std::string name;
    std::filesystem::path path;
    std::vector<std::filesystem::path> entries;
};
