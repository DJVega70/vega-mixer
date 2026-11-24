#pragma once
#include "VegaLook.h"

// A pop-up panel of knobs/toggles for one module (opened in a CallOutBox).
class ModuleEditor : public juce::Component, private juce::Timer
{
public:
    ModuleEditor (const juce::String& titleText, Param* onParam, int columns)
        : title (titleText), cols (columns)
    {
        if (onParam != nullptr)
        {
            power = std::make_unique<ParamButton> (*onParam, "ON", VC::cyan);
            addAndMakeVisible (*power);
        }
    }

    ParamSlider* knob (Param& p, const juce::String& label)
    {
        auto* s = new ParamSlider (p, ParamSlider::knobKind, true);
        addAndMakeVisible (s);
        cells.push_back ({ std::unique_ptr<juce::Component> (s), label });
        return s;
    }

    ParamButton* toggle (Param& p, const juce::String& label)
    {
        auto* b = new ParamButton (p, label, VC::cyan);
        addAndMakeVisible (b);
        cells.push_back ({ std::unique_ptr<juce::Component> (b), {} });
        return b;
    }

    void gap() { cells.push_back ({ nullptr, {} }); }

    void grMeter (std::atomic<float>& src)
    {
        gr = std::make_unique<GRBar> (src);
        addAndMakeVisible (*gr);
        startTimerHz (30);
    }

    void note (const juce::String& t) { footnote = t; }

    void finish()
    {
        const int rows = ((int) cells.size() + cols - 1) / cols;
        setSize (juce::jmax (220, cols * cellW + 24), 40 + rows * cellH + (gr ? 26 : 0) + (footnote.isNotEmpty() ? 18 : 0) + 8);
    }

    void refreshAll()
    {
        for (auto& c : cells)
        {
            if (auto* s = dynamic_cast<ParamSlider*> (c.comp.get())) s->refresh();
            if (auto* b = dynamic_cast<ParamButton*> (c.comp.get())) b->refresh();
        }
        if (power) power->refresh();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (VC::cyan);
        g.setFont (VegaFonts::mono (15.0f));
        g.drawText (title, 12, 6, getWidth() - 80, 22, juce::Justification::centredLeft);
        g.setColour (VC::line);
        g.drawHorizontalLine (32, 10.0f, (float) getWidth() - 10.0f);

        g.setFont (VegaFonts::label (11.0f));
        g.setColour (VC::label);
        for (size_t i = 0; i < cells.size(); ++i)
            if (cells[i].comp != nullptr && cells[i].label.isNotEmpty())
                g.drawText (cells[i].label.toUpperCase(), cellRect ((int) i).removeFromTop (12), juce::Justification::centred);

        if (footnote.isNotEmpty())
        {
            g.setColour (VC::label);
            g.drawText (footnote, 12, getHeight() - 22, getWidth() - 24, 16, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        if (power) power->setBounds (getWidth() - 58, 8, 46, 20);
        for (size_t i = 0; i < cells.size(); ++i)
        {
            if (cells[i].comp == nullptr) continue;
            auto r = cellRect ((int) i);
            if (dynamic_cast<ParamButton*> (cells[i].comp.get()) != nullptr)
                cells[i].comp->setBounds (r.withSizeKeepingCentre (cellW - 14, 22));
            else
                cells[i].comp->setBounds (r.withTrimmedTop (12));
        }
        if (gr)
        {
            const int rows = ((int) cells.size() + cols - 1) / cols;
            gr->setBounds (12, 40 + rows * cellH + 2, getWidth() - 24, 22);
        }
    }

private:
    void timerCallback() override { if (gr) gr->update(); }

    juce::Rectangle<int> cellRect (int i) const
    {
        const int totalW = cols * cellW;
        const int x0 = (getWidth() - totalW) / 2;
        return { x0 + (i % cols) * cellW, 40 + (i / cols) * cellH, cellW, cellH };
    }

    struct Cell { std::unique_ptr<juce::Component> comp; juce::String label; };
    static constexpr int cellW = 70, cellH = 78;
    juce::String title, footnote;
    int cols;
    std::vector<Cell> cells;
    std::unique_ptr<ParamButton> power;
    std::unique_ptr<GRBar> gr;
};

//==============================================================================
namespace Editors
{
inline std::unique_ptr<ModuleEditor> eq (const EqParams& p, const juce::String& title)
{
    auto e = std::make_unique<ModuleEditor> (title, p.on, 4);
    e->knob (*p.lfGain, "LF");   e->knob (*p.lmfGain, "LMF");  e->knob (*p.hmfGain, "HMF");  e->knob (*p.hfGain, "HF");
    e->knob (*p.lfFreq, "Freq"); e->knob (*p.lmfFreq, "Freq"); e->knob (*p.hmfFreq, "Freq"); e->knob (*p.hfFreq, "Freq");
    e->toggle (*p.lfBell, "BELL"); e->knob (*p.lmfQ, "Q");     e->knob (*p.hmfQ, "Q");       e->toggle (*p.hfBell, "BELL");
    e->finish();
    return e;
}

inline std::unique_ptr<ModuleEditor> comp (const CompParams& p, std::atomic<float>& grSrc)
{
    auto e = std::make_unique<ModuleEditor> ("Vega Comp", p.on, 3);
    e->knob (*p.thr, "Threshold"); e->knob (*p.ratio, "Ratio");   e->knob (*p.makeup, "Makeup");
    e->knob (*p.atk, "Attack");    e->knob (*p.rel, "Release");   e->toggle (*p.peak, "PEAK");
    e->grMeter (grSrc);
    e->finish();
    return e;
}

inline std::unique_ptr<ModuleEditor> gate (const GateParams& p, std::atomic<float>& grSrc)
{
    auto e = std::make_unique<ModuleEditor> ("Vega Gate", p.on, 3);
    e->knob (*p.thr, "Threshold"); e->knob (*p.range, "Range"); e->knob (*p.hold, "Hold");
    e->knob (*p.rel, "Release");   e->toggle (*p.exp, "EXPANDER");
    e->grMeter (grSrc);
    e->finish();
    return e;
}

inline std::unique_ptr<ModuleEditor> input (const ChannelParams& p, const juce::String& name)
{
    auto e = std::make_unique<ModuleEditor> (name + " - Input", nullptr, 3);
    e->knob (*p.trim, "Trim");     e->knob (*p.hpfFreq, "Low Cut");  e->knob (*p.lpfFreq, "High Cut");
    e->toggle (*p.phase, juce::CharPointer_UTF8 ("\xc3\x98 PHASE")); e->toggle (*p.hpfOn, "LOW CUT"); e->toggle (*p.lpfOn, "HIGH CUT");
    e->finish();
    return e;
}

inline std::unique_ptr<ModuleEditor> busComp (const MasterParams& m, std::atomic<float>& grSrc)
{
    auto e = std::make_unique<ModuleEditor> ("Bus Comp", m.busOn, 3);
    e->knob (*m.busThr, "Threshold"); e->knob (*m.busRatio, "Ratio");   e->knob (*m.busMakeup, "Makeup");
    e->knob (*m.busAtk, "Attack");    e->knob (*m.busRel, "Release");
    e->grMeter (grSrc);
    e->finish();
    return e;
}

inline std::unique_ptr<ModuleEditor> limiter (const MasterParams& m, std::atomic<float>& grSrc)
{
    auto e = std::make_unique<ModuleEditor> ("Limiter", m.limOn, 2);
    e->knob (*m.limCeiling, "Ceiling");
    e->grMeter (grSrc);
    e->finish();
    return e;
}

inline std::unique_ptr<ModuleEditor> reverb (FxParams& f)
{
    auto e = std::make_unique<ModuleEditor> ("Vega Reverb", nullptr, 4);
    auto* type = e->knob (*f.revType, "Type");
    auto* size = e->knob (*f.revSize, "Size");
    auto* damp = e->knob (*f.revDamp, "Damping");
    auto* width = e->knob (*f.revWidth, "Width");
    type->onParamChange = [&f, size, damp, width]
    {
        static const float presets[3][3] { { 0.35f, 0.55f, 0.8f }, { 0.82f, 0.35f, 1.0f }, { 0.62f, 0.12f, 1.0f } };
        const int t = f.revType->index();
        f.revSize->set (presets[t][0]); f.revDamp->set (presets[t][1]); f.revWidth->set (presets[t][2]);
        size->refresh(); damp->refresh(); width->refresh();
    };
    e->note ("Fed by every channel's SENDS set to Reverb.");
    e->finish();
    return e;
}

inline std::unique_ptr<ModuleEditor> delay (FxParams& f)
{
    auto e = std::make_unique<ModuleEditor> ("Vega Delay", nullptr, 4);
    e->knob (*f.dlyDiv, "Time"); e->knob (*f.dlyFb, "Feedback"); e->knob (*f.dlyTone, "Tone"); e->toggle (*f.dlyPing, "PING-PONG");
    e->note ("Synced to the BPM in the top bar.");
    e->finish();
    return e;
}
} // namespace Editors
