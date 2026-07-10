#pragma once

#include "dsp/Generators.h"
#include "engine/MatrixEngine.h"
#include "dsp/SpectralFrame.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

namespace synth
{

// Single polyphonic Voice. Owns runtime phase accumulators, one note-triggered
// ADSR envelope, matrix ENV sources, and control-rate interpolation buffers.
class Voice
{
  public:
    static constexpr int kMaxVoiceRenderBlockSamples = 4096;
    enum class PartialState : uint8_t
    {
        Idle = 0,
        Attack,
        Decay,
        Sustain,
        Release
    };

    void prepare(double sampleRate);

    bool isIdle() const { return idle_; }
    float getLiveMorph(int partialIdx) const
    {
        const auto i = static_cast<size_t>(partialIdx);
        if(partialIdx < 0 || partialIdx >= kMaxPartials) return 0.0f;
        if(idle_) return 0.0f;
        return std::clamp(morphCur_[i], 0.0f, 1.0f);
    }
    bool isReleasing() const { return releasing_; }
    int getNoteNumber() const { return noteNumber_; }
    uint64_t getStartTick() const { return startTick_; }
    float sourceTimeSeconds() const { return float(double(ageSamples_) / sampleRate_); }

    void noteOn(int midiNote, float velocity, const StaticSpectralFrame &frame,
                const AdsrParams &adsr,
                const std::array<AdsrParams, kMaxAmpEnvs> &ampEnvs,
                const std::array<ModSlotParams, kMaxModSlots> &modSlots,
                const UnisonParams &unison, RenderQualityMode quality, uint64_t tick);
    void noteOff();
    void steal(); // immediate fast release

    // Recompute target ampSrc[i], freq[i], dPhase[i] from frame + matrix output.
    // Called at every control-rate boundary.
    void updateControl(const StaticSpectralFrame &frame,
                       const MatrixVoiceOutput &matrixOut,
                       const AdsrParams &adsr,
                       const std::array<AdsrParams, kMaxAmpEnvs> &ampEnvs,
                       const std::array<ModSlotParams, kMaxModSlots> &modSlots,
                       const UnisonParams &unison,
                       const std::array<RenderTrackRuntime, kMaxSourceTracks> &trackRuntime,
                       int renderTrackCount,
                       RenderQualityMode quality,
                       float globalGain,
                       int blockSize);

    // Shared scratch for cross-track (source) modulation. Owned by SynthCore and
    // shared across voices (voices render sequentially), so it stays out of the
    // per-voice footprint. One buffer per source track.
    struct ModScratch
    {
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxSourceTracks> bufL {};
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxSourceTracks> bufR {};
        // Output buffers for per-voice filter nodes when evaluating the route DAG.
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxPerVoiceFilters> filterL {};
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxPerVoiceFilters> filterR {};
        // Output buffers for amp-env route node instances.
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxAmpEnvRouteNodes> ampEnvL {};
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxAmpEnvRouteNodes> ampEnvR {};
        // Output buffers for utility nodes (component output-router).
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxUtilNodes> utilL {};
        std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxUtilNodes> utilR {};
    };
    void setModScratch(ModScratch *s) { modScratch_ = s; }

    // Compiled per-voice routing DAG (filters sum their inputs). Set per block
    // before renderAdd; empty/invalid → legacy per-track filter chains are used.
    void setCompiledRoute(const CompiledPerVoiceRoute &r) { route_ = r; }

    // Per-strip bus output: voices accumulate each source track into its own bus
    // (pre-zeroed by SynthCore); strip inserts then run on the bus, not per-voice.
    void setTrackBuses(float *const *busL, float *const *busR) { busL_ = busL; busR_ = busR; }

    // Audio-rate render. With buses set, accumulates per-track into the buses;
    // otherwise (buses null) falls back to summing into left/right.
    void renderAdd(float *left, float *right, int numSamples);
    void setWavetableRenderState(std::shared_ptr<const WavetableSeedRenderState> state)
    {
        wavetableHolder_ = std::move(state);
        wavetable_ = wavetableHolder_.get();
    }
    int activePartialCount() const { return activeCount_; }
    void beginPartialRender(int numSamples);
    void renderPartialRangeRaw(float *left, float *right, int numSamples, int partialBegin, int partialEnd,
                               const float *pmBuffer = nullptr, const float *syncBuffer = nullptr);
    void finishPartialRender(float *left, float *right, const float *rawLeft, const float *rawRight, int numSamples);
    void finalizeBlock(int numSamples); // age + idle detection (per-track bus path)
    static bool debugVerifyDecayTransient();

    // Helpful for keytrack / matrix.
    float velocity() const { return velocity_; }
    float keyTrack01() const { return keyTrack01_; }
    float averageEnv() const { return avgEnv_; }
    const std::array<float, kMaxModSlots> &modSlotLevels() const { return slotLevel_; }
    std::array<float, kMaxAmpEnvs> ampEnvLevels() const
    {
        std::array<float, kMaxAmpEnvs> v {};
        for(int i = 0; i < kMaxAmpEnvs; ++i) v[(size_t)i] = sharedAmpEnvState_[(size_t)i].value;
        return v;
    }
    uint32_t voiceRandomSeed() const { return rngSeed_; }

  private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kTwoPi = 6.28318530717958647692f;
    static constexpr float kMidiToHzA4 = 440.0f;
    static constexpr int kSineTableSize = kWavetableSize;
    static constexpr float kRadiansToTable = float(kSineTableSize) / kTwoPi;
    static constexpr float kCosTableOffset = 0.25f * float(kSineTableSize);
    static constexpr float kMorphSmoothingSeconds = 0.015f;

    void seedPhases(const StaticSpectralFrame &frame, const UnisonParams &unison);
    void updateUnisonLayout(const UnisonParams &unison);
    void updateUnisonPhaseOffsets(const UnisonParams &unison, int sourceIndex);
    static const std::array<float, kSineTableSize + 1> &sineTable();
    static float lookupTablePosition(const std::array<float, kWavetableSize + 1> &table, float tablePosition);
    static float wrapTablePosition(float tablePosition);
    static float lookupSineTablePosition(float tablePosition);
    static float midiToHz(int midiNote)
    {
        return kMidiToHzA4 * std::pow(2.0f, (float(midiNote) - 69.0f) / 12.0f);
    }

    double sampleRate_ = 48000.0;
    bool idle_ = true;
    bool releasing_ = false;
    bool controlsPrimed_ = false;
    int noteNumber_ = -1;
    float velocity_ = 0.0f;
    float keyTrack01_ = 0.5f;
    float voiceF0_ = 440.0f;
    uint64_t startTick_ = 0;
    uint64_t ageSamples_ = 0;
    uint32_t rngSeed_ = 1u;

    int activeCount_ = 0;

    int requestedUnisonCount_ = 1;
    int unisonCount_ = 1;
    float unisonDetuneCents_ = 0.0f;
    float unisonWidthStereo_ = 0.0f;
    std::shared_ptr<const WavetableSeedRenderState> wavetableHolder_ {};
    const WavetableSeedRenderState *wavetable_ = nullptr;

    // Audio-rate wavetable phase per unison voice and partial.
    std::array<std::array<float, kMaxRenderPartials>, kMaxUnison> thetaTable_ {};
    std::array<float, kMaxPartials> phaseInitTable_ {};
    std::array<float, kMaxUnison> unisonDetuneRatio_ {};
    std::array<float, kMaxUnison> unisonGainL_ {};
    std::array<float, kMaxUnison> unisonGainR_ {};
    std::array<float, kMaxUnison> unisonPhaseOffsetTable_ {};

    // Linearly interpolated targets. Each control block, *_target is set; per sample current += step.
    std::array<float, kMaxPartials> ampCur_ {};
    std::array<float, kMaxPartials> ampStep_ {};
    std::array<float, kMaxPartials> freqCur_ {};
    std::array<float, kMaxPartials> freqStep_ {};
    std::array<float, kMaxPartials> dPhaseCur_ {};
    std::array<float, kMaxPartials> dPhaseStep_ {};
    std::array<float, kMaxPartials> dPanCur_ {};
    std::array<float, kMaxPartials> dPanStep_ {};
    std::array<float, kMaxPartials> dWarpCur_ {};
    std::array<float, kMaxPartials> dWarpStep_ {};
    std::array<float, kMaxPartials> phaseDriftCur_ {};
    std::array<float, kMaxPartials> phaseDriftStep_ {};
    std::array<float, kMaxPartials> phaseJitterCur_ {};
    std::array<float, kMaxPartials> phaseJitterStep_ {};
    std::array<float, kMaxPartials> morphCur_ {};
    std::array<float, kMaxPartials> morphTarget_ {};
    std::array<bool, kMaxPartials> morphInitialized_ {};
    float morphSmoothingCoeff_ = 1.0f;
    std::array<float, kMaxVoiceRenderBlockSamples> globalEnvScratch_ {};
    std::array<float, kMaxVoiceRenderBlockSamples> serialRawL_ {};
    std::array<float, kMaxVoiceRenderBlockSamples> serialRawR_ {};
    std::array<float, kMaxVoiceRenderBlockSamples> sourceRawL_ {};
    std::array<float, kMaxVoiceRenderBlockSamples> sourceRawR_ {};

    // Cross-track FM integrates the modulator into a phase offset; the integral
    // must stay continuous across render blocks, so it lives here (carrier track
    // × mod slot) instead of resetting per block. Wrapped to ±π each block to
    // preserve float precision over long notes.
    std::array<std::array<double, kMaxTrackMods>, kMaxSourceTracks> fmPhaseAcc_ {};
    // Hard-sync edge state: previous modulator sample per carrier track, so an
    // upward zero-crossing spanning a block boundary isn't missed.
    std::array<float, kMaxSourceTracks> syncPrev_ {};

    struct SourceFilterRuntime
    {
        float lp1L = 0.0f;
        float lp2L = 0.0f;
        float lp3L = 0.0f;
        float lp4L = 0.0f;
        float bpL = 0.0f;
        float lp1R = 0.0f;
        float lp2R = 0.0f;
        float lp3R = 0.0f;
        float lp4R = 0.0f;
        float bpR = 0.0f;
    };
    void processSourceFilter(float *left, float *right, int numSamples, const GeneratorSourceParams &source,
                             SourceFilterRuntime &state, bool applyGainPan = true);
    void processSourceFilterParams(float *left, float *right, int numSamples, const SourceFilterParams &filter,
                                   SourceFilterRuntime &state, float gainL = 1.0f, float gainR = 1.0f);
    void processPerVoiceFilters(float *left, float *right, int numSamples,
                                const RenderTrackRuntime &runtime, SourceFilterRuntime *states,
                                bool applyGainPan = true);
    void evaluateRouteGraph(int numSamples, int sourceCount, const bool *rendered,
                            const std::function<void(int, const float *, const float *)> &flushTrack);
    // Evaluate one compiled route node (filter / amp-env / util) into its scratch
    // buffer. Idempotent per block via the *EvalDone_ flags so a node tapped as a
    // mod source (evaluated early, before its carrier renders) isn't re-run when
    // evaluateRouteGraph walks the full order later.
    void evaluateGraphNode(const RouteNodeRef &node, int numSamples, int sourceCount, const bool *rendered);
    int renderIndexOfTrackId(uint32_t tid, int sourceCount) const;
    // Resolve a route node ref to its output buffers; false → skip (unrendered
    // track, bad slot, or a ModOnly source that must not reach the audio output).
    bool resolveNodeBuf(const RouteNodeRef &r, int sourceCount, const bool *rendered,
                        const float *&L, const float *&R) const;
    // Transitive TRACK inputs of a route node (which sources must have rendered
    // before the node's output is meaningful).
    void collectNodeTrackDeps(const RouteNodeRef &ref, int sourceCount,
                              std::array<bool, kMaxSourceTracks> &deps) const;
    void applyGainPan(float *left, float *right, int numSamples, const GeneratorSourceParams &source);
    static void applySourceMod(float *cL, float *cR, const float *mL, const float *mR,
                               int numSamples, SourceModType type, float depth);
    std::array<std::array<SourceFilterRuntime, kMaxPerVoiceFilters>, kMaxSourceTracks> sourceFilterStates_ {};
    std::array<SourceFilterRuntime, kMaxPerVoiceFilters> filterNodeStates_ {}; // per filter NODE (route DAG)
    struct UtilBandState { float loL = 0, loR = 0, hiL = 0, hiR = 0; };
    std::array<UtilBandState, kMaxUtilNodes> utilBandStates_ {}; // per utility node band-pass state
    // Per-block "node already evaluated" flags (see evaluateGraphNode).
    std::array<bool, kMaxPerVoiceFilters> filterEvalDone_ {};
    std::array<bool, kMaxAmpEnvRouteNodes> ampEnvEvalDone_ {};
    std::array<bool, kMaxUtilNodes> utilEvalDone_ {};
    std::array<GeneratorSourceParams, kMaxSourceTracks> sourceParams_ {};
    std::array<RenderTrackRuntime, kMaxSourceTracks> trackRuntime_ {};
    CompiledPerVoiceRoute route_ {};
    ModScratch *modScratch_ = nullptr;
    float *const *busL_ = nullptr;  // per-track output buses (owned by SynthCore)
    float *const *busR_ = nullptr;
    std::array<float, kMaxVoiceRenderBlockSamples> pmScratch_ {};      // phase-mod (radians) for FM/PM
    std::array<float, kMaxVoiceRenderBlockSamples> syncMonoScratch_ {}; // mono modulator for hard sync
    int renderTrackCount_ = 0;
    std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxAmpEnvs> ampEnvScratch_ {};
    std::array<std::array<float, kMaxVoiceRenderBlockSamples>, kMaxSourceTracks> trackEnvScratch_ {};
    int currentRenderTrack_ = 0;

    struct AdsrRuntimeState
    {
        PartialState state = PartialState::Idle;
        int stageSample = 0;
        int attackSamples = 1;
        int decaySamples = 1;
        int releaseSamples = 1;
        float value = 0.0f;
        float releaseFrom = 0.0f;
    };

    AdsrRuntimeState ampEnv_;
    std::array<AdsrRuntimeState, kMaxSourceTracks> trackEnvState_ {};
    // Unified modulator slots: per-voice phase + level (loop or one-shot).
    std::array<ModSlotParams, kMaxModSlots> slotParams_ {};
    std::array<float, kMaxModSlots> slotPhase_ {};
    std::array<float, kMaxModSlots> slotLevel_ {};
    std::array<AdsrRuntimeState, kMaxAmpEnvs> sharedAmpEnvState_ {};
    std::array<AdsrParams, kMaxAmpEnvs> sharedAmpEnvParams_ {};
    float sustain_ = 0.7f;
    float adsrCurve_ = 0.5f;
    AdsrParams frozenAdsr_ {};

    float avgEnv_ = 0.0f;
    RenderQualityMode renderQuality_ = RenderQualityMode::Normal;
};

} // namespace synth
