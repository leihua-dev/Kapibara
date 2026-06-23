#pragma once

// Per-strip 3-band EQ insert (low shelf / mid peak / high shelf).

#include "dsp/InsertEffects.h"

namespace synth { namespace fx {

struct EqFxState
{
    BiquadState band[3][2];
    void reset() { for(auto &b : band) { b[0].reset(); b[1].reset(); } }
};

// modOff: additive offsets {lowDb, midDb, highDb, midHz(oct)} (small).
inline void processEq(float *L, float *R, int n, double sampleRate,
                      EqSlotParams eq, EqFxState &st, const float modOff[4])
{
    eq.lowDb  += modOff[0] * 18.0f;
    eq.midDb  += modOff[1] * 18.0f;
    eq.highDb += modOff[2] * 18.0f;
    eq.midHz  *= std::pow(2.0f, modOff[3] * 2.0f);
    BiquadCoeffs c[3];
    designEqBiquads(eq, sampleRate, c);
    for(int s = 0; s < n; ++s)
    {
        float l = L[s], r = R[s];
        for(int b = 0; b < 3; ++b)
        {
            l = st.band[b][0].process(c[b], l);
            r = st.band[b][1].process(c[b], r);
        }
        L[s] = l; R[s] = r;
    }
}

}} // namespace synth::fx
