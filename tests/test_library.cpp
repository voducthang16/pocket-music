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
void scansSupportedAudio() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Album B" / "Second.mp3");
    touch(music / "Album A" / "First.flac");
    touch(music / "notes.txt");
    MusicLibrary library(music);
    require(library.scan(), "valid directory must scan");
    require(library.tracks().size() == 2, "only audio files must become tracks");
}
void ignoresAppleDoubleAudioFiles() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "Song.mp3");
    touch(music / "._Song.mp3");
    MusicLibrary library(music);
    require(library.scan(), "library with AppleDouble metadata must scan");
    require(library.tracks().size() == 1, "AppleDouble metadata must not become tracks");
    require(library.tracks().front().title == "Song", "real audio file must remain visible");
}
}  // namespace
void addLibraryTests(TestCases& tests) {
    tests.emplace_back("missing library", missingLibrary);
    tests.emplace_back("scan supported audio", scansSupportedAudio);
    tests.emplace_back("ignore AppleDouble audio files", ignoresAppleDoubleAudioFiles);
}
