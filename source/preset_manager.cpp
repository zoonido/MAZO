// MAZO - preset manager
#include "preset_manager.h"
#include "plugin_processor.h"
#include "dsp/presets.h"

juce::File PresetManager::folder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("MAZO").getChildFile ("Presets");
}

juce::StringArray PresetManager::factoryNames() const
{
    juce::StringArray s; for (const auto& f : mazo::factoryPresets()) s.add (juce::String::fromUTF8 (f.name)); return s;
}

juce::StringArray PresetManager::factoryStyles() const
{
    juce::StringArray s; for (const auto& f : mazo::factoryPresets()) s.add (juce::String::fromUTF8 (f.style)); return s;
}

juce::StringArray PresetManager::userNames() const
{
    juce::StringArray s;
    for (const auto& f : folder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
        s.add (f.getFileNameWithoutExtension());
    s.sortNatural();
    return s;
}

void PresetManager::setCurrent (const juce::String& name, bool user)
{
    currentName = name; currentUser = user;
    if (onChange) onChange();
}

void PresetManager::loadFactory (int index)
{
    const auto& list = mazo::factoryPresets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    auto p = proc.currentParams();
    mazo::applyFactory (p, index, lockKey);
    proc.applyParams (p);
    setCurrent (juce::String::fromUTF8 (list[(size_t) index].name), false);
}

bool PresetManager::loadUser (const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (folder().getChildFile (name + extension));
    if (xml == nullptr || ! xml->hasTagName ("MAZOPRESET")) return false;

    const auto now = proc.currentParams();
    mazo::Params p;                                   // anything missing from an older preset file gets its default
    for (const auto& b : MazoProcessor::getBindings())
        if (auto* e = xml->getChildByAttribute ("id", b.id))
            b.set (p, (float) e->getDoubleAttribute ("value", (double) b.get (p)));
    if (lockKey) { p.key = now.key; p.octave = now.octave; p.fine = now.fine; }
    proc.applyParams (p);
    setCurrent (name, true);
    return true;
}

PresetManager::SaveResult PresetManager::save (const juce::String& rawName, bool overwrite)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return SaveResult::invalid;
    if (factoryNames().contains (name, true)) return SaveResult::factoryName;
    const auto file = folder().getChildFile (name + extension);
    if (file.existsAsFile() && ! overwrite) return SaveResult::exists;

    juce::XmlElement xml ("MAZOPRESET");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    const auto p = proc.currentParams();
    for (const auto& b : MazoProcessor::getBindings())
    {
        auto* e = xml.createNewChildElement ("PARAM");
        e->setAttribute ("id", b.id);
        e->setAttribute ("value", (double) b.get (p));
    }
    if (! folder().createDirectory() || ! xml.writeTo (file)) return SaveResult::failed;
    setCurrent (name, true);
    return SaveResult::saved;
}

bool PresetManager::remove (const juce::String& name)
{
    const auto file = folder().getChildFile (name + extension);
    if (! file.existsAsFile() || ! file.deleteFile()) return false;
    if (currentUser && currentName == name) setCurrent (currentName, false);   // the knobs stay; it's just not a saved preset any more
    else if (onChange) onChange();
    return true;
}

void PresetManager::next()
{
    const auto f = factoryNames(); const auto u = userNames();
    const int total = f.size() + u.size();
    int pos = currentUser ? f.size() + u.indexOf (currentName) : f.indexOf (currentName);
    pos = (pos + 1 + total) % total;
    if (pos < f.size()) loadFactory (pos); else loadUser (u[pos - f.size()]);
}

void PresetManager::previous()
{
    const auto f = factoryNames(); const auto u = userNames();
    const int total = f.size() + u.size();
    int pos = currentUser ? f.size() + u.indexOf (currentName) : f.indexOf (currentName);
    if (pos < 0) pos = 0;
    pos = (pos - 1 + total) % total;
    if (pos < f.size()) loadFactory (pos); else loadUser (u[pos - f.size()]);
}

void PresetManager::restore (const juce::String& name, bool lock)
{
    lockKey = lock;
    currentName = name;
    currentUser = ! factoryNames().contains (name) && folder().getChildFile (name + extension).existsAsFile();
    if (onChange) onChange();
}
