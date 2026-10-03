#pragma once

#include "Sfx.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Pedals.h"
#include "DSP/Params.h"
#include "DSP/Order.h"
#include "DSP/EnvelopeCurve.h"
#include "Presets.h"

class GroovyRackProcessor : public juce::AudioProcessor
{
public:
    GroovyRackProcessor();
    ~GroovyRackProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    // I preset di fabbrica sono esposti alla DAW come "programmi"
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Usati dall'interfaccia
    grv::EnvelopeCurve& getCurve() { return curve; }
    void storeCurveInState();

    // Stato "live" dell'envelope per l'animazione (0..1)
    std::atomic<float> uiPhase { 1.f }, uiValue { 0.f };

    // Oscilloscopio: ultimi campioni in uscita (mono), letti dall'interfaccia
    static constexpr int kScopeSize = 2048;
    void readScope (float* dest, int numSamples) const;

    PresetManager& getPresets() { return *presets; }

    // suoni dell'easter egg
    void loadSfx();                                    // decodifica i suoni incorporati (la prima volta)
    void playSfx (grv::Sfx::Cmd c) { sfx.request (c); }
    bool isSfxMusicPaused() const { return sfx.isMusicPaused(); }   // per le prove

    // skin della scocca (0 = STEEL, 1 = BLACK, 2 = WHITE): salvata nello stato e ricordata per le nuove istanze
    int getSkin() const { return skinIndex.load(); }
    void setSkin (int index);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float computeEnvelope (const juce::AudioBuffer<float>& buffer, int pos, int len,
                           double blockPpq, bool hasPpq, bool triggered);

    grv::EnvelopeCurve curve;
    grv::Sfx sfx;
    std::unique_ptr<PresetManager> presets;
    std::atomic<int> skinIndex { 0 };

    std::array<std::atomic<float>, kScopeSize> scopeBuf;
    std::atomic<int> scopeWrite { 0 };

    grv::Distortion dist;
    grv::Saturation sat;
    grv::Bitcrush crush;
    grv::ReverbPedal verb;

    // parametri in cache (thread audio)
    std::atomic<float>* raw[4][4] {};
    std::atomic<float>* mod[4][4] {};
    std::atomic<float>* on[4] {};
    juce::NormalisableRange<float> ranges[4][4];
    std::atomic<float> *envOn = nullptr, *envMode = nullptr, *envRate = nullptr, *envTime = nullptr,
                       *envLoop = nullptr, *envAttack = nullptr, *envRelease = nullptr,
                       *envGain = nullptr, *orderParam = nullptr;

    // stato envelope
    double sampleRate = 44100.0;
    double envPhase = 1.0;
    bool envRunning = false;
    float follLevel = 0.f;
    double freePpq = 0.0;
    double lastBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovyRackProcessor)
};
