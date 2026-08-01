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

void renderNoiseBlock(NoiseVoiceState &state, float colour, float *outL, float *outR,
                      int numSamples, double sampleRate)
{
    if(outL == nullptr || outR == nullptr || numSamples <= 0)
        return;

    // Tilt rather than a plain filter sweep: at 0.5 the noise passes through
    // white, and each direction crossfades towards the low- or high-passed
    // version, so the control has a usable middle instead of a dead end.
    const float tilt = std::clamp(colour, 0.0f, 1.0f) * 2.0f - 1.0f;
    const float cutoffHz = 1200.0f;
    const float a = float(1.0 - std::exp(-6.283185307179586 * double(cutoffHz)
                                         / std::max(1.0, sampleRate)));
    const float dark = std::max(0.0f, -tilt);
    const float bright = std::max(0.0f, tilt);

    uint32_t rng = state.rng;
    const auto next = [&rng]() {
        // xorshift32: no allocation, no locks, deterministic per note.
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float(int32_t(rng)) * (1.0f / 2147483648.0f);
    };

    for(int s = 0; s < numSamples; ++s)
    {
        // Independent draws per channel: one shared stream would collapse the
        // noise to the centre of the image.
        const float xl = next();
        const float xr = next();
        state.lpL += a * (xl - state.lpL);
        state.lpR += a * (xr - state.lpR);
        const float dl = state.lpL, dr = state.lpR;
        const float bl = xl - state.lpL, br = xr - state.lpR;
        outL[s] = xl + dark * (dl - xl) + bright * (bl - xl);
        outR[s] = xr + dark * (dr - xr) + bright * (br - xr);
    }
    state.rng = rng;
}

} // namespace synth
