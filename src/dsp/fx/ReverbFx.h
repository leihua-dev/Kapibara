#pragma once

// Per-strip reverb insert: a small Schroeder-style network (4 combs + 2 allpass)
// per channel. Per-strip state means tails continue past individual notes.

#include "dsp/InsertEffects.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace synth { namespace fx {

struct ReverbFxState
{
    static constexpr int kCombs = 4;
    static constexpr int kAllpass = 2;
    std::array<std::vector<float>, kCombs> comb[2];     // [channel][comb]
    std::array<int, kCombs> combIdx[2] {};
    std::array<float, kCombs> combLp[2] {};
    std::array<std::vector<float>, kAllpass> ap[2];     // [channel][allpass]
    std::array<int, kAllpass> apIdx[2] {};
    bool ready = false;

    void prepare(double sampleRate)
    {
        // Base tunings (samples @44.1k), scaled to the current rate.
        const float sr = float(sampleRate) / 44100.0f;
        const int combTune[kCombs] = { 1116, 1188, 1277, 1356 };
        const int apTune[kAllpass] = { 556, 441 };
        for(int ch = 0; ch < 2; ++ch)
        {
            for(int c = 0; c < kCombs; ++c)
            {
                comb[ch][(size_t)c].assign((size_t)std::max(1, int(combTune[c] * sr) + ch * 23), 0.0f);
                combIdx[ch][(size_t)c] = 0; combLp[ch][(size_t)c] = 0.0f;
            }
            for(int a = 0; a < kAllpass; ++a)
            {
                ap[ch][(size_t)a].assign((size_t)std::max(1, int(apTune[a] * sr) + ch * 19), 0.0f);
                apIdx[ch][(size_t)a] = 0;
            }
        }
        ready = true;
    }
    void reset()
    {
        for(int ch = 0; ch < 2; ++ch)
        {
            for(int c = 0; c < kCombs; ++c) { std::fill(comb[ch][(size_t)c].begin(), comb[ch][(size_t)c].end(), 0.0f); combLp[ch][(size_t)c] = 0.0f; }
            for(int a = 0; a < kAllpass; ++a) std::fill(ap[ch][(size_t)a].begin(), ap[ch][(size_t)a].end(), 0.0f);
        }
    }
};

inline void processReverb(float *L, float *R, int n, double sampleRate,
                          ReverbSlotParams rp, ReverbFxState &st, const float modOff[4])
{
    if(!st.ready) st.prepare(sampleRate);
    rp.size  = std::max(0.0f, std::min(1.0f, rp.size + modOff[0]));
    rp.decay = std::max(0.0f, std::min(0.95f, rp.decay + modOff[1]));
    rp.mix   = std::max(0.0f, std::min(1.0f, rp.mix + modOff[2]));
    rp.damp  = std::max(0.0f, std::min(1.0f, rp.damp + modOff[3]));
    const float fb = 0.7f + 0.28f * rp.decay;
    const float damp = std::max(0.02f, rp.damp);
    const float mix = rp.mix;
    const float apCoef = 0.5f;
    for(int s = 0; s < n; ++s)
    {
        for(int ch = 0; ch < 2; ++ch)
        {
            const float in = (ch == 0 ? L[s] : R[s]) * 0.25f;
            float acc = 0.0f;
            for(int c = 0; c < ReverbFxState::kCombs; ++c)
            {
                auto &buf = st.comb[ch][(size_t)c];
                int &idx = st.combIdx[ch][(size_t)c];
                const float y = buf[(size_t)idx];
                st.combLp[ch][(size_t)c] += damp * (y - st.combLp[ch][(size_t)c]);
                buf[(size_t)idx] = in + st.combLp[ch][(size_t)c] * fb;
                if(++idx >= int(buf.size())) idx = 0;
                acc += y;
            }
            float v = acc;
            for(int a = 0; a < ReverbFxState::kAllpass; ++a)
            {
                auto &buf = st.ap[ch][(size_t)a];
                int &idx = st.apIdx[ch][(size_t)a];
                const float bufv = buf[(size_t)idx];
                const float out = -v + bufv;
                buf[(size_t)idx] = v + bufv * apCoef;
                if(++idx >= int(buf.size())) idx = 0;
                v = out;
            }
            float &x = ch == 0 ? L[s] : R[s];
            x += v * mix;
        }
    }
}

}} // namespace synth::fx
