#pragma once

// Per-strip filter insert (state-variable / biquad with optional input drive).
// Separated from the other effects so each effect lives in its own module.

#include "dsp/InsertEffects.h"
#include <cmath>

namespace synth { namespace fx {

struct FilterFxState
{
    BiquadState ch[2];
    // Disperser cascade: every section has its OWN coefficients, so each needs
    // its own delay pair. BiquadState cannot serve here — it holds one set of
    // coefficients for all of its stages.
    float apZ1[2][kMaxDisperserStages] {};
    float apZ2[2][kMaxDisperserStages] {};
    void reset()
    {
        ch[0].reset(); ch[1].reset();
        for(int c = 0; c < 2; ++c)
            for(int i = 0; i < kMaxDisperserStages; ++i)
            { apZ1[c][i] = 0.0f; apZ2[c][i] = 0.0f; }
    }
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

    const float mix = std::max(0.0f, std::min(1.0f, fs.mix));
    const float drive = std::max(1.0f, std::min(16.0f, fs.drive));
    const bool driven = drive > 1.01f;
    const float norm = driven ? 1.0f / std::tanh(drive) : 1.0f;

    if(isAllpassAlgo(fs.algo))
    {
        // Per-section coefficients, computed once per block. Spread walks the
        // section frequencies across +/-2 octaves around the cutoff and the Qs
        // around the set resonance; with both at zero every section is identical
        // and this reduces exactly to the old cascade.
        const int sections = disperserSections(fs);
        float b0[kMaxDisperserStages], b1[kMaxDisperserStages], b2[kMaxDisperserStages];
        float a1[kMaxDisperserStages], a2[kMaxDisperserStages];
        const float nyq = float(sampleRate * 0.45);
        for(int k = 0; k < sections; ++k)
        {
            float oct = 0.0f, qMul = 1.0f;
            disperserStage(fs, sections, k, oct, qMul);
            const float f = std::max(20.0f, std::min(nyq, fs.cutoffHz * std::pow(2.0f, oct)));
            const float q = std::max(0.05f, std::min(10.0f, fs.resonance * qMul));
            const float w0 = 6.28318530717958647692f * f / float(sampleRate);
            const float cw = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * q);
            const float a0 = 1.0f + alpha;
            // RBJ allpass: poles and zeros mirrored about the unit circle, so
            // magnitude is flat and only phase moves.
            b0[k] = (1.0f - alpha) / a0;
            b1[k] = (-2.0f * cw) / a0;
            b2[k] = 1.0f;
            a1[k] = (-2.0f * cw) / a0;
            a2[k] = (1.0f - alpha) / a0;
        }
        for(int s = 0; s < n; ++s)
        {
            const float dl = L[s], dr = R[s];
            float vL = driven ? std::tanh(dl * drive) * norm : dl;
            float vR = driven ? std::tanh(dr * drive) * norm : dr;
            for(int k = 0; k < sections; ++k)
            {
                const float yL = b0[k] * vL + st.apZ1[0][k];
                st.apZ1[0][k] = b1[k] * vL - a1[k] * yL + st.apZ2[0][k];
                st.apZ2[0][k] = b2[k] * vL - a2[k] * yL;
                vL = yL;
                const float yR = b0[k] * vR + st.apZ1[1][k];
                st.apZ1[1][k] = b1[k] * vR - a1[k] * yR + st.apZ2[1][k];
                st.apZ2[1][k] = b2[k] * vR - a2[k] * yR;
                vR = yR;
            }
            L[s] = dl + (vL - dl) * mix;
            R[s] = dr + (vR - dr) * mix;
        }
        return;
    }

    const BiquadCoeffs c = designInsertBiquad(fs, sampleRate);
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
