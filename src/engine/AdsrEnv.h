#pragma once

#include <cstdint>

namespace synth
{

enum class EnvCurve : uint8_t
{
    Exp = 0,
    Power = 1,
    Sigmoid = 2
};

float envCurveEval(EnvCurve mode, float tau, float eta);

struct AdsrParams
{
    float attack = 0.005f;
    float decay = 0.35f;
    float sustain = 1.0f;
    float release = 0.08f;
    float curve = 0.5f;
    float curveA = 0.5f;
    float curveD = 0.5f;
    float curveR = 0.5f;
};

float adsrCurveEval(float tau, float curve);

} // namespace synth
