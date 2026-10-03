#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace grv
{
// Tasto rotondo (i 4 tasti a destra): sceglie il pedale mostrato sullo schermo basso
class FaceButton : public juce::Component
{
public:
    FaceButton (juce::String letterToShow, juce::Colour c) : letter (std::move (letterToShow)), colour (c) {}

    std::function<void()> onClick;

    void setState (bool isLit, bool isSelected)
    {
        if (lit != isLit || selected != isSelected)
        {
            lit = isLit;
            selected = isSelected;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat().reduced (5.f);
        const bool down = isMouseButtonDown();
        const auto face = down ? b.translated (0.f, 1.5f) : b;

        // selezionato: il tasto si illumina (alone del suo colore, niente contorno bianco)
        if (selected)
            for (int i = 5; i >= 1; --i)
            {
                g.setColour (colour.withAlpha (0.07f * (float) (6 - i)));
                g.fillEllipse (face.expanded ((float) i));
            }
        else
        {
            g.setColour (Colours::black.withAlpha (0.4f));
            g.fillEllipse (b.translated (0.f, 2.f));
        }

        Colour base = lit ? colour : colour.withMultipliedBrightness (0.38f).withMultipliedSaturation (0.7f);
        if (selected)
            base = colour.brighter (0.3f);

        g.setGradientFill (ColourGradient (base.brighter (selected ? 0.6f : 0.35f), face.getX(), face.getY(),
                                           base.darker (selected ? 0.1f : 0.35f), face.getX(), face.getBottom(), false));
        g.fillEllipse (face);

        // riflesso lucido
        g.setColour (Colours::white.withAlpha (selected ? 0.45f : 0.25f));
        g.fillEllipse (face.getX() + face.getWidth() * 0.2f, face.getY() + 2.f, face.getWidth() * 0.6f, face.getHeight() * 0.3f);

        g.setColour (Colours::black.withAlpha (selected ? 0.7f : (lit ? 0.75f : 0.55f)));
        g.setFont (Font (FontOptions (15.f, Font::bold)));
        g.drawText (letter, face, Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        repaint();
        if (contains (e.getPosition()) && onClick)
            onClick();
    }

private:
    juce::String letter;
    juce::Colour colour;
    bool lit = false, selected = false;
};

//==============================================================================
// Due frecce verticali per scorrere i preset: su = precedente, giù = successivo (tenendo premuto ripete)
class PresetPad : public juce::Component, private juce::Timer
{
public:
    std::function<void (int delta)> onStep; // -1 = su (preset precedente), +1 = giù (successivo)

    // Croce direzionale ridotta a un solo asse: un unico pezzo di plastica lucida con due frecce (su / giù) e un incavo al centro
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto full = getLocalBounds().toFloat().reduced (3.f, 1.f);
        const float w = full.getWidth();
        const float corner = w * 0.3f;

        // ombra sul fondo della scocca
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (full.translated (0.f, 3.f), corner);

        // corpo: plastica scura con bordo smussato
        g.setColour (Colour (0xff07080d));
        g.fillRoundedRectangle (full, corner);
        const auto body = full.reduced (1.6f);
        g.setGradientFill (ColourGradient (Colour (0xff4a4e66), body.getX(), body.getY(), Colour (0xff181a26), body.getRight(), body.getBottom(), false));
        g.fillRoundedRectangle (body, corner - 1.f);

        // riflesso lucido sul lato alto-sinistro
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (body.getX() + 3.f, body.getY() + 3.f, body.getWidth() * 0.42f, body.getHeight() - 6.f, corner - 4.f);

        // metà premuta: si scurisce
        if (pressed >= 0)
        {
            const auto half = buttonRect (pressed).reduced (3.f, 1.f);
            Graphics::ScopedSaveState ss (g);
            Path clip;
            clip.addRoundedRectangle (body, corner - 1.f);
            g.reduceClipRegion (clip);
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillRect (half.withHeight (half.getHeight() + (pressed == 0 ? 0.f : 1.f)));
        }

        // incavo circolare al centro (come il centro di una croce)
        const auto mid = full.getCentre();
        g.setColour (Colour (0xff090a10));
        g.fillEllipse (mid.x - 8.f, mid.y - 8.f, 16.f, 16.f);
        g.setGradientFill (ColourGradient (Colour (0xff12131c), mid.x, mid.y - 7.f, Colour (0xff3a3e56), mid.x, mid.y + 7.f, false));
        g.fillEllipse (mid.x - 6.5f, mid.y - 6.5f, 13.f, 13.f);

        // frecce incise
        for (int i = 0; i < 2; ++i)
        {
            const bool down = pressed == i;
            const auto r = buttonRect (i);
            const float cy = i == 0 ? r.getY() + r.getHeight() * 0.42f : r.getBottom() - r.getHeight() * 0.42f;
            const float cx = full.getCentreX(), s = 8.5f + (down ? -0.5f : 0.f), dy = down ? 1.f : 0.f;
            Path tri;
            if (i == 0) tri.addTriangle (cx, cy - s * 0.75f + dy, cx - s, cy + s * 0.55f + dy, cx + s, cy + s * 0.55f + dy);
            else        tri.addTriangle (cx, cy + s * 0.75f + dy, cx - s, cy - s * 0.55f + dy, cx + s, cy - s * 0.55f + dy);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.fillPath (tri, AffineTransform::translation (0.f, -1.f));
            g.setColour (down ? Colour (0xffffe066) : Colour (0xffb4b9d6));
            g.fillPath (tri);
        }

        // filo chiaro sul bordo superiore
        g.setColour (Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (body.reduced (0.5f), corner - 1.f, 0.8f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        pressed = e.position.y < (float) getHeight() * 0.5f ? 0 : 1;
        fire();
        startTimer (380);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        stopTimer();
        pressed = -1;
        repaint();
    }

private:
    juce::Rectangle<float> buttonRect (int i) const
    {
        const float h = (float) getHeight() * 0.5f;
        return { 0.f, h * (float) i, (float) getWidth(), h };
    }
    void fire() { if (onStep && pressed >= 0) onStep (pressed == 0 ? -1 : 1); }
    void timerCallback() override
    {
        fire();
        startTimer (90);
    }

    int pressed = -1;
};

//==============================================================================
// Tasti piccoli (SELECT / START)
class PillButton : public juce::Component
{
public:
    explicit PillButton (juce::String text) : label (std::move (text)) {}
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat().reduced (1.f);
        const bool down = isMouseButtonDown();
        g.setGradientFill (ColourGradient (Colour (down ? 0xff20222e : 0xff40445a), b.getX(), b.getY(),
                                           Colour (0xff181a24), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, b.getHeight() / 2.f);
        g.setColour (Colour (0xff6b7090));
        g.drawRoundedRectangle (b, b.getHeight() / 2.f, 1.f);
        g.setColour (Colour (0xffc9cee8));
        g.setFont (Font (FontOptions (8.f, Font::bold)));
        g.drawText (label, getLocalBounds(), Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        repaint();
        if (contains (e.getPosition()) && onClick)
            onClick();
    }

private:
    juce::String label;
};
} // namespace grv
