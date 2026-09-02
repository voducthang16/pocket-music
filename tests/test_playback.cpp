#include <algorithm>
#include <memory>
#include <set>

#include "core/playback_controller.hpp"
#include "fake_player.hpp"
#include "test_suites.hpp"

namespace fs = std::filesystem;

namespace {
struct PlaybackFixture {
    TemporaryDirectory temporary;
    MusicLibrary library{temporary.path / "Music"};
    FakePlayer* player = nullptr;
    std::unique_ptr<PlaybackController> controller;

    PlaybackFixture() {
        touch(library.root() / "One.mp3");
        touch(library.root() / "Two.mp3");
        touch(library.root() / "Three.mp3");
        require(library.scan(), "playback fixture must scan");
        auto fake = std::make_unique<FakePlayer>();
        player = fake.get();
        controller = std::make_unique<PlaybackController>(library, std::move(fake));
    }
};

void loadLifecycleAndStaleEvents() {
    PlaybackFixture fixture;
    const auto& all = fixture.library.allTrackIndexes();
    require(fixture.controller->play(0, all), "first load must start");
    require(!fixture.controller->snapshot().trackIndex,
            "a pending load must not become the active track before file-loaded");
    require(fixture.controller->play(1, all), "second load must replace first");
    fixture.player->emit({PlayerEventType::FileLoaded, 1});
    fixture.controller->update();
    require(fixture.controller->snapshot().phase == PlaybackPhase::Loading,
            "late events from an older generation must be ignored");
    fixture.player->emit({PlayerEventType::FileLoaded, 2});
    fixture.controller->update();
    require(fixture.controller->snapshot().phase == PlaybackPhase::Playing &&
                fixture.controller->snapshot().trackIndex == 1,
            "matching file-loaded must commit the pending track");
}

void loadingFailureIsVisible() {
    PlaybackFixture fixture;
    fixture.controller->play(0, fixture.library.allTrackIndexes());
    fixture.player->emit({PlayerEventType::Failed, 1, 0, false, "broken"});
    fixture.controller->update();
    require(fixture.controller->snapshot().phase == PlaybackPhase::Error &&
                fixture.controller->snapshot().errorMessage == "broken",
            "asynchronous load failure must become visible state");
}

void synchronousLoadFailureCanRetry() {
    PlaybackFixture fixture;
    fixture.player->loadSucceeds = false;
    require(!fixture.controller->play(0, fixture.library.allTrackIndexes()),
            "synchronous player startup failure must propagate");
    fixture.player->loadSucceeds = true;
    fixture.controller->retry();
    require(fixture.player->loadCount == 2 &&
                fixture.player->loadedPath == fixture.library.tracks()[0].path,
            "Retry must restart the track after a synchronous load failure");
}

void previousUsesThreeSecondThreshold() {
    PlaybackFixture fixture;
    const auto& all = fixture.library.allTrackIndexes();
    fixture.controller->play(1, all);
    fixture.player->emit({PlayerEventType::FileLoaded, 1});
    fixture.player->emit({PlayerEventType::PositionChanged, 1, 8});
    fixture.controller->update();
    fixture.controller->previous();
    require(fixture.player->absoluteSeek == 0 && fixture.player->loadCount == 1,
            "Previous after three seconds must restart the current track");
    fixture.player->emit({PlayerEventType::PositionChanged, 1, 2});
    fixture.controller->update();
    fixture.controller->previous();
    require(fixture.player->loadCount == 2 && fixture.controller->queue().current() == 0,
            "Previous near the start must load the previous queue entry");
}

void shuffledCycleVisitsEveryTrack() {
    PlaybackQueue queue;
    queue.reset({0, 1, 2}, 0, true, 42);
    std::set<size_t> visited{queue.current()};
    while (const auto track = queue.next(false, 42)) visited.insert(*track);
    require(visited == std::set<size_t>({0, 1, 2}),
            "shuffle must visit every track once before finishing");
}

void repeatPoliciesRespectBoundaries() {
    PlaybackFixture fixture;
    fixture.controller->play(2, fixture.library.allTrackIndexes());
    fixture.player->emit({PlayerEventType::FileLoaded, 1});
    fixture.player->emit({PlayerEventType::Ended, 1});
    fixture.controller->update();
    require(fixture.controller->snapshot().phase == PlaybackPhase::Finished,
            "repeat off must finish at the queue boundary");

    fixture.controller->play(2, fixture.library.allTrackIndexes());
    fixture.controller->setRepeatMode(RepeatMode::One);
    fixture.player->emit({PlayerEventType::FileLoaded, 2});
    fixture.player->emit({PlayerEventType::Ended, 2});
    fixture.controller->update();
    require(fixture.player->loadCount == 3 && fixture.controller->queue().current() == 2,
            "repeat one must reload the same track after natural EOF");
}

void rapidControlsRemainCumulative() {
    PlaybackFixture fixture;
    fixture.controller->play(0, fixture.library.allTrackIndexes());
    fixture.player->emit({PlayerEventType::SeekableChanged, 1, 0, true});
    fixture.player->emit({PlayerEventType::FileLoaded, 1});
    fixture.controller->update();
    fixture.controller->togglePause();
    fixture.controller->togglePause();
    fixture.controller->seekRelative(10);
    fixture.controller->seekRelative(10);
    require(fixture.player->toggleCount == 2, "rapid pause toggles must both reach the player");
    require(fixture.player->relativeSeeks == std::vector<int>({10, 10}),
            "rapid relative seeks must remain cumulative commands");
}

void restoredQueueStartsPaused() {
    PlaybackFixture fixture;
    require(fixture.controller->restore({0, 1, 2}, {2, 0, 1}, {0}, 2, true, 18.5, "Album"),
            "valid queue session must restore");
    fixture.player->emit({PlayerEventType::FileLoaded, 1});
    fixture.controller->update();
    require(fixture.controller->snapshot().phase == PlaybackPhase::Paused,
            "restored playback must open paused");
    require(fixture.player->pausedValue && fixture.player->absoluteSeek == 18.5,
            "restore must apply pause and position after file-loaded");
    require(fixture.controller->queue().order() == std::vector<size_t>({2, 0, 1}) &&
                fixture.controller->sourceTitle() == "Album",
            "restore must retain play order and source title");
}

void automaticErrorsSkipWithBoundedAttempts() {
    PlaybackFixture fixture;
    fixture.controller->play(0, fixture.library.allTrackIndexes());
    fixture.player->emit({PlayerEventType::FileLoaded, 1});
    fixture.player->emit({PlayerEventType::Ended, 1});
    fixture.controller->update();
    fixture.player->emit({PlayerEventType::Failed, 2, 0, false, "broken two"});
    fixture.controller->update();
    require(fixture.player->loadCount == 3, "automatic playback must skip a broken next track");
    fixture.player->emit({PlayerEventType::Failed, 3, 0, false, "broken three"});
    fixture.controller->update();
    require(fixture.controller->snapshot().phase == PlaybackPhase::Error &&
                fixture.player->loadCount == 3,
            "automatic error skipping must stop at the queue boundary");
    fixture.controller->retry();
    require(fixture.player->loadCount == 4 &&
                fixture.player->loadedPath == fixture.library.tracks()[2].path,
            "Retry must reload the track that produced the visible error");
}

void togglingShufflePreservesCurrentTrack() {
    PlaybackQueue queue;
    queue.reset({0, 1, 2}, 1, false);
    queue.setShuffle(true, 7);
    require(queue.current() == 1, "enabling shuffle must preserve the current track");
    queue.setShuffle(false, 7);
    require(queue.current() == 1 && queue.order() == std::vector<size_t>({0, 1, 2}),
            "disabling shuffle must restore source order around the current track");
}

void shuffledPlaybackWrapsAfterEveryTrack() {
    PlaybackFixture fixture;
    fixture.controller->play(0, fixture.library.allTrackIndexes());
    fixture.controller->setShuffle(true, 42);
    fixture.controller->next();
    fixture.controller->next();
    fixture.controller->next();
    require(fixture.player->loadCount == 4,
            "shuffle next must start a new cycle after every track has played");
    require(fixture.controller->snapshot().phase != PlaybackPhase::Finished,
            "shuffle next must not stop at the queue boundary");
}

void duplicatePlaylistEntriesArePreserved() {
    PlaybackQueue queue;
    queue.reset({0, 1, 1, 2}, 2, true, 11);
    require(queue.sourcePosition() == 2,
            "queue selection must retain the selected duplicate entry identity");
    auto order = queue.order();
    std::sort(order.begin(), order.end());
    require(order == std::vector<size_t>({0, 1, 1, 2}),
            "shuffle must preserve duplicate playlist entries");
}
}  // namespace

void addPlaybackTests(TestCases& tests) {
    tests.emplace_back("load lifecycle", loadLifecycleAndStaleEvents);
    tests.emplace_back("async load error", loadingFailureIsVisible);
    tests.emplace_back("sync load retry", synchronousLoadFailureCanRetry);
    tests.emplace_back("previous threshold", previousUsesThreeSecondThreshold);
    tests.emplace_back("shuffle cycle", shuffledCycleVisitsEveryTrack);
    tests.emplace_back("repeat boundaries", repeatPoliciesRespectBoundaries);
    tests.emplace_back("rapid controls", rapidControlsRemainCumulative);
    tests.emplace_back("paused restore", restoredQueueStartsPaused);
    tests.emplace_back("bounded error skipping", automaticErrorsSkipWithBoundedAttempts);
    tests.emplace_back("shuffle toggle", togglingShufflePreservesCurrentTrack);
    tests.emplace_back("shuffle wraps", shuffledPlaybackWrapsAfterEveryTrack);
    tests.emplace_back("duplicate queue entries", duplicatePlaylistEntriesArePreserved);
}
