#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

// A single automatable value shared between the UI (writes) and the audio thread (reads).
struct Param
{
    enum class Unit { dB, Hz, ms, ratio, plain, toggle, choice, pan, fader, q };

    Param (juce::String idIn, juce::String nameIn, Unit u, float mn, float mx, float d, float mid = 0.0f,
           juce::StringArray choiceList = {})
        : id (std::move (idIn)), name (std::move (nameIn)), unit (u),
          minV (mn), maxV (mx), def (d), skewMid (mid), choices (std::move (choiceList)), value (d) {}

    juce::String id, name;
    Unit unit;
    float minV, maxV, def, skewMid;
    juce::StringArray choices;
    std::atomic<float> value;

    float get() const noexcept     { return value.load (std::memory_order_relaxed); }
    bool  on() const noexcept      { return get() > 0.5f; }
    int   index() const noexcept   { return (int) std::lround (get()); }
    void  set (float v) noexcept   { value.store (juce::jlimit (minV, maxV, v), std::memory_order_relaxed); }

    juce::String toText (float v) const
    {
        switch (unit)
        {
            case Unit::dB:     return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
            case Unit::fader:  return v <= minV + 0.05f ? juce::String ("-inf") : (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
            case Unit::Hz:     return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz";
            case Unit::ms:     return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String ((int) std::round (v)) + " ms";
            case Unit::ratio:  return juce::String (v, v < 10.0f ? 1 : 0) + ":1";
            case Unit::q:      return "Q " + juce::String (v, 2);
            case Unit::pan:    return std::abs (v) < 0.01f ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String ((int) std::round (std::abs (v) * 100.0f));
            case Unit::toggle: return v > 0.5f ? "On" : "Off";
            case Unit::choice: return choices[(int) std::lround (v)];
            case Unit::plain:
            default:           return juce::String (v, 2);
        }
    }
};

class ParamSet
{
public:
    Param* add (const juce::String& id, const juce::String& name, Param::Unit u, float mn, float mx, float d,
                float mid = 0.0f, juce::StringArray choices = {})
    {
        params.push_back (std::make_unique<Param> (id, name, u, mn, mx, d, mid, std::move (choices)));
        return params.back().get();
    }

    Param* find (const juce::String& id) const
    {
        for (auto& p : params)
            if (p->id == id) return p.get();
        return nullptr;
    }

    void writeTo (juce::XmlElement& xml) const
    {
        auto* e = xml.createNewChildElement ("PARAMS");
        for (auto& p : params)
            e->setAttribute (p->id, p->get());
    }

    void readFrom (const juce::XmlElement& xml)
    {
        if (auto* e = xml.getChildByName ("PARAMS"))
            for (auto& p : params)
                if (e->hasAttribute (p->id))
                    p->set ((float) e->getDoubleAttribute (p->id, p->def));
    }

    void resetAll()
    {
        for (auto& p : params) p->set (p->def);
    }

private:
    std::vector<std::unique_ptr<Param>> params;
};
