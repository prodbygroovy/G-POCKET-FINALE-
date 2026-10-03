#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// Definizione centrale di pedali e parametri: la usano sia il processor che l'interfaccia.

namespace grv
{
struct ParamDef
{
    const char* suffix;
    const char* label;
    float min, max, def, skew, interval;
    const char* unit; // "%" = valore 0..1 mostrato in percentuale
};

struct PedalDef
{
    const char* prefix;
    const char* name;
    ParamDef p[4];
};

inline const PedalDef* pedalDefs()
{
    static const PedalDef d[4] = {
        { "dist", "DISTORTION",
          { { "drive", "Drive", 0.f, 40.f, 12.f, 1.f, 0.01f, " dB" },
            { "tone",  "Tone",  500.f, 16000.f, 6000.f, 0.4f, 1.f, " Hz" },
            { "mix",   "Mix",   0.f, 1.f, 1.f, 1.f, 0.001f, "%" },
            { "level", "Level", -24.f, 12.f, 0.f, 1.f, 0.01f, " dB" } } },

        { "sat", "SATURATION",
          { { "drive", "Drive",  0.f, 30.f, 8.f, 1.f, 0.01f, " dB" },
            { "warm",  "Warmth", 0.f, 1.f, 0.3f, 1.f, 0.001f, "%" },
            { "mix",   "Mix",    0.f, 1.f, 1.f, 1.f, 0.001f, "%" },
            { "level", "Level",  -24.f, 12.f, 0.f, 1.f, 0.01f, " dB" } } },

        { "crush", "BITCRUSH",
          { { "bits", "Bits", 1.f, 16.f, 8.f, 1.f, 0.1f, " bit" },
            { "rate", "Rate", 1.f, 64.f, 4.f, 0.5f, 0.1f, " x" },
            { "mix",  "Mix",  0.f, 1.f, 1.f, 1.f, 0.001f, "%" },
            { "level", "Level", -24.f, 12.f, 0.f, 1.f, 0.01f, " dB" } } },

        { "verb", "REVERB",
          { { "size",  "Size",  0.f, 1.f, 0.5f, 1.f, 0.001f, "%" },
            { "damp",  "Damp",  0.f, 1.f, 0.5f, 1.f, 0.001f, "%" },
            { "width", "Width", 0.f, 1.f, 1.f, 1.f, 0.001f, "%" },
            { "mix",   "Mix",   0.f, 1.f, 0.3f, 1.f, 0.001f, "%" } } },
    };
    return d;
}

inline juce::String pid (int pedal, int k)
{
    return juce::String (pedalDefs()[pedal].prefix) + "_" + pedalDefs()[pedal].p[k].suffix;
}

inline juce::String modId (int pedal, int k) { return pid (pedal, k) + "_mod"; }
inline juce::String onId (int pedal)          { return juce::String (pedalDefs()[pedal].prefix) + "_on"; }

// Indice del parametro "mix" (quello che viene azzerato dal bypass) per ogni pedale
inline int mixIndex (int pedal) { return pedal == 3 ? 3 : 2; }

inline juce::String formatValue (float v, const juce::String& unit)
{
    if (unit == "%")
        return juce::String (juce::roundToInt (v * 100.f)) + " %";

    const float a = std::abs (v);
    return juce::String (v, a < 10.f ? 2 : (a < 100.f ? 1 : 0)) + unit;
}

// Envelope
// Rate dell'envelope in sync: valori normali e terzine ("T"), in ordine di durata.
inline constexpr int kNumRates = 13;
inline constexpr int kDefaultRate = 6;   // 1/4
inline const char* const kRateNames[kNumRates] = { "1/32", "1/32T", "1/16", "1/16T", "1/8", "1/8T", "1/4", "1/4T",
                                                   "1/2", "1/2T", "1 bar", "2 bars", "4 bars" };
inline constexpr double kRateBeats[kNumRates]  = { 0.125, 0.125 * 2.0 / 3.0, 0.25, 0.25 * 2.0 / 3.0, 0.5, 0.5 * 2.0 / 3.0,
                                                   1.0, 2.0 / 3.0, 2.0, 4.0 / 3.0, 4.0, 8.0, 16.0 };

enum EnvMode { MidiMode = 0, FollowerMode = 1, SyncMode = 2 };
} // namespace grv
