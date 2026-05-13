#pragma once

#include "Generators.h"
#include "MatrixEngine.h"
#include "SpectralFrame.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace synth
{

// Single polyphonic Voice. Owns:
//   - runtime phase accumulator theta_i (audio rate)
//   - per-partial envelope (Attack/Decay/Sustain/Release/Idle)
//   - control-rate target buffers + per-sample increments for linear interp
//
// Architecture references: §0.3 (theta_i), §3 (Matrix), §5 (RenderState), §6 (control vs audio rate).
class Voice
{
  public:
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
    bool isReleasing() const { return releasing_; }
    int getNoteNumber() const { return noteNumber_; }
    uint64_t getStartTick() const { return startTick_; }
    float sourceTimeSeconds() const { return float(double(ageSamples_) / sampleRate_); }

    void noteOn(int midiNote, float velocity, const StaticSpectralFrame &frame,
                const UnisonParams &unison, uint64_t tick);
    void noteOff();
    void steal(); // immediate fast release

    // Recompute target ampSrc[i], freq[i], dPhase[i] from frame + matrix output.
    // Called at every control-rate boundary.
    void updateControl(const StaticSpectralFrame &frame,
                       const MatrixVoiceOutput &matrixOut,
                       const GlobalAdsrParams &adsr,
                       const UnisonParams &unison,
                       float globalGain,
                       int blockSize);

    // Audio-rate render. Adds to left/right buffers (mix into pre-zeroed output).
    void renderAdd(float *left, float *right, int numSamples);

    // Helpful for keytrack / matrix.
    float velocity() const { return velocity_; }
    float keyTrack01() const { return keyTrack01_; }
    float averageEnv() const { return avgEnv_; }
    uint32_t voiceRandomSeed() const { return rngSeed_; }

  private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kTwoPi = 6.28318530717958647692f;
    static constexpr float kMidiToHzA4 = 440.0f;

    void seedPhases(const StaticSpectralFrame &frame, const UnisonParams &unison);
    void updateUnisonLayout(const UnisonParams &unison);
    static float midiToHz(int midiNote)
    {
        return kMidiToHzA4 * std::pow(2.0f, (float(midiNote) - 69.0f) / 12.0f);
    }

    double sampleRate_ = 48000.0;
    bool idle_ = true;
    bool releasing_ = false;
    int noteNumber_ = -1;
    float velocity_ = 0.0f;
    float keyTrack01_ = 0.5f;
    float voiceF0_ = 440.0f;
    uint64_t startTick_ = 0;
    uint64_t ageSamples_ = 0;
    uint32_t rngSeed_ = 1u;

    int activeCount_ = 0;

    // Unison state. unisonCount_ in [1, kMaxUnison].
    int unisonCount_ = 1;
    std::array<float, kMaxUnison> unisonDetune_ {}; // multiplicative freq ratio per sub-voice
    std::array<float, kMaxUnison> unisonGainL_ {};  // pan-law L weight per sub-voice
    std::array<float, kMaxUnison> unisonGainR_ {};  // pan-law R weight per sub-voice
    float unisonNormGain_ = 1.0f;                   // sum normalisation, ~1/sqrt(U)

    // Audio-rate per partial state. Outer dim = unison sub-voice, inner = partial.
    std::array<std::array<float, kMaxPartials>, kMaxUnison> theta_ {};
    std::array<std::array<float, kMaxPartials>, kMaxUnison> phaseInit_ {};

    // Linearly interpolated targets. Each control block, *_target is set; per sample current += step.
    std::array<float, kMaxPartials> ampCur_ {};
    std::array<float, kMaxPartials> ampStep_ {};
    std::array<float, kMaxPartials> freqCur_ {};
    std::array<float, kMaxPartials> freqStep_ {};
    std::array<float, kMaxPartials> dPhaseCur_ {};
    std::array<float, kMaxPartials> dPhaseStep_ {};
    std::array<float, kMaxPartials> phaseDriftCur_ {};
    std::array<float, kMaxPartials> phaseDriftStep_ {};
    std::array<float, kMaxPartials> phaseJitterCur_ {};
    std::array<float, kMaxPartials> phaseJitterStep_ {};

    // Per-partial envelope state.
    std::array<PartialState, kMaxPartials> pState_ {};
    std::array<int, kMaxPartials> stageSample_ {};
    std::array<int, kMaxPartials> attackSamples_ {};
    std::array<int, kMaxPartials> decaySamples_ {};
    std::array<int, kMaxPartials> releaseSamples_ {};
    std::array<float, kMaxPartials> envValue_ {};
    std::array<float, kMaxPartials> attackInc_ {};
    std::array<float, kMaxPartials> decayInc_ {};
    std::array<float, kMaxPartials> releaseInc_ {};
    std::array<float, kMaxPartials> releaseFrom_ {};
    std::array<float, kMaxPartials> sustainLevel_ {};
    float sustain_ = 0.7f;
    EnvCurve attackCurve_ = EnvCurve::Exp;
    EnvCurve decayCurve_ = EnvCurve::Exp;
    EnvCurve releaseCurve_ = EnvCurve::Exp;
    float etaA_ = 4.0f;
    float etaD_ = 4.0f;
    float etaR_ = 4.0f;

    float avgEnv_ = 0.0f;
};

} // namespace synth
