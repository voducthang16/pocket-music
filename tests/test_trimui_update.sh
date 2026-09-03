#!/bin/sh
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CHECKER="$ROOT/platform/trimui/update/check-update.sh"
APPLIER="$ROOT/platform/trimui/update/apply-update.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

fail() {
  echo "TrimUI updater test failed: $1" >&2
  exit 1
}

write_fake_binary() {
  path=$1
  version=$2
  mkdir -p "$(dirname "$path")"
  cat > "$path" <<EOF
#!/bin/sh
if [ "\${1:-}" = "--version" ]; then
  echo "$version"
  exit 0
fi
exit 0
EOF
  chmod +x "$path"
}

populate_app() {
  app=$1
  version=$2
  mkdir -p "$app/assets" "$app/update" "$app/data"
  write_fake_binary "$app/bin/pocket-music" "$version"
  printf 'assets-%s\n' "$version" > "$app/assets/version.txt"
  printf '{}\n' > "$app/config.json"
  printf 'icon-%s\n' "$version" > "$app/icon.png"
  cp "$ROOT/platform/trimui/launch.sh" "$app/launch.sh"
  cp "$CHECKER" "$APPLIER" "$app/update/"
  chmod +x "$app/launch.sh" "$app/update/check-update.sh" "$app/update/apply-update.sh"
}

APP="$TMP/app/PocketMusic"
DATA="$APP/data"
RELEASES="$TMP/releases"
NEW_ROOT="$TMP/new/PocketMusic"
mkdir -p "$RELEASES"
populate_app "$APP" "0.1.0"
printf 'preserve-me\n' > "$DATA/user-state"

populate_app "$NEW_ROOT" "0.2.0"
rm -rf "$NEW_ROOT/data"
ASSET="PocketMusic-v0.2.0-update.tar.gz"
tar -C "$TMP/new" -czf "$RELEASES/$ASSET" PocketMusic
SHA256=$(sha256sum "$RELEASES/$ASSET" | awk '{print $1}')
SIZE=$(wc -c < "$RELEASES/$ASSET" | tr -d ' ')
cat > "$RELEASES/pocket-music-update.txt" <<EOF
version=0.2.0
asset=$ASSET
sha256=$SHA256
size=$SIZE
EOF

set +e
UP_TO_DATE_OUTPUT=$(POCKET_MUSIC_UPDATE_BASE_URL="file://$RELEASES" \
  "$CHECKER" "0.10.0" "$APP" "$DATA" 2>&1)
UP_TO_DATE_STATUS=$?
set -e
[ "$UP_TO_DATE_STATUS" -eq 0 ] || fail "newer installed version must not downgrade"
printf '%s\n' "$UP_TO_DATE_OUTPUT" | grep 'up to date' >/dev/null || fail "up-to-date result missing"

set +e
CHECK_OUTPUT=$(POCKET_MUSIC_UPDATE_BASE_URL="file://$RELEASES" \
  "$CHECKER" "0.1.0" "$APP" "$DATA" 2>&1)
CHECK_STATUS=$?
set -e
[ "$CHECK_STATUS" -eq 10 ] || fail "new update must return ready status"
printf '%s\n' "$CHECK_OUTPUT" | grep 'ready to install' >/dev/null || fail "ready result missing"
[ -f "$DATA/update/pending-update" ] || fail "pending update metadata missing"
[ "$(cat "$DATA/user-state")" = "preserve-me" ] || fail "check modified persistent data"

PENDING_BEFORE=$(cat "$DATA/update/pending-update")
set +e
FAILED_CHECK_OUTPUT=$(POCKET_MUSIC_UPDATE_BASE_URL="file://$TMP/missing-release" \
  "$CHECKER" "0.1.0" "$APP" "$DATA" 2>&1)
FAILED_CHECK_STATUS=$?
set -e
[ "$FAILED_CHECK_STATUS" -ne 0 ] || fail "missing release must fail the update check"
[ -f "$DATA/update/pending-update" ] || fail "failed check discarded verified pending update"
[ "$(cat "$DATA/update/pending-update")" = "$PENDING_BEFORE" ] || \
  fail "failed check changed verified pending metadata"

APPLY_OUTPUT=$("$APPLIER" "$APP" "$DATA" 2>&1) || {
  printf '%s\n' "$APPLY_OUTPUT" >&2
  fail "verified update did not install"
}
[ "$("$APP/bin/pocket-music" --version)" = "0.2.0" ] || fail "installed version mismatch"
[ "$(cat "$DATA/user-state")" = "preserve-me" ] || fail "install modified persistent data"
[ ! -f "$DATA/update/update-in-progress" ] || fail "success left recovery marker"
[ ! -f "$DATA/update/pending-update" ] || fail "success left pending update"

RECOVERY_APP="$TMP/recovery/PocketMusic"
RECOVERY_DATA="$RECOVERY_APP/data"
populate_app "$RECOVERY_APP" "0.1.0"
printf 'recover-me\n' > "$RECOVERY_DATA/user-state"
BACKUP="$RECOVERY_DATA/update/backup"
mkdir -p "$BACKUP"
for path in bin assets icon.png config.json launch.sh update; do
  if [ -e "$RECOVERY_APP/$path" ]; then
    cp -Rp "$RECOVERY_APP/$path" "$BACKUP/$path"
  fi
done
printf 'version=0.2.0\n' > "$RECOVERY_DATA/update/update-in-progress"
rm -rf "$RECOVERY_APP/bin" "$RECOVERY_APP/assets" "$RECOVERY_APP/update"
mkdir -p "$RECOVERY_APP/bin" "$RECOVERY_APP/assets" "$RECOVERY_APP/update"
printf 'partial\n' > "$RECOVERY_APP/bin/partial-file"
printf 'partial\n' > "$RECOVERY_APP/config.json"
printf '#!/bin/sh\nexit 99\n' > "$RECOVERY_APP/launch.sh"
chmod +x "$RECOVERY_APP/launch.sh"

RECOVERY_OUTPUT=$("$APPLIER" --recover "$RECOVERY_APP" "$RECOVERY_DATA" 2>&1) || {
  printf '%s\n' "$RECOVERY_OUTPUT" >&2
  fail "interrupted update did not recover"
}
[ "$("$RECOVERY_APP/bin/pocket-music" --version)" = "0.1.0" ] || fail "recovery version mismatch"
[ "$(cat "$RECOVERY_DATA/user-state")" = "recover-me" ] || fail "recovery modified persistent data"
[ ! -f "$RECOVERY_DATA/update/update-in-progress" ] || fail "recovery marker was not cleared"

printf 'TrimUI updater integration test passed\n'
