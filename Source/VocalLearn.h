#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace bh
{
struct VocalProfile
{
    bool valid = false;
    bool male = true;
    float f0 = 118.f;                       // median pitch, Hz
    float sibLo = 2.f, sibHi = 6.f, sibPk = 4.8f;   // kHz
    float dyn = 14.f;                       // dB between loud and quiet parts
    float spike = 1300.f; bool spikeFound = false;  // narrow resonance, Hz
    bool dull = true, room = false;
    float peakDb = -8.f, rmsDb = -20.f;
    bool beatValid = false; float bpm = 0.f, bassHz = 0.f; bool beatMuddy = false;
};

class VocalAnalyzer
{
public:
    static constexpr int order = 12, N = 1 << order, hop = N / 2;

    static VocalProfile analyze (const std::vector<float>& vox, const std::vector<float>& beat, double sr)
    {
        VocalProfile p;
        if ((int) vox.size() < N * 2) return p;

        juce::dsp::FFT fft (order);
        std::vector<float> win ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.f * juce::MathConstants<float>::pi * (float) i / (float) (N - 1));

        struct Frame { float rms; std::vector<float> mag; int start; };
        std::vector<Frame> frames;
        std::vector<float> buf ((size_t) N * 2);
        float peak = 0;
        for (float s : vox) peak = std::max (peak, std::abs (s));

        for (int st = 0; st + N <= (int) vox.size(); st += hop)
        {
            double e = 0;
            for (int i = 0; i < N; ++i) { const float x = vox[(size_t) (st + i)]; e += (double) x * x; buf[(size_t) i] = x * win[(size_t) i]; }
            std::fill (buf.begin() + N, buf.end(), 0.f);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            Frame f; f.rms = 10.f * std::log10 ((float) (e / N) + 1.0e-12f); f.start = st;
            f.mag.assign (buf.begin(), buf.begin() + N / 2 + 1);
            frames.push_back (std::move (f));
        }

        std::vector<float> r; for (auto& f : frames) r.push_back (f.rms);
        std::sort (r.begin(), r.end());
        auto pct = [&] (const std::vector<float>& v, float q) { return v[(size_t) juce::jlimit (0, (int) v.size() - 1, (int) (q * (float) (v.size() - 1)))]; };
        const float p90 = pct (r, 0.9f), p10 = pct (r, 0.1f);
        p.peakDb = 20.f * std::log10 (std::max (peak, 1.0e-6f));
        if (p90 < -60.f) return p;                         // nothing but silence

        const float gate = std::max (-55.f, p90 - 22.f);
        std::vector<int> voiced;
        for (size_t i = 0; i < frames.size(); ++i) if (frames[i].rms > gate) voiced.push_back ((int) i);
        if (voiced.size() < 3) return p;

        // ---- level, dynamics
        std::vector<float> vr; double ms = 0;
        for (int i : voiced) { vr.push_back (frames[(size_t) i].rms); ms += frames[(size_t) i].rms; }
        std::sort (vr.begin(), vr.end());
        p.rmsDb = (float) (ms / (double) voiced.size());
        p.dyn = pct (vr, 0.95f) - pct (vr, 0.10f);
        p.room = p10 > p90 - 40.f;

        // ---- pitch (autocorrelation on a 4x decimated signal)
        std::vector<float> f0s;
        const int dec = 4, dn = N / dec; const double dsr = sr / dec;
        const int minLag = (int) (dsr / 400.0), maxLag = std::min (dn / 2, (int) (dsr / 70.0));
        std::vector<float> d ((size_t) dn);
        for (int fi : voiced)
        {
            const int st = frames[(size_t) fi].start;
            for (int i = 0; i < dn; ++i) { float a = 0; for (int k = 0; k < dec; ++k) a += vox[(size_t) (st + i * dec + k)]; d[(size_t) i] = a / dec; }
            double r0 = 0; for (int i = 0; i < dn; ++i) r0 += (double) d[(size_t) i] * d[(size_t) i];
            if (r0 < 1.0e-9) continue;
            double best = 0; int bl = 0; std::vector<double> ac ((size_t) maxLag + 1, 0.0);
            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double s = 0; for (int i = 0; i + lag < dn; ++i) s += (double) d[(size_t) i] * d[(size_t) (i + lag)];
                ac[(size_t) lag] = s / r0; if (ac[(size_t) lag] > best) { best = ac[(size_t) lag]; bl = lag; }
            }
            if (best < 0.5 || bl == 0) continue;
            for (int lag = minLag; lag < bl; ++lag)       // prefer the shortest strong peak (avoid octave-down errors)
                if (ac[(size_t) lag] > 0.9 * best && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1]) { bl = lag; break; }
            f0s.push_back ((float) (dsr / bl));
        }
        if (f0s.size() >= 3) { std::sort (f0s.begin(), f0s.end()); p.f0 = f0s[f0s.size() / 2]; p.male = p.f0 < 165.f; }

        // ---- spectra
        const int nb = N / 2 + 1; const float binHz = (float) (sr / N);
        auto bin = [&] (float hz) { return juce::jlimit (1, nb - 1, (int) (hz / binHz)); };
        std::vector<double> avg ((size_t) nb, 0.0), sib ((size_t) nb, 0.0);
        std::vector<std::pair<float, int>> ratio;
        for (int fi : voiced)
        {
            double tot = 0, hi = 0;
            for (int b = bin (80.f); b < bin (16000.f); ++b) { const double pw = (double) frames[(size_t) fi].mag[(size_t) b] * frames[(size_t) fi].mag[(size_t) b]; tot += pw; if (b >= bin (4000.f)) hi += pw; }
            ratio.push_back ({ (float) (hi / (tot + 1.0e-12)), fi });
            for (int b = 0; b < nb; ++b) avg[(size_t) b] += (double) frames[(size_t) fi].mag[(size_t) b] * frames[(size_t) fi].mag[(size_t) b];
        }
        std::sort (ratio.begin(), ratio.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        const size_t nSib = std::max<size_t> (2, ratio.size() / 10);
        for (size_t i = 0; i < nSib && i < ratio.size(); ++i)
            for (int b = 0; b < nb; ++b) { const double m = frames[(size_t) ratio[i].second].mag[(size_t) b]; sib[(size_t) b] += m * m; }

        auto toDb = [&] (const std::vector<double>& v) { std::vector<float> o ((size_t) nb); for (int b = 0; b < nb; ++b) o[(size_t) b] = 10.f * std::log10 ((float) v[(size_t) b] + 1.0e-12f); return o; };
        auto smooth = [&] (std::vector<float> v, int w) { std::vector<float> o = v; for (int b = 0; b < nb; ++b) { float s = 0; int c = 0; for (int k = -w; k <= w; ++k) if (b + k >= 0 && b + k < nb) { s += v[(size_t) (b + k)]; ++c; } o[(size_t) b] = s / (float) c; } return o; };
        const auto avgDb = toDb (avg), sibDb = smooth (toDb (sib), 3);

        // sibilance: strongest region 3k..12k and how wide it is (6 dB down)
        const int sLo = bin (3000.f), sHi = bin (std::min (12000.f, (float) sr * 0.45f));
        int pb = sLo; for (int b = sLo; b <= sHi; ++b) if (sibDb[(size_t) b] > sibDb[(size_t) pb]) pb = b;
        int lo = pb, hi2 = pb;
        while (lo > bin (1500.f) && sibDb[(size_t) lo] > sibDb[(size_t) pb] - 6.f) --lo;
        while (hi2 < sHi && sibDb[(size_t) hi2] > sibDb[(size_t) pb] - 6.f) ++hi2;
        p.sibPk = juce::jlimit (3.f, 11.f, (float) pb * binHz / 1000.f);
        p.sibLo = juce::jlimit (1.5f, 8.f, (float) lo * binHz / 1000.f);
        p.sibHi = juce::jlimit (3.f, 14.f, (float) hi2 * binHz / 1000.f);

        // narrow resonance: bump above its own surroundings, 300 Hz .. 5 kHz
        const auto sm = smooth (avgDb, 1);
        float bestScore = 0; int bestBin = 0;
        for (int b = bin (300.f); b < bin (5000.f); ++b)
        {
            const int w = std::max (4, (int) (0.15f * (float) b)); float s = 0; int c = 0;
            for (int k = -w; k <= w; ++k) if (std::abs (k) > 1 && b + k > 0 && b + k < nb) { s += sm[(size_t) (b + k)]; ++c; }
            const float score = sm[(size_t) b] - s / (float) c;
            if (score > bestScore) { bestScore = score; bestBin = b; }
        }
        if (bestScore > 4.f) { p.spikeFound = true; p.spike = juce::jlimit (300.f, 4500.f, (float) bestBin * binHz); }

        // dull = very little energy above 8 kHz compared with the body of the voice
        auto bandMean = [&] (const std::vector<float>& v, float a, float b2) { float s = 0; int c = 0; for (int b = bin (a); b < bin (b2); ++b) { s += v[(size_t) b]; ++c; } return c ? s / (float) c : -120.f; };
        p.dull = (bandMean (avgDb, 8000.f, 14000.f) - bandMean (avgDb, 1000.f, 4000.f)) < -28.f;

        // ---- the beat (sidechain input)
        if ((int) beat.size() >= N * 4)
        {
            double be = 0; for (float s : beat) be += (double) s * s;
            if (10.0 * std::log10 (be / (double) beat.size() + 1.0e-12) > -60.0)
            {
                p.beatValid = true;
                std::vector<double> bAvg ((size_t) nb, 0.0); int fc = 0;
                for (int st = 0; st + N <= (int) beat.size(); st += hop, ++fc)
                {
                    for (int i = 0; i < N; ++i) buf[(size_t) i] = beat[(size_t) (st + i)] * win[(size_t) i];
                    std::fill (buf.begin() + N, buf.end(), 0.f);
                    fft.performFrequencyOnlyForwardTransform (buf.data());
                    for (int b = 0; b < nb; ++b) bAvg[(size_t) b] += (double) buf[(size_t) b] * buf[(size_t) b];
                }
                const auto bDb = toDb (bAvg);
                int bb = bin (30.f); for (int b = bin (30.f); b <= bin (120.f); ++b) if (bDb[(size_t) b] > bDb[(size_t) bb]) bb = b;
                p.bassHz = (float) bb * binHz;
                // the beat is "muddy" where it sits close to the vocal in the 200-450 Hz zone
                const float vBand = bandMean (avgDb, 200.f, 450.f) - 10.f * std::log10 ((float) voiced.size());
                const float bBand = bandMean (bDb, 200.f, 450.f) - 10.f * std::log10 ((float) std::max (fc, 1));
                p.beatMuddy = (bBand - vBand) > -6.f;

                // tempo from the onset envelope
                const int blk = 512; std::vector<float> env; float prev = 0;
                for (int st = 0; st + blk <= (int) beat.size(); st += blk)
                { double e = 0; for (int i = 0; i < blk; ++i) e += (double) beat[(size_t) (st + i)] * beat[(size_t) (st + i)]; const float x = std::sqrt ((float) (e / blk)); env.push_back (std::max (0.f, x - prev)); prev = x; }
                const double hopSec = blk / sr; double bestAc = 0; float bpm = 0;
                for (int b = 70; b <= 180; ++b)
                {
                    const int lag = (int) std::round (60.0 / b / hopSec); double s = 0; int c = 0;
                    for (size_t i = 0; i + (size_t) lag < env.size(); ++i, ++c) s += env[i] * env[i + (size_t) lag];
                    if (c) s /= c;
                    if (s > bestAc) { bestAc = s; bpm = (float) b; }
                }
                p.bpm = bpm;
            }
        }
        p.valid = true;
        return p;
    }
};
} // namespace bh
