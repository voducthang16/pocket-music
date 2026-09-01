# Pocket Music

A lightweight, controller-first offline music player for Linux handhelds. The current MVP runs on macOS for rapid development; TrimUI Brick Hammer Stock OS is the first hardware target.

The agreed product scope and acceptance criteria are documented in [`docs/MVP.md`](docs/MVP.md).

The UI bundles Noto Sans under the SIL Open Font License so text renders consistently across
development and handheld targets.

## MVP features

- Browse songs, albums, artists, and M3U/M3U8 playlists
- Read Unicode metadata and duration with TagLib
- Play MP3, FLAC, WAV, and OGG through mpv
- Display `cover.jpg`, `cover.png`, `folder.jpg`, or `folder.png` beside an album
- Play/pause, seek, previous/next, shuffle, and repeat state
- Deterministic shuffle cycles, playback history, and three-second Previous behavior
- Restore the queue, play order, history, and exact position in a paused state
- Display loading, finished, and recoverable playback-error states
- Keyboard and SDL game-controller navigation
- In-app Dark and Light themes with persistent preferences

Embedded cover art and the final Hammer launcher/button map belong to the hardware-validation checkpoint.

## macOS setup

The shortest development command is:

```sh
make run
```

It configures CMake, compiles only changed source files, and opens the app using `Music/`.
Other useful shortcuts:

```sh
make build   # Build without opening the app
make test    # Build and run tests
make format  # Format C++ source
make help    # Show all shortcuts
```

To use another music directory:

```sh
make run MUSIC_DIR="/path/to/music"
```

The equivalent manual setup is:

```sh
brew install cmake sdl2 sdl2_ttf sdl2_image taglib mpv
cmake -S . -B build
cmake --build build
```

Put music under `Music/`, or point at another directory:

```sh
./build/pocket-music --music "/path/to/Music"
./build/pocket-music --music "/path/to/Music" --fullscreen
```

Pocket Music stores appearance preferences separately from playback state. Use
`--preferences "/path/to/preferences"` to override the default preferences file.

## Controls

| Keyboard | Hammer target | Action |
| --- | --- | --- |
| Arrow keys | D-pad | Navigate / seek 10 seconds |
| Enter or A | A | Select / play |
| Escape or B | B | Back / exit from main menu |
| Space or S | Start | Play / pause |
| Q / E | L1 / R1 | Previous / next track |
| X | X | Now Playing |
| Y | Y | Toggle shuffle |
| R | To validate | Cycle repeat mode |

## Music card layout

```text
Music/
  Artist/
    Album/
      cover.jpg
      01 - Song.mp3
  Favorites.m3u
```

## Verify

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The test suite covers safe library discovery, cached groups, BOM-aware M3U playlists, atomic and
validated Unicode session persistence, typed mpv events, navigation history, shuffle/repeat queue
semantics, stale-load races, and failed audio loads. When mpv is installed, CTest also runs a real
IPC integration test with a generated WAV fixture. Physical controls remain a runtime check on the
target handheld.

The vendored JSON decoder is nlohmann/json 3.12.0 under the MIT License; its license is stored next
to the single-header dependency in `third_party/nlohmann/`.

## Hammer checkpoint

The device build still requires verification of its ARM userspace, bundled libraries, Stock OS app manifest/launcher format, physical key codes, audio output, suspend behavior, and SD-card paths. Those details should be measured on the real Hammer rather than guessed on macOS.
