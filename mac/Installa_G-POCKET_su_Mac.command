#!/bin/bash
# G-POCKET - installazione automatica su Mac
cd "$(dirname "$0")" || exit 1
echo "=== G-POCKET: installazione su Mac ==="
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"
ok=0
for p in *.vst3; do
  [ -e "$p" ] || continue
  rm -rf "$VST3_DIR/$p"; cp -R "$p" "$VST3_DIR/"
  T="$VST3_DIR/$p"; xattr -cr "$T"; chmod -R a+rX "$T"; chmod +x "$T"/Contents/MacOS/* 2>/dev/null
  codesign --force --deep -s - "$T" 2>/dev/null; echo "Installato: $T"; ok=1
done
for p in *.component; do
  [ -e "$p" ] || continue
  rm -rf "$AU_DIR/$p"; cp -R "$p" "$AU_DIR/"
  T="$AU_DIR/$p"; xattr -cr "$T"; chmod -R a+rX "$T"; chmod +x "$T"/Contents/MacOS/* 2>/dev/null
  codesign --force --deep -s - "$T" 2>/dev/null; echo "Installato: $T"; ok=1
done
if [ $ok = 1 ]; then
  echo ""; echo "Fatto! Chiudi e riapri la tua DAW e rifai la scansione dei plugin."
  echo "In Logic Pro: Audio FX > Audio Units > Groovy > G-POCKET."
else
  echo "Non ho trovato i file G-POCKET.vst3 / G-POCKET.component accanto a questo file."
fi
echo ""; read -n 1 -s -r -p "Premi un tasto per chiudere..."
