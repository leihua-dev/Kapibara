#pragma once

#include "model/SpectralFrame.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace synth
{

// Architecture §2.2: Operator writes non-destructive rules / params into a Working Source.
// We implement a simple linear OperatorChain that transforms a frame in place.
// Concrete operators below correspond to the "edit operations" listed in §2.2.3.

enum class OperatorType : uint8_t
{
    PartialMask = 0,        // mask partials by index range or group mu
    AmpScalePerGroup = 1,   // multiply amp by per-mu gain
    FrequencyJitter = 2,    // add deterministic random delta to nu_i
    SpectralTilt = 3,       // post-tilt: amp_i *= n_i^(-extraTilt)
    HarmonicLock = 4        // snap nu_i to nearest integer harmonic by amount
};

struct OperatorBase
{
    bool enabled = true;
    OperatorType type = OperatorType::PartialMask;

    // PartialMask
    int maskLow = 0;          // keep [maskLow, maskHigh)
    int maskHigh = kMaxPartials;
    bool maskGroupLow = true;
    bool maskGroupMid = true;
    bool maskGroupHigh = true;

    // AmpScalePerGroup
    float gainLow = 1.0f;
    float gainMid = 1.0f;
    float gainHigh = 1.0f;

    // FrequencyJitter
    float jitterAmount = 0.0f; // 0..1 -> +/- 5% scaling
    uint32_t jitterSeed = 7u;

    // SpectralTilt
    float extraTilt = 0.0f;    // adds tilt slope on top of generator

    // HarmonicLock
    float lockAmount = 0.0f;   // 0..1
};

class OperatorChain
{
  public:
    void apply(StaticSpectralFrame &f) const;
    void apply(SpectralTimeline &t) const;
    std::vector<OperatorBase> ops;
};

const char *operatorTypeName(OperatorType t);

} // namespace synth
