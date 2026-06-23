#pragma once

#include "engine/MatrixEngine.h"
#include "dsp/InsertEffects.h"
#include "model/SpectralFrame.h"

#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <string>
#include <vector>

namespace synth
{

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

constexpr int kMaxWavetablePartials = 64;
constexpr int kEditableMetaPartials = 8;
constexpr int kMaxSourceTracks = 16;
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
    BasicOscillatorShape basicShape = BasicOscillatorShape::Sine;
    float pulseWidth = 0.5f;
    float subLevel = 0.0f;
    SampleNoiseMode sampleNoiseMode = SampleNoiseMode::Noise;
    float noiseColor = 0.5f;
    // Unbounded per-strip insert chain; each effect carries its own parameters.
    std::vector<InsertEffect> inserts {};
    std::array<SourceModEntry, kMaxTrackMods> mods {}; // source-as-modulator entries
};

// A UI group rendered as its own bus: members are summed, then the group's insert chain runs.
struct SourceGroupDef
{
    std::vector<uint32_t> memberTrackIds;
    std::vector<InsertEffect> inserts;
};

struct RenderTrackRuntime
{
    uint32_t trackId = 0;
    GeneratorSourceParams strip {};
    int ampEnvIndex = 0;
    UnisonParams unison {};
    SourceTrackOutputMode outputMode = SourceTrackOutputMode::Audio;
    std::array<SourceModEntry, kMaxTrackMods> mods {};
    std::vector<InsertEffect> inserts {}; // this strip's own insert chain
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
    std::array<WavetablePartialRenderData, kMaxWavetablePartials> partials {};
    int trackCount = 0;
    std::array<int, kMaxSourceTracks> trackBegin {};
    std::array<int, kMaxSourceTracks> trackEnd {};
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

} // namespace synth
