#!/bin/sh

APP_DIR=$(dirname "$0")
SD_ROOT=/mnt/SDCARD
DATA_DIR="$APP_DIR/data"
LOG_FILE="$SD_ROOT/pocket-music.log"
PENDING_UPDATE="$DATA_DIR/update/pending-update"

mkdir -p "$DATA_DIR" "$SD_ROOT/Music"
cd "$APP_DIR" || exit 1

export HOME="$DATA_DIR"
export PATH="$APP_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$APP_DIR/lib:/usr/trimui/lib:$LD_LIBRARY_PATH"
export POCKET_MUSIC_FONT="$APP_DIR/assets/fonts/NotoSans-Regular.ttf"
export POCKET_MUSIC_APP_DIR="$APP_DIR"
export POCKET_MUSIC_DATA_DIR="$DATA_DIR"
export POCKET_MUSIC_UPDATE_CHECKER="$APP_DIR/update/check-update.sh"

"$APP_DIR/bin/pocket-music" \
  --music "$SD_ROOT/Music" \
  --state "$DATA_DIR/playback-state" \
  --fullscreen >"$LOG_FILE" 2>&1
APP_STATUS=$?

if [ -f "$PENDING_UPDATE" ] && [ -x "$APP_DIR/update/apply-update.sh" ]; then
  cp "$APP_DIR/update/apply-update.sh" "$DATA_DIR/update/apply-update.sh"
  chmod +x "$DATA_DIR/update/apply-update.sh"
  if "$DATA_DIR/update/apply-update.sh" "$APP_DIR" "$DATA_DIR" >>"$LOG_FILE" 2>&1; then
    exec "$APP_DIR/launch.sh"
  fi
fi

sync
exit "$APP_STATUS"
