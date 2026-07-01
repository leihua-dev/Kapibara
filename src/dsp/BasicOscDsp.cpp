#include "Generators.h"

#include <algorithm>
#include <cmath>

namespace synth
{

void buildBasicSeed(const SourceTrackParams &track, WavetableSeedParams &seed)
{
    seed = WavetableSeedParams {};
    const auto setPartial = [&](int i, float ratio, float amp) {
        if(i < 0 || i >= kMaxWavetablePartials)
            return;
        auto &p = seed.partials[(size_t)i];
        p.enabled = true;
        p.ratio = ratio;
        p.amp = amp;
        p.phase = 0.0f;
    };

    switch(track.basicShape)
    {
        case BasicOscillatorShape::Sine:
            seed.partialCount = 1;
            setPartial(0, 1.0f, 1.0f);
            break;
        case BasicOscillatorShape::Sub:
            seed.partialCount = 2;
            setPartial(0, 0.5f, 1.0f);
            setPartial(1, 1.0f, std::clamp(track.subLevel, 0.0f, 1.0f));
            break;
        case BasicOscillatorShape::Saw:
            seed.partialCount = 32;
            for(int i = 0; i < seed.partialCount; ++i)
                setPartial(i, float(i + 1), 1.0f / float(i + 1));
            break;
        case BasicOscillatorShape::Triangle:
            seed.partialCount = 31;
            for(int i = 0; i < seed.partialCount; i += 2)
            {
                const int n = i + 1;
                const float sign = ((n - 1) / 2) & 1 ? -1.0f : 1.0f;
                setPartial(i, float(n), std::abs(sign / float(n * n)));
                seed.partials[(size_t)i].phase = sign < 0.0f ? 3.14159265358979323846f : 0.0f;
            }
            break;
        case BasicOscillatorShape::Pulse:
            seed.partialCount = 32;
            for(int i = 0; i < seed.partialCount; ++i)
            {
                const float n = float(i + 1);
                const float duty = std::clamp(track.pulseWidth, 0.05f, 0.95f);
                setPartial(i, n, std::abs(std::sin(3.14159265358979323846f * n * duty) / n));
            }
            break;
    }
}

} // namespace synth
