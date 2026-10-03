#include "Presets.h"
#include "PluginProcessor.h"

namespace
{
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // id parametro -> valore reale
    const char* curve;                                  // "x,y,curva;..." oppure nullptr = curva di default
    int cat = 0;                                        // 0 = BASIC, 1 = LIGHT, 2 = DESTROY
};

// Modo env: 0 = MIDI, 1 = Follower, 2 = Sync.  Rate sync (indice): 0 = 1/32, 2 = 1/16, 4 = 1/8, 6 = 1/4, 8 = 1/2, 10 = 1 bar; i dispari sono le terzine
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "INIT",
          { { "dist_on", 0.0f }, { "sat_on", 0.0f }, { "crush_on", 0.0f }, { "verb_on", 0.0f } },
          nullptr },

        { "LO-FI TAPE",
          { { "dist_on", 0.0f },
            { "sat_on", 1.0f }, { "sat_drive", 10.0f }, { "sat_warm", 0.7f }, { "sat_mix", 0.8f },
            { "crush_on", 1.0f }, { "crush_bits", 12.0f }, { "crush_rate", 2.0f }, { "crush_mix", 0.3f },
            { "verb_on", 1.0f }, { "verb_size", 0.35f }, { "verb_damp", 0.8f }, { "verb_mix", 0.2f },
            { "order", 9.0f } }, // sat -> crush -> verb -> dist (indice della permutazione)
          nullptr },

        { "8-BIT CRUNCH",
          { { "dist_on", 1.0f }, { "dist_drive", 8.0f }, { "dist_mix", 0.5f },
            { "sat_on", 0.0f },
            { "crush_on", 1.0f }, { "crush_bits", 5.0f }, { "crush_rate", 6.0f }, { "crush_mix", 1.0f },
            { "verb_on", 0.0f },
            { "crush_rate_mod", 0.5f },
            { "env_mode", 0.0f }, { "env_time", 400.0f } },
          nullptr },

        { "PUMP GLUE",
          { { "dist_on", 0.0f }, { "sat_on", 1.0f }, { "sat_drive", 6.0f }, { "sat_level_mod", -0.6f },
            { "crush_on", 0.0f }, { "verb_on", 1.0f }, { "verb_mix", 0.25f },
            { "env_mode", 2.0f }, { "env_rate", 6.0f } },
          "0,1,-0.5;0.6,0,0;1,0,0" },

        { "WIDE HALL",
          { { "dist_on", 0.0f }, { "sat_on", 1.0f }, { "sat_drive", 3.0f }, { "sat_mix", 0.6f },
            { "crush_on", 0.0f },
            { "verb_on", 1.0f }, { "verb_size", 0.85f }, { "verb_damp", 0.35f }, { "verb_width", 1.0f }, { "verb_mix", 0.45f } },
          nullptr },

        { "FILTHY DRIVE",
          { { "dist_on", 1.0f }, { "dist_drive", 30.0f }, { "dist_tone", 3500.0f }, { "dist_level", -6.0f },
            { "sat_on", 1.0f }, { "sat_drive", 12.0f }, { "sat_warm", 0.5f },
            { "crush_on", 0.0f }, { "verb_on", 0.0f } },
          nullptr },

        { "FOLLOW WOBBLE",
          { { "dist_on", 1.0f }, { "dist_drive", 10.0f }, { "dist_drive_mod", 0.7f },
            { "sat_on", 0.0f },
            { "crush_on", 1.0f }, { "crush_bits", 8.0f }, { "crush_bits_mod", -0.4f }, { "crush_mix", 0.8f },
            { "verb_on", 0.0f },
            { "env_mode", 1.0f }, { "env_attack", 3.0f }, { "env_release", 200.0f } },
          "0,0,0;1,1,0" },

        { "RISER FX",
          { { "dist_on", 1.0f }, { "dist_drive", 6.0f }, { "dist_drive_mod", 0.5f },
            { "sat_on", 0.0f }, { "crush_on", 0.0f },
            { "verb_on", 1.0f }, { "verb_size", 0.7f }, { "verb_mix", 0.1f }, { "verb_mix_mod", 0.7f },
            { "env_mode", 0.0f }, { "env_time", 4000.0f } },
          "0,0,0.6;1,1,0" },

        { "TELEPHONE",
          { { "dist_on", 1.0f }, { "dist_drive", 10.0f }, { "dist_tone", 1200.0f }, { "dist_level", -2.0f },
            { "sat_on", 0.0f },
            { "crush_on", 1.0f }, { "crush_bits", 10.0f }, { "crush_rate", 3.0f }, { "crush_mix", 0.5f },
            { "verb_on", 0.0f } },
          nullptr },

        { "GHOST ROOM",
          { { "dist_on", 0.0f }, { "sat_on", 1.0f }, { "sat_drive", 2.0f }, { "sat_mix", 0.3f },
            { "crush_on", 0.0f },
            { "verb_on", 1.0f }, { "verb_size", 0.75f }, { "verb_damp", 0.2f }, { "verb_width", 1.0f }, { "verb_mix", 0.5f } },
          nullptr },

        //======================================================================
        // LIGHT: ritocchi leggeri, il suono resta riconoscibile
        { "TAPE WARMTH",
          { { "sat_on", 1.0f }, { "sat_drive", 4.0f }, { "sat_warm", 0.6f }, { "sat_mix", 0.6f } },
          nullptr, 1 },

        { "SOFT GLUE",
          { { "sat_on", 1.0f }, { "sat_drive", 3.0f }, { "sat_warm", 0.4f }, { "sat_mix", 0.5f },
            { "verb_on", 1.0f }, { "verb_size", 0.3f }, { "verb_mix", 0.08f } },
          nullptr, 1 },

        { "GENTLE AIR",
          { { "verb_on", 1.0f }, { "verb_size", 0.45f }, { "verb_damp", 0.3f }, { "verb_width", 1.0f }, { "verb_mix", 0.12f } },
          nullptr, 1 },

        { "VINTAGE SHEEN",
          { { "dist_on", 1.0f }, { "dist_drive", 3.0f }, { "dist_tone", 9000.0f }, { "dist_mix", 0.2f },
            { "sat_on", 1.0f }, { "sat_drive", 5.0f }, { "sat_warm", 0.8f }, { "sat_mix", 0.4f } },
          nullptr, 1 },

        { "DUSTY VOCAL",
          { { "sat_on", 1.0f }, { "sat_drive", 3.0f }, { "sat_warm", 0.5f }, { "sat_mix", 0.5f },
            { "crush_on", 1.0f }, { "crush_bits", 12.0f }, { "crush_rate", 2.0f }, { "crush_mix", 0.15f },
            { "verb_on", 1.0f }, { "verb_size", 0.25f }, { "verb_mix", 0.1f } },
          nullptr, 1 },

        { "SMOOTH BREATH",
          { { "sat_on", 1.0f }, { "sat_drive", 5.0f }, { "sat_mix", 0.6f }, { "sat_drive_mod", 0.25f },
            { "verb_on", 1.0f }, { "verb_size", 0.4f }, { "verb_mix", 0.1f },
            { "env_mode", 1.0f }, { "env_attack", 10.0f }, { "env_release", 300.0f } },
          "0,0,0;1,1,0", 1 },

        { "TINY ROOM",
          { { "sat_on", 1.0f }, { "sat_drive", 2.0f }, { "sat_mix", 0.4f },
            { "verb_on", 1.0f }, { "verb_size", 0.2f }, { "verb_damp", 0.6f }, { "verb_mix", 0.15f } },
          nullptr, 1 },

        { "SUBTLE PUSH",
          { { "dist_on", 1.0f }, { "dist_drive", 5.0f }, { "dist_tone", 8000.0f }, { "dist_mix", 0.25f }, { "dist_level", -1.0f },
            { "sat_on", 1.0f }, { "sat_drive", 3.0f }, { "sat_mix", 0.4f } },
          nullptr, 1 },

        { "SLOW BLOOM",
          { { "verb_on", 1.0f }, { "verb_size", 0.6f }, { "verb_damp", 0.4f }, { "verb_mix", 0.15f }, { "verb_mix_mod", 0.3f },
            { "env_mode", 0.0f }, { "env_time", 1500.0f } },
          "0,0,0.4;1,1,0", 1 },

        { "GENTLE PUMP",
          { { "sat_on", 1.0f }, { "sat_drive", 3.0f }, { "sat_mix", 0.5f }, { "sat_level_mod", -0.25f },
            { "env_mode", 2.0f }, { "env_rate", 6.0f } },
          "0,1,-0.5;0.6,0,0;1,0,0", 1 },

        //======================================================================
        // DESTROY: il suono in ingresso viene demolito e ricostruito da capo
        { "TOTAL MELTDOWN",
          { { "dist_on", 1.0f }, { "dist_drive", 40.0f }, { "dist_tone", 3000.0f }, { "dist_mix", 1.0f }, { "dist_level", -8.0f },
            { "sat_on", 1.0f }, { "sat_drive", 28.0f }, { "sat_warm", 0.8f }, { "sat_mix", 1.0f }, { "sat_level", -4.0f },
            { "crush_on", 1.0f }, { "crush_bits", 4.0f }, { "crush_rate", 8.0f }, { "crush_mix", 1.0f }, { "crush_level", -4.0f },
            { "verb_on", 0.0f } },
          nullptr, 2 },

        { "ROBOT FACTORY",
          { { "dist_on", 1.0f }, { "dist_drive", 25.0f }, { "dist_tone", 1800.0f }, { "dist_mix", 0.8f }, { "dist_level", -6.0f },
            { "crush_on", 1.0f }, { "crush_bits", 3.0f }, { "crush_rate", 20.0f }, { "crush_mix", 1.0f }, { "crush_rate_mod", 0.6f },
            { "verb_on", 1.0f }, { "verb_size", 0.5f }, { "verb_damp", 0.8f }, { "verb_mix", 0.4f },
            { "env_mode", 2.0f }, { "env_rate", 4.0f } },
          "0,1,0;0.5,0,0;1,0,0", 2 },

        { "ATOMIC CRUSH",
          { { "sat_on", 1.0f }, { "sat_drive", 24.0f }, { "sat_mix", 1.0f }, { "sat_level", -4.0f },
            { "crush_on", 1.0f }, { "crush_bits", 2.0f }, { "crush_rate", 40.0f }, { "crush_mix", 1.0f }, { "crush_level", -6.0f },
            { "order", 12.0f } },
          nullptr, 2 },

        { "DEAD RADIO",
          { { "dist_on", 1.0f }, { "dist_drive", 35.0f }, { "dist_tone", 900.0f }, { "dist_mix", 1.0f }, { "dist_level", -6.0f },
            { "crush_on", 1.0f }, { "crush_bits", 6.0f }, { "crush_rate", 12.0f }, { "crush_mix", 1.0f },
            { "verb_on", 1.0f }, { "verb_size", 0.9f }, { "verb_damp", 0.9f }, { "verb_mix", 0.3f } },
          nullptr, 2 },

        { "GLITCH STORM",
          { { "dist_on", 1.0f }, { "dist_drive", 20.0f }, { "dist_drive_mod", 0.8f }, { "dist_mix", 1.0f },
            { "crush_on", 1.0f }, { "crush_bits", 4.0f }, { "crush_rate", 16.0f }, { "crush_mix", 1.0f },
            { "crush_rate_mod", 0.9f }, { "crush_bits_mod", -0.6f },
            { "env_mode", 2.0f }, { "env_rate", 2.0f } },
          "0,0,0;0.25,1,0;0.5,0,0;0.75,1,0;1,0,0", 2 },

        { "CATHEDRAL OF NOISE",
          { { "dist_on", 1.0f }, { "dist_drive", 30.0f }, { "dist_tone", 2500.0f }, { "dist_mix", 1.0f }, { "dist_level", -8.0f },
            { "sat_on", 1.0f }, { "sat_drive", 20.0f }, { "sat_mix", 1.0f }, { "sat_level", -4.0f },
            { "verb_on", 1.0f }, { "verb_size", 1.0f }, { "verb_damp", 0.2f }, { "verb_width", 1.0f }, { "verb_mix", 0.8f } },
          nullptr, 2 },

        { "BROKEN GAMEBOY",
          { { "sat_on", 1.0f }, { "sat_drive", 12.0f }, { "sat_mix", 1.0f },
            { "crush_on", 1.0f }, { "crush_bits", 3.0f }, { "crush_rate", 24.0f }, { "crush_mix", 1.0f },
            { "crush_bits_mod", -0.5f }, { "crush_rate_mod", 0.5f },
            { "verb_on", 1.0f }, { "verb_size", 0.2f }, { "verb_damp", 0.9f }, { "verb_mix", 0.25f },
            { "env_mode", 1.0f }, { "env_attack", 2.0f }, { "env_release", 120.0f } },
          "0,0,0;1,1,0", 2 },

        { "SCREAMING ENGINE",
          { { "dist_on", 1.0f }, { "dist_drive", 40.0f }, { "dist_tone", 6000.0f }, { "dist_mix", 1.0f }, { "dist_level", -10.0f }, { "dist_drive_mod", 0.5f },
            { "sat_on", 1.0f }, { "sat_drive", 30.0f }, { "sat_warm", 1.0f }, { "sat_mix", 1.0f }, { "sat_level", -6.0f },
            { "env_mode", 0.0f }, { "env_time", 2500.0f } },
          "0,0,0.5;1,1,0", 2 },

        { "VOID TEXTURE",
          { { "dist_on", 1.0f }, { "dist_drive", 18.0f }, { "dist_mix", 0.7f },
            { "crush_on", 1.0f }, { "crush_bits", 5.0f }, { "crush_rate", 30.0f }, { "crush_mix", 0.9f },
            { "verb_on", 1.0f }, { "verb_size", 1.0f }, { "verb_damp", 0.7f }, { "verb_width", 1.0f }, { "verb_mix", 0.9f },
            { "order", 22.0f } },
          nullptr, 2 },

        { "NUCLEAR PUMP",
          { { "dist_on", 1.0f }, { "dist_drive", 35.0f }, { "dist_mix", 1.0f }, { "dist_level", -8.0f },
            { "sat_on", 1.0f }, { "sat_drive", 25.0f }, { "sat_mix", 1.0f }, { "sat_level_mod", -0.8f },
            { "crush_on", 1.0f }, { "crush_bits", 6.0f }, { "crush_rate", 6.0f }, { "crush_mix", 0.7f },
            { "env_mode", 2.0f }, { "env_rate", 6.0f } },
          "0,1,-0.6;0.7,0,0;1,0,0", 2 },
    };
    return presets;
}
} // namespace

//==============================================================================
PresetManager::PresetManager (GroovyRackProcessor& p) : proc (p)
{
    rescan();
}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Groovy").getChildFile ("G-POCKET").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    const auto currentName = juce::isPositiveAndBelow (currentIndex, (int) entries.size()) ? entries[(size_t) currentIndex].name : juce::String();

    entries.clear();
    const auto& fp = factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        entries.push_back ({ fp[(size_t) i].name, true, i, {}, fp[(size_t) i].cat });
    factoryCount = (int) fp.size();

    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();
    for (auto& f : files)
        entries.push_back ({ f.getFileNameWithoutExtension().toUpperCase(), false, 0, f, 3 });

    currentIndex = (currentName.isEmpty() && currentIndex < 0) ? -1 : 0;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].name == currentName)
            currentIndex = i;
}

juce::String PresetManager::name (int i) const
{
    return juce::isPositiveAndBelow (i, size()) ? entries[(size_t) i].name : juce::String();
}

bool PresetManager::isFactory (int i) const
{
    return juce::isPositiveAndBelow (i, size()) && entries[(size_t) i].factory;
}

int PresetManager::category (int i) const
{
    return juce::isPositiveAndBelow (i, size()) ? entries[(size_t) i].cat : 3;
}

const char* PresetManager::categoryName (int cat)
{
    static const char* names[4] = { "BASIC", "LIGHT", "DESTROY", "USER" };
    return names[juce::jlimit (0, 3, cat)];
}

bool PresetManager::deleteUser (int index)
{
    if (! juce::isPositiveAndBelow (index, size()) || entries[(size_t) index].factory)
        return false;

    const auto f = entries[(size_t) index].file;
    if (f.existsAsFile() && ! f.deleteFile())
        return false;

    const bool wasCurrent = index == currentIndex;
    rescan();
    if (wasCurrent)
    {
        currentIndex = -1;                                    // il suono resta com'è, ma nessun preset risulta selezionato
        proc.apvts.state.setProperty ("presetName", "", nullptr);
    }
    return true;
}

juce::String PresetManager::suggestUserName() const
{
    for (int n = 1; n < 1000; ++n)
    {
        const auto candidate = "USER " + juce::String (n).paddedLeft ('0', 2);
        bool taken = false;
        for (auto& e : entries)
            taken |= e.name == candidate;
        if (! taken)
            return candidate;
    }
    return "USER";
}

void PresetManager::step (int delta)
{
    if (size() == 0)
        return;
    load (((currentIndex + delta) % size() + size()) % size());
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, size()))
        return;

    const auto& e = entries[(size_t) index];
    if (e.factory)
        loadFactory (e.factoryIndex);
    else
        loadFile (e.file);

    currentIndex = index;
    proc.apvts.state.setProperty ("presetName", e.name, nullptr);
}

void PresetManager::loadFactory (int factoryIndex)
{
    const auto& fp = factoryPresets()[(size_t) factoryIndex];

    for (auto* prm : proc.getParameters())
        prm->setValueNotifyingHost (prm->getDefaultValue());

    for (auto& [id, value] : fp.values)
        if (auto* rp = proc.apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        else
            jassertfalse; // id sbagliato in un preset

    if (fp.curve != nullptr)
        proc.getCurve().fromString (fp.curve);
    else
        proc.getCurve().resetDefault();

    proc.storeCurveInState();
}

void PresetManager::loadFile (const juce::File& f)
{
    if (auto xml = juce::parseXML (f))
    {
        if (xml->hasTagName (proc.apvts.state.getType()))
        {
            proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
            proc.apvts.state.setProperty ("skin", proc.getSkin(), nullptr);   // un preset non cambia la skin
            proc.getCurve().fromString (proc.apvts.state.getChildWithName ("ENV").getProperty ("points").toString());
        }
    }
}

bool PresetManager::saveUser (const juce::String& rawName)
{
    auto cleaned = juce::File::createLegalFileName (rawName.trim()).trim();
    if (cleaned.isEmpty())
        return false;

    auto dir = userFolder();
    if (! dir.createDirectory().wasOk())
        return false;

    proc.storeCurveInState();
    proc.apvts.state.setProperty ("presetName", cleaned.toUpperCase(), nullptr);

    auto xml = proc.apvts.copyState().createXml();
    if (xml == nullptr || ! xml->writeTo (dir.getChildFile (cleaned + ".xml")))
        return false;

    rescan();
    for (int i = 0; i < size(); ++i)
        if (entries[(size_t) i].name == cleaned.toUpperCase() && ! entries[(size_t) i].factory)
            currentIndex = i;
    return true;
}

void PresetManager::syncFromState()
{
    const auto n = proc.apvts.state.getProperty ("presetName").toString();
    for (int i = 0; i < size(); ++i)
        if (entries[(size_t) i].name == n)
        {
            currentIndex = i;
            return;
        }
}
