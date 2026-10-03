// MAZO - look: colours, fonts and drawing, taken from the approved mockup
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace mazo::ui
{
namespace col
{
    const juce::Colour bg        { 0xff151311 }, panel { 0xff1e1b18 }, border { 0xff332e29 }, knob { 0xff2a2622 },
                       knobEdge  { 0xff3d3731 }, button { 0xff26221e }, text { 0xffece6dc }, muted { 0xffa39a8d },
                       soft      { 0xffcfc6b8 }, accent { 0xffe8913a }, onAccent { 0xff1a1612 }, plot { 0xff171512 },
                       grid      { 0xff2c2824 }, pitch { 0xffe9dcc6 }, keyLine { 0xff6b5a45 };
}

// Archivo for text, JetBrains Mono for values (both SIL Open Font License, see resources/fonts)
struct Fonts
{
    juce::Typeface::Ptr regular, semibold, extrabold, mono, monoSemi;
    static Fonts& get()
    {
        static Fonts f;
        return f;
    }
    static juce::Font make (const juce::Typeface::Ptr& t, float height, float tracking = 0.0f)
    {
        auto o = t != nullptr ? juce::FontOptions (t) : juce::FontOptions();
        return juce::Font (o.withHeight (height).withKerningFactor (tracking / height));
    }
    // sizes are in mockup pixels; tracking is letter spacing in pixels
    static juce::Font label (float h, float tracking = 0.0f)  { return make (get().regular, h, tracking); }
    static juce::Font title (float h, float tracking = 0.0f)  { return make (get().semibold, h, tracking); }
    static juce::Font logo  (float h, float tracking = 0.0f)  { return make (get().extrabold, h, tracking); }
    static juce::Font value (float h)                         { return make (get().mono, h); }
    static juce::Font valueBold (float h)                     { return make (get().monoSemi, h); }

private:
    Fonts()
    {
        auto load = [] (const void* d, int n) { return juce::Typeface::createSystemTypefaceFor (d, (size_t) n); };
        regular   = load (BinaryData::ArchivoRegular_ttf, BinaryData::ArchivoRegular_ttfSize);
        semibold  = load (BinaryData::ArchivoSemiBold_ttf, BinaryData::ArchivoSemiBold_ttfSize);
        extrabold = load (BinaryData::ArchivoExtraBold_ttf, BinaryData::ArchivoExtraBold_ttfSize);
        mono      = load (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
        monoSemi  = load (BinaryData::JetBrainsMonoSemiBold_ttf, BinaryData::JetBrainsMonoSemiBold_ttfSize);
    }
};

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        using namespace juce;
        setDefaultSansSerifTypeface (Fonts::get().regular);
        setColour (ResizableWindow::backgroundColourId, col::bg);
        setColour (ComboBox::backgroundColourId, col::panel);
        setColour (ComboBox::outlineColourId, col::knobEdge);
        setColour (ComboBox::textColourId, col::text);
        setColour (ComboBox::arrowColourId, col::soft);
        setColour (PopupMenu::backgroundColourId, col::panel);
        setColour (PopupMenu::textColourId, col::text);
        setColour (PopupMenu::headerTextColourId, col::accent);
        setColour (PopupMenu::highlightedBackgroundColourId, col::accent);
        setColour (PopupMenu::highlightedTextColourId, col::onAccent);
        setColour (TextButton::buttonColourId, col::button);
        setColour (TextButton::textColourOffId, col::soft);
        setColour (TextButton::textColourOnId, col::onAccent);
        setColour (ToggleButton::textColourId, col::soft);
        setColour (ToggleButton::tickColourId, col::onAccent);
        setColour (AlertWindow::backgroundColourId, col::panel);
        setColour (AlertWindow::textColourId, col::text);
        setColour (AlertWindow::outlineColourId, col::border);
        setColour (TextEditor::backgroundColourId, col::button);
        setColour (TextEditor::textColourId, col::text);
        setColour (TextEditor::outlineColourId, col::knobEdge);
        setColour (TextEditor::focusedOutlineColourId, col::accent);
        setColour (TooltipWindow::backgroundColourId, col::panel);
        setColour (TooltipWindow::textColourId, col::text);
        setColour (TooltipWindow::outlineColourId, col::border);
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int) override
    {
        const bool mono = b.getProperties().getWithDefault ("mono", false);
        const bool on = b.getToggleState();
        return mono ? (on ? Fonts::valueBold (12.0f) : Fonts::value (12.0f)) : (on ? Fonts::title (11.0f) : Fonts::label (11.0f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::title (14.0f); }
    juce::Font getPopupMenuFont() override { return Fonts::label (14.0f); }
    juce::Font getAlertWindowMessageFont() override { return Fonts::label (14.0f); }
    juce::Font getAlertWindowTitleFont() override { return Fonts::title (16.0f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const float rad = (float) b.getProperties().getWithDefault ("radius", 4.0);
        const bool on = b.getToggleState();
        auto fill = on ? col::accent : col::button;
        if (! on && over) fill = fill.brighter (0.08f);
        if (down) fill = fill.darker (0.1f);
        g.setColour (fill); g.fillRoundedRectangle (r, rad);
        g.setColour (on ? col::accent : col::knobEdge); g.drawRoundedRectangle (r, rad, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? col::onAccent : (b.isEnabled() ? col::soft : col::muted.withAlpha (0.5f)));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
    {
        const float d = (float) juce::jmin (w, h);
        const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const bool ring = s.getProperties().getWithDefault ("accentRing", false);
        const auto circle = juce::Rectangle<float> (d, d).withCentre (c).reduced (1.0f);
        g.setColour (col::knob); g.fillEllipse (circle);
        g.setColour (ring ? col::accent : col::knobEdge); g.drawEllipse (circle, 2.0f);

        const float a = start + pos * (end - start);
        // faint value arc just inside the edge
        juce::Path arc; arc.addCentredArc (c.x, c.y, d * 0.5f - 5.0f, d * 0.5f - 5.0f, 0.0f, start, a, true);
        g.setColour (col::accent.withAlpha (0.28f)); g.strokePath (arc, juce::PathStrokeType (2.0f));
        // the indicator, like the mockup
        const float r1 = d * 0.5f - 4.0f, r0 = r1 - d * 0.34f;
        juce::Line<float> line (c.getPointOnCircumference (r0, a), c.getPointOnCircumference (r1, a));
        g.setColour (col::accent); g.drawLine (line, 2.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::panel); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.hasKeyboardFocus (false) ? col::accent : col::knobEdge); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (col::muted); g.setFont (Fonts::label (11.0f, 2.0f));
        g.drawText ("PRESET", 14, 0, 70, h, juce::Justification::centredLeft);
        juce::Path arrow; const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
        arrow.startNewSubPath (ax - 5, ay - 2.5f); arrow.lineTo (ax, ay + 2.5f); arrow.lineTo (ax + 5, ay - 2.5f);
        g.setColour (col::soft); g.strokePath (arrow, juce::PathStrokeType (1.6f));
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (84, 0, box.getWidth() - 110, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredRight);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        const float s = 16.0f; const auto box = juce::Rectangle<float> (2.0f, ((float) b.getHeight() - s) * 0.5f, s, s);
        g.setColour (b.getToggleState() ? col::accent : (over ? col::button.brighter (0.1f) : col::button)); g.fillRoundedRectangle (box, 3.0f);
        g.setColour (b.getToggleState() ? col::accent : col::knobEdge); g.drawRoundedRectangle (box, 3.0f, 1.0f);
        if (b.getToggleState())
        {
            juce::Path tick; tick.startNewSubPath (box.getX() + 4, box.getCentreY()); tick.lineTo (box.getX() + 7, box.getBottom() - 4); tick.lineTo (box.getRight() - 3.5f, box.getY() + 4);
            g.setColour (col::onAccent); g.strokePath (tick, juce::PathStrokeType (2.0f));
        }
        g.setColour (col::soft); g.setFont (Fonts::label (11.0f, 1.5f));
        g.drawText (b.getButtonText().toUpperCase(), (int) box.getRight() + 7, 0, b.getWidth() - (int) box.getRight() - 7, b.getHeight(), juce::Justification::centredLeft);
    }
};
} // namespace mazo::ui
