#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "dsp/Effects.h"
#include "dsp/Generators.h"
#include "engine/MatrixEngine.h"
#include "dsp/Operators.h"

namespace synth
{

enum class ParameterScope : uint8_t
{
    Global = 0,
    Seed,
    Note
};

enum class ParameterTargetDomain : uint8_t
{
    GlobalParam = 0,
    SeedParam,
    GeneratorParam,
    PartialParam,
    FxParam
};

enum class LockValueMode : uint8_t
{
    Override = 0,
    Additive,
    Multiplicative
};

enum class LockValueType : uint8_t
{
    Float = 0,
    Integer,
    Boolean
};

struct ParameterTarget
{
    ParameterTargetDomain domain = ParameterTargetDomain::GeneratorParam;
    std::string parameterId;
    int partialIndex = -1;
    int groupIndex = -1;
};

struct LockValue
{
    LockValueType type = LockValueType::Float;
    LockValueMode mode = LockValueMode::Override;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
};

struct ParameterLock
{
    ParameterScope scope = ParameterScope::Note;
    ParameterTarget target;
    LockValue value;
    bool enabled = true;
};

struct SeedPatch
{
    std::string name = "Seed";
    SourceGenParams generator;
    OperatorChain operatorChain;
    AdsrParams adsr;
    std::array<AdsrParams, kMaxAmpEnvs> ampEnvParams = [] {
        std::array<AdsrParams, kMaxAmpEnvs> envs {};
        for(auto &env : envs)
            env.sustain = 1.0f;
        return envs;
    }();
    std::array<LfoParams, kMaxLfos> lfoParams {};
    std::array<MatrixEnvParams, kMaxModEnvs> matrixEnvParams {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    EffectsChainParams toneFx;
    std::vector<ParameterLock> defaultLocks;
};

} // namespace synth
