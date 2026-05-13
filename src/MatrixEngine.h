#pragma once

#include "SpectralFrame.h"

#include <array>
#include <cstdint>

namespace synth
{

// Architecture §3 Matrix / Performance Mapping.
// Provides:
//   - 8 LFOs with the asymmetric shape from §3.8 (xi, rho_lfo, p_u, p_d).
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

struct LfoParams
{
    bool enabled = false;
    LfoShape shape = LfoShape::Asymmetric;
    float frequencyHz = 1.0f;
    float phase0 = 0.0f;     // phi_0 in [0, 1)
    float rhoLfo = 0.5f;     // rho_lfo asymmetry in (0, 1)
    float pUp = 1.0f;        // p_u
    float pDown = 1.0f;      // p_d
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

struct GlobalAdsrParams
{
    float attack = 0.01f;
    float decay = 0.20f;
    float sustain = 0.70f;
    float release = 0.40f;
    EnvCurve attackCurve = EnvCurve::Exp;
    EnvCurve decayCurve = EnvCurve::Exp;
    EnvCurve releaseCurve = EnvCurve::Exp;
    float etaA = 4.0f;
    float etaD = 4.0f;
    float etaR = 4.0f;
};

// -----------------------------------------------------------------------------
// Matrix rule
// -----------------------------------------------------------------------------
enum class ModSource : uint8_t
{
    None = 0,
    Lfo1 = 1, Lfo2, Lfo3, Lfo4, Lfo5, Lfo6, Lfo7, Lfo8,
    Velocity = 9,
    KeyTrack = 10,
    Random = 11,
    Adsr = 12,
    GeneratorSelf = 13,
    Chaos = 14,
    Shape = 15
};

enum class ModDestination : uint8_t
{
    Amp = 0,         // M_i^amp
    Freq = 1,        // M_i^freq
    Phase = 2,       // Delta phi_i
    DecayTime = 3    // scales global decay (channel 0 of voice ADSR)
};

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
    float depth = 0.0f;       // alpha_k in -2..+2
    WeightMode weight = WeightMode::All;
    int bandLo = 0;           // for WeightMode::BandIndex
    int bandHi = kMaxPartials;
};

// -----------------------------------------------------------------------------
// Per-voice output buffers consumed by Voice during audio rendering.
// -----------------------------------------------------------------------------
struct MatrixVoiceOutput
{
    std::array<float, kMaxPartials> mAmp {};   // M_i^amp[i]  (multiplicative)
    std::array<float, kMaxPartials> mFreq {};  // M_i^freq[i] (multiplicative)
    std::array<float, kMaxPartials> dPhase {}; // Delta phi_i[i] (additive radians)
    float decayTimeMul = 1.0f;
};

inline void initMatrixOutput(MatrixVoiceOutput &o)
{
    o.mAmp.fill(1.0f);
    o.mFreq.fill(1.0f);
    o.dPhase.fill(0.0f);
    o.decayTimeMul = 1.0f;
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
                   const std::array<MatrixRule, kMaxMatrixRules> &rules,
                   const ChaosParams &chaos,
                   const ShapeSourceParams &shape);

    void setLfoParams(int idx, const LfoParams &p);
    LfoParams getLfoParams(int idx) const;
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
                          float randPerVoice) const;

  private:
    static float weightFn(const MatrixRule &r, int i, const StaticSpectralFrame &frame);
    static float shapeOutput(const ShapeSourceParams &p, float x);

    std::array<LfoParams, kMaxLfos> lfoParams_ {};
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
