#pragma once

// Per-strip filter insert (state-variable / biquad with optional input drive).
// Separated from the other effects so each effect lives in its own module.

#include "dsp/InsertEffects.h"
#include <cmath>

namespace synth { namespace fx {

struct FilterFxState
{
    BiquadState ch[2];
    void reset() { ch[0].reset(); ch[1].reset(); }
};

// modOff: additive modulation offsets {cutoff(oct), reso, drive, mix}.
inline void processFilter(float *L, float *R, int n, double sampleRate,
                          FilterSlotParams fs, FilterFxState &st, const float modOff[4])
{
    fs.cutoffHz  *= std::pow(2.0f, std::max(-6.0f, std::min(6.0f, modOff[0])));
    fs.resonance += modOff[1] * 9.95f;
    fs.drive     += modOff[2] * 15.0f;
    fs.mix        = std::max(0.0f, std::min(1.0f, fs.mix + modOff[3]));
    fs.cutoffHz   = std::max(20.0f, std::min(fs.cutoffHz, float(sampleRate * 0.45)));
    fs.resonance  = std::max(0.05f, std::min(10.0f, fs.resonance));

    const BiquadCoeffs c = designInsertBiquad(fs, sampleRate);
    const float mix = std::max(0.0f, std::min(1.0f, fs.mix));
    const float drive = std::max(1.0f, std::min(16.0f, fs.drive));
    const bool driven = drive > 1.01f;
    const float norm = driven ? 1.0f / std::tanh(drive) : 1.0f;
    for(int s = 0; s < n; ++s)
    {
        const float dl = L[s], dr = R[s];
        float inL = driven ? std::tanh(dl * drive) * norm : dl;
        float inR = driven ? std::tanh(dr * drive) * norm : dr;
        L[s] = dl + (st.ch[0].process(c, inL) - dl) * mix;
        R[s] = dr + (st.ch[1].process(c, inR) - dr) * mix;
    }
}

}} // namespace synth::fx
