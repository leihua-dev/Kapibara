#include "AdsrEnv.h"

#include <cmath>

namespace synth
{

namespace
{
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }
} // namespace

float envCurveEval(EnvCurve mode, float tau, float eta)
{
    tau = clampf(tau, 0.0f, 1.0f);
    eta = std::max(0.05f, eta);
    switch(mode)
    {
        case EnvCurve::Exp:
        {
            const float denom = 1.0f - std::exp(-eta);
            return (1.0f - std::exp(-eta * tau)) / std::max(1e-6f, denom);
        }
        case EnvCurve::Power:
            return std::pow(tau, eta);
        case EnvCurve::Sigmoid:
        {
            const float a = sigmoid(eta * (tau - 0.5f));
            const float lo = sigmoid(-eta * 0.5f);
            const float hi = sigmoid(eta * 0.5f);
            return (a - lo) / std::max(1e-6f, (hi - lo));
        }
    }
    return tau;
}

float adsrCurveEval(float tau, float curve)
{
    tau = clampf(tau, 0.0f, 1.0f);
    curve = clampf(curve, 0.0f, 1.0f);
    const float exponent = std::pow(2.0f, (0.5f - curve) * 6.0f);
    return std::pow(tau, std::max(0.05f, exponent));
}

} // namespace synth
