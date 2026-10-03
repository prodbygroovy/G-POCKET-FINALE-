#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PixelCanvas.h"

namespace grv::px
{
// Barra a segmenti (stile "barra della vita") collegata a un parametro
struct PixelBar
{
    juce::Rectangle<int> r;
    juce::RangedAudioParameter* p = nullptr;
    bool bipolar = false;
    int segs = 20;
    juce::Colour col = pal::light;

    static PixelBar make (juce::RangedAudioParameter* param, int x, int y, int w, int h, int segments,
                          juce::Colour colour, bool isBipolar = false)
    {
        PixelBar b;
        b.p = param;
        b.segs = segments;
        b.col = colour;
        b.bipolar = isBipolar;
        const int segW = std::max (1, (w + 1) / segments - 1);
        b.r = { x, y, segments * (segW + 1) - 1, h };
        return b;
    }

    bool hit (juce::Point<float> v) const { return r.expanded (2, 3).toFloat().contains (v); }

    float normAt (float vx) const
    {
        float n = juce::jlimit (0.f, 1.f, (vx - (float) r.getX()) / (float) r.getWidth());
        if (bipolar && std::abs (n - 0.5f) < 0.03f)
            n = 0.5f; // aggancio allo zero
        return n;
    }

    void draw (Canvas& c, float overrideNorm = -1.f) const
    {
        const float n = overrideNorm >= 0.f ? overrideNorm : (p ? p->getValue() : 0.f);
        const int segW = std::max (1, (r.getWidth() + 1) / segs - 1);

        int litFrom = 0, litTo = -1; // intervallo di segmenti accesi
        if (bipolar)
        {
            const int centre = segs / 2;
            const int count = juce::roundToInt (std::abs (n - 0.5f) * 2.f * (float) centre);
            if (n > 0.5f) { litFrom = centre; litTo = centre + count - 1; }
            else          { litFrom = centre - count; litTo = centre - 1; }
        }
        else
        {
            litTo = juce::roundToInt (n * (float) segs) - 1;
        }

        for (int i = 0; i < segs; ++i)
        {
            const bool lit = i >= litFrom && i <= litTo;
            c.fill (r.getX() + i * (segW + 1), r.getY(), segW, r.getHeight(), lit ? col : pal::grid);
        }
        if (bipolar)
            c.plot (r.getX() + (segs / 2) * (segW + 1) - 1, r.getBottom() + 1, pal::mid);
    }
};

inline void toggleParam (juce::RangedAudioParameter* p)
{
    if (p == nullptr)
        return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.f : 1.f);
    p->endChangeGesture();
}

inline void setChoice (juce::RangedAudioParameter* p, int index)
{
    if (p == nullptr)
        return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    p->endChangeGesture();
}

inline int choiceIndex (juce::RangedAudioParameter* p)
{
    return p ? juce::roundToInt (p->convertFrom0to1 (p->getValue())) : 0;
}

inline juce::String paramText (juce::RangedAudioParameter* p)
{
    return p ? p->getText (p->getValue(), 12).trim() : juce::String();
}
} // namespace grv::px
