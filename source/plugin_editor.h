// MAZO - the designed window (from the approved mockup), drawn at 1280 x 842 and scaled to the window size
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "plugin_processor.h"
#include "ui_look.h"

namespace mazo::ui
{
// A knob with its name and live value under it
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name, bool showValue = true, bool accentRing = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    static constexpr int width = 62, height = 80;
private:
    juce::RangedAudioParameter* param;
    juce::String name; bool showValue;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};

// A row of buttons for a choice, switch or int parameter (one lit at a time)
class Segmented : public juce::Component
{
public:
    Segmented (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label,
               const juce::StringArray& texts, const juce::Array<float>& values, bool mono = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::OwnedArray<juce::TextButton> buttons;
    int columns = 0;           // 0 = one row, right-aligned after the label; >0 = grid filling the width
    int buttonWidth (int i) const;
private:
    void show (float v);
    juce::String label; juce::Array<float> values;
    juce::ParameterAttachment attach;
};

// A panel: title, small note on the right, switch rows, then knobs that wrap
class Section : public juce::Component
{
public:
    Section (juce::String t, juce::String n) : title (std::move (t)), note (std::move (n)) {}
    void addSwitch (Segmented* s) { switches.add (s); addAndMakeVisible (s); }
    void addKnob (Knob* k) { knobs.add (k); addAndMakeVisible (k); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::String title, note;
    juce::OwnedArray<Segmented> switches; juce::OwnedArray<Knob> knobs;
};

// The kick drawn from the current settings, with the pitch sweep landing on the Key
class WaveDisplay : public juce::Component
{
public:
    explicit WaveDisplay (MazoProcessor& p) : proc (p) {}
    void recompute();
    void paint (juce::Graphics&) override;
private:
    MazoProcessor& proc;
    std::vector<float> lo, hi, pitch;   // the kick (every 4th sample) and the pitch curve
    juce::String keyText;
};

class TuningPanel : public juce::Component
{
public:
    explicit TuningPanel (MazoProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh() { repaint(); }
private:
    MazoProcessor& proc;
    Segmented octave, notes;
    Knob fine;
};

class Meter : public juce::Component
{
public:
    void set (float peak);
    void paint (juce::Graphics&) override;
private:
    float level = 0, hold = 0;
};

class Header : public juce::Component
{
public:
    explicit Header (MazoProcessor& p);
    ~Header() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshPresets();
    Meter meter;
private:
    void askToSave();
    void askToDelete();
    PresetManager& presets;
    juce::TextButton prev { "" }, next { "" }, saveButton { "Save" }, deleteButton { "Delete" };
    juce::ComboBox box;
    juce::ToggleButton lockKey { "Lock Key" };
    std::unique_ptr<juce::AlertWindow> nameDialog;
    static constexpr int userIdBase = 1000;
};

// Everything, at the mockup's 1280 x 842
class Main : public juce::Component, private juce::Timer, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit Main (MazoProcessor& p);
    ~Main() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int W = 1280, H = 842;
    Header header;
private:
    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override { dirty = true; }
    MazoProcessor& proc;
    WaveDisplay wave;
    TuningPanel tuning;
    juce::OwnedArray<Section> rowA, rowB;
    std::atomic<bool> dirty { true };
};
} // namespace mazo::ui

class MazoEditor : public juce::AudioProcessorEditor
{
public:
    explicit MazoEditor (MazoProcessor& p);
    ~MazoEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (mazo::ui::col::bg); }
    mazo::ui::Main& getMain() { return main; }
private:
    mazo::ui::Look look;
    juce::TooltipWindow tips { this, 600 };
    mazo::ui::Main main;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MazoEditor)
};
