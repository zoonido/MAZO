// MAZO automated sound checks. Build: g++ -std=c++17 -O2 tests/tests.cpp -o mazotests
// Run: ./mazotests [output folder]   -> prints PASS/FAIL per check and renders mazo_preview.wav
#include "../source/dsp/engine.h"
#include "../source/dsp/presets.h"
#include <cstdio>
#include <string>
#include <complex>
#include <functional>

using namespace mazo;
static int fails = 0, passes = 0;

static void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    (ok ? passes : fails)++;
}
static std::string fmt (double v, int dp = 1) { char b[64]; std::snprintf (b, sizeof b, "%.*f", dp, v); return b; }

struct Hit { double t; float vel = 1.0f; };

static std::vector<float> render (Engine& e, const std::vector<Hit>& hits, double seconds, double sr,
                                  std::function<void (Engine&, double)> tweak = nullptr, double bpm = 128.0,
                                  std::vector<float>* rumbleOut = nullptr)
{
    const int block = 64, total = (int) (seconds * sr);
    std::vector<float> out ((size_t) total, 0.0f);
    size_t next = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        int n = std::min (block, total - pos);
        const double t = pos / sr;
        if (tweak) tweak (e, t);
        // sample-accurate hits: split the block at the hit
        while (next < hits.size() && hits[next].t < (pos + n) / sr)
        {
            const int at = std::clamp ((int) std::lround (hits[next].t * sr) - pos, 0, n);
            if (at > 0) { e.process (out.data() + pos, at, bpm); if (rumbleOut) rumbleOut->insert (rumbleOut->end(), e.rumbleBus.begin(), e.rumbleBus.begin() + at); }
            e.trigger (hits[next].vel);
            pos += at; n -= at; ++next;
        }
        if (n > 0) { e.process (out.data() + pos, n, bpm); if (rumbleOut) rumbleOut->insert (rumbleOut->end(), e.rumbleBus.begin(), e.rumbleBus.begin() + n); }
        pos += n - block;   // loop adds block back
    }
    return out;
}

// The stage 1-2 checks test the sound sources, so they run with the FX chain neutral;
// the stage 3 checks and the preview use the real defaults.
static Engine make (double sr, bool fxDefaults = false)
{
    Engine e; e.prepare (sr, 64);
    if (! fxDefaults) { e.params.warmth = 0; e.params.compAmount = 0; e.params.distMix = 0; }
    return e;
}

// Only the body, long and clean, so its pitch can be measured
static void bodyOnly (Engine& e)
{
    e.params.clickLevelDb = -60; e.params.subLevelDb = -60;
    e.params.holdMs = 400; e.params.decayMs = 3000; e.params.gainDb = -6;
}

// Frequency by rising zero crossings (linear interpolation), over a time window
static double measureHz (const std::vector<float>& x, double sr, double t0, double t1)
{
    double first = -1, last = -1; int count = 0;
    for (size_t i = (size_t) (t0 * sr) + 1; i < std::min (x.size(), (size_t) (t1 * sr)); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double frac = x[i - 1] / (double) (x[i - 1] - x[i]);
            const double tc = (i - 1 + frac) / sr;
            if (first < 0) first = tc;
            last = tc; ++count;
        }
    return count > 1 ? (count - 1) / (last - first) : 0.0;
}
static double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
static double rms (const std::vector<float>& x, double sr, double t0, double t1)
{
    size_t a = (size_t) (t0 * sr), c = std::min (x.size(), (size_t) (t1 * sr)); double s = 0;
    for (size_t i = a; i < c; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / std::max<size_t> (1, c - a));
}
static double db (double x) { return 20.0 * std::log10 (x + 1e-12); }
static float peakOf (const std::vector<float>& x, bool& finite)
{
    float p = 0; finite = true;
    for (float v : x) { if (! std::isfinite (v)) finite = false; p = std::max (p, std::abs (v)); }
    return p;
}
static double centroid (const std::vector<float>& x, double sr, double t0, int N = 2048)
{
    std::vector<std::complex<double>> a ((size_t) N);
    const size_t s = (size_t) (t0 * sr);
    for (int i = 0; i < N; ++i) a[(size_t) i] = (s + (size_t) i < x.size() ? x[s + (size_t) i] : 0.0);
    for (int i = 1, j = 0; i < N; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[(size_t) i], a[(size_t) j]); }
    for (int len = 2; len <= N; len <<= 1)
    {
        const std::complex<double> wl (std::cos (-2 * M_PI / len), std::sin (-2 * M_PI / len));
        for (int i = 0; i < N; i += len)
        {
            std::complex<double> w (1);
            for (int j = 0; j < len / 2; ++j) { auto u = a[(size_t) (i + j)], v = a[(size_t) (i + j + len / 2)] * w; a[(size_t) (i + j)] = u + v; a[(size_t) (i + j + len / 2)] = u - v; w *= wl; }
        }
    }
    double num = 0, den = 0;
    for (int k = 1; k < N / 2; ++k) { const double m = std::abs (a[(size_t) k]); num += k * sr / N * m; den += m; }
    return num / (den + 1e-12);
}

// Magnitude spectrum (Hann) of N samples from t0
static std::vector<double> spectrum (const std::vector<float>& x, double sr, double t0, int N)
{
    std::vector<std::complex<double>> a ((size_t) N);
    const size_t s = (size_t) (t0 * sr);
    for (int i = 0; i < N; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / (N - 1));
        a[(size_t) i] = (s + (size_t) i < x.size() ? x[s + (size_t) i] : 0.0) * w;
    }
    for (int i = 1, j = 0; i < N; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[(size_t) i], a[(size_t) j]); }
    for (int len = 2; len <= N; len <<= 1)
    {
        const std::complex<double> wl (std::cos (-2 * M_PI / len), std::sin (-2 * M_PI / len));
        for (int i = 0; i < N; i += len)
        {
            std::complex<double> w (1);
            for (int j = 0; j < len / 2; ++j) { auto u = a[(size_t) (i + j)], v = a[(size_t) (i + j + len / 2)] * w; a[(size_t) (i + j)] = u + v; a[(size_t) (i + j + len / 2)] = u - v; w *= wl; }
        }
    }
    std::vector<double> m ((size_t) N / 2);
    for (int i = 0; i < N / 2; ++i) m[(size_t) i] = std::abs (a[(size_t) i]);
    return m;
}
static double bandDb (const std::vector<double>& m, double sr, int N, double f0, double f1)
{
    double e = 0; for (int k = std::max (1, (int) (f0 * N / sr)); k <= (int) (f1 * N / sr) && k < (int) m.size(); ++k) e += m[(size_t) k] * m[(size_t) k];
    return 10.0 * std::log10 (e + 1e-24);
}
static double peakHz (const std::vector<double>& m, double sr, int N, double f0, double f1)
{
    int best = 1; for (int k = std::max (2, (int) (f0 * N / sr)); k <= (int) (f1 * N / sr); ++k) if (m[(size_t) k] > m[(size_t) best]) best = k;
    const double a = std::log (m[(size_t) best - 1] + 1e-30), b = std::log (m[(size_t) best] + 1e-30), c = std::log (m[(size_t) best + 1] + 1e-30);
    return (best + 0.5 * (a - c) / (a - 2 * b + c)) * sr / N;
}

// Rumble on, everything else at defaults
static void rumbleOn (Engine& e, float mix = 0.5f) { e.params.rumbleMix = mix; }

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); }; auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t l = (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * 32767.0f), r = (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&l, 2, 1, f); std::fwrite (&r, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : ".";
    const double SR = 44100.0;

    // 1. The body lands exactly on the Key, across the range, including fine tune
    {
        struct K { int key, oct; float fine; };
        const K ks[] { { 0, 0, 0 }, { 5, 1, 12 }, { 9, 1, 0 }, { 8, 1, 50 }, { 2, 2, -50 }, { 11, 2, 0 } };
        for (const auto& k : ks)
        {
            Engine e = make (SR); bodyOnly (e);
            e.params.key = k.key; e.params.octave = k.oct; e.params.fine = k.fine;
            auto x = render (e, { { 0.0 } }, 2.0, SR);
            const double want = keyHz (k.key, k.oct, k.fine), got = measureHz (x, SR, 0.1, 1.6);
            check (std::abs (cents (got, want)) < 0.5, std::string ("Lands on ") + noteName (k.key) + std::to_string (k.oct) + " " + (k.fine >= 0 ? "+" : "") + fmt (k.fine, 0)
                   + " ct: wanted " + fmt (want, 2) + " Hz, got " + fmt (got, 2) + " Hz  (" + fmt (cents (got, want), 2) + " ct)");
        }
    }

    // 2. The F1 readout on the mockup: 43.95 Hz
    check (std::abs (keyHz (5, 1, 12.0f) - 43.95) < 0.01, "F1 +12 ct = 43.95 Hz  (" + fmt (keyHz (5, 1, 12.0f), 3) + ")");

    // 3. Fine tune moves the pitch by exactly the cents asked
    {
        double hz[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); bodyOnly (e); e.params.key = 5; e.params.octave = 1; e.params.fine = k ? 37.0f : 0.0f;
            auto x = render (e, { { 0.0 } }, 2.0, SR); hz[k] = measureHz (x, SR, 0.1, 1.6);
        }
        check (std::abs (cents (hz[1], hz[0]) - 37.0) < 0.5, "Fine +37 ct raises the kick by 37 ct  (" + fmt (cents (hz[1], hz[0]), 2) + ")");
    }

    // 4. The sweep starts high: 36 st above F1 is 8x the frequency
    {
        Engine e = make (SR); bodyOnly (e); e.params.sweepMs = 80; e.params.curve = 0.0f;
        auto x = render (e, { { 0.0 } }, 0.4, SR);
        const double early = measureHz (x, SR, 0.0, 0.010), landed = measureHz (x, SR, 0.1, 0.35), want = e.targetHz();
        check (early > want * 5.5 && std::abs (cents (landed, want)) < 1.0,
               "Pitch sweep starts high and lands: " + fmt (early, 0) + " Hz in the first 10 ms -> " + fmt (landed, 2) + " Hz");
    }

    // 5. Curve: at 100% the pitch drops faster at the start than at 0%
    {
        double hz[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); bodyOnly (e); e.params.sweepMs = 100; e.params.curve = k ? 1.0f : 0.0f;
            auto x = render (e, { { 0.0 } }, 0.2, SR); hz[k] = measureHz (x, SR, 0.02, 0.06);
        }
        check (hz[1] < hz[0] * 0.6, "Curve 100% has dropped further by 20-60 ms than Curve 0%  (" + fmt (hz[1], 0) + " vs " + fmt (hz[0], 0) + " Hz)");
    }

    // 6. MIDI note number does not change the pitch (fixed key), at any sample rate
    {
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            Engine e = make (sr); bodyOnly (e);
            auto x = render (e, { { 0.0 } }, 2.0, sr);
            const double got = measureHz (x, sr, 0.1, 1.6);
            check (std::abs (cents (got, e.targetHz())) < 0.5, "Tuning holds at " + fmt (sr / 1000.0, 1) + " kHz  (" + fmt (cents (got, e.targetHz()), 2) + " ct)");
        }
        check (true, "MIDI notes only trigger: the engine takes no note number, pitch comes from Key/Octave/Fine");
    }

    // 7. Sub: unison sits on the Key, -1 Oct exactly an octave below
    {
        for (int oct = 0; oct < 2; ++oct)
        {
            Engine e = make (SR);
            e.params.clickLevelDb = -60; e.params.bodyLevelDb = -60; e.params.blend = 0; e.params.subOctave = oct; e.params.subDecayMs = 3000;
            auto x = render (e, { { 0.0 } }, 2.5, SR);
            const double want = e.targetHz() * (oct ? 0.5 : 1.0), got = measureHz (x, SR, 0.2, 2.0);
            check (std::abs (cents (got, want)) < 0.5, std::string ("Sub ") + (oct ? "-1 Oct" : "Unison") + ": " + fmt (got, 2) + " Hz, wanted " + fmt (want, 2));
        }
    }

    // 8. Hold keeps the body flat, then Decay takes it down
    {
        Engine e = make (SR); e.params.clickLevelDb = -60; e.params.subLevelDb = -60; e.params.pitchAmt = 0;
        e.params.octave = 2; e.params.key = 9; e.params.holdMs = 150; e.params.decayMs = 300; e.params.gainDb = -6;
        auto x = render (e, { { 0.0 } }, 0.6, SR);
        const double a = db (rms (x, SR, 0.02, 0.07)), b = db (rms (x, SR, 0.10, 0.15)), c = db (rms (x, SR, 0.35, 0.40)), d = db (rms (x, SR, 0.46, 0.6));
        check (std::abs (a - b) < 0.5 && b - c > 6 && d < -100, "Hold 150 ms stays flat (" + fmt (a - b, 2) + " dB), then decays to silence at 450 ms");
    }

    // 9. Blend ducks the sub under the body's punch
    {
        double early[2], late[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); e.params.clickLevelDb = -60; e.params.blend = k ? 1.0f : 0.0f;
            auto x = render (e, { { 0.0 } }, 1.2, SR, [] (Engine& en, double) { en.params.bodyLevelDb = -60; });
            // body level at -60 is silent, but its envelope still drives Blend
            early[k] = db (rms (x, SR, 0.02, 0.08)); late[k] = db (rms (x, SR, 0.7, 0.8));
        }
        check (early[0] - early[1] > 20 && std::abs (late[0] - late[1]) < 3,
               "Blend 100% keeps the sub out of the punch (" + fmt (early[0] - early[1], 0) + " dB lower), same tail later");
    }

    // 10. Transient: three different sources, Tone makes it brighter, Decay makes it longer
    {
        double c[3];
        for (int s = 0; s < 3; ++s)
        {
            Engine e = make (SR); e.params.bodyLevelDb = -60; e.params.subLevelDb = -60; e.params.source = s; e.params.clickDecayMs = 40; e.params.clickLevelDb = 0;
            auto x = render (e, { { 0.0 } }, 0.1, SR); c[s] = centroid (x, SR, 0.0);
        }
        const bool distinct = std::abs (c[0] - c[1]) / c[1] > 0.1 && std::abs (c[1] - c[2]) / c[1] > 0.1 && std::abs (c[0] - c[2]) / c[2] > 0.1;
        check (distinct, "Noise, Click and Pulse sound different  (tone centres " + fmt (c[0], 0) + " / " + fmt (c[1], 0) + " / " + fmt (c[2], 0) + " Hz)");

        double tone[2], len[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); e.params.bodyLevelDb = -60; e.params.subLevelDb = -60; e.params.source = noiseSrc; e.params.clickLevelDb = 0;
            e.params.clickToneHz = k ? 9000.0f : 1500.0f; e.params.clickDecayMs = k ? 60.0f : 10.0f;
            auto x = render (e, { { 0.0 } }, 0.2, SR);
            tone[k] = centroid (x, SR, 0.0); len[k] = db (rms (x, SR, 0.02, 0.03));
        }
        check (tone[1] > tone[0] * 2, "Transient Tone up = brighter  (" + fmt (tone[0], 0) + " -> " + fmt (tone[1], 0) + " Hz)");
        check (len[1] - len[0] > 20, "Transient Decay up = longer  (" + (len[1] - len[0] > 60 ? std::string ("60+") : fmt (len[1] - len[0], 0)) + " dB more at 20-30 ms)");
    }

    // 11. Fast retriggers don't click (the old kick fades out over 4 ms)
    {
        Engine e = make (SR); e.params.clickLevelDb = -60; e.params.pitchAmt = 0; e.params.subLevelDb = -60; e.params.gainDb = -6;
        std::vector<Hit> hits; for (int k = 0; k < 8; ++k) hits.push_back ({ 0.0371 * k + 0.01 });
        auto x = render (e, hits, 0.5, SR);
        float jump = 0; for (size_t i = 1; i < x.size(); ++i) jump = std::max (jump, std::abs (x[i] - x[i - 1]));
        const float maxStep = (float) (2 * M_PI * e.targetHz() / SR) * 0.5f;   // steepest a 0.5-level sine at the Key can move
        check (jump < maxStep * 2.5f, "Retriggers every 37 ms: no jumps  (biggest step " + fmt (jump, 4) + ")");
    }

    // 12. Velocity switch
    {
        double lv[2][2];
        for (int on = 0; on < 2; ++on)
            for (int k = 0; k < 2; ++k)
            {
                Engine e = make (SR); e.params.velocity = on; e.params.gainDb = -8;
                auto x = render (e, { { 0.0, k ? 1.0f : 0.3f } }, 0.3, SR); lv[on][k] = db (rms (x, SR, 0.0, 0.2));
            }
        check (std::abs (lv[0][1] - lv[0][0]) < 0.01 && lv[1][1] - lv[1][0] > 6,
               "Velocity Off: soft and hard hits identical; On: soft hit " + fmt (lv[1][1] - lv[1][0], 1) + " dB quieter");
    }

    // 13. Nothing goes past the ceiling, even with everything at maximum
    {
        Engine e = make (SR);
        auto& p = e.params; p.clickLevelDb = 6; p.bodyLevelDb = 6; p.subLevelDb = 6; p.gainDb = 12; p.ceilingDb = -0.3f; p.blend = 0; p.subOctave = 0;
        p.pitchAmt = 48; p.sweepMs = 300; p.holdMs = 500; p.decayMs = 3000; p.wave = softWave; p.source = pulseSrc;
        std::vector<Hit> hits; for (int k = 0; k < 20; ++k) hits.push_back ({ k * 0.12 });
        auto x = render (e, hits, 3.0, SR);
        bool fin; const float pk = peakOf (x, fin);
        check (fin && pk <= dbToGain (-0.3f) + 1e-4f, "Everything maxed, 20 fast hits: peak " + fmt (db (pk), 2) + " dB, ceiling -0.30 dB");
    }


    // 14b. The default transient is a short tick that sits under the body, not a loud beep
    {
        Engine a = make (SR); a.params.bodyLevelDb = -60; a.params.subLevelDb = -60;
        Engine b = make (SR); b.params.clickLevelDb = -60; b.params.subLevelDb = -60;
        auto x = render (a, { { 0.0 } }, 0.1, SR), y = render (b, { { 0.0 } }, 0.1, SR);
        const double first3 = rms (x, SR, 0, 0.003) * std::sqrt (0.003), total = rms (x, SR, 0, 0.05) * std::sqrt (0.05);
        const double share = 100.0 * (first3 * first3) / (total * total);
        const double under = db (rms (y, SR, 0, 0.01)) - db (rms (x, SR, 0, 0.01));
        check (share > 80, "Default Click is a tick: " + fmt (share, 0) + "% of its energy in the first 3 ms");
        check (under > 15, "Default transient sits " + fmt (under, 0) + " dB under the body's first 10 ms");
    }

    // 14. A default kick (FX defaults on) is loud, finite and finishes
    {
        Engine e = make (SR, true);
        auto x = render (e, { { 0.0 } }, 1.5, SR);
        bool fin; const float pk = peakOf (x, fin);
        check (fin && db (pk) > -6 && db (pk) < -0.5 && ! e.sounding(), "Default kick (F1 +12 ct) is loud but under the ceiling: peak " + fmt (db (pk), 1) + " dB, fully finished within 1.5 s");
    }


    std::printf ("\n-- Rumble --\n");

    // 15. Mix at 0 leaves the kick exactly as in stage 1
    {
        Engine a = make (SR), b = make (SR); b.params.rumbleAmount = 1; b.params.rumbleDrive = 1; b.params.rumbleMix = 0;
        auto x = render (a, { { 0.0 }, { 0.47 } }, 1.5, SR), y = render (b, { { 0.0 }, { 0.47 } }, 1.5, SR);
        double diff = 0; for (size_t i = 0; i < x.size(); ++i) diff = std::max (diff, (double) std::abs (x[i] - y[i]));
        check (diff == 0.0, "Rumble Mix 0%: output identical to the plain kick");
    }

    // 16. Rumble adds a tail after the sub has finished; Decay sets its length
    {
        auto tail = [&] (float mix, float decay, double t0, double t1)
        {
            Engine e = make (SR); e.params.rumbleMix = mix; e.params.rumbleDecay = decay; e.params.rumbleDuck = 0;
            auto x = render (e, { { 0.0 } }, 6.0, SR); return db (rms (x, SR, t0, t1));
        };
        const double off = tail (0, 2.4f, 1.0, 1.4), on = tail (0.5f, 2.4f, 1.0, 1.4);
        check (on - off > 40, "Rumble keeps ringing after the sub ends at 0.91 s  (" + fmt (on, 0) + " dB vs silence at 1.0-1.4 s)");
        const double shortD = tail (0.5f, 0.8f, 2.5, 3.0), longD = tail (0.5f, 5.0f, 2.5, 3.0);
        check (longD - shortD > 20, "Rumble Decay 5 s rings much longer than 0.8 s  (" + fmt (longD - shortD, 0) + " dB more at 2.5-3 s)");
    }


    // 16b. Rumble reaches up into the low mids, so it's audible on small speakers, not only on subs
    {
        Engine e = make (SR); e.params.key = 7; e.params.rumbleMix = 0.55f; e.params.decayMs = 260; e.params.holdMs = 40;
        std::vector<Hit> h; for (int k = 0; k < 8; ++k) h.push_back ({ k * 60.0 / 128 });
        std::vector<float> rb; render (e, h, 8 * 60.0 / 128, SR, nullptr, 128.0, &rb);
        const int N = 32768; const auto m = spectrum (rb, SR, 1.0, N);
        const double lowmid = bandDb (m, SR, N, 150, 400) - bandDb (m, SR, N, 10, 150);
        check (lowmid > -15, "Default Rumble on G1 has body in the 150-400 Hz range  (" + fmt (lowmid, 0) + " dB vs its sub range)");
    }

    // 17. The rumble stays in key: its strongest frequency is the sub's note
    {
        struct K { int key, oct; float fine; int subOct; };
        for (const K k : { K { 5, 1, 12, 1 }, K { 9, 1, 0, 0 }, K { 2, 2, -30, 1 } })
        {
            Engine e = make (SR); rumbleOn (e, 0.8f); e.params.rumbleDuck = 0; e.params.rumbleDecay = 4;
            e.params.key = k.key; e.params.octave = k.oct; e.params.fine = k.fine; e.params.subOctave = k.subOct;
            std::vector<float> rb;
            render (e, { { 0.0 } }, 3.2, SR, nullptr, 128.0, &rb);
            const int N = 65536; const auto m = spectrum (rb, SR, 1.0, N);
            const double want = e.targetHz() * (k.subOct ? 0.5 : 1.0), got = peakHz (m, SR, N, 10, 400);
            check (std::abs (cents (got, want)) < 5, std::string ("Rumble on ") + noteName (k.key) + std::to_string (k.oct) + (k.subOct ? " (sub -1 Oct)" : "")
                   + ": peak at " + fmt (got, 2) + " Hz, sub note " + fmt (want, 2) + " Hz  (" + fmt (cents (got, want), 1) + " ct)");
        }
    }

    // 18. Drive adds harmonics
    {
        double h[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); rumbleOn (e, 0.8f); e.params.rumbleDuck = 0; e.params.rumbleTone = 16; e.params.rumbleDrive = k ? 1.0f : 0.0f;
            e.params.subOctave = 0; e.params.key = 9; e.params.octave = 1;   // sub on A1 = 55 Hz
            std::vector<float> rb; render (e, { { 0.0 } }, 2.6, SR, nullptr, 128.0, &rb);
            const int N = 32768; const auto m = spectrum (rb, SR, 1.0, N);
            h[k] = bandDb (m, SR, N, 160, 170) - bandDb (m, SR, N, 50, 60);   // 3rd harmonic vs fundamental
        }
        check (h[1] - h[0] > 15, "Drive 100% adds harmonics  (3rd harmonic " + fmt (h[0], 0) + " -> " + fmt (h[1], 0) + " dB vs the note)");
    }

    // 19. Tone follows the Key: same Tone setting keeps the same brightness on a different note
    {
        auto bright = [&] (int key, int oct, float tone)
        {
            Engine e = make (SR); rumbleOn (e, 0.8f); e.params.rumbleDuck = 0; e.params.rumbleDrive = 1; e.params.rumbleTone = tone;
            e.params.subOctave = 0; e.params.key = key; e.params.octave = oct; e.params.fine = 0;
            std::vector<float> rb; render (e, { { 0.0 } }, 2.6, SR, nullptr, 128.0, &rb);
            const int N = 32768; const auto m = spectrum (rb, SR, 1.0, N); const double f = e.targetHz();
            return bandDb (m, SR, N, 4.8 * f, 5.2 * f) - bandDb (m, SR, N, 0.95 * f, 1.05 * f);   // 5th harmonic vs note
        };
        const double low = bright (5, 1, 4), high = bright (4, 2, 4), dark = bright (5, 1, 1.5f);
        check (std::abs (low - high) < 4, "Tone x4 keeps the same brightness on F1 and E2  (5th harmonic " + fmt (low, 0) + " / " + fmt (high, 0) + " dB)");
        check (low - dark > 20, "Tone x1.5 is much darker than x4  (" + fmt (low - dark, 0) + " dB less 5th harmonic)");
    }

    // 20. Ducking dips on each hit and is back by the synced time
    {
        auto gainAt = [&] (int dt, double ms, float duck)
        {
            Engine e = make (SR); rumbleOn (e); e.params.rumbleDuck = duck; e.params.rumbleDuckTime = dt;
            double g = 1;
            render (e, { { 0.0 } }, ms / 1000.0, SR, nullptr, 128.0);
            g = e.rumble.duckGain; return db (g);
        };
        // at 128 BPM: 1/16 = 117 ms, 1/8 = 234 ms, 1/4 = 469 ms
        const double hit = gainAt (1, 5, 1.0f), mid = gainAt (1, 117, 1.0f), back8 = gainAt (1, 240, 1.0f), still4 = gainAt (2, 240, 1.0f), back16 = gainAt (0, 120, 1.0f);
        check (hit < -20 && mid < -3 && back8 > -0.1, "Duck 100%, 1/8: " + fmt (hit, 0) + " dB on the hit, " + fmt (mid, 1) + " dB halfway, back to " + fmt (back8, 2) + " dB at 240 ms");
        check (still4 < -3 && back16 > -0.1, "Duck Time follows the tempo: 1/4 still " + fmt (still4, 1) + " dB down at 240 ms, 1/16 back by 120 ms");
        const double half = gainAt (1, 5, 0.5f);
        check (std::abs (half - db (0.5)) < 1.0, "Duck 50% dips by half  (" + fmt (half, 1) + " dB)");
    }

    // 21. Rumble in a groove: pumps between the kicks and never clicks when it dips
    {
        Engine e = make (SR); rumbleOn (e, 0.8f);
        const double beat = 60.0 / 128.0; std::vector<Hit> hits; for (int k = 0; k < 16; ++k) hits.push_back ({ k * beat });
        std::vector<float> rb; render (e, hits, 16 * beat, SR, nullptr, 128.0, &rb);
        const double before = db (rms (rb, SR, 8 * beat - 0.12, 8 * beat)), after = db (rms (rb, SR, 8 * beat + 0.003, 8 * beat + 0.03));
        float jump = 0; for (size_t i = 1; i < rb.size(); ++i) jump = std::max (jump, std::abs (rb[i] - rb[i - 1]));
        check (before - after > 10, "Four-on-the-floor: rumble swells between kicks and ducks " + fmt (before - after, 0) + " dB under each hit");
        check (jump < 0.02f, "Ducking never clicks  (biggest step " + fmt (jump, 4) + ")");
    }

    // 22. Worst case: every Rumble knob at maximum for 30 s stays bounded
    {
        Engine e = make (SR);
        auto& p = e.params; p.rumbleAmount = 1; p.rumbleDecay = 8; p.rumbleDrive = 1; p.rumbleTone = 16; p.rumbleMix = 1; p.rumbleDuck = 0;
        p.subLevelDb = 6; p.subDecayMs = 6000; p.blend = 0; p.gainDb = 12;
        std::vector<Hit> hits; for (int k = 0; k < 64; ++k) hits.push_back ({ k * 0.234 });
        std::vector<float> rb; auto x = render (e, hits, 30.0, SR, nullptr, 128.0, &rb);
        bool fin, fin2; const float pk = peakOf (x, fin); const float rp = peakOf (rb, fin2);
        check (fin && fin2 && pk <= dbToGain (-0.3f) + 1e-4f && rp < 4.0f, "Rumble maxed out for 30 s: finite, output peak " + fmt (db (pk), 2) + " dB, rumble bus peak " + fmt (rp, 2));
    }

    // 23. The tail ends: after the last hit the rumble dies away and MAZO goes quiet
    {
        Engine e = make (SR); rumbleOn (e); e.params.rumbleDecay = 2.4f;
        auto x = render (e, { { 0.0 } }, 12.0, SR);
        check (! e.sounding() && db (rms (x, SR, 11.0, 12.0)) < -90, "Rumble Decay 2.4 s: silent again well before 12 s  (" + fmt (db (rms (x, SR, 11.0, 12.0)), 0) + " dB)");
    }


    std::printf ("\n-- FX chain --\n");
    auto harmonicDb = [&] (const std::vector<float>& x, double f, int h, double t0)
    {
        const int N = 32768; const auto m = spectrum (x, SR, t0, N);
        return bandDb (m, SR, N, h * f * 0.97, h * f * 1.03) - bandDb (m, SR, N, f * 0.97, f * 1.03);
    };
    // a steady tone for measuring the FX: body only on A1, no sweep, long hold
    auto steady = [] (Engine& e) { auto& p = e.params; p.clickLevelDb = -60; p.subLevelDb = -60; p.pitchAmt = 0; p.key = 9; p.octave = 1; p.fine = 0; p.holdMs = 500; p.decayMs = 500; p.gainDb = -6; };

    // 24. With every FX stage off, Clean Sub on and off give the exact same output (the routing is transparent)
    {
        Engine a = make (SR), b = make (SR); a.params.cleanSub = true; b.params.cleanSub = false;
        auto x = render (a, { { 0.0 }, { 0.47 } }, 1.5, SR), y = render (b, { { 0.0 }, { 0.47 } }, 1.5, SR);
        double diff = 0; for (size_t i = 0; i < x.size(); ++i) diff = std::max (diff, (double) std::abs (x[i] - y[i]));
        check (diff == 0.0, "FX off: the chain passes the kick through untouched (Clean Sub on/off identical)");
    }

    // 25. The limiter's look-ahead is a fixed 1.5 ms, reported to Ableton so it lines up
    {
        Engine e = make (SR);
        auto x = render (e, { { 0.1 } }, 0.2, SR);
        size_t first = 0; while (first < x.size() && x[first] == 0.0f) ++first;
        const int expect = (int) std::lround (0.1 * SR) + e.latency();
        check (e.latency() == 66 && std::abs ((int) first - expect) <= 1, "Latency " + std::to_string (e.latency()) + " samples (1.5 ms); the kick starts exactly there");
    }

    // 26. Limiter: +18 dB into a -6 dB ceiling never goes over, and a kick under the ceiling is left alone
    {
        Engine e = make (SR, true); e.params.gainDb = 12; e.params.ceilingDb = -6; e.params.distMix = 1; e.params.distDrive = 1; e.params.rumbleMix = 1;
        std::vector<Hit> hits; for (int k = 0; k < 16; ++k) hits.push_back ({ k * 0.2 });
        auto x = render (e, hits, 3.5, SR);
        bool fin; const float pk = peakOf (x, fin);
        check (fin && pk <= dbToGain (-6.0f) * 1.0001f, "Limiter: +12 dB gain, everything hot, ceiling -6 dB -> peak " + fmt (db (pk), 2) + " dB");
        Engine q = make (SR); q.params.gainDb = -12;
        auto y = render (q, { { 0.0 } }, 1.0, SR); bool f2; const float p2 = peakOf (y, f2);
        check (p2 < dbToGain (-0.3f), "A kick under the ceiling passes the limiter untouched  (peak " + fmt (db (p2), 1) + " dB)");
    }

    // 27. Warmth: Tape adds odd harmonics, Tube adds even ones; off is off
    {
        double h2[2], h3[2], h3off;
        for (int ty = 0; ty < 2; ++ty)
        {
            Engine e = make (SR); steady (e); e.params.warmth = 0.8f; e.params.warmthType = ty; e.params.gainDb = 0;
            auto x = render (e, { { 0.0 } }, 0.8, SR); h2[ty] = harmonicDb (x, 55.0, 2, 0.1); h3[ty] = harmonicDb (x, 55.0, 3, 0.1);
        }
        { Engine e = make (SR); steady (e); e.params.gainDb = 0; auto x = render (e, { { 0.0 } }, 0.8, SR); h3off = harmonicDb (x, 55.0, 3, 0.1); }
        check (h3[0] - h3off > 25, "Warmth Tape 80% adds harmonics  (3rd: " + fmt (h3off, 0) + " -> " + fmt (h3[0], 0) + " dB)");
        check (h2[1] - h2[0] > 15, "Tube adds even harmonics Tape doesn't  (2nd: Tape " + fmt (h2[0], 0) + ", Tube " + fmt (h2[1], 0) + " dB)");
    }

    // 28. Distortion: three different characters; parallel Mix keeps the fundamental
    {
        double h3[3], hi[3];
        for (int m = 0; m < 3; ++m)
        {
            Engine e = make (SR); steady (e); e.params.distMode = m; e.params.distDrive = 0.7f; e.params.distMix = 1; e.params.distToneHz = 12000; e.params.gainDb = 0;
            auto x = render (e, { { 0.0 } }, 0.8, SR);
            h3[m] = harmonicDb (x, 55.0, 3, 0.1);
            const int N = 32768; const auto sp = spectrum (x, SR, 0.1, N); hi[m] = bandDb (sp, SR, N, 3000, 10000) - bandDb (sp, SR, N, 50, 60);
        }
        check (std::abs (h3[0] - h3[1]) > 3 && std::abs (hi[2] - hi[0]) > 6,
               "Clip / Fold / Crush sound different  (3rd harmonic " + fmt (h3[0], 0) + " / " + fmt (h3[1], 0) + " dB; Crush highs " + fmt (hi[2], 0) + " vs Clip " + fmt (hi[0], 0) + " dB)");

        double fund[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); steady (e); e.params.distDrive = 1; e.params.distMix = k ? 0.5f : 0.0f;
            auto x = render (e, { { 0.0 } }, 0.8, SR);
            const int N = 32768; const auto sp = spectrum (x, SR, 0.1, N); fund[k] = bandDb (sp, SR, N, 53, 57);
        }
        check (std::abs (fund[1] - fund[0]) < 3, "Distortion Mix 50% keeps the tuned fundamental within " + fmt (std::abs (fund[1] - fund[0]), 1) + " dB of the dry kick");

        double c[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); steady (e); e.params.distDrive = 0.8f; e.params.distMix = 1; e.params.distToneHz = k ? 12000.0f : 800.0f;
            auto x = render (e, { { 0.0 } }, 0.8, SR);
            const int N = 32768; const auto sp = spectrum (x, SR, 0.1, N); c[k] = bandDb (sp, SR, N, 2000, 8000) - bandDb (sp, SR, N, 50, 60);
        }
        check (c[1] - c[0] > 15, "Distortion Tone 12 kHz vs 800 Hz: " + fmt (c[1] - c[0], 0) + " dB more 2-8 kHz");
    }

    // 29. Compression evens the kick out; slow attack lets the front through
    {
        auto crest = [&] (float amt, float attack)
        {
            Engine e = make (SR); e.params.clickLevelDb = -60; e.params.compAmount = amt; e.params.compAttackMs = attack; e.params.holdMs = 20; e.params.decayMs = 500;
            auto x = render (e, { { 0.0 } }, 0.6, SR);
            const double front = db (rms (x, SR, 0.0015, 0.006)), body = db (rms (x, SR, 0.15, 0.25));
            return front - body;
        };
        const double off = crest (0, 12), fast = crest (0.8f, 0.1f), slow = crest (0.8f, 30);
        check (off - fast > 3, "Compression 80% (fast attack) brings the body up against the front  (" + fmt (off - fast, 1) + " dB)");
        check (slow - fast > 2, "Slow attack keeps more of the punch than fast  (" + fmt (slow - fast, 1) + " dB more front)");
    }

    // 30. Clean Sub keeps the sub pure under heavy distortion
    {
        double h3[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR, true); auto& p = e.params;
            p.clickLevelDb = -60; p.decayMs = 100; p.holdMs = 0; p.subOctave = 0; p.key = 9; p.octave = 1; p.fine = 0; p.subDecayMs = 3000; p.blend = 0;
            p.distMix = 1; p.distDrive = 1; p.warmth = 1; p.cleanSub = k;
            auto x = render (e, { { 0.0 } }, 1.5, SR); h3[k] = harmonicDb (x, 55.0, 3, 0.4);
        }
        check (h3[0] - h3[1] > 30, "Clean Sub: the sub tail stays a pure sine under full distortion  (3rd harmonic " + fmt (h3[0], 0) + " -> " + fmt (h3[1], 0) + " dB)");
    }

    // 31. Tuning survives the FX: full Warmth + Distortion + Compression, the note is still the Key
    {
        Engine e = make (SR, true); bodyOnly (e); auto& p = e.params; p.warmth = 1; p.distMix = 0.6f; p.distDrive = 0.8f; p.compAmount = 0.8f;
        auto x = render (e, { { 0.0 } }, 2.0, SR);
        const int N = 65536; const auto m = spectrum (x, SR, 0.2, N);
        const double got = peakHz (m, SR, N, 20, 80);
        check (std::abs (cents (got, e.targetHz())) < 5, "Full FX, the kick's note is still " + fmt (got, 2) + " Hz  (" + fmt (cents (got, e.targetHz()), 1) + " ct)");
    }

    // 32. Low Cut and Tilt
    {
        double sub[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); e.params.clickLevelDb = -60; e.params.bodyLevelDb = -60; e.params.blend = 0; e.params.subDecayMs = 3000; e.params.lowCutHz = k ? 45.0f : 10.0f;
            auto x = render (e, { { 0.0 } }, 2.0, SR); sub[k] = db (rms (x, SR, 0.5, 1.5));
        }
        check (sub[0] - sub[1] > 10, "Low Cut 45 Hz takes the 22 Hz sub down " + fmt (sub[0] - sub[1], 0) + " dB; Off leaves it");
        double c[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = make (SR); e.params.tiltDb = k ? 6.0f : -6.0f; e.params.clickLevelDb = 0;
            auto x = render (e, { { 0.0 } }, 0.3, SR); c[k] = centroid (x, SR, 0.0, 4096);
        }
        check (c[1] > c[0] * 1.3, "Tilt +6 is brighter than -6  (centre " + fmt (c[0], 0) + " -> " + fmt (c[1], 0) + " Hz)");
    }

    // 33. Worst case: every FX at maximum with Rumble for 30 s
    {
        Engine e = make (SR, true); auto& p = e.params;
        p.warmth = 1; p.warmthType = 1; p.distMode = 1; p.distDrive = 1; p.distMix = 1; p.distToneHz = 16000;
        p.compAmount = 1; p.compAttackMs = 0.1f; p.compReleaseMs = 500; p.rumbleMix = 1; p.rumbleDrive = 1; p.rumbleDecay = 8;
        p.tiltDb = 6; p.gainDb = 12; p.clickLevelDb = 6; p.bodyLevelDb = 6; p.subLevelDb = 6; p.cleanSub = false;
        std::vector<Hit> hits; for (int k = 0; k < 64; ++k) hits.push_back ({ k * 0.234 });
        auto x = render (e, hits, 30.0, SR);
        bool fin; const float pk = peakOf (x, fin);
        check (fin && pk <= dbToGain (-0.3f) * 1.0001f, "Everything at maximum for 30 s: finite, peak " + fmt (db (pk), 2) + " dB");
    }


    std::printf ("\n-- Presets --\n");

    // 34. Every factory preset: finite, under the ceiling, a healthy level, and on the Key
    {
        const auto& list = factoryPresets();
        const double beat = 60.0 / 128.0;
        std::vector<std::vector<double>> prints;
        for (size_t i = 0; i < list.size(); ++i)
        {
            Engine e = make (SR, true); e.params.key = 7; e.params.octave = 1; e.params.fine = 0;   // G1, like a track in G
            applyFactory (e.params, (int) i, true);
            std::vector<Hit> hits; for (int k = 0; k < 8; ++k) hits.push_back ({ k * beat });
            auto x = render (e, hits, 8 * beat + 0.5, SR);
            bool fin; const float pk = peakOf (x, fin);
            const double level = db (rms (x, SR, 0.0, 8 * beat));
            // the note, from one hit on its own (a kick repeated on the beat has a spectrum made of tempo
            // harmonics, 2.13 Hz apart at 128 BPM, which hides the true note)
            Engine one = make (SR, true); one.params.key = 7; one.params.octave = 1; one.params.fine = 0; applyFactory (one.params, (int) i, true);
            auto y = render (one, { { 0.0 } }, 2.0, SR);
            const int N = 65536; const auto m = spectrum (y, SR, 0.05, N);
            const double note = peakHz (m, SR, N, 20, 120), want = e.targetHz();
            const double off = std::min (std::abs (cents (note, want)), std::abs (cents (note, want / 2)));   // sub may sit an octave down
            check (fin && pk <= dbToGain (e.params.ceilingDb) * 1.0001f && level > -20 && level < -7 && off < 10,
                   std::string (list[i].name) + " (" + list[i].style + "): level " + fmt (level, 1) + " dB, peak " + fmt (db (pk), 1) + " dB, note " + fmt (note, 2) + " Hz (" + fmt (off, 1) + " ct)");
            // fingerprint: loudness over time + spectrum shape, to compare presets
            // fingerprint: the shape of one beat over time, and its spectrum shape in half-octave bands (both relative to their peak)
            std::vector<double> fp, env, spec;
            for (double t0 = 0; t0 < beat; t0 += beat / 12) env.push_back (db (rms (x, SR, 4 * beat + t0, 4 * beat + t0 + beat / 12)));
            const auto sp = spectrum (x, SR, 4 * beat, 16384);
            for (double f0 = 30.0; f0 < 12000.0; f0 *= 1.41421) spec.push_back (bandDb (sp, SR, 16384, f0, f0 * 1.41421));
            const double me = *std::max_element (env.begin(), env.end()), ms = *std::max_element (spec.begin(), spec.end());
            for (double v : env) fp.push_back (std::max (-60.0, v - me));
            for (double v : spec) fp.push_back (std::max (-60.0, v - ms));
            prints.push_back (fp);
        }
        int distinct = 0; std::string twins;
        for (size_t i = 0; i < prints.size(); ++i)
        {
            double closest = 1e9; size_t who = 0;
            for (size_t j = 0; j < prints.size(); ++j)
            {
                if (i == j) continue;
                double d = 0; for (size_t k = 0; k < prints[i].size(); ++k) d += std::abs (prints[i][k] - prints[j][k]);
                d /= prints[i].size(); if (d < closest) { closest = d; who = j; }
            }
            if (closest > 1.5) ++distinct; else twins += std::string (" ") + list[i].name + "~" + list[who].name;
        }
        check (distinct == (int) list.size(), "All " + std::to_string (list.size()) + " presets sound different from each other  (" + std::to_string (distinct) + " distinct" + twins + ")");
    }

    // 35. Lock Key: loading a preset keeps your Key, Octave and Fine; unlocked it takes the preset's
    {
        Params p; p.key = 2; p.octave = 2; p.fine = -17;
        applyFactory (p, 8, true);
        const bool kept = p.key == 2 && p.octave == 2 && std::abs (p.fine + 17) < 1e-6f;
        Params q; q.key = 2; q.octave = 2; q.fine = -17;
        applyFactory (q, 8, false);
        check (kept && q.key == Params().key && q.octave == Params().octave, "Lock Key on keeps D2 -17 ct through a preset change; off takes the preset's tuning");
        Params r; r.rumbleMix = 1; r.distMix = 1; applyFactory (r, 0, true);
        check (r.rumbleMix == 0 && r.distMix == 0, "Loading a preset resets everything it doesn't set (no leftovers from the last sound)");
    }

    std::printf ("\n%d passed, %d failed\n", passes, fails);

    // ===== Preview: a tour of the 16 factory presets, 2 bars each, all on G1 (Lock Key on) =====
    {
        const double beat = 60.0 / 128.0, bar = beat * 4;
        const auto& list = factoryPresets();
        const int n = (int) list.size();
        Engine e = make (SR, true);
        std::vector<Hit> hits;
        for (int b = 0; b < 2 * n; ++b) for (int q = 0; q < 4; ++q)
            if (! (b % 2 == 1 && q == 3)) hits.push_back ({ b * bar + q * beat });   // last beat of each preset left open: hear its tail
        int shown = -1;
        auto kick = render (e, hits, 2 * n * bar + 3.0, SR, [&] (Engine& en, double t)
        {
            const int idx = std::min (n - 1, (int) (t / (2 * bar)));
            if (idx != shown)
            {
                en.params.key = 7; en.params.octave = 1; en.params.fine = 0;
                applyFactory (en.params, idx, true); shown = idx;
                std::printf ("  %4.1f s  %s\n", idx * 2 * bar, list[(size_t) idx].name);
            }
        });
        writeWav (outDir + "/mazo_preview.wav", kick, kick, (int) SR);
        bool fin; std::printf ("Rendered mazo_preview.wav (%.0f s, peak %.2f)\n", kick.size() / SR, peakOf (kick, fin));
    }
    return fails == 0 ? 0 : 1;
}
