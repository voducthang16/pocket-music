#!/bin/sh
set -eu

MODE=apply
if [ "${1:-}" = "--recover" ]; then
  MODE=recover
  shift
fi

APP_DIR=${1:?app dir required}
DATA_DIR=${2:?data dir required}
UPDATE_DIR="$DATA_DIR/update"
PENDING="$UPDATE_DIR/pending-update"
STAGE="$UPDATE_DIR/stage"
PREPARED="$UPDATE_DIR/prepared"
BACKUP="$UPDATE_DIR/backup"
MARKER="$UPDATE_DIR/update-in-progress"
SUCCESS=0

field() {
  sed -n "s/^$1=//p" "$PENDING" | head -n 1
}

restore_backup() {
  [ -f "$MARKER" ] || return 0
  [ -d "$BACKUP" ] || {
    echo "Update recovery backup is missing" >&2
    return 2
  }
  [ -x "$BACKUP/bin/pocket-music" ] || {
    echo "Update recovery binary backup is missing" >&2
    return 2
  }
  [ -f "$BACKUP/launch.sh" ] || {
    echo "Update recovery launcher backup is missing" >&2
    return 2
  }
  [ -f "$BACKUP/config.json" ] || {
    echo "Update recovery config backup is missing" >&2
    return 2
  }

  for path in bin assets certs update; do
    rm -rf "$APP_DIR/$path"
    if [ -e "$BACKUP/$path" ]; then
      cp -Rp "$BACKUP/$path" "$APP_DIR/$path"
    fi
  done

  for path in icon.png config.json launch.sh; do
    if [ -e "$BACKUP/$path" ]; then
      temp="$APP_DIR/.pocket-music-rollback-$path"
      rm -rf "$temp"
      cp -Rp "$BACKUP/$path" "$temp"
      mv -f "$temp" "$APP_DIR/$path"
    else
      rm -f "$APP_DIR/$path"
    fi
  done

  chmod +x "$APP_DIR/bin/pocket-music" "$APP_DIR/launch.sh"
  [ ! -d "$APP_DIR/update" ] || chmod +x "$APP_DIR"/update/*.sh 2>/dev/null || true
  sync
  rm -f "$MARKER"
  sync
  rm -rf "$STAGE" "$PREPARED" "$BACKUP"
  echo "Pocket Music update rolled back"
}

rollback_on_exit() {
  [ "$SUCCESS" -eq 1 ] && return 0
  trap - EXIT HUP INT TERM
  if [ -f "$MARKER" ]; then
    restore_backup || true
  fi
}

if [ "$MODE" = "recover" ]; then
  if [ ! -f "$MARKER" ]; then
    echo "No interrupted Pocket Music update"
    exit 0
  fi
  restore_backup
  exit 0
fi

[ ! -f "$MARKER" ] || {
  echo "Interrupted update must be recovered before installing" >&2
  exit 2
}
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
[ "${#SHA256}" -eq 64 ] || {
  echo "Pending update checksum is invalid" >&2
  exit 2
}
ACTUAL_SHA=$(sha256sum "$ARCHIVE" | awk '{print $1}')
[ "$ACTUAL_SHA" = "$SHA256" ] || {
  echo "Pending update checksum verification failed" >&2
  exit 2
}

rm -rf "$STAGE" "$PREPARED" "$BACKUP"
mkdir -p "$STAGE" "$PREPARED" "$BACKUP"

ARCHIVE_LIST=$(tar -tzf "$ARCHIVE") || {
  echo "Could not inspect update archive" >&2
  exit 2
}
printf '%s\n' "$ARCHIVE_LIST" | grep -E '(^|/)\.\.(/|$)' >/dev/null 2>&1 && {
  echo "Update archive contains unsafe paths" >&2
  exit 2
}
printf '%s\n' "$ARCHIVE_LIST" | grep -v -E '^PocketMusic(/|$)' | grep . >/dev/null 2>&1 && {
  echo "Update archive contains files outside PocketMusic" >&2
  exit 2
}
printf '%s\n' "$ARCHIVE_LIST" | grep -E '^PocketMusic/data(/|$)' >/dev/null 2>&1 && {
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
[ -f "$NEW_ROOT/config.json" ] || {
  echo "Update does not contain config.json" >&2
  exit 2
}
[ -f "$NEW_ROOT/certs/ca-certificates.crt" ] || {
  echo "Update does not contain CA certificates" >&2
  exit 2
}
[ -f "$NEW_ROOT/update/apply-update.sh" ] || {
  echo "Update does not contain updater scripts" >&2
  exit 2
}

# Prepare a complete replacement tree while the live app is still untouched.
for path in bin assets certs icon.png config.json launch.sh update; do
  if [ -e "$NEW_ROOT/$path" ]; then
    cp -Rp "$NEW_ROOT/$path" "$PREPARED/$path"
  fi
done

# Build a complete recovery backup before marking the transaction in progress.
for path in bin assets certs icon.png config.json launch.sh update; do
  if [ -e "$APP_DIR/$path" ]; then
    cp -Rp "$APP_DIR/$path" "$BACKUP/$path"
  fi
done
[ -x "$BACKUP/bin/pocket-music" ] || {
  echo "Could not create binary recovery backup" >&2
  exit 2
}
[ -f "$BACKUP/launch.sh" ] || {
  echo "Could not create launcher recovery backup" >&2
  exit 2
}
[ -f "$BACKUP/config.json" ] || {
  echo "Could not create config recovery backup" >&2
  exit 2
}
sync

cat > "$MARKER.tmp" <<EOF
version=$VERSION
EOF
mv "$MARKER.tmp" "$MARKER"
sync
trap rollback_on_exit EXIT
trap 'exit 2' HUP INT TERM

# Directories may briefly be absent, but launch.sh remains present and the recovery marker
# lets the launcher restore the backup after an unexpected reboot.
for path in bin assets certs update; do
  rm -rf "$APP_DIR/$path"
  if [ -e "$PREPARED/$path" ]; then
    mv "$PREPARED/$path" "$APP_DIR/$path"
  fi
done

# Root files are replaced atomically. launch.sh is deliberately replaced last.
for path in icon.png config.json launch.sh; do
  if [ -e "$PREPARED/$path" ]; then
    mv -f "$PREPARED/$path" "$APP_DIR/$path"
  else
    rm -f "$APP_DIR/$path"
  fi
done

chmod +x "$APP_DIR/bin/pocket-music" "$APP_DIR/launch.sh"
[ ! -d "$APP_DIR/update" ] || chmod +x "$APP_DIR"/update/*.sh 2>/dev/null || true

INSTALLED_VERSION=$("$APP_DIR/bin/pocket-music" --version)
[ "$INSTALLED_VERSION" = "$VERSION" ] || {
  echo "Installed version verification failed: expected $VERSION, got $INSTALLED_VERSION" >&2
  exit 2
}

# The new installation is verified before the recovery marker is cleared.
rm -f "$MARKER"
sync
SUCCESS=1
trap - EXIT HUP INT TERM

rm -f "$PENDING" "$ARCHIVE" "$UPDATE_DIR/pocket-music-update.txt"
rm -rf "$STAGE" "$PREPARED" "$BACKUP"
sync

echo "Pocket Music updated to $VERSION"
