#pragma once

#include "DistrhoPlugin.hpp"

#include "../../engine/SynthCore.h"

#include <atomic>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

struct WavetablePresetEntry
{
    std::string name;
    std::string path;
};

class KapibaraPlugin final : public Plugin
{
  public:
    KapibaraPlugin();

    void updateGlobalGain(float value);
    void updateAdsr(float attack, float decay, float sustain, float release, float curve);
    void updateGenerator(int partialCount, float inharmonic, int freqShape, int sourceCount, int unisonVoices,
                         float detune, float width, float phaseSpread);
    void updateGeneratorSource(int index, const synth::GeneratorSourceParams &source);
    uint32_t addSourceTrack(synth::SourceTrackType type, const char *name = nullptr);
    void removeSourceTrack(uint32_t trackId);
    void moveSourceTrack(uint32_t trackId, int newIndex);
    void updateSourceTrack(uint32_t trackId, const synth::SourceTrackParams &track);
    void updateSourceTrackMorphOnly(uint32_t trackId, float morph);
    void updateSourceTracks(const std::vector<synth::SourceTrackParams> &tracks);
    void setPartialEnabled(int index, bool enabled);
    void setPartialAmp(int index, float amp);
    void setPartialRatio(int index, float ratio);
    void updatePartialRuntime(int index, bool enabled, float ratio, float amp, float phase, float pan, float morph,
                              int warpMode, float warpAmount);
    void updatePartialSlot(int index, const synth::WavetablePartialSlot &slot);
    bool loadWavetableFrame(int partialIndex, int frameIndex, const char *path);
    void previewNoteOn(int midiNote, float velocity);
    void previewNoteOff(int midiNote);
    void panic();
    std::vector<std::string> presetNames() const;
    std::vector<WavetablePresetEntry> wavetablePresetEntries() const;
    bool saveUserPreset(const char *name = nullptr);
    bool loadUserPreset(const char *name = nullptr);
    bool deleteUserPreset(const char *name = nullptr);
    void resetUserPreset();
    const char *presetStatus() const;

    synth::SourceGenParams generatorParams() const;
    synth::AdsrParams adsrParams() const;
    synth::AdsrParams ampEnvParams(int index) const;
    synth::OperatorChain operatorChain() const;
    synth::LfoParams lfoParams(int index) const;
    synth::MatrixEnvParams matrixEnvParams(int index) const;
    synth::MatrixRule matrixRule(int index) const;
    synth::ChaosParams chaosParams() const;
    synth::ShapeSourceParams shapeSourceParams() const;
    synth::EffectsChainParams effectsParams() const;
    float globalGain() const;
    int activeVoiceCount() const;
    float sourceLiveMorph(int trackIndex) const;

    void updateLfo(int index, const synth::LfoParams &params);
    void updateMatrixEnv(int index, const synth::MatrixEnvParams &params);
    void updateAmpEnv(int index, const synth::AdsrParams &params);
    void updateMatrixRule(int index, const synth::MatrixRule &rule);
    void updateChaos(const synth::ChaosParams &params);
    void updateShapeSource(const synth::ShapeSourceParams &params);
    void updateEffects(const synth::EffectsChainParams &params);
    void updateOperatorChain(const synth::OperatorChain &chain);
    void updateSourceGroups(const std::vector<synth::SourceGroupDef> &groups);

  protected:
    const char *getLabel() const override;
    const char *getDescription() const override;
    const char *getMaker() const override;
    const char *getHomePage() const override;
    const char *getLicense() const override;
    uint32_t getVersion() const override;

    void initAudioPort(bool input, uint32_t index, AudioPort &port) override;
    void activate() override;
    void deactivate() override;
    void sampleRateChanged(double newSampleRate) override;
    void run(const float **inputs, float **outputs, uint32_t frames, const MidiEvent *midiEvents,
             uint32_t midiEventCount) override;

  private:
    void render(float **outputs, uint32_t frames);
    void prepareCore(double rate);
    void handleMidi(const MidiEvent &event);
    static void syncPartialCountEnabledState(synth::WavetableSeedParams &params);

    synth::SynthCore core_;
    std::atomic<double> sampleRate_ { 48000.0 };
    std::atomic<bool> prepared_ { false };
    mutable std::string presetStatus_ { "Select preset" };

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KapibaraPlugin)
};

END_NAMESPACE_DISTRHO
