#pragma once

#include "ModCurve.h"
#include "AdsrEnv.h"
#include "dsp/SpectralFrame.h"

#include <array>
#include <cstdint>

namespace synth
{

// -----------------------------------------------------------------------------
// Modulation sources and destinations
// -----------------------------------------------------------------------------
enum class ModSource : uint8_t
{
    None = 0,
    Lfo1 = 1, Lfo2, Lfo3, Lfo4,
    Env1 = 5, Env2, Env3, Env4,
    Velocity = 9,
    KeyTrack = 10,
    Random = 11,
    Adsr = 12,
    GeneratorSelf = 13,
    Chaos = 14,
    Shape = 15,
    Adsr1 = 16, Adsr2, Adsr3, Adsr4,
    // Constant 1.0 — combined with a rule's spatial mask it yields a pure static
    // distribution (e.g. a drawn per-partial pitch-offset profile).
    Unit = 20
};

enum class ModDestination : uint8_t
{
    Amp = 0,
    Freq = 1,
    Phase = 2,
    DecayTime = 3,
    SpectralDecay = 4,
    TrackGain = 5,
    TrackPan,
    PitchOct,
    PitchSem,
    PitchFine,
    PitchCrs,
    MetaMorph,
    MetaWarp,
    MetaPan,
    InsertP0,
    InsertP1,
    InsertP2,
    InsertP3
};

constexpr int kModDestinationCount = int(ModDestination::InsertP3) + 1;

constexpr int kMaxModInserts = 8;
constexpr int kInsertModParams = 4;
inline int insertModIndex(int insertIdx, int param) { return insertIdx * kInsertModParams + param; }

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
    All = 0,
    LowPartials = 1,
    HighPartials = 2,
    GroupLow = 3,
    GroupMid = 4,
    GroupHigh = 5,
    BandIndex = 6
};

// -----------------------------------------------------------------------------
// A single modulation routing rule
// -----------------------------------------------------------------------------
struct MatrixRule
{
    bool enabled = false;
    ModSource source = ModSource::None;
    ModDestination dest = ModDestination::Amp;
    float depth = 0.0f;
    WeightMode weight = WeightMode::All;
    int bandLo = 0;
    int bandHi = kMaxPartials;
    uint32_t targetTrackId = 0;
    int targetSlot = 0;
    // Spatial mask: sample a MOD slot's breakpoint curve across a countable axis
    // and multiply it into the per-partial weight — a drawable distribution over
    // "things that exist at once" instead of over time. -1 = no mask.
    // maskAxis: 0 = partial index within the rule's target range, 1 = spectral x,
    // 2 = normalized log-frequency, 3 = index scrolled by the mask slot's phase,
    // 4 = source-track index (the curve distributes across the SOURCE rack).
    int8_t maskSlot = -1;
    uint8_t maskAxis = 0;
    // Serum-style response bend on the SOURCE value, -1..+1 (0 = linear).
    // Applied sign-magnitude (sign(m)·bend(|m|)) so zero stays zero for both
    // bipolar and unipolar sources.
    float transferCurve = 0.0f;
    // Bypass without losing the configuration (enabled still means "slot used").
    uint8_t muted = 0;
};

// -----------------------------------------------------------------------------
// Mask groups (advanced tier): one base MOD-slot curve fanned into N lanes.
// Lane k is the base LFO with its rate and phase successively offset
// (progression optionally bent), driving lane k's own target — either one of
// 16 discrete parameter slots, or the whole partial family of a track
// (one lane per partial).
// -----------------------------------------------------------------------------
constexpr int kMaxMaskGroups = 4;
constexpr int kMaskGroupSlots = 16;

struct MaskGroupTarget
{
    bool enabled = false;
    ModDestination dest = ModDestination::Amp;
    uint32_t targetTrackId = 0;  // 0 = all partials
    float depth = 0.0f;
};

struct MaskGroup
{
    bool enabled = false;
    int8_t baseSlot = 0;         // MOD slot whose curve + rate is the base LFO
    float freqSpread = 0.0f;     // -1..+1: lane rate multiplier offset at fan end (0..2x)
    float phaseSpread = 0.0f;    // -1..+1: lane phase offset at fan end, in cycles
    float spreadCurve = 0.0f;    // progression bend across the fan (0 = linear)
    uint8_t family = 0;          // 0 = discrete slots, 1 = per-partial fan
    ModDestination familyDest = ModDestination::Freq;
    uint32_t familyTrackId = 0;  // 0 = all partials
    float familyDepth = 0.0f;
    std::array<MaskGroupTarget, kMaskGroupSlots> targets {};
};

// -----------------------------------------------------------------------------
// Per-voice output buffers consumed by Voice during audio rendering
// -----------------------------------------------------------------------------
struct MatrixVoiceOutput
{
    std::array<float, kMaxPartials> mAmp {};
    std::array<float, kMaxPartials> mFreq {};
    std::array<float, kMaxPartials> dPhase {};
    std::array<float, kMaxPartials> dPan {};
    std::array<float, kMaxPartials> dMorph {};
    std::array<float, kMaxPartials> dWarp {};
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
// ModMatrix: shared LFO/Chaos state + per-voice modulation evaluation
// -----------------------------------------------------------------------------
class ModMatrix
{
  public:
    void prepare(double sampleRate);
    void reset();

    void setParams(const std::array<ModSlotParams, kMaxModSlots> &slots,
                   const std::array<MatrixRule, kMaxMatrixRules> &rules,
                   const ChaosParams &chaos,
                   const ShapeSourceParams &shape);

    void setModSlotParams(int idx, const ModSlotParams &p);
    ModSlotParams getModSlotParams(int idx) const;
    void setChaosParams(const ChaosParams &p);
    ChaosParams getChaosParams() const;
    void setShapeSourceParams(const ShapeSourceParams &p);
    ShapeSourceParams getShapeSourceParams() const;

    void setRule(int idx, const MatrixRule &r);
    MatrixRule getRule(int idx) const;

    void setMaskGroups(const std::array<MaskGroup, kMaxMaskGroups> &groups);

    void advanceControl(int samples);

    void evaluateForVoice(MatrixVoiceOutput &out,
                          const StaticSpectralFrame &frame,
                          float velocity,
                          float keyTrack01,
                          float adsrLevel,
                          const std::array<float, kMaxModSlots> &slotLevels,
                          float randPerVoice,
                          const uint32_t *trackIds = nullptr,
                          const int *trackBegin = nullptr,
                          const int *trackEnd = nullptr,
                          int trackCount = 0,
                          const std::array<float, kMaxAmpEnvs> *ampEnvLevels = nullptr) const;

    float globalModSource(ModSource s, float adsrRep, const std::array<float, kMaxModSlots> &slotRep,
                          const std::array<float, kMaxAmpEnvs> &ampRep) const;

    // Serum-style response bend on a source value (see MatrixRule::transferCurve).
    // Public/static so the insert-param path in SynthCore shares the exact math.
    static float applyTransfer(const MatrixRule &r, float m);
    // Unit-domain bend used by transfer curves and the mask-group fan progression.
    static float bend01(float x, float c);

  private:
    static float weightFn(const MatrixRule &r, int i, const StaticSpectralFrame &frame);
    static float shapeOutput(const ShapeSourceParams &p, float x);

    // Spatial-mask lookup table: each slot's curve is baked to a small LUT when
    // its points change (dirty-checked in setParams — which runs per control
    // block on the audio thread), so per-partial mask sampling is one lerp
    // instead of a breakpoint scan with pow().
    static constexpr int kMaskLutSize = 128;
    void bakeMaskLut(int slot, const ModSlotParams &p);
    float maskLookup(int slot, float x) const
    {
        const float fx = (x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x)) * float(kMaskLutSize);
        const int k = std::min(int(fx), kMaskLutSize - 1);
        const float t = fx - float(k);
        const auto &lut = maskLut_[(size_t)slot];
        return lut[(size_t)k] + (lut[(size_t)k + 1] - lut[(size_t)k]) * t;
    }
    std::array<std::array<float, kMaskLutSize + 1>, kMaxModSlots> maskLut_ {};
    bool maskLutReady_ = false;

    // Lane value of a mask group at normalized fan position x (0 = first lane).
    float maskGroupLane(int gi, const MaskGroup &g, float x) const
    {
        const float xb = bend01(x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x), g.spreadCurve);
        const int bs = std::min(std::max(int(g.baseSlot), 0), kMaxModSlots - 1);
        const double ph = groupTime_[(size_t)gi] * (1.0 + double(xb) * double(g.freqSpread))
                          + double(xb) * double(g.phaseSpread);
        return maskLookup(bs, float(ph - std::floor(ph))) * 2.0f - 1.0f;
    }

    std::array<ModSlotParams, kMaxModSlots> slotParams_ {};
    std::array<float, kMaxModSlots> slotPhase_ {};
    std::array<float, kMaxModSlots> slotValue_ {};
    std::array<MatrixRule, kMaxMatrixRules> rules_ {};
    std::array<MaskGroup, kMaxMaskGroups> groups_ {};
    // Unbounded fan time per group (double for precision; wrapped very rarely so
    // freq-spread lane phases stay continuous).
    std::array<double, kMaxMaskGroups> groupTime_ {};
    ChaosParams chaosParams_ {};
    ShapeSourceParams shapeParams_ {};
    float chaosValue_ = 0.0f;
    float chaosTarget_ = 0.0f;
    float crackleState_ = 0.371f;
    int chaosCounter_ = 0;
    double sampleRate_ = 48000.0;
};

// Backward-compat alias so callers that still say MatrixEngine compile unchanged.
using MatrixEngine = ModMatrix;

} // namespace synth
