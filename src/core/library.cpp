#include "core/library.hpp"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

#include <algorithm>
#include <cctype>
#include <set>

namespace fs = std::filesystem;

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool isAudio(const fs::path& path) {
    static const std::set<std::string> extensions = {".mp3", ".flac", ".wav", ".ogg"};
    return extensions.find(lower(path.extension().string())) != extensions.end();
}

bool isAppleDouble(const fs::path& path) {
    return path.filename().string().compare(0, 2, "._") == 0;
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

}  // namespace

MusicLibrary::MusicLibrary(fs::path root) : root_(std::move(root)) {}

bool MusicLibrary::scan() {
    tracks_.clear();
    error_.clear();

    std::error_code error;
    if (!fs::is_directory(root_, error)) {
        error_ = error ? error.message() : "Music directory does not exist: " + root_.string();
        return false;
    }

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
        if (isAppleDouble(path) || !isAudio(path)) continue;

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
            if (const auto* properties = file.audioProperties())
                track.durationSeconds = properties->lengthInSeconds();
        }
        tracks_.push_back(std::move(track));
    }

    std::sort(tracks_.begin(), tracks_.end(),
              [](const Track& a, const Track& b) { return lower(a.title) < lower(b.title); });
    return true;
}
