#pragma once
#include "Params.h"
#include "DSP.h"
#include "Recorder.h"

constexpr int kNumInputs  = 8;                 // input channels
constexpr int kReverbStrip = kNumInputs;       // FX return strips
constexpr int kDelayStrip  = kNumInputs + 1;
constexpr int kNumStrips  = kNumInputs + 2;
constexpr int kMixTrack   = kNumStrips;        // recorder slot for the stereo mix
constexpr int kNumSlots   = 3;

// Insert module types
enum ChannelModule { modEmpty = 0, modEq, modComp, modGate };
enum MasterModule  { mModEmpty = 0, mModEq, mModBusComp, mModLimiter };

struct EqParams   { Param *on, *hfGain, *hfFreq, *hfBell, *hmfGain, *hmfFreq, *hmfQ, *lmfGain, *lmfFreq, *lmfQ, *lfGain, *lfFreq, *lfBell; };
struct CompParams { Param *on, *peak, *thr, *ratio, *atk, *rel, *makeup; };
struct GateParams { Param *on, *exp, *thr, *range, *hold, *rel; };

struct ChannelParams
{
    Param *srcMode, *srcIndex, *trim, *phase, *hpfOn, *hpfFreq, *lpfOn, *lpfFreq;
    Param *conHi, *conMid, *conLo, *tape;
    Param* slot[kNumSlots];
    EqParams eq; CompParams comp; GateParams gate;
    Param *sendDest, *sendLevel, *outDest, *recDry;
    Param *pan, *fader, *mute, *solo, *recArm, *colour;
};

struct FxParams
{
    Param *revType, *revSize, *revDamp, *revWidth;
    Param *dlyDiv, *dlyFb, *dlyTone, *dlyPing;
};

struct MasterParams
{
    Param *outPair, *conHi, *conMid, *conLo;
    Param* slot[kNumSlots];
    EqParams eq;
    Param *busOn, *busThr, *busRatio, *busAtk, *busRel, *busMakeup;
    Param *limOn, *limCeiling;
    Param *tapeOn, *tapeBypass, *tapeIps, *tapeDrive, *tapeCal, *tapeFormula;
    Param *fader, *mute, *recMix, *bpm;
};

struct MeterState
{
    std::atomic<float> l { 0 }, r { 0 }, vu { 0 }, gr { 0 }, gate { 0 }, lim { 0 };
    static void pushMax (std::atomic<float>& a, float v) noexcept { if (v > a.load()) a.store (v); }
    static void pushMin (std::atomic<float>& a, float v) noexcept { if (v < a.load()) a.store (v); }
};

class MixerEngine : public juce::AudioIODeviceCallback
{
public:
    MixerEngine();

    ParamSet params;
    std::array<ChannelParams, kNumStrips> ch;
    FxParams fx;
    MasterParams master;
    std::array<juce::String, kNumStrips> names;

    std::array<MeterState, kNumStrips> meters;
    MeterState masterMeter;
    std::atomic<bool> tapeRolling { false };

    Recorder<kNumStrips + 1> recorder;

    static bool isFxStrip (int i) noexcept { return i >= kNumInputs; }

    // Put a module in a slot (message thread). A module lives in one slot only, so it is moved if already used.
    void setChannelSlot (int strip, int slot, int module);
    void setMasterSlot (int slot, int module);
    void resetToDefaults();

    bool startRecording (const juce::File& takeFolder, juce::StringArray& createdFiles, juce::String& error);
    void stopRecording() { recorder.stop(); }
    double getSampleRate() const noexcept { return sampleRate.load(); }

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void prepare (double sampleRate);

private:
    struct EqDSP
    {
        vdsp::Biquad lf, lmf, hmf, hf;
        void reset() { lf.reset(); lmf.reset(); hmf.reset(); hf.reset(); }
        void process (double sr, const EqParams& p, float* const* d, int nch, int n);
    };
    struct ConsoleDSP
    {
        vdsp::Biquad lo, mid, hi;
        void reset() { lo.reset(); mid.reset(); hi.reset(); }
        void process (double sr, float loDb, float midDb, float hiDb, float* const* d, int nch, int n);
    };
    struct StripDSP
    {
        vdsp::Biquad hpf1, hpf2, lpf;
        ConsoleDSP console;
        EqDSP eq;
        vdsp::Gate gate;
        vdsp::Compressor comp;
        vdsp::Tape tape;
        juce::LinearSmoothedValue<float> gain, panL, panR, send;
    };

    void processChunk (const float* const* in, int numIn, float* const* out, int numOut, int n);
    void processStrip (int idx, float* const* d, int nch, int n);
    void mixStrip (int idx, float* const* d, int nch, int n, bool anySolo, int numOut);

    std::atomic<double> sampleRate { 48000.0 };
    static constexpr int kChunk = 1024, kMaxOut = 64;
    juce::AudioBuffer<float> work { 2, kChunk }, dry { 2, kChunk }, mix { 2, kChunk },
                             revBus { 2, kChunk }, dlyBus { 2, kChunk }, direct { kMaxOut, kChunk };
    std::array<float, kChunk> silence {};
    std::array<StripDSP, kNumStrips> dsp;

    EqDSP masterEq;
    ConsoleDSP masterConsole;
    vdsp::Compressor busComp;
    vdsp::Limiter limiter;
    vdsp::Tape masterTape;
    juce::LinearSmoothedValue<float> masterGain;

    juce::Reverb reverb;
    vdsp::StereoDelay delay;
    int lastRevType = -1;
    float lastRevSize = -1, lastRevDamp = -1, lastRevWidth = -1;
};
