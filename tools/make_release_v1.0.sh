#!/usr/bin/env bash
#
# make_release_v1.0.sh
# -----------------------------------------------------------------------------
# Prepare les fichiers a joindre a une release GitHub : renomme le binaire
# compile et genere latest.json avec la bonne somme MD5.
#
# NE PUBLIE RIEN. Les fichiers sont ecrits dans ./dist/, la publication reste
# manuelle (voir docs/OTA.md).
#
# Usage :
#   ./make_release_v1.0.sh <version> <chemin_du_bin> [<user/depot>]
#
# Exemple :
#   ./make_release_v1.0.sh 2.0.1 \
#       firmware/esp32/WordClock_ESP32/build/esp32.esp32.esp32/WordClock_ESP32.ino.bin
# -----------------------------------------------------------------------------

set -euo pipefail

VERSION="${1:-}"
BIN="${2:-}"
REPO="${3:-wizhard2006/WordClock_WiZ}"

[[ -n "$VERSION" && -n "$BIN" ]] || { echo "Usage : $0 <version> <chemin_du_bin> [<user/depot>]" >&2; exit 1; }
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Version attendue au format X.Y.Z" >&2; exit 1; }
[[ -f "$BIN" ]] || { echo "Binaire introuvable : $BIN" >&2; exit 1; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# Coherence entre la version demandee et celle compilee dans le croquis
SKETCH="firmware/esp32/WordClock_ESP32/WordClock_ESP32.ino"
if [[ -f "$SKETCH" ]]; then
  SRCVER="$(grep -m1 '#define WC_FW_VERSION_MAJOR' "$SKETCH" | awk '{print $3}')."
  SRCVER+="$(grep -m1 '#define WC_FW_VERSION_MINOR' "$SKETCH" | awk '{print $3}')."
  SRCVER+="$(grep -m1 '#define WC_FW_VERSION_PATCH' "$SKETCH" | awk '{print $3}')"
  if [[ "$SRCVER" != "$VERSION" ]]; then
    echo "Incoherence : le croquis declare $SRCVER, tu publies $VERSION." >&2
    echo "Corrige WC_FW_VERSION_* dans $SKETCH, recompile, puis relance." >&2
    exit 1
  fi
fi

if command -v md5sum >/dev/null 2>&1; then MD5="$(md5sum "$BIN" | cut -d' ' -f1)"
elif command -v md5 >/dev/null 2>&1;    then MD5="$(md5 -q "$BIN")"
else echo "Ni md5sum ni md5 disponible." >&2; exit 1; fi
SIZE="$(wc -c < "$BIN" | tr -d ' ')"

mkdir -p dist
OUTBIN="dist/WordClock_ESP32_${VERSION}.bin"
OUTJSON="dist/latest.json"
cp "$BIN" "$OUTBIN"
cat > "$OUTJSON" <<EOF
{
  "version": "${VERSION}",
  "bin_url": "https://github.com/${REPO}/releases/latest/download/WordClock_ESP32_${VERSION}.bin",
  "md5": "${MD5}"
}
EOF

echo "Version : ${VERSION}"
echo "Taille  : ${SIZE} octets"
echo "MD5     : ${MD5}"
echo
echo "Fichiers prets a joindre a la release :"
echo "  $OUTBIN"
echo "  $OUTJSON"
echo
echo "Etape suivante : git tag -a esp32-v${VERSION} -m \"WordClock ESP32 ${VERSION}\""
