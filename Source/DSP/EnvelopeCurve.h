#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// Curva disegnabile a mano (stile Serum): punti + curvatura per ogni segmento.
//  - Il thread grafico modifica i punti (protetti da lock) e "cuoce" una tabella di 512 valori.
//  - Il thread audio legge SOLO la tabella (atomici), quindi nessun lock in audio.

namespace grv
{
class EnvelopeCurve
{
public:
    struct Point
    {
        float x = 0.f, y = 0.f;
        float curve = 0.f; // -1..1, curvatura del segmento che parte da questo punto
    };

    static constexpr int kTableSize = 512;

    EnvelopeCurve() { resetDefault(); }

    void resetDefault()
    {
        setPoints ({ { 0.f, 1.f, -0.35f }, { 1.f, 0.f, 0.f } });
    }

    //==========================================================================
    // Thread messaggi
    std::vector<Point> getPoints() const
    {
        const juce::ScopedLock sl (lock);
        return points;
    }

    void setPoints (std::vector<Point> newPoints)
    {
        if (newPoints.size() < 2)
            newPoints = { { 0.f, 1.f, -0.35f }, { 1.f, 0.f, 0.f } };

        std::stable_sort (newPoints.begin(), newPoints.end(),
                          [] (const Point& a, const Point& b) { return a.x < b.x; });

        for (auto& p : newPoints)
        {
            p.x = juce::jlimit (0.f, 1.f, p.x);
            p.y = juce::jlimit (0.f, 1.f, p.y);
            p.curve = juce::jlimit (-1.f, 1.f, p.curve);
        }
        newPoints.front().x = 0.f;
        newPoints.back().x = 1.f;

        {
            const juce::ScopedLock sl (lock);
            points = std::move (newPoints);
            bake();
        }
    }

    juce::String toString() const
    {
        juce::StringArray parts;
        for (auto& p : getPoints())
            parts.add (juce::String (p.x, 5) + "," + juce::String (p.y, 5) + "," + juce::String (p.curve, 5));
        return parts.joinIntoString (";");
    }

    void fromString (const juce::String& s)
    {
        std::vector<Point> pts;
        for (auto& part : juce::StringArray::fromTokens (s, ";", ""))
        {
            auto v = juce::StringArray::fromTokens (part, ",", "");
            if (v.size() == 3)
                pts.push_back ({ v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue() });
        }
        if (pts.size() >= 2)
            setPoints (std::move (pts));
    }

    static float shape (float t, float c)
    {
        if (std::abs (c) < 1e-4f)
            return t;
        return std::pow (t, std::pow (8.f, c));
    }

    //==========================================================================
    // Thread audio (senza lock): x in 0..1 -> valore 0..1
    float eval (float x) const noexcept
    {
        const float pos = juce::jlimit (0.f, 1.f, x) * (float) (kTableSize - 1);
        const int i = (int) pos;
        const int j = std::min (i + 1, kTableSize - 1);
        const float f = pos - (float) i;
        return table[(size_t) i].load (std::memory_order_relaxed) * (1.f - f)
             + table[(size_t) j].load (std::memory_order_relaxed) * f;
    }

private:
    // da chiamare con il lock preso
    float evalPoints (float x) const
    {
        const auto n = points.size();
        if (x <= points.front().x)
            return points.front().y;

        for (size_t i = 0; i + 1 < n; ++i)
        {
            const auto& a = points[i];
            const auto& b = points[i + 1];
            if (x < b.x || i + 2 == n)
            {
                if (b.x - a.x <= 1e-6f)
                    return b.y;
                const float t = juce::jlimit (0.f, 1.f, (x - a.x) / (b.x - a.x));
                return a.y + (b.y - a.y) * shape (t, a.curve);
            }
        }
        return points.back().y;
    }

    void bake()
    {
        for (int i = 0; i < kTableSize; ++i)
            table[(size_t) i].store (evalPoints ((float) i / (float) (kTableSize - 1)), std::memory_order_relaxed);
    }

    mutable juce::CriticalSection lock;
    std::vector<Point> points;
    std::array<std::atomic<float>, kTableSize> table;
};
} // namespace grv
