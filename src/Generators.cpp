#include "Generators.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace synth
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float clipSym(float x, float lo, float hi) { return clampf(x, lo, hi); }
inline float wrapPi(float x)
{
    while(x > kPi) x -= kTwoPi;
    while(x < -kPi) x += kTwoPi;
    return x;
}

// Phi_mode shape functions (§1.1.4)
inline float phiTilt(float x, float pt)
{
    pt = std::max(0.05f, pt);
    const float xp = std::pow(std::max(1e-6f, x), pt);
    const float ymp = std::pow(std::max(1e-6f, 1.0f - x), pt);
    return 2.0f * (xp / (xp + ymp)) - 1.0f;
}
inline float phiSym(float x, float cs, float ps)
{
    ps = std::max(0.05f, ps);
    const float v = 1.0f - std::pow(std::abs(2.0f * (x - cs)), ps);
    return clipSym(v, -1.0f, 1.0f);
}
inline float phiSkew(float x, float pk)
{
    pk = std::max(0.05f, pk);
    const float u = 2.0f * x - 1.0f;
    const float sgn = (u >= 0.0f) ? 1.0f : -1.0f;
    return 4.0f * x * (1.0f - x) * sgn * std::pow(std::abs(u), pk);
}
inline float warpEval(WarpMode m, float x, float p, float cs)
{
    switch(m)
    {
        case WarpMode::Tilt: return phiTilt(x, p);
        case WarpMode::Sym: return phiSym(x, cs, p);
        case WarpMode::Skew: return phiSkew(x, p);
    }
    return 0.0f;
}

// Initialize phase-locked array based on mode (used as Phi_i^init source by Voice).
void initPhaseTable(StaticSpectralFrame &f, PhaseInitMode m, uint32_t seed, int N)
{
    f.phaseInitMode = m;
    f.phaseSeed = seed;
    std::mt19937 rng(seed ^ 0x9E3779B9u);
    std::uniform_real_distribution<float> uni(0.0f, kTwoPi);
    for(int i = 0; i < kMaxPartials; ++i)
    {
        switch(m)
        {
            case PhaseInitMode::Zero:        f.phaseLocked[i] = 0.0f; break;
            case PhaseInitMode::Random:      f.phaseLocked[i] = uni(rng); break;
            case PhaseInitMode::Locked:      f.phaseLocked[i] = (float)((i * 7) % 16) / 16.0f * kTwoPi; break;
            case PhaseInitMode::Alternating: f.phaseLocked[i] = (i & 1) ? kPi : 0.0f; break;
        }
        if(i >= N)
            f.phaseLocked[i] = 0.0f;
    }
}

void zeroTrailing(StaticSpectralFrame &f, int N)
{
    for(int i = N; i < kMaxPartials; ++i)
    {
        f.nu[i] = 0.0f;
        f.amp[i] = 0.0f;
        f.x[i] = 0.0f;
        f.mu[i] = 0u;
        f.phaseLocked[i] = 0.0f;
        f.phaseDriftHz[i] = 0.0f;
        f.phaseJitter[i] = 0.0f;
        f.attackScale[i] = 1.0f;
        f.decayScale[i] = 1.0f;
        f.sustainLevel[i] = 1.0f;
        f.releaseScale[i] = 1.0f;
    }
}

void applyPerPartialAdsrSpread(StaticSpectralFrame &f, int N, float decaySpread, float releaseSpread)
{
    for(int i = 0; i < N; ++i)
    {
        const float xi = f.x[i];
        // High x => smaller scale => faster decay/release.
        f.decayScale[i]   = 1.0f / (1.0f + decaySpread   * 4.0f * xi);
        f.releaseScale[i] = 1.0f / (1.0f + releaseSpread * 4.0f * xi);
    }
}

inline float computeNuDirect(FreqShape shape, float n, float inharm)
{
    switch(shape)
    {
        case FreqShape::Harmonic:    return std::pow(n, 1.0f + 0.5f * inharm);
        case FreqShape::Linear:      return 1.0f + (n - 1.0f) * (1.0f + 0.3f * inharm);
        case FreqShape::Exponential: return n * std::exp(0.03f * inharm * (n - 1.0f));
    }
    return n;
}

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
    // Capture the full audio length so per-partial envelopes (especially the
    // transient and the natural release tail) are fully observable.
    // The previous 1<<21 hard cap was dropped per design notes; we still guard
    // against absurd 32-bit overflow only.
    const juce::int64 raw = reader->lengthInSamples;
    constexpr juce::int64 kSampleHardCap = juce::int64(1) << 27; // ~2730 s @ 48 kHz
    const int len = (int)std::min<juce::int64>(raw, kSampleHardCap);
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

// The old single-pass sample analyzer has been removed in v0.3 and replaced by
// the FunctionalSampleSource pipeline (see FunctionalSampleSource.cpp).
#if 0
bool extractSamplePartialSet_REMOVED(StaticSpectralFrame &out)
{
    std::vector<float> mono;
    double sr = 48000.0;
    if(!loadMonoFile(p.filePath, mono, sr) || mono.size() < 2048)
        return false;

    const int monoLen = (int)mono.size();

    // -----------------------------------------------------------------
    // Pass 1 - peak detection on a long-window magnitude average.
    // Large FFT (~2.9 Hz bin @ 48 kHz) so harmonics resolve cleanly.
    // -----------------------------------------------------------------
    constexpr int detectFftOrder = 14;
    constexpr int detectFftSize = 1 << detectFftOrder;
    const int detectHop = std::max(1, detectFftSize / 2);
    juce::dsp::FFT detectFft(detectFftOrder);
    juce::dsp::WindowingFunction<float> detectWin(
        (size_t)detectFftSize, juce::dsp::WindowingFunction<float>::hann);

    const int detectFrames = std::max(1, (monoLen - detectFftSize) / detectHop + 1);
    std::vector<float> avgMag((size_t)detectFftSize / 2, 0.0f);
    std::vector<float> phaseRe((size_t)detectFftSize / 2, 0.0f);
    std::vector<float> phaseIm((size_t)detectFftSize / 2, 0.0f);

    std::vector<float> detectBuf((size_t)detectFftSize * 2, 0.0f);
    for(int f = 0; f < detectFrames; ++f)
    {
        const int start = std::min(f * detectHop, std::max(0, monoLen - detectFftSize));
        std::fill(detectBuf.begin(), detectBuf.end(), 0.0f);
        for(int i = 0; i < detectFftSize; ++i)
            detectBuf[(size_t)i] = mono[(size_t)(start + i)];
        detectWin.multiplyWithWindowingTable(detectBuf.data(), (size_t)detectFftSize);
        detectFft.performRealOnlyForwardTransform(detectBuf.data());

        for(int b = 1; b < detectFftSize / 2; ++b)
        {
            const float re = detectBuf[(size_t)2 * b];
            const float im = detectBuf[(size_t)2 * b + 1];
            const float mag = std::sqrt(re * re + im * im);
            avgMag[(size_t)b] += mag;
            if(mag > 1e-9f)
            {
                phaseRe[(size_t)b] += (re / mag) * mag;
                phaseIm[(size_t)b] += (im / mag) * mag;
            }
        }
    }
    const float invDetectFrames = 1.0f / float(detectFrames);
    for(auto &m : avgMag)
        m *= invDetectFrames;

    struct Peak { float hz; float mag; int bin; };
    std::vector<Peak> peaks;
    const float detectBinHz = (float)sr / (float)detectFftSize;
    const int loBin = std::max(1, (int)(20.0f / detectBinHz));
    const int hiBin = std::min(detectFftSize / 2 - 2, (int)(kAudibleMaxHz / detectBinHz));
    for(int b = loBin + 1; b < hiBin; ++b)
    {
        const float m = avgMag[(size_t)b];
        if(m > avgMag[(size_t)b - 1] && m > avgMag[(size_t)b + 1] && m > 1e-5f)
        {
            const float a = avgMag[(size_t)b - 1];
            const float c = avgMag[(size_t)b + 1];
            const float denom = a - 2.0f * m + c;
            const float delta = std::abs(denom) > 1e-9f ? 0.5f * (a - c) / denom : 0.0f;
            peaks.push_back({(float(b) + delta) * detectBinHz, m, b});
        }
    }
    if(peaks.empty())
        return false;

    auto rootIt = std::max_element(peaks.begin(), peaks.end(), [&](const Peak &a, const Peak &b) {
        const bool aIn = a.hz >= p.minRootHz && a.hz <= p.maxRootHz;
        const bool bIn = b.hz >= p.minRootHz && b.hz <= p.maxRootHz;
        if(aIn != bIn)
            return !aIn;
        return a.mag < b.mag;
    });
    if(rootIt == peaks.end() || rootIt->hz < p.minRootHz || rootIt->hz > p.maxRootHz)
        return false;

    const float f0 = std::max(1.0f, rootIt->hz);
    std::vector<Peak> selected;
    selected.reserve((size_t)kMaxPartials);
    selected.push_back(*rootIt);
    for(const auto &peak : peaks)
        if(peak.hz >= f0 * 1.20f)
            selected.push_back(peak);

    std::sort(selected.begin(), selected.end(), [](const Peak &a, const Peak &b) { return a.mag > b.mag; });
    const int N = std::clamp(p.partialCount, 1, kMaxPartials);
    if((int)selected.size() > N)
        selected.resize((size_t)N);
    std::sort(selected.begin(), selected.end(), [](const Peak &a, const Peak &b) { return a.hz < b.hz; });

    out.partialCount = (int)selected.size();
    out.freqMode = FreqMode::RelativeRatio;
    out.phaseInitMode = PhaseInitMode::Locked;
    out.phaseSeed = p.phaseSeed;
    const int got = out.partialCount;

    // -----------------------------------------------------------------
    // Pass 2 - per-partial amplitude / phase tracking with a small FFT
    // (~43 ms window, ~5 ms hop). This is what captures the piano's
    // ~5-15 ms attack instead of smearing it across the long detect FFT.
    // -----------------------------------------------------------------
    constexpr int trackFftOrder = 11;            // 2048 samples
    constexpr int trackFftSize = 1 << trackFftOrder;
    const int trackHop = std::max(64, (int)std::round(sr * 0.005)); // ~5 ms
    juce::dsp::FFT trackFft(trackFftOrder);
    juce::dsp::WindowingFunction<float> trackWin(
        (size_t)trackFftSize, juce::dsp::WindowingFunction<float>::hann);

    const int trackFrames = std::max(1, (monoLen - trackFftSize) / trackHop + 1);
    const float tPerTrackFrame = float(trackHop) / float(sr);
    const float trackBinHz = (float)sr / (float)trackFftSize;

    std::vector<std::vector<float>> magTrack((size_t)got,
                                             std::vector<float>((size_t)trackFrames, 0.0f));
    std::vector<std::vector<float>> phaseTrack((size_t)got,
                                               std::vector<float>((size_t)trackFrames, 0.0f));
    // Pre-compute search windows for each partial in the small-FFT bin space.
    struct BinWindow { int center; int lo; int hi; };
    std::vector<BinWindow> binWin((size_t)got);
    for(int j = 0; j < got; ++j)
    {
        const float hz = selected[(size_t)j].hz;
        const int center = std::clamp((int)std::round(hz / trackBinHz), 1, trackFftSize / 2 - 2);
        // Spread search width with frequency to allow for slight inharmonicity.
        const int search = std::max(2, (int)std::round(0.02f * hz / trackBinHz) + 2);
        binWin[(size_t)j] = { center, std::max(1, center - search),
                              std::min(trackFftSize / 2 - 2, center + search) };
    }

    std::vector<float> trackBuf((size_t)trackFftSize * 2, 0.0f);
    for(int f = 0; f < trackFrames; ++f)
    {
        const int start = std::min(f * trackHop, std::max(0, monoLen - trackFftSize));
        std::fill(trackBuf.begin(), trackBuf.end(), 0.0f);
        for(int i = 0; i < trackFftSize; ++i)
            trackBuf[(size_t)i] = mono[(size_t)(start + i)];
        trackWin.multiplyWithWindowingTable(trackBuf.data(), (size_t)trackFftSize);
        trackFft.performRealOnlyForwardTransform(trackBuf.data());

        for(int j = 0; j < got; ++j)
        {
            float bestMag = 0.0f;
            float bestRe = 0.0f, bestIm = 0.0f;
            for(int b = binWin[(size_t)j].lo; b <= binWin[(size_t)j].hi; ++b)
            {
                const float re = trackBuf[(size_t)2 * b];
                const float im = trackBuf[(size_t)2 * b + 1];
                const float mag = std::sqrt(re * re + im * im);
                if(mag > bestMag)
                {
                    bestMag = mag;
                    bestRe = re;
                    bestIm = im;
                }
            }
            magTrack[(size_t)j][(size_t)f] = bestMag;
            phaseTrack[(size_t)j][(size_t)f] = std::atan2(bestIm, bestRe);
        }
    }

    auto estimatePartialAdsr = [&](int j, float &attackSec, float &decaySec,
                                   float &sustainRatio, float &releaseSec) {
        const auto &tr = magTrack[(size_t)j];
        if(tr.empty())
        {
            attackSec = decaySec = releaseSec = tPerTrackFrame;
            sustainRatio = 1.0f;
            return;
        }
        const auto maxIt = std::max_element(tr.begin(), tr.end());
        const float peak = std::max(1e-9f, *maxIt);
        const int peakIdx = (int)std::distance(tr.begin(), maxIt);

        // Attack: time between 10% and 90% of the peak (real-instrument convention).
        int onset = 0;
        while(onset < trackFrames && tr[(size_t)onset] < peak * 0.10f)
            ++onset;
        int attackEnd = onset;
        while(attackEnd < trackFrames && tr[(size_t)attackEnd] < peak * 0.90f)
            ++attackEnd;

        // Sustain ≈ mean amplitude over the middle third of the recording.
        const int midStart = std::clamp(trackFrames / 3, 0, trackFrames - 1);
        const int midEnd = std::clamp((trackFrames * 2) / 3, midStart + 1, trackFrames);
        float midSum = 0.0f;
        for(int f = midStart; f < midEnd; ++f)
            midSum += tr[(size_t)f];
        const float midAvg = midSum / float(std::max(1, midEnd - midStart));
        sustainRatio = clampf(midAvg / peak, 0.02f, 1.25f);

        const float decayTarget = std::max(peak * sustainRatio, peak * 0.20f);
        int decayEnd = peakIdx;
        while(decayEnd < trackFrames && tr[(size_t)decayEnd] > decayTarget)
            ++decayEnd;

        int releaseEnd = std::max(peakIdx, midEnd - 1);
        while(releaseEnd < trackFrames && tr[(size_t)releaseEnd] > peak * 0.08f)
            ++releaseEnd;

        attackSec = std::max(tPerTrackFrame, float(std::max(1, attackEnd - onset)) * tPerTrackFrame);
        decaySec = std::max(tPerTrackFrame, float(std::max(1, decayEnd - peakIdx)) * tPerTrackFrame);
        releaseSec = std::max(tPerTrackFrame,
                              float(std::max(1, releaseEnd - std::max(peakIdx, midEnd - 1)))
                                  * tPerTrackFrame);
    };

    auto estimatePhaseMotion = [&](int j, float partialHz, float &driftHz, float &jitter) {
        const auto &mag = magTrack[(size_t)j];
        const auto &ph = phaseTrack[(size_t)j];
        const float expected = kTwoPi * partialHz * tPerTrackFrame;
        float prev = ph.empty() ? 0.0f : ph[0];
        float weightedSum = 0.0f;
        float weightedAbs = 0.0f;
        float weight = 0.0f;
        for(int f = 1; f < trackFrames; ++f)
        {
            const float delta = wrapPi(ph[(size_t)f] - prev - expected);
            const float w = std::max(1e-9f, 0.5f * (mag[(size_t)f] + mag[(size_t)f - 1]));
            weightedSum += delta * w;
            weightedAbs += std::abs(delta) * w;
            weight += w;
            prev = ph[(size_t)f];
        }
        const float invDur = 1.0f / std::max(1e-6f, tPerTrackFrame);
        driftHz = (weight > 0.0f) ? (weightedSum / weight) * invDur / kTwoPi : 0.0f;
        jitter = (weight > 0.0f) ? (weightedAbs / weight) / kPi : 0.0f;
    };

    std::vector<float> attackSec((size_t)got, 0.0f), decaySec((size_t)got, 0.0f);
    std::vector<float> sustainRatio((size_t)got, 1.0f), releaseSec((size_t)got, 0.0f);
    std::vector<float> driftHz((size_t)got, 0.0f), phaseJitter((size_t)got, 0.0f);
    std::vector<float> partialPeakAmp((size_t)got, 0.0f);
    for(int i = 0; i < got; ++i)
    {
        estimatePartialAdsr(i, attackSec[(size_t)i], decaySec[(size_t)i],
                            sustainRatio[(size_t)i], releaseSec[(size_t)i]);
        estimatePhaseMotion(i, selected[(size_t)i].hz, driftHz[(size_t)i], phaseJitter[(size_t)i]);
        const auto &tr = magTrack[(size_t)i];
        partialPeakAmp[(size_t)i] = tr.empty() ? 0.0f : *std::max_element(tr.begin(), tr.end());
    }
    const float refAttack = std::max(1e-4f, attackSec[0]);
    const float refDecay = std::max(1e-4f, decaySec[0]);
    const float refSustain = std::max(1e-4f, sustainRatio[0]);
    const float refRelease = std::max(1e-4f, releaseSec[0]);

    out.refAttackSec = refAttack;
    out.refDecaySec = refDecay;
    out.refReleaseSec = refRelease;

    float maxAmp = 1e-6f;
    for(int i = 0; i < got; ++i)
    {
        const auto &peak = selected[(size_t)i];
        const float xi = got > 1 ? float(i) / float(got - 1) : 0.0f;
        out.nu[i] = peak.hz / f0;
        // Use the per-partial peak amplitude observed in pass 2 (so partials whose
        // peak occurs only briefly during the transient still get their true level)
        // and fall back to the long-term average from pass 1 if pass 2 was empty.
        out.amp[i] = partialPeakAmp[(size_t)i] > 0.0f ? partialPeakAmp[(size_t)i] : peak.mag;
        out.x[i] = xi;
        out.mu[i] = (xi < 0.34f) ? 0u : ((xi < 0.67f) ? 1u : 2u);
        out.phaseLocked[i] = std::atan2(phaseIm[(size_t)peak.bin], phaseRe[(size_t)peak.bin]);

        out.attackScale[i] = clampf(attackSec[(size_t)i] / refAttack, 0.10f, 4.0f);
        out.decayScale[i] = clampf(decaySec[(size_t)i] / refDecay, 0.10f, 4.0f);
        out.sustainLevel[i] = clampf(sustainRatio[(size_t)i] / refSustain, 0.10f, 2.0f);
        out.releaseScale[i] = clampf(releaseSec[(size_t)i] / refRelease, 0.10f, 4.0f);
        out.phaseDriftHz[i] = clampf(driftHz[(size_t)i], -12.0f, 12.0f);
        out.phaseJitter[i] = clampf(phaseJitter[(size_t)i], 0.0f, 1.0f);
        if(out.amp[i] > maxAmp)
            maxAmp = out.amp[i];
    }

    const float invPeak = 1.0f / maxAmp;
    for(int i = 0; i < got; ++i)
        out.amp[i] *= invPeak;
    zeroTrailing(out, got);
    return true;
}
#endif // legacy extractSamplePartialSet removed in v0.3
} // namespace

// -----------------------------------------------------------------------------
// DirectPartialGenerator implementation: simple base seed.
// -----------------------------------------------------------------------------
void GeneratorBank::runDirect(const DirectPartialParams &p, StaticSpectralFrame &out)
{
    const int N = std::clamp(p.partialCount, 1, kMaxPartials);
    out.partialCount = N;
    out.freqMode = FreqMode::RelativeRatio;

    const float invDen = (N > 1) ? 1.0f / float(N - 1) : 0.0f;
    for(int i = 0; i < N; ++i)
    {
        const float n = float(i + 1);
        const float xi = (N > 1) ? float(i) * invDen : 0.0f;

        out.nu[i] = n;
        out.amp[i] = 1.0f;
        out.x[i] = xi;
        out.mu[i] = (xi < 0.34f) ? 0u : ((xi < 0.67f) ? 1u : 2u);
    }
    zeroTrailing(out, N);
}

// -----------------------------------------------------------------------------
// ModalODEGenerator (precomputed; §1.2.2 path)
// -----------------------------------------------------------------------------
void GeneratorBank::runModal(const ModalODEParams &p, StaticSpectralFrame &out)
{
    const int N = std::clamp(p.partialCount, 1, kMaxPartials);
    out.partialCount = N;
    out.freqMode = FreqMode::RelativeRatio;

    const float invDen = (N > 1) ? 1.0f / float(N - 1) : 0.0f;
    float maxAmp = 1e-6f;

    for(int i = 0; i < N; ++i)
    {
        const float n = float(i + 1);
        const float xi = (N > 1) ? float(i) * invDen : 0.0f;

        float rho = n;
        switch(p.shape)
        {
            case ModalShape::StringClampedClamped: rho = n; break;
            case ModalShape::StringFreeFree: rho = (n - 0.5f); break;
            case ModalShape::BarClampedFree:
            {
                // Bell-ish: f_n ~ (n - 0.5)^2 normalised so first mode = 1.
                const float v = (n - 0.5f);
                rho = (v * v) / (0.5f * 0.5f);
                break;
            }
            case ModalShape::Tube:
                // Odd harmonics only.
                rho = 2.0f * n - 1.0f;
                break;
        }
        // Inharmonicity from "stiffness" B (piano-like).
        const float B = std::max(0.0f, p.stiffness);
        rho *= std::sqrt(1.0f + B * n * n);

        out.nu[i] = rho;
        out.x[i] = xi;
        out.mu[i] = (xi < 0.34f) ? 0u : ((xi < 0.67f) ? 1u : 2u);

        // Pickup-position weight: G_i = sin(pi * n * x_o)  (clamped string fundamental shapes)
        const float pickupW = std::abs(std::sin(kPi * n * p.pickupPosition));
        // 1/n base spectrum + pickup weighting.
        const float ai = pickupW / (n + 0.001f);
        out.amp[i] = ai;
        if(ai > maxAmp)
            maxAmp = ai;

        // Per-partial decay/release: zeta_i = base * (1 + dampingHF * x_i)
        const float zRatio = 1.0f / (1.0f + p.dampingHF * xi);
        out.decayScale[i] = zRatio;
        out.releaseScale[i] = zRatio;
    }

    const float invPeak = 1.0f / maxAmp;
    for(int i = 0; i < N; ++i)
        out.amp[i] *= invPeak;

    zeroTrailing(out, N);
}

// -----------------------------------------------------------------------------
// PDEModalGenerator (precomputed body modal table; §1.3)
// -----------------------------------------------------------------------------
void GeneratorBank::runPDE(const PDEModalParams &p, StaticSpectralFrame &out)
{
    const int N = std::clamp(p.partialCount, 1, kMaxPartials);
    out.partialCount = N;
    out.freqMode = FreqMode::RelativeRatio;
    const float invDen = (N > 1) ? 1.0f / float(N - 1) : 0.0f;

    // Build modal frequency table (rho_i normalized so first partial = 1).
    std::array<float, kMaxPartials> rho {};

    if(p.body == PDEBody::StiffString)
    {
        const float B = std::max(0.0f, p.param1);
        for(int i = 0; i < N; ++i)
        {
            const float n = float(i + 1);
            rho[i] = n * std::sqrt(1.0f + B * n * n);
        }
    }
    else if(p.body == PDEBody::Plate)
    {
        // 2D rectangular plate clamped: f_mn ~ sqrt(m^2 + (Lx/Ly)^2 * n^2)
        const float aspect = std::max(0.1f, p.param2);
        std::vector<float> fs;
        fs.reserve(64);
        for(int m = 1; m <= 12; ++m)
            for(int nn = 1; nn <= 12; ++nn)
                fs.push_back(std::sqrt(float(m * m) + aspect * aspect * float(nn * nn)));
        std::sort(fs.begin(), fs.end());
        const float f1 = fs.front();
        for(int i = 0; i < N; ++i)
            rho[i] = (i < (int)fs.size()) ? fs[(size_t)i] / f1 : (float)(i + 1);
    }
    else if(p.body == PDEBody::Membrane)
    {
        // Approximate Bessel zeros for circular membrane, normalized to first.
        // Pre-tabulated zeros j_{m,n}.
        static const float kZeros[] = {
            2.4048f, 3.8317f, 5.1356f, 5.5201f, 6.3802f, 7.0156f,
            7.5883f, 8.4172f, 8.6537f, 8.7715f, 9.7610f, 9.9361f,
            10.1735f, 10.7415f, 11.0647f, 11.6198f, 11.7915f, 12.2251f,
            12.3386f, 13.0152f, 13.3237f, 13.3543f, 13.5893f, 14.3725f,
            14.4755f, 14.4796f, 14.7960f, 15.5898f, 15.5898f, 15.7002f
        };
        const int K = (int)(sizeof(kZeros) / sizeof(kZeros[0]));
        const float f1 = kZeros[0];
        for(int i = 0; i < N; ++i)
            rho[i] = (i < K) ? kZeros[i] / f1 : (float)(i + 1) * 1.5f;
    }
    else // Bar (free-free): f_n proportional to (2n+1)^2
    {
        for(int i = 0; i < N; ++i)
        {
            const float n = float(i + 1);
            const float v = (2.0f * n - 0.5f);
            rho[i] = v * v / (1.5f * 1.5f);
        }
    }

    float maxAmp = 1e-6f;
    for(int i = 0; i < N; ++i)
    {
        const float n = float(i + 1);
        const float xi = (N > 1) ? float(i) * invDen : 0.0f;

        out.nu[i] = rho[i];
        out.x[i] = xi;
        out.mu[i] = (xi < 0.34f) ? 0u : ((xi < 0.67f) ? 1u : 2u);

        const float pickupW = std::abs(std::sin(kPi * n * p.pickupPosition));
        const float ai = pickupW / (n + 0.001f);
        out.amp[i] = ai;
        if(ai > maxAmp)
            maxAmp = ai;

        const float zRatio = 1.0f / (1.0f + p.dampingHF * xi);
        out.decayScale[i] = zRatio;
        out.releaseScale[i] = zRatio;
    }

    const float invPeak = 1.0f / maxAmp;
    for(int i = 0; i < N; ++i)
        out.amp[i] *= invPeak;

    zeroTrailing(out, N);
}

// SampleAnalysisGenerator was removed in v0.2: it was a one-shot FFT peak-picker
// with no time evolution. SamplePartialSetGenerator below subsumes it and
// captures per-partial amplitude / phase / ADSR over the full sample length.
#if 0
void GeneratorBank::runSample_REMOVED(StaticSpectralFrame &out)
{
    const int N = std::clamp(p.partialCount, 1, kMaxPartials);
    out.partialCount = N;
    out.freqMode = FreqMode::AbsoluteHz;

    juce::AudioFormatManager fmtMgr;
    fmtMgr.registerBasicFormats();

    juce::File file(p.filePath);
    bool ok = false;
    std::vector<float> mono;
    double sr = 48000.0;

    if(file.existsAsFile())
    {
        std::unique_ptr<juce::AudioFormatReader> reader(fmtMgr.createReaderFor(file));
        if(reader != nullptr && reader->lengthInSamples > 0)
        {
            sr = reader->sampleRate;
            const int len = (int)std::min<juce::int64>(reader->lengthInSamples, 1 << 20);
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
            ok = true;
        }
    }

    if(!ok)
    {
        // Fallback: harmonic series so at least we emit something.
        for(int i = 0; i < N; ++i)
        {
            out.nu[i] = p.referenceHz * float(i + 1);
            out.amp[i] = 1.0f / float(i + 1);
            out.x[i] = (N > 1) ? float(i) / float(N - 1) : 0.0f;
            out.mu[i] = 0u;
            out.decayScale[i] = 1.0f;
            out.releaseScale[i] = 1.0f;
        }
        zeroTrailing(out, N);
        return;
    }

    // Window and FFT a chunk near the middle.
    constexpr int fftOrder = 14;             // 16384
    constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft(fftOrder);

    std::vector<float> fftBuf((size_t)fftSize * 2, 0.0f);
    const int start = std::max(0, (int)mono.size() / 2 - fftSize / 2);
    const int copy = std::min(fftSize, (int)mono.size() - start);
    juce::dsp::WindowingFunction<float> win((size_t)copy, juce::dsp::WindowingFunction<float>::hann);
    for(int i = 0; i < copy; ++i)
        fftBuf[(size_t)i] = mono[(size_t)(start + i)];
    win.multiplyWithWindowingTable(fftBuf.data(), (size_t)copy);
    fft.performFrequencyOnlyForwardTransform(fftBuf.data());

    // Peak pick: find local maxima in magnitude bins, sort by magnitude.
    struct Peak { float hz; float mag; };
    std::vector<Peak> peaks;
    peaks.reserve(fftSize / 4);
    const float binHz = (float)sr / (float)fftSize;
    const int loBin = std::max(1, (int)(p.minHz / binHz));
    const int hiBin = std::min(fftSize / 2 - 1, (int)(p.maxHz / binHz));

    for(int b = loBin + 1; b < hiBin - 1; ++b)
    {
        const float m = fftBuf[(size_t)b];
        if(m > fftBuf[(size_t)(b - 1)] && m > fftBuf[(size_t)(b + 1)] && m > 1e-4f)
        {
            // Parabolic interpolation for sub-bin frequency.
            const float a = fftBuf[(size_t)(b - 1)];
            const float c = fftBuf[(size_t)(b + 1)];
            const float denom = (a - 2.0f * m + c);
            const float delta = std::abs(denom) > 1e-9f ? 0.5f * (a - c) / denom : 0.0f;
            peaks.push_back({(float(b) + delta) * binHz, m});
        }
    }
    std::sort(peaks.begin(), peaks.end(), [](const Peak &x, const Peak &y) { return x.mag > y.mag; });
    if((int)peaks.size() > N)
        peaks.resize((size_t)N);
    std::sort(peaks.begin(), peaks.end(), [](const Peak &x, const Peak &y) { return x.hz < y.hz; });

    const int got = (int)peaks.size();
    float maxAmp = 1e-6f;
    for(int i = 0; i < N; ++i)
    {
        if(i < got)
        {
            out.nu[i] = peaks[(size_t)i].hz;
            out.amp[i] = peaks[(size_t)i].mag;
        }
        else
        {
            out.nu[i] = (got > 0 ? peaks.back().hz : p.referenceHz) * float(i + 1);
            out.amp[i] = 0.0f;
        }
        if(out.amp[i] > maxAmp)
            maxAmp = out.amp[i];

        const float xi = (N > 1) ? float(i) / float(N - 1) : 0.0f;
        out.x[i] = xi;
        out.mu[i] = (xi < 0.34f) ? 0u : ((xi < 0.67f) ? 1u : 2u);
        out.decayScale[i] = 1.0f;
        out.releaseScale[i] = 1.0f;
    }
    const float invPeak = 1.0f / maxAmp;
    for(int i = 0; i < N; ++i)
        out.amp[i] *= invPeak;

    zeroTrailing(out, N);
}
#endif // SampleAnalysisGenerator removed.

// -----------------------------------------------------------------------------
// FunctionalSampleSource generator wrapper (architecture md §1.1.2).
//
// All heavy analysis lives in FunctionalSampleSource.cpp. This method only
// owns the cache: if the input file / partial count / quality / root override
// match the last analyzed source, we skip analysis and just re-bake. That
// makes macro knobs (attackSharpness / brightnessDecay / bodyResonance) snappy
// because they only trigger reconstruction, not a multi-pass STFT + linker.
// -----------------------------------------------------------------------------
void GeneratorBank::runFunctionalSampleSource(const FunctionalSampleSourceParams &p,
                                              StaticSpectralFrame &out) const
{
    out.partialCount = std::clamp(p.partialCount, 1, kMaxPartials);
    out.freqMode = FreqMode::RelativeRatio;

    const bool cacheHit = cachedSource_.valid
                          && cachedFilePath_ == p.filePath
                          && cachedPartialCount_ == p.partialCount
                          && cachedQuality_ == p.quality
                          && cachedUserLockRoot_ == p.userLockRoot
                          && std::abs(cachedUserRootHz_ - p.userRootHz) < 0.01f;
    if(!cacheHit)
    {
        cachedSource_ = functional::analyzeSource(p);
        cachedFilePath_ = p.filePath;
        cachedPartialCount_ = p.partialCount;
        cachedQuality_ = p.quality;
        cachedUserLockRoot_ = p.userLockRoot;
        cachedUserRootHz_ = p.userRootHz;
    }
    if(cachedSource_.valid)
    {
        cachedSource_.transientAmount = std::clamp(p.transientAmount, 0.0f, 1.0f);
        cachedSource_.residualAmount = std::clamp(p.residualAmount, 0.0f, 1.0f);
        cachedSource_.transientEnabled = cachedSource_.transientAmount > 1e-4f;
        cachedSource_.residualEnabled = cachedSource_.residualAmount > 1e-4f;
    }

    if(cachedSource_.valid)
    {
        functional::bakeToStaticFrame(cachedSource_, p, out);
    }
    else
    {
        DirectPartialParams fallback;
        fallback.partialCount = out.partialCount;
        runDirect(fallback, out);
        out.phaseInitMode = PhaseInitMode::Zero;
        out.phaseSeed = p.phaseSeed;
    }
}

void GeneratorBank::bakeFunctionalSourceToTimeline(const FunctionalSpectralSource &src,
                                                   const FunctionalSampleSourceParams &p,
                                                   SpectralTimeline &out)
{
    functional::bakeToTimeline(src, p, out);
}

void GeneratorBank::bakeFunctionalSourceToStaticFrame(const FunctionalSpectralSource &src,
                                                      const FunctionalSampleSourceParams &p,
                                                      StaticSpectralFrame &out)
{
    functional::bakeToStaticFrame(src, p, out);
}

// buildTimelineFromStaticDynamics has been retired together with the v0.2
// ADSR-fit pipeline. The FunctionalSampleSource path now bakes timelines
// directly from FunctionalSpectralSource via functional::bakeToTimeline().

// -----------------------------------------------------------------------------
// Generator Preprocessor: shared post-generator offset/shaping layer (§1A).
// -----------------------------------------------------------------------------
void GeneratorBank::applyPreprocessor(const SpectralPreprocessParams &p,
                                      StaticSpectralFrame &out,
                                      bool preserveExtractedTimbre)
{
    const int N = std::clamp(out.partialCount, 1, kMaxPartials);
    out.partialCount = N;

    auto applyFrequencyShape = [&]() {
        for(int i = 0; i < N; ++i)
        {
            const float n = float(i + 1);
            const float shapedNu = computeNuDirect(p.freqShape, n, p.inharmonicAmount);
            const float freqMul = shapedNu / std::max(1e-6f, n);
            out.nu[i] *= freqMul;
        }
    };

    if(preserveExtractedTimbre)
    {
        applyFrequencyShape();
        zeroTrailing(out, N);
        return;
    }

    std::mt19937 rng(p.phaseSeed ^ 0xA5A5A5A5u);
    std::uniform_real_distribution<float> uni(-1.0f, 1.0f);

    const float widthSafe = std::max(1e-3f, p.focusWidth);
    const float invSigSq2 = 1.0f / (2.0f * widthSafe * widthSafe);

    float maxAmp = 1e-6f;
    for(int i = 0; i < N; ++i)
    {
        const float n = float(i + 1);
        const float xi = out.x[i];

        // Frequency Shape Function acts as an offset around the generator's base nu.
        // Direct seed has nu=n, so this becomes the full frequency shape. Modal/PDE/Sample
        // keep their base structure and receive a multiplicative spectral offset.
        const float shapedNu = computeNuDirect(p.freqShape, n, p.inharmonicAmount);
        const float freqMul = shapedNu / std::max(1e-6f, n);
        out.nu[i] *= freqMul;

        const float dx = xi - p.focusCenter;
        const float gauss = std::exp(-(dx * dx) * invSigSq2);
        const float warp = p.warpAmount * warpEval(p.warpMode, xi, p.warpP, p.warpCenter);
        const float decayTerm = -p.lambda * std::pow(std::max(1e-6f, xi), std::max(0.05f, p.gamma));
        const float eps = uni(rng);

        const float Li = -p.tilt * std::log(n)
                         + warp
                         + decayTerm
                         + p.focusAmount * gauss
                         + p.ampRandom * eps;
        out.amp[i] *= std::exp(Li);
        if(out.amp[i] > maxAmp)
            maxAmp = out.amp[i];
    }

    const float invPeak = 1.0f / maxAmp;
    for(int i = 0; i < N; ++i)
        out.amp[i] *= invPeak;

    applyPerPartialAdsrSpread(out, N, p.decaySpread, p.releaseSpread);
    initPhaseTable(out, p.phaseInitMode, p.phaseSeed, N);
    zeroTrailing(out, N);
}

// -----------------------------------------------------------------------------
// Dispatcher
// -----------------------------------------------------------------------------
void GeneratorBank::generate(const SourceGenParams &p, StaticSpectralFrame &out) const
{
    initFrameDefaults(out);
    switch(p.type)
    {
        case GeneratorType::DirectPartial:          runDirect(p.direct, out); break;
        case GeneratorType::ModalODE:               runModal(p.modal, out); break;
        case GeneratorType::PDEModal:               runPDE(p.pde, out); break;
        case GeneratorType::FunctionalSampleSource: runFunctionalSampleSource(p.functionalSource, out); break;
        case GeneratorType::SamplePlayback:
            out.partialCount = 1;
            out.freqMode = FreqMode::RelativeRatio;
            out.nu[0] = 1.0f;
            out.amp[0] = 0.0f;
            break;
    }
    applyPreprocessor(p.pre, out,
                      p.type == GeneratorType::FunctionalSampleSource);
}

void GeneratorBank::generateTimeline(const SourceGenParams &p, SpectralTimeline &out) const
{
    if(p.type == GeneratorType::FunctionalSampleSource)
    {
        // Make sure the analysis cache is up to date (this also fills a static
        // frame view for the spectrum UI). We discard that frame here because
        // the timeline pipeline reconstructs every frame from the source.
        StaticSpectralFrame scratch;
        initFrameDefaults(scratch);
        runFunctionalSampleSource(p.functionalSource, scratch);
        if(cachedSource_.valid)
        {
            bakeFunctionalSourceToTimeline(cachedSource_, p.functionalSource, out);
            return;
        }
        // Fallback to a single-frame timeline of the static scratch (Direct
        // partial fallback was already written into `scratch` by runFSS).
        applyPreprocessor(p.pre, scratch, false);
        initTimelineDefaults(out);
        out.frameCount = 1;
        out.durationSeconds = 0.0f;
        out.loop = false;
        out.timeSeconds[0] = 0.0f;
        out.frames[0] = scratch;
        return;
    }

    if(p.type == GeneratorType::SamplePlayback)
    {
        StaticSpectralFrame silent;
        initFrameDefaults(silent);
        silent.partialCount = 1;
        silent.freqMode = FreqMode::RelativeRatio;
        silent.nu[0] = 1.0f;
        silent.amp[0] = 0.0f;
        out = makeStaticTimeline(silent);
        return;
    }

    // Non-sample generators: single-frame timeline (NOTE: avoid local
    // SpectralTimeline values; ~460 kB each).
    StaticSpectralFrame frame;
    generate(p, frame);
    initTimelineDefaults(out);
    out.frameCount = 1;
    out.durationSeconds = 0.0f;
    out.loop = false;
    out.timeSeconds[0] = 0.0f;
    out.frames[0] = frame;
}

} // namespace synth
