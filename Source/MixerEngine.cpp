#include "MixerEngine.h"

using U = Param::Unit;

static const float kBusRatios[]  { 2.0f, 4.0f, 10.0f };
static const float kBusAttacks[] { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
static const float kBusRels[]    { 100.0f, 300.0f, 600.0f, 1200.0f, -1.0f }; // -1 = auto
static const float kDelayBeats[] { 1.0f, 0.5f, 0.75f, 0.25f, 2.0f / 3.0f, 2.0f };
static const float kCalDb[]      { 3.0f, 6.0f, 9.0f };

static EqParams makeEq (ParamSet& ps, const juce::String& pre, bool onByDefault)
{
    EqParams e;
    e.on      = ps.add (pre + "eqOn", "Vega EQ", U::toggle, 0, 1, onByDefault ? 1.0f : 0.0f);
    e.hfGain  = ps.add (pre + "hfGain", "HF Gain", U::dB, -15, 15, 0);
    e.hfFreq  = ps.add (pre + "hfFreq", "HF Freq", U::Hz, 1500, 16000, 8000, 5000);
    e.hfBell  = ps.add (pre + "hfBell", "HF Bell", U::toggle, 0, 1, 0);
    e.hmfGain = ps.add (pre + "hmfGain", "HMF Gain", U::dB, -15, 15, 0);
    e.hmfFreq = ps.add (pre + "hmfFreq", "HMF Freq", U::Hz, 600, 7000, 2500, 2000);
    e.hmfQ    = ps.add (pre + "hmfQ", "HMF Q", U::q, 0.3f, 5, 1, 1);
    e.lmfGain = ps.add (pre + "lmfGain", "LMF Gain", U::dB, -15, 15, 0);
    e.lmfFreq = ps.add (pre + "lmfFreq", "LMF Freq", U::Hz, 200, 2500, 600, 600);
    e.lmfQ    = ps.add (pre + "lmfQ", "LMF Q", U::q, 0.3f, 5, 1, 1);
    e.lfGain  = ps.add (pre + "lfGain", "LF Gain", U::dB, -15, 15, 0);
    e.lfFreq  = ps.add (pre + "lfFreq", "LF Freq", U::Hz, 30, 450, 100, 100);
    e.lfBell  = ps.add (pre + "lfBell", "LF Bell", U::toggle, 0, 1, 0);
    return e;
}

MixerEngine::MixerEngine()
{
    const juce::StringArray chModules { "Empty", "Vega EQ", "Vega Comp", "Vega Gate" };
    const int defaultColours[kNumStrips] { 0, 0, 1, 2, 2, 4, 5, 6, 5, 5 };

    for (int i = 0; i < kNumStrips; ++i)
    {
        const juce::String pre = "ch" + juce::String (i + 1) + "_";
        auto P = [&] (const char* key, const char* name, U u, float mn, float mx, float d, float mid = 0.0f, juce::StringArray c = {})
        {
            return params.add (pre + key, name, u, mn, mx, d, mid, c);
        };
        const bool fxStrip = isFxStrip (i);
        auto& c = ch[(size_t) i];

        c.srcMode  = P ("srcMode", "Source", U::plain, 0, 3, fxStrip ? 2.0f : (i < 2 ? 1.0f : (i == 2 ? 2.0f : 0.0f)));
        c.srcIndex = P ("srcIndex", "Input", U::plain, 0, 63, i == 1 ? 1.0f : 0.0f);
        c.trim     = P ("trim", "Trim", U::dB, -20, 20, 0);
        c.phase    = P ("phase", "Phase", U::toggle, 0, 1, 0);
        c.hpfOn    = P ("hpfOn", "HPF", U::toggle, 0, 1, 0);
        c.hpfFreq  = P ("hpfFreq", "HPF", U::Hz, 20, 500, 80, 100);
        c.lpfOn    = P ("lpfOn", "LPF", U::toggle, 0, 1, 0);
        c.lpfFreq  = P ("lpfFreq", "LPF", U::Hz, 2000, 20000, 18000, 8000);

        c.conHi  = P ("conHi", "Console HI", U::dB, -12, 12, 0);
        c.conMid = P ("conMid", "Console MID", U::dB, -12, 12, 0);
        c.conLo  = P ("conLo", "Console LO", U::dB, -12, 12, 0);
        c.tape   = P ("tape", "Tape", U::choice, 0, 2, 0, 0, { "Off", "15 ips", "30 ips" });

        for (int s = 0; s < kNumSlots; ++s)
            c.slot[s] = P (("slot" + juce::String (s + 1)).toRawUTF8(), "Insert", U::choice, 0, 3,
                           s == 0 ? (float) modEq : 0.0f, 0, chModules);

        c.eq = makeEq (params, pre, true);
        c.comp.on     = P ("compOn", "Vega Comp", U::toggle, 0, 1, 0);
        c.comp.peak   = P ("compPeak", "Peak", U::toggle, 0, 1, 0);
        c.comp.thr    = P ("compThr", "Threshold", U::dB, -50, 0, -12);
        c.comp.ratio  = P ("compRatio", "Ratio", U::ratio, 1, 20, 4, 4);
        c.comp.atk    = P ("compAtk", "Attack", U::ms, 0.1f, 100, 10, 10);
        c.comp.rel    = P ("compRel", "Release", U::ms, 10, 2000, 150, 250);
        c.comp.makeup = P ("compMakeup", "Makeup", U::dB, 0, 24, 0);
        c.gate.on     = P ("gateOn", "Vega Gate", U::toggle, 0, 1, 0);
        c.gate.exp    = P ("gateExp", "Expander", U::toggle, 0, 1, 0);
        c.gate.thr    = P ("gateThr", "Threshold", U::dB, -80, 0, -50);
        c.gate.range  = P ("gateRange", "Range", U::dB, 0, 80, 40);
        c.gate.hold   = P ("gateHold", "Hold", U::ms, 0, 500, 40, 80);
        c.gate.rel    = P ("gateRel", "Release", U::ms, 10, 2000, 200, 250);

        c.sendDest  = P ("sendDest", "Send", U::choice, 0, 2, 0, 0, { "None", "Reverb", "Delay" });
        c.sendLevel = P ("sendLevel", "Send Level", U::fader, -70, 6, -12, -15);
        c.outDest   = P ("outDest", "Output", U::plain, 0, 31, 0);
        c.recDry    = P ("recDry", "Record", U::choice, 0, 1, 0, 0, { "REC POST", "REC DRY" });

        c.pan    = P ("pan", "Pan", U::pan, -1, 1, 0);
        c.fader  = P ("fader", "Level", U::fader, -70, 12, 0, -15);
        c.mute   = P ("mute", "Mute", U::toggle, 0, 1, 0);
        c.solo   = P ("solo", "Solo", U::toggle, 0, 1, 0);
        c.recArm = P ("recArm", "Rec Arm", U::toggle, 0, 1, 0);
        c.colour = P ("colour", "Colour", U::plain, 0, 7, (float) defaultColours[i]);

        names[(size_t) i] = i == kReverbStrip ? "REVERB" : (i == kDelayStrip ? "DELAY" : "CH " + juce::String (i + 1));
    }

    fx.revType  = params.add ("fx_revType", "Reverb", U::choice, 0, 2, 1, 0, { "Room", "Hall", "Plate" });
    fx.revSize  = params.add ("fx_revSize", "Size", U::plain, 0, 1, 0.8f);
    fx.revDamp  = params.add ("fx_revDamp", "Damping", U::plain, 0, 1, 0.35f);
    fx.revWidth = params.add ("fx_revWidth", "Width", U::plain, 0, 1, 1.0f);
    fx.dlyDiv   = params.add ("fx_dlyDiv", "Time", U::choice, 0, 5, 2, 0, { "1/4", "1/8", "1/8 D", "1/16", "1/4 T", "1/2" });
    fx.dlyFb    = params.add ("fx_dlyFb", "Feedback", U::plain, 0, 0.95f, 0.35f);
    fx.dlyTone  = params.add ("fx_dlyTone", "Tone", U::Hz, 800, 18000, 6000, 4000);
    fx.dlyPing  = params.add ("fx_dlyPing", "Ping-Pong", U::toggle, 0, 1, 1);

    const juce::StringArray mModules { "Empty", "Vega EQ", "Bus Comp", "Limiter" };
    auto& m = master;
    m.outPair = params.add ("m_outPair", "Output", U::plain, 0, 31, 0);
    m.conHi   = params.add ("m_conHi", "Console HI", U::dB, -12, 12, 0);
    m.conMid  = params.add ("m_conMid", "Console MID", U::dB, -12, 12, 0);
    m.conLo   = params.add ("m_conLo", "Console LO", U::dB, -12, 12, 0);
    for (int s = 0; s < kNumSlots; ++s)
        m.slot[s] = params.add ("m_slot" + juce::String (s + 1), "Insert", U::choice, 0, 3,
                                s == 0 ? (float) mModBusComp : (s == 1 ? (float) mModLimiter : 0.0f), 0, mModules);
    m.eq        = makeEq (params, "m_", false);
    m.busOn     = params.add ("m_busOn", "Bus Comp", U::toggle, 0, 1, 0);
    m.busThr    = params.add ("m_busThr", "Threshold", U::dB, -30, 0, -10);
    m.busRatio  = params.add ("m_busRatio", "Ratio", U::choice, 0, 2, 1, 0, { "2:1", "4:1", "10:1" });
    m.busAtk    = params.add ("m_busAtk", "Attack", U::choice, 0, 5, 3, 0, { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" });
    m.busRel    = params.add ("m_busRel", "Release", U::choice, 0, 4, 4, 0, { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" });
    m.busMakeup = params.add ("m_busMakeup", "Makeup", U::dB, 0, 15, 0);
    m.limOn      = params.add ("m_limOn", "Limiter", U::toggle, 0, 1, 1);
    m.limCeiling = params.add ("m_limCeiling", "Ceiling", U::dB, -6, 0, -0.3f);
    m.tapeOn      = params.add ("m_tapeOn", "Tape On", U::toggle, 0, 1, 0);
    m.tapeBypass  = params.add ("m_tapeBypass", "Tape Bypass", U::toggle, 0, 1, 0);
    m.tapeIps     = params.add ("m_tapeIps", "IPS", U::choice, 0, 1, 1, 0, { "15 IPS", "30 IPS" });
    m.tapeDrive   = params.add ("m_tapeDrive", "Drive", U::dB, -6, 12, 0);
    m.tapeCal     = params.add ("m_tapeCal", "Cal", U::choice, 0, 2, 0, 0, { "+3 CAL", "+6 CAL", "+9 CAL" });
    m.tapeFormula = params.add ("m_tapeFormula", "Tape", U::choice, 0, 2, 0, 0, { "Vega Tape 2T - 456", "Vega Tape 2T - 499", "Vega Tape 2T - GP9" });
    m.fader  = params.add ("m_fader", "Master", U::fader, -70, 12, 0, -15);
    m.mute   = params.add ("m_mute", "Monitor Mute", U::toggle, 0, 1, 0);
    m.recMix = params.add ("m_recMix", "Rec Mix", U::toggle, 0, 1, 1);
    m.bpm    = params.add ("m_bpm", "BPM", U::plain, 40, 220, 124);
}

void MixerEngine::resetToDefaults()
{
    params.resetAll();
    for (int i = 0; i < kNumStrips; ++i)
        names[(size_t) i] = i == kReverbStrip ? "REVERB" : (i == kDelayStrip ? "DELAY" : "CH " + juce::String (i + 1));
}

void MixerEngine::setChannelSlot (int strip, int slot, int module)
{
    auto& c = ch[(size_t) strip];
    if (module != modEmpty)
        for (int s = 0; s < kNumSlots; ++s)
            if (s != slot && c.slot[s]->index() == module) c.slot[s]->set (0);
    c.slot[slot]->set ((float) module);
    // switching a module in turns it on
    if (module == modEq)   c.eq.on->set (1);
    if (module == modComp) c.comp.on->set (1);
    if (module == modGate) c.gate.on->set (1);
}

void MixerEngine::setMasterSlot (int slot, int module)
{
    if (module != mModEmpty)
        for (int s = 0; s < kNumSlots; ++s)
            if (s != slot && master.slot[s]->index() == module) master.slot[s]->set (0);
    master.slot[slot]->set ((float) module);
    if (module == mModEq)      master.eq.on->set (1);
    if (module == mModBusComp) master.busOn->set (1);
    if (module == mModLimiter) master.limOn->set (1);
}

//==============================================================================
void MixerEngine::EqDSP::process (double sr, const EqParams& p, float* const* d, int nch, int n)
{
    if (! p.on->on()) return;
    if (std::abs (p.lfGain->get()) > 0.05f)
    {
        if (p.lfBell->on()) lf.peak (sr, p.lfFreq->get(), 0.9, p.lfGain->get()); else lf.lowShelf (sr, p.lfFreq->get(), p.lfGain->get());
        lf.process (d, nch, n);
    }
    if (std::abs (p.lmfGain->get()) > 0.05f) { lmf.peak (sr, p.lmfFreq->get(), p.lmfQ->get(), p.lmfGain->get()); lmf.process (d, nch, n); }
    if (std::abs (p.hmfGain->get()) > 0.05f) { hmf.peak (sr, p.hmfFreq->get(), p.hmfQ->get(), p.hmfGain->get()); hmf.process (d, nch, n); }
    if (std::abs (p.hfGain->get()) > 0.05f)
    {
        if (p.hfBell->on()) hf.peak (sr, p.hfFreq->get(), 0.9, p.hfGain->get()); else hf.highShelf (sr, p.hfFreq->get(), p.hfGain->get());
        hf.process (d, nch, n);
    }
}

void MixerEngine::ConsoleDSP::process (double sr, float loDb, float midDb, float hiDb, float* const* d, int nch, int n)
{
    if (std::abs (loDb)  > 0.05f) { lo.lowShelf (sr, 100.0, loDb);   lo.process (d, nch, n); }
    if (std::abs (midDb) > 0.05f) { mid.peak (sr, 1000.0, 0.7, midDb); mid.process (d, nch, n); }
    if (std::abs (hiDb)  > 0.05f) { hi.highShelf (sr, 10000.0, hiDb); hi.process (d, nch, n); }
}

//==============================================================================
void MixerEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate());
}

void MixerEngine::prepare (double sr)
{
    sampleRate = sr;

    for (auto& s : dsp)
    {
        s.gate.sr = s.comp.sr = s.tape.sr = sr;
        s.gate.reset(); s.comp.reset(); s.tape.reset();
        s.hpf1.reset(); s.hpf2.reset(); s.lpf.reset(); s.console.reset(); s.eq.reset();
        for (auto* v : { &s.gain, &s.panL, &s.panR, &s.send }) v->reset (sr, 0.02);
    }
    masterEq.reset(); masterConsole.reset();
    busComp.sr = limiter.sr = masterTape.sr = sr;
    busComp.reset(); limiter.reset(); masterTape.reset();
    masterGain.reset (sr, 0.02);
    reverb.setSampleRate (sr);
    reverb.reset();
    delay.prepare (sr);
    lastRevType = -1;
}

void MixerEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::ScopedNoDenormals noDenormals;
    int pos = 0;
    while (pos < numSamples)
    {
        const int n = juce::jmin (kChunk, numSamples - pos);
        const float* inPtrs[kMaxOut] {};
        float* outPtrs[kMaxOut] {};
        const int ni = juce::jmin (numIn, kMaxOut), no = juce::jmin (numOut, kMaxOut);
        for (int c = 0; c < ni; ++c) inPtrs[c] = in[c] != nullptr ? in[c] + pos : nullptr;
        for (int c = 0; c < no; ++c) outPtrs[c] = out[c] != nullptr ? out[c] + pos : nullptr;
        processChunk (inPtrs, ni, outPtrs, no, n);
        pos += n;
    }
}

void MixerEngine::processStrip (int idx, float* const* d, int nch, int n)
{
    auto& p = ch[(size_t) idx];
    auto& s = dsp[(size_t) idx];
    const double sr = sampleRate.load();
    auto& m = meters[(size_t) idx];

    const float g = vdsp::dbToGain (p.trim->get()) * (p.phase->on() ? -1.0f : 1.0f);
    if (std::abs (g - 1.0f) > 1.0e-6f)
        for (int c = 0; c < nch; ++c) juce::FloatVectorOperations::multiply (d[c], g, n);

    if (p.hpfOn->on())
    {
        s.hpf1.highPass (sr, p.hpfFreq->get(), 0.5412); s.hpf1.process (d, nch, n);
        s.hpf2.highPass (sr, p.hpfFreq->get(), 1.3066); s.hpf2.process (d, nch, n);
    }
    if (p.lpfOn->on()) { s.lpf.lowPass (sr, p.lpfFreq->get()); s.lpf.process (d, nch, n); }

    s.console.process (sr, p.conLo->get(), p.conMid->get(), p.conHi->get(), d, nch, n);

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        switch (p.slot[slot]->index())
        {
            case modEq:
                s.eq.process (sr, p.eq, d, nch, n);
                break;
            case modComp:
                if (p.comp.on->on())
                    MeterState::pushMin (m.gr, s.comp.process (d, nch, n, p.comp.thr->get(), p.comp.ratio->get(), p.comp.atk->get(),
                                                              p.comp.rel->get(), p.comp.makeup->get(), p.comp.peak->on()));
                break;
            case modGate:
                if (p.gate.on->on())
                    MeterState::pushMin (m.gate, s.gate.process (d, nch, n, p.gate.thr->get(), p.gate.range->get(),
                                                                p.gate.hold->get(), p.gate.rel->get(), p.gate.exp->on()));
                break;
            default: break;
        }
    }

    if (const int t = p.tape->index(); t > 0)
        s.tape.process (d, nch, n, t - 1, 0.0f, 3.0f, master.tapeFormula->index());

    // console VU (pre-fader)
    float sum = 0.0f;
    for (int c = 0; c < nch; ++c)
        for (int k = 0; k < n; ++k) sum += d[c][k] * d[c][k];
    MeterState::pushMax (m.vu, std::sqrt (sum / (float) juce::jmax (1, n * nch)));
}

void MixerEngine::mixStrip (int idx, float* const* d, int nch, int n, bool anySolo, int numOut)
{
    auto& p = ch[(size_t) idx];
    auto& s = dsp[(size_t) idx];

    const bool audible = ! p.mute->on() && (! anySolo || p.solo->on());
    const float fdb = p.fader->get();
    s.gain.setTargetValue (audible && fdb > -69.95f ? vdsp::dbToGain (fdb) : 0.0f);

    const float pan = p.pan->get();
    if (nch == 1)
    {
        const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        s.panL.setTargetValue (std::cos (a) * 1.41421f);
        s.panR.setTargetValue (std::sin (a) * 1.41421f);
    }
    else
    {
        s.panL.setTargetValue (pan > 0 ? 1.0f - pan : 1.0f);
        s.panR.setTargetValue (pan < 0 ? 1.0f + pan : 1.0f);
    }

    const int sendDest = isFxStrip (idx) ? 0 : p.sendDest->index();
    const float sdb = p.sendLevel->get();
    s.send.setTargetValue (sendDest > 0 && sdb > -69.95f ? vdsp::dbToGain (sdb) : 0.0f);
    auto& sendBus = sendDest == 2 ? dlyBus : revBus;

    float* dstL = mix.getWritePointer (0);
    float* dstR = mix.getWritePointer (1);
    if (const int o = p.outDest->index(); o > 0 && 2 * o + 1 < numOut)
    {
        dstL = direct.getWritePointer (2 * o);
        dstR = direct.getWritePointer (2 * o + 1);
    }

    float* sL = sendBus.getWritePointer (0);
    float* sR = sendBus.getWritePointer (1);
    float pkL = 0, pkR = 0;
    const float* inL = d[0];
    const float* inR = nch > 1 ? d[1] : d[0];
    for (int k = 0; k < n; ++k)
    {
        const float gg = s.gain.getNextValue();
        const float l = inL[k] * gg * s.panL.getNextValue();
        const float r = inR[k] * gg * s.panR.getNextValue();
        const float sg = s.send.getNextValue();
        dstL[k] += l; dstR[k] += r;
        if (sendDest > 0) { sL[k] += l * sg; sR[k] += r * sg; }
        pkL = juce::jmax (pkL, std::abs (l));
        pkR = juce::jmax (pkR, std::abs (r));
    }
    MeterState::pushMax (meters[(size_t) idx].l, pkL);
    MeterState::pushMax (meters[(size_t) idx].r, pkR);
}

void MixerEngine::processChunk (const float* const* in, int numIn, float* const* out, int numOut, int n)
{
    mix.clear (0, n); revBus.clear (0, n); dlyBus.clear (0, n);
    for (int c = 0; c < numOut; ++c) direct.clear (c, 0, n);

    bool anySolo = false;
    for (auto& c : ch) anySolo = anySolo || c.solo->on();
    const bool recording = recorder.isActive();
    const double sr = sampleRate.load();

    auto recordStrip = [&] (int i, const float* const* src, int nch)
    {
        const int recCh = recording ? recorder.channelsFor (i) : 0;
        if (recCh == 0) return;
        if (nch == 0) { const float* z[2] { silence.data(), silence.data() }; recorder.write (i, z, n); return; }
        const float* ptrs[2] { src[0], nch > 1 ? src[1] : src[0] };
        recorder.write (i, ptrs, n);
    };

    // ---- system audio (Windows playback), read once per block
    bool sysRead = false;

    // ---- input channels
    for (int i = 0; i < kNumInputs; ++i)
    {
        auto& p = ch[(size_t) i];
        const int mode = p.srcMode->index(), first = p.srcIndex->index();
        int nch = 0;
        if (mode == 3)
        {
            if (! sysRead) { sysAudio.read (sysBuf.getWritePointer (0), sysBuf.getWritePointer (1), n); sysRead = true; }
            nch = 2;
            for (int c = 0; c < 2; ++c) { work.copyFrom (c, 0, sysBuf, c, 0, n); dry.copyFrom (c, 0, sysBuf, c, 0, n); }
        }
        else
        {
            nch = mode == 1 ? 1 : (mode == 2 ? 2 : 0);
            if (first + nch > numIn) nch = 0;
            for (int c = 0; c < nch; ++c) if (in[first + c] == nullptr) nch = 0;
            for (int c = 0; c < nch; ++c) { work.copyFrom (c, 0, in[first + c], n); dry.copyFrom (c, 0, in[first + c], n); }
        }

        if (nch == 0) { recordStrip (i, nullptr, 0); continue; }

        float* d[2] { work.getWritePointer (0), work.getWritePointer (1) };
        processStrip (i, d, nch, n);

        const float* drySrc[2] { dry.getReadPointer (0), dry.getReadPointer (1) };
        const float* postSrc[2] { d[0], d[1] };
        recordStrip (i, p.recDry->index() == 1 ? drySrc : postSrc, nch);
        mixStrip (i, d, nch, n, anySolo, numOut);
    }

    // ---- FX returns
    {
        const float size = fx.revSize->get(), damp = fx.revDamp->get(), width = fx.revWidth->get();
        if (size != lastRevSize || damp != lastRevDamp || width != lastRevWidth)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = size; rp.damping = damp; rp.width = width;
            rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
            reverb.setParameters (rp);
            lastRevSize = size; lastRevDamp = damp; lastRevWidth = width;
        }
        reverb.processStereo (revBus.getWritePointer (0), revBus.getWritePointer (1), n);

        const float beats = kDelayBeats[juce::jlimit (0, 5, fx.dlyDiv->index())];
        float* dl[2] { dlyBus.getWritePointer (0), dlyBus.getWritePointer (1) };
        delay.process (dl, n, beats * 60.0f / juce::jmax (40.0f, master.bpm->get()), fx.dlyFb->get(), fx.dlyTone->get(), fx.dlyPing->on());

        for (int i : { kReverbStrip, kDelayStrip })
        {
            auto& bus = i == kReverbStrip ? revBus : dlyBus;
            for (int c = 0; c < 2; ++c) { work.copyFrom (c, 0, bus, c, 0, n); dry.copyFrom (c, 0, bus, c, 0, n); }
            float* d[2] { work.getWritePointer (0), work.getWritePointer (1) };
            processStrip (i, d, 2, n);
            const float* drySrc[2] { dry.getReadPointer (0), dry.getReadPointer (1) };
            const float* postSrc[2] { d[0], d[1] };
            recordStrip (i, ch[(size_t) i].recDry->index() == 1 ? drySrc : postSrc, 2);
            mixStrip (i, d, 2, n, anySolo, numOut);
        }
    }

    // ---- Master: console -> tape machine -> inserts -> fader
    float* md[2] { mix.getWritePointer (0), mix.getWritePointer (1) };
    masterConsole.process (sr, master.conLo->get(), master.conMid->get(), master.conHi->get(), md, 2, n);

    const bool tapeOn = master.tapeOn->on();
    tapeRolling = tapeOn;
    if (tapeOn && ! master.tapeBypass->on())
        masterTape.process (md, 2, n, master.tapeIps->index(), master.tapeDrive->get(),
                            kCalDb[juce::jlimit (0, 2, master.tapeCal->index())], master.tapeFormula->index());

    for (int slot = 0; slot < kNumSlots; ++slot)
    {
        switch (master.slot[slot]->index())
        {
            case mModEq: masterEq.process (sr, master.eq, md, 2, n); break;
            case mModBusComp:
                if (master.busOn->on())
                {
                    const float rel = kBusRels[juce::jlimit (0, 4, master.busRel->index())];
                    MeterState::pushMin (masterMeter.gr,
                        busComp.process (md, 2, n, master.busThr->get(), kBusRatios[juce::jlimit (0, 2, master.busRatio->index())],
                                         kBusAttacks[juce::jlimit (0, 5, master.busAtk->index())], rel < 0 ? 300.0f : rel,
                                         master.busMakeup->get(), false, rel < 0));
                }
                break;
            case mModLimiter:
                if (master.limOn->on())
                    MeterState::pushMin (masterMeter.lim, limiter.process (md, 2, n, master.limCeiling->get()));
                break;
            default: break;
        }
    }

    float sum = 0.0f;
    const float mfdb = master.fader->get();
    masterGain.setTargetValue (mfdb > -69.95f ? vdsp::dbToGain (mfdb) : 0.0f);
    float pkL = 0, pkR = 0;
    for (int k = 0; k < n; ++k)
    {
        const float g = masterGain.getNextValue();
        md[0][k] *= g; md[1][k] *= g;
        pkL = juce::jmax (pkL, std::abs (md[0][k]));
        pkR = juce::jmax (pkR, std::abs (md[1][k]));
        sum += md[0][k] * md[0][k] + md[1][k] * md[1][k];
    }
    MeterState::pushMax (masterMeter.l, pkL);
    MeterState::pushMax (masterMeter.r, pkR);
    MeterState::pushMax (masterMeter.vu, std::sqrt (sum / (float) juce::jmax (1, 2 * n)));

    if (recording && recorder.channelsFor (kMixTrack) > 0)
    {
        const float* ptrs[2] { md[0], md[1] };
        recorder.write (kMixTrack, ptrs, n);
    }
    recorder.advance (n);

    // ---- outputs
    const int mp = master.outPair->index();
    const bool monitor = ! master.mute->on();
    for (int c = 0; c < numOut; ++c)
    {
        if (out[c] == nullptr) continue;
        juce::FloatVectorOperations::copy (out[c], direct.getReadPointer (c), n);
        if (monitor && numOut == 1 && c == 0)
            for (int k = 0; k < n; ++k) out[c][k] += 0.5f * (md[0][k] + md[1][k]);
        else if (monitor && (c == 2 * mp || c == 2 * mp + 1))
            juce::FloatVectorOperations::add (out[c], md[c - 2 * mp], n);
    }
}

bool MixerEngine::startRecording (const juce::File& takeFolder, juce::StringArray& createdFiles, juce::String& error)
{
    std::vector<Recorder<kNumStrips + 1>::TrackSpec> specs;
    for (int i = 0; i < kNumStrips; ++i)
    {
        auto& p = ch[(size_t) i];
        if (! p.recArm->on()) continue;
        const int mode = isFxStrip (i) ? 2 : p.srcMode->index();
        if (mode == 0) continue;
        const auto fname = juce::String (i + 1).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (names[(size_t) i]) + ".wav";
        specs.push_back ({ i, mode >= 2 ? 2 : 1, takeFolder.getChildFile (fname) });
    }
    if (master.recMix->on())
        specs.push_back ({ kMixTrack, 2, takeFolder.getChildFile ("Mix.wav") });

    if (specs.empty())
    {
        error = "Nothing to record. Arm a channel with its Rec button (and give it an input), or turn on REC MIX on the master.";
        return false;
    }
    if (! recorder.start (specs, sampleRate.load(), error))
        return false;
    for (auto& s : specs) createdFiles.add (s.file.getFullPathName());
    return true;
}
