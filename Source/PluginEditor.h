#pragma once

#include "PluginProcessor.h"
#include "UI/UpperScreen.h"
#include "UI/LowerScreen.h"
#include "UI/Handheld.h"

#ifndef GROOVY_HAS_SKIN
 #define GROOVY_HAS_SKIN 0   // diventa 1 se esiste Resources/skin.png (vedi CMakeLists.txt)
#endif

// Posizioni di tutti i pezzi nel contenuto a dimensione fissa (640 x 840).
// Per usare la grafica definitiva: metti Resources/skin.png (640x840 o 1280x1680) e, se serve,
// sistema qui i rettangoli in modo che coincidano con schermi, frecce preset e tasti del disegno.
namespace skin
{
constexpr int kW = 640, kH = 840;
inline juce::Rectangle<int> upperScreen() { return { 96, 40, 448, 336 }; }
inline juce::Rectangle<int> lowerScreen() { return { 96, 462, 448, 336 }; }
// i comandi laterali sono centrati a metà tra il bordo del guscio (x=20 / x=620) e lo schermo (x=96 / x=544)
inline juce::Rectangle<int> presetPad()   { return { 35, 580, 46, 100 }; }
// 4 tasti in colonna: centro della colonna, y del primo tasto e passo
inline int faceX()                        { return 582; }
constexpr int kFaceY0 = 560, kFaceSpacing = 48, kFaceSize = 42;
inline juce::Rectangle<int> selectBtn()   { return { 559, 480, 46, 13 }; }
inline juce::Rectangle<int> startBtn()    { return { 559, 498, 46, 13 }; }
} // namespace skin

// Contenuto dell'editor: due schermi + frecce preset + 4 tasti + SELECT/START
class RackContent : public juce::Component, private juce::Timer
{
public:
    explicit RackContent (GroovyRackProcessor& p);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // azioni (usate dai tasti, e richiamabili anche dai test)
    void selectPedal (int pedal);
    void toggleView();
    void stepPreset (int delta);
    void saveDialog();

    // apertura/chiusura del "guscio": con immediate=true salta direttamente allo stato finale (senza animazione)
    void setOpen (bool shouldBeOpen, bool immediate = false);
    bool isOpen() const { return progress >= 1.f; }
    void setProgress (float p) { progress = juce::jlimit (0.f, 1.f, p); setScreenPower (progress >= 1.f ? 1.f : 0.f); applyLayout(); repaint(); }   // per prove
    float screenPower() const { return power; }
    bool isGameMode() const { return ui.gameMode; }
    bool clickScrew (juce::Point<int> p) { return screwClick (p); }
    int getScrewStep() const { return screwStep; }   // per prove
    float skinTransition() const { return skinTrans; }   // 1 = nessuna transizione di skin in corso

    grv::UiState ui;
    grv::UpperScreen upper;
    grv::LowerScreen lower;

private:
    void timerCallback() override;
    void renderBase (juce::Graphics&);   // sfondo + metà bassa
    void renderLid (juce::Graphics&);    // metà alta (interno del coperchio), y 0..421
    void renderCover (juce::Graphics&);  // esterno del coperchio a guscio chiuso, y 421..840
    void applyLayout();
    bool screwClick (juce::Point<int> pos);   // easter egg: le 4 viti agli angoli da cliccare in ordine entro 5 secondi
    void activateGameMode();
public:
    ~RackContent() override;
private:
    void resetScrews();                 // spegne l'animazione delle viti e svita quelle girate
    void paintScrewFx (juce::Graphics&);
    float eased() const { return progress * progress * (3.f - 2.f * progress); }
    int currentDy() const { return juce::roundToInt (-211.f * (1.f - eased())); }

    juce::Image bodyImage, lidImage, coverImage;
    juce::Image prevBody, prevLid;   // aspetto della skin precedente, visibile mentre dura la transizione
    int screwStep = 0;               // quante viti giuste di fila sono state cliccate
    juce::uint32 screwStartMs = 0;   // istante del primo click della sequenza
    float screwAngle[4] = { 0.f, 0.f, 0.f, 0.f }, screwTarget[4] = { 0.f, 0.f, 0.f, 0.f };   // rotazione delle viti (animata)
    double screwPulse[4] = { -1, -1, -1, -1 }, screwErr[4] = { -1, -1, -1, -1 };          // ms dall'ultimo impulso / errore (-1 = niente)
    bool screwArmed[4] = { false, false, false, false };                                   // vite già "caricata" nella sequenza
    double kickMs = -1.0, kickDur = 200.0;                                                 // piccolo glitch degli schermi a ogni vite giusta
    float kickAmp = 0.f;
    double gameMs = 0.0;             // tempo trascorso da quando è partito il gioco (per il glitch)
    float skinTrans = 1.f;           // 0 = skin vecchia, 1 = transizione finita
    void beginSkinTransition();
    int shownSkin = -1;          // skin con cui sono state disegnate le immagini in cache
    float progress = 0.f;        // 0 = chiuso, 1 = aperto
    bool wantOpen = true;
    float power = 0.f;           // accensione degli schermi (pixel per pixel)
    void setScreenPower (float p) { power = juce::jlimit (0.f, 1.f, p); upper.setPower (power); lower.setPower (power); }
    juce::Image lidFrame;        // coperchio + schermo alto fotografato, usato mentre si muove
    double introDelayMs = 450.0, lastTickMs = 0.0;
    bool fastTimer = false;
    GroovyRackProcessor& proc;
    grv::PresetPad presetPad;
    std::array<std::unique_ptr<grv::FaceButton>, 4> faces;
    grv::PillButton selectBtn { "SELECT" }, startBtn { "START" };
};

class GroovyRackEditor : public juce::AudioProcessorEditor
{
public:
    explicit GroovyRackEditor (GroovyRackProcessor&);
    ~GroovyRackEditor() override = default;

    void resized() override;
    RackContent& getContent() { return content; }

private:
    RackContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovyRackEditor)
};
