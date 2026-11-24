#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace vdsp
{
inline float dbToGain (float db) noexcept   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept    { return 20.0f * std::log10 (g + 1.0e-9f); }
inline float coef (double sr, float ms) noexcept
{
    return ms <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * sr));
}

//==============================================================================
// RBJ biquad, transposed direct form II, up to 2 channels.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    static double clampF (double sr, double f) { return juce::jlimit (10.0, sr * 0.45, f); }

    void peak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * clampF (sr, f) / sr;
        const double cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        setNormalised (1 + alpha * A, -2 * cs, 1 - alpha * A, 1 + alpha / A, -2 * cs, 1 - alpha / A);
    }

    void lowShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * clampF (sr, f) / sr;
        const double cs = std::cos (w), sA = 2.0 * std::sqrt (A) * (std::sin (w) / std::sqrt (2.0));
        setNormalised (A * ((A + 1) - (A - 1) * cs + sA), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sA),
                       (A + 1) + (A - 1) * cs + sA, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sA);
    }

    void highShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * clampF (sr, f) / sr;
        const double cs = std::cos (w), sA = 2.0 * std::sqrt (A) * (std::sin (w) / std::sqrt (2.0));
        setNormalised (A * ((A + 1) + (A - 1) * cs + sA), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sA),
                       (A + 1) - (A - 1) * cs + sA, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sA);
    }

    void highPass (double sr, double f, double q = 0.7071)
    {
        const double w = juce::MathConstants<double>::twoPi * clampF (sr, f) / sr, cs = std::cos (w), alpha = std::sin (w) / (2 * q);
        setNormalised ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
    }

    void lowPass (double sr, double f, double q = 0.7071)
    {
        const double w = juce::MathConstants<double>::twoPi * clampF (sr, f) / sr, cs = std::cos (w), alpha = std::sin (w) / (2 * q);
        setNormalised ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + alpha, -2 * cs, 1 - alpha);
    }

    void process (float* const* d, int nch, int n) noexcept
    {
        for (int c = 0; c < nch; ++c)
        {
            float s1 = z1[c], s2 = z2[c];
            auto* x = d[c];
            for (int i = 0; i < n; ++i)
            {
                const float in = x[i];
                const float y = b0 * in + s1;
                s1 = b1 * in - a1 * y + s2;
                s2 = b2 * in - a2 * y;
                x[i] = y;
            }
            z1[c] = s1; z2[c] = s2;
        }
    }
};

//==============================================================================
// Feed-forward, stereo-linked compressor with soft knee.
struct Compressor
{
    double sr = 48000.0;
    float env = 0.0f, envSlow = 0.0f, rms = 0.0f;

    void reset() noexcept { env = envSlow = rms = 0.0f; }

    // returns the deepest gain reduction (dB, <= 0) in this block
    float process (float* const* d, int nch, int n, float thrDb, float ratio, float atkMs, float relMs,
                   float makeupDb, bool peakMode, bool autoRelease = false) noexcept
    {
        const float a = coef (sr, atkMs), r = coef (sr, relMs), rc = coef (sr, 10.0f);
        const float rFast = coef (sr, 120.0f), rSlow = coef (sr, 1200.0f);
        const float slope = 1.0f / juce::jmax (1.0f, ratio) - 1.0f, knee = 6.0f;
        float deepest = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float x = std::abs (d[0][i]);
            if (nch > 1) x = juce::jmax (x, std::abs (d[1][i]));

            float lvl;
            if (peakMode) lvl = x;
            else { rms = rc * rms + (1.0f - rc) * x * x; lvl = std::sqrt (rms) * 1.41421f; }

            const float over = gainToDb (lvl) - thrDb;
            float tgt;
            if (2.0f * over < -knee)      tgt = 0.0f;
            else if (2.0f * over > knee)  tgt = slope * over;
            else { const float t = over + knee * 0.5f; tgt = slope * t * t / (2.0f * knee); }

            float gr;
            if (! autoRelease)
            {
                env = tgt < env ? a * env + (1.0f - a) * tgt : r * env + (1.0f - r) * tgt;
                gr = env;
            }
            else
            {
                env     = tgt < env     ? a * env     + (1.0f - a) * tgt : rFast * env     + (1.0f - rFast) * tgt;
                envSlow = tgt < envSlow ? a * envSlow + (1.0f - a) * tgt : rSlow * envSlow + (1.0f - rSlow) * tgt;
                gr = 0.6f * env + 0.4f * envSlow;   // program-dependent dual release
            }

            deepest = juce::jmin (deepest, gr);
            const float g = dbToGain (gr + makeupDb);
            for (int c = 0; c < nch; ++c) d[c][i] *= g;
        }
        return deepest;
    }
};

//==============================================================================
// Gate / 2:1 downward expander with hold.
struct Gate
{
    double sr = 48000.0;
    float det = 0.0f, gDb = 0.0f;
    int hold = 0;

    void reset() noexcept { det = 0.0f; gDb = 0.0f; hold = 0; }

    float process (float* const* d, int nch, int n, float thrDb, float rangeDb, float holdMs, float relMs, bool expander) noexcept
    {
        const float dr = coef (sr, 20.0f), atk = coef (sr, 0.3f), rel = coef (sr, relMs);
        const int holdSamples = (int) (holdMs * 0.001 * sr);
        float deepest = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float x = std::abs (d[0][i]);
            if (nch > 1) x = juce::jmax (x, std::abs (d[1][i]));
            det = x > det ? x : dr * det + (1.0f - dr) * x;
            const float xdb = gainToDb (det);

            float tgt;
            if (xdb >= thrDb)    { tgt = 0.0f; hold = holdSamples; }
            else if (hold > 0)   { --hold; tgt = 0.0f; }
            else                 tgt = expander ? juce::jmax (-rangeDb, xdb - thrDb) : -rangeDb;

            gDb = tgt > gDb ? atk * gDb + (1.0f - atk) * tgt : rel * gDb + (1.0f - rel) * tgt;
            deepest = juce::jmin (deepest, gDb);
            const float g = dbToGain (gDb);
            for (int c = 0; c < nch; ++c) d[c][i] *= g;
        }
        return deepest;
    }
};

//==============================================================================
// Tape stage: head bump + HF loss per speed, then asymmetric saturation.
struct Tape
{
    double sr = 48000.0;
    Biquad bump, hfLoss;
    int lastIps = -1, lastFormula = -1;
    double lastSr = 0;

    void reset() noexcept { bump.reset(); hfLoss.reset(); lastIps = -1; }

    // ips: 0 = 15 ips, 1 = 30 ips. formula: 0 = 456, 1 = 499, 2 = GP9. calDb: 3, 6 or 9.
    void process (float* const* d, int nch, int n, int ips, float driveDb, float calDb, int formula) noexcept
    {
        if (ips != lastIps || sr != lastSr)
        {
            if (ips == 0) { bump.peak (sr, 55.0, 1.2, 2.0);  hfLoss.highShelf (sr, 12000.0, -2.5); }
            else          { bump.peak (sr, 40.0, 1.0, 1.2);  hfLoss.highShelf (sr, 16000.0, -1.0); }
            lastIps = ips; lastSr = sr;
        }
        bump.process (d, nch, n);
        hfLoss.process (d, nch, n);

        static const float satAmount[] { 1.0f, 0.8f, 0.6f };   // 456 is the most colourful
        static const float bias[]      { 0.08f, 0.05f, 0.03f };
        const int f = juce::jlimit (0, 2, formula);
        const float drive = dbToGain (driveDb - (calDb - 3.0f)) * satAmount[f] * 0.7f;
        const float b = bias[f], tb = std::tanh (b);
        // dividing by drive keeps the small-signal level at unity; louder peaks round off
        for (int c = 0; c < nch; ++c)
        {
            auto* x = d[c];
            for (int i = 0; i < n; ++i)
                x[i] = (std::tanh (x[i] * drive + b) - tb) / drive;
        }
    }
};

//==============================================================================
// Simple peak limiter (instant attack, smooth release).
struct Limiter
{
    double sr = 48000.0;
    float g = 1.0f;
    void reset() noexcept { g = 1.0f; }
    float process (float* const* d, int nch, int n, float ceilingDb) noexcept
    {
        const float ceil = dbToGain (ceilingDb), rel = coef (sr, 80.0f);
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float pk = std::abs (d[0][i]);
            if (nch > 1) pk = juce::jmax (pk, std::abs (d[1][i]));
            const float need = pk > ceil ? ceil / pk : 1.0f;
            g = need < g ? need : rel * g + (1.0f - rel) * 1.0f;
            g = juce::jmin (g, need);
            minG = juce::jmin (minG, g);
            for (int c = 0; c < nch; ++c) d[c][i] *= g;
        }
        return gainToDb (minG);
    }
};

//==============================================================================
// Tempo-synced stereo delay with feedback tone and optional ping-pong.
struct StereoDelay
{
    double sr = 48000.0;
    juce::AudioBuffer<float> buf;
    int write = 0;
    float lpState[2] {};

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        buf.setSize (2, (int) (sr * 4.0) + 8);
        buf.clear();
        write = 0; lpState[0] = lpState[1] = 0.0f;
    }

    void process (float* const* io, int n, float delaySec, float feedback, float toneHz, bool pingPong) noexcept
    {
        const int size = buf.getNumSamples();
        if (size < 16) return;
        const int dl = juce::jlimit (1, size - 2, (int) (delaySec * sr));
        const float lp = std::exp (-juce::MathConstants<float>::twoPi * toneHz / (float) sr);
        auto* bl = buf.getWritePointer (0);
        auto* br = buf.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            int r = write - dl; if (r < 0) r += size;
            float yl = bl[r], yr = br[r];
            lpState[0] = lp * lpState[0] + (1.0f - lp) * yl;
            lpState[1] = lp * lpState[1] + (1.0f - lp) * yr;
            const float inL = io[0][i], inR = io[1][i];
            if (pingPong)
            {
                bl[write] = 0.5f * (inL + inR) + feedback * lpState[1];
                br[write] = feedback * lpState[0];
            }
            else
            {
                bl[write] = inL + feedback * lpState[0];
                br[write] = inR + feedback * lpState[1];
            }
            io[0][i] = yl; io[1][i] = yr;
            if (++write >= size) write = 0;
        }
    }
};
} // namespace vdsp
