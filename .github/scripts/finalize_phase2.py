from pathlib import Path

plan_path = Path("REFACTOR-PLAN.md")
plan = plan_path.read_text()
for line in (
    "- [ ] Navigation no longer owns updater process/platform details.",
    "- [ ] OS process identifiers are not stored in UI/application state.",
    "- [ ] Update cancellation is non-blocking.",
):
    plan = plan.replace(line, line.replace("[ ]", "[x]"))

start = plan.index("# Phase 2 —")
end = plan.index("# Phase 3 —")
phase2 = plan[start:end].replace("- [ ]", "- [x]")
phase2 = phase2.replace(
    "- [x] Add a cancellation lifecycle test.\n",
    "- [x] Add a cancellation lifecycle test.\n\n"
    "Current cancellation policy: send SIGTERM immediately, keep the UI responsive, and escalate "
    "to SIGKILL after a 500 ms grace period if the preparer has not exited. Reaping remains in "
    "the regular non-blocking poll path.\n",
)
phase2 = phase2.replace(
    "- [x] Remove renderer-specific arbitration between unrelated state channels.\n",
    "- [x] Remove renderer-specific arbitration between unrelated state channels.\n\n"
    "Current notice policy: notices persist until a newer notice replaces them; update lifecycle "
    "state itself returns to `Idle` after emitting completion/error text.\n",
)
plan = plan[:start] + phase2 + plan[end:]

for finding in (
    "- [ ] `navigation.cpp` owns updater platform/process logic → 2.1",
    "- [ ] PID leaks into `AppState` → 2.2",
    "- [ ] Deep/repeated updater `getenv()` path lookup → 2.3",
    "- [ ] `fork()` in multithreaded SDL/audio process → 2.4",
    "- [ ] Blocking `waitpid()` on cancel → 2.5",
    "- [ ] Duplicate updater metadata formats → 2.6",
    "- [ ] `check-update.sh` name understates behavior → 2.7",
    "- [ ] Update result state doubles as notification system → 2.8",
    "- [ ] Managed installer path knowledge repeated → 2.9",
    "- [ ] Launcher orchestration not integration-tested → 2.10",
):
    plan = plan.replace(finding, finding.replace("[ ]", "[x]"))
plan_path.write_text(plan)

readme_path = Path("README.md")
readme = readme_path.read_text()
readme = readme.replace(
    "Select `Check for Updates` from Home. Pocket Music stays open while the update checker runs in the\n"
    "background.",
    "Select `Check for Updates` from Home. Pocket Music stays open while the update preparer runs in the\n"
    "background.",
)
readme = readme.replace(
    "The checker downloads release metadata", "The preparer downloads release metadata"
)
readme = readme.replace(
    "network/check process is active", "update preparation process is active"
)
readme = readme.replace(
    "The check path runs inside the application process lifecycle;",
    "The preparation path runs inside the application process lifecycle;",
)
old = """Native FFmpeg/SDL playback, Home/Songs/Now Playing navigation, controller input, paused session
restore, transactional OTA installation, in-app update loading phases, and the deferred install
handoff have been exercised on a TrimUI Brick Hammer. The current update UX was verified with a
real OTA from v0.2.5 to v0.2.6 without manually copying the application over SSH. Suspend/resume
behavior and every Stock OS firmware revision are not covered by automated tests."""
new = """Native FFmpeg/SDL playback, Home/Songs/Now Playing navigation, controller input, paused session
restore, transactional OTA installation, in-app update loading phases, and the deferred install
handoff have been exercised on a TrimUI Brick Hammer. The released v0.2.6 update UX was verified
with a real OTA from v0.2.5 to v0.2.6 without manually copying the application over SSH. The
post-v0.2.6 updater-internal refactor is covered by automated tests and the AArch64 target gate, but
still requires the final physical Brick smoke test. Suspend/resume behavior and every Stock OS
firmware revision are not covered by automated tests."""
if old not in readme:
    raise SystemExit("README hardware verification paragraph changed unexpectedly")
readme_path.write_text(readme.replace(old, new))
