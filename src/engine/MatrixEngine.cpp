#include "MatrixEngine.h"

#include <algorithm>
#include <cmath>

namespace synth
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

// Cheap lcg used per-voice for Random source so different voices spread.
inline float frac01(float v) { return v - std::floor(v); }
} // namespace

// -----------------------------------------------------------------------------
// Envelope shape library (§3.5)
// -----------------------------------------------------------------------------
float envCurveEval(EnvCurve mode, float tau, float eta)
{
    tau = clampf(tau, 0.0f, 1.0f);
    eta = std::max(0.05f, eta);
    switch(mode)
    {
        case EnvCurve::Exp:
        {
            const float denom = 1.0f - std::exp(-eta);
            return (1.0f - std::exp(-eta * tau)) / std::max(1e-6f, denom);
        }
        case EnvCurve::Power:
            return std::pow(tau, eta);
        case EnvCurve::Sigmoid:
        {
            const float a = sigmoid(eta * (tau - 0.5f));
            const float lo = sigmoid(-eta * 0.5f);
            const float hi = sigmoid(eta * 0.5f);
            return (a - lo) / std::max(1e-6f, (hi - lo));
        }
    }
    return tau;
}

float adsrCurveEval(float tau, float curve)
{
    tau = clampf(tau, 0.0f, 1.0f);
    curve = clampf(curve, 0.0f, 1.0f);
    const float exponent = std::pow(2.0f, (0.5f - curve) * 6.0f);
    return std::pow(tau, std::max(0.05f, exponent));
}

// -----------------------------------------------------------------------------
// Lfo
// -----------------------------------------------------------------------------
void Lfo::prepare(double sr)
{
    sampleRate_ = sr > 1.0 ? sr : 48000.0;
    xi_ = 0.0f;
    shCounter_ = 0;
    lastSh_ = 0.0f;
}
void Lfo::reset(float phase) { xi_ = frac01(phase); shCounter_ = 0; }

float Lfo::shapeOutput(const LfoParams &p, float xi)
{
    if(p.usePoints)
        return pointCurveEval(p.points.data(), p.pointCount, xi) * 2.0f - 1.0f; // [0,1] -> [-1,1]
    switch(p.shape)
    {
        case LfoShape::Asymmetric:
        {
            const float rho = clampf(p.rhoLfo, 0.001f, 0.999f);
            const float pu = std::max(0.05f, p.pUp);
            const float pd = std::max(0.05f, p.pDown);
            if(xi < rho)
                return -1.0f + 2.0f * std::pow(xi / rho, pu);
            else
                return 1.0f - 2.0f * std::pow((xi - rho) / (1.0f - rho), pd);
        }
        case LfoShape::Sine:     return std::sin(kTwoPi * xi);
        case LfoShape::Square:   return xi < 0.5f ? 1.0f : -1.0f;
        case LfoShape::Triangle: return xi < 0.5f ? (4.0f * xi - 1.0f) : (3.0f - 4.0f * xi);
        case LfoShape::SampleHold:
            return 0.0f; // handled in tick()
    }
    return 0.0f;
}

float Lfo::tick(const LfoParams &p, int samples)
{
    const float dphi = float(p.frequencyHz * double(samples) / sampleRate_);
    xi_ = frac01(xi_ + dphi);

    if(!p.usePoints && p.shape == LfoShape::SampleHold)
    {
        const int interval = std::max(1, int(sampleRate_ / std::max(0.001f, p.frequencyHz)));
        shCounter_ += samples;
        if(shCounter_ >= interval)
        {
            shCounter_ %= interval;
            // Cheap deterministic-ish hash on xi
            float u = std::sin(12.9898f * (xi_ + p.phase0)) * 43758.5453f;
            lastSh_ = (frac01(u) * 2.0f - 1.0f);
        }
        return lastSh_;
    }
    return shapeOutput(p, frac01(xi_ + p.phase0));
}

// -----------------------------------------------------------------------------
// MatrixEngine
// -----------------------------------------------------------------------------
void MatrixEngine::prepare(double sr)
{
    sampleRate_ = std::max(1.0, sr);
    for(auto &l : lfos_)
        l.prepare(sr);
    lfoLastValue_.fill(0.0f);
}
void MatrixEngine::reset()
{
    for(auto &l : lfos_)
        l.reset();
    lfoLastValue_.fill(0.0f);
    chaosValue_ = 0.0f;
    chaosTarget_ = 0.0f;
    chaosCounter_ = 0;
}

void MatrixEngine::setParams(const std::array<LfoParams, kMaxLfos> &lfos,
                             const std::array<MatrixEnvParams, kMaxModEnvs> &envs,
                             const std::array<MatrixRule, kMaxMatrixRules> &rules,
                             const ChaosParams &chaos,
                             const ShapeSourceParams &shape)
{
    lfoParams_ = lfos;
    envParams_ = envs;
    rules_ = rules;
    chaosParams_ = chaos;
    shapeParams_ = shape;
}

void MatrixEngine::setLfoParams(int idx, const LfoParams &p)
{
    if(idx >= 0 && idx < kMaxLfos)
        lfoParams_[(size_t)idx] = p;
}
LfoParams MatrixEngine::getLfoParams(int idx) const
{
    return (idx >= 0 && idx < kMaxLfos) ? lfoParams_[(size_t)idx] : LfoParams {};
}
void MatrixEngine::setEnvParams(int idx, const MatrixEnvParams &p)
{
    if(idx >= 0 && idx < kMaxModEnvs)
        envParams_[(size_t)idx] = p;
}
MatrixEnvParams MatrixEngine::getEnvParams(int idx) const
{
    return (idx >= 0 && idx < kMaxModEnvs) ? envParams_[(size_t)idx] : MatrixEnvParams {};
}
void MatrixEngine::setChaosParams(const ChaosParams &p) { chaosParams_ = p; }
ChaosParams MatrixEngine::getChaosParams() const { return chaosParams_; }
void MatrixEngine::setShapeSourceParams(const ShapeSourceParams &p) { shapeParams_ = p; }
ShapeSourceParams MatrixEngine::getShapeSourceParams() const { return shapeParams_; }
void MatrixEngine::setRule(int idx, const MatrixRule &r)
{
    if(idx >= 0 && idx < kMaxMatrixRules)
        rules_[(size_t)idx] = r;
}
MatrixRule MatrixEngine::getRule(int idx) const
{
    return (idx >= 0 && idx < kMaxMatrixRules) ? rules_[(size_t)idx] : MatrixRule {};
}

void MatrixEngine::advanceControl(int samples)
{
    for(int i = 0; i < kMaxLfos; ++i)
    {
        // LFOs are always running (no enable gate); they are cheap and the UI
        // no longer exposes an on/off toggle.
        lfoLastValue_[(size_t)i] = lfos_[(size_t)i].tick(lfoParams_[(size_t)i], samples);
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

float MatrixEngine::weightFn(const MatrixRule &r, int i, const StaticSpectralFrame &frame)
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

float MatrixEngine::shapeOutput(const ShapeSourceParams &p, float x)
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
        case LfoShape::Sine:     return std::sin(kTwoPi * x);
        case LfoShape::Square:   return x < 0.5f ? 1.0f : -1.0f;
        case LfoShape::Triangle: return x < 0.5f ? (4.0f * x - 1.0f) : (3.0f - 4.0f * x);
        case LfoShape::SampleHold:
        {
            const float bucket = std::floor(x * 24.0f);
            const float u = frac01(std::sin(12.9898f * (bucket + p.phase0)) * 43758.5453f);
            return 2.0f * u - 1.0f;
        }
    }
    return 0.0f;
}

void MatrixEngine::evaluateForVoice(MatrixVoiceOutput &out,
                                    const StaticSpectralFrame &frame,
                                    float velocity,
                                    float keyTrack01,
                                    float adsrLevel,
                                    const std::array<float, kMaxModEnvs> &envLevels,
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
            return lfoLastValue_[(size_t)((int)s - (int)ModSource::Lfo1)];
        if(s >= ModSource::Env1 && s <= ModSource::Env4)
        {
            const int idx = (int)s - (int)ModSource::Env1;
            return clampf(envLevels[(size_t)idx], 0.0f, 1.0f); // ENVs always active
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
            case ModSource::Chaos:    return chaosValue_;
            case ModSource::Shape:
            {
                const float axis = shapeParams_.useSpectralX
                                       ? frame.x[partialIndex]
                                       : (frame.partialCount > 1
                                              ? float(partialIndex) / float(frame.partialCount - 1)
                                              : 0.0f);
                return shapeOutput(shapeParams_, axis);
            }
            case ModSource::None:     return 0.0f;
            default: return 0.0f;
        }
    };

    const int N = frame.partialCount;
    for(int r = 0; r < kMaxMatrixRules; ++r)
    {
        const auto &rule = rules_[(size_t)r];
        if(!rule.enabled || rule.source == ModSource::None || std::abs(rule.depth) < 1e-6f)
            continue;
        // Insert-parameter destinations are handled globally (per-strip) by SynthCore.
        if(insertModParamForDest(rule.dest) >= 0)
            continue;
        int begin = 0;
        int end = N;
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
            const float w = weightFn(rule, i, frame);
            const float contrib = rule.depth * m * w;
            switch(rule.dest)
            {
                case ModDestination::Amp:
                    out.mAmp[i] *= clampf(1.0f + contrib, 0.0f, 32.0f);
                    break;
                case ModDestination::Freq:
                    out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -12.0f, 12.0f));
                    break;
                case ModDestination::Phase:
                    out.dPhase[i] += contrib * kPi; // depth=1 -> +/- pi swing
                    break;
                case ModDestination::DecayTime:
                    break;
                case ModDestination::SpectralDecay:
                    break;
                case ModDestination::TrackGain:
                    out.mAmp[i] *= clampf(1.0f + contrib, 0.0f, 4.0f);
                    break;
                case ModDestination::TrackPan:
                case ModDestination::MetaPan:
                    out.dPan[i] += contrib;
                    break;
                case ModDestination::PitchOct:
                    out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -4.0f, 4.0f));
                    break;
                case ModDestination::PitchSem:
                    out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -48.0f, 48.0f) / 12.0f);
                    break;
                case ModDestination::PitchFine:
                case ModDestination::PitchCrs:
                    out.mFreq[i] *= std::pow(2.0f, clampf(contrib, -400.0f, 400.0f) / 1200.0f);
                    break;
                case ModDestination::MetaMorph:
                    out.dMorph[i] += contrib;
                    break;
                case ModDestination::MetaWarp:
                    out.dWarp[i] += contrib;
                    break;
                default: // effect-parameter destinations handled in evaluateEffectMod
                    break;
            }
        }
    }
}

float MatrixEngine::globalModSource(ModSource s, float adsrRep, const std::array<float, kMaxModEnvs> &envRep,
                                    const std::array<float, kMaxAmpEnvs> &ampRep) const
{
    if(s >= ModSource::Lfo1 && s <= ModSource::Lfo4)
        return lfoLastValue_[(size_t)((int)s - (int)ModSource::Lfo1)];
    if(s >= ModSource::Env1 && s <= ModSource::Env4)
    {
        const int idx = (int)s - (int)ModSource::Env1;
        return clampf(envRep[(size_t)idx], 0.0f, 1.0f); // ENVs always active
    }
    if(s >= ModSource::Adsr1 && s <= ModSource::Adsr4)
        return clampf(ampRep[(size_t)((int)s - (int)ModSource::Adsr1)], 0.0f, 1.0f);
    switch(s)
    {
        case ModSource::Adsr:  return clampf(adsrRep, 0.0f, 1.0f);
        case ModSource::Chaos: return chaosValue_;
        case ModSource::Shape: return shapeOutput(shapeParams_, 0.5f);
        default:               return 0.0f; // per-voice-only sources are n/a for per-strip FX
    }
}

} // namespace synth
