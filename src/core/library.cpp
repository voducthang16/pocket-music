#include "core/library.hpp"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <numeric>
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
    std::error_code error;
    for (const char* name : {"cover.jpg", "cover.png", "folder.jpg", "folder.png"}) {
        const auto candidate = directory / name;
        if (fs::is_regular_file(candidate, error)) return candidate;
        error.clear();
    }
    return {};
}

std::string canonicalKey(const fs::path& path) {
    std::error_code error;
    auto canonical = fs::weakly_canonical(path, error);
    return error ? fs::absolute(path, error).lexically_normal().string() : canonical.string();
}

std::string trackCount(size_t count) {
    return std::to_string(count) + (count == 1 ? " track" : " tracks");
}
}  // namespace

MusicLibrary::MusicLibrary(fs::path root) : root_(std::move(root)) {}

bool MusicLibrary::scan() {
    tracks_.clear();
    playlists_.clear();
    artists_.clear();
    albums_.clear();
    allTrackIndexes_.clear();
    error_.clear();

    std::error_code error;
    if (!fs::is_directory(root_, error)) {
        error_ = error ? error.message() : "Music directory does not exist: " + root_.string();
        return false;
    }

    std::vector<fs::path> playlistPaths;
    fs::recursive_directory_iterator iterator(root_, fs::directory_options::skip_permission_denied,
                                              error);
    if (error) {
        error_ = "Could not read Music directory: " + error.message();
        return false;
    }
    const fs::recursive_directory_iterator end;
    while (iterator != end) {
        if (error) {
            error.clear();
            iterator.increment(error);
            continue;
        }
        const auto entry = *iterator;
        iterator.increment(error);
        std::error_code typeError;
        if (!entry.is_regular_file(typeError)) continue;
        const auto path = entry.path();
        const auto extension = lower(path.extension().string());
        if (extension == ".m3u" || extension == ".m3u8") {
            playlistPaths.push_back(path);
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
                track.trackNumber = static_cast<int>(tag->track());
            }
            if (const auto* properties = file.audioProperties())
                track.durationSeconds = properties->lengthInSeconds();
        }
        tracks_.push_back(std::move(track));
    }

    std::sort(tracks_.begin(), tracks_.end(),
              [](const Track& a, const Track& b) { return lower(a.title) < lower(b.title); });
    allTrackIndexes_.resize(tracks_.size());
    std::iota(allTrackIndexes_.begin(), allTrackIndexes_.end(), 0);

    std::map<std::pair<std::string, std::string>, std::vector<size_t>> albums;
    std::map<std::string, std::vector<size_t>> artists;
    std::unordered_map<std::string, size_t> pathIndex;
    for (size_t index = 0; index < tracks_.size(); ++index) {
        albums[{tracks_[index].artist, tracks_[index].album}].push_back(index);
        artists[tracks_[index].artist].push_back(index);
        pathIndex[canonicalKey(tracks_[index].path)] = index;
    }
    for (auto& [key, indexes] : albums) {
        std::stable_sort(indexes.begin(), indexes.end(), [&](size_t left, size_t right) {
            const auto& a = tracks_[left];
            const auto& b = tracks_[right];
            if (a.trackNumber > 0 && b.trackNumber > 0 && a.trackNumber != b.trackNumber)
                return a.trackNumber < b.trackNumber;
            if ((a.trackNumber > 0) != (b.trackNumber > 0)) return a.trackNumber > 0;
            return lower(a.title) < lower(b.title);
        });
        albums_.push_back({key.second, key.first + " - " + trackCount(indexes.size()), indexes});
    }
    std::sort(albums_.begin(), albums_.end(), [](const auto& a, const auto& b) {
        if (lower(a.title) != lower(b.title)) return lower(a.title) < lower(b.title);
        return lower(a.subtitle) < lower(b.subtitle);
    });
    for (auto& [artist, indexes] : artists)
        artists_.push_back({artist, trackCount(indexes.size()), std::move(indexes)});

    std::sort(playlistPaths.begin(), playlistPaths.end());
    for (const auto& path : playlistPaths) {
        Playlist playlist{path.stem().string(), path, {}};
        std::ifstream stream(path);
        std::string line;
        bool firstLine = true;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (firstLine && line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
            firstLine = false;
            if (line.empty() || line.front() == '#' || line.find("://") != std::string::npos)
                continue;
            fs::path item(line);
            if (!item.is_absolute()) item = path.parent_path() / item;
            if (const auto found = pathIndex.find(canonicalKey(item)); found != pathIndex.end())
                playlist.trackIndexes.push_back(found->second);
        }
        playlists_.push_back(std::move(playlist));
    }
    return true;
}
