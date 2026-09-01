#!/bin/sh

APP_DIR=$(dirname "$0")
SD_ROOT=/mnt/SDCARD
DATA_DIR="$APP_DIR/data"
LOG_FILE="$SD_ROOT/pocket-music.log"

mkdir -p "$DATA_DIR" "$SD_ROOT/Music"
cd "$APP_DIR" || exit 1

export HOME="$DATA_DIR"
export PATH="$APP_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$APP_DIR/lib:/usr/trimui/lib:$LD_LIBRARY_PATH"
export POCKET_MUSIC_FONT="$APP_DIR/assets/fonts/NotoSans-Regular.ttf"

"$APP_DIR/bin/pocket-music" \
  --music "$SD_ROOT/Music" \
  --state "$DATA_DIR/playback-state" \
  --preferences "$DATA_DIR/preferences" \
  --fullscreen >"$LOG_FILE" 2>&1

sync
