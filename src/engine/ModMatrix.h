#pragma once

#include "ModCurve.h"
#include "AdsrEnv.h"
#include "dsp/SpectralFrame.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

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
    InsertP3,
    // Depth of a Basic Oscillator rack's own cross-unit modulation. Scoped to a
    // track, not a partial — the rule's targetTrackId picks the rack.
    OscModDepth
};

constexpr int kModDestinationCount = int(ModDestination::OscModDepth) + 1;

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
// Real phase accumulators available to one group's fan. The fan spans exactly as
// many lanes as the group has sampling positions: 16 for the discrete slots, one
// per element for a partial family — 64 partials really are 64 independent
// layers, not 16 interpolated ones. Families larger than this crossfade.
constexpr int kMaskFanLanes = 64;
// Per-lane waveform resolution when a wavetable drives the fan.
constexpr int kMaskWaveLut = 128;

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
    int8_t baseSlot = 0;         // MOD slot supplying the base curve (shape only)
    // The fan's own rate. Deliberately not the base MOD slot's: the group is a
    // component and owns its speed, so the shape source can change (curve slot,
    // wavetable) without the fan changing speed — and unlike ModSlotParams this
    // is persisted with the group.
    float rateHz = 1.0f;
    float freqSpread = 0.0f;     // -1..+1: lane rate multiplier offset at fan end (0..2x)
    float phaseSpread = 0.0f;    // -1..+1: lane phase offset at fan end, in cycles
    float spreadCurve = 0.0f;    // progression bend across the fan (0 = linear)
    uint8_t family = 0;          // 0 = discrete slots, 1 = per-partial fan
    // Lane SHAPE source: 0 = the base MOD slot's curve on every lane,
    // 1 = a multi-frame wavetable, morphed across the fan so lane k plays the
    // frame sitting at its own bent fan position. The base MOD slot still
    // supplies the fan's rate in both cases.
    uint8_t waveSource = 0;
    uint32_t waveTrackId = 0;    // source track whose wavetable feeds the fan
    ModDestination familyDest = ModDestination::Freq;
    uint32_t familyTrackId = 0;  // 0 = all partials
    float familyDepth = 0.0f;
    std::array<MaskGroupTarget, kMaskGroupSlots> targets {};
};

// A wavetable decimated to modulation resolution: one bipolar LUT per frame.
// Baked on the parameter thread (SynthCore) whenever the table's identity
// changes; the audio thread only ever reads it.
struct MaskWaveFrames
{
    int count = 0;
    std::vector<std::array<float, kMaskWaveLut + 1>> frame;
};

struct MaskWaveLanes
{
    int lanes = kMaskGroupSlots;                   // real accumulators in the fan
    std::shared_ptr<const MaskWaveFrames> frames;  // null = fan the base MOD curve
};

// Published in the render snapshot; rebuilt only when a group's lane count or
// wavetable identity actually changes, so the audio thread's per-control-block
// handoff is a pointer compare.
struct MaskWaveBank
{
    std::array<MaskWaveLanes, kMaxMaskGroups> group {};
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
    // Per-track, not per-partial: see ModDestination::OscModDepth.
    std::array<float, kMaxSourceTracks> dOscMod {};
};

inline void initMatrixOutput(MatrixVoiceOutput &o)
{
    o.mAmp.fill(1.0f);
    o.mFreq.fill(1.0f);
    o.dPhase.fill(0.0f);
    o.dPan.fill(0.0f);
    o.dMorph.fill(0.0f);
    o.dWarp.fill(0.0f);
    o.dOscMod.fill(0.0f);
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
    // Borrowed, NOT owned: the bank belongs to the RenderSnapshot the caller is
    // holding for this control block. Taking a shared_ptr here would make the
    // audio thread the last owner of the previous bank and free ~33 KB inside
    // renderBlock. Must be refreshed at the top of every control block, before
    // any evaluate — see SynthCore::renderBlock.
    void setMaskWaveBank(const MaskWaveBank *bank) { waveBank_ = bank; }

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

    // Straight lerp over a baked wave frame LUT.
    static float waveLookup(const std::array<float, kMaskWaveLut + 1> &lut, float x)
    {
        const float fx = (x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x)) * float(kMaskWaveLut);
        const int k = std::min(int(fx), kMaskWaveLut - 1);
        const float t = fx - float(k);
        return lut[(size_t)k] + (lut[(size_t)k + 1] - lut[(size_t)k]) * t;
    }

    const MaskWaveFrames *waveFramesFor(int gi) const
    {
        if(!waveBank_)
            return nullptr;
        const auto *fr = waveBank_->group[(size_t)gi].frames.get();
        return (fr != nullptr && fr->count > 0) ? fr : nullptr;
    }

    // One lane's live output, bipolar ±1. Every lane owns a real phase
    // accumulator; its bent fan position supplies both the phase offset and —
    // when a wavetable drives the fan — the frame it morphs to.
    float laneValue(int gi, const MaskGroup &g, int k) const
    {
        const double raw = lanePhase_[(size_t)gi][(size_t)k]
                           + double(laneXb_[(size_t)gi][(size_t)k]) * double(g.phaseSpread);
        const float ph = float(raw - std::floor(raw));
        if(const MaskWaveFrames *fr = waveFramesFor(gi))
        {
            const float fpos = laneXb_[(size_t)gi][(size_t)k] * float(fr->count - 1);
            const int fa = std::min(std::max(int(fpos), 0), fr->count - 1);
            const int fb = std::min(fa + 1, fr->count - 1);
            const float ft = fpos - float(fa);
            const float a = waveLookup(fr->frame[(size_t)fa], ph);
            const float b = waveLookup(fr->frame[(size_t)fb], ph);
            return a + (b - a) * ft;
        }
        const int bs = std::min(std::max(int(g.baseSlot), 0), kMaxModSlots - 1);
        return maskLookup(bs, ph) * 2.0f - 1.0f;
    }

    // Lane value of a mask group at normalized fan position x (0 = first lane).
    // The fan is laneCount_[gi] real phase accumulators (each advancing at its
    // own spread rate, wrapped mod 1 individually); positions between them
    // crossfade the two neighbouring lanes' OUTPUT values. Parameter edits
    // therefore only change future lane rates — no elapsed-time-scaled phase
    // jumps. When the lane count matches the element count (the normal case) x
    // lands exactly on integer lanes and nothing is interpolated.
    //
    // Lane outputs are constant for the whole control block, so advanceControl
    // evaluates them once per lane into laneVal_ and this — called per partial
    // per voice — is only ever a lerp between two floats.
    float maskGroupLane(int gi, float x) const
    {
        const int lanes = std::min(std::max(laneCount_[(size_t)gi], 2), kMaskFanLanes);
        x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
        const float fx = x * float(lanes - 1);
        const int k = std::min(int(fx), lanes - 2);
        const float t = fx - float(k);
        const auto &lv = laneVal_[(size_t)gi];
        const float v0 = lv[(size_t)k];
        return v0 + (lv[(size_t)k + 1] - v0) * t;
    }

    std::array<ModSlotParams, kMaxModSlots> slotParams_ {};
    std::array<float, kMaxModSlots> slotPhase_ {};
    std::array<float, kMaxModSlots> slotValue_ {};
    std::array<MatrixRule, kMaxMatrixRules> rules_ {};
    std::array<MaskGroup, kMaxMaskGroups> groups_ {};
    const MaskWaveBank *waveBank_ = nullptr;  // borrowed for one control block
    // Fan lane state per group: accumulated phase (mod 1), the cached bent fan
    // position of each lane, this block's lane outputs, and the lane count both
    // advance and evaluate agree on (latched in advanceControl so a mid-block
    // bank swap can't desync them).
    std::array<std::array<double, kMaskFanLanes>, kMaxMaskGroups> lanePhase_ {};
    std::array<std::array<float, kMaskFanLanes>, kMaxMaskGroups> laneXb_ {};
    std::array<std::array<float, kMaskFanLanes>, kMaxMaskGroups> laneVal_ {};
    // Latched by advanceControl before any evaluate; the clamp in maskGroupLane
    // keeps the zero-initialized state harmless.
    std::array<int, kMaxMaskGroups> laneCount_ {};
    // Dirty keys for the laneXb_ cache (bend01 is a pow — see advanceControl).
    std::array<int, kMaxMaskGroups> xbLanes_ {};
    std::array<float, kMaxMaskGroups> xbCurve_ {};
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
