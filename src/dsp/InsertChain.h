#pragma once

// A strip-grid insert chain: an ordered, unbounded list of effects, each
// carrying its OWN parameters (no shared bank, no slot numbers). Processed on the
// strip bus (post-mix of that strip's voices), so delay/reverb tails are correct.

#include "dsp/InsertEffects.h"
#include "dsp/fx/FilterFx.h"
#include "dsp/fx/DistortionFx.h"
#include "dsp/fx/EqFx.h"
#include "dsp/fx/CompressorFx.h"
#include "dsp/fx/DelayFx.h"
#include "dsp/fx/ReverbFx.h"
#include "dsp/fx/ConvolutionFx.h"

#include <array>
#include <algorithm>
#include <memory>
#include <vector>

namespace synth
{

// Runtime state for one strip-grid chain. Grows to match the chain length.
struct InsertChainState
{
    struct SlotState
    {
        fx::FilterFxState filter;
        fx::DistortionFxState dist;
        fx::EqFxState eq;
        fx::CompressorFxState comp;
        fx::DelayFxState delay;
        fx::ReverbFxState reverb;
        fx::ConvReverbFxState conv;
        std::array<std::shared_ptr<InsertChainState>, 3> bandStates {};
        std::array<std::vector<float>, 6> bandBuffers {};
        static constexpr int kXoverTaps = 129;
        std::array<float, kXoverTaps> xoverLp1 {};
        std::array<float, kXoverTaps> xoverLp2 {};
        std::array<float, kXoverTaps> xoverHistL {};
        std::array<float, kXoverTaps> xoverHistR {};
        int xoverHistPos = 0;
        float xoverLowHz = -1.0f;
        float xoverHighHz = -1.0f;
        double xoverSampleRate = 0.0;
    };
    std::vector<SlotState> slots;
    void reset()
    {
        for(auto &s : slots)
        {
            s.filter.reset(); s.dist.reset(); s.eq.reset();
            s.comp.reset(); s.delay.reset(); s.reverb.reset(); s.conv.reset();
            for(auto &b : s.bandStates)
                if(b) b->reset();
            s.xoverHistL.fill(0.0f);
            s.xoverHistR.fill(0.0f);
            s.xoverHistPos = 0;
        }
    }
};

inline void designLinearPhaseLowpass(std::array<float, InsertChainState::SlotState::kXoverTaps> &coeff,
                                     float cutoffHz, double sampleRate)
{
    constexpr float pi = 3.14159265358979323846f;
    constexpr int taps = InsertChainState::SlotState::kXoverTaps;
    constexpr int mid = taps / 2;
    const float sr = float(sampleRate > 1.0 ? sampleRate : 48000.0);
    const float fc = std::clamp(cutoffHz, 20.0f, sr * 0.45f) / sr;
    float sum = 0.0f;
    for(int i = 0; i < taps; ++i)
    {
        const int n = i - mid;
        const float sinc = n == 0 ? 2.0f * fc : std::sin(2.0f * pi * fc * float(n)) / (pi * float(n));
        const float w = 0.42f - 0.5f * std::cos(2.0f * pi * float(i) / float(taps - 1))
                       + 0.08f * std::cos(4.0f * pi * float(i) / float(taps - 1));
        coeff[(size_t)i] = sinc * w;
        sum += coeff[(size_t)i];
    }
    if(std::abs(sum) > 1.0e-9f)
        for(auto &c : coeff)
            c /= sum;
}

inline float runLinearPhaseFir(const std::array<float, InsertChainState::SlotState::kXoverTaps> &coeff,
                               const std::array<float, InsertChainState::SlotState::kXoverTaps> &hist,
                               int newest)
{
    constexpr int taps = InsertChainState::SlotState::kXoverTaps;
    float y = 0.0f;
    for(int k = 0; k < taps; ++k)
    {
        int idx = newest - k;
        if(idx < 0) idx += taps;
        y += coeff[(size_t)k] * hist[(size_t)idx];
    }
    return y;
}

inline void processInsertChain(float *L, float *R, int n, double sampleRate,
                               const std::vector<InsertEffect> &chain, InsertChainState &state,
                               const float *modByInsert = nullptr, int modStride = 8);

inline void processOneInsert(float *L, float *R, int n, double sampleRate,
                             const InsertEffect &fx, InsertChainState::SlotState &st,
                             const float *m)
{
    switch(fx.kind)
    {
        case InsertFilter:  fx::processFilter(L, R, n, sampleRate, fx.filter, st.filter, m); break;
        case InsertDist:    fx::processDistortion(L, R, n, fx.dist, st.dist, m); break;
        case InsertEq:      fx::processEq(L, R, n, sampleRate, fx.eq, st.eq, m); break;
        case InsertComp:    fx::processCompressor(L, R, n, sampleRate, fx.comp, st.comp, m); break;
        case InsertDelay:   fx::processDelay(L, R, n, sampleRate, fx.delay, st.delay, m); break;
        case InsertReverb:  fx::processReverb(L, R, n, sampleRate, fx.reverb, st.reverb, m); break;
        case InsertConvReverb: fx::processConvReverb(L, R, n, sampleRate, fx.conv, st.conv, m); break;
        case InsertMultiband:
        {
            if(!fx.multiband)
                break;
            const float lowHz = std::clamp(fx.multiband->lowXoverHz, 20.0f, 20000.0f);
            const float highHz = std::clamp(fx.multiband->highXoverHz, lowHz + 20.0f, 20000.0f);
            if(st.xoverLowHz != lowHz || st.xoverHighHz != highHz || st.xoverSampleRate != sampleRate)
            {
                designLinearPhaseLowpass(st.xoverLp1, lowHz, sampleRate);
                designLinearPhaseLowpass(st.xoverLp2, highHz, sampleRate);
                st.xoverLowHz = lowHz;
                st.xoverHighHz = highHz;
                st.xoverSampleRate = sampleRate;
            }
            for(auto &buf : st.bandBuffers)
                if(int(buf.size()) < n) buf.resize((size_t)n);

            auto *lowL  = st.bandBuffers[0].data();
            auto *lowR  = st.bandBuffers[1].data();
            auto *midL  = st.bandBuffers[2].data();
            auto *midR  = st.bandBuffers[3].data();
            auto *highL = st.bandBuffers[4].data();
            auto *highR = st.bandBuffers[5].data();
            constexpr int taps = InsertChainState::SlotState::kXoverTaps;
            constexpr int delay = taps / 2;
            for(int i = 0; i < n; ++i)
            {
                st.xoverHistL[(size_t)st.xoverHistPos] = L[i];
                st.xoverHistR[(size_t)st.xoverHistPos] = R[i];
                const int delayedIdx = (st.xoverHistPos - delay + taps) % taps;
                const float delayedL = st.xoverHistL[(size_t)delayedIdx];
                const float delayedR = st.xoverHistR[(size_t)delayedIdx];
                const float lp1L = runLinearPhaseFir(st.xoverLp1, st.xoverHistL, st.xoverHistPos);
                const float lp1R = runLinearPhaseFir(st.xoverLp1, st.xoverHistR, st.xoverHistPos);
                const float lp2L = runLinearPhaseFir(st.xoverLp2, st.xoverHistL, st.xoverHistPos);
                const float lp2R = runLinearPhaseFir(st.xoverLp2, st.xoverHistR, st.xoverHistPos);
                lowL[i] = lp1L;
                lowR[i] = lp1R;
                midL[i] = lp2L - lp1L;
                midR[i] = lp2R - lp1R;
                highL[i] = delayedL - lp2L;
                highR[i] = delayedR - lp2R;
                st.xoverHistPos = (st.xoverHistPos + 1) % taps;
            }

            for(int band = 0; band < 3; ++band)
                if(!st.bandStates[(size_t)band])
                    st.bandStates[(size_t)band] = std::make_shared<InsertChainState>();
            processInsertChain(lowL, lowR, n, sampleRate, fx.multiband->bands[0], *st.bandStates[0]);
            processInsertChain(midL, midR, n, sampleRate, fx.multiband->bands[1], *st.bandStates[1]);
            processInsertChain(highL, highR, n, sampleRate, fx.multiband->bands[2], *st.bandStates[2]);
            const bool anySolo = fx.multiband->bandSolo[0] || fx.multiband->bandSolo[1] || fx.multiband->bandSolo[2];
            const bool useLow  = anySolo ? fx.multiband->bandSolo[0] : !fx.multiband->bandMute[0];
            const bool useMid  = anySolo ? fx.multiband->bandSolo[1] : !fx.multiband->bandMute[1];
            const bool useHigh = anySolo ? fx.multiband->bandSolo[2] : !fx.multiband->bandMute[2];
            for(int i = 0; i < n; ++i)
            {
                L[i] = (useLow ? lowL[i] : 0.0f) + (useMid ? midL[i] : 0.0f) + (useHigh ? highL[i] : 0.0f);
                R[i] = (useLow ? lowR[i] : 0.0f) + (useMid ? midR[i] : 0.0f) + (useHigh ? highR[i] : 0.0f);
            }
            break;
        }
        default: break;
    }
}

// modByInsert (optional): per-insert 8 modulation offsets laid out as
//   modByInsert[insertIdx * 8 + param]; param mapping is type-specific (4 used).
inline void processInsertChain(float *L, float *R, int n, double sampleRate,
                               const std::vector<InsertEffect> &chain, InsertChainState &state,
                               const float *modByInsert, int modStride)
{
    if(state.slots.size() < chain.size())
        state.slots.resize(chain.size());
    static const float kZeroMod[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for(size_t i = 0; i < chain.size(); ++i)
    {
        const InsertEffect &fx = chain[i];
        if(fx.kind == 0 || fx.bypass)
            continue;
        auto &st = state.slots[i];
        const float *m = modByInsert != nullptr ? modByInsert + i * size_t(modStride) : kZeroMod;
        processOneInsert(L, R, n, sampleRate, fx, st, m);
    }
}

// Route-graph ordered variant: process only the inserts listed in order[0..orderCount-1].
// state.slots are indexed by the ORIGINAL chain position so DSP state survives reordering.
inline void processInsertChainOrdered(float *L, float *R, int n, double sampleRate,
                                      const std::vector<InsertEffect> &chain, InsertChainState &state,
                                      const uint8_t *order, int orderCount,
                                      const float *modByInsert = nullptr, int modStride = 8)
{
    if(state.slots.size() < chain.size())
        state.slots.resize(chain.size());
    static const float kZeroMod[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for(int oi = 0; oi < orderCount; ++oi)
    {
        const int i = int(order[(size_t)oi]);
        if(i < 0 || i >= int(chain.size())) continue;
        const InsertEffect &fx = chain[(size_t)i];
        if(fx.kind == 0 || fx.bypass) continue;
        auto &st = state.slots[(size_t)i];
        const float *m = modByInsert != nullptr ? modByInsert + size_t(i) * size_t(modStride) : kZeroMod;
        processOneInsert(L, R, n, sampleRate, fx, st, m);
    }
}

} // namespace synth
