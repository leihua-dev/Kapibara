#include "Generators.h"

#include <cmath>

namespace synth
{

void buildNoiseSeed(const SourceTrackParams &track, WavetableSeedParams &seed)
{
    seed = WavetableSeedParams {};
    seed.partialCount = 48;
    const float color = std::clamp(track.noiseColor, 0.0f, 1.0f);
    for(int i = 0; i < seed.partialCount; ++i)
    {
        auto &p = seed.partials[(size_t)i];
        const float n = float(i + 1);
        p.enabled = true;
        p.ratio = n * (1.0f + 0.013f * float((i * 37) % 11));
        p.amp = std::pow(n, -color);
        p.phase = std::fmod(float(i * 97), 360.0f) * 0.01745329252f;
        p.pan = ((i & 1) ? 0.35f : -0.35f);
    }
}

} // namespace synth
