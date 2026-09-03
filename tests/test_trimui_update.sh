#!/bin/sh
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PREPARER="$ROOT/platform/trimui/update/prepare-update.sh"
APPLIER="$ROOT/platform/trimui/update/apply-update.sh"
LAUNCHER="$ROOT/platform/trimui/launch.sh"
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
if [ -n "\${POCKET_MUSIC_TEST_RUN_LOG:-}" ]; then
  printf '%s\n' '$version' >> "\$POCKET_MUSIC_TEST_RUN_LOG"
fi
exit 0
EOF
  chmod +x "$path"
}

populate_app() {
  app=$1
  version=$2
  mkdir -p "$app/assets" "$app/certs" "$app/update" "$app/data"
  write_fake_binary "$app/bin/pocket-music" "$version"
  printf 'assets-%s\n' "$version" > "$app/assets/version.txt"
  printf 'test-ca-bundle-%s\n' "$version" > "$app/certs/ca-certificates.crt"
  printf '{}\n' > "$app/config.json"
  printf 'icon-%s\n' "$version" > "$app/icon.png"
  cp "$LAUNCHER" "$app/launch.sh"
  cp "$PREPARER" "$APPLIER" "$app/update/"
  chmod +x "$app/launch.sh" "$app/update/prepare-update.sh" "$app/update/apply-update.sh"
}

APP="$TMP/app/PocketMusic"
DATA="$APP/data"
RELEASES="$TMP/releases"
NEW_ROOT="$TMP/new/PocketMusic"
mkdir -p "$RELEASES"
populate_app "$APP" "0.1.0"
printf 'preserve-me\n' > "$DATA/user-state"

REAL_CURL=$(command -v curl)
FAKE_BIN="$TMP/bin"
CURL_ARGS="$TMP/curl-args"
mkdir -p "$FAKE_BIN"
cat > "$FAKE_BIN/curl" <<EOF
#!/bin/sh
printf '%s\n' "\$@" >> "$CURL_ARGS"
exec "$REAL_CURL" "\$@"
EOF
chmod +x "$FAKE_BIN/curl"
export PATH="$FAKE_BIN:$PATH"

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
  "$PREPARER" "0.10.0" "$APP" "$DATA" 2>&1)
UP_TO_DATE_STATUS=$?
set -e
[ "$UP_TO_DATE_STATUS" -eq 0 ] || fail "newer installed version must not downgrade"
printf '%s\n' "$UP_TO_DATE_OUTPUT" | grep 'up to date' >/dev/null || fail "up-to-date result missing"
grep -Fx 'phase=checking' "$DATA/update/check-phase" >/dev/null || \
  fail "preparer did not report checking phase"

set +e
PREPARE_OUTPUT=$(POCKET_MUSIC_UPDATE_BASE_URL="file://$RELEASES" \
  "$PREPARER" "0.1.0" "$APP" "$DATA" 2>&1)
PREPARE_STATUS=$?
set -e
[ "$PREPARE_STATUS" -eq 10 ] || fail "new update must return ready status"
printf '%s\n' "$PREPARE_OUTPUT" | grep 'ready to install' >/dev/null || fail "ready result missing"
grep -Fx 'phase=verifying' "$DATA/update/check-phase" >/dev/null || \
  fail "preparer did not report verification phase"
grep -Fx 'version=0.2.0' "$DATA/update/check-phase" >/dev/null || \
  fail "preparer phase did not report update version"
grep -F -- '--cacert' "$CURL_ARGS" >/dev/null || fail "curl must use the packaged CA bundle"
grep -F -- "$APP/certs/ca-certificates.crt" "$CURL_ARGS" >/dev/null || \
  fail "curl must use the app CA bundle path"
[ -f "$DATA/update/pending-update" ] || fail "pending update metadata missing"
grep -Fx 'version=0.2.0' "$DATA/update/pending-update" >/dev/null || \
  fail "pending manifest version missing"
grep -Fx "asset=$ASSET" "$DATA/update/pending-update" >/dev/null || \
  fail "pending manifest asset missing"
grep -Fx "sha256=$SHA256" "$DATA/update/pending-update" >/dev/null || \
  fail "pending manifest checksum missing"
grep -Fx "size=$SIZE" "$DATA/update/pending-update" >/dev/null || \
  fail "pending manifest size missing"
[ -f "$DATA/update/$ASSET" ] || fail "verified archive missing"
[ ! -f "$DATA/update/$ASSET.download" ] || fail "verified update left temporary archive"
[ ! -f "$DATA/update/release-manifest.tmp" ] || fail "preparer left temporary release manifest"
[ ! -f "$DATA/update/pocket-music-update.txt" ] || fail "legacy durable release manifest remains"
[ "$(cat "$DATA/user-state")" = "preserve-me" ] || fail "prepare modified persistent data"

PENDING_BEFORE=$(cat "$DATA/update/pending-update")
ARCHIVE_BEFORE=$(sha256sum "$DATA/update/$ASSET" | awk '{print $1}')
set +e
FAILED_PREPARE_OUTPUT=$(POCKET_MUSIC_UPDATE_BASE_URL="file://$TMP/missing-release" \
  "$PREPARER" "0.1.0" "$APP" "$DATA" 2>&1)
FAILED_PREPARE_STATUS=$?
set -e
[ "$FAILED_PREPARE_STATUS" -ne 0 ] || fail "missing release must fail update preparation"
grep -Fx 'phase=checking' "$DATA/update/check-phase" >/dev/null || \
  fail "failed preparation did not reset phase to checking"
[ "$(cat "$DATA/update/pending-update")" = "$PENDING_BEFORE" ] || \
  fail "failed preparation changed verified pending metadata"
[ "$(sha256sum "$DATA/update/$ASSET" | awk '{print $1}')" = "$ARCHIVE_BEFORE" ] || \
  fail "failed preparation changed verified archive"

BAD_RELEASES="$TMP/bad-releases"
mkdir -p "$BAD_RELEASES"
printf 'not-the-verified-archive\n' > "$BAD_RELEASES/$ASSET"
BAD_SIZE=$(wc -c < "$BAD_RELEASES/$ASSET" | tr -d ' ')
cat > "$BAD_RELEASES/pocket-music-update.txt" <<EOF
version=0.2.0
asset=$ASSET
sha256=0000000000000000000000000000000000000000000000000000000000000000
size=$BAD_SIZE
EOF
set +e
POCKET_MUSIC_UPDATE_BASE_URL="file://$BAD_RELEASES" \
  "$PREPARER" "0.1.0" "$APP" "$DATA" >/dev/null 2>&1
BAD_PREPARE_STATUS=$?
set -e
[ "$BAD_PREPARE_STATUS" -ne 0 ] || fail "checksum mismatch must fail update preparation"
[ "$(cat "$DATA/update/pending-update")" = "$PENDING_BEFORE" ] || \
  fail "failed replacement changed verified pending metadata"
[ "$(sha256sum "$DATA/update/$ASSET" | awk '{print $1}')" = "$ARCHIVE_BEFORE" ] || \
  fail "failed replacement changed verified archive"
[ ! -f "$DATA/update/$ASSET.download" ] || fail "failed replacement left temporary archive"

APPLY_OUTPUT=$("$APPLIER" "$APP" "$DATA" 2>&1) || {
  printf '%s\n' "$APPLY_OUTPUT" >&2
  fail "verified update did not install"
}
[ "$("$APP/bin/pocket-music" --version)" = "0.2.0" ] || fail "installed version mismatch"
[ "$(cat "$APP/certs/ca-certificates.crt")" = "test-ca-bundle-0.2.0" ] || \
  fail "install did not replace CA bundle"
[ -x "$APP/update/prepare-update.sh" ] || fail "install did not include executable update preparer"
[ ! -e "$APP/update/check-update.sh" ] || fail "install retained legacy checker script"
[ "$(cat "$DATA/user-state")" = "preserve-me" ] || fail "install modified persistent data"
[ ! -f "$DATA/update/update-in-progress" ] || fail "success left recovery marker"
[ ! -f "$DATA/update/pending-update" ] || fail "success left pending update"

RECOVERY_APP="$TMP/recovery/PocketMusic"
RECOVERY_DATA="$RECOVERY_APP/data"
populate_app "$RECOVERY_APP" "0.1.0"
printf 'recover-me\n' > "$RECOVERY_DATA/user-state"
BACKUP="$RECOVERY_DATA/update/backup"
mkdir -p "$BACKUP"
for path in bin assets certs update icon.png config.json launch.sh; do
  if [ -e "$RECOVERY_APP/$path" ]; then
    cp -Rp "$RECOVERY_APP/$path" "$BACKUP/$path"
  fi
done
printf 'version=0.2.0\n' > "$RECOVERY_DATA/update/update-in-progress"
rm -rf "$RECOVERY_APP/bin" "$RECOVERY_APP/assets" "$RECOVERY_APP/certs" "$RECOVERY_APP/update"
mkdir -p "$RECOVERY_APP/bin" "$RECOVERY_APP/assets" "$RECOVERY_APP/certs" "$RECOVERY_APP/update"
printf 'partial\n' > "$RECOVERY_APP/bin/partial-file"
printf 'partial\n' > "$RECOVERY_APP/config.json"
printf '#!/bin/sh\nexit 99\n' > "$RECOVERY_APP/launch.sh"
chmod +x "$RECOVERY_APP/launch.sh"

RECOVERY_OUTPUT=$("$APPLIER" --recover "$RECOVERY_APP" "$RECOVERY_DATA" 2>&1) || {
  printf '%s\n' "$RECOVERY_OUTPUT" >&2
  fail "interrupted update did not recover"
}
[ "$("$RECOVERY_APP/bin/pocket-music" --version)" = "0.1.0" ] || fail "recovery version mismatch"
[ "$(cat "$RECOVERY_APP/certs/ca-certificates.crt")" = "test-ca-bundle-0.1.0" ] || \
  fail "recovery did not restore CA bundle"
[ "$(cat "$RECOVERY_DATA/user-state")" = "recover-me" ] || fail "recovery modified persistent data"
[ ! -f "$RECOVERY_DATA/update/update-in-progress" ] || fail "recovery marker was not cleared"

LAUNCH_APP="$TMP/launcher/PocketMusic"
LAUNCH_DATA="$LAUNCH_APP/data"
LAUNCH_SD="$TMP/sdcard"
LAUNCH_RUN_LOG="$TMP/launcher-runs"
populate_app "$LAUNCH_APP" "0.1.0"
printf 'launcher-preserve-me\n' > "$LAUNCH_DATA/user-state"
cp "$RELEASES/$ASSET" "$LAUNCH_DATA/update/$ASSET"
cat > "$LAUNCH_DATA/update/pending-update" <<EOF
version=0.2.0
asset=$ASSET
sha256=$SHA256
size=$SIZE
EOF
printf 'requested\n' > "$LAUNCH_DATA/update/install-requested"

POCKET_MUSIC_SD_ROOT="$LAUNCH_SD" POCKET_MUSIC_TEST_RUN_LOG="$LAUNCH_RUN_LOG" \
  "$LAUNCH_APP/launch.sh" || fail "launcher install handoff failed"
[ "$("$LAUNCH_APP/bin/pocket-music" --version)" = "0.2.0" ] || \
  fail "launcher did not install pending version"
[ "$(cat "$LAUNCH_DATA/user-state")" = "launcher-preserve-me" ] || \
  fail "launcher handoff modified persistent data"
[ ! -f "$LAUNCH_DATA/update/install-requested" ] || fail "launcher did not consume install marker"
[ ! -f "$LAUNCH_DATA/update/pending-update" ] || fail "launcher install left pending manifest"
grep -Fx 'Pocket Music updated to 0.2.0' "$LAUNCH_DATA/update/last-status" >/dev/null || \
  fail "launcher did not persist install result"
[ "$(sed -n '1p' "$LAUNCH_RUN_LOG")" = "0.1.0" ] || \
  fail "launcher did not run the original app before handoff"
[ "$(sed -n '2p' "$LAUNCH_RUN_LOG")" = "0.2.0" ] || \
  fail "launcher did not restart the updated app"
[ "$(wc -l < "$LAUNCH_RUN_LOG" | tr -d ' ')" -eq 2 ] || \
  fail "launcher restart count was unexpected"
[ -x "$LAUNCH_APP/update/prepare-update.sh" ] || \
  fail "launcher-installed app is missing update preparer"
[ ! -e "$LAUNCH_APP/update/check-update.sh" ] || \
  fail "launcher-installed app retained legacy checker script"

printf 'TrimUI updater integration test passed\n'
