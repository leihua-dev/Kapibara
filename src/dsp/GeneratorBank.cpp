#include "Generators.h"

#include <algorithm>
#include <cmath>

namespace synth
{

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float wrapPhaseRadians(float phase)
{
    constexpr float twoPi = kTwoPi;
    while(phase > 3.14159265358979323846f) phase -= twoPi;
    while(phase < -3.14159265358979323846f) phase += twoPi;
    return phase;
}

inline float interpolatePhaseRadians(float a, float b, float t)
{
    return wrapPhaseRadians(a + wrapPhaseRadians(b - a) * t);
}

inline float computeNuDirect(FreqShape shape, float n, float inharm)
{
    switch(shape)
    {
        case FreqShape::Harmonic:    return std::pow(n, 1.0f + 0.5f * inharm);
        case FreqShape::Linear:      return 1.0f + (n - 1.0f) * (1.0f + 0.3f * inharm);
        case FreqShape::Exponential: return n * std::exp(0.03f * inharm * (n - 1.0f));
    }
    return n;
}

void zeroTrailing(StaticSpectralFrame &f, int n)
{
    for(int i = n; i < kMaxPartials; ++i)
    {
        f.nu[(size_t)i] = 0.0f;
        f.amp[(size_t)i] = 0.0f;
        f.x[(size_t)i] = 0.0f;
        f.mu[(size_t)i] = 0u;
        f.phaseLocked[(size_t)i] = 0.0f;
        f.phaseRandom[(size_t)i] = 0.0f;
        f.phaseDriftHz[(size_t)i] = 0.0f;
        f.phaseJitter[(size_t)i] = 0.0f;
    }
}
} // namespace

void GeneratorBank::generate(const SourceGenParams &p, StaticSpectralFrame &out) const
{
    initFrameDefaults(out);
    out.partialCount = std::clamp(p.wavetableSeed.partialCount, 1, kMaxWavetablePartials);
    out.freqMode = FreqMode::RelativeRatio;
    out.phaseInitMode = PhaseInitMode::Locked;
    out.phaseSeed = 1u;
    const int bankFrameCount = std::clamp(p.wavetableSeed.frameCount, 1, kMaxWavetableFrames);
    const float bankFramePos = clampf(p.wavetableSeed.morph, 0.0f, 1.0f) * float(std::max(0, bankFrameCount - 1));
    const int bankFrameA = std::clamp(int(bankFramePos), 0, bankFrameCount - 1);
    const int bankFrameB = std::min(bankFrameA + 1, bankFrameCount - 1);
    const float bankFrameFrac = bankFramePos - float(bankFrameA);
    const auto &bankFrames = p.wavetableSeed.frames.get();
    const auto *bankA = bankFrames[(size_t)bankFrameA].get();
    const auto *bankB = bankFrames[(size_t)bankFrameB].get();
    for(int i = 0; i < out.partialCount; ++i)
    {
        const int metaSlot = metaSlotForSourcePartial(p.sourceCount, i);
        const auto &slot = p.wavetableSeed.partials[(size_t)(metaSlot >= 0 ? metaSlot : i)];
        const float x = out.partialCount > 1 ? float(i) / float(out.partialCount - 1) : 0.0f;
        const float n = float(i + 1);
        const float shapedNu = computeNuDirect(p.wavetableSeed.freqShape, n, p.wavetableSeed.inharmonicAmount);
        const float freqMul = shapedNu / std::max(1.0e-6f, n);
        out.nu[(size_t)i] = std::max(0.0f, slot.ratio * freqMul);
        float amp = slot.amp;
        float phase = slot.phase;
        if(metaSlot < 0 && bankA != nullptr)
        {
            const auto &ha = bankA->harmonics[(size_t)i];
            const auto &hb = bankB != nullptr ? bankB->harmonics[(size_t)i] : ha;
            amp = ha.amp + (hb.amp - ha.amp) * bankFrameFrac;
            phase = interpolatePhaseRadians(ha.phase, hb.phase, bankFrameFrac);
        }
        out.amp[(size_t)i] = slot.enabled ? std::max(0.0f, amp) : 0.0f;
        out.x[(size_t)i] = x;
        out.mu[(size_t)i] = (x < 0.34f) ? 0u : ((x < 0.67f) ? 1u : 2u);
        out.phaseLocked[(size_t)i] = phase;
        // Phase Rand only applies to meta oscillators; additive bank partials stay locked.
        out.phaseRandom[(size_t)i] = metaSlot >= 0 ? clampf(slot.phaseRandom, 0.0f, 1.0f) : 0.0f;
        out.phaseDriftHz[(size_t)i] = 0.0f;
        out.phaseJitter[(size_t)i] = 0.0f;
    }
    zeroTrailing(out, out.partialCount);
}

void GeneratorBank::generateTimeline(const SourceGenParams &p, SpectralTimeline &out) const
{
    StaticSpectralFrame frame;
    generate(p, frame);
    initTimelineDefaults(out);
    out.frameCount = 1;
    out.durationSeconds = 0.0f;
    out.loop = false;
    out.timeSeconds[0] = 0.0f;
    out.frames[0] = frame;
}

} // namespace synth
