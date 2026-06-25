#include "Voice.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace synth
{

namespace
{
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

inline float smoothStep(float edge0, float edge1, float x)
{
    if(edge1 <= edge0)
        return x >= edge1 ? 1.0f : 0.0f;
    float t = (x - edge0) / (edge1 - edge0);
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float warpTablePhase(float tablePosition, WavetableWarpMode mode, float amount)
{
    if(mode == WavetableWarpMode::None || std::abs(amount) < 1.0e-5f)
        return tablePosition;

    const float size = float(kWavetableSize);
    float x = tablePosition / size;
    x -= std::floor(x);
    const float centered = x * 2.0f - 1.0f;
    float y = x;
    switch(mode)
    {
        case WavetableWarpMode::Bend:
        {
            const float bend = 1.0f + 3.0f * std::abs(amount);
            y = amount >= 0.0f ? std::pow(x, bend) : 1.0f - std::pow(1.0f - x, bend);
            break;
        }
        case WavetableWarpMode::Squeeze:
        {
            const float squeeze = 1.0f - 0.85f * std::abs(amount);
            y = 0.5f + centered * 0.5f * squeeze;
            break;
        }
        case WavetableWarpMode::Skew:
            y = x + amount * x * (1.0f - x) * (x < 0.5f ? 1.0f : -1.0f);
            break;
        case WavetableWarpMode::None:
            break;
    }
    return y * size;
}

int wavetableMipLevelForFrequency(float frequencyHz, float sampleRate)
{
    const float maxHarmonic = (0.475f * sampleRate) / std::max(1.0f, frequencyHz);
    int level = 0;
    int harmonicLimit = kMaxWavetableHarmonics;
    while(level + 1 < kWavetableMipLevels && float(harmonicLimit) > maxHarmonic)
    {
        ++level;
        harmonicLimit = std::max(1, kMaxWavetableHarmonics >> level);
    }
    return level;
}

} // namespace

const std::array<float, Voice::kSineTableSize + 1> &Voice::sineTable()
{
    static const std::array<float, kSineTableSize + 1> table = [] {
        std::array<float, kSineTableSize + 1> values {};
        for(int i = 0; i < kSineTableSize; ++i)
            values[(size_t)i] = std::sin(kTwoPi * float(i) / float(kSineTableSize));
        values[(size_t)kSineTableSize] = values[0];
        return values;
    }();

    return table;
}

float Voice::wrapTablePosition(float tablePosition)
{
    if(tablePosition >= float(kSineTableSize) || tablePosition < 0.0f)
    {
        tablePosition -= std::floor(tablePosition / float(kSineTableSize)) * float(kSineTableSize);
        if(tablePosition >= float(kSineTableSize))
            tablePosition -= float(kSineTableSize);
        else if(tablePosition < 0.0f)
            tablePosition += float(kSineTableSize);
    }

    return tablePosition;
}

float Voice::lookupSineTablePosition(float tablePosition)
{
    const auto &table = sineTable();
    return lookupTablePosition(table, tablePosition);
}

float Voice::lookupTablePosition(const std::array<float, kWavetableSize + 1> &table, float tablePosition)
{
    const float tablePos = wrapTablePosition(tablePosition);
    const int index = int(tablePos);
    const float frac = tablePos - float(index);
    const float a = table[(size_t)index];
    const float b = table[(size_t)index + 1u];
    return a + (b - a) * frac;
}

void Voice::prepare(double sr)
{
    (void)sineTable();
    sampleRate_ = sr > 1.0 ? sr : 48000.0;
    morphSmoothingCoeff_ = 1.0f - std::exp(-1.0f / (kMorphSmoothingSeconds * float(sampleRate_)));
    idle_ = true;
    releasing_ = false;
    controlsPrimed_ = false;
    activeCount_ = 0;
    avgEnv_ = 0.0f;
    for(auto &lane : thetaTable_)
        lane.fill(0.0f);
    phaseInitTable_.fill(0.0f);
    unisonDetuneRatio_.fill(1.0f);
    unisonGainL_.fill(0.0f);
    unisonGainR_.fill(0.0f);
    unisonPhaseOffsetTable_.fill(0.0f);
    ampCur_.fill(0.0f); ampStep_.fill(0.0f);
    freqCur_.fill(0.0f); freqStep_.fill(0.0f);
    dPhaseCur_.fill(0.0f); dPhaseStep_.fill(0.0f);
    dPanCur_.fill(0.0f); dPanStep_.fill(0.0f);
    dMorphCur_.fill(0.0f); dMorphStep_.fill(0.0f);
    dWarpCur_.fill(0.0f); dWarpStep_.fill(0.0f);
    phaseDriftCur_.fill(0.0f); phaseDriftStep_.fill(0.0f);
    phaseJitterCur_.fill(0.0f); phaseJitterStep_.fill(0.0f);
    morphCur_.fill(0.0f);
    morphTarget_.fill(0.0f);
    morphInitialized_.fill(false);
    requestedUnisonCount_ = 1;
    unisonCount_ = 1;
    unisonDetuneCents_ = 0.0f;
    unisonWidthStereo_ = 0.0f;
    wavetable_ = nullptr;
    renderTrackCount_ = 0;
    trackRuntime_ = {};
    ampEnv_ = AdsrRuntimeState {};
    for(auto &e : trackEnvState_)
        e = AdsrRuntimeState {};
    modEnvLevel_.fill(0.0f);
    for(auto &e : modEnvState_)
        e = AdsrRuntimeState {};
}

void Voice::updateUnisonLayout(const UnisonParams &unison)
{
    requestedUnisonCount_ = std::clamp(unison.voices, 1, kMaxUnison);
    unisonCount_ = requestedUnisonCount_;
    unisonDetuneCents_ = (unisonCount_ > 1) ? std::max(0.0f, unison.detuneCents) : 0.0f;
    unisonWidthStereo_ = (unisonCount_ > 1) ? clampf(unison.widthStereo, 0.0f, 1.0f) : 0.0f;
    const float invRoot = 1.0f / std::sqrt(float(std::max(1, unisonCount_)));
    for(int u = 0; u < kMaxUnison; ++u)
    {
        if(u >= unisonCount_)
        {
            unisonDetuneRatio_[(size_t)u] = 1.0f;
            unisonGainL_[(size_t)u] = 0.0f;
            unisonGainR_[(size_t)u] = 0.0f;
            continue;
        }

        const float pos = unisonCount_ > 1
                              ? (float(u) / float(unisonCount_ - 1)) * 2.0f - 1.0f
                              : 0.0f;
        const float cents = 0.5f * unisonDetuneCents_ * pos;
        unisonDetuneRatio_[(size_t)u] = std::pow(2.0f, cents / 1200.0f);

        const float pan = clampf(pos * unisonWidthStereo_, -1.0f, 1.0f);
        const float angle = (pan + 1.0f) * 0.25f * kPi;
        unisonGainL_[(size_t)u] = std::cos(angle) * invRoot;
        unisonGainR_[(size_t)u] = std::sin(angle) * invRoot;
    }
}

void Voice::updateUnisonPhaseOffsets(const UnisonParams &unison, int sourceIndex)
{
    const int U = std::max(1, unisonCount_);
    const float spread = (U > 1) ? clampf(unison.phaseSpread, 0.0f, 1.0f) : 0.0f;
    uint32_t seed = uint32_t(noteNumber_) * 2654435761u
                    ^ rngSeed_
                    ^ unison.phaseSeed
                    ^ uint32_t(sourceIndex + 1) * 0x9E3779B9u;
    for(int u = 0; u < kMaxUnison; ++u)
    {
        seed ^= uint32_t(u + 1) * 0x85EBCA6Bu;
        seed ^= seed >> 16;
        seed *= 0x7FEB352Du;
        seed ^= seed >> 15;
        seed *= 0x846CA68Bu;
        seed ^= seed >> 16;
        const float unit = float(seed & 0x00FFFFFFu) / float(0x00FFFFFFu);
        const float phase = (unit * 2.0f - 1.0f) * kPi;
        unisonPhaseOffsetTable_[(size_t)u] = (u < U) ? wrapTablePosition(spread * phase * kRadiansToTable) : 0.0f;
    }
}

void Voice::seedPhases(const StaticSpectralFrame &frame, const UnisonParams &unison)
{
    const uint32_t baseSeed = frame.phaseSeed
                              ^ (uint32_t(noteNumber_) * 2654435761u)
                              ^ rngSeed_;
    std::mt19937 rngShared(baseSeed);
    std::uniform_real_distribution<float> uni(0.0f, kTwoPi);

    // First pass: deterministic per-partial phase from the frame's PhaseInitMode.
    std::array<float, kMaxPartials> base {};
    for(int i = 0; i < kMaxPartials; ++i)
    {
        switch(frame.phaseInitMode)
        {
            case PhaseInitMode::Zero:        base[i] = 0.0f; break;
            case PhaseInitMode::Random:      base[i] = uni(rngShared); break;
            case PhaseInitMode::Locked:      base[i] = frame.phaseLocked[i]; break;
            case PhaseInitMode::Alternating: base[i] = (i & 1) ? kPi : 0.0f; break;
        }
    }

    const int U = std::max(1, unisonCount_);
    const float spread = (U > 1) ? clampf(unison.phaseSpread, 0.0f, 1.0f) : 0.0f;
    (void)spread;
    updateUnisonPhaseOffsets(unison, 0);
    for(int i = 0; i < kMaxPartials; ++i)
    {
        phaseInitTable_[i] = wrapTablePosition(base[i] * kRadiansToTable);
        if(i < kMaxWavetablePartials)
        {
            for(auto &lane : thetaTable_)
                lane[(size_t)i] = 0.0f;
        }
    }
}

void Voice::noteOn(int midiNote, float velocity, const StaticSpectralFrame &frame,
                   const AdsrParams &adsr,
                   const std::array<AdsrParams, kMaxAmpEnvs> &ampEnvs,
                   const std::array<MatrixEnvParams, kMaxModEnvs> &matrixEnvs,
                   const UnisonParams &unison, RenderQualityMode quality, uint64_t tick)
{
    static uint32_t s_voiceCounter = 0;
    rngSeed_ = ++s_voiceCounter;

    noteNumber_ = midiNote;
    velocity_ = clampf(velocity, 0.0f, 1.0f);
    voiceF0_ = midiToHz(midiNote);
    keyTrack01_ = clampf((float(midiNote) - 21.0f) / 87.0f, 0.0f, 1.0f);
    startTick_ = tick;
    ageSamples_ = 0;
    idle_ = false;
    releasing_ = false;
    controlsPrimed_ = false;
    morphInitialized_.fill(false);
    renderQuality_ = quality;
    sourceFilterStates_ = {};

    updateUnisonLayout(unison);
    seedPhases(frame, unison);
    activeCount_ = std::clamp(frame.partialCount, 1, kMaxWavetablePartials);

    ampEnv_ = AdsrRuntimeState {};
    frozenAdsr_ = adsr;
    sustain_ = clampf(frozenAdsr_.sustain, 0.0f, 1.0f);
    adsrCurve_ = clampf(frozenAdsr_.curve, 0.0f, 1.0f);
    const float fs = float(sampleRate_);
    ampEnv_.attackSamples = std::max(1, int(std::max(0.0f, frozenAdsr_.attack) * fs));
    ampEnv_.decaySamples = std::max(1, int(std::max(0.0f, frozenAdsr_.decay) * fs));
    ampEnv_.releaseSamples = std::max(1, int(std::max(0.0f, frozenAdsr_.release) * fs));
    if(ampEnv_.attackSamples <= 1)
    {
        ampEnv_.state = PartialState::Decay;
        ampEnv_.value = 1.0f;
    }
    else
    {
        ampEnv_.state = PartialState::Attack;
    }
    for(auto &e : trackEnvState_)
        e = AdsrRuntimeState {};
    modEnvParams_ = matrixEnvs;
    sharedAmpEnvParams_ = ampEnvs;
    for(int i = 0; i < kMaxAmpEnvs; ++i)
    {
        auto &state = sharedAmpEnvState_[(size_t)i];
        const auto &params = sharedAmpEnvParams_[(size_t)i];
        state = {};
        state.attackSamples = std::max(1, int(std::max(0.0f, params.attack) * fs));
        state.decaySamples = std::max(1, int(std::max(0.0f, params.decay) * fs));
        state.releaseSamples = std::max(1, int(std::max(0.0f, params.release) * fs));
        state.state = state.attackSamples <= 1 ? PartialState::Decay : PartialState::Attack;
        state.value = state.attackSamples <= 1 ? 1.0f : 0.0f;
    }
    for(int i = 0; i < kMaxModEnvs; ++i)
    {
        auto &e = modEnvState_[(size_t)i];
        const auto &p = modEnvParams_[(size_t)i];
        e = AdsrRuntimeState {};
        // Point-curve modulators play one cycle every 1/loopRateHz seconds (loop
        // mode repeats it, one-shot plays it once then holds). Legacy ADSR-style
        // envs (pointCount < 2) keep their attack-time behaviour.
        e.attackSamples = (p.pointCount >= 2)
            ? std::max(1, int(fs / std::max(0.01f, p.loopRateHz)))
            : std::max(1, int(std::max(0.0f, p.attack) * fs));
        e.decaySamples = std::max(1, int(std::max(0.0f, p.decay) * fs));
        e.releaseSamples = std::max(1, int(std::max(0.0f, p.release) * fs));
        if(e.attackSamples <= 1)
        {
            e.state = PartialState::Decay;
            e.value = 1.0f;
        }
        else
        {
            e.state = PartialState::Attack;
        }
    }
    // Per-voice LFOs retrigger their phase on note-on.
    lfoVoicePhase_.fill(0.0f);
    lfoVoiceLevel_.fill(0.0f);
    modEnvLevel_.fill(0.0f);

    for(int i = 0; i < kMaxPartials; ++i)
    {
        ampCur_[i] = 0.0f;
        freqCur_[i] = 0.0f;
        dPhaseCur_[i] = 0.0f;
        dPanCur_[i] = 0.0f;
        dMorphCur_[i] = 0.0f;
        dWarpCur_[i] = 0.0f;
        phaseDriftCur_[i] = 0.0f;
        phaseJitterCur_[i] = 0.0f;
        ampStep_[i] = 0.0f;
        freqStep_[i] = 0.0f;
        dPhaseStep_[i] = 0.0f;
        dPanStep_[i] = 0.0f;
        dMorphStep_[i] = 0.0f;
        dWarpStep_[i] = 0.0f;
        phaseDriftStep_[i] = 0.0f;
        phaseJitterStep_[i] = 0.0f;
    }
    avgEnv_ = 0.0f;
}

void Voice::noteOff()
{
    if(idle_)
        return;
    releasing_ = true;
    ampEnv_.releaseFrom = ampEnv_.value;
    ampEnv_.stageSample = 0;
    ampEnv_.state = PartialState::Release;
    for(auto &e : modEnvState_)
    {
        e.releaseFrom = e.value;
        e.stageSample = 0;
        e.state = PartialState::Release;
    }
    for(auto &e : sharedAmpEnvState_)
    {
        e.releaseFrom = e.value;
        e.stageSample = 0;
        e.state = PartialState::Release;
    }
    for(auto &e : trackEnvState_)
    {
        e.releaseFrom = e.value;
        e.stageSample = 0;
        e.state = PartialState::Release;
    }
}

void Voice::steal()
{
    ampEnv_.releaseFrom = ampEnv_.value;
    ampEnv_.releaseSamples = std::max(1, int(0.005 * sampleRate_));
    ampEnv_.stageSample = 0;
    ampEnv_.state = PartialState::Release;
    for(auto &e : modEnvState_)
    {
        e.releaseFrom = e.value;
        e.releaseSamples = ampEnv_.releaseSamples;
        e.stageSample = 0;
        e.state = PartialState::Release;
    }
    for(auto &e : sharedAmpEnvState_)
    {
        e.releaseFrom = e.value;
        e.releaseSamples = ampEnv_.releaseSamples;
        e.stageSample = 0;
        e.state = PartialState::Release;
    }
    for(auto &e : trackEnvState_)
    {
        e.releaseFrom = e.value;
        e.releaseSamples = ampEnv_.releaseSamples;
        e.stageSample = 0;
        e.state = PartialState::Release;
    }
    releasing_ = true;
}

void Voice::updateControl(const StaticSpectralFrame &frame,
                          const MatrixVoiceOutput &matrixOut,
                          const AdsrParams &adsr,
                          const std::array<AdsrParams, kMaxAmpEnvs> &ampEnvs,
                          const std::array<MatrixEnvParams, kMaxModEnvs> &matrixEnvs,
                          const std::array<LfoParams, kMaxLfos> &lfos,
                          const UnisonParams &unison,
                          const std::array<RenderTrackRuntime, kMaxSourceTracks> &trackRuntime,
                          int renderTrackCount,
                          RenderQualityMode quality,
                          float globalGain,
                          int blockSize)
{
    renderQuality_ = quality;
    trackRuntime_ = trackRuntime;
    renderTrackCount_ = std::clamp(renderTrackCount, 0, kMaxSourceTracks);
    sourceParams_ = {};
    for(int t = 0; t < renderTrackCount_; ++t)
        sourceParams_[(size_t)t] = trackRuntime_[(size_t)t].strip;
    const float fs = float(sampleRate_);
    const float invBlock = 1.0f / float(std::max(1, blockSize));
    const float fadeStart = 0.45f * fs;
    const float fadeEnd = 0.5f * fs;

    const float vGain = velocity_ * globalGain;
    (void)adsr;
    sharedAmpEnvParams_ = ampEnvs;
    for(int i = 0; i < kMaxAmpEnvs; ++i)
    {
        auto &state = sharedAmpEnvState_[(size_t)i];
        const auto &params = sharedAmpEnvParams_[(size_t)i];
        state.attackSamples = std::max(1, int(std::max(0.0f, params.attack) * fs));
        state.decaySamples = std::max(1, int(std::max(0.0f, params.decay) * fs));
        state.releaseSamples = std::max(1, int(std::max(0.0f, params.release) * fs));
    }
    modEnvParams_ = matrixEnvs;
    for(int i = 0; i < kMaxModEnvs; ++i)
    {
        auto &st = modEnvState_[(size_t)i];
        const auto &p = modEnvParams_[(size_t)i];
        const float curveSeconds = std::max(0.001f, p.attack + p.decay + p.release);
        st.attackSamples = std::max(1, int((p.pointCount >= 2 ? curveSeconds : std::max(0.0f, p.attack)) * fs));
        st.decaySamples = std::max(1, int(std::max(0.0f, p.decay) * fs));
        st.releaseSamples = std::max(1, int(std::max(0.0f, p.release) * fs));
    }

    // Per-voice LFOs (unified modulators): advance each phase by this control block
    // and sample its waveform. loop=true repeats; loop=false is a one-shot envelope.
    voiceLfoParams_ = lfos;
    for(int i = 0; i < kMaxLfos; ++i)
    {
        const auto &lp = voiceLfoParams_[(size_t)i];
        const float inc = std::max(0.0f, lp.frequencyHz) * float(blockSize) / fs;
        float ph = lfoVoicePhase_[(size_t)i] + inc;
        if(lp.loop) ph -= std::floor(ph);
        else        ph = std::min(ph, 1.0f);
        lfoVoicePhase_[(size_t)i] = ph;
        float x = ph + lp.phase0;
        if(lp.loop) x -= std::floor(x);
        else        x = std::clamp(x, 0.0f, 1.0f);
        lfoVoiceLevel_[(size_t)i] = Lfo::shapeOutput(lp, x);
    }

    // Legacy fixed-generator path still uses global unison. Source-track mode
    // switches layout per render track inside renderAdd().
    if(renderTrackCount_ <= 0)
        updateUnisonLayout(unison);

    const int waveCount = wavetable_ != nullptr ? wavetable_->partialCount : frame.partialCount;
    const int targetActiveCount = std::clamp(std::min(frame.partialCount, waveCount), 1, kMaxWavetablePartials);
    int renderActiveCount = targetActiveCount;
    if(controlsPrimed_ && targetActiveCount < activeCount_)
    {
        bool tailSilent = true;
        for(int i = targetActiveCount; i < activeCount_; ++i)
            if(std::abs(ampCur_[(size_t)i]) > 1.0e-6f)
            {
                tailSilent = false;
                break;
            }
        renderActiveCount = tailSilent ? targetActiveCount : activeCount_;
    }
    activeCount_ = std::clamp(renderActiveCount, 1, kMaxWavetablePartials);

    for(int i = 0; i < targetActiveCount; ++i)
    {
        const auto *wave = wavetable_ != nullptr ? &wavetable_->partials[(size_t)i] : nullptr;
        morphTarget_[(size_t)i] = wave != nullptr && wave->usesMetaWavetable
                                      ? clampf(wave->morph, 0.0f, 1.0f)
                                      : 0.0f;
        if(!morphInitialized_[(size_t)i])
        {
            morphCur_[(size_t)i] = morphTarget_[(size_t)i];
            morphInitialized_[(size_t)i] = true;
        }

        // f_i^final = f0 * nu_i * M_i^freq  (RelativeRatio) or nu_i * M_i^freq (AbsoluteHz)
        const float sourceFrequency = wave != nullptr && wave->usesMetaWavetable ? wave->ratio : frame.nu[i];
        float f = (frame.freqMode == FreqMode::RelativeRatio)
                      ? voiceF0_ * sourceFrequency
                      : frame.nu[i];
        f *= matrixOut.mFreq[i];
        if(f < 0.0f)
            f = 0.0f;
        const float nyqFade = 1.0f - smoothStep(fadeStart, fadeEnd, f);

        // a_i^src * gain * Nyquist fade * matrix amp.  The note ADSR is applied once at audio rate.
        const float srcAmp = frame.amp[i] * vGain * nyqFade * matrixOut.mAmp[i];
        const float dPhaseTarget = matrixOut.dPhase[i];
        const float dPanTarget = matrixOut.dPan[i];
        const float dMorphTarget = matrixOut.dMorph[i];
        const float dWarpTarget = matrixOut.dWarp[i];
        const float phaseDriftTarget = frame.phaseDriftHz[i];
        const float phaseJitterTarget = frame.phaseJitter[i];

        if(!controlsPrimed_)
        {
            ampCur_[i] = srcAmp;
            freqCur_[i] = f;
            dPhaseCur_[i] = dPhaseTarget;
            dPanCur_[i] = dPanTarget;
            dMorphCur_[i] = dMorphTarget;
            dWarpCur_[i] = dWarpTarget;
            phaseDriftCur_[i] = phaseDriftTarget;
            phaseJitterCur_[i] = phaseJitterTarget;
            ampStep_[i] = 0.0f;
            freqStep_[i] = 0.0f;
            dPhaseStep_[i] = 0.0f;
            dPanStep_[i] = 0.0f;
            dMorphStep_[i] = 0.0f;
            dWarpStep_[i] = 0.0f;
            phaseDriftStep_[i] = 0.0f;
            phaseJitterStep_[i] = 0.0f;
        }
        else
        {
            ampStep_[i] = (srcAmp - ampCur_[i]) * invBlock;
            freqStep_[i] = (f - freqCur_[i]) * invBlock;
            dPhaseStep_[i] = (dPhaseTarget - dPhaseCur_[i]) * invBlock;
            dPanStep_[i] = (dPanTarget - dPanCur_[i]) * invBlock;
            dMorphStep_[i] = (dMorphTarget - dMorphCur_[i]) * invBlock;
            dWarpStep_[i] = (dWarpTarget - dWarpCur_[i]) * invBlock;
            phaseDriftStep_[i] = (phaseDriftTarget - phaseDriftCur_[i]) * invBlock;
            phaseJitterStep_[i] = (phaseJitterTarget - phaseJitterCur_[i]) * invBlock;
        }
    }

    // Decay any inactive leftover gracefully. This keeps live PartialCount
    // reductions from hard-cutting existing note partials at the control block.
    for(int i = targetActiveCount; i < kMaxPartials; ++i)
    {
        ampStep_[i] = -ampCur_[i] * invBlock;
        freqStep_[i] = 0.0f;
        dPhaseStep_[i] = 0.0f;
        dPanStep_[i] = -dPanCur_[i] * invBlock;
        dMorphStep_[i] = -dMorphCur_[i] * invBlock;
        dWarpStep_[i] = -dWarpCur_[i] * invBlock;
        phaseDriftStep_[i] = -phaseDriftCur_[i] * invBlock;
        phaseJitterStep_[i] = -phaseJitterCur_[i] * invBlock;
    }
    controlsPrimed_ = true;
}

void Voice::renderAdd(float *left, float *right, int numSamples)
{
    if(idle_)
        return;

    if(numSamples > kMaxVoiceRenderBlockSamples)
    {
        int written = 0;
        while(written < numSamples)
        {
            const int chunk = std::min(numSamples - written, kMaxVoiceRenderBlockSamples);
            renderAdd(left + written, right + written, chunk);
            written += chunk;
        }
        return;
    }

    std::fill(serialRawL_.begin(), serialRawL_.begin() + numSamples, 0.0f);
    std::fill(serialRawR_.begin(), serialRawR_.begin() + numSamples, 0.0f);
    beginPartialRender(numSamples);
    const bool trackMode = wavetable_ != nullptr && wavetable_->trackCount > 0;
    const int sourceCount = trackMode
                                ? std::clamp(renderTrackCount_ > 0 ? renderTrackCount_ : wavetable_->trackCount, 1, kMaxSourceTracks)
                                : 1;

    // Detect whether any track uses source modulation (needs the two-pass path).
    bool anyMod = false;
    if(trackMode && modScratch_ != nullptr)
        for(int t = 0; t < sourceCount && !anyMod; ++t)
            for(const auto &m : trackRuntime_[(size_t)t].mods)
                if(m.enabled && m.sourceTrack >= 0) { anyMod = true; break; }

    // Accumulate a finished (gain-applied) track buffer into its strip bus (tanh soft clip);
    // if no buses are set (legacy non-track path), fall back to the mixed left/right.
    const bool useBus = trackMode && busL_ != nullptr && busR_ != nullptr;
    const auto flushTrack = [&](int source, const float *L, const float *R) {
        if(useBus)
        {
            float *bl = busL_[source]; float *br = busR_[source];
            for(int s = 0; s < numSamples; ++s) { bl[s] += std::tanh(L[s]); br[s] += std::tanh(R[s]); }
        }
        else
        {
            for(int s = 0; s < numSamples; ++s) { serialRawL_[(size_t)s] += L[s]; serialRawR_[(size_t)s] += R[s]; }
        }
    };

    if(!anyMod)
    {
        // Fast single-pass path (no cross-track modulation).
        for(int source = 0; source < sourceCount; ++source)
        {
            const int begin = trackMode ? std::clamp(wavetable_->trackBegin[(size_t)source], 0, activeCount_) : 0;
            const int end = trackMode ? std::clamp(wavetable_->trackEnd[(size_t)source], begin, activeCount_) : activeCount_;
            if(begin >= end)
                continue;
            if(trackMode && trackRuntime_[(size_t)source].outputMode == SourceTrackOutputMode::ModOnly)
                continue;
            if(trackMode)
            {
                updateUnisonLayout(trackRuntime_[(size_t)source].unison);
                updateUnisonPhaseOffsets(trackRuntime_[(size_t)source].unison, source);
            }
            currentRenderTrack_ = source;
            std::fill(sourceRawL_.begin(), sourceRawL_.begin() + numSamples, 0.0f);
            std::fill(sourceRawR_.begin(), sourceRawR_.begin() + numSamples, 0.0f);
            renderPartialRangeRaw(sourceRawL_.data(), sourceRawR_.data(), numSamples, begin, end);
            processSourceFilter(sourceRawL_.data(), sourceRawR_.data(), numSamples, sourceParams_[(size_t)source],
                                sourceFilterStates_[(size_t)source]);
            flushTrack(source, sourceRawL_.data(), sourceRawR_.data());
        }
        if(useBus) finalizeBlock(numSamples);
        else       finishPartialRender(left, right, serialRawL_.data(), serialRawR_.data(), numSamples);
        return;
    }

    // Two-pass path with cross-track modulation.
    // Render order: a carrier needs its modulators rendered first (FM/PM/Sync inject at
    // oscillation time). Dependency graph is acyclic (UI prevents loops), so topo-sort.
    std::array<int, kMaxSourceTracks> order {};
    int orderN = 0;
    {
        std::array<uint8_t, kMaxSourceTracks> mark {}; // 0=unvisited,1=in-progress,2=done
        // iterative DFS post-order
        for(int root = 0; root < sourceCount; ++root)
        {
            if(mark[(size_t)root]) continue;
            std::array<int, kMaxSourceTracks * 2> stk {};
            int sp = 0; stk[sp++] = root;
            while(sp > 0)
            {
                const int t = stk[sp - 1];
                if(mark[(size_t)t] == 2) { --sp; continue; }
                if(mark[(size_t)t] == 1) { mark[(size_t)t] = 2; if(orderN < kMaxSourceTracks) order[orderN++] = t; --sp; continue; }
                mark[(size_t)t] = 1;
                for(const auto &m : trackRuntime_[(size_t)t].mods)
                    if(m.enabled && m.sourceTrack >= 0 && m.sourceTrack < sourceCount
                       && mark[(size_t)m.sourceTrack] == 0 && sp < int(stk.size()))
                        stk[sp++] = m.sourceTrack;
            }
        }
    }

    std::array<bool, kMaxSourceTracks> rendered {};
    for(int oi = 0; oi < orderN; ++oi)
    {
        const int source = order[oi];
        float *bL = modScratch_->bufL[(size_t)source].data();
        float *bR = modScratch_->bufR[(size_t)source].data();
        std::fill(bL, bL + numSamples, 0.0f);
        std::fill(bR, bR + numSamples, 0.0f);
        const int begin = std::clamp(wavetable_->trackBegin[(size_t)source], 0, activeCount_);
        const int end = std::clamp(wavetable_->trackEnd[(size_t)source], begin, activeCount_);
        if(begin >= end)
            continue;

        // Build phase-mod (FM/PM) buffer and pick a hard-sync source from this track's mods.
        const float invSr = 1.0f / float(sampleRate_);
        bool hasPm = false;
        const float *syncBuf = nullptr;
        std::fill(pmScratch_.begin(), pmScratch_.begin() + numSamples, 0.0f);
        for(const auto &m : trackRuntime_[(size_t)source].mods)
        {
            if(!m.enabled || m.sourceTrack < 0 || m.sourceTrack >= sourceCount || !rendered[(size_t)m.sourceTrack])
                continue;
            const float *mL = modScratch_->bufL[(size_t)m.sourceTrack].data();
            const float *mR = modScratch_->bufR[(size_t)m.sourceTrack].data();
            const float depth = clampf(m.depth, 0.0f, 1.0f);
            if(m.type == SourceModType::PM)
            {
                const float idx = depth * 24.0f; // radians at full depth (wide range)
                for(int s = 0; s < numSamples; ++s)
                    pmScratch_[(size_t)s] += idx * 0.5f * (mL[s] + mR[s]);
                hasPm = true;
            }
            else if(m.type == SourceModType::FM)
            {
                const float devHz = depth * 8000.0f; // peak deviation (wide range)
                double acc = 0.0;
                for(int s = 0; s < numSamples; ++s)
                {
                    acc += double(devHz) * 0.5 * double(mL[s] + mR[s]) * double(invSr) * 6.2831853;
                    pmScratch_[(size_t)s] += float(acc);
                }
                hasPm = true;
            }
            else if(m.type == SourceModType::HardSync)
            {
                for(int s = 0; s < numSamples; ++s)
                    syncMonoScratch_[(size_t)s] = 0.5f * (mL[s] + mR[s]);
                syncBuf = syncMonoScratch_.data();
            }
        }

        updateUnisonLayout(trackRuntime_[(size_t)source].unison);
        updateUnisonPhaseOffsets(trackRuntime_[(size_t)source].unison, source);
        currentRenderTrack_ = source;
        renderPartialRangeRaw(bL, bR, numSamples, begin, end,
                              hasPm ? pmScratch_.data() : nullptr, syncBuf);
        // gain-PRE filter (modulator tap is here, before strip gain/pan)
        processSourceFilter(bL, bR, numSamples, sourceParams_[(size_t)source],
                            sourceFilterStates_[(size_t)source], /*applyGainPan=*/false);
        // amplitude-domain mods (AM / Ring) read other tracks' gain-pre signal
        for(const auto &m : trackRuntime_[(size_t)source].mods)
        {
            if(!m.enabled || m.sourceTrack < 0 || m.sourceTrack >= sourceCount || !rendered[(size_t)m.sourceTrack])
                continue;
            if(m.type != SourceModType::AM && m.type != SourceModType::RingMod)
                continue;
            applySourceMod(bL, bR, modScratch_->bufL[(size_t)m.sourceTrack].data(),
                           modScratch_->bufR[(size_t)m.sourceTrack].data(),
                           numSamples, m.type, clampf(m.depth, 0.0f, 1.0f));
        }
        rendered[(size_t)source] = true;
    }

    // Mix audible tracks: apply strip gain/pan, then flush to the strip bus.
    for(int source = 0; source < sourceCount; ++source)
    {
        if(!rendered[(size_t)source])
            continue;
        if(trackRuntime_[(size_t)source].outputMode == SourceTrackOutputMode::ModOnly)
            continue;  // usable as a modulator but not summed to output
        float *bL = modScratch_->bufL[(size_t)source].data();
        float *bR = modScratch_->bufR[(size_t)source].data();
        std::copy(bL, bL + numSamples, sourceRawL_.begin());
        std::copy(bR, bR + numSamples, sourceRawR_.begin());
        applyGainPan(sourceRawL_.data(), sourceRawR_.data(), numSamples, sourceParams_[(size_t)source]);
        flushTrack(source, sourceRawL_.data(), sourceRawR_.data());
    }
    if(useBus) finalizeBlock(numSamples);
    else       finishPartialRender(left, right, serialRawL_.data(), serialRawR_.data(), numSamples);
}

// Amplitude-domain source modulation (AM / Ring), in place on the carrier.
void Voice::applySourceMod(float *cL, float *cR, const float *mL, const float *mR,
                           int numSamples, SourceModType type, float depth)
{
    const float d = depth * 2.0f; // more sensitive
    const auto shape = [&](float c, float m) -> float {
        switch(type)
        {
            case SourceModType::AM:      return c * (1.0f + d * m);
            case SourceModType::RingMod: return c * (1.0f - depth + d * m);
            default: return c;
        }
    };
    for(int s = 0; s < numSamples; ++s)
    {
        cL[s] = shape(cL[s], mL[s]);
        cR[s] = shape(cR[s], mR[s]);
    }
}

void Voice::beginPartialRender(int numSamples)
{
    if(idle_)
        return;

    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    auto advanceAdsr = [](AdsrRuntimeState &st,
                          float sustain,
                          float curveA, float curveD, float curveR) -> float {
        switch(st.state)
        {
            case PartialState::Attack:
            {
                const float tau = float(st.stageSample) / float(std::max(1, st.attackSamples));
                st.value = adsrCurveEval(tau, curveA);
                ++st.stageSample;
                if(st.stageSample >= st.attackSamples)
                {
                    st.value = 1.0f;
                    st.stageSample = 0;
                    st.state = PartialState::Decay;
                }
                break;
            }
            case PartialState::Decay:
            {
                const float tau = float(st.stageSample) / float(std::max(1, st.decaySamples));
                const float f = adsrCurveEval(tau, curveD);
                st.value = sustain + (1.0f - sustain) * (1.0f - f);
                ++st.stageSample;
                if(st.stageSample >= st.decaySamples)
                {
                    st.value = sustain;
                    st.stageSample = 0;
                    st.state = PartialState::Sustain;
                }
                break;
            }
            case PartialState::Sustain:
                st.value = sustain;
                break;
            case PartialState::Release:
            {
                const float tau = float(st.stageSample) / float(std::max(1, st.releaseSamples));
                const float f = adsrCurveEval(tau, curveR);
                st.value = st.releaseFrom * (1.0f - f);
                ++st.stageSample;
                if(st.stageSample >= st.releaseSamples)
                {
                    st.value = 0.0f;
                    st.stageSample = 0;
                    st.state = PartialState::Idle;
                }
                break;
            }
            case PartialState::Idle:
                st.value = 0.0f;
                break;
        }
        return st.value;
    };

    auto advanceMatrixEnv = [](AdsrRuntimeState &st, const MatrixEnvParams &p) -> float {
        if(p.pointCount >= 2)
        {
            switch(st.state)
            {
                case PartialState::Attack:
                {
                    const float tau = float(st.stageSample) / float(std::max(1, st.attackSamples));
                    st.value = matrixEnvBreakpointEval(p, tau);
                    ++st.stageSample;
                    if(st.stageSample >= st.attackSamples)
                    {
                        st.stageSample = 0;
                        if(!p.loop)  // loop mode replays the curve instead of holding
                        {
                            st.value = matrixEnvBreakpointEval(p, 1.0f);
                            st.state = PartialState::Sustain;
                        }
                    }
                    break;
                }
                case PartialState::Decay:
                case PartialState::Sustain:
                    st.value = matrixEnvBreakpointEval(p, 1.0f);
                    break;
                case PartialState::Release:
                {
                    const float tau = float(st.stageSample) / float(std::max(1, st.releaseSamples));
                    const float endValue = matrixEnvBreakpointEval(p, 1.0f);
                    st.value = st.releaseFrom + (endValue - st.releaseFrom) * tau;
                    ++st.stageSample;
                    if(st.stageSample >= st.releaseSamples)
                    {
                        st.value = endValue;
                        st.stageSample = 0;
                        st.state = PartialState::Idle;
                    }
                    break;
                }
                case PartialState::Idle:
                    st.value = matrixEnvBreakpointEval(p, 1.0f);
                    break;
            }
            return st.value;
        }

        const float sustain = clampf(p.sustain, 0.0f, 1.0f);
        switch(st.state)
        {
            case PartialState::Attack:
            {
                const float tau = float(st.stageSample) / float(std::max(1, st.attackSamples));
                st.value = envCurveEval(p.attackCurve, tau, p.etaA);
                ++st.stageSample;
                if(st.stageSample >= st.attackSamples)
                {
                    st.value = 1.0f;
                    st.stageSample = 0;
                    st.state = PartialState::Decay;
                }
                break;
            }
            case PartialState::Decay:
            {
                const float tau = float(st.stageSample) / float(std::max(1, st.decaySamples));
                const float f = envCurveEval(p.decayCurve, tau, p.etaD);
                st.value = sustain + (1.0f - sustain) * (1.0f - f);
                ++st.stageSample;
                if(st.stageSample >= st.decaySamples)
                {
                    st.value = sustain;
                    st.stageSample = 0;
                    st.state = PartialState::Sustain;
                }
                break;
            }
            case PartialState::Sustain:
                st.value = sustain;
                break;
            case PartialState::Release:
            {
                const float tau = float(st.stageSample) / float(std::max(1, st.releaseSamples));
                const float f = envCurveEval(p.releaseCurve, tau, p.etaR);
                st.value = st.releaseFrom * (1.0f - f);
                ++st.stageSample;
                if(st.stageSample >= st.releaseSamples)
                {
                    st.value = 0.0f;
                    st.stageSample = 0;
                    st.state = PartialState::Idle;
                }
                break;
            }
            case PartialState::Idle:
                st.value = 0.0f;
                break;
        }
        return st.value;
    };

    for(int s = 0; s < numSamples; ++s)
    {
        globalEnvScratch_[(size_t)s] = advanceAdsr(ampEnv_, sustain_, adsrCurve_, adsrCurve_, adsrCurve_);
        for(int e = 0; e < kMaxModEnvs; ++e)
        {
            const auto &p = modEnvParams_[(size_t)e];
            const float raw = advanceMatrixEnv(modEnvState_[(size_t)e], p);
            envScratch_[(size_t)e][(size_t)s] = raw;
            modEnvLevel_[(size_t)e] = raw; // ENVs always active (no enable gate)
        }
        for(int e = 0; e < kMaxAmpEnvs; ++e)
        {
            const auto &params = sharedAmpEnvParams_[(size_t)e];
            ampEnvScratch_[(size_t)e][(size_t)s] =
                advanceAdsr(sharedAmpEnvState_[(size_t)e], clampf(params.sustain, 0.0f, 1.0f),
                            clampf(params.curveA, 0.0f, 1.0f), clampf(params.curveD, 0.0f, 1.0f),
                            clampf(params.curveR, 0.0f, 1.0f));
        }
        const int trackCount = renderTrackCount_ > 0
                                   ? renderTrackCount_
                                   : (wavetable_ != nullptr ? std::clamp(wavetable_->trackCount, 0, kMaxSourceTracks) : 0);
        for(int t = 0; t < kMaxSourceTracks; ++t)
        {
            if(t < trackCount && renderTrackCount_ > 0)
            {
                const int envIndex = std::clamp(trackRuntime_[(size_t)t].ampEnvIndex, 0, kMaxAmpEnvs - 1);
                trackEnvScratch_[(size_t)t][(size_t)s] = ampEnvScratch_[(size_t)envIndex][(size_t)s];
            }
            else if(t < trackCount)
            {
                trackEnvScratch_[(size_t)t][(size_t)s] = globalEnvScratch_[(size_t)s];
            }
            else
            {
                trackEnvScratch_[(size_t)t][(size_t)s] = 1.0f;
            }
        }
    }
}

void Voice::renderPartialRangeRaw(float *left, float *right, int numSamples, int partialBegin, int partialEnd,
                                  const float *pmBuffer, const float *syncBuffer)
{
    if(idle_)
        return;

    const float invSr = 1.0f / float(sampleRate_);
    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    partialBegin = std::clamp(partialBegin, 0, activeCount_);
    partialEnd = std::clamp(partialEnd, partialBegin, activeCount_);
    float prevSync = 0.0f;  // previous modulator sample for hard-sync zero-cross detection

    for(int s = 0; s < numSamples; ++s)
    {
        float sumL = 0.0f;
        float sumR = 0.0f;
        const auto sampleAge = ageSamples_ + uint64_t(s);
        const int U = std::max(1, unisonCount_);
        // Hard sync: on the modulator's upward zero-crossing, reset carrier phases.
        if(syncBuffer != nullptr)
        {
            const float cur = syncBuffer[s];
            if(prevSync < 0.0f && cur >= 0.0f)
                for(int i = partialBegin; i < partialEnd; ++i)
                    for(int u = 0; u < U; ++u)
                        thetaTable_[(size_t)u][(size_t)i] = 0.0f;
            prevSync = cur;
        }
        const float pmTableOffset = pmBuffer != nullptr ? pmBuffer[s] * kRadiansToTable : 0.0f;
        for(int i = partialBegin; i < partialEnd; ++i)
        {
            const auto *wave = wavetable_ != nullptr ? &wavetable_->partials[(size_t)i] : nullptr;
            const bool useMetaWavetable = wave != nullptr && wave->usesMetaWavetable;
            if(useMetaWavetable)
                morphCur_[(size_t)i] += (morphTarget_[(size_t)i] - morphCur_[(size_t)i]) * morphSmoothingCoeff_;
            if(wave != nullptr && !wave->enabled && std::abs(ampCur_[(size_t)i]) < 1.0e-9f)
                continue;

            const bool trackMode = wavetable_ != nullptr && wavetable_->trackCount > 0;
            const float trackEnv =
                trackEnvScratch_[(size_t)std::clamp(currentRenderTrack_, 0, kMaxSourceTracks - 1)][(size_t)s];
            const float masterEnv = trackMode ? 1.0f : globalEnvScratch_[(size_t)s];
            const float aFinal = ampCur_[i] * masterEnv * trackEnv;
            if(std::abs(aFinal) < 1.0e-9f && std::abs(ampStep_[i]) < 1.0e-12f)
                continue;
            const float jitterPhase =
                phaseJitterCur_[i]
                * std::sin(0.00073f * float(sampleAge + 1u) * float(i + 3)
                           + 1.618f * float(rngSeed_ & 0xffu));

            const float fBase = std::max(0.0f, freqCur_[i] + phaseDriftCur_[i]);
            const float morph = useMetaWavetable
                                    ? clampf(morphCur_[(size_t)i] + dMorphCur_[(size_t)i], 0.0f, 1.0f)
                                    : 0.0f;
            const int frameCount = useMetaWavetable ? std::clamp(wave->frameCount, 1, kMaxWavetableFrames) : 1;
            const float framePos = morph * float(std::max(0, frameCount - 1));
            const int frameA = std::clamp(int(framePos), 0, frameCount - 1);
            const int frameB = std::min(frameA + 1, frameCount - 1);
            const float frameFrac = framePos - float(frameA);

            for(int u = 0; u < U; ++u)
            {
                const float phaseTable = thetaTable_[(size_t)u][(size_t)i]
                                       + phaseInitTable_[i]
                                       + unisonPhaseOffsetTable_[(size_t)u]
                                       + (dPhaseCur_[i] + jitterPhase) * kRadiansToTable
                                       + pmTableOffset;
                const float warped = useMetaWavetable
                                         ? warpTablePhase(phaseTable, wave->warpMode,
                                                          clampf(wave->warpAmount + dWarpCur_[(size_t)i], -1.0f, 1.0f))
                                         : phaseTable;
                float osc = 0.0f;
                if(useMetaWavetable)
                {
                    const float oscillatorHz = fBase * unisonDetuneRatio_[(size_t)u];
                    const int mipLevel = wave->mipTables
                                             ? wavetableMipLevelForFrequency(oscillatorHz, float(sampleRate_))
                                             : 0;
                    if(wave->mipTables)
                    {
                        const auto &frameTableA = (*wave->mipTables)[(size_t)mipLevel][(size_t)frameA];
                        const auto &frameTableB = (*wave->mipTables)[(size_t)mipLevel][(size_t)frameB];
                        const float a = lookupTablePosition(frameTableA, warped);
                        const float b = lookupTablePosition(frameTableB, warped);
                        osc = a + (b - a) * frameFrac;
                    }
                    else if(wave->tables)
                    {
                        const auto &frameTableA = (*wave->tables)[(size_t)frameA];
                        const auto &frameTableB = (*wave->tables)[(size_t)frameB];
                        const float a = lookupTablePosition(frameTableA, warped);
                        const float b = lookupTablePosition(frameTableB, warped);
                        osc = a + (b - a) * frameFrac;
                    }
                    else
                    {
                        osc = lookupSineTablePosition(warped);
                    }
                }
                else
                {
                    osc = lookupSineTablePosition(warped + kCosTableOffset);
                }

                const float pan = clampf((wave != nullptr ? wave->pan : 0.0f) + dPanCur_[(size_t)i], -1.0f, 1.0f);
                const float panAngle = (pan + 1.0f) * 0.25f * kPi;
                const float panL = std::cos(panAngle);
                const float panR = std::sin(panAngle);
                const float v = aFinal * osc;
                sumL += v * unisonGainL_[(size_t)u] * panL;
                sumR += v * unisonGainR_[(size_t)u] * panR;

                thetaTable_[(size_t)u][(size_t)i] += float(kSineTableSize) * fBase * unisonDetuneRatio_[(size_t)u] * invSr;
                while(thetaTable_[(size_t)u][(size_t)i] >= float(kSineTableSize))
                    thetaTable_[(size_t)u][(size_t)i] -= float(kSineTableSize);
            }

            // Linear interpolation toward control targets.
            ampCur_[i] += ampStep_[i];
            freqCur_[i] += freqStep_[i];
            dPhaseCur_[i] += dPhaseStep_[i];
            dPanCur_[i] += dPanStep_[i];
            dMorphCur_[i] += dMorphStep_[i];
            dWarpCur_[i] += dWarpStep_[i];
            phaseDriftCur_[i] += phaseDriftStep_[i];
            phaseJitterCur_[i] += phaseJitterStep_[i];
        }

        left[(size_t)s] += sumL;
        right[(size_t)s] += sumR;
    }
}

void Voice::finishPartialRender(float *left, float *right, const float *rawLeft, const float *rawRight, int numSamples)
{
    if(idle_)
        return;
    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    for(int s = 0; s < numSamples; ++s)
    {
        left[s] += std::tanh(rawLeft[s]);
        right[s] += std::tanh(rawRight[s]);
    }
    finalizeBlock(numSamples);
}

void Voice::finalizeBlock(int numSamples)
{
    if(idle_)
        return;
    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    ageSamples_ += (uint64_t)numSamples;

    // Idle detection at end of block. In source-track mode the per-track amp
    // envelopes own voice lifetime; the legacy master ADSR is only used by the
    // old fixed generator path.
    const int trackCount = renderTrackCount_ > 0
                               ? renderTrackCount_
                               : (wavetable_ != nullptr ? std::clamp(wavetable_->trackCount, 0, kMaxSourceTracks) : 0);
    if(trackCount > 0)
    {
        float envSum = 0.0f;
        bool anyActive = false;
        for(int t = 0; t < trackCount; ++t)
        {
            const int envIndex = renderTrackCount_ > 0
                                     ? std::clamp(trackRuntime_[(size_t)t].ampEnvIndex, 0, kMaxAmpEnvs - 1)
                                     : 0;
            envSum += sharedAmpEnvState_[(size_t)envIndex].value;
            if(sharedAmpEnvState_[(size_t)envIndex].state != PartialState::Idle)
                anyActive = true;
        }
        avgEnv_ = envSum / float(std::max(1, trackCount));
        if(!anyActive)
        {
            idle_ = true;
            releasing_ = false;
        }
        return;
    }

    avgEnv_ = ampEnv_.value;
    if(ampEnv_.state == PartialState::Idle)
    {
        idle_ = true;
        releasing_ = false;
    }
}

void Voice::applyGainPan(float *left, float *right, int numSamples, const GeneratorSourceParams &source)
{
    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    const float gain = clampf(source.gain, 0.0f, 2.0f);
    const float pan = clampf(source.pan, -1.0f, 1.0f);
    const float gainL = gain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    const float gainR = gain * (pan >= 0.0f ? 1.0f : 1.0f + pan);
    for(int s = 0; s < numSamples; ++s) { left[s] *= gainL; right[s] *= gainR; }
}

void Voice::processSourceFilter(float *left, float *right, int numSamples, const GeneratorSourceParams &source,
                                SourceFilterRuntime &state, bool applyGainPanFlag)
{
    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    const auto &filter = source.filter;
    const float mix = filter.enabled && filter.topology != SourceFilterTopology::Bypass
                          ? clampf(filter.mix, 0.0f, 1.0f)
                          : 0.0f;
    const float cutoff = clampf(filter.cutoffHz, 20.0f, float(sampleRate_ * 0.45));
    const float g = clampf(1.0f - std::exp(-kTwoPi * cutoff / float(sampleRate_)), 0.001f, 0.98f);
    const float resonance = clampf(filter.resonance, 0.0f, 0.95f);
    const float drive = clampf(filter.drive, 0.1f, 8.0f);
    const float feedback = clampf(filter.feedback, 0.0f, 0.95f);
    const float gain = applyGainPanFlag ? clampf(source.gain, 0.0f, 2.0f) : 1.0f;
    const float pan = applyGainPanFlag ? clampf(source.pan, -1.0f, 1.0f) : 0.0f;
    const float gainL = gain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    const float gainR = gain * (pan >= 0.0f ? 1.0f : 1.0f + pan);

    const auto processOne = [&](float x, bool rightChannel) {
        float &lp1 = rightChannel ? state.lp1R : state.lp1L;
        float &lp2 = rightChannel ? state.lp2R : state.lp2L;
        float &lp3 = rightChannel ? state.lp3R : state.lp3L;
        float &lp4 = rightChannel ? state.lp4R : state.lp4L;
        float &bp = rightChannel ? state.bpR : state.bpL;
        switch(filter.topology)
        {
            case SourceFilterTopology::Bypass:
                return x;
            case SourceFilterTopology::OnePoleLowPass:
                lp1 += g * (x - lp1);
                return lp1;
            case SourceFilterTopology::TwoPoleStateVariable:
            {
                const float hp = x - lp1 - resonance * bp;
                bp += g * hp;
                lp1 += g * bp;
                return lp1;
            }
            case SourceFilterTopology::FourPoleCascade:
            {
                const float input = x - resonance * lp4;
                lp1 += g * (input - lp1);
                lp2 += g * (lp1 - lp2);
                lp3 += g * (lp2 - lp3);
                lp4 += g * (lp3 - lp4);
                return lp4;
            }
            case SourceFilterTopology::FeedbackLadder:
            {
                const float input = std::tanh((x - (resonance + feedback) * lp4) * drive);
                lp1 += g * (input - lp1);
                lp2 += g * (std::tanh(lp1 * drive) - lp2);
                lp3 += g * (std::tanh(lp2 * drive) - lp3);
                lp4 += g * (std::tanh(lp3 * drive) - lp4);
                return lp4;
            }
        }
        return x;
    };

    for(int s = 0; s < numSamples; ++s)
    {
        const float dryL = left[s];
        const float dryR = right[s];
        const float wetL = processOne(dryL, false);
        const float wetR = processOne(dryR, true);
        left[s] = (dryL + (wetL - dryL) * mix) * gainL;
        right[s] = (dryR + (wetR - dryR) * mix) * gainR;
    }
}

bool Voice::debugVerifyDecayTransient()
{
    constexpr double fs = 48000.0;
    constexpr int totalSamples = int(fs * 0.25);
    constexpr int blockSize = 64;

    StaticSpectralFrame frame;
    frame.partialCount = 1;
    frame.freqMode = FreqMode::RelativeRatio;
    frame.nu[0] = 1.0f;
    frame.amp[0] = 0.8f;

    AdsrParams adsr;
    adsr.attack = 0.0f;
    adsr.decay = 0.165f;
    adsr.sustain = 0.0f;
    adsr.release = 0.08f;
    adsr.curve = 0.5f;

    Voice voice;
    voice.prepare(fs);
    UnisonParams unison;
    MatrixVoiceOutput matrixOut;
    initMatrixOutput(matrixOut);
    std::array<MatrixEnvParams, kMaxModEnvs> matrixEnvs {};
    std::array<AdsrParams, kMaxAmpEnvs> ampEnvs {};
    voice.noteOn(60, 1.0f, frame, adsr, ampEnvs, matrixEnvs, unison, RenderQualityMode::Normal, 1);

    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    float peak = 0.0f;
    float latePeak = 0.0f;
    int rendered = 0;
    while(rendered < totalSamples)
    {
        const int chunk = std::min(blockSize, totalSamples - rendered);
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        std::array<RenderTrackRuntime, kMaxSourceTracks> runtime {};
        std::array<LfoParams, kMaxLfos> noLfos {};
        voice.updateControl(frame, matrixOut, adsr, ampEnvs, matrixEnvs, noLfos, unison, runtime, 0,
                            RenderQualityMode::Normal, 1.0f, chunk);
        voice.renderAdd(left.data(), right.data(), chunk);
        for(int i = 0; i < chunk; ++i)
        {
            const float v = std::max(std::abs(left[(size_t)i]), std::abs(right[(size_t)i]));
            peak = std::max(peak, v);
            if(rendered + i > int(fs * 0.20))
                latePeak = std::max(latePeak, v);
        }
        rendered += chunk;
    }

    return peak > 0.001f && latePeak < peak * 0.1f;
}

} // namespace synth
