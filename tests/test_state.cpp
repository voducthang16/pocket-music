#include <fstream>

#include "core/playback_session.hpp"
#include "core/state.hpp"
#include "test_suites.hpp"
namespace {
void stateValidation() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "nested" / "state";
    PlaybackSession expected;
    expected.currentTrackPath = "Music/Nghệ sĩ/Bài hát.mp3";
    expected.sourcePaths = {"Music/One.mp3", expected.currentTrackPath};
    expected.orderPaths = {expected.currentTrackPath, "Music/One.mp3"};
    expected.historyPaths = {"Music/One.mp3"};
    expected.positionSeconds = 137.5;
    expected.shuffle = true;
    expected.repeatMode = 2;
    expected.screen = "now-playing";
    require(saveSession(path, expected), "atomic session write must succeed");

    std::ifstream persisted(path);
    std::string line;
    while (std::getline(persisted, line))
        require(line.rfind("paused ", 0) != 0, "paused state must not be persisted");

    const auto actual = loadSession(path);
    require(actual.currentTrackPath == expected.currentTrackPath, "Unicode path must round-trip");
    require(actual.sourcePaths == expected.sourcePaths &&
                actual.orderPaths == expected.orderPaths &&
                actual.historyPaths == expected.historyPaths,
            "queue paths and history must round-trip");
    require(actual.positionSeconds == 137.5 && actual.repeatMode == 2,
            "playback state must round-trip");
    std::ofstream(path) << "position -40\nrepeat 99\nscreen \"removed-screen\"\n";
    const auto invalid = loadSession(path);
    require(invalid.positionSeconds == 0, "negative position must be rejected");
    require(invalid.repeatMode == 0, "invalid repeat mode must be rejected");
    require(invalid.screen == "home", "invalid screen must be rejected");
}

void inconsistentSessionUsesDefaults() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "state";
    std::ofstream(path) << "current \"Missing.mp3\"\n"
                           "source \"One.mp3\"\n"
                           "order \"One.mp3\"\n"
                           "cursor 4\n";
    const auto session = loadSession(path);
    require(session.currentTrackPath.empty() && session.sourcePaths.empty(),
            "internally inconsistent sessions must be rejected as a whole");
}

void sessionCurrentTrackMustMatchCursor() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "state";
    std::ofstream(path) << "current \"One.mp3\"\n"
                           "source \"One.mp3\"\n"
                           "source \"Two.mp3\"\n"
                           "order \"One.mp3\"\n"
                           "order \"Two.mp3\"\n"
                           "cursor 1\n";
    const auto session = loadSession(path);
    require(session.currentTrackPath.empty(),
            "the durable current track must identify the queue cursor entry");
}

void missingCurrentTrackResolvesToNextEntry() {
    TemporaryDirectory temporary;
    const auto music = temporary.path / "Music";
    touch(music / "One.mp3");
    touch(music / "Three.mp3");
    MusicLibrary library(music);
    require(library.scan(), "session fixture must scan");

    PlaybackSession session;
    const auto one = (music / "One.mp3").string();
    const auto missing = (music / "Two.mp3").string();
    const auto three = (music / "Three.mp3").string();
    session.currentTrackPath = missing;
    session.sourcePaths = {one, missing, three};
    session.orderPaths = {one, missing, three};
    session.cursor = 1;

    const auto resolved = resolvePlaybackSession(session, library);
    require(resolved && resolved->source == std::vector<size_t>({0, 1}) && resolved->cursor == 1,
            "a missing current track must resolve to the next surviving queue entry");
}
}  // namespace
void addStateTests(TestCases& tests) {
    tests.emplace_back("state validation", stateValidation);
    tests.emplace_back("state consistency", inconsistentSessionUsesDefaults);
    tests.emplace_back("state current cursor", sessionCurrentTrackMustMatchCursor);
    tests.emplace_back("missing current track", missingCurrentTrackResolvesToNextEntry);
}
