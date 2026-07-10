#pragma once

// Shared model + DSP for strip-grid insert effect chains.
// A small global bank of filters (F1..F8) and distortions (D1..D8) is referenced
// by route inserts on each source track. The UI and the audio engine share these
// definitions so the displayed curve matches what is heard.

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace synth
{

// Precomputed impulse-response for the convolution reverb: the IR split into
// uniform partitions, each FFT'd to the frequency domain. Immutable + shared by
// pointer so it can be copied into render snapshots cheaply (audio-thread safe).
struct ConvIR
{
    int hop = 0;      // partition / hop size (power of two)
    int fftN = 0;     // 2 * hop
    int parts = 0;    // number of partitions
    std::vector<float> re; // parts * fftN, partition p spectrum real at p*fftN
    std::vector<float> im;
    std::string name;
};

constexpr int kInsertBankSize = 8;   // F1..F8 and D1..D8
constexpr int kMaxRouteInserts = 4;  // retained compatibility limit for route UI summaries
constexpr int kMaxRenderInserts = kMaxRouteInserts * 2;

// Order MUST match the UI's FilterAlgo enum.
enum class InsertFilterAlgo : uint8_t
{
    LP2 = 0, HP2, BP2, Notch, AP2, AP4, AP8, Peak, LowShelf, HighShelf, LP4, HP4
};

// Order MUST match the UI's DistAlgo enum.
enum class InsertDistAlgo : uint8_t
{
    SoftClip = 0, HardClip, Tube, Diode, FoldBack, SineFold, BitCrush, Tanh
};

struct FilterSlotParams
{
    bool enabled = false;
    InsertFilterAlgo algo = InsertFilterAlgo::LP2;
    float cutoffHz = 1000.0f;
    float resonance = 0.707f;
    float gainDb = 0.0f;   // retained for Peak/Shelf coefficient math (not user-exposed)
    float drive = 1.0f;    // 1..16 input saturation into the filter
    float mix = 1.0f;
};

struct DistSlotParams
{
    bool enabled = false;
    InsertDistAlgo algo = InsertDistAlgo::SoftClip;
    float drive = 2.0f;   // 1..32
    float bias = 0.0f;    // -1..1 asymmetry
    float mix = 1.0f;     // 0..1
    float outGain = 1.0f; // 0..2
};

// A single insert references a bank slot.
// kind: 0 empty, 1 filter, 2 distortion, 3 EQ, 4 compressor, 5 delay, 6 reverb
struct RouteInsert
{
    uint8_t kind = 0;
    uint8_t index = 0;   // bank slot 0..7
    bool bypass = false; // user can bypass without removing
};
enum InsertKind { InsertEmpty = 0, InsertFilter, InsertDist, InsertEq, InsertComp, InsertDelay, InsertReverb, InsertConvReverb, InsertMultiband };
constexpr int kInsertParamSlots = 8; // matrix-mod params reserved per insert

// ---- Extra effect banks (EQ / Compressor / Delay / Reverb), 8 slots each ----
struct EqSlotParams
{
    float lowDb = 0.0f;   // low shelf @ ~200 Hz
    float midDb = 0.0f;   // peak
    float highDb = 0.0f;  // high shelf @ ~4 kHz
    float midHz = 1000.0f;
};
struct CompSlotParams
{
    float threshDb = -18.0f;
    float ratio = 4.0f;       // 1..20
    float attackMs = 10.0f;
    float makeupDb = 0.0f;
};
struct DelaySlotParams
{
    float timeMs = 250.0f;    // up to ~170ms usable (per-voice buffer)
    float feedback = 0.35f;   // 0..0.95
    float mix = 0.3f;
    float tone = 0.5f;        // 0=dark 1=bright (feedback LP)
    bool  pingpong = false;   // cross-feed L/R taps for ping-pong echoes
};
struct ReverbSlotParams
{
    float size = 0.5f;        // 0..1
    float decay = 0.6f;       // 0..0.95 feedback
    float mix = 0.25f;
    float damp = 0.5f;        // HF damping
};
struct ConvSlotParams
{
    std::shared_ptr<const ConvIR> ir; // loaded impulse response (null = passthrough)
    float mix = 0.35f;        // dry/wet
    float gain = 1.0f;        // wet trim 0..2
    float predelayMs = 0.0f;  // 0..200
    std::string irName;       // for UI display + reload
};

struct MultibandSlotParams;

// One effect in a strip-grid chain — self-contained (no shared bank, no slot number).
// Only the active kind's params are used. Chains are unbounded std::vectors.
struct InsertEffect
{
    uint8_t kind = 0;      // see InsertKind (0 = empty)
    bool bypass = false;
    FilterSlotParams filter {};
    DistSlotParams dist {};
    EqSlotParams eq {};
    CompSlotParams comp {};
    DelaySlotParams delay {};
    ReverbSlotParams reverb {};
    ConvSlotParams conv {};
    std::shared_ptr<MultibandSlotParams> multiband {};
};

struct MultibandSlotParams
{
    float lowXoverHz = 250.0f;
    float highXoverHz = 2500.0f;
    bool bandMute[3] { false, false, false };
    bool bandSolo[3] { false, false, false };
    std::vector<InsertEffect> bands[3];
};

// ---- Source-as-modulator (cross-track) modulation ----
enum class SourceModType : uint8_t
{
    AM = 0,       // amplitude modulation: c * (1 + depth*m)
    RingMod,      // c * (depth*m + (1-depth))  ... depth=1 → pure ring
    FM,           // linear FM (approximated post-render)
    PM,           // phase modulation (approximated post-render)
    HardSync      // hard sync flavour (approximated)
};
constexpr int kSourceModTypeCount = 5;
constexpr int kMaxTrackMods = 3;

struct SourceModEntry
{
    bool enabled = false;
    int8_t sourceTrack = -1;          // modulator track index (-1 = none)
    SourceModType type = SourceModType::AM;
    float depth = 0.0f;               // 0..1
    // Component tap: 0 = the modulator track's own (gain-pre) output; 1 = a
    // per-voice filter node; 2 = an amp-env node. sourceNode = node slot /
    // instance. For taps > 0, sourceTrack still holds the node's home (feeder)
    // track so ordering, cycle checks, and UI fallbacks keep working.
    uint8_t sourceKind = 0;
    uint8_t sourceNode = 0;
};

inline const char *sourceModTypeName(SourceModType t)
{
    switch(t)
    {
        case SourceModType::AM: return "AM";
        case SourceModType::RingMod: return "Ring";
        case SourceModType::FM: return "FM";
        case SourceModType::PM: return "PM";
        case SourceModType::HardSync: return "Sync";
    }
    return "AM";
}

// ---- Distortion transfer function (shared by UI curve and audio render) ----
inline float distShape(InsertDistAlgo algo, float x, float drive, float bias)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float xb = x * drive + bias;
    switch(algo)
    {
        case InsertDistAlgo::SoftClip: return xb / (1.0f + std::fabs(xb));
        case InsertDistAlgo::HardClip: return std::max(-1.0f, std::min(1.0f, xb));
        case InsertDistAlgo::Tube:     return xb >= 0.0f ? std::tanh(xb) : std::tanh(xb * 0.7f) * 0.85f;
        case InsertDistAlgo::Diode:    return std::tanh(xb >= 0.0f ? xb : xb * 0.15f);
        case InsertDistAlgo::FoldBack:
        {
            float v = xb;
            for(int i = 0; i < 6 && (v > 1.0f || v < -1.0f); ++i)
            {
                if(v > 1.0f)  v = 2.0f - v;
                if(v < -1.0f) v = -2.0f - v;
            }
            return v;
        }
        case InsertDistAlgo::SineFold: return std::sin(xb * kPi * 0.5f);
        case InsertDistAlgo::BitCrush:
        {
            const float dr = std::max(1.0f, std::min(32.0f, drive));
            const int steps = std::max(2, int(2.0f + (32.0f - dr)));
            const float q = std::max(-1.0f, std::min(1.0f, x + bias * 0.5f));
            return std::max(-1.0f, std::min(1.0f, std::round(q * float(steps)) / float(steps)));
        }
        case InsertDistAlgo::Tanh: return std::tanh(xb);
    }
    return x;
}

// ---- Biquad coefficients (RBJ cookbook), normalized by a0 ----
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    int stages = 1; // cascade count (LP4/HP4/AP4 = 2, AP8 = 4)
};

inline BiquadCoeffs designInsertBiquad(const FilterSlotParams &fs, double sampleRate)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float sr = float(sampleRate > 1.0 ? sampleRate : 48000.0);
    const float cutoff = std::max(20.0f, std::min(fs.cutoffHz, sr * 0.45f));
    const float w0 = 2.0f * kPi * cutoff / sr;
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float Q = std::max(0.05f, fs.resonance);
    const float alpha = sinw0 / (2.0f * Q);
    const float A = std::pow(10.0f, fs.gainDb / 40.0f);
    float b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    int stages = 1;
    switch(fs.algo)
    {
        case InsertFilterAlgo::LP2:
            b0 = (1 - cosw0) * 0.5f; b1 = 1 - cosw0; b2 = (1 - cosw0) * 0.5f;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha; break;
        case InsertFilterAlgo::HP2:
            b0 = (1 + cosw0) * 0.5f; b1 = -(1 + cosw0); b2 = (1 + cosw0) * 0.5f;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha; break;
        case InsertFilterAlgo::BP2:
            b0 = sinw0 * 0.5f; b1 = 0; b2 = -sinw0 * 0.5f;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha; break;
        case InsertFilterAlgo::Notch:
            b0 = 1; b1 = -2 * cosw0; b2 = 1;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha; break;
        case InsertFilterAlgo::AP2:
        case InsertFilterAlgo::AP4:
        case InsertFilterAlgo::AP8:
            b0 = 1 - alpha; b1 = -2 * cosw0; b2 = 1 + alpha;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha;
            stages = fs.algo == InsertFilterAlgo::AP2 ? 1 : (fs.algo == InsertFilterAlgo::AP4 ? 2 : 4);
            break;
        case InsertFilterAlgo::Peak:
            b0 = 1 + alpha * A; b1 = -2 * cosw0; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cosw0; a2 = 1 - alpha / A; break;
        case InsertFilterAlgo::LowShelf:
        {
            const float s2a = 2.0f * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cosw0 + s2a); b1 = 2 * A * ((A - 1) - (A + 1) * cosw0); b2 = A * ((A + 1) - (A - 1) * cosw0 - s2a);
            a0 = (A + 1) + (A - 1) * cosw0 + s2a; a1 = -2 * ((A - 1) + (A + 1) * cosw0); a2 = (A + 1) + (A - 1) * cosw0 - s2a; break;
        }
        case InsertFilterAlgo::HighShelf:
        {
            const float s2a = 2.0f * std::sqrt(A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cosw0 + s2a); b1 = -2 * A * ((A - 1) + (A + 1) * cosw0); b2 = A * ((A + 1) + (A - 1) * cosw0 - s2a);
            a0 = (A + 1) - (A - 1) * cosw0 + s2a; a1 = 2 * ((A - 1) - (A + 1) * cosw0); a2 = (A + 1) - (A - 1) * cosw0 - s2a; break;
        }
        case InsertFilterAlgo::LP4:
            b0 = (1 - cosw0) * 0.5f; b1 = 1 - cosw0; b2 = (1 - cosw0) * 0.5f;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha; stages = 2; break;
        case InsertFilterAlgo::HP4:
            b0 = (1 + cosw0) * 0.5f; b1 = -(1 + cosw0); b2 = (1 + cosw0) * 0.5f;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha; stages = 2; break;
    }
    BiquadCoeffs c;
    const float inv = a0 != 0.0f ? 1.0f / a0 : 1.0f;
    c.b0 = b0 * inv; c.b1 = b1 * inv; c.b2 = b2 * inv;
    c.a1 = a1 * inv; c.a2 = a2 * inv;
    c.stages = stages;
    return c;
}

// Per-channel transposed-direct-form-II biquad state, up to 4 cascade stages.
struct BiquadState
{
    std::array<float, 4> z1 {};
    std::array<float, 4> z2 {};
    void reset() { z1.fill(0.0f); z2.fill(0.0f); }
    float process(const BiquadCoeffs &c, float x)
    {
        float v = x;
        for(int s = 0; s < c.stages && s < 4; ++s)
        {
            const float y = c.b0 * v + z1[(size_t)s];
            z1[(size_t)s] = c.b1 * v - c.a1 * y + z2[(size_t)s];
            z2[(size_t)s] = c.b2 * v - c.a2 * y;
            v = y;
        }
        return v;
    }
};

// EQ: 3 independent biquads (low shelf, mid peak, high shelf).
inline void designEqBiquads(const EqSlotParams &eq, double sampleRate, BiquadCoeffs out[3])
{
    FilterSlotParams f;
    f.algo = InsertFilterAlgo::LowShelf;  f.cutoffHz = 200.0f;  f.gainDb = eq.lowDb;  f.resonance = 0.707f;
    out[0] = designInsertBiquad(f, sampleRate);
    f.algo = InsertFilterAlgo::Peak;      f.cutoffHz = std::max(80.0f, eq.midHz); f.gainDb = eq.midDb; f.resonance = 0.9f;
    out[1] = designInsertBiquad(f, sampleRate);
    f.algo = InsertFilterAlgo::HighShelf; f.cutoffHz = 4000.0f; f.gainDb = eq.highDb; f.resonance = 0.707f;
    out[2] = designInsertBiquad(f, sampleRate);
}

} // namespace synth
