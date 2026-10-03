# G-POCKET

Rack di 4 pedali (Distorsione, Saturazione, Bitcrush, Reverb) con Envelope disegnabile
(MIDI / Follower / Sync). JUCE 8 + CMake. Formati: VST3 (Windows/Mac), AU (Mac), Standalone.
Interfaccia a due schermi in pixel-art 8-bit (224x168 virtuali per schermo, ingranditi senza sfumature).

## Comandi
- **Schermo alto** (Envelope): schede MIDI / FOLLOW / SYNC, interruttore ENV, curva disegnabile,
  oscilloscopio dell'uscita, parametri del modo, indicatori VAL/POS.
  - Doppio click nel vuoto = aggiungi punto · trascina = sposta (Shift = griglia) ·
    trascina il quadratino tra due punti = curvatura (doppio click = dritta) · click destro su un punto = elimina.
- **Schermo alto** (Preset): 30 preset di fabbrica (BASIC / LIGHT / DESTROY) + i tuoi preset (USER). Click su una riga = carica.
- **Due frecce verticali** (a sinistra): su = preset precedente, giù = successivo (tenendo premuto ripete).
- **SELECT**: alterna schermo alto tra Envelope e Preset. **START** (o il tasto SAVE sullo schermo): apre la tastiera a pixel per dare un nome e salvare il preset. **DEL** elimina il preset utente selezionato (secondo click per confermare).
- **4 tasti a destra, in colonna**: D (rosso) Distorsione, S (arancio) Saturazione, B (ciano) Bitcrush, R (viola) Reverb.
  Scelgono il pedale mostrato sullo schermo basso; il tasto acceso = pedale attivo.
- **Schermo basso**: catena dei pedali (click = seleziona), interruttore ON/OFF, frecce per spostare il pedale
  nella catena, 4 barre parametro + 4 barre "ENV" (quanto l'envelope li modula, da -100% a +100%).
  Le schede DIST / SAT / CRSH / VERB in alto accendono e spengono il rispettivo effetto.
  Doppio click su una barra = valore di default. Rotella del mouse = regolazione fine.

Skin: nella schermata PRESET (tasto SELECT) c'è la sezione SKIN con tre scocche: STEEL, BLACK e WHITE. La scelta viene salvata nel progetto e ricordata per le nuove istanze.

Preset utente: `Documenti/Groovy/G-POCKET/Presets/*.xml`.

## Grafica definitiva
Metti un PNG in `Resources/skin.png` (640x840, oppure 1280x1680 per schermi ad alta densità) e rilancia CMake:
viene usato come sfondo al posto del telaio disegnato nel codice. Gli schermi, le frecce preset e i tasti sono
componenti vivi disegnati sopra: le loro posizioni sono nel namespace `skin` in `Source/PluginEditor.h`
(rettangoli in pixel del contenuto 640x840) e vanno fatte coincidere con il disegno.

## Build su Windows (principale)
1. Installa Visual Studio 2022 (workload "Sviluppo di applicazioni desktop con C++") e CMake.
2. Da "Developer PowerShell for VS":
       cmake -S . -B build
       cmake --build build --config Release
3. Il plugin è in `build\GroovyRack_artefacts\Release\VST3\G-POCKET.vst3`
   -> copialo in `C:\Program Files\Common Files\VST3\` e rianalizza i plugin nella DAW.

## Build su Mac
       cmake -S . -B build
       cmake --build build --config Release
VST3 e AU sono in `build/GroovyRack_artefacts/Release/`. Universal (Intel + Apple Silicon).
Per distribuirlo ad altri servono firma e notarizzazione Apple.

## Build automatica Windows + Mac (GitHub Actions)
Carica la cartella su un repository GitHub: a ogni push il workflow `.github/workflows/build.yml`
compila entrambe le versioni e le trovi in "Actions -> Artifacts".


## Mac: se dice "è danneggiato e non può essere aperto"
I plugin scaricati da internet vengono bloccati da macOS. Copia i file in `~/Library/Audio/Plug-Ins/VST3` e `~/Library/Audio/Plug-Ins/Components`, poi incolla nel Terminale:

```
P=~/Library/Audio/Plug-Ins/VST3/G-POCKET.vst3; A=~/Library/Audio/Plug-Ins/Components/G-POCKET.component; for p in "$P" "$A"; do [ -e "$p" ] && { xattr -cr "$p"; chmod -R a+rX "$p"; chmod +x "$p"/Contents/MacOS/*; codesign --force --deep -s - "$p"; }; done; echo FATTO
```
Poi riapri la DAW e rifai la scansione (Logic Pro: Audio FX > Audio Units > Groovy > G-POCKET).
