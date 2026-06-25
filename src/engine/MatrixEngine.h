#pragma once

#include "model/SpectralFrame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace synth
{

// Architecture §3 Matrix / Performance Mapping.
// Provides:
//   - 4 LFOs with the asymmetric shape from §3.8 (xi, rho_lfo, p_u, p_d).
//   - 4 per-voice scalar ENV sources.
//   - Envelope shape library §3.5 (Exp / Power / Sigmoid).
//   - Up to 16 matrix rules R_k = (m_k, d_k, alpha_k, T_k, W_k).
//   - Per-block output of M_i^amp[i], M_i^freq[i], Delta_phi_i[i].

// -----------------------------------------------------------------------------
// LFO
// -----------------------------------------------------------------------------
enum class LfoShape : uint8_t
{
    Asymmetric = 0, // §3.8 power-shaped saw
    Sine = 1,
    Square = 2,
    Triangle = 3,
    SampleHold = 4
};

// Breakpoint point shared by Matrix ENVs and custom (point-curve) LFOs.
static constexpr int kMaxMatrixEnvPoints = 16;

struct MatrixEnvPoint
{
    float x = 0.0f;
    float y = 0.0f;
    float curve = 0.0f;

    constexpr MatrixEnvPoint() = default;
    constexpr MatrixEnvPoint(float xIn, float yIn, float curveIn)
        : x(xIn), y(yIn), curve(curveIn)
    {
    }
};

inline float matrixEnvSegmentValue(const MatrixEnvPoint &a, const MatrixEnvPoint &b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float curve = std::clamp(a.curve, -1.0f, 1.0f);
    const float shaped = curve >= 0.0f ? std::pow(t, 1.0f + curve * 4.0f)
                                       : 1.0f - std::pow(1.0f - t, 1.0f - curve * 4.0f);
    return a.y + (b.y - a.y) * shaped;
}

// Evaluate a breakpoint curve (sorted by x) at x in [0,1] -> [0,1].
inline float pointCurveEval(const MatrixEnvPoint *points, int pointCount, float x)
{
    const int count = std::clamp(pointCount, 2, kMaxMatrixEnvPoints);
    x = std::clamp(x, 0.0f, 1.0f);
    if(x <= points[0].x)
        return std::clamp(points[0].y, 0.0f, 1.0f);
    for(int i = 0; i + 1 < count; ++i)
    {
        const auto &a = points[(size_t)i];
        const auto &b = points[(size_t)i + 1];
        if(x <= b.x || i + 2 == count)
        {
            const float span = std::max(0.0001f, b.x - a.x);
            return std::clamp(matrixEnvSegmentValue(a, b, (x - a.x) / span), 0.0f, 1.0f);
        }
    }
    return std::clamp(points[(size_t)count - 1].y, 0.0f, 1.0f);
}

struct LfoParams
{
    bool enabled = false;
    LfoShape shape = LfoShape::Asymmetric;
    float frequencyHz = 1.0f;
    float phase0 = 0.0f;     // phi_0 in [0, 1)
    float rhoLfo = 0.5f;     // rho_lfo asymmetry in (0, 1)
    float pUp = 1.0f;        // p_u
    float pDown = 1.0f;      // p_d
    // Custom point-curve mode: when usePoints is set the LFO reads this
    // breakpoint curve as its looping waveform instead of `shape`.
    bool usePoints = false;
    int pointCount = 4;
    std::array<MatrixEnvPoint, kMaxMatrixEnvPoints> points {
        MatrixEnvPoint { 0.0f, 0.5f, 0.0f },
        MatrixEnvPoint { 0.25f, 1.0f, 0.0f },
        MatrixEnvPoint { 0.5f, 0.5f, 0.0f },
        MatrixEnvPoint { 1.0f, 0.5f, 0.0f }
    };
};

enum class ChaosNoiseType : uint8_t
{
    White = 0,
    Smooth = 1,
    Crackle = 2
};

struct ChaosParams
{
    bool enabled = false;
    ChaosNoiseType type = ChaosNoiseType::Smooth;
    float frequencyHz = 8.0f;
    float amount = 1.0f;
};

struct ShapeSourceParams
{
    LfoShape shape = LfoShape::Asymmetric;
    float phase0 = 0.0f;
    float rho = 0.5f;
    float pUp = 1.0f;
    float pDown = 1.0f;
    bool useSpectralX = false; // false: partial index is the static "time" axis.
};

class Lfo
{
  public:
    void prepare(double sampleRate);
    void reset(float phase = 0.0f);
    // Advance by `samples` and return the latest output value in [-1, 1].
    float tick(const LfoParams &p, int samples);

  private:
    static float shapeOutput(const LfoParams &p, float xi);
    double sampleRate_ = 48000.0;
    float xi_ = 0.0f;        // current phase fraction in [0,1)
    float lastSh_ = 0.0f;    // sample-and-hold latch
    int shCounter_ = 0;
};

// -----------------------------------------------------------------------------
// Envelope shape library (§3.5)
// -----------------------------------------------------------------------------
enum class EnvCurve : uint8_t
{
    Exp = 0,
    Power = 1,
    Sigmoid = 2
};

float envCurveEval(EnvCurve mode, float tau, float eta); // F~(tau; mode, eta)

struct AdsrParams
{
    float attack = 0.005f;
    float decay = 0.35f;
    float sustain = 0.0f;
    float release = 0.08f;
    float curve = 0.5f;
};

float adsrCurveEval(float tau, float curve);

struct MatrixEnvParams
{
    bool enabled = false;
    float attack = 0.01f;
    float decay = 0.20f;
    float sustain = 0.0f;
    float release = 0.30f;
    EnvCurve attackCurve = EnvCurve::Exp;
    EnvCurve decayCurve = EnvCurve::Exp;
    EnvCurve releaseCurve = EnvCurve::Exp;
    float etaA = 4.0f;
    float etaD = 4.0f;
    float etaR = 4.0f;
    int pointCount = 4;
    std::array<MatrixEnvPoint, kMaxMatrixEnvPoints> points {
        MatrixEnvPoint { 0.0f, 0.0f, 0.0f },
        MatrixEnvPoint { 0.02f, 1.0f, 0.0f },
        MatrixEnvPoint { 0.42f, 0.0f, 0.0f },
        MatrixEnvPoint { 1.0f, 0.0f, 0.0f }
    };
};

inline float matrixEnvBreakpointEval(const MatrixEnvParams &p, float x)
{
    return pointCurveEval(p.points.data(), p.pointCount, x);
}

// -----------------------------------------------------------------------------
// Matrix rule
// -----------------------------------------------------------------------------
enum class ModSource : uint8_t
{
    None = 0,
    Lfo1 = 1, Lfo2, Lfo3, Lfo4,
    Env1 = 5, Env2, Env3, Env4,
    Velocity = 9,
    KeyTrack = 10,
    Random = 11,
    Adsr = 12,            // legacy: average amp env
    GeneratorSelf = 13,
    Chaos = 14,
    Shape = 15,
    Adsr1 = 16, Adsr2, Adsr3, Adsr4  // the four shared Amp ADSR envelopes, individually
};

enum class ModDestination : uint8_t
{
    Amp = 0,         // M_i^amp
    Freq = 1,        // M_i^freq
    Phase = 2,       // Delta phi_i
    DecayTime = 3,   // legacy destination; ignored by the v0.4 ADSR path
    SpectralDecay = 4,// legacy destination; ignored by the v0.4 ADSR path
    TrackGain = 5,
    TrackPan,
    PitchOct,
    PitchSem,
    PitchFine,
    PitchCrs,
    MetaMorph,
    MetaWarp,
    MetaPan,
    // Generic insert-parameter destinations (target an insert via MatrixRule::targetSlot
    // = insert index in the track's chain). Param meaning is per-effect-type (knob 0..3).
    InsertP0,
    InsertP1,
    InsertP2,
    InsertP3
};

constexpr int kModDestinationCount = int(ModDestination::InsertP3) + 1;

// Per-strip insert modulation: matrix can modulate the first kMaxModInserts inserts of a
// track, 4 knob params each. Indexed [insertIdx * kInsertModParams + param].
constexpr int kMaxModInserts = 8;
constexpr int kInsertModParams = 4;
inline int insertModIndex(int insertIdx, int param) { return insertIdx * kInsertModParams + param; }
// Maps an insert-param ModDestination to its knob index (0..3), or -1 if not one.
inline int insertModParamForDest(ModDestination d)
{
    switch(d)
    {
        case ModDestination::InsertP0: return 0;
        case ModDestination::InsertP1: return 1;
        case ModDestination::InsertP2: return 2;
        case ModDestination::InsertP3: return 3;
        default: return -1;
    }
}

enum class WeightMode : uint8_t
{
    All = 0,         // W = 1
    LowPartials = 1, // W = 1 - x
    HighPartials = 2,// W = x
    GroupLow = 3,    // W = 1 if mu == 0
    GroupMid = 4,
    GroupHigh = 5,
    BandIndex = 6    // W = 1 in [bandLo,bandHi]
};

struct MatrixRule
{
    bool enabled = false;
    ModSource source = ModSource::None;
    ModDestination dest = ModDestination::Amp;
    float depth = 0.0f;       // alpha_k; UI exposes roughly -12..+12 for wide pitch/phase sweeps
    WeightMode weight = WeightMode::All;
    int bandLo = 0;           // for WeightMode::BandIndex
    int bandHi = kMaxPartials;
    uint32_t targetTrackId = 0; // 0 keeps legacy global-partial routing
    int targetSlot = 0;        // bank slot (0..7) for filter/dist effect destinations
};

// -----------------------------------------------------------------------------
// Per-voice output buffers consumed by Voice during audio rendering.
// -----------------------------------------------------------------------------
struct MatrixVoiceOutput
{
    std::array<float, kMaxPartials> mAmp {};   // M_i^amp[i]  (multiplicative)
    std::array<float, kMaxPartials> mFreq {};  // M_i^freq[i] (multiplicative)
    std::array<float, kMaxPartials> dPhase {}; // Delta phi_i[i] (additive radians)
    std::array<float, kMaxPartials> dPan {};   // additive pan offset
    std::array<float, kMaxPartials> dMorph {}; // additive wavetable morph offset
    std::array<float, kMaxPartials> dWarp {};  // additive wavetable warp offset
};

inline void initMatrixOutput(MatrixVoiceOutput &o)
{
    o.mAmp.fill(1.0f);
    o.mFreq.fill(1.0f);
    o.dPhase.fill(0.0f);
    o.dPan.fill(0.0f);
    o.dMorph.fill(0.0f);
    o.dWarp.fill(0.0f);
}

// -----------------------------------------------------------------------------
// MatrixEngine: shared by all voices for LFO state, evaluated per control block.
// -----------------------------------------------------------------------------
class MatrixEngine
{
  public:
    void prepare(double sampleRate);
    void reset();

    void setParams(const std::array<LfoParams, kMaxLfos> &lfos,
                   const std::array<MatrixEnvParams, kMaxModEnvs> &envs,
                   const std::array<MatrixRule, kMaxMatrixRules> &rules,
                   const ChaosParams &chaos,
                   const ShapeSourceParams &shape);

    void setLfoParams(int idx, const LfoParams &p);
    LfoParams getLfoParams(int idx) const;
    void setEnvParams(int idx, const MatrixEnvParams &p);
    MatrixEnvParams getEnvParams(int idx) const;
    void setChaosParams(const ChaosParams &p);
    ChaosParams getChaosParams() const;
    void setShapeSourceParams(const ShapeSourceParams &p);
    ShapeSourceParams getShapeSourceParams() const;

    void setRule(int idx, const MatrixRule &r);
    MatrixRule getRule(int idx) const;

    // Global LFO advance (control-block size samples since last tick).
    void advanceControl(int samples);

    // Build per-voice outputs given source values (per-voice velocity, key, env, x[]).
    void evaluateForVoice(MatrixVoiceOutput &out,
                          const StaticSpectralFrame &frame,
                          float velocity,
                          float keyTrack01,
                          float adsrLevel,
                          const std::array<float, kMaxModEnvs> &envLevels,
                          float randPerVoice,
                          const uint32_t *trackIds = nullptr,
                          const int *trackBegin = nullptr,
                          const int *trackEnd = nullptr,
                          int trackCount = 0,
                          const std::array<float, kMaxAmpEnvs> *ampEnvLevels = nullptr) const;

    // Global (per-strip, control-rate) value of a modulation source. Per-voice-only
    // sources (velocity/key/random) return 0; ADSR/ENV use the passed representatives.
    float globalModSource(ModSource s, float adsrRep, const std::array<float, kMaxModEnvs> &envRep,
                          const std::array<float, kMaxAmpEnvs> &ampRep) const;

  private:
    static float weightFn(const MatrixRule &r, int i, const StaticSpectralFrame &frame);
    static float shapeOutput(const ShapeSourceParams &p, float x);

    std::array<LfoParams, kMaxLfos> lfoParams_ {};
    std::array<MatrixEnvParams, kMaxModEnvs> envParams_ {};
    std::array<Lfo, kMaxLfos> lfos_;
    std::array<float, kMaxLfos> lfoLastValue_ {};
    std::array<MatrixRule, kMaxMatrixRules> rules_ {};
    ChaosParams chaosParams_ {};
    ShapeSourceParams shapeParams_ {};
    float chaosValue_ = 0.0f;
    float chaosTarget_ = 0.0f;
    float crackleState_ = 0.371f;
    int chaosCounter_ = 0;
    double sampleRate_ = 48000.0;
};

} // namespace synth
