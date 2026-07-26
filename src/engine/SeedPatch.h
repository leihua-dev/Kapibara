#pragma once

#include "dsp/MasterEffects.h"
#include "dsp/Generators.h"
#include "engine/MatrixEngine.h"

#include <array>
#include <string>

namespace synth
{

struct SeedPatch
{
    std::string name = "Seed";
    SourceGenParams generator;
    AdsrParams adsr;
    std::array<AdsrParams, kMaxAmpEnvs> ampEnvParams = [] {
        std::array<AdsrParams, kMaxAmpEnvs> envs {};
        for(auto &env : envs)
            env.sustain = 1.0f;
        return envs;
    }();
    std::array<ModSlotParams, kMaxModSlots> modSlotParams {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    std::array<MaskGroup, kMaxMaskGroups> maskGroups {};
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    MasterEffectsParams toneFx;
};

} // namespace synth
