constexpr float kPi = 3.14159265358979323846f;

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool contains(float px, float py) const
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct ModRouteTarget
{
    bool valid = false;
    synth::ModDestination destination = synth::ModDestination::Amp;
    uint32_t trackId = 0;
    Rect rect {};
    const char *name = "";
    int slot = 0; // bank slot for filter/dist effect destinations
};

using InsertEffect = synth::InsertEffect;

constexpr int InsertFilter = synth::InsertFilter;
constexpr int InsertDist = synth::InsertDist;
constexpr int InsertEq = synth::InsertEq;
constexpr int InsertComp = synth::InsertComp;
constexpr int InsertDelay = synth::InsertDelay;
constexpr int InsertReverb = synth::InsertReverb;
constexpr int InsertConvReverb = synth::InsertConvReverb;
constexpr int InsertMultiband = synth::InsertMultiband;
constexpr int kInsertTypeCount = 8; // filter,dist,eq,comp,delay,reverb,IR,multiband (kind 1..8)

constexpr int kFilterAlgoCount = 12;
constexpr const char *kFilterAlgoNames[] = {
    "LP 12","HP 12","BP 12","Notch","APF 2","APF 4","APF 8",
    "Peak","Lo Shelf","Hi Shelf","LP 24","HP 24"
};

constexpr int kDistAlgoCount = 8;
constexpr const char *kDistAlgoNames[] = {
    "Soft","Hard","Tube","Diode","Fold","SinFld","Crush","Tanh"
};

constexpr synth::ModSource kGridSourcePool[] = {
    synth::ModSource::Lfo1, synth::ModSource::Lfo2, synth::ModSource::Lfo3, synth::ModSource::Lfo4,
    synth::ModSource::Env1, synth::ModSource::Env2, synth::ModSource::Env3, synth::ModSource::Env4,
    synth::ModSource::Velocity, synth::ModSource::KeyTrack, synth::ModSource::Random, synth::ModSource::Chaos,
    synth::ModSource::Adsr1, synth::ModSource::Adsr2, synth::ModSource::Adsr3, synth::ModSource::Adsr4,
    synth::ModSource::Unit
};

constexpr synth::ModDestination kGridDestPool[] = {
    synth::ModDestination::Amp, synth::ModDestination::Freq, synth::ModDestination::Phase,
    synth::ModDestination::MetaMorph, synth::ModDestination::MetaWarp, synth::ModDestination::MetaPan,
    synth::ModDestination::TrackGain, synth::ModDestination::TrackPan,
    synth::ModDestination::PitchOct, synth::ModDestination::PitchSem, synth::ModDestination::PitchFine
};

// Wider pools for the ROUTES card pickers (no taken-filter there).
constexpr synth::ModSource kCardSourcePool[] = {
    synth::ModSource::Lfo1, synth::ModSource::Lfo2, synth::ModSource::Lfo3, synth::ModSource::Lfo4,
    synth::ModSource::Env1, synth::ModSource::Env2, synth::ModSource::Env3, synth::ModSource::Env4,
    synth::ModSource::Velocity, synth::ModSource::KeyTrack, synth::ModSource::Random, synth::ModSource::Chaos,
    synth::ModSource::Shape, synth::ModSource::Adsr1, synth::ModSource::Adsr2, synth::ModSource::Adsr3,
    synth::ModSource::Adsr4, synth::ModSource::Unit
};
constexpr synth::ModDestination kCardDestPool[] = {
    synth::ModDestination::Amp, synth::ModDestination::Freq, synth::ModDestination::Phase,
    synth::ModDestination::MetaMorph, synth::ModDestination::MetaWarp, synth::ModDestination::MetaPan,
    synth::ModDestination::TrackGain, synth::ModDestination::TrackPan,
    synth::ModDestination::PitchOct, synth::ModDestination::PitchSem, synth::ModDestination::PitchFine,
    synth::ModDestination::PitchCrs, synth::ModDestination::DecayTime, synth::ModDestination::SpectralDecay
};

struct StripGroup
{
    std::string name;
    std::vector<int> memberIndices;
};

struct InsertHit
{
    Rect rect {};
    int trackId = -1;   // >=0 => track insert chain (by id)
    int mergeIdx = -1;  // unused by the new strip grid; retained for menu helpers
    int slot = 0;       // insert index in the chain, or -1 for the "+ add" chip
};

struct RoutePortHit
{
    Rect rect {};
    synth::GridPortRef port {};
    bool output = false;
};

struct RouteNodeHit
{
    Rect rect {};
    uint32_t nodeId = 0;
};

struct WireIntersectionInfo
{
    synth::GridPoint pt {};
    int segIdx = 0;    // which segment of the polyline (0-based) this intersection falls on
    float segT = 0.0f; // parameter within that segment (0..1)
};

struct RouteWireDraft
{
    bool active = false;
    synth::GridPortRef from {};
    std::vector<synth::GridPoint> points {};
    float mouseX = 0.0f;
    float mouseY = 0.0f;
};

struct FxKnobHit
{
    Rect rect {};
    int trackId = -1;
    int mergeIdx = -1;
    int insertIdx = 0;
    int knob = 0;
};

struct FxBtnHit
{
    Rect rect {};
    int trackId = -1;
    int mergeIdx = -1;
    int insertIdx = 0;
};

struct ModHit
{
    Rect rect {};
    int trackId = -1;
    int slot = 0; // slot = -1 means "add"
};

// Clickable mode chip in the global OSC MOD diagram → (carrier track, mod slot).
struct OscModDiagHit
{
    Rect rect {};
    int track = -1;
    int slot = -1;
};

// One clickable zone on a ROUTES card → (rule index, control kind).
struct MatrixCardHit
{
    enum Kind : uint8_t
    {
        Row = 0, Mute, Source, Dest, Depth, Delete, Xfer,
        Weight, BandLo, BandHi, Mask, MaskAxis, Target
    };
    Rect rect {};
    int rule = -1;
    uint8_t kind = Row;
};

struct MatrixCell
{
    Rect rect;
    synth::ModSource src;
    synth::ModDestination dst;
};

enum class DragTarget
{
    None,
    Attack, Decay, Sustain, Release, Curve, Gain,
    PartialCount, Inharmonic,
    TrackGain, TrackPan, TrackSend, PartialAmp, PartialRatio, BasicPulse, BasicSub, NoiseColor,
    SourceGain, SourcePan, SourceFilterCutoff, SourceFilterResonance, SourceFilterDrive, SourceFilterFeedback,
    SourceFilterMix,
    UnisonVoices, UnisonDetune, UnisonWidth, UnisonPhase,
    MetaRatio, MetaAmp, MetaPhase, MetaPhaseRand, MetaPan, MetaFrameCount, MetaMorph, MetaMorphSlider, MetaWarpAmount,
    MetaPitchOct, MetaPitchSem, MetaPitchFin, MetaPitchCrs,
    MetaFrameScan, MetaWaveform, MetaHarmonicRatio, MetaHarmonicAmp, MetaHarmonicPhase,
    PartialTableAmp, PartialTablePhase,
    MatrixEnvCurve, MatrixEnvSeg, ModEnvRate, AmpAdsrSeg, HarmonicEditor, MetaTimeEditor, MetaSpectrumEditor,
    RuleDepth, RuleBandLo, RuleBandHi, RuleXfer, MatrixRoutesScroll,
    ModDepth,
    ChaosRate, ChaosAmount, ShapePhase, ShapeRho, ShapeUp, ShapeDown,
    EqLow, EqMid, EqHigh, EqDrive,
    FilterCutoff, FilterResonance, FilterDrive, UiScale, ManualCycleLength,
    MetaFrameScroll,
    LayoutVSplit, LayoutRackSplit, LayoutStripSplit,
    StripScroll, ModEntryDepth,
    FxInsertKnob
};

enum class MetaEditorDomain
{
    Time,
    Spectrum
};

enum class PresetNameEditTarget
{
    None,
    Synth,
    Wavetable
};

struct Kwt2Header
{
    char magic[4] {'K', 'W', 'T', '2'};
    uint32_t frameCount = 0;
    uint32_t binCount = 0;
};

struct Kwt2PackedBin
{
    uint16_t amplitude = 0;
    int16_t phase = 0;
};

inline float clampf(float value, float lo, float hi)
{
    return std::max(lo, std::min(hi, value));
}

inline int clampi(int value, int lo, int hi)
{
    return std::max(lo, std::min(hi, value));
}

inline float cutoffToNorm(float hz)
{
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    return clampf((std::log(clampf(hz, 20.0f, 20000.0f)) - lo) / (hi - lo), 0.0f, 1.0f);
}

inline float normToCutoff(float norm)
{
    const float lo = std::log(20.0f);
    const float hi = std::log(20000.0f);
    return std::exp(lo + clampf(norm, 0.0f, 1.0f) * (hi - lo));
}

inline int noteForComputerKey(uint key)
{
    if(key >= 'A' && key <= 'Z')
        key += 'a' - 'A';
    static constexpr char keys[] = "zsxdcvgbhnjmq2w3er5t6y7ui";
    for(size_t i = 0; i + 1 < sizeof(keys); ++i)
        if(uint(keys[i]) == key)
            return 48 + int(i);
    return -1;
}

inline Color rgba(uint32_t packed)
{
    return Color(int((packed >> 24) & 0xffu),
                 int((packed >> 16) & 0xffu),
                 int((packed >> 8) & 0xffu),
                 float(packed & 0xffu) / 255.0f);
}

// Lighten (amount>0) or darken (amount<0) a color, preserving alpha.
inline Color shade(Color c, float amount)
{
    if(amount >= 0.0f)
    {
        c.red   += (1.0f - c.red)   * amount;
        c.green += (1.0f - c.green) * amount;
        c.blue  += (1.0f - c.blue)  * amount;
    }
    else
    {
        const float k = 1.0f + amount;
        c.red   *= k;
        c.green *= k;
        c.blue  *= k;
    }
    return c;
}

struct DesignTokens
{
    static Color appBackground()     { return rgba(0x0b1014ff); }
    static Color panelBackground()   { return rgba(0x11181eff); }
    static Color panelRaised()       { return rgba(0x151e24ff); }
    static Color controlBackground() { return rgba(0x1a242bff); }
    static Color border()            { return rgba(0x26323aff); }
    static Color divider()           { return rgba(0x202a31ff); }
    static Color textPrimary()       { return rgba(0xe5e9ebff); }
    static Color textSecondary()     { return rgba(0x8d9aa2ff); }
    static Color accentCyan()        { return rgba(0x62d7dfff); }
    static Color accentGreen()       { return rgba(0x8bea62ff); }
    static Color accentBlue()        { return rgba(0x4aa8e8ff); }

    // Industrial-panel pass: square-ish corners (2-4px, not the old 5-8px rounded
    // cards), panels/dividers carry the visual weight instead of layered rounded
    // boxes.
    static constexpr float panelRadius = 3.0f;
    static constexpr float controlRadius = 2.0f;
    static constexpr float borderWidth = 1.0f;
    static constexpr float knobStart = 3.0f * kPi / 4.0f;
    static constexpr float knobSweep = 3.0f * kPi / 2.0f;

    // Metal-groove tone used for recessed tracks/insets (darker than divider()).
    static Color groove() { return rgba(0x0c1215ff); }
};

inline bool isBlackKey(int note)
{
    const int pc = note % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

// Colour code for OSC MOD wires / badges: amp-domain mods cool, phase-domain warm,
// sync red — the wire alone tells the mode at a glance.
inline Color oscModTypeColor(synth::SourceModType t)
{
    switch(t)
    {
        case synth::SourceModType::AM:       return rgba(0x62d7dfff); // cyan
        case synth::SourceModType::RingMod:  return rgba(0x4aa8e8ff); // blue
        case synth::SourceModType::FM:       return rgba(0xe8b34aff); // amber
        case synth::SourceModType::PM:       return rgba(0xc070e0ff); // purple
        case synth::SourceModType::HardSync: return rgba(0xef6a5aff); // red
    }
    return rgba(0x62d7dfff);
}

inline const char *sourceName(synth::ModSource s)
{
    switch(s)
    {
        case synth::ModSource::None: return "None";
        // Unified modulators: Lfo1-4 / Env1-4 are presented as MOD1-8.
        case synth::ModSource::Lfo1: return "MOD1";
        case synth::ModSource::Lfo2: return "MOD2";
        case synth::ModSource::Lfo3: return "MOD3";
        case synth::ModSource::Lfo4: return "MOD4";
        case synth::ModSource::Env1: return "MOD5";
        case synth::ModSource::Env2: return "MOD6";
        case synth::ModSource::Env3: return "MOD7";
        case synth::ModSource::Env4: return "MOD8";
        case synth::ModSource::Velocity: return "Velocity";
        case synth::ModSource::KeyTrack: return "Key";
        case synth::ModSource::Random: return "Random";
        case synth::ModSource::Adsr: return "ADSR";
        case synth::ModSource::GeneratorSelf: return "Self";
        case synth::ModSource::Chaos: return "Chaos";
        case synth::ModSource::Shape: return "Shape";
        case synth::ModSource::Adsr1: return "AE1";
        case synth::ModSource::Adsr2: return "AE2";
        case synth::ModSource::Adsr3: return "AE3";
        case synth::ModSource::Adsr4: return "AE4";
        case synth::ModSource::Unit: return "ONE";
    }
    return "Source";
}

inline const char *destName(synth::ModDestination d)
{
    switch(d)
    {
        case synth::ModDestination::Amp: return "Amp";
        case synth::ModDestination::Freq: return "Freq";
        case synth::ModDestination::Phase: return "Phase";
        case synth::ModDestination::DecayTime: return "Decay";
        case synth::ModDestination::SpectralDecay: return "Spectral";
        case synth::ModDestination::TrackGain: return "Gain";
        case synth::ModDestination::TrackPan: return "Pan";
        case synth::ModDestination::PitchOct: return "Oct";
        case synth::ModDestination::PitchSem: return "Sem";
        case synth::ModDestination::PitchFine: return "Fine";
        case synth::ModDestination::PitchCrs: return "Crs";
        case synth::ModDestination::MetaMorph: return "Morph";
        case synth::ModDestination::MetaWarp: return "Warp";
        case synth::ModDestination::MetaPan: return "Osc Pan";
        case synth::ModDestination::InsertP0: return "Fx P1";
        case synth::ModDestination::InsertP1: return "Fx P2";
        case synth::ModDestination::InsertP2: return "Fx P3";
        case synth::ModDestination::InsertP3: return "Fx P4";
    }
    return "Dest";
}


inline const char *warpModeName(synth::WavetableWarpMode mode)
{
    switch(mode)
    {
        case synth::WavetableWarpMode::None: return "Warp None";
        case synth::WavetableWarpMode::Bend: return "Warp Bend";
        case synth::WavetableWarpMode::Squeeze: return "Warp Squeeze";
        case synth::WavetableWarpMode::Skew: return "Warp Skew";
    }
    return "Warp";
}

inline const char *freqShapeName(synth::FreqShape shape)
{
    switch(shape)
    {
        case synth::FreqShape::Harmonic: return "Harmonic";
        case synth::FreqShape::Linear: return "Linear";
        case synth::FreqShape::Exponential: return "Exp";
    }
    return "Mode";
}

inline const char *wavetableImportModeName(synth::WavetableImportMode mode)
{
    switch(mode)
    {
        case synth::WavetableImportMode::AutoDetect: return "Detect Automatically";
        case synth::WavetableImportMode::FixedFrames: return "Fixed 2048 Frames";
        case synth::WavetableImportMode::SingleCycle: return "Single Cycle";
        case synth::WavetableImportMode::ConstantPitch: return "Constant Pitch";
        case synth::WavetableImportMode::ManualCycleLength: return "Manual Cycle Length";
    }
    return "Detect Automatically";
}

inline bool fxHasMode(int kind)
{
    return kind == InsertFilter || kind == InsertDist || kind == InsertDelay || kind == InsertConvReverb;
}

inline synth::MultibandSlotParams &ensureMultibandParams(InsertEffect &e)
{
    if(!e.multiband)
        e.multiband = std::make_shared<synth::MultibandSlotParams>();
    if(e.multiband->highXoverHz <= e.multiband->lowXoverHz + 20.0f)
        e.multiband->highXoverHz = std::min(20000.0f, e.multiband->lowXoverHz + 1000.0f);
    return *e.multiband;
}

inline const char *fxKnobName(int kind, int i)
{
    static const char *F[4] = { "Cutoff", "Q", "Drive", "Mix" };
    static const char *D[4] = { "Drive", "Bias", "Mix", "Out" };
    static const char *E[4] = { "Low", "Mid", "High", "MidHz" };
    static const char *C[4] = { "Thr", "Ratio", "Atk", "Makeup" };
    static const char *L[4] = { "Time", "FB", "Mix", "Tone" };
    static const char *R[4] = { "Size", "Decay", "Mix", "Damp" };
    static const char *V[4] = { "Mix", "Gain", "PreDly", "-" };
    static const char *M[4] = { "Low/Mid", "Mid/High", "-", "-" };
    switch(kind)
    {
        case InsertFilter: return F[i];
        case InsertDist: return D[i];
        case InsertEq: return E[i];
        case InsertComp: return C[i];
        case InsertDelay: return L[i];
        case InsertConvReverb: return V[i];
        case InsertMultiband: return M[i];
        case InsertReverb: return R[i];
        default: return R[i];
    }
}

inline float fxKnobNorm(const InsertEffect &e, int i)
{
    switch(e.kind)
    {
        case InsertFilter: {
            const auto &f = e.filter;
            switch(i)
            {
                case 0: return std::log10(std::max(20.0f, f.cutoffHz) / 20.0f) / std::log10(1000.0f);
                case 1: return clampf((f.resonance - 0.05f) / 9.95f, 0.0f, 1.0f);
                case 2: return clampf((f.drive - 1.0f) / 15.0f, 0.0f, 1.0f);
                default: return f.mix;
            }
        }
        case InsertDist: {
            const auto &d = e.dist;
            switch(i)
            {
                case 0: return clampf((d.drive - 1.0f) / 31.0f, 0.0f, 1.0f);
                case 1: return (d.bias + 1.0f) * 0.5f;
                case 2: return d.mix;
                default: return d.outGain * 0.5f;
            }
        }
        case InsertEq: {
            const auto &q = e.eq;
            switch(i)
            {
                case 0: return (q.lowDb + 18.0f) / 36.0f;
                case 1: return (q.midDb + 18.0f) / 36.0f;
                case 2: return (q.highDb + 18.0f) / 36.0f;
                default: return std::log10(std::max(80.0f, q.midHz) / 80.0f) / std::log10(8000.0f / 80.0f);
            }
        }
        case InsertComp: {
            const auto &c = e.comp;
            switch(i)
            {
                case 0: return (c.threshDb + 48.0f) / 48.0f;
                case 1: return (c.ratio - 1.0f) / 19.0f;
                case 2: return c.attackMs / 100.0f;
                default: return c.makeupDb / 24.0f;
            }
        }
        case InsertDelay: {
            const auto &l = e.delay;
            switch(i)
            {
                case 0: return clampf(l.timeMs / 1000.0f, 0.0f, 1.0f);
                case 1: return l.feedback / 0.95f;
                case 2: return l.mix;
                default: return l.tone;
            }
        }
        case InsertConvReverb: {
            const auto &v = e.conv;
            switch(i)
            {
                case 0: return v.mix;
                case 1: return v.gain * 0.5f;
                case 2: return v.predelayMs / 200.0f;
                default: return 0.0f;
            }
        }
        case InsertMultiband: {
            const auto *m = e.multiband.get();
            const float lo = m ? m->lowXoverHz : 250.0f;
            const float hi = m ? m->highXoverHz : 2500.0f;
            switch(i)
            {
                case 0: return cutoffToNorm(lo);
                case 1: return cutoffToNorm(hi);
                default: return 0.0f;
            }
        }
        default: {
            const auto &r = e.reverb;
            switch(i)
            {
                case 0: return r.size;
                case 1: return r.decay / 0.92f;
                case 2: return r.mix;
                default: return r.damp;
            }
        }
    }
}

inline float fxKnobDisp(const InsertEffect &e, int i)
{
    switch(e.kind)
    {
        case InsertFilter: {
            const auto &f = e.filter;
            switch(i) { case 0: return f.cutoffHz; case 1: return f.resonance; case 2: return f.drive; default: return f.mix; }
        }
        case InsertDist: {
            const auto &d = e.dist;
            switch(i) { case 0: return d.drive; case 1: return d.bias; case 2: return d.mix; default: return d.outGain; }
        }
        case InsertEq: {
            const auto &q = e.eq;
            switch(i) { case 0: return q.lowDb; case 1: return q.midDb; case 2: return q.highDb; default: return q.midHz; }
        }
        case InsertComp: {
            const auto &c = e.comp;
            switch(i) { case 0: return c.threshDb; case 1: return c.ratio; case 2: return c.attackMs; default: return c.makeupDb; }
        }
        case InsertDelay: {
            const auto &l = e.delay;
            switch(i) { case 0: return l.timeMs; case 1: return l.feedback; case 2: return l.mix; default: return l.tone; }
        }
        case InsertConvReverb: {
            const auto &v = e.conv;
            switch(i) { case 0: return v.mix; case 1: return v.gain; case 2: return v.predelayMs; default: return 0.0f; }
        }
        case InsertMultiband: {
            const auto *m = e.multiband.get();
            switch(i)
            {
                case 0: return m ? m->lowXoverHz : 250.0f;
                case 1: return m ? m->highXoverHz : 2500.0f;
                default: return 0.0f;
            }
        }
        default: {
            const auto &r = e.reverb;
            switch(i) { case 0: return r.size; case 1: return r.decay; case 2: return r.mix; default: return r.damp; }
        }
    }
}

inline void fxKnobSetNorm(InsertEffect &e, int i, float n)
{
    n = clampf(n, 0.0f, 1.0f);
    switch(e.kind)
    {
        case InsertFilter: {
            auto &f = e.filter;
            switch(i) { case 0: f.cutoffHz = 20.0f * std::pow(1000.0f, n); break; case 1: f.resonance = 0.05f + n * 9.95f; break; case 2: f.drive = 1.0f + n * 15.0f; break; default: f.mix = n; break; }
            break;
        }
        case InsertDist: {
            auto &d = e.dist;
            switch(i) { case 0: d.drive = 1.0f + n * 31.0f; break; case 1: d.bias = n * 2.0f - 1.0f; break; case 2: d.mix = n; break; default: d.outGain = n * 2.0f; break; }
            break;
        }
        case InsertEq: {
            auto &q = e.eq;
            switch(i) { case 0: q.lowDb = n * 36.0f - 18.0f; break; case 1: q.midDb = n * 36.0f - 18.0f; break; case 2: q.highDb = n * 36.0f - 18.0f; break; default: q.midHz = 80.0f * std::pow(8000.0f / 80.0f, n); break; }
            break;
        }
        case InsertComp: {
            auto &c = e.comp;
            switch(i) { case 0: c.threshDb = n * 48.0f - 48.0f; break; case 1: c.ratio = 1.0f + n * 19.0f; break; case 2: c.attackMs = n * 100.0f; break; default: c.makeupDb = n * 24.0f; break; }
            break;
        }
        case InsertDelay: {
            auto &l = e.delay;
            switch(i) { case 0: l.timeMs = n * 1000.0f; break; case 1: l.feedback = n * 0.95f; break; case 2: l.mix = n; break; default: l.tone = n; break; }
            break;
        }
        case InsertConvReverb: {
            auto &v = e.conv;
            switch(i) { case 0: v.mix = n; break; case 1: v.gain = n * 2.0f; break; case 2: v.predelayMs = n * 200.0f; break; default: break; }
            break;
        }
        case InsertMultiband: {
            auto &m = ensureMultibandParams(e);
            if(i == 0)
            {
                m.lowXoverHz = std::min(normToCutoff(n), m.highXoverHz - 20.0f);
            }
            else if(i == 1)
            {
                m.highXoverHz = std::max(normToCutoff(n), m.lowXoverHz + 20.0f);
            }
            break;
        }
        default: {
            auto &r = e.reverb;
            switch(i) { case 0: r.size = n; break; case 1: r.decay = n * 0.92f; break; case 2: r.mix = n; break; default: r.damp = n; break; }
            break;
        }
    }
}

inline const char *insertTypeName(int kind)
{
    static const char *names[9] = { "", "Filter", "Distortion", "EQ", "Compressor", "Delay", "Reverb", "IR Reverb", "XOver" };
    return (kind >= 1 && kind <= 8) ? names[kind] : "";
}

inline const char *fxModeTitle(int kind)
{
    if(kind == InsertFilter) return "Filter mode";
    if(kind == InsertDist) return "Dist mode";
    if(kind == InsertDelay) return "Delay mode";
    if(kind == InsertConvReverb) return "Impulse (presets/irs)";
    return "Mode";
}

inline int hexDigit(char value)
{
    if(value >= '0' && value <= '9') return value - '0';
    if(value >= 'a' && value <= 'f') return value - 'a' + 10;
    if(value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline std::string localPathFromUri(std::string uri)
{
    while(!uri.empty() && (uri.back() == '\r' || uri.back() == '\n' || uri.back() == '\0'))
        uri.pop_back();
    if(uri.rfind("file://", 0) == 0)
    {
        uri.erase(0, 7);
        if(!uri.empty() && uri[0] != '/')
        {
            const size_t slash = uri.find('/');
            if(slash == std::string::npos)
                return {};
            uri.erase(0, slash);
        }
    }
    std::string path;
    path.reserve(uri.size());
    for(size_t i = 0; i < uri.size(); ++i)
    {
        if(uri[i] == '%' && i + 2 < uri.size())
        {
            const int hi = hexDigit(uri[i + 1]);
            const int lo = hexDigit(uri[i + 2]);
            if(hi >= 0 && lo >= 0)
            {
                path.push_back(char((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        path.push_back(uri[i]);
    }
    return path;
}

inline bool hasWavExtension(const std::string &path)
{
    if(path.size() < 4)
        return false;
    std::string ext = path.substr(path.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    return ext == ".wav";
}
