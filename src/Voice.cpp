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
} // namespace

void Voice::prepare(double sr)
{
    sampleRate_ = sr > 1.0 ? sr : 48000.0;
    idle_ = true;
    releasing_ = false;
    activeCount_ = 0;
    avgEnv_ = 0.0f;
    for(auto &row : theta_) row.fill(0.0f);
    for(auto &row : phaseInit_) row.fill(0.0f);
    pState_.fill(PartialState::Idle);
    stageSample_.fill(0);
    attackSamples_.fill(1);
    decaySamples_.fill(1);
    releaseSamples_.fill(1);
    envValue_.fill(0.0f);
    ampCur_.fill(0.0f); ampStep_.fill(0.0f);
    freqCur_.fill(0.0f); freqStep_.fill(0.0f);
    dPhaseCur_.fill(0.0f); dPhaseStep_.fill(0.0f);
    phaseDriftCur_.fill(0.0f); phaseDriftStep_.fill(0.0f);
    phaseJitterCur_.fill(0.0f); phaseJitterStep_.fill(0.0f);
    sustainLevel_.fill(1.0f);
    unisonCount_ = 1;
    unisonDetune_.fill(1.0f);
    unisonGainL_.fill(1.0f);
    unisonGainR_.fill(1.0f);
    unisonNormGain_ = 1.0f;
}

void Voice::updateUnisonLayout(const UnisonParams &unison)
{
    const int U = std::clamp(unison.voices, 1, kMaxUnison);
    unisonCount_ = U;
    const float widthCents = std::max(0.0f, unison.detuneCents);
    const float widthStereo = clampf(unison.widthStereo, 0.0f, 1.0f);
    for(int u = 0; u < U; ++u)
    {
        // Symmetric spread: u=0 mapped to -1, u=U-1 mapped to +1.
        const float t = (U > 1) ? (float(u) / float(U - 1)) * 2.0f - 1.0f : 0.0f;
        const float cents = 0.5f * widthCents * t;
        unisonDetune_[u] = std::pow(2.0f, cents / 1200.0f);

        // Equal-power pan, then attenuate cross-channel by (1-widthStereo).
        const float pan = t * widthStereo;
        const float ang = 0.25f * kPi * (1.0f + pan); // pan=-1 -> 0, pan=+1 -> pi/2
        unisonGainL_[u] = std::cos(ang);
        unisonGainR_[u] = std::sin(ang);
    }
    for(int u = U; u < kMaxUnison; ++u)
    {
        unisonDetune_[u] = 1.0f;
        unisonGainL_[u] = 0.0f;
        unisonGainR_[u] = 0.0f;
    }
    // 1/sqrt(U) normalisation keeps the perceived loudness roughly constant.
    unisonNormGain_ = 1.0f / std::sqrt(float(U));
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

    // Second pass: scatter unison sub-voices around the base phase using their
    // own deterministic seed so the unison "smear" is reproducible.
    // IMPORTANT: phaseSpread is a UNISON property; with U=1 there is nothing to
    // scatter and the only sub-voice must match the base PhaseInitMode exactly.
    const int U = std::max(1, unisonCount_);
    const float spread = (U > 1) ? clampf(unison.phaseSpread, 0.0f, 1.0f) : 0.0f;
    for(int u = 0; u < kMaxUnison; ++u)
    {
        std::mt19937 rngU(baseSeed ^ unison.phaseSeed ^ (uint32_t(u + 1) * 0x9E3779B9u));
        std::uniform_real_distribution<float> uniU(-kPi, kPi);
        for(int i = 0; i < kMaxPartials; ++i)
        {
            // u=0 stays anchored on base[i] so the canonical voice is unchanged;
            // only u>=1 (and only when U>1) takes the scatter offset.
            const float offset = (u >= 1 && u < U) ? (spread * uniU(rngU)) : 0.0f;
            float ph = base[i] + offset;
            while(ph >= kTwoPi) ph -= kTwoPi;
            while(ph < 0.0f) ph += kTwoPi;
            phaseInit_[u][i] = ph;
            theta_[u][i] = 0.0f;
        }
    }
}

void Voice::noteOn(int midiNote, float velocity, const StaticSpectralFrame &frame,
                   const UnisonParams &unison, uint64_t tick)
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

    updateUnisonLayout(unison);
    seedPhases(frame, unison);
    activeCount_ = std::min(frame.partialCount, kMaxPartials);

    for(int i = 0; i < kMaxPartials; ++i)
    {
        envValue_[i] = 0.0f;
        ampCur_[i] = 0.0f;
        freqCur_[i] = 0.0f;
        dPhaseCur_[i] = 0.0f;
        phaseDriftCur_[i] = 0.0f;
        phaseJitterCur_[i] = 0.0f;
        ampStep_[i] = 0.0f;
        freqStep_[i] = 0.0f;
        dPhaseStep_[i] = 0.0f;
        phaseDriftStep_[i] = 0.0f;
        phaseJitterStep_[i] = 0.0f;
        stageSample_[i] = 0;
        pState_[i] = (i < activeCount_) ? PartialState::Attack : PartialState::Idle;
    }
    avgEnv_ = 0.0f;
}

void Voice::noteOff()
{
    if(idle_)
        return;
    releasing_ = true;
    for(int i = 0; i < activeCount_; ++i)
    {
        if(pState_[i] != PartialState::Idle)
        {
            releaseFrom_[i] = envValue_[i];
            stageSample_[i] = 0;
            pState_[i] = PartialState::Release;
        }
    }
}

void Voice::steal()
{
    // Fast release: ~5 ms regardless of release setting.
    const float fastInc = 1.0f / std::max(1.0f, 0.005f * float(sampleRate_));
    for(int i = 0; i < activeCount_; ++i)
    {
        if(pState_[i] != PartialState::Idle)
        {
            releaseFrom_[i] = envValue_[i];
            releaseInc_[i] = fastInc;
            releaseSamples_[i] = std::max(1, int(0.005 * sampleRate_));
            stageSample_[i] = 0;
            pState_[i] = PartialState::Release;
        }
    }
    releasing_ = true;
}

void Voice::updateControl(const StaticSpectralFrame &frame,
                          const MatrixVoiceOutput &matrixOut,
                          const GlobalAdsrParams &adsr,
                          const UnisonParams &unison,
                          float globalGain,
                          int blockSize)
{
    const float fs = float(sampleRate_);
    const float invBlock = 1.0f / float(std::max(1, blockSize));
    const float fadeStart = 0.45f * fs;
    const float fadeEnd = 0.5f * fs;

    sustain_ = clampf(adsr.sustain, 0.0f, 1.0f);
    attackCurve_ = adsr.attackCurve;
    decayCurve_ = adsr.decayCurve;
    releaseCurve_ = adsr.releaseCurve;
    etaA_ = adsr.etaA;
    etaD_ = adsr.etaD;
    etaR_ = adsr.etaR;

    const float A = std::max(1e-4f, adsr.attack);
    const float D = std::max(1e-4f, adsr.decay) * std::max(0.05f, matrixOut.decayTimeMul);
    const float R = std::max(1e-4f, adsr.release);
    const float vGain = velocity_ * globalGain;

    // Refresh unison layout if the user changed unison settings while the note is held.
    if(unisonCount_ != std::clamp(unison.voices, 1, kMaxUnison))
        updateUnisonLayout(unison);
    else
        updateUnisonLayout(unison); // cheap: just rebuilds detune/pan tables.

    // Cap partials so anything strictly above 20 kHz (audible bound) or Nyquist is skipped.
    // Use the relative-ratio frame's base nu for the cull decision; for AbsoluteHz mode
    // nu IS the partial frequency and we cull against the same bound.
    const float audibleCap = std::min(kAudibleMaxHz, 0.5f * fs);
    int hardN = std::min(frame.partialCount, kMaxPartials);
    for(int i = 0; i < hardN; ++i)
    {
        const float fi = (frame.freqMode == FreqMode::RelativeRatio)
                             ? voiceF0_ * frame.nu[i]
                             : frame.nu[i];
        if(fi > audibleCap)
        {
            hardN = i;
            break;
        }
    }
    activeCount_ = std::max(1, hardN);

    for(int i = 0; i < activeCount_; ++i)
    {
        // f_i^final = f0 * nu_i * M_i^freq  (RelativeRatio) or nu_i * M_i^freq (AbsoluteHz)
        float f = (frame.freqMode == FreqMode::RelativeRatio)
                      ? voiceF0_ * frame.nu[i]
                      : frame.nu[i];
        f *= matrixOut.mFreq[i];
        if(f < 0.0f)
            f = 0.0f;
        const float nyqFade = 1.0f - smoothStep(fadeStart, fadeEnd, f);

        // a_i^src * gain * Nyquist fade * matrix amp.  Per-partial envelope is multiplied at audio rate.
        const float srcAmp = frame.amp[i] * vGain * nyqFade * matrixOut.mAmp[i];
        const float dPhaseTarget = matrixOut.dPhase[i];
        const float phaseDriftTarget = frame.phaseDriftHz[i];
        const float phaseJitterTarget = frame.phaseJitter[i];

        ampStep_[i] = (srcAmp - ampCur_[i]) * invBlock;
        freqStep_[i] = (f - freqCur_[i]) * invBlock;
        dPhaseStep_[i] = (dPhaseTarget - dPhaseCur_[i]) * invBlock;
        phaseDriftStep_[i] = (phaseDriftTarget - phaseDriftCur_[i]) * invBlock;
        phaseJitterStep_[i] = (phaseJitterTarget - phaseJitterCur_[i]) * invBlock;

        // Per-partial envelope rates.
        const float aPart = A * std::max(0.05f, frame.attackScale[i]);
        const float dPart = D * std::max(0.05f, frame.decayScale[i]);
        const float rPart = R * std::max(0.05f, frame.releaseScale[i]);
        attackInc_[i] = 1.0f / (aPart * fs);
        attackSamples_[i] = std::max(1, int(aPart * fs));
        decaySamples_[i] = std::max(1, int(dPart * fs));
        releaseSamples_[i] = std::max(1, int(rPart * fs));
        sustainLevel_[i] = clampf(sustain_ * frame.sustainLevel[i], 0.0f, 1.0f);
        decayInc_[i] = (1.0f - sustainLevel_[i]) / (dPart * fs);
        if(pState_[i] == PartialState::Release)
            releaseInc_[i] = releaseFrom_[i] / (rPart * fs);
        else
            releaseInc_[i] = 1.0f / (rPart * fs); // placeholder; real value set on noteOff
    }

    // Decay any inactive (i >= activeCount_) leftover gracefully.
    for(int i = activeCount_; i < kMaxPartials; ++i)
    {
        ampStep_[i] = -ampCur_[i] * invBlock;
        freqStep_[i] = 0.0f;
        dPhaseStep_[i] = 0.0f;
        phaseDriftStep_[i] = -phaseDriftCur_[i] * invBlock;
        phaseJitterStep_[i] = -phaseJitterCur_[i] * invBlock;
    }
}

void Voice::renderAdd(float *left, float *right, int numSamples)
{
    if(idle_)
        return;

    float envSum = 0.0f;
    int activePartialCount = 0;
    const float invSr = 1.0f / float(sampleRate_);
    const int U = std::max(1, unisonCount_);
    const float gNorm = unisonNormGain_;

    for(int s = 0; s < numSamples; ++s)
    {
        float sumL = 0.0f;
        float sumR = 0.0f;
        for(int i = 0; i < activeCount_; ++i)
        {
            // Per-partial ADSR (shared across unison sub-voices).
            switch(pState_[i])
            {
                case PartialState::Attack:
                {
                    const float tau = float(stageSample_[i]) / float(std::max(1, attackSamples_[i]));
                    envValue_[i] = envCurveEval(attackCurve_, tau, etaA_);
                    ++stageSample_[i];
                    if(stageSample_[i] >= attackSamples_[i])
                    {
                        envValue_[i] = 1.0f;
                        stageSample_[i] = 0;
                        pState_[i] = PartialState::Decay;
                    }
                    break;
                }
                case PartialState::Decay:
                {
                    const float tau = float(stageSample_[i]) / float(std::max(1, decaySamples_[i]));
                    const float f = envCurveEval(decayCurve_, tau, etaD_);
                    envValue_[i] = sustainLevel_[i] + (1.0f - sustainLevel_[i]) * (1.0f - f);
                    ++stageSample_[i];
                    if(stageSample_[i] >= decaySamples_[i])
                    {
                        envValue_[i] = sustainLevel_[i];
                        stageSample_[i] = 0;
                        pState_[i] = PartialState::Sustain;
                    }
                    break;
                }
                case PartialState::Sustain:
                    envValue_[i] = sustainLevel_[i];
                    break;
                case PartialState::Release:
                {
                    const float tau = float(stageSample_[i]) / float(std::max(1, releaseSamples_[i]));
                    const float f = envCurveEval(releaseCurve_, tau, etaR_);
                    envValue_[i] = releaseFrom_[i] * (1.0f - f);
                    ++stageSample_[i];
                    if(stageSample_[i] >= releaseSamples_[i])
                    {
                        envValue_[i] = 0.0f;
                        stageSample_[i] = 0;
                        pState_[i] = PartialState::Idle;
                    }
                    break;
                }
                case PartialState::Idle:
                    envValue_[i] = 0.0f;
                    break;
            }

            const float aFinal = ampCur_[i] * envValue_[i] * gNorm;
            const float dPhaseBias = phaseInit_[0][i] + dPhaseCur_[i];
            (void)dPhaseBias; // silence "unused" if compiler folds.

            // Sum across unison sub-voices (audio-rate).
            for(int u = 0; u < U; ++u)
            {
                const float jitterPhase =
                    phaseJitterCur_[i]
                    * std::sin(0.00073f * float(ageSamples_ + 1u) * float(i + 3)
                               + 1.618f * float(u + 1));
                const float phi = theta_[u][i] + phaseInit_[u][i] + dPhaseCur_[i] + jitterPhase;
                const float c = std::cos(phi);
                const float partialSample = aFinal * c;
                sumL += partialSample * unisonGainL_[u];
                sumR += partialSample * unisonGainR_[u];

                const float fSub = std::max(0.0f, (freqCur_[i] * unisonDetune_[u]) + phaseDriftCur_[i]);
                theta_[u][i] += kTwoPi * fSub * invSr;
                if(theta_[u][i] >= kTwoPi)
                    theta_[u][i] -= kTwoPi;
            }

            // Linear interpolation toward control targets.
            ampCur_[i] += ampStep_[i];
            freqCur_[i] += freqStep_[i];
            dPhaseCur_[i] += dPhaseStep_[i];
            phaseDriftCur_[i] += phaseDriftStep_[i];
            phaseJitterCur_[i] += phaseJitterStep_[i];
        }

        left[s] += std::tanh(sumL);
        right[s] += std::tanh(sumR);
        ++ageSamples_;
    }

    // Idle detection at end of block.
    (void)envSum;
    bool anyAlive = false;
    float sumEnv = 0.0f;
    activePartialCount = 0;
    for(int i = 0; i < activeCount_; ++i)
    {
        if(pState_[i] != PartialState::Idle)
        {
            anyAlive = true;
            sumEnv += envValue_[i];
            ++activePartialCount;
        }
    }
    avgEnv_ = (activePartialCount > 0) ? (sumEnv / float(activePartialCount)) : 0.0f;

    if(!anyAlive)
    {
        idle_ = true;
        releasing_ = false;
    }
}

} // namespace synth
