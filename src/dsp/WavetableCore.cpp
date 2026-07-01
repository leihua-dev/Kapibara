#include "Generators.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace synth
{

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

uint16_t readU16(const std::vector<uint8_t> &data, size_t off)
{
    if(off + 2 > data.size())
        return 0;
    return uint16_t(data[off]) | (uint16_t(data[off + 1]) << 8u);
}

uint32_t readU32(const std::vector<uint8_t> &data, size_t off)
{
    if(off + 4 > data.size())
        return 0;
    return uint32_t(data[off]) | (uint32_t(data[off + 1]) << 8u)
         | (uint32_t(data[off + 2]) << 16u) | (uint32_t(data[off + 3]) << 24u);
}

int32_t readI24(const uint8_t *p)
{
    int32_t v = int32_t(p[0]) | (int32_t(p[1]) << 8) | (int32_t(p[2]) << 16);
    if(v & 0x00800000)
        v |= ~0x00ffffff;
    return v;
}

void fft2048(std::array<std::complex<float>, kWavetableSize> &values, bool inverse)
{
    for(size_t i = 1, j = 0; i < values.size(); ++i)
    {
        size_t bit = values.size() >> 1u;
        for(; j & bit; bit >>= 1u)
            j ^= bit;
        j ^= bit;
        if(i < j)
            std::swap(values[i], values[j]);
    }
    for(size_t len = 2; len <= values.size(); len <<= 1u)
    {
        const float angle = (inverse ? kTwoPi : -kTwoPi) / float(len);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for(size_t base = 0; base < values.size(); base += len)
        {
            std::complex<float> w(1.0f, 0.0f);
            for(size_t j = 0; j < len / 2; ++j)
            {
                const auto even = values[base + j];
                const auto odd = values[base + j + len / 2] * w;
                values[base + j] = even + odd;
                values[base + j + len / 2] = even - odd;
                w *= step;
            }
        }
    }
    if(inverse)
        for(auto &value : values)
            value /= float(values.size());
}

void removeDcAndNormalize(std::array<float, kWavetableSize> &wave)
{
    float mean = 0.0f;
    for(const float value : wave)
        mean += value;
    mean /= float(wave.size());
    float peak = 0.0f;
    for(float &value : wave)
    {
        value -= mean;
        peak = std::max(peak, std::abs(value));
    }
    if(peak > 1.0e-6f)
    {
        const float gain = 1.0f / peak;
        for(float &value : wave)
            value *= gain;
    }
}

struct DecodedWav
{
    std::vector<float> mono;
    uint32_t sampleRate = 0;
};

bool decodeWavMono(const std::string &path, DecodedWav &decoded)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
        return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if(data.size() < 44
       || (std::memcmp(data.data(), "RIFF", 4) != 0 && std::memcmp(data.data(), "RF64", 4) != 0)
       || std::memcmp(data.data() + 8, "WAVE", 4) != 0)
        return false;

    uint16_t format = 0;
    uint16_t channels = 0;
    uint16_t bitsPerSample = 0;
    uint32_t sampleRate = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;
    size_t off = 12;
    while(off + 8 <= data.size())
    {
        const uint32_t chunkSize = readU32(data, off + 4);
        const size_t chunkData = off + 8;
        if(chunkData + chunkSize > data.size())
            break;
        if(std::memcmp(data.data() + off, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            format = readU16(data, chunkData);
            channels = readU16(data, chunkData + 2);
            sampleRate = readU32(data, chunkData + 4);
            bitsPerSample = readU16(data, chunkData + 14);
            if(format == 0xfffeu && chunkSize >= 40)
                format = readU16(data, chunkData + 24);
        }
        else if(std::memcmp(data.data() + off, "data", 4) == 0)
        {
            dataOffset = uint32_t(chunkData);
            dataSize = chunkSize;
        }
        off = chunkData + chunkSize + (chunkSize & 1u);
    }
    if(dataOffset == 0 || dataSize == 0 || channels == 0 || sampleRate == 0)
        return false;
    const uint16_t bytesPerSample = uint16_t((bitsPerSample + 7u) / 8u);
    if(bytesPerSample == 0)
        return false;
    const uint32_t frameBytes = uint32_t(bytesPerSample) * uint32_t(channels);
    const uint32_t sampleFrames = dataSize / std::max(1u, frameBytes);
    if(sampleFrames < 2)
        return false;
    decoded.mono.assign((size_t)sampleFrames, 0.0f);
    decoded.sampleRate = sampleRate;
    for(uint32_t s = 0; s < sampleFrames; ++s)
    {
        float sum = 0.0f;
        for(uint16_t ch = 0; ch < channels; ++ch)
        {
            const size_t p = size_t(dataOffset) + size_t(s) * frameBytes + size_t(ch) * bytesPerSample;
            float value = 0.0f;
            if(format == 1 && bitsPerSample == 8)
                value = (float(data[p]) - 128.0f) / 128.0f;
            else if(format == 1 && bitsPerSample == 16)
                value = float(int16_t(readU16(data, p))) / 32768.0f;
            else if(format == 1 && bitsPerSample == 24 && p + 3 <= data.size())
                value = float(readI24(data.data() + p)) / 8388608.0f;
            else if(format == 1 && bitsPerSample == 32)
                value = float(int32_t(readU32(data, p))) / 2147483648.0f;
            else if(format == 3 && bitsPerSample == 32 && p + 4 <= data.size())
                std::memcpy(&value, data.data() + p, sizeof(float));
            else
                return false;
            sum += value;
        }
        decoded.mono[(size_t)s] = sum / float(channels);
    }
    return true;
}

// 在 offset 附近找最近过零点（上升沿）用于边界对齐
static size_t findNearestZeroCrossing(const std::vector<float> &source, size_t offset, int searchRadius)
{
    const int n = int(source.size());
    int best = int(offset);
    int bestDist = searchRadius + 1;
    for(int d = 0; d <= searchRadius; ++d)
    {
        for(int sign : {-1, 1})
        {
            const int pos = int(offset) + sign * d;
            if(pos <= 0 || pos >= n - 1)
                continue;
            // 上升沿过零：前一样本 < 0，当前样本 >= 0
            if(source[(size_t)(pos - 1)] < 0.0f && source[(size_t)pos] >= 0.0f)
            {
                if(d < bestDist) { bestDist = d; best = pos; }
                goto found;
            }
        }
    }
found:
    return size_t(best);
}

// 高质量线性插值重采样，可选交叉淡化消边界不连续
void resampleCycle(const std::vector<float> &source, size_t offset, int length,
                   std::array<float, kWavetableSize> &destination)
{
    if(source.empty() || length < 2 || offset >= source.size())
    {
        destination.fill(0.0f);
        return;
    }
    const size_t available = std::min<size_t>(size_t(length), source.size() - offset);

    // 线性插值重采样（周期循环）
    for(int sample = 0; sample < kWavetableSize; ++sample)
    {
        const float position = float(sample) * float(available) / float(kWavetableSize);
        const size_t a = std::min<size_t>(size_t(position), available - 1);
        const size_t b = (a + 1) % available;
        const float frac = position - float(a);
        destination[(size_t)sample] = source[offset + a] + (source[offset + b] - source[offset + a]) * frac;
    }

    // 交叉淡化：帧两端 crossfade_len 个样本做首尾平滑，消除拼接跳变
    constexpr int crossfade_len = kWavetableSize / 16; // ~128 samples at 2048
    for(int i = 0; i < crossfade_len; ++i)
    {
        const float t = float(i) / float(crossfade_len);
        const float wEnd   = t;       // 末端（来自循环开始）的权重
        const float wStart = 1.0f - t;
        // 末端样本：destination[N - crossfade_len + i] 与 destination[i] 融合
        const int pos = kWavetableSize - crossfade_len + i;
        destination[(size_t)pos] = destination[(size_t)pos] * wStart + destination[(size_t)i] * wEnd;
    }
}

// 去DC并做 RMS 归一化（比峰值归一更平衡帧间响度）
static void removeDcAndNormalizeRms(std::array<float, kWavetableSize> &wave)
{
    // 去 DC
    float mean = 0.0f;
    for(const float v : wave)
        mean += v;
    mean /= float(wave.size());
    for(float &v : wave)
        v -= mean;

    // RMS 计算
    float rms = 0.0f;
    for(const float v : wave)
        rms += v * v;
    rms = std::sqrt(rms / float(wave.size()));

    if(rms > 1.0e-6f)
    {
        // 目标 RMS = 0.3（约 -10 dBFS），再裁剪峰值到 1.0
        const float targetRms = 0.30f;
        float gain = targetRms / rms;
        float peak = 0.0f;
        for(const float v : wave)
            peak = std::max(peak, std::abs(v * gain));
        if(peak > 1.0f)
            gain /= peak;
        for(float &v : wave)
            v *= gain;
    }
}

// 帧间相位对齐：通过互相关找出最优相位偏移，使两帧基频相位连续
// 返回 shiftSamples（应将当前帧循环移位多少样本以对齐前一帧）
static int computePhaseAlignment(const std::array<float, kWavetableSize> &ref,
                                  const std::array<float, kWavetableSize> &cur)
{
    // 在 ±kWavetableSize/2 范围内搜索最大互相关
    constexpr int halfN = kWavetableSize / 2;
    float bestCorr = -1e38f;
    int bestShift = 0;
    // 只搜 16 个候选偏移以减少开销（步长=8）
    for(int d = -halfN; d < halfN; d += 8)
    {
        float corr = 0.0f;
        for(int i = 0; i < kWavetableSize; ++i)
        {
            const int j = (i + d + kWavetableSize * 2) % kWavetableSize;
            corr += ref[(size_t)i] * cur[(size_t)j];
        }
        if(corr > bestCorr) { bestCorr = corr; bestShift = d; }
    }
    return bestShift;
}

struct PitchEstimate
{
    int period = 0;
    float confidence = 0.0f;
};

PitchEstimate estimateConstantPitch(const DecodedWav &decoded)
{
    const auto &samples = decoded.mono;
    if(samples.size() < 128 || decoded.sampleRate == 0)
        return {};
    const int minPeriod = std::max(8, int(decoded.sampleRate / 2000u));
    const int maxPeriod = std::min<int>(int(decoded.sampleRate / 30u), int(samples.size() / 3));
    if(maxPeriod <= minPeriod)
        return {};

    const size_t analysisLength = std::min<size_t>(samples.size() - size_t(maxPeriod), 32768u);
    const size_t start = samples.size() > analysisLength + size_t(maxPeriod)
                             ? (samples.size() - analysisLength - size_t(maxPeriod)) / 2u
                             : 0u;
    PitchEstimate best;
    for(int period = minPeriod; period <= maxPeriod; ++period)
    {
        double cross = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;
        for(size_t i = 0; i < analysisLength; i += 2)
        {
            const float a = samples[start + i];
            const float b = samples[start + i + size_t(period)];
            cross += double(a) * double(b);
            energyA += double(a) * double(a);
            energyB += double(b) * double(b);
        }
        const float correlation = float(cross / std::sqrt(std::max(1.0e-18, energyA * energyB)));
        if(correlation > best.confidence)
        {
            best.period = period;
            best.confidence = correlation;
        }
    }
    return best;
}

inline float computeNuDirect(FreqShape shape, float n, float inharm)
{
    switch(shape)
    {
        case FreqShape::Harmonic: return std::pow(n, 1.0f + 0.5f * inharm);
        case FreqShape::Linear: return 1.0f + (n - 1.0f) * (1.0f + 0.3f * inharm);
        case FreqShape::Exponential: return n * std::exp(0.03f * inharm * (n - 1.0f));
    }
    return n;
}

float wrapPhaseRadians(float phase)
{
    constexpr float twoPi = 6.28318530717958647692f;
    while(phase > 3.14159265358979323846f) phase -= twoPi;
    while(phase < -3.14159265358979323846f) phase += twoPi;
    return phase;
}

[[maybe_unused]] float interpolatePhaseRadians(float a, float b, float t)
{
    return wrapPhaseRadians(a + wrapPhaseRadians(b - a) * t);
}

[[maybe_unused]] void zeroTrailing(StaticSpectralFrame &f, int n)
{
    for(int i = n; i < kMaxPartials; ++i)
    {
        f.nu[(size_t)i] = 0.0f;
        f.amp[(size_t)i] = 0.0f;
        f.x[(size_t)i] = 0.0f;
        f.mu[(size_t)i] = 0u;
        f.phaseLocked[(size_t)i] = 0.0f;
        f.phaseDriftHz[(size_t)i] = 0.0f;
        f.phaseJitter[(size_t)i] = 0.0f;
    }
}

void bakeFrameTable(const WavetableFrame &frame, std::array<float, kWavetableSize + 1> &table, int maxHarmonic)
{
    if(frame.useImportedWaveform && frame.waveform)
    {
        std::array<std::complex<float>, kWavetableSize> bins {};
        for(int s = 0; s < kWavetableSize; ++s)
            bins[(size_t)s] = std::complex<float>((*frame.waveform)[(size_t)s], 0.0f);
        fft2048(bins, false);
        const int limit = std::clamp(maxHarmonic, 1, kWavetableSize / 2);
        for(int k = limit + 1; k < kWavetableSize - limit; ++k)
            bins[(size_t)k] = {};
        fft2048(bins, true);
        float peak = 0.0f;
        for(int s = 0; s < kWavetableSize; ++s)
            peak = std::max(peak, std::abs(bins[(size_t)s].real()));
        const float gain = peak > 1.0e-6f ? 1.0f / peak : 1.0f;
        for(int s = 0; s < kWavetableSize; ++s)
            table[(size_t)s] = bins[(size_t)s].real() * gain;
        table[(size_t)kWavetableSize] = table[0];
        return;
    }

    float peak = 0.0f;
    for(int s = 0; s < kWavetableSize; ++s)
    {
        const float phase = kTwoPi * float(s) / float(kWavetableSize);
        float v = 0.0f;
        const int harmonicLimit = std::clamp(maxHarmonic, 1, kMaxWavetableHarmonics);
        for(int harmonicIndex = 0; harmonicIndex < harmonicLimit; ++harmonicIndex)
        {
            const auto &h = frame.harmonics[(size_t)harmonicIndex];
            if(h.amp <= 0.0f || h.ratio <= 0.0f)
                continue;
            if(h.ratio > float(harmonicLimit) + 0.001f)
                continue;
            v += h.amp * std::sin(phase * h.ratio + h.phase);
        }
        table[(size_t)s] = v;
        peak = std::max(peak, std::abs(v));
    }

    if(peak > 1.0e-6f)
    {
        const float invPeak = 1.0f / peak;
        for(int s = 0; s < kWavetableSize; ++s)
            table[(size_t)s] *= invPeak;
    }
    table[(size_t)kWavetableSize] = table[0];
}

int mipHarmonicLimit(int level)
{
    return std::max(1, kMaxWavetableHarmonics >> std::clamp(level, 0, kWavetableMipLevels - 1));
}

void bakePartialTableCache(const WavetablePartialSlot &src, WavetablePartialRenderData &dst)
{
    WavetablePartialSlot aligned = src;
    aligned.frameCount = dst.frameCount;
    for(int frame = 1; frame < aligned.frameCount; ++frame)
        alignWavetableFramePhase(aligned, frame, frame - 1);

    auto mipTables = std::make_shared<WavetablePartialRenderData::MipTables>();
    auto baseTables = std::make_shared<WavetablePartialRenderData::FrameTables>();
    baseTables->resize((size_t)dst.frameCount);
    for(int level = 0; level < kWavetableMipLevels; ++level)
    {
        (*mipTables)[(size_t)level].resize((size_t)dst.frameCount);
        for(int f = 0; f < dst.frameCount; ++f)
            bakeFrameTable(aligned.frames[(size_t)f], (*mipTables)[(size_t)level][(size_t)f], mipHarmonicLimit(level));
    }
    for(int f = 0; f < dst.frameCount; ++f)
        (*baseTables)[(size_t)f] = (*mipTables)[0][(size_t)f];
    dst.tables = std::move(baseTables);
    dst.mipTables = std::move(mipTables);
}
} // namespace

WavetableFrameStorage::WavetableFrameStorage(const WavetableFrameStorage &other)
{
    data = other.data;
}

WavetableFrameStorage &WavetableFrameStorage::operator=(const WavetableFrameStorage &other)
{
    if(this == &other)
        return *this;
    data = other.data;
    return *this;
}

WavetableFrameArray &WavetableFrameStorage::ensure()
{
    if(!data)
        data = std::make_shared<WavetableFrameArray>();
    else if(!data.unique())
        data = std::make_shared<WavetableFrameArray>(*data);
    return *data;
}

const WavetableFrameArray &WavetableFrameStorage::get() const
{
    static const WavetableFrameArray empty {};
    return data ? *data : empty;
}

WavetableFrame &WavetableFrameStorage::operator[](size_t index)
{
    auto &frame = ensure()[index];
    if(!frame)
        frame = std::make_shared<WavetableFrame>();
    else if(!frame.unique())
        frame = std::make_shared<WavetableFrame>(*frame);
    return *frame;
}

const WavetableFrame &WavetableFrameStorage::operator[](size_t index) const
{
    static const WavetableFrame emptyFrame {};
    const auto &frame = get()[index];
    return frame ? *frame : emptyFrame;
}

WavetableSeedParams::WavetableSeedParams()
{
    partialCount = 1;
    frameCount = 1;
    morph = 0.0f;
    auto &bankFrame = frames[(size_t)0];
    for(int i = 0; i < kMaxWavetablePartials; ++i)
    {
        auto &p = partials[(size_t)i];
        p.enabled = i < partialCount;
        p.ratio = float(i + 1);
        p.amp = 1.0f / float(i + 1);
        p.phase = 0.0f;
        p.pan = 0.0f;
        bankFrame.harmonics[(size_t)i].ratio = float(i + 1);
        bankFrame.harmonics[(size_t)i].amp = p.amp;
        bankFrame.harmonics[(size_t)i].phase = p.phase;
        p.frameCount = (i < kEditableMetaPartials) ? kDefaultWavetableFrames : 1;
        for(int f = 0; f < p.frameCount; ++f)
        {
            p.frames[(size_t)f].harmonics[0].ratio = 1.0f;
            p.frames[(size_t)f].harmonics[0].amp = 1.0f;
            p.frames[(size_t)f].harmonics[0].phase = 0.0f;
        }
    }
}

void initDefaultWavetableSeed(WavetableSeedParams &p)
{
    p = WavetableSeedParams {};
}

int sanitizeGeneratorSourceCount(int sourceCount)
{
    if(sourceCount <= 1)
        return 1;
    if(sourceCount <= 2)
        return 2;
    if(sourceCount <= 4)
        return 4;
    return 8;
}

int partialsPerGeneratorSource(int sourceCount)
{
    return kMaxWavetablePartials / sanitizeGeneratorSourceCount(sourceCount);
}

int metaPartialsPerGeneratorSource(int sourceCount)
{
    return kEditableMetaPartials / sanitizeGeneratorSourceCount(sourceCount);
}

int generatorSourceForPartial(int sourceCount, int partialIndex)
{
    const int count = sanitizeGeneratorSourceCount(sourceCount);
    return std::clamp(partialIndex / partialsPerGeneratorSource(count), 0, count - 1);
}

int localPartialIndexInSource(int sourceCount, int partialIndex)
{
    const int perSource = partialsPerGeneratorSource(sourceCount);
    return std::clamp(partialIndex, 0, kMaxWavetablePartials - 1) % perSource;
}

int metaSlotForSourcePartial(int sourceCount, int partialIndex)
{
    const int count = sanitizeGeneratorSourceCount(sourceCount);
    const int source = generatorSourceForPartial(count, partialIndex);
    const int local = localPartialIndexInSource(count, partialIndex);
    const int metaPerSource = metaPartialsPerGeneratorSource(count);
    if(local >= metaPerSource)
        return -1;
    return source * metaPerSource + local;
}

const char *sourceFilterTopologyName(SourceFilterTopology t)
{
    switch(t)
    {
        case SourceFilterTopology::Bypass: return "Bypass";
        case SourceFilterTopology::OnePoleLowPass: return "1P LP";
        case SourceFilterTopology::TwoPoleStateVariable: return "2P SVF";
        case SourceFilterTopology::FourPoleCascade: return "4P";
        case SourceFilterTopology::FeedbackLadder: return "Feedback";
    }
    return "Filter";
}

const char *sourceTrackTypeName(SourceTrackType t)
{
    switch(t)
    {
        case SourceTrackType::PartialBank: return "Partial Bank";
        case SourceTrackType::MetaOscillator: return "Meta Oscillator";
        case SourceTrackType::BasicOscillator: return "Basic Oscillator";
        case SourceTrackType::SampleNoise: return "Sample / Noise";
    }
    return "Source";
}

const char *sourceTrackOutputModeName(SourceTrackOutputMode m)
{
    switch(m)
    {
        case SourceTrackOutputMode::Audio: return "Audio";
        case SourceTrackOutputMode::ModOnly: return "Mod Only";
        case SourceTrackOutputMode::AudioAndMod: return "Audio + Mod";
    }
    return "Audio";
}

const char *basicOscillatorShapeName(BasicOscillatorShape s)
{
    switch(s)
    {
        case BasicOscillatorShape::Sine: return "Sine";
        case BasicOscillatorShape::Triangle: return "Triangle";
        case BasicOscillatorShape::Saw: return "Saw";
        case BasicOscillatorShape::Pulse: return "Pulse";
        case BasicOscillatorShape::Sub: return "Sub";
    }
    return "Sine";
}

const char *sampleNoiseModeName(SampleNoiseMode m)
{
    switch(m)
    {
        case SampleNoiseMode::Noise: return "Noise";
        case SampleNoiseMode::File: return "File";
        case SampleNoiseMode::Capture: return "Capture";
    }
    return "Noise";
}

bool loadImpulseResponseMono(const std::string &path, std::vector<float> &out, uint32_t &srcRate)
{
    DecodedWav decoded;
    if(!decodeWavMono(path, decoded) || decoded.mono.empty())
        return false;
    out = std::move(decoded.mono);
    srcRate = decoded.sampleRate;
    return true;
}

bool loadWavetableFrameFromWav(const std::string &path, WavetableFrame &frame)
{
    DecodedWav decoded;
    if(!decodeWavMono(path, decoded))
        return false;
    auto wave = std::make_shared<std::array<float, kWavetableSize>>();
    resampleCycle(decoded.mono, 0, int(decoded.mono.size()), *wave);
    removeDcAndNormalize(*wave);
    frame.useImportedWaveform = true;
    frame.waveform = std::move(wave);
    analyzeWavetableFrame(frame);
    return true;
}

void analyzeWavetableFrame(WavetableFrame &frame)
{
    if(!frame.waveform)
        return;
    auto spectrum = std::make_shared<WavetableFrame::Spectrum>();
    std::array<std::complex<float>, kWavetableSize> bins {};
    for(int i = 0; i < kWavetableSize; ++i)
        bins[(size_t)i] = std::complex<float>((*frame.waveform)[(size_t)i], 0.0f);
    fft2048(bins, false);
    for(size_t k = 0; k < spectrum->size(); ++k)
        (*spectrum)[k] = bins[k];
    frame.spectrum = std::move(spectrum);
    for(int h = 0; h < kMaxWavetableHarmonics; ++h)
    {
        const int bin = h + 1;
        const auto value = bins[(size_t)bin];
        frame.harmonics[(size_t)h].ratio = float(bin);
        frame.harmonics[(size_t)h].amp = 2.0f * std::abs(value) / float(kWavetableSize);
        frame.harmonics[(size_t)h].phase = std::arg(value) + 0.5f * 3.14159265358979323846f;
    }
}

void rebuildWavetableFrameFromSpectrum(WavetableFrame &frame)
{
    if(!frame.spectrum)
        return;
    std::array<std::complex<float>, kWavetableSize> bins {};
    for(size_t k = 0; k < frame.spectrum->size(); ++k)
        bins[k] = (*frame.spectrum)[k];
    for(int k = 1; k < kWavetableSize / 2; ++k)
        bins[(size_t)(kWavetableSize - k)] = std::conj(bins[(size_t)k]);
    bins[(size_t)(kWavetableSize / 2)] = std::complex<float>(bins[(size_t)(kWavetableSize / 2)].real(), 0.0f);
    fft2048(bins, true);
    auto wave = std::make_shared<std::array<float, kWavetableSize>>();
    for(int i = 0; i < kWavetableSize; ++i)
        (*wave)[(size_t)i] = bins[(size_t)i].real();
    removeDcAndNormalize(*wave);
    frame.waveform = std::move(wave);
    frame.useImportedWaveform = true;
    analyzeWavetableFrame(frame);
}

void materializeWavetableFrame(WavetableFrame &frame)
{
    if(frame.waveform)
    {
        if(!frame.spectrum)
            analyzeWavetableFrame(frame);
        return;
    }

    auto wave = std::make_shared<std::array<float, kWavetableSize>>();
    for(int sample = 0; sample < kWavetableSize; ++sample)
    {
        const float phase = kTwoPi * float(sample) / float(kWavetableSize);
        float value = 0.0f;
        for(const auto &harmonic : frame.harmonics)
        {
            if(harmonic.amp <= 0.0f || harmonic.ratio <= 0.0f)
                continue;
            value += harmonic.amp * std::sin(phase * harmonic.ratio + harmonic.phase);
        }
        (*wave)[(size_t)sample] = value;
    }
    removeDcAndNormalize(*wave);
    frame.useImportedWaveform = true;
    frame.waveform = std::move(wave);
    analyzeWavetableFrame(frame);
}

bool addWavetableFrame(WavetablePartialSlot &slot, int afterIndex)
{
    slot.frameCount = std::clamp(slot.frameCount, 1, kMaxWavetableFrames);
    if(slot.frameCount >= kMaxWavetableFrames)
        return false;
    afterIndex = std::clamp(afterIndex, -1, slot.frameCount - 1);
    const int insertIndex = afterIndex + 1;
    auto &frames = slot.frames.ensure();
    for(int i = slot.frameCount; i > insertIndex; --i)
        frames[(size_t)i] = frames[(size_t)(i - 1)];
    frames[(size_t)insertIndex] = std::make_shared<WavetableFrame>();
    ++slot.frameCount;
    return true;
}

bool duplicateWavetableFrame(WavetablePartialSlot &slot, int frameIndex)
{
    if(frameIndex < 0 || frameIndex >= slot.frameCount || slot.frameCount >= kMaxWavetableFrames)
        return false;
    const auto source = slot.frames[(size_t)frameIndex];
    if(!addWavetableFrame(slot, frameIndex))
        return false;
    slot.frames[(size_t)(frameIndex + 1)] = source;
    return true;
}

bool deleteWavetableFrame(WavetablePartialSlot &slot, int frameIndex)
{
    slot.frameCount = std::clamp(slot.frameCount, 1, kMaxWavetableFrames);
    if(slot.frameCount <= 1 || frameIndex < 0 || frameIndex >= slot.frameCount)
        return false;
    auto &frames = slot.frames.ensure();
    for(int i = frameIndex; i + 1 < slot.frameCount; ++i)
        frames[(size_t)i] = frames[(size_t)(i + 1)];
    frames[(size_t)(slot.frameCount - 1)].reset();
    --slot.frameCount;
    return true;
}

int deleteSelectedWavetableFrames(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount)
{
    if(selectedFrames == nullptr)
        return 0;
    const int limit = std::min(frameCount, slot.frameCount);
    int deleted = 0;
    for(int i = limit - 1; i >= 0 && slot.frameCount > 1; --i)
    {
        if(selectedFrames[(size_t)i] && deleteWavetableFrame(slot, i))
            ++deleted;
    }
    return deleted;
}

bool moveWavetableFrame(WavetablePartialSlot &slot, int fromIndex, int toIndex)
{
    slot.frameCount = std::clamp(slot.frameCount, 1, kMaxWavetableFrames);
    if(fromIndex < 0 || fromIndex >= slot.frameCount || toIndex < 0 || toIndex >= slot.frameCount
       || fromIndex == toIndex)
        return false;
    auto &frames = slot.frames.ensure();
    const auto moving = frames[(size_t)fromIndex];
    if(fromIndex < toIndex)
        for(int i = fromIndex; i < toIndex; ++i)
            frames[(size_t)i] = frames[(size_t)(i + 1)];
    else
        for(int i = fromIndex; i > toIndex; --i)
            frames[(size_t)i] = frames[(size_t)(i - 1)];
    frames[(size_t)toIndex] = moving;
    return true;
}

bool alignWavetableFramePhase(WavetablePartialSlot &slot, int frameIndex, int referenceIndex)
{
    if(frameIndex < 0 || referenceIndex < 0 || frameIndex >= slot.frameCount || referenceIndex >= slot.frameCount
       || frameIndex == referenceIndex)
        return false;
    auto &target = slot.frames[(size_t)frameIndex];
    auto &reference = slot.frames[(size_t)referenceIndex];
    materializeWavetableFrame(target);
    materializeWavetableFrame(reference);
    if(!target.waveform || !reference.waveform)
        return false;

    std::array<std::complex<float>, kWavetableSize> referenceBins {};
    std::array<std::complex<float>, kWavetableSize> targetBins {};
    for(int sample = 0; sample < kWavetableSize; ++sample)
    {
        referenceBins[(size_t)sample] = (*reference.waveform)[(size_t)sample];
        targetBins[(size_t)sample] = (*target.waveform)[(size_t)sample];
    }
    fft2048(referenceBins, false);
    fft2048(targetBins, false);
    for(int bin = 0; bin < kWavetableSize; ++bin)
        referenceBins[(size_t)bin] = std::conj(referenceBins[(size_t)bin]) * targetBins[(size_t)bin];
    fft2048(referenceBins, true);
    int bestShift = 0;
    float bestCorrelation = referenceBins[0].real();
    for(int shift = 1; shift < kWavetableSize; ++shift)
    {
        if(referenceBins[(size_t)shift].real() > bestCorrelation)
        {
            bestCorrelation = referenceBins[(size_t)shift].real();
            bestShift = shift;
        }
    }

    auto aligned = std::make_shared<std::array<float, kWavetableSize>>();
    for(int sample = 0; sample < kWavetableSize; ++sample)
        (*aligned)[(size_t)sample] = (*target.waveform)[(size_t)((sample + bestShift) & (kWavetableSize - 1))];
    target.waveform = std::move(aligned);
    target.useImportedWaveform = true;
    analyzeWavetableFrame(target);
    return true;
}

bool morphWavetableFrame(WavetablePartialSlot &slot, int destinationIndex, int leftIndex, int rightIndex,
                         float amount, WavetableMorphMode mode)
{
    if(destinationIndex < 0 || leftIndex < 0 || rightIndex < 0 || destinationIndex >= slot.frameCount
       || leftIndex >= slot.frameCount || rightIndex >= slot.frameCount || leftIndex == rightIndex)
        return false;
    amount = clampf(amount, 0.0f, 1.0f);
    auto left = slot.frames[(size_t)leftIndex];
    auto right = slot.frames[(size_t)rightIndex];
    materializeWavetableFrame(left);
    materializeWavetableFrame(right);
    if(!left.waveform || !right.waveform)
        return false;

    WavetableFrame result;
    if(mode == WavetableMorphMode::Linear)
    {
        result.waveform = std::make_shared<std::array<float, kWavetableSize>>();
        for(int sample = 0; sample < kWavetableSize; ++sample)
            (*result.waveform)[(size_t)sample] = (1.0f - amount) * (*left.waveform)[(size_t)sample]
                                               + amount * (*right.waveform)[(size_t)sample];
        removeDcAndNormalize(*result.waveform);
        result.useImportedWaveform = true;
        analyzeWavetableFrame(result);
    }
    else
    {
        if(!left.spectrum)
            analyzeWavetableFrame(left);
        if(!right.spectrum)
            analyzeWavetableFrame(right);
        result.spectrum = std::make_shared<WavetableFrame::Spectrum>();
        for(size_t bin = 0; bin < result.spectrum->size(); ++bin)
        {
            const float ampA = std::abs((*left.spectrum)[bin]);
            const float ampB = std::abs((*right.spectrum)[bin]);
            const float phaseA = std::arg((*left.spectrum)[bin]);
            const float phaseB = std::arg((*right.spectrum)[bin]);
            const float delta = std::remainder(phaseB - phaseA, kTwoPi);
            (*result.spectrum)[bin] = std::polar((1.0f - amount) * ampA + amount * ampB,
                                                 phaseA + amount * delta);
        }
        rebuildWavetableFrameFromSpectrum(result);
    }
    slot.frames[(size_t)destinationIndex] = std::move(result);
    return true;
}

bool expandSelectedWavetableFrames(WavetablePartialSlot &slot, const bool *selectedFrames,
                                   int selectedFrameCount, int targetFrameCount)
{
    targetFrameCount = std::clamp(targetFrameCount, 2, kMaxWavetableFrames);
    selectedFrameCount = std::clamp(selectedFrameCount, 0, slot.frameCount);
    if(selectedFrames == nullptr || selectedFrameCount < 2)
        return false;

    std::vector<WavetableFrame> keyFrames;
    keyFrames.reserve((size_t)selectedFrameCount);
    for(int i = 0; i < slot.frameCount; ++i)
    {
        if(!selectedFrames[(size_t)i])
            continue;
        keyFrames.push_back(slot.frames[(size_t)i]);
        materializeWavetableFrame(keyFrames.back());
    }
    if(keyFrames.size() < 2)
        return false;

    auto expanded = std::make_shared<WavetableFrameArray>();
    for(int destination = 0; destination < targetFrameCount; ++destination)
    {
        const float keyPosition = float(destination) * float(keyFrames.size() - 1)
                                / float(targetFrameCount - 1);
        const int leftIndex = std::min(int(keyPosition), int(keyFrames.size()) - 2);
        const int rightIndex = leftIndex + 1;
        const float amount = keyPosition - float(leftIndex);
        const auto &left = keyFrames[(size_t)leftIndex];
        const auto &right = keyFrames[(size_t)rightIndex];

        if(amount <= 1.0e-6f)
        {
            (*expanded)[(size_t)destination] = std::make_shared<WavetableFrame>(left);
            continue;
        }
        if(amount >= 1.0f - 1.0e-6f)
        {
            (*expanded)[(size_t)destination] = std::make_shared<WavetableFrame>(right);
            continue;
        }

        WavetableFrame result;
        result.spectrum = std::make_shared<WavetableFrame::Spectrum>();
        for(size_t bin = 0; bin < result.spectrum->size(); ++bin)
        {
            const float ampA = std::abs((*left.spectrum)[bin]);
            const float ampB = std::abs((*right.spectrum)[bin]);
            const float phaseA = std::arg((*left.spectrum)[bin]);
            const float phaseB = std::arg((*right.spectrum)[bin]);
            const float phaseDelta = std::remainder(phaseB - phaseA, kTwoPi);
            (*result.spectrum)[bin] = std::polar(ampA + (ampB - ampA) * amount,
                                                 phaseA + phaseDelta * amount);
        }
        rebuildWavetableFrameFromSpectrum(result);
        (*expanded)[(size_t)destination] = std::make_shared<WavetableFrame>(std::move(result));
    }

    slot.frames.data = std::move(expanded);
    slot.frameCount = targetFrameCount;
    return true;
}

int loadWavetableFramesFromWav(const std::string &path, WavetablePartialSlot &slot, int startFrame)
{
    const auto result = importWavetableFramesFromWav(path, slot, startFrame, {});
    return result.importedFrames;
}

WavetableImportResult importWavetableFramesFromWav(const std::string &path, WavetablePartialSlot &slot,
                                                    int startFrame, const WavetableImportOptions &requestedOptions)
{
    WavetableImportResult result;
    DecodedWav decoded;
    if(!decodeWavMono(path, decoded))
    {
        result.message = "WAV decode failed";
        return result;
    }
    startFrame = std::clamp(startFrame, 0, kMaxWavetableFrames - 1);
    const int available = std::min(kMaxWavetableFrames - startFrame,
                                   std::clamp(requestedOptions.maxFrames, 1, kMaxWavetableFrames));
    WavetableImportOptions options = requestedOptions;
    PitchEstimate pitch;
    if(options.mode == WavetableImportMode::AutoDetect)
    {
        const int fixedFrames = int(decoded.mono.size() / size_t(kWavetableSize));
        if(decoded.mono.size() % size_t(kWavetableSize) == 0 && fixedFrames >= 2)
            options.mode = WavetableImportMode::FixedFrames;
        else if(decoded.mono.size() <= size_t(kWavetableSize * 2))
            options.mode = WavetableImportMode::SingleCycle;
        else
        {
            pitch = estimateConstantPitch(decoded);
            options.mode = pitch.confidence >= 0.55f ? WavetableImportMode::ConstantPitch
                                                     : WavetableImportMode::SingleCycle;
        }
    }

    int cycleLength = kWavetableSize;
    switch(options.mode)
    {
        case WavetableImportMode::FixedFrames:
            cycleLength = std::clamp(options.frameLength, 32, 65536);
            break;
        case WavetableImportMode::SingleCycle:
            cycleLength = int(decoded.mono.size());
            break;
        case WavetableImportMode::ConstantPitch:
            if(pitch.period == 0)
                pitch = estimateConstantPitch(decoded);
            if(pitch.period == 0 || pitch.confidence < 0.15f)
            {
                result.message = "No stable pitch detected";
                return result;
            }
            cycleLength = pitch.period;
            result.estimatedPitchHz = float(decoded.sampleRate) / float(cycleLength);
            result.confidence = pitch.confidence;
            break;
        case WavetableImportMode::ManualCycleLength:
            cycleLength = std::clamp(options.manualCycleLength, 32, 65536);
            break;
        case WavetableImportMode::AutoDetect:
            break;
    }
    if(cycleLength < 2 || decoded.mono.size() < size_t(cycleLength))
    {
        result.message = "WAV is shorter than the selected cycle length";
        return result;
    }

    result.cycleLength = cycleLength;
    result.sourceFrames = options.mode == WavetableImportMode::SingleCycle
                              ? 1
                              : int(decoded.mono.size() / size_t(cycleLength));
    result.importedFrames = std::clamp(result.sourceFrames, 1, available);

    for(int destination = 0; destination < result.importedFrames; ++destination)
    {
        const int sourceIndex = result.importedFrames <= 1
                                    ? 0
                                    : int((int64_t(destination) * int64_t(result.sourceFrames - 1))
                                          / int64_t(result.importedFrames - 1));
        auto &frame = slot.frames[(size_t)(startFrame + destination)];
        auto wave = std::make_shared<std::array<float, kWavetableSize>>();
        resampleCycle(decoded.mono, size_t(sourceIndex) * size_t(cycleLength), cycleLength, *wave);
        removeDcAndNormalize(*wave);
        frame.useImportedWaveform = true;
        frame.waveform = std::move(wave);
        analyzeWavetableFrame(frame);
    }

    slot.frameCount = startFrame == 0 ? result.importedFrames
                                      : std::max(slot.frameCount, startFrame + result.importedFrames);
    slot.enabled = true;
    result.success = true;
    result.message = std::to_string(result.sourceFrames) + " source frames -> "
                   + std::to_string(result.importedFrames) + " imported";
    if(result.estimatedPitchHz > 0.0f)
        result.message += " at " + std::to_string(int(std::round(result.estimatedPitchHz))) + " Hz";
    return result;
}

// ---- 手动波表处理操作 ----

static bool frameSelected(const bool *sel, int i, int fc)
{
    return sel != nullptr && i < fc && sel[(size_t)i];
}

static void makeFrameWaveformWritable(WavetableFrame &frame)
{
    materializeWavetableFrame(frame);
    if(frame.waveform && !frame.waveform.unique())
        frame.waveform = std::make_shared<std::array<float, kWavetableSize>>(*frame.waveform);
}

// 去 DC + RMS 归一化（使用 RMS 目标归一，比峰值归一更一致）
void processWavetableRemoveDC(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount)
{
    for(int i = 0; i < frameCount; ++i)
    {
        if(!frameSelected(selectedFrames, i, slot.frameCount)) continue;
        auto &frm = slot.frames[(size_t)i];
        makeFrameWaveformWritable(frm);
        removeDcAndNormalizeRms(*frm.waveform);
        frm.spectrum.reset();
        analyzeWavetableFrame(frm);
    }
}

// 帧间相位对齐（选中帧相对前一帧对齐）
void processWavetableAlignPhases(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount)
{
    int previousSelected = -1;
    const int limit = std::min(frameCount, slot.frameCount);
    for(int i = 0; i < limit; ++i)
    {
        if(!frameSelected(selectedFrames, i, slot.frameCount))
            continue;
        auto &current = slot.frames[(size_t)i];
        materializeWavetableFrame(current);
        if(previousSelected >= 0)
        {
            auto &reference = slot.frames[(size_t)previousSelected];
            materializeWavetableFrame(reference);
            const int shift = computePhaseAlignment(*reference.waveform, *current.waveform);
            if(shift != 0)
            {
                auto shifted = std::make_shared<std::array<float, kWavetableSize>>();
                for(int sample = 0; sample < kWavetableSize; ++sample)
                {
                    const int source = (sample - shift + kWavetableSize * 2) % kWavetableSize;
                    (*shifted)[(size_t)sample] = (*current.waveform)[(size_t)source];
                }
                current.waveform = std::move(shifted);
                current.spectrum.reset();
                analyzeWavetableFrame(current);
            }
        }
        previousSelected = i;
    }
}

// 帧间能量平滑
void processWavetableEnergySmooth(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount)
{
    std::vector<int> selectedIndices;
    const int limit = std::min(frameCount, slot.frameCount);
    for(int i = 0; i < limit; ++i)
        if(frameSelected(selectedFrames, i, slot.frameCount))
            selectedIndices.push_back(i);
    if(selectedIndices.empty())
        return;

    std::vector<float> rmsValues(selectedIndices.size(), 0.0f);
    for(size_t i = 0; i < selectedIndices.size(); ++i)
    {
        auto &frame = slot.frames[(size_t)selectedIndices[i]];
        makeFrameWaveformWritable(frame);
        for(float sample : *frame.waveform)
            rmsValues[i] += sample * sample;
        rmsValues[i] = std::sqrt(rmsValues[i] / float(kWavetableSize));
    }

    const int window = std::min(5, int(selectedIndices.size()));
    std::vector<float> smoothed(rmsValues.size(), 0.0f);
    for(int i = 0; i < int(rmsValues.size()); ++i)
    {
        int count = 0;
        for(int offset = -window / 2; offset <= window / 2; ++offset)
        {
            const int neighbour = i + offset;
            if(neighbour < 0 || neighbour >= int(rmsValues.size()))
                continue;
            smoothed[(size_t)i] += rmsValues[(size_t)neighbour];
            ++count;
        }
        smoothed[(size_t)i] /= float(std::max(1, count));
    }

    for(size_t i = 0; i < selectedIndices.size(); ++i)
    {
        if(rmsValues[i] < 1.0e-6f || smoothed[i] < 1.0e-6f)
            continue;
        auto &frame = slot.frames[(size_t)selectedIndices[i]];
        const float scale = smoothed[i] / rmsValues[i];
        for(float &sample : *frame.waveform)
            sample = clampf(sample * scale, -1.0f, 1.0f);
        frame.spectrum.reset();
        analyzeWavetableFrame(frame);
    }
}

// 交叉淡化边界：对每帧首尾做 crossfade 消点击声
void processWavetableCrossfade(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount)
{
    constexpr int len = kWavetableSize / 16;
    for(int i = 0; i < frameCount; ++i)
    {
        if(!frameSelected(selectedFrames, i, slot.frameCount)) continue;
        auto &frm = slot.frames[(size_t)i];
        makeFrameWaveformWritable(frm);
        auto &w = *frm.waveform;
        for(int s = 0; s < len; ++s)
        {
            const float t = float(s) / float(len);
            const int pos = kWavetableSize - len + s;
            w[(size_t)pos] = w[(size_t)pos] * (1.0f - t) + w[(size_t)s] * t;
        }
        frm.spectrum.reset();
        analyzeWavetableFrame(frm);
    }
}

// 过零点对齐：在每帧中找最近的上升沿过零点，循环移位帧使其从那里开始
void processWavetableZeroAlign(WavetablePartialSlot &slot, const bool *selectedFrames, int frameCount)
{
    for(int fi = 0; fi < frameCount; ++fi)
    {
        if(!frameSelected(selectedFrames, fi, slot.frameCount)) continue;
        auto &frm = slot.frames[(size_t)fi];
        makeFrameWaveformWritable(frm);
        auto &w = *frm.waveform;

        // 把波形展开成 vector，用 findNearestZeroCrossing 搜索最优起始点
        std::vector<float> tmp(w.begin(), w.end());
        // 在整个周期范围内（搜索半径 = 半个周期）寻找最近上升沿过零点
        const size_t bestOff = findNearestZeroCrossing(tmp, 0, kWavetableSize / 2);
        if(bestOff > 0)
        {
            std::array<float, kWavetableSize> shifted;
            for(int s = 0; s < kWavetableSize; ++s)
                shifted[(size_t)s] = w[(size_t)((s + int(bestOff)) % kWavetableSize)];
            w = shifted;
        }
        frm.spectrum.reset();
        analyzeWavetableFrame(frm);
    }
}

// ---- end 手动处理 ----

void bakeWavetableSeed(const WavetableSeedParams &params, WavetableSeedRenderState &out)
{
    bakeWavetableSeed(params, 1, out);
}

void bakeWavetableSeed(const WavetableSeedParams &params, int sourceCount, WavetableSeedRenderState &out)
{
    refreshWavetableSeedRuntime(params, sourceCount, out);
    for(int i = 0; i < kMaxWavetablePartials; ++i)
    {
        const int metaSlot = metaSlotForSourcePartial(out.sourceCount, i);
        const auto &src = params.partials[(size_t)(metaSlot >= 0 ? metaSlot : i)];
        auto &dst = out.partials[(size_t)i];

        if(metaSlot >= 0)
        {
            bakePartialTableCache(src, dst);
        }
        else
        {
            dst.tables.reset();
            dst.mipTables.reset();
            dst.usesMetaWavetable = false;
            dst.frameCount = 1;
            dst.morph = 0.0f;
            dst.warpMode = WavetableWarpMode::None;
            dst.warpAmount = 0.0f;
        }
    }
}

void rebakeWavetableSeedMetaPartial(const WavetableSeedParams &params, int sourceCount, int metaIndex,
                                    WavetableSeedRenderState &out)
{
    if(metaIndex < 0 || metaIndex >= kEditableMetaPartials)
        return;
    refreshWavetableSeedRuntime(params, sourceCount, out);
    for(int i = 0; i < kMaxWavetablePartials; ++i)
    {
        const int metaSlot = metaSlotForSourcePartial(out.sourceCount, i);
        if(metaSlot != metaIndex)
            continue;
        bakePartialTableCache(params.partials[(size_t)metaIndex], out.partials[(size_t)i]);
    }
}

void refreshWavetableSeedRuntime(const WavetableSeedParams &params, int sourceCount, WavetableSeedRenderState &out)
{
    out.partialCount = std::clamp(params.partialCount, 1, kMaxWavetablePartials);
    out.sourceCount = sanitizeGeneratorSourceCount(sourceCount);
    for(int i = 0; i < kMaxWavetablePartials; ++i)
    {
        const int metaSlot = metaSlotForSourcePartial(out.sourceCount, i);
        const auto &src = params.partials[(size_t)(metaSlot >= 0 ? metaSlot : i)];
        auto &dst = out.partials[(size_t)i];
        const float n = float(i + 1);
        const float shapedNu = computeNuDirect(params.freqShape, n, params.inharmonicAmount);
        const float freqMul = shapedNu / std::max(1.0e-6f, n);
        dst.enabled = src.enabled && i < out.partialCount && src.amp > 0.0f;
        dst.usesMetaWavetable = metaSlot >= 0;
        dst.ratio = std::max(0.0f, src.ratio * freqMul);
        dst.amp = std::max(0.0f, src.amp);
        dst.phase = src.phase;
        dst.pan = clampf(src.pan, -1.0f, 1.0f);
        dst.frameCount = std::clamp(src.frameCount, 1, kMaxWavetableFrames);
        dst.morph = clampf(src.morph, 0.0f, 1.0f);
        dst.warpMode = src.warpMode;
        dst.warpAmount = clampf(src.warpAmount, -1.0f, 1.0f);
        if(metaSlot < 0)
        {
            dst.tables.reset();
            dst.mipTables.reset();
            dst.usesMetaWavetable = false;
            dst.frameCount = 1;
            dst.morph = 0.0f;
            dst.warpMode = WavetableWarpMode::None;
            dst.warpAmount = 0.0f;
        }
    }
}

} // namespace synth
