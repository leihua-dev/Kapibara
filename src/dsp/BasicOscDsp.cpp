#include "Generators.h"

#include <algorithm>
#include <cmath>

namespace synth
{

namespace
{
constexpr float kPiF = 3.14159265358979323846f;
}

int basicOscPartials(const BasicOscUnit &unit, int budget, float *ratio, float *amp, float *phase)
{
    int n = 0;
    const auto put = [&](float r, float a, float ph) {
        if(n >= budget)
            return;
        ratio[n] = r;
        amp[n] = a;
        phase[n] = ph;
        ++n;
    };
    switch(unit.shape)
    {
        case BasicOscillatorShape::Sine:
            put(1.0f, 1.0f, 0.0f);
            break;
        case BasicOscillatorShape::Sub:
            put(0.5f, 1.0f, 0.0f);
            put(1.0f, std::clamp(unit.subLevel, 0.0f, 1.0f), 0.0f);
            break;
        case BasicOscillatorShape::Saw:
            for(int i = 0; i < 32 && n < budget; ++i)
                put(float(i + 1), 1.0f / float(i + 1), 0.0f);
            break;
        case BasicOscillatorShape::Triangle:
            // Odd harmonics only, packed contiguously. They used to be written to
            // every OTHER slot, which left the slots between them at the default
            // ratio 1 / amp 1 — 15 extra full-level fundamentals stacked on the
            // triangle, so it did not sound like one.
            for(int k = 0; n < budget; ++k)
            {
                const int h = 2 * k + 1;
                if(h > 31)
                    break;
                const bool flip = (k & 1) != 0;
                put(float(h), 1.0f / float(h * h), flip ? kPiF : 0.0f);
            }
            break;
        case BasicOscillatorShape::Pulse:
        {
            const float duty = std::clamp(unit.pulseWidth, 0.05f, 0.95f);
            for(int i = 0; i < 32 && n < budget; ++i)
            {
                const float h = float(i + 1);
                put(h, std::abs(std::sin(kPiF * h * duty) / h), 0.0f);
            }
            break;
        }
    }
    return n;
}

void buildBasicSeed(const SourceTrackParams &track, WavetableSeedParams &seed, int *unitCount)
{
    if(unitCount != nullptr)
        for(int i = 0; i < kBasicOscUnits; ++i)
            unitCount[i] = 0;
    seed = WavetableSeedParams {};
    // The default seed fills every slot with a harmonic at amp 1/n; silence them
    // all first so a shape only occupies the slots it actually writes.
    for(auto &p : seed.partials)
    {
        p.enabled = false;
        p.ratio = 1.0f;
        p.amp = 0.0f;
        p.phase = 0.0f;
    }

    int active = 0;
    for(const auto &u : track.basicUnits)
        if(u.enabled)
            ++active;
    const bool fallbackToFirst = active == 0;  // never render silence
    if(fallbackToFirst)
        active = 1;
    // Share the track's partial budget between the active units.
    const int budget = std::max(1, kMaxWavetablePartials / active);

    float ratio[kMaxWavetablePartials], amp[kMaxWavetablePartials], phase[kMaxWavetablePartials];
    int write = 0;
    for(int ui = 0; ui < kBasicOscUnits; ++ui)
    {
        const auto &u = track.basicUnits[(size_t)ui];
        if(!u.enabled && !(fallbackToFirst && ui == 0))
            continue;
        const int room = std::min(budget, kMaxWavetablePartials - write);
        if(room <= 0)
            break;
        const int n = basicOscPartials(u, room, ratio, amp, phase);
        // Pitch scales the whole series, so the shape is preserved and only its
        // fundamental moves.
        const float semis = float(u.pitchOct) * 12.0f + float(u.pitchSem)
                            + u.pitchFin / 100.0f + u.pitchCrs / 100.0f;
        const float pitchRatio = std::pow(2.0f, semis / 12.0f);
        const float level = std::clamp(u.level, 0.0f, 1.0f);
        if(unitCount != nullptr)
            unitCount[ui] = n;
        for(int i = 0; i < n; ++i)
        {
            auto &p = seed.partials[(size_t)write++];
            p.enabled = true;
            p.ratio = ratio[i] * pitchRatio;
            p.amp = amp[i] * level;
            p.phase = phase[i];
        }
    }
    seed.partialCount = std::max(1, write);
}

} // namespace synth
