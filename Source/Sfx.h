#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <array>
#include <atomic>

// Suoni dell'easter egg (entrata, sigla, salto, morte). I file sono incorporati nel plugin (Resources/sfx) e vengono
// MISCELATI nell'uscita audio del plugin, quindi escono dalla DAW come qualsiasi altro suono, senza aprire altre schede audio.
namespace grv
{
class Sfx
{
public:
    enum Id { Intro = 0, Theme, Jump, Death, numIds };
    enum Cmd { StopAll = 0, PlayIntroThenTheme, PlayJump, PlayDeath };

    Sfx() : fifo (64) {}

    // decodifica i file (thread dell'interfaccia). Si può chiamare più volte: lo fa una volta sola.
    void load (const void* const* data, const int* sizes)
    {
        if (loaded.load (std::memory_order_acquire))
            return;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        for (int i = 0; i < numIds; ++i)
        {
            auto stream = std::make_unique<juce::MemoryInputStream> (data[i], (size_t) sizes[i], false);
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (std::move (stream)));
            if (reader == nullptr || reader->lengthInSamples <= 0)
                continue;
            auto& b = bufs[(size_t) i];
            b.setSize (2, (int) reader->lengthInSamples);
            reader->read (&b, 0, (int) reader->lengthInSamples, 0, true, true);
            if (reader->numChannels < 2)
                b.copyFrom (1, 0, b, 0, 0, b.getNumSamples());
            srcRate[(size_t) i] = reader->sampleRate;
        }
        loaded.store (true, std::memory_order_release);
    }
    bool isMusicPaused() const { return musicPaused.load (std::memory_order_relaxed); }   // per le prove
    bool isLoaded() const { return loaded.load (std::memory_order_acquire); }

    void prepare (double sampleRate) { hostRate.store (sampleRate > 0 ? sampleRate : 44100.0); }

    // dal thread dell'interfaccia (o da qualsiasi thread): coda di comandi letta dal thread audio
    void request (Cmd c)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 > 0) { cmds[(size_t) start1] = (int) c; fifo.finishedWrite (1); }
    }

    // thread audio: somma i suoni attivi nel buffer
    void mix (juce::AudioBuffer<float>& out)
    {
        // comandi in arrivo
        while (fifo.getNumReady() > 0)
        {
            int start1, size1, start2, size2;
            fifo.prepareToRead (1, start1, size1, start2, size2);
            if (size1 <= 0) break;
            const int c = cmds[(size_t) start1];
            fifo.finishedRead (1);
            if (! loaded.load (std::memory_order_acquire))
                continue;
            switch (c)
            {
                case StopAll:            for (auto& v : voices) v.active = false; break;
                case PlayIntroThenTheme: start (Intro, Theme); break;
                case PlayJump:           start (Jump, -1); break;
                case PlayDeath:          start (Death, -1); pauseMusic (1.0); break;   // la musica tace 1 secondo per far sentire la morte
                default: break;
            }
        }

        const int n = out.getNumSamples(), nch = out.getNumChannels();
        if (nch < 1)
            return;
        const double host = hostRate.load();
        for (auto& v : voices)
        {
            if (! v.active)
                continue;
            const float fadeDown = (float) (1.0 / (host * 0.02)), fadeUp = (float) (1.0 / (host * 0.04));
            for (int i = 0; i < n && v.active; ++i)
            {
                const bool paused = v.pauseLeft > 0.0;
                if (paused)
                {
                    v.pauseLeft -= 1.0;
                    v.gain = std::max (0.f, v.gain - fadeDown);          // dissolvenza rapida: niente click
                    if (v.gain <= 0.f)
                        continue;                                        // in pausa: la posizione resta ferma
                }
                else if (v.gain < 1.f)
                    v.gain = std::min (1.f, v.gain + fadeUp);            // la musica riparte con una breve dissolvenza

                const auto& b = bufs[(size_t) v.id];
                const int len = b.getNumSamples();
                const int p = (int) v.pos;
                const float f = (float) (v.pos - (double) p);
                const int p2 = p + 1 < len ? p + 1 : (v.id == Theme ? 0 : p);   // la sigla è in loop: l'ultimo campione si lega al primo
                const float l = (b.getSample (0, p) * (1.f - f) + b.getSample (0, p2) * f) * v.gain;
                const float r = (b.getSample (1, p) * (1.f - f) + b.getSample (1, p2) * f) * v.gain;
                if (nch == 1)
                    out.getWritePointer (0)[i] += 0.5f * (l + r);
                else
                {
                    out.getWritePointer (0)[i] += l;
                    out.getWritePointer (1)[i] += r;
                }
                v.pos += srcRate[(size_t) v.id] / host;
                if (v.pos >= (double) len)
                {
                    if (v.id == Theme)
                        v.pos -= (double) len;                  // sigla: continua finché non si chiude il guscio
                    else if (v.next >= 0)
                    {
                        v.id = v.next;                          // finita l'entrata: parte la sigla
                        v.next = -1;
                        v.pos = 0.0;
                    }
                    else
                        v.active = false;
                }
            }
        }
        musicPaused.store (std::any_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.active && v.pauseLeft > 0.0; }),
                           std::memory_order_relaxed);
        // rete di sicurezza contro i picchi
        for (int ch = 0; ch < nch; ++ch)
        {
            auto* d = out.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                d[i] = juce::jlimit (-1.f, 1.f, d[i]);
        }
    }

private:
    struct Voice { bool active = false; int id = 0, next = -1; double pos = 0.0; float gain = 1.f; double pauseLeft = 0.0; };

    // la musica (sigla, o entrata che sta per diventare sigla) si ferma per 'seconds' secondi e poi riparte da dove era
    void pauseMusic (double seconds)
    {
        for (auto& v : voices)
            if (v.active && (v.id == Theme || v.next == Theme))
                v.pauseLeft = seconds * hostRate.load();
    }

    void start (int id, int next)
    {
        if (bufs[(size_t) id].getNumSamples() <= 0)
            return;
        if (id == Intro)
            for (auto& v : voices) v.active = false;            // una nuova entrata riparte da zero
        Voice* slot = nullptr;
        for (auto& v : voices)
            if (! v.active) { slot = &v; break; }
        if (slot == nullptr)
            slot = &voices[0];
        *slot = { true, id, next, 0.0, 1.f, 0.0 };
    }

    std::array<juce::AudioBuffer<float>, numIds> bufs;
    std::array<double, numIds> srcRate { 44100.0, 44100.0, 44100.0, 44100.0 };
    std::atomic<bool> loaded { false };
    std::atomic<double> hostRate { 44100.0 };
    std::atomic<bool> musicPaused { false };
    juce::AbstractFifo fifo;
    std::array<int, 64> cmds {};
    std::array<Voice, 8> voices;
};
} // namespace grv
