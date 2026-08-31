#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/model.hpp"

class MusicLibrary {
   public:
    explicit MusicLibrary(std::filesystem::path root);
    void scan();
    const std::vector<Track>& tracks() const { return tracks_; }
    const std::vector<Playlist>& playlists() const { return playlists_; }
    std::vector<std::string> artists() const;
    std::vector<std::string> albums() const;
    std::vector<size_t> byArtist(const std::string& artist) const;
    std::vector<size_t> byAlbum(const std::string& album) const;
    std::vector<size_t> fromPlaylist(const Playlist& playlist) const;
    const std::filesystem::path& root() const { return root_; }

   private:
    std::filesystem::path root_;
    std::vector<Track> tracks_;
    std::vector<Playlist> playlists_;
};
