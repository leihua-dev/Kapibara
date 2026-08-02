#pragma once

// Per-strip filter insert (state-variable / biquad with optional input drive).
// Separated from the other effects so each effect lives in its own module.

#include "dsp/InsertEffects.h"
#include <cmath>

namespace synth { namespace fx {

struct FilterFxState
{
    // Every section carries its OWN coefficients, so each needs its own delay
    // pair. BiquadState cannot serve here — it holds one coefficient set for all
    // of its stages, which is precisely the limitation that kept the cascade
    // allpass-only.
    float apZ1[2][kMaxDisperserStages] {};
    float apZ2[2][kMaxDisperserStages] {};
    // One sample of each slot's output, held for its own feedback loop. The
    // delay is what makes the loop computable at all.
    float fbZ[2][kMaxDisperserStages] {};
    // DC blocker per slot, used only where that slot distorts. Tube and Diode
    // are deliberately asymmetric — that asymmetry IS their even-harmonic
    // character — but it also leaves a constant offset, measured at 0.48 and
    // 0.86 respectively. Over a chain that offset biases every shaper downstream
    // and eats headroom, so each distorting slot removes its own.
    float dcX[2][kMaxDisperserStages] {};
    float dcY[2][kMaxDisperserStages] {};
    void reset()
    {
        for(int c = 0; c < 2; ++c)
            for(int i = 0; i < kMaxDisperserStages; ++i)
            { apZ1[c][i] = 0.0f; apZ2[c][i] = 0.0f; fbZ[c][i] = 0.0f;
              dcX[c][i] = 0.0f; dcY[c][i] = 0.0f; }
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

    // One cascade for every algo. The disperser was never allpass-specific:
    // disperserStage() only says where a section sits (octaves off the cutoff,
    // Q as a multiple of the knob), which is exactly what a staggered cascade of
    // ANY response is. designInsertBiquad already covers every algo, so a section
    // is just the slot re-designed at its own cutoff and Q.
    //
    // With apStages == 0 and both spreads at zero this reduces to identical
    // sections at the algo's natural depth — bit-identical to the two separate
    // paths this replaces, for allpass and biquad alike.
    const int sections = disperserSections(fs);
    float b0[kMaxDisperserStages], b1[kMaxDisperserStages], b2[kMaxDisperserStages];
    float a1[kMaxDisperserStages], a2[kMaxDisperserStages];
    for(int k = 0; k < sections; ++k)
    {
        const BiquadCoeffs c = designInsertBiquad(disperserSectionSlot(fs, sections, k, sampleRate),
                                                  sampleRate);
        // c.stages is the algo's built-in depth; the cascade is ours to run, and
        // disperserSections() already accounts for it. Honouring it here would
        // square LP4 into eight poles.
        b0[k] = c.b0; b1[k] = c.b1; b2[k] = c.b2;
        a1[k] = c.a1; a2[k] = c.a2;
    }
    if(fs.apParallel != 0)
    {
        // Sections side by side: each sees the same input and the outputs sum.
        // This is the architecture magnitude types actually want — parallel
        // bandpasses are a formant bank, whereas chaining them cancels to
        // nothing. Serial is left alone above.
        float g[kMaxDisperserStages];
        for(int k = 0; k < sections; ++k)
            g[k] = disperserSectionGain(fs, k);
        const float pnorm = disperserParallelNorm(sections);
        for(int s = 0; s < n; ++s)
        {
            const float dl = L[s], dr = R[s];
            const float xL = driven ? std::tanh(dl * drive) * norm : dl;
            const float xR = driven ? std::tanh(dr * drive) * norm : dr;
            float accL = 0.0f, accR = 0.0f;
            for(int k = 0; k < sections; ++k)
            {
                const float yL = b0[k] * xL + st.apZ1[0][k];
                st.apZ1[0][k] = b1[k] * xL - a1[k] * yL + st.apZ2[0][k];
                st.apZ2[0][k] = b2[k] * xL - a2[k] * yL;
                accL += yL * g[k];
                const float yR = b0[k] * xR + st.apZ1[1][k];
                st.apZ1[1][k] = b1[k] * xR - a1[k] * yR + st.apZ2[1][k];
                st.apZ2[1][k] = b2[k] * xR - a2[k] * yR;
                accR += yR * g[k];
            }
            L[s] = dl + (accL * pnorm - dl) * mix;
            R[s] = dr + (accR * pnorm - dr) * mix;
        }
        return;
    }

    // Per-slot voicing, resolved once per block. `anyVoiced` keeps the plain
    // cascade below on its original arithmetic when nothing is configured, which
    // is what every preset written before this looks like.
    float slotFb[kMaxDisperserStages];
    float slotDrive[kMaxDisperserStages];
    InsertDistAlgo slotDist[kMaxDisperserStages];
    bool slotDistOn[kMaxDisperserStages];
    bool anyVoiced = false;
    for(int k = 0; k < sections; ++k)
    {
        slotFb[k] = disperserSlotFeedback(fs, k);
        slotDistOn[k] = disperserSlotDistOn(fs, k);
        slotDist[k] = disperserSlotDist(fs, k);
        slotDrive[k] = disperserSlotDrive(fs, k);
        if(slotFb[k] > 0.0f || slotDistOn[k])
            anyVoiced = true;
    }

    if(anyVoiced)
    {
        // ~5 Hz one-pole DC blocker, well below anything musical.
        const float dcR = 1.0f - 6.2831853f * 5.0f / float(sampleRate > 1.0 ? sampleRate : 48000.0);
        for(int s = 0; s < n; ++s)
        {
            const float dl = L[s], dr = R[s];
            float vL = driven ? std::tanh(dl * drive) * norm : dl;
            float vR = driven ? std::tanh(dr * drive) * norm : dr;
            for(int k = 0; k < sections; ++k)
            {
                float inL = vL, inR = vR;
                if(slotFb[k] > 0.0f)
                {
                    // The saturator on the fed-back sample is NOT optional. An
                    // allpass loop is unconditionally stable (|H| == 1, so the
                    // loop gain is exactly the feedback amount), but any
                    // resonant slot has |H| >> 1 at its peak: a 2-pole lowpass
                    // at Q 10 with 0.99 feedback and no saturator was measured
                    // running away to 2.1e9 with a DC offset of -179000. With it,
                    // the same slot peaks at 1.5 and sits at DC 0.0002.
                    inL -= slotFb[k] * std::tanh(st.fbZ[0][k]);
                    inR -= slotFb[k] * std::tanh(st.fbZ[1][k]);
                }
                if(slotDistOn[k])
                {
                    // Inside the loop on purpose: the harmonics this makes are
                    // fed back and then dispersed by the slots downstream.
                    inL = distShape(slotDist[k], inL, slotDrive[k], 0.0f);
                    inR = distShape(slotDist[k], inR, slotDrive[k], 0.0f);
                    // Strip the offset, keep the harmonics.
                    const float oL = inL, oR = inR;
                    inL = oL - st.dcX[0][k] + dcR * st.dcY[0][k];
                    inR = oR - st.dcX[1][k] + dcR * st.dcY[1][k];
                    st.dcX[0][k] = oL; st.dcY[0][k] = inL;
                    st.dcX[1][k] = oR; st.dcY[1][k] = inR;
                }
                const float yL = b0[k] * inL + st.apZ1[0][k];
                st.apZ1[0][k] = b1[k] * inL - a1[k] * yL + st.apZ2[0][k];
                st.apZ2[0][k] = b2[k] * inL - a2[k] * yL;
                st.fbZ[0][k] = yL;
                vL = yL;
                const float yR = b0[k] * inR + st.apZ1[1][k];
                st.apZ1[1][k] = b1[k] * inR - a1[k] * yR + st.apZ2[1][k];
                st.apZ2[1][k] = b2[k] * inR - a2[k] * yR;
                st.fbZ[1][k] = yR;
                vR = yR;
            }
            L[s] = dl + (vL - dl) * mix;
            R[s] = dr + (vR - dr) * mix;
        }
        return;
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
}

}} // namespace synth::fx
