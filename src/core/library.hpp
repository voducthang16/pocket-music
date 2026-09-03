#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/model.hpp"

class MusicLibrary {
   public:
    explicit MusicLibrary(std::filesystem::path root);
    bool scan();
    const std::vector<Track>& tracks() const { return tracks_; }
    const std::filesystem::path& root() const { return root_; }
    const std::string& error() const { return error_; }

   private:
    std::filesystem::path root_;
    std::vector<Track> tracks_;
    std::string error_;
};
