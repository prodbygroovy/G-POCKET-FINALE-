#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// I 4 pedali del rack. Ogni pedale lavora su blocchi piccoli (max kChunk campioni)
// e riceve i parametri già "modulati" dall'envelope. Il bypass è gestito dal mix
// (mix = 0 quando il pedale è spento) con smoothing, quindi niente click.

namespace grv
{
constexpr int kChunk = 32;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

using Smoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

//==============================================================================
class Distortion
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto* s : { &drive, &tone, &mix, &level })
            s->reset (sr, 0.004);
        drive.setCurrentAndTargetValue (1.0f);
        tone.setCurrentAndTargetValue (0.5f);
        mix.setCurrentAndTargetValue (0.0f);
        level.setCurrentAndTargetValue (1.0f);
        lp[0] = lp[1] = 0.0f;
    }

    void setParams (float driveDb, float toneHz, float mixAmt, float levelDb)
    {
        drive.setTargetValue (dbToGain (driveDb));
        const float f = juce::jlimit (20.0f, (float) (sampleRate * 0.45), toneHz);
        tone.setTargetValue (1.0f - std::exp (-juce::MathConstants<float>::twoPi * f / (float) sampleRate));
        mix.setTargetValue (mixAmt);
        level.setTargetValue (dbToGain (levelDb));
    }

    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = drive.getNextValue();
            const float a = tone.getNextValue();
            const float m = mix.getNextValue();
            const float lv = level.getNextValue();

            for (int c = 0; c < 2; ++c)
            {
                const float dry = ch[c][i];
                // hard clip leggermente ammorbidito
                float y = juce::jlimit (-1.0f, 1.0f, dry * g * 1.2f);
                lp[c] += a * (y - lp[c]);
                ch[c][i] = dry * (1.0f - m) + lp[c] * 0.7f * lv * m;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    Smoother drive, tone, mix, level;
    float lp[2] {};
};

//==============================================================================
class Saturation
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto* s : { &drive, &warmth, &lpCoef, &mix, &level })
            s->reset (sr, 0.004);
        drive.setCurrentAndTargetValue (1.0f);
        warmth.setCurrentAndTargetValue (0.0f);
        lpCoef.setCurrentAndTargetValue (1.0f);
        mix.setCurrentAndTargetValue (0.0f);
        level.setCurrentAndTargetValue (1.0f);
        lp[0] = lp[1] = 0.0f;
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
    }

    void setParams (float driveDb, float warmthAmt, float mixAmt, float levelDb)
    {
        drive.setTargetValue (dbToGain (driveDb));
        warmth.setTargetValue (warmthAmt);
        // più warmth = più scuro
        const float f = 18000.0f - warmthAmt * 14000.0f;
        lpCoef.setTargetValue (1.0f - std::exp (-juce::MathConstants<float>::twoPi * f / (float) sampleRate));
        mix.setTargetValue (mixAmt);
        level.setTargetValue (dbToGain (levelDb));
    }

    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = drive.getNextValue();
            const float w = warmth.getNextValue();
            const float a = lpCoef.getNextValue();
            const float m = mix.getNextValue();
            const float lv = level.getNextValue();
            const float bias = w * 0.4f;
            const float comp = 1.0f / std::tanh (g);
            const float offset = std::tanh (g * bias);

            for (int c = 0; c < 2; ++c)
            {
                const float dry = ch[c][i];
                // tanh con bias: aggiunge armoniche pari (calore), poi DC blocker
                float y = (std::tanh (g * (dry + bias)) - offset) * comp;
                const float hp = y - dcX[c] + 0.995f * dcY[c];
                dcX[c] = y;
                dcY[c] = hp;
                lp[c] += a * (hp - lp[c]);
                ch[c][i] = dry * (1.0f - m) + lp[c] * lv * m;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    Smoother drive, warmth, lpCoef, mix, level;
    float lp[2] {}, dcX[2] {}, dcY[2] {};
};

//==============================================================================
class Bitcrush
{
public:
    void prepare (double sr)
    {
        mix.reset (sr, 0.004);
        level.reset (sr, 0.004);
        mix.setCurrentAndTargetValue (0.0f);
        level.setCurrentAndTargetValue (1.0f);
        counter[0] = counter[1] = 0.0f;
        held[0] = held[1] = 0.0f;
    }

    void setParams (float bits, float rateReduction, float mixAmt, float levelDb)
    {
        levels = std::pow (2.0f, juce::jmax (1.0f, bits) - 1.0f);
        rate = juce::jmax (1.0f, rateReduction);
        mix.setTargetValue (mixAmt);
        level.setTargetValue (dbToGain (levelDb));
    }

    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float m = mix.getNextValue();
            const float lv = level.getNextValue();

            for (int c = 0; c < 2; ++c)
            {
                const float dry = ch[c][i];
                counter[c] += 1.0f;
                if (counter[c] >= rate)
                {
                    counter[c] -= rate;
                    held[c] = std::round (dry * levels) / levels;
                }
                ch[c][i] = dry * (1.0f - m) + held[c] * lv * m;
            }
        }
    }

private:
    Smoother mix, level;
    float levels = 128.0f, rate = 1.0f;
    float counter[2] {}, held[2] {};
};

//==============================================================================
class ReverbPedal
{
public:
    void prepare (double sr)
    {
        reverb.setSampleRate (sr);
        reverb.reset();
        mix.reset (sr, 0.004);
        mix.setCurrentAndTargetValue (0.0f);
    }

    void setParams (float size, float damp, float width, float mixAmt)
    {
        juce::Reverb::Parameters p;
        p.roomSize = size;
        p.damping = damp;
        p.width = width;
        p.wetLevel = 0.33f; // ~guadagno unitario sul segnale wet
        p.dryLevel = 0.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
        mix.setTargetValue (mixAmt);
    }

    void process (float* l, float* r, int n)
    {
        float wl[kChunk], wr[kChunk];
        juce::FloatVectorOperations::copy (wl, l, n);
        juce::FloatVectorOperations::copy (wr, r, n);
        reverb.processStereo (wl, wr, n);

        for (int i = 0; i < n; ++i)
        {
            const float m = mix.getNextValue();
            l[i] = l[i] * (1.0f - m) + wl[i] * m;
            r[i] = r[i] * (1.0f - m) + wr[i] * m;
        }
    }

private:
    juce::Reverb reverb;
    Smoother mix;
};

} // namespace grv
