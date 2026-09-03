#!/bin/sh
set -eu

CURRENT_VERSION=${1:?current version required}
APP_DIR=${2:?app dir required}
DATA_DIR=${3:?data dir required}
UPDATE_DIR="$DATA_DIR/update"
BASE_URL=${POCKET_MUSIC_UPDATE_BASE_URL:-https://github.com/voducthang16/pocket-music/releases/latest/download}
RELEASE_MANIFEST="$UPDATE_DIR/release-manifest.tmp"
PENDING="$UPDATE_DIR/pending-update"
PHASE_FILE="$UPDATE_DIR/check-phase"
CA_BUNDLE="$APP_DIR/certs/ca-certificates.crt"

[ -d "$APP_DIR" ] || {
  echo "Pocket Music app directory is missing" >&2
  exit 2
}

mkdir -p "$UPDATE_DIR"
rm -f "$RELEASE_MANIFEST" "$UPDATE_DIR/pocket-music-update.txt"
trap 'rm -f "$RELEASE_MANIFEST"' EXIT HUP INT TERM

write_phase() {
  phase=$1
  version=${2:-}
  {
    printf 'phase=%s\n' "$phase"
    if [ -n "$version" ]; then
      printf 'version=%s\n' "$version"
    fi
  } > "$PHASE_FILE"
}

write_phase checking

[ -f "$CA_BUNDLE" ] || {
  echo "Pocket Music CA bundle is missing" >&2
  exit 2
}

need_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required command: $1" >&2
    exit 2
  }
}

if command -v curl >/dev/null 2>&1; then
  download() { curl --cacert "$CA_BUNDLE" -fsSL --connect-timeout 10 --max-time 180 -o "$2" "$1"; }
elif command -v wget >/dev/null 2>&1; then
  download() { SSL_CERT_FILE="$CA_BUNDLE" wget -q -O "$2" "$1"; }
else
  echo "Neither curl nor wget is available" >&2
  exit 2
fi

need_command sha256sum

field() {
  sed -n "s/^$1=//p" "$RELEASE_MANIFEST" | head -n 1
}

pending_field() {
  key=$1
  [ -f "$PENDING" ] || return 0
  sed -n "s/^$key=//p" "$PENDING" | head -n 1
}

valid_asset() {
  case "$1" in
    ''|*/*|*\\*) return 1 ;;
  esac
  return 0
}

valid_version() {
  awk -v version="$1" 'BEGIN {
    if (version !~ /^[0-9]+(\.[0-9]+)*$/) exit 1;
    exit 0;
  }'
}

is_newer_version() {
  awk -v current="$1" -v latest="$2" 'BEGIN {
    nc = split(current, c, ".");
    nl = split(latest, l, ".");
    n = (nc > nl ? nc : nl);
    for (i = 1; i <= n; ++i) {
      cv = (i <= nc ? c[i] + 0 : 0);
      lv = (i <= nl ? l[i] + 0 : 0);
      if (lv > cv) exit 0;
      if (lv < cv) exit 1;
    }
    exit 1;
  }'
}

OLD_ASSET=$(pending_field asset)
download "$BASE_URL/pocket-music-update.txt" "$RELEASE_MANIFEST"

VERSION=$(field version)
ASSET=$(field asset)
SHA256=$(field sha256)
SIZE=$(field size)

valid_version "$CURRENT_VERSION" || {
  echo "Invalid installed version" >&2
  exit 2
}
valid_version "$VERSION" || {
  echo "Invalid update version" >&2
  exit 2
}
valid_asset "$ASSET" || {
  echo "Invalid update asset" >&2
  exit 2
}
case "$SHA256" in
  ''|*[!0-9a-fA-F]*) echo "Invalid update checksum" >&2; exit 2 ;;
esac
[ "${#SHA256}" -eq 64 ] || {
  echo "Invalid update checksum" >&2
  exit 2
}
case "$SIZE" in
  ''|*[!0-9]*) echo "Invalid update size" >&2; exit 2 ;;
esac

if ! is_newer_version "$CURRENT_VERSION" "$VERSION"; then
  if valid_asset "$OLD_ASSET"; then
    rm -f "$UPDATE_DIR/$OLD_ASSET"
  fi
  rm -f "$PENDING"
  echo "Pocket Music $CURRENT_VERSION is up to date"
  exit 0
fi

ARCHIVE="$UPDATE_DIR/$ASSET"
ARCHIVE_TMP="$ARCHIVE.download"
rm -f "$ARCHIVE_TMP"
write_phase downloading "$VERSION"
download "$BASE_URL/$ASSET" "$ARCHIVE_TMP"

write_phase verifying "$VERSION"
ACTUAL_SIZE=$(wc -c < "$ARCHIVE_TMP" | tr -d ' ')
[ "$ACTUAL_SIZE" = "$SIZE" ] || {
  echo "Update size verification failed" >&2
  rm -f "$ARCHIVE_TMP"
  exit 2
}

ACTUAL_SHA=$(sha256sum "$ARCHIVE_TMP" | awk '{print $1}')
[ "$ACTUAL_SHA" = "$SHA256" ] || {
  echo "Update checksum verification failed" >&2
  rm -f "$ARCHIVE_TMP"
  exit 2
}

mv -f "$ARCHIVE_TMP" "$ARCHIVE"
cat > "$PENDING.tmp" <<EOF
version=$VERSION
asset=$ASSET
sha256=$SHA256
size=$SIZE
EOF
mv -f "$PENDING.tmp" "$PENDING"
if [ -n "$OLD_ASSET" ] && [ "$OLD_ASSET" != "$ASSET" ] && valid_asset "$OLD_ASSET"; then
  rm -f "$UPDATE_DIR/$OLD_ASSET"
fi
sync

echo "Pocket Music $VERSION is ready to install"
exit 10
