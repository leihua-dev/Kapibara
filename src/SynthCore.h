#pragma once

#include "Generators.h"
#include "Effects.h"
#include "ResamplingEngine.h"
#include "SamplePlaybackEngine.h"
#include "CompositionModel.h"
#include "MatrixEngine.h"
#include "Operators.h"
#include "SpectralFrame.h"
#include "Voice.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace synth
{

// Source structure per architecture §2.2.1:
// Source = (GeneratorType, GeneratorParams, SpectralTimeline, OperatorChain, Metadata).
struct SourceState
{
    SourceGenParams gen;
    OperatorChain chain;
    SpectralTimeline timeline;        // live timeline fed to voices
    StaticSpectralFrame frame;        // first-frame preview for visualisation
    std::string presetName = "Init";
};

struct RenderSnapshot
{
    SpectralTimeline timeline;
    StaticSpectralFrame frame;
    GlobalAdsrParams globalAdsr;
    UnisonParams unison;
    float globalGain = 0.30f;
    std::array<LfoParams, kMaxLfos> lfoParams {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    EffectsChainParams effectsParams {};
    ResamplingEngineParams resamplingParams {};
    EffectsChainParams postResampleEffectsParams {};
    SamplePlaybackParams samplePlaybackParams {};
};

// Top-level synth.
class SynthCore
{
  public:
    SynthCore();

    void prepare(double sampleRate);
    void renderBlock(float *left, float *right, int numSamples);

    // MIDI events (thread-safe; called from GUI / MIDI threads).
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();

    // Source / Operator (working source = gen+chain). Apply -> Bake -> Commit.
    void setGeneratorParams(const SourceGenParams &p);
    SourceGenParams getGeneratorParams() const;

    void setOperatorChain(const OperatorChain &c);
    OperatorChain getOperatorChain() const;

    // Performance / global params.
    void setGlobalAdsr(const GlobalAdsrParams &a);
    GlobalAdsrParams getGlobalAdsr() const;
    void setGlobalGain(float g);
    float getGlobalGain() const;

    // Matrix.
    void setLfoParams(int idx, const LfoParams &p);
    LfoParams getLfoParams(int idx) const;
    void setMatrixRule(int idx, const MatrixRule &r);
    MatrixRule getMatrixRule(int idx) const;
    void setChaosParams(const ChaosParams &p);
    ChaosParams getChaosParams() const;
    void setShapeSourceParams(const ShapeSourceParams &p);
    ShapeSourceParams getShapeSourceParams() const;
    std::array<LfoParams, kMaxLfos> getLfoParamsSnapshot() const;
    std::array<MatrixRule, kMaxMatrixRules> getMatrixRulesSnapshot() const;

    // Final effects chain.
    void setEffectsParams(const EffectsChainParams &p);
    EffectsChainParams getEffectsParams() const;
    void setResamplingParams(const ResamplingEngineParams &p);
    ResamplingEngineParams getResamplingParams() const;
    bool importResampleBuffer(const std::string &path);
    void clearResampleBuffer();
    bool hasResampleBuffer() const;
    ResamplingDisplayState getResamplingDisplayState() const;
    void setPostResampleEffectsParams(const EffectsChainParams &p);
    EffectsChainParams getPostResampleEffectsParams() const;
    bool importSamplePlaybackFile(const std::string &path);
    void setCompositionProject(const CompositionProject &p);
    CompositionProject getCompositionProject() const;

    // Visualization snapshot.
    StaticSpectralFrame getFrameSnapshot() const;
    SpectralTimeline getTimelineSnapshot() const;

    int getActiveVoiceCount() const;

  private:
    void regenerateFrameNoLock();
    void publishSnapshotNoLock();
    void applyOutputSafetyBuffer(float *left, float *right, int numSamples);
    Voice *findVoiceForNote(int note);
    Voice *allocateVoice(int note);

    GeneratorBank generator;
    MatrixEngine matrix;
    EffectsChain effects;
    ResamplingEngine resampling;
    EffectsChain postResampleEffects;
    SamplePlaybackEngine samplePlayback;

    SourceState source {};
    GlobalAdsrParams globalAdsr;
    float globalGain = 0.30f;
    std::array<LfoParams, kMaxLfos> lfoParams {};
    std::array<MatrixRule, kMaxMatrixRules> matrixRules {};
    ChaosParams chaosParams {};
    ShapeSourceParams shapeSourceParams {};
    EffectsChainParams effectsParams {};
    ResamplingEngineParams resamplingParams {};
    EffectsChainParams postResampleEffectsParams {};
    CompositionProject compositionProject {};
    std::shared_ptr<const RenderSnapshot> renderSnapshot;

    mutable std::mutex paramMutex;

    double sampleRate = 48000.0;
    int controlSamplesLeft = 0;
    uint64_t startTickCounter = 0;
    float outputSafetyGain = 1.0f;

    std::array<Voice, kMaxVoices> voices;

    // MIDI event SPSC ring (small, mono->poly).
    struct MidiEvent
    {
        uint8_t type = 0; // 0=noteOn, 1=noteOff, 2=allOff
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
