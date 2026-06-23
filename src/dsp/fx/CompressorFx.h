#pragma once

// Per-strip compressor insert (peak envelope follower + soft gain computer).

#include "dsp/InsertEffects.h"
#include <cmath>

namespace synth { namespace fx {

struct CompressorFxState
{
    float env[2] = { 0.0f, 0.0f };
    void reset() { env[0] = env[1] = 0.0f; }
};

// modOff: additive offsets {thresh(dB), ratio, attack, makeup(dB)} (small).
inline void processCompressor(float *L, float *R, int n, double sampleRate,
                              CompSlotParams cp, CompressorFxState &st, const float modOff[4])
{
    cp.threshDb += modOff[0] * 24.0f;
    cp.ratio    += modOff[1] * 10.0f;
    cp.makeupDb += modOff[3] * 12.0f;
    const float thr = std::pow(10.0f, cp.threshDb / 20.0f);
    const float ratio = std::max(1.0f, std::min(20.0f, cp.ratio));
    const float atk = std::exp(-1.0f / (std::max(0.1f, cp.attackMs) * 0.001f * float(sampleRate)));
    const float rel = std::exp(-1.0f / (120.0f * 0.001f * float(sampleRate)));
    const float makeup = std::pow(10.0f, cp.makeupDb / 20.0f);
    for(int s = 0; s < n; ++s)
    {
        for(int ch = 0; ch < 2; ++ch)
        {
            float &x = ch == 0 ? L[s] : R[s];
            const float a = std::fabs(x);
            float &env = st.env[ch];
            env = a > env ? atk * (env - a) + a : rel * (env - a) + a;
            float gain = 1.0f;
            if(env > thr && env > 1e-9f)
                gain = std::pow(env / thr, 1.0f / ratio - 1.0f);
            x = x * gain * makeup;
        }
    }
}

}} // namespace synth::fx
