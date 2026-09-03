# Pocket Music

Pocket Music is a lightweight, controller-first offline music player for the TrimUI Brick Hammer.
It runs as a native SDL application on TrimUI Stock OS and also builds on macOS for development.
The interface targets the Hammer's native 1024×768 landscape display.

Playback runs in-process through FFmpeg and SDL audio. The package does not launch mpv or depend on
an external media-player process.

## Features

- Browse every supported audio file from one Songs list
- Read Unicode title, artist, album, and duration metadata with TagLib
- Play MP3, FLAC, WAV, OGG Vorbis, and OGG Opus
- Show `cover.jpg`, `cover.png`, `folder.jpg`, or `folder.png` from an album directory
- Play/pause, seek, previous/next, shuffle, and repeat
- Visit every queue entry before starting a new shuffle cycle
- Preserve playback history and apply the three-second Previous behavior
- Restore queue, shuffle order, history, repeat mode, and position in a paused state
- Handle loading, finished, empty-library, and recoverable playback-error states
- Open Songs, Now Playing, and About from Home
- Return from a restored Now Playing session to Home before exiting the app
- Check, download, and verify remote updates without blocking or closing the UI
- Hand verified updates to the launcher for transactional installation and restart
- Navigate with a keyboard or SDL game controller
- Use a pastel vintage stationery art direction: warm ivory paper, blush watercolor, pressed
  flowers, soft cocoa details, and a small Hello Kitty cassette vignette kept outside the UI-safe
  area

Embedded cover art is not currently extracted; use one of the supported image filenames beside the
audio files.

## macOS development

Install dependencies:

```sh
brew install cmake ffmpeg sdl2 sdl2_image sdl2_ttf taglib
```

Build and run against the ignored `Music/` directory:

```sh
make run
```

Use a different library without changing source code:

```sh
make run MUSIC_DIR="/path/to/Music"
```

Other commands:

```sh
make build           # Configure and compile
make test            # Build and run all automated tests
make format          # Format C++ source and tests
make trimui-package  # Cross-build the Stock OS package with Docker
```

Direct invocation:

```sh
./build/pocket-music \
  --music "/path/to/Music" \
  --state "/path/to/playback-state"
```

Add `--fullscreen` to use the current desktop display.

## Controls

The face-button names refer to the labels printed on the Hammer. SDL reports both face-button
pairs in reverse (A/B and X/Y), and the input adapter normalizes that hardware detail.

| Keyboard | TrimUI Brick Hammer | Action |
| --- | --- | --- |
| Up / Down | D-pad Up / Down | Move selection |
| Left / Right | D-pad Left / Right | Seek backward / forward 10 seconds |
| Enter or A | A | Select / play |
| Escape or B | B | Back; open exit confirmation from Home |
| Space or S | Start / `+` | Play / pause; retry a failed load |
| Q / E | L1 / R1 | Previous / next track |
| X | X | Open Now Playing |
| Y | Y | Toggle shuffle |
| R | Select / `−` | Cycle Repeat Off → One → All |

Shuffle starts a new randomized cycle after every queue entry has played and avoids immediately
repeating the final entry of the previous cycle. Repeat All cycles the queue; Repeat One restarts
the current track after it ends.

## Music directory

Pocket Music scans recursively:

```text
Music/
  Artist/
    Album/
      cover.jpg
      01 - Song.mp3
```

Folder names do not currently define navigation groups. Pocket Music adds supported audio files to
Songs recursively. Title and artist prefer file metadata. Album prefers file metadata and falls
back to the audio file's parent directory name when the album tag is empty.

## TrimUI Stock OS package

With Docker running:

```sh
make trimui-package
```

The package is written to `build/trimui/PocketMusic/`. Copy that `PocketMusic` directory to
`Apps/` on the Stock OS SD card. The launcher:

- reads music from `/mnt/SDCARD/Music`
- stores playback state and updater data in `Apps/PocketMusic/data`
- writes runtime output to `/mnt/SDCARD/pocket-music.log`
- uses SDL libraries supplied by Stock OS
- recovers interrupted installs before launching the app
- applies verified updates after Pocket Music performs the install handoff

The package cross-compiles as AArch64 C++17 and statically links the pinned FFmpeg and TagLib
builds. It also includes a CA certificate bundle for verified HTTPS downloads because TrimUI Stock
OS may not provide one. The macOS build currently uses C++20. Invalid playback state falls back
safely to defaults.

Pushing a tag that matches the CMake project version publishes a full install ZIP, a verified OTA
archive, and update metadata to GitHub Releases. For example, a project version `0.2.6` must be
tagged `v0.2.6`.

### Remote updates

Select `Check for Updates` from Home. Pocket Music stays open while the update checker runs in the
background. The modal reflects the real preparation phases:

```text
Checking for Updates
        ↓
Downloading vX.Y.Z...
        ↓
Verifying update...
```

The checker downloads release metadata and the OTA archive over verified HTTPS, validates the
reported archive size and SHA-256 checksum, and only then exposes `Install Update` on Home. The
pending version is shown beside the install action. The check can be cancelled with B while the
network/check process is active.

Selecting `Install Update` first presents the install handoff modal. Pocket Music renders that
state before exiting; the launcher then owns the filesystem transaction. Installation preserves
`Apps/PocketMusic/data`, backs up the managed application files, replaces them transactionally, and
can restore the previous installation after a failed or interrupted update. The launcher restarts
Pocket Music after the transaction and the app displays the resulting update status.

The check path runs inside the application process lifecycle; the launcher no longer performs a
second legacy update check after Pocket Music exits. The launcher remains responsible for install
handoff and interrupted-update recovery.

Runtime artwork is limited to `background-hello-kitty-v6.png`, `fallback-vinyl.png`, `icon.png`,
and the bundled Noto Sans font. `app-icon-source.png` is retained as the editable source for future
icon exports.

Native FFmpeg/SDL playback, Home/Songs/Now Playing navigation, controller input, paused session
restore, transactional OTA installation, in-app update loading phases, and the deferred install
handoff have been exercised on a TrimUI Brick Hammer. The current update UX was verified with a
real OTA from v0.2.5 to v0.2.6 without manually copying the application over SSH. Suspend/resume
behavior and every Stock OS firmware revision are not covered by automated tests.

## Verification

```sh
make test
```

CTest runs:

- unit tests for scanning, navigation, input mapping, queue policies, persistence,
  theme/layout/presentation primitives, and playback lifecycle
- a smoke scan against a generated empty music directory
- an FFmpeg/SDL integration test with a generated WAV and SDL's dummy audio driver

Verify the target build separately with `make trimui-package`. Physical display, controls,
speakers, and SD-card behavior still require a final check on the handheld after TrimUI-sensitive
changes.

## License

Pocket Music source code is available under the [MIT License](LICENSE). You may use, modify,
redistribute, and use the code commercially as long as the copyright and license notice remain
with the software. The software is provided without warranty.

The bundled Noto Sans font uses the SIL Open Font License in `assets/fonts/OFL.txt`. FFmpeg and
TagLib are fetched at pinned commits by the Docker build; review their upstream licenses when
changing build options or redistributing the package. Character-inspired artwork and third-party
marks are not covered by the MIT License and remain subject to their respective owners' rights.
