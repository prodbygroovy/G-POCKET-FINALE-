#pragma once

#include "PixelBar.h"
#include <functional>
#include "Shell.h"
#include "Game.h"
#include "../PluginProcessor.h"

namespace grv
{
struct UiState
{
    enum View { EnvelopeView = 0, PresetView = 1 };
    int selectedPedal = 0;
    View view = EnvelopeView;
    bool gameMode = false;        // easter egg: lo schermo alto diventa un gioco
    std::function<void()> onGameRelease;   // rilasciato JUMP
    std::function<void()> onGameTap;   // premuto JUMP (sullo schermo basso): fa saltare la pallina
    bool naming = false;          // schermata per scrivere il nome di un preset da salvare
    juce::String nameBuf;
};

// Schermo alto: envelope (curva disegnabile) + oscilloscopio, oppure lista preset
class UpperScreen : public px::PixelScreen
{
public:
    UpperScreen (GroovyRackProcessor& p, UiState& u) : proc (p), ui (u)
    {
        // suoni del gioco: salto e morte (la sigla non si ferma quando si muore)
        game.onEvent = [this] (int e) { proc.playSfx (e == 0 ? Sfx::PlayJump : Sfx::PlayDeath); };
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto v = toPx (e);
        dragBarIndex = dragPoint = dragSegment = -1;
        pts.clear();

        if (ui.gameMode)
            return;                                  // si gioca solo col tasto JUMP dello schermo basso

        // oscilloscopio: un click lo ingrandisce a tutto schermo, un altro click lo riporta com'era
        if (ui.view == UiState::EnvelopeView && (scopeFull || scopeBox().toFloat().contains (v)))
        {
            scopeFull = ! scopeFull;
            return;
        }

        if (ui.view == UiState::PresetView && ui.naming)
        {
            nameMouse (v);
            return;
        }

        if (ui.view == UiState::PresetView)
        {
            if (saveRect().toFloat().contains (v))
            {
                beginNaming();
                return;
            }
            if (delRect().toFloat().contains (v))
            {
                auto& pm = proc.getPresets();
                if (pm.category (pm.current()) != 3)
                    delPending = false;                                  // i preset di fabbrica non si eliminano
                else if (delArmed())
                {
                    pm.deleteUser (pm.current());
                    delPending = false;
                }
                else
                    { delArmedAt = juce::Time::getMillisecondCounter(); delPending = true; }   // primo click: chiede conferma
                return;
            }
            delPending = false;
            for (int i = 0; i < kNumShells; ++i)
                if (skinRect (i).toFloat().contains (v))
                {
                    proc.setSkin (i);
                    return;
                }
            const auto rows = listRect();
            if (rows.toFloat().contains (v))
            {
                const int idx = firstVisible() + (int) ((v.y - (float) rows.getY()) / (float) kRowH);
                proc.getPresets().load (idx);
            }
            return;
        }

        // schede modo
        for (int i = 0; i < 3; ++i)
            if (tabRect (i).toFloat().contains (v))
            {
                px::setChoice (param ("env_mode"), i);
                return;
            }
        if (toggleRect().toFloat().contains (v))
        {
            px::toggleParam (param ("env_on"));
            return;
        }

        // barre e selettori dei parametri di modo
        if (modeControlsMouse (v))
            return;

        // editor della curva
        pts = proc.getCurve().getPoints();
        const int hp = hitPoint (v);
        if (e.mods.isPopupMenu())
        {
            if (hp > 0 && hp < (int) pts.size() - 1)
            {
                pts.erase (pts.begin() + hp);
                commit();
            }
            return;
        }
        if (hp >= 0)
            dragPoint = hp;
        else
            dragSegment = hitHandle (v);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto v = toPx (e);

        if (dragBarIndex >= 0)
        {
            auto& b = dragBars[(size_t) dragBarIndex];
            b.p->setValueNotifyingHost (b.normAt (v.x));
            return;
        }
        if (pts.empty())
            return;

        if (dragPoint >= 0)
        {
            auto f = fromArea (v);
            if (e.mods.isShiftDown())
            {
                f.x = std::round (f.x * 16.f) / 16.f;
                f.y = std::round (f.y * 8.f) / 8.f;
            }
            auto& p = pts[(size_t) dragPoint];
            p.y = juce::jlimit (0.f, 1.f, f.y);
            if (dragPoint > 0 && dragPoint < (int) pts.size() - 1)
            {
                const float lo = pts[(size_t) dragPoint - 1].x + 0.0005f;
                const float hi = pts[(size_t) dragPoint + 1].x - 0.0005f;
                p.x = juce::jlimit (lo, juce::jmax (lo, hi), f.x);
            }
            commit();
        }
        else if (dragSegment >= 0)
        {
            const auto& a = pts[(size_t) dragSegment];
            const auto& b = pts[(size_t) dragSegment + 1];
            if (std::abs (b.y - a.y) > 0.01f)
            {
                const float my = fromArea (v).y;
                const float s = juce::jlimit (0.02f, 0.98f, (my - a.y) / (b.y - a.y));
                const float pw = std::log (s) / std::log (0.5f);
                pts[(size_t) dragSegment].curve = juce::jlimit (-1.f, 1.f, std::log (pw) / std::log (8.f));
                commit();
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragBarIndex >= 0)
            dragBars[(size_t) dragBarIndex].p->endChangeGesture();
        dragBarIndex = dragPoint = dragSegment = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (ui.gameMode)
            return;
        if (ui.view != UiState::EnvelopeView || scopeFull)
            return;
        const auto v = toPx (e);

        // doppio click su una barra = valore di default
        for (auto& b : paramBars())
            if (b.p != nullptr && b.hit (v))
            {
                b.p->beginChangeGesture();
                b.p->setValueNotifyingHost (b.p->getDefaultValue());
                b.p->endChangeGesture();
                return;
            }

        if (! envArea().expanded (4).toFloat().contains (v))
            return;

        pts = proc.getCurve().getPoints();
        if (const int hp = hitPoint (v); hp >= 0)
        {
            if (hp > 0 && hp < (int) pts.size() - 1)
            {
                pts.erase (pts.begin() + hp);
                commit();
            }
            return;
        }
        if (const int hs = hitHandle (v); hs >= 0)
        {
            pts[(size_t) hs].curve = 0.f;
            commit();
            return;
        }

        const auto f = fromArea (v);
        grv::EnvelopeCurve::Point np { juce::jlimit (0.f, 1.f, f.x), juce::jlimit (0.f, 1.f, f.y), 0.f };
        auto pos = std::upper_bound (pts.begin(), pts.end(), np, [] (const auto& a, const auto& b) { return a.x < b.x; });
        pts.insert (pos, np);
        commit();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (ui.gameMode)
            return;
        const auto v = toPx (e);
        if (ui.view == UiState::PresetView)
        {
            proc.getPresets().step (w.deltaY > 0 ? -1 : 1);
            return;
        }
        if (scopeFull)
            return;
        for (auto& b : paramBars())
            if (b.p != nullptr && b.hit (v))
            {
                b.p->beginChangeGesture();
                b.p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, b.p->getValue() + w.deltaY * 0.08f));
                b.p->endChangeGesture();
                return;
            }
    }

protected:
    void draw (px::Canvas& c) override
    {
        const bool wantFast = ui.gameMode;
        if (wantFast != fastRefresh)
        {
            fastRefresh = wantFast;
            setRefreshRate (wantFast ? 60 : 30);
        }
        if (ui.gameMode)
        {
            game.update();
            game.draw (c);
            return;
        }
        c.clear (px::pal::bg);
        if (ui.view == UiState::EnvelopeView)
        {
            if (scopeFull) drawScope (c, { 3, 3, 218, 162 });
            else           drawEnvelopeView (c);
        }
        else if (ui.naming)
            drawNameView (c);
        else
            drawPresetView (c);
    }

public:
    // easter egg: avvia il gioco della pallina
    void startGame()
    {
        game.reset();
        ui.gameMode = true;
        ui.naming = false;
    }
    const RunnerGame& getGame() const { return game; }
    RunnerGame& gameRef() { return game; }
    void tapGame() { game.tap(); }
    void releaseGame() { game.release(); }

    // salvataggio: si scrive il nome con la tastiera a schermo (funziona in qualsiasi DAW, senza finestre di sistema)
    void beginNaming()
    {
        auto& pm = proc.getPresets();
        ui.view = UiState::PresetView;
        ui.naming = true;
        ui.nameBuf = pm.category (pm.current()) == 3 ? pm.name (pm.current()) : pm.suggestUserName();
        delPending = false;
    }

    void confirmNaming()
    {
        if (! ui.naming)
            return;
        if (ui.nameBuf.trim().isNotEmpty() && proc.getPresets().saveUser (ui.nameBuf))
            ui.naming = false;
    }

private:
    using Point = grv::EnvelopeCurve::Point;
    static constexpr int kRowH = 10, kVisibleRows = 11;
    bool scopeFull = false;

    juce::RangedAudioParameter* param (const char* id) const { return proc.apvts.getParameter (id); }
    int modeIndex() const { return px::choiceIndex (param ("env_mode")); }

    static juce::Rectangle<int> tabRect (int i)
    {
        static const int x[3] = { 3, 34, 77 }, w[3] = { 29, 41, 29 };
        return { x[i], 2, w[i], 11 };
    }
    static juce::Rectangle<int> toggleRect() { return { 176, 2, 45, 11 }; }
    static juce::Rectangle<int> envBox() { return { 3, 15, 148, 150 }; }
    static juce::Rectangle<int> envArea() { return envBox().reduced (7); }
    static juce::Rectangle<int> scopeBox() { return { 156, 15, 65, 36 }; }
    static juce::Rectangle<int> listRect() { return { 3, 13, 212, kRowH * kVisibleRows }; }
    static juce::Rectangle<int> skinRect (int i)
    {
        // con 4 skin: 4 riquadri stretti; con 3 (versione pubblica) i riquadri si allargano e non resta nessun posto vuoto
        const int w = kNumShells >= 4 ? 50 : 70, step = kNumShells >= 4 ? 54 : 74;
        return { 3 + i * step, 141, w, 24 };
    }
    static juce::Rectangle<int> saveRect() { return { 148, 1, 35, 11 }; }
    static juce::Rectangle<int> delRect() { return { 186, 1, 35, 11 }; }
    static juce::Rectangle<int> cancelRect() { return { 172, 1, 49, 11 }; }

    // tastiera a schermo: 10 colonne x 4 righe (A-Z, 0-9, trattino, spazio, cancella, OK)
    static constexpr int kKeyW = 21, kKeyH = 22, kKeyX = 7, kKeyY = 40, kMaxName = 16;
    static juce::Rectangle<int> keyRect (int i) { return { kKeyX + (i % 10) * kKeyW, kKeyY + (i / 10) * kKeyH, kKeyW - 1, kKeyH - 1 }; }
    static juce::String keyChars() { return "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; }
    RunnerGame game;
    bool fastRefresh = false;
    juce::uint32 delArmedAt = 0;
    bool delPending = false;   // true dopo il primo click su DEL, finché non passano 2,5 s
    bool delArmed() const { return delPending && juce::Time::getMillisecondCounter() - delArmedAt < 2500u; }

    void nameMouse (juce::Point<float> v)
    {
        if (cancelRect().toFloat().contains (v))
        {
            ui.naming = false;
            return;
        }
        for (int i = 0; i < 40; ++i)
            if (keyRect (i).toFloat().contains (v))
            {
                if (i < 36)
                {
                    if (ui.nameBuf.length() < kMaxName)
                        ui.nameBuf += keyChars().substring (i, i + 1);
                }
                else if (i == 36) { if (ui.nameBuf.length() < kMaxName) ui.nameBuf += "-"; }
                else if (i == 37) { if (ui.nameBuf.length() < kMaxName) ui.nameBuf += " "; }
                else if (i == 38) ui.nameBuf = ui.nameBuf.dropLastCharacters (1);
                else              confirmNaming();
                return;
            }
    }

    int firstVisible() const
    {
        const int n = proc.getPresets().size();
        return juce::jlimit (0, juce::jmax (0, n - kVisibleRows), proc.getPresets().current() - kVisibleRows / 2);
    }

    //==========================================================================
    // Barre dei parametri di modo (colonna destra)
    std::vector<px::PixelBar> paramBars() const
    {
        using B = px::PixelBar;
        std::vector<B> v;
        const int x = 156, w = 65;
        auto rowY = [] (int i) { return 54 + 17 * i + 9; };

        switch (modeIndex())
        {
            case grv::MidiMode:
                v.push_back (B::make (param ("env_time"), x, rowY (0), w, 5, 16, px::pal::yellow));
                break;
            case grv::FollowerMode:
                v.push_back (B::make (param ("env_attack"), x, rowY (0), w, 5, 16, px::pal::yellow));
                v.push_back (B::make (param ("env_release"), x, rowY (1), w, 5, 16, px::pal::yellow));
                v.push_back (B::make (param ("env_gain"), x, rowY (2), w, 5, 16, px::pal::yellow, true));
                break;
            default: break;
        }
        return v;
    }

    bool modeControlsMouse (juce::Point<float> v)
    {
        dragBars = paramBars();
        for (size_t i = 0; i < dragBars.size(); ++i)
            if (dragBars[i].hit (v))
            {
                dragBarIndex = (int) i;
                dragBars[i].p->beginChangeGesture();
                dragBars[i].p->setValueNotifyingHost (dragBars[i].normAt (v.x));
                return true;
            }

        const int mode = modeIndex();
        const juce::Rectangle<int> row1 { 156, 54 + 17, 65, 16 };
        if (mode == grv::MidiMode && row1.toFloat().contains (v))
        {
            px::toggleParam (param ("env_loop"));
            return true;
        }
        if (mode == grv::SyncMode)
        {
            const juce::Rectangle<int> left { 154, 62, 14, 12 }, right { 208, 62, 14, 12 };
            const int idx = px::choiceIndex (param ("env_rate"));
            if (left.toFloat().contains (v)) { px::setChoice (param ("env_rate"), juce::jmax (0, idx - 1)); return true; }
            if (right.toFloat().contains (v)) { px::setChoice (param ("env_rate"), juce::jmin (grv::kNumRates - 1, idx + 1)); return true; }
        }
        return false;
    }

    //==========================================================================
    // Curva
    juce::Point<float> toArea (float x, float y) const
    {
        const auto a = envArea();
        return { (float) a.getX() + x * (float) (a.getWidth() - 1), (float) (a.getBottom() - 1) - y * (float) (a.getHeight() - 1) };
    }
    juce::Point<float> fromArea (juce::Point<float> p) const
    {
        const auto a = envArea();
        return { (p.x - (float) a.getX()) / (float) (a.getWidth() - 1),
                 ((float) (a.getBottom() - 1) - p.y) / (float) (a.getHeight() - 1) };
    }
    juce::Point<float> handlePos (const std::vector<Point>& v, size_t i) const
    {
        const auto& a = v[i];
        const auto& b = v[i + 1];
        return toArea ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * grv::EnvelopeCurve::shape (0.5f, a.curve));
    }
    int hitPoint (juce::Point<float> v) const
    {
        for (size_t i = 0; i < pts.size(); ++i)
            if (toArea (pts[i].x, pts[i].y).getDistanceFrom (v) < 5.f)
                return (int) i;
        return -1;
    }
    int hitHandle (juce::Point<float> v) const
    {
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            if (handlePos (pts, i).getDistanceFrom (v) < 5.f)
                return (int) i;
        return -1;
    }
    void commit()
    {
        proc.getCurve().setPoints (pts);
        proc.storeCurveInState();
    }

    //==========================================================================
    void drawEnvelopeView (px::Canvas& c)
    {
        using namespace px;
        const int mode = modeIndex();
        const bool envOn = param ("env_on")->getValue() > 0.5f;

        // schede modo + interruttore
        static const char* names[3] = { "MIDI", "FOLLOW", "SYNC" };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = tabRect (i);
            const bool sel = i == mode;
            c.panel (r, sel ? pal::yellow : pal::panel, sel ? pal::yellow : pal::grid);
            c.textCentre (names[i], r.getCentreX(), r.getY() + 2, sel ? pal::bg : pal::mid);
        }
        {
            const auto r = toggleRect();
            c.panel (r, pal::panel, envOn ? pal::yellow : pal::grid);
            c.text ("ENV", r.getX() + 4, r.getY() + 2, envOn ? pal::light : pal::dim);
            c.fill (r.getRight() - 12, r.getY() + 3, 5, 5, envOn ? pal::yellow : pal::dim);
        }

        // riquadro envelope
        const auto box = envBox();
        const auto a = envArea();
        c.panel (box, juce::Colour (0xff0b0b16), pal::grid);
        for (int i = 0; i <= 8; ++i)
            c.dottedV (a.getX() + i * (a.getWidth() - 1) / 8, a.getY(), a.getHeight(), pal::grid);
        for (int i = 0; i <= 4; ++i)
            c.dottedH (a.getX(), a.getY() + i * (a.getHeight() - 1) / 4, a.getWidth(), pal::grid);

        const auto colour = envOn ? pal::yellow : pal::dim;
        auto& curve = proc.getCurve();
        int prevY = 0;
        for (int i = 0; i < a.getWidth(); ++i)
        {
            const float x = (float) i / (float) (a.getWidth() - 1);
            const auto sp = toArea (x, curve.eval (x));
            const int px_ = a.getX() + i, py = juce::roundToInt (sp.y);
            c.dither (px_, py + 1, 1, a.getBottom() - py - 1, colour.withAlpha (0.55f));
            if (i == 0)
                c.plot (px_, py, colour);
            else
                c.line (px_ - 1, prevY, px_, py, colour);
            prevY = py;
        }

        const auto points = proc.getCurve().getPoints();
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            const auto h = handlePos (points, i);
            c.frame ({ (int) std::round (h.x) - 1, (int) std::round (h.y) - 1, 3, 3 }, pal::light);
        }
        for (auto& p : points)
        {
            const auto sp = toArea (p.x, p.y);
            const int x = (int) std::round (sp.x), y = (int) std::round (sp.y);
            c.fill (x - 2, y - 2, 5, 5, pal::bg);
            c.fill (x - 1, y - 1, 3, 3, pal::light);
            c.frame ({ x - 2, y - 2, 5, 5 }, pal::light);
        }

        // posizione "live"
        const float ph = proc.uiPhase.load (std::memory_order_relaxed);
        const float val = proc.uiValue.load (std::memory_order_relaxed);
        const auto head = toArea (ph, val);
        c.dottedV ((int) std::round (head.x), a.getY(), a.getHeight(), pal::light);
        c.fill ((int) std::round (head.x) - 1, (int) std::round (head.y) - 1, 3, 3, juce::Colours::white);

        drawScope (c, scopeBox());

        // controlli del modo
        auto label = [&] (int row, const juce::String& text, const juce::String& value = {})
        {
            c.text (text, 156, 54 + 17 * row, pal::mid);
            if (value.isNotEmpty())
                c.textRight (value, 221, 54 + 17 * row, pal::light);
        };
        for (auto& b : paramBars())
        {
            const int row = (b.r.getY() - 9 - 54) / 17;
            static const char* mid[3][3] = { { "TIME", "", "" }, { "ATT", "REL", "GAIN" }, { "", "", "" } };
            label (row, mid[mode][row], px::paramText (b.p));
            b.draw (c);
        }
        if (mode == grv::MidiMode)
        {
            const bool loop = param ("env_loop")->getValue() > 0.5f;
            label (1, "LOOP");
            c.frame ({ 211, 54 + 17, 8, 8 }, loop ? pal::yellow : pal::dim);
            if (loop)
                c.fill (213, 54 + 17 + 2, 4, 4, pal::yellow);
        }
        else if (mode == grv::SyncMode)
        {
            label (0, "RATE");
            const int idx = px::choiceIndex (param ("env_rate"));
            c.triangle (160, 68, 0, 3, idx > 0 ? pal::light : pal::dim);
            c.triangle (215, 68, 1, 3, idx < 7 ? pal::light : pal::dim);
            c.textCentre (grv::kRateNames[idx], 188, 65, pal::yellow);
        }

        // indicatori live
        label (3, "VAL", juce::String (juce::roundToInt (val * 100.f)) + "%");
        PixelBar::make (nullptr, 156, 54 + 17 * 3 + 9, 65, 5, 16, juce::Colours::white).draw (c, val);
        label (4, mode == grv::FollowerMode ? "IN" : "POS", juce::String (juce::roundToInt (ph * 100.f)) + "%");
        PixelBar::make (nullptr, 156, 54 + 17 * 4 + 9, 65, 5, 16, pal::mid).draw (c, ph);
    }

    void drawScope (px::Canvas& c, juce::Rectangle<int> r)
    {
        using namespace px;
        c.panel (r, juce::Colour (0xff0b0b16), pal::grid);
        const auto in = r.reduced (2);
        const int cy = in.getCentreY();
        c.dottedH (in.getX(), cy, in.getWidth(), pal::grid);

        float buf[1024];
        proc.readScope (buf, 1024);

        int start = 0;
        for (int i = 1; i < 512; ++i)
            if (buf[i - 1] <= 0.f && buf[i] > 0.f)
            {
                start = i;
                break;
            }

        constexpr int span = 480;
        const int cols = in.getWidth();
        const float amp = (float) (in.getHeight() / 2 - 1);
        for (int col = 0; col < cols; ++col)
        {
            const int s0 = start + col * span / cols, s1 = start + (col + 1) * span / cols;
            float lo = 1.f, hi = -1.f;
            for (int i = s0; i < juce::jmax (s1, s0 + 1); ++i)
            {
                lo = juce::jmin (lo, buf[i]);
                hi = juce::jmax (hi, buf[i]);
            }
            const int yTop = cy - juce::roundToInt (juce::jlimit (-1.f, 1.f, hi) * amp);
            const int yBot = cy - juce::roundToInt (juce::jlimit (-1.f, 1.f, lo) * amp);
            c.fill (in.getX() + col, juce::jmin (yTop, yBot), 1, std::abs (yBot - yTop) + 1, juce::Colours::white);
        }
    }

    void drawNameView (px::Canvas& c)
    {
        using namespace px;
        c.text ("NAME", 3, 3, pal::yellow);
        c.panel (cancelRect(), pal::panel, pal::grid);
        c.textCentre ("CANCEL", cancelRect().getCentreX(), cancelRect().getY() + 2, pal::mid);

        const juce::Rectangle<int> field (3, 16, 218, 17);
        c.panel (field, juce::Colour (0xff0b0b16), pal::yellow);
        c.text (ui.nameBuf, 8, 21, pal::light);
        if (ui.nameBuf.length() < kMaxName && (juce::Time::getMillisecondCounter() / 400) % 2 == 0)
            c.fill (8 + ui.nameBuf.length() * 6, 21, 5, 7, pal::yellow);

        const auto chars = keyChars();
        for (int i = 0; i < 40; ++i)
        {
            const auto r = keyRect (i);
            const bool ok = i == 39;
            c.panel (r, ok ? pal::yellow : pal::panel, ok ? pal::yellow : pal::grid);
            const int cx = r.getCentreX(), ty = r.getY() + 7;
            if (i < 36)         c.textCentre (chars.substring (i, i + 1), cx, ty, pal::light);
            else if (i == 36)   c.textCentre ("-", cx, ty, pal::light);
            else if (i == 37)   c.textCentre ("SP", cx, ty, pal::mid);
            else if (i == 38)   c.triangle (cx, r.getCentreY(), 0, 4, pal::red);
            else                c.textCentre ("OK", cx, ty, pal::bg);
        }
        c.textCentre ("MAX 16 CHARS - START = OK", 112, 134, pal::dim);
    }

    void drawPresetView (px::Canvas& c)
    {
        using namespace px;
        auto& pm = proc.getPresets();
        c.text ("PRESET", 3, 3, pal::yellow);
        {
            const bool user = pm.category (pm.current()) == 3;
            const bool armed = delArmed();
            c.panel (saveRect(), pal::panel, pal::grid);
            c.textCentre ("SAVE", saveRect().getCentreX(), saveRect().getY() + 2, pal::light);
            if (armed)
            {
                c.fill (delRect(), pal::red);
                c.textCentre ("SURE?", delRect().getCentreX(), delRect().getY() + 2, pal::bg);
            }
            else
            {
                c.panel (delRect(), pal::panel, user ? pal::red : pal::grid);
                c.textCentre ("DEL", delRect().getCentreX(), delRect().getY() + 2, user ? pal::red : pal::dim);
            }
        }

        const auto list = listRect();
        c.panel (list.expanded (0, 0), juce::Colour (0xff0b0b16), pal::grid);

        const int first = firstVisible();
        for (int i = 0; i < kVisibleRows; ++i)
        {
            const int idx = first + i;
            if (idx >= pm.size())
                break;
            const int y = list.getY() + i * kRowH;
            const bool sel = idx == pm.current();
            if (sel)
                c.fill (list.getX() + 1, y, list.getWidth() - 2, kRowH, pal::yellow);
            const auto ink = sel ? pal::bg : pal::light;
            c.text (juce::String (idx + 1).paddedLeft ('0', 2), 7, y + 2, sel ? pal::bg : pal::dim);
            c.text (pm.name (idx), 24, y + 2, ink);
            const int cat = pm.category (idx);
            if (cat != 0)
            {
                const auto tag = cat == 1 ? pal::mid : (cat == 2 ? pal::red : pal::cyan);
                c.textRight (PresetManager::categoryName (cat), 208, y + 2, sel ? pal::bg : tag);
            }
        }

        // barra di scorrimento
        const int n = juce::jmax (1, pm.size());
        const int trackH = list.getHeight();
        c.fill (217, list.getY(), 3, trackH, pal::grid);
        const int thumbH = juce::jmax (6, trackH * juce::jmin (kVisibleRows, n) / n);
        const int thumbY = list.getY() + (n > kVisibleRows ? (trackH - thumbH) * first / (n - kVisibleRows) : 0);
        c.fill (217, thumbY, 3, thumbH, pal::mid);

        // sezione SKIN: colore della scocca
        c.text ("SKIN", 3, 128, pal::yellow);
        c.dottedH (30, 131, 160, pal::grid);
        c.textRight ("V1.2", 221, 128, pal::dim);          // numero di versione: serve a capire quale compilazione si sta usando
        const int cur = proc.getSkin();
        for (int i = 0; i < kNumShells; ++i)
        {
            const auto r = skinRect (i);
            const auto& sh = shellFor (i);
            const bool sel = i == cur;
            c.panel (r, juce::Colour (0xff0b0b16), sel ? pal::yellow : pal::grid);
            if (sel)
                c.frame (r.reduced (1), pal::yellow);

            // mini scocca: piastra del colore della skin con la riga di luce
            const auto sw = juce::Rectangle<int> (r.getX() + 5, r.getY() + 4, r.getWidth() - 10, 8);
            c.fill (sw.getX(), sw.getY(), sw.getWidth(), sw.getHeight(), juce::Colour (sh.swatch));
            c.fill (sw.getX(), sw.getY(), sw.getWidth(), 2, juce::Colour (sh.top));
            c.fill (sw.getX(), sw.getBottom() - 2, sw.getWidth(), 2, juce::Colour (sh.bottom));
            if (sh.limited)
            {
                c.fill (sw.getX() + 3, sw.getY() + 2, 10, 1, juce::Colour (0xffe8283a));
                c.fill (sw.getX() + 14, sw.getY() + 4, 14, 1, juce::Colour (0xffe8283a));
            }
            c.frame (sw, pal::mid);
            c.textCentre (sh.name, r.getCentreX(), r.getY() + 13, sel ? pal::yellow : pal::light);
        }
    }

    //==========================================================================
    GroovyRackProcessor& proc;
    UiState& ui;
    std::vector<Point> pts;
    std::vector<px::PixelBar> dragBars;
    int dragBarIndex = -1, dragPoint = -1, dragSegment = -1;
};
} // namespace grv
