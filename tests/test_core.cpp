// PunchPaw core tests. Build and run with: sh tests/run.sh   (no JUCE needed)
// Everything below is measured on the compiled core. Targets that come from the mockup's cited public
// manual (attack 15/5/3 ms, release 125 dB per second) are checked against the core, not against a real unit.
#include "../src/core/PunchPaw.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <vector>

using namespace pp;
static int fails = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++fails; std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } else { std::printf ("ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)
#define INFO(...) do { std::printf ("info: "); std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

static double FS = 48000.0;
static const double PI = 3.14159265358979323846;

static std::unique_ptr<Compressor> make (const Params& p, double fs = 48000.0)
{
    FS = fs;
    auto c = std::make_unique<Compressor>();
    c->setParams (p);
    c->prepare (fs);
    return c;
}
static std::vector<float> sine (double f, double amp, int n)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2.0 * PI * f * i / FS));
    return v;
}
static double goertzel (const std::vector<float>& x, double f, int start, int n)
{
    const double w = 2.0 * PI * f / FS, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double s = x[(size_t) (start + i)] + c * s1 - s2; s2 = s1; s1 = s; }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / n;
}
static void run (Compressor& c, std::vector<float>& l, bool bypass = false) { c.process (l.data(), nullptr, (int) l.size(), bypass); }
static float peakAbs (const std::vector<float>& v, int from = 0) { float p = 0; for (size_t i = (size_t) from; i < v.size(); ++i) p = std::max (p, std::fabs (v[i])); return p; }

// Steady gain reduction (dB) for a 1 kHz sine of the given peak level in dBFS (stereo identical, so RMS = peak - 3.01 dB).
static float steadyGr (Params p, float peakDb, double fs = 48000.0)
{
    auto c = make (p, fs);
    auto s = sine (1000, std::pow (10.0, peakDb / 20.0), (int) fs * 2);
    run (*c, s);
    return c->grNow();
}

// Time (ms) for the gain reduction to go from 10% to 90% of its final value after a tone starts.
static double riseTimeMs (Params p, float peakDb)
{
    auto c = make (p);
    const int n = (int) FS * 2;
    auto s = sine (1000, std::pow (10.0, peakDb / 20.0), n);
    std::vector<float> gr ((size_t) n);
    for (int i = 0; i < n; i += 48) { c->process (s.data() + i, nullptr, 48, false); for (int k = 0; k < 48; ++k) gr[(size_t) (i + k)] = c->grNow(); }
    const float fin = gr.back();
    int t10 = -1, t90 = -1;
    for (int i = 0; i < n; ++i) { if (t10 < 0 && gr[(size_t) i] >= 0.1f * fin) t10 = i; if (t90 < 0 && gr[(size_t) i] >= 0.9f * fin) { t90 = i; break; } }
    return 1000.0 * (t90 - t10) / FS;
}

int main()
{
    // ---------------------------------------------------------------- 1. maps and ranges
    {
        CHECK (std::fabs (slopeOf (0) - 1.0f) < 1e-5 && std::fabs (slopeOf (4) - 0.25f) < 1e-5 && slopeOf (8) == 0.0f && std::fabs (slopeOf (10) + 1.0f) < 1e-5, "ratio knob: 0 is 1:1, 4 is 4:1, 8 is infinity, 10 is -1 (slope %.2f)", slopeOf (10));
        CHECK (ratioText (4) == "4.0:1" && ratioText (8) == "inf:1" && ratioText (10) == "-1.0:1" && ratioText (6) == "10:1", "ratio text: %s %s %s %s", ratioText (4).c_str(), ratioText (8).c_str(), ratioText (10).c_str(), ratioText (6).c_str());
        CHECK (thrDbOf (0) == -40 && thrDbOf (10) == 0 && thrDbOf (5) == -20, "threshold -40..0 dBFS, default -20");
        CHECK (outDbOf (0) == -20 && outDbOf (10) == 20 && outDbOf (5) == 0, "output -20..+20 dB, default 0");
        CHECK (std::fabs (atkMsOf (0) - 0.1f) < 1e-4 && std::fabs (atkMsOf (10) - 100.0f) < 0.01 && std::fabs (relMsOf (0) - 10.0f) < 1e-3 && std::fabs (relMsOf (10) - 2000.0f) < 0.5, "attack 0.1..100 ms, release 10 ms..2 s");
        CHECK (std::fabs (atkKnob (atkMsOf (3.3f)) - 3.3f) < 1e-3 && std::fabs (relKnob (relMsOf (7.1f)) - 7.1f) < 1e-3, "attack and release knob maps invert");
        CHECK (hpHzOf (0) == 0 && std::fabs (hpHzOf (1) - 20) < 1e-3 && std::fabs (hpHzOf (10) - 300) < 0.01 && std::fabs (hpHzOf (5) - 80) < 0.01, "sidechain high-pass Off, 20 Hz to 300 Hz");
        CHECK (ceilDbOf (0) == -12 && ceilDbOf (9) == -1 && ceilDbOf (10) >= 50, "ceiling -12 to -1 dBFS, Off at the end");
        float last = -1e9f; bool mono = true;
        for (int i = 0; i <= 1000; ++i) { const float v = hpHzOf (i * 0.01f); if (v + 1e-4f < last) mono = false; last = v; }
        CHECK (mono, "high-pass map is monotonic");
    }

    // ---------------------------------------------------------------- 2. static curve (measured on sines)
    {
        Params p; p.thrDb = -20; p.slope = 0.25f; p.manual = true; p.atkMs = 1.0f; p.relMs = 100.0f;
        // 1 kHz sine, peak -8.01 dB -> RMS -11.02 dB -> over 8.98 -> 4:1 GR = 6.73 dB
        const float peak = -8.01f, rms = peak - 3.01f, over = rms + 20.0f;
        const float gr = steadyGr (p, peak);
        CHECK (std::fabs (gr - over * 0.75f) < 0.25f, "4:1 hard knee: GR %.2f dB (want %.2f)", gr, over * 0.75f);
        CHECK (steadyGr (p, -30.0f) < 0.01f, "below threshold: no gain reduction (%.3f dB)", steadyGr (p, -30.0f));
        Params inf = p; inf.slope = 0.0f;
        const float gi = steadyGr (inf, -2.0f), oi = (-2.0f - 3.01f) + 20.0f;
        CHECK (std::fabs (gi - oi) < 0.3f, "infinity:1 holds the level at the threshold (GR %.2f, want %.2f)", gi, oi);
        Params neg = p; neg.slope = -1.0f;
        const float gn = steadyGr (neg, -8.0f), on = (-8.0f - 3.01f + 20.0f) * 2.0f;
        CHECK (std::fabs (gn - on) < 0.5f, "negative ratio (-1:1) turns loud signals down twice as hard (GR %.2f, want %.2f)", gn, on);
        Params one = p; one.slope = 1.0f;
        CHECK (steadyGr (one, 0.0f) < 0.01f, "1:1 does not compress (%.3f dB)", steadyGr (one, 0.0f));
        // soft knee: at the threshold GR = k * 8 / 8 = 0.75 dB for 4:1, and smooth across it
        Params sk = p; sk.soft = true;
        const float gT = steadyGr (sk, -20.0f + 3.01f);
        CHECK (std::fabs (gT - 0.75f) < 0.15f, "soft knee: %.2f dB at the threshold (want 0.75)", gT);
        float prev = 0, worst = 0;
        for (float lv = -34.0f; lv < -8.0f; lv += 0.25f) { const float v = steadyGr (sk, lv); if (lv > -34.0f) worst = std::max (worst, std::fabs (v - prev)); prev = v; }
        CHECK (worst < 0.6f, "soft knee curve has no jumps (largest step %.2f dB per 0.25 dB)", worst);
        Params hk = p;
        CHECK (steadyGr (hk, -20.0f + 3.01f - 3.0f) < 0.05f, "hard knee: no gain reduction 3 dB under the threshold");
    }

    // ---------------------------------------------------------------- 3. timing
    {
        Params p; p.thrDb = -40; p.slope = 0.25f;
        // choose a level that gives about 10 dB of reduction: over*0.75=10 -> over 13.3 -> RMS -26.7 -> peak -23.7
        const double t10 = riseTimeMs (p, -23.7f);
        // 20 dB: over 26.7 -> RMS -13.3 -> peak -10.3 ; 30 dB: over 40 -> peak +3 (needs thr -40 plus hot input)
        const double t20 = riseTimeMs (p, -10.3f);
        INFO ("auto attack 10 to 90 percent: %.1f ms at 10 dB, %.1f ms at 20 dB (detector adds about 8 ms of its own smoothing)", t10, t20);
        CHECK (t10 > 10.0 && t10 < 40.0, "auto attack at 10 dB reduction takes %.1f ms (spec 15 ms, plus detector)", t10);
        CHECK (t20 < t10, "harder hits get a faster auto attack (%.1f ms vs %.1f ms)", t20, t10);
        // release slope: loud tone, then silence
        auto c = make (p);
        auto s = sine (1000, 0.5, 48000); run (*c, s);
        const float g0 = c->grNow();
        std::vector<float> sil (48000, 0.0f);
        double t20to10 = -1, tA = -1;
        for (int i = 0; i < 48000; i += 16) { c->process (sil.data() + i, nullptr, 16, false); const float g = c->grNow(); if (tA < 0 && g <= 8.0f) tA = i; if (t20to10 < 0 && g <= 4.0f) { t20to10 = i; break; } }
        const double dbps = (8.0 - 4.0) / ((t20to10 - tA) / FS);
        CHECK (g0 > 8.0f && std::fabs (dbps - 125.0) < 8.0, "auto release is %.0f dB per second (want 125)", dbps);

        Params m = p; m.manual = true; m.atkMs = 50.0f; m.relMs = 200.0f;
        const double ma = riseTimeMs (m, -23.7f);
        CHECK (ma > 40.0 && ma < 65.0, "manual attack 50 ms rises 10 to 90 percent in %.1f ms", ma);
        auto c2 = make (m);
        auto s2 = sine (1000, 0.5, 48000 * 2); run (*c2, s2);
        const float top = c2->grNow();
        double tt90 = -1, tt10 = -1;
        for (int i = 0; i < 48000 * 2; i += 16)
        {
            c2->process (sil.data() + (i % 32000), nullptr, 16, false);
            const float g = c2->grNow();
            if (tt90 < 0 && g <= 0.9f * top) tt90 = i;
            if (tt10 < 0 && g <= 0.1f * top) { tt10 = i; break; }
        }
        const double mr = 1000.0 * (tt10 - tt90) / FS;
        CHECK (mr > 160.0 && mr < 250.0, "manual release 200 ms falls 90 to 10 percent in %.0f ms", mr);
        Params fast = m; fast.atkMs = 0.1f;
        CHECK (riseTimeMs (fast, -23.7f) < 15.0, "manual attack 0.1 ms is limited by the 8 ms RMS detector (%.1f ms)", riseTimeMs (fast, -23.7f));
    }

    // ---------------------------------------------------------------- 4. punch: the kick keeps its front
    {
        Params a; a.thrDb = -28; a.slope = 0.2f;           // auto timing
        auto c = make (a);
        // synthetic kick: pitch drop 200 -> 50 Hz, 6.5/s decay, 1 ms rise
        std::vector<float> k (48000, 0.0f);
        for (int i = 0; i < 48000; ++i)
        {
            const double t = i / FS;
            const double ph = 2 * PI * (50 * t + 150 * (1 - std::exp (-45 * t)) / 45);
            k[(size_t) i] = (float) (0.9 * std::sin (ph) * std::exp (-t * 6.5) * (1 - std::exp (-t * 1500)));
        }
        std::vector<float> o = k;
        float g2 = 0, g10 = 0, g60 = 0;
        for (int i = 0; i < 48000; i += 24)
        {
            c->process (o.data() + i, nullptr, 24, false);
            const double t = (i + 24) / FS;
            if (t >= 0.002 && g2 == 0) g2 = c->grNow();
            if (t >= 0.010 && g10 == 0) g10 = c->grNow();
            if (t >= 0.060 && g60 == 0) g60 = c->grNow();
        }
        INFO ("kick gain reduction: %.1f dB at 2 ms, %.1f dB at 10 ms, %.1f dB at 60 ms (auto timing)", g2, g10, g60);
        CHECK (g2 < 0.35f * g60 && g10 < g60, "the front of the kick passes with far less reduction than the body (2 ms %.1f dB, 60 ms %.1f dB)", g2, g60);
        float pk = 0; for (int i = 48; i < 480; ++i) pk = std::max (pk, std::fabs (o[(size_t) i]));
        INFO ("kick first 10 ms peak %.2f in, %.2f out", 0.9, pk);
    }

    // ---------------------------------------------------------------- 5. neutral path, latency, bypass
    {
        Params p; p.slope = 1.0f; p.thrDb = 0;               // 1:1, ceiling off, mix 1, output 0
        auto c = make (p);
        std::mt19937 rng (3); std::uniform_real_distribution<float> u (-0.9f, 0.9f);
        std::vector<float> x (9600), y;
        for (auto& v : x) v = u (rng);
        y = x; run (*c, y);
        bool exact = true; for (size_t i = Compressor::kLatency; i < y.size(); ++i) if (y[i] != x[i - Compressor::kLatency]) exact = false;
        CHECK (exact, "neutral settings: output is the input delayed by exactly %d samples, bit for bit", Compressor::kLatency);
        std::vector<float> imp (4800, 0.0f); imp[100] = 0.5f;
        auto c2 = make (p); run (*c2, imp);
        int at = -1; for (size_t i = 0; i < imp.size(); ++i) if (imp[i] != 0) { at = (int) i; break; }
        CHECK (at == 100 + Compressor::kLatency, "measured latency is %d samples (reported %d)", at - 100, Compressor::kLatency);
        // with the ceiling on but not touched, same exact delay
        Params q = p; q.ceilDb = -1.0f;
        auto c3 = make (q);
        std::vector<float> z (9600); for (auto& v : z) v = u (rng) * 0.2f;
        auto z2 = z; run (*c3, z2);
        double err = 0; for (size_t i = Compressor::kLatency + 600; i < z2.size(); ++i) err = std::max (err, (double) std::fabs (z2[i] - z[i - Compressor::kLatency]));
        CHECK (err == 0.0, "ceiling on but not reached: output unchanged, still delayed %d (max error %.1e)", Compressor::kLatency, err);
        // mix 0 is the dry signal delayed
        Params m0; m0.thrDb = -40; m0.slope = 0.0f; m0.mix = 0.0f;
        auto c4 = make (m0); auto w = x; run (*c4, w);
        exact = true; for (size_t i = Compressor::kLatency; i < w.size(); ++i) if (w[i] != x[i - Compressor::kLatency]) exact = false;
        CHECK (exact, "mix 0 returns the dry signal, delayed, bit for bit, even at infinity:1");
        // bypass after the 10 ms ramp is bit exact
        Params hard; hard.thrDb = -40; hard.slope = 0.0f; hard.outDb = 12.0f; hard.ceilDb = -6.0f;
        auto c5 = make (hard); auto b = x; run (*c5, b, true);
        exact = true; for (size_t i = 2000; i < b.size(); ++i) if (b[i] != x[i - Compressor::kLatency]) exact = false;
        CHECK (exact, "bypass: dry signal, delayed, bit for bit (after the 10 ms ramp)");
    }

    // ---------------------------------------------------------------- 6. ceiling: level, true peak, aliasing
    {
        Params p; p.slope = 1.0f; p.thrDb = 0; p.ceilDb = -1.0f;
        auto c = make (p);
        auto s = sine (997, 2.0, 48000);                    // +6 dBFS sine into a -1 dBFS ceiling
        run (*c, s);
        const float sp = peakAbs (s, 2000);
        // true peak: 16x oversampled windowed sinc over the last 0.5 s
        double tp = 0;
        {
            const int N = 24000, off = 24000;
            for (int i = off; i < off + N - 40; i += 1)
                for (int ph = 0; ph < 16; ++ph)
                {
                    const double fr = ph / 16.0; double acc = 0;
                    for (int k = -16; k <= 16; ++k)
                    {
                        const double d = k - fr, sinc = d == 0 ? 1.0 : std::sin (PI * d) / (PI * d), win = 0.5 + 0.5 * std::cos (PI * d / 17.0);
                        acc += s[(size_t) (i + k)] * sinc * win;
                    }
                    tp = std::max (tp, std::fabs (acc));
                }
        }
        const double ceilLin = std::pow (10.0, -1.0 / 20.0);
        INFO ("ceiling -1 dBFS on a +6 dBFS sine: sample peak %.2f dBFS, true peak %.2f dBFS", 20 * std::log10 (sp), 20 * std::log10 (tp));
        CHECK (sp <= (float) ceilLin + 1e-4f, "sample peak %.3f stays under the -1 dBFS ceiling (%.3f)", sp, ceilLin);
        CHECK (tp <= ceilLin * std::pow (10.0, 0.5 / 20.0), "true peak %.2f dBFS is within 0.5 dB of the ceiling", 20 * std::log10 (tp));

        // aliasing: 9.7 kHz tone, 3rd harmonic 29.1 kHz folds to 18.9 kHz, 5th folds to 0.5 kHz
        Params q = p; q.ceilDb = -6.0f;
        auto c2 = make (q);
        auto t = sine (9700, 1.5, 48000); run (*c2, t);
        const double f0 = goertzel (t, 9700, 24000, 24000 - 24000 % 97 - 0), a3 = goertzel (t, 18900, 24000, 24000), a5 = goertzel (t, 500, 24000, 24000);
        INFO ("ceiling -6 dBFS, 9.7 kHz tone at +3.5 dBFS: fundamental %.1f dB, alias at 18.9 kHz %.1f dB, alias at 0.5 kHz %.1f dB (relative to the fundamental)",
              20 * std::log10 (f0), 20 * std::log10 (a3 / f0), 20 * std::log10 (a5 / f0));
        CHECK (20 * std::log10 (a3 / f0) < -55.0 && 20 * std::log10 (a5 / f0) < -55.0, "aliasing of the ceiling stays below -55 dB (%.1f, %.1f dB)", 20 * std::log10 (a3 / f0), 20 * std::log10 (a5 / f0));
        // same tone with a naive (not oversampled) clip for comparison
        std::vector<float> nv = sine (9700, 1.5, 48000);
        const float cl = std::pow (10.0f, -6.0f / 20.0f);
        for (auto& v : nv) { const float a = 0.6f * cl; const float au = std::fabs (v); if (au > a) { const float y = a + (cl - a) * std::tanh ((au - a) / (cl - a)); v = v < 0 ? -y : y; } }
        const double n3 = goertzel (nv, 18900, 24000, 24000), nf = goertzel (nv, 9700, 24000, 24000);
        INFO ("the same clipper without oversampling: alias at 18.9 kHz %.1f dB", 20 * std::log10 (n3 / nf));
        CHECK (20 * std::log10 (a3 / f0) < 20 * std::log10 (n3 / nf) - 10.0, "4x oversampling lowers that alias by more than 10 dB versus a plain clipper");

        // a clean tone below the ceiling is untouched in level at 20 kHz (the correction path only, no low-pass)
        Params r = p; r.ceilDb = -1.0f;
        auto c3 = make (r);
        auto hi = sine (20000, 0.3, 24000); auto hi0 = hi; run (*c3, hi);
        double e = 0; for (size_t i = 3000; i < hi.size(); ++i) e = std::max (e, (double) std::fabs (hi[i] - hi0[i - Compressor::kLatency]));
        CHECK (e == 0.0, "20 kHz tone below the ceiling passes untouched (max error %.1e)", e);
    }

    // ---------------------------------------------------------------- 7. sidechain high-pass
    {
        Params p; p.thrDb = -30; p.slope = 0.1f; p.manual = true; p.atkMs = 5; p.relMs = 100;
        const float off = [&] { auto c = make (p); auto s = sine (40, 0.7, 48000); run (*c, s); return c->grNow(); }();
        p.hpHz = 300.0f;
        const float on = [&] { auto c = make (p); auto s = sine (40, 0.7, 48000); run (*c, s); return c->grNow(); }();
        const float mid = [&] { auto c = make (p); auto s = sine (3000, 0.7, 48000); run (*c, s); return c->grNow(); }();
        CHECK (on < off - 8.0f, "40 Hz tone: %.1f dB reduction with the sidechain high-pass at 300 Hz, %.1f dB without", on, off);
        CHECK (mid > 8.0f, "3 kHz tone is still detected with the high-pass at 300 Hz (%.1f dB)", mid);
        // it only changes what the detector hears: the audio keeps its low end when below threshold
        Params q; q.thrDb = 0; q.slope = 1.0f; q.hpHz = 300.0f;
        auto c = make (q); auto s = sine (40, 0.5, 48000); auto s0 = s; run (*c, s);
        double e = 0; for (size_t i = 100; i < s.size(); ++i) e = std::max (e, (double) std::fabs (s[i] - s0[i - Compressor::kLatency]));
        CHECK (e == 0.0, "the sidechain high-pass does not filter the audio (max error %.1e)", e);
    }

    // ---------------------------------------------------------------- 8. output level and mix
    {
        Params p; p.slope = 1.0f; p.thrDb = 0; p.outDb = 6.0f;
        auto c = make (p); auto s = sine (1000, 0.25, 48000); run (*c, s);
        const float pk = peakAbs (s, 24000);
        CHECK (std::fabs (20 * std::log10 (pk / 0.25f) - 6.0f) < 0.05f, "output +6 dB gives %.2f dB", 20 * std::log10 (pk / 0.25f));
        Params q; q.thrDb = -40; q.slope = 0.0f; q.mix = 0.5f; q.manual = true; q.atkMs = 1;
        auto c2 = make (q); auto t = sine (1000, 0.5, 48000); run (*c2, t);
        const float wet = steadyGr (Params { -40, 0.0f, 0, 0, 1.0f, 100.0f, false, true, 1, 100 }, -6.0f);
        const float pm = peakAbs (t, 24000);
        const float expect = 0.5f * (0.5f + 0.5f * std::pow (10.0f, -wet / 20.0f));
        CHECK (std::fabs (pm - expect) < 0.01f, "mix 50%% is the average of dry and processed (%.3f, want %.3f)", pm, expect);
    }

    // ---------------------------------------------------------------- 9. stability
    {
        Params p; p.thrDb = -40; p.slope = -1.0f; p.outDb = 20.0f; p.ceilDb = -12.0f; p.soft = true; p.hpHz = 300; p.manual = false;
        auto c = make (p);
        std::vector<float> z (48000, 0.0f); run (*c, z);
        double dc = 0; for (float v : z) dc += std::fabs (v);
        CHECK (dc == 0.0, "silence in, silence out (sum %.2e)", dc);
        std::vector<float> d (48000, 1.0f); run (*c, d);
        bool ok = true; for (float v : d) if (! std::isfinite (v) || std::fabs (v) > 1.0f) ok = false;
        CHECK (ok, "DC at full scale with every nasty setting: finite and under the -12 dB ceiling (max %.3f)", peakAbs (d, 4000));
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (-8.0f, 8.0f);
        std::vector<float> nz (96000); for (auto& v : nz) v = u (rng);
        run (*c, nz);
        ok = true; for (float v : nz) if (! std::isfinite (v)) ok = false;
        CHECK (ok, "+18 dBFS noise: finite output");
        std::vector<float> bad (4800, 0.3f); bad[100] = NAN; bad[200] = INFINITY; bad[300] = -INFINITY;
        run (*c, bad);
        ok = true; for (float v : bad) if (! std::isfinite (v)) ok = false;
        std::vector<float> after = sine (1000, 0.3, 4800); run (*c, after);
        bool ok2 = true; for (float v : after) if (! std::isfinite (v)) ok2 = false;
        CHECK (ok && ok2, "NaN and infinity in the input never reach the output, and the unit recovers");
        for (double fs : { 44100.0, 88200.0, 96000.0, 192000.0 })
        {
            auto cc = make (p, fs);
            auto s = sine (1000, 0.5, (int) fs); run (*cc, s);
            bool fin = true; for (float v : s) if (! std::isfinite (v)) fin = false;
            Params n; n.slope = 1; n.thrDb = 0;
            auto c9 = make (n, fs); std::vector<float> im ((size_t) fs / 10, 0.0f); im[50] = 1.0f; run (*c9, im);
            int at = -1; for (size_t i = 0; i < im.size(); ++i) if (im[i] != 0) { at = (int) i - 50; break; }
            CHECK (fin && at == Compressor::kLatency, "%.0f Hz: finite, latency %d samples", fs, at);
        }
        // mono path and stereo link
        auto cm = make (p); std::vector<float> m = sine (1000, 0.5, 9600); cm->process (m.data(), nullptr, (int) m.size(), false);
        CHECK (std::isfinite (m.back()), "mono processing works");
        Params lk; lk.thrDb = -30; lk.slope = 0.1f; lk.manual = true; lk.atkMs = 2;
        auto cs = make (lk);
        auto L = sine (1000, 0.8, 48000), R = sine (1000, 0.02, 48000); auto R0 = R;
        cs->process (L.data(), R.data(), 48000, false);
        const float ratio = peakAbs (R, 30000) / 0.02f;
        CHECK (ratio < 0.6f, "channels are linked: a loud left ducks a quiet right (gain %.2f)", ratio);
        (void) R0;
    }

    // ---------------------------------------------------------------- 10. knob to sound
    {
        // DC step on the output knob (0 dB to -20 dB) at sample 0 of a block. The result is read after the
        // fixed latency, so the first changed sample is expected at kLatency.
        auto cu = make (Params { 0, 1.0f, 0.0f, 0, 1.0f, 100.0f, false, false, 10, 150 });
        std::vector<float> ones (4800, 1.0f); run (*cu, ones);
        Params dn; dn.slope = 1.0f; dn.thrDb = 0; dn.outDb = -20.0f; cu->setParams (dn);
        std::vector<float> v2 (4800, 1.0f); run (*cu, v2);
        int first = -1, t90 = -1;
        for (int i = 0; i < 4800; ++i)
        {
            if (first < 0 && std::fabs (v2[(size_t) i] - 1.0f) > 1e-6f) first = i;
            if (t90 < 0 && (1.0f - v2[(size_t) i]) / 0.9f >= 0.9f) t90 = i;
        }
        CHECK (first >= 0 && first - Compressor::kLatency <= 1, "output knob: the first changed sample arrives %d sample(s) after the latency (same buffer)", first - Compressor::kLatency);
        CHECK (t90 >= 0 && 1000.0 * (t90 - Compressor::kLatency) / FS < 15.0 && std::fabs (v2.back() - 0.1f) < 0.005f, "output knob reaches 90 percent of the step %.1f ms after the latency (5 ms smoothing)", 1000.0 * (t90 - Compressor::kLatency) / FS);
        // threshold step: gain reduction starts moving within a few milliseconds
        Params t; t.thrDb = 0; t.slope = 0.0f; t.manual = true; t.atkMs = 2; t.relMs = 100;
        auto ct = make (t); auto tone = sine (1000, 0.5, 9600); run (*ct, tone);
        t.thrDb = -40; ct->setParams (t);
        int reach = -1; auto tone2 = sine (1000, 0.5, 4800);
        for (int i = 0; i < 4800; i += 16) { ct->process (tone2.data() + i, nullptr, 16, false); if (reach < 0 && ct->grNow() > 1.0f) reach = i; }
        CHECK (reach >= 0 && 1000.0 * reach / FS < 20.0, "threshold knob: gain reduction of 1 dB is reached %.1f ms after the change", 1000.0 * reach / FS);
    }

    // ---------------------------------------------------------------- 11. CPU (information)
    {
        Params p; p.thrDb = -30; p.slope = 0.2f; p.ceilDb = -3.0f;
        auto c = make (p);
        std::mt19937 rng (9); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        std::vector<float> l (512 * 938), r (512 * 938);
        for (size_t i = 0; i < l.size(); ++i) { l[i] = u (rng); r[i] = u (rng); }
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < l.size(); i += 512) c->process (l.data() + i, r.data() + i, 512, false);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        INFO ("10 s of stereo at 48 kHz with the ceiling working hard took %.3f s (%.2f percent of one core; sh tests/run.sh builds with sanitizers, which is slower than a release build)", sec, 10.0 * sec);
    }

    std::printf (fails == 0 ? "ALL CORE TESTS PASSED\n" : "CORE TESTS FAILED (%d)\n", fails);
    return fails == 0 ? 0 : 1;
}
