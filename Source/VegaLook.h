#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "MixerEngine.h"

namespace VC   // Vega colours (from the Vega Productions DAW mixer design)
{
    const juce::Colour bg       { 0xff242424 };
    const juce::Colour panel    { 0xff2b2b2b };
    const juce::Colour strip    { 0xff2f2f2f };
    const juce::Colour field    { 0xff1d1d1d };
    const juce::Colour line     { 0xff3b3b3b };
    const juce::Colour topBar   { 0xff1f1f1f };
    const juce::Colour text     { 0xffd6d6d6 };
    const juce::Colour label    { 0xff8c8c8c };
    const juce::Colour dim      { 0xff5c5c5c };
    const juce::Colour cyan     { 0xff5fe3e0 };
    const juce::Colour red      { 0xffff5a5a };
    const juce::Colour white    { 0xffeeeeee };
    const juce::Colour knob     { 0xff151515 };

    inline juce::Colour stripColour (int i)
    {
        static const juce::Colour pal[] { juce::Colour (0xffff5a5a), juce::Colour (0xffff8c3c), juce::Colour (0xfff2c14e),
                                          juce::Colour (0xff5fd38a), juce::Colour (0xff5fe3e0), juce::Colour (0xff4d8dff),
                                          juce::Colour (0xffa66bff), juce::Colour (0xffff6bc1) };
        return pal[juce::jlimit (0, 7, i)];
    }
    inline juce::StringArray colourNames() { return { "Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink" }; }
}

struct VegaFonts
{
    static juce::Typeface::Ptr sans()
    {
        static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
        return t;
    }
    static juce::Typeface::Ptr monoFace()
    {
        static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
        return t;
    }
    static juce::Font label (float h) { return juce::Font (juce::FontOptions (sans()).withHeight (h)); }
    static juce::Font mono (float h)  { return juce::Font (juce::FontOptions (monoFace()).withHeight (h)); }
};

//==============================================================================
class VegaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VegaLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, VC::bg);
        setColour (juce::ComboBox::backgroundColourId, VC::field);
        setColour (juce::ComboBox::outlineColourId, VC::line);
        setColour (juce::ComboBox::textColourId, VC::text);
        setColour (juce::ComboBox::arrowColourId, VC::text);
        setColour (juce::PopupMenu::backgroundColourId, VC::field);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, VC::line);
        setColour (juce::PopupMenu::highlightedTextColourId, VC::cyan);
        setColour (juce::PopupMenu::textColourId, VC::text);
        setColour (juce::PopupMenu::headerTextColourId, VC::label);
        setColour (juce::Label::textColourId, VC::text);
        setColour (juce::TextButton::buttonColourId, VC::line);
        setColour (juce::TextButton::textColourOffId, VC::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::BubbleComponent::backgroundColourId, VC::field);
        setColour (juce::BubbleComponent::outlineColourId, VC::cyan);
        setColour (juce::TooltipWindow::backgroundColourId, VC::field);
        setColour (juce::TooltipWindow::textColourId, VC::text);
        setColour (juce::TooltipWindow::outlineColourId, VC::line);
        setColour (juce::ToggleButton::textColourId, VC::text);
        setColour (juce::ToggleButton::tickColourId, VC::cyan);
        setColour (juce::ToggleButton::tickDisabledColourId, VC::label);
        setColour (juce::AlertWindow::backgroundColourId, VC::panel);
        setColour (juce::AlertWindow::textColourId, VC::text);
        setColour (juce::AlertWindow::outlineColourId, VC::line);
        setColour (juce::ScrollBar::thumbColourId, VC::line.brighter (0.2f));
        setColour (juce::TextEditor::backgroundColourId, VC::field);
        setColour (juce::TextEditor::textColourId, VC::text);
        setColour (juce::TextEditor::highlightColourId, VC::cyan.withAlpha (0.35f));
        setColour (juce::TextEditor::outlineColourId, VC::line);
        setColour (juce::TextEditor::focusedOutlineColourId, VC::cyan);
        setColour (juce::CaretComponent::caretColourId, VC::cyan);
        setColour (juce::ListBox::backgroundColourId, VC::field);
        setColour (juce::ProgressBar::foregroundColourId, VC::cyan);
        setColour (juce::ProgressBar::backgroundColourId, VC::field);
        setColour (juce::Slider::textBoxTextColourId, VC::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setDefaultSansSerifTypeface (VegaFonts::sans());
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int h) override
    {
        if (b.getProperties().contains ("monoText")) return VegaFonts::mono (juce::jmin (13.0f, (float) h * 0.62f));
        return VegaFonts::label (juce::jmin (13.0f, (float) h * 0.68f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override       { return VegaFonts::label (11.5f); }
    juce::Font getPopupMenuFont() override                     { return VegaFonts::label (15.0f); }
    juce::Font getSliderPopupFont (juce::Slider&) override      { return VegaFonts::mono (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override           { return l.getFont(); }
    juce::Font getAlertWindowMessageFont() override             { return VegaFonts::label (15.0f); }
    juce::Font getAlertWindowTitleFont() override               { return VegaFonts::label (18.0f); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (VC::field);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (box.hasKeyboardFocus (false) ? VC::cyan.withAlpha (0.5f) : VC::line.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
        // up chevron
        const float cx = (float) w - 9.0f, cy = (float) h * 0.5f;
        juce::Path p;
        p.startNewSubPath (cx - 4.0f, cy + 2.0f); p.lineTo (cx, cy - 2.0f); p.lineTo (cx + 4.0f, cy + 2.0f);
        g.setColour (box.isEnabled() ? VC::text : VC::dim);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (2, 0, box.getWidth() - 18, box.getHeight());
        l.setFont (getComboBoxFont (box));
        l.setJustificationType (juce::Justification::centred);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto& props = b.getProperties();
        const auto led = props.contains ("led") ? juce::Colour::fromString (props["led"].toString()) : VC::cyan;

        if (props.contains ("tab"))                          // top-bar tabs (ARRANGE / SESSION / MIXER style)
        {
            g.setColour (on ? (props.contains ("led") ? led : VC::white) : VC::line.brighter (over ? 0.15f : 0.0f));
            g.fillRoundedRectangle (r, 2.0f);
            return;
        }
        if (props.contains ("pad"))                          // square Rec / S / M pads
        {
            if (on)
            {
                g.setColour (led.withAlpha (0.35f));
                g.fillRoundedRectangle (r.expanded (2.0f), 4.0f);
                g.setColour (led);
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.drawRoundedRectangle (r.reduced (2.5f), 2.0f, 1.0f);
            }
            else
            {
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (VC::line.brighter (over ? 0.3f : 0.1f));
                g.drawRoundedRectangle (r.reduced (2.5f), 2.0f, 1.2f);
            }
            return;
        }
        if (props.contains ("slot"))                         // insert slots
        {
            g.setColour (VC::field.brighter (over ? 0.06f : 0.0f));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (VC::line.withAlpha (down ? 1.0f : 0.5f));
            g.drawRoundedRectangle (r, 2.0f, 1.0f);
            return;
        }
        // default flat button
        g.setColour (on ? led : VC::line.brighter (over ? 0.15f : 0.0f));
        g.fillRoundedRectangle (r, 2.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto& props = b.getProperties();
        if (props.contains ("pad")) return;
        juce::Colour c = b.getToggleState() ? juce::Colours::black : VC::text;
        if (props.contains ("slot"))
            c = props.contains ("bypassed") ? VC::dim : (b.getButtonText() == "+" ? VC::dim : VC::cyan);
        if (props.contains ("textColour")) c = juce::Colour::fromString (props["textColour"].toString());
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.isEnabled() ? c : c.withAlpha (0.4f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float dia = juce::jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float rad = dia * 0.5f;
        const float ang = a0 + pos * (a1 - a0);

        // dotted ring
        const int dots = dia > 36.0f ? 21 : 11;
        g.setColour (VC::label.withAlpha (0.85f));
        for (int i = 0; i < dots; ++i)
        {
            const auto p = c.getPointOnCircumference (rad - 1.2f, a0 + (a1 - a0) * (float) i / (float) (dots - 1));
            g.fillEllipse (p.x - 0.8f, p.y - 0.8f, 1.6f, 1.6f);
        }

        const float kr = rad - (dia > 36.0f ? 5.0f : 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - kr + 0.5f, c.y - kr + 1.5f, kr * 2.0f, kr * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), c.x, c.y - kr, juce::Colour (0xff0e0e0e), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

        g.setColour (s.isEnabled() ? VC::white : VC::dim);
        g.drawLine ({ c.getPointOnCircumference (kr * 0.25f, ang), c.getPointOnCircumference (kr * 0.92f, ang) }, dia > 36.0f ? 2.6f : 1.8f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        if (style == juce::Slider::LinearBar || style == juce::Slider::LinearHorizontal)   // send level bar
        {
            auto bar = r.withSizeKeepingCentre (r.getWidth(), 4.0f);
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (s.isEnabled() ? VC::cyan.withAlpha (0.8f) : VC::dim);
            g.fillRoundedRectangle (bar.withRight (sliderPos), 2.0f);
            return;
        }

        const float cx = r.getCentreX() + 2.0f;
        g.setColour (VC::dim);
        for (float db : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -20.0f, -30.0f, -40.0f, -50.0f, -60.0f })
        {
            const float py = (float) s.getPositionOfValue (db);
            g.drawLine (cx - 22.0f, py, cx - 16.0f, py, 1.0f);
        }
        auto slot = juce::Rectangle<float> (cx - 2.0f, r.getY() + 4.0f, 4.0f, r.getHeight() - 8.0f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (slot, 2.0f);

        auto capR = juce::Rectangle<float> (30.0f, 16.0f).withCentre ({ cx, sliderPos });
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (capR.translated (0.0f, 2.0f), 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4a), capR.getX(), capR.getY(), juce::Colour (0xff222222), capR.getX(), capR.getBottom(), false));
        g.fillRoundedRectangle (capR, 2.0f);
        const auto lineCol = s.getProperties().contains ("cap") ? juce::Colour::fromString (s.getProperties()["cap"].toString()) : VC::white;
        g.setColour (lineCol);
        g.fillRect (capR.getX() + 5.0f, capR.getCentreY() - 0.75f, capR.getWidth() - 10.0f, 1.5f);
    }

    void drawCallOutBoxBackground (juce::CallOutBox& box, juce::Graphics& g, const juce::Path& path, juce::Image&) override
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillPath (path, juce::AffineTransform::translation (0.0f, 3.0f));
        g.setColour (VC::panel);
        g.fillPath (path);
        g.setColour (VC::line.brighter (0.2f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
        juce::ignoreUnused (box);
    }
    int getCallOutBoxBorderSize (const juce::CallOutBox&) override { return 14; }
};

//==============================================================================
// Slider bound to a Param.
class ParamSlider : public juce::Slider
{
public:
    enum Kind { knobKind, faderKind, barKind };

    ParamSlider (Param& p, Kind kind = knobKind, bool showValue = false) : param (p)
    {
        setSliderStyle (kind == faderKind ? juce::Slider::LinearVertical
                        : kind == barKind  ? juce::Slider::LinearHorizontal
                                           : juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (showValue ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, true, 70, 14);
        setRange (p.minV, p.maxV, p.unit == Param::Unit::choice ? 1.0 : 0.0);
        if (p.skewMid > p.minV && p.skewMid < p.maxV) setSkewFactorFromMidPoint (p.skewMid);
        setDoubleClickReturnValue (true, p.def);
        setValue (p.get(), juce::dontSendNotification);
        setPopupDisplayEnabled (! showValue, true, nullptr, 1000);
        setMouseDragSensitivity (200);
        setTooltip (p.name);
        onValueChange = [this] { param.set ((float) getValue()); if (onParamChange) onParamChange(); };
    }

    juce::String getTextFromValue (double v) override { return isPopupDisplay() ? param.name + "  " + param.toText ((float) v) : param.toText ((float) v); }
    void refresh() { setValue (param.get(), juce::dontSendNotification); }

    Param& param;
    std::function<void()> onParamChange;

private:
    bool isPopupDisplay() const { return getTextBoxPosition() == juce::Slider::NoTextBox; }
};

class ParamButton : public juce::TextButton
{
public:
    ParamButton (Param& p, const juce::String& label, juce::Colour led, bool pad = false) : juce::TextButton (label), param (p)
    {
        setClickingTogglesState (true);
        getProperties().set ("led", led.toString());
        if (pad) getProperties().set ("pad", true);
        setToggleState (p.on(), juce::dontSendNotification);
        setTooltip (p.name);
        onClick = [this] { param.set (getToggleState() ? 1.0f : 0.0f); if (onParamChange) onParamChange(); };
    }
    void refresh() { setToggleState (param.on(), juce::dontSendNotification); }
    Param& param;
    std::function<void()> onParamChange;
};

// Small grey section caption ("INPUT", "CONSOLE", ...)
inline void drawCaption (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, juce::Colour c = VC::label)
{
    g.setColour (c);
    g.setFont (VegaFonts::label (11.0f));
    g.drawText (t.toUpperCase(), r, juce::Justification::centred);
}

//==============================================================================
// Analog console VU (-20 .. +3) with a cyan needle.
class VUMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VUMeter (std::atomic<float>& src) : value (src) {}
    std::function<void()> onClick;

    void update()
    {
        const float lvl = value.exchange (0.0f);
        const float vu = juce::Decibels::gainToDecibels (lvl, -60.0f) + 18.0f;   // -18 dBFS RMS = 0 VU
        target = juce::jlimit (-22.0f, 4.0f, vu);
        const float nd = needle + (target - needle) * 0.18f;   // ~300 ms ballistics at 30 fps
        if (std::abs (nd - needle) > 0.02f) { needle = nd; repaint(); }
    }

    void mouseUp (const juce::MouseEvent& e) override { if (onClick && e.mouseWasClicked()) onClick(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (VC::field);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (VC::line.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);

        const auto pivot = juce::Point<float> (r.getCentreX(), r.getBottom() + r.getHeight() * 0.55f);
        const float rad = r.getHeight() * 1.35f;
        auto ang = [] (float vu) { return juce::jmap (juce::jlimit (-20.0f, 3.0f, vu), -20.0f, 3.0f, -0.62f, 0.62f); };

        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, ang (-20.0f), ang (0.0f), true);
        g.setColour (VC::label);
        g.strokePath (arc, juce::PathStrokeType (1.0f));
        juce::Path redArc;
        redArc.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, ang (0.0f), ang (3.0f), true);
        g.setColour (VC::red);
        g.strokePath (redArc, juce::PathStrokeType (2.0f));

        for (float t : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
        {
            g.setColour (t > 0.0f ? VC::red : VC::label);
            g.drawLine ({ pivot.getPointOnCircumference (rad, ang (t)), pivot.getPointOnCircumference (rad - 3.0f, ang (t)) }, 1.0f);
        }
        g.setFont (VegaFonts::label (9.0f));
        g.setColour (VC::text);
        g.drawText ("-20", juce::Rectangle<float> (r.getX() + 4.0f, r.getY() + 3.0f, 20.0f, 10.0f), juce::Justification::left);
        g.setColour (VC::red);
        g.drawText ("+3", juce::Rectangle<float> (r.getRight() - 22.0f, r.getY() + 3.0f, 18.0f, 10.0f), juce::Justification::right);
        g.setColour (VC::label);
        g.drawText ("VU", juce::Rectangle<float> (r.getCentreX() - 10.0f, r.getBottom() - 11.0f, 20.0f, 10.0f), juce::Justification::centred);

        g.saveState();
        g.reduceClipRegion (r.reduced (1.0f).toNearestInt());
        g.setColour (VC::cyan);
        g.drawLine ({ pivot.getPointOnCircumference (rad * 0.62f, ang (needle)), pivot.getPointOnCircumference (rad + 2.0f, ang (needle)) }, 1.6f);
        g.restoreState();
    }

private:
    std::atomic<float>& value;
    float target = -22.0f, needle = -22.0f;
};

//==============================================================================
// Segmented meter: cyan, white near the top, red clip.
class LedMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LedMeter (MeterState& m, bool stereoBars = false) : state (m), stereo (stereoBars) {}

    void update()
    {
        auto step = [] (float& disp, float in)
        {
            const float db = juce::Decibels::gainToDecibels (in, -60.0f);
            disp = db > disp ? db : juce::jmax (-60.0f, disp - 1.0f);
        };
        step (dispL, state.l.exchange (0.0f));
        step (dispR, state.r.exchange (0.0f));
        const bool live = dispL > -59.0f || dispR > -59.0f;
        if (live || wasLive) repaint();
        wasLive = live;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (stereo)
        {
            const float bw = (r.getWidth() - 3.0f) * 0.5f;
            drawBar (g, r.withWidth (bw), dispL);
            drawBar (g, r.withTrimmedLeft (bw + 3.0f), dispR);
        }
        else drawBar (g, r, juce::jmax (dispL, dispR));
    }

private:
    static float norm (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); }

    void drawBar (juce::Graphics& g, juce::Rectangle<float> r, float db)
    {
        const int segs = juce::jmax (10, (int) (r.getHeight() / 6.0f));
        const float sh = r.getHeight() / (float) segs;
        const int lit = (int) std::round (norm (db) * (float) segs);
        for (int i = 0; i < segs; ++i)
        {
            const float frac = (float) (i + 1) / (float) segs;
            auto seg = juce::Rectangle<float> (r.getX(), r.getBottom() - (float) (i + 1) * sh + 1.0f, r.getWidth(), sh - 2.0f);
            juce::Colour c = frac > norm (0.0f) ? VC::red : (frac > norm (-12.0f) ? VC::white : VC::cyan);
            g.setColour (i < lit ? c : juce::Colour (0xff383838));
            g.fillRect (seg);
        }
    }

    MeterState& state;
    bool stereo;
    float dispL = -60, dispR = -60;
    bool wasLive = true;
};

// Gain-reduction readout used inside the dynamics editors.
class GRBar : public juce::Component
{
public:
    explicit GRBar (std::atomic<float>& src) : value (src) {}
    void update()
    {
        const float v = value.exchange (0.0f);
        const float nd = v < disp ? v : juce::jmin (0.0f, disp + 0.5f);
        if (std::abs (nd - disp) > 0.01f) { disp = nd; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto bar = r.removeFromBottom (6.0f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (VC::cyan);
        g.fillRoundedRectangle (bar.removeFromRight (bar.getWidth() * juce::jlimit (0.0f, 1.0f, -disp / 20.0f)), 2.0f);
        g.setColour (VC::label);
        g.setFont (VegaFonts::mono (11.0f));
        g.drawText ("GR " + juce::String (disp, 1) + " dB", r, juce::Justification::centredLeft);
    }
private:
    std::atomic<float>& value;
    float disp = 0.0f;
};

// Fader value readout ("-5.2 dB"); double-click to type a level.
class DbReadout : public juce::Label
{
public:
    explicit DbReadout (Param& p) : param (p)
    {
        setFont (VegaFonts::mono (13.0f));
        setJustificationType (juce::Justification::centred);
        setColour (juce::Label::backgroundColourId, VC::field);
        setColour (juce::Label::outlineColourId, VC::line.withAlpha (0.6f));
        setColour (juce::Label::textColourId, VC::text);
        setEditable (false, true);
        setTooltip ("Double-click to type a level");
        onTextChange = [this]
        {
            const auto t = getText().trim().toLowerCase();
            param.set (t.startsWith ("-inf") ? param.minV : t.getFloatValue());
            if (onEdited) onEdited();
            refresh();
        };
        refresh();
    }
    void refresh()
    {
        const float v = param.get();
        if (std::abs (v - shown) > 0.01f || getText().isEmpty())
        {
            shown = v;
            setText (v <= param.minV + 0.05f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB", juce::dontSendNotification);
        }
    }
    std::function<void()> onEdited;
private:
    Param& param;
    float shown = 1.0e9f;
};
