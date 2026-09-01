#include <fstream>

#include "core/library.hpp"
#include "test_suites.hpp"
namespace fs = std::filesystem;
namespace {
void missingLibrary() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    MusicLibrary library(music);
    require(!library.scan(), "missing directory must be reported");
    require(!fs::exists(music), "scan must not mutate the filesystem");
    require(!library.error().empty(), "scan failure must explain its cause");
}
void cachedGroups() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Album B" / "Second.mp3");
    touch(music / "Album A" / "First.flac");
    touch(music / "notes.txt");
    MusicLibrary library(music);
    require(library.scan(), "valid directory must scan");
    require(library.tracks().size() == 2, "only audio files must become tracks");
    require(library.albums().size() == 2, "albums must be cached");
    require(library.artists().size() == 1, "artists must be cached");
    require(library.allTrackIndexes().size() == 2, "full-library queue must be cached");
}
void playlistBom() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Album" / "One.mp3");
    touch(music / "Album" / "Two.mp3");
    std::ofstream playlist(music / "Favorites.m3u8", std::ios::binary);
    playlist << "\xEF\xBB\xBF#EXTM3U\nhttps://example.com/radio\nAlbum/Two.mp3\n"
                "Album/Missing.mp3\nAlbum/One.mp3\n";
    playlist.close();
    MusicLibrary library(music);
    require(library.scan(), "playlist fixture must scan");
    require(library.playlists().size() == 1, "playlist must be discovered");
    const auto& indexes = library.playlists().front().trackIndexes;
    require(indexes.size() == 2, "URL and missing entries must be ignored");
    require(library.tracks()[indexes[0]].title == "Two", "playlist order must be preserved");
    require(library.tracks()[indexes[1]].title == "One", "playlist order must be preserved");
}
}  // namespace
void addLibraryTests(TestCases& tests) {
    tests.emplace_back("missing library", missingLibrary);
    tests.emplace_back("cached groups", cachedGroups);
    tests.emplace_back("playlist BOM", playlistBom);
}
