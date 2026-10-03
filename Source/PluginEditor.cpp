#include "PluginEditor.h"

#if GROOVY_HAS_SKIN
 #include "BinaryData.h"
#endif

using namespace grv;

//==============================================================================
RackContent::RackContent (GroovyRackProcessor& p)
    : upper (p, ui), lower (p, ui), proc (p)
{
    addAndMakeVisible (upper);
    addAndMakeVisible (lower);
    addAndMakeVisible (presetPad);
    addAndMakeVisible (selectBtn);
    addAndMakeVisible (startBtn);

    // Iniziale e colore dell'effetto assegnato, in colonna: D S B R
    static const char* letters[4] = { "D", "S", "B", "R" };
    for (int i = 0; i < 4; ++i)
    {
        faces[(size_t) i] = std::make_unique<FaceButton> (letters[i], px::pal::pedal (i));
        faces[(size_t) i]->onClick = [this, i] { selectPedal (i); };
        addAndMakeVisible (*faces[(size_t) i]);
    }

    presetPad.onStep = [this] (int delta) { stepPreset (delta); };
    selectBtn.onClick = [this] { toggleView(); };
    startBtn.onClick = [this] { saveDialog(); };

    setSize (skin::kW, skin::kH);
    setScreenPower (0.f);   // il plugin si apre con gli schermi SPENTI: si accendono solo con l'animazione
#if GROOVY_HAS_SKIN
    setOpen (true, true);   // con la grafica esterna non c'è animazione
#endif
    lastTickMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
    fastTimer = true;
    applyLayout();
    timerCallback();
}

void RackContent::setOpen (bool shouldBeOpen, bool immediate)
{
    wantOpen = shouldBeOpen;
    if (immediate)
    {
        progress = shouldBeOpen ? 1.f : 0.f;
        setScreenPower (shouldBeOpen ? 1.f : 0.f);
        introDelayMs = 0.0;
        applyLayout();
        repaint();
    }
}

// Le 4 viti agli angoli (coordinate virtuali 640x840): in alto a sinistra, in alto a destra, in basso a sinistra,
// in basso a destra. Vanno cliccate una dopo l'altra, tutte entro 5 secondi dal primo click.
// A ogni vite giusta: la vite gira, parte un anello luminoso, gli schermi fanno un piccolo glitch (sempre più forte)
// e le viti già "caricate" restano accese: così si capisce subito che sta succedendo qualcosa.
namespace
{
const juce::Point<int> kScrewPos[4] = { { 44, 33 }, { 596, 33 }, { 44, 808 }, { 596, 808 } };
}

void RackContent::resetScrews()
{
    screwStep = 0;
    for (int i = 0; i < 4; ++i)
    {
        screwArmed[i] = false;
        screwTarget[i] = 0.f;      // le viti girate tornano indietro
    }
}

bool RackContent::screwClick (juce::Point<int> pos)
{
    int hit = -1;
    for (int i = 0; i < 4; ++i)
        if (pos.getDistanceFrom (kScrewPos[i]) <= 14)
            hit = i;
    if (hit < 0)
        return false;

    const auto now = juce::Time::getMillisecondCounter();
    if (screwStep > 0 && now - screwStartMs > 5000u)
        resetScrews();                               // tempo scaduto: si riparte da capo

    auto armScrew = [this] (int i)
    {
        screwArmed[i] = true;
        screwTarget[i] += juce::MathConstants<float>::halfPi;
        screwPulse[i] = 0.0;
        static const float amp[3] = { 0.22f, 0.38f, 0.65f };
        const int n = juce::jlimit (1, 3, screwStep);
        if (screwStep == 1)
            proc.loadSfx();                          // intanto si preparano i suoni, così alla quarta vite partono subito
        kickAmp = amp[n - 1];
        kickDur = 150.0 + 90.0 * n;
        kickMs = 0.0;
        timerCallback();
    };

    if (hit == screwStep)
    {
        if (screwStep == 0)
            screwStartMs = now;
        ++screwStep;
        if (screwStep == 4)
        {
            for (int i = 0; i < 4; ++i)
            {
                screwAngle[i] = screwTarget[i] = 0.f;
                screwArmed[i] = false;
            }
            screwStep = 0;
            activateGameMode();
        }
        else
            armScrew (hit);
    }
    else
    {
        resetScrews();                               // vite sbagliata: da capo (lampo rosso)
        screwErr[hit] = 0.0;
        if (hit == 0)                                // se è la prima, conta già come primo click
        {
            screwStep = 1;
            screwStartMs = now;
            armScrew (0);
        }
    }
    return true;
}

RackContent::~RackContent()
{
    proc.playSfx (grv::Sfx::StopAll);              // finestra chiusa: la sigla si ferma
}

void RackContent::activateGameMode()
{
    proc.loadSfx();
    proc.playSfx (grv::Sfx::PlayIntroThenTheme);   // all'ultima vite: suono d'entrata, poi parte la sigla
    gameMs = 0.0;
    ui.onGameTap = [this] { upper.tapGame(); };
    ui.onGameRelease = [this] { upper.releaseGame(); };
    upper.startGame();
    upper.setGlitch (1.f);
    lower.setGlitch (1.f);
    repaint();
}

void RackContent::mouseDown (const juce::MouseEvent& e)
{
    if (progress >= 1.f && wantOpen && ! ui.gameMode && screwClick (e.getPosition()))
        return;

    if (progress <= 0.f && ! wantOpen)
        setOpen (true);                                  // guscio chiuso: un click lo apre
    else if (progress >= 1.f && wantOpen && juce::Rectangle<int> (150, 396, 340, 50).contains (e.getPosition()))
        setOpen (false);                                 // click sulla cerniera: lo chiude
}

void RackContent::selectPedal (int pedal)
{
    ui.selectedPedal = juce::jlimit (0, 3, pedal);
    timerCallback();
}

void RackContent::toggleView()
{
    ui.naming = false;
    ui.view = ui.view == UiState::EnvelopeView ? UiState::PresetView : UiState::EnvelopeView;
}

void RackContent::stepPreset (int delta)
{
    proc.getPresets().step (delta);
}

void RackContent::saveDialog()
{
    // il tasto START apre la schermata del nome; premuto di nuovo (mentre si scrive) conferma
    if (ui.naming)
        upper.confirmNaming();
    else
        upper.beginNaming();
}

void RackContent::timerCallback()
{
    for (int i = 0; i < 4; ++i)
        faces[(size_t) i]->setState (proc.apvts.getParameter (onId (i))->getValue() > 0.5f, ui.selectedPedal == i);

#if ! GROOVY_HAS_SKIN
    if (shownSkin != proc.getSkin())
        beginSkinTransition();
#endif

    const double now = juce::Time::getMillisecondCounterHiRes();
    double dt = juce::jlimit (0.0, 60.0, now - lastTickMs);
    lastTickMs = now;

    if (introDelayMs > 0.0)
    {
        introDelayMs -= dt;
        dt = 0.0;
    }

    // apertura: il coperchio si alza, poi gli schermi si accendono pixel per pixel.
    // chiusura: prima gli schermi si spengono, poi il coperchio si abbassa.
    float target = wantOpen && introDelayMs <= 0.0 ? 1.f : 0.f;
    if (! wantOpen && power > 0.f)
        target = progress;                       // aspetta che gli schermi siano spenti

    bool animating = false;
    if (progress != target)
    {
        const float step = (float) (dt / 950.0);
        progress = target > progress ? juce::jmin (target, progress + step) : juce::jmax (target, progress - step);
        animating = true;
        applyLayout();
        repaint();
    }

    const float wantPower = progress >= 1.f && wantOpen ? 1.f : 0.f;
    if (power != wantPower)
    {
        const float step = (float) (dt / (wantPower > power ? 700.0 : 450.0));
        setScreenPower (wantPower > power ? juce::jmin (1.f, power + step) : juce::jmax (0.f, power - step));
        animating = true;
    }

    // easter egg: glitch di entrambi gli schermi all'attivazione, poi solo disturbi occasionali sullo schermo basso
    if (ui.gameMode)
    {
        gameMs += dt;
        const float burst = juce::jmax (0.f, 1.f - (float) (gameMs / 1100.0));
        const bool pulse = gameMs > 1100.0 && std::fmod (gameMs, 2600.0) < 140.0;
        upper.setGlitch (burst * burst);
        lower.setGlitch (juce::jmax (burst, pulse ? 0.45f : 0.f));
        animating = true;
        if (progress <= 0.f)                         // guscio chiuso: il gioco finisce, alla riapertura è tutto normale
        {
            ui.gameMode = false;
            proc.playSfx (grv::Sfx::StopAll);        // guscio chiuso: si ferma anche la sigla
            upper.setGlitch (0.f);
            lower.setGlitch (0.f);
        }
    }

    // animazione delle viti dell'easter egg
    {
        if (screwStep > 0 && juce::Time::getMillisecondCounter() - screwStartMs > 5000u)
            resetScrews();
        bool fx = false;
        for (int i = 0; i < 4; ++i)
        {
            screwAngle[i] += (screwTarget[i] - screwAngle[i]) * (1.f - std::exp ((float) (-dt / 70.0)));
            if (std::abs (screwTarget[i] - screwAngle[i]) < 0.004f)
                screwAngle[i] = screwTarget[i];
            if (screwPulse[i] >= 0.0 && (screwPulse[i] += dt) > 800.0) screwPulse[i] = -1.0;
            if (screwErr[i] >= 0.0 && (screwErr[i] += dt) > 500.0) screwErr[i] = -1.0;
            fx = fx || screwArmed[i] || screwAngle[i] != screwTarget[i] || screwAngle[i] != 0.f || screwPulse[i] >= 0.0 || screwErr[i] >= 0.0;
        }
        if (! ui.gameMode && kickMs >= 0.0)
        {
            kickMs += dt;
            const float k = 1.f - (float) (kickMs / kickDur);
            upper.setGlitch (k > 0.f ? kickAmp * k : 0.f);
            lower.setGlitch (k > 0.f ? kickAmp * k : 0.f);
            if (k <= 0.f)
                kickMs = -1.0;
            fx = true;
        }
        if (fx)
        {
            animating = true;
            repaint();
        }
    }

    if (skinTrans < 1.f)
    {
        skinTrans = juce::jmin (1.f, skinTrans + (float) (dt / 450.0));
        if (skinTrans >= 1.f)
            prevBody = prevLid = juce::Image();
        animating = true;
        repaint();
    }

    const bool needFast = animating || introDelayMs > 0.0;
    if (needFast != fastTimer)
    {
        fastTimer = needFast;
        startTimerHz (needFast ? 60 : 15);
    }
}

// la skin è cambiata: tiene l'aspetto vecchio e lo sostituisce a pixel (a blocchi, da sinistra a destra)
void RackContent::beginSkinTransition()
{
    if (shownSkin >= 0 && ! bodyImage.isNull() && progress >= 1.f)
    {
        prevBody = bodyImage;
        prevLid = lidImage;
        skinTrans = 0.f;
    }
    else
    {
        prevBody = prevLid = juce::Image();
        skinTrans = 1.f;
    }
    bodyImage = lidImage = coverImage = juce::Image();
    shownSkin = proc.getSkin();
}

void RackContent::applyLayout()
{
    const int dy = currentDy();
    const bool live = progress >= 1.f && wantOpen;

    auto place = [&] (juce::Component& c, juce::Rectangle<int> r)
    {
        c.setBounds (r.translated (0, dy));
        c.setInterceptsMouseClicks (live, live);
    };
    place (upper, skin::upperScreen());
    place (lower, skin::lowerScreen());
    place (presetPad, skin::presetPad());
    place (selectBtn, skin::selectBtn());
    place (startBtn, skin::startBtn());
    for (int i = 0; i < 4; ++i)
        place (*faces[(size_t) i], juce::Rectangle<int> (skin::kFaceSize, skin::kFaceSize)
                                       .withCentre ({ skin::faceX(), skin::kFaceY0 + i * skin::kFaceSpacing }));

    // a coperchio completamente aperto lo schermo alto è il componente vero (interagibile);
    // mentre il coperchio si muove se ne disegna una "fotografia" dentro il coperchio stesso
    upper.setVisible (progress >= 1.f);
    upper.setInterceptsMouseClicks (live, live);
}

void RackContent::resized()
{
    applyLayout();
}

//==============================================================================
namespace
{
// Piastra metallica satinata: stesso colore (blu-grigio) ma con bande di luce, venature "spazzolate",
// smusso illuminato dall'alto e riflesso diagonale.
void paintMetalPlate (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::uint32 seed, const Shell& sh)
{
    using namespace juce;
    Path plate;
    plate.addRoundedRectangle (r, corner);

    // ombra
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.f, 5.f), corner);

    {
        // corpo: gradiente con banda di riflesso netta a metà (effetto metallo)
        ColourGradient body (Colour (sh.top), r.getX(), r.getY(), Colour (sh.bottom), r.getX(), r.getBottom(), false);
        body.addColour (0.06, Colour (sh.s06));
        body.addColour (0.22, Colour (sh.s22));
        body.addColour (0.44, Colour (sh.s44));
        body.addColour (0.50, Colour (sh.horizon)); // orizzonte scuro del riflesso
        body.addColour (0.505, Colour (sh.s505));   // bordo netto
        body.addColour (0.70, Colour (sh.s70));
        body.addColour (0.90, Colour (sh.s90));
        g.setGradientFill (body);
        g.fillPath (plate);

        // venature spazzolate
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (plate);
            Random rnd ((int64) seed);
            for (float y = r.getY(); y < r.getBottom(); y += 1.f)
            {
                const int streaks = 1 + rnd.nextInt (2);
                for (int i = 0; i < streaks; ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth();
                    const float w = 30.f + rnd.nextFloat() * 260.f;
                    const bool light = rnd.nextBool();
                    g.setColour ((light ? Colours::white : Colours::black).withAlpha (0.01f + rnd.nextFloat() * 0.02f));
                    g.fillRect (x, y, w, 1.f);
                }
            }

            // riflesso diagonale morbido
            ColourGradient sheen (Colours::white.withAlpha (0.0f), r.getX(), r.getY() + r.getHeight() * 0.1f,
                                  Colours::white.withAlpha (0.0f), r.getRight(), r.getY() + r.getHeight() * 0.7f, false);
            sheen.addColour (0.35, Colours::white.withAlpha (0.0f));
            sheen.addColour (0.5, Colours::white.withAlpha (0.22f));
            sheen.addColour (0.65, Colours::white.withAlpha (0.0f));
            g.setGradientFill (sheen);
            g.fillPath (plate);

            // strato di vernice lucida: riflesso curvo sulla metà alta
            Path gloss;
            gloss.startNewSubPath (r.getX(), r.getY());
            gloss.lineTo (r.getRight(), r.getY());
            gloss.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.36f);
            gloss.quadraticTo (r.getCentreX(), r.getY() + r.getHeight() * 0.56f, r.getX(), r.getY() + r.getHeight() * 0.36f);
            gloss.closeSubPath();
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (sh.glossAlpha), r.getX(), r.getY(),
                                               Colours::white.withAlpha (sh.glossAlpha * 0.1f), r.getX(), r.getY() + r.getHeight() * 0.5f, false));
            g.fillPath (gloss);

            // riflesso speculare sottile in alto e luce di rimbalzo in basso
            g.setColour (Colours::white.withAlpha (0.55f));
            g.fillRoundedRectangle (r.getX() + corner, r.getY() + 3.f, r.getWidth() - corner * 2.f, 2.f, 1.f);
            g.setColour (Colours::white.withAlpha (0.10f));
            g.fillRoundedRectangle (r.getX() + corner, r.getBottom() - 8.f, r.getWidth() - corner * 2.f, 2.f, 1.f);
        }

    }

    // smusso: luce in alto, ombra in basso
    g.setGradientFill (ColourGradient (Colour (sh.bevelLight).withAlpha (0.9f), r.getX(), r.getY(),
                                       Colour (sh.bevelDark).withAlpha (0.9f), r.getX(), r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (1.2f), corner, 2.8f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (4.f), corner - 3.f, 1.f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (5.f), corner - 4.f, 1.f);

    // viti agli angoli
    for (auto c : { Point<float> (r.getX() + 24.f, r.getY() + 24.f), Point<float> (r.getRight() - 24.f, r.getY() + 24.f),
                    Point<float> (r.getX() + 24.f, r.getBottom() - 24.f), Point<float> (r.getRight() - 24.f, r.getBottom() - 24.f) })
    {
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - 5.f, c.y - 4.f, 10.f, 10.f);
        g.setGradientFill (ColourGradient (Colour (0xffd5d9ee), c.x - 4.f, c.y - 4.f, Colour (0xff4a4e68), c.x + 4.f, c.y + 4.f, false));
        g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9.f, 9.f);
        g.setColour (Colour (0xff20222f));
        g.drawLine (c.x - 3.f, c.y + 1.f, c.x + 3.f, c.y - 1.f, 1.4f);
    }
}
} // namespace

namespace
{
void paintScreenBezel (juce::Graphics& g, juce::Rectangle<int> screen)
{
    using namespace juce;
    const auto b = screen.toFloat().expanded (14.f);
    g.setColour (Colour (0xff07070c));
    g.fillRoundedRectangle (b, 10.f);
    g.setGradientFill (ColourGradient (Colour (0xff050508), b.getX(), b.getY(), Colour (0xff5a5f80), b.getX(), b.getBottom(), false));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.f, 1.6f);
}

void engravedText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, const Shell& sh)
{
    using namespace juce;
    g.setColour (Colour (sh.engraveLight).withAlpha (sh.engraveLightAlpha));
    g.drawText (t, r.translated (0, 1), Justification::centred);
    g.setColour (Colour (sh.engraveDark).withAlpha (sh.engraveDarkAlpha));
    g.drawText (t, r, Justification::centred);
}
} // namespace

namespace {
// testo piccolo "inciso" nel metallo (luce sul bordo basso, ombra dentro, bordo alto scuro), centrato in x
void engravedPathText (juce::Graphics& g, const juce::String& txt, float cx, float baseline, float size, const Shell& sh)
{
    using namespace juce;
    Path text;
    {
        GlyphArrangement ga;
        ga.addJustifiedText (Font (FontOptions (size, Font::bold).withKerningFactor (0.2f)), txt, cx - 150.f, baseline, 300.f,
                             Justification::horizontallyCentred);
        ga.createPath (text);
    }
    g.setColour (Colour (sh.engraveLight).withAlpha (jmin (0.9f, sh.engraveLightAlpha * 2.4f)));
    g.fillPath (text, AffineTransform::translation (0.f, 0.9f));
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (text);
    g.setGradientFill (ColourGradient (Colour (sh.engraveDark).withAlpha (0.85f), 0.f, baseline - size,
                                       Colour (sh.engraveDark).brighter (0.3f).withAlpha (0.5f), 0.f, baseline, false));
    g.fillPath (text);
    Path inverse;
    inverse.addRectangle (text.getBounds().expanded (4.f));
    inverse.addPath (text, AffineTransform::translation (0.f, 1.f));
    inverse.setUsingNonZeroWinding (false);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillPath (inverse);
}
}

// sfondo + metà bassa (schermo basso, tasti)
void RackContent::renderBase (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Colour (0xff0d0d12));
    const auto& sh = shellFor (proc.getSkin());
    paintMetalPlate (g, { 20.f, 430.f, 600.f, 402.f }, 30.f, 22, sh);
    paintScreenBezel (g, skin::lowerScreen());

    g.setFont (Font (FontOptions (10.f, Font::bold)));
    engravedText (g, "PRESET", Rectangle<int> (skin::presetPad().getX() - 10, skin::presetPad().getBottom() + 4, skin::presetPad().getWidth() + 20, 12), sh);
    engravedText (g, "FX", Rectangle<int> (skin::faceX() - 30, skin::kFaceY0 + 3 * skin::kFaceSpacing + 26, 60, 12), sh);

    // nome del plugin inciso in piccolo sulla parte metallica in basso (visibile solo a plugin aperto; non sulla skin CIRCUIT)
    if (! (GROOVY_LIMITED_EDITION && sh.limited))
        engravedPathText (g, "G-POCKET", 320.f, 806.f, 13.f, sh);
}

// metà alta: interno del coperchio (visto quando il guscio è aperto)
void RackContent::renderLid (juce::Graphics& g)
{
    using namespace juce;
    paintMetalPlate (g, { 20.f, 8.f, 600.f, 404.f }, 30.f, 11, shellFor (proc.getSkin()));
    paintScreenBezel (g, skin::upperScreen());
    g.setColour (px::pal::bg);   // schermo spento finché il guscio non è aperto del tutto
    g.fillRect (skin::upperScreen());

    // altoparlanti
    g.setColour (Colour (0xff0d0e17));
    for (int side = 0; side < 2; ++side)
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 4; ++col)
            {
                const float x = (float) (side == 0 ? 34 + col * 10 : 566 + col * 10), y = (float) (170 + row * 10);
                g.fillEllipse (x, y, 5.f, 5.f);
                g.setColour (Colours::white.withAlpha (0.12f));
                g.drawEllipse (x, y + 0.6f, 5.f, 5.f, 0.6f);
                g.setColour (Colour (0xff0d0e17));
            }
}

// esterno del coperchio (guscio chiuso): stessa piastra, con la firma INCISA nel metallo
void RackContent::renderCover (juce::Graphics& g)
{
    using namespace juce;
    const auto& sh = shellFor (proc.getSkin());
    paintMetalPlate (g, { 20.f, 430.f, 600.f, 402.f }, 30.f, 33, sh);


    // il testo diventa un contorno (path): l'incisione è fatta di luce sul bordo basso, ombra dentro e bordo alto scuro
    Path text;
    {
        GlyphArrangement ga;
        ga.addJustifiedText (Font (FontOptions (25.f, Font::bold)), "Made by @prodby_groovy", 60.f, 738.f, 520.f, Justification::horizontallyCentred);
        ga.createPath (text);
    }

    // 1. riflesso del bordo inferiore (il metallo "taglia" la luce)
    g.setColour (Colour (sh.engraveLight).withAlpha (jmin (0.9f, sh.engraveLightAlpha * 2.4f)));
    g.fillPath (text, AffineTransform::translation (0.f, 1.3f));

    // 2. interno incavato: scuro, ma trasparente così si vedono ancora le venature del metallo
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (text);
        g.setGradientFill (ColourGradient (Colour (sh.engraveDark).withAlpha (0.80f), 0.f, 715.f,
                                           Colour (sh.engraveDark).brighter (0.3f).withAlpha (0.45f), 0.f, 742.f, false));
        g.fillPath (text);

        // ombra interna sui bordi alti: tutto ciò che resta scoperto spostando il testo in giù
        Path inverse;
        inverse.addRectangle (text.getBounds().expanded (8.f));
        inverse.addPath (text, AffineTransform::translation (0.f, 1.8f));
        inverse.setUsingNonZeroWinding (false);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillPath (inverse);
    }

    // 3. filo scuro sul bordo superiore
    g.setColour (Colours::black.withAlpha (0.35f));
    g.strokePath (text, PathStrokeType (0.5f));
}

namespace
{
// Anta (coperchio) che ruota attorno alla cerniera, con un pizzico di prospettiva (il bordo lontano si allarga).
// lid = true: anta alzata (interno, sopra la cerniera); false: anta richiusa (esterno, sotto la cerniera).
void drawFlap (juce::Graphics& g, const juce::Image& img, bool lid, float theta, float pivot, float opacity)
{
    const float c = std::abs (std::cos (theta)), sn = std::sin (theta);
    if (c < 0.01f || opacity <= 0.f)
        return;

    const int rows = img.getHeight() / 2;
    constexpr int strips = 72;
    for (int i = 0; i < strips; ++i)
    {
        const int v0 = i * rows / strips, v1 = (i + 1) * rows / strips;
        auto uOf = [&] (float v) { return lid ? ((float) rows - v) / (float) rows : v / (float) rows; };
        auto yOf = [&] (float v) { return pivot + (lid ? -1.f : 1.f) * uOf (v) * (float) rows * c; };
        const int yA = juce::roundToInt (yOf ((float) v0)), yB = juce::roundToInt (yOf ((float) v1));
        const int top = juce::jmin (yA, yB), h = std::abs (yB - yA);
        if (h <= 0)
            continue;

        const float s = 1.f + 0.07f * uOf ((float) (v0 + v1) * 0.5f) * sn;
        const int dw = juce::roundToInt ((float) skin::kW * s), dx = juce::roundToInt ((float) skin::kW * 0.5f - (float) dw * 0.5f);
        g.setOpacity (opacity);
        g.drawImage (img, dx, top, dw, h, 0, v0 * 2, img.getWidth(), (v1 - v0) * 2);

        g.setColour (juce::Colours::black.withAlpha ((1.f - c) * 0.45f * opacity));   // si scurisce mentre si inclina
        g.fillRect (juce::roundToInt (320.f - 300.f * s), top, juce::roundToInt (600.f * s), h);
    }
    g.setOpacity (1.f);
}
} // namespace

void RackContent::paint (juce::Graphics& g)
{
#if GROOVY_HAS_SKIN
    const auto skinImage = juce::ImageCache::getFromMemory (BinaryData::skin_png, BinaryData::skin_pngSize);
    g.drawImage (skinImage, getLocalBounds().toFloat());
    return;
#endif

    // le parti statiche sono disegnate una volta a doppia risoluzione e riusate (di nuovo se cambia la skin)
    if (shownSkin != proc.getSkin())
        beginSkinTransition();
    if (bodyImage.isNull())
    {
        bodyImage = juce::Image (juce::Image::ARGB, skin::kW * 2, skin::kH * 2, true);
        { juce::Graphics ig (bodyImage); ig.addTransform (juce::AffineTransform::scale (2.f)); renderBase (ig); }

        lidImage = juce::Image (juce::Image::ARGB, skin::kW * 2, 421 * 2, true);
        { juce::Graphics ig (lidImage); ig.addTransform (juce::AffineTransform::scale (2.f)); renderLid (ig); }

        coverImage = juce::Image (juce::Image::ARGB, skin::kW * 2, (skin::kH - 421) * 2, true);
        { juce::Graphics ig (coverImage); ig.addTransform (juce::AffineTransform::scale (2.f).translated (0.f, -842.f)); renderCover (ig); }
    }
    g.fillAll (juce::Colour (0xff0d0d12));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (bodyImage, juce::Rectangle<float> (0.f, (float) currentDy(), (float) skin::kW, (float) skin::kH));

    // coperchio alzato (interno con altoparlanti e cornice): sta SOTTO lo schermo alto, che è un componente vivo
    const float theta = eased() * juce::MathConstants<float>::pi;
    if (theta >= juce::MathConstants<float>::halfPi)
    {
        const float pivot = 421.f + (float) currentDy();
        if (progress >= 1.f)
            g.drawImage (lidImage, juce::Rectangle<float> (0.f, pivot - 421.f, (float) skin::kW, 421.f));
        else
        {
            // coperchio in movimento: include la "fotografia" dello schermo alto (spento o acceso com'era)
            lidFrame = lidImage.createCopy();
            {
                juce::Graphics ig (lidFrame);
                ig.drawImageAt (upper.createComponentSnapshot (upper.getLocalBounds(), false, 2.f),
                                skin::upperScreen().getX() * 2, skin::upperScreen().getY() * 2);
            }
            drawFlap (g, lidFrame, true, theta, pivot, 1.f);
        }
    }

    // transizione di skin: i blocchi di pixel non ancora "cambiati" mostrano ancora l'aspetto vecchio
    if (skinTrans < 1.f && progress >= 1.f && ! prevBody.isNull() && ! prevLid.isNull())
    {
        constexpr int bs = 8;
        juce::RectangleList<int> keep;
        for (int by = 0; by * bs < skin::kH; ++by)
            for (int bx = 0; bx * bs < skin::kW; ++bx)
            {
                const juce::uint32 hsh = ((juce::uint32) bx * 73856093u) ^ ((juce::uint32) by * 19349663u);
                const float rnd = (float) ((hsh * 2654435761u) >> 16 & 0xffff) / 65536.f;
                const float key = 0.6f * (float) (bx * bs) / (float) skin::kW + 0.4f * rnd;
                if (key > skinTrans)
                    keep.addWithoutMerging (juce::Rectangle<int> (bx * bs, by * bs, bs, bs));
            }
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (keep);
        g.drawImage (prevBody, juce::Rectangle<float> (0.f, 0.f, (float) skin::kW, (float) skin::kH));
        g.drawImage (prevLid, juce::Rectangle<float> (0.f, 0.f, (float) skin::kW, 421.f));
    }
}

// viti animate dell'easter egg: testa che gira, anello che si espande, bagliore sulle viti già caricate, lampo rosso se sbagli
void RackContent::paintScrewFx (juce::Graphics& g)
{
    using namespace juce;
    if (progress < 1.f || ui.gameMode)
        return;
    const double nowMs = Time::getMillisecondCounterHiRes();
    const Colour cyan (0xff4de8ff);
    for (int i = 0; i < 4; ++i)
    {
        const bool active = screwArmed[i] || screwAngle[i] != 0.f || screwPulse[i] >= 0.0 || screwErr[i] >= 0.0;
        if (! active)
            continue;
        const Point<float> c = kScrewPos[i].toFloat();
        const float pop = screwPulse[i] >= 0.0 ? 1.f + 0.7f * jmax (0.f, 1.f - (float) (screwPulse[i] / 260.0)) : 1.f;
        const float rr = 4.5f * pop;

        // testa della vite (ridisegnata sopra quella statica) con la fessura che gira
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - rr, c.y - rr + 1.f, rr * 2.f, rr * 2.f);
        g.setGradientFill (ColourGradient (Colour (0xffd5d9ee), c.x - rr, c.y - rr, Colour (0xff4a4e68), c.x + rr, c.y + rr, false));
        g.fillEllipse (c.x - rr, c.y - rr, rr * 2.f, rr * 2.f);
        const float a = -0.32f + screwAngle[i];
        g.setColour (Colour (0xff20222f));
        g.drawLine (c.x - std::cos (a) * rr * 0.7f, c.y - std::sin (a) * rr * 0.7f,
                    c.x + std::cos (a) * rr * 0.7f, c.y + std::sin (a) * rr * 0.7f, 1.4f * pop);

        // bagliore ciano che pulsa: la vite è "caricata"
        if (screwArmed[i])
        {
            const float beat = 0.5f + 0.5f * std::sin ((float) nowMs * 0.012f);
            g.setColour (cyan.withAlpha (0.35f + 0.4f * beat));
            g.drawEllipse (c.x - 9.f, c.y - 9.f, 18.f, 18.f, 2.f);
            g.setColour (cyan.withAlpha (0.10f + 0.12f * beat));
            g.fillEllipse (c.x - 13.f, c.y - 13.f, 26.f, 26.f);
        }

        // anello che si espande
        if (screwPulse[i] >= 0.0)
            for (int k = 0; k < 2; ++k)
            {
                const float t = (float) (screwPulse[i] / 800.0) - 0.12f * (float) k;
                if (t <= 0.f || t >= 1.f)
                    continue;
                const float r = 7.f + 34.f * t;
                g.setColour (cyan.withAlpha ((1.f - t) * 0.9f));
                g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 2.5f * (1.f - t) + 0.8f);
            }

        // errore: lampo rosso
        if (screwErr[i] >= 0.0)
        {
            const float t = (float) (screwErr[i] / 500.0);
            g.setColour (Colour (0xffff3b4e).withAlpha (1.f - t));
            g.drawEllipse (c.x - 11.f, c.y - 11.f, 22.f, 22.f, 2.5f);
        }
    }
}

// coperchio che ruota attorno alla cerniera + cerniera
void RackContent::paintOverChildren (juce::Graphics& g)
{
#if GROOVY_HAS_SKIN
    return;
#endif
    using namespace juce;
    if (lidImage.isNull())
        return;

    paintScrewFx (g);

    const float dy = (float) currentDy();
    const float pivot = 421.f + dy;
    const float theta = eased() * MathConstants<float>::pi;
    const float c = std::abs (std::cos (theta));
    const bool covering = theta < MathConstants<float>::halfPi;
    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    // coperchio richiuso sopra la metà bassa: si vede l'esterno (con la firma)
    if (covering)
    {
        if (progress <= 0.f)
            g.drawImage (coverImage, Rectangle<float> (0.f, pivot, (float) skin::kW, (float) (skin::kH - 421)));
        else
            drawFlap (g, coverImage, false, theta, pivot, 1.f);
    }

    // cerniera
    const Rectangle<float> hinge (150.f, 400.f + dy, 340.f, 42.f);
    const auto& sh = shellFor (proc.getSkin());
    ColourGradient cyl (Colour (sh.hinge[0]), hinge.getX(), hinge.getY(), Colour (sh.hinge[0]), hinge.getX(), hinge.getBottom(), false);
    cyl.addColour (0.25, Colour (sh.hinge[1]));
    cyl.addColour (0.40, Colour (sh.hinge[2]));
    cyl.addColour (0.60, Colour (sh.hinge[3]));
    cyl.addColour (0.85, Colour (sh.hinge[4]));
    g.setGradientFill (cyl);
    g.fillRoundedRectangle (hinge, 8.f);
    g.setColour (Colour (0xff0d0d12).withAlpha (0.75f));
    for (float x : { 200.f, 320.f, 440.f })
        g.fillRect (x, hinge.getY() + 4.f, 2.f, hinge.getHeight() - 8.f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (hinge, 8.f, 1.f);

    // a 90° il coperchio è di taglio: si vede lo spessore del metallo, così la transizione non ha un "buco"
    if (c < 0.3f && progress > 0.f && progress < 1.f)
    {
        const float k = 1.f - c / 0.3f;
        const float h = 4.f + 10.f * (1.f - c);
        const Rectangle<float> edge (34.f, pivot - h * 0.5f, 572.f, h);
        ColourGradient eg (Colour (sh.bevelLight).withAlpha (k), edge.getX(), edge.getY(),
                           Colour (sh.s44).withAlpha (k), edge.getX(), edge.getBottom(), false);
        g.setGradientFill (eg);
        g.fillRoundedRectangle (edge, 3.f);
        g.setColour (Colours::black.withAlpha (0.5f * k));
        g.drawRoundedRectangle (edge, 3.f, 1.f);
    }

    // mentre il coperchio si muove, passa DAVANTI alla cerniera; a riposo la cerniera torna davanti
    const float a = jlimit (0.f, 1.f, (1.f - c) * 8.f);
    if (a > 0.f && progress > 0.f && progress < 1.f)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (hinge.getSmallestIntegerContainer().expanded (1));
        drawFlap (g, covering || lidFrame.isNull() ? coverImage : lidFrame, ! covering, theta, pivot, a);
    }
}

//==============================================================================
GroovyRackEditor::GroovyRackEditor (GroovyRackProcessor& p)
    : juce::AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);

    setResizable (true, true);
    setResizeLimits (skin::kW / 2, skin::kH / 2, skin::kW * 3 / 2, skin::kH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) skin::kW / (double) skin::kH);
    setSize (skin::kW, skin::kH);
}

void GroovyRackEditor::resized()
{
    const float scale = (float) getWidth() / (float) skin::kW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, skin::kW, skin::kH);
}
