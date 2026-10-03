// MAZO - the designed window
#include "plugin_editor.h"
#include "dsp/presets.h"

namespace mazo::ui
{
using namespace juce;

static String niceValue (String s)
{
    s = s.replace ("Key x", String::fromUTF8 ("Key \xc3\x97"));   // Key ×8.0
    if (s.startsWithChar ('-')) s = String::fromUTF8 ("\xe2\x88\x92") + s.substring (1);   // proper minus sign
    return s;
}

//==============================================================================
Knob::Knob (AudioProcessorValueTreeState& s, const String& id, const String& n, bool value, bool ring)
    : param (s.getParameter (id)), name (n), showValue (value)
{
    jassert (param != nullptr);
    getProperties().set ("paramID", id);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.getProperties().set ("accentRing", ring);
    slider.setTooltip (param->getName (64) + "  (double-click to reset)");
    addAndMakeVisible (slider);
    attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void Knob::resized() { slider.setBounds ((getWidth() - 44) / 2, 0, 44, 44); }

void Knob::paint (Graphics& g)
{
    g.setColour (col::muted); g.setFont (Fonts::label (10.0f, 1.0f));
    g.drawText (name, 0, 49, getWidth(), 12, Justification::centred, false);
    if (showValue)
    {
        g.setColour (col::text); g.setFont (Fonts::value (11.0f));
        g.drawText (niceValue (param->getCurrentValueAsText()), -4, 63, getWidth() + 8, 14, Justification::centred, false);
    }
}

//==============================================================================
Segmented::Segmented (AudioProcessorValueTreeState& s, const String& id, const String& l,
                      const StringArray& texts, const Array<float>& v, bool mono)
    : label (l), values (v), attach (*s.getParameter (id), [this] (float x) { show (x); }, nullptr)
{
    getProperties().set ("paramID", id);
    for (int i = 0; i < texts.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (texts[i]));
        b->getProperties().set ("mono", mono);
        b->onClick = [this, i] { attach.setValueAsCompleteGesture (values[i]); };
        addAndMakeVisible (b);
    }
    attach.sendInitialUpdate();
}

void Segmented::show (float v)
{
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (std::abs (values[i] - v) < 0.5f, dontSendNotification);
}

int Segmented::buttonWidth (int i) const
{
    const bool mono = buttons[i]->getProperties().getWithDefault ("mono", false);
    const auto f = mono ? Fonts::value (12.0f) : Fonts::title (11.0f);
    return jmax (26, roundToInt (GlyphArrangement::getStringWidth (f, buttons[i]->getButtonText())) + 18);
}

void Segmented::resized()
{
    if (columns > 0)
    {
        const int gap = 5, rows = (buttons.size() + columns - 1) / columns;
        const float bw = (float) (getWidth() - (columns - 1) * gap) / (float) columns, bh = (float) (getHeight() - (rows - 1) * gap) / (float) rows;
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (roundToInt ((float) (i % columns) * (bw + (float) gap)), roundToInt ((float) (i / columns) * (bh + (float) gap)), roundToInt (bw), roundToInt (bh));
        return;
    }
    int x = getWidth();
    for (int i = buttons.size(); --i >= 0;)
    {
        const int w = buttonWidth (i);
        x -= w; buttons[i]->setBounds (x, 0, w, getHeight()); x -= 3;
    }
}

void Segmented::paint (Graphics& g)
{
    if (label.isEmpty()) return;
    g.setColour (col::muted); g.setFont (Fonts::label (10.0f, 1.5f));
    g.drawText (label, 0, 0, getWidth(), getHeight(), Justification::centredLeft, false);
}

//==============================================================================
static void panel (Graphics& g, Rectangle<int> r)
{
    g.setColour (col::panel); g.fillRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f);
    g.setColour (col::border); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
}

void Section::paint (Graphics& g)
{
    panel (g, getLocalBounds());
    g.setColour (col::text); g.setFont (Fonts::title (12.0f, 2.5f));
    g.drawText (title, 16, 14, getWidth() - 32, 16, Justification::centredLeft, false);
    g.setColour (col::muted); g.setFont (Fonts::label (10.0f, 1.0f));
    g.drawText (note, 16, 14, getWidth() - 32, 16, Justification::centredRight, false);
}

void Section::resized()
{
    int y = 16 + 16 + 12;
    for (auto* s : switches) { s->setBounds (16, y, getWidth() - 32, 26); y += 26 + 12; }
    int x = 16;
    for (auto* k : knobs)
    {
        if (x + Knob::width > getWidth() - 16) { x = 16; y += Knob::height + 12; }
        k->setBounds (x, y, Knob::width, Knob::height);
        x += Knob::width + 6;
    }
}

//==============================================================================
void WaveDisplay::recompute()
{
    Engine e; e.prepare (44100.0, 512);
    e.params = proc.currentParams();
    const int lat = e.latency(), total = (int) (0.5 * 44100.0) + lat;
    std::vector<float> out ((size_t) total, 0.0f);
    e.trigger (1.0f);
    for (int pos = 0; pos < total; pos += 512) e.process (out.data() + pos, jmin (512, total - pos), 120.0);

    // the line: every 4th sample (enough to follow the start of the sweep), plus the pitch curve per point
    const int n = (total - lat) / 4;
    lo.assign ((size_t) n, 0.0f); pitch.assign ((size_t) n, 0.0f); hi.clear();
    const auto& p = e.params;
    for (int k = 0; k < n; ++k)
    {
        lo[(size_t) k] = out[(size_t) (lat + k * 4)];
        const double t = (double) k * 4.0 / 44100.0;
        pitch[(size_t) k] = p.pitchAmt > 0.01f ? (float) Shapes::pitch (t, p.sweepMs * 0.001, p.curve) : 0.0f;   // 1 = top of the sweep, 0 = the Key
    }
    keyText = String (noteName (p.key)) + String (p.octave) + " " + (p.fine >= 0 ? "+" : String::fromUTF8 ("\xe2\x88\x92")) + String (std::abs (p.fine), 0) + " ct";
    repaint();
}

void WaveDisplay::paint (Graphics& g)
{
    panel (g, getLocalBounds());
    g.setColour (col::text); g.setFont (Fonts::title (12.0f, 2.5f));
    g.drawText ("WAVEFORM", 16, 14, 200, 16, Justification::centredLeft, false);
    g.setColour (col::muted); g.setFont (Fonts::value (11.0f));
    g.drawText (String::fromUTF8 ("\xe2\x94\x81 amplitude     \xe2\x94\x85 pitch \xe2\x86\x92 ") + keyText, 16, 14, getWidth() - 32, 16, Justification::centredRight, false);

    const auto plot = Rectangle<float> (16.0f, 40.0f, (float) getWidth() - 32.0f, (float) getHeight() - 56.0f);
    g.setColour (col::plot); g.fillRoundedRectangle (plot, 6.0f);
    const float mid = plot.getCentreY(), keyY = plot.getBottom() - 12.0f, amp = plot.getHeight() * 0.41f;
    g.setColour (col::grid); g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
    { Path kl; kl.startNewSubPath (plot.getX(), keyY); kl.lineTo (plot.getRight(), keyY);
      Path d; const float dash[] { 3.0f, 5.0f }; PathStrokeType (1.0f).createDashedStroke (d, kl, dash, 2); g.setColour (col::keyLine); g.fillPath (d); }

    if (lo.empty()) return;
    const float xs = plot.getWidth() / (float) lo.size();
    Path w;
    for (size_t k = 0; k < lo.size(); ++k)
    {
        const float x = plot.getX() + (float) k * xs, y = mid - lo[k] * amp / 0.97f;
        if (k == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
    }
    g.setColour (col::accent); g.strokePath (w, PathStrokeType (1.5f, PathStrokeType::curved));

    Path pp;
    for (size_t c = 0; c < pitch.size(); ++c)
    {
        if (c % 4 != 0 && c != pitch.size() - 1) continue;
        const float x = plot.getX() + (float) c * xs, y = keyY - pitch[c] * (keyY - plot.getY() - 6.0f);
        if (c == 0) pp.startNewSubPath (x, y); else pp.lineTo (x, y);
    }
    Path d; const float dash[] { 5.0f, 4.0f }; PathStrokeType (1.4f).createDashedStroke (d, pp, dash, 2);
    g.setColour (col::pitch); g.fillPath (d);
}

//==============================================================================
TuningPanel::TuningPanel (MazoProcessor& p)
    : proc (p),
      octave (p.apvts, "octave", "", { "0", "1", "2" }, { 0, 1, 2 }, true),
      notes (p.apvts, "key", "", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, true),
      fine (p.apvts, "fine", "FINE", false, true)
{
    notes.columns = 6;
    addAndMakeVisible (octave); addAndMakeVisible (notes); addAndMakeVisible (fine);
}

void TuningPanel::resized()
{
    octave.setBounds (getWidth() - 16 - 86, 12, 86, 24);
    fine.setBounds (getWidth() - 16 - Knob::width, 46, Knob::width, 64);
    notes.setBounds (16, 118, getWidth() - 32, 62);
}

void TuningPanel::paint (Graphics& g)
{
    panel (g, getLocalBounds());
    g.setColour (col::text); g.setFont (Fonts::title (12.0f, 2.5f));
    g.drawText (String::fromUTF8 ("TUNING \xc2\xb7 FIXED KEY"), 16, 14, 260, 16, Justification::centredLeft, false);
    g.setColour (col::muted); g.setFont (Fonts::label (10.0f, 1.5f));
    g.drawText ("OCT", getWidth() - 16 - 86 - 34, 12, 30, 24, Justification::centredRight, false);

    const auto p = proc.currentParams();
    const String note = String (noteName (p.key)) + String (p.octave);
    g.setColour (col::accent); g.setFont (Fonts::valueBold (46.0f));
    const int nw = roundToInt (GlyphArrangement::getStringWidth (g.getCurrentFont(), note)) + 2;
    g.drawText (note, 16, 50, nw, 52, Justification::centredLeft, false);
    g.setColour (col::text); g.setFont (Fonts::value (16.0f));
    g.drawText (String (keyHz (p.key, p.octave, p.fine), 2) + " Hz", 16 + nw + 12, 58, 160, 20, Justification::centredLeft, false);
    g.setColour (col::muted); g.setFont (Fonts::value (12.0f));
    g.drawText ((p.fine >= 0 ? "+" : String::fromUTF8 ("\xe2\x88\x92")) + String (std::abs (p.fine), 1) + " ct", 16 + nw + 12, 80, 160, 16, Justification::centredLeft, false);
}

//==============================================================================
void Meter::set (float peak)
{
    level = jmax (peak, level * 0.86f);
    hold = jmax (peak, hold * 0.97f);
    repaint();
}

void Meter::paint (Graphics& g)
{
    g.setColour (col::muted); g.setFont (Fonts::label (11.0f, 2.0f));
    g.drawText ("OUT", 0, 0, 36, getHeight(), Justification::centredLeft, false);
    const float x = 48.0f, w = 140.0f;
    const float frac = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (level, -60.0f) + 48.0f) / 48.0f);
    for (int i = 0; i < 2; ++i)
    {
        const auto r = Rectangle<float> (x, (float) getHeight() * 0.5f - 9.0f + (float) i * 11.0f, w, 7.0f);
        g.setColour (col::knob); g.fillRoundedRectangle (r, 3.5f);
        g.setColour (col::accent); g.fillRoundedRectangle (r.withWidth (w * frac), 3.5f);
    }
    g.setColour (col::soft); g.setFont (Fonts::value (12.0f));
    const float db = Decibels::gainToDecibels (hold, -60.0f);
    g.drawText (db <= -59.0f ? String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e dB") : niceValue (String (db, 1)) + " dB", (int) (x + w + 10), 0, 70, getHeight(), Justification::centredLeft, false);
}

//==============================================================================
Header::Header (MazoProcessor& p) : presets (p.presets)
{
    for (auto* c : std::initializer_list<Component*> { &prev, &box, &next, &saveButton, &deleteButton, &lockKey, &meter }) addAndMakeVisible (c);
    prev.setButtonText ("<"); next.setButtonText (">");
    prev.getProperties().set ("mono", true); next.getProperties().set ("mono", true);
    for (auto* b : { &prev, &next, &saveButton, &deleteButton }) b->getProperties().set ("radius", 6.0);
    prev.setTooltip ("Previous preset"); next.setTooltip ("Next preset");
    lockKey.setTooltip ("On: loading a preset keeps your Key, Octave and Fine");
    box.setTextWhenNothingSelected ("Inicio");

    prev.onClick = [this] { presets.previous(); };
    next.onClick = [this] { presets.next(); };
    saveButton.onClick = [this] { askToSave(); };
    deleteButton.onClick = [this] { askToDelete(); };
    lockKey.onClick = [this] { presets.setLockKey (lockKey.getToggleState()); };
    box.onChange = [this]
    {
        const int id = box.getSelectedId();
        if (id <= 0) return;
        if (id >= userIdBase) presets.loadUser (presets.userNames()[id - userIdBase]);
        else presets.loadFactory (id - 1);
    };
    presets.onChange = [this] { refreshPresets(); };
    refreshPresets();
}

Header::~Header() { presets.onChange = nullptr; }

void Header::refreshPresets()
{
    box.clear (dontSendNotification);
    const auto f = presets.factoryNames(), st = presets.factoryStyles(), u = presets.userNames();
    box.addSectionHeading ("Factory");
    for (int i = 0; i < f.size(); ++i) box.addItem (f[i] + "   (" + st[i] + ")", i + 1);
    if (! u.isEmpty())
    {
        box.addSeparator(); box.addSectionHeading ("Yours");
        for (int i = 0; i < u.size(); ++i) box.addItem (u[i], userIdBase + i);
    }
    const auto name = presets.getCurrentName();
    const int id = presets.currentIsUser() ? (u.indexOf (name) >= 0 ? userIdBase + u.indexOf (name) : 0)
                                           : (f.indexOf (name) >= 0 ? f.indexOf (name) + 1 : 0);
    if (id > 0) box.setSelectedId (id, dontSendNotification);
    box.setText (name, dontSendNotification);   // just the name in the box; the style shows in the list
    deleteButton.setEnabled (presets.currentIsUser());
    lockKey.setToggleState (presets.getLockKey(), dontSendNotification);
}

void Header::askToSave()
{
    nameDialog = std::make_unique<AlertWindow> ("Save preset", "Name for your preset:", MessageBoxIconType::NoIcon, this);
    nameDialog->setLookAndFeel (&getLookAndFeel());
    nameDialog->addTextEditor ("name", presets.currentIsUser() ? presets.getCurrentName() : String(), {});
    nameDialog->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    nameDialog->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    nameDialog->enterModalState (true, ModalCallbackFunction::create ([this] (int result)
    {
        if (nameDialog == nullptr) return;
        const auto name = nameDialog->getTextEditorContents ("name");
        nameDialog->setVisible (false);
        if (result != 1) return;
        const auto r = presets.save (name, false);
        Component::SafePointer<Header> safe (this);
        if (r == PresetManager::SaveResult::exists)
            AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::QuestionIcon)
                                        .withTitle ("Overwrite?").withMessage ("\"" + name.trim() + "\" already exists. Replace it?")
                                        .withButton ("Replace").withButton ("Cancel").withAssociatedComponent (this),
                                    [safe, name] (int r2) { if (safe != nullptr && r2 == 1) safe->presets.save (name, true); });
        else if (r == PresetManager::SaveResult::factoryName)
            AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::InfoIcon)
                                        .withTitle ("Name taken").withMessage ("That's a factory preset name. Pick another one.")
                                        .withButton ("OK").withAssociatedComponent (this), nullptr);
        else if (r == PresetManager::SaveResult::failed)
            AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::WarningIcon)
                                        .withTitle ("Couldn't save").withMessage ("MAZO couldn't write to " + PresetManager::folder().getFullPathName())
                                        .withButton ("OK").withAssociatedComponent (this), nullptr);
    }), false);
}

void Header::askToDelete()
{
    if (! presets.currentIsUser()) return;
    const auto name = presets.getCurrentName();
    Component::SafePointer<Header> safe (this);
    AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::QuestionIcon)
                                .withTitle ("Delete preset?").withMessage ("Delete \"" + name + "\"? The sound stays loaded, only the file goes.")
                                .withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this),
                            [safe, name] (int r) { if (safe != nullptr && r == 1) safe->presets.remove (name); });
}

void Header::paint (Graphics& g)
{
    g.setColour (col::accent); g.setFont (Fonts::logo (34.0f, 6.0f));
    g.drawText ("MAZO", 4, 4, 150, 42, Justification::centredLeft, false);
    g.setColour (col::muted); g.setFont (Fonts::label (12.0f, 3.0f));
    g.drawText ("KICK SYNTH", 144, 16, 120, 28, Justification::centredLeft, false);
    g.setColour (col::border); g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void Header::resized()
{
    const int y = 10, h = 36;
    prev.setBounds (282, y, 36, h);
    box.setBounds (326, y, 290, h);
    next.setBounds (624, y, 36, h);
    saveButton.setBounds (676, y, 60, h);
    deleteButton.setBounds (742, y, 66, h);
    lockKey.setBounds (822, y, 110, h);
    meter.setBounds (getWidth() - 262, y, 262, h);
}

//==============================================================================
Main::Main (MazoProcessor& p) : header (p), proc (p), wave (p), tuning (p)
{
    auto& s = p.apvts;
    auto knob = [&s] (const char* id, const char* name) { return new Knob (s, id, name); };
    auto seg = [&s] (const char* id, const char* label, StringArray t) { Array<float> v; for (int i = 0; i < t.size(); ++i) v.add ((float) i); return new Segmented (s, id, label, t, v); };

    auto* tr = rowA.add (new Section ("TRANSIENT", "click layer"));
    tr->addSwitch (seg ("src", "SOURCE", { "Noise", "Click", "Pulse" }));
    tr->addKnob (knob ("click_decay", "DECAY")); tr->addKnob (knob ("click_tone", "TONE")); tr->addKnob (knob ("click_level", "LEVEL"));

    auto* bo = rowA.add (new Section ("BODY", "lands on key"));
    bo->addSwitch (seg ("wave", "WAVE", { "Sine", "Tri", "Soft" }));
    for (auto [id, n] : { std::pair { "pitch_amt", "PITCH AMT" }, { "sweep", "SWEEP" }, { "curve", "CURVE" }, { "attack", "ATTACK" },
                          { "hold", "HOLD" }, { "decay", "DECAY" }, { "body_level", "LEVEL" } })
        bo->addKnob (knob (id, n));

    auto* su = rowA.add (new Section ("SUB / TAIL", "locked to key"));
    su->addSwitch (seg ("sub_oct", "OCTAVE", { "Unison", String::fromUTF8 ("\xe2\x88\x92" "1 Oct") }));
    su->addSwitch (seg ("clean_sub", "CLEAN SUB", { "Off", "On" }));
    for (auto [id, n] : { std::pair { "sub_attack", "ATTACK" }, { "sub_decay", "DECAY" }, { "sub_level", "LEVEL" }, { "blend", "BLEND" } })
        su->addKnob (knob (id, n));

    auto* ru = rowA.add (new Section ("RUMBLE", "on sub / tail"));
    ru->addSwitch (seg ("rumble_duck_time", "DUCK TIME", { "1/16", "1/8", "1/4" }));
    for (auto [id, n] : { std::pair { "rumble_amount", "AMOUNT" }, { "rumble_decay", "DECAY" }, { "rumble_drive", "DRIVE" },
                          { "rumble_tone", "TONE" }, { "rumble_duck", "DUCK" }, { "rumble_mix", "MIX" } })
        ru->addKnob (knob (id, n));

    auto* wa = rowB.add (new Section ("WARMTH", ""));
    wa->addSwitch (seg ("warmth_type", "TYPE", { "Tape", "Tube" }));
    wa->addKnob (knob ("warmth", "AMOUNT"));

    auto* di = rowB.add (new Section ("DISTORTION", "parallel"));
    di->addSwitch (seg ("dist_mode", "MODE", { "Clip", "Fold", "Crush" }));
    for (auto [id, n] : { std::pair { "dist_drive", "DRIVE" }, { "dist_tone", "TONE" }, { "dist_mix", "MIX" } }) di->addKnob (knob (id, n));

    auto* co = rowB.add (new Section ("COMPRESSION", ""));
    for (auto [id, n] : { std::pair { "comp_attack", "ATTACK" }, { "comp_release", "RELEASE" }, { "comp_amount", "AMOUNT" }, { "comp_mix", "MIX" } })
        co->addKnob (knob (id, n));

    auto* ou = rowB.add (new Section ("OUTPUT", "limiter last"));
    ou->addSwitch (seg ("velocity", "VELOCITY", { "Off", "On" }));
    for (auto [id, n] : { std::pair { "low_cut", "LOW CUT" }, { "tilt", "TILT" }, { "ceiling", "CEILING" }, { "gain", "GAIN" } })
        ou->addKnob (knob (id, n));

    addAndMakeVisible (header); addAndMakeVisible (wave); addAndMakeVisible (tuning);
    for (auto* x : rowA) addAndMakeVisible (x);
    for (auto* x : rowB) addAndMakeVisible (x);
    for (const auto& b : MazoProcessor::getBindings()) p.apvts.addParameterListener (b.id, this);
    setSize (W, H);
    wave.recompute();
    startTimerHz (30);
}

Main::~Main()
{
    stopTimer();
    for (const auto& b : MazoProcessor::getBindings()) proc.apvts.removeParameterListener (b.id, this);
}

void Main::timerCallback()
{
    header.meter.set (proc.outPeak.exchange (0.0f));
    if (dirty.exchange (false)) { wave.recompute(); tuning.refresh(); }
}

void Main::paint (Graphics& g)
{
    g.fillAll (col::bg);
    g.setFont (Fonts::value (10.0f));
    g.setColour (col::muted); g.drawText ("FX CHAIN", 24, 596, 70, 14, Justification::centredLeft, false);
    g.setColour (col::soft);
    g.drawText (String::fromUTF8 ("TRANSIENT + BODY + SUB\xe2\x86\x92RUMBLE  \xe2\x86\x92  WARMTH  \xe2\x86\x92  DISTORTION  \xe2\x86\x92  COMPRESSION  \xe2\x86\x92  LIMITER"),
                96, 596, 800, 14, Justification::centredLeft, false);
}

static void layoutRow (OwnedArray<Section>& row, std::initializer_list<int> widths, int y, int h)
{
    int x = 24, i = 0;
    for (int w : widths) { row[i++]->setBounds (x, y, w, h); x += w + 16; }
}

void Main::resized()
{
    header.setBounds (24, 24, 1232, 56);
    wave.setBounds (24, 96, 820, 200);
    tuning.setBounds (860, 96, 396, 200);
    layoutRow (rowA, { 241, 334, 304, 305 }, 312, 268);   // widths from the approved mockup
    layoutRow (rowB, { 167, 295, 361, 361 }, 618, 200);
}
} // namespace mazo::ui

//==============================================================================
MazoEditor::MazoEditor (MazoProcessor& p) : AudioProcessorEditor (p), main (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (main);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) mazo::ui::Main::W / mazo::ui::Main::H);
    setResizeLimits (768, 505, 1600, 1053);
    setSize (1024, 674);
}

MazoEditor::~MazoEditor() { setLookAndFeel (nullptr); }

void MazoEditor::resized()
{
    main.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) mazo::ui::Main::W));
}
