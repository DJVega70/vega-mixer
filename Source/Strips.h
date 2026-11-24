#pragma once
#include "Editors.h"

//==============================================================================
// ComboBox bound to a choice-style Param (item id = value + 1).
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (Param& p, const juce::StringArray& items) : param (p)
    {
        setItems (items);
        onChange = [this] { if (getSelectedId() > 0) { param.set ((float) (getSelectedId() - 1)); if (onParamChange) onParamChange(); } };
    }
    void setItems (const juce::StringArray& items)
    {
        clear (juce::dontSendNotification);
        for (int i = 0; i < items.size(); ++i) addItem (items[i], i + 1);
        refresh();
    }
    void refresh()
    {
        const int id = param.index() + 1;
        setSelectedId (indexOfItemId (id) >= 0 ? id : 1, juce::dontSendNotification);
    }
    Param& param;
    std::function<void()> onParamChange;
};

class SlotButton : public juce::TextButton
{
public:
    SlotButton()
    {
        getProperties().set ("slot", true);
        getProperties().set ("monoText", true);
    }
    std::function<void (bool popup)> onSlotClick;
    void clicked (const juce::ModifierKeys& m) override { if (onSlotClick) onSlotClick (m.isPopupMenu()); }
};

inline juce::StringArray outputPairNames (int numOuts, int firstPair)
{
    juce::StringArray s;
    for (int i = firstPair; 2 * i + 1 < juce::jmax (2, numOuts); ++i)
        s.add ("Out " + juce::String (2 * i + 1) + "-" + juce::String (2 * i + 2));
    return s;
}

inline void launchEditor (std::unique_ptr<juce::Component> editor, juce::Component& target)
{
    juce::CallOutBox::launchAsynchronously (std::move (editor), target.getScreenBounds(), nullptr);
}

//==============================================================================
class ChannelStrip : public juce::Component
{
public:
    static constexpr int W = 96, H = 856;

    ChannelStrip (MixerEngine& e, int idx)
        : engine (e), index (idx), p (e.ch[(size_t) idx]), fx (MixerEngine::isFxStrip (idx)),
          vu (e.meters[(size_t) idx].vu), meter (e.meters[(size_t) idx]),
          hi (*p.conHi), mid (*p.conMid), lo (*p.conLo),
          tape (*p.tape, { "Off", "15 ips", "30 ips" }),
          send (*p.sendDest, { "None", "Reverb", "Delay" }),
          sendLevel (*p.sendLevel, ParamSlider::barKind),
          output (*p.outDest, { "Master" }),
          recMode (*p.recDry, p.recDry->choices),
          pan (*p.pan),
          rec (*p.recArm, {}, VC::red, true), solo (*p.solo, {}, VC::cyan, true), mute (*p.mute, {}, VC::stripColour (1), true),
          fader (*p.fader, ParamSlider::faderKind), readout (*p.fader)
    {
        if (index == kReverbStrip)     fxCombo = std::make_unique<ParamCombo> (*e.fx.revType, juce::StringArray { "Reverb: Room", "Reverb: Hall", "Reverb: Plate" });
        else if (index == kDelayStrip) fxCombo = std::make_unique<ParamCombo> (*e.fx.dlyDiv, juce::StringArray { "Delay 1/4", "Delay 1/8", "Delay 1/8 D", "Delay 1/16", "Delay 1/4 T", "Delay 1/2" });

        if (fxCombo)
        {
            addAndMakeVisible (*fxCombo);
            if (index == kReverbStrip)
                fxCombo->onParamChange = [this]
                {
                    static const float pr[3][3] { { 0.35f, 0.55f, 0.8f }, { 0.82f, 0.35f, 1.0f }, { 0.62f, 0.12f, 1.0f } };
                    const int t = engine.fx.revType->index();
                    engine.fx.revSize->set (pr[t][0]); engine.fx.revDamp->set (pr[t][1]); engine.fx.revWidth->set (pr[t][2]);
                };
        }
        else
        {
            addAndMakeVisible (source);
            source.onChange = [this] { sourceChanged(); };
        }

        vu.onClick = [this] { openInputEditor(); };
        vu.setTooltip (fx ? "Click for " + juce::String (index == kReverbStrip ? "reverb" : "delay") + " settings" : "Click for trim, phase and low/high cut");
        addAndMakeVisible (vu);
        for (auto* k : { &hi, &mid, &lo }) addAndMakeVisible (k);
        hi.setTooltip ("Console HI (10 kHz shelf)"); mid.setTooltip ("Console MID (1 kHz)"); lo.setTooltip ("Console LO (100 Hz shelf)");

        addAndMakeVisible (tape);
        tape.setTooltip ("Channel tape saturation");

        for (int s = 0; s < kNumSlots; ++s)
        {
            addAndMakeVisible (slots[s]);
            slots[s].onSlotClick = [this, s] (bool popup) { slotClicked (s, popup); };
            slots[s].setTooltip ("Click to edit, right-click to change");
        }

        addAndMakeVisible (send);
        addAndMakeVisible (sendLevel);
        sendLevel.setTooltip ("Send level");
        if (fx) { send.setEnabled (false); sendLevel.setEnabled (false); }

        addAndMakeVisible (output);
        output.setTooltip ("Master bus, or a direct hardware output");
        addAndMakeVisible (recMode);
        recMode.setTooltip ("Record after the channel processing (POST) or the raw input (DRY)");

        addAndMakeVisible (pan);
        rec.setTooltip ("Record arm"); solo.setTooltip ("Solo"); mute.setTooltip ("Mute");
        for (auto* b : { &rec, &solo, &mute }) addAndMakeVisible (b);

        fader.getProperties().set ("cap", VC::white.toString());
        addAndMakeVisible (fader);
        addAndMakeVisible (meter);
        fader.onParamChange = [this] { readout.refresh(); };
        readout.onEdited = [this] { fader.refresh(); };
        addAndMakeVisible (readout);

        name.setFont (VegaFonts::label (15.0f));
        name.setJustificationType (juce::Justification::centred);
        name.setEditable (false, true);
        name.setTooltip ("Double-click to rename, right-click for colour");
        name.onTextChange = [this] { engine.names[(size_t) index] = name.getText().toUpperCase(); name.setText (engine.names[(size_t) index], juce::dontSendNotification); };
        name.addMouseListener (this, false);
        addAndMakeVisible (name);

        refreshAll();
        setSize (W, H);
    }

    void rebuildDevice (const juce::StringArray& inputs, int numOuts)
    {
        if (! fx)
        {
            source.clear (juce::dontSendNotification);
            source.addItem ("No input", 1);
            source.addSectionHeading ("Mono");
            for (int i = 0; i < inputs.size(); ++i)
                source.addItem ("In " + juce::String (i + 1) + "  " + inputs[i], 100 + i);
            source.addSectionHeading ("Windows");
            source.addItem ("System Audio (what Windows plays)", 300);
            if (inputs.size() > 1)
            {
                source.addSectionHeading ("Stereo");
                for (int i = 0; i + 1 < inputs.size(); i += 2)
                    source.addItem ("In " + juce::String (i + 1) + "-" + juce::String (i + 2), 200 + i);
            }
            syncSource();
        }
        auto outs = outputPairNames (numOuts, 1);
        outs.insert (0, "Master");
        output.setItems (outs);
    }

    void refreshAll()
    {
        for (auto* k : { &hi, &mid, &lo, &pan, &fader, &sendLevel }) k->refresh();
        for (auto* c : { &tape, &send, &output, &recMode }) c->refresh();
        if (fxCombo) fxCombo->refresh();
        for (auto* b : { &rec, &solo, &mute }) b->refresh();
        readout.refresh();
        syncSource();
        name.setText (engine.names[(size_t) index], juce::dontSendNotification);
        refreshSlots();
        repaint();
    }

    void updateMeters()
    {
        vu.update();
        meter.update();
        refreshSlots();
        sendLevel.setEnabled (! fx && p.sendDest->index() > 0);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &name && e.mods.isPopupMenu()) showNameMenu();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (VC::strip);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawVerticalLine (W - 1, 0.0f, (float) H);

        drawCaption (g, { 0, 6, W, 12 }, "Input");
        drawCaption (g, { 0, 46, W, 12 }, "Console");
        g.setFont (VegaFonts::label (10.0f));
        g.setColour (VC::label);
        g.drawText ("HI", 8, 134, 26, 10, juce::Justification::centred);
        g.drawText ("MID", 35, 134, 26, 10, juce::Justification::centred);
        g.drawText ("LO", 62, 134, 26, 10, juce::Justification::centred);
        drawCaption (g, { 0, 148, W, 12 }, "Tape");
        drawCaption (g, { 0, 190, W, 12 }, "Inserts");
        drawCaption (g, { 0, 290, W, 12 }, "Sends");
        drawCaption (g, { 0, 338, W, 12 }, "Output");

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (381, 0.0f, (float) W);

        g.setFont (VegaFonts::label (10.0f));
        g.setColour (VC::label);
        g.drawText ("Rec", 14, 474, 18, 10, juce::Justification::centred);
        g.drawText ("S", 39, 474, 18, 10, juce::Justification::centred);
        g.drawText ("M", 64, 474, 18, 10, juce::Justification::centred);

        g.setColour (VC::stripColour (p.colour->index()));
        g.fillRect (0, 822, W, 3);
        g.setColour (juce::Colour (0xff292929));
        g.fillRect (0, 825, W, H - 825);
    }

    void resized() override
    {
        (fxCombo ? static_cast<juce::Component&> (*fxCombo) : source).setBounds (8, 20, 80, 20);
        vu.setBounds (7, 60, 82, 42);
        hi.setBounds (8, 105, 26, 28); mid.setBounds (35, 105, 26, 28); lo.setBounds (62, 105, 26, 28);
        tape.setBounds (8, 162, 80, 20);
        for (int s = 0; s < kNumSlots; ++s) slots[s].setBounds (8, 204 + s * 28, 80, 22);
        send.setBounds (8, 304, 80, 20);
        sendLevel.setBounds (8, 325, 80, 8);
        output.setBounds (8, 352, 80, 20);
        recMode.setBounds (8, 390, 80, 20);
        pan.setBounds (32, 418, 32, 32);
        rec.setBounds (14, 454, 18, 18); solo.setBounds (39, 454, 18, 18); mute.setBounds (64, 454, 18, 18);
        fader.setBounds (12, 492, 56, 298);
        meter.setBounds (72, 500, 8, 282);
        readout.setBounds (8, 796, 80, 20);
        name.setBounds (2, 828, W - 4, 26);
    }

private:
    static juce::String moduleName (int m) { return m == modEq ? "Vega EQ" : m == modComp ? "Vega Comp" : m == modGate ? "Vega Gate" : "+"; }

    Param* moduleOn (int m) const { return m == modEq ? p.eq.on : m == modComp ? p.comp.on : m == modGate ? p.gate.on : nullptr; }

    void refreshSlots()
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            const int m = p.slot[s]->index();
            const auto text = moduleName (m);
            const bool bypassed = m != modEmpty && ! moduleOn (m)->on();
            auto& props = slots[s].getProperties();
            if (slots[s].getButtonText() != text || (bool) props.contains ("bypassed") != bypassed)
            {
                slots[s].setButtonText (text);
                if (bypassed) props.set ("bypassed", true); else props.remove ("bypassed");
                slots[s].repaint();
            }
        }
    }

    void slotClicked (int s, bool popup)
    {
        const int m = p.slot[s]->index();
        if (m != modEmpty && ! popup) { openModuleEditor (m, slots[s]); return; }

        juce::PopupMenu menu;
        for (int mod : { modEq, modComp, modGate })
            menu.addItem (mod, moduleName (mod), true, mod == m);
        if (m != modEmpty)
        {
            menu.addSeparator();
            menu.addItem (10, moduleOn (m)->on() ? "Bypass" : "Enable");
            menu.addItem (11, "Remove");
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slots[s]),
                            [this, s, m] (int r)
                            {
                                if (r == 0) return;
                                if (r == 10) { auto* on = moduleOn (m); on->set (on->on() ? 0.0f : 1.0f); }
                                else if (r == 11) engine.setChannelSlot (index, s, modEmpty);
                                else { engine.setChannelSlot (index, s, r); openModuleEditor (r, slots[s]); }
                                refreshSlots();
                            });
    }

    void openModuleEditor (int m, juce::Component& target)
    {
        auto& meters = engine.meters[(size_t) index];
        std::unique_ptr<juce::Component> ed;
        if (m == modEq)   ed = Editors::eq (p.eq, "Vega EQ  " + engine.names[(size_t) index]);
        if (m == modComp) ed = Editors::comp (p.comp, meters.gr);
        if (m == modGate) ed = Editors::gate (p.gate, meters.gate);
        if (ed) launchEditor (std::move (ed), target);
    }

    void openInputEditor()
    {
        if (index == kReverbStrip)      launchEditor (Editors::reverb (engine.fx), vu);
        else if (index == kDelayStrip)  launchEditor (Editors::delay (engine.fx), vu);
        else                            launchEditor (Editors::input (p, engine.names[(size_t) index]), vu);
    }

    void syncSource()
    {
        if (fx) return;
        const int mode = p.srcMode->index(), first = p.srcIndex->index();
        const int id = mode == 3 ? 300 : mode == 1 ? 100 + first : (mode == 2 ? 200 + first : 1);
        source.setSelectedId (source.indexOfItemId (id) >= 0 ? id : 1, juce::dontSendNotification);
    }

    void sourceChanged()
    {
        const int id = source.getSelectedId();
        if (id == 300)      p.srcMode->set (3);
        else if (id >= 200)      { p.srcMode->set (2); p.srcIndex->set ((float) (id - 200)); }
        else if (id >= 100) { p.srcMode->set (1); p.srcIndex->set ((float) (id - 100)); }
        else                  p.srcMode->set (0);
    }

    void showNameMenu()
    {
        juce::PopupMenu menu, colours;
        menu.addItem (1, "Rename...");
        const auto names = VC::colourNames();
        for (int i = 0; i < names.size(); ++i)
            colours.addColouredItem (100 + i, names[i], VC::stripColour (i), true, p.colour->index() == i);
        menu.addSubMenu ("Colour", colours);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name), [this] (int r)
        {
            if (r == 1) name.showEditor();
            if (r >= 100) { p.colour->set ((float) (r - 100)); repaint(); }
        });
    }

    MixerEngine& engine;
    int index;
    ChannelParams& p;
    bool fx;

    juce::ComboBox source;
    std::unique_ptr<ParamCombo> fxCombo;
    VUMeter vu;
    LedMeter meter;
    ParamSlider hi, mid, lo;
    ParamCombo tape;
    SlotButton slots[kNumSlots];
    ParamCombo send;
    ParamSlider sendLevel;
    ParamCombo output, recMode;
    ParamSlider pan;
    ParamButton rec, solo, mute;
    ParamSlider fader;
    DbReadout readout;
    juce::Label name;
};

//==============================================================================
class MasterStrip : public juce::Component
{
public:
    static constexpr int W = 128, H = ChannelStrip::H;

    explicit MasterStrip (MixerEngine& e)
        : engine (e), m (e.master),
          output (*m.outPair, { "Out 1-2" }),
          vu (e.masterMeter.vu),
          recMix (*m.recMix, { "REC MIX OFF", "REC MIX ON" }),
          mute (*m.mute, {}, VC::red, true),
          fader (*m.fader, ParamSlider::faderKind), readout (*m.fader), meter (e.masterMeter, true)
    {
        addAndMakeVisible (output);
        output.setTooltip ("Hardware outputs for the main mix");
        addAndMakeVisible (vu);
        vu.setTooltip ("Master console VU");

        // The TAPE combo drives the master tape machine (on/off + speed).
        tape.addItemList ({ "Off", "2T - 15 ips", "2T - 30 ips" }, 1);
        tape.setTooltip ("Master tape machine (see MACHINE panel)");
        tape.onChange = [this]
        {
            const int id = tape.getSelectedId();
            m.tapeOn->set (id > 1 ? 1.0f : 0.0f);
            if (id > 1) m.tapeIps->set ((float) (id - 2));
        };
        addAndMakeVisible (tape);

        for (int s = 0; s < kNumSlots; ++s)
        {
            addAndMakeVisible (slots[s]);
            slots[s].onSlotClick = [this, s] (bool popup) { slotClicked (s, popup); };
            slots[s].setTooltip ("Click to edit, right-click to change");
        }

        addAndMakeVisible (recMix);
        recMix.setTooltip ("Record the stereo mix (Mix.wav) when you press REC");
        mute.setTooltip ("Mute the monitor output (recording continues)");
        addAndMakeVisible (mute);
        fader.getProperties().set ("cap", VC::cyan.toString());
        addAndMakeVisible (fader);
        addAndMakeVisible (meter);
        fader.onParamChange = [this] { readout.refresh(); };
        readout.onEdited = [this] { fader.refresh(); };
        addAndMakeVisible (readout);

        refreshAll();
        setSize (W, H);
    }

    void rebuildDevice (int numOuts) { output.setItems (outputPairNames (numOuts, 0)); }

    void refreshAll()
    {
        output.refresh(); recMix.refresh(); mute.refresh(); fader.refresh(); readout.refresh();
        syncTape();
        refreshSlots();
    }

    void updateMeters()
    {
        vu.update(); meter.update();
        syncTape();
        refreshSlots();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (VC::panel);
        drawCaption (g, { 0, 6, W, 12 }, "Master", VC::cyan);
        drawCaption (g, { 0, 46, W, 12 }, "Console");
        drawCaption (g, { 0, 110, W, 12 }, "Tape");
        drawCaption (g, { 0, 152, W, 12 }, "Inserts");
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (381, 0.0f, (float) W);
        g.setFont (VegaFonts::label (10.0f));
        g.setColour (VC::label);
        g.drawText ("M", 55, 474, 18, 10, juce::Justification::centred);

        auto main = juce::Rectangle<int> (0, 822, W, 32);
        g.setColour (juce::Colour (0xffdcdcdc));
        g.fillRect (main);
        g.setColour (juce::Colour (0xff2a2a2a));
        g.setFont (VegaFonts::mono (17.0f));
        g.drawText ("MAIN", main, juce::Justification::centred);
    }

    void resized() override
    {
        output.setBounds (10, 20, W - 20, 20);
        vu.setBounds (10, 60, W - 20, 44);
        tape.setBounds (10, 124, W - 20, 20);
        for (int s = 0; s < kNumSlots; ++s) slots[s].setBounds (10, 166 + s * 28, W - 20, 22);
        recMix.setBounds (10, 390, W - 20, 20);
        mute.setBounds (55, 454, 18, 18);
        fader.setBounds (18, 492, 60, 298);
        meter.setBounds (84, 500, 22, 282);
        readout.setBounds (10, 796, W - 20, 20);
    }

private:
    static juce::String moduleName (int mod) { return mod == mModEq ? "Vega EQ" : mod == mModBusComp ? "Bus Comp" : mod == mModLimiter ? "Limiter" : "+"; }
    Param* moduleOn (int mod) const { return mod == mModEq ? m.eq.on : mod == mModBusComp ? m.busOn : mod == mModLimiter ? m.limOn : nullptr; }

    void syncTape()
    {
        const int id = m.tapeOn->on() ? 2 + m.tapeIps->index() : 1;
        if (tape.getSelectedId() != id) tape.setSelectedId (id, juce::dontSendNotification);
    }

    void refreshSlots()
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            const int mod = m.slot[s]->index();
            const auto text = moduleName (mod);
            const bool bypassed = mod != mModEmpty && ! moduleOn (mod)->on();
            auto& props = slots[s].getProperties();
            if (slots[s].getButtonText() != text || (bool) props.contains ("bypassed") != bypassed)
            {
                slots[s].setButtonText (text);
                if (bypassed) props.set ("bypassed", true); else props.remove ("bypassed");
                slots[s].repaint();
            }
        }
    }

    void openEditor (int mod, juce::Component& target)
    {
        if (mod == mModEq)      launchEditor (Editors::eq (m.eq, "Vega EQ  MASTER"), target);
        if (mod == mModBusComp) launchEditor (Editors::busComp (m, engine.masterMeter.gr), target);
        if (mod == mModLimiter) launchEditor (Editors::limiter (m, engine.masterMeter.lim), target);
    }

    void slotClicked (int s, bool popup)
    {
        const int cur = m.slot[s]->index();
        if (cur != mModEmpty && ! popup) { openEditor (cur, slots[s]); return; }
        juce::PopupMenu menu;
        for (int mod : { mModEq, mModBusComp, mModLimiter })
            menu.addItem (mod, moduleName (mod), true, mod == cur);
        if (cur != mModEmpty)
        {
            menu.addSeparator();
            menu.addItem (10, moduleOn (cur)->on() ? "Bypass" : "Enable");
            menu.addItem (11, "Remove");
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slots[s]), [this, s, cur] (int r)
        {
            if (r == 0) return;
            if (r == 10) { auto* on = moduleOn (cur); on->set (on->on() ? 0.0f : 1.0f); }
            else if (r == 11) engine.setMasterSlot (s, mModEmpty);
            else { engine.setMasterSlot (s, r); openEditor (r, slots[s]); }
            refreshSlots();
        });
    }

    MixerEngine& engine;
    MasterParams& m;
    ParamCombo output;
    VUMeter vu;
    juce::ComboBox tape;
    SlotButton slots[kNumSlots];
    ParamCombo recMix;
    ParamButton mute;
    ParamSlider fader;
    DbReadout readout;
    LedMeter meter;
};

//==============================================================================
// Animated 2-track reel display.
class ReelDisplay : public juce::Component, private juce::Timer
{
public:
    explicit ReelDisplay (MixerEngine& e) : engine (e) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff171717));
        g.fillRoundedRectangle (r, 3.0f);

        auto& m = engine.master;
        const bool active = m.tapeOn->on() && ! m.tapeBypass->on();
        auto reelArea = r.reduced (12.0f, 10.0f).withTrimmedBottom (30.0f);
        const float d = juce::jmin (reelArea.getHeight(), reelArea.getWidth() * 0.5f - 4.0f);
        const juce::Point<float> c1 (reelArea.getX() + d * 0.5f, reelArea.getCentreY());
        const juce::Point<float> c2 (reelArea.getRight() - d * 0.5f, reelArea.getCentreY());

        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillRect (r.getX() + 50.0f, r.getBottom() - 34.0f, r.getWidth() - 100.0f, 2.0f);   // tape path

        drawReel (g, c1, d * 0.5f, juce::Colour (0xff5a3a22), d * 0.47f, angle, active);
        drawReel (g, c2, d * 0.5f, juce::Colour (0xff8f8f8f), d * 0.33f, angle * 1.35f, active);

        g.setColour (active ? VC::cyan : VC::dim);
        g.setFont (VegaFonts::mono (13.0f));
        const juce::String ips = m.tapeIps->index() == 0 ? "15 IPS" : "30 IPS";
        g.drawText (ips + "  .  " + m.tapeCal->toText (m.tapeCal->get()), r.removeFromBottom (26.0f), juce::Justification::centred);
    }

private:
    static void drawReel (juce::Graphics& g, juce::Point<float> c, float flangeR, juce::Colour pack, float packR, float ang, bool active)
    {
        g.setColour (juce::Colour (0xff222222));
        g.fillEllipse (c.x - flangeR, c.y - flangeR, flangeR * 2, flangeR * 2);
        g.setColour (active ? pack : pack.withMultipliedSaturation (0.4f).darker (0.35f));
        g.fillEllipse (c.x - packR, c.y - packR, packR * 2, packR * 2);
        const float hub = flangeR * 0.42f;
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2);
        g.setColour (juce::Colour (0xff444444));
        g.drawEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2, 1.2f);
        for (int i = 0; i < 3; ++i)   // hub spokes, rotating
        {
            const float a = ang + juce::MathConstants<float>::twoPi * (float) i / 3.0f;
            g.drawLine ({ c.getPointOnCircumference (hub * 0.3f, a), c.getPointOnCircumference (hub * 0.9f, a) }, 2.0f);
        }
        g.setColour (juce::Colour (0xff555555));
        g.fillEllipse (c.x - 3, c.y - 3, 6, 6);
    }

    void timerCallback() override
    {
        auto& m = engine.master;
        const bool active = m.tapeOn->on() && ! m.tapeBypass->on() && engine.tapeRolling.load();
        if (active) angle += m.tapeIps->index() == 0 ? 0.09f : 0.18f;
        if (active || wasActive != active || ++idleRepaint % 15 == 0) repaint();   // idle refresh keeps the label current
        wasActive = active;
    }

    MixerEngine& engine;
    float angle = 0.0f;
    bool wasActive = true;
    int idleRepaint = 0;
};

class MachinePanel : public juce::Component
{
public:
    static constexpr int W = 228;

    explicit MachinePanel (MixerEngine& e)
        : engine (e), formula (*e.master.tapeFormula, e.master.tapeFormula->choices), reels (e),
          drive (*e.master.tapeDrive), ips (*e.master.tapeIps), cal (*e.master.tapeCal),
          on (*e.master.tapeOn, {}, VC::cyan, true), bypass (*e.master.tapeBypass, {}, VC::cyan, true)
    {
        addAndMakeVisible (formula);
        formula.setTooltip ("Tape formula: 456 is the most saturated, GP9 the cleanest");
        addAndMakeVisible (reels);
        for (auto* k : { &drive, &ips, &cal }) addAndMakeVisible (k);
        on.setTooltip ("Tape machine on the master bus");
        bypass.setTooltip ("A/B: bypass the tape machine");
        addAndMakeVisible (on);
        addAndMakeVisible (bypass);

        addAndMakeVisible (presets);
        presets.setTextWhenNothingSelected ("No preset");
        presets.setTooltip ("Mixer presets");
        for (auto* b : { &delBtn, &renameBtn, &saveBtn })
        {
            b->getProperties().set ("tab", true);
            addAndMakeVisible (b);
        }
        saveBtn.getProperties().set ("textColour", juce::Colour (0xff222222).toString());
        saveBtn.setToggleState (true, juce::dontSendNotification);
        saveBtn.setClickingTogglesState (false);
    }

    juce::ComboBox presets;
    juce::TextButton delBtn { "DELETE" }, renameBtn { "RENAME" }, saveBtn { "SAVE" };

    void refreshAll() { formula.refresh(); drive.refresh(); ips.refresh(); cal.refresh(); on.refresh(); bypass.refresh(); reels.repaint(); }
    void updateMeters() { if (on.getToggleState() != engine.master.tapeOn->on()) on.refresh(); if (ips.getValue() != engine.master.tapeIps->get()) ips.refresh(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (VC::panel);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine (W - 1, 0.0f, (float) getHeight());

        g.setColour (VC::label);
        g.setFont (VegaFonts::label (12.0f));
        g.drawText ("MACHINE", 14, 10, 100, 14, juce::Justification::centredLeft);
        g.setFont (VegaFonts::label (11.0f));
        g.drawText ("DRIVE", 14, 306, 60, 12, juce::Justification::centred);
        g.drawText ("IPS", 84, 306, 60, 12, juce::Justification::centred);
        g.drawText ("CAL", 154, 306, 60, 12, juce::Justification::centred);
        g.setFont (VegaFonts::label (10.0f));
        g.drawText ("On", 70, 368, 30, 10, juce::Justification::centred);
        g.drawText ("Bypass", 116, 368, 40, 10, juce::Justification::centred);

        g.setColour (VC::line);
        g.drawHorizontalLine (392, 14.0f, (float) W - 14.0f);
        g.setColour (VC::label);
        g.setFont (VegaFonts::label (12.0f));
        g.drawText ("PRESET", 14, 410, 100, 14, juce::Justification::centredLeft);
    }

    void resized() override
    {
        formula.setBounds (14, 32, W - 28, 20);
        reels.setBounds (14, 62, W - 28, 154);
        drive.setBounds (19, 252, 50, 50);
        ips.setBounds (89, 252, 50, 50);
        cal.setBounds (159, 252, 50, 50);
        on.setBounds (76, 344, 20, 20);
        bypass.setBounds (126, 344, 20, 20);
        presets.setBounds (14, 432, W - 28, 20);
        delBtn.setBounds (14, 464, 62, 22);
        renameBtn.setBounds (83, 464, 62, 22);
        saveBtn.setBounds (152, 464, 62, 22);
    }

private:
    MixerEngine& engine;
    ParamCombo formula;
    ReelDisplay reels;
    ParamSlider drive, ips, cal;
    ParamButton on, bypass;
};
