#include "Operators.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace synth
{

const char *operatorTypeName(OperatorType t)
{
    switch(t)
    {
        case OperatorType::PartialMask:      return "PartialMask";
        case OperatorType::AmpScalePerGroup: return "AmpScalePerGroup";
        case OperatorType::FrequencyJitter:  return "FrequencyJitter";
        case OperatorType::SpectralTilt:     return "SpectralTilt";
        case OperatorType::HarmonicLock:     return "HarmonicLock";
    }
    return "Unknown";
}

namespace
{
void applyPartialMask(StaticSpectralFrame &f, const OperatorBase &op)
{
    const int N = f.partialCount;
    for(int i = 0; i < N; ++i)
    {
        const bool inRange = (i >= op.maskLow && i < op.maskHigh);
        const uint8_t g = f.mu[i];
        const bool groupOk = (g == 0 ? op.maskGroupLow
                              : g == 1 ? op.maskGroupMid : op.maskGroupHigh);
        if(!inRange || !groupOk)
            f.amp[i] = 0.0f;
    }
}

void applyAmpScalePerGroup(StaticSpectralFrame &f, const OperatorBase &op)
{
    const int N = f.partialCount;
    for(int i = 0; i < N; ++i)
    {
        const uint8_t g = f.mu[i];
        const float gain = (g == 0 ? op.gainLow : g == 1 ? op.gainMid : op.gainHigh);
        f.amp[i] *= gain;
    }
}

void applyFrequencyJitter(StaticSpectralFrame &f, const OperatorBase &op)
{
    if(op.jitterAmount <= 0.0f)
        return;
    const int N = f.partialCount;
    std::mt19937 rng(op.jitterSeed ^ 0xDEADBEEFu);
    std::uniform_real_distribution<float> uni(-1.0f, 1.0f);
    const float k = 0.05f * op.jitterAmount;  // up to +/-5% deviation
    for(int i = 0; i < N; ++i)
        f.nu[i] *= (1.0f + k * uni(rng));
}

void applySpectralTilt(StaticSpectralFrame &f, const OperatorBase &op)
{
    if(std::abs(op.extraTilt) < 1e-6f)
        return;
    const int N = f.partialCount;
    float maxAmp = 1e-6f;
    for(int i = 0; i < N; ++i)
    {
        const float n = float(i + 1);
        f.amp[i] *= std::pow(n, -op.extraTilt);
        if(f.amp[i] > maxAmp)
            maxAmp = f.amp[i];
    }
    const float invPeak = 1.0f / maxAmp;
    for(int i = 0; i < N; ++i)
        f.amp[i] *= invPeak;
}

void applyHarmonicLock(StaticSpectralFrame &f, const OperatorBase &op)
{
    const float a = std::clamp(op.lockAmount, 0.0f, 1.0f);
    if(a <= 0.0f || f.freqMode != FreqMode::RelativeRatio)
        return;
    const int N = f.partialCount;
    for(int i = 0; i < N; ++i)
    {
        const float target = std::round(f.nu[i]);
        f.nu[i] = (1.0f - a) * f.nu[i] + a * target;
    }
}
} // namespace

void OperatorChain::apply(StaticSpectralFrame &f) const
{
    for(const auto &op : ops)
    {
        if(!op.enabled)
            continue;
        switch(op.type)
        {
            case OperatorType::PartialMask:      applyPartialMask(f, op); break;
            case OperatorType::AmpScalePerGroup: applyAmpScalePerGroup(f, op); break;
            case OperatorType::FrequencyJitter:  applyFrequencyJitter(f, op); break;
            case OperatorType::SpectralTilt:     applySpectralTilt(f, op); break;
            case OperatorType::HarmonicLock:     applyHarmonicLock(f, op); break;
        }
    }
}

void OperatorChain::apply(SpectralTimeline &t) const
{
    const int n = std::clamp(t.frameCount, 1, kMaxTimelineFrames);
    t.frameCount = n;
    for(int i = 0; i < n; ++i)
        apply(t.frames[(size_t)i]);
}

} // namespace synth
