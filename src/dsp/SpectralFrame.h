#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace synth
{

// Architectural scaling constants. See SpectralTimeline 中心架构.md sections 4 / 6 / 7.
constexpr int kMaxPartials = 500;      // N hard cap per StaticSpectralFrame.
constexpr int kMaxUnison = 16;         // U: unison spread parameter upper bound (§4 Unison).
constexpr int kMaxTimelineFrames = 32; // T: source-intrinsic frames per SpectralTimeline
constexpr int kMaxVoices = 16;         // V: polyphony budget (§7)
constexpr int kMaxModSlots = 8;        // Unified modulator slots (loop + one-shot, was kMaxLfos+kMaxModEnvs)
constexpr int kMaxLfos = 4;            // kept for backward compat (= first half of kMaxModSlots)
constexpr int kMaxModEnvs = 4;         // kept for backward compat (= second half of kMaxModSlots)
constexpr int kMaxAmpEnvs = 4;         // Shared ADSR envelopes referenced by source tracks
constexpr int kMaxMatrixRules = 32;    // routing rules (was 16; presets with rules ≥16 don't load in older builds)
constexpr int kControlBlockSize = 32;  // control-rate granularity (§6)

// Manual UI helper. It does not impose a realtime partial budget.
inline int maxPartialCountForRefHz(float refHz)
{
    (void)refHz;
    return kMaxPartials;
}

enum class FreqMode : uint8_t
{
    RelativeRatio = 0,  // nu_i = rho_i,  f_i = f0 * rho_i
    AbsoluteHz = 1      // nu_i = f_i (Hz),  f0 ignored (§0.1)
};

enum class PhaseInitMode : uint8_t
{
    Zero = 0,         // Phi_i^init = 0
    Random = 1,       // Phi_i^init ~ U(0, 2pi)
    Locked = 2,       // Phi_i^init = phi_i^table  (uses frame.phaseLocked[i])
    Alternating = 3   // Phi_i^init = pi * (i mod 2)
};

// P_i^src = (nu_i, a_i, x_i, mu_i) per architecture §0.
// Plus phase init / dynamic phase metadata. Audio-rate theta_i is still owned by Voice (§0.3).
struct StaticSpectralFrame
{
    int partialCount = 32;
    FreqMode freqMode = FreqMode::RelativeRatio;
    PhaseInitMode phaseInitMode = PhaseInitMode::Zero;
    uint32_t phaseSeed = 1u;

    // P_i^src components
    std::array<float, kMaxPartials> nu {};
    std::array<float, kMaxPartials> amp {};
    std::array<float, kMaxPartials> x {};
    std::array<uint8_t, kMaxPartials> mu {};   // 0=low, 1=mid, 2=high (group label)

    // Optional: locked phase table used when phaseInitMode == Locked.
    std::array<float, kMaxPartials> phaseLocked {};
    // Per-partial phase randomization ratio [0,1] (Serum-style meta osc phase Rand).
    std::array<float, kMaxPartials> phaseRandom {};
    // Optional dynamic phase seed, in Hz-equivalent phase drift relative to the nominal partial.
    std::array<float, kMaxPartials> phaseDriftHz {};
    std::array<float, kMaxPartials> phaseJitter {};

    float refAttackSec = 0.0f;
    float refDecaySec = 0.0f;
    float refReleaseSec = 0.0f;
};

inline void initFrameDefaults(StaticSpectralFrame &f)
{
    f.phaseDriftHz.fill(0.0f);
    f.phaseJitter.fill(0.0f);
    f.phaseRandom.fill(0.0f);
    f.refAttackSec = 0.0f;
    f.refDecaySec = 0.0f;
    f.refReleaseSec = 0.0f;
}

struct SpectralTimeline
{
    int frameCount = 1;
    float durationSeconds = 0.0f;
    bool loop = false;
    std::array<float, kMaxTimelineFrames> timeSeconds {};
    std::array<StaticSpectralFrame, kMaxTimelineFrames> frames {};
};

inline void initTimelineDefaults(SpectralTimeline &t)
{
    t.frameCount = 1;
    t.durationSeconds = 0.0f;
    t.loop = false;
    t.timeSeconds.fill(0.0f);
    for(auto &f : t.frames)
        initFrameDefaults(f);
}

inline SpectralTimeline makeStaticTimeline(const StaticSpectralFrame &frame)
{
    SpectralTimeline t;
    initTimelineDefaults(t);
    t.frames[0] = frame;
    return t;
}

inline float lerpFloat(float a, float b, float x)
{
    return a + (b - a) * x;
}

inline float lerpAngle(float a, float b, float x)
{
    constexpr float twoPi = 6.28318530717958647692f;
    constexpr float pi = 3.14159265358979323846f;
    float d = b - a;
    while(d > pi) d -= twoPi;
    while(d < -pi) d += twoPi;
    float y = a + d * x;
    while(y >= twoPi) y -= twoPi;
    while(y < 0.0f) y += twoPi;
    return y;
}

inline StaticSpectralFrame interpolateFrames(const StaticSpectralFrame &a,
                                             const StaticSpectralFrame &b,
                                             float x)
{
    StaticSpectralFrame out = a;
    initFrameDefaults(out);
    out.partialCount = std::clamp(std::max(a.partialCount, b.partialCount), 1, kMaxPartials);
    out.freqMode = a.freqMode;
    out.phaseInitMode = a.phaseInitMode;
    out.phaseSeed = a.phaseSeed;

    for(int i = 0; i < out.partialCount; ++i)
    {
        out.nu[i] = lerpFloat(a.nu[i], b.nu[i], x);
        out.amp[i] = lerpFloat(a.amp[i], b.amp[i], x);
        out.x[i] = lerpFloat(a.x[i], b.x[i], x);
        out.mu[i] = x < 0.5f ? a.mu[i] : b.mu[i];
        out.phaseLocked[i] = lerpAngle(a.phaseLocked[i], b.phaseLocked[i], x);
        out.phaseRandom[i] = lerpFloat(a.phaseRandom[i], b.phaseRandom[i], x);
        out.phaseDriftHz[i] = lerpFloat(a.phaseDriftHz[i], b.phaseDriftHz[i], x);
        out.phaseJitter[i] = lerpFloat(a.phaseJitter[i], b.phaseJitter[i], x);
    }
    out.refAttackSec = lerpFloat(a.refAttackSec, b.refAttackSec, x);
    out.refDecaySec = lerpFloat(a.refDecaySec, b.refDecaySec, x);
    out.refReleaseSec = lerpFloat(a.refReleaseSec, b.refReleaseSec, x);
    return out;
}

inline StaticSpectralFrame sampleTimeline(const SpectralTimeline &timeline, float sourceTimeSeconds)
{
    const int count = std::clamp(timeline.frameCount, 1, kMaxTimelineFrames);
    if(count <= 1 || timeline.durationSeconds <= 1e-6f)
        return timeline.frames[0];

    float t = std::max(0.0f, sourceTimeSeconds);
    if(timeline.loop)
        t = std::fmod(t, std::max(1e-6f, timeline.durationSeconds));
    else
        t = std::min(t, timeline.timeSeconds[(size_t)count - 1]);

    int hi = 1;
    while(hi < count && timeline.timeSeconds[(size_t)hi] < t)
        ++hi;
    if(hi >= count)
        return timeline.frames[(size_t)count - 1];

    const int lo = std::max(0, hi - 1);
    const float t0 = timeline.timeSeconds[(size_t)lo];
    const float t1 = timeline.timeSeconds[(size_t)hi];
    const float x = (t1 > t0 + 1e-6f) ? std::clamp((t - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;
    return interpolateFrames(timeline.frames[(size_t)lo], timeline.frames[(size_t)hi], x);
}

} // namespace synth
