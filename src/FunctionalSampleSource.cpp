// =============================================================================
// FunctionalSampleSource.cpp -- analysis + bake pipeline for the
// FunctionalSampleSource generator (architecture md §1.1.2).
//
// Pipeline (v1):
//
//   loadMonoFile
//     -> preprocessSample        (DC remove, peak normalize, leading silence trim)
//     -> estimateRoot            (HPS-style FFT peak picker; user override)
//     -> medium-STFT analysis
//     -> extractPeakCandidates   (parabolic interpolation, per frame)
//     -> linkPartialTracks       (greedy with cost function)
//     -> cleanAndClassifyTracks  (confidence, classification, top-N selection)
//     -> compressToFunctional    (shared interpretable time-basis, per-partial
//                                 ridge-regression for amp weights)
//
// Bake-time:
//
//   bakeToTimeline  applies macro multipliers (attackSharpness / brightnessDecay
//                   / bodyResonance) to basis weights and reconstructs amp+ratio
//                   on a non-uniform timeline that is dense across the attack.
//
// v2 runtime additions now live here:
//   - Frequency time-basis (pitch relax + vibrato-like periodic drift)
//   - Transient emphasis lane + residual phase-jitter lane
//   - Curvature-aware adaptive frame placement
//   - Cross-quality loudness normalization
//
// Still future-facing research items:
//   - Reassigned spectrogram / CQT
//   - Learned PCA residual basis on top of interpretable basis
//   - Richer phase residual models beyond the residual jitter lane
// =============================================================================

#include "Generators.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

namespace synth
{
namespace functional
{
namespace
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float wrapPi(float x)
{
    while(x > kPi) x -= kTwoPi;
    while(x < -kPi) x += kTwoPi;
    return x;
}

// -----------------------------------------------------------------------------
// 0. Load mono audio file (full length; small hard cap defends against insane
// inputs only).
// -----------------------------------------------------------------------------
bool loadMonoFile(const std::string &path, std::vector<float> &mono, double &sr)
{
    juce::AudioFormatManager fmtMgr;
    fmtMgr.registerBasicFormats();
    juce::File file(path);
    if(!file.existsAsFile())
        return false;
    std::unique_ptr<juce::AudioFormatReader> reader(fmtMgr.createReaderFor(file));
    if(reader == nullptr || reader->lengthInSamples <= 0)
        return false;
    sr = reader->sampleRate;
    constexpr juce::int64 kHardCap = juce::int64(1) << 27; // ~2730 s @ 48 kHz
    const int len = (int)std::min<juce::int64>(reader->lengthInSamples, kHardCap);
    juce::AudioBuffer<float> buf((int)reader->numChannels, len);
    reader->read(&buf, 0, len, 0, true, true);
    mono.assign((size_t)len, 0.0f);
    for(int ch = 0; ch < (int)reader->numChannels; ++ch)
    {
        const float *r = buf.getReadPointer(ch);
        for(int s = 0; s < len; ++s)
            mono[(size_t)s] += r[s];
    }
    const float invCh = 1.0f / float(reader->numChannels);
    for(auto &v : mono)
        v *= invCh;
    return true;
}

// -----------------------------------------------------------------------------
// 1. Sample Preprocess: DC remove, peak normalize, trim leading silence.
// -----------------------------------------------------------------------------
void preprocessSample(std::vector<float> &mono, double sr)
{
    if(mono.size() < 64)
        return;

    // DC removal
    double mean = 0.0;
    for(float v : mono)
        mean += v;
    mean /= (double)mono.size();
    for(float &v : mono)
        v -= (float)mean;

    // Peak normalize to 0.95 to leave a tiny headroom against parabolic interp
    // overshoot during peak picking.
    float peak = 1e-6f;
    for(float v : mono)
        peak = std::max(peak, std::abs(v));
    if(peak > 1e-6f)
    {
        const float inv = 0.95f / peak;
        for(float &v : mono)
            v *= inv;
    }

    // Trim leading silence below ~-60 dB so analysis t=0 lines up with onset.
    const float thr = 1e-3f;
    size_t lead = 0;
    while(lead < mono.size() && std::abs(mono[lead]) < thr)
        ++lead;
    const int preRoll = std::max(64, (int)(0.001 * sr)); // keep 1 ms pre-roll
    lead = (lead > (size_t)preRoll) ? (lead - (size_t)preRoll) : 0;
    if(lead > 0)
        mono.erase(mono.begin(), mono.begin() + (long)lead);
}

// -----------------------------------------------------------------------------
// 2. Root / f0 estimation. Long-window magnitude average + Harmonic Product
// Spectrum-style score so we don't pick the loudest overtone by accident.
// User override is respected before anything else.
// -----------------------------------------------------------------------------
float estimateRoot(const std::vector<float> &mono, double sr,
                   const FunctionalSampleSourceParams &p)
{
    if(p.userLockRoot)
        return clampf(p.userRootHz, 1.0f, (float)sr * 0.45f);
    if(mono.size() < 4096)
        return clampf(p.userRootHz, 1.0f, (float)sr * 0.45f);

    constexpr int order = 14;
    constexpr int sz = 1 << order;
    if((int)mono.size() < sz)
        return clampf(p.userRootHz, 1.0f, (float)sr * 0.45f);

    juce::dsp::FFT fft(order);
    juce::dsp::WindowingFunction<float> win((size_t)sz,
                                            juce::dsp::WindowingFunction<float>::hann);
    const int hop = sz / 2;
    const int frames = std::max(1, ((int)mono.size() - sz) / hop + 1);
    std::vector<float> buf((size_t)sz * 2, 0.0f);
    std::vector<float> avgMag((size_t)sz / 2, 0.0f);

    for(int f = 0; f < frames; ++f)
    {
        const int start = std::min(f * hop, std::max(0, (int)mono.size() - sz));
        std::fill(buf.begin(), buf.end(), 0.0f);
        for(int i = 0; i < sz; ++i)
            buf[(size_t)i] = mono[(size_t)(start + i)];
        win.multiplyWithWindowingTable(buf.data(), (size_t)sz);
        fft.performRealOnlyForwardTransform(buf.data());
        for(int b = 1; b < sz / 2; ++b)
        {
            const float re = buf[(size_t)2 * b];
            const float im = buf[(size_t)2 * b + 1];
            avgMag[(size_t)b] += std::sqrt(re * re + im * im);
        }
    }

    const float binHz = (float)sr / (float)sz;
    const int loBin = std::max(1, (int)(p.minRootHz / binHz));
    const int hiBin = std::min(sz / 2 - 4, (int)(p.maxRootHz / binHz));
    if(hiBin <= loBin)
        return clampf(p.userRootHz, p.minRootHz, p.maxRootHz);

    float bestScore = 0.0f;
    int bestBin = loBin;
    for(int b = loBin; b <= hiBin; ++b)
    {
        const float v1 = avgMag[(size_t)b];
        if(v1 < 1e-5f) continue;
        // Reject candidates that aren't local maxima -- avoids attaching to
        // skirts of strong harmonics.
        if(b > 1 && v1 < avgMag[(size_t)(b - 1)]) continue;
        if(b + 1 < sz / 2 && v1 < avgMag[(size_t)(b + 1)]) continue;

        const float v2 = (2 * b < sz / 2) ? avgMag[(size_t)(2 * b)] : 0.0f;
        const float v3 = (3 * b < sz / 2) ? avgMag[(size_t)(3 * b)] : 0.0f;
        const float v4 = (4 * b < sz / 2) ? avgMag[(size_t)(4 * b)] : 0.0f;
        // HPS-style harmonic agreement boosts true f0 over half-period mirage.
        const float score = v1 * (v2 + 0.6f * v3 + 0.3f * v4 + 1e-6f);
        if(score > bestScore)
        {
            bestScore = score;
            bestBin = b;
        }
    }

    // Parabolic interpolation on the magnitude spectrum around bestBin.
    float refined = float(bestBin);
    if(bestBin > 0 && bestBin < sz / 2 - 1)
    {
        const float a = avgMag[(size_t)(bestBin - 1)];
        const float c = avgMag[(size_t)(bestBin + 1)];
        const float m = avgMag[(size_t)bestBin];
        const float denom = a - 2.0f * m + c;
        if(std::abs(denom) > 1e-9f)
            refined = float(bestBin) + 0.5f * (a - c) / denom;
    }
    return clampf(refined * binHz, p.minRootHz, p.maxRootHz);
}

// -----------------------------------------------------------------------------
// 3. Per-frame peak candidate extraction. Frequency / magnitude are refined
// with parabolic interpolation in log-mag. Phase is taken from the bin center.
// -----------------------------------------------------------------------------
struct PeakCandidate
{
    float freqHz;
    float mag;
    float phase;
};

void extractPeaksOneFrame(const std::vector<float> &fftBuf, int fftSize,
                          float binHz, float magFloor,
                          std::vector<PeakCandidate> &out)
{
    out.clear();
    auto bin = [&](int b) {
        const float re = fftBuf[(size_t)2 * b];
        const float im = fftBuf[(size_t)2 * b + 1];
        return std::sqrt(re * re + im * im);
    };

    for(int b = 2; b < fftSize / 2 - 2; ++b)
    {
        const float m = bin(b);
        if(m < magFloor) continue;
        const float lm = bin(b - 1);
        const float rm = bin(b + 1);
        if(m <= lm || m <= rm) continue;

        const float denom = lm - 2.0f * m + rm;
        const float delta = (std::abs(denom) > 1e-9f) ? 0.5f * (lm - rm) / denom : 0.0f;
        const float hz = (float(b) + delta) * binHz;

        const float re = fftBuf[(size_t)2 * b];
        const float im = fftBuf[(size_t)2 * b + 1];
        out.push_back({ hz, m, std::atan2(im, re) });
    }
}

// -----------------------------------------------------------------------------
// 4. Partial track linking. Greedy: for each frame we try to extend every
// surviving track by matching the cheapest peak that's within frequency /
// amplitude tolerances. Unmatched peaks become new tracks (birth). Unmatched
// tracks accumulate a gap counter and die after maxGapFrames.
//
// Cost function (paraphrased from architecture spec §8.5):
//   cost = w_f * |log(f_new / f_pred)| + w_a * |log(a_new / a_pred)|
// (phase + harmonic terms left for v2 once we trust the frequency model.)
// -----------------------------------------------------------------------------
struct DenseTrack
{
    std::vector<int> frameIdx;   // index into the dense STFT frame grid
    std::vector<float> freqHz;
    std::vector<float> amp;
    std::vector<float> logAmp;
    std::vector<float> phase;

    bool alive = true;
    int gapCount = 0;

    // Computed by the cleaner:
    float baseFreq = 1.0f;
    float baseRatio = 1.0f;
    float baseLogAmp = 0.0f;
    float peakAmp = 0.0f;
    float meanLogAmp = 0.0f;
    float birthSec = 0.0f;
    float deathSec = 0.0f;
    float freqStability = 1.0f;
    float confidence = 0.0f;
    float harmonicity = 1.0f;
    int harmonicIndex = 1;
    float xPos = 0.0f;
    float importance = 0.0f;
    uint32_t groupFlags = 0u;
    PartialClass partialClass = PartialClass::Uncertain;
};

void linkTracks(const std::vector<std::vector<PeakCandidate>> &perFrame,
                float tPerFrame,
                const FunctionalSampleSourceParams &p,
                std::vector<DenseTrack> &tracks)
{
    tracks.clear();
    tracks.reserve(kMaxAnalysisTracks);

    const int frameCount = (int)perFrame.size();
    if(frameCount == 0) return;

    // Initialize tracks from the first frame's peaks.
    for(const auto &peak : perFrame[0])
    {
        if((int)tracks.size() >= kMaxAnalysisTracks) break;
        DenseTrack tr;
        tr.frameIdx.push_back(0);
        tr.freqHz.push_back(peak.freqHz);
        tr.amp.push_back(peak.mag);
        tr.logAmp.push_back(std::log(std::max(1e-9f, peak.mag)));
        tr.phase.push_back(peak.phase);
        tr.alive = true;
        tr.gapCount = 0;
        tracks.push_back(std::move(tr));
    }

    const float maxFreqJumpLog = std::log(std::pow(2.0f, p.maxFreqJumpCents / 1200.0f));
    const float maxAmpJumpLog  = std::log(std::pow(10.0f, p.maxAmpJumpDb / 20.0f));
    const float wF = 1.0f;
    const float wA = 0.30f;

    std::vector<int> assignedPeak(kMaxAnalysisTracks, -1);

    for(int f = 1; f < frameCount; ++f)
    {
        const auto &peaks = perFrame[(size_t)f];
        const int peakN = (int)peaks.size();
        std::vector<bool> peakUsed((size_t)peakN, false);

        // Build cheapest match for each alive track. Greedy by track index.
        for(int t = 0; t < (int)tracks.size(); ++t)
        {
            auto &tr = tracks[(size_t)t];
            if(!tr.alive) continue;
            const float fPred = tr.freqHz.back();
            const float aPred = tr.amp.back();
            const float lfPred = std::log(std::max(1e-9f, fPred));
            const float laPred = std::log(std::max(1e-9f, aPred));

            int bestIdx = -1;
            float bestCost = 1e9f;
            for(int k = 0; k < peakN; ++k)
            {
                if(peakUsed[(size_t)k]) continue;
                const float fNew = peaks[(size_t)k].freqHz;
                const float aNew = peaks[(size_t)k].mag;
                const float lfNew = std::log(std::max(1e-9f, fNew));
                const float laNew = std::log(std::max(1e-9f, aNew));
                const float dF = std::abs(lfNew - lfPred);
                if(dF > maxFreqJumpLog) continue;
                const float dA = std::abs(laNew - laPred);
                if(dA > maxAmpJumpLog) continue;
                const float cost = wF * dF + wA * dA;
                if(cost < bestCost)
                {
                    bestCost = cost;
                    bestIdx = k;
                }
            }

            if(bestIdx >= 0)
            {
                peakUsed[(size_t)bestIdx] = true;
                tr.frameIdx.push_back(f);
                tr.freqHz.push_back(peaks[(size_t)bestIdx].freqHz);
                tr.amp.push_back(peaks[(size_t)bestIdx].mag);
                tr.logAmp.push_back(std::log(std::max(1e-9f, peaks[(size_t)bestIdx].mag)));
                tr.phase.push_back(peaks[(size_t)bestIdx].phase);
                tr.gapCount = 0;
            }
            else
            {
                ++tr.gapCount;
                if(tr.gapCount > p.maxGapFrames)
                    tr.alive = false;
            }
        }

        // Unmatched peaks: spawn new tracks (track birth).
        for(int k = 0; k < peakN; ++k)
        {
            if(peakUsed[(size_t)k]) continue;
            if((int)tracks.size() >= kMaxAnalysisTracks) break;
            DenseTrack tr;
            tr.frameIdx.push_back(f);
            tr.freqHz.push_back(peaks[(size_t)k].freqHz);
            tr.amp.push_back(peaks[(size_t)k].mag);
            tr.logAmp.push_back(std::log(std::max(1e-9f, peaks[(size_t)k].mag)));
            tr.phase.push_back(peaks[(size_t)k].phase);
            tr.alive = true;
            tracks.push_back(std::move(tr));
        }
    }

    // Final per-track stats (birth/death/peak/mean/stability).
    for(auto &tr : tracks)
    {
        if(tr.frameIdx.empty()) continue;
        tr.birthSec = float(tr.frameIdx.front()) * tPerFrame;
        tr.deathSec = float(tr.frameIdx.back()) * tPerFrame;
        tr.peakAmp = *std::max_element(tr.amp.begin(), tr.amp.end());
        double sum = 0.0;
        for(float la : tr.logAmp) sum += la;
        tr.meanLogAmp = (float)(sum / (double)tr.logAmp.size());
        // Freq stability: 1 / (1 + std/median).
        std::vector<float> freqs = tr.freqHz;
        std::sort(freqs.begin(), freqs.end());
        const float median = freqs[freqs.size() / 2];
        double var = 0.0;
        for(float f : tr.freqHz) { const double d = (double)(f - median); var += d * d; }
        const float std_ = std::sqrt((float)(var / (double)tr.freqHz.size()));
        tr.freqStability = (median > 1e-6f) ? 1.0f / (1.0f + std_ / median) : 0.0f;
        tr.baseFreq = median;
    }
}

// -----------------------------------------------------------------------------
// 5. Clean / classify / select top-N tracks ranked by perceptual importance.
// Perceptual importance combines steady-state energy with transient peak so
// short attack-only partials are not silently dropped.
// -----------------------------------------------------------------------------
void cleanAndSelect(std::vector<DenseTrack> &tracks, float rootHz,
                    float tPerFrame, int targetCount,
                    std::vector<DenseTrack> &out)
{
    out.clear();
    if(tracks.empty()) return;

    const float minDuration = 6.0f * tPerFrame;
    const float minPeakAmp = 1e-4f;

    for(auto &tr : tracks)
    {
        if(tr.frameIdx.size() < 3) continue;
        const float dur = tr.deathSec - tr.birthSec;
        if(dur < minDuration && tr.peakAmp < 10.0f * minPeakAmp) continue;
        if(tr.peakAmp < minPeakAmp) continue;

        tr.baseRatio = std::max(1e-3f, tr.baseFreq / std::max(1e-3f, rootHz));
        const float nearestHarm = std::round(tr.baseRatio);
        tr.harmonicIndex = (int)std::max(1.0f, nearestHarm);
        tr.harmonicity = std::abs(tr.baseRatio - nearestHarm);

        // Confidence = stability * (perceptual loudness proxy)
        const float energyProxy = std::exp(tr.meanLogAmp);
        const float perceptualW = std::log(std::max(2.0f, tr.baseFreq) / 20.0f); // mild high-band weight
        tr.confidence = tr.freqStability *
                        std::tanh(energyProxy * 8.0f) *
                        clampf(perceptualW * 0.3f, 0.2f, 1.5f);

        // Importance: prefer long & loud, but also keep loud-but-short transients.
        const float sustained = std::tanh(dur * 4.0f) * energyProxy;
        const float transient = tr.peakAmp;
        tr.importance = 0.7f * sustained + 0.3f * transient;

        // Classification (cheap heuristic; v2 will sharpen this).
        if(tr.harmonicity < 0.03f && tr.freqStability > 0.8f) tr.partialClass = PartialClass::HarmonicStable;
        else if(tr.harmonicity < 0.03f && dur < 0.10f)        tr.partialClass = PartialClass::HarmonicTransient;
        else if(tr.freqStability > 0.8f)                       tr.partialClass = PartialClass::InharmonicStable;
        else if(dur < 0.10f)                                   tr.partialClass = PartialClass::InharmonicTransient;
        else if(tr.freqStability < 0.55f && tr.harmonicity > 0.12f)
                                                                  tr.partialClass = PartialClass::NoiseLike;
        else                                                     tr.partialClass = PartialClass::Uncertain;

        out.push_back(std::move(tr));
    }
    if(out.empty()) return;

    // Sort by importance desc, keep top-N.
    std::sort(out.begin(), out.end(),
              [](const DenseTrack &a, const DenseTrack &b) { return a.importance > b.importance; });
    if((int)out.size() > targetCount)
        out.resize((size_t)targetCount);

    // Then re-sort by base ratio asc so PartialStatic.harmonicIndex stays monotonic.
    std::sort(out.begin(), out.end(),
              [](const DenseTrack &a, const DenseTrack &b) { return a.baseRatio < b.baseRatio; });
}

// -----------------------------------------------------------------------------
// 6. Time basis evaluation (shared between analyzer and baker).
// All decays are normalized to peak = 1 at their natural peak time so weight
// magnitudes are interpretable.
// -----------------------------------------------------------------------------
float evalBasis(const TimeBasis &b, float t)
{
    const float tau = std::max(1e-4f, b.tau);
    switch(b.kind)
    {
        case TimeBasisKind::GlobalEnvelope:
        case TimeBasisKind::FastDecay:
        case TimeBasisKind::SlowDecay:
        case TimeBasisKind::BrightnessDecay:
        case TimeBasisKind::BodyResonance:
        case TimeBasisKind::PitchRelax:
            return std::exp(-std::max(0.0f, t) / tau);
        case TimeBasisKind::AttackBurst:
        {
            const float d = t - b.center;
            const float s = std::max(1e-6f, b.sigma);
            return std::exp(-(d * d) / (2.0f * s * s));
        }
        case TimeBasisKind::VibratoSine:
        {
            const float rate = std::max(0.05f, b.rateHz);
            return std::sin(kTwoPi * rate * std::max(0.0f, t))
                   * std::exp(-std::max(0.0f, t) / tau);
        }
    }
    return 0.0f;
}

// Macro routing: which macro knob scales which basis at bake time.
float macroScaleForBasis(TimeBasisKind kind, const FunctionalSampleSourceParams &p)
{
    switch(kind)
    {
        case TimeBasisKind::AttackBurst:     return clampf(p.attackSharpness, 0.0f, 4.0f);
        case TimeBasisKind::FastDecay:
        case TimeBasisKind::BrightnessDecay: return clampf(p.brightnessDecay, 0.0f, 4.0f);
        case TimeBasisKind::SlowDecay:
        case TimeBasisKind::BodyResonance:   return clampf(p.bodyResonance, 0.0f, 4.0f);
        case TimeBasisKind::GlobalEnvelope:
        case TimeBasisKind::PitchRelax:
        case TimeBasisKind::VibratoSine:     return 1.0f;
    }
    return 1.0f;
}

// -----------------------------------------------------------------------------
// 7. Build the basis bank for the analyzer. Standard pack covers a piano-class
// sound: short attack burst, fast die-off, slower decay tail. Draft drops two
// of the basis; High adds two more for brightness / body resonance.
// -----------------------------------------------------------------------------
int buildAmpBasis(FunctionalSampleQuality q, float duration,
                  std::array<TimeBasis, kMaxFunctionalBasis> &out)
{
    int n = 0;
    auto push = [&](TimeBasisKind k, float tau, float center = 0.0f, float sigma = 0.005f) {
        if(n >= kMaxFunctionalBasis) return;
        TimeBasis b;
        b.kind = k;
        b.tau = tau;
        b.center = center;
        b.sigma = sigma;
        out[(size_t)n++] = b;
    };

    const float dur = std::max(0.1f, duration);

    push(TimeBasisKind::GlobalEnvelope, std::max(0.5f, dur / 2.5f));
    push(TimeBasisKind::AttackBurst,    1.0f, 0.0f, std::max(0.005f, 0.015f));
    push(TimeBasisKind::FastDecay,      0.060f);

    if(q == FunctionalSampleQuality::Draft)
        return n;

    push(TimeBasisKind::SlowDecay, std::max(0.25f, dur * 0.45f));

    if(q == FunctionalSampleQuality::High)
    {
        push(TimeBasisKind::BrightnessDecay, 0.180f);
        push(TimeBasisKind::BodyResonance,   std::max(0.7f, dur * 1.2f));
    }
    return n;
}

int buildFreqBasis(FunctionalSampleQuality q, float duration,
                   std::array<TimeBasis, kMaxFunctionalBasis> &out)
{
    int n = 0;
    auto push = [&](TimeBasisKind k, float tau, float rateHz = 0.0f) {
        if(n >= kMaxFunctionalBasis) return;
        TimeBasis b;
        b.kind = k;
        b.tau = tau;
        b.rateHz = rateHz;
        out[(size_t)n++] = b;
    };

    const float dur = std::max(0.1f, duration);
    push(TimeBasisKind::PitchRelax, std::clamp(dur * 0.20f, 0.04f, 0.60f));
    if(q != FunctionalSampleQuality::Draft)
        push(TimeBasisKind::PitchRelax, std::clamp(dur * 0.75f, 0.18f, 2.40f));
    if(q == FunctionalSampleQuality::High)
        push(TimeBasisKind::VibratoSine, std::clamp(dur * 0.90f, 0.25f, 3.00f), 5.0f);
    return n;
}

// -----------------------------------------------------------------------------
// 8. Tiny ridge-regression solver. M is [B x F] basis values, r is [F]
// residual log-amp samples, w is [B] weight vector. We solve
//   (M M^T + lambda I) w = M r
// with a small Cholesky decomposition. B <= kMaxFunctionalBasis (6), so this
// is cheap.
// -----------------------------------------------------------------------------
void ridgeSolve(const std::vector<std::vector<float>> &M, // [B][F]
                const std::vector<float> &r,              // [F]
                float lambda,
                std::vector<float> &w)                    // [B] out
{
    const int B = (int)M.size();
    const int F = (int)r.size();
    w.assign((size_t)B, 0.0f);
    if(B == 0 || F == 0) return;

    // A = M M^T + lambda I (B x B)
    std::vector<std::vector<double>> A((size_t)B, std::vector<double>((size_t)B, 0.0));
    std::vector<double> y((size_t)B, 0.0);
    for(int i = 0; i < B; ++i)
    {
        for(int j = i; j < B; ++j)
        {
            double s = 0.0;
            for(int k = 0; k < F; ++k)
                s += (double)M[(size_t)i][(size_t)k] * (double)M[(size_t)j][(size_t)k];
            A[(size_t)i][(size_t)j] = s;
            A[(size_t)j][(size_t)i] = s;
        }
        A[(size_t)i][(size_t)i] += (double)lambda;
        double s = 0.0;
        for(int k = 0; k < F; ++k)
            s += (double)M[(size_t)i][(size_t)k] * (double)r[(size_t)k];
        y[(size_t)i] = s;
    }

    // Cholesky A = L L^T (in-place on a copy).
    std::vector<std::vector<double>> L = A;
    for(int i = 0; i < B; ++i)
    {
        for(int j = 0; j <= i; ++j)
        {
            double s = L[(size_t)i][(size_t)j];
            for(int k = 0; k < j; ++k)
                s -= L[(size_t)i][(size_t)k] * L[(size_t)j][(size_t)k];
            if(i == j)
            {
                if(s <= 0.0) { return; } // not pos-def; leave weights zero
                L[(size_t)i][(size_t)j] = std::sqrt(s);
            }
            else
            {
                L[(size_t)i][(size_t)j] = s / L[(size_t)j][(size_t)j];
            }
        }
        for(int j = i + 1; j < B; ++j) L[(size_t)i][(size_t)j] = 0.0;
    }

    // Solve L z = y
    std::vector<double> z((size_t)B, 0.0);
    for(int i = 0; i < B; ++i)
    {
        double s = y[(size_t)i];
        for(int k = 0; k < i; ++k) s -= L[(size_t)i][(size_t)k] * z[(size_t)k];
        z[(size_t)i] = s / L[(size_t)i][(size_t)i];
    }
    // Solve L^T w = z
    for(int i = B - 1; i >= 0; --i)
    {
        double s = z[(size_t)i];
        for(int k = i + 1; k < B; ++k) s -= L[(size_t)k][(size_t)i] * (double)w[(size_t)k];
        w[(size_t)i] = (float)(s / L[(size_t)i][(size_t)i]);
    }
}

// -----------------------------------------------------------------------------
// 9. FunctionalCompressor: per partial, fit log A_i(t) ~= baseLog + sum_m W_m T_m(t).
// baseLog is the median log-amplitude across the live track so the residual to
// be fitted is mean-zero. We use a small ridge regularizer (lambda) to keep
// weights bounded and avoid overfitting on short tracks.
// -----------------------------------------------------------------------------
void compressToFunctional(const std::vector<DenseTrack> &tracks,
                          float rootHz,
                          float durationSec,
                          float tPerFrame,
                          FunctionalSampleQuality quality,
                          FunctionalSpectralSource &out)
{
    out.partialCount = (int)std::min<size_t>(tracks.size(), (size_t)kMaxPartials);
    out.durationSeconds = durationSec;
    out.rootHz = rootHz;
    out.rootMidi = (int)std::round(69.0f + 12.0f * std::log2(std::max(1e-3f, rootHz) / 440.0f));

    out.ampBasisCount = buildAmpBasis(quality, durationSec, out.ampBasis);
    out.freqBasisCount = buildFreqBasis(quality, durationSec, out.freqBasis);

    // We'll need track energy for bake-time loudness balancing across quality levels.
    float globalPeak = 1e-6f;
    double selectedPeakEnergy = 0.0;

    const int B = out.ampBasisCount;
    const int FB = out.freqBasisCount;
    for(int i = 0; i < out.partialCount; ++i)
    {
        const auto &tr = tracks[(size_t)i];
        PartialStatic ps;
        ps.baseRatio = tr.baseRatio;
        ps.phaseInit = tr.phase.empty() ? 0.0f : wrapPi(tr.phase.front());
        ps.birthTime = tr.birthSec;
        ps.deathTime = tr.deathSec;
        ps.confidence = clampf(tr.confidence, 0.0f, 1.0f);
        ps.harmonicity = clampf(tr.harmonicity, 0.0f, 2.0f);
        ps.harmonicIndex = (uint8_t)std::min(255, std::max(1, tr.harmonicIndex));
        ps.partialClass = (uint8_t)tr.partialClass;
        ps.groupFlags = 0u;

        // baseLogAmp = median of logAmp samples.
        std::vector<float> sorted = tr.logAmp;
        std::sort(sorted.begin(), sorted.end());
        const float median = sorted.empty() ? -10.0f : sorted[sorted.size() / 2];
        ps.baseLogAmp = median;

        // Build basis matrix and residual.
        const int F = (int)tr.logAmp.size();
        std::vector<std::vector<float>> M((size_t)B, std::vector<float>((size_t)F, 0.0f));
        std::vector<float> r((size_t)F, 0.0f);
        for(int k = 0; k < F; ++k)
        {
            const float tk = float(tr.frameIdx[(size_t)k]) * tPerFrame;
            r[(size_t)k] = tr.logAmp[(size_t)k] - ps.baseLogAmp;
            for(int m = 0; m < B; ++m)
                M[(size_t)m][(size_t)k] = evalBasis(out.ampBasis[(size_t)m], tk);
        }
        // Ridge solve; lambda scaled to track length so short tracks aren't dominated.
        const float lambda = std::max(1e-4f, 0.05f / float(std::max(1, F)));
        std::vector<float> w;
        ridgeSolve(M, r, lambda, w);
        for(int m = 0; m < B; ++m)
            out.ampWeights[(size_t)i][(size_t)m] = clampf(w[(size_t)m], -8.0f, 8.0f);
        // Zero unused slots.
        for(int m = B; m < kMaxFunctionalBasis; ++m)
            out.ampWeights[(size_t)i][(size_t)m] = 0.0f;

        std::vector<std::vector<float>> FM((size_t)FB, std::vector<float>((size_t)F, 0.0f));
        std::vector<float> fr((size_t)F, 0.0f);
        const float baseFreq = std::max(1e-6f, tr.baseFreq);
        for(int k = 0; k < F; ++k)
        {
            const float tk = float(tr.frameIdx[(size_t)k]) * tPerFrame;
            fr[(size_t)k] = std::log(std::max(1e-6f, tr.freqHz[(size_t)k]) / baseFreq);
            for(int m = 0; m < FB; ++m)
                FM[(size_t)m][(size_t)k] = evalBasis(out.freqBasis[(size_t)m], tk);
        }
        std::vector<float> fw;
        ridgeSolve(FM, fr, std::max(1e-4f, 0.10f / float(std::max(1, F))), fw);
        for(int m = 0; m < FB; ++m)
            out.freqWeights[(size_t)i][(size_t)m] = clampf(fw[(size_t)m], -0.40f, 0.40f);
        for(int m = FB; m < kMaxFunctionalBasis; ++m)
            out.freqWeights[(size_t)i][(size_t)m] = 0.0f;

        out.partials[(size_t)i] = ps;
        globalPeak = std::max(globalPeak, tr.peakAmp);
        selectedPeakEnergy += double(tr.peakAmp) * double(tr.peakAmp);
    }

    // Compute x_i = normalized log ratio in [0, 1] over kept partials.
    float minLog = 1e9f, maxLog = -1e9f;
    for(int i = 0; i < out.partialCount; ++i)
    {
        const float lr = std::log(std::max(1e-3f, out.partials[(size_t)i].baseRatio));
        minLog = std::min(minLog, lr);
        maxLog = std::max(maxLog, lr);
    }
    const float spread = std::max(1e-3f, maxLog - minLog);
    for(int i = 0; i < out.partialCount; ++i)
    {
        const float lr = std::log(std::max(1e-3f, out.partials[(size_t)i].baseRatio));
        const float xi = (lr - minLog) / spread;
        out.partials[(size_t)i].xPos = clampf(xi, 0.0f, 1.0f);
        out.partials[(size_t)i].groupFlags = (uint32_t)((xi < 0.34f) ? 0 : ((xi < 0.67f) ? 1 : 2));
    }

    // Normalize peak amp so that exp(baseLogAmp + sum weight * basisAt0) max ~ 1.
    // Cheaper proxy: divide by global peak so baseLogAmp is referenced against
    // the maximum observed amplitude. Voice level / global gain takes it from there.
    const float logPeak = std::log(std::max(1e-9f, globalPeak));
    for(int i = 0; i < out.partialCount; ++i)
        out.partials[(size_t)i].baseLogAmp -= logPeak;
    const double normEnergy = selectedPeakEnergy / std::max(1e-12, double(globalPeak) * double(globalPeak));
    const float normEnergyF = (float)std::max(1e-6, normEnergy);
    out.loudnessNormGain = clampf(0.90f / std::sqrt(normEnergyF), 0.08f, 1.0f);

    // Quality summary.
    float confSum = 0.0f;
    for(int i = 0; i < out.partialCount; ++i)
        confSum += out.partials[(size_t)i].confidence;
    out.analysisQuality = out.partialCount > 0
                              ? clampf(confSum / float(out.partialCount), 0.0f, 1.0f)
                              : 0.0f;

    out.valid = (out.partialCount > 0);
}

// -----------------------------------------------------------------------------
// 10. Top-level analyzer.
// -----------------------------------------------------------------------------
void analyzeSourceImpl(const FunctionalSampleSourceParams &p,
                       FunctionalSpectralSource &out)
{
    // Reset.
    out = FunctionalSpectralSource {};
    out.transientEnabled = p.transientAmount > 1e-4f;
    out.residualEnabled = p.residualAmount > 1e-4f;
    out.transientAmount = clampf(p.transientAmount, 0.0f, 1.0f);
    out.residualAmount = clampf(p.residualAmount, 0.0f, 1.0f);

    std::vector<float> mono;
    double sr = 48000.0;
    if(!loadMonoFile(p.filePath, mono, sr))
        return;
    if(mono.size() < 4096)
        return;

    preprocessSample(mono, sr);
    if(mono.size() < 2048)
        return;

    const float rootHz = estimateRoot(mono, sr, p);

    // STFT params by quality.
    int trackOrder = 11;            // 2048
    int trackHopSamples = std::max(64, (int)std::round(sr * 0.005));
    int targetPartials = 64;
    switch(p.quality)
    {
        case FunctionalSampleQuality::Draft:
            trackOrder = 10;
            trackHopSamples = std::max(64, (int)std::round(sr * 0.010));
            targetPartials = 32;
            break;
        case FunctionalSampleQuality::Standard:
            trackOrder = 11;
            trackHopSamples = std::max(64, (int)std::round(sr * 0.005));
            targetPartials = 64;
            break;
        case FunctionalSampleQuality::High:
            trackOrder = 12;
            trackHopSamples = std::max(64, (int)std::round(sr * 0.003));
            targetPartials = 128;
            break;
    }
    targetPartials = std::clamp(std::min(targetPartials, p.partialCount), 1, kMaxPartials);

    const int trackFftSize = 1 << trackOrder;
    if((int)mono.size() < trackFftSize)
        return;
    const float trackBinHz = (float)sr / (float)trackFftSize;
    const float tPerFrame = float(trackHopSamples) / float(sr);
    const int frames = std::max(1, ((int)mono.size() - trackFftSize) / trackHopSamples + 1);

    juce::dsp::FFT fft(trackOrder);
    juce::dsp::WindowingFunction<float> win((size_t)trackFftSize,
                                            juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> fftBuf((size_t)trackFftSize * 2, 0.0f);

    // Compute a rough magnitude floor across the file so we don't link noise.
    float magFloor = 0.0f;
    {
        std::fill(fftBuf.begin(), fftBuf.end(), 0.0f);
        const int probeStart = std::max(0, (int)mono.size() - trackFftSize);
        for(int i = 0; i < trackFftSize; ++i) fftBuf[(size_t)i] = mono[(size_t)(probeStart + i)];
        win.multiplyWithWindowingTable(fftBuf.data(), (size_t)trackFftSize);
        fft.performRealOnlyForwardTransform(fftBuf.data());
        double sum = 0.0;
        int cnt = 0;
        for(int b = 1; b < trackFftSize / 2; ++b)
        {
            const float re = fftBuf[(size_t)2 * b];
            const float im = fftBuf[(size_t)2 * b + 1];
            const float m = std::sqrt(re * re + im * im);
            sum += m;
            ++cnt;
        }
        const float avg = (cnt > 0) ? (float)(sum / cnt) : 0.0f;
        magFloor = std::max(1e-4f, avg * 3.0f);
    }

    std::vector<std::vector<PeakCandidate>> perFrame((size_t)frames);
    std::vector<PeakCandidate> framePeaks;
    framePeaks.reserve(64);

    for(int f = 0; f < frames; ++f)
    {
        const int start = std::min(f * trackHopSamples,
                                   std::max(0, (int)mono.size() - trackFftSize));
        std::fill(fftBuf.begin(), fftBuf.end(), 0.0f);
        for(int i = 0; i < trackFftSize; ++i)
            fftBuf[(size_t)i] = mono[(size_t)(start + i)];
        win.multiplyWithWindowingTable(fftBuf.data(), (size_t)trackFftSize);
        fft.performRealOnlyForwardTransform(fftBuf.data());
        extractPeaksOneFrame(fftBuf, trackFftSize, trackBinHz, magFloor, framePeaks);
        perFrame[(size_t)f] = framePeaks;
    }

    std::vector<DenseTrack> rawTracks;
    linkTracks(perFrame, tPerFrame, p, rawTracks);

    std::vector<DenseTrack> kept;
    cleanAndSelect(rawTracks, rootHz, tPerFrame, targetPartials, kept);

    const float durationSec = float(mono.size()) / float(sr);
    compressToFunctional(kept, rootHz, durationSec, tPerFrame, p.quality, out);
}

// -----------------------------------------------------------------------------
// 11. Adaptive timeline frame placement. Candidate times are scored by basis
// curvature so attack, resonant shoulders, and frequency motion receive more
// frame budget than flat tails.
// -----------------------------------------------------------------------------
void buildFrameTimes(const FunctionalSpectralSource &src,
                     int frameCount,
                     float &durationOut,
                     std::vector<float> &times)
{
    // Find attack sigma and shortest decay tau in the basis bank.
    float attackSigma = 0.020f;
    float fastTau = 0.080f;
    for(int m = 0; m < src.ampBasisCount; ++m)
    {
        if(src.ampBasis[(size_t)m].kind == TimeBasisKind::AttackBurst)
            attackSigma = std::max(0.001f, src.ampBasis[(size_t)m].sigma);
        if(src.ampBasis[(size_t)m].kind == TimeBasisKind::FastDecay
           || src.ampBasis[(size_t)m].kind == TimeBasisKind::BrightnessDecay)
            fastTau = std::min(fastTau, src.ampBasis[(size_t)m].tau);
    }

    const float horizon = std::clamp(src.durationSeconds, 0.25f, 8.0f);
    durationOut = horizon;

    constexpr int kCandidates = 160;
    struct Candidate { float t = 0.0f; float score = 0.0f; };
    std::vector<Candidate> candidates;
    candidates.reserve(kCandidates);
    const float dt = horizon / float(kCandidates - 1);
    auto basisCurvature = [&](const TimeBasis &b, float t) {
        const float l = evalBasis(b, std::max(0.0f, t - dt));
        const float c = evalBasis(b, t);
        const float r = evalBasis(b, std::min(horizon, t + dt));
        return std::abs(l - 2.0f * c + r);
    };
    for(int i = 0; i < kCandidates; ++i)
    {
        const float t = dt * float(i);
        float score = (t <= std::max(8.0f * attackSigma, fastTau * 2.5f)) ? 0.05f : 0.0f;
        for(int m = 0; m < src.ampBasisCount; ++m)
            score += basisCurvature(src.ampBasis[(size_t)m], t);
        for(int m = 0; m < src.freqBasisCount; ++m)
            score += 0.65f * basisCurvature(src.freqBasis[(size_t)m], t);
        candidates.push_back({ t, score });
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.score > b.score; });

    times.clear();
    times.reserve((size_t)frameCount);
    times.push_back(0.0f);
    times.push_back(horizon);
    for(const auto &c : candidates)
    {
        if((int)times.size() >= frameCount) break;
        bool tooClose = false;
        for(float chosen : times)
        {
            if(std::abs(chosen - c.t) < dt * 1.5f)
            {
                tooClose = true;
                break;
            }
        }
        if(!tooClose)
            times.push_back(c.t);
    }
    while((int)times.size() < frameCount)
        times.push_back(horizon * float(times.size()) / float(std::max(1, frameCount - 1)));
    std::sort(times.begin(), times.end());
}

void reconstructFrame(const FunctionalSpectralSource &src,
                      const FunctionalSampleSourceParams &p,
                      float t,
                      StaticSpectralFrame &out)
{
    const int N = src.partialCount;
    out.partialCount = N;
    out.freqMode = FreqMode::RelativeRatio;
    out.phaseInitMode = p.phaseInitMode;
    out.phaseSeed = p.phaseSeed;

    // Pre-evaluate basis at this time once (shared across all partials).
    std::array<float, kMaxFunctionalBasis> basisVal {};
    std::array<float, kMaxFunctionalBasis> macroVal {};
    std::array<float, kMaxFunctionalBasis> freqBasisVal {};
    for(int m = 0; m < src.ampBasisCount; ++m)
    {
        basisVal[(size_t)m] = evalBasis(src.ampBasis[(size_t)m], t);
        macroVal[(size_t)m] = macroScaleForBasis(src.ampBasis[(size_t)m].kind, p);
    }
    for(int m = 0; m < src.freqBasisCount; ++m)
        freqBasisVal[(size_t)m] = evalBasis(src.freqBasis[(size_t)m], t);

    for(int i = 0; i < N; ++i)
    {
        const auto &ps = src.partials[(size_t)i];
        float logA = ps.baseLogAmp;
        for(int m = 0; m < src.ampBasisCount; ++m)
            logA += src.ampWeights[(size_t)i][(size_t)m] * macroVal[(size_t)m] * basisVal[(size_t)m];
        float amp = std::exp(logA) * src.loudnessNormGain;
        const auto cls = (PartialClass)ps.partialClass;
        if(src.transientEnabled
           && (cls == PartialClass::HarmonicTransient || cls == PartialClass::InharmonicTransient))
        {
            const float transientShape = std::exp(-(t * t) / (2.0f * 0.020f * 0.020f));
            amp *= 1.0f + clampf(src.transientAmount, 0.0f, 1.0f) * transientShape;
        }

        // Birth/death gate. Use a small fade-in around birth so the very first
        // timeline frame at t=0 doesn't pop. Death zeroes hard since most
        // natural samples decay to silence.
        if(t < ps.birthTime || t > ps.deathTime + 0.005f) amp = 0.0f;
        else if(t < ps.birthTime + 0.002f)
        {
            const float u = clampf((t - ps.birthTime) / 0.002f, 0.0f, 1.0f);
            amp *= u;
        }

        float logRatio = std::log(std::max(1e-6f, ps.baseRatio));
        for(int m = 0; m < src.freqBasisCount; ++m)
            logRatio += src.freqWeights[(size_t)i][(size_t)m] * freqBasisVal[(size_t)m];
        out.nu[i] = std::exp(logRatio);
        out.amp[i] = amp;
        out.x[i] = ps.xPos;
        out.mu[i] = (uint8_t)(ps.groupFlags & 0xFFu);
        out.phaseLocked[i] = ps.phaseInit;
        out.phaseDriftHz[i] = 0.0f;
        out.phaseJitter[i] = (src.residualEnabled
                              && (cls == PartialClass::NoiseLike || cls == PartialClass::Uncertain))
                                 ? clampf(src.residualAmount, 0.0f, 1.0f) * 0.15f
                                 : 0.0f;
        out.attackScale[i] = 1.0f;
        out.decayScale[i] = 1.0f;
        out.sustainLevel[i] = 1.0f;
        out.releaseScale[i] = 1.0f;
    }
    for(int i = N; i < kMaxPartials; ++i)
    {
        out.nu[i] = 0.0f;
        out.amp[i] = 0.0f;
        out.x[i] = 0.0f;
        out.mu[i] = 0u;
        out.phaseLocked[i] = 0.0f;
        out.phaseDriftHz[i] = 0.0f;
        out.phaseJitter[i] = 0.0f;
        out.attackScale[i] = 1.0f;
        out.decayScale[i] = 1.0f;
        out.sustainLevel[i] = 1.0f;
        out.releaseScale[i] = 1.0f;
    }
    // ref times left at 0 -- the timeline carries the envelope explicitly.
    out.refAttackSec = 0.0f;
    out.refDecaySec = 0.0f;
    out.refReleaseSec = 0.0f;
}

} // anonymous namespace

// =============================================================================
// Public API (declared in Generators.h).
// =============================================================================
FunctionalSpectralSource analyzeSource(const FunctionalSampleSourceParams &p)
{
    FunctionalSpectralSource out;
    analyzeSourceImpl(p, out);
    return out;
}

void bakeToTimeline(const FunctionalSpectralSource &src,
                    const FunctionalSampleSourceParams &p,
                    SpectralTimeline &out)
{
    initTimelineDefaults(out);
    if(!src.valid)
    {
        out.frameCount = 1;
        out.durationSeconds = 0.0f;
        out.loop = false;
        out.timeSeconds[0] = 0.0f;
        // out.frames[0] left as defaults so audio stays silent.
        return;
    }

    const int frameCount = kMaxTimelineFrames;
    std::vector<float> times;
    float duration = 0.0f;
    buildFrameTimes(src, frameCount, duration, times);

    out.frameCount = frameCount;
    out.durationSeconds = duration;
    out.loop = false;

    for(int f = 0; f < frameCount; ++f)
    {
        out.timeSeconds[(size_t)f] = times[(size_t)f];
        reconstructFrame(src, p, times[(size_t)f], out.frames[(size_t)f]);
    }
}

void bakeToStaticFrame(const FunctionalSpectralSource &src,
                       const FunctionalSampleSourceParams &p,
                       StaticSpectralFrame &out)
{
    initFrameDefaults(out);
    if(!src.valid)
        return;
    // A spot just past the attack burst gives the most representative spectrum
    // for the UI's static spectrum view. Pick max(AttackBurst sigma, 10 ms).
    float t = 0.020f;
    for(int m = 0; m < src.ampBasisCount; ++m)
    {
        if(src.ampBasis[(size_t)m].kind == TimeBasisKind::AttackBurst)
            t = std::max(t, 3.0f * src.ampBasis[(size_t)m].sigma);
    }
    reconstructFrame(src, p, t, out);
}

} // namespace functional
} // namespace synth
