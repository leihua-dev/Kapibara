#include "ResamplingEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>

namespace synth
{

namespace
{
inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
} // namespace

void ResamplingEngine::prepare(double sr)
{
    sampleRate_ = sr > 1.0 ? sr : 48000.0;
    resizeRecordBuffer();
    reset();
}

void ResamplingEngine::reset()
{
    playhead_ = 0.0f;
    stutterCounter_ = 0;
    stutterAnchor_ = 0.0f;
    wasRecording_ = params_.recording;
}

void ResamplingEngine::resizeRecordBuffer()
{
    const float sec = clampf(params_.recordSeconds, 0.25f, 60.0f);
    const int samples = std::max(2, int(std::round(sec * float(sampleRate_))));
    if((int)leftBuffer_.size() != samples)
    {
        leftBuffer_.assign((size_t)samples, 0.0f);
        rightBuffer_.assign((size_t)samples, 0.0f);
        validSamples_ = 0;
        recordWrite_ = 0;
        playhead_ = 0.0f;
    }
}

void ResamplingEngine::setParams(const ResamplingEngineParams &p)
{
    const bool recordRising = !params_.recording && p.recording;
    params_ = p;
    if(recordRising || leftBuffer_.empty() || validSamples_ <= 0)
        resizeRecordBuffer();
    if(recordRising)
    {
        validSamples_ = 0;
        recordWrite_ = 0;
        playhead_ = 0.0f;
        stutterCounter_ = 0;
        stutterAnchor_ = 0.0f;
    }
    wasRecording_ = params_.recording;
}

void ResamplingEngine::clearBuffer()
{
    std::fill(leftBuffer_.begin(), leftBuffer_.end(), 0.0f);
    std::fill(rightBuffer_.begin(), rightBuffer_.end(), 0.0f);
    validSamples_ = 0;
    recordWrite_ = 0;
    playhead_ = 0.0f;
}

bool ResamplingEngine::loadBufferFromFile(const std::string &path)
{
    juce::AudioFormatManager mgr;
    mgr.registerBasicFormats();
    juce::File file(path);
    if(!file.existsAsFile())
        return false;
    std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
    if(reader == nullptr || reader->lengthInSamples < 2)
        return false;

    const double ratio = sampleRate_ / std::max(1.0, reader->sampleRate);
    const int outputSamples = std::max(2, int(std::round(double(reader->lengthInSamples) * ratio)));
    leftBuffer_.assign((size_t)outputSamples, 0.0f);
    rightBuffer_.assign((size_t)outputSamples, 0.0f);

    juce::AudioBuffer<float> temp((int)std::max(1u, reader->numChannels),
                                  (int)reader->lengthInSamples);
    reader->read(&temp, 0, (int)reader->lengthInSamples, 0, true, true);

    const float *srcL = temp.getReadPointer(0);
    const float *srcR = temp.getNumChannels() > 1 ? temp.getReadPointer(1) : srcL;
    for(int i = 0; i < outputSamples; ++i)
    {
        const double srcPos = double(i) / ratio;
        const int i0 = std::clamp((int)std::floor(srcPos), 0, (int)reader->lengthInSamples - 1);
        const int i1 = std::min(i0 + 1, (int)reader->lengthInSamples - 1);
        const float frac = float(srcPos - double(i0));
        leftBuffer_[(size_t)i] = srcL[i0] + (srcL[i1] - srcL[i0]) * frac;
        rightBuffer_[(size_t)i] = srcR[i0] + (srcR[i1] - srcR[i0]) * frac;
    }

    validSamples_ = outputSamples;
    recordWrite_ = std::min(recordWrite_, validSamples_);
    playhead_ = 0.0f;
    params_.importedFilePath = path;
    return true;
}

ResamplingDisplayState ResamplingEngine::getDisplayState() const
{
    ResamplingDisplayState state;
    state.hasBuffer = validSamples_ > 1;
    state.recording = params_.recording;
    state.playing = params_.enabled && params_.playbackEnabled && state.hasBuffer;
    state.durationSeconds = state.hasBuffer ? float(validSamples_) / float(sampleRate_) : 0.0f;
    state.currentSeconds = state.hasBuffer ? playhead_ / float(sampleRate_) : 0.0f;
    state.playhead01 = state.hasBuffer ? clampf(playhead_ / float(std::max(1, validSamples_ - 1)), 0.0f, 1.0f) : 0.0f;
    state.recordhead01 = !leftBuffer_.empty()
                             ? clampf(float(recordWrite_) / float(std::max<size_t>(1, leftBuffer_.size() - 1)), 0.0f, 1.0f)
                             : 0.0f;
    if(state.hasBuffer)
    {
        for(size_t i = 0; i < state.waveform.size(); ++i)
        {
            const int start = int((double(i) / double(state.waveform.size())) * double(validSamples_));
            const int end = std::max(start + 1,
                                     int((double(i + 1) / double(state.waveform.size())) * double(validSamples_)));
            float peak = 0.0f;
            for(int s = start; s < std::min(end, validSamples_); ++s)
                peak = std::max(peak, 0.5f * (std::abs(leftBuffer_[(size_t)s]) + std::abs(rightBuffer_[(size_t)s])));
            state.waveform[i] = peak;
        }
    }
    return state;
}

float ResamplingEngine::readSample(const std::vector<float> &buffer, float position) const
{
    if(validSamples_ < 2 || buffer.empty())
        return 0.0f;
    const float p = clampf(position, 0.0f, float(validSamples_ - 1));
    const int i0 = std::clamp((int)std::floor(p), 0, validSamples_ - 1);
    const int i1 = std::min(i0 + 1, validSamples_ - 1);
    const float frac = p - float(i0);
    return buffer[(size_t)i0] + (buffer[(size_t)i1] - buffer[(size_t)i0]) * frac;
}

float ResamplingEngine::mapSlicePosition(float position, int start, int end) const
{
    const int len = std::max(2, end - start);
    const float local = clampf(position - float(start), 0.0f, float(len - 1));
    const int slices = std::clamp(params_.sliceCount, 1, 32);
    if(slices <= 1)
        return float(start) + local;

    const float sliceLen = float(len) / float(slices);
    const int slice = std::clamp((int)std::floor(local / std::max(1.0f, sliceLen)), 0, slices - 1);
    const int rotated = (slice + params_.sliceRotate % slices + slices) % slices;
    const float within = std::fmod(local, std::max(1.0f, sliceLen));
    return float(start) + float(rotated) * sliceLen + within;
}

void ResamplingEngine::advancePlayhead(float step, int start, int end)
{
    playhead_ += params_.reverse ? -step : step;
    const float lo = float(start);
    const float hi = float(std::max(start + 1, end - 1));
    if(params_.loopEnabled)
    {
        const float span = std::max(1.0f, hi - lo);
        while(playhead_ > hi) playhead_ -= span;
        while(playhead_ < lo) playhead_ += span;
    }
    else
    {
        playhead_ = clampf(playhead_, lo, hi);
    }
}

void ResamplingEngine::process(float *left, float *right, int numSamples)
{
    if(numSamples <= 0)
        return;

    if(params_.recording && !leftBuffer_.empty())
    {
        for(int i = 0; i < numSamples && recordWrite_ < (int)leftBuffer_.size(); ++i)
        {
            leftBuffer_[(size_t)recordWrite_] = left[i];
            rightBuffer_[(size_t)recordWrite_] = right[i];
            ++recordWrite_;
            validSamples_ = std::max(validSamples_, recordWrite_);
        }
    }

    if(!params_.enabled || !params_.playbackEnabled || validSamples_ < 2)
        return;

    const int start = std::clamp((int)std::floor(clampf(params_.sliceStart, 0.0f, 0.99f) * float(validSamples_ - 1)),
                                 0, validSamples_ - 2);
    const int end = std::clamp((int)std::ceil(clampf(params_.sliceEnd, params_.sliceStart + 0.01f, 1.0f)
                                              * float(validSamples_ - 1)),
                               start + 2, validSamples_);
    if(playhead_ < float(start) || playhead_ > float(end - 1))
        playhead_ = params_.reverse ? float(end - 1) : float(start);

    const float rate = std::pow(2.0f, params_.pitchSemitones / 12.0f);
    const float wet = clampf(params_.dryWet, 0.0f, 1.0f);
    const float gain = std::max(0.0f, params_.playbackGain);
    const int stutterPeriod = std::max(1, int(sampleRate_ / std::max(0.25f, params_.stutterRateHz)));
    const float grainOffset = std::max(1.0f, params_.grainSizeMs * 0.001f * float(sampleRate_));

    for(int i = 0; i < numSamples; ++i)
    {
        const float readPos = mapSlicePosition(playhead_, start, end);
        float wetL = readSample(leftBuffer_, readPos);
        float wetR = readSample(rightBuffer_, readPos);

        if(params_.granularAmount > 1e-4f)
        {
            const float osc = 0.5f + 0.5f * std::sin(0.00091f * float(stutterCounter_ + i + 1));
            const float altPos = clampf(readPos + (osc - 0.5f) * grainOffset, float(start), float(end - 1));
            const float amt = clampf(params_.granularAmount, 0.0f, 1.0f);
            wetL = wetL * (1.0f - amt) + readSample(leftBuffer_, altPos) * amt;
            wetR = wetR * (1.0f - amt) + readSample(rightBuffer_, altPos) * amt;
        }

        if(params_.stutterAmount > 1e-4f)
        {
            ++stutterCounter_;
            if(stutterCounter_ >= stutterPeriod)
            {
                stutterCounter_ = 0;
                stutterAnchor_ = playhead_;
            }
            if(stutterCounter_ < int(float(stutterPeriod) * clampf(params_.stutterAmount, 0.0f, 1.0f)))
                playhead_ = stutterAnchor_;
        }

        const float dryL = left[i];
        const float dryR = right[i];
        left[i] = dryL * (1.0f - wet) + wetL * gain * wet;
        right[i] = dryR * (1.0f - wet) + wetR * gain * wet;
        advancePlayhead(rate, start, end);
    }
}

} // namespace synth
