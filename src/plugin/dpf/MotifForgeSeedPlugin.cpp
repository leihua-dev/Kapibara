#include "MotifForgeSeedPlugin.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

START_NAMESPACE_DISTRHO

namespace
{
constexpr float kPi = 3.14159265358979323846f;
const char *kUserPresetPath = "presets/user_seed.mfpreset";
const char *kPresetDirectory = "presets";
const char *kWavetablePresetDirectory = "presets/wavetables";
const char *kPresetExtension = ".mfpreset";

float clampf(float value, float lo, float hi)
{
    return std::max(lo, std::min(hi, value));
}

std::string cleanPresetName(const char *name)
{
    std::string out;
    if(name != nullptr)
    {
        for(const unsigned char ch : std::string(name))
        {
            if(std::isalnum(ch) || ch == '_' || ch == '-' || ch == ' ')
                out.push_back(char(ch));
        }
    }
    while(!out.empty() && out.front() == ' ')
        out.erase(out.begin());
    while(!out.empty() && out.back() == ' ')
        out.pop_back();
    if(out.empty())
        out = "user_seed";
    if(out.size() > 64)
        out.resize(64);
    return out;
}

std::filesystem::path presetPathForName(const char *name)
{
    return std::filesystem::path(kPresetDirectory) / (cleanPresetName(name) + kPresetExtension);
}
} // namespace

MotifForgeSeedPlugin::MotifForgeSeedPlugin()
    : Plugin(0, 0, 0)
{
    prepareCore(48000.0);
}

const char *MotifForgeSeedPlugin::getLabel() const
{
    return "motifforge_seed";
}

const char *MotifForgeSeedPlugin::getDescription() const
{
    return "Seed-only wavetable synth built with DPF UI, NanoVG and OpenGL3.";
}

const char *MotifForgeSeedPlugin::getMaker() const
{
    return "MotifForge";
}

const char *MotifForgeSeedPlugin::getHomePage() const
{
    return "https://local/motifforge-seed";
}

const char *MotifForgeSeedPlugin::getLicense() const
{
    return "Proprietary";
}

uint32_t MotifForgeSeedPlugin::getVersion() const
{
    return d_version(0, 5, 0);
}

void MotifForgeSeedPlugin::initAudioPort(bool input, uint32_t index, AudioPort &port)
{
    Plugin::initAudioPort(input, index, port);
    port.groupId = kPortGroupStereo;
}

void MotifForgeSeedPlugin::activate()
{
    prepareCore(getSampleRate());
}

void MotifForgeSeedPlugin::deactivate()
{
    core_.allNotesOff();
}

void MotifForgeSeedPlugin::sampleRateChanged(double newSampleRate)
{
    prepareCore(newSampleRate);
}

void MotifForgeSeedPlugin::prepareCore(double rate)
{
    if(rate <= 0.0)
        rate = 48000.0;

    sampleRate_.store(rate, std::memory_order_relaxed);
    core_.prepare(rate);
    prepared_.store(true, std::memory_order_release);
}

void MotifForgeSeedPlugin::syncPartialCountEnabledState(synth::WavetableSeedParams &params)
{
    params.partialCount = std::clamp(params.partialCount, 1, synth::kMaxWavetablePartials);
    for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
        params.partials[(size_t)i].enabled = i < params.partialCount;
}

void MotifForgeSeedPlugin::render(float **outputs, uint32_t frames)
{
    if(!prepared_.load(std::memory_order_acquire))
        prepareCore(getSampleRate());

    if(outputs == nullptr || outputs[0] == nullptr || outputs[1] == nullptr)
        return;

    core_.renderBlock(outputs[0], outputs[1], static_cast<int>(frames));
}

void MotifForgeSeedPlugin::run(const float **inputs, float **outputs, uint32_t frames, const MidiEvent *midiEvents,
                               uint32_t midiEventCount)
{
    for(uint32_t i = 0; i < midiEventCount; ++i)
        handleMidi(midiEvents[i]);

    (void)inputs;
    render(outputs, frames);
}

void MotifForgeSeedPlugin::handleMidi(const MidiEvent &event)
{
    if(event.size < 1)
        return;

    const uint8_t status = event.data[0] & 0xf0u;
    const uint8_t note = event.size > 1 ? event.data[1] : 0u;
    const uint8_t velocity = event.size > 2 ? event.data[2] : 0u;

    if(status == 0x90u)
    {
        if(velocity == 0)
            core_.noteOff(note);
        else
            core_.noteOn(note, float(velocity) / 127.0f);
        return;
    }

    if(status == 0x80u)
    {
        core_.noteOff(note);
        return;
    }

    if(status == 0xb0u && event.size > 2 && (note == 120u || note == 123u))
        core_.allNotesOff();
}

void MotifForgeSeedPlugin::updateGlobalGain(float value)
{
    core_.setGlobalGain(clampf(value, 0.0f, 1.0f));
}

void MotifForgeSeedPlugin::updateAdsr(float attack, float decay, float sustain, float release, float curve)
{
    synth::AdsrParams params = core_.getGlobalAdsr();
    params.attack = clampf(attack, 0.0f, 5.0f);
    params.decay = clampf(decay, 0.0f, 5.0f);
    params.sustain = clampf(sustain, 0.0f, 1.0f);
    params.release = clampf(release, 0.0f, 8.0f);
    params.curve = clampf(curve, 0.0f, 1.0f);
    core_.setGlobalAdsr(params);
}

void MotifForgeSeedPlugin::updateGenerator(int partialCount, float inharmonic, int freqShape, int sourceCount,
                                           int unisonVoices, float detune, float width, float phaseSpread)
{
    const int nextSourceCount = synth::sanitizeGeneratorSourceCount(sourceCount);
    const int nextPartialCount = std::clamp(partialCount, 1, synth::kMaxWavetablePartials);
    const auto nextFreqShape = static_cast<synth::FreqShape>(std::clamp(freqShape, 0, 2));
    const float nextInharmonic = clampf(inharmonic, 0.0f, 1.0f);
    synth::UnisonParams nextUnison;
    nextUnison.voices = std::clamp(unisonVoices, 1, 16);
    nextUnison.detuneCents = clampf(detune, 0.0f, 80.0f);
    nextUnison.widthStereo = clampf(width, 0.0f, 1.0f);
    nextUnison.phaseSpread = clampf(phaseSpread, 0.0f, 1.0f);
    core_.setGeneratorBasicParams(nextPartialCount, nextFreqShape, nextInharmonic, nextSourceCount, nextUnison);
}

void MotifForgeSeedPlugin::updateGeneratorSource(int index, const synth::GeneratorSourceParams &source)
{
    if(index < 0 || index >= 8)
        return;

    auto clean = source;
    clean.gain = clampf(clean.gain, 0.0f, 2.0f);
    clean.pan = clampf(clean.pan, -1.0f, 1.0f);
    clean.filter.cutoffHz = clampf(clean.filter.cutoffHz, 20.0f, 20000.0f);
    clean.filter.resonance = clampf(clean.filter.resonance, 0.0f, 0.95f);
    clean.filter.drive = clampf(clean.filter.drive, 0.1f, 8.0f);
    clean.filter.feedback = clampf(clean.filter.feedback, 0.0f, 0.95f);
    clean.filter.mix = clampf(clean.filter.mix, 0.0f, 1.0f);
    const int topology = std::clamp(int(clean.filter.topology), 0, int(synth::SourceFilterTopology::FeedbackLadder));
    clean.filter.topology = static_cast<synth::SourceFilterTopology>(topology);
    core_.setGeneratorSourceParams(index, clean);
}

uint32_t MotifForgeSeedPlugin::addSourceTrack(synth::SourceTrackType type, const char *name)
{
    return core_.addSourceTrack(type, name != nullptr ? name : synth::sourceTrackTypeName(type));
}

void MotifForgeSeedPlugin::removeSourceTrack(uint32_t trackId)
{
    core_.allNotesOff();
    core_.removeSourceTrack(trackId);
}

void MotifForgeSeedPlugin::moveSourceTrack(uint32_t trackId, int newIndex)
{
    core_.moveSourceTrack(trackId, newIndex);
}

void MotifForgeSeedPlugin::updateSourceTrack(uint32_t trackId, const synth::SourceTrackParams &track)
{
    auto clean = track;
    clean.gain = clampf(clean.gain, 0.0f, 2.0f);
    clean.pan = clampf(clean.pan, -1.0f, 1.0f);
    clean.send = clampf(clean.send, 0.0f, 1.0f);
    clean.ampEnvIndex = std::clamp(clean.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
    clean.unison.voices = std::clamp(clean.unison.voices, 1, synth::kMaxUnison);
    clean.unison.detuneCents = clampf(clean.unison.detuneCents, 0.0f, 80.0f);
    clean.unison.widthStereo = clampf(clean.unison.widthStereo, 0.0f, 1.0f);
    clean.unison.phaseSpread = clampf(clean.unison.phaseSpread, 0.0f, 1.0f);
    clean.ampEnvelope.attack = clampf(clean.ampEnvelope.attack, 0.0f, 5.0f);
    clean.ampEnvelope.decay = clampf(clean.ampEnvelope.decay, 0.0f, 5.0f);
    clean.ampEnvelope.sustain = clampf(clean.ampEnvelope.sustain, 0.0f, 1.0f);
    clean.ampEnvelope.release = clampf(clean.ampEnvelope.release, 0.0f, 8.0f);
    clean.ampEnvelope.curve = clampf(clean.ampEnvelope.curve, 0.0f, 1.0f);
    clean.partialBank.partialCount = std::clamp(clean.partialBank.partialCount, 1, synth::kMaxWavetablePartials);
    clean.metaOsc.frameCount = std::clamp(clean.metaOsc.frameCount, 1, synth::kMaxWavetableFrames);
    clean.pulseWidth = clampf(clean.pulseWidth, 0.05f, 0.95f);
    clean.subLevel = clampf(clean.subLevel, 0.0f, 1.0f);
    clean.noiseColor = clampf(clean.noiseColor, 0.0f, 1.0f);
    core_.setSourceTrack(trackId, clean);
}

void MotifForgeSeedPlugin::updateSourceTrackMorphOnly(uint32_t trackId, float morph)
{
    core_.setSourceTrackMorphOnly(trackId, clampf(morph, 0.0f, 1.0f));
}

void MotifForgeSeedPlugin::updateSourceTracks(const std::vector<synth::SourceTrackParams> &tracks)
{
    core_.setSourceTracks(tracks);
}

void MotifForgeSeedPlugin::setPartialEnabled(int index, bool enabled)
{
    if(index < 0 || index >= synth::kMaxWavetablePartials)
        return;
    core_.setPartialEnabled(index, enabled);
}

void MotifForgeSeedPlugin::setPartialAmp(int index, float amp)
{
    if(index < 0 || index >= synth::kMaxWavetablePartials)
        return;
    core_.setPartialAmp(index, clampf(amp, 0.0f, 1.0f));
}

void MotifForgeSeedPlugin::setPartialRatio(int index, float ratio)
{
    if(index < 0 || index >= synth::kMaxWavetablePartials)
        return;
    core_.setPartialRatio(index, clampf(ratio, 0.01f, 128.0f));
}

void MotifForgeSeedPlugin::updatePartialRuntime(int index, bool enabled, float ratio, float amp, float phase, float pan,
                                                float morph, int warpMode, float warpAmount)
{
    if(index < 0 || index >= synth::kEditableMetaPartials)
        return;
    const auto cleanWarp = static_cast<synth::WavetableWarpMode>(std::clamp(warpMode, 0, 3));
    core_.setMetaPartialRuntime(index, enabled, clampf(ratio, 0.01f, 128.0f), clampf(amp, 0.0f, 1.0f),
                                clampf(phase, -kPi, kPi), clampf(pan, -1.0f, 1.0f), clampf(morph, 0.0f, 1.0f),
                                cleanWarp, clampf(warpAmount, -1.0f, 1.0f));
}

void MotifForgeSeedPlugin::updatePartialSlot(int index, const synth::WavetablePartialSlot &slot)
{
    if(index < 0 || index >= synth::kEditableMetaPartials)
        return;

    auto clean = slot;
    clean.ratio = clampf(clean.ratio, 0.01f, 128.0f);
    clean.amp = clampf(clean.amp, 0.0f, 1.0f);
    clean.phase = clampf(clean.phase, -kPi, kPi);
    clean.pan = clampf(clean.pan, -1.0f, 1.0f);
    clean.frameCount = std::clamp(clean.frameCount, 1, synth::kMaxWavetableFrames);
    clean.morph = clampf(clean.morph, 0.0f, 1.0f);
    clean.warpAmount = clampf(clean.warpAmount, -1.0f, 1.0f);
    core_.setMetaPartialSlot(index, clean);
}

bool MotifForgeSeedPlugin::loadWavetableFrame(int partialIndex, int frameIndex, const char *path)
{
    if(partialIndex < 0 || partialIndex >= synth::kEditableMetaPartials || frameIndex < 0
       || frameIndex >= synth::kMaxWavetableFrames || path == nullptr || path[0] == '\0')
        return false;

    return core_.loadWavetableFrame(partialIndex, frameIndex, path);
}

void MotifForgeSeedPlugin::previewNoteOn(int midiNote, float velocity)
{
    core_.noteOn(std::clamp(midiNote, 0, 127), clampf(velocity, 0.0f, 1.0f));
}

void MotifForgeSeedPlugin::previewNoteOff(int midiNote)
{
    core_.noteOff(std::clamp(midiNote, 0, 127));
}

void MotifForgeSeedPlugin::panic()
{
    core_.allNotesOff();
}

std::vector<std::string> MotifForgeSeedPlugin::presetNames() const
{
    std::vector<std::string> names;
    std::error_code ec;
    if(!std::filesystem::exists(kPresetDirectory, ec))
        return names;
    for(const auto &entry : std::filesystem::directory_iterator(kPresetDirectory, ec))
    {
        if(ec)
            break;
        if(!entry.is_regular_file(ec))
            continue;
        const auto path = entry.path();
        if(path.extension() == kPresetExtension)
            names.push_back(path.stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<WavetablePresetEntry> MotifForgeSeedPlugin::wavetablePresetEntries() const
{
    std::vector<WavetablePresetEntry> entries;
    std::error_code ec;
    const std::filesystem::path root(kWavetablePresetDirectory);
    if(!std::filesystem::exists(root, ec))
        return entries;
    for(const auto &entry : std::filesystem::recursive_directory_iterator(root, ec))
    {
        if(ec)
            break;
        if(!entry.is_regular_file(ec))
            continue;
        auto extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char ch) { return char(std::tolower(ch)); });
        if(extension != ".wav")
            continue;
        auto relative = std::filesystem::relative(entry.path(), root, ec);
        if(ec)
        {
            ec.clear();
            relative = entry.path().filename();
        }
        relative.replace_extension();
        entries.push_back({ relative.generic_string(), entry.path().string() });
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
    return entries;
}

bool MotifForgeSeedPlugin::saveUserPreset(const char *name)
{
    std::error_code ec;
    std::filesystem::create_directories(kPresetDirectory, ec);
    const std::string presetName = cleanPresetName(name);
    std::ofstream out(presetPathForName(presetName.c_str()), std::ios::trunc);
    if(!out)
    {
        presetStatus_ = "Save failed";
        return false;
    }

    const auto gen = core_.getGeneratorParams();
    const auto adsr = core_.getGlobalAdsr();
    out << "MotifForgeSeedPreset 5 " << synth::kMaxWavetableHarmonics << "\n";
    out << "gain " << core_.getGlobalGain() << "\n";
    out << "adsr " << adsr.attack << ' ' << adsr.decay << ' ' << adsr.sustain << ' ' << adsr.release << ' ' << adsr.curve << "\n";
    for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
    {
        const auto env = core_.getAmpEnvParams(i);
        out << "ampenv " << i << ' ' << env.attack << ' ' << env.decay << ' ' << env.sustain << ' '
            << env.release << ' ' << env.curve << "\n";
    }
    out << "generator " << gen.wavetableSeed.partialCount << ' ' << int(gen.wavetableSeed.freqShape) << ' '
        << gen.wavetableSeed.inharmonicAmount << ' ' << gen.unison.voices << ' ' << gen.unison.detuneCents << ' '
        << gen.unison.widthStereo << ' ' << gen.unison.phaseSpread << ' ' << gen.sourceCount << "\n";

    for(int i = 0; i < 8; ++i)
    {
        const auto &source = gen.sources[(size_t)i];
        const auto &filter = source.filter;
        out << "source " << i << ' ' << source.gain << ' ' << source.pan << ' ' << filter.enabled << ' '
            << int(filter.topology) << ' ' << filter.cutoffHz << ' ' << filter.resonance << ' ' << filter.drive
            << ' ' << filter.feedback << ' ' << filter.mix << "\n";
    }

    for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
    {
        const auto &slot = gen.wavetableSeed.partials[(size_t)i];
        out << "partial " << i << ' ' << slot.enabled << ' ' << slot.ratio << ' ' << slot.amp << ' '
            << slot.phase << ' ' << slot.pan << ' ' << slot.frameCount << ' ' << slot.morph << ' '
            << int(slot.warpMode) << ' ' << slot.warpAmount << "\n";
        if(i < synth::kEditableMetaPartials)
        {
            for(int f = 0; f < slot.frameCount; ++f)
            {
                out << "frame " << i << ' ' << f;
                const auto &frame = slot.frames[(size_t)f];
                for(const auto &h : frame.harmonics)
                    out << ' ' << h.ratio << ' ' << h.amp << ' ' << h.phase;
                out << "\n";
            }
        }
    }

    presetStatus_ = "Saved " + presetName;
    return true;
}

bool MotifForgeSeedPlugin::loadUserPreset(const char *name)
{
    const std::string presetName = cleanPresetName(name);
    std::ifstream in(presetPathForName(presetName.c_str()));
    if(!in && name == nullptr)
        in.open(kUserPresetPath);
    if(!in)
    {
        presetStatus_ = "Load failed";
        return false;
    }

    std::string tag;
    int version = 0;
    int harmonicCount = 16;
    in >> tag >> version;
    if(tag != "MotifForgeSeedPreset")
    {
        presetStatus_ = "Bad preset";
        return false;
    }
    if(version >= 2)
        in >> harmonicCount;
    harmonicCount = std::clamp(harmonicCount, 1, synth::kMaxWavetableHarmonics);

    auto gen = core_.getGeneratorParams();
    auto adsr = core_.getGlobalAdsr();
    float gain = core_.getGlobalGain();

    while(in >> tag)
    {
        if(tag == "gain")
        {
            in >> gain;
        }
        else if(tag == "adsr")
        {
            in >> adsr.attack >> adsr.decay >> adsr.sustain >> adsr.release >> adsr.curve;
        }
        else if(tag == "ampenv")
        {
            int index = 0;
            synth::AdsrParams env;
            in >> index >> env.attack >> env.decay >> env.sustain >> env.release >> env.curve;
            if(index < 0 || index >= synth::kMaxAmpEnvs)
                return false;
            core_.setAmpEnvParams(index, env);
        }
        else if(tag == "generator")
        {
            int shape = 0;
            in >> gen.wavetableSeed.partialCount >> shape >> gen.wavetableSeed.inharmonicAmount
               >> gen.unison.voices >> gen.unison.detuneCents >> gen.unison.widthStereo;
            if(version >= 3)
                in >> gen.unison.phaseSpread;
            if(version >= 4)
                in >> gen.sourceCount;
            gen.wavetableSeed.partialCount = std::clamp(gen.wavetableSeed.partialCount, 1, synth::kMaxWavetablePartials);
            gen.wavetableSeed.freqShape = static_cast<synth::FreqShape>(std::clamp(shape, 0, 2));
            gen.sourceCount = synth::sanitizeGeneratorSourceCount(gen.sourceCount);
        }
        else if(tag == "source")
        {
            int index = 0;
            int enabled = 0;
            int topology = 0;
            in >> index;
            if(index < 0 || index >= 8)
                return false;
            auto &source = gen.sources[(size_t)index];
            auto &filter = source.filter;
            in >> source.gain >> source.pan >> enabled >> topology >> filter.cutoffHz >> filter.resonance
               >> filter.drive >> filter.feedback >> filter.mix;
            source.gain = clampf(source.gain, 0.0f, 2.0f);
            source.pan = clampf(source.pan, -1.0f, 1.0f);
            filter.enabled = enabled != 0;
            filter.topology = static_cast<synth::SourceFilterTopology>(
                std::clamp(topology, 0, int(synth::SourceFilterTopology::FeedbackLadder)));
            filter.cutoffHz = clampf(filter.cutoffHz, 20.0f, 20000.0f);
            filter.resonance = clampf(filter.resonance, 0.0f, 0.95f);
            filter.drive = clampf(filter.drive, 0.1f, 8.0f);
            filter.feedback = clampf(filter.feedback, 0.0f, 0.95f);
            filter.mix = clampf(filter.mix, 0.0f, 1.0f);
        }
        else if(tag == "partial")
        {
            int index = 0;
            int enabled = 0;
            int warp = 0;
            in >> index;
            if(index < 0 || index >= synth::kMaxWavetablePartials)
                return false;
            auto &slot = gen.wavetableSeed.partials[(size_t)index];
            in >> enabled >> slot.ratio >> slot.amp >> slot.phase >> slot.pan >> slot.frameCount
               >> slot.morph >> warp >> slot.warpAmount;
            slot.enabled = enabled != 0;
            slot.frameCount = std::clamp(slot.frameCount, 1, synth::kMaxWavetableFrames);
            slot.warpMode = static_cast<synth::WavetableWarpMode>(std::clamp(warp, 0, 3));
        }
        else if(tag == "frame")
        {
            int partial = 0;
            int frameIndex = 0;
            in >> partial >> frameIndex;
            if(partial < 0 || partial >= synth::kEditableMetaPartials || frameIndex < 0 || frameIndex >= synth::kMaxWavetableFrames)
                return false;
            auto &frame = gen.wavetableSeed.partials[(size_t)partial].frames[(size_t)frameIndex];
            frame.useImportedWaveform = false;
            frame.waveform.reset();
            frame.spectrum.reset();
            for(auto &h : frame.harmonics)
                h = {};
            for(int h = 0; h < harmonicCount; ++h)
                in >> frame.harmonics[(size_t)h].ratio >> frame.harmonics[(size_t)h].amp >> frame.harmonics[(size_t)h].phase;
        }
        else
        {
            presetStatus_ = "Bad preset";
            return false;
        }
    }

    core_.allNotesOff();
    core_.setGlobalGain(clampf(gain, 0.0f, 2.0f));
    core_.setGlobalAdsr(adsr);
    core_.setGeneratorParams(gen);
    presetStatus_ = "Loaded " + presetName;
    return true;
}

bool MotifForgeSeedPlugin::deleteUserPreset(const char *name)
{
    const std::string presetName = cleanPresetName(name);
    std::error_code ec;
    const bool removed = std::filesystem::remove(presetPathForName(presetName.c_str()), ec);
    if(ec || !removed)
    {
        presetStatus_ = "Delete failed";
        return false;
    }
    presetStatus_ = "Deleted " + presetName;
    return true;
}

void MotifForgeSeedPlugin::resetUserPreset()
{
    core_.allNotesOff();
    synth::SourceGenParams gen {};
    synth::AdsrParams adsr {};
    core_.setGeneratorParams(gen);
    core_.setGlobalAdsr(adsr);
    for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
    {
        synth::AdsrParams ampEnv;
        ampEnv.sustain = 1.0f;
        core_.setAmpEnvParams(i, ampEnv);
    }
    core_.setGlobalGain(0.30f);
    presetStatus_ = "Default seed";
}

const char *MotifForgeSeedPlugin::presetStatus() const
{
    return presetStatus_.c_str();
}

synth::SourceGenParams MotifForgeSeedPlugin::generatorParams() const
{
    return core_.getGeneratorParams();
}

synth::AdsrParams MotifForgeSeedPlugin::adsrParams() const
{
    return core_.getGlobalAdsr();
}

synth::AdsrParams MotifForgeSeedPlugin::ampEnvParams(int index) const
{
    return core_.getAmpEnvParams(index);
}

synth::OperatorChain MotifForgeSeedPlugin::operatorChain() const
{
    return core_.getOperatorChain();
}

synth::LfoParams MotifForgeSeedPlugin::lfoParams(int index) const
{
    return core_.getLfoParams(index);
}

synth::MatrixEnvParams MotifForgeSeedPlugin::matrixEnvParams(int index) const
{
    return core_.getMatrixEnvParams(index);
}

synth::MatrixRule MotifForgeSeedPlugin::matrixRule(int index) const
{
    return core_.getMatrixRule(index);
}

synth::ChaosParams MotifForgeSeedPlugin::chaosParams() const
{
    return core_.getChaosParams();
}

synth::ShapeSourceParams MotifForgeSeedPlugin::shapeSourceParams() const
{
    return core_.getShapeSourceParams();
}

synth::EffectsChainParams MotifForgeSeedPlugin::effectsParams() const
{
    return core_.getEffectsParams();
}

float MotifForgeSeedPlugin::globalGain() const
{
    return core_.getGlobalGain();
}

int MotifForgeSeedPlugin::activeVoiceCount() const
{
    return core_.getActiveVoiceCount();
}

float MotifForgeSeedPlugin::sourceLiveMorph(int trackIndex) const
{
    return core_.getLiveTrackMorph(trackIndex);
}

void MotifForgeSeedPlugin::updateLfo(int index, const synth::LfoParams &params)
{
    core_.setLfoParams(index, params);
}

void MotifForgeSeedPlugin::updateMatrixEnv(int index, const synth::MatrixEnvParams &params)
{
    core_.setMatrixEnvParams(index, params);
}

void MotifForgeSeedPlugin::updateAmpEnv(int index, const synth::AdsrParams &params)
{
    synth::AdsrParams clean = params;
    clean.attack = clampf(clean.attack, 0.0f, 5.0f);
    clean.decay = clampf(clean.decay, 0.0f, 5.0f);
    clean.sustain = clampf(clean.sustain, 0.0f, 1.0f);
    clean.release = clampf(clean.release, 0.0f, 8.0f);
    clean.curve = clampf(clean.curve, 0.0f, 1.0f);
    core_.setAmpEnvParams(index, clean);
}

void MotifForgeSeedPlugin::updateMatrixRule(int index, const synth::MatrixRule &rule)
{
    core_.setMatrixRule(index, rule);
}

void MotifForgeSeedPlugin::updateChaos(const synth::ChaosParams &params)
{
    core_.setChaosParams(params);
}

void MotifForgeSeedPlugin::updateShapeSource(const synth::ShapeSourceParams &params)
{
    core_.setShapeSourceParams(params);
}

void MotifForgeSeedPlugin::updateEffects(const synth::EffectsChainParams &params)
{
    core_.setEffectsParams(params);
}

void MotifForgeSeedPlugin::updateOperatorChain(const synth::OperatorChain &chain)
{
    core_.setOperatorChain(chain);
}

void MotifForgeSeedPlugin::updateSourceGroups(const std::vector<synth::SourceGroupDef> &groups)
{
    core_.setSourceGroups(groups);
}

Plugin *createPlugin()
{
    return new MotifForgeSeedPlugin();
}

END_NAMESPACE_DISTRHO
