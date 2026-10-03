#pragma once

#include <juce_graphics/juce_graphics.h>

namespace grv
{
// Colori della scocca. Tre skin: STEEL (acciaio blu-grigio, quella originale), BLACK (nero laccato), WHITE (bianco perla).
struct Shell
{
    const char* name;
    juce::uint32 top, s06, s22, s44, horizon, s505, s70, s90, bottom;   // gradiente del corpo
    juce::uint32 bevelLight, bevelDark;                                 // smusso del bordo
    juce::uint32 hinge[5];                                              // cilindro della cerniera (bordo, 25%, 40%, 60%, 85%)
    juce::uint32 engraveLight, engraveDark;                             // scritte incise: luce sul bordo basso / fondo scuro
    float engraveLightAlpha, engraveDarkAlpha;
    float glossAlpha;                                                   // intensità del riflesso della vernice
    juce::uint32 swatch;                                                // colore dell'anteprima nel selettore
    bool limited;                                                       // true = skin CIRCUIT: scheda elettronica sotto plastica trasparente
};

// GROOVY_LIMITED_EDITION = 1 solo nella versione privata: aggiunge la skin CIRCUIT (4 skin).
// La versione pubblica ne ha 3 e il codice della skin CIRCUIT non viene nemmeno compilato.
#ifndef GROOVY_LIMITED_EDITION
 #define GROOVY_LIMITED_EDITION 0
#endif
constexpr int kNumShells = GROOVY_LIMITED_EDITION ? 4 : 3;

inline const Shell& shellFor (int index)
{
    static const Shell shells[kNumShells] = {
        { "STEEL",
          0xffb3b9dc, 0xff8f96be, 0xff5c6288, 0xff2b2e45, 0xff141625, 0xff4a4f74, 0xff5a6088, 0xff353952, 0xff272a3d,
          0xffc4c9e6, 0xff07080e,
          { 0xff1b1d2a, 0xff8a90b3, 0xffc7cce8, 0xff595e7e, 0xff2b2d40 },
          0xffffffff, 0xff12131c, 0.18f, 1.0f, 0.30f, 0xff7a82b0, false },

        { "BLACK",
          0xff5b5c68, 0xff43444f, 0xff2a2b33, 0xff17181d, 0xff050506, 0xff2f3038, 0xff26272e, 0xff15161a, 0xff0d0e11,
          0xff9a9cab, 0xff000000,
          { 0xff08080a, 0xff4c4d57, 0xff8e909e, 0xff2c2d34, 0xff121316 },
          0xffffffff, 0xff000000, 0.26f, 0.85f, 0.42f, 0xff1c1c22, false },

        { "WHITE",
          0xfffbfcff, 0xffeef0f8, 0xffd6d9e6, 0xffbdc0d0, 0xff8f92a6, 0xffcdd0df, 0xffdcdfeb, 0xffc2c5d4, 0xffa7aabb,
          0xffffffff, 0xff6a6d82,
          { 0xff7e8195, 0xffeceef7, 0xffffffff, 0xffb1b4c6, 0xff7e8195 },
          0xffffffff, 0xff5b5e74, 0.85f, 0.75f, 0.20f, 0xfff0f1f8, false },

    };
    return shells[juce::jlimit (0, kNumShells - 1, index)];
}
} // namespace grv
