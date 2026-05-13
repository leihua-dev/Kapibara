#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace synth
{

enum class ParameterScope : uint8_t
{
    Global = 0,
    Seed,
    Creator,
    Motif,
    Structure,
    Mixing,
    Note,
    Track = Seed,
    Pattern = Motif
};

enum class ParameterTargetDomain : uint8_t
{
    GlobalParam = 0,
    SeedParam,
    CreatorParam,
    MotifParam,
    StructureParam,
    MixingParam,
    GeneratorParam,
    PartialParam,
    SamplePlaybackParam,
    FxParam,
    ResampleParam
};

enum class LockValueMode : uint8_t
{
    Override = 0,
    Additive,
    Multiplicative
};

enum class LockValueType : uint8_t
{
    Float = 0,
    Integer,
    Boolean
};

struct ParameterTarget
{
    ParameterTargetDomain domain = ParameterTargetDomain::GeneratorParam;
    std::string parameterId;
    int partialIndex = -1;
    int groupIndex = -1;
};

enum class SeedKind : uint8_t
{
    Spectral = 0,
    SamplePlayback
};

enum class CreatorKind : uint8_t
{
    MultiSeed = 0,
    DrumRack,
    PianoGrid,
    SampleCraft
};

struct LockValue
{
    LockValueType type = LockValueType::Float;
    LockValueMode mode = LockValueMode::Override;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
};

struct ParameterLock
{
    ParameterScope scope = ParameterScope::Note;
    ParameterTarget target;
    LockValue value;
    bool enabled = true;
};

struct SeedPreset
{
    uint64_t id = 0;
    std::string name = "Seed";
    SeedKind kind = SeedKind::Spectral;
    std::vector<ParameterLock> defaultLocks;
};

struct SeedInstance
{
    uint64_t id = 0;
    uint64_t seedPresetId = 0;
    std::string name = "Seed Instance";
    std::vector<ParameterLock> overrides;
};

struct CreatorPreset
{
    uint64_t id = 0;
    std::string name = "Creator";
    CreatorKind kind = CreatorKind::MultiSeed;
    std::vector<SeedInstance> seedInstances;
    std::vector<ParameterLock> macroDefaults;
};

struct CreatorInstance
{
    uint64_t id = 0;
    uint64_t creatorPresetId = 0;
    std::string name = "Creator Instance";
    std::vector<ParameterLock> overrides;
};

struct PianoNoteEvent
{
    uint64_t id = 0;
    int midiNote = 60;
    float startBeats = 0.0f;
    float durationBeats = 1.0f;
    float velocity = 0.8f;
    std::string sourceRef;
    std::vector<ParameterLock> parameterLocks;
};

enum class MotifEventKind : uint8_t
{
    SpectralNote = 0,
    SamplePlaybackNote,
    RhythmTrigger,
    ResampleTrigger
};

struct MotifEventRef
{
    MotifEventKind kind = MotifEventKind::SpectralNote;
    uint64_t eventId = 0;
};

struct MotifDefinition
{
    uint64_t id = 0;
    std::string name = "Motif";
    std::vector<PianoNoteEvent> noteEvents;
    std::vector<MotifEventRef> eventRefs;
    std::vector<ParameterLock> localAutomationDefaults;
};

struct MotifClip
{
    uint64_t id = 0;
    std::string name = "Motif";
    float lengthBeats = 4.0f;
    std::vector<CreatorInstance> creatorRefs;
    std::vector<PianoNoteEvent> noteEvents;
    std::vector<ParameterLock> localAutomation;
};

struct PatternDefinition
{
    uint64_t id = 0;
    std::string name = "Pattern";
    float lengthBeats = 4.0f;
    std::vector<uint64_t> motifIds;
    std::vector<ParameterLock> localAutomation;
};

enum class TrackPreviewMode : uint8_t
{
    Waveform = 0,
    Waterfall,
    PartialPreview
};

struct ArrangeTrack
{
    uint64_t id = 0;
    std::string name = "Track";
    TrackPreviewMode previewMode = TrackPreviewMode::Waveform;
    std::vector<ParameterLock> automation;
};

struct ArrangeRegion
{
    uint64_t id = 0;
    uint64_t trackId = 0;
    uint64_t patternId = 0;
    float startBeats = 0.0f;
    float lengthBeats = 4.0f;
};

struct StructureEvent
{
    uint64_t id = 0;
    uint64_t motifClipId = 0;
    float startBeats = 0.0f;
    float lengthBeats = 4.0f;
    std::vector<ParameterLock> transform;
};

struct StructureProject
{
    std::vector<StructureEvent> events;
    std::vector<ParameterLock> globalAutomation;
};

struct MixerChannel
{
    uint64_t id = 0;
    std::string name = "Channel";
    uint64_t sourceRef = 0;
    std::vector<ParameterLock> mixAutomation;
};

struct MixerState
{
    std::vector<MixerChannel> seedChannels;
    std::vector<MixerChannel> creatorChannels;
    std::vector<MixerChannel> motifBuses;
    std::vector<ParameterLock> masterAutomation;
};

struct AutomationGroup
{
    uint64_t id = 0;
    std::string name = "Automation";
    ParameterScope scope = ParameterScope::Track;
    std::vector<ParameterLock> locks;
};

struct AutomationGroupMatrix
{
    std::vector<AutomationGroup> groups;
};

struct CompositionProject
{
    std::vector<SeedPreset> seedPresets;
    std::vector<CreatorPreset> creatorPresets;
    std::vector<MotifClip> motifClips;
    StructureProject structure;
    MixerState mixer;

    // v0.2 compatibility fields, retained while the UI migrates.
    std::vector<MotifDefinition> motifs;
    std::vector<PatternDefinition> patterns;
    std::vector<ArrangeTrack> arrangeTracks;
    std::vector<ArrangeRegion> arrangeRegions;
    AutomationGroupMatrix automationMatrix;
};

} // namespace synth
