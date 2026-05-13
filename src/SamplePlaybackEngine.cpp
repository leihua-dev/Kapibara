#include "SamplePlaybackEngine.h"

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

void SamplePlaybackEngine::prepare(double sr)
{
    sampleRate_ = sr > 1.0 ? sr : 48000.0;
}

void SamplePlaybackEngine::setParams(const SamplePlaybackParams &p)
{
    params_ = p;
}

bool SamplePlaybackEngine::loadFile(const std::string &path)
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
    params_.filePath = path;
    return true;
}

float SamplePlaybackEngine::readSample(const std::vector<float> &buffer, float position) const
{
    if(validSamples_ < 2)
        return 0.0f;
    const float p = clampf(position, 0.0f, float(validSamples_ - 1));
    const int i0 = std::clamp((int)std::floor(p), 0, validSamples_ - 1);
    const int i1 = std::min(i0 + 1, validSamples_ - 1);
    const float frac = p - float(i0);
    return buffer[(size_t)i0] + (buffer[(size_t)i1] - buffer[(size_t)i0]) * frac;
}

void SamplePlaybackEngine::configureVoice(Voice &voice, int midiNote, float velocity)
{
    const int start = std::clamp((int)std::floor(clampf(params_.start01, 0.0f, 0.99f) * float(validSamples_ - 1)),
                                 0, std::max(0, validSamples_ - 2));
    const int end = std::clamp((int)std::ceil(clampf(params_.end01, params_.start01 + 0.01f, 1.0f)
                                              * float(validSamples_ - 1)),
                               start + 2, validSamples_);
    voice.active = true;
    voice.releasing = false;
    voice.note = midiNote;
    voice.velocity = clampf(velocity, 0.0f, 1.0f);
    voice.position = params_.reverse ? float(end - 1) : float(start);
    const float semis = float(midiNote) - params_.rootMidi + params_.pitchOffsetSemitones;
    voice.step = std::pow(2.0f, semis / 12.0f);
    voice.env = 0.0f;
}

void SamplePlaybackEngine::noteOn(int midiNote, float velocity)
{
    if(validSamples_ < 2)
        return;
    for(auto &voice : voices_)
    {
        if(!voice.active)
        {
            configureVoice(voice, midiNote, velocity);
            return;
        }
    }
    configureVoice(voices_[0], midiNote, velocity);
}

void SamplePlaybackEngine::noteOff(int midiNote)
{
    for(auto &voice : voices_)
        if(voice.active && voice.note == midiNote)
            voice.releasing = true;
}

void SamplePlaybackEngine::allNotesOff()
{
    for(auto &voice : voices_)
        if(voice.active)
            voice.releasing = true;
}

void SamplePlaybackEngine::process(float *left, float *right, int numSamples)
{
    if(validSamples_ < 2)
        return;
    const int start = std::clamp((int)std::floor(clampf(params_.start01, 0.0f, 0.99f) * float(validSamples_ - 1)),
                                 0, validSamples_ - 2);
    const int end = std::clamp((int)std::ceil(clampf(params_.end01, params_.start01 + 0.01f, 1.0f)
                                              * float(validSamples_ - 1)),
                               start + 2, validSamples_);
    const float attackInc = 1.0f / std::max(1.0f, params_.attackMs * 0.001f * float(sampleRate_));
    const float releaseInc = 1.0f / std::max(1.0f, params_.releaseMs * 0.001f * float(sampleRate_));
    for(int s = 0; s < numSamples; ++s)
    {
        float sumL = 0.0f;
        float sumR = 0.0f;
        for(auto &voice : voices_)
        {
            if(!voice.active) continue;
            if(voice.releasing)
                voice.env = std::max(0.0f, voice.env - releaseInc);
            else
                voice.env = std::min(1.0f, voice.env + attackInc);
            if(voice.env <= 0.0f && voice.releasing)
            {
                voice.active = false;
                continue;
            }
            sumL += readSample(leftBuffer_, voice.position) * voice.velocity * voice.env * params_.playbackGain;
            sumR += readSample(rightBuffer_, voice.position) * voice.velocity * voice.env * params_.playbackGain;
            voice.position += params_.reverse ? -voice.step : voice.step;
            if(params_.loopEnabled)
            {
                const float span = float(std::max(1, end - start));
                while(voice.position >= float(end)) voice.position -= span;
                while(voice.position < float(start)) voice.position += span;
            }
            else if(voice.position >= float(end) || voice.position < float(start))
            {
                voice.releasing = true;
            }
        }
        left[s] += sumL;
        right[s] += sumR;
    }
}

} // namespace synth
