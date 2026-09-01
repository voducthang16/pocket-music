#include <fstream>

#include "core/state.hpp"
#include "test_suites.hpp"
namespace {
void stateValidation() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "nested" / "state";
    SavedState expected{"Music/Nghệ sĩ/Bài hát.mp3", 137, true, 2, "now-playing"};
    require(saveState(path, expected), "atomic state write must succeed");
    const auto actual = loadState(path);
    require(actual.trackPath == expected.trackPath, "Unicode path must round-trip");
    require(actual.positionSeconds == 137 && actual.repeatMode == 2,
            "playback state must round-trip");
    std::ofstream(path) << "position -40\nrepeat 99\nscreen \"removed-screen\"\n";
    const auto invalid = loadState(path);
    require(invalid.positionSeconds == 0, "negative position must be rejected");
    require(invalid.repeatMode == 0, "invalid repeat mode must be rejected");
    require(invalid.screen == "library", "invalid screen must be rejected");
}
}  // namespace
void addStateTests(TestCases& tests) { tests.emplace_back("state validation", stateValidation); }
