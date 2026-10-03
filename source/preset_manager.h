// MAZO - presets: the 16 factory presets plus your own, saved as .mazopreset files in ~/Music/MAZO/Presets
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class MazoProcessor;

class PresetManager
{
public:
    explicit PresetManager (MazoProcessor& p) : proc (p) {}

    static juce::File folder();                       // ~/Music/MAZO/Presets
    static constexpr const char* extension = ".mazopreset";

    juce::StringArray factoryNames() const;
    juce::StringArray factoryStyles() const;
    juce::StringArray userNames() const;              // sorted, from the folder

    void loadFactory (int index);
    bool loadUser (const juce::String& name);

    enum class SaveResult { saved, exists, factoryName, invalid, failed };
    SaveResult save (const juce::String& name, bool overwrite);
    bool remove (const juce::String& name);           // user presets only

    void next();                                      // steps through factory, then user presets
    void previous();

    juce::String getCurrentName() const { return currentName; }
    bool currentIsUser() const { return currentUser; }
    void setLockKey (bool on) { lockKey = on; }
    bool getLockKey() const { return lockKey; }
    void restore (const juce::String& name, bool lock);   // from a saved Live Set (doesn't change the knobs)

    std::function<void()> onChange;                   // the preset bar refreshes itself

private:
    void setCurrent (const juce::String& name, bool user);
    MazoProcessor& proc;
    juce::String currentName { "Inicio" };
    bool currentUser = false, lockKey = true;
};
