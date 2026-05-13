#pragma once

#include "SpectralFrame.h"
#include "SamplePlaybackEngine.h"

#include <string>

namespace synth
{

// =============================================================================
// Generator types per architecture §1
// =============================================================================
// NOTE: GeneratorType::FunctionalSampleSource = 3 used to be called
// SamplePartialSet. We renamed it once the sample analyzer evolved from a
// per-frame peak picker into a full sinusoidal-track + functional compression
// pipeline (see §1.1.2 in the architecture md). The numeric value stays at 3
// so existing presets keep loading; the JSON key is also accepted under both
// the old and new names.
enum class GeneratorType : uint8_t
{
    DirectPartial = 0,
    ModalODE = 1,
    PDEModal = 2,
    FunctionalSampleSource = 3,
    SamplePlayback = 4
};

// -----------------------------------------------------------------------------
// 1A Generator Preprocessor: shared post-generator spectral shaping.
// Applied after Direct / Modal / PDE / FunctionalSampleSource base partials.
// -----------------------------------------------------------------------------
enum class FreqShape : uint8_t
{
    Harmonic = 0,     // rho_i = n_i^alpha
    Linear = 1,       // rho_i = 1 + k * n_i
    Exponential = 2   // rho_i = exp(k * n_i)
};

enum class WarpMode : uint8_t
{
    Tilt = 0, // Phi_tilt(x;p_t)
    Sym = 1,  // Phi_sym(x;c_s,p_s)
    Skew = 2  // Phi_skew(x;p_k)
};

struct SpectralPreprocessParams
{
    FreqShape freqShape = FreqShape::Harmonic;
    float inharmonicAmount = 0.0f;   // 0..1: maps to multiplicative nu offset
    // log-amp post-shape: amp *= exp(L_i), then peak-normalize.
    float tilt = 1.0f;            // spectrum tilt (default 1.0 reproduces 1/n)
    float lambda = 0.0f;          // additional decay slope on x (-2..+2)
    float gamma = 1.0f;           // decay curve exponent (0.25..4)
    WarpMode warpMode = WarpMode::Tilt;
    float warpAmount = 0.0f;      // a_w  (-1..+1)
    float warpP = 1.0f;           // p_t / p_s / p_k  (0.5..6)
    float warpCenter = 0.5f;      // c_s for Sym
    float focusCenter = 0.5f;     // c_f
    float focusWidth = 0.2f;      // sigma_f
    float focusAmount = 0.0f;     // a_f
    float ampRandom = 0.0f;       // a_r in 0..1

    // Per-partial ADSR scaling preset (used by Voice).
    float decaySpread = 0.0f;     // 0..1: high partials decay faster
    float releaseSpread = 0.0f;   // 0..1: high partials release faster

    PhaseInitMode phaseInitMode = PhaseInitMode::Zero;
    uint32_t phaseSeed = 1u;
};

// -----------------------------------------------------------------------------
// 1.1 DirectPartialGenerator: emits a simple default partial seed.
// Frequency / amplitude / shape / phase are applied by SpectralPreprocessParams.
// -----------------------------------------------------------------------------
struct DirectPartialParams
{
    int partialCount = 32;
};

// -----------------------------------------------------------------------------
// 1.2 ModalODEGenerator (precomputed modal frequency table per §1.2.2)
// -----------------------------------------------------------------------------
enum class ModalShape : uint8_t
{
    StringClampedClamped = 0, // f_n = n * f0
    StringFreeFree = 1,       // f_n = (n+0.5) * f0
    BarClampedFree = 2,       // f_n proportional to (n - 0.5)^2 (bell-like)
    Tube = 3                  // n odd-only series
};

struct ModalODEParams
{
    int partialCount = 32;
    ModalShape shape = ModalShape::StringClampedClamped;
    float stiffness = 0.0f;       // B inharmonicity coefficient (piano-like)
    float dampingZeta = 0.001f;   // zeta_i base damping
    float dampingHF = 0.5f;       // extra damping on high partials (0..2)
    float pickupPosition = 0.25f; // x_o in [0,1] - influences amplitude weights
    PhaseInitMode phaseInitMode = PhaseInitMode::Random;
    uint32_t phaseSeed = 1u;
};

// -----------------------------------------------------------------------------
// 1.3 PDEModalGenerator (precomputed modal table per §1.3)
// -----------------------------------------------------------------------------
enum class PDEBody : uint8_t
{
    StiffString = 0, // f_n = n*f0*sqrt(1 + B*n^2)
    Plate = 1,       // 2D modal grid
    Membrane = 2,    // Bessel zeros approximation
    Bar = 3          // (2n-1)^2 series
};

struct PDEModalParams
{
    int partialCount = 32;
    PDEBody body = PDEBody::StiffString;
    float param1 = 0.0001f;  // body-specific (B for stiff string, aspect for plate)
    float param2 = 1.0f;     // body-specific (Lx/Ly for plate)
    float dampingZeta = 0.002f;
    float dampingHF = 0.7f;
    float pickupPosition = 0.3f;
    PhaseInitMode phaseInitMode = PhaseInitMode::Random;
    uint32_t phaseSeed = 1u;
};

// =============================================================================
// FunctionalSampleSource (architecture §1.1.2)
//
// Replaces the v0.1 SamplePartialSetGenerator. The architectural intent is:
//
//     audio sample
//        -> Preprocess -> Root estimation (with user override)
//        -> Multi-resolution STFT
//        -> Peak candidate extraction
//        -> Partial track linking (greedy + cost function)
//        -> Track cleaning / classification
//        -> Functional compression:
//               log A_i(t)   ~= base_i + sum_m W_m^A(x_i) * T_m^A(t)
//               log rho_i(t) ~= base_i + sum_m W_m^F(x_i) * T_m^F(t)
//        -> FunctionalSpectralSource (compact, persistable)
//        -> Adaptive frame baking -> SpectralTimeline
//
// What lives in this header is the *persistable / runtime-edited* model
// (FunctionalSpectralSource). The dense per-frame analysis tracks are
// intermediate-only and live inside Generators.cpp; they never enter a preset.
// Transient / residual layers are stubbed (data + flags) and will be wired in
// follow-up versions.
// =============================================================================

// Quality preset: trades analysis cost vs partial count and basis density.
enum class FunctionalSampleQuality : uint8_t
{
    Draft = 0,    // 32 partials, 3 amp basis, big hop -- fast preview
    Standard = 1, // 64 partials, 4 amp basis, ~5 ms hop -- default
    High = 2      // 128 partials, 6 amp basis, ~3 ms hop -- editing / mastering
};

// Interpretable time-basis kinds shared across partials. These map onto a
// human-readable behavior (attack burst, slow body, brightness decay, etc.)
// so Functional Operators and macro knobs can target them by meaning. Learned
// PCA residual basis is a v2 extension and not enumerated here.
enum class TimeBasisKind : uint8_t
{
    GlobalEnvelope = 0,  // exp(-t / tau)             slow global decay
    AttackBurst = 1,     // gaussian centered near 0  early energy spike
    FastDecay = 2,       // exp(-t / tauFast)         quick die-off
    SlowDecay = 3,       // exp(-t / tauSlow)         tail/sustain
    BrightnessDecay = 4, // exp(-t / tauBright)       used by high-x partials
    BodyResonance = 5,   // exp(-t / tauBody)         long ringing tail
    PitchRelax = 6,      // exp(-t / tauPitch)        attack-to-body freq drift
    VibratoSine = 7      // sin(2*pi*rate*t)*decay    residual periodic drift
};

constexpr int kMaxFunctionalBasis = 8;
// Cap on intermediate sinusoidal tracks the linker may produce before the
// FunctionalCompressor reduces them to <= kMaxPartials.
constexpr int kMaxAnalysisTracks = 384;

// One interpretable time basis function T_m(t). All current kinds are
// monotonic exponentials / gaussian, so a single tau / center / sigma /
// power encodes them. Tremolo / Vibrato would add rateHz; left for v2.
struct TimeBasis
{
    TimeBasisKind kind = TimeBasisKind::GlobalEnvelope;
    float tau = 0.2f;     // seconds, used by all *Decay kinds and GlobalEnvelope
    float center = 0.0f;  // seconds, gaussian center for AttackBurst
    float sigma = 0.005f; // seconds, gaussian width
    float power = 1.0f;   // optional shape exponent (unused for now)
    float rateHz = 5.0f;  // Hz, used by VibratoSine
};

// Per-partial static summary inside the compressed source.
struct PartialStatic
{
    float baseRatio = 1.0f;   // rho_i  (median over track)
    float baseLogAmp = -8.0f; // log A_i base level
    float phaseInit = 0.0f;
    float birthTime = 0.0f; // seconds from analysis t=0
    float deathTime = 1.0f;
    float xPos = 0.0f;        // normalized log-ratio in [0,1]
    float confidence = 1.0f;  // track quality from cleaner
    float harmonicity = 1.0f; // |ratio - nearest integer|
    uint32_t groupFlags = 0u;
    uint8_t harmonicIndex = 0;
    uint8_t partialClass = 0; // PartialClass enum coded as uint8 for compactness
};

// PartialClass coded as a plain enum so it can travel as uint8 in PartialStatic.
enum class PartialClass : uint8_t
{
    HarmonicStable = 0,
    HarmonicTransient = 1,
    InharmonicStable = 2,
    InharmonicTransient = 3,
    NoiseLike = 4,
    Uncertain = 5
};

// Persistable compressed source. ~22 KB; safe to put inside SourceGenParams.
struct FunctionalSpectralSource
{
    int partialCount = 0;
    float durationSeconds = 0.0f;
    float rootHz = 261.6256f;
    int rootMidi = 60;
    float analysisQuality = 0.0f; // 0..1, mean confidence over kept tracks
    float loudnessNormGain = 1.0f; // bake-time quality equalizer from analysis energy

    std::array<PartialStatic, kMaxPartials> partials {};

    // Shared interpretable time basis bank for amplitude.
    int ampBasisCount = 0;
    std::array<TimeBasis, kMaxFunctionalBasis> ampBasis {};
    // Per-partial weights W_m^A(x_i). Stored as discrete samples; downstream
    // we just look up W[i][m] per partial (no x-domain spline fit in v1).
    std::array<std::array<float, kMaxFunctionalBasis>, kMaxPartials> ampWeights {};

    // Frequency basis layer. V2 now fits residual log-ratio motion so pitch
    // relaxation and light periodic drift survive the compression step.
    int freqBasisCount = 0;
    std::array<TimeBasis, kMaxFunctionalBasis> freqBasis {};
    std::array<std::array<float, kMaxFunctionalBasis>, kMaxPartials> freqWeights {};

    // Residual / Transient layers. The compressed source keeps the routing
    // flags while bake/Voice map them into attack emphasis and noisy phase drift.
    bool transientEnabled = false;
    bool residualEnabled = false;
    float transientAmount = 0.0f;
    float residualAmount = 0.0f;

    bool valid = false;
};

// -----------------------------------------------------------------------------
// FunctionalSampleSourceParams -- per-instance configuration of the analyzer
// and the macro/bake controls. Macro knobs (attack/brightness/body) scale
// basis weights at *bake time*; they do not re-run the analyzer.
// -----------------------------------------------------------------------------
struct FunctionalSampleSourceParams
{
    int partialCount = 64;
    std::string filePath;

    // Root estimation. Soft guard range is broad on purpose; user can also
    // hard-lock the root via userLockRoot / userRootHz to bypass the auto pick.
    float minRootHz = 50.0f;
    float maxRootHz = 2000.0f;
    bool userLockRoot = false;
    float userRootHz = 261.6256f;

    // Macro controls. These multiply basis weights when baking and are not
    // baked into the FunctionalSpectralSource itself, so the same compressed
    // source can be re-rendered with different macros.
    float attackSharpness = 1.0f; // 0..2 -> scales AttackBurst weights
    float brightnessDecay = 1.0f; // 0..2 -> scales FastDecay / BrightnessDecay
    float bodyResonance = 1.0f;   // 0..2 -> scales SlowDecay / BodyResonance

    FunctionalSampleQuality quality = FunctionalSampleQuality::Standard;

    // Track linker tuning.
    float maxFreqJumpCents = 80.0f;
    float maxAmpJumpDb = 24.0f;
    int maxGapFrames = 3;

    // v2 placeholders.
    float transientAmount = 0.0f;
    float residualAmount = 0.0f;

    PhaseInitMode phaseInitMode = PhaseInitMode::Locked;
    uint32_t phaseSeed = 1u;
};

// -----------------------------------------------------------------------------
// Unison: per-voice detuning / stereo spread layer applied at Voice render time.
// Up to kMaxUnison sub-voices per Voice. Detune is symmetric around the note's
// f0, stereo pan is distributed across the field by widthStereo.
// -----------------------------------------------------------------------------
struct UnisonParams
{
    int voices = 1;            // U in [1, kMaxUnison]
    float detuneCents = 12.0f; // total spread between outermost sub-voices, in cents
    float widthStereo = 0.7f;  // 0 = mono, 1 = hard L/R panning at extremes
    float phaseSpread = 1.0f;  // 0..1 randomize sub-voice initial phase
    uint32_t phaseSeed = 17u;  // deterministic seed for unison phase distribution
};

// =============================================================================
// Unified parameter bundle: Source generator selection
// =============================================================================
struct SourceGenParams
{
    GeneratorType type = GeneratorType::DirectPartial;
    SpectralPreprocessParams pre;
    DirectPartialParams direct;
    ModalODEParams modal;
    PDEModalParams pde;
    FunctionalSampleSourceParams functionalSource;
    SamplePlaybackParams samplePlayback;
    UnisonParams unison;
    // Reference fundamental frequency used by the UI to compute the maximum
    // usable partial count (floor(20000 / partialMaxRefHz)). Defaults to A2.
    float partialMaxRefHz = 110.0f;
};

// Free analyze/bake API lives in the synth::functional namespace so the
// FunctionalSpectralSource pipeline (preprocess, root, STFT, peak detection,
// track linking, cleaner, compressor, baker) can live in its own translation
// unit and stay out of the existing Generators.cpp. See FunctionalSampleSource.cpp.
namespace functional
{
FunctionalSpectralSource analyzeSource(const FunctionalSampleSourceParams &p);
void bakeToTimeline(const FunctionalSpectralSource &src,
                    const FunctionalSampleSourceParams &p,
                    SpectralTimeline &out);
void bakeToStaticFrame(const FunctionalSpectralSource &src,
                       const FunctionalSampleSourceParams &p,
                       StaticSpectralFrame &out);
} // namespace functional

class GeneratorBank
{
  public:
    void generate(const SourceGenParams &p, StaticSpectralFrame &out) const;
    void generateTimeline(const SourceGenParams &p, SpectralTimeline &out) const;

    // Expose the last analyzed FunctionalSpectralSource for the UI / preset
    // layer. Returns a pointer because analysis may fail or never have run.
    const FunctionalSpectralSource *getCachedFunctionalSource() const { return cachedSource_.valid ? &cachedSource_ : nullptr; }

  private:
    static void runDirect(const DirectPartialParams &p, StaticSpectralFrame &out);
    static void runModal(const ModalODEParams &p, StaticSpectralFrame &out);
    static void runPDE(const PDEModalParams &p, StaticSpectralFrame &out);
    void runFunctionalSampleSource(const FunctionalSampleSourceParams &p, StaticSpectralFrame &out) const;
    static void bakeFunctionalSourceToTimeline(const FunctionalSpectralSource &src,
                                               const FunctionalSampleSourceParams &p,
                                               SpectralTimeline &out);
    static void bakeFunctionalSourceToStaticFrame(const FunctionalSpectralSource &src,
                                                  const FunctionalSampleSourceParams &p,
                                                  StaticSpectralFrame &out);
    static void applyPreprocessor(const SpectralPreprocessParams &p, StaticSpectralFrame &out, bool preserveExtractedTimbre);

    // Cache of the last analyzed source. Re-analysis is expensive (multi-pass
    // STFT + track linking) so we only re-run when filePath, partialCount,
    // quality, or root override changes. Macro knobs only trigger a re-bake.
    mutable FunctionalSpectralSource cachedSource_ {};
    mutable std::string cachedFilePath_;
    mutable int cachedPartialCount_ = -1;
    mutable FunctionalSampleQuality cachedQuality_ = FunctionalSampleQuality::Standard;
    mutable bool cachedUserLockRoot_ = false;
    mutable float cachedUserRootHz_ = 0.0f;
};

} // namespace synth
