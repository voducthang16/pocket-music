#!/bin/sh

APP_DIR=$(dirname "$0")
SD_ROOT=/mnt/SDCARD
DATA_DIR="$APP_DIR/data"
LOG_FILE="$SD_ROOT/pocket-music.log"
UPDATE_DIR="$DATA_DIR/update"
CHECK_REQUESTED="$UPDATE_DIR/check-requested"
PENDING_UPDATE="$UPDATE_DIR/pending-update"
STATUS_FILE="$UPDATE_DIR/last-status"

mkdir -p "$DATA_DIR" "$SD_ROOT/Music" "$UPDATE_DIR"
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

if [ -f "$CHECK_REQUESTED" ] && [ -x "$APP_DIR/update/check-update.sh" ]; then
  rm -f "$CHECK_REQUESTED"
  CURRENT_VERSION=$("$APP_DIR/bin/pocket-music" --version 2>/dev/null)
  CHECK_OUTPUT=$("$APP_DIR/update/check-update.sh" "$CURRENT_VERSION" "$APP_DIR" "$DATA_DIR" 2>&1)
  CHECK_STATUS=$?
  printf '%s\n' "$CHECK_OUTPUT" | tail -n 1 > "$STATUS_FILE"

  if [ "$CHECK_STATUS" -eq 10 ] && [ -f "$PENDING_UPDATE" ]; then
    cp "$APP_DIR/update/apply-update.sh" "$UPDATE_DIR/apply-update.sh"
    chmod +x "$UPDATE_DIR/apply-update.sh"
    if APPLY_OUTPUT=$("$UPDATE_DIR/apply-update.sh" "$APP_DIR" "$DATA_DIR" 2>&1); then
      printf '%s\n' "$APPLY_OUTPUT" | tail -n 1 > "$STATUS_FILE"
    else
      printf '%s\n' "$APPLY_OUTPUT" | tail -n 1 > "$STATUS_FILE"
    fi
  fi

  exec "$APP_DIR/launch.sh"
fi

sync
exit "$APP_STATUS"
