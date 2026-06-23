#pragma once

#include "model/CompositionModel.h"
#include "dsp/Effects.h"
#include "dsp/Generators.h"
#include "dsp/InsertChain.h"
#include "engine/MatrixEngine.h"
#include "dsp/Operators.h"
#include "model/SpectralFrame.h"
#include "engine/Voice.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace synth
{

struct SourceState
{
    SourceGenParams gen;
    OperatorChain chain;
    std::shared_ptr<const SpectralTimeline> timeline;
    StaticSpectralFrame frame;
    std::shared_ptr<const WavetableSeedRenderState> wavetable;
    std::string presetName = "Seed";
};

struct RenderSnapshot
{
    std::shared_ptr<const SpectralTimeline> timeline;
    StaticSpectralFrame frame;
    std::shared_ptr<const WavetableSeedRenderState> wavetable;
    std::array<SourceTrackParams, kMaxSourceTracks> tracks {};
    int trackCount = 0;
    std::array<RenderTrackRuntime, kMaxSourceTracks> trackRuntime {};
    int renderTrackCount = 0;
    AdsrParams adsr;
    std::array<AdsrParams, kMaxAmpEnvs> ampEnvParams {};
    UnisonParams unison;
    std::array<GeneratorSourceParams, kMaxSourceTracks> generatorSources {};
    RenderQualityMode renderQuality = RenderQualityMode::Normal;
    float globalGain = 0.30f;
    std::array<LfoParams, kMaxLfos> lfoParams {};
    std::array<MatrixEnvParams, kMaxModEnvs> matrixEnvParams {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    EffectsChainParams effectsParams {};
    std::vector<SourceGroupDef> groups {}; // per-strip group buses
};

class SynthCore
{
  public:
    SynthCore();
    ~SynthCore();

    void prepare(double sampleRate);
    void renderBlock(float *left, float *right, int numSamples);

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();

    void setGeneratorParams(const SourceGenParams &p);
    uint32_t addSourceTrack(SourceTrackType type, const std::string &name);
    void removeSourceTrack(uint32_t trackId);
    void moveSourceTrack(uint32_t trackId, int newIndex);
    void setSourceTrack(uint32_t trackId, const SourceTrackParams &track);
    void setSourceTrackMorphOnly(uint32_t trackId, float morph);
    void setSourceTracks(const std::vector<SourceTrackParams> &tracks);
    std::vector<SourceTrackParams> getSourceTracks() const;
    void setGeneratorBasicParams(int partialCount, FreqShape freqShape, float inharmonic, int sourceCount,
                                 const UnisonParams &unison);
    void setGeneratorRuntimeParams(const UnisonParams &unison, RenderQualityMode quality);
    void setGeneratorSourceParams(int index, const GeneratorSourceParams &sourceParams);
    void setPartialEnabled(int index, bool enabled);
    void setPartialAmp(int index, float amp);
    void setPartialRatio(int index, float ratio);
    void setMetaPartialRuntime(int index, bool enabled, float ratio, float amp, float phase, float pan,
                               float morph, WavetableWarpMode warpMode, float warpAmount);
    void setMetaPartialSlot(int index, const WavetablePartialSlot &slot);
    bool loadWavetableFrame(int partialIndex, int frameIndex, const std::string &path);
    SourceGenParams getGeneratorParams() const;
    // Legacy-compatible single-Seed wrappers. The DPF app currently ignores
    // seedPresetId and routes all edits to the active Seed.
    void setSeedGeneratorParams(uint64_t seedPresetId, const SourceGenParams &p);
    SourceGenParams getSeedGeneratorParams(uint64_t seedPresetId) const;

    void setGlobalAdsr(const AdsrParams &a);
    AdsrParams getGlobalAdsr() const;
    void setAmpEnvParams(int idx, const AdsrParams &params);
    AdsrParams getAmpEnvParams(int idx) const;
    void setSeedAdsrParams(uint64_t seedPresetId, const AdsrParams &a);
    AdsrParams getSeedAdsrParams(uint64_t seedPresetId) const;

    void setOperatorChain(const OperatorChain &c);
    OperatorChain getOperatorChain() const;
    void setSeedOperatorChain(uint64_t seedPresetId, const OperatorChain &c);
    OperatorChain getSeedOperatorChain(uint64_t seedPresetId) const;

    void setGlobalGain(float g);
    float getGlobalGain() const;

    void setLfoParams(int idx, const LfoParams &p);
    void setLfoParamsWithUndo(int idx, const LfoParams &p);
    LfoParams getLfoParams(int idx) const;
    void setSeedLfoParams(uint64_t seedPresetId, int idx, const LfoParams &p);
    void setSeedLfoParamsWithUndo(uint64_t seedPresetId, int idx, const LfoParams &p);
    LfoParams getSeedLfoParams(uint64_t seedPresetId, int idx) const;
    void setMatrixEnvParams(int idx, const MatrixEnvParams &p);
    void setMatrixEnvParamsWithUndo(int idx, const MatrixEnvParams &p);
    MatrixEnvParams getMatrixEnvParams(int idx) const;
    void setSeedMatrixEnvParams(uint64_t seedPresetId, int idx, const MatrixEnvParams &p);
    void setSeedMatrixEnvParamsWithUndo(uint64_t seedPresetId, int idx, const MatrixEnvParams &p);
    MatrixEnvParams getSeedMatrixEnvParams(uint64_t seedPresetId, int idx) const;
    void setMatrixRule(int idx, const MatrixRule &r);
    void setMatrixRuleWithUndo(int idx, const MatrixRule &r);
    MatrixRule getMatrixRule(int idx) const;
    void setSeedMatrixRule(uint64_t seedPresetId, int idx, const MatrixRule &r);
    void setSeedMatrixRuleWithUndo(uint64_t seedPresetId, int idx, const MatrixRule &r);
    MatrixRule getSeedMatrixRule(uint64_t seedPresetId, int idx) const;
    void setChaosParams(const ChaosParams &p);
    void setChaosParamsWithUndo(const ChaosParams &p);
    ChaosParams getChaosParams() const;
    void setSeedChaosParams(uint64_t seedPresetId, const ChaosParams &p);
    void setSeedChaosParamsWithUndo(uint64_t seedPresetId, const ChaosParams &p);
    ChaosParams getSeedChaosParams(uint64_t seedPresetId) const;
    void setShapeSourceParams(const ShapeSourceParams &p);
    void setShapeSourceParamsWithUndo(const ShapeSourceParams &p);
    ShapeSourceParams getShapeSourceParams() const;
    void setSeedShapeSourceParams(uint64_t seedPresetId, const ShapeSourceParams &p);
    void setSeedShapeSourceParamsWithUndo(uint64_t seedPresetId, const ShapeSourceParams &p);
    ShapeSourceParams getSeedShapeSourceParams(uint64_t seedPresetId) const;

    void setEffectsParams(const EffectsChainParams &p);
    EffectsChainParams getEffectsParams() const;
    void setSourceGroups(const std::vector<SourceGroupDef> &groups);
    void setSeedEffectsParams(uint64_t seedPresetId, const EffectsChainParams &p);
    EffectsChainParams getSeedEffectsParams(uint64_t seedPresetId) const;

    // Conservative boundary for future DPF-native state/preset support.
    void setSeedPatch(const SeedPatch &patch);
    SeedPatch getSeedPatch() const;

    // Legacy name retained for compatibility with old callers; it restores the
    // active Seed parameter snapshot only, not a composition graph.
    bool undoCompositionChange();
    StaticSpectralFrame getFrameSnapshot() const;
    SpectralTimeline getTimelineSnapshot() const;
    int getActiveVoiceCount() const;
    float getLiveTrackMorph(int trackIdx) const
    {
        if(trackIdx < 0 || trackIdx >= int(kMaxSourceTracks)) return 0.0f;
        return liveTrackMorph_[(size_t)trackIdx].load(std::memory_order_relaxed);
    }

  private:
    void regenerateFrameNoLock();
    void refreshGeneratorFrameNoLock();
    void refreshWavetableRuntimeNoLock();
    void ensureSourceTracksNoLock();
    void rebuildTrackRenderStateNoLock(bool rebakeTables);
    void updateMetaTrackRenderParamsNoLock(uint32_t trackId);
    void publishSnapshotNoLock();
    void pushUndoSnapshotNoLock();
    void applyOutputSafetyBuffer(float *left, float *right, int numSamples);
    Voice *allocateVoice(int note);

    GeneratorBank generator;
    MatrixEngine matrix;
    EffectsChain effects;

    SourceState source {};
    AdsrParams globalAdsr;
    std::array<AdsrParams, kMaxAmpEnvs> ampEnvParams {};
    float globalGain = 0.30f;
    std::array<LfoParams, kMaxLfos> lfoParams {};
    std::array<MatrixEnvParams, kMaxModEnvs> matrixEnvParams {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    EffectsChainParams effectsParams {};
    std::vector<SourceGroupDef> groups_ {};

    struct UndoSnapshot
    {
        SourceGenParams gen;
        OperatorChain chain;
        AdsrParams adsr;
        std::array<AdsrParams, kMaxAmpEnvs> ampEnvParams {};
        std::array<LfoParams, kMaxLfos> lfoParams {};
        std::array<MatrixEnvParams, kMaxModEnvs> matrixEnvParams {};
        std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
        ChaosParams chaosParams {};
        ShapeSourceParams shapeSourceParams {};
        EffectsChainParams effectsParams {};
    };
    std::vector<UndoSnapshot> undoStack {};
    std::shared_ptr<const RenderSnapshot> renderSnapshot;

    std::array<std::atomic<float>, kMaxSourceTracks> liveTrackMorph_ {};

    mutable std::mutex paramMutex;
    double sampleRate = 48000.0;
    int controlSamplesLeft = 0;
    uint64_t startTickCounter = 0;
    uint32_t nextTrackId_ = 10;
    float outputSafetyGain = 1.0f;
    std::array<Voice, kMaxVoices> voices;
    std::unique_ptr<Voice::ModScratch> modScratch_ { std::make_unique<Voice::ModScratch>() };

    // ---- Per-strip bus rendering state (audio thread) ----
    static constexpr int kBusBlock = 4096;
    struct StereoBus { std::array<float, kBusBlock> l {}, r {}; };
    std::array<StereoBus, kMaxSourceTracks> trackBus_;            // one bus per source strip
    std::array<InsertChainState, kMaxSourceTracks> trackInsertState_;
    std::vector<StereoBus> groupBus_;                            // one bus per group
    std::vector<InsertChainState> groupInsertState_;
    // Matrix→insert modulation, per track: [insertIdx*kInsertModParams + param]
    std::array<std::array<float, kMaxModInserts * kInsertModParams>, kMaxSourceTracks> trackInsertMod_ {};
    void renderStripBuses(float *left, float *right, int numSamples,
                          const std::shared_ptr<const RenderSnapshot> &snap);

    struct MidiEvent
    {
        uint8_t type = 0;
        int note = 0;
        float velocity = 0.0f;
    };
    static constexpr int kEventQueueSize = 256;
    std::array<MidiEvent, kEventQueueSize> events_ {};
    std::atomic<int> eventWrite_ { 0 };
    std::atomic<int> eventRead_ { 0 };
    void pushEvent(const MidiEvent &e);
    bool popEvent(MidiEvent &out);
};

} // namespace synth
