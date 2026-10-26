#pragma once
#include "ModuleDefs.h"
#include <atomic>
#include <memory>
#include <cmath>
#include <algorithm>

namespace bh
{
inline float dbToLin (float d) { return std::pow (10.f, d * 0.05f); }
inline float linToDb (float l) { return 20.f * std::log10 (std::max (l, 1.0e-9f)); }
inline float tcCoef (float ms, double sr) { return 1.f - std::exp (-1.f / (0.001f * std::max (ms, 0.01f) * (float) sr)); }

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline float process (int c, float x)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    static double clampF (double sr, double f) { return std::min (std::max (f, 10.0), sr * 0.45); }

    void peaking (double sr, double f, double gDb, double q)
    {
        f = clampF (sr, f);
        const double A = std::pow (10.0, gDb / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
        const double al = std::sin (w) / (2 * q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double sr, double f, double gDb, double q)
    {
        f = clampF (sr, f);
        const double A = std::pow (10.0, gDb / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
        const double al = std::sin (w) / (2 * q), c = std::cos (w), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (double sr, double f, double gDb, double q)
    {
        f = clampF (sr, f);
        const double A = std::pow (10.0, gDb / 40.0), w = 2 * juce::MathConstants<double>::pi * f / sr;
        const double al = std::sin (w) / (2 * q), c = std::cos (w), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void highpass (double sr, double f, double q)
    {
        f = clampF (sr, f);
        const double w = 2 * juce::MathConstants<double>::pi * f / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double sr, double f, double q)
    {
        f = clampF (sr, f);
        const double w = 2 * juce::MathConstants<double>::pi * f / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
};

struct DcBlock
{
    float x1[2] = { 0, 0 }, y1[2] = { 0, 0 }, r = 0.999f;
    void prepare (double sr) { r = 1.f - 8.f * 2.f * juce::MathConstants<float>::pi / (float) sr; }
    void reset() { x1[0] = x1[1] = y1[0] = y1[1] = 0; }
    inline float process (int c, float x) { const float y = x - x1[c] + r * y1[c]; x1[c] = x; y1[c] = y; return y; }
};

// soft knee gain computer: returns gain reduction in dB (>= 0)
inline float gainComputer (float lvl, float thr, float ratio, float knee)
{
    const float over = lvl - thr;
    if (knee > 0.01f && 2.f * over > -knee && 2.f * over < knee)
    {
        const float t = over + knee * 0.5f;
        return (1.f - 1.f / ratio) * t * t / (2.f * knee);
    }
    return over > 0 ? over * (1.f - 1.f / ratio) : 0.f;
}

//==============================================================================
struct SlotVals { float k[kKnobs] {}; int w[kSw] {}; int style = 0; };

struct Engine
{
    virtual ~Engine() = default;
    virtual void prepare (double sampleRate) { fs = sampleRate; reset(); }
    virtual void reset() {}
    virtual void process (float** ch, int nch, int n, const SlotVals& v) = 0;

    std::atomic<float> gr { 0.f };   // gain reduction for the meters, dB (positive)
    double fs = 44100.0;
};

//==============================================================================
struct ParamEQEngine : Engine
{
    Biquad hp, mud, pres;
    float last[5] = { -1, -1, -1, -1, -1 };

    void reset() override { hp.reset(); mud.reset(); pres.reset(); for (auto& l : last) l = -1; }
    void process (float** ch, int nch, int n, const SlotVals& v) override
    {
        bool ch_ = false;
        for (int i = 0; i < 5; ++i) if (v.k[i] != last[i]) { ch_ = true; last[i] = v.k[i]; }
        if (ch_)
        {
            hp.highpass (fs, v.k[0], 0.7071);
            mud.peaking (fs, v.k[2], v.k[1], 1.4);
            pres.peaking (fs, v.k[4], v.k[3], 0.9);
        }
        const bool eh = v.w[0] & 1, em = v.w[0] & 2, ep = v.w[0] & 4;
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                if (eh) x = hp.process (c, x);
                if (em) x = mud.process (c, x);
                if (ep) x = pres.process (c, x);
                ch[c][i] = x;
            }
    }
};

//==============================================================================
// Split-band / wideband de-esser, level independent (sibilance is judged against the full signal)
struct DeEsserEngine : Engine
{
    Biquad bp; float lastF = -1; float envB = 0, envW = 0, g = 0;
    void reset() override { bp.reset(); envB = envW = g = 0; lastF = -1; gr = 0; }
    void process (float** ch, int nch, int n, const SlotVals& v) override
    {
        if (v.k[0] != lastF) { lastF = v.k[0]; bp.bandpass (fs, lastF, 1.4); }
        const bool listen = v.w[0] & 1, wide = v.w[0] & 2;
        const float aA = tcCoef (0.4f, fs), aR = tcCoef (35.f, fs), gA = tcCoef (1.f, fs), gR = tcCoef (40.f, fs);
        const float slope = juce::jlimit (0.f, 2.f, std::abs (v.k[1]) / 6.f), range = v.k[2];
        float grAvg = 0;
        for (int i = 0; i < n; ++i)
        {
            float band[2] = { 0, 0 }, pk = 0, bpk = 0;
            for (int c = 0; c < nch; ++c) { band[c] = bp.process (c, ch[c][i]); bpk = std::max (bpk, std::abs (band[c])); pk = std::max (pk, std::abs (ch[c][i])); }
            envB += (bpk > envB ? aA : aR) * (bpk - envB);
            envW += (pk > envW ? aA : aR) * (pk - envW);
            const float over = linToDb (envB) - (linToDb (envW) - 10.f);
            const float target = (envW > 1.0e-4f) ? juce::jlimit (0.f, range, over * slope) : 0.f;
            g += (target > g ? gA : gR) * (target - g);
            const float gl = dbToLin (-g);
            for (int c = 0; c < nch; ++c)
            {
                const float x = ch[c][i];
                ch[c][i] = listen ? band[c] : (wide ? x * gl : x - band[c] * (1.f - gl));
            }
            grAvg += g;
        }
        gr = n ? grAvg / (float) n : 0.f;
    }
};

//==============================================================================
// 1176-style FET limiting amplifier
struct FET76Engine : Engine
{
    float g = 0; DcBlock dc;
    void prepare (double sr) override { Engine::prepare (sr); dc.prepare (sr); }
    void reset() override { g = 0; gr = 0; dc.reset(); }
    void process (float** ch, int nch, int n, const SlotVals& v) override
    {
        static const float ratios[5] = { 20.f, 12.f, 8.f, 4.f, 20.f };
        static const float satAmt[5] = { 1.0f, 0.8f, 0.7f, 0.6f, 0.35f };
        const int ri = juce::jlimit (0, 4, v.w[0]);
        const float ratio = ratios[ri], inG = dbToLin (v.k[0]), outG = dbToLin (v.k[2]), mix = v.k[1] * 0.01f;
        const float att = 800.f * std::pow (20.f / 800.f, (v.k[3] - 1.f) / 6.f) * (ri == 4 ? 1.5f : 1.f);   // microseconds
        const float rel = 1100.f * std::pow (50.f / 1100.f, (v.k[4] - 1.f) / 6.f);                          // milliseconds
        const float aA = tcCoef (att * 0.001f, fs), aR = tcCoef (rel, fs);
        const float thr = ri == 4 ? -14.f : -10.f, sat = satAmt[juce::jlimit (0, 4, v.style)];
        float grAvg = 0;
        for (int i = 0; i < n; ++i)
        {
            float pk = 0;
            for (int c = 0; c < nch; ++c) pk = std::max (pk, std::abs (ch[c][i] * inG));
            const float target = gainComputer (linToDb (pk), thr, ratio, 6.f);
            g += (target > g ? aA : aR) * (target - g);
            const float gl = dbToLin (-g);
            for (int c = 0; c < nch; ++c)
            {
                const float dry = ch[c][i], x = dry * inG * gl;
                float y = x + sat * (std::tanh (x * 1.6f) / 1.6f - x) + 0.03f * sat * x * std::abs (x);
                y = dc.process (c, y) * outG;
                ch[c][i] = y * mix + dry * (1.f - mix);
            }
            grAvg += g;
        }
        gr = n ? grAvg / (float) n : 0.f;
    }
};

//==============================================================================
// Opto leveler, shared by the 2A and the 1B: two-stage release like a T4 / opto cell
struct OptoEngine : Engine
{
    bool is2A = true;
    float env = 0, gA = 0, gB = 0; Biquad emph; float lastE = -1; DcBlock dc;
    explicit OptoEngine (bool two) : is2A (two) {}
    void prepare (double sr) override { Engine::prepare (sr); dc.prepare (sr); }
    void reset() override { env = gA = gB = 0; gr = 0; emph.reset(); dc.reset(); lastE = -1; }
    void process (float** ch, int nch, int n, const SlotVals& v) override
    {
        float sc = 0, thr, ratio, knee, attA, attB, relA, relB, wA, makeup, emphDb = 0;
        if (is2A)
        {
            sc = v.k[0] * 0.25f; thr = -10.f; ratio = v.w[0] ? 10.f : 3.f; knee = 10.f;
            attA = 10.f; attB = 40.f; relA = 60.f; relB = 1500.f; wA = 0.6f; makeup = v.k[1]; emphDb = v.k[2] * 1.2f;
        }
        else
        {
            thr = v.k[0] + 2.f; ratio = v.k[1]; knee = 6.f;
            attA = v.k[2]; attB = v.k[2] * 4.f; relA = v.k[3] * 0.3f; relB = v.k[3] * 4.f; wA = 0.55f; makeup = v.k[4];
        }
        if (emphDb != lastE) { lastE = emphDb; emph.highShelf (fs, 3000.0, emphDb, 0.7); }
        const float cEnv = tcCoef (8.f, fs), cAA = tcCoef (attA, fs), cAB = tcCoef (attB, fs), cRA = tcCoef (relA, fs), cRB = tcCoef (relB, fs);
        const float outG = dbToLin (makeup);
        float grAvg = 0;
        for (int i = 0; i < n; ++i)
        {
            float pk = 0;
            for (int c = 0; c < nch; ++c) pk = std::max (pk, std::abs (emphDb > 0.01f ? emph.process (c, ch[c][i]) : ch[c][i]));
            env += cEnv * (pk - env);
            const float lvl = linToDb (env) + sc + (is2A ? 0.f : 3.f);
            const float t = gainComputer (lvl, thr, ratio, knee);
            gA += (t > gA ? cAA : cRA) * (t - gA);
            gB += (t > gB ? cAB : cRB) * (t - gB);
            const float g = wA * gA + (1.f - wA) * gB, gl = dbToLin (-g);
            for (int c = 0; c < nch; ++c)
            {
                const float x = ch[c][i] * gl;
                const float y = 0.5f * x + 0.5f * (std::tanh (x * 1.2f) / 1.2f);
                ch[c][i] = dc.process (c, y) * outG;
            }
            grAvg += g;
        }
        gr = n ? grAvg / (float) n : 0.f;
    }
};

//==============================================================================
// 670-style variable-mu tube limiter: the slope steepens the harder you push it
struct Tube670Engine : Engine
{
    float gA = 0, gB = 0; Biquad sc; int lastSc = -1; DcBlock dc;
    void prepare (double sr) override { Engine::prepare (sr); dc.prepare (sr); }
    void reset() override { gA = gB = 0; gr = 0; sc.reset(); dc.reset(); lastSc = -1; }
    void process (float** ch, int nch, int n, const SlotVals& v) override
    {
        static const float att[6] = { 0.2f, 0.2f, 0.4f, 0.8f, 0.4f, 0.4f };
        static const float rel[6] = { 300.f, 800.f, 2000.f, 5000.f, 2000.f, 10000.f };
        static const float scHz[5] = { 0.f, 50.f, 100.f, 200.f, 350.f };
        const int tc = juce::jlimit (0, 5, (int) v.k[3]), sci = juce::jlimit (0, 4, (int) v.k[4]);
        if (sci != lastSc) { lastSc = sci; if (sci) sc.highpass (fs, scHz[sci], 0.7071); }
        const float inG = dbToLin (v.k[0]), outG = dbToLin (v.k[5]);
        const float thr = -34.f + 3.2f * v.k[1], knee = 4.f + (v.k[2] + 1.f) * 3.f;
        const float cA = tcCoef (att[tc], fs), cRA = tcCoef (rel[tc], fs), cRB = tcCoef (tc >= 4 ? 10000.f : rel[tc], fs);
        const float drive = 0.8f + 0.06f * v.k[0];
        float grAvg = 0;
        for (int i = 0; i < n; ++i)
        {
            float pk = 0;
            for (int c = 0; c < nch; ++c) pk = std::max (pk, std::abs (sci ? sc.process (c, ch[c][i] * inG) : ch[c][i] * inG));
            const float over = linToDb (pk) - thr;
            const float t = over > 0 ? over - knee * std::log (1.f + over / knee) : 0.f;
            gA += (t > gA ? cA : cRA) * (t - gA);
            gB += (t > gB ? cA : cRB) * (t - gB);
            const float g = tc >= 4 ? 0.6f * gA + 0.4f * gB : gA, gl = dbToLin (-g);
            for (int c = 0; c < nch; ++c)
            {
                const float x = ch[c][i] * inG * gl;
                const float y = std::tanh (x * drive + 0.08f) - std::tanh (0.08f);
                ch[c][i] = dc.process (c, y / drive) * outG;
            }
            grAvg += g;
        }
        gr = n ? grAvg / (float) n : 0.f;
    }
};

//==============================================================================
// SNOBTECH: passive-style program EQ (mid-range and low/high variants)
struct SnobTechEngine : Engine
{
    Biquad b[4]; float last[kKnobs]; int lastStyle = -1;
    SnobTechEngine() { for (auto& l : last) l = -999; }
    void reset() override { for (auto& x : b) x.reset(); for (auto& l : last) l = -999; lastStyle = -1; }
    void process (float** ch, int nch, int n, const SlotVals& v) override
    {
        bool chg = lastStyle != v.style;
        for (int i = 0; i < kKnobs; ++i) if (v.k[i] != last[i]) { chg = true; last[i] = v.k[i]; }
        lastStyle = v.style;
        if (chg)
        {
            const auto& d = getDef (SnobTech, v.style);
            auto hz = [&] (int j) { return parseHz (d.knobs[(size_t) j].steps[juce::jlimit (0, d.knobs[(size_t) j].steps.size() - 1, (int) v.k[j])]); };
            if (v.style == 0)
            {
                b[0].peaking (fs, hz (0), v.k[1] * 1.1, 1.2);
                b[1].peaking (fs, hz (2), -v.k[3] * 1.0, 0.8);
                b[2].peaking (fs, hz (4), v.k[5] * 1.0, 1.1);
                b[3].peaking (fs, 1000, 0, 1);
            }
            else
            {
                const double lf = hz (0);
                b[0].lowShelf (fs, lf, v.k[1] * 1.2, 0.55);                 // boost shelf at the dial frequency
                b[1].lowShelf (fs, lf * 1.9, -v.k[2] * 1.4, 0.55);           // cut shelf a bit higher: the Pultec trick
                b[2].peaking (fs, hz (3), v.k[5] * 1.2, juce::jlimit (0.45, 3.0, 3.0 - v.k[4] * 0.25));
                static const double att[3] = { 5000, 10000, 20000 };
                b[3].highShelf (fs, att[juce::jlimit (0, 2, (int) v.k[6])], -v.k[7] * 1.0, 0.6);
            }
        }
        if (! (v.w[0] & 1)) return;
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                for (auto& q : b) x = q.process (c, x);
                ch[c][i] = 0.85f * x + 0.15f * std::tanh (x * 1.1f) / 1.1f;     // a touch of tube / transformer
            }
    }
};

//==============================================================================
// Maag-style air band: broad, silky top-end shelf
struct AirEQEngine : Engine
{
    Biquad s1, s2; float last[3] = { -1, -1, -1 }; int lastW = -1;
    void reset() override { s1.reset(); s2.reset(); for (auto& l : last) l = -1; }
    void process (float** ch, int nch, int n, const SlotVals& v) override
    {
        bool chg = lastW != v.w[0];
        for (int i = 0; i < 3; ++i) if (v.k[i] != last[i]) { chg = true; last[i] = v.k[i]; }
        lastW = v.w[0];
        const int fi = juce::jlimit (0, 5, (int) v.k[1]);
        if (chg && fi > 0)
        {
            static const double f[6] = { 0, 2500, 5000, 10000, 20000, 40000 };
            const double q = (v.w[0] & 2) ? 0.35 : 0.5;
            s1.highShelf (fs, f[fi] * 0.55, v.k[0] * 0.6, q);
            s2.highShelf (fs, f[fi] * 1.3, v.k[0] * 0.4, q);
        }
        const float tr = dbToLin (v.k[2]);
        const bool on = (v.w[0] & 1) && fi > 0;
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                if (on) x = s2.process (c, s1.process (c, x));
                ch[c][i] = x * tr;
            }
    }
};

inline std::unique_ptr<Engine> makeEngine (int type)
{
    switch (type)
    {
        case ParamEQ:  return std::make_unique<ParamEQEngine>();
        case DeEsser:  return std::make_unique<DeEsserEngine>();
        case FET76:    return std::make_unique<FET76Engine>();
        case Opto2A:   return std::make_unique<OptoEngine> (true);
        case Opto1B:   return std::make_unique<OptoEngine> (false);
        case Tube670:  return std::make_unique<Tube670Engine>();
        case SnobTech: return std::make_unique<SnobTechEngine>();
        case AirEQ:    return std::make_unique<AirEQEngine>();
        default:       return nullptr;
    }
}
} // namespace bh
