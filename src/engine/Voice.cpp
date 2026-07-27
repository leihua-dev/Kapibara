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

// 4-point Catmull-Rom over frame-table samples. Linear frame morph is only C0 at
// frame boundaries (the (A,B) pair switches when framePos crosses an integer), so a
// morph sweep with >2 frames produces slope kinks that read as clicks. Catmull-Rom
// is C1 across those boundaries. With p0==p1 / p3==p2 at the clamped ends it
// degrades gracefully toward the linear result.
inline float catmullRomFrame(float p0, float p1, float p2, float p3, float t)
{
    const float a = p1;
    const float b = 0.5f * (p2 - p0);
    const float c = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    const float d = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
    return ((d * t + c) * t + b) * t + a;
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
    slotLevel_.fill(0.0f);
    slotPhase_.fill(0.0f);
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
        // Serum-style phase Rand: blend in a per-note random offset by the ratio. One
        // draw per partial from the note's shared RNG, so all unison lanes of this note
        // share the same offset (added into base[i], which is unison-independent).
        base[i] += clampf(frame.phaseRandom[i], 0.0f, 1.0f) * uni(rngShared);
    }

    const int U = std::max(1, unisonCount_);
    const float spread = (U > 1) ? clampf(unison.phaseSpread, 0.0f, 1.0f) : 0.0f;
    (void)spread;
    updateUnisonPhaseOffsets(unison, 0);
    for(int i = 0; i < kMaxPartials; ++i)
    {
        phaseInitTable_[i] = wrapTablePosition(base[i] * kRadiansToTable);
        if(i < kMaxRenderPartials)
        {
            for(auto &lane : thetaTable_)
                lane[(size_t)i] = 0.0f;
        }
    }
}

void Voice::noteOn(int midiNote, float velocity, const StaticSpectralFrame &frame,
                   const AdsrParams &adsr,
                   const std::array<AdsrParams, kMaxAmpEnvs> &ampEnvs,
                   const std::array<ModSlotParams, kMaxModSlots> &modSlots,
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
    filterNodeStates_ = {};
    utilBandStates_ = {};
    for(auto &acc : fmPhaseAcc_) acc.fill(0.0);
    oscFmAcc_.fill(0.0);
    syncPrev_.fill(0.0f);

    updateUnisonLayout(unison);
    seedPhases(frame, unison);
    activeCount_ = std::clamp(frame.partialCount, 1, kMaxRenderPartials);

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
    slotParams_ = modSlots;
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
    // Retrigger all modulator phases on note-on.
    slotPhase_.fill(0.0f);
    slotLevel_.fill(0.0f);

    for(int i = 0; i < kMaxPartials; ++i)
    {
        ampCur_[i] = 0.0f;
        freqCur_[i] = 0.0f;
        dPhaseCur_[i] = 0.0f;
        dPanCur_[i] = 0.0f;
        dWarpCur_[i] = 0.0f;
        phaseDriftCur_[i] = 0.0f;
        phaseJitterCur_[i] = 0.0f;
        ampStep_[i] = 0.0f;
        freqStep_[i] = 0.0f;
        dPhaseStep_[i] = 0.0f;
        dPanStep_[i] = 0.0f;
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
                          const std::array<ModSlotParams, kMaxModSlots> &modSlots,
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
    // Matrix offset for each rack's own cross-unit modulation depth — the one
    // bridge between the router and a modulation that is otherwise source-local.
    oscModDepthMod_ = matrixOut.dOscMod;
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
    // Unified modulator slots: advance per-voice phase and sample the curve.
    slotParams_ = modSlots;
    for(int i = 0; i < kMaxModSlots; ++i)
    {
        const auto &p = slotParams_[(size_t)i];
        if(!p.enabled) { slotLevel_[(size_t)i] = 0.0f; continue; }
        const float inc = std::max(0.0f, p.rateHz) * float(blockSize) / fs;
        float ph = slotPhase_[(size_t)i] + inc;
        if(p.loop) ph -= std::floor(ph);
        else       ph = std::min(ph, 1.0f);
        slotPhase_[(size_t)i] = ph;
        slotLevel_[(size_t)i] = pointCurveEval(p.points.data(), p.pointCount, ph) * 2.0f - 1.0f;
    }

    // Legacy fixed-generator path still uses global unison. Source-track mode
    // switches layout per render track inside renderAdd().
    if(renderTrackCount_ <= 0)
        updateUnisonLayout(unison);

    const int waveCount = wavetable_ != nullptr ? wavetable_->partialCount : frame.partialCount;
    const int targetActiveCount = std::clamp(std::min(frame.partialCount, waveCount), 1, kMaxRenderPartials);
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
    activeCount_ = std::clamp(renderActiveCount, 1, kMaxRenderPartials);

    for(int i = 0; i < targetActiveCount; ++i)
    {
        const auto *wave = wavetable_ != nullptr ? &wavetable_->partials[(size_t)i] : nullptr;
        const float baseMorph = wave != nullptr && wave->usesMetaWavetable
                                    ? clampf(wave->morph, 0.0f, 1.0f)
                                    : 0.0f;

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

        // Method A: matrix morph modulation is folded into the single per-sample
        // smoothed morph value (morphCur_ toward morphTarget_) instead of getting its
        // own per-block linear ramp. framePos = morph*(frameCount-1) multiplies any
        // morph-velocity kink by the frame count, so a per-block-linear dMorph clicked
        // badly at high frame counts; one continuous smoother keeps velocity C1.
        morphTarget_[(size_t)i] = clampf(baseMorph + dMorphTarget, 0.0f, 1.0f);
        if(!morphInitialized_[(size_t)i])
        {
            morphCur_[(size_t)i] = morphTarget_[(size_t)i];
            morphInitialized_[(size_t)i] = true;
        }

        if(!controlsPrimed_)
        {
            ampCur_[i] = srcAmp;
            freqCur_[i] = f;
            dPhaseCur_[i] = dPhaseTarget;
            dPanCur_[i] = dPanTarget;
            dWarpCur_[i] = dWarpTarget;
            phaseDriftCur_[i] = phaseDriftTarget;
            phaseJitterCur_[i] = phaseJitterTarget;
            ampStep_[i] = 0.0f;
            freqStep_[i] = 0.0f;
            dPhaseStep_[i] = 0.0f;
            dPanStep_[i] = 0.0f;
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
    // Route-DAG mode: evaluate the compiled per-voice graph (filters sum their
    // inputs; track buses sum their feeders). Requires the shared scratch for
    // node buffers. Falls back to legacy per-track chains when invalid.
    const bool graphMode = route_.valid && useBus && modScratch_ != nullptr;
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

    if(!anyMod && !graphMode)
    {
        // Fast single-pass path (no cross-track modulation, legacy per-track chain).
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
            renderTrackPartials(sourceRawL_.data(), sourceRawR_.data(), numSamples, source,
                                begin, end, nullptr, nullptr, oscModDepthMod_[(size_t)source]);
            if(trackRuntime_[(size_t)source].muted)
            {
                std::fill(sourceRawL_.begin(), sourceRawL_.begin() + numSamples, 0.0f);
                std::fill(sourceRawR_.begin(), sourceRawR_.begin() + numSamples, 0.0f);
            }
            processPerVoiceFilters(sourceRawL_.data(), sourceRawR_.data(), numSamples,
                                   trackRuntime_[(size_t)source],
                                   sourceFilterStates_[(size_t)source].data());
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
                {
                    if(!m.enabled) continue;
                    if(m.sourceKind != 0 && graphMode)
                    {
                        // Component tap: depend on every track feeding that node.
                        std::array<bool, kMaxSourceTracks> deps {};
                        collectNodeTrackDeps(RouteNodeRef { m.sourceKind, m.sourceNode }, sourceCount, deps);
                        for(int d = 0; d < sourceCount; ++d)
                            if(deps[(size_t)d] && d != t && mark[(size_t)d] == 0 && sp < int(stk.size()))
                                stk[sp++] = d;
                        continue;
                    }
                    if(m.sourceTrack >= 0 && m.sourceTrack < sourceCount
                       && mark[(size_t)m.sourceTrack] == 0 && sp < int(stk.size()))
                        stk[sp++] = m.sourceTrack;
                }
            }
        }
    }

    std::array<bool, kMaxSourceTracks> rendered {};
    filterEvalDone_.fill(false);
    ampEnvEvalDone_.fill(false);
    utilEvalDone_.fill(false);
    // Resolve one mod entry's tap buffers; false → not available this block.
    const auto resolveModTap = [&](const SourceModEntry &m, const float *&L, const float *&R) -> bool {
        if(m.sourceKind == 1)
        {
            if(!graphMode || int(m.sourceNode) >= kMaxPerVoiceFilters
               || !filterEvalDone_[(size_t)m.sourceNode]) return false;
            L = modScratch_->filterL[(size_t)m.sourceNode].data();
            R = modScratch_->filterR[(size_t)m.sourceNode].data();
            return true;
        }
        if(m.sourceKind == 2)
        {
            if(!graphMode || int(m.sourceNode) >= kMaxAmpEnvRouteNodes
               || !ampEnvEvalDone_[(size_t)m.sourceNode]) return false;
            L = modScratch_->ampEnvL[(size_t)m.sourceNode].data();
            R = modScratch_->ampEnvR[(size_t)m.sourceNode].data();
            return true;
        }
        if(m.sourceTrack < 0 || m.sourceTrack >= sourceCount || !rendered[(size_t)m.sourceTrack])
            return false;
        L = modScratch_->bufL[(size_t)m.sourceTrack].data();
        R = modScratch_->bufR[(size_t)m.sourceTrack].data();
        return true;
    };
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

        // Component taps: evaluate any graph node whose feeder tracks have all
        // rendered, so this carrier can read it (e.g. a post-filter FM source).
        if(graphMode)
        {
            bool wantsNodes = false;
            for(const auto &m : trackRuntime_[(size_t)source].mods)
                if(m.enabled && m.sourceKind != 0) { wantsNodes = true; break; }
            if(wantsNodes)
                for(int no = 0; no < route_.evalOrderCount; ++no)
                {
                    const RouteNodeRef nd = route_.evalOrder[(size_t)no];
                    std::array<bool, kMaxSourceTracks> deps {};
                    collectNodeTrackDeps(nd, sourceCount, deps);
                    bool ready = true;
                    for(int d = 0; d < sourceCount; ++d)
                        if(deps[(size_t)d] && !rendered[(size_t)d]) { ready = false; break; }
                    if(ready)
                        evaluateGraphNode(nd, numSamples, sourceCount, rendered.data());
                }
        }

        // Build phase-mod (FM/PM) buffer and pick a hard-sync source from this track's mods.
        const float invSr = 1.0f / float(sampleRate_);
        bool hasPm = false;
        const float *syncBuf = nullptr;
        std::fill(pmScratch_.begin(), pmScratch_.begin() + numSamples, 0.0f);
        for(int mi = 0; mi < kMaxTrackMods; ++mi)
        {
            const auto &m = trackRuntime_[(size_t)source].mods[(size_t)mi];
            if(!m.enabled)
                continue;
            const float *mL = nullptr, *mR = nullptr;
            if(!resolveModTap(m, mL, mR))
                continue;
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
                // FM = integral of the modulator. The accumulator persists across
                // render blocks — resetting it per block restarts the phase ramp at
                // every block boundary, which reads as a harsh buzz.
                double acc = fmPhaseAcc_[(size_t)source][(size_t)mi];
                for(int s = 0; s < numSamples; ++s)
                {
                    acc += double(devHz) * 0.5 * double(mL[s] + mR[s]) * double(invSr) * 6.2831853;
                    pmScratch_[(size_t)s] += float(acc);
                }
                // Phase is 2π-periodic: wrap so float conversion stays precise.
                fmPhaseAcc_[(size_t)source][(size_t)mi] = std::remainder(acc, 6.283185307179586);
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
        renderTrackPartials(bL, bR, numSamples, source, begin, end,
                            hasPm ? pmScratch_.data() : nullptr, syncBuf,
                            oscModDepthMod_[(size_t)source]);
        if(trackRuntime_[(size_t)source].muted)
        {
            std::fill(bL, bL + numSamples, 0.0f);
            std::fill(bR, bR + numSamples, 0.0f);
        }
        // gain-PRE filter (modulator tap is here, before strip gain/pan).
        // In route-DAG mode the filters are graph nodes evaluated after this loop,
        // so the node buffer stays raw (unfiltered) here.
        if(!graphMode)
            processPerVoiceFilters(bL, bR, numSamples, trackRuntime_[(size_t)source],
                                   sourceFilterStates_[(size_t)source].data(), /*applyGainPan=*/false);
        // amplitude-domain mods (AM / Ring) read the modulator's gain-pre signal
        for(const auto &m : trackRuntime_[(size_t)source].mods)
        {
            if(!m.enabled || (m.type != SourceModType::AM && m.type != SourceModType::RingMod))
                continue;
            const float *mL = nullptr, *mR = nullptr;
            if(!resolveModTap(m, mL, mR))
                continue;
            applySourceMod(bL, bR, mL, mR, numSamples, m.type, clampf(m.depth, 0.0f, 1.0f));
        }
        rendered[(size_t)source] = true;
    }

    if(graphMode)
    {
        evaluateRouteGraph(numSamples, sourceCount, rendered.data(), flushTrack);
    }
    else
    {
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
    }
    if(useBus) finalizeBlock(numSamples);
    else       finishPartialRender(left, right, serialRawL_.data(), serialRawR_.data(), numSamples);
}

// Evaluate the compiled per-voice routing DAG. Source node buffers are the
// already-rendered (raw, mod-applied) per-track buffers in modScratch_. Filter
// nodes sum their inputs and filter once; each track's strip bus sums the nodes
// wired into it, then strip gain/pan is applied and the result flushed to the bus.
int Voice::renderIndexOfTrackId(uint32_t tid, int sourceCount) const
{
    for(int t = 0; t < sourceCount; ++t)
        if(trackRuntime_[(size_t)t].trackId == tid) return t;
    return -1;
}

bool Voice::resolveNodeBuf(const RouteNodeRef &r, int sourceCount, const bool *rendered,
                           const float *&L, const float *&R) const
{
    if(r.kind == 0)
    {
        const int t = renderIndexOfTrackId(r.id, sourceCount);
        if(t < 0 || !rendered[t]) return false;
        if(trackRuntime_[(size_t)t].outputMode == SourceTrackOutputMode::ModOnly) return false;
        L = modScratch_->bufL[(size_t)t].data();
        R = modScratch_->bufR[(size_t)t].data();
        return true;
    }
    if(r.kind == 1 && int(r.id) < kMaxPerVoiceFilters)
    {
        L = modScratch_->filterL[(size_t)r.id].data();
        R = modScratch_->filterR[(size_t)r.id].data();
        return true;
    }
    if(r.kind == 2 && int(r.id) < kMaxAmpEnvRouteNodes)
    {
        L = modScratch_->ampEnvL[(size_t)r.id].data();
        R = modScratch_->ampEnvR[(size_t)r.id].data();
        return true;
    }
    if(r.kind == 3 && int(r.id) < kMaxUtilNodes)
    {
        L = modScratch_->utilL[(size_t)r.id].data();
        R = modScratch_->utilR[(size_t)r.id].data();
        return true;
    }
    return false;
}

void Voice::collectNodeTrackDeps(const RouteNodeRef &ref, int sourceCount,
                                 std::array<bool, kMaxSourceTracks> &deps) const
{
    std::array<bool, kMaxPerVoiceFilters> seenF {};
    std::array<bool, kMaxAmpEnvRouteNodes> seenA {};
    std::array<bool, kMaxUtilNodes> seenU {};
    std::array<RouteNodeRef, kMaxPerVoiceFilters + kMaxAmpEnvRouteNodes + kMaxUtilNodes + 1> stack {};
    int sp = 0;
    const auto push = [&](const RouteNodeRef &r) {
        if(r.kind == 0)
        {
            const int t = renderIndexOfTrackId(r.id, sourceCount);
            if(t >= 0) deps[(size_t)t] = true;
        }
        else if(r.kind == 1 && int(r.id) < kMaxPerVoiceFilters && !seenF[(size_t)r.id])
        { seenF[(size_t)r.id] = true; if(sp < int(stack.size())) stack[(size_t)sp++] = r; }
        else if(r.kind == 2 && int(r.id) < kMaxAmpEnvRouteNodes && !seenA[(size_t)r.id])
        { seenA[(size_t)r.id] = true; if(sp < int(stack.size())) stack[(size_t)sp++] = r; }
        else if(r.kind == 3 && int(r.id) < kMaxUtilNodes && !seenU[(size_t)r.id])
        { seenU[(size_t)r.id] = true; if(sp < int(stack.size())) stack[(size_t)sp++] = r; }
    };
    push(ref);
    while(sp > 0)
    {
        const RouteNodeRef n = stack[(size_t)--sp];
        const RouteNodeRef *arr = nullptr;
        int c = 0;
        if(n.kind == 1) { arr = route_.filterInputs[(size_t)n.id].data(); c = route_.filterInputCount[(size_t)n.id]; }
        else if(n.kind == 2) { arr = route_.ampEnvInputs[(size_t)n.id].data(); c = route_.ampEnvInputCount[(size_t)n.id]; }
        else if(n.kind == 3) { arr = route_.utilInputs[(size_t)n.id].data(); c = route_.utilInputCount[(size_t)n.id]; }
        for(int k = 0; k < std::min(c, kMaxRouteInputs); ++k)
            push(arr[k]);
    }
}

void Voice::evaluateGraphNode(const RouteNodeRef &node, int numSamples, int sourceCount, const bool *rendered)
{
    const CompiledPerVoiceRoute &rt = route_;
    // Global per-voice filter params (identical across tracks → use render track 0).
    const RenderTrackRuntime &g = trackRuntime_[0];

    if(node.kind == 1)
    {
        const int slot = int(node.id);
        if(slot < 0 || slot >= kMaxPerVoiceFilters || filterEvalDone_[(size_t)slot]) return;
        filterEvalDone_[(size_t)slot] = true;
        float *fl = modScratch_->filterL[(size_t)slot].data();
        float *fr = modScratch_->filterR[(size_t)slot].data();
        std::fill(fl, fl + numSamples, 0.0f);
        std::fill(fr, fr + numSamples, 0.0f);
        const int inN = std::min<int>(rt.filterInputCount[(size_t)slot], kMaxRouteInputs);
        for(int k = 0; k < inN; ++k)
        {
            const float *L = nullptr, *R = nullptr;
            if(!resolveNodeBuf(rt.filterInputs[(size_t)slot][(size_t)k], sourceCount, rendered, L, R)) continue;
            for(int s = 0; s < numSamples; ++s) { fl[s] += L[s]; fr[s] += R[s]; }
        }
        if(slot < g.perVoiceFilterCount)
            processSourceFilterParams(fl, fr, numSamples, g.perVoiceFilters[(size_t)slot],
                                      filterNodeStates_[(size_t)slot]);
    }
    else if(node.kind == 2)
    {
        const int e = int(node.id);
        if(e < 0 || e >= kMaxAmpEnvRouteNodes || ampEnvEvalDone_[(size_t)e]) return;
        ampEnvEvalDone_[(size_t)e] = true;
        const int envSlot = std::clamp(int(rt.ampEnvSlot[(size_t)e]), 0, kMaxAmpEnvs - 1);
        float *al = modScratch_->ampEnvL[(size_t)e].data();
        float *ar = modScratch_->ampEnvR[(size_t)e].data();
        std::fill(al, al + numSamples, 0.0f);
        std::fill(ar, ar + numSamples, 0.0f);
        const int inN = std::min<int>(rt.ampEnvInputCount[(size_t)e], kMaxRouteInputs);
        for(int k = 0; k < inN; ++k)
        {
            const float *L = nullptr, *R = nullptr;
            if(!resolveNodeBuf(rt.ampEnvInputs[(size_t)e][(size_t)k], sourceCount, rendered, L, R)) continue;
            for(int s = 0; s < numSamples; ++s) { al[s] += L[s]; ar[s] += R[s]; }
        }
        // Multiply by this amp-env's per-sample level.
        for(int s = 0; s < numSamples; ++s)
        {
            const float lv = ampEnvScratch_[(size_t)envSlot][(size_t)s];
            al[s] *= lv;
            ar[s] *= lv;
        }
    }
    else if(node.kind == 3)
    {
        const int ui = int(node.id);
        if(ui < 0 || ui >= kMaxUtilNodes || utilEvalDone_[(size_t)ui]) return;
        utilEvalDone_[(size_t)ui] = true;
        float *ul = modScratch_->utilL[(size_t)ui].data();
        float *ur = modScratch_->utilR[(size_t)ui].data();
        std::fill(ul, ul + numSamples, 0.0f);
        std::fill(ur, ur + numSamples, 0.0f);
        const int inN = std::min<int>(rt.utilInputCount[(size_t)ui], kMaxRouteInputs);
        for(int k = 0; k < inN; ++k)
        {
            const float *L = nullptr, *R = nullptr;
            if(!resolveNodeBuf(rt.utilInputs[(size_t)ui][(size_t)k], sourceCount, rendered, L, R)) continue;
            for(int s = 0; s < numSamples; ++s) { ul[s] += L[s]; ur[s] += R[s]; }
        }
        const RouteUtilParams &up = rt.utilParams[(size_t)ui];
        // Optional custom band-pass: 1-pole HP at bandLo + 1-pole LP at bandHi.
        if(up.bandOn)
        {
            const float sr = float(sampleRate_);
            const float gLo = clampf(1.0f - std::exp(-kTwoPi * clampf(up.bandLoHz, 20.0f, sr * 0.45f) / sr), 0.0f, 0.999f);
            const float gHi = clampf(1.0f - std::exp(-kTwoPi * clampf(up.bandHiHz, 20.0f, sr * 0.45f) / sr), 0.0f, 0.999f);
            UtilBandState &st = utilBandStates_[(size_t)ui];
            for(int s = 0; s < numSamples; ++s)
            {
                st.loL += gLo * (ul[s] - st.loL); const float hpL = ul[s] - st.loL; st.hiL += gHi * (hpL - st.hiL); ul[s] = st.hiL;
                st.loR += gLo * (ur[s] - st.loR); const float hpR = ur[s] - st.loR; st.hiR += gHi * (hpR - st.hiR); ur[s] = st.hiR;
            }
        }
        // Level + equal-power pan (unity at center).
        const float ang = (clampf(up.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
        const float gL = up.level * std::cos(ang) * 1.41421356f;
        const float gR = up.level * std::sin(ang) * 1.41421356f;
        for(int s = 0; s < numSamples; ++s) { ul[s] *= gL; ur[s] *= gR; }
    }
}

void Voice::evaluateRouteGraph(int numSamples, int sourceCount, const bool *rendered,
                               const std::function<void(int, const float *, const float *)> &flushTrack)
{
    const CompiledPerVoiceRoute &rt = route_;
    if(renderTrackCount_ <= 0)
        return;

    // 1) Filter + amp-env + util nodes in topological order (skips any node
    //    already evaluated early as a mod-source tap).
    for(int oi = 0; oi < rt.evalOrderCount; ++oi)
        evaluateGraphNode(rt.evalOrder[(size_t)oi], numSamples, sourceCount, rendered);

    // 2) Each track's strip bus = sum of its feeders → gain/pan → flush.
    for(int ti = 0; ti < rt.trackCount; ++ti)
    {
        const int t = renderIndexOfTrackId(rt.trackId[(size_t)ti], sourceCount);
        if(t < 0) continue;
        const int inN = std::min<int>(rt.busInputCount[(size_t)ti], kMaxRouteInputs);
        if(inN <= 0) continue;
        std::fill(sourceRawL_.begin(), sourceRawL_.begin() + numSamples, 0.0f);
        std::fill(sourceRawR_.begin(), sourceRawR_.begin() + numSamples, 0.0f);
        for(int k = 0; k < inN; ++k)
        {
            const float *L = nullptr, *R = nullptr;
            if(!resolveNodeBuf(rt.busInputs[(size_t)ti][(size_t)k], sourceCount, rendered, L, R)) continue;
            for(int s = 0; s < numSamples; ++s) { sourceRawL_[(size_t)s] += L[s]; sourceRawR_[(size_t)s] += R[s]; }
        }
        applyGainPan(sourceRawL_.data(), sourceRawR_.data(), numSamples, sourceParams_[(size_t)t]);
        flushTrack(t, sourceRawL_.data(), sourceRawR_.data());
    }
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

    for(int s = 0; s < numSamples; ++s)
    {
        globalEnvScratch_[(size_t)s] = advanceAdsr(ampEnv_, sustain_, adsrCurve_, adsrCurve_, adsrCurve_);
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
                // Source tracks no longer have an implicit amp ADSR. Envelope
                // shaping is explicit: wire AE1..AE4 nodes in the per-voice graph.
                trackEnvScratch_[(size_t)t][(size_t)s] = 1.0f;
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

// Cross-unit modulation inside one Basic Oscillator rack. The units are just
// consecutive slices of the track's partial block, so each can be rendered on
// its own and fed to another exactly the way a cross-TRACK source mod is — this
// is the same machinery, scoped inside a single source.
void Voice::renderTrackPartials(float *left, float *right, int numSamples, int source,
                                int partialBegin, int partialEnd,
                                const float *pmBuffer, const float *syncBuffer, float depthMod)
{
    const auto &mod = trackRuntime_[(size_t)source].basicMod;
    const float depth = clampf(mod.depth + depthMod, 0.0f, 1.0f);
    const int si = int(mod.source), ti = int(mod.target);
    // Deliberately NOT gated on depth: with a mode selected the modulator unit is
    // rendered whether or not it is switched on, and the plain path would put a
    // switched-off modulator straight into the output. Depth only decides whether
    // the modulation is applied, below.
    const bool wants = mod.mode != BasicOscModMode::Off
                       && si != ti && si >= 0 && si < kBasicOscUnits
                       && ti >= 0 && ti < kBasicOscUnits && wavetable_ != nullptr;
    const bool active = depth > 1.0e-4f;
    if(!wants)
    {
        renderPartialRangeRaw(left, right, numSamples, partialBegin, partialEnd, pmBuffer, syncBuffer);
        return;
    }

    const auto &ub = wavetable_->unitBegin[(size_t)source];
    const auto &ue = wavetable_->unitEnd[(size_t)source];
    const int mb = std::clamp(ub[(size_t)si], partialBegin, partialEnd);
    const int me = std::clamp(ue[(size_t)si], mb, partialEnd);
    const int cb = std::clamp(ub[(size_t)ti], partialBegin, partialEnd);
    const int ce = std::clamp(ue[(size_t)ti], cb, partialEnd);
    // A disabled unit contributes no partials, so it can be neither modulator nor
    // carrier — fall back rather than silently doing nothing.
    if(mb >= me || cb >= ce)
    {
        renderPartialRangeRaw(left, right, numSamples, partialBegin, partialEnd, pmBuffer, syncBuffer);
        return;
    }

    // Every partial must be rendered EXACTLY once per block: the oscillator phase
    // accumulators advance inside renderPartialRangeRaw, so covering a partial
    // twice would run it at double speed and desync the copy used as a modulator.
    // The modulator and carrier ranges are disjoint slices of the track's block,
    // so the untouched remainder is the three gaps around them.
    const int lo0 = std::min(mb, cb), lo1 = (lo0 == mb) ? me : ce;
    const int hi0 = std::max(mb, cb), hi1 = (hi0 == mb) ? me : ce;

    // 1) The modulator unit on its own. It stays audible in the rack's sum, so
    //    this buffer is both the modulation source and part of the output.
    std::fill(oscModL_.begin(), oscModL_.begin() + numSamples, 0.0f);
    std::fill(oscModR_.begin(), oscModR_.begin() + numSamples, 0.0f);
    renderPartialRangeRaw(oscModL_.data(), oscModR_.data(), numSamples, mb, me, pmBuffer, syncBuffer);

    // 2) The units neither side of the modulation, straight into the output.
    if(partialBegin < lo0)
        renderPartialRangeRaw(left, right, numSamples, partialBegin, lo0, pmBuffer, syncBuffer);
    if(lo1 < hi0)
        renderPartialRangeRaw(left, right, numSamples, lo1, hi0, pmBuffer, syncBuffer);
    if(hi1 < partialEnd)
        renderPartialRangeRaw(left, right, numSamples, hi1, partialEnd, pmBuffer, syncBuffer);

    // 3) The carrier unit, driven by the modulator in whichever domain the mode
    //    asks for: phase (FM/PM/Sync) before it renders, amplitude (Ring/AM) after.
    std::fill(oscCarL_.begin(), oscCarL_.begin() + numSamples, 0.0f);
    std::fill(oscCarR_.begin(), oscCarR_.begin() + numSamples, 0.0f);
    const float *carrierSync = syncBuffer;
    const float *carrierPm = pmBuffer;
    if(active && mod.mode == BasicOscModMode::Sync)
    {
        for(int s = 0; s < numSamples; ++s)
            syncMonoScratch_[(size_t)s] = 0.5f * (oscModL_[(size_t)s] + oscModR_[(size_t)s]);
        carrierSync = syncMonoScratch_.data();
    }
    else if(active && (mod.mode == BasicOscModMode::PM || mod.mode == BasicOscModMode::FM))
    {
        // Added to, never replacing, any cross-track phase modulation already
        // aimed at this carrier.
        if(mod.mode == BasicOscModMode::PM)
        {
            const float idx = depth * 24.0f;  // radians at full depth
            for(int s = 0; s < numSamples; ++s)
                oscPmScratch_[(size_t)s] = (pmBuffer != nullptr ? pmBuffer[s] : 0.0f)
                    + idx * 0.5f * (oscModL_[(size_t)s] + oscModR_[(size_t)s]);
        }
        else
        {
            const float devHz = depth * 8000.0f;  // peak deviation
            const double invSr = 1.0 / double(sampleRate_);
            double acc = oscFmAcc_[(size_t)source];
            for(int s = 0; s < numSamples; ++s)
            {
                acc += double(devHz) * 0.5 * double(oscModL_[(size_t)s] + oscModR_[(size_t)s])
                       * invSr * 6.2831853071795865;
                oscPmScratch_[(size_t)s] = (pmBuffer != nullptr ? pmBuffer[s] : 0.0f) + float(acc);
            }
            oscFmAcc_[(size_t)source] = std::remainder(acc, 6.283185307179586);
        }
        carrierPm = oscPmScratch_.data();
    }
    renderPartialRangeRaw(oscCarL_.data(), oscCarR_.data(), numSamples, cb, ce, carrierPm, carrierSync);

    if(active && (mod.mode == BasicOscModMode::Ring || mod.mode == BasicOscModMode::AM))
        applySourceMod(oscCarL_.data(), oscCarR_.data(), oscModL_.data(), oscModR_.data(),
                       numSamples,
                       mod.mode == BasicOscModMode::Ring ? SourceModType::RingMod
                                                         : SourceModType::AM,
                       depth);

    // The modulator reaches the output only when its own switch is on. Switched
    // off it still drove the carrier above — the switch controls audibility, not
    // whether the unit exists.
    const bool modAudible = trackRuntime_[(size_t)source].basicModSourceAudible;
    for(int s = 0; s < numSamples; ++s)
    {
        left[s] += oscCarL_[(size_t)s] + (modAudible ? oscModL_[(size_t)s] : 0.0f);
        right[s] += oscCarR_[(size_t)s] + (modAudible ? oscModR_[(size_t)s] : 0.0f);
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
    // Previous modulator sample for hard-sync zero-cross detection. Persists per
    // carrier track so a crossing that spans a render-block boundary still fires.
    const int syncTrack = std::clamp(currentRenderTrack_, 0, kMaxSourceTracks - 1);
    float prevSync = syncPrev_[(size_t)syncTrack];

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
                                    ? clampf(morphCur_[(size_t)i], 0.0f, 1.0f)
                                    : 0.0f;
            const int frameCount = useMetaWavetable ? std::clamp(wave->frameCount, 1, kMaxWavetableFrames) : 1;
            const float framePos = morph * float(std::max(0, frameCount - 1));
            const int frameA = std::clamp(int(framePos), 0, frameCount - 1);
            const int frameB = std::min(frameA + 1, frameCount - 1);
            const float frameFrac = framePos - float(frameA);
            // Only 3+ frames can cross a frame boundary during a sweep; 2 frames stay
            // on the single [0,1] segment where linear already == the endpoints, so
            // skip the two extra lookups there.
            const bool cubicFrames = useMetaWavetable && frameCount > 2;
            const int framePrev = std::max(frameA - 1, 0);
            const int frameNext = std::min(frameB + 1, frameCount - 1);

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
                    const auto *frameSet =
                        wave->mipTables ? &(*wave->mipTables)[(size_t)mipLevel]
                                        : (wave->tables ? wave->tables.get() : nullptr);
                    if(frameSet != nullptr)
                    {
                        const auto &set = *frameSet;
                        const float p1 = lookupTablePosition(set[(size_t)frameA], warped);
                        const float p2 = lookupTablePosition(set[(size_t)frameB], warped);
                        if(cubicFrames)
                        {
                            const float p0 = lookupTablePosition(set[(size_t)framePrev], warped);
                            const float p3 = lookupTablePosition(set[(size_t)frameNext], warped);
                            osc = catmullRomFrame(p0, p1, p2, p3, frameFrac);
                        }
                        else
                        {
                            osc = p1 + (p2 - p1) * frameFrac;
                        }
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
            dWarpCur_[i] += dWarpStep_[i];
            phaseDriftCur_[i] += phaseDriftStep_[i];
            phaseJitterCur_[i] += phaseJitterStep_[i];
        }

        left[(size_t)s] += sumL;
        right[(size_t)s] += sumR;
    }
    if(syncBuffer != nullptr)
        syncPrev_[(size_t)syncTrack] = prevSync;
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
        int envCount = 0;
        if(route_.valid)
        {
            std::array<bool, kMaxAmpEnvs> seenEnv {};
            for(int e = 0; e < kMaxAmpEnvRouteNodes; ++e)
            {
                if(route_.ampEnvInputCount[(size_t)e] == 0)
                    continue;
                const int envIndex = std::clamp(int(route_.ampEnvSlot[(size_t)e]), 0, kMaxAmpEnvs - 1);
                if(seenEnv[(size_t)envIndex])
                    continue;
                seenEnv[(size_t)envIndex] = true;
                envSum += sharedAmpEnvState_[(size_t)envIndex].value;
                ++envCount;
                if(sharedAmpEnvState_[(size_t)envIndex].state != PartialState::Idle)
                    anyActive = true;
            }
        }
        if(envCount == 0)
        {
            for(int t = 0; t < trackCount; ++t)
            {
                const int envIndex = renderTrackCount_ > 0
                                         ? std::clamp(trackRuntime_[(size_t)t].ampEnvIndex, 0, kMaxAmpEnvs - 1)
                                         : 0;
                envSum += sharedAmpEnvState_[(size_t)envIndex].value;
                ++envCount;
                if(sharedAmpEnvState_[(size_t)envIndex].state != PartialState::Idle)
                    anyActive = true;
            }
        }
        avgEnv_ = envSum / float(std::max(1, envCount));
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
    const float gain = applyGainPanFlag ? clampf(source.gain, 0.0f, 2.0f) : 1.0f;
    const float pan = applyGainPanFlag ? clampf(source.pan, -1.0f, 1.0f) : 0.0f;
    const float gainL = gain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    const float gainR = gain * (pan >= 0.0f ? 1.0f : 1.0f + pan);
    processSourceFilterParams(left, right, numSamples, source.filter, state, gainL, gainR);
}

void Voice::processSourceFilterParams(float *left, float *right, int numSamples, const SourceFilterParams &filter,
                                      SourceFilterRuntime &state, float gainL, float gainR)
{
    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    const float mix = filter.enabled && filter.topology != SourceFilterTopology::Bypass
                          ? clampf(filter.mix, 0.0f, 1.0f)
                          : 0.0f;
    const float cutoff = clampf(filter.cutoffHz, 20.0f, float(sampleRate_ * 0.45));
    const float g = clampf(1.0f - std::exp(-kTwoPi * cutoff / float(sampleRate_)), 0.001f, 0.98f);
    const float resonance = clampf(filter.resonance, 0.0f, 0.95f);
    const float drive = clampf(filter.drive, 0.1f, 8.0f);
    const float feedback = clampf(filter.feedback, 0.0f, 0.95f);

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

void Voice::processPerVoiceFilters(float *left, float *right, int numSamples,
                                   const RenderTrackRuntime &runtime, SourceFilterRuntime *states,
                                   bool applyGainPanFlag)
{
    numSamples = std::min(numSamples, kMaxVoiceRenderBlockSamples);
    const int count = std::clamp(runtime.perVoiceFilterOrderCount, 0, kMaxPerVoiceFilters);
    for(int i = 0; i < count; ++i)
    {
        const int filterIdx = int(runtime.perVoiceFilterOrder[(size_t)i]);
        if(filterIdx < 0 || filterIdx >= runtime.perVoiceFilterCount || filterIdx >= kMaxPerVoiceFilters)
            continue;
        processSourceFilterParams(left, right, numSamples, runtime.perVoiceFilters[(size_t)filterIdx], states[filterIdx]);
    }
    if(applyGainPanFlag)
        applyGainPan(left, right, numSamples, runtime.strip);
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
    std::array<ModSlotParams, kMaxModSlots> modSlots {};
    std::array<AdsrParams, kMaxAmpEnvs> ampEnvs {};
    voice.noteOn(60, 1.0f, frame, adsr, ampEnvs, modSlots, unison, RenderQualityMode::Normal, 1);

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
        voice.updateControl(frame, matrixOut, adsr, ampEnvs, modSlots, unison, runtime, 0,
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
