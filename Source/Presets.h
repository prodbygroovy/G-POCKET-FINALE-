#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class GroovyRackProcessor;

// Preset di fabbrica (nel codice) + preset utente (file .xml in Documenti/Groovy/G-POCKET/Presets)
class PresetManager
{
public:
    explicit PresetManager (GroovyRackProcessor& p);

    int size() const { return (int) entries.size(); }
    int numFactory() const { return factoryCount; }
    juce::String name (int index) const;
    bool isFactory (int index) const;
    // 0 = BASIC, 1 = LIGHT, 2 = DESTROY, 3 = USER
    int category (int index) const;
    static const char* categoryName (int cat);

    // elimina un preset utente (i preset di fabbrica non si possono eliminare); ritorna false se non riesce
    bool deleteUser (int index);
    // nome libero tipo "USER 01", "USER 02"...
    juce::String suggestUserName() const;

    int current() const { return currentIndex; }
    void load (int index);
    void step (int delta); // circolare

    // salva lo stato attuale come preset utente; ritorna false se non riesce a scrivere il file
    bool saveUser (const juce::String& name);
    void rescan();
    void syncFromState(); // rilegge il nome del preset dallo stato (dopo il caricamento di un progetto)

    static juce::File userFolder();

private:
    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = 0;
        juce::File file;
        int cat = 3;
    };

    void loadFactory (int factoryIndex);
    void loadFile (const juce::File& f);

    GroovyRackProcessor& proc;
    std::vector<Entry> entries;
    int factoryCount = 0;
    int currentIndex = 0;
};
