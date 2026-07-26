#pragma once

#include "engine/SeedPatch.h"
#include "dsp/MasterEffects.h"
#include "dsp/Generators.h"
#include "dsp/InsertChain.h"
#include "engine/MatrixEngine.h"
#include "dsp/SpectralFrame.h"
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
    std::array<ModSlotParams, kMaxModSlots> modSlotParams {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    std::array<MaskGroup, kMaxMaskGroups> maskGroups {};
    // Lane counts + baked lane waveforms for the mask-group fans. Shared, not
    // copied: rebuilt only when a group's lane count or wavetable changes.
    std::shared_ptr<const MaskWaveBank> maskWaves;
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    MasterEffectsParams effectsParams {};
    std::vector<SourceGroupDef> groups {}; // per-strip group buses
    CompiledPerVoiceRoute route {};        // per-voice DAG (filters sum their inputs)
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
    // Lightweight update of the globally-shared per-voice filter slots: writes the
    // filter params onto every track and republishes the snapshot WITHOUT rebaking
    // wavetables — cheap enough for realtime knob drags.
    void setPerVoiceFiltersGlobal(const std::array<SourceFilterParams, kMaxPerVoiceFilters> &filters, int count);
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

    void setGlobalGain(float g);
    float getGlobalGain() const;

    void setModSlotParams(int idx, const ModSlotParams &p);
    void setModSlotParamsWithUndo(int idx, const ModSlotParams &p);
    ModSlotParams getModSlotParams(int idx) const;
    void setSeedModSlotParams(uint64_t seedPresetId, int idx, const ModSlotParams &p);
    void setSeedModSlotParamsWithUndo(uint64_t seedPresetId, int idx, const ModSlotParams &p);
    ModSlotParams getSeedModSlotParams(uint64_t seedPresetId, int idx) const;

    void setMatrixRule(int idx, const MatrixRule &r);
    void setMaskGroup(int idx, const MaskGroup &g);
    MaskGroup getMaskGroup(int idx) const;
    // How many real lanes group `idx`'s fan currently spans (16 for the discrete
    // slots, one per partial for a family). UI readout only.
    int getMaskGroupLanes(int idx) const;
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

    void setEffectsParams(const MasterEffectsParams &p);
    MasterEffectsParams getEffectsParams() const;
    void setSourceGroups(const std::vector<SourceGroupDef> &groups);
    // Per-voice routing DAG compiled from the UI route wires.
    void setCompiledRoute(const CompiledPerVoiceRoute &route);
    void setSeedEffectsParams(uint64_t seedPresetId, const MasterEffectsParams &p);
    MasterEffectsParams getSeedEffectsParams(uint64_t seedPresetId) const;

    // Conservative boundary for future DPF-native state/preset support.
    void setSeedPatch(const SeedPatch &patch);
    SeedPatch getSeedPatch() const;

    // Legacy name retained for compatibility with old callers; it restores the
    // active Seed parameter snapshot only, not a composition graph.
    bool undoCompositionChange();
    StaticSpectralFrame getFrameSnapshot() const;
    SpectralTimeline getTimelineSnapshot() const;
    int getActiveVoiceCount() const;
    // Returns [0,1] while a voice is actively playing this track, or a negative
    // sentinel when idle so the UI can fall back to the static knob value instead
    // of a stale frozen-in-time morph from whatever note played last.
    float getLiveTrackMorph(int trackIdx) const
    {
        if(trackIdx < 0 || trackIdx >= int(kMaxSourceTracks)) return -1.0f;
        return liveTrackMorph_[(size_t)trackIdx].load(std::memory_order_relaxed);
    }
    // Post-insert peak level of a strip bus (0..~1), for the UI level meters.
    float getTrackLevel(int trackIdx) const
    {
        if(trackIdx < 0 || trackIdx >= int(kMaxSourceTracks)) return 0.0f;
        return trackLevel_[(size_t)trackIdx].load(std::memory_order_relaxed);
    }

  private:
    void regenerateFrameNoLock();
    void refreshGeneratorFrameNoLock();
    void refreshWavetableRuntimeNoLock();
    void ensureSourceTracksNoLock();
    void rebuildTrackRenderStateNoLock(bool rebakeTables);
    void updateMetaTrackRenderParamsNoLock(uint32_t trackId);
    void publishSnapshotNoLock();
    void rebuildMaskWaveBankNoLock();
    void pushUndoSnapshotNoLock();
    void applyOutputSafetyBuffer(float *left, float *right, int numSamples);
    Voice *allocateVoice(int note);

    GeneratorBank generator;
    MatrixEngine matrix;
    MasterEffectsChain effects;

    SourceState source {};
    AdsrParams globalAdsr;
    std::array<AdsrParams, kMaxAmpEnvs> ampEnvParams {};
    float globalGain = 0.30f;
    std::array<ModSlotParams, kMaxModSlots> modSlotParams_ {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    std::array<MaskGroup, kMaxMaskGroups> maskGroups_ {};
    // Published mask-group fan bank + the key that decides when it must be
    // rebuilt. Frames are cached per group so a lane-count change costs a
    // pointer copy instead of a rebake. See rebuildMaskWaveBankNoLock.
    struct MaskWaveKey
    {
        uint8_t waveSource = 0;
        uint32_t trackId = 0;
        const void *framesData = nullptr;  // COW identity of the frame array
        int frameCount = 0;
        int lanes = kMaskGroupSlots;
    };
    std::shared_ptr<const MaskWaveBank> maskWaveBank_;
    std::array<MaskWaveKey, kMaxMaskGroups> maskWaveKey_ {};
    std::array<std::shared_ptr<const MaskWaveFrames>, kMaxMaskGroups> maskWaveFrames_ {};
    // Mirror of the bank's lane counts for the UI readout, which polls it every
    // repaint — going through paramMutex for that would put a lock in a draw path.
    std::array<std::atomic<int>, kMaxMaskGroups> maskGroupLanes_ {};
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    MasterEffectsParams effectsParams {};
    std::vector<SourceGroupDef> groups_ {};
    CompiledPerVoiceRoute compiledRoute_ {};

    struct UndoSnapshot
    {
        SourceGenParams gen;
        AdsrParams adsr;
        std::array<AdsrParams, kMaxAmpEnvs> ampEnvParams {};
        std::array<ModSlotParams, kMaxModSlots> modSlotParams {};
        std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
        ChaosParams chaosParams {};
        ShapeSourceParams shapeSourceParams {};
        MasterEffectsParams effectsParams {};
    };
    std::vector<UndoSnapshot> undoStack {};
    std::shared_ptr<const RenderSnapshot> renderSnapshot;

    std::array<std::atomic<float>, kMaxSourceTracks> liveTrackMorph_ {};
    std::array<std::atomic<float>, kMaxSourceTracks> trackLevel_ {};

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
    std::vector<StereoBus> groupBus_;                            // one bus per merge group
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
