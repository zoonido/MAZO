// MAZO - sound engine (stages 1-3)
// One kick voice: Transient (click layer) + Body (pitch sweep that lands exactly on the Key) + Sub/Tail,
// and Rumble on the Sub/Tail: reverb -> drive -> key-tracked low pass -> tempo-synced ducking.
// FX chain: Warmth -> Distortion -> Compression (Clean Sub goes around these) -> Low Cut + Tilt -> Gain -> Limiter.
// Plain C++17, no JUCE, so the same code runs in the plugin and in the automated tests.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace mazo
{
constexpr double twoPiD = 6.283185307179586;
constexpr float pi = 3.14159265358979f;

inline float dbToGain (float db) { return db <= -59.9f ? 0.0f : std::pow (10.0f, db / 20.0f); }

inline const char* noteName (int k)
{
    static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[std::clamp (k, 0, 11)];
}

// Key (0 = C .. 11 = B), Octave (0..2), Fine in cents -> exact frequency. F1 = 43.65 Hz.
inline double keyHz (int key, int octave, float fineCents)
{
    const int midi = 12 + 12 * octave + key;
    return 440.0 * std::pow (2.0, ((double) midi - 69.0 + (double) fineCents / 100.0) / 12.0);
}

enum Source { noiseSrc, clickSrc, pulseSrc };
enum Wave { sineWave, triWave, softWave };

//==============================================================================
struct Params
{
    // Tuning: MIDI notes only trigger, the pitch always comes from here
    int key = 5, octave = 1; float fine = 12.0f;   // F1 +12 ct
    bool velocity = false;                           // off = every hit the same (machine-flat)

    // Transient
    int source = clickSrc;
    float clickDecayMs = 6.0f, clickToneHz = 3500.0f, clickLevelDb = -8.0f;

    // Body
    int wave = sineWave;
    float pitchAmt = 36.0f;          // semitones above the Key at the start of the sweep
    float sweepMs = 28.0f;           // time to land on the Key
    float curve = 0.7f;              // 0 = straight line, 1 = drops fast then settles slowly
    float attackMs = 0.0f, holdMs = 60.0f, decayMs = 340.0f, bodyLevelDb = 0.0f;

    // Sub / Tail
    int subOctave = 1;               // 0 = unison, 1 = one octave down
    bool cleanSub = true;            // stage 3: keeps the sub out of the FX
    float subAttackMs = 10.0f, subDecayMs = 900.0f, subLevelDb = -4.0f;
    float blend = 0.45f;             // 0 = sub under the whole kick, 1 = sub only blooms as the body fades

    // Rumble (on the Sub/Tail only). Mix starts at 0 so a new MAZO is a clean kick;
    // turn Mix up and the rest is already set for a rolling techno rumble.
    float rumbleAmount = 0.6f;       // how much of the kick goes into the rumble (0..1)
    float rumbleDecay = 2.4f;        // reverb length in seconds (0.3..8)
    float rumbleDrive = 0.65f;       // saturation after the reverb (0..1)
    float rumbleTone = 8.0f;         // low-pass cutoff as a multiple of the Key frequency (1..16)
    float rumbleDuck = 0.8f;         // how far the rumble dips on each hit (0..1)
    int rumbleDuckTime = 1;          // recovery: 0 = 1/16, 1 = 1/8, 2 = 1/4
    float rumbleMix = 0.0f;          // rumble level (0..1, 100% = +6 dB)

    // Warmth: gentle tape/tube saturation (0 = off)
    int warmthType = 0;              // 0 = Tape, 1 = Tube
    float warmth = 0.25f;

    // Distortion: parallel, so the clean fundamental (and the tuning) stays solid. Mix starts at 0.
    int distMode = 0;                // 0 = Clip, 1 = Fold, 2 = Crush
    float distDrive = 0.4f, distToneHz = 2800.0f, distMix = 0.0f;

    // Compression
    float compAttackMs = 12.0f, compReleaseMs = 90.0f, compAmount = 0.3f, compMix = 1.0f;

    // Output
    float lowCutHz = 10.0f;          // 10 = off (the -1 Oct sub on F1 sits at 22 Hz)
    float tiltDb = 0.0f;             // -6..+6, pivot around 700 Hz
    float gainDb = -4.0f, ceilingDb = -0.3f;
};

inline const char* warmthName (int i) { static const char* n[] { "Tape", "Tube" }; return n[std::clamp (i, 0, 1)]; }
inline const char* distName (int i) { static const char* n[] { "Clip", "Fold", "Crush" }; return n[std::clamp (i, 0, 2)]; }

inline const char* duckTimeName (int i) { static const char* n[] { "1/16", "1/8", "1/4" }; return n[std::clamp (i, 0, 2)]; }
inline double duckTimeQuarters (int i) { static const double q[] { 0.25, 0.5, 1.0 }; return q[std::clamp (i, 0, 2)]; }

//==============================================================================
struct Svf   // Zavalishin TPT state variable filter
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1.4f;
    void set (float hz, float sr, float damping)
    {
        hz = std::clamp (hz, 10.0f, 0.45f * sr);
        const float g = std::tan (pi * hz / sr); k = damping;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    float lp (float v0)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v2;
    }
    float hp (float v0)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v0 - k * v1 - v2;
    }
    float bp (float v0)   // unity-peak band pass
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v1 * k;
    }
};

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 8388608.0f - 1.0f; }
};

//==============================================================================
// Envelope shapes. Every shape reaches exactly its end value at the end of its time,
// which is what makes the pitch land exactly on the Key (no slow exponential drift).
struct Shapes
{
    // pitch sweep: 1 -> 0 over the sweep time
    static double pitch (double t, double sweepSec, double curve)
    {
        if (sweepSec <= 0.0 || t >= sweepSec) return 0.0;
        const double p = 1.0 + 5.0 * curve;
        return std::pow (1.0 - t / sweepSec, p);
    }
    // body amplitude: attack (linear) -> hold (flat) -> decay (rounded fall to exactly 0)
    static double body (double t, double a, double h, double d)
    {
        if (t < a) return t / a;
        t -= a;
        if (t < h) return 1.0;
        t -= h;
        if (t >= d) return 0.0;
        return std::pow (1.0 - t / d, 2.2);
    }
    static double sub (double t, double a, double d)
    {
        if (t < a) return t / a;
        t -= a;
        if (t >= d) return 0.0;
        return std::pow (1.0 - t / d, 1.8);
    }
};

//==============================================================================
struct Voice
{
    bool active = false;
    long n = 0;                  // samples since the hit
    double bodyPh = 0, subPh = 0, pulsePh = 0;
    float velGain = 1, fade = 1, fadeStep = 0;
    Svf clickF, clickHp;
    Rng rng;

    void start (float vg, uint32_t seed)
    {
        active = true; n = 0; bodyPh = subPh = pulsePh = 0; velGain = vg; fade = 1; fadeStep = 0;
        clickF.reset(); clickHp.reset(); rng.s = seed | 1u;
    }
};


//==============================================================================
// Rumble: the classic techno rumble in one module.
// A TUNED reverb: an 8-line feedback delay network whose line lengths are whole multiples of the
// sub note's period, so every line resonates exactly on the note and the tail rings in key
// (an ordinary reverb rings on its own room modes, which at bass frequencies are tens of cents off).
// Then drive, then a 24 dB low pass tuned to a multiple of the Key, then ducking on every hit.
class Rumble
{
public:
    static constexpr double maxLineSec = 0.5;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int k = 0; k < 8; ++k) { buf[k].assign ((size_t) (maxLineSec * sr) + 4, 0.0f); w[k] = 0; len[k] = -1; }
        sinceHit = 1 << 30;
        reset();
    }

    void hit() { sinceHit = 0; }

    // silence without reallocating (safe on the audio thread)
    void reset()
    {
        for (int k = 0; k < 8; ++k) { std::fill (buf[k].begin(), buf[k].end(), 0.0f); damp[k] = 0; }
        lp1.reset(); lp2.reset(); duckGain = 1; energy = 0;
    }

    // per block: coefficients from the knobs, the note, the Key and the tempo
    void setup (const Params& p, double noteHz, double keyHz, double bpm)
    {
        static const double ms[8] { 61.0, 73.0, 89.0, 101.0, 113.0, 127.0, 139.0, 157.0 };
        const double period = sr / std::max (4.0, noteHz);
        for (int k = 0; k < 8; ++k)
        {
            const double cycles = std::max (1.0, std::round (ms[k] * 0.001 * sr / period));
            double L = std::min (cycles * period, maxLineSec * sr - 4.0);
            if (L > maxLineSec * sr - 4.0) L = std::floor ((maxLineSec * sr - 4.0) / period) * period;
            target[k] = L;
            if (len[k] < 0) len[k] = L;
        }
        lenGlide = 1.0 - std::exp (-1.0 / (0.03 * sr));     // a new Key glides in over ~30 ms instead of clicking
        const double rt = std::clamp ((double) p.rumbleDecay, 0.1, 20.0);
        for (int k = 0; k < 8; ++k) g[k] = (float) std::pow (10.0, -3.0 * target[k] / (rt * sr));
        dampA = 1.0f - std::exp (-2.0f * pi * 3000.0f / (float) sr);
        drive = 1.0f + 39.0f * p.rumbleDrive * p.rumbleDrive;
        makeup = 1.0f / std::sqrt (drive);
        const float fc = (float) std::clamp (keyHz * std::clamp ((double) p.rumbleTone, 1.0, 16.0), 20.0, 0.45 * sr);
        lp1.set (fc, (float) sr, 1.848f); lp2.set (fc, (float) sr, 0.765f);   // 4-pole Butterworth: flat up to the cutoff
        duck = p.rumbleDuck;
        duckLen = std::max (1.0, 60.0 / std::max (20.0, bpm) * duckTimeQuarters (p.rumbleDuckTime) * sr);
        smooth = 1.0f - std::exp (-1.0f / (0.0015f * (float) sr));
    }

    float tick (float in)
    {
        float o[8];
        for (int k = 0; k < 8; ++k)
        {
            len[k] += lenGlide * (target[k] - len[k]);
            const float r = read (k, len[k]);
            damp[k] += dampA * (r - damp[k]);
            o[k] = damp[k];
        }
        float h[8]; for (int k = 0; k < 8; ++k) h[k] = o[k];
        for (int s = 1; s < 8; s <<= 1)                   // fast Hadamard mix
            for (int i = 0; i < 8; i += s << 1)
                for (int j = i; j < i + s; ++j) { const float a = h[j], b = h[j + s]; h[j] = a + b; h[j + s] = a - b; }
        float wet = 0;
        for (int k = 0; k < 8; ++k)
        {
            buf[k][(size_t) w[k]] = in + h[k] * 0.35355339f * g[k];
            if (++w[k] >= (int) buf[k].size()) w[k] = 0;
            wet += o[k];
        }
        wet *= 0.3f;

        // drive, then the key-tracked low pass
        float y = std::tanh (wet * drive) * makeup;
        y = lp2.lp (lp1.lp (y));

        // ducking: dips on the hit, recovers over the synced time (a 1.5 ms slope so the dip never clicks)
        const double x = (double) sinceHit / duckLen;
        const float tg = x < 1.0 ? 1.0f - duck * (float) std::pow (1.0 - x, 1.5) : 1.0f;
        if (sinceHit < (1 << 30)) ++sinceHit;
        duckGain += smooth * (tg - duckGain);
        y *= duckGain;

        energy = 0.9995f * energy + 0.0005f * y * y;
        return y;
    }

    bool ringing() const { return energy > 1e-10f; }
    float duckGain = 1;

private:
    float read (int k, double delay) const
    {
        const auto& b = buf[k]; const int N = (int) b.size();
        double pos = (double) w[k] - delay; while (pos < 0) pos += N;
        const int i1 = (int) pos; const float f = (float) (pos - i1);
        const float y0 = b[(size_t) ((i1 - 1 + N) % N)], y1 = b[(size_t) i1], y2 = b[(size_t) ((i1 + 1) % N)], y3 = b[(size_t) ((i1 + 2) % N)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    double sr = 44100;
    std::vector<float> buf[8];
    int w[8] {};
    double len[8] {}, target[8] {}, lenGlide = 0.001;
    float g[8] {}, damp[8] {}, dampA = 0.3f, drive = 1, makeup = 1, duck = 0, smooth = 0.01f, energy = 0;
    double duckLen = 10000;
    long sinceHit = 1 << 30;
    Svf lp1, lp2;
};


//==============================================================================
// FX chain. Every stage passes the signal through untouched (bit for bit) when it is off.
struct OnePole
{
    float z = 0, a = 0.5f;
    void set (float hz, double sr) { a = 1.0f - std::exp (-2.0f * pi * std::clamp (hz, 1.0f, 0.49f * (float) sr) / (float) sr); }
    float lp (float x) { z += a * (x - z); return z; }
};

class Warmth
{
public:
    void prepare (double sampleRate) { sr = sampleRate; lowBump.z = hfLp.z = 0; dcY = 0; }
    void setup (const Params& p)
    {
        amt = std::clamp (p.warmth, 0.0f, 1.0f); tube = p.warmthType == 1;
        drive = 1.0f + 4.0f * amt;
        lowBump.set (110.0f, sr);                                          // a little low-end weight
        hfLp.set (tube ? 12000.0f - 5000.0f * amt : 18000.0f - 11000.0f * amt, sr);   // tape rounds the top more
        dcA = 1.0f - std::exp (-2.0f * pi * 8.0f / (float) sr);
    }
    float process (float x)
    {
        if (amt <= 0.0f) return x;
        const float in = x + lowBump.lp (x) * 0.45f * amt;
        float y;
        if (tube)
        {
            const float bias = 0.35f * amt;                                  // asymmetric: adds even harmonics
            y = (std::tanh (drive * in + bias) - std::tanh (bias)) / std::sqrt (drive);
            dcY += dcA * (y - dcY); y -= dcY;                                // remove the offset the bias creates
        }
        else y = std::tanh (drive * in) / std::sqrt (drive);
        y = hfLp.lp (y);
        const float fade = std::min (1.0f, amt * 10.0f);                     // no jump when it's first turned on
        return x + fade * (y - x);
    }
private:
    double sr = 44100; float amt = 0, drive = 1, dcA = 0.001f, dcY = 0; bool tube = false;
    OnePole lowBump, hfLp;
};

class Distortion
{
public:
    void prepare (double sampleRate) { sr = sampleRate; tone.reset(); hold = 0; holdCount = 0; }
    void setup (const Params& p)
    {
        mode = p.distMode; mix = std::clamp (p.distMix, 0.0f, 1.0f);
        const float d = std::clamp (p.distDrive, 0.0f, 1.0f);
        pre = mode == 2 ? 1.0f + 2.0f * d : 1.0f + 30.0f * d * d;
        bits = std::pow (2.0f, 15.0f - 12.0f * d);                          // Crush: 16 bits down to 4
        holdLen = 1 + (int) std::lround (d * d * 24.0 * sr / 44100.0);      // Crush: sample rate down to ~1.8 kHz
        tone.set (std::clamp (p.distToneHz, 200.0f, 18000.0f), (float) sr, 1.3f);
    }
    float process (float x)
    {
        float w;
        const float v = x * pre;
        if (mode == 0) w = v <= -1.0f ? -1.0f : (v >= 1.0f ? 1.0f : 1.5f * v - 0.5f * v * v * v);   // Clip: soft corners
        else if (mode == 1) w = std::sin (0.5f * pi * v);                                           // Fold: folds back over
        else
        {
            if (holdCount-- <= 0) { hold = std::round (v * bits) / bits; holdCount = holdLen - 1; }
            w = std::clamp (hold, -1.0f, 1.0f);                                                      // Crush
        }
        w = tone.lp (w) * 0.7f;
        return x + mix * (w - x);
    }
private:
    double sr = 44100; int mode = 0, holdLen = 1, holdCount = 0; float mix = 0, pre = 1, bits = 1024, hold = 0;
    Svf tone;
};

class Compressor
{
public:
    void prepare (double sampleRate) { sr = sampleRate; env = 0; }
    void setup (const Params& p)
    {
        amount = std::clamp (p.compAmount, 0.0f, 1.0f); mix = std::clamp (p.compMix, 0.0f, 1.0f);
        thresh = -6.0f - 24.0f * amount; ratio = 1.0f + 7.0f * amount;
        makeup = 0.5f * (-thresh) * (1.0f - 1.0f / ratio);
        att = 1.0f - std::exp (-1.0f / (std::max (0.05f, p.compAttackMs) * 0.001f * (float) sr));
        rel = 1.0f - std::exp (-1.0f / (std::max (5.0f, p.compReleaseMs) * 0.001f * (float) sr));
    }
    float process (float x)
    {
        const float a = std::abs (x);
        env += (a > env ? att : rel) * (a - env);
        if (amount <= 0.0f) return x;
        const float lvl = 20.0f * std::log10 (env + 1e-9f);
        const float over = lvl - thresh, knee = 6.0f;
        float gr = 0;                                                        // gain reduction in dB (soft knee)
        if (over > knee * 0.5f) gr = over * (1.0f - 1.0f / ratio);
        else if (over > -knee * 0.5f) { const float o = over + knee * 0.5f; gr = (1.0f - 1.0f / ratio) * o * o / (2.0f * knee); }
        lastGr = gr;
        const float wet = x * dbToGain (makeup - gr);
        return x + mix * (wet - x);
    }
    float lastGr = 0;
private:
    double sr = 44100; float amount = 0, mix = 1, thresh = 0, ratio = 1, makeup = 0, att = 0.1f, rel = 0.001f, env = 0;
};

class OutputEq
{
public:
    void prepare (double sampleRate) { sr = sampleRate; hp.reset(); split.z = 0; }
    void setup (const Params& p)
    {
        cut = p.lowCutHz > 10.5f;
        if (cut) hp.set (p.lowCutHz, (float) sr, 1.41421f);                   // 12 dB/oct Butterworth
        tilt = std::clamp (p.tiltDb, -6.0f, 6.0f); tilted = std::abs (tilt) > 0.001f;
        split.set (700.0f, sr);
        gLow = dbToGain (-0.5f * tilt); gHigh = dbToGain (0.5f * tilt);
    }
    float process (float x)
    {
        if (cut) x = hp.hp (x);
        const float lo = split.lp (x);
        if (! tilted) return x;
        return lo * gLow + (x - lo) * gHigh;
    }
private:
    double sr = 44100; bool cut = false, tilted = false; float tilt = 0, gLow = 1, gHigh = 1; Svf hp; OnePole split;
};

// Look-ahead peak limiter: never lets a sample past the ceiling, and shapes the gain change over the look-ahead
// window so it doesn't click. Adds a fixed 1.5 ms delay, which the plugin reports to Ableton (compensated automatically).
class Limiter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate; look = std::max (1, (int) std::lround (0.0015 * sr));
        delay.assign ((size_t) look + 1, 0.0f); need.assign ((size_t) look + 1, 1.0f); w = 0; gain = 1;
        att = 1.0f - std::exp (-1.0f / (0.3f * (float) look));
        rel = 1.0f - std::exp (-1.0f / (0.06f * (float) sr));
    }
    int latency() const { return look; }
    float process (float x, float ceil)
    {
        const float a = std::abs (x);
        delay[(size_t) w] = x; need[(size_t) w] = a > ceil ? ceil / a : 1.0f;
        if (++w > look) w = 0;
        float m = 1.0f; for (float v : need) m = std::min (m, v);              // lowest gain needed in the window ahead
        gain += (m < gain ? att : rel) * (m - gain);
        const float out = delay[(size_t) w];                                  // the oldest sample: 'look' samples ago
        const float g = std::min (gain, need[(size_t) w]);                    // hard guarantee for this sample
        return out * g;
    }
private:
    double sr = 44100; int look = 64, w = 0; float gain = 1, att = 0.1f, rel = 0.001f;
    std::vector<float> delay, need;
};

//==============================================================================
class Engine
{
public:
    Params params;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        for (auto& v : voices) v.active = false;
        const size_t m = (size_t) std::max (maxBlock, 16);
        clickBus.assign (m, 0); bodyBus.assign (m, 0); subBus.assign (m, 0); rumbleBus.assign (m, 0);
        rumble.prepare (sampleRate);
        warmth.prepare (sampleRate); dist.prepare (sampleRate); comp.prepare (sampleRate); eq.prepare (sampleRate); limiter.prepare (sampleRate);
    }

    double targetHz() const { return keyHz (params.key, params.octave, params.fine); }
    int latency() const { return limiter.latency(); }   // samples; the plugin reports this to the host

    // Any MIDI note fires the kick; its pitch is ignored (fixed key)
    void trigger (float velocity)
    {
        const float vg = params.velocity ? 0.2f + 0.8f * std::clamp (velocity, 0.0f, 1.0f) : 1.0f;
        // the kick still sounding fades out over 4 ms so the retrigger never clicks
        for (auto& v : voices)
            if (v.active && v.fadeStep <= 0.0f) v.fadeStep = 1.0f / (float) (0.004 * sr);
        Voice* free = nullptr;
        for (auto& v : voices) if (! v.active) { free = &v; break; }
        if (free == nullptr) free = &voices[0];
        seed = seed * 1664525u + 1013904223u;
        free->start (vg, seed);
        rumble.hit();
    }

    void allOff() { for (auto& v : voices) v.active = false; rumble.reset(); }
    bool sounding() const
    {
        for (auto& v : voices) if (v.active) return true;
        return params.rumbleMix > 0.0f && rumble.ringing();
    }

    // Mono kick into out[]. The layers stay on separate buses for stage 3
    // (Clean Sub bypassing the FX, the Rumble going through them).
    void process (float* out, int num, double bpm = 120.0)
    {
        std::fill (clickBus.begin(), clickBus.begin() + num, 0.0f);
        std::fill (bodyBus.begin(), bodyBus.begin() + num, 0.0f);
        std::fill (subBus.begin(), subBus.begin() + num, 0.0f);

        const Params& p = params;
        const double target = targetHz();
        const double sweep = p.sweepMs * 0.001, att = p.attackMs * 0.001, hold = p.holdMs * 0.001, dec = std::max (0.005, (double) p.decayMs * 0.001);
        const double sAtt = p.subAttackMs * 0.001, sDec = std::max (0.01, (double) p.subDecayMs * 0.001);
        const double subHz = target * (p.subOctave == 1 ? 0.5 : 1.0);
        const float clickLen = std::max (0.5f, p.clickDecayMs) * 0.001f;
        const float clickTau = clickLen / 4.6f;                        // -40 dB at the Decay time
        const long clickEnd = (long) (clickLen * 1.6f * sr);
        const long bodyEnd = (long) ((att + hold + dec) * sr), subEnd = (long) ((sAtt + sDec) * sr);
        const long voiceEnd = std::max (clickEnd, std::max (bodyEnd, subEnd)) + 2;
        const float gClick = dbToGain (p.clickLevelDb), gBody = dbToGain (p.bodyLevelDb), gSub = dbToGain (p.subLevelDb);

        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (p.source == noiseSrc) v.clickF.set (p.clickToneHz, (float) sr, 1.2f);
            else if (p.source == clickSrc) v.clickF.set (p.clickToneHz, (float) sr, 1.6f);   // wide band: a tick, not a beep
            else v.clickF.set (p.clickToneHz * 1.5f, (float) sr, 1.0f);
            v.clickHp.set (180.0f, (float) sr, 1.4f);   // the transient never adds low end (the body owns it)

            for (int i = 0; i < num; ++i)
            {
                const double t = (double) v.n / sr;
                float g = v.velGain;
                if (v.fadeStep > 0.0f) { v.fade -= v.fadeStep; if (v.fade <= 0.0f) { v.active = false; break; } g *= v.fade; }

                // Body: frequency = Key x 2^(amount x sweep / 12); the sweep ends at exactly 0, so the tail IS the Key
                const double e = Shapes::pitch (t, sweep, p.curve);
                const double f = target * std::pow (2.0, (double) p.pitchAmt * e / 12.0);
                v.bodyPh += f / sr; v.bodyPh -= std::floor (v.bodyPh);
                const double bodyEnv = Shapes::body (t, att, hold, dec);
                float w = (float) std::sin (twoPiD * v.bodyPh);
                if (p.wave == triWave) { const float ph = (float) v.bodyPh; w = ph < 0.25f ? 4 * ph : (ph < 0.75f ? 2 - 4 * ph : 4 * ph - 4); }
                else if (p.wave == softWave) w = std::tanh (2.5f * w) / std::tanh (2.5f);
                bodyBus[(size_t) i] += w * (float) bodyEnv * gBody * g;

                // Sub: pure sine locked to the Key (or an octave below), ducked under the body by Blend
                v.subPh += subHz / sr; v.subPh -= std::floor (v.subPh);
                const double subEnv = Shapes::sub (t, sAtt, sDec) * (1.0 - p.blend * bodyEnv);
                subBus[(size_t) i] += (float) (std::sin (twoPiD * v.subPh) * subEnv) * gSub * g;

                // Transient
                //  Noise: a short burst of filtered noise (air, "tss")
                //  Click: an impulse through a wide band pass at Tone plus a little noise - the sharp analog tick
                //  Pulse: a narrow pulse wave through a low pass (a woody "tok")
                if (v.n < clickEnd && gClick > 0.0f)
                {
                    const float env = std::exp (-(float) t / clickTau);
                    float c;
                    if (p.source == noiseSrc) c = v.clickF.lp (v.rng.next()) * 1.1f;
                    else if (p.source == clickSrc)
                    {
                        const float imp = v.n == 0 ? (float) (0.35 * sr / p.clickToneHz) : 0.0f;
                        c = v.clickF.bp (imp + 0.35f * v.rng.next());
                    }
                    else
                    {
                        v.pulsePh += p.clickToneHz / 6.0 / sr; v.pulsePh -= std::floor (v.pulsePh);
                        c = v.clickF.lp (v.pulsePh < 0.25 ? 1.0f : -0.33f) * 1.1f;
                    }
                    clickBus[(size_t) i] += v.clickHp.hp (c) * env * gClick * g;
                }

                if (++v.n > voiceEnd) { v.active = false; break; }
            }
        }

        // Rumble: always running (so turning Mix up mid-tail works), heard only when Mix is up
        rumble.setup (p, subHz, target, bpm);
        const float send = p.rumbleAmount * 1.5f, mix = p.rumbleMix * 2.0f;
        // fed by the sub and the body (like reverbing the whole kick); the transient stays out so the rumble doesn't hiss
        for (int i = 0; i < num; ++i) rumbleBus[(size_t) i] = rumble.tick ((subBus[(size_t) i] + 0.7f * bodyBus[(size_t) i]) * send) * mix;

        // FX chain. Clean Sub keeps the sub out of Warmth / Distortion / Compression, so the pure tuned
        // fundamental sits underneath however hard they're pushed. The Rumble goes through them.
        warmth.setup (p); dist.setup (p); comp.setup (p); eq.setup (p);
        const float gain = dbToGain (p.gainDb), ceil = dbToGain (p.ceilingDb);
        for (int i = 0; i < num; ++i)
        {
            const size_t k = (size_t) i;
            float x = clickBus[k] + bodyBus[k] + rumbleBus[k] + (p.cleanSub ? 0.0f : subBus[k]);
            x = comp.process (dist.process (warmth.process (x)));
            if (p.cleanSub) x += subBus[k];
            x = eq.process (x) * gain;
            out[i] = limiter.process (x, ceil);
        }
    }

    std::vector<float> clickBus, bodyBus, subBus, rumbleBus;
    Rumble rumble;
    Warmth warmth; Distortion dist; Compressor comp; OutputEq eq; Limiter limiter;

private:
    double sr = 44100;
    Voice voices[2];
    uint32_t seed = 12345;
};

} // namespace mazo
