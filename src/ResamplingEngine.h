#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace synth
{

struct ResamplingEngineParams
{
    bool enabled = false;
    bool recording = false;
    bool playbackEnabled = false;
    bool loopEnabled = true;
    bool reverse = false;

    float recordSeconds = 8.0f;
    float dryWet = 1.0f;
    float playbackGain = 0.8f;
    float pitchSemitones = 0.0f;
    float sliceStart = 0.0f;
    float sliceEnd = 1.0f;
    int sliceCount = 1;
    int sliceRotate = 0;

    float granularAmount = 0.0f;
    float grainSizeMs = 80.0f;
    float stutterAmount = 0.0f;
    float stutterRateHz = 8.0f;

    std::string importedFilePath;
};

struct ResamplingDisplayState
{
    bool hasBuffer = false;
    bool recording = false;
    bool playing = false;
    float playhead01 = 0.0f;
    float recordhead01 = 0.0f;
    float durationSeconds = 0.0f;
    float currentSeconds = 0.0f;
    std::array<float, 256> waveform {};
};

class ResamplingEngine
{
  public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const ResamplingEngineParams &p);
    ResamplingEngineParams getParams() const { return params_; }

    bool loadBufferFromFile(const std::string &path);
    void clearBuffer();
    bool hasBuffer() const { return validSamples_ > 1; }
    int validSamples() const { return validSamples_; }
    ResamplingDisplayState getDisplayState() const;

    // Signal enters after the first FX chain. The engine can record that signal,
    // pass it through untouched, and/or blend frozen buffer playback back in.
    void process(float *left, float *right, int numSamples);

  private:
    float readSample(const std::vector<float> &buffer, float position) const;
    float mapSlicePosition(float position, int start, int end) const;
    void advancePlayhead(float step, int start, int end);
    void resizeRecordBuffer();

    ResamplingEngineParams params_ {};
    double sampleRate_ = 48000.0;
    std::vector<float> leftBuffer_;
    std::vector<float> rightBuffer_;
    int validSamples_ = 0;
    int recordWrite_ = 0;
    float playhead_ = 0.0f;
    int stutterCounter_ = 0;
    float stutterAnchor_ = 0.0f;
    bool wasRecording_ = false;
};

} // namespace synth
