#pragma once

#include "engine/MatrixEngine.h"
#include "dsp/InsertEffects.h"
#include "dsp/RouteGraph.h"
#include "dsp/SpectralFrame.h"

#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <string>
#include <vector>

namespace synth
{

constexpr int kMaxPerVoiceFilters = 4;
constexpr int kMaxStripInserts = 8;

enum class FreqShape : uint8_t
{
    Harmonic = 0,
    Linear = 1,
    Exponential = 2
};

enum class RenderQualityMode : uint8_t
{
    Draft = 0,
    Normal = 1,
    High = 2,
    Render = 3
};

struct UnisonParams
{
    int voices = 1;
    float detuneCents = 12.0f;
    float widthStereo = 0.7f;
    float phaseSpread = 1.0f;
    uint32_t phaseSeed = 17u;
};

constexpr int kMaxWavetablePartials = 64; // per-source-track partial slot count (per-bank max)
// Size of the shared cross-track partial pool the engine renders into. Decoupled
// from the per-bank max so many tracks can each carry their partials without
// starving each other; bounded by the spectral-frame ceiling (kMaxPartials).
constexpr int kMaxRenderPartials = kMaxPartials;
constexpr int kEditableMetaPartials = 8;
constexpr int kWavetableSize = 2048;
constexpr int kMaxWavetableFrames = 512;
constexpr int kDefaultWavetableFrames = 16;
constexpr int kVisibleWavetableFrames = 16;
constexpr int kMaxWavetableHarmonics = 1024;
constexpr int kEditableWavetableHarmonics = 256;
constexpr int kWavetableMipLevels = 11;

enum class WavetableWarpMode : uint8_t
{
    None = 0,
    Bend,
    Squeeze,
    Skew
};

enum class WavetableMorphMode : uint8_t
{
    Linear = 0,
    Spectral = 1
};

enum class WavetableImportMode : uint8_t
{
    AutoDetect = 0,
    FixedFrames,
    SingleCycle,
    ConstantPitch,
    ManualCycleLength
};

struct WavetableImportOptions
{
    WavetableImportMode mode = WavetableImportMode::AutoDetect;
    int frameLength = kWavetableSize;
    int manualCycleLength = kWavetableSize;
    int maxFrames = 128;
};

struct WavetableImportResult
{
    bool success = false;
    int sourceFrames = 0;
    int importedFrames = 0;
    int cycleLength = 0;
    float estimatedPitchHz = 0.0f;
    float confidence = 0.0f;
    std::string message;
};

enum class SourceFilterTopology : uint8_t
{
    Bypass = 0,
    OnePoleLowPass = 1,
    TwoPoleStateVariable = 2,
    FourPoleCascade = 3,
    FeedbackLadder = 4
};

struct SourceFilterParams
{
    bool enabled = false;
    SourceFilterTopology topology = SourceFilterTopology::Bypass;
    float cutoffHz = 12000.0f;
    float resonance = 0.0f;
    float drive = 1.0f;
    float feedback = 0.0f;
    float mix = 1.0f;
};

struct GeneratorSourceParams
{
    float gain = 1.0f;
    float pan = 0.0f;
    SourceFilterParams filter {};
};

enum class SourceTrackType : uint8_t
{
    PartialBank = 0,
    MetaOscillator = 1,
    BasicOscillator = 2,
    SampleNoise = 3
};

enum class SourceTrackOutputMode : uint8_t
{
    Audio = 0,
    ModOnly = 1,
    AudioAndMod = 2
};

enum class BasicOscillatorShape : uint8_t
{
    Sine = 0,
    Triangle = 1,
    Saw = 2,
    Pulse = 3,
    Sub = 4
};

enum class SampleNoiseMode : uint8_t
{
    Noise = 0,
    File = 1,
    Capture = 2
};

// A Basic Oscillator track hosts a small stack of independent oscillators rather
// than one: on its own a basic shape is a handful of controls, and three of them
// side by side is what the shape is actually useful for.
constexpr int kBasicOscUnits = 3;

struct BasicOscUnit
{
    bool enabled = false;
    BasicOscillatorShape shape = BasicOscillatorShape::Sine;
    float pulseWidth = 0.5f;   // Pulse only
    float subLevel = 0.0f;     // Sub only
    float level = 1.0f;
    int pitchOct = 0;
    int pitchSem = 0;
    float pitchFin = 0.0f;
    float pitchCrs = 0.0f;
};

// Modulation BETWEEN the units of one Basic Oscillator rack. This is local to
// the source — it is not a matrix route — but its depth is a matrix destination,
// so an LFO can still sweep it.
enum class BasicOscModMode : uint8_t
{
    Off = 0,
    Ring = 1,   // carrier * modulator
    AM = 2,     // carrier * (1 + modulator)
    Sync = 3,   // modulator's upward zero crossings reset the carrier's phases
    FM = 4,     // modulator integrated into the carrier's phase
    PM = 5      // modulator added to the carrier's phase
};
constexpr int kBasicOscModModes = 6;

struct BasicOscModParams
{
    BasicOscModMode mode = BasicOscModMode::Off;
    uint8_t source = 1;   // modulator unit index
    uint8_t target = 0;   // carrier unit index
    float depth = 0.0f;
};

// One unit's harmonic series, written into caller-provided arrays (at most
// `budget` entries). Shared by the seed builder and the UI's waveform display so
// the picture cannot drift from what is rendered. Returns how many it wrote.
int basicOscPartials(const BasicOscUnit &unit, int budget,
                     float *ratio, float *amp, float *phase);

struct WavetableHarmonic
{
    float ratio = 1.0f;
    float amp = 0.0f;
    float phase = 0.0f;
};

struct WavetableFrame
{
    using Spectrum = std::array<std::complex<float>, kWavetableSize / 2 + 1>;

    bool useImportedWaveform = false;
    std::shared_ptr<std::array<float, kWavetableSize>> waveform;
    std::shared_ptr<Spectrum> spectrum;
    std::array<WavetableHarmonic, kMaxWavetableHarmonics> harmonics {};
};

using WavetableFrameArray = std::array<std::shared_ptr<WavetableFrame>, kMaxWavetableFrames>;

struct WavetableFrameStorage
{
    std::shared_ptr<WavetableFrameArray> data;

    WavetableFrameStorage() = default;
    WavetableFrameStorage(const WavetableFrameStorage &other);
    WavetableFrameStorage &operator=(const WavetableFrameStorage &other);

    WavetableFrameArray &ensure();
    const WavetableFrameArray &get() const;
    WavetableFrame &operator[](size_t index);
    const WavetableFrame &operator[](size_t index) const;
};

struct WavetablePartialSlot
{
    bool enabled = true;
    float ratio = 1.0f;   // 合成引擎使用的频率乘数，由oct/sem/fin/crs合成
    float amp = 1.0f;
    float phase = 0.0f;
    // Serum-style per-note phase randomization ratio [0,1]. 0 = start phase locked to
    // `phase`; 1 = fully random start phase each note-on (all unison voices share the
    // note's draw). Only meta oscillators use it.
    float phaseRandom = 0.0f;
    float pan = 0.0f;
    int frameCount = 1;
    float morph = 0.0f;
    WavetableWarpMode warpMode = WavetableWarpMode::None;
    float warpAmount = 0.0f;
    WavetableFrameStorage frames {};

    // Serum 风格 pitch 控件分量（UI 编辑，合并后写入 ratio）
    int   pitchOct = 0;          // 整数八度 -4 ~ +4
    int   pitchSem = 0;          // 整数半音 -12 ~ +12
    float pitchFin = 0.0f;       // 音分，整数显示 -100 ~ +100
    float pitchCrs = 0.0f;       // 音分，小数显示 -100 ~ +100

    // 从四个分量重新计算 ratio
    void syncRatioFromPitch()
    {
        const float totalSemis = float(pitchOct) * 12.0f
                               + float(pitchSem)
                               + pitchFin  / 100.0f
                               + pitchCrs  / 100.0f;
        ratio = std::pow(2.0f, totalSemis / 12.0f);
    }
};

struct WavetableSeedParams
{
    int partialCount = 1;
    FreqShape freqShape = FreqShape::Harmonic;
    float inharmonicAmount = 0.0f;
    // PartialBank frame table: each frame maps harmonics[0..63] to the 64 additive
    // partials' amp/phase. This intentionally reuses the Meta wavetable harmonic
    // file format; only the first 64 harmonics are used by PartialBank.
    int frameCount = 1;
    float morph = 0.0f;
    WavetableFrameStorage frames {};
    std::array<WavetablePartialSlot, kMaxWavetablePartials> partials {};

    WavetableSeedParams();
};

struct SourceTrackParams
{
    uint32_t id = 1;
    std::string name = "Partial Bank";
    SourceTrackType type = SourceTrackType::PartialBank;
    SourceTrackOutputMode outputMode = SourceTrackOutputMode::Audio;
    bool mute = false;
    bool solo = false;
    float gain = 1.0f;
    float pan = 0.0f;
    float send = 0.0f;
    int ampEnvIndex = 0;
    UnisonParams unison {};
    // Legacy migration field. New source-track runtime uses ampEnvIndex and
    // shared Matrix ENV slots instead of per-track copied ADSR data.
    AdsrParams ampEnvelope {};
    GeneratorSourceParams strip {};
    WavetableSeedParams partialBank {};
    WavetablePartialSlot metaOsc {};
    std::array<BasicOscUnit, kBasicOscUnits> basicUnits {};
    BasicOscModParams basicMod {};
    SampleNoiseMode sampleNoiseMode = SampleNoiseMode::Noise;
    float noiseColor = 0.5f;
    int perVoiceFilterCount = 0;
    std::array<SourceFilterParams, kMaxPerVoiceFilters> perVoiceFilters {};
    int perVoiceFilterOrderCount = 0;
    std::array<uint8_t, kMaxPerVoiceFilters> perVoiceFilterOrder {};
    // Unbounded per-strip insert chain; each effect carries its own parameters.
    std::vector<InsertEffect> inserts {};
    // Route-graph-driven insert ordering. If insertOrderCount > 0 the engine
    // applies only the listed indices (0-based into inserts) in that order.
    // insertOrderCount == 0 means the router has not wired any inserts → bypass all.
    int insertOrderCount = 0;
    std::array<uint8_t, kMaxStripInserts> insertOrder {};
    bool connectedToMaster = false; // true only when route graph wires reach MASTER
    std::array<SourceModEntry, kMaxTrackMods> mods {}; // source-as-modulator entries
};

// Source-router merge group: members are summed into one bus. Bus FX live in
// each source's strip grid, not on the merge group itself.
struct SourceGroupDef
{
    std::vector<uint32_t> memberTrackIds;
};

// ---------------------------------------------------------------------------
// Compiled per-voice routing graph.
//
// The route-graph UI lets sources, per-voice filter nodes, strip nodes and
// MASTER be wired arbitrarily. The audio engine evaluates the *per-voice* part
// of that graph as a real DAG inside each voice: a filter node sums all its
// inputs and filters once; a track's strip bus sums every per-voice node wired
// into it. (Strip inserts + master mix still run per-track-bus downstream.)
//
// A plain linear chain (source → filter → … → strip) compiles to exactly the
// same signal flow the engine produced before, so existing patches are
// unchanged; only true merges (multiple outputs into one node) differ.
struct RouteNodeRef
{
    // 0 = source track (id = trackId), 1 = per-voice filter (id = slot),
    // 2 = amp-env route node instance (id = node index 0..kMaxAmpEnvRouteNodes-1),
    // 3 = utility node (id = util node index 0..kMaxUtilNodes-1)
    uint8_t kind = 0;
    uint32_t id = 0;
};

constexpr int kMaxAmpEnvRouteNodes = 16;
constexpr int kMaxUtilNodes = 16;
constexpr int kMaxRouteInputs = kMaxSourceTracks + kMaxPerVoiceFilters + kMaxAmpEnvRouteNodes + kMaxUtilNodes;

// A utility node (component output-router): level + pan, and an optional custom
// band-pass (keep only [bandLoHz, bandHiHz]).
struct RouteUtilParams
{
    float level = 1.0f;
    float pan = 0.0f;
    float bandLoHz = 20.0f;
    float bandHiHz = 20000.0f;
    bool  bandOn = false;
};

// Carries only graph *topology*. Filter params travel via the per-track runtime
// (globally shared), so realtime knob drags don't need to rebuild the route.
struct CompiledPerVoiceRoute
{
    bool valid = false;                 // false → engine uses the legacy per-track chains
    int  filterCount = 0;               // number of active filter slots

    // Filter nodes, in topological evaluation order (inputs computed first).
    int filterOrderCount = 0;
    std::array<uint8_t, kMaxPerVoiceFilters> filterOrder {};
    std::array<uint8_t, kMaxPerVoiceFilters> filterInputCount {};
    std::array<std::array<RouteNodeRef, kMaxRouteInputs>, kMaxPerVoiceFilters> filterInputs {};

    // Amp-env route nodes: many graph nodes can reference the same AE1..AE4
    // parameter slot, but each node has independent graph inputs/output.
    int ampEnvNodeCount = 0;
    std::array<uint8_t, kMaxAmpEnvRouteNodes> ampEnvSlot {};
    std::array<uint8_t, kMaxAmpEnvRouteNodes> ampEnvInputCount {};
    std::array<std::array<RouteNodeRef, kMaxRouteInputs>, kMaxAmpEnvRouteNodes> ampEnvInputs {};

    // Utility nodes (component output-router): apply level/pan/band to the sum of
    // their inputs. Each component's structure compiles into these.
    int utilCount = 0;
    std::array<RouteUtilParams, kMaxUtilNodes> utilParams {};
    std::array<uint8_t, kMaxUtilNodes> utilInputCount {};
    std::array<std::array<RouteNodeRef, kMaxRouteInputs>, kMaxUtilNodes> utilInputs {};

    // Unified topological eval order over filter + amp-env + utility nodes (inputs first).
    int evalOrderCount = 0;
    std::array<RouteNodeRef, kMaxPerVoiceFilters + kMaxAmpEnvRouteNodes + kMaxUtilNodes> evalOrder {};

    // Per route track slot: true if this source reaches an amp-env node downstream,
    // so the implicit source amp-env is bypassed (the node applies it instead).
    std::array<uint8_t, kMaxSourceTracks> sourceEnvBypass {};

    // Per-track strip-bus feeders (which per-voice nodes flow into each track's
    // bus, before that track's strip inserts). Indexed by route track slot;
    // trackId[] maps the slot to a concrete track so the voice can resolve it to
    // its current render index (mute/solo may reorder render tracks).
    int trackCount = 0;
    std::array<uint32_t, kMaxSourceTracks> trackId {};
    std::array<uint8_t, kMaxSourceTracks> busInputCount {};
    std::array<std::array<RouteNodeRef, kMaxRouteInputs>, kMaxSourceTracks> busInputs {};
};

struct RenderTrackRuntime
{
    uint32_t trackId = 0;
    GeneratorSourceParams strip {};
    bool muted = false;
    int ampEnvIndex = 0;
    UnisonParams unison {};
    SourceTrackOutputMode outputMode = SourceTrackOutputMode::Audio;
    std::array<SourceModEntry, kMaxTrackMods> mods {};
    BasicOscModParams basicMod {};        // the rack's own cross-unit modulation
    // Whether the modulating unit is itself audible. A unit switched off is still
    // rendered when it is the modulation source — it just doesn't reach the sum.
    bool basicModSourceAudible = true;
    int perVoiceFilterCount = 0;
    std::array<SourceFilterParams, kMaxPerVoiceFilters> perVoiceFilters {};
    int perVoiceFilterOrderCount = 0;
    std::array<uint8_t, kMaxPerVoiceFilters> perVoiceFilterOrder {};
    std::vector<InsertEffect> inserts {}; // this strip's own insert chain
    int insertOrderCount = 0;
    std::array<uint8_t, kMaxStripInserts> insertOrder {};
    bool connectedToMaster = false;
};

struct WavetablePartialRenderData
{
    using FrameTable = std::array<float, kWavetableSize + 1>;
    using FrameTables = std::vector<FrameTable>;
    using MipTables = std::array<FrameTables, kWavetableMipLevels>;

    bool enabled = true;
    bool usesMetaWavetable = false;
    float ratio = 1.0f;
    float amp = 1.0f;
    float phase = 0.0f;
    float pan = 0.0f;
    int frameCount = 1;
    float morph = 0.0f;
    WavetableWarpMode warpMode = WavetableWarpMode::None;
    float warpAmount = 0.0f;
    std::shared_ptr<const FrameTables> tables;
    std::shared_ptr<const MipTables> mipTables;
};

struct WavetableSeedRenderState
{
    int partialCount = 1;
    int sourceCount = 1;
    // Concatenated partials across all tracks (the shared render pool).
    std::array<WavetablePartialRenderData, kMaxRenderPartials> partials {};
    int trackCount = 0;
    std::array<int, kMaxSourceTracks> trackBegin {};
    std::array<int, kMaxSourceTracks> trackEnd {};
    // Where each Basic Oscillator unit landed inside its track's block, so the
    // voice can render one unit as a modulator for another.
    std::array<std::array<int, kBasicOscUnits>, kMaxSourceTracks> unitBegin {};
    std::array<std::array<int, kBasicOscUnits>, kMaxSourceTracks> unitEnd {};
    std::array<AdsrParams, kMaxSourceTracks> trackAdsr {};
    std::array<SourceTrackOutputMode, kMaxSourceTracks> trackOutputMode {};
};

struct SourceGenParams
{
    WavetableSeedParams wavetableSeed;
    UnisonParams unison;
    int sourceCount = 1;
    std::array<GeneratorSourceParams, 8> sources {};
    std::vector<SourceTrackParams> tracks {};
    float partialMaxRefHz = 40.0f;
    RenderQualityMode renderQuality = RenderQualityMode::Normal;
};

// Decimate one wavetable frame into a bipolar modulation LUT: `lutSize`+1 floats
// with out[lutSize] repeating out[0] so a lerp needs no wrap test. Band-limited
// to a modulation-sane harmonic count — a mask lane is sampled at control rate,
// so the audio table's 1024 harmonics would only alias.
// Deliberately does NOT normalize (unlike the audio-path bakeFrameTable, which
// peak-normalizes each frame in isolation): it returns the frame's peak so the
// caller can scale a whole table by one factor and keep the frame-to-frame
// amplitude contour, which for a modulation fan is signal, not loudness.
float bakeModWaveLut(const WavetableFrame &frame, float *out, int lutSize);

void initDefaultWavetableSeed(WavetableSeedParams &p);
int sanitizeGeneratorSourceCount(int sourceCount);
int partialsPerGeneratorSource(int sourceCount);
int metaPartialsPerGeneratorSource(int sourceCount);
int generatorSourceForPartial(int sourceCount, int partialIndex);
int localPartialIndexInSource(int sourceCount, int partialIndex);
int metaSlotForSourcePartial(int sourceCount, int partialIndex);
const char *sourceFilterTopologyName(SourceFilterTopology t);
const char *sourceTrackTypeName(SourceTrackType t);
const char *sourceTrackOutputModeName(SourceTrackOutputMode m);
const char *basicOscillatorShapeName(BasicOscillatorShape s);
const char *sampleNoiseModeName(SampleNoiseMode m);
void bakeWavetableSeed(const WavetableSeedParams &params, WavetableSeedRenderState &out);
void bakeWavetableSeed(const WavetableSeedParams &params, int sourceCount, WavetableSeedRenderState &out);
void rebakeWavetableSeedMetaPartial(const WavetableSeedParams &params, int sourceCount, int metaIndex,
                                    WavetableSeedRenderState &out);
void refreshWavetableSeedRuntime(const WavetableSeedParams &params, int sourceCount, WavetableSeedRenderState &out);
bool loadWavetableFrameFromWav(const std::string &path, WavetableFrame &frame);
// Decode a WAV impulse response down-mixed to mono (full length, not resampled).
bool loadImpulseResponseMono(const std::string &path, std::vector<float> &out, uint32_t &srcRate);
int loadWavetableFramesFromWav(const std::string &path, WavetablePartialSlot &slot, int startFrame = 0);
WavetableImportResult importWavetableFramesFromWav(const std::string &path, WavetablePartialSlot &slot,
                                                    int startFrame, const WavetableImportOptions &options);
void analyzeWavetableFrame(WavetableFrame &frame);
void rebuildWavetableFrameFromSpectrum(WavetableFrame &frame);
void materializeWavetableFrame(WavetableFrame &frame);
bool addWavetableFrame(WavetablePartialSlot &slot, int afterIndex);
bool duplicateWavetableFrame(WavetablePartialSlot &slot, int frameIndex);
bool deleteWavetableFrame(WavetablePartialSlot &slot, int frameIndex);
int deleteSelectedWavetableFrames(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount);
bool moveWavetableFrame(WavetablePartialSlot &slot, int fromIndex, int toIndex);
bool alignWavetableFramePhase(WavetablePartialSlot &slot, int frameIndex, int referenceIndex);
bool morphWavetableFrame(WavetablePartialSlot &slot, int destinationIndex, int leftIndex, int rightIndex,
                         float amount, WavetableMorphMode mode);
bool expandSelectedWavetableFrames(WavetablePartialSlot &slot, const bool *selectedFrames,
                                   int selectedFrameCount, int targetFrameCount);

// 手动波表后处理操作
void processWavetableRemoveDC(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount);
void processWavetableZeroAlign(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount);
void processWavetableAlignPhases(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount);
void processWavetableEnergySmooth(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount);
void processWavetableCrossfade(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount);

class GeneratorBank
{
  public:
    void generate(const SourceGenParams &p, StaticSpectralFrame &out) const;
    void generateTimeline(const SourceGenParams &p, SpectralTimeline &out) const;
};

// unitCount, when given, receives each unit's partial count (kBasicOscUnits
// entries) so the caller can map units onto the flattened partial pool.
void buildBasicSeed(const SourceTrackParams &track, WavetableSeedParams &seed,
                    int *unitCount = nullptr);
void buildNoiseSeed(const SourceTrackParams &track, WavetableSeedParams &seed);

} // namespace synth
