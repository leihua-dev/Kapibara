#include "Generators.h"

#include <algorithm>
#include <cmath>

namespace synth
{

// A Sample / Noise track is a STREAM source: it is rendered as audio in Voice,
// not summed out of the additive partial pool. It still claims one partial slot,
// silent, so that everything keyed off a track's partial range keeps working —
// matrix rules scoped to the track resolve, and the stream reads its own slot's
// modulated amplitude. Costing it 48 partials for a sound it never produced was
// the old shape; those slots now go to tracks that actually use them.
void buildNoiseSeed(const SourceTrackParams &track, WavetableSeedParams &seed)
{
    (void)track;
    seed = WavetableSeedParams {};
    seed.partialCount = 1;
    for(auto &p : seed.partials)
    {
        p.enabled = false;
        p.ratio = 1.0f;
        p.amp = 0.0f;
        p.phase = 0.0f;
    }
}

bool samplerRegionForNote(const SamplerParams &p, int frames, int note,
                          int &regionBegin, int &regionEnd)
{
    regionBegin = regionEnd = 0;
    if(frames <= 1)
        return false;
    const float a = std::clamp(std::min(p.startNorm, p.endNorm), 0.0f, 1.0f);
    const float b = std::clamp(std::max(p.startNorm, p.endNorm), 0.0f, 1.0f);
    int begin = int(a * float(frames));
    int end = int(b * float(frames));
    begin = std::clamp(begin, 0, frames - 1);
    end = std::clamp(end, begin + 1, frames);

    const int slices = std::clamp(p.sliceCount, 1, 64);
    if(slices > 1)
    {
        // Note picks the slice, wrapping so the whole keyboard stays useful
        // instead of going silent above the last slice.
        const int span = end - begin;
        int idx = note - p.rootNote;
        idx = ((idx % slices) + slices) % slices;
        const int sliceLen = std::max(1, span / slices);
        const int sb = begin + idx * sliceLen;
        begin = std::clamp(sb, 0, frames - 1);
        end = std::clamp(idx == slices - 1 ? end : sb + sliceLen, begin + 1, frames);
    }
    regionBegin = begin;
    regionEnd = end;
    return end > begin;
}

const char *noiseTypeName(NoiseType t)
{
    switch(t)
    {
        case NoiseType::Pink:    return "PINK";
        case NoiseType::Brown:   return "BROWN";
        case NoiseType::Blue:    return "BLUE";
        case NoiseType::Violet:  return "VIOLET";
        case NoiseType::Crackle: return "CRACKLE";
        case NoiseType::White:   break;
    }
    return "WHITE";
}

void renderNoiseBlock(NoiseVoiceState &state, NoiseType type, float colour,
                      float *outL, float *outR, int numSamples, double sampleRate)
{
    if(outL == nullptr || outR == nullptr || numSamples <= 0)
        return;

    // Colour tilts whatever the type produced, rather than replacing it: at 0.5
    // the type passes through untouched, and each direction crossfades towards
    // the low- or high-passed version. A control with a usable middle instead of
    // a dead end.
    const float tilt = std::clamp(colour, 0.0f, 1.0f) * 2.0f - 1.0f;
    const float a = float(1.0 - std::exp(-6.283185307179586 * 1200.0
                                         / std::max(1.0, sampleRate)));
    const float dark = std::max(0.0f, -tilt);
    const float bright = std::max(0.0f, tilt);

    uint32_t rng = state.rng;
    const auto next = [&rng]() {
        // xorshift32: no allocation, no locks, deterministic per note.
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float(int32_t(rng)) * (1.0f / 2147483648.0f);
    };
    // Paul Kellett's filter cascade — pink noise from white for the cost of a
    // few multiply-adds, no FFT and no lookup table.
    const auto pink = [](float *p, float w) {
        p[0] = 0.99886f * p[0] + w * 0.0555179f;
        p[1] = 0.99332f * p[1] + w * 0.0750759f;
        p[2] = 0.96900f * p[2] + w * 0.1538520f;
        p[3] = 0.86650f * p[3] + w * 0.3104856f;
        p[4] = 0.55000f * p[4] + w * 0.5329522f;
        p[5] = -0.7616f * p[5] - w * 0.0168980f;
        const float out = p[0] + p[1] + p[2] + p[3] + p[4] + p[5] + p[6] + w * 0.5362f;
        p[6] = w * 0.115926f;
        return out * 0.18f;   // back to roughly unity peak
    };

    for(int s = 0; s < numSamples; ++s)
    {
        // Independent draws per channel: one shared stream would collapse the
        // noise to the centre of the image.
        float wl = next();
        float wr = next();
        float xl = wl, xr = wr;
        switch(type)
        {
            case NoiseType::Pink:
                xl = pink(state.pinkL, wl);
                xr = pink(state.pinkR, wr);
                break;
            case NoiseType::Brown:
                // Leaky integrator; the leak stops DC from walking off. The gain
                // is picked so the output lands near the other types' level on
                // its own — scaling up afterwards would just sit on the clamp and
                // turn brown noise into a square wave.
                state.brownL = state.brownL * 0.99f + wl * 0.1f;
                state.brownR = state.brownR * 0.99f + wr * 0.1f;
                xl = state.brownL;
                xr = state.brownR;
                break;
            case NoiseType::Blue:
            {
                // Differentiated pink. The cascade must be advanced ONCE per
                // sample — calling it again to fetch "the previous value" would
                // run the filter at double rate.
                const float pl = pink(state.pinkL, wl);
                const float pr = pink(state.pinkR, wr);
                xl = (pl - state.prevL) * 1.4f;
                xr = (pr - state.prevR) * 1.4f;
                state.prevL = pl;
                state.prevR = pr;
                break;
            }
            case NoiseType::Violet:
                xl = (wl - state.prevL) * 0.5f;
                xr = (wr - state.prevR) * 0.5f;
                state.prevL = wl;
                state.prevR = wr;
                break;
            case NoiseType::Crackle:
            {
                // Sparse impulses: mostly silence with occasional full-scale hits.
                const float thr = 0.985f;
                xl = std::abs(wl) > thr ? (wl > 0.0f ? 1.0f : -1.0f) : 0.0f;
                xr = std::abs(wr) > thr ? (wr > 0.0f ? 1.0f : -1.0f) : 0.0f;
                break;
            }
            case NoiseType::White:
                break;
        }

        state.lpL += a * (xl - state.lpL);
        state.lpR += a * (xr - state.lpR);
        const float dl = state.lpL, dr = state.lpR;
        const float bl = xl - state.lpL, br = xr - state.lpR;
        outL[s] = std::clamp(xl + dark * (dl - xl) + bright * (bl - xl), -1.0f, 1.0f);
        outR[s] = std::clamp(xr + dark * (dr - xr) + bright * (br - xr), -1.0f, 1.0f);
    }
    state.rng = rng;
}

} // namespace synth
