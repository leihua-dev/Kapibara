#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace synth
{

struct SamplePlaybackParams
{
    std::string filePath;
    bool loopEnabled = false;
    bool reverse = false;
    float rootMidi = 60.0f;
    float start01 = 0.0f;
    float end01 = 1.0f;
    float playbackGain = 0.8f;
    float pitchOffsetSemitones = 0.0f;
    float attackMs = 2.0f;
    float releaseMs = 40.0f;
};

class SamplePlaybackEngine
{
  public:
    void prepare(double sampleRate);
    void setParams(const SamplePlaybackParams &p);
    SamplePlaybackParams getParams() const { return params_; }
    bool loadFile(const std::string &path);
    bool hasBuffer() const { return validSamples_ > 1; }

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    void process(float *left, float *right, int numSamples);

  private:
    struct Voice
    {
        bool active = false;
        bool releasing = false;
        int note = -1;
        float velocity = 0.0f;
        float position = 0.0f;
        float step = 1.0f;
        float env = 0.0f;
    };

    float readSample(const std::vector<float> &buffer, float position) const;
    void configureVoice(Voice &voice, int midiNote, float velocity);

    SamplePlaybackParams params_ {};
    double sampleRate_ = 48000.0;
    std::vector<float> leftBuffer_;
    std::vector<float> rightBuffer_;
    int validSamples_ = 0;
    std::array<Voice, 16> voices_ {};
};

} // namespace synth
