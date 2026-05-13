#include "SynthCore.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>

namespace synth
{

SynthCore::SynthCore()
{
    initFrameDefaults(source.frame);
    initTimelineDefaults(source.timeline);
    regenerateFrameNoLock();
    publishSnapshotNoLock();
}

void SynthCore::prepare(double sr)
{
    sampleRate = sr > 1.0 ? sr : 48000.0;
    matrix.prepare(sampleRate);
    effects.prepare(sampleRate);
    resampling.prepare(sampleRate);
    postResampleEffects.prepare(sampleRate);
    samplePlayback.prepare(sampleRate);
    for(auto &v : voices)
        v.prepare(sampleRate);
    controlSamplesLeft = 0;
    auto snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);
    if(snap)
    {
        matrix.setParams(snap->lfoParams, snap->matrixRules, snap->chaosParams, snap->shapeSourceParams);
        effects.setParams(snap->effectsParams);
        resampling.setParams(snap->resamplingParams);
        postResampleEffects.setParams(snap->postResampleEffectsParams);
    }
}

void SynthCore::regenerateFrameNoLock()
{
    initTimelineDefaults(source.timeline);
    generator.generateTimeline(source.gen, source.timeline);
    source.chain.apply(source.timeline);
    if(source.gen.type == GeneratorType::FunctionalSampleSource)
    {
        // The audio timeline must start at the true onset, but the inspector
        // wants a representative partial snapshot instead of an all-zero
        // birth-gated t=0 frame.
        generator.generate(source.gen, source.frame);
        auto inspectorTimeline = makeStaticTimeline(source.frame);
        source.chain.apply(inspectorTimeline);
        source.frame = inspectorTimeline.frames[0];
    }
    else
    {
        source.frame = sampleTimeline(source.timeline, 0.0f);
    }
}

void SynthCore::publishSnapshotNoLock()
{
    auto snap = std::make_shared<RenderSnapshot>();
    snap->timeline = source.timeline;
    snap->frame = source.frame;
    snap->globalAdsr = globalAdsr;
    snap->unison = source.gen.unison;
    snap->globalGain = globalGain;
    snap->lfoParams = lfoParams;
    snap->matrixRules = matrixRules;
    snap->chaosParams = chaosParams;
    snap->shapeSourceParams = shapeSourceParams;
    snap->effectsParams = effectsParams;
    snap->resamplingParams = resamplingParams;
    snap->postResampleEffectsParams = postResampleEffectsParams;
    snap->samplePlaybackParams = source.gen.samplePlayback;
    std::atomic_store_explicit(&renderSnapshot,
                               std::shared_ptr<const RenderSnapshot>(snap),
                               std::memory_order_release);
}

// -----------------------------------------------------------------------------
// MIDI event queue
// -----------------------------------------------------------------------------
void SynthCore::pushEvent(const MidiEvent &e)
{
    int w = eventWrite_.load(std::memory_order_relaxed);
    int next = (w + 1) % kEventQueueSize;
    if(next == eventRead_.load(std::memory_order_acquire))
        return; // full, drop
    events_[(size_t)w] = e;
    eventWrite_.store(next, std::memory_order_release);
}

bool SynthCore::popEvent(MidiEvent &out)
{
    int r = eventRead_.load(std::memory_order_relaxed);
    if(r == eventWrite_.load(std::memory_order_acquire))
        return false;
    out = events_[(size_t)r];
    eventRead_.store((r + 1) % kEventQueueSize, std::memory_order_release);
    return true;
}

void SynthCore::noteOn(int midiNote, float velocity)
{
    pushEvent({0, midiNote, velocity});
}
void SynthCore::noteOff(int midiNote) { pushEvent({1, midiNote, 0.0f}); }
void SynthCore::allNotesOff() { pushEvent({2, 0, 0.0f}); }

// -----------------------------------------------------------------------------
// Source / Operator
// -----------------------------------------------------------------------------
void SynthCore::setGeneratorParams(const SourceGenParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    source.gen = p;
    samplePlayback.setParams(source.gen.samplePlayback);
    regenerateFrameNoLock();
    publishSnapshotNoLock();
}
SourceGenParams SynthCore::getGeneratorParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return source.gen;
}
void SynthCore::setOperatorChain(const OperatorChain &c)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    source.chain = c;
    regenerateFrameNoLock();
    publishSnapshotNoLock();
}
OperatorChain SynthCore::getOperatorChain() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return source.chain;
}
// -----------------------------------------------------------------------------
// Performance / Matrix params
// -----------------------------------------------------------------------------
void SynthCore::setGlobalAdsr(const GlobalAdsrParams &a)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    globalAdsr = a;
    publishSnapshotNoLock();
}
GlobalAdsrParams SynthCore::getGlobalAdsr() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return globalAdsr;
}
void SynthCore::setGlobalGain(float g)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    globalGain = std::clamp(g, 0.0f, 1.0f);
    publishSnapshotNoLock();
}
float SynthCore::getGlobalGain() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return globalGain;
}

void SynthCore::setLfoParams(int idx, const LfoParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(idx >= 0 && idx < kMaxLfos)
    {
        lfoParams[(size_t)idx] = p;
        publishSnapshotNoLock();
    }
}
LfoParams SynthCore::getLfoParams(int idx) const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return (idx >= 0 && idx < kMaxLfos) ? lfoParams[(size_t)idx] : LfoParams {};
}
void SynthCore::setMatrixRule(int idx, const MatrixRule &r)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(idx >= 0 && idx < kMaxMatrixRules)
    {
        matrixRules[(size_t)idx] = r;
        publishSnapshotNoLock();
    }
}
MatrixRule SynthCore::getMatrixRule(int idx) const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return (idx >= 0 && idx < kMaxMatrixRules) ? matrixRules[(size_t)idx] : MatrixRule {};
}
void SynthCore::setChaosParams(const ChaosParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    chaosParams = p;
    publishSnapshotNoLock();
}
ChaosParams SynthCore::getChaosParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return chaosParams;
}
void SynthCore::setShapeSourceParams(const ShapeSourceParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    shapeSourceParams = p;
    publishSnapshotNoLock();
}
ShapeSourceParams SynthCore::getShapeSourceParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return shapeSourceParams;
}
void SynthCore::setEffectsParams(const EffectsChainParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    effectsParams = p;
    publishSnapshotNoLock();
}
EffectsChainParams SynthCore::getEffectsParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return effectsParams;
}

void SynthCore::setResamplingParams(const ResamplingEngineParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    resamplingParams = p;
    publishSnapshotNoLock();
}

ResamplingEngineParams SynthCore::getResamplingParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return resamplingParams;
}

bool SynthCore::importResampleBuffer(const std::string &path)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(!resampling.loadBufferFromFile(path))
        return false;
    resamplingParams.importedFilePath = path;
    publishSnapshotNoLock();
    return true;
}

void SynthCore::clearResampleBuffer()
{
    std::lock_guard<std::mutex> lock(paramMutex);
    resampling.clearBuffer();
}

bool SynthCore::hasResampleBuffer() const
{
    return resampling.hasBuffer();
}

ResamplingDisplayState SynthCore::getResamplingDisplayState() const
{
    return resampling.getDisplayState();
}

bool SynthCore::importSamplePlaybackFile(const std::string &path)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(!samplePlayback.loadFile(path))
        return false;
    source.gen.samplePlayback.filePath = path;
    samplePlayback.setParams(source.gen.samplePlayback);
    publishSnapshotNoLock();
    return true;
}

void SynthCore::setPostResampleEffectsParams(const EffectsChainParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    postResampleEffectsParams = p;
    publishSnapshotNoLock();
}

EffectsChainParams SynthCore::getPostResampleEffectsParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return postResampleEffectsParams;
}

void SynthCore::setCompositionProject(const CompositionProject &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    compositionProject = p;
}

CompositionProject SynthCore::getCompositionProject() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return compositionProject;
}
std::array<LfoParams, kMaxLfos> SynthCore::getLfoParamsSnapshot() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return lfoParams;
}
std::array<MatrixRule, kMaxMatrixRules> SynthCore::getMatrixRulesSnapshot() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return matrixRules;
}

StaticSpectralFrame SynthCore::getFrameSnapshot() const
{
    auto snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);
    return snap ? snap->frame : StaticSpectralFrame {};
}

SpectralTimeline SynthCore::getTimelineSnapshot() const
{
    auto snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);
    if(snap)
        return snap->timeline;
    StaticSpectralFrame frame;
    initFrameDefaults(frame);
    return makeStaticTimeline(frame);
}

int SynthCore::getActiveVoiceCount() const
{
    int n = 0;
    for(const auto &v : voices)
        if(!v.isIdle())
            ++n;
    return n;
}

// -----------------------------------------------------------------------------
// Voice allocation (§7)
// -----------------------------------------------------------------------------
Voice *SynthCore::findVoiceForNote(int note)
{
    for(auto &v : voices)
        if(!v.isIdle() && !v.isReleasing() && v.getNoteNumber() == note)
            return &v;
    return nullptr;
}

Voice *SynthCore::allocateVoice(int note)
{
    // 1. Idle voice.
    for(auto &v : voices)
        if(v.isIdle())
            return &v;
    // 2. Oldest releasing.
    Voice *bestRelease = nullptr;
    uint64_t bestReleaseTick = UINT64_MAX;
    for(auto &v : voices)
    {
        if(v.isReleasing() && v.getStartTick() < bestReleaseTick)
        {
            bestRelease = &v;
            bestReleaseTick = v.getStartTick();
        }
    }
    if(bestRelease)
    {
        bestRelease->steal();
        return bestRelease;
    }
    // 3. Steal oldest active.
    Voice *bestActive = &voices[0];
    uint64_t bestActiveTick = bestActive->getStartTick();
    for(auto &v : voices)
    {
        if(v.getStartTick() < bestActiveTick)
        {
            bestActive = &v;
            bestActiveTick = v.getStartTick();
        }
    }
    bestActive->steal();
    (void)note;
    return bestActive;
}

// -----------------------------------------------------------------------------
// Render
// -----------------------------------------------------------------------------
void SynthCore::renderBlock(float *left, float *right, int numSamples)
{
    auto snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);

    // Drain MIDI events at block start.
    MidiEvent e;
    while(popEvent(e))
    {
        if(e.type == 0) // note on
        {
            if(source.gen.type == GeneratorType::SamplePlayback)
            {
                samplePlayback.noteOn(e.note, e.velocity);
            }
            else
            {
                Voice *v = findVoiceForNote(e.note);
                if(v == nullptr)
                    v = allocateVoice(e.note);
                if(v && snap)
                    v->noteOn(e.note, e.velocity, sampleTimeline(snap->timeline, 0.0f),
                              snap->unison, ++startTickCounter);
            }
        }
        else if(e.type == 1) // note off
        {
            samplePlayback.noteOff(e.note);
            for(auto &v : voices)
                if(!v.isIdle() && !v.isReleasing() && v.getNoteNumber() == e.note)
                    v.noteOff();
        }
        else if(e.type == 2) // all off
        {
            for(auto &v : voices)
                v.noteOff();
            samplePlayback.allNotesOff();
        }
    }

    // Clear output.
    for(int s = 0; s < numSamples; ++s)
    {
        left[s] = 0.0f;
        right[s] = 0.0f;
    }

    // Control-rate / audio-rate split (§6).
    int written = 0;
    while(written < numSamples)
    {
        if(controlSamplesLeft <= 0)
        {
            snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);
            if(snap)
            {
                matrix.setParams(snap->lfoParams, snap->matrixRules, snap->chaosParams, snap->shapeSourceParams);
                effects.setParams(snap->effectsParams);
                resampling.setParams(snap->resamplingParams);
                postResampleEffects.setParams(snap->postResampleEffectsParams);
                matrix.advanceControl(kControlBlockSize);
            }

            // Per-voice update.
            for(auto &v : voices)
            {
                if(v.isIdle() || !snap)
                    continue;
                const auto voiceFrame = sampleTimeline(snap->timeline, v.sourceTimeSeconds());
                MatrixVoiceOutput mtx;
                matrix.evaluateForVoice(mtx, voiceFrame,
                                        v.velocity(), v.keyTrack01(),
                                        v.averageEnv(),
                                        float((v.voiceRandomSeed() & 0xFF)) / 255.0f);
                v.updateControl(voiceFrame, mtx, snap->globalAdsr, snap->unison,
                                snap->globalGain, kControlBlockSize);
            }
            controlSamplesLeft = kControlBlockSize;
        }

        const int chunk = std::min(numSamples - written, controlSamplesLeft);
        for(auto &v : voices)
        {
            if(!v.isIdle())
                v.renderAdd(left + written, right + written, chunk);
        }
        if(source.gen.type == GeneratorType::SamplePlayback)
            samplePlayback.process(left + written, right + written, chunk);
        written += chunk;
        controlSamplesLeft -= chunk;
    }

    if(snap)
    {
        effects.process(left, right, numSamples);
        resampling.process(left, right, numSamples);
        postResampleEffects.process(left, right, numSamples);
    }
    applyOutputSafetyBuffer(left, right, numSamples);
}

void SynthCore::applyOutputSafetyBuffer(float *left, float *right, int numSamples)
{
    float peak = 0.0f;
    for(int s = 0; s < numSamples; ++s)
    {
        peak = std::max(peak, std::abs(left[s]));
        peak = std::max(peak, std::abs(right[s]));
    }

    const float ceiling = 0.96f;
    const float target = (peak > ceiling) ? (ceiling / std::max(peak, 1e-6f)) : 1.0f;
    const float attack = 0.25f;
    const float release = 0.004f;
    outputSafetyGain += ((target < outputSafetyGain) ? attack : release) * (target - outputSafetyGain);
    outputSafetyGain = std::clamp(outputSafetyGain, 0.05f, 1.0f);

    for(int s = 0; s < numSamples; ++s)
    {
        float l = left[s] * outputSafetyGain;
        float r = right[s] * outputSafetyGain;
        if(std::abs(l) > ceiling)
            l = ceiling * std::tanh(l / ceiling);
        if(std::abs(r) > ceiling)
            r = ceiling * std::tanh(r / ceiling);
        left[s] = std::clamp(l, -0.999f, 0.999f);
        right[s] = std::clamp(r, -0.999f, 0.999f);
    }
}

} // namespace synth
