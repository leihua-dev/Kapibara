#pragma once

// Per-strip feedback delay insert. Buffer is allocated lazily (per-strip, so long
// buffers and tails that outlive individual notes are fine).

#include "dsp/InsertEffects.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace synth { namespace fx {

struct DelayFxState
{
    std::vector<float> bufL, bufR;
    int write = 0;
    float toneL = 0.0f, toneR = 0.0f;
    void ensure(int size)
    {
        if(int(bufL.size()) != size)
        {
            bufL.assign((size_t)size, 0.0f);
            bufR.assign((size_t)size, 0.0f);
            write = 0;
        }
    }
    void reset()
    {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        write = 0; toneL = toneR = 0.0f;
    }
};

// modOff: additive offsets {time, feedback, mix, tone}.
inline void processDelay(float *L, float *R, int n, double sampleRate,
                         DelaySlotParams dp, DelayFxState &st, const float modOff[4])
{
    const int size = std::max(64, int(2.0 * sampleRate)); // up to 2 s
    st.ensure(size);
    dp.timeMs   = std::max(1.0f, dp.timeMs + modOff[0] * 1000.0f);
    dp.feedback = std::max(0.0f, std::min(0.95f, dp.feedback + modOff[1]));
    dp.mix      = std::max(0.0f, std::min(1.0f, dp.mix + modOff[2]));
    dp.tone     = std::max(0.05f, std::min(1.0f, dp.tone + modOff[3]));
    const int d = std::clamp(int(dp.timeMs * 0.001f * float(sampleRate)), 1, size - 1);
    const float fb = dp.feedback, mix = dp.mix, toneA = dp.tone;
    if(dp.pingpong)
    {
        // Ping-pong: feedback crosses channels so echoes bounce L<->R.
        for(int s = 0; s < n; ++s)
        {
            int r = st.write - d; if(r < 0) r += size;
            const float dlyL = st.bufL[(size_t)r], dlyR = st.bufR[(size_t)r];
            st.toneL += toneA * (dlyL - st.toneL);
            st.toneR += toneA * (dlyR - st.toneR);
            // input sums to one side, feedback swaps sides
            st.bufL[(size_t)st.write] = L[s] + st.toneR * fb;
            st.bufR[(size_t)st.write] = R[s] + st.toneL * fb;
            L[s] += dlyL * mix;
            R[s] += dlyR * mix;
            if(++st.write >= size) st.write = 0;
        }
        return;
    }
    for(int s = 0; s < n; ++s)
    {
        int r = st.write - d; if(r < 0) r += size;
        const float dlyL = st.bufL[(size_t)r], dlyR = st.bufR[(size_t)r];
        st.toneL += toneA * (dlyL - st.toneL);
        st.toneR += toneA * (dlyR - st.toneR);
        st.bufL[(size_t)st.write] = L[s] + st.toneL * fb;
        st.bufR[(size_t)st.write] = R[s] + st.toneR * fb;
        L[s] += dlyL * mix;
        R[s] += dlyR * mix;
        if(++st.write >= size) st.write = 0;
    }
}

}} // namespace synth::fx
