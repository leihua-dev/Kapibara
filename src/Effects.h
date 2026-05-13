#pragma once

#include <array>
#include <cstdint>

namespace synth
{

enum class EffectProcessMode : uint8_t
{
    Normal = 0,
    Linear = 1,
    Nonlinear = 2
};

enum class FilterType : uint8_t
{
    LowPass = 0,
    HighPass = 1,
    BandPass = 2
};

struct EqParams
{
    bool enabled = false;
    EffectProcessMode mode = EffectProcessMode::Normal;
    float lowGainDb = 0.0f;
    float midGainDb = 0.0f;
    float highGainDb = 0.0f;
    float drive = 1.0f;
};

struct FilterParams
{
    bool enabled = false;
    EffectProcessMode mode = EffectProcessMode::Normal;
    FilterType type = FilterType::LowPass;
    float cutoffHz = 12000.0f;
    float resonance = 0.0f;
    float drive = 1.0f;
};

struct EffectsChainParams
{
    EqParams eq;
    FilterParams filter;
};

class EffectsChain
{
  public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const EffectsChainParams &p);
    EffectsChainParams getParams() const { return params_; }
    void process(float *left, float *right, int numSamples);

  private:
    struct EqState
    {
        float low = 0.0f;
        float highLow = 0.0f;
    };
    struct FilterState
    {
        float lp1 = 0.0f;
        float lp2 = 0.0f;
    };

    float processEqSample(float x, EqState &s) const;
    float processFilterSample(float x, FilterState &s) const;

    EffectsChainParams params_ {};
    double sampleRate_ = 48000.0;
    std::array<EqState, 2> eqState_ {};
    std::array<FilterState, 2> filterState_ {};
};

const char *effectModeName(EffectProcessMode m);
const char *filterTypeName(FilterType t);

} // namespace synth
