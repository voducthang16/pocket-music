# Pocket Music Codebase Cleanup Plan

This document is the source of truth for the early-stage cleanup/refactor pass that starts after v0.2.6.

## Refactor policy

Pocket Music is still early-stage. This cleanup intentionally optimizes for a small, clear codebase rather than compatibility with old internal APIs, old state-file schemas, or superseded updater protocols.

- Breaking internal changes are allowed.
- Backward-compatibility shims are not required.
- Dead code should be deleted instead of deprecated.
- Old persistence/update schemas may be replaced instead of migrated.
- Do not preserve abstractions that exist only for hypothetical future features.
- Preserve current user-facing behavior only when it is still intentional.
- Preserve the verified safety properties of the transactional TrimUI installer: protected `data`, archive verification, backup, rollback, interrupted-update recovery, and HTTPS CA verification.
- Keep feature work such as Albums/Artists out of this cleanup until the baseline is clean.
- Each phase should remain reviewable and should finish with automated tests passing. TrimUI-sensitive changes also require a real AArch64 cross-build before merge.

## Definition of done

The cleanup is complete when:

- [x] No known legacy updater path remains.
- [ ] No known persisted-but-unused state remains.
- [x] Navigation no longer owns updater process/platform details.
- [x] OS process identifiers are not stored in UI/application state.
- [x] Update cancellation is non-blocking.
- [x] Playback status wording is derived from one canonical mapping.
- [ ] View models do not duplicate track metadata unnecessarily.
- [ ] SDL/audio lifetime is RAII-driven rather than manually invalidating controller internals.
- [ ] Production source lists and compiler settings are not duplicated between app and tests.
- [ ] Desktop development uses the same C++ language level as TrimUI.
- [ ] CI and release verification share one project-level verification entry point.
- [x] README describes the current implementation rather than historical updater behavior.
- [x] `make test` passes.
- [x] `make trimui-package` passes after TrimUI-sensitive refactors.
- [ ] Final physical Brick smoke test passes.

---

# Phase 1 — Dead code, wording, and source-of-truth cleanup

Goal: remove confirmed legacy/dead pieces and fix places where code/documentation currently says something different from what the application actually does. Avoid architectural redesign in this phase.

## 1.1 Remove legacy launcher-driven update-check protocol

Finding: update checking now runs inside the application as a background process, but `launch.sh` still contains the pre-v0.2.3 `check-requested` path that checks only after the app exits.

- [x] Remove `CHECK_REQUESTED` from `platform/trimui/launch.sh`.
- [x] Remove the launcher block that executes `check-update.sh` after app exit.
- [x] Remove tests/fixtures that exist only for `check-requested`, if any.
- [x] Search the repository and verify no production reference to `check-requested` remains.

Acceptance:

- [x] In-app Check for Updates remains functional.
- [x] Launcher remains responsible for install handoff/recovery only.

## 1.2 Remove persisted-but-unused `PlaybackSession.paused`

Finding: the state writer persists `paused`, but the loader unconditionally forces restored sessions to paused, so the persisted value is ignored.

- [x] Remove `PlaybackSession::paused`.
- [x] Remove `paused` parsing from the state file.
- [x] Remove `paused` serialization.
- [x] Update state tests to test the intended rule directly: restored playback always starts paused.
- [x] Do not add migration support for old state files.

Acceptance:

- [x] Session restore still starts paused.
- [x] No `paused` key exists in the new durable state format.

## 1.3 Remove unused `PlayerEventType::Disconnected`

Finding: the current in-process FFmpeg/SDL player never emits `Disconnected`; it is a leftover abstraction from an external/disconnectable player model.

- [x] Remove `PlayerEventType::Disconnected`.
- [x] Remove its handling branch from `PlaybackController`.
- [x] Confirm no test or production emitter references it.

## 1.4 Remove trivial dead/no-op code

- [x] Remove `need_command tar` from `check-update.sh`; that script never invokes `tar`.
- [x] Remove no-op status expressions such as `[ "$APPLY_STATUS" -eq 0 ] || true` when the value has no effect.
- [x] Re-evaluate `$APP_DIR/lib` in `LD_LIBRARY_PATH`; remove it if the package still ships no `lib` directory.
- [x] Remove unused includes discovered during audit (`<vector>` in `model.hpp`, `ui/layout.hpp` in `input.cpp`, `<cerrno>` where unused, etc.).
- [x] Remove unused local variables such as the unused marquee text height if still present.
- [x] Run formatting after cleanup.

Acceptance:

- [x] Build is warning-clean under existing `-Wall -Wextra -Wpedantic` settings.

## 1.5 Fix Home update-version rendering

Finding: the Home view stores the pending update version in the Install Update item's subtitle, but the Home renderer only renders the subtitle/trailing text for row 0.

- [x] Render the pending version for `Install Update` from the actual item data, not its row number.
- [x] Avoid renderer rules that depend on magic row indexes.
- [x] Add/update a test that covers the rendered/model contract rather than only checking the `ViewItem` subtitle exists.

Acceptance:

- [x] Home visibly shows the pending update version next to Install Update.

## 1.6 Centralize playback status wording

Finding: Home mini-player and Now Playing infer status independently. Non-paused states can currently be labelled `PLAYING`/`PAUSE` even when playback is Loading, Error, or Finished.

- [x] Define one canonical presentation mapping for `PlaybackPhase`.
- [x] Cover at least Idle, Loading, Playing, Paused, Finished, and Error.
- [x] Use the same mapping in mini-player and Now Playing.
- [x] Ensure transport/action wording does not imply playback is active during Loading/Error/Finished.
- [x] Add focused tests for the mapping.

Suggested semantic statuses:

- `Idle` → `READY`
- `Loading` → `LOADING`
- `Playing` → `PLAYING`
- `Paused` → `PAUSED`
- `Finished` → `FINISHED`
- `Error` → `PLAYBACK ERROR`

## 1.7 Fix inconsistent UI wording

- [x] Replace `Choose a song from Home` because tracks are selected from Songs.
- [x] Make the empty Now Playing messages internally consistent.
- [x] Review Check/Install update strings for consistent capitalization and terminology.
- [x] Prefer short handheld-friendly wording over implementation terminology.

## 1.8 Rename `Liner Notes` to `About`

Finding: the screen contains app version, author, source, music folder, and supported formats; semantically it is an About screen, not liner notes.

- [x] Rename `Screen::LinerNotes` to `Screen::About`.
- [x] Rename `ViewAction::OpenLinerNotes` to `ViewAction::OpenAbout`.
- [x] Rename renderer/source functions/files where appropriate.
- [x] Change Home label to `About`.
- [x] Replace the hard-coded `/mnt/SDCARD/Music` display with `MusicLibrary::root()` or another actual runtime path source.
- [x] Update tests and README terminology.

Acceptance:

- [x] Desktop/custom `--music` paths are represented correctly in About.

## 1.9 Rewrite stale README sections to current truth

Known stale content includes the old update behavior and terminology.

- [x] Use `Check for Updates` consistently.
- [x] Remove the statement that Pocket Music exits before network work.
- [x] Document the current in-app Checking → Downloading → Verifying flow.
- [x] Document the install modal/deferred handoff and launcher-owned transaction.
- [x] Remove historical pre-v0.2.2 compatibility/bootstrap guidance if it is no longer a supported contract.
- [x] Update examples that unnecessarily anchor documentation to v0.2.2.
- [x] Clarify metadata fallbacks: album may fall back to parent directory name.
- [x] Defer verified-on-device wording until the final physical Brick test in 4.6; do not claim unverified hardware behavior.

Phase 1 gate:

- [x] `make format`
- [x] `make test`
- [x] TrimUI updater integration test passes.
- [x] No new feature work included.

---

# Phase 2 — Update subsystem architecture cleanup

Goal: make updating a self-contained subsystem. Navigation and UI should consume semantic update state, not own POSIX process mechanics or updater filesystem protocol details.

## 2.1 Introduce `UpdateController`

Current problem: `navigation.cpp` currently owns app/data path lookup, pending manifest parsing, process spawning, process polling, cancellation, phase parsing, and install handoff.

Target shape:

```text
src/update/
  update_controller.hpp
  update_controller.cpp
  update_state.hpp
```

- [x] Move updater path/protocol/process logic out of `navigation.cpp`.
- [x] Keep navigation responsible only for user actions and view transitions.
- [x] Expose semantic operations such as `check()`, `poll()`, `cancel()`, and `requestInstall()`.
- [x] Expose read-only semantic status to the UI.

## 2.2 Remove OS PID from `AppState`

Finding: `UpdateState::processId` leaks process implementation details into application/UI state.

- [x] Remove `processId` from `UpdateState`.
- [x] Store process ownership privately inside `UpdateController`.
- [x] UI state must contain only phase/version/detail or a smaller semantic representation.

## 2.3 Introduce explicit runtime/platform paths

Finding: updater code repeatedly reads environment variables (`POCKET_MUSIC_APP_DIR`, `POCKET_MUSIC_DATA_DIR`, checker override) deep inside navigation helpers.

- [x] Resolve runtime paths once near application startup.
- [x] Pass/inject them into the update subsystem.
- [x] Keep environment variables at the platform/bootstrap boundary.
- [x] Preserve a clean test injection mechanism without production code repeatedly calling `getenv()`.

## 2.4 Replace `fork()` + `execl()` with a spawn API

Finding: Pocket Music is multithreaded because the audio backend has a worker thread. A direct `fork()` from that process is avoidable.

- [x] Replace the checker launch path with `posix_spawn()`/`posix_spawnp()` if supported by the TrimUI toolchain/runtime.
- [x] Redirect checker stdout/stderr to the update log using spawn file actions.
- [x] Preserve checker process-group cancellation semantics where practical.
- [x] Validate the implementation with the actual AArch64 cross-toolchain.

Acceptance:

- [x] No `fork()` remains in updater production code.

## 2.5 Make cancellation non-blocking

Finding: current cancel sends SIGTERM and immediately performs a blocking `waitpid(..., 0)` on the SDL/UI thread.

- [x] Add an explicit cancellation state or cancellation flag.
- [x] Send termination without blocking the input handler.
- [x] Reap the child from the regular poll path with `WNOHANG`.
- [x] Define behavior if the child takes longer than expected to terminate.
- [x] Add a cancellation lifecycle test.

Current cancellation policy: send SIGTERM immediately, keep the UI responsive, and escalate to SIGKILL after a 500 ms grace period if the preparer has not exited. Reaping remains in the regular non-blocking poll path.

Acceptance:

- [x] The UI loop never blocks waiting for update-check process termination.

## 2.6 Use one canonical pending-update manifest

Finding: downloaded release metadata and `pending-update` duplicate version/asset/checksum information, and pending metadata currently drops size.

- [x] Define one pending manifest schema containing every field required for install verification.
- [x] Write it atomically only after download and verification succeed.
- [x] Keep `size` in the verified pending manifest.
- [x] Remove redundant intermediate durable metadata where unnecessary.
- [x] No backward parser for the previous pending format is required.

## 2.7 Rename backend checker to match its real responsibility

Finding: `check-update.sh` also downloads and verifies the full update archive.

Candidate terminology:

- UI: `Check for Updates`
- backend: `prepare-update.sh`
- controller: `prepareUpdate()` / `check()` as appropriate

- [x] Rename the script and package references if the resulting terminology is clearer.
- [x] Update Docker packaging, launcher exports, tests, and documentation in the same change.
- [x] Do not retain a compatibility copy/symlink under the old script name.

## 2.8 Separate update state from transient notices

Finding: completed update status currently remains in `UpdateState`, while generic playback/application errors use `app.message`; renderer owns priority between both channels.

- [x] Define a small application notice/banner model if banners are still desired.
- [x] Let the update subsystem emit a completion notice instead of indefinitely owning banner text.
- [x] Give notices an explicit dismissal/lifetime policy or intentionally persistent semantics.
- [x] Remove renderer-specific arbitration between unrelated state channels.

Current notice policy: notices persist until a newer notice replaces them; update lifecycle state itself returns to `Idle` after emitting completion/error text.

## 2.9 Deduplicate managed updater paths

Finding: installer knowledge of managed directories/files is repeated across backup/restore/install logic.

- [x] Define managed directories once.
- [x] Define managed files once.
- [x] Reuse those definitions for backup, restore, cleanup, and install operations.
- [x] Preserve `launch.sh` replacement ordering needed for safe handoff.

## 2.10 Add launcher orchestration integration coverage

Finding: updater integration tests call checker/applier directly but do not execute the real app-exit → marker → launcher → installer → restart orchestration.

- [x] Make TrimUI launcher filesystem roots injectable/testable where necessary.
- [x] Add an integration fixture for `install-requested` handoff.
- [x] Verify launcher consumes the marker.
- [x] Verify installer runs and protected data survives.
- [x] Verify restart/relaunch behavior without touching real `/mnt/SDCARD` in CI.

Phase 2 gate:

- [x] `make format`
- [x] `make test`
- [x] Shell syntax validation passes.
- [x] Full updater integration passes.
- [x] `make trimui-package` passes with actual AArch64 toolchain.
- [x] No old update protocol compatibility path remains.

---

# Phase 3 — Application, navigation, view-model, and playback cleanup

Goal: reduce coupling before Albums/Artists or other library features expand the UI model.

## 3.1 Replace controller-to-keyboard emulation with semantic input actions

Finding: controller buttons are currently translated into artificial SDL keycodes, so physical input mapping and application intent are mixed together.

Target idea:

```cpp
enum class InputAction {
    Up,
    Down,
    SeekBack,
    SeekForward,
    Confirm,
    Back,
    PlayPause,
    Previous,
    Next,
    NowPlaying,
    Shuffle,
    Repeat,
};
```

- [x] Map keyboard input to `InputAction`.
- [x] Map TrimUI/SDL controller input to `InputAction`.
- [x] Handle application behavior once from semantic actions.
- [x] Keep physical TrimUI A/B/X/Y reversal isolated in the controller adapter.

## 3.2 Replace generic `ViewItem` track/menu union

Finding: one `ViewItem` type represents both menu commands and tracks using optional fields and invalid combinations. Songs also duplicates title/artist into the item then re-reads metadata at render time.

- [x] Separate menu-item state from track-list state.
- [x] Store track IDs/indexes for Songs rather than duplicated title/artist strings.
- [x] Render current metadata from `MusicLibrary` as the source of truth.
- [x] Avoid row-index-based semantics.
- [x] Design this only for current needs; do not pre-build Albums/Artists in this phase.

## 3.3 Remove `MusicLibrary::allTrackIndexes_` cached identity vector

Finding: it is always `0..N-1` and duplicates the shape of `tracks_`.

- [x] Remove the stored identity vector.
- [x] Generate a queue/source sequence at the call site or via a lightweight helper when needed.
- [x] Keep `MusicLibrary` focused on track/library data.

## 3.4 Re-evaluate duplicate queue-entry support

Finding: `PlaybackQueue` contains non-trivial position/count mapping specifically to preserve duplicate track IDs, although current UI cannot create duplicate entries.

- [ ] Decide explicitly whether duplicate queue entries are a current product requirement.
- [ ] If not required now, simplify queue representation and remove duplicate-entry complexity/tests.
- [ ] Do not preserve hypothetical playlist behavior prematurely.
- [ ] If duplicates are later needed, model real queue-entry identity rather than implicit duplicate source positions.

## 3.5 Simplify durable playback session schema

- [ ] Define a small explicit schema/version for the new state file.
- [ ] Persist only data required for current restore behavior.
- [ ] Remove unused/historical fields such as `paused` and any source metadata proven unnecessary.
- [ ] Reject/discard unsupported schemas instead of migrating them.
- [ ] Keep consistency validation for the new schema.

## 3.6 Remove manual `PlaybackController::shutdown()` invalid state

Finding: `shutdown()` simply destroys `player_`, leaving a live controller that can no longer safely perform player operations. It exists only to force player destruction before `SDL_Quit()`.

- [ ] Redesign application/SDL runtime lifetime so playback is naturally destroyed before SDL teardown.
- [ ] Remove `PlaybackController::shutdown()`.
- [ ] Ensure no controller method can observe a null player caused by lifecycle teardown.
- [ ] Keep/replace the test proving the audio backend is destroyed before SDL shutdown.

## 3.7 Reduce `AppState` ownership breadth

Current `AppState` owns SDL resources, library, playback, updater UI state, navigation, exit dialog, notices, texture cache, controllers, and animation state.

- [ ] Identify resources that have their own natural owner/lifetime.
- [ ] Prefer scoped runtime/resource objects over a growing public struct.
- [ ] Keep the final state shape pragmatic; do not introduce framework-like abstractions solely for purity.

## 3.8 Narrow renderer internal APIs

- [ ] Remove helpers from `renderer_internal.hpp` that are only used within one `.cpp`.
- [ ] Move file-local helpers into anonymous namespaces.
- [ ] Keep shared renderer API explicit and minimal.

## 3.9 Simplify one-theme abstraction

Finding: `resolveTheme()` always returns one compile-time palette.

- [ ] Replace it with a simple constant/value if there is still only one theme after the cleanup.
- [ ] Reintroduce theme resolution only when multiple themes actually exist.

Phase 3 gate:

- [ ] `make format`
- [ ] `make test`
- [ ] Playback tests cover all intended queue/repeat/shuffle/restore semantics after simplification.
- [ ] Manual desktop smoke test for keyboard/controller navigation.
- [ ] `make trimui-package` if lifecycle/input/platform-sensitive code changed.

---

# Phase 4 — Build, CI, reproducibility, and verification cleanup

Goal: remove build/test duplication and make the constrained TrimUI target define compatibility early in development.

## 4.1 Use one C++ standard everywhere

Finding: desktop builds use C++20 while TrimUI uses C++17, allowing desktop-only language features to pass local tests and fail later during cross-build.

- [ ] Use C++17 for both desktop and TrimUI unless a concrete target requirement changes.
- [ ] Remove conditional language-level selection.
- [ ] Verify macOS/Linux development still builds cleanly.

## 4.2 Deduplicate production source lists in CMake

Finding: production `.cpp` files and compiler configuration are repeated between `pocket-music` and `pocket-music-tests`.

- [ ] Create an internal/core static or object library for shared production sources.
- [ ] Link the app executable to it.
- [ ] Link unit tests to it.
- [ ] Keep platform/UI dependencies attached at the narrowest appropriate target.
- [ ] Remove duplicated source/compiler-definition lists.

## 4.3 Add one project verification command

Target idea:

```text
make verify
  → shell syntax
  → updater integration
  → C++ build/tests
```

- [ ] Add a single local verification entry point.
- [ ] Make CI call it.
- [ ] Make release workflow call it before cross-building/package creation.
- [ ] Keep `make test` available for fast C++ test work if useful.

## 4.4 Deduplicate CI/release dependency setup pragmatically

- [ ] Avoid copying verification logic between `.github/workflows/ci.yml` and `release.yml`.
- [ ] Prefer project scripts/Make targets over a complicated reusable-workflow framework.
- [ ] Keep release-only steps limited to cross-build, artifact creation, and publishing after verification succeeds.

## 4.5 Pin TrimUI Docker toolchain reproducibly

Finding: FFmpeg/TagLib commits are pinned, but the base image is `ghcr.io/loveretro/tg5040-toolchain:latest`.

- [ ] Resolve a known-good immutable image digest or stable version tag.
- [ ] Use it consistently in all Dockerfile stages.
- [ ] Record the reason/update procedure in developer documentation.

## 4.6 Final test and documentation pass

- [ ] Run `make verify`.
- [ ] Run `make trimui-package`.
- [ ] Confirm output binary is AArch64.
- [ ] Review generated package contents for obsolete paths/files.
- [ ] Review all user-facing strings once more.
- [ ] Review README against code, not prior release history.
- [ ] Perform physical Brick smoke test: launch, library scan, playback, pause/seek/next/previous, shuffle/repeat, restore, Check for Updates, cancel check, Install Update, restart/status.

---

# Deferred feature work

These are intentionally not part of the cleanup and should start only after the relevant cleanup phases are complete:

- [ ] Albums
- [ ] Artists
- [ ] Embedded cover art
- [ ] Volume control + persistence
- [ ] Favorites
- [ ] Search
- [ ] Playlists
- [ ] Recently Played
- [ ] Suspend/resume hardening

The cleanup should make these features easier to add; it should not implement their speculative abstractions in advance.

---

# Audit findings index

This index keeps the original audit findings traceable to tasks above.

- [x] Legacy `check-requested` launcher path → 1.1
- [x] Persisted-but-unused `paused` field → 1.2 / 3.5
- [x] Unused `Disconnected` player event → 1.3
- [x] Checker `tar` dependency with no use → 1.4
- [x] Launcher no-op exit-status expressions → 1.4
- [x] Possibly unused `$APP_DIR/lib` runtime path → 1.4
- [x] Dead includes/locals → 1.4
- [x] Install Update version stored but not rendered correctly → 1.5
- [x] Duplicate/inaccurate playback status wording → 1.6
- [x] `Choose a song from Home` wording mismatch → 1.7
- [x] `Liner Notes` is semantically an About screen → 1.8
- [x] Hard-coded music path in app info → 1.8
- [x] Stale README updater behavior/terminology → 1.9
- [x] `navigation.cpp` owns updater platform/process logic → 2.1
- [x] PID leaks into `AppState` → 2.2
- [x] Deep/repeated updater `getenv()` path lookup → 2.3
- [x] `fork()` in multithreaded SDL/audio process → 2.4
- [x] Blocking `waitpid()` on cancel → 2.5
- [x] Duplicate updater metadata formats → 2.6
- [x] `check-update.sh` name understates behavior → 2.7
- [x] Update result state doubles as notification system → 2.8
- [x] Managed installer path knowledge repeated → 2.9
- [x] Launcher orchestration not integration-tested → 2.10
- [x] Controller input translated through fake keyboard events → 3.1
- [x] Generic `ViewItem` duplicates/mixes track and menu data → 3.2
- [x] Cached `allTrackIndexes_` identity vector → 3.3
- [ ] Duplicate-entry queue complexity before playlists exist → 3.4
- [ ] Playback state schema carries historical complexity → 3.5
- [ ] Manual `PlaybackController::shutdown()` creates invalid half-alive object → 3.6
- [ ] `AppState` owns too many unrelated resources → 3.7
- [ ] Renderer internal header exports implementation details → 3.8
- [ ] Single fixed theme behind premature resolver abstraction → 3.9
- [ ] C++17/C++20 target split → 4.1
- [ ] CMake app/test source-list duplication → 4.2
- [ ] CI/release verification duplication → 4.3 / 4.4
- [ ] Docker base toolchain uses mutable `latest` → 4.5
