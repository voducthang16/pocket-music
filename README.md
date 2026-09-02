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
- Open Songs, Now Playing, and the music-themed Liner Notes app information from Home
- Return from a restored Now Playing session to Home before exiting the app
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
| Escape or B | B | Back; exit from Home |
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

Folder names do not define navigation groups. Pocket Music recursively adds supported audio files
to Songs and uses file metadata only for track details.

## TrimUI Stock OS package

With Docker running:

```sh
make trimui-package
```

The package is written to `build/trimui/PocketMusic/`. Copy that `PocketMusic` directory to
`Apps/` on the Stock OS SD card. The launcher:

- reads music from `/mnt/SDCARD/Music`
- stores playback state in `Apps/PocketMusic/data`
- writes runtime output to `/mnt/SDCARD/pocket-music.log`
- uses SDL libraries supplied by Stock OS

The package cross-compiles as AArch64 C++17 and statically links the pinned FFmpeg and TagLib
builds. The macOS build uses C++20. Invalid playback state falls back safely to defaults.

Pushing a tag matching the CMake project version, such as `v0.1.0`, builds
`PocketMusic-v0.1.0.zip` and publishes it to GitHub Releases.

Runtime artwork is limited to `background-hello-kitty-v6.png`, `fallback-vinyl.png`, `icon.png`,
and the bundled Noto Sans font. `app-icon-source.png` is retained as the editable source for future
icon exports.

Native FFmpeg/SDL playback, Home/Songs navigation, controller input, and paused session restore have
been exercised on the TrimUI Brick Hammer. Suspend/resume behavior and every Stock OS firmware
revision are not covered by automated tests.

## Verification

```sh
make test
```

CTest runs:

- unit tests for scanning, navigation, input mapping, queue policies, persistence,
  theme/layout primitives, and playback lifecycle
- a smoke scan against `Music/`
- an FFmpeg/SDL integration test with a generated WAV and SDL's dummy audio driver

Verify the target build separately with `make trimui-package`. Physical display, controls,
speakers, and SD-card behavior still require a final check on the handheld.

## Licenses

The bundled Noto Sans font uses the SIL Open Font License in `assets/fonts/OFL.txt`. FFmpeg and
TagLib are fetched at pinned commits by the Docker build; review their upstream licenses when
changing build options or redistributing the package.
