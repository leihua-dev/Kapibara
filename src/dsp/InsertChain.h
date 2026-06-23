#pragma once

// A per-strip / per-group insert chain: an ordered, unbounded list of effects, each
// carrying its OWN parameters (no shared bank, no slot numbers). Processed on the
// strip bus (post-mix of that strip's voices), so delay/reverb tails are correct.

#include "dsp/InsertEffects.h"
#include "dsp/fx/FilterFx.h"
#include "dsp/fx/DistortionFx.h"
#include "dsp/fx/EqFx.h"
#include "dsp/fx/CompressorFx.h"
#include "dsp/fx/DelayFx.h"
#include "dsp/fx/ReverbFx.h"

#include <vector>

namespace synth
{

// Runtime state for one chain (one per strip / group). Grows to match the chain length.
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
    };
    std::vector<SlotState> slots;
    void reset()
    {
        for(auto &s : slots)
        {
            s.filter.reset(); s.dist.reset(); s.eq.reset();
            s.comp.reset(); s.delay.reset(); s.reverb.reset();
        }
    }
};

// modByInsert (optional): per-insert 8 modulation offsets laid out as
//   modByInsert[insertIdx * 8 + param]; param mapping is type-specific (4 used).
inline void processInsertChain(float *L, float *R, int n, double sampleRate,
                               const std::vector<InsertEffect> &chain, InsertChainState &state,
                               const float *modByInsert = nullptr, int modStride = 8)
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
        switch(fx.kind)
        {
            case InsertFilter:  fx::processFilter(L, R, n, sampleRate, fx.filter, st.filter, m); break;
            case InsertDist:    fx::processDistortion(L, R, n, fx.dist, st.dist, m); break;
            case InsertEq:      fx::processEq(L, R, n, sampleRate, fx.eq, st.eq, m); break;
            case InsertComp:    fx::processCompressor(L, R, n, sampleRate, fx.comp, st.comp, m); break;
            case InsertDelay:   fx::processDelay(L, R, n, sampleRate, fx.delay, st.delay, m); break;
            case InsertReverb:  fx::processReverb(L, R, n, sampleRate, fx.reverb, st.reverb, m); break;
            default: break;
        }
    }
}

} // namespace synth
