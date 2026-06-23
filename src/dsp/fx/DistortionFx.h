#pragma once

// Per-strip distortion insert (waveshaper + DC blocker against note-edge clicks).

#include "dsp/InsertEffects.h"
#include <cmath>

namespace synth { namespace fx {

struct DistortionFxState
{
    float x1[2] = { 0.0f, 0.0f };
    float y1[2] = { 0.0f, 0.0f };
    void reset() { x1[0] = x1[1] = y1[0] = y1[1] = 0.0f; }
};

// modOff: additive offsets {drive, bias, mix, out}.
inline void processDistortion(float *L, float *R, int n, DistSlotParams ds,
                              DistortionFxState &st, const float modOff[4])
{
    ds.drive   = std::max(1.0f, std::min(32.0f, ds.drive + modOff[0] * 31.0f));
    ds.bias    = std::max(-1.0f, std::min(1.0f, ds.bias + modOff[1]));
    ds.mix     = std::max(0.0f, std::min(1.0f, ds.mix + modOff[2]));
    ds.outGain = std::max(0.0f, std::min(2.0f, ds.outGain + modOff[3] * 2.0f));
    const float mix = ds.mix;
    constexpr float kR = 0.9985f;
    for(int s = 0; s < n; ++s)
    {
        for(int ch = 0; ch < 2; ++ch)
        {
            float &x = ch == 0 ? L[s] : R[s];
            const float dry = x;
            float w = distShape(ds.algo, dry, ds.drive, ds.bias) * ds.outGain;
            const float y = w - st.x1[ch] + kR * st.y1[ch];  // DC blocker
            st.x1[ch] = w; st.y1[ch] = y;
            x = dry + (y - dry) * mix;
        }
    }
}

}} // namespace synth::fx
