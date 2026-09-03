#!/bin/sh
set -eu

CURRENT_VERSION=${1:?current version required}
APP_DIR=${2:?app dir required}
DATA_DIR=${3:?data dir required}
UPDATE_DIR="$DATA_DIR/update"
BASE_URL="https://github.com/voducthang16/pocket-music/releases/latest/download"
MANIFEST="$UPDATE_DIR/pocket-music-update.txt"
PENDING="$UPDATE_DIR/pending-update"

mkdir -p "$UPDATE_DIR"
rm -f "$MANIFEST" "$PENDING"

need_command() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required command: $1" >&2
    exit 2
  }
}

if command -v curl >/dev/null 2>&1; then
  download() { curl -fL --connect-timeout 10 --max-time 180 -o "$2" "$1"; }
elif command -v wget >/dev/null 2>&1; then
  download() { wget -O "$2" "$1"; }
else
  echo "Neither curl nor wget is available" >&2
  exit 2
fi

need_command sha256sum
need_command tar

field() {
  sed -n "s/^$1=//p" "$MANIFEST" | head -n 1
}

valid_version() {
  awk -v version="$1" 'BEGIN {
    if (version !~ /^[0-9]+(\.[0-9]+)*$/) exit 1;
    exit 0;
  }'
}

is_newer_version() {
  awk -v current="$1" -v latest="$2" 'BEGIN {
    split(current, c, "."); split(latest, l, ".");
    n = (length(c) > length(l) ? length(c) : length(l));
    for (i = 1; i <= n; ++i) {
      cv = (c[i] == "" ? 0 : c[i] + 0);
      lv = (l[i] == "" ? 0 : l[i] + 0);
      if (lv > cv) exit 0;
      if (lv < cv) exit 1;
    }
    exit 1;
  }'
}

download "$BASE_URL/pocket-music-update.txt" "$MANIFEST"

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
case "$ASSET" in
  ''|*/*|*\\*) echo "Invalid update asset" >&2; exit 2 ;;
esac
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
  echo "Pocket Music $CURRENT_VERSION is up to date"
  exit 0
fi

ARCHIVE="$UPDATE_DIR/$ASSET"
rm -f "$ARCHIVE"
download "$BASE_URL/$ASSET" "$ARCHIVE"

ACTUAL_SIZE=$(wc -c < "$ARCHIVE" | tr -d ' ')
[ "$ACTUAL_SIZE" = "$SIZE" ] || {
  echo "Update size verification failed" >&2
  rm -f "$ARCHIVE"
  exit 2
}

ACTUAL_SHA=$(sha256sum "$ARCHIVE" | awk '{print $1}')
[ "$ACTUAL_SHA" = "$SHA256" ] || {
  echo "Update checksum verification failed" >&2
  rm -f "$ARCHIVE"
  exit 2
}

cat > "$PENDING.tmp" <<EOF
version=$VERSION
asset=$ASSET
sha256=$SHA256
EOF
mv "$PENDING.tmp" "$PENDING"
sync

echo "Pocket Music $VERSION is ready to install"
exit 10
