#!/bin/bash
# Crea l'installer macOS (.pkg) di G-POCKET a partire dai plugin compilati.
# Uso:  mac/pkg/make_pkg.sh <cartella_build> <file_pkg_output> [versione]
# Esempio (GitHub Actions):  mac/pkg/make_pkg.sh build G-POCKET_Installer_Mac.pkg 1.0
set -eu

BUILD_DIR="${1:?Manca la cartella di build (es. build)}"
OUT="${2:?Manca il nome del file .pkg di output}"
VER="${3:-1.0}"
HERE="$(cd "$(dirname "$0")" && pwd)"

# trova i bundle compilati (solo dentro *_artefacts, ignora le copie in ~/Library)
find_bundle() {
  find "$BUILD_DIR" -maxdepth 8 -path '*_artefacts*' -name "$1" 2>/dev/null | head -n 1 || true
}
VST3="$(find_bundle 'G-POCKET.vst3')"
AU="$(find_bundle 'G-POCKET.component')"
APP="$(find_bundle 'G-POCKET.app')"

if [ -z "$VST3" ]; then
  echo "ERRORE: G-POCKET.vst3 non trovato in '$BUILD_DIR'" >&2
  exit 1
fi
echo "VST3: $VST3"
echo "AU:   ${AU:-non trovato (salto)}"
echo "APP:  ${APP:-non trovata (salto)}"

STAGE="$(mktemp -d)"
ROOT="$STAGE/root"
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Library/Audio/Plug-Ins/Components" "$ROOT/Applications"

ditto "$VST3" "$ROOT/Library/Audio/Plug-Ins/VST3/G-POCKET.vst3"
[ -n "$AU" ]  && ditto "$AU"  "$ROOT/Library/Audio/Plug-Ins/Components/G-POCKET.component"
[ -n "$APP" ] && ditto "$APP" "$ROOT/Applications/G-POCKET.app"
rmdir "$ROOT/Library/Audio/Plug-Ins/Components" 2>/dev/null || true
rmdir "$ROOT/Applications" 2>/dev/null || true

# pulizia attributi e permessi dentro il pacchetto
xattr -cr "$ROOT" || true
chmod -R a+rX "$ROOT"
find "$ROOT" -path '*/Contents/MacOS/*' -type f -exec chmod +x {} \; || true

# i bundle NON devono essere "relocatable", altrimenti l'installer puo' scrivere altrove
pkgbuild --analyze --root "$ROOT" "$STAGE/components.plist"
CPLIST=""
if python3 - "$STAGE/components.plist" <<'PY'
import plistlib, sys
p = sys.argv[1]
with open(p, "rb") as f:
    d = plistlib.load(f)
items = d if isinstance(d, list) else [d]
for it in items:
    it["BundleIsRelocatable"] = False
    it["BundleIsVersionChecked"] = False
    it["BundleOverwriteAction"] = "overwrite"
with open(p, "wb") as f:
    plistlib.dump(d, f)
sys.exit(0 if items else 1)
PY
then
  CPLIST="--component-plist $STAGE/components.plist"
fi

pkgbuild --root "$ROOT" \
         $CPLIST \
         --scripts "$HERE/scripts" \
         --identifier com.groovy.gpocket \
         --version "$VER" \
         --install-location / \
         "$STAGE/G-POCKET-component.pkg"

sed "s/@VERSION@/$VER/g" "$HERE/distribution.xml" > "$STAGE/distribution.xml"
productbuild --distribution "$STAGE/distribution.xml" \
             --resources "$HERE/resources" \
             --package-path "$STAGE" \
             "$OUT"

echo "Creato: $OUT"
rm -rf "$STAGE"
