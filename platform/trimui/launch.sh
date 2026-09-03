#!/bin/sh

APP_DIR=$(dirname "$0")
SD_ROOT=/mnt/SDCARD
DATA_DIR="$APP_DIR/data"
LOG_FILE="$SD_ROOT/pocket-music.log"
UPDATE_DIR="$DATA_DIR/update"
CHECK_REQUESTED="$UPDATE_DIR/check-requested"
INSTALL_REQUESTED="$UPDATE_DIR/install-requested"
PENDING_UPDATE="$UPDATE_DIR/pending-update"
IN_PROGRESS="$UPDATE_DIR/update-in-progress"
RECOVERY_HELPER="$UPDATE_DIR/apply-update.sh"
STATUS_FILE="$UPDATE_DIR/last-status"
UPDATE_LOG="$UPDATE_DIR/update.log"

mkdir -p "$DATA_DIR" "$SD_ROOT/Music" "$UPDATE_DIR"
cd "$APP_DIR" || exit 1

write_update_result() {
  printf '%s\n' "$1" > "$UPDATE_LOG"
  printf '%s\n' "$1" | tail -n 1 > "$STATUS_FILE"
}

recover_interrupted_update() {
  [ -f "$IN_PROGRESS" ] || return 0

  if [ ! -x "$RECOVERY_HELPER" ]; then
    write_update_result "Interrupted Pocket Music update needs manual recovery"
    sync
    return 2
  fi

  RECOVERY_OUTPUT=$("$RECOVERY_HELPER" --recover "$APP_DIR" "$DATA_DIR" 2>&1)
  RECOVERY_STATUS=$?
  write_update_result "$RECOVERY_OUTPUT"
  sync
  return "$RECOVERY_STATUS"
}

if ! recover_interrupted_update; then
  exit 2
fi

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

if [ -f "$INSTALL_REQUESTED" ]; then
  rm -f "$INSTALL_REQUESTED"

  if [ ! -f "$PENDING_UPDATE" ]; then
    write_update_result "No Pocket Music update is ready to install"
  elif [ ! -x "$APP_DIR/update/apply-update.sh" ]; then
    write_update_result "Pocket Music updater is unavailable"
  else
    cp "$APP_DIR/update/apply-update.sh" "$RECOVERY_HELPER.tmp"
    chmod +x "$RECOVERY_HELPER.tmp"
    mv -f "$RECOVERY_HELPER.tmp" "$RECOVERY_HELPER"
    sync

    APPLY_OUTPUT=$("$RECOVERY_HELPER" "$APP_DIR" "$DATA_DIR" 2>&1)
    APPLY_STATUS=$?
    write_update_result "$APPLY_OUTPUT"
    [ "$APPLY_STATUS" -eq 0 ] || true
  fi

  sync
  exec "$APP_DIR/launch.sh"
fi

if [ -f "$CHECK_REQUESTED" ] && [ -x "$APP_DIR/update/check-update.sh" ]; then
  rm -f "$CHECK_REQUESTED"
  CURRENT_VERSION=$("$APP_DIR/bin/pocket-music" --version 2>/dev/null)
  CHECK_OUTPUT=$("$APP_DIR/update/check-update.sh" "$CURRENT_VERSION" "$APP_DIR" "$DATA_DIR" 2>&1)
  CHECK_STATUS=$?
  write_update_result "$CHECK_OUTPUT"
  [ "$CHECK_STATUS" -eq 0 ] || [ "$CHECK_STATUS" -eq 10 ] || true

  sync
  exec "$APP_DIR/launch.sh"
fi

sync
exit "$APP_STATUS"
