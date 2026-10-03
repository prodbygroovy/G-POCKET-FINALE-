#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/Shell.h"
#include "GrvSfxData.h"

using namespace grv;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GroovyRackProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto* defs = pedalDefs();

    for (int p = 0; p < 4; ++p)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { onId (p), 1 },
                                                          String (defs[p].name) + " On", false));
        for (int k = 0; k < 4; ++k)
        {
            const auto& d = defs[p].p[k];
            const String unit (d.unit);

            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { pid (p, k), 1 }, String (defs[p].name) + " " + d.label,
                NormalisableRange<float> (d.min, d.max, d.interval, d.skew), d.def,
                AudioParameterFloatAttributes().withStringFromValueFunction (
                    [unit] (float v, int) { return formatValue (v, unit); })));

            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { modId (p, k), 1 }, String (defs[p].name) + " " + d.label + " Env",
                NormalisableRange<float> (-1.f, 1.f, 0.001f), 0.f,
                AudioParameterFloatAttributes().withStringFromValueFunction (
                    [] (float v, int) { return String (roundToInt (v * 100.f)) + " %"; })));
        }
    }

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "order", 1 }, "Pedal Order", 0, 23, 0));

    // Envelope
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "env_on", 1 }, "Env On", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "env_mode", 1 }, "Env Mode",
                                                        StringArray { "MIDI", "Follower", "Sync" }, 0));
    StringArray rates;
    for (auto* r : kRateNames)
        rates.add (r);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "env_rate", 1 }, "Env Sync Rate", rates, kDefaultRate));

    auto ms = [] (float v, int) { return String (v, v < 10.f ? 1 : 0) + " ms"; };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "env_time", 1 }, "Env Time",
        NormalisableRange<float> (10.f, 5000.f, 1.f, 0.4f), 500.f,
        AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "env_loop", 1 }, "Env Loop", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "env_attack", 1 }, "Env Attack",
        NormalisableRange<float> (0.1f, 200.f, 0.1f, 0.4f), 5.f,
        AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "env_release", 1 }, "Env Release",
        NormalisableRange<float> (5.f, 2000.f, 1.f, 0.4f), 150.f,
        AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "env_gain", 1 }, "Env Follower Gain",
        NormalisableRange<float> (-24.f, 24.f, 0.1f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (v, 1) + " dB"; })));

    return layout;
}

//==============================================================================
GroovyRackProcessor::GroovyRackProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GroovyRack", createLayout())
{
    for (int p = 0; p < 4; ++p)
    {
        on[p] = apvts.getRawParameterValue (onId (p));
        for (int k = 0; k < 4; ++k)
        {
            raw[p][k] = apvts.getRawParameterValue (pid (p, k));
            mod[p][k] = apvts.getRawParameterValue (modId (p, k));
            ranges[p][k] = apvts.getParameterRange (pid (p, k));
        }
    }
    {
        // skin usata l'ultima volta (per i progetti nuovi; quelli salvati ricordano la propria)
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("Groovy").getChildFile ("G-POCKET").getChildFile ("skin.txt");
        if (f.existsAsFile())
            skinIndex.store (juce::jlimit (0, grv::kNumShells - 1, f.loadFileAsString().getIntValue()));
    }
    envOn      = apvts.getRawParameterValue ("env_on");
    envMode    = apvts.getRawParameterValue ("env_mode");
    envRate    = apvts.getRawParameterValue ("env_rate");
    envTime    = apvts.getRawParameterValue ("env_time");
    envLoop    = apvts.getRawParameterValue ("env_loop");
    envAttack  = apvts.getRawParameterValue ("env_attack");
    envRelease = apvts.getRawParameterValue ("env_release");
    envGain    = apvts.getRawParameterValue ("env_gain");
    orderParam = apvts.getRawParameterValue ("order");

    for (auto& v : scopeBuf)
        v.store (0.f, std::memory_order_relaxed);

    storeCurveInState();
    presets = std::make_unique<PresetManager> (*this);
    apvts.state.setProperty ("presetName", presets->name (0), nullptr);
}

GroovyRackProcessor::~GroovyRackProcessor() = default;

int GroovyRackProcessor::getNumPrograms() { return juce::jmax (1, presets->numFactory()); }
int GroovyRackProcessor::getCurrentProgram() { return presets->isFactory (presets->current()) ? presets->current() : 0; }
void GroovyRackProcessor::setCurrentProgram (int index) { presets->load (index); }
const juce::String GroovyRackProcessor::getProgramName (int index) { return presets->name (index); }

void GroovyRackProcessor::readScope (float* dest, int n) const
{
    n = juce::jmin (n, kScopeSize);
    const int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
        dest[i] = scopeBuf[(size_t) ((w - n + i) & (kScopeSize - 1))].load (std::memory_order_relaxed);
}

void GroovyRackProcessor::storeCurveInState()
{
    apvts.state.getOrCreateChildWithName ("ENV", nullptr).setProperty ("points", curve.toString(), nullptr);
}

bool GroovyRackProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GroovyRackProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    sfx.prepare (sr);
    dist.prepare (sr);
    sat.prepare (sr);
    crush.prepare (sr);
    verb.prepare (sr);

    envPhase = 1.0;
    envRunning = false;
    follLevel = 0.f;
    freePpq = 0.0;
}

//==============================================================================
float GroovyRackProcessor::computeEnvelope (const juce::AudioBuffer<float>& buffer, int pos, int len,
                                            double blockPpq, bool hasPpq, bool triggered)
{
    const int mode = (int) envMode->load();
    float value = 0.f;
    float phaseOut = 0.f;

    if (mode == MidiMode)
    {
        if (triggered)
        {
            envPhase = 0.0;
            envRunning = true;
        }
        if (envRunning)
        {
            const double seconds = juce::jmax (0.001, (double) envTime->load() * 0.001);
            envPhase += (double) len / (sampleRate * seconds);
            if (envPhase >= 1.0)
            {
                if (envLoop->load() > 0.5f)
                    envPhase -= std::floor (envPhase);
                else
                {
                    envPhase = 1.0;
                    envRunning = false;
                }
            }
        }
        phaseOut = (float) envPhase;
        value = curve.eval (phaseOut);
    }
    else if (mode == FollowerMode)
    {
        // livello d'ingresso (prima dei pedali) -> smoothing attack/release -> curva come "transfer"
        float peak = 0.f;
        for (int ch = 0; ch < 2; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, pos, len));

        const float db = 20.f * std::log10 (peak + 1.0e-6f) + envGain->load();
        const float target = juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f);

        const float ms = target > follLevel ? envAttack->load() : envRelease->load();
        const float coef = std::exp (-(float) len / (float) (sampleRate * (double) ms * 0.001));
        follLevel = target + coef * (follLevel - target);

        phaseOut = follLevel;
        value = curve.eval (follLevel);
    }
    else // Sync
    {
        const double beatsPerSample = lastBpm / 60.0 / sampleRate;
        double ppq;
        if (hasPpq)
            ppq = blockPpq + (double) pos * beatsPerSample;
        else
        {
            ppq = freePpq;
            freePpq += (double) len * beatsPerSample;
        }

        const double rateBeats = kRateBeats[juce::jlimit (0, kNumRates - 1, (int) envRate->load())];
        double ph = std::fmod (ppq / rateBeats, 1.0);
        if (ph < 0.0)
            ph += 1.0;

        phaseOut = (float) ph;
        value = curve.eval (phaseOut);
    }

    uiPhase.store (phaseOut, std::memory_order_relaxed);
    uiValue.store (value, std::memory_order_relaxed);
    return value;
}

void GroovyRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
        return;

    // Trasporto della DAW (per la modalità Sync)
    double blockPpq = 0.0;
    bool hasPpq = false;
    if (auto* ph = getPlayHead())
    {
        if (auto position = ph->getPosition())
        {
            if (auto bpm = position->getBpm())
                lastBpm = juce::jmax (20.0, *bpm);
            if (auto ppq = position->getPpqPosition(); ppq && position->getIsPlaying())
            {
                blockPpq = *ppq;
                hasPpq = true;
            }
        }
    }

    const auto order = decodeOrder ((int) orderParam->load());
    const bool envActive = envOn->load() > 0.5f;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);

    auto it = midi.begin();
    const auto midiEnd = midi.end();

    for (int pos = 0; pos < numSamples; pos += kChunk)
    {
        const int len = std::min (kChunk, numSamples - pos);

        bool triggered = false;
        while (it != midiEnd && (*it).samplePosition < pos + len)
        {
            if ((*it).getMessage().isNoteOn())
                triggered = true;
            ++it;
        }

        const float env = computeEnvelope (buffer, pos, len, blockPpq, hasPpq, triggered);

        // parametri (con modulazione dell'envelope) -> plain values
        float v[4][4];
        for (int p = 0; p < 4; ++p)
            for (int k = 0; k < 4; ++k)
            {
                float value = raw[p][k]->load();
                const float depth = mod[p][k]->load();
                if (envActive && depth != 0.f)
                {
                    const float norm = ranges[p][k].convertTo0to1 (value);
                    value = ranges[p][k].convertFrom0to1 (juce::jlimit (0.f, 1.f, norm + depth * env));
                }
                v[p][k] = value;
            }

        // bypass = mix a zero (con smoothing, senza click)
        auto active = [this] (int p) { return on[p]->load() > 0.5f; };
        dist.setParams  (v[0][0], v[0][1], active (0) ? v[0][2] : 0.f, v[0][3]);
        sat.setParams   (v[1][0], v[1][1], active (1) ? v[1][2] : 0.f, v[1][3]);
        crush.setParams (v[2][0], v[2][1], active (2) ? v[2][2] : 0.f, v[2][3]);
        verb.setParams  (v[3][0], v[3][1], v[3][2], active (3) ? v[3][3] : 0.f);

        for (int slot = 0; slot < 4; ++slot)
        {
            switch (order[(size_t) slot])
            {
                case 0: dist.process  (l + pos, r + pos, len); break;
                case 1: sat.process   (l + pos, r + pos, len); break;
                case 2: crush.process (l + pos, r + pos, len); break;
                case 3: verb.process  (l + pos, r + pos, len); break;
                default: break;
            }
        }
    }

    sfx.mix (buffer);                                   // suoni dell'easter egg (di solito silenzio)

    // oscilloscopio: mono dell'uscita
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
        scopeBuf[(size_t) ((w + i) & (kScopeSize - 1))].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed);
    scopeWrite.store ((w + numSamples) & (kScopeSize - 1), std::memory_order_relaxed);
}

//==============================================================================
void GroovyRackProcessor::loadSfx()
{
    const void* data[grv::Sfx::numIds] = { GrvSfx::sfx_intro_wav, GrvSfx::sfx_theme_ogg, GrvSfx::sfx_jump_wav, GrvSfx::sfx_death_wav };
    const int sizes[grv::Sfx::numIds] = { GrvSfx::sfx_intro_wavSize, GrvSfx::sfx_theme_oggSize, GrvSfx::sfx_jump_wavSize, GrvSfx::sfx_death_wavSize };
    sfx.load (data, sizes);
}

juce::AudioProcessorEditor* GroovyRackProcessor::createEditor()
{
    return new GroovyRackEditor (*this);
}

void GroovyRackProcessor::setSkin (int index)
{
    index = juce::jlimit (0, grv::kNumShells - 1, index);
    skinIndex.store (index);
    apvts.state.setProperty ("skin", index, nullptr);

    // ricordo l'ultima scelta anche per le prossime istanze del plugin
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Groovy").getChildFile ("G-POCKET").getChildFile ("skin.txt");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String (index));
}

void GroovyRackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("skin", skinIndex.load(), nullptr);
    storeCurveInState();
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GroovyRackProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            curve.fromString (apvts.state.getChildWithName ("ENV").getProperty ("points").toString());
            presets->syncFromState();
            if (apvts.state.hasProperty ("skin"))
                skinIndex.store (juce::jlimit (0, grv::kNumShells - 1, (int) apvts.state.getProperty ("skin")));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GroovyRackProcessor();
}
