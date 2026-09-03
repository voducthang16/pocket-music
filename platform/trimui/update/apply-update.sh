#!/bin/sh
set -eu

APP_DIR=${1:?app dir required}
DATA_DIR=${2:?data dir required}
UPDATE_DIR="$DATA_DIR/update"
PENDING="$UPDATE_DIR/pending-update"
STAGE="$UPDATE_DIR/stage"
BACKUP="$UPDATE_DIR/backup"
SUCCESS=0

field() {
  sed -n "s/^$1=//p" "$PENDING" | head -n 1
}

rollback() {
  [ "$SUCCESS" -eq 1 ] && return 0
  [ -d "$BACKUP" ] || return 0

  for path in bin assets icon.png config.json launch.sh update; do
    rm -rf "$APP_DIR/$path"
    if [ -e "$BACKUP/$path" ]; then
      cp -Rp "$BACKUP/$path" "$APP_DIR/$path"
    fi
  done
  sync
}
trap rollback EXIT HUP INT TERM

[ -f "$PENDING" ] || {
  echo "No pending Pocket Music update" >&2
  exit 2
}

VERSION=$(field version)
ASSET=$(field asset)
SHA256=$(field sha256)
ARCHIVE="$UPDATE_DIR/$ASSET"

[ -f "$ARCHIVE" ] || {
  echo "Pending update archive is missing" >&2
  exit 2
}

ACTUAL_SHA=$(sha256sum "$ARCHIVE" | awk '{print $1}')
[ "$ACTUAL_SHA" = "$SHA256" ] || {
  echo "Pending update checksum verification failed" >&2
  exit 2
}

rm -rf "$STAGE" "$BACKUP"
mkdir -p "$STAGE" "$BACKUP"

tar -tzf "$ARCHIVE" | grep '^PocketMusic/data\(/\|$\)' >/dev/null 2>&1 && {
  echo "Update archive unexpectedly contains persistent data" >&2
  exit 2
}

tar -xzf "$ARCHIVE" -C "$STAGE"
NEW_ROOT="$STAGE/PocketMusic"
[ -x "$NEW_ROOT/bin/pocket-music" ] || {
  echo "Update does not contain Pocket Music binary" >&2
  exit 2
}
[ -f "$NEW_ROOT/launch.sh" ] || {
  echo "Update does not contain launch.sh" >&2
  exit 2
}

for path in bin assets icon.png config.json launch.sh update; do
  if [ -e "$APP_DIR/$path" ]; then
    cp -Rp "$APP_DIR/$path" "$BACKUP/$path"
  fi
done

for path in bin assets icon.png config.json launch.sh update; do
  rm -rf "$APP_DIR/$path"
  if [ -e "$NEW_ROOT/$path" ]; then
    cp -Rp "$NEW_ROOT/$path" "$APP_DIR/$path"
  fi
done

chmod +x "$APP_DIR/bin/pocket-music" "$APP_DIR/launch.sh"
[ ! -d "$APP_DIR/update" ] || chmod +x "$APP_DIR"/update/*.sh 2>/dev/null || true

INSTALLED_VERSION=$("$APP_DIR/bin/pocket-music" --version)
[ "$INSTALLED_VERSION" = "$VERSION" ] || {
  echo "Installed version verification failed: expected $VERSION, got $INSTALLED_VERSION" >&2
  exit 2
}

SUCCESS=1
trap - EXIT HUP INT TERM
rm -f "$PENDING" "$ARCHIVE" "$UPDATE_DIR/pocket-music-update.txt"
rm -rf "$STAGE" "$BACKUP"
sync

echo "Pocket Music updated to $VERSION"
