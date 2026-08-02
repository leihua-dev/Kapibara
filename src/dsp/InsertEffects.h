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

// Disperser-style allpass cascade. The AP algos used to be a cascade of at most
// four IDENTICAL sections, which is a plain phase shift; smearing a transient
// into a descending "pew" needs many sections AND a different frequency per
// section, so the group delay varies across the spectrum.
constexpr int kMaxDisperserStages = 32;

struct FilterSlotParams
{
    bool enabled = false;
    InsertFilterAlgo algo = InsertFilterAlgo::LP2;
    float cutoffHz = 1000.0f;
    float resonance = 0.707f;
    float gainDb = 0.0f;   // retained for Peak/Shelf coefficient math (not user-exposed)
    float drive = 1.0f;    // 1..16 input saturation into the filter
    float mix = 1.0f;
    // Allpass-only. Appending these is safe for old presets: the hex blob reader
    // stops at the end of the stored string and leaves the tail at its defaults,
    // and apStages = 0 keeps the historic AP2/AP4/AP8 section counts exactly.
    uint8_t apStages = 0;    // 0 = derive from the algo (AP2/AP4/AP8 -> 1/2/4)
    float apSpread = 0.0f;   // -1..+1: section frequency spread, in octaves
    float apQSpread = 0.0f;  // -1..+1: section Q spread
    float apCurve = 0.0f;    // -1..+1: bend of the progression across sections
    // Hand-drawn distribution, one entry per section. The three macros above are
    // generators: they decide the shape until a section is dragged, at which
    // point these arrays become the truth and the macros are no longer read.
    // apCustom is what tells the two apart, so an old preset (whose blob tail
    // decodes to zero) still plays exactly the macro shape it was saved with,
    // and the arrays' zero-init never has to mean anything.
    uint8_t apCustom = 0;
    std::array<float, kMaxDisperserStages> apOct {};   // octaves off the cutoff
    std::array<float, kMaxDisperserStages> apQMul {};  // x the resonance knob
    // Serial stacks only shape phase without saturating; magnitude types need
    // their sections side by side instead — parallel bandpasses are a formant
    // bank, parallel in series is silence. Zero-init is the historic serial
    // chain at unity, so an old preset's blob tail still means exactly nothing.
    uint8_t apParallel = 0;                             // 0 = chain, 1 = summed
    std::array<float, kMaxDisperserStages> apGainDb {}; // per-band trim, parallel only
    // Per-slot voicing: the cascade stops being N copies of one filter and
    // becomes a chain of N configurable ones. Every field uses 0 to mean "as
    // before", so a zero-init tail — which is what an old preset's blob decodes
    // to — is byte-for-byte the behaviour these arrays replace.
    std::array<uint8_t, kMaxDisperserStages> apAlgo {};   // 0 = inherit fs.algo, else algo+1
    std::array<uint8_t, kMaxDisperserStages> apDist {};   // 0 = clean, else InsertDistAlgo+1
    std::array<uint8_t, kMaxDisperserStages> apDrive {};  // 0..255 -> drive 1..16
    std::array<uint8_t, kMaxDisperserStages> apFb {};     // 0..255 -> feedback 0..0.99
};

// Unit-domain bend, same family as the modulation curves: c>0 pushes the
// progression late, c<0 early, 0 is linear.
inline float apBend01(float x, float c)
{
    x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
    if(std::abs(c) < 1.0e-4f)
        return x;
    return c >= 0.0f ? std::pow(x, 1.0f + c * 4.0f)
                     : 1.0f - std::pow(1.0f - x, 1.0f - c * 4.0f);
}

constexpr float kDisperserMaxOct = 4.0f;
constexpr float kDisperserMinQMul = 0.1f;
constexpr float kDisperserMaxQMul = 8.0f;

// The cascade depth an algo ships with when apStages says nothing. This is also
// the line the resonance guard below keys off: at or under it the filter is
// exactly what it always was.
inline int naturalSections(InsertFilterAlgo a)
{
    switch(a)
    {
        case InsertFilterAlgo::AP4:
        case InsertFilterAlgo::LP4:
        case InsertFilterAlgo::HP4: return 2;
        case InsertFilterAlgo::AP8: return 4;
        default: return 1;
    }
}

inline int disperserSections(const FilterSlotParams &fs)
{
    if(fs.apStages != 0)
        return fs.apStages < kMaxDisperserStages ? int(fs.apStages) : kMaxDisperserStages;
    return naturalSections(fs.algo);
}

inline bool isAllpassAlgo(InsertFilterAlgo a)
{
    return a == InsertFilterAlgo::AP2 || a == InsertFilterAlgo::AP4 || a == InsertFilterAlgo::AP8;
}

// Which algos a SERIAL stack is musical for, i.e. which ones get the cascade
// controls. Phase shaping does not saturate — every allpass section adds group
// delay that is still audible at 32 — and serial notches spread apart are a real
// comb. Magnitude shaping does saturate: measured on a lowpass, going from 4 to
// 32 identical sections moves the stopband from -49 dB to -779 dB, which is the
// same silence twice, while the corner quietly slides down. Worse, serial
// bandpasses cancel to nothing (32 sections with spread measured rms 0.0000).
// Those types need PARALLEL sections, not more of them in a row.
inline bool isCascadeAlgo(InsertFilterAlgo a)
{
    return isAllpassAlgo(a) || a == InsertFilterAlgo::Notch;
}

// Identical RESONANT sections multiply their peaks: N of them at Q each reach
// roughly Q^N at the cutoff, so 32 stages of a Q=10 lowpass is an explosion, not
// a filter. An allpass is immune — flat magnitude, nothing to compound — and so
// is any depth an algo already shipped with. The guard therefore engages only
// ABOVE naturalSections(), territory no saved preset can be in, which is what
// keeps every existing patch bit-identical while making deep cascades safe.
inline float disperserMaxSectionQ(const FilterSlotParams &fs, InsertFilterAlgo algo, int sections)
{
    // Parallel sections sum, they do not chain, so nothing compounds — and a
    // formant bank is exactly a row of narrow, high-Q bands. Only a serial stack
    // of a magnitude type needs holding back.
    if(fs.apParallel != 0 || isAllpassAlgo(algo) || sections <= naturalSections(algo))
        return 10.0f;   // the historic clamp
    // Hold the whole cascade's resonant peak near +12 dB however deep it goes.
    // A 2-pole section peaks at Q / sqrt(1 - 1/(4Q^2)), not at Q — treating the
    // two as equal is only true for large Q and badly under-clamps a deep stack
    // (32 sections came out +38 dB). Invert that expression for this section's
    // share of the target: with G^2 = 4Q^4/(4Q^2-1), Q^2 = (G^2 + G*sqrt(G^2-1))/2.
    constexpr float kCascadePeak = 4.0f;   // +12 dB
    const float g = std::pow(kCascadePeak, 1.0f / float(sections < 1 ? 1 : sections));
    if(g <= 1.0f)
        return 0.70710678f;   // no peak at all below Butterworth Q
    const float q = std::sqrt((g * g + g * std::sqrt(g * g - 1.0f)) * 0.5f);
    return q > 10.0f ? 10.0f : q;
}

// Where one section sits: octaves off the cutoff, and its Q as a multiple of
// the resonance knob. The audio thread, the group-delay graph and the stage
// editor all go through here, so the drawn distribution is the played one.
inline void disperserStage(const FilterSlotParams &fs, int sections, int k,
                           float &octOut, float &qMulOut)
{
    if(fs.apCustom != 0)
    {
        const int i = k < 0 ? 0 : (k >= kMaxDisperserStages ? kMaxDisperserStages - 1 : k);
        octOut  = fs.apOct[(size_t)i];
        qMulOut = fs.apQMul[(size_t)i];
    }
    else if(sections <= 1)
    {
        // A lone section has no distribution to be part of, so SPREAD / PINCH /
        // CURVE have nothing to act on and it sits exactly on the filter's own
        // cutoff and Q. The generic formula put it at t = 0, the BOTTOM of the
        // spread range — with SPREAD at 1 a "1000 Hz" lowpass actually filtered
        // at 250 Hz, and PINCH scaled its Q on top. The knob said one thing and
        // the filter did another, which reads as the cutoff being broken.
        octOut  = 0.0f;
        qMulOut = 1.0f;
    }
    else
    {
        const float t  = float(k) / float(sections - 1);
        const float xb = apBend01(t, fs.apCurve) - 0.5f;
        octOut  = fs.apSpread * 4.0f * xb;
        qMulOut = 1.0f + fs.apQSpread * 1.5f * xb;
    }
    octOut  = octOut < -kDisperserMaxOct ? -kDisperserMaxOct
                                         : (octOut > kDisperserMaxOct ? kDisperserMaxOct : octOut);
    qMulOut = qMulOut < kDisperserMinQMul ? kDisperserMinQMul
                                          : (qMulOut > kDisperserMaxQMul ? kDisperserMaxQMul : qMulOut);
}

inline int disperserSlotIndex(int k)
{
    return k < 0 ? 0 : (k >= kMaxDisperserStages ? kMaxDisperserStages - 1 : k);
}

// What this slot IS. 0 means "whatever the filter is set to", which is how a
// preset written before per-slot voicing keeps behaving like one filter.
inline InsertFilterAlgo disperserSlotAlgo(const FilterSlotParams &fs, int k)
{
    const uint8_t v = fs.apAlgo[(size_t)disperserSlotIndex(k)];
    if(v == 0 || v > uint8_t(InsertFilterAlgo::HP4) + 1)
        return fs.algo;
    return InsertFilterAlgo(v - 1);
}

// Feedback around THIS slot alone. Capped below 1: the loop gain of a single
// section is what this multiplies, and at 1 it stops being a filter.
inline float disperserSlotFeedback(const FilterSlotParams &fs, int k)
{
    return float(fs.apFb[(size_t)disperserSlotIndex(k)]) * (0.99f / 255.0f);
}

inline bool disperserSlotDistOn(const FilterSlotParams &fs, int k)
{
    return fs.apDist[(size_t)disperserSlotIndex(k)] != 0;
}

inline InsertDistAlgo disperserSlotDist(const FilterSlotParams &fs, int k)
{
    const uint8_t v = fs.apDist[(size_t)disperserSlotIndex(k)];
    const uint8_t last = uint8_t(InsertDistAlgo::Tanh) + 1;
    return InsertDistAlgo(v == 0 || v > last ? 0 : v - 1);
}

inline float disperserSlotDrive(const FilterSlotParams &fs, int k)
{
    return 1.0f + float(fs.apDrive[(size_t)disperserSlotIndex(k)]) * (15.0f / 255.0f);
}

// Has anything been voiced per slot? Once it has, the cascade controls are worth
// showing whatever the base algo is: STAGES stops meaning "N copies of one
// filter" and starts meaning "how much of my chain is live".
inline bool anySlotVoiced(const FilterSlotParams &fs)
{
    for(int k = 0; k < kMaxDisperserStages; ++k)
        if(fs.apAlgo[(size_t)k] != 0 || fs.apDist[(size_t)k] != 0 || fs.apFb[(size_t)k] != 0)
            return true;
    return false;
}

// One section's realised slot: the filter's cutoff/resonance moved to where that
// section sits in the distribution. Audio thread, response graph and stage editor
// all go through here, so the drawn cascade is the played one.
inline FilterSlotParams disperserSectionSlot(const FilterSlotParams &fs, int sections, int k,
                                             double sampleRate)
{
    float oct = 0.0f, qMul = 1.0f;
    disperserStage(fs, sections, k, oct, qMul);
    const float nyq = float((sampleRate > 1.0 ? sampleRate : 48000.0) * 0.45);
    const InsertFilterAlgo algo = disperserSlotAlgo(fs, k);
    const float maxQ = disperserMaxSectionQ(fs, algo, sections);
    FilterSlotParams sec = fs;
    sec.algo = algo;
    sec.cutoffHz = fs.cutoffHz * std::pow(2.0f, oct);
    sec.cutoffHz = sec.cutoffHz < 20.0f ? 20.0f : (sec.cutoffHz > nyq ? nyq : sec.cutoffHz);
    sec.resonance = fs.resonance * qMul;
    sec.resonance = sec.resonance < 0.05f ? 0.05f : (sec.resonance > maxQ ? maxQ : sec.resonance);
    return sec;
}

// A band's level in the parallel sum. Serial sections have no such thing — a
// gain in a chain is just a level trim that N sections would compound — so it
// only applies when the sections are side by side.
inline float disperserSectionGain(const FilterSlotParams &fs, int k)
{
    if(fs.apParallel == 0)
        return 1.0f;
    const int i = k < 0 ? 0 : (k >= kMaxDisperserStages ? kMaxDisperserStages - 1 : k);
    return std::pow(10.0f, fs.apGainDb[(size_t)i] / 20.0f);
}

// Summing N bands needs a headroom rule. 1/sqrt(N) is the incoherent-sum answer:
// bands spread apart stay near unity, and the worst case — every section landing
// on the same frequency, where they add coherently — tops out at sqrt(N) instead
// of N. At one section it is exactly 1, so serial and parallel agree there.
inline float disperserParallelNorm(int sections)
{
    return sections > 1 ? 1.0f / std::sqrt(float(sections)) : 1.0f;
}

// Freeze the macro shape into the arrays so it can be hand-edited from exactly
// where it sounded. Sections the count doesn't reach yet continue the ramp
// linearly, so raising STAGES extends the sweep instead of piling new sections
// on top of the last one.
inline void disperserMaterialize(FilterSlotParams &fs)
{
    if(fs.apCustom != 0)
        return;
    const int n = disperserSections(fs);
    float lastO = 0.0f, lastQ = 1.0f, stepO = 0.0f, stepQ = 0.0f;
    for(int k = 0; k < n; ++k)
    {
        float o, q;
        disperserStage(fs, n, k, o, q);
        if(k == n - 1 && n > 1)
        {
            float po, pq;
            disperserStage(fs, n, k - 1, po, pq);
            stepO = o - po;
            stepQ = q - pq;
        }
        fs.apOct[(size_t)k]  = o;
        fs.apQMul[(size_t)k] = q;
        lastO = o;
        lastQ = q;
    }
    for(int k = n; k < kMaxDisperserStages; ++k)
    {
        lastO += stepO;
        lastQ += stepQ;
        fs.apOct[(size_t)k]  = lastO < -kDisperserMaxOct ? -kDisperserMaxOct
                                                         : (lastO > kDisperserMaxOct ? kDisperserMaxOct : lastO);
        fs.apQMul[(size_t)k] = lastQ < kDisperserMinQMul ? kDisperserMinQMul
                                                         : (lastQ > kDisperserMaxQMul ? kDisperserMaxQMul : lastQ);
    }
    fs.apCustom = 1;
}

// Group delay of the whole cascade, in ms. An allpass has flat magnitude, so
// this curve IS the effect: it is what the editor's graph plots.
inline float disperserGroupDelayMs(const FilterSlotParams &fs, float freqHz, double sampleRate)
{
    const int sections = disperserSections(fs);
    const float w = 6.28318530717958647692f * freqHz / float(sampleRate);
    const float nyq = float(sampleRate * 0.45);
    float total = 0.0f;
    for(int k = 0; k < sections; ++k)
    {
        float oct = 0.0f, qMul = 1.0f;
        disperserStage(fs, sections, k, oct, qMul);
        const float f = std::max(20.0f, std::min(nyq, fs.cutoffHz * std::pow(2.0f, oct)));
        const float q = std::max(0.05f, std::min(10.0f, fs.resonance * qMul));
        const float w0 = 6.28318530717958647692f * f / float(sampleRate);
        const float alpha = std::sin(w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        const float a1 = (-2.0f * std::cos(w0)) / a0;
        const float a2 = (1.0f - alpha) / a0;
        // Poles r*e^(+-j0). An allpass' group delay is the sum of the Poisson
        // kernel over its poles — exact, and far steadier than differencing a
        // wrapped phase response.
        const float r = std::sqrt(std::max(0.0f, a2));
        if(r < 1.0e-5f || r >= 1.0f)
            continue;
        const float ct = std::max(-1.0f, std::min(1.0f, -a1 / (2.0f * r)));
        const float th = std::acos(ct);
        const float num = 1.0f - r * r;
        const float d1 = 1.0f - 2.0f * r * std::cos(w - th) + r * r;
        const float d2 = 1.0f - 2.0f * r * std::cos(w + th) + r * r;
        total += num / std::max(1.0e-6f, d1) + num / std::max(1.0e-6f, d2);
    }
    return total / float(sampleRate) * 1000.0f;
}

// Section frequency in Hz, clamped away from DC and Nyquist.
inline float disperserStageHz(const FilterSlotParams &fs, int sections, int k, double sampleRate)
{
    float oct = 0.0f, qm = 1.0f;
    disperserStage(fs, sections, k, oct, qm);
    const float nyq = float(sampleRate * 0.45);
    const float f = fs.cutoffHz * std::pow(2.0f, oct);
    return f < 20.0f ? 20.0f : (f > nyq ? nyq : f);
}

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
            // Closed-form triangle fold. The old loop gave up after six
            // reflections and returned whatever was left, so a hot input escaped
            // the ±1 range entirely: drive 16 on a unit signal came out at 4.
            // One shaper only sounded a bit loud, but sixteen of them in a slot
            // chain compounded to 2e14. Wrapping analytically has no iteration
            // count to run out of, and agrees with the loop everywhere the loop
            // actually converged.
            const float t = xb + 1.0f;
            const float w = t - 4.0f * std::floor(t * 0.25f);   // [0, 4)
            return w < 2.0f ? w - 1.0f : 3.0f - w;
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
    if(isAllpassAlgo(fs.algo) && a0 != 0.0f)
    {
        // Divide instead of multiplying by the reciprocal, and write b2's exact
        // value (an allpass numerator is its denominator reversed, so it
        // normalises to precisely 1). This is the form the hand-written allpass
        // cascade used before the two filter paths merged. The reciprocal form
        // lands a ulp off on b0/a2, and an allpass pole sits close enough to the
        // unit circle that the recursion walks it up to ~-78 dB at a 40 Hz
        // cutoff — inaudible, but there is no reason to move a saved patch at all.
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = 1.0f;
        c.a1 = a1 / a0; c.a2 = a2 / a0;
    }
    else
    {
        const float inv = a0 != 0.0f ? 1.0f / a0 : 1.0f;
        c.b0 = b0 * inv; c.b1 = b1 * inv; c.b2 = b2 * inv;
        c.a1 = a1 * inv; c.a2 = a2 * inv;
    }
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
