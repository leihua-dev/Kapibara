#include "ModMatrix.h"

#include <algorithm>
#include <cmath>

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
    chaosValue_ = 0.0f;
    chaosTarget_ = 0.0f;
    chaosCounter_ = 0;
}

void ModMatrix::setParams(const std::array<ModSlotParams, kMaxModSlots> &slots,
                          const std::array<MatrixRule, kMaxMatrixRules> &rules,
                          const ChaosParams &chaos,
                          const ShapeSourceParams &shape)
{
    slotParams_ = slots;
    rules_ = rules;
    chaosParams_ = chaos;
    shapeParams_ = shape;
}

void ModMatrix::setModSlotParams(int idx, const ModSlotParams &p)
{
    if(idx >= 0 && idx < kMaxModSlots)
        slotParams_[(size_t)idx] = p;
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
    for(int r = 0; r < kMaxMatrixRules; ++r)
    {
        const auto &rule = rules_[(size_t)r];
        if(!rule.enabled || rule.source == ModSource::None || std::abs(rule.depth) < 1e-6f)
            continue;
        if(insertModParamForDest(rule.dest) >= 0)
            continue;
        int begin = 0, end = N;
        if(rule.targetTrackId != 0)
        {
            begin = end = 0;
            for(int t = 0; t < trackCount; ++t)
            {
                if(trackIds != nullptr && trackIds[t] == rule.targetTrackId)
                {
                    begin = std::clamp(trackBegin != nullptr ? trackBegin[t] : 0, 0, N);
                    end = std::clamp(trackEnd != nullptr ? trackEnd[t] : N, begin, N);
                    break;
                }
            }
            if(begin >= end)
                continue;
        }
        for(int i = begin; i < end; ++i)
        {
            const float m = sourceValue(rule.source, i);
            float w = weightFn(rule, i, frame);
            // Spatial mask: the mask slot's CURVE sampled over a countable axis
            // (partial index within the rule's range, or spectral x) scales the
            // weight — a drawn distribution across simultaneous elements.
            if(rule.maskSlot >= 0 && rule.maskSlot < kMaxModSlots && w > 0.0f)
            {
                const auto &mp = slotParams_[(size_t)rule.maskSlot];
                const float ax = rule.maskAxis == 1
                                     ? clampf(frame.x[i], 0.0f, 1.0f)
                                     : (end - begin > 1 ? float(i - begin) / float(end - begin - 1) : 0.0f);
                w *= pointCurveEval(mp.points.data(), mp.pointCount, ax);
            }
            const float contrib = rule.depth * m * w;
            switch(rule.dest)
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
