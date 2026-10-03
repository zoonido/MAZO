// MAZO - plugin shell: parameters, MIDI timing, saving state
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/engine.h"
#include "preset_manager.h"

class MazoProcessor : public juce::AudioProcessor
{
public:
    MazoProcessor();
    ~MazoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MAZO"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }   // long Rumble decays

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // parameter ID <-> mazo::Params field
    struct Binding { const char* id; float (*get) (const mazo::Params&); void (*set) (mazo::Params&, float); };
    static const std::vector<Binding>& getBindings();
    mazo::Params currentParams() const;            // the knobs as they are now
    void applyParams (const mazo::Params& p);      // set every knob (message thread), Ableton sees the changes

    PresetManager presets;
    std::atomic<float> outPeak { 0.0f };           // read (and reset) by the window's meter

private:
    void readParams();
    float f (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    int   i (const juce::String& id) const { return (int) std::lround (f (id)); }
    bool  b (const juce::String& id) const { return f (id) > 0.5f; }

    mazo::Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MazoProcessor)
};
