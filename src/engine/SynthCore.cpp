#include "SynthCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace synth
{

namespace
{
constexpr int kSeedControlBlockSize = 64;

StaticSpectralFrame clampedFrame(StaticSpectralFrame frame)
{
    frame.partialCount = std::clamp(frame.partialCount, 1, kMaxWavetablePartials);
    return frame;
}

template <typename T>
bool sameBytes(const T &a, const T &b)
{
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}

bool frameTableContentChanged(const WavetablePartialSlot &a, const WavetablePartialSlot &b)
{
    if(a.frameCount != b.frameCount)
        return true;
    const auto &framesA = a.frames.get();
    const auto &framesB = b.frames.get();
    for(int f = 0; f < a.frameCount; ++f)
    {
        const auto &frameA = framesA[(size_t)f];
        const auto &frameB = framesB[(size_t)f];
        if(frameA == frameB)
            continue;
        if(!frameA || !frameB)
            return true;
        const auto &fa = *frameA;
        const auto &fb = *frameB;
        if(fa.useImportedWaveform != fb.useImportedWaveform || fa.waveform != fb.waveform)
            return true;
        for(int h = 0; h < kMaxWavetableHarmonics; ++h)
        {
            const auto &ha = fa.harmonics[(size_t)h];
            const auto &hb = fb.harmonics[(size_t)h];
            if(std::abs(ha.ratio - hb.ratio) > 1.0e-6f
               || std::abs(ha.amp - hb.amp) > 1.0e-6f
               || std::abs(ha.phase - hb.phase) > 1.0e-6f)
                return true;
        }
    }
    return false;
}

bool wavetableSeedContentChanged(const WavetableSeedParams &a, const WavetableSeedParams &b)
{
    if(a.partialCount != b.partialCount || a.freqShape != b.freqShape
       || std::abs(a.inharmonicAmount - b.inharmonicAmount) > 1.0e-6f)
        return true;
    for(int i = 0; i < kMaxWavetablePartials; ++i)
    {
        const auto &pa = a.partials[(size_t)i];
        const auto &pb = b.partials[(size_t)i];
        if(pa.enabled != pb.enabled
           || std::abs(pa.ratio - pb.ratio) > 1.0e-6f
           || std::abs(pa.amp - pb.amp) > 1.0e-6f
           || std::abs(pa.phase - pb.phase) > 1.0e-6f
           || std::abs(pa.pan - pb.pan) > 1.0e-6f
           || pa.frameCount != pb.frameCount
           || std::abs(pa.morph - pb.morph) > 1.0e-6f
           || pa.warpMode != pb.warpMode
           || std::abs(pa.warpAmount - pb.warpAmount) > 1.0e-6f)
            return true;
    }
    return false;
}

bool sourceTrackSoundContentChanged(const SourceTrackParams &a, const SourceTrackParams &b)
{
    if(a.type != b.type)
        return true;
    switch(a.type)
    {
        case SourceTrackType::PartialBank:
            return wavetableSeedContentChanged(a.partialBank, b.partialBank);
        case SourceTrackType::MetaOscillator:
            // Only the waveform table content requires a full rebake.
            // Runtime params (ratio/amp/phase/pan/morph/warp) are patched via fast path.
            return frameTableContentChanged(a.metaOsc, b.metaOsc);
        case SourceTrackType::BasicOscillator:
            return a.basicShape != b.basicShape
                   || std::abs(a.pulseWidth - b.pulseWidth) > 1.0e-6f
                   || std::abs(a.subLevel - b.subLevel) > 1.0e-6f;
        case SourceTrackType::SampleNoise:
            return a.sampleNoiseMode != b.sampleNoiseMode
                   || std::abs(a.noiseColor - b.noiseColor) > 1.0e-6f;
    }
    return true;
}

void syncPartialCountEnabledState(WavetableSeedParams &params)
{
    params.partialCount = std::clamp(params.partialCount, 1, kMaxWavetablePartials);
    for(int i = 0; i < kMaxWavetablePartials; ++i)
        params.partials[(size_t)i].enabled = i < params.partialCount;
}

SourceTrackParams makeDefaultTrack(SourceTrackType type, uint32_t id, const char *name)
{
    SourceTrackParams t;
    t.id = id;
    t.type = type;
    t.name = name != nullptr && name[0] != '\0' ? name : sourceTrackTypeName(type);
    t.outputMode = SourceTrackOutputMode::Audio;
    t.gain = 1.0f;
    t.pan = 0.0f;
    t.strip.gain = 1.0f;
    t.strip.pan = 0.0f;
    t.ampEnvIndex = 0;
    t.unison.voices = 1;
    t.unison.detuneCents = 12.0f;
    t.unison.widthStereo = 0.7f;
    t.unison.phaseSpread = 1.0f;
    t.ampEnvelope.attack = 0.005f;
    t.ampEnvelope.decay = 0.35f;
    t.ampEnvelope.sustain = 1.0f;
    t.ampEnvelope.release = 0.08f;
    t.ampEnvelope.curve = 0.5f;
    if(type == SourceTrackType::MetaOscillator)
    {
        t.metaOsc.enabled = true;
        t.metaOsc.frameCount = kDefaultWavetableFrames;
        t.metaOsc.frames[0].harmonics[0].ratio = 1.0f;
        t.metaOsc.frames[0].harmonics[0].amp = 1.0f;
        t.name = name != nullptr && name[0] != '\0' ? name : "Meta Osc";
    }
    else if(type == SourceTrackType::BasicOscillator)
    {
        t.basicShape = BasicOscillatorShape::Sine;
        t.name = name != nullptr && name[0] != '\0' ? name : "Basic Osc";
    }
    else if(type == SourceTrackType::SampleNoise)
    {
        t.sampleNoiseMode = SampleNoiseMode::Noise;
        t.name = name != nullptr && name[0] != '\0' ? name : "Noise";
    }
    return t;
}

void buildBasicSeed(const SourceTrackParams &track, WavetableSeedParams &seed)
{
    seed = WavetableSeedParams {};
    const auto setPartial = [&](int i, float ratio, float amp) {
        if(i < 0 || i >= kMaxWavetablePartials)
            return;
        auto &p = seed.partials[(size_t)i];
        p.enabled = true;
        p.ratio = ratio;
        p.amp = amp;
        p.phase = 0.0f;
    };

    switch(track.basicShape)
    {
        case BasicOscillatorShape::Sine:
            seed.partialCount = 1;
            setPartial(0, 1.0f, 1.0f);
            break;
        case BasicOscillatorShape::Sub:
            seed.partialCount = 2;
            setPartial(0, 0.5f, 1.0f);
            setPartial(1, 1.0f, std::clamp(track.subLevel, 0.0f, 1.0f));
            break;
        case BasicOscillatorShape::Saw:
            seed.partialCount = 32;
            for(int i = 0; i < seed.partialCount; ++i)
                setPartial(i, float(i + 1), 1.0f / float(i + 1));
            break;
        case BasicOscillatorShape::Triangle:
            seed.partialCount = 31;
            for(int i = 0; i < seed.partialCount; i += 2)
            {
                const int n = i + 1;
                const float sign = ((n - 1) / 2) & 1 ? -1.0f : 1.0f;
                setPartial(i, float(n), std::abs(sign / float(n * n)));
                seed.partials[(size_t)i].phase = sign < 0.0f ? 3.14159265358979323846f : 0.0f;
            }
            break;
        case BasicOscillatorShape::Pulse:
            seed.partialCount = 32;
            for(int i = 0; i < seed.partialCount; ++i)
            {
                const float n = float(i + 1);
                const float duty = std::clamp(track.pulseWidth, 0.05f, 0.95f);
                setPartial(i, n, std::abs(std::sin(3.14159265358979323846f * n * duty) / n));
            }
            break;
    }
}

void buildNoiseSeed(const SourceTrackParams &track, WavetableSeedParams &seed)
{
    seed = WavetableSeedParams {};
    seed.partialCount = 48;
    const float color = std::clamp(track.noiseColor, 0.0f, 1.0f);
    for(int i = 0; i < seed.partialCount; ++i)
    {
        auto &p = seed.partials[(size_t)i];
        const float n = float(i + 1);
        p.enabled = true;
        p.ratio = n * (1.0f + 0.013f * float((i * 37) % 11));
        p.amp = std::pow(n, -color);
        p.phase = std::fmod(float(i * 97), 360.0f) * 0.01745329252f;
        p.pan = ((i & 1) ? 0.35f : -0.35f);
    }
}

WavetableSeedParams seedForTrack(const SourceTrackParams &track)
{
    if(track.type == SourceTrackType::PartialBank)
        return track.partialBank;
    WavetableSeedParams seed {};
    if(track.type == SourceTrackType::MetaOscillator)
    {
        seed.partialCount = 1;
        seed.partials[0] = track.metaOsc;
        seed.partials[0].enabled = true;
        return seed;
    }
    if(track.type == SourceTrackType::BasicOscillator)
    {
        buildBasicSeed(track, seed);
        return seed;
    }
    buildNoiseSeed(track, seed);
    return seed;
}

void fillSineOnlyRenderState(const WavetableSeedParams &seed, const StaticSpectralFrame &frame,
                             WavetableSeedRenderState &out)
{
    out.partialCount = std::clamp(frame.partialCount, 1, kMaxWavetablePartials);
    out.sourceCount = 1;
    out.trackCount = 0;
    for(int i = 0; i < kMaxWavetablePartials; ++i)
    {
        const auto &slot = seed.partials[(size_t)i];
        auto &dst = out.partials[(size_t)i];
        dst.enabled = i < out.partialCount && slot.enabled && frame.amp[(size_t)i] > 0.0f;
        dst.usesMetaWavetable = false;
        dst.ratio = frame.nu[(size_t)i];
        dst.amp = std::max(0.0f, frame.amp[(size_t)i]);
        dst.phase = frame.phaseLocked[(size_t)i];
        dst.pan = std::clamp(slot.pan, -1.0f, 1.0f);
        dst.frameCount = 1;
        dst.morph = 0.0f;
        dst.warpMode = WavetableWarpMode::None;
        dst.warpAmount = 0.0f;
        dst.tables.reset();
        dst.mipTables.reset();
    }
}
} // namespace

SynthCore::SynthCore()
{
    for(auto &env : ampEnvParams)
        env.sustain = 1.0f;
    regenerateFrameNoLock();
    publishSnapshotNoLock();
}

SynthCore::~SynthCore()
{
    allNotesOff();
}

void SynthCore::prepare(double sr)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    sampleRate = sr > 1000.0 ? sr : 48000.0;
    matrix.prepare(sampleRate);
    effects.prepare(sampleRate);
    for(auto &v : voices)
    {
        v.prepare(sampleRate);
        v.setModScratch(modScratch_.get());
    }
    controlSamplesLeft = 0;
    outputSafetyGain = 1.0f;
}

void SynthCore::regenerateFrameNoLock()
{
    ensureSourceTracksNoLock();
    rebuildTrackRenderStateNoLock(true);
}

void SynthCore::refreshGeneratorFrameNoLock()
{
    if(!source.gen.tracks.empty())
    {
        rebuildTrackRenderStateNoLock(false);
        return;
    }
    auto timeline = std::make_shared<SpectralTimeline>();
    generator.generateTimeline(source.gen, *timeline);
    source.chain.apply(*timeline);
    source.frame = timeline->frameCount > 0 ? timeline->frames[0] : StaticSpectralFrame {};
    source.timeline = timeline;
}

void SynthCore::refreshWavetableRuntimeNoLock()
{
    if(!source.gen.tracks.empty())
    {
        rebuildTrackRenderStateNoLock(false);
        return;
    }
    auto runtime = source.wavetable ? std::make_shared<WavetableSeedRenderState>(*source.wavetable)
                                    : std::make_shared<WavetableSeedRenderState>();
    refreshWavetableSeedRuntime(source.gen.wavetableSeed, source.gen.sourceCount, *runtime);
    source.wavetable = runtime;
}

void SynthCore::ensureSourceTracksNoLock()
{
    if(!source.gen.tracks.empty())
        return;
    auto track = makeDefaultTrack(SourceTrackType::PartialBank, 1u, "Partial Bank 1");
    track.partialBank = source.gen.wavetableSeed;
    track.strip = source.gen.sources[0];
    track.gain = track.strip.gain;
    track.pan = track.strip.pan;
    track.ampEnvIndex = 0;
    track.unison = source.gen.unison;
    track.ampEnvelope = globalAdsr;
    if(track.ampEnvelope.sustain <= 0.0f)
        track.ampEnvelope.sustain = 1.0f;
    source.gen.tracks.push_back(std::move(track));
    nextTrackId_ = 2u;
}

void SynthCore::rebuildTrackRenderStateNoLock(bool rebakeTables)
{
    ensureSourceTracksNoLock();
    auto timeline = std::make_shared<SpectralTimeline>();
    initTimelineDefaults(*timeline);
    auto baked = rebakeTables || !source.wavetable
                     ? std::make_shared<WavetableSeedRenderState>()
                     : std::make_shared<WavetableSeedRenderState>(*source.wavetable);
    StaticSpectralFrame mixed;
    initFrameDefaults(mixed);
    mixed.freqMode = FreqMode::RelativeRatio;
    mixed.phaseInitMode = PhaseInitMode::Locked;
    mixed.phaseSeed = 1u;
    baked->partialCount = 0;
    baked->trackCount = 0;
    baked->sourceCount = 1;
    baked->trackBegin.fill(0);
    baked->trackEnd.fill(0);

    const bool anySolo = std::any_of(source.gen.tracks.begin(), source.gen.tracks.end(),
                                     [](const SourceTrackParams &t) { return t.solo; });
    int partialOffset = 0;
    int renderTrack = 0;
    for(const auto &track : source.gen.tracks)
    {
        if(renderTrack >= kMaxSourceTracks || partialOffset >= kMaxWavetablePartials)
            break;
        if(track.mute || (anySolo && !track.solo))
            continue;

        auto seed = seedForTrack(track);
        seed.partialCount = std::clamp(seed.partialCount, 1, kMaxWavetablePartials);
        SourceGenParams local;
        local.wavetableSeed = seed;
        local.unison = track.unison;
        local.renderQuality = source.gen.renderQuality;
        local.sourceCount = 1;
        StaticSpectralFrame frame;
        generator.generate(local, frame);
        WavetableSeedRenderState localWave;
        if(track.type == SourceTrackType::MetaOscillator)
            bakeWavetableSeed(seed, 1, localWave);
        else
            fillSineOnlyRenderState(seed, frame, localWave);

        const int begin = partialOffset;
        const int copyCount = std::min(frame.partialCount, kMaxWavetablePartials - partialOffset);
        for(int i = 0; i < copyCount; ++i)
        {
            const int dst = partialOffset + i;
            mixed.nu[(size_t)dst] = frame.nu[(size_t)i];
            mixed.amp[(size_t)dst] = frame.amp[(size_t)i];
            mixed.x[(size_t)dst] = frame.x[(size_t)i];
            mixed.mu[(size_t)dst] = frame.mu[(size_t)i];
            mixed.phaseLocked[(size_t)dst] = frame.phaseLocked[(size_t)i];
            mixed.phaseDriftHz[(size_t)dst] = frame.phaseDriftHz[(size_t)i];
            mixed.phaseJitter[(size_t)dst] = frame.phaseJitter[(size_t)i];
            baked->partials[(size_t)dst] = localWave.partials[(size_t)i];
        }
        partialOffset += copyCount;
        baked->trackBegin[(size_t)renderTrack] = begin;
        baked->trackEnd[(size_t)renderTrack] = partialOffset;
        baked->trackAdsr[(size_t)renderTrack] = track.ampEnvelope;
        baked->trackOutputMode[(size_t)renderTrack] = track.outputMode;
        auto strip = track.strip;
        strip.gain = std::clamp(track.gain, 0.0f, 2.0f);
        strip.pan = std::clamp(track.pan, -1.0f, 1.0f);
        if(renderTrack < int(source.gen.sources.size()))
            source.gen.sources[(size_t)renderTrack] = strip;
        ++renderTrack;
    }

    mixed.partialCount = std::clamp(partialOffset, 1, kMaxWavetablePartials);
    for(int i = mixed.partialCount; i < kMaxPartials; ++i)
    {
        mixed.nu[(size_t)i] = 1.0f;
        mixed.amp[(size_t)i] = 0.0f;
        mixed.x[(size_t)i] = 0.0f;
        mixed.mu[(size_t)i] = 0;
        mixed.phaseLocked[(size_t)i] = 0.0f;
        mixed.phaseDriftHz[(size_t)i] = 0.0f;
        mixed.phaseJitter[(size_t)i] = 0.0f;
    }
    baked->partialCount = mixed.partialCount;
    baked->trackCount = renderTrack;
    timeline->frameCount = 1;
    timeline->durationSeconds = 0.0f;
    timeline->loop = false;
    timeline->timeSeconds[0] = 0.0f;
    timeline->frames[0] = mixed;
    source.frame = mixed;
    source.timeline = timeline;
    source.wavetable = baked;
}

void SynthCore::publishSnapshotNoLock()
{
    auto snap = std::make_shared<RenderSnapshot>();
    snap->timeline = source.timeline;
    snap->frame = source.frame;
    snap->wavetable = source.wavetable;
    snap->trackCount = std::min<int>(int(source.gen.tracks.size()), kMaxSourceTracks);
    for(int i = 0; i < snap->trackCount; ++i)
        snap->tracks[(size_t)i] = source.gen.tracks[(size_t)i];
    snap->adsr = globalAdsr;
    snap->ampEnvParams = ampEnvParams;
    snap->unison = source.gen.unison;
    snap->generatorSources = {};
    snap->trackRuntime = {};
    snap->renderTrackCount = 0;
    const bool anySolo = std::any_of(source.gen.tracks.begin(), source.gen.tracks.end(),
                                     [](const SourceTrackParams &t) { return t.solo; });
    for(const auto &track : source.gen.tracks)
    {
        if(snap->renderTrackCount >= kMaxSourceTracks)
            break;
        if(track.mute || (anySolo && !track.solo))
            continue;
        RenderTrackRuntime runtime;
        runtime.trackId = track.id;
        runtime.strip = track.strip;
        runtime.strip.gain = std::clamp(track.gain, 0.0f, 2.0f);
        runtime.strip.pan = std::clamp(track.pan, -1.0f, 1.0f);
        runtime.ampEnvIndex = std::clamp(track.ampEnvIndex, 0, kMaxAmpEnvs - 1);
        runtime.unison = track.unison;
        runtime.unison.voices = std::clamp(runtime.unison.voices, 1, kMaxUnison);
        runtime.outputMode = track.outputMode;
        runtime.mods = track.mods;
        runtime.inserts = track.inserts;
        snap->trackRuntime[(size_t)snap->renderTrackCount] = runtime;
        snap->generatorSources[(size_t)snap->renderTrackCount] = runtime.strip;
        ++snap->renderTrackCount;
    }
    if(source.gen.tracks.empty())
        for(size_t i = 0; i < source.gen.sources.size() && i < snap->generatorSources.size(); ++i)
            snap->generatorSources[i] = source.gen.sources[i];
    snap->renderQuality = source.gen.renderQuality;
    snap->globalGain = globalGain;
    snap->lfoParams = lfoParams;
    snap->matrixEnvParams = matrixEnvParams;
    snap->matrixRules = matrixRules;
    snap->chaosParams = chaosParams;
    snap->shapeSourceParams = shapeSourceParams;
    snap->effectsParams = effectsParams;
    snap->groups = groups_;
    std::atomic_store_explicit(&renderSnapshot, std::shared_ptr<const RenderSnapshot>(snap), std::memory_order_release);
}

void SynthCore::pushUndoSnapshotNoLock()
{
    UndoSnapshot s;
    s.gen = source.gen;
    s.chain = source.chain;
    s.adsr = globalAdsr;
    s.ampEnvParams = ampEnvParams;
    s.lfoParams = lfoParams;
    s.matrixEnvParams = matrixEnvParams;
    s.matrixRules = matrixRules;
    s.chaosParams = chaosParams;
    s.shapeSourceParams = shapeSourceParams;
    s.effectsParams = effectsParams;
    undoStack.push_back(std::move(s));
    if(undoStack.size() > 64)
        undoStack.erase(undoStack.begin());
}

void SynthCore::pushEvent(const MidiEvent &e)
{
    const int w = eventWrite_.load(std::memory_order_relaxed);
    const int next = (w + 1) % kEventQueueSize;
    if(next == eventRead_.load(std::memory_order_acquire))
        return;
    events_[(size_t)w] = e;
    eventWrite_.store(next, std::memory_order_release);
}

bool SynthCore::popEvent(MidiEvent &out)
{
    const int r = eventRead_.load(std::memory_order_relaxed);
    if(r == eventWrite_.load(std::memory_order_acquire))
        return false;
    out = events_[(size_t)r];
    eventRead_.store((r + 1) % kEventQueueSize, std::memory_order_release);
    return true;
}

void SynthCore::noteOn(int midiNote, float velocity)
{
    pushEvent({ 0, midiNote, velocity });
}

void SynthCore::noteOff(int midiNote)
{
    pushEvent({ 1, midiNote, 0.0f });
}

void SynthCore::allNotesOff()
{
    pushEvent({ 2, 0, 0.0f });
}

void SynthCore::setGeneratorParams(const SourceGenParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    source.gen = p;
    ensureSourceTracksNoLock();
    regenerateFrameNoLock();
    publishSnapshotNoLock();
}

uint32_t SynthCore::addSourceTrack(SourceTrackType type, const std::string &name)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    ensureSourceTracksNoLock();
    const uint32_t id = nextTrackId_++;
    source.gen.tracks.push_back(makeDefaultTrack(type, id, name.c_str()));
    rebuildTrackRenderStateNoLock(true);
    publishSnapshotNoLock();
    return id;
}

void SynthCore::removeSourceTrack(uint32_t trackId)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    ensureSourceTracksNoLock();
    if(source.gen.tracks.size() <= 1)
        return;
    const auto oldSize = source.gen.tracks.size();
    source.gen.tracks.erase(std::remove_if(source.gen.tracks.begin(), source.gen.tracks.end(),
                                           [trackId](const SourceTrackParams &t) { return t.id == trackId; }),
                            source.gen.tracks.end());
    if(source.gen.tracks.size() == oldSize)
        return;
    rebuildTrackRenderStateNoLock(true);
    publishSnapshotNoLock();
}

void SynthCore::moveSourceTrack(uint32_t trackId, int newIndex)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    ensureSourceTracksNoLock();
    auto it = std::find_if(source.gen.tracks.begin(), source.gen.tracks.end(),
                           [trackId](const SourceTrackParams &t) { return t.id == trackId; });
    if(it == source.gen.tracks.end())
        return;
    auto track = std::move(*it);
    source.gen.tracks.erase(it);
    newIndex = std::clamp(newIndex, 0, int(source.gen.tracks.size()));
    source.gen.tracks.insert(source.gen.tracks.begin() + newIndex, std::move(track));
    rebuildTrackRenderStateNoLock(false);
    publishSnapshotNoLock();
}

void SynthCore::setSourceTrack(uint32_t trackId, const SourceTrackParams &track)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    ensureSourceTracksNoLock();
    auto it = std::find_if(source.gen.tracks.begin(), source.gen.tracks.end(),
                           [trackId](const SourceTrackParams &t) { return t.id == trackId; });
    if(it == source.gen.tracks.end())
        return;
    const bool contentChanged = sourceTrackSoundContentChanged(*it, track);
    const bool membershipChanged = it->mute != track.mute || it->solo != track.solo;
    *it = track;
    it->id = trackId;
    if(contentChanged || membershipChanged)
        rebuildTrackRenderStateNoLock(contentChanged);
    else if(track.type == SourceTrackType::MetaOscillator)
        updateMetaTrackRenderParamsNoLock(trackId);  // fast path: no rebake
    publishSnapshotNoLock();
}

void SynthCore::updateMetaTrackRenderParamsNoLock(uint32_t trackId)
{
    if(!source.wavetable) return;
    auto it = std::find_if(source.gen.tracks.cbegin(), source.gen.tracks.cend(),
                           [trackId](const SourceTrackParams &t) { return t.id == trackId; });
    if(it == source.gen.tracks.cend()) return;
    const bool anySolo = std::any_of(source.gen.tracks.begin(), source.gen.tracks.end(),
                                     [](const SourceTrackParams &t) { return t.solo; });
    int renderIdx = 0;
    bool found = false;
    for(const auto &t : source.gen.tracks)
    {
        if(renderIdx >= kMaxSourceTracks) break;
        if(t.mute || (anySolo && !t.solo)) continue;
        if(t.id == trackId) { found = true; break; }
        ++renderIdx;
    }
    if(!found) return;
    const auto &slot = it->metaOsc;
    auto newState = std::make_shared<WavetableSeedRenderState>(*source.wavetable);
    const int begin = newState->trackBegin[(size_t)renderIdx];
    const int end   = newState->trackEnd[(size_t)renderIdx];
    for(int i = begin; i < end; ++i)
    {
        auto &p = newState->partials[(size_t)i];
        p.ratio      = std::clamp(slot.ratio, 0.01f, 128.0f);
        p.amp        = std::clamp(slot.amp, 0.0f, 1.0f);
        p.phase      = slot.phase;
        p.pan        = std::clamp(slot.pan, -1.0f, 1.0f);
        p.morph      = std::clamp(slot.morph, 0.0f, 1.0f);
        p.warpMode   = slot.warpMode;
        p.warpAmount = std::clamp(slot.warpAmount, -1.0f, 1.0f);
    }
    source.wavetable = newState;
}

void SynthCore::setSourceTrackMorphOnly(uint32_t trackId, float morph)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    ensureSourceTracksNoLock();
    auto it = std::find_if(source.gen.tracks.begin(), source.gen.tracks.end(),
                           [trackId](const SourceTrackParams &t) { return t.id == trackId; });
    if(it == source.gen.tracks.end() || it->type != SourceTrackType::MetaOscillator)
        return;
    const float cleanMorph = std::clamp(morph, 0.0f, 1.0f);
    if(std::abs(it->metaOsc.morph - cleanMorph) <= 1.0e-6f)
        return;
    it->metaOsc.morph = cleanMorph;
    if(source.wavetable)
    {
        auto newState = std::make_shared<WavetableSeedRenderState>(*source.wavetable);
        const bool anySolo = std::any_of(source.gen.tracks.begin(), source.gen.tracks.end(),
                                         [](const SourceTrackParams &t) { return t.solo; });
        int renderIdx = 0;
        bool found = false;
        for(const auto &t : source.gen.tracks)
        {
            if(renderIdx >= kMaxSourceTracks) break;
            if(t.mute || (anySolo && !t.solo)) continue;
            if(t.id == trackId) { found = true; break; }
            ++renderIdx;
        }
        if(found)
        {
            const int begin = newState->trackBegin[(size_t)renderIdx];
            const int end   = newState->trackEnd[(size_t)renderIdx];
            for(int i = begin; i < end; ++i)
                newState->partials[(size_t)i].morph = cleanMorph;
        }
        source.wavetable = newState;
    }
    publishSnapshotNoLock();
}

void SynthCore::setSourceTracks(const std::vector<SourceTrackParams> &tracks)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    source.gen.tracks = tracks;
    ensureSourceTracksNoLock();
    uint32_t maxId = 0;
    for(const auto &t : source.gen.tracks)
        maxId = std::max(maxId, t.id);
    nextTrackId_ = std::max(nextTrackId_, maxId + 1u);
    rebuildTrackRenderStateNoLock(true);
    publishSnapshotNoLock();
}

std::vector<SourceTrackParams> SynthCore::getSourceTracks() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return source.gen.tracks;
}

void SynthCore::setGeneratorBasicParams(int partialCount, FreqShape freqShape, float inharmonic, int sourceCount,
                                        const UnisonParams &unison)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    const int nextSourceCount = sanitizeGeneratorSourceCount(sourceCount);
    const bool sourceCountChanged = nextSourceCount != source.gen.sourceCount;
    const int nextPartialCount = sourceCountChanged
                                     ? kMaxWavetablePartials
                                     : std::clamp(partialCount, 1, kMaxWavetablePartials);
    const bool partialCountChanged = nextPartialCount != source.gen.wavetableSeed.partialCount;
    const bool runtimeChanged =
        partialCountChanged
        || freqShape != source.gen.wavetableSeed.freqShape
        || std::abs(std::clamp(inharmonic, 0.0f, 1.0f) - source.gen.wavetableSeed.inharmonicAmount) > 1.0e-6f
        || sourceCountChanged;
    const bool unisonChanged = !sameBytes(source.gen.unison, unison);
    if(!runtimeChanged && !unisonChanged)
        return;

    source.gen.sourceCount = nextSourceCount;
    source.gen.wavetableSeed.partialCount = nextPartialCount;
    source.gen.wavetableSeed.freqShape = freqShape;
    source.gen.wavetableSeed.inharmonicAmount = std::clamp(inharmonic, 0.0f, 1.0f);
    source.gen.unison = unison;
    if(sourceCountChanged || partialCountChanged)
        syncPartialCountEnabledState(source.gen.wavetableSeed);

    refreshGeneratorFrameNoLock();
    if(sourceCountChanged)
    {
        auto baked = std::make_shared<WavetableSeedRenderState>();
        bakeWavetableSeed(source.gen.wavetableSeed, source.gen.sourceCount, *baked);
        source.wavetable = baked;
    }
    else if(runtimeChanged)
    {
        refreshWavetableRuntimeNoLock();
    }
    publishSnapshotNoLock();
}

void SynthCore::setGeneratorRuntimeParams(const UnisonParams &unison, RenderQualityMode quality)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    const auto cleanQuality = quality;
    if(sameBytes(source.gen.unison, unison) && source.gen.renderQuality == cleanQuality)
        return;
    source.gen.unison = unison;
    source.gen.renderQuality = cleanQuality;
    publishSnapshotNoLock();
}

void SynthCore::setPartialEnabled(int index, bool enabled)
{
    if(index < 0 || index >= kMaxWavetablePartials)
        return;
    std::lock_guard<std::mutex> lock(paramMutex);
    auto &slot = source.gen.wavetableSeed.partials[(size_t)index];
    if(slot.enabled == enabled)
        return;
    slot.enabled = enabled;
    refreshGeneratorFrameNoLock();
    refreshWavetableRuntimeNoLock();
    publishSnapshotNoLock();
}

void SynthCore::setPartialAmp(int index, float amp)
{
    if(index < 0 || index >= kMaxWavetablePartials)
        return;
    std::lock_guard<std::mutex> lock(paramMutex);
    auto &slot = source.gen.wavetableSeed.partials[(size_t)index];
    const float clean = std::clamp(amp, 0.0f, 1.0f);
    if(std::abs(slot.amp - clean) <= 1.0e-6f)
        return;
    slot.amp = clean;
    refreshGeneratorFrameNoLock();
    refreshWavetableRuntimeNoLock();
    publishSnapshotNoLock();
}

void SynthCore::setPartialRatio(int index, float ratio)
{
    if(index < 0 || index >= kMaxWavetablePartials)
        return;
    std::lock_guard<std::mutex> lock(paramMutex);
    auto &slot = source.gen.wavetableSeed.partials[(size_t)index];
    const float clean = std::clamp(ratio, 0.01f, 128.0f);
    if(std::abs(slot.ratio - clean) <= 1.0e-6f)
        return;
    slot.ratio = clean;
    refreshGeneratorFrameNoLock();
    refreshWavetableRuntimeNoLock();
    publishSnapshotNoLock();
}

void SynthCore::setMetaPartialRuntime(int index, bool enabled, float ratio, float amp, float phase, float pan,
                                      float morph, WavetableWarpMode warpMode, float warpAmount)
{
    if(index < 0 || index >= kEditableMetaPartials)
        return;
    std::lock_guard<std::mutex> lock(paramMutex);
    auto &slot = source.gen.wavetableSeed.partials[(size_t)index];
    const float cleanRatio = std::clamp(ratio, 0.01f, 128.0f);
    const float cleanAmp = std::clamp(amp, 0.0f, 1.0f);
    const float cleanPhase = std::clamp(phase, -3.14159265358979323846f, 3.14159265358979323846f);
    const float cleanPan = std::clamp(pan, -1.0f, 1.0f);
    const float cleanMorph = std::clamp(morph, 0.0f, 1.0f);
    const float cleanWarpAmount = std::clamp(warpAmount, -1.0f, 1.0f);
    const auto cleanWarp = static_cast<WavetableWarpMode>(std::clamp(int(warpMode), 0, 3));
    if(slot.enabled == enabled
       && std::abs(slot.ratio - cleanRatio) <= 1.0e-6f
       && std::abs(slot.amp - cleanAmp) <= 1.0e-6f
       && std::abs(slot.phase - cleanPhase) <= 1.0e-6f
       && std::abs(slot.pan - cleanPan) <= 1.0e-6f
       && std::abs(slot.morph - cleanMorph) <= 1.0e-6f
       && slot.warpMode == cleanWarp
       && std::abs(slot.warpAmount - cleanWarpAmount) <= 1.0e-6f)
        return;
    slot.enabled = enabled;
    slot.ratio = cleanRatio;
    slot.amp = cleanAmp;
    slot.phase = cleanPhase;
    slot.pan = cleanPan;
    slot.morph = cleanMorph;
    slot.warpMode = cleanWarp;
    slot.warpAmount = cleanWarpAmount;
    refreshGeneratorFrameNoLock();
    refreshWavetableRuntimeNoLock();
    publishSnapshotNoLock();
}

void SynthCore::setMetaPartialSlot(int index, const WavetablePartialSlot &slot)
{
    if(index < 0 || index >= kEditableMetaPartials)
        return;
    std::lock_guard<std::mutex> lock(paramMutex);
    const auto &oldSlot = source.gen.wavetableSeed.partials[(size_t)index];
    const bool tableDirty = frameTableContentChanged(oldSlot, slot);
    source.gen.wavetableSeed.partials[(size_t)index] = slot;
    if(index + 1 > source.gen.wavetableSeed.partialCount)
    {
        source.gen.wavetableSeed.partialCount = index + 1;
        syncPartialCountEnabledState(source.gen.wavetableSeed);
        source.gen.wavetableSeed.partials[(size_t)index].enabled = true;
    }
    refreshGeneratorFrameNoLock();
    if(tableDirty)
    {
        auto runtime = source.wavetable ? std::make_shared<WavetableSeedRenderState>(*source.wavetable)
                                        : std::make_shared<WavetableSeedRenderState>();
        rebakeWavetableSeedMetaPartial(source.gen.wavetableSeed, source.gen.sourceCount, index, *runtime);
        source.wavetable = runtime;
    }
    else
    {
        refreshWavetableRuntimeNoLock();
    }
    publishSnapshotNoLock();
}

bool SynthCore::loadWavetableFrame(int partialIndex, int frameIndex, const std::string &path)
{
    if(partialIndex < 0 || partialIndex >= kEditableMetaPartials || frameIndex < 0
       || frameIndex >= kMaxWavetableFrames || path.empty())
        return false;

    std::lock_guard<std::mutex> lock(paramMutex);
    auto &slot = source.gen.wavetableSeed.partials[(size_t)partialIndex];
    const int requiredFrameCount = frameIndex + 1;
    if(slot.frameCount < requiredFrameCount)
        slot.frameCount = requiredFrameCount;
    if(!loadWavetableFrameFromWav(path, slot.frames[(size_t)frameIndex]))
        return false;

    slot.enabled = true;
    if(partialIndex + 1 > source.gen.wavetableSeed.partialCount)
    {
        source.gen.wavetableSeed.partialCount = partialIndex + 1;
        syncPartialCountEnabledState(source.gen.wavetableSeed);
        source.gen.wavetableSeed.partials[(size_t)partialIndex].enabled = true;
    }
    regenerateFrameNoLock();
    publishSnapshotNoLock();
    return true;
}

void SynthCore::setGeneratorSourceParams(int index, const GeneratorSourceParams &sourceParams)
{
    if(index < 0 || index >= 8)
        return;
    std::lock_guard<std::mutex> lock(paramMutex);
    auto &target = source.gen.sources[(size_t)index];
    if(sameBytes(target, sourceParams))
        return;
    target = sourceParams;
    publishSnapshotNoLock();
}

SourceGenParams SynthCore::getGeneratorParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return source.gen;
}

void SynthCore::setSeedGeneratorParams(uint64_t, const SourceGenParams &p) { setGeneratorParams(p); }
SourceGenParams SynthCore::getSeedGeneratorParams(uint64_t) const { return getGeneratorParams(); }

void SynthCore::setGlobalAdsr(const AdsrParams &a)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(globalAdsr, a))
        return;
    globalAdsr = a;
    publishSnapshotNoLock();
}

AdsrParams SynthCore::getGlobalAdsr() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return globalAdsr;
}

void SynthCore::setAmpEnvParams(int idx, const AdsrParams &params)
{
    if(idx < 0 || idx >= kMaxAmpEnvs)
        return;
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(ampEnvParams[(size_t)idx], params))
        return;
    ampEnvParams[(size_t)idx] = params;
    publishSnapshotNoLock();
}

AdsrParams SynthCore::getAmpEnvParams(int idx) const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return idx >= 0 && idx < kMaxAmpEnvs ? ampEnvParams[(size_t)idx] : AdsrParams {};
}

void SynthCore::setSeedAdsrParams(uint64_t, const AdsrParams &a) { setGlobalAdsr(a); }
AdsrParams SynthCore::getSeedAdsrParams(uint64_t) const { return getGlobalAdsr(); }

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

void SynthCore::setSeedOperatorChain(uint64_t, const OperatorChain &c) { setOperatorChain(c); }
OperatorChain SynthCore::getSeedOperatorChain(uint64_t) const { return getOperatorChain(); }

void SynthCore::setGlobalGain(float g)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    const float clean = std::clamp(g, 0.0f, 2.0f);
    if(std::abs(globalGain - clean) <= 1.0e-6f)
        return;
    globalGain = clean;
    publishSnapshotNoLock();
}

float SynthCore::getGlobalGain() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return globalGain;
}

void SynthCore::setLfoParams(int idx, const LfoParams &p)
{
    if(idx < 0 || idx >= kMaxLfos) return;
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(lfoParams[(size_t)idx], p))
        return;
    lfoParams[(size_t)idx] = p;
    publishSnapshotNoLock();
}

void SynthCore::setLfoParamsWithUndo(int idx, const LfoParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(idx < 0 || idx >= kMaxLfos) return;
    pushUndoSnapshotNoLock();
    lfoParams[(size_t)idx] = p;
    publishSnapshotNoLock();
}

LfoParams SynthCore::getLfoParams(int idx) const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return (idx >= 0 && idx < kMaxLfos) ? lfoParams[(size_t)idx] : LfoParams {};
}

void SynthCore::setSeedLfoParams(uint64_t, int idx, const LfoParams &p) { setLfoParams(idx, p); }
void SynthCore::setSeedLfoParamsWithUndo(uint64_t, int idx, const LfoParams &p) { setLfoParamsWithUndo(idx, p); }
LfoParams SynthCore::getSeedLfoParams(uint64_t, int idx) const { return getLfoParams(idx); }

void SynthCore::setMatrixEnvParams(int idx, const MatrixEnvParams &p)
{
    if(idx < 0 || idx >= kMaxModEnvs) return;
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(matrixEnvParams[(size_t)idx], p))
        return;
    matrixEnvParams[(size_t)idx] = p;
    publishSnapshotNoLock();
}

void SynthCore::setMatrixEnvParamsWithUndo(int idx, const MatrixEnvParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(idx < 0 || idx >= kMaxModEnvs) return;
    pushUndoSnapshotNoLock();
    matrixEnvParams[(size_t)idx] = p;
    publishSnapshotNoLock();
}

MatrixEnvParams SynthCore::getMatrixEnvParams(int idx) const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return (idx >= 0 && idx < kMaxModEnvs) ? matrixEnvParams[(size_t)idx] : MatrixEnvParams {};
}

void SynthCore::setSeedMatrixEnvParams(uint64_t, int idx, const MatrixEnvParams &p) { setMatrixEnvParams(idx, p); }
void SynthCore::setSeedMatrixEnvParamsWithUndo(uint64_t, int idx, const MatrixEnvParams &p) { setMatrixEnvParamsWithUndo(idx, p); }
MatrixEnvParams SynthCore::getSeedMatrixEnvParams(uint64_t, int idx) const { return getMatrixEnvParams(idx); }

void SynthCore::setMatrixRule(int idx, const MatrixRule &r)
{
    if(idx < 0 || idx >= kMaxMatrixRules) return;
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(matrixRules[(size_t)idx], r))
        return;
    matrixRules[(size_t)idx] = r;
    publishSnapshotNoLock();
}

void SynthCore::setMatrixRuleWithUndo(int idx, const MatrixRule &r)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(idx < 0 || idx >= kMaxMatrixRules) return;
    pushUndoSnapshotNoLock();
    matrixRules[(size_t)idx] = r;
    publishSnapshotNoLock();
}

MatrixRule SynthCore::getMatrixRule(int idx) const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return (idx >= 0 && idx < kMaxMatrixRules) ? matrixRules[(size_t)idx] : MatrixRule {};
}

void SynthCore::setSeedMatrixRule(uint64_t, int idx, const MatrixRule &r) { setMatrixRule(idx, r); }
void SynthCore::setSeedMatrixRuleWithUndo(uint64_t, int idx, const MatrixRule &r) { setMatrixRuleWithUndo(idx, r); }
MatrixRule SynthCore::getSeedMatrixRule(uint64_t, int idx) const { return getMatrixRule(idx); }

void SynthCore::setChaosParams(const ChaosParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(chaosParams, p))
        return;
    chaosParams = p;
    publishSnapshotNoLock();
}

void SynthCore::setChaosParamsWithUndo(const ChaosParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    pushUndoSnapshotNoLock();
    chaosParams = p;
    publishSnapshotNoLock();
}

ChaosParams SynthCore::getChaosParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return chaosParams;
}

void SynthCore::setSeedChaosParams(uint64_t, const ChaosParams &p) { setChaosParams(p); }
void SynthCore::setSeedChaosParamsWithUndo(uint64_t, const ChaosParams &p) { setChaosParamsWithUndo(p); }
ChaosParams SynthCore::getSeedChaosParams(uint64_t) const { return getChaosParams(); }

void SynthCore::setShapeSourceParams(const ShapeSourceParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(shapeSourceParams, p))
        return;
    shapeSourceParams = p;
    publishSnapshotNoLock();
}

void SynthCore::setShapeSourceParamsWithUndo(const ShapeSourceParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    pushUndoSnapshotNoLock();
    shapeSourceParams = p;
    publishSnapshotNoLock();
}

ShapeSourceParams SynthCore::getShapeSourceParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return shapeSourceParams;
}

void SynthCore::setSeedShapeSourceParams(uint64_t, const ShapeSourceParams &p) { setShapeSourceParams(p); }
void SynthCore::setSeedShapeSourceParamsWithUndo(uint64_t, const ShapeSourceParams &p) { setShapeSourceParamsWithUndo(p); }
ShapeSourceParams SynthCore::getSeedShapeSourceParams(uint64_t) const { return getShapeSourceParams(); }

void SynthCore::setEffectsParams(const EffectsChainParams &p)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(sameBytes(effectsParams, p))
        return;
    effectsParams = p;
    publishSnapshotNoLock();
}

EffectsChainParams SynthCore::getEffectsParams() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    return effectsParams;
}

void SynthCore::setSeedEffectsParams(uint64_t, const EffectsChainParams &p) { setEffectsParams(p); }
EffectsChainParams SynthCore::getSeedEffectsParams(uint64_t) const { return getEffectsParams(); }

void SynthCore::setSourceGroups(const std::vector<SourceGroupDef> &groups)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    groups_ = groups;
    publishSnapshotNoLock();
}

// Apply per-strip insert chains to the track buses, fold groups into their own buses,
// then mix everything (ungrouped strips + group buses) into the output.
void SynthCore::renderStripBuses(float *left, float *right, int numSamples,
                                 const std::shared_ptr<const RenderSnapshot> &snap)
{
    if(!snap)
        return;
    const int rc = std::min(snap->renderTrackCount, kMaxSourceTracks);

    // 1) Per-strip inserts on each track bus (matrix-modulated).
    for(int t = 0; t < rc; ++t)
    {
        const auto &rt = snap->trackRuntime[(size_t)t];
        if(!rt.inserts.empty())
            processInsertChain(trackBus_[(size_t)t].l.data(), trackBus_[(size_t)t].r.data(), numSamples,
                               sampleRate, rt.inserts, trackInsertState_[(size_t)t],
                               trackInsertMod_[(size_t)t].data(), kInsertModParams);
    }

    // 2) Group buses: sum member strips, run group inserts, and remember which strips
    //    are grouped so they aren't also mixed directly.
    const int ng = int(snap->groups.size());
    if(int(groupBus_.size()) < ng) groupBus_.resize((size_t)ng);
    if(int(groupInsertState_.size()) < ng) groupInsertState_.resize((size_t)ng);
    std::array<bool, kMaxSourceTracks> grouped {};
    for(int g = 0; g < ng; ++g)
    {
        auto &gb = groupBus_[(size_t)g];
        std::fill(gb.l.begin(), gb.l.begin() + numSamples, 0.0f);
        std::fill(gb.r.begin(), gb.r.begin() + numSamples, 0.0f);
        for(uint32_t id : snap->groups[(size_t)g].memberTrackIds)
            for(int t = 0; t < rc; ++t)
                if(snap->trackRuntime[(size_t)t].trackId == id)
                {
                    grouped[(size_t)t] = true;
                    for(int s = 0; s < numSamples; ++s)
                    {
                        gb.l[(size_t)s] += trackBus_[(size_t)t].l[(size_t)s];
                        gb.r[(size_t)s] += trackBus_[(size_t)t].r[(size_t)s];
                    }
                }
        if(!snap->groups[(size_t)g].inserts.empty())
            processInsertChain(gb.l.data(), gb.r.data(), numSamples, sampleRate,
                               snap->groups[(size_t)g].inserts, groupInsertState_[(size_t)g]);
        for(int s = 0; s < numSamples; ++s) { left[s] += gb.l[(size_t)s]; right[s] += gb.r[(size_t)s]; }
    }

    // 3) Mix ungrouped strips directly.
    for(int t = 0; t < rc; ++t)
    {
        if(grouped[(size_t)t]) continue;
        for(int s = 0; s < numSamples; ++s)
        {
            left[s] += trackBus_[(size_t)t].l[(size_t)s];
            right[s] += trackBus_[(size_t)t].r[(size_t)s];
        }
    }
}

void SynthCore::setSeedPatch(const SeedPatch &patch)
{
    std::lock_guard<std::mutex> lock(paramMutex);
    source.presetName = patch.name;
    source.gen = patch.generator;
    ensureSourceTracksNoLock();
    source.chain = patch.operatorChain;
    globalAdsr = patch.adsr;
    ampEnvParams = patch.ampEnvParams;
    lfoParams = patch.lfoParams;
    matrixEnvParams = patch.matrixEnvParams;
    matrixRules = patch.matrixRules;
    chaosParams = patch.chaosParams;
    shapeSourceParams = patch.shapeSourceParams;
    effectsParams = patch.toneFx;
    regenerateFrameNoLock();
    publishSnapshotNoLock();
}

SeedPatch SynthCore::getSeedPatch() const
{
    std::lock_guard<std::mutex> lock(paramMutex);
    SeedPatch patch;
    patch.name = source.presetName;
    patch.generator = source.gen;
    patch.operatorChain = source.chain;
    patch.adsr = globalAdsr;
    patch.ampEnvParams = ampEnvParams;
    patch.lfoParams = lfoParams;
    patch.matrixEnvParams = matrixEnvParams;
    patch.matrixRules = matrixRules;
    patch.chaosParams = chaosParams;
    patch.shapeSourceParams = shapeSourceParams;
    patch.toneFx = effectsParams;
    return patch;
}

bool SynthCore::undoCompositionChange()
{
    std::lock_guard<std::mutex> lock(paramMutex);
    if(undoStack.empty())
        return false;
    auto s = std::move(undoStack.back());
    undoStack.pop_back();
    source.gen = s.gen;
    ensureSourceTracksNoLock();
    source.chain = s.chain;
    globalAdsr = s.adsr;
    ampEnvParams = s.ampEnvParams;
    lfoParams = s.lfoParams;
    matrixEnvParams = s.matrixEnvParams;
    matrixRules = s.matrixRules;
    chaosParams = s.chaosParams;
    shapeSourceParams = s.shapeSourceParams;
    effectsParams = s.effectsParams;
    regenerateFrameNoLock();
    publishSnapshotNoLock();
    return true;
}

StaticSpectralFrame SynthCore::getFrameSnapshot() const
{
    auto snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);
    return snap ? snap->frame : StaticSpectralFrame {};
}

SpectralTimeline SynthCore::getTimelineSnapshot() const
{
    auto snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);
    return snap && snap->timeline ? *snap->timeline : SpectralTimeline {};
}

int SynthCore::getActiveVoiceCount() const
{
    int count = 0;
    for(const auto &v : voices)
        if(!v.isIdle())
            ++count;
    return count;
}

Voice *SynthCore::allocateVoice(int note)
{
    for(auto &v : voices)
        if(v.isIdle())
            return &v;

    Voice *oldest = &voices[0];
    for(auto &v : voices)
        if(v.getStartTick() < oldest->getStartTick())
            oldest = &v;
    oldest->steal();
    (void)note;
    return oldest;
}

void SynthCore::renderBlock(float *left, float *right, int numSamples)
{
    auto snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);

    MidiEvent e;
    while(popEvent(e))
    {
        if(e.type == 0)
        {
            for(auto &v : voices)
            {
                if(!v.isIdle() && v.getNoteNumber() == e.note)
                    v.steal();
            }
            if(auto *v = allocateVoice(e.note); v != nullptr && snap && snap->timeline && snap->wavetable)
            {
                v->setWavetableRenderState(snap->wavetable);
                v->noteOn(e.note, std::clamp(e.velocity, 0.0f, 1.0f),
                          clampedFrame(sampleTimeline(*snap->timeline, 0.0f)),
                          snap->adsr,
                          snap->ampEnvParams,
                          snap->matrixEnvParams,
                          snap->unison,
                          snap->renderQuality,
                          ++startTickCounter);
            }
        }
        else if(e.type == 1)
        {
            for(auto &v : voices)
            {
                if(!v.isIdle() && v.getNoteNumber() == e.note && !v.isReleasing())
                    v.noteOff();
            }
        }
        else if(e.type == 2)
        {
            for(auto &v : voices)
                v.steal();
        }
    }

    for(int s = 0; s < numSamples; ++s)
    {
        left[s] = 0.0f;
        right[s] = 0.0f;
    }

    int written = 0;
    while(written < numSamples)
    {
        if(controlSamplesLeft <= 0)
        {
            snap = std::atomic_load_explicit(&renderSnapshot, std::memory_order_acquire);
            if(snap)
            {
                matrix.setParams(snap->lfoParams, snap->matrixEnvParams, snap->matrixRules,
                                 snap->chaosParams, snap->shapeSourceParams);
                effects.setParams(snap->effectsParams);
                matrix.advanceControl(kSeedControlBlockSize);
            }

            std::array<uint32_t, kMaxSourceTracks> matrixTrackIds {};
            std::array<int, kMaxSourceTracks> matrixTrackBegin {};
            std::array<int, kMaxSourceTracks> matrixTrackEnd {};
            if(snap)
            {
                for(int t = 0; t < snap->renderTrackCount; ++t)
                {
                    matrixTrackIds[(size_t)t] = snap->trackRuntime[(size_t)t].trackId;
                    if(snap->wavetable)
                    {
                        matrixTrackBegin[(size_t)t] = snap->wavetable->trackBegin[(size_t)t];
                        matrixTrackEnd[(size_t)t] = snap->wavetable->trackEnd[(size_t)t];
                    }
                }
            }
            // Per-strip (global) insert modulation: representative ADSR/ENV across voices.
            float adsrRep = 0.0f;
            std::array<float, kMaxModEnvs> envRep {};
            int activeCount = 0;
            for(auto &v : voices)
            {
                if(v.isIdle()) continue;
                ++activeCount;
                adsrRep += v.averageEnv();
                const auto &el = v.modEnvLevels();
                for(int e = 0; e < kMaxModEnvs; ++e) envRep[(size_t)e] += el[(size_t)e];
            }
            if(activeCount > 0)
            {
                adsrRep /= float(activeCount);
                for(int e = 0; e < kMaxModEnvs; ++e) envRep[(size_t)e] /= float(activeCount);
            }
            for(auto &m : trackInsertMod_) m.fill(0.0f);
            if(snap)
            {
                for(const auto &rule : snap->matrixRules)
                {
                    const int param = insertModParamForDest(rule.dest);
                    if(!rule.enabled || param < 0 || std::abs(rule.depth) < 1e-6f) continue;
                    if(rule.targetSlot < 0 || rule.targetSlot >= kMaxModInserts) continue;
                    int rt = -1;
                    for(int t = 0; t < snap->renderTrackCount; ++t)
                        if(snap->trackRuntime[(size_t)t].trackId == rule.targetTrackId) { rt = t; break; }
                    if(rt < 0) continue;
                    trackInsertMod_[(size_t)rt][(size_t)insertModIndex(rule.targetSlot, param)] +=
                        rule.depth * matrix.globalModSource(rule.source, adsrRep, envRep);
                }
            }

            for(auto &v : voices)
            {
                if(v.isIdle() || !snap)
                    continue;
                if(!snap->timeline)
                    continue;
                const auto frame = clampedFrame(sampleTimeline(*snap->timeline, v.sourceTimeSeconds()));
                v.setWavetableRenderState(snap->wavetable);
                MatrixVoiceOutput mtx;
                matrix.evaluateForVoice(mtx, frame,
                                        v.velocity(), v.keyTrack01(),
                                        v.averageEnv(), v.modEnvLevels(),
                                        float((v.voiceRandomSeed() & 0xFF)) / 255.0f,
                                        matrixTrackIds.data(), matrixTrackBegin.data(), matrixTrackEnd.data(),
                                        snap->renderTrackCount);
                v.updateControl(frame, mtx, snap->adsr, snap->ampEnvParams, snap->matrixEnvParams,
                                snap->unison, snap->trackRuntime, snap->renderTrackCount, snap->renderQuality,
                                snap->globalGain, kSeedControlBlockSize);
            }
            controlSamplesLeft = kSeedControlBlockSize;
        }

        const int chunk = std::min(numSamples - written, controlSamplesLeft);
        // Per-strip bus rendering: zero buses → voices accumulate per-track → strip inserts → mix.
        std::array<float *, kMaxSourceTracks> busLptr {}, busRptr {};
        for(int t = 0; t < kMaxSourceTracks; ++t)
        {
            std::fill(trackBus_[(size_t)t].l.begin(), trackBus_[(size_t)t].l.begin() + chunk, 0.0f);
            std::fill(trackBus_[(size_t)t].r.begin(), trackBus_[(size_t)t].r.begin() + chunk, 0.0f);
            busLptr[(size_t)t] = trackBus_[(size_t)t].l.data();
            busRptr[(size_t)t] = trackBus_[(size_t)t].r.data();
        }
        for(auto &v : voices)
            if(!v.isIdle())
            {
                v.setTrackBuses(busLptr.data(), busRptr.data());
                v.renderAdd(left + written, right + written, chunk);
            }
        renderStripBuses(left + written, right + written, chunk, snap);
        written += chunk;
        controlSamplesLeft -= chunk;
    }

    if(snap)
    {
        effects.process(left, right, numSamples);
        if(std::abs(snap->globalGain - 1.0f) > 0.0001f)
        {
            for(int s = 0; s < numSamples; ++s)
            {
                left[s] *= snap->globalGain;
                right[s] *= snap->globalGain;
            }
        }
    }
    applyOutputSafetyBuffer(left, right, numSamples);

    // Write per-track live morph for UI display (lock-free, stale-by-one-block is acceptable)
    if(snap && snap->wavetable)
    {
        const auto &wt = *snap->wavetable;
        for(int ti = 0; ti < snap->renderTrackCount && ti < kMaxSourceTracks; ++ti)
        {
            const int begin = wt.trackBegin[(size_t)ti];
            float bestMorph = liveTrackMorph_[(size_t)ti].load(std::memory_order_relaxed);
            for(const auto &v : voices)
            {
                if(!v.isIdle())
                {
                    bestMorph = v.getLiveMorph(begin);
                    break;
                }
            }
            liveTrackMorph_[(size_t)ti].store(bestMorph, std::memory_order_relaxed);
        }
    }
}

void SynthCore::applyOutputSafetyBuffer(float *left, float *right, int numSamples)
{
    float peak = 0.0f;
    for(int s = 0; s < numSamples; ++s)
    {
        peak = std::max(peak, std::abs(left[s]));
        peak = std::max(peak, std::abs(right[s]));
    }
    const float target = peak > 0.98f ? 0.98f / peak : std::min(1.0f, outputSafetyGain + 0.001f);
    outputSafetyGain += (target - outputSafetyGain) * 0.05f;
    for(int s = 0; s < numSamples; ++s)
    {
        left[s] *= outputSafetyGain;
        right[s] *= outputSafetyGain;
        left[s] = std::tanh(left[s]);
        right[s] = std::tanh(right[s]);
    }
}

} // namespace synth
