#pragma once

#include "UpperScreen.h" // UiState, PixelBar

namespace grv
{
// Schermo basso: catena dei pedali, pedale selezionato con animazione, parametri e profondità envelope
class LowerScreen : public px::PixelScreen
{
public:
    LowerScreen (GroovyRackProcessor& p, UiState& u) : proc (p), ui (u) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto v = toPx (e);
        dragBarIndex = -1;
        if (ui.gameMode)
        {
            if (jumpRect().toFloat().contains (v))
            {
                jumpPressedAt = juce::Time::getMillisecondCounter();
                jumpHeld = true;
                if (ui.onGameTap)
                    ui.onGameTap();
            }
            return;
        }

        const auto order = currentOrder();
        for (int slot = 0; slot < 4; ++slot)
            if (tabRect (slot).toFloat().contains (v))
            {
                px::toggleParam (proc.apvts.getParameter (onId (order[(size_t) slot])));
                return;
            }

        if (onRect().toFloat().contains (v))
        {
            px::toggleParam (proc.apvts.getParameter (onId (ui.selectedPedal)));
            return;
        }
        if (arrowRect (0).toFloat().contains (v)) { moveSelected (-1); return; }
        if (arrowRect (1).toFloat().contains (v)) { moveSelected (+1); return; }

        dragBars = buildBars();
        for (size_t i = 0; i < dragBars.size(); ++i)
            if (dragBars[i].hit (v))
            {
                dragBarIndex = (int) i;
                dragBars[i].p->beginChangeGesture();
                dragBars[i].p->setValueNotifyingHost (dragBars[i].normAt (v.x));
                return;
            }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragBarIndex >= 0)
            dragBars[(size_t) dragBarIndex].p->setValueNotifyingHost (dragBars[(size_t) dragBarIndex].normAt (toPx (e).x));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (jumpHeld)
        {
            jumpHeld = false;
            if (ui.onGameRelease)
                ui.onGameRelease();
        }
        if (dragBarIndex >= 0)
            dragBars[(size_t) dragBarIndex].p->endChangeGesture();
        dragBarIndex = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (ui.gameMode)
            return;
        const auto v = toPx (e);
        for (auto& b : buildBars())
            if (b.hit (v))
            {
                b.p->beginChangeGesture();
                b.p->setValueNotifyingHost (b.p->getDefaultValue());
                b.p->endChangeGesture();
                return;
            }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (ui.gameMode)
            return;
        const auto v = toPx (e);
        for (auto& b : buildBars())
            if (b.hit (v))
            {
                b.p->beginChangeGesture();
                b.p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, b.p->getValue() + w.deltaY * 0.08f));
                b.p->endChangeGesture();
                return;
            }
    }

    void moveSelected (int direction)
    {
        auto order = currentOrder();
        const auto slot = (int) (std::find (order.begin(), order.end(), ui.selectedPedal) - order.begin());
        const int other = slot + direction;
        if (other < 0 || other > 3)
            return;
        std::swap (order[(size_t) slot], order[(size_t) other]);
        if (auto* prm = proc.apvts.getParameter ("order"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) encodeOrder (order)));
            prm->endChangeGesture();
        }
    }

protected:
    static juce::Rectangle<int> jumpRect() { return { 52, 78, 120, 48 }; }

    // schermata "GAME MODE" (easter egg): testo 8 bit con le due copie colorate sfalsate, come un segnale disturbato,
    // e il grande tasto JUMP con cui si gioca
    void drawGameMode (px::Canvas& c)
    {
        using namespace px;
        c.clear (pal::bg);
        const auto t = juce::Time::getMillisecondCounter();
        const bool jitter = (t / 90) % 11 == 0;
        const int jx = jitter ? (int) ((t / 90) % 5) - 2 : 0;

        for (int y = 0; y < kH; y += 2)
            c.fill (0, y, kW, 1, juce::Colour (0xff171728));
        c.frame ({ 4, 4, kW - 8, kH - 8 }, pal::grid);

        const int scale = 3, w = c.textWidth ("GAME MODE", scale), x = (kW - w) / 2 + jx, y = 36;
        c.text ("GAME MODE", x - 2, y, pal::red, scale);
        c.text ("GAME MODE", x + 2, y, pal::cyan, scale);
        c.text ("GAME MODE", x, y, juce::Colours::white, scale);

        // tasto JUMP (si "abbassa" per un attimo quando lo premi)
        const bool pressed = jumpHeld || t - jumpPressedAt < 130u;
        auto r = jumpRect();
        if (pressed)
            r = r.translated (0, 3);
        else
            c.fill (r.translated (0, 3), juce::Colour (0xff7a1f2a));
        c.fill (r, pressed ? juce::Colour (0xffff9aa4) : pal::red);
        c.frame (r, juce::Colour (0xffb0303e));
        c.fill (r.getX() + 2, r.getY() + 2, r.getWidth() - 4, 2, juce::Colours::white.withAlpha (pressed ? 0.f : 0.6f));
        c.textCentre ("JUMP", r.getCentreX(), r.getCentreY() - 10, juce::Colours::white, 4);

        c.textCentre ("CLOSE THE SHELL TO EXIT", 112, 146, juce::Colour (0xff8a96ba));
    }

    void draw (px::Canvas& c) override
    {
        using namespace px;
        if (ui.gameMode)
        {
            drawGameMode (c);
            return;
        }
        c.clear (pal::bg);

        const int ped = ui.selectedPedal;
        const auto& def = pedalDefs()[ped];
        const auto col = pal::pedal (ped);
        const bool on = proc.apvts.getParameter (onId (ped))->getValue() > 0.5f;
        const auto order = currentOrder();

        // catena
        static const char* shortNames[4] = { "DIST", "SAT", "CRSH", "VERB" };
        for (int slot = 0; slot < 4; ++slot)
        {
            const int p = order[(size_t) slot];
            const auto r = tabRect (slot);
            const bool sel = p == ped;
            const bool pOn = proc.apvts.getParameter (onId (p))->getValue() > 0.5f;
            c.panel (r, sel ? pal::pedal (p) : pal::panel, sel ? pal::pedal (p) : pal::grid);
            c.fill (r.getX() + 4, r.getCentreY() - 1, 3, 3, pOn ? (sel ? pal::bg : pal::pedal (p)) : pal::dim);
            c.textCentre (shortNames[p], r.getCentreX() + 3, r.getY() + 2, sel ? pal::bg : (pOn ? pal::light : pal::dim));
            if (slot < 3)
                c.triangle (r.getRight() + 1, r.getCentreY(), 1, 1, pal::dim);
        }

        // arte + titolo
        drawArt (c, ped, artBox(), on);
        c.text (def.name, 74, 18, on ? col : pal::dim, 2);

        const auto sw = onRect();
        c.panel (sw, on ? col : pal::panel, on ? col : pal::grid);
        c.textCentre (on ? "ON" : "OFF", sw.getCentreX(), sw.getY() + 5, on ? pal::bg : pal::dim);

        const int slot = (int) (std::find (order.begin(), order.end(), ped) - order.begin());
        c.triangle (arrowRect (0).getCentreX(), arrowRect (0).getCentreY(), 0, 3, slot > 0 ? pal::light : pal::dim);
        c.triangle (arrowRect (1).getCentreX(), arrowRect (1).getCentreY(), 1, 3, slot < 3 ? pal::light : pal::dim);
        c.textCentre ("SLOT " + juce::String (slot + 1), 160, 51, pal::mid);

        // parametri
        const auto bars = buildBars();
        for (int k = 0; k < 4; ++k)
        {
            const int y = rowY (k);
            c.text (def.p[k].label, 4, y, on ? col : pal::dim);
            c.textRight (px::paramText (bars[(size_t) k * 2].p), 150, y, pal::light);
            c.text ("ENV", 158, y, pal::dim);
            c.textRight (px::paramText (bars[(size_t) k * 2 + 1].p), 220, y, pal::mid);
            bars[(size_t) k * 2].draw (c);
            bars[(size_t) k * 2 + 1].draw (c);
        }
    }

private:
    static int rowY (int k) { return 68 + k * 24; }
    static juce::Rectangle<int> tabRect (int slot) { return { 2 + slot * 56, 2, 52, 12 }; }
    static juce::Rectangle<int> artBox() { return { 4, 18, 64, 46 }; }
    static juce::Rectangle<int> onRect() { return { 74, 46, 44, 16 }; }
    static juce::Rectangle<int> arrowRect (int i) { return i == 0 ? juce::Rectangle<int> { 124, 47, 14, 14 }
                                                                  : juce::Rectangle<int> { 182, 47, 14, 14 }; }

    grv::Order currentOrder() const { return decodeOrder ((int) proc.apvts.getRawParameterValue ("order")->load()); }

    std::vector<px::PixelBar> buildBars() const
    {
        std::vector<px::PixelBar> v;
        const int ped = ui.selectedPedal;
        for (int k = 0; k < 4; ++k)
        {
            const int y = rowY (k) + 9;
            v.push_back (px::PixelBar::make (proc.apvts.getParameter (pid (ped, k)), 4, y, 146, 8, 24, px::pal::pedal (ped)));
            v.push_back (px::PixelBar::make (proc.apvts.getParameter (modId (ped, k)), 158, y, 62, 8, 20, px::pal::light, true));
        }
        return v;
    }

    float plain (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    // Ogni pedale ha un mini "schermo" che mostra quello che fa davvero, con i parametri attuali
    void drawArt (px::Canvas& c, int ped, juce::Rectangle<int> box, bool on)
    {
        using namespace px;
        const auto col = on ? pal::pedal (ped) : pal::dim;
        const auto faint = on ? pal::pedal (ped).withAlpha (0.35f) : pal::grid;
        c.panel (box, juce::Colour (0xff0b0b16), on ? col : pal::grid);

        const auto in = box.reduced (2);
        const int w = in.getWidth(), cy = in.getCentreY(), amp = in.getHeight() / 2 - 2;
        c.dottedH (in.getX(), cy, w, pal::grid);

        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        auto s = [&] (int x) { return std::sin (juce::MathConstants<float>::twoPi * 2.f * (float) x / (float) w + (float) t * 3.f); };

        auto plotFn = [&] (auto fn, juce::Colour colour)
        {
            int prev = 0;
            for (int x = 0; x < w; ++x)
            {
                const int y = cy - juce::roundToInt (juce::jlimit (-1.f, 1.f, fn (x)) * (float) amp);
                if (x == 0) c.plot (in.getX(), y, colour);
                else        c.line (in.getX() + x - 1, prev, in.getX() + x, y, colour);
                prev = y;
            }
        };

        switch (ped)
        {
            case 0: // distorsione: onda in ingresso vs onda tagliata
            {
                const float g = grv::dbToGain (plain ("dist_drive")) * 1.2f;
                plotFn ([&] (int x) { return s (x); }, faint);
                plotFn ([&] (int x) { return g * s (x); }, col);
                break;
            }
            case 1: // saturazione: curva morbida (tanh) con bias = calore
            {
                const float g = grv::dbToGain (plain ("sat_drive"));
                const float bias = plain ("sat_warm") * 0.4f;
                plotFn ([&] (int x) { return s (x); }, faint);
                plotFn ([&] (int x) { return (std::tanh (g * (s (x) + bias)) - std::tanh (g * bias)) / std::tanh (g); }, col);
                break;
            }
            case 2: // bitcrush: la scala a gradini è proprio bit + sample rate
            {
                const float levels = std::pow (2.f, juce::jmax (1.f, plain ("crush_bits")) - 1.f);
                const int step = juce::jlimit (1, 20, juce::roundToInt (plain ("crush_rate")));
                plotFn ([&] (int x) { return s (x); }, faint);
                int prev = 0;
                for (int x = 0; x < w; ++x)
                {
                    const float q = std::round (s ((x / step) * step) * levels) / levels;
                    const int y = cy - juce::roundToInt (juce::jlimit (-1.f, 1.f, q) * (float) amp);
                    if (x == 0) c.plot (in.getX(), y, col);
                    else        c.line (in.getX() + x - 1, prev, in.getX() + x, y, col);
                    prev = y;
                }
                break;
            }
            default: // riverbero: barre = riflessioni che decadono (size/damp), L sopra e R sotto (width)
            {
                const float size = plain ("verb_size"), damp = plain ("verb_damp"), width = plain ("verb_width");
                const float decay = juce::jlimit (0.3f, 0.97f, 0.62f + 0.36f * size - 0.2f * damp);
                const int spacing = 3 + juce::roundToInt (size * 3.f);
                const int count = (w - 4) / spacing;
                const int hot = (int) (t * 9.0) % juce::jmax (1, count);
                for (int i = 0; i < count; ++i)
                {
                    const int hgt = juce::jmax (1, juce::roundToInt ((float) amp * std::pow (decay, (float) i)));
                    const int x = in.getX() + 2 + i * spacing;
                    const auto bc = i == hot && on ? pal::light : col;
                    c.fill (x, cy - hgt, 2, hgt, bc);
                    c.fill (x + juce::roundToInt (width * 2.f), cy + 1, 2, juce::jmax (1, hgt * 9 / 10), i == hot && on ? pal::light : faint);
                }
                break;
            }
        }
    }

    GroovyRackProcessor& proc;
    UiState& ui;
    juce::uint32 jumpPressedAt = 0;
    bool jumpHeld = false;
    std::vector<px::PixelBar> dragBars;
    int dragBarIndex = -1;
};
} // namespace grv
