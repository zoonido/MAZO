// MAZO - plugin shell
#include "plugin_processor.h"
#include "plugin_editor.h"

//==============================================================================
// One table ties every parameter ID to its place in mazo::Params, so reading the knobs, loading presets
// and saving presets can never disagree. Values are in the parameter's own units (percent shown as 0-100).
namespace
{
using P = mazo::Params;
#define MZ_F(ID, FIELD)   { ID, [] (const P& p) { return (float) p.FIELD; },          [] (P& p, float v) { p.FIELD = v; } }
#define MZ_PCT(ID, FIELD) { ID, [] (const P& p) { return p.FIELD * 100.0f; },         [] (P& p, float v) { p.FIELD = v / 100.0f; } }
#define MZ_I(ID, FIELD)   { ID, [] (const P& p) { return (float) p.FIELD; },          [] (P& p, float v) { p.FIELD = (int) std::lround (v); } }
#define MZ_B(ID, FIELD)   { ID, [] (const P& p) { return p.FIELD ? 1.0f : 0.0f; },    [] (P& p, float v) { p.FIELD = v > 0.5f; } }
const std::vector<MazoProcessor::Binding> bindings {
    MZ_I ("key", key), MZ_I ("octave", octave), MZ_F ("fine", fine), MZ_B ("velocity", velocity),
    MZ_I ("src", source), MZ_F ("click_decay", clickDecayMs), MZ_F ("click_tone", clickToneHz), MZ_F ("click_level", clickLevelDb),
    MZ_I ("wave", wave), MZ_F ("pitch_amt", pitchAmt), MZ_F ("sweep", sweepMs), MZ_PCT ("curve", curve),
    MZ_F ("attack", attackMs), MZ_F ("hold", holdMs), MZ_F ("decay", decayMs), MZ_F ("body_level", bodyLevelDb),
    MZ_I ("sub_oct", subOctave), MZ_B ("clean_sub", cleanSub), MZ_F ("sub_attack", subAttackMs), MZ_F ("sub_decay", subDecayMs),
    MZ_F ("sub_level", subLevelDb), MZ_PCT ("blend", blend),
    MZ_PCT ("rumble_amount", rumbleAmount), MZ_F ("rumble_decay", rumbleDecay), MZ_PCT ("rumble_drive", rumbleDrive), MZ_F ("rumble_tone", rumbleTone),
    MZ_PCT ("rumble_duck", rumbleDuck), MZ_I ("rumble_duck_time", rumbleDuckTime), MZ_PCT ("rumble_mix", rumbleMix),
    MZ_I ("warmth_type", warmthType), MZ_PCT ("warmth", warmth),
    MZ_I ("dist_mode", distMode), MZ_PCT ("dist_drive", distDrive), MZ_F ("dist_tone", distToneHz), MZ_PCT ("dist_mix", distMix),
    MZ_F ("comp_attack", compAttackMs), MZ_F ("comp_release", compReleaseMs), MZ_PCT ("comp_amount", compAmount), MZ_PCT ("comp_mix", compMix),
    MZ_F ("low_cut", lowCutHz), MZ_F ("tilt", tiltDb), MZ_F ("gain", gainDb), MZ_F ("ceiling", ceilingDb) };
#undef MZ_F
#undef MZ_PCT
#undef MZ_I
#undef MZ_B
}

const std::vector<MazoProcessor::Binding>& MazoProcessor::getBindings() { return bindings; }

mazo::Params MazoProcessor::currentParams() const
{
    mazo::Params p;
    for (const auto& b : bindings) b.set (p, apvts.getRawParameterValue (b.id)->load());
    return p;
}

void MazoProcessor::applyParams (const mazo::Params& p)
{
    for (const auto& b : bindings)
        if (auto* prm = apvts.getParameter (b.id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (b.get (p)));
            prm->endChangeGesture();
        }
}

juce::AudioProcessorValueTreeState::ParameterLayout MazoProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    // real-unit parameter; 'centre' puts that value in the middle of the knob travel
    auto num = [&] (const String& id, const String& name, float lo, float hi, float def, const String& unit, float centre = 0.0f, int decimals = 0)
    {
        NormalisableRange<float> r (lo, hi, 0.0f);
        if (centre > lo && centre < hi) r.setSkewForCentre (centre);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([unit, decimals] (float v, int)
            {
                if (unit == "Hz" && v >= 1000.0f) return String (v / 1000.0f, 1) + " kHz";
                if (unit == "ms" && v >= 1000.0f) return String (v / 1000.0f, 2) + " s";
                if (unit == "dB" && v <= -59.9f) return String ("Off");
                if (unit == "xKey") return "Key x" + String (v, 1);
                if (unit == "cut") return v <= 10.5f ? String ("Off") : String (roundToInt (v)) + " Hz";
                return String (v, decimals) + " " + unit;
            })));
    };
    auto choice = [&] (const String& id, const String& name, StringArray items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto toggle = [&] (const String& id, const String& name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    const mazo::Params d;
    StringArray notes; for (int k = 0; k < 12; ++k) notes.add (mazo::noteName (k));

    // Tuning
    choice ("key", "Key", notes, d.key);
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", 0, 2, d.octave));
    num ("fine", "Fine", -50.0f, 50.0f, d.fine, "ct", 0.0f, 1);
    toggle ("velocity", "Velocity", d.velocity);

    // Transient
    choice ("src", "Transient Source", { "Noise", "Click", "Pulse" }, d.source);
    num ("click_decay", "Transient Decay", 1.0f, 100.0f, d.clickDecayMs, "ms", 20.0f);
    num ("click_tone", "Transient Tone", 500.0f, 12000.0f, d.clickToneHz, "Hz", 3000.0f);
    num ("click_level", "Transient Level", -60.0f, 6.0f, d.clickLevelDb, "dB", 0.0f, 1);

    // Body
    choice ("wave", "Body Wave", { "Sine", "Tri", "Soft" }, d.wave);
    num ("pitch_amt", "Pitch Amount", 0.0f, 48.0f, d.pitchAmt, "st", 0.0f, 1);
    num ("sweep", "Sweep", 1.0f, 300.0f, d.sweepMs, "ms", 40.0f);
    num ("curve", "Curve", 0.0f, 100.0f, d.curve * 100.0f, "%");
    num ("attack", "Body Attack", 0.0f, 20.0f, d.attackMs, "ms", 3.0f, 1);
    num ("hold", "Body Hold", 0.0f, 500.0f, d.holdMs, "ms", 100.0f);
    num ("decay", "Body Decay", 20.0f, 3000.0f, d.decayMs, "ms", 400.0f);
    num ("body_level", "Body Level", -60.0f, 6.0f, d.bodyLevelDb, "dB", 0.0f, 1);

    // Sub / Tail
    choice ("sub_oct", "Sub Octave", { "Unison", "-1 Oct" }, d.subOctave);
    toggle ("clean_sub", "Clean Sub", d.cleanSub);
    num ("sub_attack", "Sub Attack", 0.0f, 300.0f, d.subAttackMs, "ms", 40.0f);
    num ("sub_decay", "Sub Decay", 50.0f, 6000.0f, d.subDecayMs, "ms", 900.0f);
    num ("sub_level", "Sub Level", -60.0f, 6.0f, d.subLevelDb, "dB", 0.0f, 1);
    num ("blend", "Sub Blend", 0.0f, 100.0f, d.blend * 100.0f, "%");

    // Rumble (on the Sub/Tail)
    num ("rumble_amount", "Rumble Amount", 0.0f, 100.0f, d.rumbleAmount * 100.0f, "%");
    num ("rumble_decay", "Rumble Decay", 0.3f, 8.0f, d.rumbleDecay, "s", 2.0f, 2);
    num ("rumble_drive", "Rumble Drive", 0.0f, 100.0f, d.rumbleDrive * 100.0f, "%");
    num ("rumble_tone", "Rumble Tone", 1.0f, 16.0f, d.rumbleTone, "xKey", 4.0f, 1);
    num ("rumble_duck", "Rumble Duck", 0.0f, 100.0f, d.rumbleDuck * 100.0f, "%");
    choice ("rumble_duck_time", "Rumble Duck Time", { "1/16", "1/8", "1/4" }, d.rumbleDuckTime);
    num ("rumble_mix", "Rumble Mix", 0.0f, 100.0f, d.rumbleMix * 100.0f, "%");

    // Warmth
    choice ("warmth_type", "Warmth Type", { "Tape", "Tube" }, d.warmthType);
    num ("warmth", "Warmth", 0.0f, 100.0f, d.warmth * 100.0f, "%");

    // Distortion (parallel)
    choice ("dist_mode", "Distortion Mode", { "Clip", "Fold", "Crush" }, d.distMode);
    num ("dist_drive", "Distortion Drive", 0.0f, 100.0f, d.distDrive * 100.0f, "%");
    num ("dist_tone", "Distortion Tone", 200.0f, 16000.0f, d.distToneHz, "Hz", 2500.0f);
    num ("dist_mix", "Distortion Mix", 0.0f, 100.0f, d.distMix * 100.0f, "%");

    // Compression
    num ("comp_attack", "Comp Attack", 0.1f, 50.0f, d.compAttackMs, "ms", 8.0f, 1);
    num ("comp_release", "Comp Release", 10.0f, 500.0f, d.compReleaseMs, "ms", 100.0f);
    num ("comp_amount", "Comp Amount", 0.0f, 100.0f, d.compAmount * 100.0f, "%");
    num ("comp_mix", "Comp Mix", 0.0f, 100.0f, d.compMix * 100.0f, "%");

    // Output
    num ("low_cut", "Low Cut", 10.0f, 200.0f, d.lowCutHz, "cut", 40.0f);
    num ("tilt", "Tilt", -6.0f, 6.0f, d.tiltDb, "dB", 0.0f, 1);
    num ("gain", "Output Gain", -24.0f, 12.0f, d.gainDb, "dB", 0.0f, 1);
    num ("ceiling", "Ceiling", -12.0f, 0.0f, d.ceilingDb, "dB", 0.0f, 1);

    return { p.begin(), p.end() };
}

MazoProcessor::MazoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAZO", createLayout()),
      presets (*this)
{
}

bool MazoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MazoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    engine.prepare (sampleRate, 512);   // the block is rendered in chunks of up to 512 samples
    setLatencySamples (engine.latency());   // the limiter's 1.5 ms look-ahead; Ableton compensates for it
}

void MazoProcessor::readParams()
{
    for (const auto& b : bindings) b.set (engine.params, apvts.getRawParameterValue (b.id)->load());
}

void MazoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    readParams();

    double bpm = 120.0;   // Rumble Duck Time follows the Live tempo
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto t = pos->getBpm()) bpm = *t;

    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);

    // Any note-on fires the kick at its exact sample; note number is ignored (fixed key), note-offs too
    int pos = 0;
    auto renderTo = [&] (int end)
    {
        end = juce::jlimit (0, n, end);
        while (end > pos)
        {
            const int chunk = juce::jmin (end - pos, 512);
            engine.process (L + pos, chunk, bpm);
            pos += chunk;
        }
    };
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { renderTo (meta.samplePosition); engine.trigger (m.getFloatVelocity()); }
        else if (m.isAllSoundOff()) { renderTo (meta.samplePosition); engine.allOff(); }
    }
    renderTo (n);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch) buffer.copyFrom (ch, 0, buffer, 0, 0, n);

    // for the OUT meter: the loudest sample since the window last looked
    const float pk = buffer.getMagnitude (0, 0, n);
    float prev = outPeak.load();
    while (pk > prev && ! outPeak.compare_exchange_weak (prev, pk)) {}
    midi.clear();
}

juce::AudioProcessorEditor* MazoProcessor::createEditor() { return new MazoEditor (*this); }

// The Live Set keeps every knob plus the preset name and the Lock Key switch
void MazoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("lockKey", presets.getLockKey(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MazoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            presets.restore (tree.getProperty ("presetName", "Inicio").toString(), (bool) tree.getProperty ("lockKey", true));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MazoProcessor(); }
