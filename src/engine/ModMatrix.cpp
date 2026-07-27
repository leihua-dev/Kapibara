#include "ModMatrix.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace synth
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float frac01(float v) { return v - std::floor(v); }
} // namespace

void ModMatrix::prepare(double sr)
{
    sampleRate_ = std::max(1.0, sr);
    slotPhase_.fill(0.0f);
    slotValue_.fill(0.0f);
}

void ModMatrix::reset()
{
    slotPhase_.fill(0.0f);
    slotValue_.fill(0.0f);
    // Mask-group fans are global, not per-voice: a reset must put every lane
    // back on a common phase or the next note inherits the old scatter.
    for(auto &g : lanePhase_) g.fill(0.0);
    laneCount_.fill(0);
    chaosValue_ = 0.0f;
    chaosTarget_ = 0.0f;
    chaosCounter_ = 0;
}

void ModMatrix::bakeMaskLut(int slot, const ModSlotParams &p)
{
    auto &lut = maskLut_[(size_t)slot];
    for(int k = 0; k <= kMaskLutSize; ++k)
        lut[(size_t)k] = pointCurveEval(p.points.data(), p.pointCount, float(k) / float(kMaskLutSize));
}

void ModMatrix::setParams(const std::array<ModSlotParams, kMaxModSlots> &slots,
                          const std::array<MatrixRule, kMaxMatrixRules> &rules,
                          const ChaosParams &chaos,
                          const ShapeSourceParams &shape)
{
    // Called per control block on the audio thread: rebake a slot's mask LUT
    // only when its curve actually changed (a small memcmp per slot).
    for(int i = 0; i < kMaxModSlots; ++i)
    {
        const auto &np = slots[(size_t)i];
        const auto &op = slotParams_[(size_t)i];
        if(!maskLutReady_ || np.pointCount != op.pointCount
           || std::memcmp(np.points.data(), op.points.data(),
                          sizeof(MatrixEnvPoint) * size_t(kMaxMatrixEnvPoints)) != 0)
            bakeMaskLut(i, np);
    }
    maskLutReady_ = true;
    slotParams_ = slots;
    rules_ = rules;
    chaosParams_ = chaos;
    shapeParams_ = shape;
}

void ModMatrix::setModSlotParams(int idx, const ModSlotParams &p)
{
    if(idx >= 0 && idx < kMaxModSlots)
    {
        slotParams_[(size_t)idx] = p;
        bakeMaskLut(idx, p);
    }
}
ModSlotParams ModMatrix::getModSlotParams(int idx) const
{
    return (idx >= 0 && idx < kMaxModSlots) ? slotParams_[(size_t)idx] : ModSlotParams {};
}
void ModMatrix::setChaosParams(const ChaosParams &p) { chaosParams_ = p; }
ChaosParams ModMatrix::getChaosParams() const { return chaosParams_; }
void ModMatrix::setShapeSourceParams(const ShapeSourceParams &p) { shapeParams_ = p; }
ShapeSourceParams ModMatrix::getShapeSourceParams() const { return shapeParams_; }

void ModMatrix::setRule(int idx, const MatrixRule &r)
{
    if(idx >= 0 && idx < kMaxMatrixRules)
        rules_[(size_t)idx] = r;
}
MatrixRule ModMatrix::getRule(int idx) const
{
    return (idx >= 0 && idx < kMaxMatrixRules) ? rules_[(size_t)idx] : MatrixRule {};
}

void ModMatrix::advanceControl(int samples)
{
    // Mask-group fan: every lane accumulates its own phase at the CURRENT
    // spread rate and wraps mod 1 — spread/curve edits change only future
    // rates, so no lane ever jumps.
    for(int gi = 0; gi < kMaxMaskGroups; ++gi)
    {
        const auto &g = groups_[(size_t)gi];
        if(!g.enabled)
            continue;
        const double dt = double(std::clamp(g.rateHz, 0.0f, 100.0f))
                          * double(samples) / sampleRate_;
        // Lane count comes from the bank: 16 for the discrete slots, one lane per
        // element for a partial family. Latched here so evaluateForVoice cannot
        // read a different count than the one these phases were advanced with.
        const int lanes = std::clamp(waveBank_ ? waveBank_->group[(size_t)gi].lanes
                                               : kMaskGroupSlots,
                                     2, kMaskFanLanes);
        const int prevLanes = laneCount_[(size_t)gi];
        laneCount_[(size_t)gi] = lanes;
        auto &lp = lanePhase_[(size_t)gi];
        auto &lx = laneXb_[(size_t)gi];

        // The fan just changed width (family toggled, partial count edited...).
        // Lane k now sits at a different fan position, and lanes that were never
        // active still hold phase 0 — leaving them would split the fan into an
        // old half and a frozen half that never reconverge. Resample the old
        // fan's phases onto the new lane grid so the shape carries over.
        if(prevLanes >= 2 && prevLanes != lanes)
        {
            std::array<double, kMaskFanLanes> resampled {};
            for(int k = 0; k < lanes; ++k)
            {
                const int src = int(float(k) / float(lanes - 1) * float(prevLanes - 1) + 0.5f);
                resampled[(size_t)k] = lp[(size_t)std::clamp(src, 0, prevLanes - 1)];
            }
            for(int k = 0; k < lanes; ++k)
                lp[(size_t)k] = resampled[(size_t)k];
        }

        // With no rate spread the lanes have no reason to diverge — one LFO is
        // exactly what the preview draws and what the user asked for. Free
        // accumulators alone would keep whatever scatter an earlier spread drag
        // left behind forever, so pull them back onto lane 0. Smoothly: a snap
        // would click, and this way lowering FREQ SPRD visibly gathers the fan.
        if(std::abs(g.freqSpread) < 1.0e-4f)
        {
            constexpr double kRelock = 0.02;  // ~33 ms at a 32-sample control block
            const double ref = lp[0];
            for(int k = 1; k < lanes; ++k)
            {
                double d = lp[(size_t)k] - ref;
                d -= std::floor(d + 0.5);  // shortest signed distance around the circle
                const double p = lp[(size_t)k] - d * kRelock;
                lp[(size_t)k] = p - std::floor(p);
            }
        }
        // The bent fan positions depend only on the curve and the lane count, and
        // bend01 is a pow() — rebuild them when they actually change, not 64×4
        // times per control block.
        if(lanes != xbLanes_[(size_t)gi] || g.spreadCurve != xbCurve_[(size_t)gi])
        {
            for(int k = 0; k < lanes; ++k)
                lx[(size_t)k] = bend01(float(k) / float(lanes - 1), g.spreadCurve);
            xbLanes_[(size_t)gi] = lanes;
            xbCurve_[(size_t)gi] = g.spreadCurve;
        }
        for(int k = 0; k < lanes; ++k)
        {
            const double p = lp[(size_t)k]
                             + dt * (1.0 + double(lx[(size_t)k]) * double(g.freqSpread));
            lp[(size_t)k] = p - std::floor(p);
        }

        // Freeze this block's lane outputs. Everything laneValue reads is fixed
        // for the block, and evaluateForVoice would otherwise redo this work
        // once per partial per voice — up to millions of times a second on a
        // wide family fan, for at most 64 distinct answers.
        auto &lv = laneVal_[(size_t)gi];
        for(int k = 0; k < lanes; ++k)
            lv[(size_t)k] = laneValue(gi, g, k);
    }

    for(int i = 0; i < kMaxModSlots; ++i)
    {
        const auto &p = slotParams_[(size_t)i];
        if(!p.enabled) { slotValue_[(size_t)i] = 0.0f; continue; }
        const float inc = std::max(0.0f, p.rateHz) * float(samples) / float(sampleRate_);
        float ph = slotPhase_[(size_t)i] + inc;
        if(p.loop) ph -= std::floor(ph);
        else       ph = std::min(ph, 1.0f);
        slotPhase_[(size_t)i] = ph;
        slotValue_[(size_t)i] = pointCurveEval(p.points.data(), p.pointCount, ph) * 2.0f - 1.0f;
    }

    if(!chaosParams_.enabled)
    {
        chaosValue_ = 0.0f;
        chaosCounter_ = 0;
        return;
    }

    const int interval = std::max(1, int(sampleRate_ / std::max(0.01f, chaosParams_.frequencyHz)));
    chaosCounter_ += samples;
    if(chaosCounter_ >= interval)
    {
        chaosCounter_ %= interval;
        const float u = frac01(std::sin(12.9898f * (chaosTarget_ + float(chaosCounter_) + 0.123f)) * 43758.5453f);
        chaosTarget_ = 2.0f * u - 1.0f;
        if(chaosParams_.type == ChaosNoiseType::White)
            chaosValue_ = chaosTarget_;
        else if(chaosParams_.type == ChaosNoiseType::Crackle)
        {
            crackleState_ = frac01(crackleState_ * 1.997f + 0.217f + 0.07f * chaosTarget_);
            chaosValue_ = (crackleState_ > 0.72f ? 1.0f : -0.35f) * std::abs(chaosTarget_);
        }
    }
    if(chaosParams_.type == ChaosNoiseType::Smooth)
        chaosValue_ += 0.18f * (chaosTarget_ - chaosValue_);
    chaosValue_ = clampf(chaosValue_ * chaosParams_.amount, -1.0f, 1.0f);
}

float ModMatrix::bend01(float x, float c)
{
    c = clampf(c, -1.0f, 1.0f);
    if(std::abs(c) < 1.0e-4f)
        return x;
    return c >= 0.0f ? std::pow(x, 1.0f + c * 4.0f)
                     : 1.0f - std::pow(1.0f - x, 1.0f - c * 4.0f);
}

float ModMatrix::applyTransfer(const MatrixRule &r, float m)
{
    if(std::abs(r.transferCurve) < 1.0e-4f)
        return m;
    // Same bend family as the MOD-curve segments (ModCurve.h): drag up = convex
    // (fast onset), down = concave. Sign-magnitude keeps f(0) = 0.
    const float shaped = bend01(std::min(std::abs(m), 1.0f), r.transferCurve);
    return m < 0.0f ? -shaped : shaped;
}

void ModMatrix::setMaskGroups(const std::array<MaskGroup, kMaxMaskGroups> &groups)
{
    groups_ = groups;
}

float ModMatrix::weightFn(const MatrixRule &r, int i, const StaticSpectralFrame &frame)
{
    const float xi = frame.x[i];
    const uint8_t g = frame.mu[i];
    switch(r.weight)
    {
        case WeightMode::All:          return 1.0f;
        case WeightMode::LowPartials:  return 1.0f - xi;
        case WeightMode::HighPartials: return xi;
        case WeightMode::GroupLow:     return (g == 0) ? 1.0f : 0.0f;
        case WeightMode::GroupMid:     return (g == 1) ? 1.0f : 0.0f;
        case WeightMode::GroupHigh:    return (g == 2) ? 1.0f : 0.0f;
        case WeightMode::BandIndex:    return (i >= r.bandLo && i < r.bandHi) ? 1.0f : 0.0f;
    }
    return 1.0f;
}

float ModMatrix::shapeOutput(const ShapeSourceParams &p, float x)
{
    x = frac01(x + p.phase0);
    switch(p.shape)
    {
        case LfoShape::Asymmetric:
        {
            const float rho = clampf(p.rho, 0.001f, 0.999f);
            const float pu = std::max(0.05f, p.pUp);
            const float pd = std::max(0.05f, p.pDown);
            return x < rho
                       ? -1.0f + 2.0f * std::pow(x / rho, pu)
                       : 1.0f - 2.0f * std::pow((x - rho) / (1.0f - rho), pd);
        }
        case LfoShape::Sine:      return std::sin(kTwoPi * x);
        case LfoShape::Square:    return x < 0.5f ? 1.0f : -1.0f;
        case LfoShape::Triangle:  return x < 0.5f ? (4.0f * x - 1.0f) : (3.0f - 4.0f * x);
        case LfoShape::SampleHold:
        {
            const float bucket = std::floor(x * 24.0f);
            const float u = frac01(std::sin(12.9898f * (bucket + p.phase0)) * 43758.5453f);
            return 2.0f * u - 1.0f;
        }
    }
    return 0.0f;
}

void ModMatrix::evaluateForVoice(MatrixVoiceOutput &out,
                                 const StaticSpectralFrame &frame,
                                 float velocity,
                                 float keyTrack01,
                                 float adsrLevel,
                                 const std::array<float, kMaxModSlots> &slotLevels,
                                 float randPerVoice,
                                 const uint32_t *trackIds,
                                 const int *trackBegin,
                                 const int *trackEnd,
                                 int trackCount,
                                 const std::array<float, kMaxAmpEnvs> *ampEnvLevels) const
{
    initMatrixOutput(out);

    float minNu = 1e9f, maxNu = 1e-9f;
    for(int i = 0; i < frame.partialCount; ++i)
    {
        if(frame.nu[i] > 0.0f)
        {
            minNu = std::min(minNu, frame.nu[i]);
            maxNu = std::max(maxNu, frame.nu[i]);
        }
    }
    const float logMin = std::log(std::max(1e-6f, minNu));
    const float logMax = std::log(std::max(1e-6f, maxNu));

    auto sourceValue = [&](ModSource s, int partialIndex) -> float {
        if(s >= ModSource::Lfo1 && s <= ModSource::Lfo4)
        {
            const int idx = (int)s - (int)ModSource::Lfo1;
            return slotLevels[(size_t)idx];
        }
        if(s >= ModSource::Env1 && s <= ModSource::Env4)
        {
            const int idx = (int)s - (int)ModSource::Env1;
            return slotLevels[(size_t)(kMaxLfos + idx)];
        }
        if(s >= ModSource::Adsr1 && s <= ModSource::Adsr4)
        {
            const int idx = (int)s - (int)ModSource::Adsr1;
            return ampEnvLevels != nullptr ? clampf((*ampEnvLevels)[(size_t)idx], 0.0f, 1.0f) : 0.0f;
        }
        switch(s)
        {
            case ModSource::Velocity: return velocity * 2.0f - 1.0f;
            case ModSource::KeyTrack: return keyTrack01 * 2.0f - 1.0f;
            case ModSource::Random:   return randPerVoice * 2.0f - 1.0f;
            case ModSource::Adsr:     return clampf(adsrLevel, 0.0f, 1.0f);
            case ModSource::GeneratorSelf:
            {
                const float ln = std::log(std::max(1e-6f, frame.nu[partialIndex]));
                const float norm = (logMax > logMin + 1e-6f) ? (ln - logMin) / (logMax - logMin) : 0.0f;
                return norm * 2.0f - 1.0f;
            }
            case ModSource::Chaos: return chaosValue_;
            case ModSource::Shape:
            {
                const float axis = shapeParams_.useSpectralX
                                       ? frame.x[partialIndex]
                                       : (frame.partialCount > 1
                                              ? float(partialIndex) / float(frame.partialCount - 1)
                                              : 0.0f);
                return shapeOutput(shapeParams_, axis);
            }
            case ModSource::Unit: return 1.0f;
            case ModSource::None: return 0.0f;
            default: return 0.0f;
        }
    };

    const int N = frame.partialCount;

    // Destination application shared by rules and mask groups.
    const auto applyDest = [&out](ModDestination dest, int i, float contrib) {
        switch(dest)
        {
            case ModDestination::Amp:
                out.mAmp[i] *= clampf(1.0f + contrib, 0.0f, 32.0f); break;
            case ModDestination::Freq:
                out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -12.0f, 12.0f)); break;
            case ModDestination::Phase:
                out.dPhase[i] += contrib * kPi; break;
            case ModDestination::TrackGain:
                out.mAmp[i] *= clampf(1.0f + contrib, 0.0f, 4.0f); break;
            case ModDestination::TrackPan:
            case ModDestination::MetaPan:
                out.dPan[i] += contrib; break;
            case ModDestination::PitchOct:
                out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -4.0f, 4.0f)); break;
            case ModDestination::PitchSem:
                out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -48.0f, 48.0f) / 12.0f); break;
            case ModDestination::PitchFine:
            case ModDestination::PitchCrs:
                out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -400.0f, 400.0f) / 1200.0f); break;
            case ModDestination::MetaMorph:
                out.dMorph[i] += contrib; break;
            case ModDestination::MetaWarp:
                out.dWarp[i] += contrib; break;
            default: break;
        }
    };
    // Resolve a target track id to its flattened partial range (0 = all).
    const auto resolveRange = [&](uint32_t targetTrackId, int &begin, int &end) -> bool {
        begin = 0;
        end = N;
        if(targetTrackId == 0)
            return begin < end;
        begin = end = 0;
        for(int t = 0; t < trackCount; ++t)
            if(trackIds != nullptr && trackIds[t] == targetTrackId)
            {
                begin = std::clamp(trackBegin != nullptr ? trackBegin[t] : 0, 0, N);
                end = std::clamp(trackEnd != nullptr ? trackEnd[t] : N, begin, N);
                break;
            }
        return begin < end;
    };

    // Per-partial "which source track" axis, normalized 0..1 over the track list.
    // Built lazily — only rules with a TRACK-axis mask pay for it (one fill per
    // voice per control block).
    std::array<float, kMaxPartials> trackAxis {};
    bool trackAxisBuilt = false;
    const auto buildTrackAxis = [&]() {
        if(trackAxisBuilt) return;
        trackAxisBuilt = true;
        std::fill(trackAxis.begin(), trackAxis.begin() + N, 0.0f);
        if(trackBegin != nullptr && trackEnd != nullptr && trackCount > 1)
            for(int t = 0; t < trackCount; ++t)
            {
                const int b = std::clamp(trackBegin[t], 0, N);
                const int e = std::clamp(trackEnd[t], b, N);
                const float v = float(t) / float(trackCount - 1);
                for(int i = b; i < e; ++i) trackAxis[(size_t)i] = v;
            }
    };

    for(int r = 0; r < kMaxMatrixRules; ++r)
    {
        const auto &rule = rules_[(size_t)r];
        if(!rule.enabled || rule.muted || rule.source == ModSource::None
           || std::abs(rule.depth) < 1e-6f)
            continue;
        if(insertModParamForDest(rule.dest) >= 0)
            continue;
        if(rule.dest == ModDestination::OscModDepth)
        {
            // Track-scoped, not per-partial: the rack's own cross-unit modulation
            // depth. An untargeted rule would have to mean "every rack", which is
            // never what a user means here, so it is simply skipped.
            if(rule.targetTrackId == 0 || trackIds == nullptr)
                continue;
            for(int t = 0; t < trackCount && t < kMaxSourceTracks; ++t)
                if(trackIds[t] == rule.targetTrackId)
                {
                    out.dOscMod[(size_t)t] += rule.depth
                                              * applyTransfer(rule, sourceValue(rule.source, 0));
                    break;
                }
            continue;
        }
        int begin, end;
        if(!resolveRange(rule.targetTrackId, begin, end))
            continue;
        // Response bend: most sources are partial-invariant, so shape once and
        // hoist; only GeneratorSelf/Shape vary with the partial index.
        const bool perPartialSrc = rule.source == ModSource::GeneratorSelf
                                   || rule.source == ModSource::Shape;
        const float mHoisted = perPartialSrc ? 0.0f
                                             : applyTransfer(rule, sourceValue(rule.source, begin));
        for(int i = begin; i < end; ++i)
        {
            const float m = perPartialSrc ? applyTransfer(rule, sourceValue(rule.source, i))
                                          : mHoisted;
            float w = weightFn(rule, i, frame);
            // Spatial mask: the mask slot's CURVE (pre-baked LUT — one lerp per
            // partial, no breakpoint scan) sampled over a countable axis scales
            // the weight — a drawn distribution across simultaneous elements.
            if(rule.maskSlot >= 0 && rule.maskSlot < kMaxModSlots && w > 0.0f)
            {
                float ax;
                switch(rule.maskAxis)
                {
                    case 1: // spectral x
                        ax = clampf(frame.x[i], 0.0f, 1.0f);
                        break;
                    case 2: // normalized log-frequency
                    {
                        const float ln = std::log(std::max(1e-6f, frame.nu[i]));
                        ax = (logMax > logMin + 1e-6f) ? (ln - logMin) / (logMax - logMin) : 0.0f;
                        break;
                    }
                    case 3: // partial index, scrolled by the mask slot's own phase
                    {
                        const float base = end - begin > 1 ? float(i - begin) / float(end - begin - 1) : 0.0f;
                        ax = frac01(base + slotPhase_[(size_t)rule.maskSlot]);
                        break;
                    }
                    case 4: // source-track index: the curve distributes across the
                            // SOURCE rack (all partials of one track share a weight)
                        buildTrackAxis();
                        ax = trackAxis[(size_t)i];
                        break;
                    default: // partial index within the rule's target range
                        ax = end - begin > 1 ? float(i - begin) / float(end - begin - 1) : 0.0f;
                        break;
                }
                w *= maskLookup(rule.maskSlot, ax);
            }
            applyDest(rule.dest, i, rule.depth * m * w);
        }
    }

    // ---- Mask groups: one lane of the fanned base LFO per target -------------
    for(int gi = 0; gi < kMaxMaskGroups; ++gi)
    {
        const auto &g = groups_[(size_t)gi];
        if(!g.enabled)
            continue;
        if(g.family)
        {
            // Per-partial fan: every partial in the family range gets its own
            // successively offset lane.
            if(std::abs(g.familyDepth) < 1e-6f)
                continue;
            int begin, end;
            if(!resolveRange(g.familyTrackId, begin, end))
                continue;
            for(int i = begin; i < end; ++i)
            {
                const float x = end - begin > 1 ? float(i - begin) / float(end - begin - 1) : 0.0f;
                applyDest(g.familyDest, i, g.familyDepth * maskGroupLane(gi, x));
            }
        }
        else
        {
            // 16 discrete lanes, one per configured slot.
            for(int k = 0; k < kMaskGroupSlots; ++k)
            {
                const auto &t = g.targets[(size_t)k];
                if(!t.enabled || std::abs(t.depth) < 1e-6f)
                    continue;
                if(insertModParamForDest(t.dest) >= 0)
                    continue;  // per-partial path only
                int begin, end;
                if(!resolveRange(t.targetTrackId, begin, end))
                    continue;
                const float v = t.depth * maskGroupLane(gi, float(k) / float(kMaskGroupSlots - 1));
                for(int i = begin; i < end; ++i)
                    applyDest(t.dest, i, v);
            }
        }
    }
}

float ModMatrix::globalModSource(ModSource s, float adsrRep,
                                 const std::array<float, kMaxModSlots> &slotRep,
                                 const std::array<float, kMaxAmpEnvs> &ampRep) const
{
    if(s >= ModSource::Lfo1 && s <= ModSource::Lfo4)
        return slotRep[(size_t)((int)s - (int)ModSource::Lfo1)];
    if(s >= ModSource::Env1 && s <= ModSource::Env4)
        return slotRep[(size_t)(kMaxLfos + (int)s - (int)ModSource::Env1)];
    if(s >= ModSource::Adsr1 && s <= ModSource::Adsr4)
        return clampf(ampRep[(size_t)((int)s - (int)ModSource::Adsr1)], 0.0f, 1.0f);
    switch(s)
    {
        case ModSource::Adsr:  return clampf(adsrRep, 0.0f, 1.0f);
        case ModSource::Chaos: return chaosValue_;
        case ModSource::Shape: return shapeOutput(shapeParams_, 0.5f);
        case ModSource::Unit:  return 1.0f;
        default:               return 0.0f;
    }
}

} // namespace synth
