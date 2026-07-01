#pragma once

#include <array>
#include <cstdint>

namespace synth
{

enum class MasterEffectProcessMode : uint8_t
{
    Normal = 0,
    Linear = 1,
    Nonlinear = 2
};

enum class MasterFilterType : uint8_t
{
    LowPass = 0,
    HighPass = 1,
    BandPass = 2
};

struct MasterEqParams
{
    bool enabled = false;
    MasterEffectProcessMode mode = MasterEffectProcessMode::Normal;
    float lowGainDb = 0.0f;
    float midGainDb = 0.0f;
    float highGainDb = 0.0f;
    float drive = 1.0f;
};

struct MasterFilterParams
{
    bool enabled = false;
    MasterEffectProcessMode mode = MasterEffectProcessMode::Normal;
    MasterFilterType type = MasterFilterType::LowPass;
    float cutoffHz = 12000.0f;
    float resonance = 0.0f;
    float drive = 1.0f;
};

struct MasterEffectsParams
{
    MasterEqParams eq;
    MasterFilterParams filter;
};

class MasterEffectsChain
{
  public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const MasterEffectsParams &p);
    MasterEffectsParams getParams() const { return params_; }
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

    MasterEffectsParams params_ {};
    double sampleRate_ = 48000.0;
    std::array<EqState, 2> eqState_ {};
    std::array<FilterState, 2> filterState_ {};
};

const char *masterEffectModeName(MasterEffectProcessMode m);
const char *masterFilterTypeName(MasterFilterType t);

} // namespace synth
