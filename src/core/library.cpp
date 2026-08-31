#include "core/library.hpp"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool isAudio(const fs::path& path) {
    static const std::set<std::string> extensions = {".mp3", ".flac", ".wav", ".ogg"};
    return extensions.contains(lower(path.extension().string()));
}

std::string utf8(const TagLib::String& value) { return value.to8Bit(true); }

std::string fallback(const std::string& value, const std::string& other) {
    return value.empty() ? other : value;
}

fs::path findCover(const fs::path& directory) {
    for (const char* name : {"cover.jpg", "cover.png", "folder.jpg", "folder.png"}) {
        const auto candidate = directory / name;
        if (fs::exists(candidate)) return candidate;
    }
    return {};
}
}  // namespace

MusicLibrary::MusicLibrary(fs::path root) : root_(std::move(root)) {}

void MusicLibrary::scan() {
    tracks_.clear();
    playlists_.clear();
    std::error_code error;
    if (!fs::exists(root_, error)) fs::create_directories(root_, error);
    if (error) return;

    for (const auto& entry : fs::recursive_directory_iterator(
             root_, fs::directory_options::skip_permission_denied, error)) {
        if (error || !entry.is_regular_file()) continue;
        const auto& path = entry.path();
        if (lower(path.extension().string()) == ".m3u" ||
            lower(path.extension().string()) == ".m3u8") {
            Playlist playlist{path.stem().string(), path, {}};
            std::ifstream stream(path);
            std::string line;
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.empty() || line.front() == '#') continue;
                fs::path item(line);
                playlist.entries.push_back(item.is_absolute() ? item : path.parent_path() / item);
            }
            playlists_.push_back(std::move(playlist));
            continue;
        }
        if (!isAudio(path)) continue;

        Track track;
        track.path = path;
        track.title = path.stem().string();
        track.artist = "Unknown Artist";
        track.album = path.parent_path().filename().string();
        track.coverPath = findCover(path.parent_path());

        TagLib::FileRef file(path.c_str(), true, TagLib::AudioProperties::Fast);
        if (!file.isNull()) {
            if (const auto* tag = file.tag()) {
                track.title = fallback(utf8(tag->title()), track.title);
                track.artist = fallback(utf8(tag->artist()), track.artist);
                track.album = fallback(utf8(tag->album()), track.album);
            }
            if (const auto* properties = file.audioProperties()) {
                track.durationSeconds = properties->lengthInSeconds();
            }
        }
        tracks_.push_back(std::move(track));
    }

    std::sort(tracks_.begin(), tracks_.end(),
              [](const Track& a, const Track& b) { return lower(a.title) < lower(b.title); });
    std::sort(playlists_.begin(), playlists_.end(),
              [](const Playlist& a, const Playlist& b) { return lower(a.name) < lower(b.name); });
}

std::vector<std::string> MusicLibrary::artists() const {
    std::set<std::string> values;
    for (const auto& track : tracks_) values.insert(track.artist);
    return {values.begin(), values.end()};
}

std::vector<std::string> MusicLibrary::albums() const {
    std::set<std::string> values;
    for (const auto& track : tracks_) values.insert(track.album);
    return {values.begin(), values.end()};
}

std::vector<size_t> MusicLibrary::byArtist(const std::string& artist) const {
    std::vector<size_t> result;
    for (size_t i = 0; i < tracks_.size(); ++i)
        if (tracks_[i].artist == artist) result.push_back(i);
    return result;
}

std::vector<size_t> MusicLibrary::byAlbum(const std::string& album) const {
    std::vector<size_t> result;
    for (size_t i = 0; i < tracks_.size(); ++i)
        if (tracks_[i].album == album) result.push_back(i);
    return result;
}

std::vector<size_t> MusicLibrary::fromPlaylist(const Playlist& playlist) const {
    std::vector<size_t> result;
    for (const auto& entry : playlist.entries) {
        std::error_code error;
        const auto wanted = fs::weakly_canonical(entry, error);
        for (size_t i = 0; i < tracks_.size(); ++i) {
            if (fs::weakly_canonical(tracks_[i].path, error) == wanted) {
                result.push_back(i);
                break;
            }
        }
    }
    return result;
}
