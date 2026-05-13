#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace synth
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;

inline float dbToGain(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

inline float onePoleCoeff(float hz, double sampleRate)
{
    const float safeHz = std::clamp(hz, 5.0f, float(sampleRate * 0.45));
    return 1.0f - std::exp(-2.0f * kPi * safeHz / float(sampleRate));
}

inline float saturate(float x, float drive)
{
    const float d = std::max(0.1f, drive);
    const float norm = std::tanh(d);
    return std::tanh(x * d) / std::max(1e-6f, norm);
}
} // namespace

const char *effectModeName(EffectProcessMode m)
{
    switch(m)
    {
        case EffectProcessMode::Normal: return "Normal";
        case EffectProcessMode::Linear: return "Linear";
        case EffectProcessMode::Nonlinear: return "Nonlinear";
    }
    return "Normal";
}

const char *filterTypeName(FilterType t)
{
    switch(t)
    {
        case FilterType::LowPass: return "LowPass";
        case FilterType::HighPass: return "HighPass";
        case FilterType::BandPass: return "BandPass";
    }
    return "LowPass";
}

void EffectsChain::prepare(double sr)
{
    sampleRate_ = sr > 1.0 ? sr : 48000.0;
    reset();
}

void EffectsChain::reset()
{
    eqState_ = {};
    filterState_ = {};
}

void EffectsChain::setParams(const EffectsChainParams &p)
{
    params_ = p;
}

float EffectsChain::processEqSample(float x, EqState &s) const
{
    if(!params_.eq.enabled)
        return x;

    const float lowCoeff = onePoleCoeff(220.0f, sampleRate_);
    const float highCoeff = onePoleCoeff(2400.0f, sampleRate_);
    s.low += lowCoeff * (x - s.low);
    s.highLow += highCoeff * (x - s.highLow);

    const float low = s.low;
    const float high = x - s.highLow;
    const float mid = x - low - high;

    float y = low * dbToGain(params_.eq.lowGainDb)
            + mid * dbToGain(params_.eq.midGainDb)
            + high * dbToGain(params_.eq.highGainDb);

    if(params_.eq.mode == EffectProcessMode::Normal)
        y = 0.98f * y + 0.02f * x;
    else if(params_.eq.mode == EffectProcessMode::Nonlinear)
        y = saturate(y, params_.eq.drive);

    return y;
}

float EffectsChain::processFilterSample(float x, FilterState &s) const
{
    if(!params_.filter.enabled)
        return x;

    float input = x;
    if(params_.filter.mode == EffectProcessMode::Nonlinear)
        input = saturate(input, params_.filter.drive);

    const float coeff = onePoleCoeff(params_.filter.cutoffHz, sampleRate_);
    const float res = std::clamp(params_.filter.resonance, 0.0f, 0.95f);
    const float feedback = res * (s.lp1 - s.lp2);
    s.lp1 += coeff * ((input - feedback) - s.lp1);
    s.lp2 += coeff * (s.lp1 - s.lp2);

    float y = s.lp2;
    switch(params_.filter.type)
    {
        case FilterType::LowPass: y = s.lp2; break;
        case FilterType::HighPass: y = input - s.lp2; break;
        case FilterType::BandPass: y = s.lp1 - s.lp2; break;
    }

    if(params_.filter.mode == EffectProcessMode::Normal)
        y = 0.995f * y + 0.005f * input;
    else if(params_.filter.mode == EffectProcessMode::Nonlinear)
        y = saturate(y, std::max(0.5f, params_.filter.drive));

    return y;
}

void EffectsChain::process(float *left, float *right, int numSamples)
{
    for(int i = 0; i < numSamples; ++i)
    {
        float l = processEqSample(left[i], eqState_[0]);
        float r = processEqSample(right[i], eqState_[1]);
        l = processFilterSample(l, filterState_[0]);
        r = processFilterSample(r, filterState_[1]);
        left[i] = std::clamp(l, -1.5f, 1.5f);
        right[i] = std::clamp(r, -1.5f, 1.5f);
    }
}

} // namespace synth
