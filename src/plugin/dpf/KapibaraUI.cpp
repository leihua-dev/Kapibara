#include "DistrhoUI.hpp"

#include "KapibaraPlugin.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#ifndef DGL_NO_SHARED_RESOURCES
#include "NanoVG.hpp"
#endif

START_NAMESPACE_DISTRHO

namespace
{
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

enum class DragTarget
{
    None,
    Attack, Decay, Sustain, Release, Curve, Gain,
    PartialCount, Inharmonic,
    TrackGain, TrackPan, TrackSend, PartialAmp, PartialRatio, BasicPulse, BasicSub, NoiseColor,
    SourceGain, SourcePan, SourceFilterCutoff, SourceFilterResonance, SourceFilterDrive, SourceFilterFeedback,
    SourceFilterMix,
    UnisonVoices, UnisonDetune, UnisonWidth, UnisonPhase,
    MetaRatio, MetaAmp, MetaPhase, MetaPan, MetaFrameCount, MetaMorph, MetaWarpAmount,
    MetaPitchOct, MetaPitchSem, MetaPitchFin, MetaPitchCrs,
    MetaFrameScan, MetaWaveform, MetaHarmonicRatio, MetaHarmonicAmp, MetaHarmonicPhase,
    PartialTableAmp, PartialTablePhase,
    LfoFreq, LfoPhase, LfoRho,
    EnvPointA, EnvPointB, EnvCurveA, MatrixEnvCurve, MatrixEnvSeg, ModEnvRate, HarmonicEditor, MetaTimeEditor, MetaSpectrumEditor,
    RuleDepth, RuleBandLo, RuleBandHi,
    ModDepth,
    ChaosRate, ChaosAmount, ShapePhase, ShapeRho, ShapeUp, ShapeDown,
    EqLow, EqMid, EqHigh, EqDrive,
    FilterCutoff, FilterResonance, FilterDrive, UiScale, ManualCycleLength,
    OpParamA, OpParamB, OpParamC, OpParamD, MetaFrameScroll,
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

float clampf(float value, float lo, float hi)
{
    return std::max(lo, std::min(hi, value));
}

int clampi(int value, int lo, int hi)
{
    return std::max(lo, std::min(hi, value));
}

Color rgba(uint32_t packed)
{
    return Color(int((packed >> 24) & 0xffu),
                 int((packed >> 16) & 0xffu),
                 int((packed >> 8) & 0xffu),
                 float(packed & 0xffu) / 255.0f);
}

// Lighten (amount>0) or darken (amount<0) a color, preserving alpha.
Color shade(Color c, float amount)
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

    static constexpr float panelRadius = 8.0f;
    static constexpr float controlRadius = 5.0f;
    static constexpr float borderWidth = 1.0f;
    static constexpr float knobStart = 3.0f * kPi / 4.0f;
    static constexpr float knobSweep = 3.0f * kPi / 2.0f;
};

bool isBlackKey(int note)
{
    const int pc = note % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

const char *lfoShapeName(synth::LfoShape shape)
{
    switch(shape)
    {
        case synth::LfoShape::Asymmetric: return "Asym";
        case synth::LfoShape::Sine: return "Sine";
        case synth::LfoShape::Square: return "Square";
        case synth::LfoShape::Triangle: return "Tri";
        case synth::LfoShape::SampleHold: return "S/H";
    }
    return "LFO";
}

const char *sourceName(synth::ModSource s)
{
    switch(s)
    {
        case synth::ModSource::None: return "None";
        case synth::ModSource::Lfo1: return "LFO1";
        case synth::ModSource::Lfo2: return "LFO2";
        case synth::ModSource::Lfo3: return "LFO3";
        case synth::ModSource::Lfo4: return "LFO4";
        case synth::ModSource::Env1: return "ENV1";
        case synth::ModSource::Env2: return "ENV2";
        case synth::ModSource::Env3: return "ENV3";
        case synth::ModSource::Env4: return "ENV4";
        case synth::ModSource::Velocity: return "Velocity";
        case synth::ModSource::KeyTrack: return "Key";
        case synth::ModSource::Random: return "Random";
        case synth::ModSource::Adsr: return "ADSR";
        case synth::ModSource::GeneratorSelf: return "Self";
        case synth::ModSource::Chaos: return "Chaos";
        case synth::ModSource::Shape: return "Shape";
        case synth::ModSource::Adsr1: return "AENV1";
        case synth::ModSource::Adsr2: return "AENV2";
        case synth::ModSource::Adsr3: return "AENV3";
        case synth::ModSource::Adsr4: return "AENV4";
    }
    return "Source";
}

const char *destName(synth::ModDestination d)
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


const char *warpModeName(synth::WavetableWarpMode mode)
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

const char *freqShapeName(synth::FreqShape shape)
{
    switch(shape)
    {
        case synth::FreqShape::Harmonic: return "Harmonic";
        case synth::FreqShape::Linear: return "Linear";
        case synth::FreqShape::Exponential: return "Exp";
    }
    return "Mode";
}

int hexDigit(char value)
{
    if(value >= '0' && value <= '9') return value - '0';
    if(value >= 'a' && value <= 'f') return value - 'a' + 10;
    if(value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::string localPathFromUri(std::string uri)
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

bool hasWavExtension(const std::string &path)
{
    if(path.size() < 4)
        return false;
    std::string ext = path.substr(path.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    return ext == ".wav";
}
} // namespace

class KapibaraUI final : public UI
{
    // ---- nested types required before inline method signatures ----
    enum class FilterAlgo : uint8_t {
        LP2=0, HP2, BP2, Notch, AP2, AP4, AP8, Peak, LowShelf, HighShelf, LP4, HP4
    };
    static constexpr int kFilterAlgoCount = 12;
    static constexpr const char *kFilterAlgoNames[] = {
        "LP 12","HP 12","BP 12","Notch","APF 2","APF 4","APF 8",
        "Peak","Lo Shelf","Hi Shelf","LP 24","HP 24"
    };
    struct FilterSlotUI {
        bool enabled = false;
        FilterAlgo algo = FilterAlgo::LP2;
        float cutoffHz = 1000.0f;
        float resonance = 0.707f;
        float gainDb = 0.0f;   // kept for Peak/Shelf math, not user-exposed
        float drive = 1.0f;    // 1..16 input saturation
        float mix = 1.0f;
    };
    enum class DistAlgo : uint8_t {
        SoftClip=0, HardClip, Tube, Diode, FoldBack, SineFold, BitCrush, Tanh
    };
    static constexpr int kDistAlgoCount = 8;
    static constexpr const char *kDistAlgoNames[] = {
        "Soft","Hard","Tube","Diode","Fold","SinFld","Crush","Tanh"
    };
    struct DistSlotUI {
        bool enabled = false;
        DistAlgo algo = DistAlgo::SoftClip;
        float drive = 2.0f;   // 1..32
        float bias = 0.0f;    // -1..1 asymmetry
        float mix = 1.0f;     // 0..1
        float outGain = 1.0f; // 0..2
    };
    // Each insert is self-contained (own params); chains are unbounded vectors.
    using InsertEffect = synth::InsertEffect;
    static constexpr int InsertFilter = synth::InsertFilter, InsertDist = synth::InsertDist,
                         InsertEq = synth::InsertEq, InsertComp = synth::InsertComp,
                         InsertDelay = synth::InsertDelay, InsertReverb = synth::InsertReverb,
                         InsertConvReverb = synth::InsertConvReverb;
    static constexpr int kInsertTypeCount = 7; // filter,dist,eq,comp,delay,reverb,IR (kind 1..7)
    struct StripGroup {
        std::string name;
        std::vector<int> memberIndices;
        std::vector<InsertEffect> inserts;
    };
    // A clickable insert-slot button registered during draw.
    struct InsertHit {
        Rect rect {};
        int trackId = -1;   // >=0 => track insert chain (by id)
        int groupIdx = -1;  // >=0 => group insert chain
        int slot = 0;       // insert index in the chain, or -1 for the "+ add" chip
    };
    // Clickable elements inside the flattened route-FX editor.
    struct FxKnobHit { Rect rect {}; int trackId = -1; int groupIdx = -1; int insertIdx = 0; int knob = 0; };
    struct FxBtnHit  { Rect rect {}; int trackId = -1; int groupIdx = -1; int insertIdx = 0; };
  public:
    KapibaraUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
#ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf");
#else
        loadSharedResources();
#endif
        // Lock to a fixed 11:7 aspect ratio (matches the default 1320x840). The
        // min size must share that ratio or the window jumps ratio on resize.
        setGeometryConstraints(1100, 700, true);
        getWindow().setIgnoringKeyRepeat(true);
        computerKeys_.fill(false);
        pressedKeycodeNotes_.fill(-1);
        pullFromPlugin();
        pushGroups();
    }

  protected:
    void onNanoDisplay() override
    {
        metaFramesShown_ = false; // set true by any frame strip drawn this frame
        updateLetterbox();
        // Fill the whole real window with the letterbox border colour.
        beginPath();
        rect(0.0f, 0.0f, realW_, realH_);
        fillColor(rgba(0x05070aff));
        fill();
        // Draw the fixed-aspect UI inside the centered letterbox region.
        save();
        translate(lbX_, lbY_);
        scale(uiRenderScale_, uiRenderScale_);
        drawBackground();
        drawToolbar();
        drawCurrentPage();
        drawModulationOverlays();
        drawPresetMenu();
        drawOptionsMenu();
        drawHarmonicEditor();
        drawKeyboard();
        drawWavetablePresetMenu();
        drawMetaProcessContextMenu();
        drawRouteContextMenu();
        drawStripGroupContextMenu();
        drawInsertMenu();
        drawModeMenu();
        drawModSourceMenu();
        drawWavetableImportMenu();
        drawGridAxisPicker();
        drawInsertDragGhost();
        restore();
    }

    // Floating label that follows the cursor while reordering a strip insert.
    void drawInsertDragGhost()
    {
        if(!insertDragActive_ || insertPendSlot_ < 0)
            return;
        auto *chain = insertChainFor(insertPendTrackId_, insertPendGroup_);
        if(chain == nullptr || insertPendSlot_ >= int(chain->size()))
            return;
        const auto &ins = (*chain)[(size_t)insertPendSlot_];
        const Rect g { insertDragX_ + 8.0f, insertDragY_ - 8.0f, 70.0f, 16.0f };
        drawPanel(g, rgba(0x17242cf0), rgba(0x9eff50ffU));
        fontSize(8.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(g.x + g.w * 0.5f, g.y + g.h * 0.5f, insertTypeName(ins.kind), nullptr);
    }

    void onResize(const ResizeEvent &ev) override
    {
        UI::onResize(ev);
        realW_ = float(ev.size.getWidth());
        realH_ = float(ev.size.getHeight());
        updateLetterbox();
        repaint();
    }

    // Layout is authored in a FIXED logical canvas (1320x840). The whole UI is then
    // uniformly scaled to fill the real window, so fonts and controls grow together
    // instead of the panels stretching while text/knobs stay tiny.
    static constexpr float kCanvasW = 1320.0f;
    static constexpr float kCanvasH = 900.0f;
    static constexpr float kUiAspect = kCanvasW / kCanvasH;
    float uiW() const { return kCanvasW; }
    float uiH() const { return kCanvasH; }
    void updateLetterbox()
    {
        const float rw = realW_ > 1.0f ? realW_ : float(DISTRHO_UI_DEFAULT_WIDTH);
        const float rh = realH_ > 1.0f ? realH_ : float(DISTRHO_UI_DEFAULT_HEIGHT);
        // Largest uniform scale that keeps the 11:7 canvas inside the window.
        uiRenderScale_ = std::min(rw / kCanvasW, rh / kCanvasH);
        lbW_ = kCanvasW * uiRenderScale_;
        lbH_ = kCanvasH * uiRenderScale_;
        lbX_ = (rw - lbW_) * 0.5f;
        lbY_ = (rh - lbH_) * 0.5f;
    }

    // 周期性刷新，驱动 LFO / 失真曲线的时变动画与实时波形预览
    void uiIdle() override
    {
        animPhase_ += 0.12f;
        if(animPhase_ > 1.0e6f) animPhase_ = 0.0f;
        repaint();
    }

    void uiFocus(bool focus, DGL_NAMESPACE::CrossingMode mode) override
    {
        (void)mode;
        if(!focus)
        {
            releaseAllUiNotes();
            modRouteDragActive_ = false;
            modRouteDragMoved_ = false;
            modRouteHover_ = {};
            dragTarget_ = DragTarget::None;
            repaint();
        }
    }

    bool onKeyboard(const KeyboardEvent &ev) override
    {
        if(loadPathEditing_)
        {
            if(!ev.press)
                return true;
            if(ev.key == kKeyEnter)
            {
                commitWavetableLoad();
                repaint();
                return true;
            }
            if(ev.key == kKeyEscape)
            {
                loadPathEditing_ = false;
                repaint();
                return true;
            }
            if(ev.key == kKeyBackspace)
            {
                if(!loadPathBuffer_.empty())
                    loadPathBuffer_.pop_back();
                repaint();
                return true;
            }
            return true;
        }
        if(presetNameEditing_)
        {
            if(!ev.press)
                return true;
            if(ev.key == kKeyEnter)
            {
                commitPresetNameEdit();
                repaint();
                return true;
            }
            if(ev.key == kKeyEscape)
            {
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                repaint();
                return true;
            }
            if(ev.key == kKeyBackspace)
            {
                if(!presetNameBuffer_.empty())
                    presetNameBuffer_.pop_back();
                repaint();
                return true;
            }
            if(ev.key >= 32 && ev.key <= 126)
            {
                appendPresetNameChar(char(ev.key));
                skipNextPresetCharacterInput_ = true;
                repaint();
                return true;
            }
            return true;
        }
        if(ev.press && ev.key == kKeyEnter)
        {
            if(presetMenuOpen_)
            {
                beginSynthPresetRename();
                repaint();
                return true;
            }
            if(wavetablePresetMenuOpen_)
            {
                beginWavetablePresetRename();
                repaint();
                return true;
            }
            if(modeMenuOpen_ && modeMenuKind_ == InsertConvReverb)
            {
                commitModeMenuSelection();
                repaint();
                return true;
            }
        }

        // Control is held? Match 'a'/'A' and the control-code (Ctrl+A -> 0x01).
        const bool ctrlHeld = (ev.mod & kModifierControl) != 0;
        const auto ctrlKey = [&](char base) {
            const uint b = uint(base);
            return ev.key == b || ev.key == uint(base - 'a' + 'A') || ev.key == uint(base - 'a' + 1);
        };
        // Ctrl+A selects all frames; Ctrl+click defines a range (mouse) — both in the
        // meta wavetable editor / wherever a frame strip is visible.
        const bool metaFrameCtx = harmonicEditorOpen_ || metaFramesShown_;
        if(ev.press && metaFrameCtx && ctrlHeld && ctrlKey('a'))
        {
            selectAllMetaFrames();
            repaint();
            return true;
        }
        if(ev.press && metaFrameCtx && ctrlHeld && ctrlKey('z'))
        {
            if(undoMeta())
                repaint();
            return true;
        }
        if(ev.press && harmonicEditorOpen_
           && (ev.key == kKeyDelete || ev.key == kKeyBackspace))
        {
            performMetaFrameAction(2);
            repaint();
            return true;
        }
        // Delete / Backspace removes the selected source strip (replaces the old DEL button).
        if(ev.press && !harmonicEditorOpen_ && !loadPathEditing_ && !presetNameEditing_
           && (ev.key == kKeyDelete || ev.key == kKeyBackspace))
        {
            deleteSelectedTrack();
            repaint();
            return true;
        }

        // Disable the computer-keyboard MIDI piano while the meta wavetable editor is
        // open, so letter keys (and Ctrl combos) drive editing, not notes.
        if(harmonicEditorOpen_)
            return true;

        const int note = noteForComputerKey(ev.key);
        if(note < 0)
            return false;
        const int keySlot = keycodeSlot(ev.keycode);

        if(ev.press)
        {
            if(keySlot >= 0 && pressedKeycodeNotes_[(size_t)keySlot] == note)
                return true;

            if(keySlot >= 0 && pressedKeycodeNotes_[(size_t)keySlot] >= 0)
                releaseComputerNote(pressedKeycodeNotes_[(size_t)keySlot], keySlot);

            if(!computerKeys_[(size_t)note])
            {
                computerKeys_[(size_t)note] = true;
                if(auto *p = plugin())
                    p->previewNoteOn(note, 0.82f);
            }
            if(keySlot >= 0)
                pressedKeycodeNotes_[(size_t)keySlot] = note;
        }
        else
        {
            if(keySlot >= 0)
            {
                if(pressedKeycodeNotes_[(size_t)keySlot] == note)
                    releaseComputerNote(note, keySlot);
            }
            else if(computerKeys_[(size_t)note])
            {
                releaseComputerNote(note, -1);
            }
        }
        repaint();
        return true;
    }

    bool onCharacterInput(const CharacterInputEvent &ev) override
    {
        if(presetNameEditing_)
        {
            if(skipNextPresetCharacterInput_)
            {
                skipNextPresetCharacterInput_ = false;
                return true;
            }
            if(ev.string[0] != '\0')
            {
                for(const char *p = ev.string; *p != '\0'; ++p)
                    appendPresetNameChar(*p);
                repaint();
            }
            return true;
        }
        if(!loadPathEditing_)
            return false;
        if(ev.string[0] != '\0')
        {
            loadPathBuffer_ += ev.string;
            if(loadPathBuffer_.size() > 512)
                loadPathBuffer_.resize(512);
            repaint();
        }
        return true;
    }

    bool onMouse(const MouseEvent &ev) override
    {
        const float x = (static_cast<float>(ev.pos.getX()) - lbX_) / uiRenderScale_;
        const float y = (static_cast<float>(ev.pos.getY()) - lbY_) / uiRenderScale_;

        if(!ev.press)
        {
            if(insertPending_)
            {
                finishInsertInteraction(x, y);
                repaint();
                return true;
            }
            if(modRouteDragActive_)
            {
                if(modRouteDragMoved_)
                    finishModRouteDrag(x, y);
                modRouteDragActive_ = false;
                modRouteDragMoved_ = false;
                modRouteHover_ = {};
                repaint();
                return true;
            }
            releaseMouseKey();
            if(metaEditorDirty_)
            {
                metaEditorDirty_ = false;
                pushCurrentTrack();
            }
            // Env-curve edits are published once on release (drag stays smooth — the
            // per-motion full-snapshot publish was the source of the lag).
            if((dragTarget_ == DragTarget::MatrixEnvCurve || dragTarget_ == DragTarget::MatrixEnvSeg)
               && matrixEnvDirty_)
            {
                matrixEnvDirty_ = false;
                pushCurCurve();
            }
            // Flush the exact final value. During drag, PartialBank Partials and
            // Inharmonic are already pushed at UI-frame cadence for live notes.
            if(deferTrackPush_) { deferTrackPush_ = false; pushCurrentTrackDuringRealtimeDrag(true); }
            if(deferGenPush_)   { deferGenPush_ = false; pushGeneratorDuringRealtimeDrag(true); }
            dragTarget_ = DragTarget::None;
            dragTrackIndex_ = -1;
            prevTimeEditX_ = -1.0f;
            prevTimeEditY_ = -1.0f;
            return true;
        }

        if(ev.button == kMouseButtonRight && harmonicEditorOpen_
           && currentTrack() != nullptr
           && currentTrack()->type == synth::SourceTrackType::MetaOscillator)
        {
            openMetaProcessContextMenu(x, y);
            repaint();
            return true;
        }
        if(ev.button == kMouseButtonRight)
        {
            // Right-click a matrix grid node → clear that route.
            if(handleMatrixGridDelete(x, y))
            {
                repaint();
                return true;
            }
            // Right-click a grid axis label → remove that source/destination row/col.
            for(size_t s = 0; s < gridSrcLabelRects_.size() && s < gridSources_.size(); ++s)
                if(gridSrcLabelRects_[s].contains(x, y))
                {
                    gridSources_.erase(gridSources_.begin() + long(s));
                    repaint();
                    return true;
                }
            for(size_t d = 0; d < gridDestLabelRects_.size() && d < gridDests_.size(); ++d)
                if(gridDestLabelRects_[d].contains(x, y))
                {
                    gridDests_.erase(gridDests_.begin() + long(d));
                    repaint();
                    return true;
                }
            // Right-click a strip MOD slot → pick / change the modulation source
            for(const auto &hit : modHits_)
                if(hit.rect.contains(x, y))
                {
                    const int ti = trackIndexOfId(uint32_t(hit.trackId));
                    if(ti >= 0) { selectedTrack_ = ti; selectedGroupView_ = -1; }
                    openModSourceMenu(hit.trackId, hit.slot, x, y);
                    repaint();
                    return true;
                }
            for(int slot = 0; slot < 2; ++slot)
            {
                if(stripRouteRects_[(size_t)slot].contains(x, y)
                   && stripRouteRuleIndices_[(size_t)slot] >= 0)
                {
                    routeContextRuleIndex_ = stripRouteRuleIndices_[(size_t)slot];
                    routeContextX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW())  - 208.0f));
                    routeContextY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - 90.0f));
                    routeContextMenuOpen_ = true;
                    repaint();
                    return true;
                }
            }
            // Right-click on a group bus → ungroup menu
            for(size_t gi = 0; gi < stripGroupBusRects_.size() && gi < stripGroups_.size(); ++gi)
            {
                if(stripGroupBusRects_[gi].contains(x, y))
                {
                    groupContextTargetGroup_ = int(gi);
                    stripGroupContextX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW())  - 220.0f));
                    stripGroupContextY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - 100.0f));
                    stripGroupContextMenuOpen_ = true;
                    repaint();
                    return true;
                }
            }
            // Right-click on a strip opens the strip group context menu
            for(size_t i = 0; i < stripRects_.size(); ++i)
            {
                if(stripRects_[i].contains(x, y) && i < generator_.tracks.size())
                {
                    // Ensure the clicked strip is selected
                    if(!selectedStrips_[i])
                    {
                        selectedStrips_[i] = true;
                        selectedTrack_ = int(i);
                    }
                    groupContextTargetGroup_ = -1;
                    stripGroupContextX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW())  - 220.0f));
                    stripGroupContextY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - 100.0f));
                    stripGroupContextMenuOpen_ = true;
                    repaint();
                    return true;
                }
            }
        }

        if(ev.button != 1)
            return false;

        // Modifier state from the event mask (cleared every press, never sticky).
        ctrlDown_  = (ev.mod & kModifierControl) != 0;
        shiftDown_ = (ev.mod & kModifierShift) != 0;

        currentClickIsDouble_ = (ev.time - lastClickTime_) < 400u
                                && std::abs(x - lastClickX_) < 8.0f
                                && std::abs(y - lastClickY_) < 8.0f;
        lastClickTime_ = ev.time;
        lastClickX_ = x;
        lastClickY_ = y;

        if(handleModDepthPress(x, y))
        {
            repaint();
            return true;
        }

        if(handleWavetablePresetMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleMetaProcessContextMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleRouteContextMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleStripGroupContextMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleInsertMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleModeMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleModSourceMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleWavetableImportMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleHarmonicEditorClick(x, y))
        {
            repaint();
            return true;
        }

        // 双击重置到默认值
        if(currentClickIsDouble_ && handleDoubleClickReset(x, y)) { repaint(); return true; }

        if(handleToolbarClick(x, y) || handlePageClick(x, y) || handleKeyboardPress(x, y))
        {
            repaint();
            return true;
        }

        return false;
    }

    bool onMotion(const MotionEvent &ev) override
    {
        const float x = (static_cast<float>(ev.pos.getX()) - lbX_) / uiRenderScale_;
        const float y = (static_cast<float>(ev.pos.getY()) - lbY_) / uiRenderScale_;
        // Keep snap modifier live during a drag (Shift to disable grid snap).
        shiftDown_ = (ev.mod & kModifierShift) != 0;

        if(insertPending_)
        {
            const float dx = x - insertPendX_, dy = y - insertPendY_;
            if(dx * dx + dy * dy > 25.0f)
                insertDragActive_ = true;
            insertDragX_ = x; insertDragY_ = y;
            repaint();
            return true;
        }

        if(modRouteDragActive_)
        {
            modRouteMouseX_ = x;
            modRouteMouseY_ = y;
            const float dx = x - modRouteStartX_;
            const float dy = y - modRouteStartY_;
            modRouteDragMoved_ = modRouteDragMoved_ || dx * dx + dy * dy > 16.0f;
            modRouteHover_ = modRouteTargetAt(x, y);
            repaint();
            return true;
        }

        if(mouseKey_ >= 0)
        {
            const int key = keyAt(x, y);
            if(key != mouseKey_)
            {
                releaseMouseKey();
                if(key >= 0)
                    pressMouseKey(key);
            }
            return true;
        }

        if(dragTarget_ == DragTarget::None)
            return false;

        applyDragValue(x, y);
        repaint();
        return true;
    }

#if DISTRHO_UI_FILE_BROWSER
    void uiFileBrowserSelected(const char *filename) override
    {
        if(filename == nullptr || filename[0] == '\0')
        {
            fileBrowserSaving_ = false;
            return;
        }
        if(fileBrowserSaving_)
        {
            fileBrowserSaving_ = false;
            saveWavetableToFile(filename);
            repaint();
            return;
        }
        loadPathBuffer_ = filename;
        commitWavetableLoad();
        repaint();
    }
#endif

    uint32_t uiClipboardDataOffer() override
    {
        for(const auto &offer : getClipboardDataOfferTypes())
            if(offer.type != nullptr && std::strcmp(offer.type, "text/uri-list") == 0)
                return offer.id;
        return 0;
    }

    void uiClipboardData(const char *mimeType, const void *data, size_t dataSize) override
    {
        if(mimeType == nullptr || std::strcmp(mimeType, "text/uri-list") != 0 || data == nullptr || dataSize == 0)
            return;
        const std::string uriList(static_cast<const char *>(data), dataSize);
        size_t offset = 0;
        while(offset < uriList.size())
        {
            const size_t end = uriList.find('\n', offset);
            const std::string line = uriList.substr(offset, end == std::string::npos ? std::string::npos : end - offset);
            offset = end == std::string::npos ? uriList.size() : end + 1;
            if(line.empty() || line[0] == '#')
                continue;
            const std::string path = localPathFromUri(line);
            if(!hasWavExtension(path))
                continue;
            auto *track = currentTrack();
            if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            {
                loadStatus_ = "select a Meta Oscillator before dropping WAV";
                metaEditorStatus_ = loadStatus_;
                repaint();
                return;
            }
            loadPathBuffer_ = path;
            droppedWavPending_ = true;
            wavetableImportMenuOpen_ = true;
            metaEditorStatus_ = "select WAV import mode";
            repaint();
            return;
        }
        loadStatus_ = "drop ignored: expected a .wav file";
        metaEditorStatus_ = loadStatus_;
        repaint();
    }

  private:
    KapibaraPlugin *plugin() const
    {
        return static_cast<KapibaraPlugin *>(getPluginInstancePointer());
    }

    void pullFromPlugin()
    {
        auto *p = plugin();
        if(p == nullptr)
            return;

        generator_ = p->generatorParams();
        adsr_ = p->adsrParams();
        if(generator_.tracks.empty())
        {
            synth::SourceTrackParams track;
            track.id = 1;
            track.name = "Partial Bank 1";
            track.type = synth::SourceTrackType::PartialBank;
            track.partialBank = generator_.wavetableSeed;
            track.gain = generator_.sources[0].gain;
            track.pan = generator_.sources[0].pan;
            track.strip = generator_.sources[0];
            track.ampEnvIndex = 0;
            track.unison = generator_.unison;
            track.ampEnvelope = adsr_;
            generator_.tracks.push_back(track);
        }
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        operatorChain_ = p->operatorChain();
        ensureDefaultOperatorChain();
        for(int i = 0; i < synth::kMaxLfos; ++i)
            lfos_[(size_t)i] = p->lfoParams(i);
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
            ampEnvs_[(size_t)i] = p->ampEnvParams(i);
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
            envs_[(size_t)i] = p->matrixEnvParams(i);
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            rules_[(size_t)i] = p->matrixRule(i);
        chaos_ = p->chaosParams();
        shape_ = p->shapeSourceParams();
        effects_ = p->effectsParams();
        gain_ = p->globalGain();
        activeVoices_ = p->activeVoiceCount();
        presetNames_ = p->presetNames();
        wavetablePresets_ = p->wavetablePresetEntries();
        if(selectedWavetablePresetIndex_ >= int(wavetablePresets_.size()))
            selectedWavetablePresetIndex_ = wavetablePresets_.empty() ? -1 : int(wavetablePresets_.size()) - 1;
        if(selectedPresetIndex_ >= int(presetNames_.size()))
            selectedPresetIndex_ = presetNames_.empty() ? -1 : int(presetNames_.size()) - 1;
        presetLabel_ = (selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
                           ? presetNames_[(size_t)selectedPresetIndex_]
                           : p->presetStatus();
    }

    void pushGenerator()
    {
        if(auto *p = plugin())
            p->updateGenerator(generator_.wavetableSeed.partialCount, generator_.wavetableSeed.inharmonicAmount,
                               static_cast<int>(generator_.wavetableSeed.freqShape), generator_.sourceCount,
                               generator_.unison.voices, generator_.unison.detuneCents, generator_.unison.widthStereo,
                               generator_.unison.phaseSpread);
    }

    void pushSource()
    {
        if(auto *p = plugin())
            p->updateGeneratorSource(selectedSource_, generator_.sources[(size_t)selectedSource_]);
    }

    void pushCurrentTrack()
    {
        auto *track = currentTrack();
        if(track == nullptr)
            return;
        if(auto *p = plugin())
            p->updateSourceTrack(track->id, *track);
    }

    uint64_t uiNowMs() const
    {
        using clock = std::chrono::steady_clock;
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count());
    }

    bool realtimeDragPushDue(uint64_t &lastPushMs, bool force)
    {
        const uint64_t now = uiNowMs();
        if(!force && lastPushMs != 0u && now - lastPushMs < kRealtimeDragPushIntervalMs)
            return false;
        lastPushMs = now;
        return true;
    }

    void pushCurrentTrackDuringRealtimeDrag(bool force = false)
    {
        if(!realtimeDragPushDue(lastTrackRealtimeDragPushMs_, force))
            return;
        pushCurrentTrack();
    }

    void pushGeneratorDuringRealtimeDrag(bool force = false)
    {
        if(!realtimeDragPushDue(lastGenRealtimeDragPushMs_, force))
            return;
        pushGenerator();
    }

    // Track currently being drag-edited from the strip rack (gain/pan/send), or the
    // selected track as a fallback.
    synth::SourceTrackParams *dragTrack()
    {
        const int ti = dragTrackIndex_ >= 0 ? dragTrackIndex_ : selectedTrack_;
        if(ti >= 0 && ti < int(generator_.tracks.size()))
            return &generator_.tracks[(size_t)ti];
        return nullptr;
    }

    void pushCurrentTrackMorphOnly()
    {
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        if(auto *p = plugin())
            p->updateSourceTrackMorphOnly(track->id, track->metaOsc.morph);
    }

    void pushTrackById(uint32_t id)
    {
        for(auto &t : generator_.tracks)
            if(t.id == id)
            {
                if(auto *p = plugin())
                    p->updateSourceTrack(t.id, t);
                return;
            }
    }

    void deleteSelectedTrack()
    {
        if(generator_.tracks.size() <= 1)
            return;
        if(selectedTrack_ < 0 || selectedTrack_ >= int(generator_.tracks.size()))
            return;
        const uint32_t id = generator_.tracks[(size_t)selectedTrack_].id;
        if(auto *p = plugin())
        {
            p->removeSourceTrack(id);
            pullFromPlugin();
        }
        // Drop the deleted index from any groups and clamp selection.
        for(auto &g : stripGroups_)
        {
            std::vector<int> kept;
            for(int mi : g.memberIndices)
                if(mi != selectedTrack_)
                    kept.push_back(mi > selectedTrack_ ? mi - 1 : mi);
            g.memberIndices = std::move(kept);
        }
        stripGroups_.erase(std::remove_if(stripGroups_.begin(), stripGroups_.end(),
                                          [](const StripGroup &g) { return g.memberIndices.size() < 2; }),
                           stripGroups_.end());
        selectedGroupView_ = -1;
        selectedStrips_.fill(false);
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        pushGroups();
    }

    // Build SourceGroupDef list from the UI groups and push to the engine (group buses).
    void pushGroups()
    {
        std::vector<synth::SourceGroupDef> defs;
        defs.reserve(stripGroups_.size());
        for(const auto &g : stripGroups_)
        {
            synth::SourceGroupDef d;
            d.inserts = g.inserts;
            for(int mi : g.memberIndices)
                if(mi >= 0 && mi < int(generator_.tracks.size()))
                    d.memberTrackIds.push_back(generator_.tracks[(size_t)mi].id);
            defs.push_back(std::move(d));
        }
        if(auto *p = plugin())
            p->updateSourceGroups(defs);
    }

    // 在修改 MetaOsc 帧数据之前调用，记录快照用于 Ctrl+Z
    void pushMetaUndoSnapshot()
    {
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        if(int(metaUndoStack_.size()) >= kMetaUndoMax)
            metaUndoStack_.erase(metaUndoStack_.begin());
        metaUndoStack_.push_back(track->metaOsc);
    }

    bool undoMeta()
    {
        if(metaUndoStack_.empty())
            return false;
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return false;
        track->metaOsc = metaUndoStack_.back();
        metaUndoStack_.pop_back();
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, track->metaOsc.frameCount - 1));
        pushCurrentTrack();
        return true;
    }

    void pushMetaPartial()
    {
        if(selectedMetaPartial_ < 0 || selectedMetaPartial_ >= synth::kEditableMetaPartials)
            return;
        if(auto *p = plugin())
            p->updatePartialSlot(selectedMetaPartial_, generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_]);
    }

    void pushMetaPartialRuntime()
    {
        if(selectedMetaPartial_ < 0 || selectedMetaPartial_ >= synth::kEditableMetaPartials)
            return;
        const auto &slot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        if(auto *p = plugin())
            p->updatePartialRuntime(selectedMetaPartial_, slot.enabled, slot.ratio, slot.amp, slot.phase, slot.pan,
                                    slot.morph, int(slot.warpMode), slot.warpAmount);
    }

    void pushAdsr()
    {
        if(auto *p = plugin())
            p->updateAdsr(adsr_.attack, adsr_.decay, adsr_.sustain, adsr_.release, adsr_.curve);
    }

    void pushGain()
    {
        if(auto *p = plugin())
            p->updateGlobalGain(gain_);
    }

    void pushMatrix()
    {
        if(auto *p = plugin())
        {
            p->updateLfo(selectedLfo_, lfos_[(size_t)selectedLfo_]);
            p->updateMatrixEnv(selectedEnv_, envs_[(size_t)selectedEnv_]);
            p->updateMatrixRule(selectedRule_, rules_[(size_t)selectedRule_]);
            p->updateChaos(chaos_);
            p->updateShapeSource(shape_);
        }
    }
    void pushLfoOnly()    { if(auto *p = plugin()) p->updateLfo(selectedLfo_, lfos_[(size_t)selectedLfo_]); }
    void pushEnvOnly()    { if(auto *p = plugin()) p->updateMatrixEnv(selectedEnv_, envs_[(size_t)selectedEnv_]); }
    void pushRuleOnly()   { if(auto *p = plugin()) p->updateMatrixRule(selectedRule_, rules_[(size_t)selectedRule_]); }
    void pushChaosOnly()  { if(auto *p = plugin()) p->updateChaos(chaos_); }
    void pushShapeOnly()  { if(auto *p = plugin()) p->updateShapeSource(shape_); }

    void pushAmpEnv()
    {
        if(auto *p = plugin())
            p->updateAmpEnv(selectedAmpEnv_, ampEnvs_[(size_t)selectedAmpEnv_]);
    }

    void pushOperator()
    {
        if(auto *p = plugin())
            p->updateOperatorChain(operatorChain_);
    }

    void pushEffects()
    {
        if(auto *p = plugin())
            p->updateEffects(effects_);
    }

    void useUiFont()
    {
#ifdef DGL_NO_SHARED_RESOURCES
        fontFace("sans");
#else
        fontFace(NANOVG_DEJAVU_SANS_TTF);
#endif
    }

    void uiFontSize(float size)
    {
        fontSize(size * uiScale_);
    }

    // Approximate master output level from the per-source live levels, scaled by
    // the master gain — good enough to drive the toolbar meter.
    float masterLevel()
    {
        float m = 0.0f;
        if(const auto *p = plugin())
            for(int i = 0; i < int(generator_.tracks.size()); ++i)
                m += p->sourceLiveLevel(i);
        return clampf(m * gain_, 0.0f, 1.0f);
    }

    void drawMasterMeter(const Rect &r)
    {
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 3.0f);
        fillColor(DesignTokens::controlBackground());
        fill();
        const float lvl = masterLevel();
        const float fillW = clampf(lvl, 0.0f, 1.0f) * (r.w - 2.0f);
        if(fillW > 1.0f)
        {
            beginPath();
            roundedRect(r.x + 1.0f, r.y + 1.0f, fillW, r.h - 2.0f, 2.0f);
            fillPaint(linearGradient(r.x, r.y, r.x + r.w, r.y,
                                     DesignTokens::accentGreen(), rgba(0xff5a4effU)));
            fill();
        }
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, 3.0f);
        strokeColor(DesignTokens::border());
        strokeWidth(1.0f);
        stroke();
    }

    void drawBackground()
    {
        const float w = static_cast<float>(uiW());
        const float h = static_cast<float>(uiH());
        beginPath();
        rect(0.0f, 0.0f, w, h);
        fillColor(DesignTokens::appBackground());
        fill();
    }

    void drawToolbar()
    {
        useUiFont();
        toolbar_ = { 0.0f, 0.0f, static_cast<float>(uiW()), 64.0f };
        beginPath();
        rect(toolbar_.x, toolbar_.y, toolbar_.w, toolbar_.h);
        fillColor(DesignTokens::panelBackground());
        fill();
        beginPath();
        rect(0.0f, toolbar_.h - 1.0f, toolbar_.w, 1.0f);
        fillColor(DesignTokens::divider());
        fill();

        // Logo mark.
        beginPath();
        roundedRect(16.0f, 16.0f, 8.0f, 32.0f, 3.0f);
        fillColor(DesignTokens::accentCyan());
        fill();

        uiFontSize(20.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(34.0f, 26.0f, "Kapibara", nullptr);
        uiFontSize(8.5f);
        fillColor(DesignTokens::textSecondary());
        text(35.0f, 42.0f, "ADDITIVE  SYNTH", nullptr);

        const float edge = static_cast<float>(uiW()) - 14.0f;
        // ---- Master section (far right): output knob + horizontal level meter ----
        gainRect_        = { edge - 96.0f, 12.0f, 96.0f, 40.0f };
        masterMeterRect_ = { gainRect_.x - 66.0f, 27.0f, 60.0f, 9.0f };
        const float right = masterMeterRect_.x - 16.0f;
        aboutRect_ = { right - 78.0f, 12.0f, 78.0f, 36.0f };
        menuRect_ = { aboutRect_.x - 86.0f, 12.0f, 78.0f, 36.0f };
        panicRect_ = { menuRect_.x - 76.0f, 12.0f, 68.0f, 36.0f };
        const Rect abRect { panicRect_.x - 102.0f, 12.0f, 94.0f, 36.0f };
        presetPrevRect_ = { 220.0f, 14.0f, 38.0f, 34.0f };
        presetNextRect_ = { abRect.x - 48.0f, 14.0f, 38.0f, 34.0f };
        presetSelectRect_ = { 264.0f, 8.0f, std::max(180.0f, presetNextRect_.x - 272.0f), 46.0f };
        presetSaveRect_ = {};
        presetLoadRect_ = {};

        drawButton(presetPrevRect_, "<", false);
        drawButton(presetSelectRect_, presetLabel_.empty() ? "Select preset" : presetLabel_.c_str(), presetMenuOpen_);
        drawButton(presetNextRect_, ">", false);
        drawButton(abRect, "A -> B", false);
        drawButton(panicRect_, "Panic", false);
        drawButton(menuRect_, "MENU", false);
        drawButton(aboutRect_, "ABOUT", false);

        // "MASTER" caption above the meter, then the meter and the output dial.
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        text(masterMeterRect_.x, masterMeterRect_.y - 11.0f, "MASTER", nullptr);
        drawMasterMeter(masterMeterRect_);
        drawKnob(gainRect_, "Master", gain_, gain_);

        statusRect_ = { 160.0f, 50.0f, 56.0f, 14.0f };
        char status[128];
        std::snprintf(status, sizeof(status), "%d voices", activeVoices_);
        uiFontSize(10.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(rgba(0x6a7d88ff));
        text(statusRect_.x, statusRect_.y + statusRect_.h * 0.5f, status, nullptr);

    }

    void drawPresetMenu()
    {
        if(!presetMenuOpen_)
            return;

        const Rect r { presetSelectRect_.x - 245.0f, toolbar_.y + toolbar_.h + 8.0f, 720.0f, 316.0f };
        presetMenuPanelRect_ = r;
        drawPanel(r, rgba(0x181d24f5), rgba(0x39404dff));
        presetSearchRect_ = { r.x + 16.0f, r.y + 16.0f, r.w - 256.0f, 34.0f };
        presetMenuNewRect_ = { r.x + r.w - 224.0f, r.y + 16.0f, 60.0f, 44.0f };
        presetMenuSaveRect_ = { r.x + r.w - 156.0f, r.y + 16.0f, 140.0f, 44.0f };
        presetMenuLoadRect_ = {};
        presetMenuDeleteRect_ = { r.x + r.w - 156.0f, r.y + 72.0f, 140.0f, 44.0f };
        presetMenuResetRect_ = { r.x + r.w - 156.0f, r.y + 128.0f, 140.0f, 44.0f };
        presetListRect_ = { r.x + 16.0f, r.y + 62.0f, r.w - 188.0f, 188.0f };
        std::string synthNameText = "Name...";
        if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Synth)
            synthNameText = "> " + presetNameBuffer_;
        else if(selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
            synthNameText = presetNames_[(size_t)selectedPresetIndex_];
        else if(!presetLabel_.empty())
            synthNameText = presetLabel_;
        const std::string nameText = synthNameText;
        drawLabelBox(presetSearchRect_, nameText.c_str());
        drawPanel(presetListRect_, rgba(0x20252dff), rgba(0x39404dff));
        for(auto &row : presetRowRects_)
            row = {};
        const int visible = std::min<int>(int(presetRowRects_.size()), int(presetNames_.size()));
        const int first = selectedPresetIndex_ >= 0
                              ? clampi(selectedPresetIndex_ - visible / 2, 0, std::max(0, int(presetNames_.size()) - visible))
                              : 0;
        for(int i = 0; i < visible; ++i)
        {
            const int presetIndex = first + i;
            presetRowRects_[(size_t)i] = { presetListRect_.x + 8.0f, presetListRect_.y + 8.0f + float(i) * 24.0f,
                                           presetListRect_.w - 16.0f, 22.0f };
            drawButton(presetRowRects_[(size_t)i], presetNames_[(size_t)presetIndex].c_str(), selectedPresetIndex_ == presetIndex);
        }
        if(visible == 0)
            drawLabelBox({ presetListRect_.x + 8.0f, presetListRect_.y + 8.0f, presetListRect_.w - 16.0f, 24.0f },
                         "No user presets");
        drawButton(presetMenuNewRect_, "NEW", presetNameEditing_ && presetNameBuffer_.empty());
        drawButton(presetMenuSaveRect_, presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Synth ? "SAVE NAME" : "SAVE AS", false);
        drawButton(presetMenuDeleteRect_, "DELETE", false);
        drawButton(presetMenuResetRect_, "RESET", false);
        drawLabelBox({ r.x + r.w - 156.0f, r.y + 184.0f, 140.0f, 44.0f }, "Double-click loads");
        drawLabelBox({ r.x + 16.0f, r.y + 282.0f, 88.0f, 22.0f }, "All");
        drawLabelBox({ r.x + 108.0f, r.y + 282.0f, 104.0f, 22.0f }, "User");
        drawLabelBox({ r.x + 216.0f, r.y + 282.0f, 122.0f, 22.0f }, "Favourites");
    }

    void drawWavetablePresetMenu()
    {
        if(!wavetablePresetMenuOpen_)
            return;

        const float menuWidth = std::min(520.0f, float(uiW()) - 24.0f);
        const float menuX = clampf(metaWavetableNameRect_.x, 12.0f, float(uiW()) - menuWidth - 12.0f);
        const Rect panel { menuX, metaWavetableNameRect_.y + metaWavetableNameRect_.h + 6.0f,
                           menuWidth, 316.0f };
        wavetablePresetPanelRect_ = panel;
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(panel.x + 16.0f, panel.y + 12.0f, "Wavetable Presets");

        static constexpr const char *builtins[] = { "Sine", "Saw", "Square", "Triangle", "Clear" };
        const float builtinGap = 6.0f;
        const float builtinW = (panel.w - 32.0f - builtinGap * 4.0f) / 5.0f;
        for(size_t i = 0; i < wavetableBuiltinRects_.size(); ++i)
        {
            wavetableBuiltinRects_[i] = { panel.x + 16.0f + float(i) * (builtinW + builtinGap),
                                           panel.y + 38.0f, builtinW, 28.0f };
            drawButton(wavetableBuiltinRects_[i], builtins[i], false);
        }

        wavetablePresetListRect_ = { panel.x + 16.0f, panel.y + 76.0f, panel.w - 32.0f, 176.0f };
        drawPanel(wavetablePresetListRect_, rgba(0x0c1318ff), rgba(0x354851ff));
        for(auto &row : wavetablePresetRowRects_)
            row = {};
        const int visible = std::min<int>(int(wavetablePresetRowRects_.size()), int(wavetablePresets_.size()));
        const int first = selectedWavetablePresetIndex_ >= 0
                              ? clampi(selectedWavetablePresetIndex_ - visible / 2, 0,
                                       std::max(0, int(wavetablePresets_.size()) - visible))
                              : 0;
        for(int i = 0; i < visible; ++i)
        {
            const int presetIndex = first + i;
            wavetablePresetRowRects_[(size_t)i] = { wavetablePresetListRect_.x + 8.0f,
                                                     wavetablePresetListRect_.y + 7.0f + float(i) * 20.0f,
                                                     wavetablePresetListRect_.w - 16.0f, 18.0f };
            drawButton(wavetablePresetRowRects_[(size_t)i], wavetablePresets_[(size_t)presetIndex].name.c_str(),
                       presetIndex == selectedWavetablePresetIndex_);
        }
        if(visible == 0)
            drawLabelBox({ wavetablePresetListRect_.x + 8.0f, wavetablePresetListRect_.y + 8.0f,
                           wavetablePresetListRect_.w - 16.0f, 24.0f },
                         "No wavetable files in presets/wavetables");

        const float by = panel.y + 266.0f;
        wavetablePresetLoadRect_ = {};
        wavetablePresetNameRect_ = { panel.x + 16.0f, by, 160.0f, 34.0f };
        wavetablePresetSaveRect_ = { panel.x + 184.0f, by, 78.0f, 34.0f };
        wavetablePresetImportRect_ = { panel.x + 270.0f, by, 108.0f, 34.0f };
        wavetablePresetRefreshRect_ = { panel.x + 386.0f, by, 92.0f, 34.0f };
        wavetablePresetCloseRect_ = { panel.x + panel.w - 90.0f, by, 74.0f, 34.0f };
        std::string wtNameText = wavetablePresetLabel_.empty() ? "wavetable" : wavetablePresetLabel_;
        if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Wavetable)
            wtNameText = "> " + presetNameBuffer_;
        else if(selectedWavetablePresetIndex_ >= 0 && selectedWavetablePresetIndex_ < int(wavetablePresets_.size()))
            wtNameText = wavetablePresets_[(size_t)selectedWavetablePresetIndex_].name;
        drawLabelBox(wavetablePresetNameRect_, wtNameText.c_str());
        drawButton(wavetablePresetSaveRect_, presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Wavetable ? "SAVE" : "RENAME", false);
        drawButton(wavetablePresetImportRect_, "IMPORT WAV", false);
        drawButton(wavetablePresetRefreshRect_, "REFRESH", false);
        drawButton(wavetablePresetCloseRect_, "CLOSE", false);
    }

    void refreshWavetablePresets()
    {
        if(auto *p = plugin())
            wavetablePresets_ = p->wavetablePresetEntries();
        selectedWavetablePresetIndex_ = wavetablePresets_.empty()
                                              ? -1
                                              : clampi(selectedWavetablePresetIndex_, 0,
                                                       int(wavetablePresets_.size()) - 1);
    }

    bool loadWavetablePreset(int index)
    {
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank)
           || index < 0 || index >= int(wavetablePresets_.size()))
            return false;
        selectedWavetablePresetIndex_ = index;
        loadPathBuffer_ = wavetablePresets_[(size_t)index].path;
        wavetableImportMode_ = synth::WavetableImportMode::AutoDetect;
        importFrameLimit_ = synth::kMaxWavetableFrames;
        if(!commitWavetableLoad())
            return false;
        wavetablePresetLabel_ = wavetablePresets_[(size_t)index].name;
        presetNameBuffer_ = wavetablePresetLabel_;
        return true;
    }

    bool handleWavetablePresetMenuClick(float x, float y)
    {
        if(!wavetablePresetMenuOpen_)
            return false;
        static constexpr const char *builtins[] = { "Sine", "Saw", "Square", "Triangle", "Clear" };
        for(size_t i = 0; i < wavetableBuiltinRects_.size(); ++i)
        {
            if(!wavetableBuiltinRects_[i].contains(x, y))
                continue;
            if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                pushMetaUndoSnapshot();
            applyFramePreset(int(i));
            wavetablePresetLabel_ = builtins[i];
            wavetablePresetMenuOpen_ = false;
            return true;
        }

        const int visible = std::min<int>(int(wavetablePresetRowRects_.size()), int(wavetablePresets_.size()));
        const int first = selectedWavetablePresetIndex_ >= 0
                              ? clampi(selectedWavetablePresetIndex_ - visible / 2, 0,
                                       std::max(0, int(wavetablePresets_.size()) - visible))
                              : 0;
        for(int i = 0; i < visible; ++i)
        {
            const int presetIndex = first + i;
            if(wavetablePresetRowRects_[(size_t)i].contains(x, y))
            {
                selectedWavetablePresetIndex_ = presetIndex;
                presetNameBuffer_ = wavetablePresets_[(size_t)presetIndex].name;
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                if(currentClickIsDouble_ && loadWavetablePreset(selectedWavetablePresetIndex_))
                    wavetablePresetMenuOpen_ = false;
                return true;
            }
        }
        if(wavetablePresetNameRect_.contains(x, y))
        {
            beginWavetablePresetRename();
            return true;
        }
        if(wavetablePresetSaveRect_.contains(x, y))
        {
            if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Wavetable)
                commitPresetNameEdit();
            else
                beginWavetablePresetRename();
            return true;
        }
        if(wavetablePresetImportRect_.contains(x, y))
        {
            wavetablePresetMenuOpen_ = false;
            beginWavetableImport();
            return true;
        }
        if(wavetablePresetRefreshRect_.contains(x, y))
        {
            refreshWavetablePresets();
            return true;
        }
        if(wavetablePresetCloseRect_.contains(x, y))
        {
            wavetablePresetMenuOpen_ = false;
            return true;
        }
        if(wavetablePresetPanelRect_.contains(x, y))
            return true;
        wavetablePresetMenuOpen_ = false;
        return true;
    }

    void drawOptionsMenu()
    {
        if(!optionsMenuOpen_)
            return;
        const Rect r { menuRect_.x - 230.0f, toolbar_.y + toolbar_.h + 8.0f, 320.0f, 118.0f };
        optionsMenuPanelRect_ = r;
        drawPanel(r, rgba(0x181d24f5), rgba(0x39404dff));
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, "Menu");
        uiScaleRect_ = { r.x + 16.0f, r.y + 48.0f, r.w - 32.0f, 26.0f };
        drawSlider(uiScaleRect_, "UI Scale", (uiScale_ - 0.75f) / 0.75f, uiScale_);
        drawLabelBox({ r.x + 16.0f, r.y + 82.0f, r.w - 32.0f, 24.0f }, "Affects panel text and control labels");
    }

    const char *wavetableImportModeName(synth::WavetableImportMode mode) const
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

    void drawWavetableImportMenu()
    {
        if(!wavetableImportMenuOpen_)
            return;
        const Rect r { (float(uiW()) - 440.0f) * 0.5f, (float(uiH()) - 410.0f) * 0.5f,
                       440.0f, 410.0f };
        wavetableImportPanelRect_ = r;
        drawPanel(r, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(r.x + 18.0f, r.y + 16.0f, "Import WAV as Wavetable");
        for(int i = 0; i < 5; ++i)
        {
            wavetableImportModeRects_[(size_t)i] = { r.x + 18.0f, r.y + 52.0f + float(i) * 46.0f,
                                                     r.w - 36.0f, 36.0f };
            const auto mode = static_cast<synth::WavetableImportMode>(i);
            drawButton(wavetableImportModeRects_[(size_t)i], wavetableImportModeName(mode),
                       wavetableImportMode_ == mode);
        }
        drawLabelBox({ r.x + 18.0f, r.y + 282.0f, 104.0f, 30.0f }, "Frame limit");
        const int limits[3] = { 128, 256, 512 };
        for(int i = 0; i < 3; ++i)
        {
            importFrameLimitRects_[(size_t)i] = { r.x + 130.0f + float(i) * 76.0f, r.y + 282.0f, 68.0f, 30.0f };
            drawButton(importFrameLimitRects_[(size_t)i], buttonText("%d", limits[i]), importFrameLimit_ == limits[i]);
        }
        manualCycleMinusRect_ = { r.x + 18.0f, r.y + 322.0f, 42.0f, 30.0f };
        manualCyclePlusRect_ = { r.x + 66.0f, r.y + 322.0f, 42.0f, 30.0f };
        wavetableImportCancelRect_ = { r.x + r.w - 108.0f, r.y + 322.0f, 90.0f, 30.0f };
        drawButton(manualCycleMinusRect_, "-", false);
        drawButton(manualCyclePlusRect_, "+", false);
        manualCycleValueRect_ = { r.x + 116.0f, r.y + 322.0f, 168.0f, 30.0f };
        drawLabelBox(manualCycleValueRect_,
                     buttonText("Cycle %d samples", manualCycleLength_));
        drawButton(wavetableImportCancelRect_, "Cancel", false);
        drawLabelBox({ r.x + 18.0f, r.y + 364.0f, r.w - 36.0f, 22.0f },
                     droppedWavPending_ ? "Dropped WAV ready" : "Choose a mode, then choose a WAV file");
    }

    // LFO shape value at phase xi∈[0,1) (mirrors engine Lfo::shapeOutput) for the preview.
    static float lfoShapeValue(const synth::LfoParams &p, float xi)
    {
        switch(p.shape)
        {
            case synth::LfoShape::Asymmetric: {
                const float rho = clampf(p.rhoLfo, 0.001f, 0.999f);
                const float pu = std::max(0.05f, p.pUp);
                const float pd = std::max(0.05f, p.pDown);
                if(xi < rho) return -1.0f + 2.0f * std::pow(xi / rho, pu);
                return 1.0f - 2.0f * std::pow((xi - rho) / (1.0f - rho), pd);
            }
            case synth::LfoShape::Sine:     return std::sin(2.0f * kPi * xi);
            case synth::LfoShape::Square:   return xi < 0.5f ? 1.0f : -1.0f;
            case synth::LfoShape::Triangle: return xi < 0.5f ? (4.0f * xi - 1.0f) : (3.0f - 4.0f * xi);
            case synth::LfoShape::SampleHold: {
                const int step = int(xi * 8.0f);
                const uint32_t h = uint32_t(step) * 2654435761u + 1013904223u;
                return float(double(h & 0xffffu) / 32768.0 - 1.0);
            }
        }
        return 0.0f;
    }

    // LFO shape preview with animated playhead.
    void drawLfoCurve(const Rect &r, const synth::LfoParams &lfo)
    {
        drawPanel(r, rgba(0x101820ff), rgba(0x263842ff));
        const float midY = r.y + r.h * 0.5f;
        strokeLine(r.x + 4.0f, midY, r.x + r.w - 4.0f, midY, rgba(0x2b3f48cc), 0.6f);
        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        // 两个周期，带相位偏移
        beginPath();
        const int kN = 96;
        for(int n = 0; n < kN; ++n)
        {
            const float u  = float(n) / float(kN - 1);
            const float xi = std::fmod(u * 2.0f + lfo.phase0, 1.0f);
            const float v  = lfoShapeValue(lfo, xi);
            const float px = r.x + 4.0f + u * (r.w - 8.0f);
            const float py = midY - v * (r.h * 0.5f - 5.0f);
            if(n == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(rgba(lfo.enabled ? 0x63d2ffffU : 0x63d2ff66U));
        strokeWidth(1.6f);
        stroke();
        // 移动相位指示（动画）
        const float playX = std::fmod(animPhase_ * lfo.frequencyHz * 0.15f, 1.0f);
        const float mx = r.x + 4.0f + playX * 0.5f * (r.w - 8.0f);
        strokeLine(mx, r.y + 3.0f, mx, r.y + r.h - 3.0f, rgba(0xffd16699), 1.0f);
        resetScissor();
    }

    // Distortion slot editor (controls + curves) for uiDist_[selectedDist_].
    void drawStripGroupContextMenu()
    {
        if(!stripGroupContextMenuOpen_)
            return;
        constexpr float rowH = 28.0f;

        // ---- Ungroup mode (right-clicked a group bus) ----
        if(groupContextTargetGroup_ >= 0)
        {
            const Rect panel { stripGroupContextX_, stripGroupContextY_, 200.0f, 30.0f + rowH };
            drawPanel(panel, rgba(0x10171df8), rgba(0xc070e0ccU));
            drawSectionTitle(panel.x + 12.0f, panel.y + 8.0f, "Group");
            stripGroupContextRects_[0] = { panel.x + 8.0f, panel.y + 26.0f, panel.w - 16.0f, rowH - 2.0f };
            drawButton(stripGroupContextRects_[0], "Ungroup (dissolve)", false);
            stripGroupContextRects_[1] = {};
            return;
        }

        // ---- Create-group mode (right-clicked strips) ----
        int selCount = 0;
        for(size_t i = 0; i < generator_.tracks.size() && i < selectedStrips_.size(); ++i)
            if(selectedStrips_[i]) ++selCount;

        const int rows = selCount >= 2 ? 2 : 1;
        const Rect panel { stripGroupContextX_, stripGroupContextY_, 210.0f, 30.0f + rowH * float(rows) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(panel.x + 12.0f, panel.y + 8.0f, "Strip");
        if(selCount >= 2)
        {
            stripGroupContextRects_[0] = { panel.x + 8.0f, panel.y + 26.0f, panel.w - 16.0f, rowH - 2.0f };
            char label[48];
            std::snprintf(label, sizeof(label), "Create Group (%d strips)", selCount);
            drawButton(stripGroupContextRects_[0], label, false);
        }
        const int clearRow = selCount >= 2 ? 1 : 0;
        stripGroupContextRects_[(size_t)clearRow] = { panel.x + 8.0f, panel.y + 26.0f + float(clearRow) * rowH, panel.w - 16.0f, rowH - 2.0f };
        drawButton(stripGroupContextRects_[(size_t)clearRow], "Clear Selection", false);
    }

    bool handleStripGroupContextMenuClick(float x, float y)
    {
        if(!stripGroupContextMenuOpen_)
            return false;
        stripGroupContextMenuOpen_ = false;

        // ---- Ungroup ----
        if(groupContextTargetGroup_ >= 0)
        {
            const int gi = groupContextTargetGroup_;
            groupContextTargetGroup_ = -1;
            if(stripGroupContextRects_[0].contains(x, y) && gi < int(stripGroups_.size()))
            {
                stripGroups_.erase(stripGroups_.begin() + gi);
                selectedGroupView_ = -1;
                pushGroups();
            }
            return true;
        }

        int selCount = 0;
        for(size_t i = 0; i < generator_.tracks.size() && i < selectedStrips_.size(); ++i)
            if(selectedStrips_[i]) ++selCount;
        // Create Group row
        if(selCount >= 2 && stripGroupContextRects_[0].contains(x, y))
        {
            StripGroup grp;
            grp.name = "Group " + std::to_string(int(stripGroups_.size()) + 1);
            for(size_t i = 0; i < generator_.tracks.size() && i < selectedStrips_.size(); ++i)
                if(selectedStrips_[i]) grp.memberIndices.push_back(int(i));
            stripGroups_.push_back(std::move(grp));
            pushGroups();
            return true;
        }
        // Clear Selection row
        const int clearRow = selCount >= 2 ? 1 : 0;
        if(stripGroupContextRects_[(size_t)clearRow].contains(x, y))
        {
            selectedStrips_.fill(false);
            if(selectedTrack_ >= 0 && selectedTrack_ < int(generator_.tracks.size()))
                selectedStrips_[(size_t)selectedTrack_] = true;
            return true;
        }
        return true;
    }

    std::vector<InsertEffect> *insertChainFor(int trackId, int groupIdx)
    {
        if(groupIdx >= 0 && groupIdx < int(stripGroups_.size()))
            return &stripGroups_[(size_t)groupIdx].inserts;
        if(trackId >= 0)
            return trackInsertsFor(uint32_t(trackId));
        return nullptr;
    }

    void commitChainChange(int trackId, int group)
    {
        if(group >= 0) pushGroups();
        else if(trackId >= 0) pushTrackById(uint32_t(trackId));
    }

    static bool fxHasMode(int kind) { return kind == InsertFilter || kind == InsertDist || kind == InsertDelay || kind == InsertConvReverb; }

    static const char *fxKnobName(int kind, int i)
    {
        static const char *F[4]={"Cutoff","Q","Drive","Mix"};
        static const char *D[4]={"Drive","Bias","Mix","Out"};
        static const char *E[4]={"Low","Mid","High","MidHz"};
        static const char *C[4]={"Thr","Ratio","Atk","Makeup"};
        static const char *L[4]={"Time","FB","Mix","Tone"};
        static const char *R[4]={"Size","Decay","Mix","Damp"};
        static const char *V[4]={"Mix","Gain","PreDly","-"};
        switch(kind){case InsertFilter:return F[i];case InsertDist:return D[i];case InsertEq:return E[i];
                     case InsertComp:return C[i];case InsertDelay:return L[i];case InsertConvReverb:return V[i];
                     case InsertReverb:return R[i];default:return R[i];}
    }
    // normalized 0..1 for knob i of an insert
    static float fxKnobNorm(const InsertEffect &e, int i)
    {
        switch(e.kind){
            case InsertFilter:{const auto&f=e.filter;switch(i){case 0:return std::log10(std::max(20.f,f.cutoffHz)/20.f)/std::log10(1000.f);case 1:return clampf((f.resonance-0.05f)/9.95f,0,1);case 2:return clampf((f.drive-1.f)/15.f,0,1);default:return f.mix;}}
            case InsertDist:{const auto&d=e.dist;switch(i){case 0:return clampf((d.drive-1.f)/31.f,0,1);case 1:return (d.bias+1.f)*0.5f;case 2:return d.mix;default:return d.outGain*0.5f;}}
            case InsertEq:{const auto&q=e.eq;switch(i){case 0:return (q.lowDb+18.f)/36.f;case 1:return (q.midDb+18.f)/36.f;case 2:return (q.highDb+18.f)/36.f;default:return std::log10(std::max(80.f,q.midHz)/80.f)/std::log10(8000.f/80.f);}}
            case InsertComp:{const auto&c=e.comp;switch(i){case 0:return (c.threshDb+48.f)/48.f;case 1:return (c.ratio-1.f)/19.f;case 2:return c.attackMs/100.f;default:return c.makeupDb/24.f;}}
            case InsertDelay:{const auto&l=e.delay;switch(i){case 0:return clampf(l.timeMs/1000.f,0,1);case 1:return l.feedback/0.95f;case 2:return l.mix;default:return l.tone;}}
            case InsertConvReverb:{const auto&v=e.conv;switch(i){case 0:return v.mix;case 1:return v.gain*0.5f;case 2:return v.predelayMs/200.f;default:return 0.f;}}
            default:{const auto&r=e.reverb;switch(i){case 0:return r.size;case 1:return r.decay/0.92f;case 2:return r.mix;default:return r.damp;}}
        }
    }
    static float fxKnobDisp(const InsertEffect &e, int i)
    {
        switch(e.kind){
            case InsertFilter:{const auto&f=e.filter;switch(i){case 0:return f.cutoffHz;case 1:return f.resonance;case 2:return f.drive;default:return f.mix;}}
            case InsertDist:{const auto&d=e.dist;switch(i){case 0:return d.drive;case 1:return d.bias;case 2:return d.mix;default:return d.outGain;}}
            case InsertEq:{const auto&q=e.eq;switch(i){case 0:return q.lowDb;case 1:return q.midDb;case 2:return q.highDb;default:return q.midHz;}}
            case InsertComp:{const auto&c=e.comp;switch(i){case 0:return c.threshDb;case 1:return c.ratio;case 2:return c.attackMs;default:return c.makeupDb;}}
            case InsertDelay:{const auto&l=e.delay;switch(i){case 0:return l.timeMs;case 1:return l.feedback;case 2:return l.mix;default:return l.tone;}}
            case InsertConvReverb:{const auto&v=e.conv;switch(i){case 0:return v.mix;case 1:return v.gain;case 2:return v.predelayMs;default:return 0.f;}}
            default:{const auto&r=e.reverb;switch(i){case 0:return r.size;case 1:return r.decay;case 2:return r.mix;default:return r.damp;}}
        }
    }
    static void fxKnobSetNorm(InsertEffect &e, int i, float n)
    {
        n = clampf(n, 0.0f, 1.0f);
        switch(e.kind){
            case InsertFilter:{auto&f=e.filter;switch(i){case 0:f.cutoffHz=20.f*std::pow(1000.f,n);break;case 1:f.resonance=0.05f+n*9.95f;break;case 2:f.drive=1.f+n*15.f;break;default:f.mix=n;break;}break;}
            case InsertDist:{auto&d=e.dist;switch(i){case 0:d.drive=1.f+n*31.f;break;case 1:d.bias=n*2.f-1.f;break;case 2:d.mix=n;break;default:d.outGain=n*2.f;break;}break;}
            case InsertEq:{auto&q=e.eq;switch(i){case 0:q.lowDb=n*36.f-18.f;break;case 1:q.midDb=n*36.f-18.f;break;case 2:q.highDb=n*36.f-18.f;break;default:q.midHz=80.f*std::pow(8000.f/80.f,n);break;}break;}
            case InsertComp:{auto&c=e.comp;switch(i){case 0:c.threshDb=n*48.f-48.f;break;case 1:c.ratio=1.f+n*19.f;break;case 2:c.attackMs=n*100.f;break;default:c.makeupDb=n*24.f;break;}break;}
            case InsertDelay:{auto&l=e.delay;switch(i){case 0:l.timeMs=n*1000.f;break;case 1:l.feedback=n*0.95f;break;case 2:l.mix=n;break;default:l.tone=n;break;}break;}
            case InsertConvReverb:{auto&v=e.conv;switch(i){case 0:v.mix=n;break;case 1:v.gain=n*2.f;break;case 2:v.predelayMs=n*200.f;break;default:break;}break;}
            default:{auto&r=e.reverb;switch(i){case 0:r.size=n;break;case 1:r.decay=n*0.92f;break;case 2:r.mix=n;break;default:r.damp=n;break;}break;}
        }
    }

    // ---- Insert TYPE picker (append a new effect to a chain) ----
    void openInsertMenu(int trackId, int groupIdx, float x, float y)
    {
        insertMenuTrackId_ = trackId;
        insertMenuGroup_   = groupIdx;
        insertMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW())  - 130.0f));
        insertMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - 160.0f));
        insertMenuOpen_ = true;
    }

    static const char *insertTypeName(int kind)
    {
        static const char *n[8] = { "", "Filter", "Distortion", "EQ", "Compressor", "Delay", "Reverb", "IR Reverb" };
        return (kind >= 1 && kind <= 7) ? n[kind] : "";
    }

    void drawInsertMenu()
    {
        if(!insertMenuOpen_)
            return;
        constexpr float rowH = 20.0f;
        const float menuW = 120.0f;
        const Rect panel { insertMenuX_, insertMenuY_, menuW, 22.0f + rowH * float(kInsertTypeCount) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 4.0f, "Add effect", nullptr);
        for(int c = 0; c < kInsertTypeCount; ++c)
        {
            insertMenuRects_[(size_t)c] = { panel.x + 6.0f, panel.y + 20.0f + float(c) * rowH, menuW - 12.0f, rowH - 2.0f };
            drawButton(insertMenuRects_[(size_t)c], insertTypeName(c + 1), false);
        }
    }

    bool handleInsertMenuClick(float x, float y)
    {
        if(!insertMenuOpen_)
            return false;
        insertMenuOpen_ = false;
        auto *chain = insertChainFor(insertMenuTrackId_, insertMenuGroup_);
        if(chain == nullptr)
            return true;
        for(int c = 0; c < kInsertTypeCount; ++c)
            if(insertMenuRects_[(size_t)c].contains(x, y))
            {
                InsertEffect e;
                e.kind = uint8_t(c + 1);
                chain->push_back(e);
                commitChainChange(insertMenuTrackId_, insertMenuGroup_);
                return true;
            }
        return true;
    }

    // ---- Filter/dist algorithm picker for a specific insert ----
    int fxModeCount(int kind) const
    {
        if(kind == InsertFilter) return kFilterAlgoCount;
        if(kind == InsertDist)   return kDistAlgoCount;
        if(kind == InsertDelay)  return 2; // Stereo / PingPong
        if(kind == InsertConvReverb) return int(irFiles_.size());
        return 0;
    }
    const char *fxModeName(int kind, int i)
    {
        if(kind == InsertFilter) return kFilterAlgoNames[i];
        if(kind == InsertDist)   return kDistAlgoNames[i];
        if(kind == InsertDelay)  { static const char *D[2] = { "Stereo", "PingPong" }; return D[i]; }
        if(kind == InsertConvReverb) return (i >= 0 && i < int(irFiles_.size())) ? irFiles_[(size_t)i].first.c_str() : "";
        return "";
    }
    static const char *fxModeTitle(int kind)
    {
        if(kind == InsertFilter) return "Filter mode";
        if(kind == InsertDist)   return "Dist mode";
        if(kind == InsertDelay)  return "Delay mode";
        if(kind == InsertConvReverb) return "Impulse (presets/irs)";
        return "Mode";
    }
    int fxCurrentMode(const InsertEffect &ins, int kind)
    {
        if(kind == InsertFilter) return int(ins.filter.algo);
        if(kind == InsertDist)   return int(ins.dist.algo);
        if(kind == InsertDelay)  return ins.delay.pingpong ? 1 : 0;
        if(kind == InsertConvReverb)
            for(int i = 0; i < int(irFiles_.size()); ++i)
                if(irFiles_[(size_t)i].first == ins.conv.irName) return i;
        return 0;
    }

    void openModeMenu(int trackId, int groupIdx, int insertIdx, int kind, float x, float y)
    {
        modeMenuTrackId_ = trackId; modeMenuGroup_ = groupIdx; modeMenuInsertIdx_ = insertIdx; modeMenuKind_ = kind;
        if(kind == InsertConvReverb) refreshIrFiles();
        modeMenuSelectedIndex_ = 0;
        if(auto *chain = insertChainFor(trackId, groupIdx);
           chain != nullptr && insertIdx >= 0 && insertIdx < int(chain->size()))
            modeMenuSelectedIndex_ = fxCurrentMode((*chain)[(size_t)insertIdx], kind);
        const int rows = std::min(fxModeCount(kind), int(modeMenuRects_.size()));
        modeMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - 130.0f));
        modeMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - (28.0f + float(rows) * 18.0f)));
        modeMenuOpen_ = true;
    }

    void drawModeMenu()
    {
        if(!modeMenuOpen_)
            return;
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuGroup_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size())) { modeMenuOpen_ = false; return; }
        const int rows = std::min(fxModeCount(modeMenuKind_), int(modeMenuRects_.size()));
        constexpr float rowH = 18.0f;
        const float menuW = 122.0f;
        const Rect panel { modeMenuX_, modeMenuY_, menuW, 22.0f + rowH * float(rows) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, fxModeTitle(modeMenuKind_), nullptr);
        const auto &ins = (*chain)[(size_t)modeMenuInsertIdx_];
        const int cur = fxCurrentMode(ins, modeMenuKind_);
        modeMenuSelectedIndex_ = clampi(modeMenuSelectedIndex_, 0, std::max(0, rows - 1));
        for(int i = 0; i < rows; ++i)
        {
            modeMenuRects_[(size_t)i] = { panel.x + 6.0f, panel.y + 20.0f + float(i) * rowH, menuW - 12.0f, rowH - 2.0f };
            drawButton(modeMenuRects_[(size_t)i], fxModeName(modeMenuKind_, i), cur == i || modeMenuSelectedIndex_ == i);
        }
    }

    void commitModeMenuSelection()
    {
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuGroup_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size()))
        {
            modeMenuOpen_ = false;
            return;
        }
        const int rows = std::min(fxModeCount(modeMenuKind_), int(modeMenuRects_.size()));
        const int i = clampi(modeMenuSelectedIndex_, 0, std::max(0, rows - 1));
        auto &ins = (*chain)[(size_t)modeMenuInsertIdx_];
        if(modeMenuKind_ == InsertFilter)     ins.filter.algo = static_cast<synth::InsertFilterAlgo>(i);
        else if(modeMenuKind_ == InsertDist)  ins.dist.algo = static_cast<synth::InsertDistAlgo>(i);
        else if(modeMenuKind_ == InsertDelay) ins.delay.pingpong = (i == 1);
        else if(modeMenuKind_ == InsertConvReverb && i < int(irFiles_.size()))
            loadImpulseIntoInsert(ins, irFiles_[(size_t)i].first, irFiles_[(size_t)i].second);
        commitChainChange(modeMenuTrackId_, modeMenuGroup_);
        modeMenuOpen_ = false;
    }

    bool handleModeMenuClick(float x, float y)
    {
        if(!modeMenuOpen_)
            return false;
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuGroup_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size()))
        {
            modeMenuOpen_ = false;
            return true;
        }
        const int rows = std::min(fxModeCount(modeMenuKind_), int(modeMenuRects_.size()));
        for(int i = 0; i < rows; ++i)
            if(modeMenuRects_[(size_t)i].contains(x, y))
            {
                modeMenuSelectedIndex_ = i;
                if(currentClickIsDouble_)
                    commitModeMenuSelection();
                return true;
            }
        modeMenuOpen_ = false;
        return true;
    }

    bool handleInsertButtonClick(float x, float y)
    {
        // Press on a strip insert chip: pend (click = add-menu for "+", drag = reorder).
        for(const auto &hit : insertHits_)
            if(hit.rect.contains(x, y))
            {
                insertPending_ = true;
                insertDragActive_ = false;
                insertPendTrackId_ = hit.trackId;
                insertPendGroup_ = hit.groupIdx;
                insertPendSlot_ = hit.slot;  // -1 = the "+ add" chip
                insertPendX_ = x;
                insertPendY_ = y;
                return true;
            }
        return false;
    }

    void finishInsertInteraction(float x, float y)
    {
        if(!insertPending_)
            return;
        const int fromSlot = insertPendSlot_;
        const int trackId = insertPendTrackId_;
        const int group = insertPendGroup_;
        const bool wasDrag = insertDragActive_;
        insertPending_ = false;
        insertDragActive_ = false;
        auto *chain = insertChainFor(trackId, group);
        if(chain == nullptr)
            return;
        if(fromSlot < 0)  // the "+ add" chip
        {
            if(!wasDrag) openInsertMenu(trackId, group, x, y);
            return;
        }
        if(!wasDrag)
            return;  // a click on an existing chip does nothing (edit in OSC editor)
        // Drag → reorder within the same chain (move fromSlot to the chip under cursor).
        for(const auto &hit : insertHits_)
            if(hit.trackId == trackId && hit.groupIdx == group && hit.slot >= 0 && hit.rect.contains(x, y))
            {
                int toSlot = hit.slot;
                if(toSlot != fromSlot && fromSlot < int(chain->size()) && toSlot < int(chain->size()))
                {
                    InsertEffect moved = (*chain)[(size_t)fromSlot];
                    chain->erase(chain->begin() + fromSlot);
                    chain->insert(chain->begin() + toSlot, moved);
                    commitChainChange(trackId, group);
                }
                break;
            }
    }

    void drawRouteContextMenu()
    {
        if(!routeContextMenuOpen_)
            return;
        static constexpr const char *labels[] = { "Mute Route (zero depth)", "Delete Route" };
        constexpr float rowH = 28.0f;
        const Rect panel { routeContextX_, routeContextY_, 200.0f, 30.0f + rowH * 2.0f };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(panel.x + 12.0f, panel.y + 8.0f, "Route");
        for(int i = 0; i < 2; ++i)
        {
            routeContextRects_[(size_t)i] = { panel.x + 8.0f, panel.y + 26.0f + float(i) * rowH,
                                               panel.w - 16.0f, rowH - 2.0f };
            drawButton(routeContextRects_[(size_t)i], labels[i], false);
        }
    }

    bool handleRouteContextMenuClick(float x, float y)
    {
        if(!routeContextMenuOpen_)
            return false;
        routeContextMenuOpen_ = false;
        for(int i = 0; i < 2; ++i)
        {
            if(!routeContextRects_[(size_t)i].contains(x, y))
                continue;
            const int ri = routeContextRuleIndex_;
            if(ri < 0 || ri >= synth::kMaxMatrixRules)
                break;
            auto &rule = rules_[(size_t)ri];
            if(i == 0)
                rule.depth = 0.0f;    // mute
            else
                rule.enabled = false; // delete
            if(auto *p = plugin())
                p->updateMatrixRule(ri, rule);
            break;
        }
        return true;
    }

    void drawMetaProcessContextMenu()
    {
        if(!metaProcessContextMenuOpen_)
            return;

        static constexpr const char *labels[] = {
            "Remove DC + Normalize",
            "Crossfade Boundaries",
            "Zero-Crossing Align",
            "Align Frame Phases",
            "Smooth Frame Energy",
            "Morph Selection to 256",
            "Morph Selection to 512"
        };
        constexpr float rowHeight = 28.0f;
        metaProcessContextPanelRect_ = { metaProcessContextX_, metaProcessContextY_, 224.0f,
                                         34.0f + rowHeight * float(metaProcessContextRects_.size()) };
        drawPanel(metaProcessContextPanelRect_, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(metaProcessContextPanelRect_.x + 12.0f,
                         metaProcessContextPanelRect_.y + 8.0f, "Frame Operations");
        for(size_t i = 0; i < metaProcessContextRects_.size(); ++i)
        {
            metaProcessContextRects_[i] = { metaProcessContextPanelRect_.x + 8.0f,
                                             metaProcessContextPanelRect_.y + 30.0f + float(i) * rowHeight,
                                             metaProcessContextPanelRect_.w - 16.0f, rowHeight - 2.0f };
            drawButton(metaProcessContextRects_[i], labels[i], false);
        }
    }

    void openMetaProcessContextMenu(float x, float y)
    {
        if(!harmonicEditorPanelRect_.contains(x, y))
        {
            metaProcessContextMenuOpen_ = false;
            return;
        }

        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            if(!metaFrameRects_[(size_t)local].contains(x, y))
                continue;
            const int frameIndex = metaFramePageStart_ + local;
            if(auto *track = currentTrack(); track != nullptr && frameIndex < track->metaOsc.frameCount
               && !metaFrameSelected_[(size_t)frameIndex])
            {
                metaFrameSelected_.fill(false);
                selectedMetaFrame_ = frameIndex;
                metaFrameSelected_[(size_t)frameIndex] = true;
                metaFrameRangeAnchor_ = frameIndex;
            }
            break;
        }

        constexpr float menuWidth = 224.0f;
        constexpr float menuHeight = 230.0f;
        metaProcessContextX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - menuWidth - 4.0f));
        metaProcessContextY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - menuHeight - 4.0f));
        metaProcessContextMenuOpen_ = true;
    }

    bool handleMetaProcessContextMenuClick(float x, float y)
    {
        if(!metaProcessContextMenuOpen_)
            return false;
        metaProcessContextMenuOpen_ = false;
        for(size_t i = 0; i < metaProcessContextRects_.size(); ++i)
        {
            if(!metaProcessContextRects_[i].contains(x, y))
                continue;
            if(i < 5)
                applyWavetableProcess(int(i));
            else
                applySelectedFrameMorph(i == 5 ? 256 : 512);
            break;
        }
        return true;
    }

    bool handleWavetableImportMenuClick(float x, float y)
    {
        if(!wavetableImportMenuOpen_)
            return false;
        if(manualCycleMinusRect_.contains(x, y))
        {
            manualCycleLength_ = std::max(32, manualCycleLength_ - 32);
            return true;
        }
        const int limits[3] = { 128, 256, 512 };
        for(int i = 0; i < 3; ++i)
        {
            if(importFrameLimitRects_[(size_t)i].contains(x, y))
            {
                importFrameLimit_ = limits[i];
                return true;
            }
        }
        if(manualCyclePlusRect_.contains(x, y))
        {
            manualCycleLength_ = std::min(65536, manualCycleLength_ + 32);
            return true;
        }
        if(manualCycleValueRect_.contains(x, y))
        {
            dragTarget_ = DragTarget::ManualCycleLength;
            applyDragValue(x, y);
            return true;
        }
        if(wavetableImportCancelRect_.contains(x, y))
        {
            wavetableImportMenuOpen_ = false;
            droppedWavPending_ = false;
            return true;
        }
        for(int i = 0; i < 5; ++i)
        {
            if(!wavetableImportModeRects_[(size_t)i].contains(x, y))
                continue;
            wavetableImportMode_ = static_cast<synth::WavetableImportMode>(i);
            wavetableImportMenuOpen_ = false;
            if(droppedWavPending_)
            {
                droppedWavPending_ = false;
                commitWavetableLoad();
            }
            else
                openWavetableFileBrowser();
            return true;
        }
        return true;
    }

    void drawPartialTableEditor(synth::SourceTrackParams &track)
    {
        auto &seed = track.partialBank;
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
        selectedPartialIndex_ = clampi(selectedPartialIndex_, 0, synth::kMaxWavetablePartials - 1);
        selectedMetaHarmonic_ = selectedPartialIndex_;
        ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
        auto &frame = seed.frames[(size_t)selectedMetaFrame_];
        auto &h = frame.harmonics[(size_t)selectedPartialIndex_];

        const Rect r { 42.0f, 92.0f, static_cast<float>(uiW()) - 84.0f,
                       static_cast<float>(uiH()) - 190.0f };
        harmonicEditorPanelRect_ = r;
        drawPanel(r, rgba(0x0b1117f7), rgba(0x4a6470ff));
        drawSectionTitle(r.x + 18.0f, r.y + 16.0f, "Partial Table Editor");
        harmonicEditorCloseRect_ = { r.x + r.w - 86.0f, r.y + 14.0f, 68.0f, 28.0f };
        drawButton(harmonicEditorCloseRect_, "Close", false);

        const float toolY = r.y + 50.0f;
        metaEditorImportRect_ = { r.x + 18.0f, toolY, 88.0f, 24.0f };
        metaEditorAddRect_ = { r.x + 112.0f, toolY, 52.0f, 24.0f };
        metaEditorDuplicateRect_ = { r.x + 170.0f, toolY, 70.0f, 24.0f };
        metaEditorDeleteRect_ = { r.x + 246.0f, toolY, 64.0f, 24.0f };
        metaEditorLeftRect_ = { r.x + 316.0f, toolY, 36.0f, 24.0f };
        metaEditorRightRect_ = { r.x + 358.0f, toolY, 36.0f, 24.0f };
        drawButton(metaEditorImportRect_, "Import KWT", false);
        drawButton(metaEditorAddRect_, "Add", false);
        drawButton(metaEditorDuplicateRect_, "Duplicate", false);
        drawButton(metaEditorDeleteRect_, "Delete", false);
        drawButton(metaEditorLeftRect_, "<", false);
        drawButton(metaEditorRightRect_, ">", false);
        fontSize(8.0f);
        fillColor(rgba(0x6a8090ff));
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 404.0f, toolY + 12.0f, "Ctrl+click=range  Ctrl+A=all  drag phase/level bars", nullptr);

        synth::WavetablePartialSlot frameStripSlot;
        frameStripSlot.frameCount = seed.frameCount;
        frameStripSlot.morph = seed.morph;
        frameStripSlot.frames = seed.frames;
        metaEditorFrameStripRect_ = { r.x + 18.0f, r.y + 82.0f, r.w - 36.0f, 48.0f };
        drawMetaFrameStrip(metaEditorFrameStripRect_, frameStripSlot);
        metaFrameScrollRect_ = { r.x + 18.0f, r.y + 134.0f, r.w - 36.0f, 12.0f };
        drawFrameScrollbar(metaFrameScrollRect_, frameStripSlot);

        char title[176];
        std::snprintf(title, sizeof(title), "Frame %03d/%03d  Partial %02d  Level %.3g  Phase %.3g  %s",
                      selectedMetaFrame_ + 1, seed.frameCount, selectedPartialIndex_ + 1,
                      h.amp, h.phase, metaEditorStatus_.c_str());
        drawLabelBox({ r.x + 18.0f, r.y + 150.0f, r.w - 36.0f, 26.0f }, title);

        const float totalDispH = r.h - 230.0f;
        const float phaseH = std::max(42.0f, totalDispH * 0.36f);
        const float ampH = std::max(72.0f, totalDispH - phaseH - 8.0f);
        const float dispY = r.y + 180.0f;

        harmonicEditorPhaseRect_ = { r.x + 18.0f, dispY, r.w - 36.0f, phaseH };
        drawPanel(harmonicEditorPhaseRect_, rgba(0x0d1620ff), rgba(0x1e3040ff));
        {
            const Rect &pr = harmonicEditorPhaseRect_;
            const float barW = pr.w / float(synth::kMaxWavetablePartials);
            const float midY = pr.y + pr.h * 0.5f;
            strokeLine(pr.x + 2.0f, midY, pr.x + pr.w - 2.0f, midY, rgba(0x2b3f48ff), 0.8f);
            scissor(pr.x + 2.0f, pr.y + 2.0f, pr.w - 4.0f, pr.h - 4.0f);
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                const float ph = frame.harmonics[(size_t)i].phase;
                const float norm = clampf(ph / kPi, -1.0f, 1.0f);
                const float bx = pr.x + float(i) * barW + 0.5f;
                const float bw = std::max(1.0f, barW - 1.0f);
                const float bh = norm * (pr.h * 0.44f);
                const float by = bh >= 0.0f ? midY - bh : midY;
                beginPath();
                rect(bx, by, bw, std::abs(bh));
                fillColor(i == selectedPartialIndex_ ? rgba(0xffa23add) : rgba(0x7f68b0aa));
                fill();
            }
            resetScissor();
            fontSize(8.0f);
            fillColor(rgba(0x6080a0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(pr.x + 4.0f, pr.y + 2.0f, "PHASE PER PARTIAL", nullptr);
        }

        harmonicEditorSpectrumRect_ = { r.x + 18.0f, dispY + phaseH + 8.0f, r.w - 36.0f, ampH };
        harmonicEditorBarsRect_ = harmonicEditorSpectrumRect_;
        drawPanel(harmonicEditorSpectrumRect_, rgba(0x101820ff), rgba(0x263842ff));
        {
            const Rect &sr = harmonicEditorSpectrumRect_;
            const float barW = sr.w / float(synth::kMaxWavetablePartials);
            scissor(sr.x + 2.0f, sr.y + 2.0f, sr.w - 4.0f, sr.h - 4.0f);
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                const auto &hm = frame.harmonics[(size_t)i];
                const float amp = clampf(hm.amp, 0.0f, 1.0f);
                const float bx = sr.x + float(i) * barW;
                const float bh = amp * (sr.h - 12.0f);
                beginPath();
                rect(bx + 0.5f, sr.y + sr.h - 6.0f - bh, std::max(1.0f, barW - 1.0f), bh);
                fillColor(i == selectedPartialIndex_ ? rgba(0x8be87dff) : rgba(0x4d8a80dd));
                fill();
            }
            resetScissor();
            fontSize(8.0f);
            fillColor(rgba(0x6080a0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(sr.x + 4.0f, sr.y + 2.0f, "PARTIAL LEVEL RATIO", nullptr);
        }

        const float sliderY = r.y + r.h - 50.0f;
        const float third = (r.w - 52.0f) / 3.0f;
        metaHarmonicRatioRect_ = { r.x + 18.0f, sliderY, third, 24.0f };
        metaHarmonicAmpRect_ = { metaHarmonicRatioRect_.x + third + 8.0f, sliderY, third, 24.0f };
        metaHarmonicPhaseRect_ = { metaHarmonicAmpRect_.x + third + 8.0f, sliderY, third, 24.0f };
        drawSlider(metaHarmonicRatioRect_, "Partial", float(selectedPartialIndex_) / float(synth::kMaxWavetablePartials - 1),
                   float(selectedPartialIndex_ + 1));
        drawSlider(metaHarmonicAmpRect_, "Level", h.amp, h.amp);
        drawSlider(metaHarmonicPhaseRect_, "Phase", (h.phase + kPi) / (2.0f * kPi), h.phase);
    }

    void drawHarmonicEditor()
    {
        if(!harmonicEditorOpen_)
            return;
        auto *track = currentTrack();
        if(track != nullptr && track->type == synth::SourceTrackType::PartialBank)
        {
            drawPartialTableEditor(*track);
            return;
        }
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
        {
            harmonicEditorOpen_ = false;
            harmonicEditorPanelRect_ = {};
            return;
        }

        const Rect r { 42.0f, 92.0f, static_cast<float>(uiW()) - 84.0f,
                       static_cast<float>(uiH()) - 190.0f };
        harmonicEditorPanelRect_ = r;
        drawPanel(r, rgba(0x0b1117f7), rgba(0x4a6470ff));
        drawSectionTitle(r.x + 18.0f, r.y + 16.0f, "Meta Wavetable Editor");
        harmonicEditorCloseRect_ = { r.x + r.w - 86.0f, r.y + 14.0f, 68.0f, 28.0f };
        drawButton(harmonicEditorCloseRect_, "Close", false);

        auto &slot = track->metaOsc;
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
        selectedMetaHarmonic_ = clampi(selectedMetaHarmonic_, 0, synth::kEditableWavetableHarmonics - 1);
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        auto &h = frame.harmonics[(size_t)selectedMetaHarmonic_];

        // Tool buttons row (no TIME/SPECTRUM tabs — both panels always visible)
        const float toolY = r.y + 50.0f;
        const float toolW = 64.0f;
        metaEditorImportRect_ = { r.x + 18.0f, toolY, 88.0f, 24.0f };
        metaEditorAddRect_ = { r.x + 112.0f, toolY, 52.0f, 24.0f };
        metaEditorDuplicateRect_ = { r.x + 170.0f, toolY, 70.0f, 24.0f };
        metaEditorDeleteRect_ = { r.x + 246.0f, toolY, toolW, 24.0f };
        metaEditorLeftRect_ = { r.x + 316.0f, toolY, 36.0f, 24.0f };
        metaEditorRightRect_ = { r.x + 358.0f, toolY, 36.0f, 24.0f };
        metaSelAllRect_ = {};
        drawButton(metaEditorImportRect_, "Import WAV", false);
        drawButton(metaEditorAddRect_, "Add", false);
        drawButton(metaEditorDuplicateRect_, "Duplicate", false);
        drawButton(metaEditorDeleteRect_, "Delete", false);
        drawButton(metaEditorLeftRect_, "<", false);
        drawButton(metaEditorRightRect_, ">", false);
        // Selection hint: Ctrl+click = range, Ctrl+A = all.
        fontSize(8.0f); fillColor(rgba(0x6a8090ff)); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 404.0f, toolY + 12.0f, "Ctrl+click=range  Ctrl+A=all", nullptr);

        // Frame strip (48 px tall — room for mini waveform preview)
        metaEditorFrameStripRect_ = { r.x + 18.0f, r.y + 82.0f, r.w - 36.0f, 48.0f };
        drawMetaFrameStrip(metaEditorFrameStripRect_, slot);

        // Continuous scroll bar below frame strip
        metaFrameScrollRect_ = { r.x + 18.0f, r.y + 134.0f, r.w - 36.0f, 12.0f };
        drawFrameScrollbar(metaFrameScrollRect_, slot);

        // Status bar
        char title[160];
        std::snprintf(title, sizeof(title), "Frame %02d/%02d  Bin %03d  Amp %.3g  Phase %.3g  %s",
                      selectedMetaFrame_ + 1, slot.frameCount, selectedMetaHarmonic_ + 1,
                      h.amp, h.phase, metaEditorStatus_.c_str());
        drawLabelBox({ r.x + 18.0f, r.y + 150.0f, r.w - 36.0f, 26.0f }, title);

        // 三分布局: 相位图(22%) + 振幅谱(28%) + 波形(50%)
        const float totalDispH = r.h - 230.0f;
        const float phaseH = std::max(30.0f, totalDispH * 0.22f);
        const float specH  = std::max(30.0f, totalDispH * 0.28f);
        const float waveH  = totalDispH - phaseH - specH - 12.0f;
        const float dispY  = r.y + 180.0f;

        // --- 相位图面板 (上方) ---
        harmonicEditorPhaseRect_ = { r.x + 18.0f, dispY, r.w - 36.0f, phaseH };
        drawPanel(harmonicEditorPhaseRect_, rgba(0x0d1620ff), rgba(0x1e3040ff));
        {
            const Rect &pr = harmonicEditorPhaseRect_;
            const float barW = pr.w / float(synth::kEditableWavetableHarmonics);
            const float midY = pr.y + pr.h * 0.5f;
            strokeLine(pr.x + 2.0f, midY, pr.x + pr.w - 2.0f, midY, rgba(0x2b3f48ff), 0.8f);
            scissor(pr.x + 2.0f, pr.y + 2.0f, pr.w - 4.0f, pr.h - 4.0f);
            for(int i = 0; i < synth::kEditableWavetableHarmonics; ++i)
            {
                const float ph = frame.harmonics[(size_t)i].phase; // -π..+π
                const float norm = ph / kPi; // -1..+1
                const float bx = pr.x + float(i) * barW + 0.5f;
                const float bw = std::max(1.0f, barW - 1.0f);
                const float bh = norm * (pr.h * 0.44f);
                const float by = bh >= 0.0f ? midY - bh : midY;
                beginPath();
                rect(bx, by, bw, std::abs(bh));
                fillColor(i == selectedMetaHarmonic_ ? rgba(0xff9030dd) : rgba(0x7060a0aa));
                fill();
            }
            resetScissor();
            // 标签
            fontSize(8.0f);
            fillColor(rgba(0x6080a0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(pr.x + 4.0f, pr.y + 2.0f, "PHASE", nullptr);
        }

        // --- Spectrum panel (harmonic amplitude) ---
        harmonicEditorSpectrumRect_ = { r.x + 18.0f, dispY + phaseH + 6.0f, r.w - 36.0f, specH };
        drawPanel(harmonicEditorSpectrumRect_, rgba(0x101820ff), rgba(0x263842ff));
        {
            const Rect &sr = harmonicEditorSpectrumRect_;
            const float barW = sr.w / float(synth::kEditableWavetableHarmonics);
            scissor(sr.x + 2.0f, sr.y + 2.0f, sr.w - 4.0f, sr.h - 4.0f);
            for(int i = 0; i < synth::kEditableWavetableHarmonics; ++i)
            {
                const float amp = clampf(frame.harmonics[(size_t)i].amp, 0.0f, 1.0f);
                const float bx = sr.x + float(i) * barW;
                const float bh = amp * (sr.h - 10.0f);
                beginPath();
                rect(bx + 0.5f, sr.y + sr.h - 5.0f - bh, std::max(1.0f, barW - 1.0f), bh);
                fillColor(i == selectedMetaHarmonic_ ? rgba(0x8be87dff) : rgba(0x4d7780dd));
                fill();
            }
            resetScissor();
        }

        // --- Waveform panel (time domain) ---
        harmonicEditorBarsRect_ = { r.x + 18.0f, harmonicEditorSpectrumRect_.y + specH + 6.0f, r.w - 36.0f, waveH };
        drawPanel(harmonicEditorBarsRect_, rgba(0x101820ff), rgba(0x263842ff));
        {
            const Rect &wr = harmonicEditorBarsRect_;
            strokeLine(wr.x + 4.0f, wr.y + wr.h * 0.5f,
                       wr.x + wr.w - 4.0f, wr.y + wr.h * 0.5f, rgba(0x2b3f48ff), 1.0f);
            scissor(wr.x + 2.0f, wr.y + 2.0f, wr.w - 4.0f, wr.h - 4.0f);
            beginPath();
            for(int i = 0; i < 512; ++i)
            {
                const float t = float(i) / 511.0f;
                float value = 0.0f;
                if(frame.waveform)
                {
                    const int sample = clampi(int(t * float(synth::kWavetableSize - 1)), 0, synth::kWavetableSize - 1);
                    value = (*frame.waveform)[(size_t)sample];
                }
                else
                {
                    for(const auto &harmonic : frame.harmonics)
                        if(harmonic.amp > 0.0f && harmonic.ratio > 0.0f)
                            value += harmonic.amp * std::sin(2.0f * kPi * t * harmonic.ratio + harmonic.phase);
                    value = clampf(value, -1.0f, 1.0f);
                }
                const float px = wr.x + 4.0f + t * (wr.w - 8.0f);
                const float py = wr.y + wr.h * 0.5f - value * (wr.h * 0.42f);
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(rgba(0x63d2ffff));
            strokeWidth(1.5f);
            stroke();
            resetScissor();
        }

        const float sliderY = r.y + r.h - 50.0f;
        const float third = (r.w - 52.0f) / 3.0f;
        metaHarmonicRatioRect_ = { r.x + 18.0f, sliderY, third, 24.0f };
        metaHarmonicAmpRect_ = { metaHarmonicRatioRect_.x + third + 8.0f, sliderY, third, 24.0f };
        metaHarmonicPhaseRect_ = { metaHarmonicAmpRect_.x + third + 8.0f, sliderY, third, 24.0f };
        drawSlider(metaHarmonicRatioRect_, "Bin", float(selectedMetaHarmonic_) / float(synth::kEditableWavetableHarmonics - 1), float(selectedMetaHarmonic_ + 1));
        drawSlider(metaHarmonicAmpRect_, "H Amp", h.amp, h.amp);
        drawSlider(metaHarmonicPhaseRect_, "H Phase", (h.phase + kPi) / (2.0f * kPi), h.phase);
    }

    void drawCurrentPage()
    {
        const Rect page { 16.0f, 82.0f, static_cast<float>(uiW()) - 32.0f,
                          static_cast<float>(uiH()) - 184.0f };
        drawPanel(page, rgba(0x10171bff), rgba(0x293842ff));

        // Layout: editor (top-left) + matrix (top-right narrow column), strips along
        // the full-width bottom row. Matrix gets the tall top column so its env
        // editors stop overflowing; strips get the full width so they aren't cramped.
        const float gap     = 14.0f;
        const float bottomH = clampf(page.h * layoutBottomRatio_, 200.0f, page.h - 160.0f);
        const float topH    = page.h - bottomH - gap * 2.0f;
        const float matrixW = clampf(page.w * layoutMatrixRatio_, 340.0f, page.w * 0.5f);

        const Rect matrix { page.x + page.w - gap - matrixW, page.y + gap, matrixW, topH };
        const Rect editor { page.x + gap, page.y + gap,
                            std::max(200.0f, matrix.x - page.x - gap * 2.0f), topH };
        const Rect strip  { page.x + gap, page.y + topH + gap * 2.0f,
                            page.w - gap * 2.0f, bottomH - gap };

        drawTrackEditor(editor);
        drawMatrixDashboard(matrix);
        drawStripRack(strip);

        // Layout is fixed (no draggable splitters) — the whole canvas scales as a
        // unit, so per-panel resize handles are unnecessary.
        layoutVSplitHandle_     = {};
        layoutRackSplitHandle_  = {};
        layoutStripSplitHandle_ = {};
    }

    void drawSourceRack(const Rect &r)
    {
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        drawSectionTitle(r.x + 14.0f, r.y + 12.0f, "SOURCE RACK");
        addTrackRect_ = { r.x + 14.0f, r.y + 40.0f, r.w - 88.0f, 28.0f };
        removeTrackRect_ = { r.x + r.w - 66.0f, r.y + 40.0f, 52.0f, 28.0f };
        drawButton(addTrackRect_, addTrackMenuOpen_ ? "Choose Source Type" : "+ Add Source Track", addTrackMenuOpen_);
        drawButton(removeTrackRect_, "DEL", false);
        const char *labels[4] = { "Partial Bank", "Meta Oscillator", "Basic Oscillator", "Sample / Noise" };
        for(int i = 0; i < 4; ++i)
        {
            addTrackTypeRects_[(size_t)i] = addTrackMenuOpen_
                                                ? Rect { r.x + 14.0f, r.y + 74.0f + float(i) * 26.0f, r.w - 28.0f, 22.0f }
                                                : Rect {};
            if(addTrackMenuOpen_)
                drawButton(addTrackTypeRects_[(size_t)i], labels[i], false);
        }

        if(generator_.tracks.empty())
            generator_.tracks.push_back(synth::SourceTrackParams {});
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        const float startY = r.y + (addTrackMenuOpen_ ? 188.0f : 82.0f);
        const float rowH = 60.0f; // taller rows to fit mini waveform preview
        const float rowGap = 4.0f;
        for(auto &rr : trackRowRects_)
            rr = {};
        for(size_t i = 0; i < generator_.tracks.size() && i < trackRowRects_.size(); ++i)
        {
            const auto &track = generator_.tracks[i];
            const Rect row { r.x + 14.0f, startY + float(i) * (rowH + rowGap), r.w - 28.0f, rowH };
            trackRowRects_[i] = row;
            const bool isSelected = selectedTrack_ == int(i);
            const bool refsEnv = clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) == selectedAmpEnv_;
            drawPanel(row, isSelected ? rgba(0x17242cff) : rgba(0x101820ff),
                      isSelected ? rgba(0x70d77aff) : rgba(0x263842ff));
            char label[64] {};
            std::snprintf(label, sizeof(label), "%02zu  %s", i + 1, track.name.c_str());
            fontSize(10.0f);
            fillColor(isSelected || refsEnv ? rgba(0x9eff50ff) : rgba(0xb0c8d0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(row.x + 6.0f, row.y + 4.0f, label, nullptr);
            // Mini waveform preview for MetaOscillator
            if(track.type == synth::SourceTrackType::MetaOscillator && track.metaOsc.frameCount > 0)
            {
                // Get live morph from audio engine if available
                float liveMorph = track.metaOsc.morph;
                if(const auto *p = plugin())
                    liveMorph = p->sourceLiveMorph(int(i));
                const int frameIdx = clampi(
                    int(liveMorph * float(track.metaOsc.frameCount - 1) + 0.5f),
                    0, track.metaOsc.frameCount - 1);
                const auto &frm = track.metaOsc.frames[(size_t)frameIdx];
                const Rect wr { row.x + 4.0f, row.y + 20.0f, row.w - 8.0f, rowH - 24.0f };
                scissor(wr.x, wr.y, wr.w, wr.h);
                const float midY = wr.y + wr.h * 0.5f;
                beginPath();
                for(int s = 0; s < int(wr.w); ++s)
                {
                    const float t = float(s) / wr.w;
                    const float v = sampleFrame(frm, t);
                    const float px2 = wr.x + float(s);
                    const float py2 = midY - v * wr.h * 0.46f;
                    if(s == 0) moveTo(px2, py2); else lineTo(px2, py2);
                }
                strokeColor(isSelected ? rgba(0x9eff50cc) : rgba(0x4d7780bb));
                strokeWidth(1.0f);
                stroke();
                resetScissor();
            }
            else
            {
                fontSize(9.0f);
                fillColor(rgba(0x607080ff));
                textAlign(ALIGN_LEFT | ALIGN_TOP);
                text(row.x + 6.0f, row.y + 22.0f, synth::sourceTrackTypeName(track.type), nullptr);
            }
        }
        const int metaUsed = int(std::count_if(generator_.tracks.begin(), generator_.tracks.end(), [](const auto &t) {
            return t.type == synth::SourceTrackType::MetaOscillator;
        }));
        int partialUsed = 0;
        int basicUsed = 0;
        int noiseUsed = 0;
        for(const auto &t : generator_.tracks)
        {
            if(t.type == synth::SourceTrackType::PartialBank)
                partialUsed += t.partialBank.partialCount;
            else if(t.type == synth::SourceTrackType::BasicOscillator)
                ++basicUsed;
            else if(t.type == synth::SourceTrackType::SampleNoise)
                ++noiseUsed;
        }
        drawLabelBox({ r.x + 14.0f, r.y + r.h - 86.0f, r.w - 28.0f, 22.0f },
                     buttonText("Meta %d/8   Partials %d/64", metaUsed, partialUsed));
        drawLabelBox({ r.x + 14.0f, r.y + r.h - 58.0f, r.w - 28.0f, 22.0f },
                     buttonText("Basic %d/8   Noise %d/4", basicUsed, noiseUsed));
        drawLabelBox({ r.x + 14.0f, r.y + r.h - 30.0f, r.w - 28.0f, 22.0f },
                     partialUsed > 64 || metaUsed > 8 ? "ENGINE BUDGET OVER" : "ENGINE BUDGET OK");
    }

    synth::SourceTrackParams *currentTrack()
    {
        if(generator_.tracks.empty())
            return nullptr;
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        return &generator_.tracks[(size_t)selectedTrack_];
    }

    static float modulationDepthLimit(synth::ModDestination destination)
    {
        switch(destination)
        {
            case synth::ModDestination::PitchOct: return 4.0f;
            case synth::ModDestination::PitchSem: return 12.0f;
            case synth::ModDestination::PitchFine:
            case synth::ModDestination::PitchCrs: return 100.0f;
            case synth::ModDestination::Freq: return 2.0f;
            default: return 1.0f;
        }
    }

    static float defaultModulationDepth(synth::ModDestination destination)
    {
        switch(destination)
        {
            case synth::ModDestination::PitchOct: return 1.0f;
            case synth::ModDestination::PitchSem: return 12.0f;
            case synth::ModDestination::PitchFine:
            case synth::ModDestination::PitchCrs: return 50.0f;
            case synth::ModDestination::Freq: return 0.25f;
            default: return 0.5f;
        }
    }

    static Color modulationSourceColor(synth::ModSource source)
    {
        if(source >= synth::ModSource::Lfo1 && source <= synth::ModSource::Lfo4)
            return rgba(0x55c9ffff);
        if(source >= synth::ModSource::Env1 && source <= synth::ModSource::Env4)
            return rgba(0xffa84fff);
        if(source >= synth::ModSource::Adsr1 && source <= synth::ModSource::Adsr4)
            return rgba(0x6ee7a0ff);
        return rgba(0xb68cffff);
    }

    ModRouteTarget modRouteTargetAt(float x, float y) const
    {
        if(generator_.tracks.empty() || selectedTrack_ < 0 || selectedTrack_ >= int(generator_.tracks.size()))
            return {};
        const auto &track = generator_.tracks[(size_t)selectedTrack_];
        const auto make = [&](const Rect &rect, synth::ModDestination destination, const char *name) {
            return rect.w > 0.0f && rect.contains(x, y)
                       ? ModRouteTarget { true, destination, track.id, rect, name }
                       : ModRouteTarget {};
        };
        ModRouteTarget target;
        if((target = make(trackGainRect_, synth::ModDestination::TrackGain, "Gain")).valid) return target;
        if((target = make(trackPanRect_, synth::ModDestination::TrackPan, "Pan")).valid) return target;
        if((target = make(partialAmpRect_, synth::ModDestination::Amp, "Amp")).valid) return target;
        if((target = make(partialRatioRect_, synth::ModDestination::Freq, "Freq")).valid) return target;
        if((target = make(metaOctRect_, synth::ModDestination::PitchOct, "Oct")).valid) return target;
        if((target = make(metaSemRect_, synth::ModDestination::PitchSem, "Sem")).valid) return target;
        if((target = make(metaFinRect_, synth::ModDestination::PitchFine, "Fine")).valid) return target;
        if((target = make(metaCrsRect_, synth::ModDestination::PitchCrs, "Crs")).valid) return target;
        if((target = make(metaMorphRect_, synth::ModDestination::MetaMorph, "Morph")).valid) return target;
        if((target = make(metaWarpAmountRect_, synth::ModDestination::MetaWarp, "Warp")).valid) return target;
        if((target = make(metaPhaseRect_, synth::ModDestination::Phase, "Phase")).valid) return target;
        if((target = make(metaPanRect_, synth::ModDestination::MetaPan, "Pan")).valid) return target;
        (void)track;
        // ROUTE FX insert knobs (flattened editor) → generic InsertP0-3 destination.
        for(const auto &h : fxKnobHits_)
            if(h.trackId >= 0 && h.rect.w > 0.0f && h.rect.contains(x, y))
            {
                ModRouteTarget t;
                t.valid = true;
                t.destination = static_cast<synth::ModDestination>(int(synth::ModDestination::InsertP0) + h.knob);
                t.trackId = uint32_t(h.trackId);
                t.rect = h.rect;
                t.name = destName(t.destination);
                t.slot = h.insertIdx;
                return t;
            }
        return {};
    }

    Rect modulationDestinationRect(const synth::MatrixRule &rule) const
    {
        switch(rule.dest)
        {
            case synth::ModDestination::TrackGain: return trackGainRect_;
            case synth::ModDestination::TrackPan: return trackPanRect_;
            case synth::ModDestination::MetaPan: return metaPanRect_;
            case synth::ModDestination::PitchOct: return metaOctRect_;
            case synth::ModDestination::PitchSem: return metaSemRect_;
            case synth::ModDestination::PitchFine: return metaFinRect_;
            case synth::ModDestination::PitchCrs: return metaCrsRect_;
            case synth::ModDestination::MetaMorph: return metaMorphRect_;
            case synth::ModDestination::MetaWarp: return metaWarpAmountRect_;
            case synth::ModDestination::Amp: return partialAmpRect_;
            case synth::ModDestination::Freq: return partialRatioRect_;
            case synth::ModDestination::Phase: return metaPhaseRect_;
            case synth::ModDestination::InsertP0:
            case synth::ModDestination::InsertP1:
            case synth::ModDestination::InsertP2:
            case synth::ModDestination::InsertP3:
            {
                const int param = int(rule.dest) - int(synth::ModDestination::InsertP0);
                for(const auto &h : fxKnobHits_)
                    if(h.trackId == int(rule.targetTrackId) && h.insertIdx == rule.targetSlot && h.knob == param)
                        return h.rect;
                return {};
            }
            default: return {};
        }
    }

    void beginModRouteDrag(synth::ModSource source, const Rect &sourceRect, float x, float y)
    {
        modRouteDragActive_ = true;
        modRouteDragMoved_ = false;
        modRouteSource_ = source;
        modRouteSourceRect_ = sourceRect;
        modRouteStartX_ = modRouteMouseX_ = x;
        modRouteStartY_ = modRouteMouseY_ = y;
        modRouteHover_ = {};
    }

    void finishModRouteDrag(float x, float y)
    {
        const auto target = modRouteTargetAt(x, y);
        if(!target.valid)
            return;
        const bool isEffectDest = synth::insertModParamForDest(target.destination) >= 0;
        int ruleIndex = -1;
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
        {
            const auto &rule = rules_[(size_t)i];
            if(rule.source == modRouteSource_ && rule.dest == target.destination
               && rule.targetTrackId == target.trackId
               && (!isEffectDest || rule.targetSlot == target.slot))
            {
                ruleIndex = i;
                break;
            }
            if(ruleIndex < 0 && !rule.enabled)
                ruleIndex = i;
        }
        if(ruleIndex < 0)
            return;
        selectedRule_ = ruleIndex;
        auto &rule = rules_[(size_t)ruleIndex];
        rule.enabled = true;
        rule.source = modRouteSource_;
        rule.dest = target.destination;
        rule.targetTrackId = target.trackId;
        rule.targetSlot = target.slot;
        rule.weight = synth::WeightMode::All;
        if(std::abs(rule.depth) < 1.0e-6f)
            rule.depth = defaultModulationDepth(rule.dest);
        if(modRouteSource_ >= synth::ModSource::Lfo1 && modRouteSource_ <= synth::ModSource::Lfo4)
        {
            selectedLfo_ = int(modRouteSource_) - int(synth::ModSource::Lfo1);
            lfos_[(size_t)selectedLfo_].enabled = true;
        }
        else if(modRouteSource_ >= synth::ModSource::Env1 && modRouteSource_ <= synth::ModSource::Env4)
        {
            selectedEnv_ = int(modRouteSource_) - int(synth::ModSource::Env1);
            envs_[(size_t)selectedEnv_].enabled = true;
        }
        else if(modRouteSource_ >= synth::ModSource::Adsr1 && modRouteSource_ <= synth::ModSource::Adsr4)
        {
            selectedAmpEnv_ = int(modRouteSource_) - int(synth::ModSource::Adsr1);
        }
        pushMatrix();
    }

    bool handleModDepthPress(float x, float y)
    {
        const auto *track = currentTrack();
        if(track == nullptr)
            return false;
        for(int offset = 0; offset < synth::kMaxMatrixRules; ++offset)
        {
            const int index = (selectedRule_ + offset) % synth::kMaxMatrixRules;
            const auto &rule = rules_[(size_t)index];
            if(!rule.enabled || rule.targetTrackId != track->id)
                continue;
            const Rect rect = modulationDestinationRect(rule);
            if(rect.w <= 0.0f)
                continue;
            const float cx = rect.x + rect.w * 0.5f;
            const float cy = rect.y + rect.h * 0.5f;
            const float radius = std::min(rect.w, rect.h) * 0.5f + 4.0f;
            const float distance = std::hypot(x - cx, y - cy);
            if(std::abs(distance - radius) > 5.0f)
                continue;
            selectedRule_ = index;
            dragTarget_ = DragTarget::ModDepth;
            dragStartY_ = y;
            dragStartDepth_ = rule.depth;
            dragDepthLimit_ = modulationDepthLimit(rule.dest);
            return true;
        }
        return false;
    }

    void drawModulationOverlays()
    {
        const auto *track = currentTrack();
        if(track != nullptr)
        {
            for(const auto &rule : rules_)
            {
                if(!rule.enabled || rule.targetTrackId != track->id)
                    continue;
                const Rect rect = modulationDestinationRect(rule);
                if(rect.w <= 0.0f)
                    continue;
                const Color color = modulationSourceColor(rule.source);
                const float cx = rect.x + rect.w * 0.5f;
                const float cy = rect.y + rect.h * 0.5f;
                const float radius = std::min(rect.w, rect.h) * 0.5f + 4.0f;
                beginPath();
                arc(cx, cy, radius, -0.75f * kPi, 0.75f * kPi, CW);
                strokeColor(rgba(0x263842ff));
                strokeWidth(3.0f);
                stroke();
                const float amount = clampf(std::abs(rule.depth) / modulationDepthLimit(rule.dest), 0.0f, 1.0f);
                beginPath();
                arc(cx, cy, radius, -0.75f * kPi, (-0.75f + 1.5f * amount) * kPi, CW);
                strokeColor(color);
                strokeWidth(3.0f);
                stroke();
            }
        }

        // Highlight routed/dragging sources with a colored BORDER only, so the
        // button label underneath stays visible (no opaque overlay).
        const auto highlightSource = [&](const Rect &rc, Color col) {
            beginPath();
            roundedRect(rc.x + 1.0f, rc.y + 1.0f, rc.w - 2.0f, rc.h - 2.0f, 6.0f);
            strokeColor(col);
            strokeWidth(2.0f);
            stroke();
        };
        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            const auto source = static_cast<synth::ModSource>(int(synth::ModSource::Lfo1) + i);
            const bool routed = std::any_of(rules_.begin(), rules_.end(),
                                            [source](const auto &r) { return r.enabled && r.source == source; });
            if(routed || (modRouteDragActive_ && modRouteSource_ == source))
                highlightSource(lfoSelectRects_[(size_t)i], modulationSourceColor(source));
        }
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
        {
            const auto source = static_cast<synth::ModSource>(int(synth::ModSource::Env1) + i);
            const bool routed = std::any_of(rules_.begin(), rules_.end(),
                                            [source](const auto &r) { return r.enabled && r.source == source; });
            if(routed || (modRouteDragActive_ && modRouteSource_ == source))
                highlightSource(envSelectRects_[(size_t)i], modulationSourceColor(source));
        }
        if(modRouteDragActive_ && modRouteDragMoved_)
        {
            const Color color = modulationSourceColor(modRouteSource_);
            strokeLine(modRouteSourceRect_.x + modRouteSourceRect_.w * 0.5f,
                       modRouteSourceRect_.y + modRouteSourceRect_.h * 0.5f,
                       modRouteMouseX_, modRouteMouseY_, color, 2.5f);
            if(modRouteHover_.valid)
                highlightSource(modRouteHover_.rect, color);
        }
    }

    // Route-FX editor: shows the controls for whichever filter/distortion slot the
    // active strip/group routes to. Effects are always on (per-insert Bypass toggle).
    // Flattened route-FX editor: all inserts laid out left→right, arrows showing order.
    // Each panel: header (type + mode + bypass + delete) and 4 generic knobs.
    void drawRouteFxEditor(const Rect &region, std::vector<InsertEffect> *chain,
                           int chainTrackId, int chainGroup)
    {
        routeFxChainTrackId_ = chainTrackId;
        routeFxChainGroup_ = chainGroup;
        fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
        drawSectionTitle(region.x, region.y, "ROUTE FX (signal flows left -> right)");
        if(chain == nullptr)
            return;
        if(chain->empty())
        {
            drawLabelBox({ region.x, region.y + 24.0f, region.w, 24.0f },
                         "No FX. Add via a strip ROUTE  + chip.");
            return;
        }
        const float panelW = 150.0f, panelH = std::max(96.0f, region.h - 28.0f);
        const float arrowW = 16.0f;
        const float top = region.y + 24.0f;
        float x = region.x;
        const int n = int(chain->size());
        for(int i = 0; i < n; ++i)
        {
            const Rect p { x, top, panelW, panelH };
            drawInsertPanel(p, (*chain)[(size_t)i], chainTrackId, chainGroup, i);
            x += panelW;
            if(i + 1 < n)
            {
                // arrow to next
                const float ay = top + panelH * 0.5f;
                strokeLine(x + 2.0f, ay, x + arrowW - 2.0f, ay, rgba(0x8aa0b0ff), 2.0f);
                beginPath();
                moveTo(x + arrowW - 2.0f, ay); lineTo(x + arrowW - 7.0f, ay - 4.0f); lineTo(x + arrowW - 7.0f, ay + 4.0f);
                closePath(); fillColor(rgba(0x8aa0b0ff)); fill();
                x += arrowW;
            }
            if(x > region.x + region.w - panelW) break; // clip overflow (no horizontal scroll yet)
        }
    }

    void drawInsertPanel(const Rect &p, InsertEffect &e, int trackId, int groupIdx, int insertIdx)
    {
        const bool byp = e.bypass;
        drawPanel(p, rgba(byp ? 0x10141aff : 0x10171bff), rgba(0x3b5560ff));
        // header
        fontSize(8.5f); fillColor(rgba(byp ? 0x6a7884ff : 0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(p.x + 5.0f, p.y + 3.0f, insertTypeName(e.kind), nullptr);
        const Rect bypR { p.x + p.w - 38.0f, p.y + 2.0f, 18.0f, 14.0f };
        const Rect delR { p.x + p.w - 18.0f, p.y + 2.0f, 16.0f, 14.0f };
        drawButton(bypR, "b", !byp);
        drawButton(delR, "x", false);
        fxBypassHits_.push_back(FxBtnHit { bypR, trackId, groupIdx, insertIdx });
        fxDeleteHits_.push_back(FxBtnHit { delR, trackId, groupIdx, insertIdx });
        float knobsY = p.y + 20.0f;
        if(fxHasMode(e.kind))
        {
            const Rect modeR { p.x + 5.0f, p.y + 18.0f, p.w - 10.0f, 16.0f };
            const char *algo = e.kind == InsertConvReverb
                                   ? (e.conv.irName.empty() ? "Load IR..." : e.conv.irName.c_str())
                                   : fxModeName(e.kind, fxCurrentMode(e, e.kind));
            drawButton(modeR, algo, false);
            fxModeHits_.push_back(FxBtnHit { modeR, trackId, groupIdx, insertIdx });
            knobsY = p.y + 38.0f;
        }
        const float kw = (p.w - 12.0f) * 0.25f;
        const float knobH = 44.0f;
        for(int i = 0; i < 4; ++i)
        {
            const Rect kr { p.x + 4.0f + float(i) * (kw + 1.0f), knobsY, kw, knobH };
            drawKnob(kr, fxKnobName(e.kind, i), fxKnobNorm(e, i), fxKnobDisp(e, i));
            fxKnobHits_.push_back(FxKnobHit { kr, trackId, groupIdx, insertIdx, i });
        }
        // Response/transfer graph below the knobs (filter / eq / dist / comp).
        const float graphTop = knobsY + knobH + 6.0f;
        const float graphBot = p.y + p.h - 5.0f;
        if(fxHasGraph(e.kind) && graphBot - graphTop > 22.0f)
            drawInsertGraph({ p.x + 5.0f, graphTop, p.w - 10.0f, graphBot - graphTop }, e);
    }

    static bool fxHasGraph(int kind)
    {
        return kind == InsertFilter || kind == InsertEq || kind == InsertDist || kind == InsertComp;
    }

    // Magnitude (linear) of a biquad cascade at digital frequency w.
    static float biquadMagnitude(const synth::BiquadCoeffs &c, float w)
    {
        const float cw = std::cos(w), sw = std::sin(w);
        const float c2 = std::cos(2.0f * w), s2 = std::sin(2.0f * w);
        const float nRe = c.b0 + c.b1 * cw + c.b2 * c2;
        const float nIm = -(c.b1 * sw + c.b2 * s2);
        const float dRe = 1.0f + c.a1 * cw + c.a2 * c2;
        const float dIm = -(c.a1 * sw + c.a2 * s2);
        const float den = std::sqrt(dRe * dRe + dIm * dIm);
        const float num = std::sqrt(nRe * nRe + nIm * nIm);
        const float m = den > 1e-9f ? num / den : 0.0f;
        return std::pow(m, float(std::max(1, c.stages)));
    }

    void drawInsertGraph(const Rect &g, const InsertEffect &e)
    {
        // Backing panel + center line.
        beginPath();
        roundedRect(g.x, g.y, g.w, g.h, 3.0f);
        fillColor(rgba(0x0a0f13ff));
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();
        scissor(g.x + 1.0f, g.y + 1.0f, g.w - 2.0f, g.h - 2.0f);
        const float midY = g.y + g.h * 0.5f;
        strokeLine(g.x + 1.0f, midY, g.x + g.w - 1.0f, midY, DesignTokens::divider(), 1.0f);

        const Color line = DesignTokens::accentCyan();
        const int steps = std::max(8, int(g.w));
        const double sr = 48000.0;

        if(e.kind == InsertFilter || e.kind == InsertEq)
        {
            // Log-frequency magnitude response, +/-24 dB window.
            synth::BiquadCoeffs eqc[3];
            int nb = 1;
            synth::BiquadCoeffs single;
            if(e.kind == InsertFilter) { single = synth::designInsertBiquad(e.filter, sr); }
            else { synth::designEqBiquads(e.eq, sr, eqc); nb = 3; }
            const float fLo = 20.0f, fHi = 20000.0f;
            const float logLo = std::log10(fLo), logHi = std::log10(fHi);
            beginPath();
            for(int i = 0; i <= steps; ++i)
            {
                const float t = float(i) / float(steps);
                const float f = std::pow(10.0f, logLo + t * (logHi - logLo));
                const float w = 2.0f * kPi * f / float(sr);
                float mag = 1.0f;
                if(e.kind == InsertFilter) mag = biquadMagnitude(single, w);
                else for(int b = 0; b < nb; ++b) mag *= biquadMagnitude(eqc[b], w);
                const float db = 20.0f * std::log10(std::max(1e-4f, mag));
                const float yn = clampf((db + 24.0f) / 48.0f, 0.0f, 1.0f);
                const float px = g.x + t * g.w;
                const float py = g.y + g.h - yn * g.h;
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line); strokeWidth(1.4f); stroke();
        }
        else if(e.kind == InsertDist)
        {
            // Input/output transfer curve over x in [-1, 1].
            beginPath();
            for(int i = 0; i <= steps; ++i)
            {
                const float xin = -1.0f + 2.0f * float(i) / float(steps);
                float yo = synth::distShape(e.dist.algo, xin, e.dist.drive, e.dist.bias) * e.dist.outGain;
                yo = clampf(yo, -1.2f, 1.2f) / 1.2f;
                const float px = g.x + (xin * 0.5f + 0.5f) * g.w;
                const float py = midY - yo * (g.h * 0.5f - 2.0f);
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line); strokeWidth(1.4f); stroke();
        }
        else if(e.kind == InsertComp)
        {
            // Static compression curve: input dB (-60..0) -> output dB.
            const float thr = e.comp.threshDb;
            const float ratio = std::max(1.0f, e.comp.ratio);
            const float makeup = e.comp.makeupDb;
            beginPath();
            for(int i = 0; i <= steps; ++i)
            {
                const float inDb = -60.0f + 60.0f * float(i) / float(steps);
                float outDb = inDb <= thr ? inDb : thr + (inDb - thr) / ratio;
                outDb += makeup;
                const float px = g.x + (inDb + 60.0f) / 60.0f * g.w;
                const float py = g.y + g.h - clampf((outDb + 60.0f) / 60.0f, 0.0f, 1.0f) * g.h;
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line); strokeWidth(1.4f); stroke();
        }
        resetScissor();
    }

    bool handleRouteFxClick(float x, float y)
    {
        auto *chain = insertChainFor(routeFxChainTrackId_, routeFxChainGroup_);
        if(chain == nullptr)
            return false;
        for(const auto &h : fxBypassHits_)
            if(h.rect.contains(x, y) && h.insertIdx < int(chain->size()))
            { (*chain)[(size_t)h.insertIdx].bypass = !(*chain)[(size_t)h.insertIdx].bypass; commitChainChange(h.trackId, h.groupIdx); return true; }
        for(const auto &h : fxDeleteHits_)
            if(h.rect.contains(x, y) && h.insertIdx < int(chain->size()))
            { chain->erase(chain->begin() + h.insertIdx); commitChainChange(h.trackId, h.groupIdx); return true; }
        for(const auto &h : fxModeHits_)
            if(h.rect.contains(x, y) && h.insertIdx < int(chain->size()))
            { openModeMenu(h.trackId, h.groupIdx, h.insertIdx, (*chain)[(size_t)h.insertIdx].kind, x, y); return true; }
        return false;
    }

    // Source-modulation editor inside the OSC editor (rows of enable/source/type/depth).
    // Does routing track[srcIdx] → track[targetIdx] create a modulation cycle?
    bool modSourceCausesCycle(int targetIdx, int srcIdx) const
    {
        if(srcIdx < 0 || srcIdx == targetIdx)
            return srcIdx == targetIdx;
        // Follow dependency edges (track → its mod sources) from srcIdx; cycle if we reach target.
        std::array<bool, synth::kMaxSourceTracks> visited {};
        std::vector<int> stack { srcIdx };
        while(!stack.empty())
        {
            const int cur = stack.back();
            stack.pop_back();
            if(cur < 0 || cur >= int(generator_.tracks.size()) || visited[(size_t)cur])
                continue;
            visited[(size_t)cur] = true;
            for(const auto &m : generator_.tracks[(size_t)cur].mods)
            {
                if(!m.enabled || m.sourceTrack < 0)
                    continue;
                if(m.sourceTrack == targetIdx)
                    return true;
                stack.push_back(m.sourceTrack);
            }
        }
        return false;
    }

    static bool trackHasAnyMod(const synth::SourceTrackParams &t)
    {
        for(const auto &m : t.mods)
            if(modEntryActive(m)) return true;
        return false;
    }

    // Editor shown only for routed entries: [source] [type] [depth] [x]. One row per mod.
    void drawModEditor(const Rect &region, synth::SourceTrackParams &track)
    {
        drawSectionTitle(region.x, region.y, "MODULATION (source → this)");
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        const float rowH = std::max(20.0f, (region.h - 24.0f) / float(synth::kMaxTrackMods) - 4.0f);
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
        {
            auto &m = track.mods[(size_t)i];
            if(!modEntryActive(m) || m.sourceTrack >= int(generator_.tracks.size()))
                continue;
            const float ry = region.y + 24.0f + float(i) * (rowH + 4.0f);
            modSrcRects_[(size_t)i]    = { region.x, ry, region.w * 0.30f, rowH };
            modTypeRects_[(size_t)i]   = { modSrcRects_[(size_t)i].x + modSrcRects_[(size_t)i].w + 4.0f, ry, region.w * 0.18f, rowH };
            modDeleteRects_[(size_t)i] = { region.x + region.w - 22.0f, ry, 22.0f, rowH };
            modDepthRects_[(size_t)i]  = { modTypeRects_[(size_t)i].x + modTypeRects_[(size_t)i].w + 4.0f, ry,
                                          modDeleteRects_[(size_t)i].x - 4.0f - (modTypeRects_[(size_t)i].x + modTypeRects_[(size_t)i].w + 4.0f), rowH };
            std::snprintf(scratch_, sizeof(scratch_), "%s", generator_.tracks[(size_t)m.sourceTrack].name.c_str());
            drawButton(modSrcRects_[(size_t)i], scratch_, selectedModSlot_ == i);
            drawButton(modTypeRects_[(size_t)i], synth::sourceModTypeName(m.type), false);
            drawSlider(modDepthRects_[(size_t)i], "Depth", m.depth, m.depth);
            drawButton(modDeleteRects_[(size_t)i], "x", false);
        }
    }

    bool handleModColumnClick(float x, float y)
    {
        for(const auto &hit : modHits_)
            if(hit.rect.contains(x, y))
            {
                const int ti = trackIndexOfId(uint32_t(hit.trackId));
                if(ti >= 0)
                {
                    selectedTrack_ = ti;
                    selectedGroupView_ = -1;
                }
                if(hit.slot < 0)          // "+ add" row → pick a source
                    openModSourceMenu(hit.trackId, -1, x, y);
                else
                    selectedModSlot_ = hit.slot;  // select existing entry for OSC editing
                return true;
            }
        return false;
    }

    // Next selectable modulator track after `current` (skips self & cyclic), -1 = none.
    bool handleModEditorClick(float x, float y)
    {
        auto *track = currentTrack();
        if(track == nullptr)
            return false;
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
        {
            auto &m = track->mods[(size_t)i];
            if(modSrcRects_[(size_t)i].w > 0.0f && modSrcRects_[(size_t)i].contains(x, y))
            {
                // click source name → reselect via the source picker
                openModSourceMenu(int(track->id), i, x, y);
                return true;
            }
            if(modTypeRects_[(size_t)i].w > 0.0f && modTypeRects_[(size_t)i].contains(x, y))
            {
                m.type = static_cast<synth::SourceModType>((int(m.type) + 1) % synth::kSourceModTypeCount);
                pushCurrentTrack();
                return true;
            }
            if(modDeleteRects_[(size_t)i].w > 0.0f && modDeleteRects_[(size_t)i].contains(x, y))
            {
                m = synth::SourceModEntry {};  // remove entry
                if(selectedModSlot_ == i) selectedModSlot_ = -1;
                pushCurrentTrack();
                return true;
            }
        }
        return false;
    }

    // ---- Mod source picker menu ----
    void openModSourceMenu(int trackId, int slot, float x, float y)
    {
        modSourceMenuTrackId_ = trackId;
        modSourceMenuSlot_ = slot;
        const int rows = int(generator_.tracks.size()) + 1;  // remove + tracks
        modSourceMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - 150.0f));
        modSourceMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - (26.0f + float(rows) * 18.0f)));
        modSourceMenuOpen_ = true;
    }

    void drawModSourceMenu()
    {
        if(!modSourceMenuOpen_)
            return;
        const int self = trackIndexOfId(uint32_t(modSourceMenuTrackId_));
        const int n = int(generator_.tracks.size());
        constexpr float rowH = 18.0f;
        const float menuW = 150.0f;
        const Rect panel { modSourceMenuX_, modSourceMenuY_, menuW, 22.0f + rowH * float(n + 1) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, "Mod source", nullptr);
        modSourceMenuRects_.fill({});
        modSourceMenuRects_[0] = { panel.x + 6.0f, panel.y + 20.0f, menuW - 12.0f, rowH - 2.0f };
        drawButton(modSourceMenuRects_[0], "(remove)", false);
        for(int i = 0; i < n && i + 1 < int(modSourceMenuRects_.size()); ++i)
        {
            modSourceMenuRects_[(size_t)(i + 1)] = { panel.x + 6.0f, panel.y + 20.0f + float(i + 1) * rowH, menuW - 12.0f, rowH - 2.0f };
            const bool disabled = (i == self) || modSourceCausesCycleFor(self, i);
            std::snprintf(scratch_, sizeof(scratch_), "%s%s", generator_.tracks[(size_t)i].name.c_str(),
                          disabled ? "  (n/a)" : "");
            drawButton(modSourceMenuRects_[(size_t)(i + 1)], scratch_, false);
        }
    }

    // cycle check using an explicit self index (menu context)
    bool modSourceCausesCycleFor(int selfIdx, int cand) const
    {
        if(cand < 0 || cand == selfIdx) return cand == selfIdx;
        std::array<bool, synth::kMaxSourceTracks> visited {};
        std::vector<int> stack { cand };
        while(!stack.empty())
        {
            const int cur = stack.back(); stack.pop_back();
            if(cur < 0 || cur >= int(generator_.tracks.size()) || visited[(size_t)cur]) continue;
            visited[(size_t)cur] = true;
            for(const auto &m : generator_.tracks[(size_t)cur].mods)
            {
                if(!m.enabled || m.sourceTrack < 0) continue;
                if(m.sourceTrack == selfIdx) return true;
                stack.push_back(m.sourceTrack);
            }
        }
        return false;
    }

    bool handleModSourceMenuClick(float x, float y)
    {
        if(!modSourceMenuOpen_)
            return false;
        modSourceMenuOpen_ = false;
        const int self = trackIndexOfId(uint32_t(modSourceMenuTrackId_));
        if(self < 0)
            return true;
        auto &track = generator_.tracks[(size_t)self];
        // find target slot: existing slot, or first free
        int slot = modSourceMenuSlot_;
        if(slot < 0)
        {
            slot = -1;
            for(int i = 0; i < synth::kMaxTrackMods; ++i)
                if(!modEntryActive(track.mods[(size_t)i])) { slot = i; break; }
        }
        if(modSourceMenuRects_[0].contains(x, y))
        {
            if(modSourceMenuSlot_ >= 0)
            {
                track.mods[(size_t)modSourceMenuSlot_] = synth::SourceModEntry {};
                pushTrackById(track.id);
            }
            return true;
        }
        const int n = int(generator_.tracks.size());
        for(int i = 0; i < n; ++i)
        {
            if(!modSourceMenuRects_[(size_t)(i + 1)].contains(x, y))
                continue;
            if(i == self || modSourceCausesCycleFor(self, i) || slot < 0)
                return true;  // invalid choice / no free slot
            auto &m = track.mods[(size_t)slot];
            m.sourceTrack = int8_t(i);
            m.enabled = true;
            if(m.depth <= 0.0f) m.depth = 0.5f;
            selectedTrack_ = self;
            selectedModSlot_ = slot;
            pushTrackById(track.id);
            return true;
        }
        return true;
    }

    // Group OSC view: members → bus diagram + the group's route FX.
    void drawGroupEditor(const Rect &r, int gi)
    {
        drawPanel(r, rgba(0x140d1aff), rgba(0xc070e0aaU));
        clearTrackEditorRects();
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        if(gi < 0 || gi >= int(stripGroups_.size()))
            return;
        auto &grp = stripGroups_[(size_t)gi];
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, grp.name.c_str());
        drawSectionTitle(r.x + 16.0f, r.y + 44.0f, "Members -> Bus");
        float yy = r.y + 70.0f;
        for(int mi : grp.memberIndices)
        {
            if(mi < 0 || mi >= int(generator_.tracks.size()))
                continue;
            const auto &t = generator_.tracks[(size_t)mi];
            std::snprintf(scratch_, sizeof(scratch_), "%s  ->  %s", t.name.c_str(), grp.name.c_str());
            drawLabelBox({ r.x + 16.0f, yy, r.w - 32.0f, 22.0f }, scratch_);
            yy += 26.0f;
        }
        // Group route FX fills the lower portion
        const Rect fx { r.x + 16.0f, std::max(yy + 8.0f, r.y + r.h - 250.0f), r.w - 32.0f,
                        std::min(250.0f, r.y + r.h - std::max(yy + 8.0f, r.y + r.h - 250.0f) - 12.0f) };
        drawRouteFxEditor(fx, &grp.inserts, -1, gi);
    }

    void drawTrackEditor(const Rect &r)
    {
        // Group view takes over the editor when a group bus is selected.
        if(selectedGroupView_ >= 0 && selectedGroupView_ < int(stripGroups_.size()))
        {
            drawGroupEditor(r, selectedGroupView_);
            return;
        }
        drawPanel(r, rgba(0x0d151aff), rgba(0x3b5560ff));
        clearTrackEditorRects();
        auto *track = currentTrack();
        if(track == nullptr)
            return;
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, track->name.c_str());
        // Track type / ADSR-route / Duplicate live in the strip now, not here.
        trackOutputModeRect_ = {};  // output mode 选择从 UI 移除，默认 AudioAndMod
        track->outputMode = synth::SourceTrackOutputMode::AudioAndMod;
        track->ampEnvIndex = clampi(track->ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
        ampEnvSelectRect_ = {};
        duplicateEnvRect_ = {};

        // ---- Tabs: SOURCE / SHAPE / VOICE / MAPPING ----
        static const char *const editorTabs[] = { "SOURCE", "SHAPE", "VOICE", "MAPPING" };
        const Rect tabBar { r.x + 16.0f, r.y + 42.0f, std::min(r.w - 32.0f, 420.0f), 24.0f };
        drawTabBar(tabBar, editorTabs, 4, editorTab_, editorTabRects_.data());

        // Only the active tab repopulates its hit rects — clear them all first so a
        // hidden tab's stale controls can't catch clicks.
        unisonVoicesRect_ = unisonDetuneRect_ = unisonWidthRect_ = unisonPhaseRect_ = {};
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
        routeFxChainTrackId_ = -1;
        routeFxChainGroup_ = -1;

        const Rect content { r.x + 16.0f, r.y + 80.0f, r.w - 32.0f, r.h - 92.0f };
        switch(editorTab_)
        {
            case 1:  // SHAPE — routed insert FX chain (filter / dist / eq / delay …)
                drawRouteFxEditor(content, &track->inserts, int(track->id), -1);
                break;
            case 2:  // VOICE — unison / voicing
                drawVoiceTab(content, *track);
                break;
            case 3:  // MAPPING — per-source modulation routing
                drawModEditor(content, *track);
                break;
            default: // SOURCE — oscillator / source body
                if(track->type == synth::SourceTrackType::PartialBank)
                    drawPartialBankTrackEditor(content, *track);
                else if(track->type == synth::SourceTrackType::MetaOscillator)
                    drawMetaTrackEditor(content, *track);
                else if(track->type == synth::SourceTrackType::BasicOscillator)
                    drawBasicTrackEditor(content, *track);
                else
                    drawNoiseTrackEditor(content, *track);
                break;
        }
    }

    void drawVoiceTab(const Rect &r, synth::SourceTrackParams &track)
    {
        drawSectionTitle(r.x, r.y, "Unison / Voicing");
        const float kw = 78.0f;
        const float gap = 22.0f;
        const float ky = r.y + 44.0f;
        unisonVoicesRect_ = { r.x,                    ky, kw, kw };
        unisonDetuneRect_ = { r.x + (kw + gap),       ky, kw, kw };
        unisonWidthRect_  = { r.x + (kw + gap) * 2.0f, ky, kw, kw };
        unisonPhaseRect_  = { r.x + (kw + gap) * 3.0f, ky, kw, kw };
        drawKnob(unisonVoicesRect_, "Voices", float(track.unison.voices - 1) / 15.0f, float(track.unison.voices));
        drawKnob(unisonDetuneRect_, "Detune", track.unison.detuneCents / 80.0f, track.unison.detuneCents);
        drawKnob(unisonWidthRect_,  "Width",  track.unison.widthStereo, track.unison.widthStereo);
        drawKnob(unisonPhaseRect_,  "Rnd Ph", track.unison.phaseSpread, track.unison.phaseSpread);
    }

    void clearTrackEditorRects()
    {
        partialCountRect_ = {};
        inharmonicModeRect_ = {};
        inharmonicRect_ = {};
        partialSpectrumRect_ = {};
        partialAmpRect_ = {};
        partialRatioRect_ = {};
        metaEnableRect_ = {};
        metaWavetableNameRect_ = {};
        metaWavetablePrevRect_ = {};
        metaWavetableNextRect_ = {};
        metaWarpModeRect_ = {};
        metaFrameButtonRect_ = {};
        metaHarmonicEditRect_ = {};
        metaLoadRect_ = {};
        metaLoadPathRect_ = {};
        metaFrameStripRect_ = {};
        for(auto &r : metaFramePresetRects_)
            r = {};
        for(auto &r : metaFrameRects_)
            r = {};
        metaOctRect_ = {};
        metaSemRect_ = {};
        metaFinRect_ = {};
        metaCrsRect_ = {};
        metaRatioRect_ = {};
        metaAmpRect_ = {};
        metaPhaseRect_ = {};
        metaPanRect_ = {};
        metaFrameCountRect_ = {};
        metaMorphRect_ = {};
        metaWarpAmountRect_ = {};
        metaWaveformRect_ = {};
        metaHarmonicRatioRect_ = {};
        metaHarmonicAmpRect_ = {};
        metaHarmonicPhaseRect_ = {};
        basicShapeRect_ = {};
        basicPulseRect_ = {};
        basicSubRect_ = {};
        noiseModeRect_ = {};
        noiseColorRect_ = {};
        attackRect_ = {};
        decayRect_ = {};
        sustainRect_ = {};
        releaseRect_ = {};
        curveRect_ = {};
        ampEnvSelectRect_ = {};
        duplicateEnvRect_ = {};
        unisonVoicesRect_ = {};
        unisonDetuneRect_ = {};
        unisonWidthRect_ = {};
        unisonPhaseRect_ = {};
    }

    // 取得某 track 的 insert 链（存储在 SourceTrackParams 内）
    std::vector<InsertEffect> *trackInsertsFor(uint32_t trackId)
    {
        for(auto &t : generator_.tracks)
            if(t.id == trackId)
                return &t.inserts;
        return nullptr;
    }

    static bool trackHasRoutedFx(const synth::SourceTrackParams &t) { return !t.inserts.empty(); }

    static const char *insertKindShort(uint8_t kind)
    {
        switch(kind){case InsertFilter:return "F";case InsertDist:return "D";case InsertEq:return "EQ";
                     case InsertComp:return "CP";case InsertDelay:return "DL";case InsertReverb:return "RV";}
        return "?";
    }

    // Strip insert list: one chip per effect (no number), a trailing "+" to add. Drag to reorder.
    void drawInsertColumn(const Rect &region, const std::vector<InsertEffect> &inserts,
                          int trackId, int groupIdx)
    {
        const float gap = 2.0f, rh = 14.0f;
        const bool dragOnThis = insertDragActive_
                                && insertPendTrackId_ == trackId && insertPendGroup_ == groupIdx;
        int row = 0;
        const int maxRows = std::max(1, int((region.h + gap) / (rh + gap)));
        for(int i = 0; i < int(inserts.size()) && row < maxRows - 1; ++i, ++row)
        {
            const Rect b { region.x, region.y + float(row) * (rh + gap), region.w, rh };
            const auto &ins = inserts[(size_t)i];
            char lbl[24];
            if(fxHasMode(ins.kind))
                std::snprintf(lbl, sizeof(lbl), "%s %s%s", insertKindShort(ins.kind),
                              ins.kind == InsertFilter ? kFilterAlgoNames[int(ins.filter.algo)]
                                                       : kDistAlgoNames[int(ins.dist.algo)],
                              ins.bypass ? " b" : "");
            else
                std::snprintf(lbl, sizeof(lbl), "%s%s", insertTypeName(ins.kind), ins.bypass ? " b" : "");
            const bool isDragSrc = dragOnThis && insertPendSlot_ == i;
            drawButton(b, lbl, !isDragSrc);
            // Drop-target preview: highlight the chip the dragged insert would land on.
            if(dragOnThis && !isDragSrc && b.contains(insertDragX_, insertDragY_))
            {
                beginPath(); rect(b.x, b.y, b.w, b.h);
                strokeColor(rgba(0x9eff50ffU)); strokeWidth(1.6f); stroke();
            }
            insertHits_.push_back(InsertHit { b, trackId, groupIdx, i });
        }
        // trailing "+ add" chip
        const Rect addR { region.x, region.y + float(row) * (rh + gap), region.w, rh };
        drawButton(addR, "+ add fx", false);
        insertHits_.push_back(InsertHit { addR, trackId, groupIdx, -1 });
    }

    // Mixer-style modulation list in the strip: one mod slot per row (summary + click).
    static bool modEntryActive(const synth::SourceModEntry &m)
    {
        return m.sourceTrack >= 0;
    }

    void drawModColumn(const Rect &region, const synth::SourceTrackParams &track, int trackId)
    {
        const float rowH = 14.0f;
        const float gap = 2.0f;
        int row = 0;
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
        {
            const auto &m = track.mods[(size_t)i];
            if(!modEntryActive(m) || m.sourceTrack >= int(generator_.tracks.size()))
                continue;
            const Rect b { region.x, region.y + float(row) * (rowH + gap), region.w, rowH };
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "%s %s %d%%",
                          generator_.tracks[(size_t)m.sourceTrack].name.c_str(),
                          synth::sourceModTypeName(m.type), int(m.depth * 100.0f + 0.5f));
            const bool active = selectedTrack_ == trackIndexOfId(uint32_t(trackId)) && selectedModSlot_ == i;
            drawButton(b, lbl, active);
            modHits_.push_back(ModHit { b, trackId, i });
            ++row;
        }
        // "add" row (left- or right-click to pick a source)
        if(row < synth::kMaxTrackMods)
        {
            const Rect b { region.x, region.y + float(row) * (rowH + gap), region.w, rowH };
            drawButton(b, "+ add (R-click)", false);
            modHits_.push_back(ModHit { b, trackId, -1 });
        }
    }

    int trackIndexOfId(uint32_t id) const
    {
        for(size_t i = 0; i < generator_.tracks.size(); ++i)
            if(generator_.tracks[i].id == id) return int(i);
        return -1;
    }

    int matrixRouteCountForTrack(const synth::SourceTrackParams &track) const
    {
        int count = 0;
        for(const auto &rule : rules_)
            if(rule.enabled && rule.targetTrackId == track.id)
                ++count;
        return count;
    }

    int activeModCountForTrack(const synth::SourceTrackParams &track) const
    {
        int count = 0;
        for(const auto &mod : track.mods)
            if(mod.enabled && mod.sourceTrack >= 0)
                ++count;
        return count;
    }

    // Vertical mixer-style fader. norm 0..1 maps bottom->top.
    void drawFader(const Rect &r, float norm, const char *label, float value, bool active)
    {
        norm = clampf(norm, 0.0f, 1.0f);
        const float cx = r.x + r.w * 0.5f;
        const float top = r.y + 6.0f;
        const float bot = r.y + r.h - 16.0f;
        const float span = std::max(1.0f, bot - top);
        // Groove.
        beginPath();
        roundedRect(cx - 2.5f, top, 5.0f, span, 2.5f);
        fillColor(DesignTokens::controlBackground());
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();
        // Filled portion below the handle.
        const float hy = bot - norm * span;
        beginPath();
        roundedRect(cx - 2.5f, hy, 5.0f, bot - hy, 2.5f);
        fillColor(active ? DesignTokens::accentGreen() : DesignTokens::accentCyan());
        fill();
        // Handle.
        beginPath();
        roundedRect(cx - 9.0f, hy - 5.0f, 18.0f, 10.0f, 3.0f);
        fillPaint(linearGradient(cx, hy - 5.0f, cx, hy + 5.0f,
                                 shade(DesignTokens::panelRaised(), 0.22f),
                                 shade(DesignTokens::panelRaised(), -0.12f)));
        fill();
        strokeColor(active ? DesignTokens::accentGreen() : DesignTokens::border());
        strokeWidth(1.0f);
        stroke();
        // Label + value.
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.2f", value);
        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        text(cx, r.y + r.h - 11.0f, label, nullptr);
    }

    // Vertical peak level meter (green->yellow->red bottom to top).
    void drawLevelMeter(const Rect &r, float level)
    {
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 2.0f);
        fillColor(rgba(0x0a0f13ff));
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();
        const float v = clampf(level, 0.0f, 1.2f) / 1.2f;
        const float fillH = v * (r.h - 2.0f);
        if(fillH > 0.5f)
        {
            const float fy = r.y + r.h - 1.0f - fillH;
            beginPath();
            roundedRect(r.x + 1.0f, fy, r.w - 2.0f, fillH, 1.5f);
            fillPaint(linearGradient(r.x, r.y + r.h, r.x, r.y,
                                     rgba(0x4fe0a0ff), rgba(0xff5a4fff)));
            fill();
        }
    }

    // dB tick scale drawn just right of a vertical level meter (0/-6/-12/-24 dB).
    void drawMeterScale(const Rect &meter)
    {
        struct Tick { const char *label; float amp; };
        static const Tick ticks[] = { { "0", 1.0f }, { "-6", 0.5f }, { "-12", 0.25f }, { "-24", 0.063f } };
        const float tx = meter.x + meter.w + 2.0f;
        useUiFont();
        uiFontSize(6.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for(const auto &t : ticks)
        {
            const float yn = clampf(t.amp / 1.2f, 0.0f, 1.0f);
            const float ty = meter.y + meter.h - 1.0f - yn * (meter.h - 2.0f);
            strokeLine(meter.x + meter.w, ty, tx + 1.0f, ty, DesignTokens::divider(), 1.0f);
            fillColor(DesignTokens::textSecondary());
            text(tx + 3.0f, ty, t.label, nullptr);
        }
    }

    void drawStripThumbnail(const Rect &r, const synth::SourceTrackParams &track, int trackIndex, bool selected)
    {
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, DesignTokens::controlRadius);
        fillColor(DesignTokens::controlBackground());
        fill();
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, DesignTokens::controlRadius);
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();

        const Rect plot { r.x + 5.0f, r.y + 5.0f, r.w - 10.0f, r.h - 10.0f };
        const Color line = selected ? DesignTokens::accentGreen() : DesignTokens::accentCyan();
        scissor(plot.x, plot.y, plot.w, plot.h);

        if(track.type == synth::SourceTrackType::MetaOscillator && track.metaOsc.frameCount > 0)
        {
            float liveMorph = track.metaOsc.morph;
            if(const auto *p = plugin()) liveMorph = p->sourceLiveMorph(trackIndex);
            const int fIdx = clampi(int(liveMorph * float(track.metaOsc.frameCount - 1) + 0.5f), 0,
                                    track.metaOsc.frameCount - 1);
            const auto &frm = track.metaOsc.frames[(size_t)fIdx];
            const float midY = plot.y + plot.h * 0.5f;
            beginPath();
            for(int sp = 0; sp < int(plot.w); ++sp)
            {
                const float t = float(sp) / std::max(1.0f, plot.w);
                const float v = sampleFrameWarped(frm, t, track.metaOsc.warpMode, track.metaOsc.warpAmount);
                const float px = plot.x + float(sp);
                const float py = midY - v * plot.h * 0.42f;
                if(sp == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line);
            strokeWidth(1.25f);
            stroke();
        }
        else if(track.type == synth::SourceTrackType::PartialBank)
        {
            const int count = std::max(1, track.partialBank.partialCount);
            const float barW = std::max(1.0f, plot.w / float(count));
            for(int i = 0; i < count; ++i)
            {
                const auto &p = track.partialBank.partials[(size_t)i];
                const float h = clampf(p.amp, 0.0f, 1.0f) * plot.h;
                beginPath();
                roundedRect(plot.x + float(i) * barW, plot.y + plot.h - h, std::max(1.0f, barW - 1.0f), h, 1.0f);
                fillColor(line.withAlpha(0.78f));
                fill();
            }
        }
        else
        {
            const float midY = plot.y + plot.h * 0.5f;
            beginPath();
            for(int sp = 0; sp < int(plot.w); ++sp)
            {
                const float t = float(sp) / std::max(1.0f, plot.w);
                const float v = track.type == synth::SourceTrackType::BasicOscillator
                                  ? std::sin(t * kPi * 4.0f)
                                  : 0.55f * std::sin(t * kPi * 41.0f) * std::sin(t * kPi * 7.0f);
                const float px = plot.x + float(sp);
                const float py = midY - v * plot.h * 0.38f;
                if(sp == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line.withAlpha(track.type == synth::SourceTrackType::BasicOscillator ? 0.85f : 0.55f));
            strokeWidth(1.25f);
            stroke();
        }
        resetScissor();
    }

    void drawModulationMatrixPreview(const Rect &r)
    {
        drawPlotBackground(r, 6, 5);
        static constexpr synth::ModSource sources[] = {
            synth::ModSource::Lfo1, synth::ModSource::Lfo2, synth::ModSource::Env1,
            synth::ModSource::Env2, synth::ModSource::Adsr, synth::ModSource::KeyTrack
        };
        static constexpr synth::ModDestination dests[] = {
            synth::ModDestination::Amp, synth::ModDestination::Freq, synth::ModDestination::MetaMorph,
            synth::ModDestination::MetaWarp, synth::ModDestination::TrackPan
        };
        constexpr int sourceCount = int(sizeof(sources) / sizeof(sources[0]));
        constexpr int destCount = int(sizeof(dests) / sizeof(dests[0]));
        const float labelW = 52.0f;
        const float headH = 14.0f;
        const float cellW = (r.w - 12.0f - labelW) / float(destCount);
        const float cellH = (r.h - 14.0f - headH) / float(sourceCount);

        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary());
        for(int d = 0; d < destCount; ++d)
            text(r.x + 6.0f + labelW + cellW * (float(d) + 0.5f), r.y + 8.0f, destName(dests[d]), nullptr);

        for(int s = 0; s < sourceCount; ++s)
        {
            const float cy = r.y + headH + 7.0f + cellH * (float(s) + 0.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(r.x + 8.0f, cy, sourceName(sources[s]), nullptr);
            for(int d = 0; d < destCount; ++d)
            {
                const Rect cell { r.x + 6.0f + labelW + cellW * float(d), r.y + headH + 7.0f + cellH * float(s),
                                  cellW, cellH };
                beginPath();
                rect(cell.x, cell.y, cell.w, cell.h);
                strokeColor(DesignTokens::divider().withAlpha(0.55f));
                strokeWidth(1.0f);
                stroke();
                for(const auto &rule : rules_)
                {
                    if(!rule.enabled || rule.source != sources[s] || rule.dest != dests[d])
                        continue;
                    const float amount = clampf(std::abs(rule.depth) / modulationDepthLimit(rule.dest), 0.0f, 1.0f);
                    const float radius = 4.0f + 5.0f * amount;
                    beginPath();
                    circle(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, radius);
                    fillColor(DesignTokens::accentBlue().withAlpha(0.28f));
                    fill();
                    strokeColor(DesignTokens::accentCyan());
                    strokeWidth(1.0f);
                    stroke();
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f", double(rule.depth));
                    uiFontSize(7.0f);
                    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                    fillColor(DesignTokens::textPrimary());
                    text(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, buf, nullptr);
                    break;
                }
            }
        }
    }

    void drawStripRack(const Rect &r)
    {
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        drawSectionTitle(r.x + 14.0f, r.y + 12.0f, "STRIPS");
        fontSize(8.0f); fillColor(rgba(0x4a6070ff));
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        text(r.x + r.w - 14.0f, r.y + 14.0f, "Shift+click=multi-select", nullptr);

        // ---- Add OSC button (delete via keyboard Delete/Backspace) ----
        addTrackRect_    = { r.x + 14.0f, r.y + 38.0f, r.w - 28.0f, 24.0f };
        removeTrackRect_ = {};
        drawButton(addTrackRect_, addTrackMenuOpen_ ? "Choose OSC Type" : "+ Add OSC Strip", addTrackMenuOpen_);
        const char *oscLabels[4] = { "Partial Bank", "Meta Oscillator", "Basic Oscillator", "Sample / Noise" };
        for(int i = 0; i < 4; ++i)
        {
            addTrackTypeRects_[(size_t)i] = addTrackMenuOpen_
                ? Rect{ r.x + 14.0f, r.y + 66.0f + float(i) * 24.0f, r.w - 28.0f, 20.0f }
                : Rect{};
            if(addTrackMenuOpen_) drawButton(addTrackTypeRects_[(size_t)i], oscLabels[i], false);
        }

        const float startY   = r.y + (addTrackMenuOpen_ ? 168.0f : 68.0f);
        constexpr float kScrollH = 14.0f;
        const float stripH   = r.y + r.h - startY - kScrollH - 8.0f;

        // ---- Compute visible columns (regular tracks + group buses) ----
        const int totalTracks = int(generator_.tracks.size());
        const int totalGroups = int(stripGroups_.size());
        const int totalCols   = totalTracks + totalGroups;
        constexpr float kMinStripW = 90.0f;
        const float gap  = 6.0f;
        const int maxVis = std::max(1, int((r.w - 28.0f + gap) / (kMinStripW + gap)));
        const int totalScroll = std::max(0, totalCols - maxVis);
        stripScrollF_ = clampf(stripScrollF_, 0.0f, float(totalScroll));
        // Fixed column width sized to the visible count → scrolling pans smoothly
        // (fractional offset) instead of jumping a whole column at a time.
        const int shownCols = std::max(1, std::min(totalCols, maxVis));
        const float w = (r.w - 28.0f - gap * float(shownCols - 1)) / float(shownCols);
        const float viewX = r.x + 14.0f;
        const float viewW = r.w - 28.0f;
        const int firstCol = int(std::floor(stripScrollF_));
        const float fracOff = stripScrollF_ - float(firstCol);
        // Map a global column index to its on-screen x (may be off-viewport).
        const auto colX = [&](int globalIdx) {
            return viewX + (float(globalIdx - firstCol) - fracOff) * (w + gap);
        };

        // ---- Clear hit rects ----
        trackGainRect_ = {}; trackPanRect_ = {}; trackSendRect_ = {};
        stripEnvRect_ = {}; stripDupRect_ = {};
        stripRouteRects_.fill({}); stripRouteRuleIndices_.fill(-1);
        for(auto &rr : stripRects_) rr = {};
        for(auto &rr : stripGroupBusRects_) rr = {};
        for(auto &rr : stripGainRects_) rr = {};
        for(auto &rr : stripPanRects_) rr = {};
        for(auto &rr : stripSendRects_) rr = {};
        for(auto &rr : stripMuteRects_) rr = {};
        for(auto &rr : stripSoloRects_) rr = {};
        insertHits_.clear();
        modHits_.clear();

        // ---- Draw group brackets (behind strips, for member ranges) ----
        for(int gi = 0; gi < totalGroups; ++gi)
        {
            const auto &grp = stripGroups_[(size_t)gi];
            if(grp.memberIndices.empty()) continue;
            float firstX = 1.0e9f;
            float lastX = -1.0e9f;
            for(int mi : grp.memberIndices)
            {
                const float sx = colX(mi);
                if(sx + w >= viewX && sx <= viewX + viewW)
                {
                    firstX = std::min(firstX, sx);
                    lastX = std::max(lastX, sx);
                }
            }
            if(lastX < -1.0e8f || firstX > 1.0e8f) continue;
            const float bx = firstX - 2.0f;
            const float bw = (lastX - firstX) + w + 4.0f;
            beginPath();
            rect(bx, startY - 4.0f, bw, stripH + 8.0f);
            fillColor(rgba(0xc070e01aU));
            fill();
            beginPath();
            rect(bx, startY - 4.0f, bw, stripH + 8.0f);
            strokeColor(rgba(0xc070e066U));
            strokeWidth(1.5f);
            stroke();
        }

        // ---- Draw strips ----
        const int drawLast = std::min(totalCols, firstCol + shownCols + 2);
        for(int globalIdx = firstCol; globalIdx < drawLast; ++globalIdx)
        {
            const bool isGroup  = globalIdx >= totalTracks;
            const float sx = colX(globalIdx);
            if(sx + w < viewX || sx > viewX + viewW)
                continue;
            const Rect  s  { sx, startY, w, stripH };

            if(isGroup)
            {
                // ---- GROUP BUS STRIP ----
                const int gi = globalIdx - totalTracks;
                if(gi >= totalGroups) continue;
                auto &grp = stripGroups_[(size_t)gi];
                drawPanel(s, rgba(0x14101cff), rgba(0xc070e0ccU));
                stripGroupBusRects_[(size_t)gi] = s;
                fontSize(8.5f); fillColor(rgba(0xd088f8ffU));
                textAlign(ALIGN_CENTER | ALIGN_TOP);
                text(s.x + s.w * 0.5f, s.y + 6.0f, grp.name.c_str(), nullptr);
                // Line under title
                beginPath();
                moveTo(s.x + 4.0f, s.y + 20.0f);
                lineTo(s.x + s.w - 4.0f, s.y + 20.0f);
                strokeColor(rgba(0xc070e080U)); strokeWidth(0.8f); stroke();
                // Group route inserts (filter / distortion chain)
                fontSize(7.0f); fillColor(rgba(0xc070e0bbU));
                textAlign(ALIGN_LEFT | ALIGN_TOP);
                text(s.x + 4.0f, s.y + 24.0f, "ROUTE", nullptr);
                drawInsertColumn({ s.x + 4.0f, s.y + 34.0f, s.w - 8.0f, 76.0f }, grp.inserts, -1, gi);
                // Combined member mod routes
                int rrow = 0;
                const float routeBaseY = s.y + 116.0f;
                for(int mi : grp.memberIndices)
                {
                    if(mi < 0 || mi >= totalTracks) continue;
                    const auto &track = generator_.tracks[(size_t)mi];
                    for(int ri = 0; ri < synth::kMaxMatrixRules && rrow < 4; ++ri)
                    {
                        const auto &rule = rules_[(size_t)ri];
                        if(!rule.enabled || rule.targetTrackId != track.id) continue;
                        std::snprintf(scratch_, sizeof(scratch_), "[%s] %s>%s %+.1f",
                                      track.name.c_str(), sourceName(rule.source),
                                      destName(rule.dest), double(rule.depth));
                        drawLabelBox({ s.x + 4.0f, routeBaseY + float(rrow) * 20.0f, s.w - 8.0f, 18.0f }, scratch_);
                        ++rrow;
                    }
                }
                if(rrow == 0)
                    drawLabelBox({ s.x + 4.0f, routeBaseY, s.w - 8.0f, 18.0f }, "no mod");
                // Member list at bottom
                char mlist[64] {};
                int moff = 0;
                for(int mi : grp.memberIndices)
                    if(mi >= 0 && mi < totalTracks)
                        moff += std::snprintf(mlist + moff, sizeof(mlist) - moff, "%s ", generator_.tracks[(size_t)mi].name.c_str());
                fontSize(7.0f); fillColor(rgba(0xc070e099U));
                textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
                text(s.x + s.w * 0.5f, s.y + s.h - 4.0f, mlist, nullptr);
                continue;
            }

            // ---- REGULAR OSC STRIP ----
            auto &track = generator_.tracks[(size_t)globalIdx];
            stripRects_[(size_t)globalIdx] = s;
            const bool isPrimary  = selectedTrack_ == globalIdx;
            const bool isMultiSel = selectedStrips_[(size_t)globalIdx];

            // Find group membership
            int grpIdx = -1;
            for(int gi = 0; gi < totalGroups; ++gi)
                for(int mi : stripGroups_[(size_t)gi].memberIndices)
                    if(mi == globalIdx) { grpIdx = gi; break; }

            drawPanel(s, DesignTokens::panelRaised(), DesignTokens::border());
            if(isPrimary || isMultiSel)
            {
                beginPath();
                roundedRect(s.x + 0.5f, s.y + 0.5f, s.w - 1.0f, s.h - 1.0f, DesignTokens::panelRadius);
                strokeColor(isPrimary ? DesignTokens::accentGreen() : DesignTokens::accentBlue());
                strokeWidth(1.0f);
                stroke();
            }

            char number[16] {};
            std::snprintf(number, sizeof(number), "%02d", globalIdx + 1);
            useUiFont();
            uiFontSize(9.0f);
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            fillColor(isPrimary ? DesignTokens::accentGreen() : DesignTokens::accentCyan());
            text(s.x + 8.0f, s.y + 8.0f, number, nullptr);
            fillColor(isPrimary ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
            // Clip the name so a long track name can't run under the M/S buttons.
            scissor(s.x + 28.0f, s.y + 4.0f, std::max(10.0f, s.w - 28.0f - 50.0f), 18.0f);
            text(s.x + 28.0f, s.y + 8.0f, track.name.c_str(), nullptr);
            resetScissor();

            const Rect muteR { s.x + s.w - 43.0f, s.y + 6.0f, 16.0f, 16.0f };
            const Rect soloR { s.x + s.w - 23.0f, s.y + 6.0f, 16.0f, 16.0f };
            stripMuteRects_[(size_t)globalIdx] = muteR;
            stripSoloRects_[(size_t)globalIdx] = soloR;
            drawButton(muteR, "M", track.mute);
            drawButton(soloR, "S", track.solo);

            const int modCount = activeModCountForTrack(track);
            const int fxCount = int(track.inserts.size());
            const int matrixCount = matrixRouteCountForTrack(track);

            // Group tag (top-right, left of the M/S buttons)
            if(grpIdx >= 0)
            {
                uiFontSize(7.0f); fillColor(DesignTokens::accentBlue());
                textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
                text(s.x + s.w - 50.0f, s.y + 14.0f, stripGroups_[(size_t)grpIdx].name.c_str(), nullptr);
            }

            // ===== Horizontal channel: wide-and-short strip along the bottom row =====
            //  LEFT  zone : waveform thumbnail, info line, GAIN fader + meter, Pan, Send
            //  RIGHT zone : ROUTE / MOD / MATRIX / ADSR / UNI stacked compactly
            const float contentY = s.y + 28.0f;
            const float leftW = s.w * 0.44f;

            const Rect thumb { s.x + 8.0f, contentY, leftW - 16.0f, 46.0f };
            drawStripThumbnail(thumb, track, globalIdx, isPrimary);
            useUiFont();
            uiFontSize(8.0f);
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary());
            std::snprintf(scratch_, sizeof(scratch_), "%d Mods  %d FX  Mtx %d", modCount, fxCount, matrixCount);
            text(s.x + 8.0f, contentY + 50.0f, scratch_, nullptr);

            float level = 0.0f;
            if(const auto *p = plugin()) level = p->sourceLiveLevel(globalIdx);
            const float rowY = contentY + 66.0f;
            const float rowH = std::max(46.0f, (s.y + s.h - 8.0f) - rowY);
            const Rect faderR { s.x + 8.0f, rowY, 22.0f, rowH };
            drawFader(faderR, track.gain * 0.5f, "GAIN", track.gain, isPrimary);
            const Rect meterR { s.x + 32.0f, rowY, 8.0f, rowH - 14.0f };
            drawLevelMeter(meterR, level);
            const float pkw = 40.0f;
            const Rect panR  { s.x + 46.0f,         rowY + 2.0f, pkw, 40.0f };
            const Rect sendR { s.x + 46.0f + pkw,   rowY + 2.0f, pkw, 40.0f };
            drawKnob(panR,  "Pan",  (track.pan + 1.0f) * 0.5f, track.pan);
            drawKnob(sendR, "Send", track.send, track.send);
            stripGainRects_[(size_t)globalIdx] = faderR;
            stripPanRects_[(size_t)globalIdx]  = panR;
            stripSendRects_[(size_t)globalIdx] = sendR;
            if(isPrimary) { trackGainRect_ = faderR; trackPanRect_ = panR; trackSendRect_ = sendR; }

            // ---- RIGHT zone ----
            const float rx = s.x + leftW + 6.0f;
            const float rw = (s.x + s.w - 8.0f) - rx;
            const float kStripRow = 16.0f;
            float ry = contentY;
            uiFontSize(7.0f); fillColor(DesignTokens::textSecondary()); textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(rx, ry, "ROUTE", nullptr);
            const float insH = float(std::min(fxCount + 1, 3)) * kStripRow;
            drawInsertColumn({ rx, ry + 10.0f, rw, insH }, track.inserts, int(track.id), -1);
            ry += 10.0f + insH + 6.0f;

            fontSize(7.0f); fillColor(rgba(0x6a8090ff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(rx, ry, "MOD", nullptr);
            const float modRegH = float(std::min(modCount + 1, 3)) * kStripRow;
            drawModColumn({ rx, ry + 10.0f, rw, modRegH }, track, int(track.id));
            ry += 10.0f + modRegH + 6.0f;

            fontSize(7.0f); fillColor(rgba(0x6a8090ff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(rx, ry, "MATRIX", nullptr);
            int mrow = 0;
            for(int ri = 0; ri < synth::kMaxMatrixRules && mrow < 1; ++ri)
            {
                const auto &rule = rules_[(size_t)ri];
                if(!rule.enabled || rule.targetTrackId != track.id)
                    continue;
                const Rect rr { rx, ry + 10.0f, rw, 16.0f };
                const bool isEff = synth::insertModParamForDest(rule.dest) >= 0;
                if(isEff)
                    std::snprintf(scratch_, sizeof(scratch_), "%s>%s%d %+.1f",
                                  sourceName(rule.source), destName(rule.dest), rule.targetSlot + 1, double(rule.depth));
                else
                    std::snprintf(scratch_, sizeof(scratch_), "%s>%s %+.1f",
                                  sourceName(rule.source), destName(rule.dest), double(rule.depth));
                drawLabelBox(rr, scratch_);
                if(isPrimary)
                {
                    stripRouteRects_[(size_t)mrow] = rr;
                    stripRouteRuleIndices_[(size_t)mrow] = ri;
                }
                ++mrow;
            }
            if(mrow == 0)
                drawLabelBox({ rx, ry + 10.0f, rw, 16.0f }, "no matrix");
            ry += 10.0f + 16.0f + 8.0f;

            const Rect envR { rx, ry, rw * 0.58f, 18.0f };
            const Rect dupR { envR.x + envR.w + 4.0f, ry, rw * 0.42f - 4.0f, 18.0f };
            drawButton(envR, buttonText("ADSR%d", clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) + 1),
                       selectedAmpEnv_ == clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1));
            drawButton(dupR, "Dup", false);
            if(isPrimary) { stripEnvRect_ = envR; stripDupRect_ = dupR; }
            ry += 22.0f;
            drawLabelBox({ rx, ry, rw, 18.0f }, buttonText("UNI×%d", clampi(track.unison.voices, 1, 16)));
        }

        // ---- Horizontal scrollbar ----
        const float sbY = startY + stripH + 4.0f;
        stripScrollbarRect_ = { r.x + 14.0f, sbY, r.w - 28.0f, kScrollH };
        drawPanel(stripScrollbarRect_, rgba(0x0d141aff), rgba(0x2a3840ff));
        if(totalCols > maxVis && totalScroll > 0)
        {
            const float thumbW = std::max(16.0f, stripScrollbarRect_.w * float(maxVis) / float(totalCols));
            const float thumbX = stripScrollbarRect_.x
                               + (stripScrollbarRect_.w - thumbW) * stripScrollF_ / float(totalScroll);
            beginPath();
            rect(thumbX, sbY + 2.0f, thumbW, kScrollH - 4.0f);
            fillColor(rgba(0x5080a0ccU));
            fill();
        }
    }

    void drawPartialBankLayerPreview(const Rect &r, synth::WavetableSeedParams &seed)
    {
        drawPlotBackground(r, 8, 4);
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        const int morphFrame = partialBankMorphFrameIndex(seed);
        constexpr int kMaxShow = 18;
        const int stride = std::max(1, seed.frameCount / kMaxShow);
        std::vector<int> frames;
        frames.reserve(kMaxShow + 1);
        for(int f = 0; f < seed.frameCount; f += stride)
            frames.push_back(f);
        if(std::find(frames.begin(), frames.end(), morphFrame) == frames.end())
            frames.push_back(morphFrame);
        std::sort(frames.begin(), frames.end());

        const float plotX = r.x + 12.0f;
        const float plotW = r.w - 24.0f;
        const float baseY = r.y + r.h - 16.0f;
        const float depthY = std::min(r.h * 0.38f, float(frames.size()) * 5.0f);
        const float barW = plotW / float(synth::kMaxWavetablePartials);
        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        for(size_t di = 0; di < frames.size(); ++di)
        {
            const int frameIndex = frames[di];
            ensurePartialBankFrameDefaults(seed, frameIndex);
            const auto &frame = seed.frames[(size_t)frameIndex];
            const float depthT = frames.size() > 1 ? float(di) / float(frames.size() - 1) : 1.0f;
            const float y0 = baseY - depthY * (1.0f - depthT);
            const float height = (r.h - depthY - 28.0f) * (0.50f + depthT * 0.50f);
            const float alpha = frameIndex == morphFrame ? 0.95f : 0.18f + 0.35f * depthT;
            const Color col = frameIndex == morphFrame
                                  ? DesignTokens::accentGreen()
                                  : DesignTokens::accentCyan().withAlpha(alpha);
            strokeLine(plotX, y0, plotX + plotW, y0, DesignTokens::divider().withAlpha(0.35f), 0.6f);
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                const float amp = clampf(frame.harmonics[(size_t)i].amp, 0.0f, 1.0f);
                if(amp <= 0.001f)
                    continue;
                const float x = plotX + float(i) * barW;
                const float bh = amp * height;
                beginPath();
                rect(x + 0.5f, y0 - bh, std::max(1.0f, barW - 1.0f), bh);
                fillColor(col);
                fill();
            }
            if(frameIndex == morphFrame)
            {
                uiFontSize(9.0f);
                fillColor(DesignTokens::accentGreen());
                textAlign(ALIGN_LEFT | ALIGN_TOP);
                text(r.x + 8.0f, y0 - height - 10.0f, buttonText("F%d", frameIndex + 1), nullptr);
            }
        }
        resetScissor();

        uiFontSize(10.0f);
        fillColor(rgba(0x7f9aabff));
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(r.x + 8.0f, r.y + 6.0f, "PARTIAL TABLE PREVIEW  amp/phase loaded per frame", nullptr);
    }

    void drawPartialBankTrackEditor(const Rect &r, synth::SourceTrackParams &track)
    {
        auto &seed = track.partialBank;
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
        ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
        for(auto &rect : partialKnobRects_)
            rect = {};

        metaWavetableNameRect_ = { r.x, r.y, std::max(120.0f, r.w - 66.0f), 28.0f };
        metaWavetablePrevRect_ = { r.x + r.w - 60.0f, r.y, 28.0f, 28.0f };
        metaWavetableNextRect_ = { r.x + r.w - 28.0f, r.y, 28.0f, 28.0f };
        const char *tableTitle = wavetablePresetLabel_.empty() ? "Select Table" : wavetablePresetLabel_.c_str();
        drawDropdown(metaWavetableNameRect_, tableTitle, wavetablePresetMenuOpen_);
        drawButton(metaWavetablePrevRect_, "<", false);
        drawButton(metaWavetableNextRect_, ">", false);

        const float topY = r.y + 40.0f;
        const float knobW = std::min(86.0f, std::max(58.0f, (r.w - 170.0f) / 4.0f));
        partialCountRect_ = { r.x, topY, knobW, 54.0f };
        inharmonicModeRect_ = { r.x + knobW + 8.0f, topY + 10.0f, 104.0f, 30.0f };
        inharmonicRect_ = { inharmonicModeRect_.x + inharmonicModeRect_.w + 8.0f, topY, knobW, 54.0f };
        metaFrameCountRect_ = {};
        metaMorphRect_ = { inharmonicRect_.x + knobW + 8.0f, topY, knobW, 54.0f };
        metaHarmonicEditRect_ = { metaMorphRect_.x + knobW + 10.0f, topY + 12.0f,
                                  std::max(76.0f, r.x + r.w - (metaMorphRect_.x + knobW + 10.0f)), 30.0f };
        drawKnob(partialCountRect_, "Partials", float(seed.partialCount - 1) / 63.0f, float(seed.partialCount));
        drawButton(inharmonicModeRect_, freqShapeName(seed.freqShape), seed.freqShape != synth::FreqShape::Harmonic);
        drawKnob(inharmonicRect_, "Harmonize", seed.inharmonicAmount, seed.inharmonicAmount);
        drawKnob(metaMorphRect_, "Morph", seed.morph, seed.morph);
        drawButton(metaHarmonicEditRect_, "Edit Table", harmonicEditorOpen_);

        metaFrameStripRect_ = {};
        metaFrameScrollRect_ = {};
        // Pin the pitch row to the panel bottom and let the spectrum grow to fill
        // all the space above it, so the panel has no dead zone at large heights.
        const float pitchRowH = 30.0f;
        const float editY = r.y + r.h - pitchRowH;
        const float spectrumTop = topY + 66.0f;
        const float spectrumH = std::max(120.0f, editY - 12.0f - spectrumTop);
        const Rect spectrum { r.x, spectrumTop, r.w, spectrumH };
        partialSpectrumRect_ = spectrum;
        drawPartialBankLayerPreview(spectrum, seed);

        selectedPartialIndex_ = clampi(selectedPartialIndex_, 0, std::max(0, seed.partialCount - 1));
        partialAmpRect_ = {};
        partialRatioRect_ = {};
        const auto &groupPitch = seed.partials[0];
        const float pitchX = r.x;
        const float pitchGap = 6.0f;
        const float pitchW = std::min(96.0f, std::max(64.0f, (r.w - pitchGap * 3.0f) * 0.25f));
        metaOctRect_ = { pitchX, editY, pitchW, 30.0f };
        metaSemRect_ = { metaOctRect_.x + pitchW + pitchGap, editY, pitchW, 30.0f };
        metaFinRect_ = { metaSemRect_.x + pitchW + pitchGap, editY, pitchW, 30.0f };
        metaCrsRect_ = { metaFinRect_.x + pitchW + pitchGap, editY, pitchW, 30.0f };
        drawPitchControl(metaOctRect_, "OCT", groupPitch.pitchOct, false);
        drawPitchControl(metaSemRect_, "SEM", groupPitch.pitchSem, false);
        drawPitchControl(metaFinRect_, "FIN", int(std::round(groupPitch.pitchFin)), false);
        drawPitchControl(metaCrsRect_, "CRS", int(std::round(groupPitch.pitchCrs)), false);
    }

    void drawMetaTrackEditor(const Rect &r, synth::SourceTrackParams &track)
    {
        auto &slot = track.metaOsc;
        metaEnableRect_ = {};  // ON/OFF button removed
        metaWavetablePrevRect_ = { r.x + r.w - 60.0f, r.y, 28.0f, 28.0f };
        metaWavetableNextRect_ = { r.x + r.w - 28.0f, r.y, 28.0f, 28.0f };
        metaWavetableNameRect_ = { r.x, r.y, std::max(80.0f, r.w - 66.0f), 28.0f };
        const char *wavetableTitle = wavetablePresetLabel_.empty() ? "Select Wavetable" : wavetablePresetLabel_.c_str();
        drawDropdown(metaWavetableNameRect_, wavetableTitle, wavetablePresetMenuOpen_);
        drawButton(metaWavetablePrevRect_, "<", false);
        drawButton(metaWavetableNextRect_, ">", false);

        const float pitchY = r.y + 32.0f;
        const float pitchGap = 6.0f;
        const float pitchW = (r.w - pitchGap * 3.0f) * 0.25f;
        metaOctRect_ = { r.x, pitchY, pitchW, 26.0f };
        metaSemRect_ = { metaOctRect_.x + pitchW + pitchGap, pitchY, pitchW, 26.0f };
        metaFinRect_ = { metaSemRect_.x + pitchW + pitchGap, pitchY, pitchW, 26.0f };
        metaCrsRect_ = { metaFinRect_.x + pitchW + pitchGap, pitchY, pitchW, 26.0f };
        drawPitchControl(metaOctRect_, "OCT", slot.pitchOct, false);
        drawPitchControl(metaSemRect_, "SEM", slot.pitchSem, false);
        drawPitchControl(metaFinRect_, "FIN", int(slot.pitchFin), false);
        drawPitchControlF(metaCrsRect_, "CRS", slot.pitchCrs);

        // Waveform display on the left; Warp mode + Morph/Warp/Phase/Pan as a
        // vertical knob column on the right (reference-style).
        const float colW = 116.0f;
        const float waveformTop = r.y + 62.0f;
        const float waveformBottom = r.y + r.h;
        const float waveformW = std::max(120.0f, r.w - colW - 12.0f);
        metaWaveformRect_ = { r.x, waveformTop, waveformW, std::max(60.0f, waveformBottom - waveformTop) };
        drawMeta3DWaveform(metaWaveformRect_, slot, selectedTrack_);

        const float cx = r.x + waveformW + 12.0f;
        float ky = waveformTop;
        metaFrameCountRect_ = {};  // Frames 控件从主界面移除
        metaWarpModeRect_   = { cx, ky, colW, 26.0f }; ky += 32.0f;
        metaMorphRect_      = { cx, ky, colW, 38.0f }; ky += 42.0f;
        metaWarpAmountRect_ = { cx, ky, colW, 38.0f }; ky += 42.0f;
        metaPhaseRect_      = { cx, ky, colW, 38.0f }; ky += 42.0f;
        metaPanRect_        = { cx, ky, colW, 38.0f };
        drawButton(metaWarpModeRect_, warpModeName(slot.warpMode), false);
        drawKnob(metaMorphRect_, "Morph", slot.morph, slot.morph);
        drawKnob(metaWarpAmountRect_, "Warp", (slot.warpAmount + 1.0f) * 0.5f, slot.warpAmount);
        drawKnob(metaPhaseRect_, "Phase", (slot.phase + kPi) / (2.0f * kPi), slot.phase);
        drawKnob(metaPanRect_, "Pan", (slot.pan + 1.0f) * 0.5f, slot.pan);

        metaLoadRect_ = {};
        for(auto &presetRect : metaFramePresetRects_)
            presetRect = {};
        metaHarmonicEditRect_ = {};
    }

    void drawBasicTrackEditor(const Rect &r, synth::SourceTrackParams &track)
    {
        basicShapeRect_ = { r.x, r.y, 180.0f, 26.0f };
        basicPulseRect_ = { r.x, r.y + 36.0f, 260.0f, 24.0f };
        basicSubRect_ = { r.x, r.y + 66.0f, 260.0f, 24.0f };
        drawButton(basicShapeRect_, synth::basicOscillatorShapeName(track.basicShape), false);
        drawSlider(basicPulseRect_, "Pulse Width", track.pulseWidth, track.pulseWidth);
        drawSlider(basicSubRect_, "Sub Level", track.subLevel, track.subLevel);
        // Large waveform preview fills the rest of the panel (no dead space).
        const float previewTop = r.y + 104.0f;
        const float previewH = (r.y + r.h) - previewTop;
        if(previewH > 70.0f)
        {
            drawSectionTitle(r.x, previewTop - 18.0f, "Waveform");
            drawStripThumbnail({ r.x, previewTop, r.w, previewH }, track, selectedTrack_, false);
        }
    }

    void drawNoiseTrackEditor(const Rect &r, synth::SourceTrackParams &track)
    {
        noiseModeRect_ = { r.x, r.y, 180.0f, 26.0f };
        noiseColorRect_ = { r.x, r.y + 36.0f, 260.0f, 24.0f };
        drawButton(noiseModeRect_, synth::sampleNoiseModeName(track.sampleNoiseMode), false);
        drawSlider(noiseColorRect_, "Noise Color", track.noiseColor, track.noiseColor);
        drawLabelBox({ r.x, r.y + 70.0f, 260.0f, 24.0f }, "File/Capture unavailable in v1");
        // Large waveform preview fills the rest of the panel (no dead space).
        const float previewTop = r.y + 122.0f;
        const float previewH = (r.y + r.h) - previewTop;
        if(previewH > 70.0f)
        {
            drawSectionTitle(r.x, previewTop - 18.0f, "Signal");
            drawStripThumbnail({ r.x, previewTop, r.w, previewH }, track, selectedTrack_, false);
        }
    }

    int envUseCount(int envIndex) const
    {
        int count = 0;
        envIndex = clampi(envIndex, 0, synth::kMaxAmpEnvs - 1);
        for(const auto &track : generator_.tracks)
            if(clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) == envIndex)
                ++count;
        return count;
    }

    void drawTrackEnvelope(const Rect &r, synth::SourceTrackParams &track)
    {
        track.ampEnvIndex = clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
        selectedAmpEnv_ = track.ampEnvIndex;
        auto &ampEnv = ampEnvs_[(size_t)selectedAmpEnv_];
        drawSectionTitle(r.x, r.y, "Amp ADSR");
        drawLabelBox({ r.x, r.y + 28.0f, 150.0f, 24.0f }, buttonText("ADSR ENV %d", track.ampEnvIndex + 1));
        drawLabelBox({ r.x + 160.0f, r.y + 28.0f, 110.0f, 24.0f },
                     buttonText("Used by %d", envUseCount(track.ampEnvIndex)));
        const float col = (r.w - 18.0f) * 0.25f;
        attackRect_ = { r.x, r.y + 60.0f, col, 22.0f };
        decayRect_ = { r.x + col + 6.0f, r.y + 60.0f, col, 22.0f };
        sustainRect_ = { r.x + (col + 6.0f) * 2.0f, r.y + 60.0f, col, 22.0f };
        releaseRect_ = { r.x + (col + 6.0f) * 3.0f, r.y + 60.0f, col, 22.0f };
        drawSlider(attackRect_, "A", ampEnv.attack / 5.0f, ampEnv.attack);
        drawSlider(decayRect_, "D", ampEnv.decay / 5.0f, ampEnv.decay);
        drawSlider(sustainRect_, "S", ampEnv.sustain, ampEnv.sustain);
        drawSlider(releaseRect_, "R", ampEnv.release / 8.0f, ampEnv.release);
        drawAdsrCurve({ r.x, r.y + 88.0f, r.w, std::max(36.0f, r.h - 88.0f) }, ampEnv);
    }

    void drawGeneratorDashboard(const Rect &r)
    {
        drawPanel(r, rgba(0x0d151aff), rgba(0x3b5560ff));
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, "Generator");
        drawSourceTopology({ r.x + 16.0f, r.y + 40.0f, r.w - 32.0f, 76.0f });
        const bool wide = r.w >= 760.0f;
        const float leftW = wide ? clampf(r.w * 0.30f, 300.0f, 390.0f) : r.w - 32.0f;
        const float rightX = wide ? r.x + leftW + 30.0f : r.x + 16.0f;
        const float rightW = wide ? r.x + r.w - rightX - 16.0f : r.w - 32.0f;
        const float w = leftW;
        const float controlY = r.y + 126.0f;
        partialCountRect_ = { r.x + 16.0f, controlY, w, 22.0f };
        inharmonicModeRect_ = { r.x + 16.0f, controlY + 27.0f, w * 0.46f, 22.0f };
        inharmonicRect_ = { r.x + 24.0f + w * 0.46f, controlY + 27.0f, w * 0.54f - 8.0f, 22.0f };
        gainRect_ = { r.x + 16.0f, controlY + 54.0f, w, 22.0f };
        drawSlider(partialCountRect_, "Partials", float(generator_.wavetableSeed.partialCount - 1) / 63.0f,
                   float(generator_.wavetableSeed.partialCount));
        drawButton(inharmonicModeRect_, freqShapeName(generator_.wavetableSeed.freqShape), generator_.wavetableSeed.freqShape != synth::FreqShape::Harmonic);
        drawSlider(inharmonicRect_, "Inharmonic", generator_.wavetableSeed.inharmonicAmount, generator_.wavetableSeed.inharmonicAmount);
        drawSlider(gainRect_, "Output Gain", gain_, gain_);

        drawSectionTitle(r.x + 16.0f, controlY + 86.0f, "ADSR");
        const float colW = (w - 8.0f) * 0.5f;
        attackRect_ = { r.x + 16.0f, controlY + 110.0f, colW, 21.0f };
        decayRect_ = { r.x + 24.0f + colW, controlY + 110.0f, colW, 21.0f };
        sustainRect_ = { r.x + 16.0f, controlY + 135.0f, colW, 21.0f };
        releaseRect_ = { r.x + 24.0f + colW, controlY + 135.0f, colW, 21.0f };
        curveRect_ = { r.x + 16.0f, controlY + 160.0f, w, 21.0f };
        drawSlider(attackRect_, "Attack", adsr_.attack / 5.0f, adsr_.attack);
        drawSlider(decayRect_, "Decay", adsr_.decay / 5.0f, adsr_.decay);
        drawSlider(sustainRect_, "Sustain", adsr_.sustain, adsr_.sustain);
        drawSlider(releaseRect_, "Release", adsr_.release / 8.0f, adsr_.release);
        drawSlider(curveRect_, "Curve", adsr_.curve, adsr_.curve);
        drawSectionTitle(r.x + 16.0f, controlY + 188.0f, "Unison");
        unisonVoicesRect_ = { r.x + 16.0f, controlY + 212.0f, colW, 21.0f };
        unisonDetuneRect_ = { r.x + 24.0f + colW, controlY + 212.0f, colW, 21.0f };
        unisonWidthRect_ = { r.x + 16.0f, controlY + 237.0f, colW, 21.0f };
        unisonPhaseRect_ = { r.x + 24.0f + colW, controlY + 237.0f, colW, 21.0f };
        drawSlider(unisonVoicesRect_, "Voices", float(generator_.unison.voices - 1) / 15.0f, float(generator_.unison.voices));
        drawSlider(unisonDetuneRect_, "Detune", generator_.unison.detuneCents / 80.0f, generator_.unison.detuneCents);
        drawSlider(unisonWidthRect_, "Width", generator_.unison.widthStereo, generator_.unison.widthStereo);
        drawSlider(unisonPhaseRect_, "Phase Rand", generator_.unison.phaseSpread, generator_.unison.phaseSpread);
        drawSourceFilterControls({ r.x + 16.0f, controlY + 264.0f, w, std::max(72.0f, r.y + r.h - controlY - 278.0f) });
        if(wide)
            drawMetaPartialEditor({ rightX, r.y + 126.0f, rightW, r.h - 144.0f });
        else
            drawMetaPartialEditor({ r.x + 16.0f, controlY + 350.0f, w, std::max(250.0f, r.h - controlY - 366.0f) });
    }

    void drawSourceTopology(const Rect &r)
    {
        generator_.sourceCount = synth::sanitizeGeneratorSourceCount(generator_.sourceCount);
        selectedSource_ = clampi(selectedSource_, 0, generator_.sourceCount - 1);
        const int options[4] = { 1, 2, 4, 8 };
        const float buttonW = 42.0f;
        for(int i = 0; i < 4; ++i)
        {
            sourceCountRects_[(size_t)i] = { r.x + float(i) * (buttonW + 6.0f), r.y, buttonW, 22.0f };
            drawButton(sourceCountRects_[(size_t)i], buttonText("%d", options[i]), generator_.sourceCount == options[i]);
        }

        const int cols = std::min(4, generator_.sourceCount);
        const int rows = (generator_.sourceCount + cols - 1) / cols;
        const float gap = 6.0f;
        const float cellW = (r.w - gap * float(cols - 1)) / float(cols);
        const float cellH = 22.0f;
        const int metas = synth::metaPartialsPerGeneratorSource(generator_.sourceCount);
        const int partials = synth::partialsPerGeneratorSource(generator_.sourceCount);
        for(auto &rect : sourceChainRects_)
            rect = {};
        for(int i = 0; i < generator_.sourceCount; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;
            sourceChainRects_[(size_t)i] = { r.x + float(col) * (cellW + gap), r.y + 30.0f + float(row) * (cellH + 6.0f),
                                             cellW, cellH };
            const auto &filter = generator_.sources[(size_t)i].filter;
            const char *filterName = filter.enabled ? synth::sourceFilterTopologyName(filter.topology) : "Bypass";
            char label[96] {};
            std::snprintf(label, sizeof(label), "S%d  %dMP+%dS -> %s -> Mix", i + 1, metas, partials - metas,
                          filterName);
            drawButton(sourceChainRects_[(size_t)i], label, selectedSource_ == i);
        }
        (void)rows;
    }

    void drawSourceFilterControls(const Rect &r)
    {
        drawSectionTitle(r.x, r.y, buttonText("Source %d Filter", selectedSource_ + 1));
        auto &source = generator_.sources[(size_t)selectedSource_];
        auto &filter = source.filter;
        const float colW = (r.w - 8.0f) * 0.5f;
        sourceGainRect_ = { r.x, r.y + 24.0f, colW, 20.0f };
        sourcePanRect_ = { r.x + colW + 8.0f, r.y + 24.0f, colW, 20.0f };
        sourceFilterEnableRect_ = { r.x, r.y + 48.0f, colW, 20.0f };
        sourceFilterTopologyRect_ = { r.x + colW + 8.0f, r.y + 48.0f, colW, 20.0f };
        sourceFilterCutoffRect_ = { r.x, r.y + 72.0f, colW, 20.0f };
        sourceFilterResRect_ = { r.x + colW + 8.0f, r.y + 72.0f, colW, 20.0f };
        sourceFilterDriveRect_ = { r.x, r.y + 96.0f, colW, 20.0f };
        sourceFilterFeedbackRect_ = { r.x + colW + 8.0f, r.y + 96.0f, colW, 20.0f };
        sourceFilterMixRect_ = { r.x, r.y + 120.0f, r.w, 20.0f };
        drawSlider(sourceGainRect_, "Src Gain", source.gain * 0.5f, source.gain);
        drawSlider(sourcePanRect_, "Src Pan", (source.pan + 1.0f) * 0.5f, source.pan);
        drawButton(sourceFilterEnableRect_, filter.enabled ? "Filter On" : "Filter Off", filter.enabled);
        drawButton(sourceFilterTopologyRect_, synth::sourceFilterTopologyName(filter.topology), false);
        drawSlider(sourceFilterCutoffRect_, "Src Cut", cutoffToNorm(filter.cutoffHz), filter.cutoffHz);
        drawSlider(sourceFilterResRect_, "Src Res", filter.resonance, filter.resonance);
        drawSlider(sourceFilterDriveRect_, "Src Drive", filter.drive / 8.0f, filter.drive);
        drawSlider(sourceFilterFeedbackRect_, "Feedback", filter.feedback, filter.feedback);
        drawSlider(sourceFilterMixRect_, "Filter Mix", filter.mix, filter.mix);
    }

    void drawOperatorDashboard(const Rect &r)
    {
        drawPanel(r, rgba(0x11181dff), rgba(0x4f6a71ff));
        drawSectionTitle(r.x + 14.0f, r.y + 12.0f, "Operator");
        ensureDefaultOperatorChain();
        for(int i = 0; i < 5; ++i)
        {
            opSelectRects_[(size_t)i] = { r.x + 14.0f + i * 56.0f, r.y + 40.0f, 48.0f, 27.0f };
            drawButton(opSelectRects_[(size_t)i], buttonText("OP%d", i + 1), selectedOp_ == i);
        }

        auto &op = operatorChain_.ops[(size_t)selectedOp_];
        opEnableRect_ = { r.x + 14.0f, r.y + 80.0f, 76.0f, 27.0f };
        opTypeRect_ = { r.x + 100.0f, r.y + 80.0f, r.w - 114.0f, 27.0f };
        drawButton(opEnableRect_, op.enabled ? "Enabled" : "Bypass", op.enabled);
        drawButton(opTypeRect_, synth::operatorTypeName(op.type), false);

        opParamARect_ = { r.x + 14.0f, r.y + 122.0f, r.w - 28.0f, 27.0f };
        opParamBRect_ = { r.x + 14.0f, r.y + 158.0f, r.w - 28.0f, 27.0f };
        opParamCRect_ = { r.x + 14.0f, r.y + 194.0f, r.w - 28.0f, 27.0f };
        opParamDRect_ = { r.x + 14.0f, r.y + 230.0f, r.w - 28.0f, 27.0f };

        switch(op.type)
        {
            case synth::OperatorType::PartialMask:
                drawSlider(opParamARect_, "Mask Lo", float(op.maskLow) / float(synth::kMaxPartials), float(op.maskLow));
                drawSlider(opParamBRect_, "Mask Hi", float(op.maskHigh) / float(synth::kMaxPartials), float(op.maskHigh));
                drawButton(opParamCRect_, op.maskGroupLow ? "Low Group On" : "Low Group Off", op.maskGroupLow);
                drawButton(opParamDRect_, op.maskGroupMid ? "Mid Group On" : "Mid Group Off", op.maskGroupMid);
                break;
            case synth::OperatorType::AmpScalePerGroup:
                drawSlider(opParamARect_, "Low Gain", op.gainLow / 2.0f, op.gainLow);
                drawSlider(opParamBRect_, "Mid Gain", op.gainMid / 2.0f, op.gainMid);
                drawSlider(opParamCRect_, "High Gain", op.gainHigh / 2.0f, op.gainHigh);
                drawLabelBox(opParamDRect_, "Group amplitude scaling");
                break;
            case synth::OperatorType::FrequencyJitter:
                drawSlider(opParamARect_, "Jitter", op.jitterAmount, op.jitterAmount);
                drawSlider(opParamBRect_, "Seed", float(op.jitterSeed % 1000u) / 999.0f, float(op.jitterSeed % 1000u));
                drawLabelBox(opParamCRect_, "Deterministic +/-5% ratio scatter");
                drawLabelBox(opParamDRect_, "Applied before voice render");
                break;
            case synth::OperatorType::SpectralTilt:
                drawSlider(opParamARect_, "Tilt", (op.extraTilt + 2.0f) / 4.0f, op.extraTilt);
                drawLabelBox(opParamBRect_, "Positive darkens high partials");
                drawLabelBox(opParamCRect_, "Negative lifts high partials");
                drawLabelBox(opParamDRect_, "Renormalizes after tilt");
                break;
            case synth::OperatorType::HarmonicLock:
                drawSlider(opParamARect_, "Lock", op.lockAmount, op.lockAmount);
                drawLabelBox(opParamBRect_, "Pulls ratios toward integer harmonics");
                drawLabelBox(opParamCRect_, "Useful for de-inharmonic morphs");
                drawLabelBox(opParamDRect_, "Relative-ratio frames only");
                break;
        }
    }

    void drawToneFxDashboard(const Rect &r)
    {
        drawPanel(r, rgba(0x10171bff), rgba(0x485f6bff));
        drawSectionTitle(r.x + 14.0f, r.y + 12.0f, "Tone FX");
        const float w = r.w - 28.0f;
        eqEnableRect_ = { r.x + 14.0f, r.y + 42.0f, 86.0f, 26.0f };
        eqModeRect_ = { r.x + 108.0f, r.y + 42.0f, 100.0f, 26.0f };
        filterEnableRect_ = { r.x + 216.0f, r.y + 42.0f, 86.0f, 26.0f };
        filterTypeRect_ = { r.x + 14.0f, r.y + 78.0f, 100.0f, 26.0f };
        filterModeRect_ = { r.x + 122.0f, r.y + 78.0f, 100.0f, 26.0f };
        drawButton(eqEnableRect_, effects_.eq.enabled ? "EQ On" : "EQ Off", effects_.eq.enabled);
        drawButton(eqModeRect_, synth::effectModeName(effects_.eq.mode), false);
        drawButton(filterEnableRect_, effects_.filter.enabled ? "Filter On" : "Filter Off", effects_.filter.enabled);
        drawButton(filterTypeRect_, synth::filterTypeName(effects_.filter.type), false);
        drawButton(filterModeRect_, synth::effectModeName(effects_.filter.mode), false);

        eqLowRect_ = { r.x + 14.0f, r.y + 118.0f, w, 25.0f };
        eqMidRect_ = { r.x + 14.0f, r.y + 149.0f, w, 25.0f };
        eqHighRect_ = { r.x + 14.0f, r.y + 180.0f, w, 25.0f };
        eqDriveRect_ = { r.x + 14.0f, r.y + 211.0f, w, 25.0f };
        filterCutoffRect_ = { r.x + 14.0f, r.y + 248.0f, w, 25.0f };
        filterResRect_ = { r.x + 14.0f, r.y + 279.0f, w, 25.0f };
        filterDriveRect_ = { r.x + 14.0f, r.y + 310.0f, w, 25.0f };
        drawSlider(eqLowRect_, "EQ Low", (effects_.eq.lowGainDb + 24.0f) / 48.0f, effects_.eq.lowGainDb);
        drawSlider(eqMidRect_, "EQ Mid", (effects_.eq.midGainDb + 24.0f) / 48.0f, effects_.eq.midGainDb);
        drawSlider(eqHighRect_, "EQ High", (effects_.eq.highGainDb + 24.0f) / 48.0f, effects_.eq.highGainDb);
        drawSlider(eqDriveRect_, "EQ Drive", effects_.eq.drive / 6.0f, effects_.eq.drive);
        drawSlider(filterCutoffRect_, "Cutoff", cutoffToNorm(effects_.filter.cutoffHz), effects_.filter.cutoffHz);
        drawSlider(filterResRect_, "Resonance", effects_.filter.resonance, effects_.filter.resonance);
        drawSlider(filterDriveRect_, "Filter Drive", effects_.filter.drive / 6.0f, effects_.filter.drive);
    }

    void drawMatrixDashboard(const Rect &r)
    {
        drawPanel(r, rgba(0x0d151aff), rgba(0x4b6972ff));
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, "Matrix");

        static const char *const matTabs[] = { "GRID", "MODULATORS", "AMP ENV" };
        const Rect tabBar { r.x + 16.0f, r.y + 38.0f, std::min(r.w - 32.0f, 360.0f), 22.0f };
        drawTabBar(tabBar, matTabs, 3, matrixTab_, matrixTabRects_.data());

        const Rect body { r.x + 16.0f, r.y + 72.0f, r.w - 32.0f, r.h - 84.0f };

        // Only the active sub-view repopulates its hit rects — clear them all first.
        matrixGridCells_.clear();
        if(matrixTab_ != 0)
        {
            gridSrcLabelRects_.clear();
            gridDestLabelRects_.clear();
            gridAddSrcRect_ = {};
            gridAddDstRect_ = {};
            gridPickerMode_ = 0;
        }
        for(auto &rc : lfoSelectRects_) rc = {};
        for(auto &rc : envSelectRects_) rc = {};
        for(auto &rc : ampEnvTabRects_) rc = {};
        lfoShapeRect_ = {}; lfoFreqRect_ = {}; lfoPhaseRect_ = {}; lfoRhoRect_ = {};
        lfoEnableRect_ = {}; envEnableRect_ = {}; adsrSourceRect_ = {};
        modModeRect_ = {}; modEnvRateRect_ = {};
        envPointARect_ = {}; envPointBRect_ = {}; envCurveARect_ = {}; matrixEnvCurveRect_ = {};
        attackRect_ = {}; decayRect_ = {}; sustainRect_ = {}; releaseRect_ = {};
        ruleEnableRect_ = {}; ruleSourceRect_ = {}; ruleDestRect_ = {}; ruleWeightRect_ = {};
        ruleDepthRect_ = {}; ruleBandLoRect_ = {}; ruleBandHiRect_ = {};
        for(auto &rc : ruleSelectRects_) rc = {};
        chaosEnableRect_ = {}; shapeAxisRect_ = {};
        chaosRateRect_ = {}; chaosAmountRect_ = {};
        shapePhaseRect_ = {}; shapeRhoRect_ = {}; shapeUpRect_ = {}; shapeDownRect_ = {};

        if(matrixTab_ == 1)      drawMatrixModulators(body);
        else if(matrixTab_ == 2) drawMatrixAmpEnv(body);
        else                     drawMatrixGrid(body);
    }

    void drawMatrixModulators(const Rect &r)
    {
        // One unified row of modulator slots: LFO1-4 (looping shapes) then
        // ENV1-4 (point curves). The graph below shows whichever is selected.
        const int nLfo = synth::kMaxLfos;
        const int nEnv = synth::kMaxModEnvs;
        const int nSlots = nLfo + nEnv;
        const float bw = (r.w - float(nSlots - 1) * 4.0f) / float(nSlots);
        selectedMatrixModSlot_ = clampi(selectedMatrixModSlot_, 0, nSlots - 1);
        for(int i = 0; i < nLfo; ++i)
        {
            lfoSelectRects_[(size_t)i] = { r.x + float(i) * (bw + 4.0f), r.y, bw, 24.0f };
            drawButton(lfoSelectRects_[(size_t)i], buttonText("LFO%d", i + 1), selectedMatrixModSlot_ == i);
        }
        for(int i = 0; i < nEnv; ++i)
        {
            envSelectRects_[(size_t)i] = { r.x + float(nLfo + i) * (bw + 4.0f), r.y, bw, 24.0f };
            drawButton(envSelectRects_[(size_t)i], buttonText("ENV%d", i + 1), selectedMatrixModSlot_ == nLfo + i);
        }

        const bool isEnv = selectedMatrixModSlot_ >= nLfo;
        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());

        if(isEnv)
        {
            // ENV slot: per-voice point curve with a Mode toggle (Env / Loop).
            selectedEnv_ = selectedMatrixModSlot_ - nLfo;
            auto &env = envs_[(size_t)selectedEnv_];
            text(r.x, r.y + 34.0f, "double-click = add / remove point   ·   Ctrl-drag = bend   ·   Shift = no snap", nullptr);
            modModeRect_ = { r.x + r.w - 132.0f, r.y + 28.0f, 132.0f, 20.0f };
            drawButton(modModeRect_, env.loop ? "Mode: LOOP" : "Mode: ENV", env.loop);
            const float knobH = env.loop ? 54.0f : 0.0f;
            const float curveBottom = (r.y + r.h) - (env.loop ? knobH + 8.0f : 0.0f);
            matrixEnvCurveRect_ = { r.x, r.y + 52.0f, r.w, std::max(60.0f, curveBottom - (r.y + 52.0f)) };
            drawMatrixEnvCurve(matrixEnvCurveRect_, env.points.data(), env.pointCount, DesignTokens::accentGreen());
            if(env.loop)
            {
                modEnvRateRect_ = { r.x, r.y + r.h - knobH, (r.w - 8.0f) / 2.0f, 48.0f };
                drawKnob(modEnvRateRect_, "Rate", env.loopRateHz / 20.0f, env.loopRateHz);
            }
            else
            {
                modEnvRateRect_ = {};
            }
        }
        else
        {
            modModeRect_ = {};
            modEnvRateRect_ = {};
            text(r.x, r.y + 34.0f, "LFO · global loop   ·   double-click = add / remove point   ·   Ctrl-drag = bend   ·   Shift = no snap", nullptr);
            // LFO slot: looping point curve (custom waveform) + Rate / Phase.
            selectedLfo_ = selectedMatrixModSlot_;
            auto &lfo = lfos_[(size_t)selectedLfo_];
            if(!lfo.usePoints) { lfo.usePoints = true; pushLfoOnly(); }  // adopt point mode in this editor
            const float knobH = 54.0f;
            const float curveBottom = (r.y + r.h) - knobH - 8.0f;
            matrixEnvCurveRect_ = { r.x, r.y + 50.0f, r.w, std::max(60.0f, curveBottom - (r.y + 50.0f)) };
            drawMatrixEnvCurve(matrixEnvCurveRect_, lfo.points.data(), lfo.pointCount, DesignTokens::accentCyan());
            const float lfoKnobW = (r.w - 8.0f) / 2.0f;
            const float lfoKnobY = r.y + r.h - knobH;
            lfoFreqRect_  = { r.x,                    lfoKnobY, lfoKnobW, 48.0f };
            lfoPhaseRect_ = { r.x + lfoKnobW + 8.0f,  lfoKnobY, lfoKnobW, 48.0f };
            lfoRhoRect_   = {};
            lfoShapeRect_ = {};
            drawKnob(lfoFreqRect_,  "Rate",  lfo.frequencyHz / 20.0f, lfo.frequencyHz);
            drawKnob(lfoPhaseRect_, "Phase", lfo.phase0, lfo.phase0);
        }
    }

    void drawMatrixAmpEnv(const Rect &r)
    {
        static constexpr const char *ampEnvTabs[] = { "ENV1", "ENV2", "ENV3", "ENV4" };
        drawTabBar({ r.x, r.y, std::min(r.w, 320.0f), 24.0f }, ampEnvTabs, synth::kMaxAmpEnvs,
                   selectedAmpEnv_, ampEnvTabRects_.data());
        auto &ampEnv = ampEnvs_[(size_t)selectedAmpEnv_];
        const float kw = (r.w - 18.0f) * 0.25f;
        attackRect_  = { r.x,                      r.y + 38.0f, kw, 44.0f };
        decayRect_   = { r.x + kw + 6.0f,          r.y + 38.0f, kw, 44.0f };
        sustainRect_ = { r.x + (kw + 6.0f) * 2.0f, r.y + 38.0f, kw, 44.0f };
        releaseRect_ = { r.x + (kw + 6.0f) * 3.0f, r.y + 38.0f, kw, 44.0f };
        drawKnob(attackRect_,  "A", ampEnv.attack  / 5.0f, ampEnv.attack);
        drawKnob(decayRect_,   "D", ampEnv.decay   / 5.0f, ampEnv.decay);
        drawKnob(sustainRect_, "S", ampEnv.sustain, ampEnv.sustain);
        drawKnob(releaseRect_, "R", ampEnv.release / 8.0f, ampEnv.release);
        drawAdsrCurve({ r.x, r.y + 92.0f, r.w, std::max(60.0f, (r.y + r.h) - (r.y + 92.0f)) }, ampEnv);
    }

    static constexpr synth::ModSource kGridSourcePool[] = {
        synth::ModSource::Lfo1, synth::ModSource::Lfo2, synth::ModSource::Lfo3, synth::ModSource::Lfo4,
        synth::ModSource::Env1, synth::ModSource::Env2, synth::ModSource::Env3, synth::ModSource::Env4,
        synth::ModSource::Velocity, synth::ModSource::KeyTrack, synth::ModSource::Random, synth::ModSource::Chaos,
        synth::ModSource::Adsr1, synth::ModSource::Adsr2, synth::ModSource::Adsr3, synth::ModSource::Adsr4
    };
    static constexpr synth::ModDestination kGridDestPool[] = {
        synth::ModDestination::Amp, synth::ModDestination::Freq, synth::ModDestination::Phase,
        synth::ModDestination::MetaMorph, synth::ModDestination::MetaWarp, synth::ModDestination::MetaPan,
        synth::ModDestination::TrackGain, synth::ModDestination::TrackPan,
        synth::ModDestination::PitchOct, synth::ModDestination::PitchSem, synth::ModDestination::PitchFine
    };

    void drawMatrixGridNode(const Rect &cell, const synth::MatrixRule *rule)
    {
        const float ncx = cell.x + cell.w * 0.5f;
        const float ncy = cell.y + cell.h * 0.5f;
        if(rule == nullptr)
        {
            beginPath();
            circle(ncx, ncy, 1.6f);
            fillColor(DesignTokens::divider());
            fill();
            return;
        }
        const float nr = std::min(cell.w, cell.h) * 0.36f;
        const float amt = clampf(std::abs(rule->depth) / modulationDepthLimit(rule->dest), 0.0f, 1.0f);
        const Color col = rule->depth >= 0.0f ? DesignTokens::accentCyan() : DesignTokens::accentGreen();
        beginPath();
        circle(ncx, ncy, nr);
        fillColor(col.withAlpha(0.16f));
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(2.0f);
        stroke();
        lineCap(ROUND);
        beginPath();
        const float a0 = -kPi * 0.5f;
        arc(ncx, ncy, nr, a0, a0 + 2.0f * kPi * std::max(0.02f, amt), CW);
        strokeColor(col);
        strokeWidth(2.4f);
        stroke();
        lineCap(BUTT);
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%+.1f", double(rule->depth));
        uiFontSize(8.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(ncx, ncy, buf, nullptr);
    }

    // Interactive modulation matrix. Rows/columns are user-chosen: add via the "+"
    // buttons, remove by right-clicking an axis label. A circular amount node sits
    // at each routed cell — click empty to route, drag a node for depth, right-click
    // a node to clear. Routes are scoped to the selected track.
    void drawMatrixGrid(const Rect &r)
    {
        const auto *track = currentTrack();
        const int nS = int(gridSources_.size());
        const int nD = int(gridDests_.size());

        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        text(r.x, r.y, "click = route  ·  drag = depth  ·  right-click node = clear  ·  right-click label = remove axis", nullptr);

        const float labelW = 58.0f;
        const float headH = 16.0f;
        const float gridX = r.x + labelW;
        const float topY = r.y + 16.0f;
        const float gridY = topY + headH;
        const float addW = 26.0f;
        const float availW = (r.x + r.w) - gridX - addW;
        const float availH = (r.y + r.h) - gridY - 22.0f;
        const float cellW = clampf(availW / float(std::max(1, nD)), 32.0f, 110.0f);
        const float cellH = clampf(availH / float(std::max(1, nS)), 24.0f, 46.0f);

        gridSrcLabelRects_.clear();
        gridDestLabelRects_.clear();

        // Destination headers + "+" add-destination button.
        uiFontSize(8.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        for(int d = 0; d < nD; ++d)
        {
            const Rect hr { gridX + cellW * float(d), topY, cellW, headH };
            gridDestLabelRects_.push_back(hr);
            fillColor(DesignTokens::textPrimary());
            text(hr.x + hr.w * 0.5f, hr.y + hr.h * 0.5f, destName(gridDests_[(size_t)d]), nullptr);
        }
        gridAddDstRect_ = { gridX + cellW * float(nD) + 2.0f, topY, addW - 4.0f, headH };
        drawButton(gridAddDstRect_, "+", gridPickerMode_ == 2);

        // Source row labels + "+" add-source button.
        for(int s = 0; s < nS; ++s)
        {
            const Rect lr { r.x, gridY + cellH * float(s), labelW - 4.0f, cellH };
            gridSrcLabelRects_.push_back(lr);
            uiFontSize(8.0f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(lr.x + 2.0f, lr.y + lr.h * 0.5f, sourceName(gridSources_[(size_t)s]), nullptr);
        }
        gridAddSrcRect_ = { r.x, gridY + cellH * float(nS) + 2.0f, labelW - 4.0f, 20.0f };
        drawButton(gridAddSrcRect_, "+ src", gridPickerMode_ == 1);

        // Cells.
        matrixGridCells_.clear();
        for(int s = 0; s < nS; ++s)
            for(int d = 0; d < nD; ++d)
            {
                const Rect cell { gridX + cellW * float(d), gridY + cellH * float(s), cellW, cellH };
                matrixGridCells_.push_back(MatrixCell { cell, gridSources_[(size_t)s], gridDests_[(size_t)d] });
                beginPath();
                rect(cell.x + 1.0f, cell.y + 1.0f, cell.w - 2.0f, cell.h - 2.0f);
                strokeColor(DesignTokens::divider().withAlpha(0.5f));
                strokeWidth(1.0f);
                stroke();
                const synth::MatrixRule *rule = nullptr;
                if(track != nullptr)
                    for(const auto &ru : rules_)
                        if(ru.enabled && ru.source == gridSources_[(size_t)s] && ru.dest == gridDests_[(size_t)d]
                           && ru.targetTrackId == track->id) { rule = &ru; break; }
                drawMatrixGridNode(cell, rule);
            }
    }

    void drawGridAxisPicker()
    {
        gridPickerItemRects_.clear();
        gridPickerPoolIdx_.clear();
        if(gridPickerMode_ == 0)
            return;
        const bool srcMode = gridPickerMode_ == 1;
        const int poolN = srcMode ? int(sizeof(kGridSourcePool) / sizeof(kGridSourcePool[0]))
                                  : int(sizeof(kGridDestPool) / sizeof(kGridDestPool[0]));
        std::vector<int> avail;
        for(int i = 0; i < poolN; ++i)
        {
            const bool taken = srcMode
                ? std::find(gridSources_.begin(), gridSources_.end(), kGridSourcePool[i]) != gridSources_.end()
                : std::find(gridDests_.begin(), gridDests_.end(), kGridDestPool[i]) != gridDests_.end();
            if(!taken)
                avail.push_back(i);
        }
        const float rowH = 20.0f;
        const float w = 140.0f;
        const float h = std::max(rowH, rowH * float(avail.size())) + 8.0f;
        const float px = clampf(gridPickerX_, 4.0f, float(uiW()) - w - 4.0f);
        const float py = clampf(gridPickerY_, 4.0f, float(uiH()) - h - 4.0f);
        drawPanel({ px, py, w, h }, rgba(0x10171df8), rgba(0x5b7380ff));
        useUiFont();
        uiFontSize(11.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for(size_t k = 0; k < avail.size(); ++k)
        {
            const Rect it { px + 4.0f, py + 4.0f + rowH * float(k), w - 8.0f, rowH - 2.0f };
            gridPickerItemRects_.push_back(it);
            gridPickerPoolIdx_.push_back(avail[k]);
            fillColor(DesignTokens::textPrimary());
            const char *nm = srcMode ? sourceName(kGridSourcePool[avail[k]]) : destName(kGridDestPool[avail[k]]);
            text(it.x + 8.0f, it.y + it.h * 0.5f, nm, nullptr);
        }
        if(avail.empty())
        {
            fillColor(DesignTokens::textSecondary());
            text(px + 8.0f, py + 4.0f + rowH * 0.5f, "(all added)", nullptr);
        }
    }

    void enableModSource(synth::ModSource src)
    {
        if(src >= synth::ModSource::Lfo1 && src <= synth::ModSource::Lfo4)
        { selectedLfo_ = int(src) - int(synth::ModSource::Lfo1); lfos_[(size_t)selectedLfo_].enabled = true; }
        else if(src >= synth::ModSource::Env1 && src <= synth::ModSource::Env4)
        { selectedEnv_ = int(src) - int(synth::ModSource::Env1); envs_[(size_t)selectedEnv_].enabled = true; }
    }

    bool handleMatrixGridPress(float x, float y)
    {
        auto *track = currentTrack();
        if(track == nullptr)
            return false;
        for(const auto &c : matrixGridCells_)
        {
            if(!c.rect.contains(x, y))
                continue;
            int idx = -1, freeIdx = -1;
            for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            {
                auto &ru = rules_[(size_t)i];
                if(ru.enabled && ru.source == c.src && ru.dest == c.dst && ru.targetTrackId == track->id)
                { idx = i; break; }
                if(freeIdx < 0 && !ru.enabled)
                    freeIdx = i;
            }
            if(idx < 0)
            {
                if(freeIdx < 0)
                    return true;  // rule pool full
                idx = freeIdx;
                auto &ru = rules_[(size_t)idx];
                ru.enabled = true;
                ru.source = c.src;
                ru.dest = c.dst;
                ru.targetTrackId = track->id;
                ru.targetSlot = 0;
                ru.weight = synth::WeightMode::All;
                ru.depth = defaultModulationDepth(c.dst);
                enableModSource(c.src);
                pushMatrix();
            }
            selectedRule_ = idx;
            dragTarget_ = DragTarget::ModDepth;
            dragStartY_ = y;
            dragStartDepth_ = rules_[(size_t)idx].depth;
            dragDepthLimit_ = modulationDepthLimit(rules_[(size_t)idx].dest);
            return true;
        }
        return false;
    }

    bool handleMatrixGridDelete(float x, float y)
    {
        auto *track = currentTrack();
        if(track == nullptr)
            return false;
        for(const auto &c : matrixGridCells_)
        {
            if(!c.rect.contains(x, y))
                continue;
            for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            {
                auto &ru = rules_[(size_t)i];
                if(ru.enabled && ru.source == c.src && ru.dest == c.dst && ru.targetTrackId == track->id)
                {
                    ru.enabled = false;
                    pushMatrix();
                    break;
                }
            }
            return true;
        }
        return false;
    }

    void drawMetaPartialEditor(const Rect &r)
    {
        drawSectionTitle(r.x, r.y, "MetaPartial");
        const int sourceCount = synth::sanitizeGeneratorSourceCount(generator_.sourceCount);
        const int metas = synth::metaPartialsPerGeneratorSource(sourceCount);
        selectedSource_ = clampi(selectedSource_, 0, sourceCount - 1);
        const int firstMeta = selectedSource_ * metas;
        selectedMetaPartial_ = clampi(selectedMetaPartial_, firstMeta, firstMeta + metas - 1);
        for(auto &rect : metaSelectRects_)
            rect = {};
        const float buttonW = std::max(42.0f, (r.w - float(std::max(0, metas - 1)) * 5.0f) / float(metas));
        for(int local = 0; local < metas; ++local)
        {
            const int i = firstMeta + local;
            metaSelectRects_[(size_t)i] = { r.x + float(local) * (buttonW + 5.0f), r.y + 26.0f, buttonW, 20.0f };
            drawButton(metaSelectRects_[(size_t)i], buttonText("MP%d", i + 1), selectedMetaPartial_ == i);
        }

        auto &slot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
        selectedMetaHarmonic_ = clampi(selectedMetaHarmonic_, 0, synth::kEditableWavetableHarmonics - 1);
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        auto &harmonic = frame.harmonics[(size_t)selectedMetaHarmonic_];

        const float gap = 8.0f;
        const float colW = (r.w - gap) * 0.5f;
        metaEnableRect_ = { r.x, r.y + 54.0f, 70.0f, 20.0f };
        metaWarpModeRect_ = { r.x + 78.0f, r.y + 54.0f, 110.0f, 20.0f };
        metaFrameButtonRect_ = { r.x + 196.0f, r.y + 54.0f, std::max(68.0f, r.w - 196.0f), 20.0f };
        drawButton(metaEnableRect_, slot.enabled ? "On" : "Off", slot.enabled);
        drawButton(metaWarpModeRect_, warpModeName(slot.warpMode), false);
        drawButton(metaFrameButtonRect_, buttonText("Frame %d", selectedMetaFrame_ + 1), false);

        metaLoadRect_ = { r.x, r.y + 82.0f, 44.0f, 20.0f };
        metaSaveRect_ = { r.x + 48.0f, r.y + 82.0f, 44.0f, 20.0f };
        const float presetBase = 96.0f;
        const float presetW = std::min(46.0f, std::max(32.0f, (r.w - presetBase - 84.0f) / 5.0f));
        for(int i = 0; i < 5; ++i)
            metaFramePresetRects_[(size_t)i] = { r.x + presetBase + float(i) * (presetW + 5.0f), r.y + 82.0f, presetW, 20.0f };
        metaLoadPathRect_ = { r.x + presetBase + 5.0f * (presetW + 5.0f), r.y + 82.0f,
                              std::max(60.0f, r.w - presetBase - 5.0f * (presetW + 5.0f)), 20.0f };
        drawButton(metaLoadRect_, "Load", loadPathEditing_);
        drawButton(metaSaveRect_, "Save", false);
        drawButton(metaFramePresetRects_[0], "Sin", false);
        drawButton(metaFramePresetRects_[1], "Saw", false);
        drawButton(metaFramePresetRects_[2], "Sqr", false);
        drawButton(metaFramePresetRects_[3], "Tri", false);
        drawButton(metaFramePresetRects_[4], "Clr", false);
        const std::string pathText = loadPathEditing_ ? ("> " + loadPathBuffer_) : loadStatus_;
        drawLabelBox(metaLoadPathRect_, pathText.empty() ? "type wav path after Load" : pathText.c_str());

        metaFrameStripRect_ = { r.x, r.y + 108.0f, r.w, 22.0f };
        drawMetaFrameStrip(metaFrameStripRect_, slot);

        const float waveformH = clampf(r.h - 230.0f, 68.0f, 128.0f);
        metaWaveformRect_ = { r.x, r.y + 136.0f, r.w, waveformH };
        drawMetaWaveformEditor(metaWaveformRect_, frame);

        const float controlY = metaWaveformRect_.y + metaWaveformRect_.h + 8.0f;
        metaRatioRect_ = { r.x, controlY, colW, 20.0f };
        metaAmpRect_ = { r.x + colW + gap, controlY, colW, 20.0f };
        metaPhaseRect_ = { r.x, controlY + 26.0f, colW, 20.0f };
        metaPanRect_ = { r.x + colW + gap, controlY + 26.0f, colW, 20.0f };
        metaFrameCountRect_ = { r.x, controlY + 52.0f, colW, 20.0f };
        metaMorphRect_ = { r.x + colW + gap, controlY + 52.0f, colW, 20.0f };
        drawSlider(metaRatioRect_, "Ratio", std::log2(std::max(0.01f, slot.ratio)) / 7.0f, slot.ratio);
        drawSlider(metaAmpRect_, "Amp", slot.amp, slot.amp);
        drawSlider(metaPhaseRect_, "Phase", (slot.phase + kPi) / (2.0f * kPi), slot.phase);
        drawSlider(metaPanRect_, "Pan", (slot.pan + 1.0f) * 0.5f, slot.pan);
        drawSlider(metaFrameCountRect_, "Frames", float(slot.frameCount - 1) / float(synth::kMaxWavetableFrames - 1), float(slot.frameCount));
        drawSlider(metaMorphRect_, "Morph", slot.morph, slot.morph);

        metaWarpAmountRect_ = { r.x, controlY + 78.0f, colW, 20.0f };
        drawSlider(metaWarpAmountRect_, "Warp Amt", (slot.warpAmount + 1.0f) * 0.5f, slot.warpAmount);

        const float thirdW = (colW - gap * 2.0f) / 3.0f;
        metaHarmonicEditRect_ = { r.x + colW + gap, controlY + 78.0f, thirdW, 20.0f };
        metaHarmonicRatioRect_ = { metaHarmonicEditRect_.x + thirdW + gap, controlY + 78.0f, thirdW, 20.0f };
        metaHarmonicAmpRect_ = { metaHarmonicRatioRect_.x + thirdW + gap, controlY + 78.0f, thirdW, 20.0f };
        drawButton(metaHarmonicEditRect_, "Edit H", harmonicEditorOpen_);
        drawSlider(metaHarmonicRatioRect_, "H", float(selectedMetaHarmonic_ + 1) / float(synth::kEditableWavetableHarmonics), float(selectedMetaHarmonic_ + 1));
        drawSlider(metaHarmonicAmpRect_, "Amp", harmonic.amp, harmonic.amp);
        metaHarmonicPhaseRect_ = {};
    }

    void drawFrameScrollbar(const Rect &r, const synth::WavetablePartialSlot &slot)
    {
        drawPanel(r, rgba(0x0c1318ff), rgba(0x1e2c34ff));
        if(slot.frameCount <= synth::kVisibleWavetableFrames)
            return;
        const int maxScroll = slot.frameCount - synth::kVisibleWavetableFrames;
        const float visibleFrac = float(synth::kVisibleWavetableFrames) / float(slot.frameCount);
        const float thumbW = std::max(24.0f, (r.w - 8.0f) * visibleFrac);
        const float thumbX = r.x + 4.0f + (r.w - 8.0f - thumbW)
                           * float(metaFrameScrollStart_) / float(maxScroll);
        beginPath();
        roundedRect(thumbX, r.y + 2.0f, thumbW, r.h - 4.0f, 3.0f);
        fillColor(rgba(0x4a6c7cff));
        fill();
    }

    // Shared frame-selection logic (single, or ctrl/shift range) for every frame strip.
    // Is morph being modulated (matrix rule targeting this track's MetaMorph)?
    bool morphIsModulated(const synth::SourceTrackParams &t) const
    {
        for(const auto &r : rules_)
            if(r.enabled && r.targetTrackId == t.id && r.dest == synth::ModDestination::MetaMorph)
                return true;
        return false;
    }

    // Frame click. Ctrl/Shift held: extend a range from the anchor. Plain click:
    // single-select + (if morph isn't modulated) auto-jump morph to that frame.
    void selectMetaFrameAt(int frameIndex, int frameCount)
    {
        if(frameIndex < 0 || frameIndex >= frameCount)
            return;
        if(ctrlDown_ || shiftDown_)
        {
            if(metaFrameRangeAnchor_ < 0 || metaFrameRangeAnchor_ >= frameCount)
            {
                metaFrameRangeAnchor_ = frameIndex;
                metaFrameSelected_.fill(false);
                metaFrameSelected_[(size_t)frameIndex] = true;
                selectedMetaFrame_ = frameIndex;
                return;
            }
            const int a = std::min(metaFrameRangeAnchor_, frameIndex);
            const int b = std::max(metaFrameRangeAnchor_, frameIndex);
            metaFrameSelected_.fill(false);
            for(int i = a; i <= b; ++i)
                metaFrameSelected_[(size_t)i] = true;
            selectedMetaFrame_ = frameIndex;
            return;
        }
        // Plain single select.
        metaFrameSelected_.fill(false);
        metaFrameSelected_[(size_t)frameIndex] = true;
        selectedMetaFrame_ = frameIndex;
        metaFrameRangeAnchor_ = frameIndex;
        if(auto *track = currentTrack();
           track != nullptr && track->type == synth::SourceTrackType::MetaOscillator
           && frameCount >= 2 && !morphIsModulated(*track))
        {
            track->metaOsc.morph = float(frameIndex) / float(frameCount - 1);
            pushCurrentTrack();
        }
        else if(auto *track = currentTrack();
                track != nullptr && track->type == synth::SourceTrackType::PartialBank
                && frameCount >= 2)
        {
            track->partialBank.morph = float(frameIndex) / float(frameCount - 1);
            pushCurrentTrack();
        }
    }

    void selectAllMetaFrames()
    {
        auto *track = currentTrack();
        const int fc = track && track->type == synth::SourceTrackType::PartialBank
                           ? track->partialBank.frameCount
                           : (track ? track->metaOsc.frameCount : 0);
        for(int i = 0; i < synth::kMaxWavetableFrames; ++i)
            metaFrameSelected_[(size_t)i] = i < fc;
        metaFrameRangeAnchor_ = fc > 0 ? 0 : -1;
    }

    void drawMetaFrameStrip(const Rect &r, const synth::WavetablePartialSlot &slot)
    {
        metaFramesShown_ = true; // a clickable frame strip is on screen this frame
        metaFrameScrollStart_ = clampi(metaFrameScrollStart_, 0,
                                       std::max(0, slot.frameCount - synth::kVisibleWavetableFrames));
        metaFramePageStart_ = metaFrameScrollStart_;

        const float cellW = r.w / float(synth::kVisibleWavetableFrames);
        const float waveAreaH = r.h - 15.0f;  // top area for mini waveform
        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            const int frameIndex = metaFrameScrollStart_ + local;
            const Rect btn { r.x + float(local) * cellW + 1.0f, r.y, cellW - 2.0f, r.h };
            metaFrameRects_[(size_t)local] = btn;
            const bool active = frameIndex < slot.frameCount;
            const bool multiSel = active && (size_t)frameIndex < metaFrameSelected_.size() && metaFrameSelected_[(size_t)frameIndex];
            const bool focused = frameIndex == selectedMetaFrame_;
            const bool selected = multiSel || focused;

            // Selection is a real multi-frame set; every selected frame gets the
            // same bright treatment, while the focused edit frame gets an extra cap.
            const uint32_t bgCol  = selected ? 0x263840ff : 0x151d22ff;
            const uint32_t brdCol = selected ? 0x70d77aff : 0x354851ff;
            drawPanel(btn, rgba(bgCol), rgba(brdCol));
            if(selected && active)
            {
                beginPath();
                roundedRect(btn.x + 3.0f, btn.y + 3.0f, btn.w - 6.0f, 3.0f, 1.5f);
                fillColor(rgba(0x8df7aaff));
                fill();
            }

            // Mini waveform preview in upper area
            if(active)
            {
                const auto &frm = slot.frames[(size_t)frameIndex];
                scissor(btn.x + 2.0f, btn.y + 2.0f, btn.w - 4.0f, waveAreaH - 2.0f);
                beginPath();
                const int steps = 28;
                for(int i = 0; i < steps; ++i)
                {
                    const float t = float(i) / float(steps - 1);
                    float v = 0.0f;
                    if(frm.waveform)
                    {
                        const int s = clampi(int(t * float(synth::kWavetableSize - 1)), 0, synth::kWavetableSize - 1);
                        v = (*frm.waveform)[(size_t)s];
                    }
                    else
                    {
                        for(const auto &hm : frm.harmonics)
                            if(hm.amp > 0.0f && hm.ratio > 0.0f)
                                v += hm.amp * std::sin(2.0f * kPi * t * hm.ratio + hm.phase);
                        v = clampf(v, -1.0f, 1.0f);
                    }
                    const float px = btn.x + 3.0f + t * (btn.w - 6.0f);
                    const float py = btn.y + 2.0f + waveAreaH * 0.5f - v * (waveAreaH * 0.42f);
                    if(i == 0) moveTo(px, py); else lineTo(px, py);
                }
                strokeColor(selected ? rgba(0x63d2ffff) : rgba(0x3a7a90cc));
                strokeWidth(1.0f);
                stroke();
                resetScissor();
            }

            // Frame number label at bottom
            useUiFont();
            uiFontSize(11.0f);
            textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
            fillColor(selected ? rgba(0xf4fff4ff) : rgba(0x8aaabcff));
            char numBuf[16];
            std::snprintf(numBuf, sizeof(numBuf), "%03d", frameIndex + 1);
            text(btn.x + btn.w * 0.5f, btn.y + btn.h - 1.0f, numBuf, nullptr);

            if(!active)
            {
                beginPath();
                rect(btn.x, btn.y, btn.w, btn.h);
                fillColor(rgba(0x00000099));
                fill();
            }
        }
    }

    void drawPitchControl(const Rect &r, const char *label, int value, bool)
    {
        drawPanel(r, rgba(0x0d1920ff), rgba(0x243240ff));
        useUiFont();
        uiFontSize(10.0f);
        fillColor(rgba(0x5a8aa8ff));
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 7.0f, r.y + r.h * 0.5f, label, nullptr);

        char buf[16];
        std::snprintf(buf, sizeof(buf), value > 0 ? "+%d" : "%d", value);
        uiFontSize(13.0f);
        fillColor(rgba(0x9be7a1ff));
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        text(r.x + r.w - 7.0f, r.y + r.h * 0.5f, buf, nullptr);
    }

    void drawPitchControlF(const Rect &r, const char *label, float value)
    {
        drawPanel(r, rgba(0x0d1920ff), rgba(0x243240ff));
        useUiFont();
        uiFontSize(10.0f);
        fillColor(rgba(0x5a8aa8ff));
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 7.0f, r.y + r.h * 0.5f, label, nullptr);

        char buf[16];
        std::snprintf(buf, sizeof(buf), value >= 0.0f ? "+%.2f" : "%.2f", value);
        uiFontSize(12.0f);
        fillColor(rgba(0x9be7a1ff));
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        text(r.x + r.w - 7.0f, r.y + r.h * 0.5f, buf, nullptr);
    }

    // ------ Serum 风格 3D 多帧波形预览 ---------------------------
    // 复刻引擎 warpTablePhase 的相位重映射（归一化 0..1），用于波表预览显示 warp/bend。
    static float warpPhase01(synth::WavetableWarpMode mode, float amount, float x)
    {
        if(mode == synth::WavetableWarpMode::None || std::fabs(amount) < 1.0e-5f)
            return x;
        x -= std::floor(x);
        const float centered = x * 2.0f - 1.0f;
        float y = x;
        switch(mode)
        {
            case synth::WavetableWarpMode::Bend:
            {
                const float bend = 1.0f + 3.0f * std::fabs(amount);
                y = amount >= 0.0f ? std::pow(x, bend) : 1.0f - std::pow(1.0f - x, bend);
                break;
            }
            case synth::WavetableWarpMode::Squeeze:
            {
                const float squeeze = 1.0f - 0.85f * std::fabs(amount);
                y = 0.5f + centered * 0.5f * squeeze;
                break;
            }
            case synth::WavetableWarpMode::Skew:
                y = x + amount * x * (1.0f - x) * (x < 0.5f ? 1.0f : -1.0f);
                break;
            case synth::WavetableWarpMode::None:
                break;
        }
        y -= std::floor(y);
        return y;
    }

    static float sampleFrameWarped(const synth::WavetableFrame &frame, float t,
                                   synth::WavetableWarpMode mode, float amount)
    {
        return sampleFrame(frame, warpPhase01(mode, amount, t));
    }

    static float sampleFrame(const synth::WavetableFrame &frame, float t)
    {
        float v = 0.0f;
        if(frame.useImportedWaveform && frame.waveform)
        {
            const float pos = t * float(synth::kWavetableSize - 1);
            const int a = std::max(0, std::min(synth::kWavetableSize - 1, int(pos)));
            const int b = std::min(a + 1, synth::kWavetableSize - 1);
            v = (*frame.waveform)[(size_t)a] * (1.0f - (pos - float(a)))
              + (*frame.waveform)[(size_t)b] * (pos - float(a));
        }
        else
        {
            for(const auto &h : frame.harmonics)
            {
                if(h.amp <= 0.0f || h.ratio <= 0.0f) continue;
                v += h.amp * std::sin(2.0f * kPi * t * h.ratio + h.phase);
            }
            v = clampf(v, -1.0f, 1.0f);
        }
        return v;
    }

    void drawPlotBackground(const Rect &r, int columns = 6, int rows = 4)
    {
        drawPanel(r, DesignTokens::controlBackground(), DesignTokens::border());
        beginPath();
        roundedRect(r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, r.h - 8.0f, DesignTokens::controlRadius);
        fillColor(DesignTokens::appBackground().withAlpha(0.35f));
        fill();
        for(int i = 1; i < columns; ++i)
        {
            const float x = r.x + 6.0f + (r.w - 12.0f) * float(i) / float(columns);
            strokeLine(x, r.y + 6.0f, x, r.y + r.h - 6.0f, DesignTokens::divider().withAlpha(0.65f), 1.0f);
        }
        for(int i = 1; i < rows; ++i)
        {
            const float y = r.y + 6.0f + (r.h - 12.0f) * float(i) / float(rows);
            strokeLine(r.x + 6.0f, y, r.x + r.w - 6.0f, y, DesignTokens::divider().withAlpha(0.65f), 1.0f);
        }
    }

    void drawMeta3DWaveform(const Rect &r, const synth::WavetablePartialSlot &slot, int trackIndex = -1)
    {
        drawPlotBackground(r, 6, 4);
        if(slot.frameCount <= 0) return;

        // 用调制后的实时 morph（来自音频引擎），fallback 为静态 slot.morph
        float liveMorph = slot.morph;
        if(trackIndex >= 0)
        {
            if(const auto *p = plugin())
                liveMorph = p->sourceLiveMorph(trackIndex);
        }
        const int morphFrame = slot.frameCount > 1
            ? clampi(int(liveMorph * float(slot.frameCount - 1) + 0.5f), 0, slot.frameCount - 1)
            : 0;

        // 最多显示 16 帧（step 抽样）
        constexpr int kMaxShow = 16;
        int stride = std::max(1, slot.frameCount / kMaxShow);
        std::vector<int> frames;
        frames.reserve(kMaxShow + 1);
        for(int i = 0; i < slot.frameCount; i += stride)
            frames.push_back(i);
        // 确保 morphFrame 包含在内（不重复添加）
        if(frames.empty() || frames.back() != morphFrame)
        {
            // 若 morphFrame 还不在列表里则插入
            bool found = false;
            for(int f : frames) if(f == morphFrame) { found = true; break; }
            if(!found) frames.push_back(morphFrame);
        }
        // 按帧号排序（保持 back→front 顺序）
        std::sort(frames.begin(), frames.end());

        const int N = int(frames.size());
        if(N == 0) return;

        // 透视深度：帧越靠后y越高（向上偏移）
        const float perspH = std::min(r.h * 0.50f, float(N) * 5.0f);
        const float waveH  = r.h - perspH - 8.0f;
        // front frame(idx N-1) 绘制在 r 底部居中
        const float frontY = r.y + r.h - 8.0f - waveH * 0.5f;

        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);

        const auto lerpColor = [](Color a, Color b, float t) {
            a.red   += (b.red   - a.red)   * t;
            a.green += (b.green - a.green) * t;
            a.blue  += (b.blue  - a.blue)  * t;
            return a;
        };

        constexpr int kPts = 80;
        for(int di = 0; di < N; ++di)
        {
            const int frameIdx = frames[(size_t)di];
            const bool isMorph = (frameIdx == morphFrame);
            const float depthT = float(di) / float(std::max(1, N - 1)); // 0=back,1=front
            const float yCenter = frontY - perspH * (1.0f - depthT);

            // x 范围随深度缩小（透视）
            const float xScale = 0.65f + 0.35f * depthT;
            const float xL = r.x + r.w * (1.0f - xScale) * 0.5f + 4.0f;
            const float xR = r.x + r.w * (1.0f + xScale) * 0.5f - 4.0f;
            const float ampScale = (0.45f + 0.55f * depthT) * waveH * 0.45f;

            // Depth-graded colour: back frames blue, fading to cyan toward the
            // front; the live morph frame stays solid green.
            const Color depthCol = lerpColor(DesignTokens::accentBlue(), DesignTokens::accentCyan(), depthT);
            const Color lineCol = isMorph ? DesignTokens::accentGreen()
                                          : depthCol.withAlpha(0.22f + 0.5f * depthT);
            const float lineW = isMorph ? 2.4f : (0.7f + 0.9f * depthT);

            auto &frame = slot.frames[(size_t)frameIdx];

            // Faint translucent ribbon under every frame builds a layered surface.
            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yCenter - sampleFrameWarped(frame, t, slot.warpMode, slot.warpAmount) * ampScale;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            lineTo(xR, yCenter);
            lineTo(xL, yCenter);
            closePath();
            fillColor((isMorph ? DesignTokens::accentGreen() : depthCol)
                          .withAlpha(isMorph ? 0.10f : 0.03f + 0.05f * depthT));
            fill();

            // Waveform line.
            lineCap(ROUND);
            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yCenter - sampleFrameWarped(frame, t, slot.warpMode, slot.warpAmount) * ampScale;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(lineCol);
            strokeWidth(lineW);
            stroke();
            lineCap(BUTT);

            // 基线（morph 帧用亮色）
            if(isMorph)
            {
                strokeLine(xL, yCenter, xR, yCenter, DesignTokens::accentGreen().withAlpha(0.18f), 0.5f);
                // 帧号标注
                uiFontSize(9.0f);
                fillColor(DesignTokens::accentGreen());
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                char buf[16];
                std::snprintf(buf, sizeof(buf), "F%d", morphFrame + 1);
                text(r.x + 4.0f, yCenter - ampScale * 0.85f, buf, nullptr);
            }
        }
        resetScissor();
    }

    void drawMetaWaveformEditor(const Rect &r, const synth::WavetableFrame &frame)
    {
        drawPlotBackground(r, 8, 4);
        strokeLine(r.x + 6.0f, r.y + r.h * 0.5f, r.x + r.w - 6.0f, r.y + r.h * 0.5f,
                   DesignTokens::divider(), 1.0f);

        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        beginPath();
        for(int i = 0; i < 160; ++i)
        {
            const float t = float(i) / 159.0f;
            float v = 0.0f;
            if(frame.useImportedWaveform && frame.waveform)
            {
                const float pos = t * float(synth::kWavetableSize - 1);
                const int a = clampi(int(pos), 0, synth::kWavetableSize - 1);
                const int b = std::min(a + 1, synth::kWavetableSize - 1);
                const float frac = pos - float(a);
                v = (*frame.waveform)[(size_t)a] + ((*frame.waveform)[(size_t)b] - (*frame.waveform)[(size_t)a]) * frac;
            }
            else
            {
                for(const auto &h : frame.harmonics)
                {
                    if(h.amp <= 0.0f || h.ratio <= 0.0f)
                        continue;
                    v += h.amp * std::sin(2.0f * kPi * t * h.ratio + h.phase);
                }
                v = clampf(v, -1.0f, 1.0f);
            }
            const float px = r.x + 6.0f + t * (r.w - 12.0f);
            const float py = r.y + r.h * 0.5f - v * (r.h * 0.40f);
            if(i == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(DesignTokens::accentCyan());
        strokeWidth(2.0f);
        stroke();
        resetScissor();

        const float barW = (r.w - 12.0f) / float(synth::kEditableWavetableHarmonics);
        for(int h = 0; h < synth::kEditableWavetableHarmonics; ++h)
        {
            const auto &harmonic = frame.harmonics[(size_t)h];
            const float amp = clampf(harmonic.amp, 0.0f, 1.0f);
            const float x = r.x + 6.0f + float(h) * barW + 1.0f;
            const float y = r.y + r.h - 5.0f - amp * (r.h - 14.0f);
            beginPath();
            roundedRect(x, y, std::max(1.0f, barW - 2.0f), r.y + r.h - 5.0f - y, 1.0f);
            fillColor(h == selectedMetaHarmonic_ ? DesignTokens::accentGreen() : DesignTokens::accentBlue().withAlpha(0.62f));
            fill();
        }
    }

    void drawKeyboard()
    {
        keyboardRect_ = { 16.0f, static_cast<float>(uiH()) - 86.0f, static_cast<float>(uiW()) - 32.0f, 70.0f };
        drawPanel(keyboardRect_, DesignTokens::panelRaised(), DesignTokens::border());

        constexpr int first = 36;
        constexpr int keys = 61;
        const float keyW = keyboardRect_.w / float(keys);
        for(int i = 0; i < keys; ++i)
        {
            const int note = first + i;
            const bool black = isBlackKey(note);
            const bool pressed = note == mouseKey_ || computerKeys_[(size_t)note];
            const Rect r { keyboardRect_.x + i * keyW + 1.0f, keyboardRect_.y + 6.0f, keyW - 2.0f,
                           black ? keyboardRect_.h * 0.58f : keyboardRect_.h - 12.0f };
            beginPath();
            roundedRect(r.x, r.y, r.w, r.h, black ? 2.0f : 3.0f);
            fillColor(pressed ? DesignTokens::accentGreen()
                              : (black ? DesignTokens::appBackground() : rgba(0xdde3e5ff)));
            fill();
            beginPath();
            roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, black ? 2.0f : 3.0f);
            strokeColor(black ? DesignTokens::divider() : DesignTokens::border());
            strokeWidth(1.0f);
            stroke();
        }
    }

    void drawAdsrCurve(const Rect &r)
    {
        drawPlotBackground(r, 5, 4);
        const float a = std::max(0.01f, adsr_.attack);
        const float d = std::max(0.01f, adsr_.decay);
        const float rr = std::max(0.01f, adsr_.release);
        const float sum = a + d + rr + 0.5f;
        const float x0 = r.x + 10.0f;
        const float xA = r.x + r.w * (a / sum);
        const float xD = r.x + r.w * ((a + d) / sum);
        const float xS = r.x + r.w * ((a + d + 0.5f) / sum);
        const float xR = r.x + r.w - 10.0f;
        const float y0 = r.y + r.h - 10.0f;
        const float y1 = r.y + 10.0f;
        const float yS = r.y + r.h - 10.0f - adsr_.sustain * (r.h - 20.0f);

        beginPath();
        moveTo(x0, y0);
        lineTo(xA, y1);
        lineTo(xD, yS);
        lineTo(xS, yS);
        lineTo(xR, y0);
        lineTo(x0, y0);
        closePath();
        fillColor(DesignTokens::accentGreen().withAlpha(0.09f));
        fill();
        beginPath();
        moveTo(x0, y0);
        lineTo(xA, y1);
        lineTo(xD, yS);
        lineTo(xS, yS);
        lineTo(xR, y0);
        strokeColor(DesignTokens::accentGreen());
        strokeWidth(2.0f);
        stroke();
    }

    void drawAdsrCurve(const Rect &r, const synth::AdsrParams &env)
    {
        drawPlotBackground(r, 5, 4);
        // Times can be exactly 0 (e.g. instant attack). Segment x positions are
        // mapped into [x0, xR] so a 0-length stage sits exactly at its start edge
        // instead of overshooting backwards.
        const float a = std::max(0.0f, env.attack);
        const float d = std::max(0.0f, env.decay);
        const float rel = std::max(0.0f, env.release);
        const float sustainSpan = 0.35f;  // fixed visual width for the held sustain
        const float sum = std::max(1.0e-3f, a + d + rel + sustainSpan);
        const float x0 = r.x + 10.0f;
        const float xR = r.x + r.w - 10.0f;
        const float plotW = xR - x0;
        const float xA = x0 + plotW * (a / sum);
        const float xD = x0 + plotW * ((a + d) / sum);
        const float xS = x0 + plotW * ((a + d + sustainSpan) / sum);
        const float y0 = r.y + r.h - 8.0f;
        const float y1 = r.y + 8.0f;
        const float yS = y0 - env.sustain * (r.h - 16.0f);

        beginPath();
        moveTo(x0, y0);
        lineTo(xA, y1);
        lineTo(xD, yS);
        lineTo(xS, yS);
        lineTo(xR, y0);
        lineTo(x0, y0);
        closePath();
        fillColor(DesignTokens::accentGreen().withAlpha(0.09f));
        fill();
        beginPath();
        moveTo(x0, y0);
        lineTo(xA, y1);
        lineTo(xD, yS);
        lineTo(xS, yS);
        lineTo(xR, y0);
        strokeColor(DesignTokens::accentGreen());
        strokeWidth(2.0f);
        stroke();

        // Time scale: boundary guide lines + per-stage duration labels (seconds).
        useUiFont();
        uiFontSize(8.0f);
        const Color tcol = DesignTokens::divider().withAlpha(0.65f);
        const auto stageLabel = [&](float xpos, char tag, float secs, bool rightAlign) {
            strokeLine(xpos, r.y + 4.0f, xpos, r.y + r.h - 4.0f, tcol, 1.0f);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%c %.3gs", tag, double(secs));
            textAlign((rightAlign ? ALIGN_RIGHT : ALIGN_LEFT) | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary());
            text(xpos + (rightAlign ? -3.0f : 3.0f), r.y + 4.0f, buf, nullptr);
        };
        stageLabel(xA, 'A', a, false);
        stageLabel(xD, 'D', d, false);
        stageLabel(xR, 'R', rel, true);
    }

    void drawMatrixEnvCurve(const Rect &r, const synth::MatrixEnvPoint *points, int pointCount,
                            Color lineColor = DesignTokens::accentGreen())
    {
        drawPanel(r, DesignTokens::controlBackground(), DesignTokens::border());
        beginPath();
        roundedRect(r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, r.h - 8.0f, DesignTokens::controlRadius);
        fillColor(DesignTokens::appBackground().withAlpha(0.35f));
        fill();
        // Snap grid (16 x 8), aligned to the point coordinate mapping.
        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        const Color gridCol = DesignTokens::divider().withAlpha(0.35f);
        for(int i = 1; i < 16; ++i)
        {
            const float gx = r.x + 8.0f + (float(i) / 16.0f) * (r.w - 16.0f);
            strokeLine(gx, r.y + 5.0f, gx, r.y + r.h - 5.0f, gridCol, 1.0f);
        }
        for(int i = 1; i < 8; ++i)
        {
            const float gy = r.y + r.h - 5.0f - (float(i) / 8.0f) * (r.h - 10.0f);
            strokeLine(r.x + 8.0f, gy, r.x + r.w - 8.0f, gy, gridCol, 1.0f);
        }
        resetScissor();
        beginPath();
        const int count = clampi(pointCount, 2, synth::kMaxMatrixEnvPoints);
        for(int s = 0; s < 80; ++s)
        {
            const float x = float(s) / 79.0f;
            const float yv = synth::pointCurveEval(points, count, x);
            const float px = r.x + 8.0f + x * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - yv * (r.h - 10.0f);
            if(s == 0) moveTo(px, py); else lineTo(px, py);
        }
        lineTo(r.x + r.w - 8.0f, r.y + r.h - 5.0f);
        lineTo(r.x + 8.0f, r.y + r.h - 5.0f);
        closePath();
        fillColor(lineColor.withAlpha(0.08f));
        fill();
        beginPath();
        for(int s = 0; s < 80; ++s)
        {
            const float x = float(s) / 79.0f;
            const float yv = synth::pointCurveEval(points, count, x);
            const float px = r.x + 8.0f + x * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - yv * (r.h - 10.0f);
            if(s == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(lineColor);
        strokeWidth(2.0f);
        stroke();
        for(int i = 0; i < count; ++i)
        {
            const auto &pt = points[(size_t)i];
            const float px = r.x + 8.0f + clampf(pt.x, 0.0f, 1.0f) * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - clampf(pt.y, 0.0f, 1.0f) * (r.h - 10.0f);
            beginPath();
            circle(px, py, i == selectedEnvPoint_ ? 5.0f : 3.5f);
            fillColor(i == selectedEnvPoint_ ? DesignTokens::accentBlue() : DesignTokens::accentCyan());
            fill();
            beginPath();
            circle(px, py, i == selectedEnvPoint_ ? 5.0f : 3.5f);
            strokeColor(DesignTokens::appBackground());
            strokeWidth(1.0f);
            stroke();
        }
    }

    void drawSectionTitle(float x, float y, const char *title)
    {
        beginPath();
        roundedRect(x, y + 2.0f, 2.0f, 14.0f, 1.0f);
        fillColor(DesignTokens::accentCyan());
        fill();
        useUiFont();
        uiFontSize(12.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textPrimary());
        text(x + 8.0f, y, title, nullptr);
    }

    const char *buttonText(const char *fmt, int value)
    {
        std::snprintf(scratch_, sizeof(scratch_), fmt, value);
        return scratch_;
    }

    const char *buttonText(const char *fmt, int a, int b)
    {
        std::snprintf(scratch_, sizeof(scratch_), fmt, a, b);
        return scratch_;
    }

    void drawKnob(const Rect &r, const char *label, float norm, float value)
    {
        const bool tall = r.h >= r.w * 0.65f;
        const float sz   = tall ? std::min(r.w, r.h) : r.h;
        const float rad  = std::max(6.0f, sz * 0.5f - 8.0f);
        const float cx   = tall ? r.x + r.w * 0.5f : r.x + sz * 0.5f;
        const float cy   = tall ? r.y + sz * 0.5f + 2.0f : r.y + r.h * 0.5f;

        // Knob body: soft vertical gradient + rim gives a touch of depth.
        beginPath();
        circle(cx, cy, rad + 4.0f);
        fillPaint(linearGradient(cx, cy - rad - 4.0f, cx, cy + rad + 4.0f,
                                 shade(DesignTokens::controlBackground(), 0.20f),
                                 shade(DesignTokens::controlBackground(), -0.16f)));
        fill();
        strokeColor(DesignTokens::border());
        strokeWidth(DesignTokens::borderWidth);
        stroke();

        const float kStart = DesignTokens::knobStart;
        const float kEnd   = DesignTokens::knobStart + DesignTokens::knobSweep;
        const float kAngle = kStart + clampf(norm, 0.0f, 1.0f) * DesignTokens::knobSweep;

        lineCap(ROUND);
        // Unfilled track.
        beginPath();
        arc(cx, cy, rad, kStart, kEnd, CCW);
        strokeColor(DesignTokens::divider());
        strokeWidth(2.5f);
        stroke();
        // Filled value arc with a faint outer glow.
        if(norm > 0.001f)
        {
            beginPath();
            arc(cx, cy, rad, kStart, kAngle, CCW);
            strokeColor(DesignTokens::accentCyan().withAlpha(0.22f));
            strokeWidth(6.0f);
            stroke();
            beginPath();
            arc(cx, cy, rad, kStart, kAngle, CCW);
            strokeColor(DesignTokens::accentCyan());
            strokeWidth(2.75f);
            stroke();
        }

        // Pointer with a bright tip dot.
        const float ax = std::cos(kAngle);
        const float ay = std::sin(kAngle);
        const float p0 = rad * 0.30f;
        const float p1 = rad * 0.68f;
        beginPath();
        moveTo(cx + ax * p0, cy + ay * p0);
        lineTo(cx + ax * p1, cy + ay * p1);
        strokeColor(DesignTokens::textPrimary());
        strokeWidth(1.75f);
        stroke();
        lineCap(BUTT);
        beginPath();
        circle(cx + ax * p1, cy + ay * p1, 1.7f);
        fillColor(DesignTokens::accentCyan());
        fill();

        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3g", value);
        useUiFont();
        if(tall)
        {
            uiFontSize(10.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary());
            text(cx, r.y + sz + 2.0f, label, nullptr);
            uiFontSize(11.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(DesignTokens::textPrimary());
            text(cx, r.y + sz + 14.0f, buf, nullptr);
        }
        else
        {
            const float tx = r.x + sz + 5.0f;
            uiFontSize(10.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(tx, cy - 5.5f, label, nullptr);
            uiFontSize(12.0f);
            fillColor(DesignTokens::textPrimary());
            text(tx, cy + 6.0f, buf, nullptr);
        }
    }

    void drawSlider(const Rect &r, const char *label, float norm, float value)
    {
        if(r.w < 80.0f || r.h > r.w * 0.55f)
        {
            drawKnob(r, label, norm, value);
            return;
        }

        const float y = r.y + r.h * 0.5f;
        beginPath();
        roundedRect(r.x, y - 1.0f, r.w, 2.0f, 1.0f);
        fillColor(DesignTokens::divider());
        fill();
        beginPath();
        roundedRect(r.x, y - 1.25f, clampf(norm, 0.0f, 1.0f) * r.w, 2.5f, 1.25f);
        fillColor(DesignTokens::accentCyan());
        fill();
        const float hx = r.x + clampf(norm, 0.0f, 1.0f) * r.w;
        beginPath();
        circle(hx, y, 4.0f);
        fillColor(DesignTokens::accentCyan());
        fill();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3g", value);
        useUiFont();
        uiFontSize(10.5f);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textSecondary());
        text(r.x, r.y - 2.0f, label, nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textPrimary());
        text(r.x + r.w, r.y - 2.0f, buf, nullptr);
    }

    void drawLabelBox(const Rect &r, const char *textValue)
    {
        drawPanel(r, DesignTokens::controlBackground(), DesignTokens::border());
        useUiFont();
        uiFontSize(13.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(r.x + 10.0f, r.y + r.h * 0.5f, textValue, nullptr);
    }

    void drawButton(const Rect &r, const char *label, bool active)
    {
        const float radius = std::min(DesignTokens::controlRadius, std::min(r.w, r.h) * 0.45f);
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, radius);
        fillColor(active ? DesignTokens::panelRaised() : DesignTokens::controlBackground());
        fill();
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, radius);
        strokeColor(active ? DesignTokens::accentCyan() : DesignTokens::border());
        strokeWidth(DesignTokens::borderWidth);
        stroke();
        useUiFont();
        uiFontSize(12.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(active ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
        text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, label, nullptr);
    }

    // Wide selector control: left-aligned label + a chevron pinned to the right
    // edge, so a full-width dropdown no longer looks like an empty stretched bar.
    void drawDropdown(const Rect &r, const char *label, bool open)
    {
        const float radius = std::min(DesignTokens::controlRadius, std::min(r.w, r.h) * 0.45f);
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, radius);
        fillColor(open ? DesignTokens::panelRaised() : DesignTokens::controlBackground());
        fill();
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, radius);
        strokeColor(open ? DesignTokens::accentCyan() : DesignTokens::border());
        strokeWidth(DesignTokens::borderWidth);
        stroke();
        useUiFont();
        uiFontSize(12.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(open ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
        scissor(r.x + 10.0f, r.y, std::max(10.0f, r.w - 32.0f), r.h);
        text(r.x + 10.0f, r.y + r.h * 0.5f + 0.5f, label, nullptr);
        resetScissor();
        const float chx = r.x + r.w - 15.0f;
        const float chy = r.y + r.h * 0.5f;
        beginPath();
        moveTo(chx - 4.0f, chy - 2.0f);
        lineTo(chx, chy + 3.0f);
        lineTo(chx + 4.0f, chy - 2.0f);
        strokeColor(open ? DesignTokens::accentCyan() : DesignTokens::textSecondary());
        strokeWidth(1.5f);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);
    }

    void drawPanel(const Rect &r, Color fillValue, Color strokeValue)
    {
        const float radius = std::min(DesignTokens::panelRadius, std::min(r.w, r.h) * 0.45f);
        // Honour the caller's fill/stroke so panels keep a readable depth hierarchy
        // (page < rack < strip < control) and per-context accents (group purple etc).
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, radius);
        fillColor(fillValue);
        fill();
        // 1px brighter line along the top edge for a subtle raised look.
        if(r.h > 8.0f && r.w > radius * 2.0f + 4.0f)
        {
            beginPath();
            moveTo(r.x + radius, r.y + 1.0f);
            lineTo(r.x + r.w - radius, r.y + 1.0f);
            strokeColor(shade(fillValue, 0.16f));
            strokeWidth(1.0f);
            stroke();
        }
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, radius);
        strokeColor(strokeValue);
        strokeWidth(DesignTokens::borderWidth);
        stroke();
    }

    void drawTabBar(const Rect &r, const char *const *labels, int count, int selected, Rect *rects)
    {
        if(count <= 0)
            return;
        const float tabW = r.w / float(count);
        useUiFont();
        uiFontSize(11.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        for(int i = 0; i < count; ++i)
        {
            const Rect tab { r.x + tabW * float(i), r.y, tabW, r.h };
            if(rects != nullptr)
                rects[i] = tab;
            fillColor(i == selected ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
            text(tab.x + tab.w * 0.5f, tab.y + tab.h * 0.45f, labels[i], nullptr);
            if(i == selected)
            {
                beginPath();
                roundedRect(tab.x + 8.0f, tab.y + tab.h - 2.0f, std::max(4.0f, tab.w - 16.0f), 2.0f, 1.0f);
                fillColor(DesignTokens::accentCyan());
                fill();
            }
        }
    }

    void strokeLine(float x1, float y1, float x2, float y2, Color color, float width)
    {
        beginPath();
        moveTo(x1, y1);
        lineTo(x2, y2);
        strokeColor(color);
        strokeWidth(width);
        stroke();
    }

    void appendPresetNameChar(char ch)
    {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
                        || ch == '_' || ch == '-' || ch == ' ';
        if(!ok || presetNameBuffer_.size() >= 64)
            return;
        presetNameBuffer_.push_back(ch);
    }

    static std::string trimPresetName(std::string s)
    {
        while(!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
            s.erase(s.begin());
        while(!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
            s.pop_back();
        return s;
    }

    static std::string safeFileStem(std::string s, const char *fallback)
    {
        s = trimPresetName(std::move(s));
        std::string out;
        for(char ch : s)
        {
            const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
                            || ch == '_' || ch == '-' || ch == ' ';
            if(ok)
                out.push_back(ch);
        }
        return out.empty() ? std::string(fallback) : out;
    }

    void beginSynthPresetRename()
    {
        if(selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
            presetNameBuffer_ = presetNames_[(size_t)selectedPresetIndex_];
        else if(presetNameBuffer_.empty())
            presetNameBuffer_ = "user_kapibara";
        presetNameEditing_ = true;
        presetNameEditTarget_ = PresetNameEditTarget::Synth;
        skipNextPresetCharacterInput_ = false;
    }

    void beginWavetablePresetRename()
    {
        if(selectedWavetablePresetIndex_ >= 0 && selectedWavetablePresetIndex_ < int(wavetablePresets_.size()))
            presetNameBuffer_ = wavetablePresets_[(size_t)selectedWavetablePresetIndex_].name;
        else if(!wavetablePresetLabel_.empty() && wavetablePresetLabel_ != "Select Wavetable")
            presetNameBuffer_ = wavetablePresetLabel_;
        else
            presetNameBuffer_ = "wavetable";
        presetNameEditing_ = true;
        presetNameEditTarget_ = PresetNameEditTarget::Wavetable;
        skipNextPresetCharacterInput_ = false;
    }

    void commitPresetNameEdit()
    {
        const auto target = presetNameEditTarget_;
        presetNameEditing_ = false;
        presetNameEditTarget_ = PresetNameEditTarget::None;
        skipNextPresetCharacterInput_ = false;
        const std::string clean = safeFileStem(presetNameBuffer_, target == PresetNameEditTarget::Wavetable ? "wavetable" : "user_kapibara");
        presetNameBuffer_ = clean;
        if(target == PresetNameEditTarget::Synth)
        {
            releaseAllUiNotes();
            if(auto *p = plugin())
                p->saveUserPreset(clean.c_str());
            pullFromPlugin();
            for(int i = 0; i < int(presetNames_.size()); ++i)
                if(presetNames_[(size_t)i] == clean)
                    selectedPresetIndex_ = i;
            presetLabel_ = clean;
        }
        else if(target == PresetNameEditTarget::Wavetable)
        {
            saveOrRenameWavetablePreset(clean);
        }
    }

    static void ensurePartialBankFrameDefaults(synth::WavetableSeedParams &seed, int frameIndex)
    {
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        frameIndex = clampi(frameIndex, 0, seed.frameCount - 1);
        auto &frame = seed.frames[(size_t)frameIndex];
        for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
        {
            auto &h = frame.harmonics[(size_t)i];
            if(h.ratio <= 0.0f)
                h.ratio = float(i + 1);
            // If the frame has never been written, mirror the legacy partial
            // amp/phase once so old presets still sound the same.
            if(std::abs(h.amp) <= 1.0e-8f && std::abs(h.phase) <= 1.0e-8f && seed.partials[(size_t)i].amp > 0.0f)
            {
                h.amp = seed.partials[(size_t)i].amp;
                h.phase = seed.partials[(size_t)i].phase;
            }
        }
    }

    static int partialBankMorphFrameIndex(const synth::WavetableSeedParams &seed)
    {
        const int fc = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        return fc > 1 ? clampi(int(seed.morph * float(fc - 1) + 0.5f), 0, fc - 1) : 0;
    }

    static float partialBankPitchRatio(int oct, int sem, float fin, float crs)
    {
        const float totalSemis = float(oct) * 12.0f + float(sem) + fin / 100.0f + crs / 100.0f;
        return std::pow(2.0f, totalSemis / 12.0f);
    }

    static void applyPartialBankGroupPitch(synth::WavetableSeedParams &seed,
                                           int oct, int sem, float fin, float crs)
    {
        const float pitchRatio = partialBankPitchRatio(oct, sem, fin, crs);
        for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
        {
            auto &slot = seed.partials[(size_t)i];
            slot.pitchOct = oct;
            slot.pitchSem = sem;
            slot.pitchFin = fin;
            slot.pitchCrs = crs;
            slot.ratio = float(i + 1) * pitchRatio;
        }
    }

    static Kwt2PackedBin packKwtBin(float amp, float phase)
    {
        Kwt2PackedBin bin;
        bin.amplitude = uint16_t(std::round(clampf(amp, 0.0f, 1.0f) * 65535.0f));
        while(phase > kPi) phase -= 2.0f * kPi;
        while(phase < -kPi) phase += 2.0f * kPi;
        bin.phase = int16_t(std::round(clampf(phase / kPi, -1.0f, 1.0f) * 32767.0f));
        return bin;
    }

    static synth::WavetableHarmonic unpackKwtBin(const Kwt2PackedBin &bin, int index)
    {
        synth::WavetableHarmonic h;
        h.ratio = float(index + 1);
        h.amp = float(bin.amplitude) / 65535.0f;
        h.phase = float(bin.phase) / 32767.0f * kPi;
        return h;
    }

    bool handleDoubleClickReset(float x, float y)
    {
        // Strip rack controls
        if(trackGainRect_.contains(x, y))
        {
            if(auto *t = currentTrack()) { t->gain = 1.0f; pushCurrentTrack(); return true; }
        }
        if(trackPanRect_.contains(x, y))
        {
            if(auto *t = currentTrack()) { t->pan = 0.0f; pushCurrentTrack(); return true; }
        }
        if(trackSendRect_.contains(x, y))
        {
            if(auto *t = currentTrack()) { t->send = 0.0f; pushCurrentTrack(); return true; }
        }
        // Meta OCT/SEM/FIN/CRS
        if(metaOctRect_.contains(x, y) || metaSemRect_.contains(x, y)
           || metaFinRect_.contains(x, y) || metaCrsRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
            {
                pushMetaUndoSnapshot();
                t->metaOsc.pitchOct = 0; t->metaOsc.pitchSem = 0;
                t->metaOsc.pitchFin = 0.0f; t->metaOsc.pitchCrs = 0.0f;
                t->metaOsc.syncRatioFromPitch();
                pushCurrentTrack();
                return true;
            }
        }
        // Meta knobs (morph, phase, pan, warpAmount)
        if(metaMorphRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.morph = 0.0f; pushCurrentTrack(); return true; }
        }
        if(metaPhaseRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.phase = 0.0f; pushCurrentTrack(); return true; }
        }
        if(metaPanRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.pan = 0.0f; pushCurrentTrack(); return true; }
        }
        if(metaWarpAmountRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.warpAmount = 0.0f; pushCurrentTrack(); return true; }
        }
        // Legacy meta partial knobs
        if(metaAmpRect_.contains(x, y))
        {
            auto &s = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            s.amp = 1.0f; pushMetaPartialRuntime(); return true;
        }
        if(metaPhaseRect_.contains(x, y))
        {
            auto &s = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            s.phase = 0.0f; pushMetaPartialRuntime(); return true;
        }
        if(metaPanRect_.contains(x, y))
        {
            auto &s = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            s.pan = 0.0f; pushMetaPartialRuntime(); return true;
        }
        // Route-FX insert knobs → reset to a sensible default for that param
        for(const auto &h : fxKnobHits_)
            if(h.rect.w > 0.0f && h.rect.contains(x, y))
            {
                auto *chain = insertChainFor(h.trackId, h.groupIdx);
                if(chain != nullptr && h.insertIdx < int(chain->size()))
                {
                    InsertEffect def; def.kind = (*chain)[(size_t)h.insertIdx].kind;
                    fxKnobSetNorm((*chain)[(size_t)h.insertIdx], h.knob, fxKnobNorm(def, h.knob));
                    commitChainChange(h.trackId, h.groupIdx);
                }
                return true;
            }
        return false;
    }

    bool handleHarmonicEditorClick(float x, float y)
    {
        if(!harmonicEditorOpen_)
            return false;
        auto *activeTrack = currentTrack();
        if(harmonicEditorCloseRect_.contains(x, y))
        {
            harmonicEditorOpen_ = false;
            metaProcessContextMenuOpen_ = false;
            dragTarget_ = DragTarget::None;
            return true;
        }
        if(activeTrack != nullptr && activeTrack->type == synth::SourceTrackType::PartialBank)
        {
            if(metaFrameScrollRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaFrameScroll;
                dragScrollStartX_ = x;
                dragScrollStartVal_ = metaFrameScrollStart_;
                return true;
            }
            if(metaEditorImportRect_.contains(x, y))
            {
                beginWavetableImport();
                return true;
            }
            if(metaEditorAddRect_.contains(x, y)) return performMetaFrameAction(0);
            if(metaEditorDuplicateRect_.contains(x, y)) return performMetaFrameAction(1);
            if(metaEditorDeleteRect_.contains(x, y)) return performMetaFrameAction(2);
            if(metaEditorLeftRect_.contains(x, y)) return performMetaFrameAction(3);
            if(metaEditorRightRect_.contains(x, y)) return performMetaFrameAction(4);
            for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
            {
                if(metaFrameRects_[(size_t)local].contains(x, y))
                {
                    selectMetaFrameAt(metaFramePageStart_ + local, activeTrack->partialBank.frameCount);
                    return true;
                }
            }
            if(harmonicEditorPhaseRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::PartialTablePhase;
                editPartialTable(x, y, true);
                return true;
            }
            if(harmonicEditorSpectrumRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::PartialTableAmp;
                editPartialTable(x, y, false);
                return true;
            }
            if(metaHarmonicRatioRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaHarmonicRatio;
                dragStartY_ = y;
                dragStartNorm_ = float(selectedPartialIndex_) / float(synth::kMaxWavetablePartials - 1);
                return true;
            }
            if(metaHarmonicAmpRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaHarmonicAmp;
                dragStartY_ = y;
                const auto &frame = activeTrack->partialBank.frames[(size_t)selectedMetaFrame_];
                dragStartNorm_ = frame.harmonics[(size_t)selectedPartialIndex_].amp;
                return true;
            }
            if(metaHarmonicPhaseRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaHarmonicPhase;
                dragStartY_ = y;
                const auto &frame = activeTrack->partialBank.frames[(size_t)selectedMetaFrame_];
                dragStartNorm_ = (frame.harmonics[(size_t)selectedPartialIndex_].phase + kPi) / (2.0f * kPi);
                return true;
            }
            return harmonicEditorPanelRect_.contains(x, y);
        }
        if(metaFrameScrollRect_.contains(x, y))
        {
            dragTarget_ = DragTarget::MetaFrameScroll;
            dragScrollStartX_ = x;
            dragScrollStartVal_ = metaFrameScrollStart_;
            return true;
        }
        if(metaEditorImportRect_.contains(x, y))
        {
            beginWavetableImport();
            return true;
        }
        if(metaEditorAddRect_.contains(x, y)) return performMetaFrameAction(0);
        if(metaEditorDuplicateRect_.contains(x, y)) return performMetaFrameAction(1);
        if(metaEditorDeleteRect_.contains(x, y)) return performMetaFrameAction(2);
        if(metaEditorLeftRect_.contains(x, y)) return performMetaFrameAction(3);
        if(metaEditorRightRect_.contains(x, y)) return performMetaFrameAction(4);

        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            if(metaFrameRects_[(size_t)local].contains(x, y))
            {
                if(auto *track = currentTrack(); track != nullptr)
                    selectMetaFrameAt(metaFramePageStart_ + local, track->metaOsc.frameCount);
                return true;
            }
        }
        if(harmonicEditorBarsRect_.contains(x, y))
        {
            prevTimeEditX_ = x;
            prevTimeEditY_ = y;
            dragTarget_ = DragTarget::MetaTimeEditor;
            editMetaDomain(x, y);
            return true;
        }
        if(harmonicEditorSpectrumRect_.contains(x, y))
        {
            dragTarget_ = DragTarget::MetaSpectrumEditor;
            editMetaDomain(x, y);
            return true;
        }
        if(metaHarmonicRatioRect_.contains(x, y))
        {
            pushMetaUndoSnapshot();
            dragTarget_    = DragTarget::MetaHarmonicRatio;
            dragStartY_    = y;
            dragStartNorm_ = float(selectedMetaHarmonic_) / float(synth::kEditableWavetableHarmonics - 1);
            return true;
        }
        if(metaHarmonicAmpRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
            {
                pushMetaUndoSnapshot();
                dragTarget_    = DragTarget::MetaHarmonicAmp;
                dragStartY_    = y;
                dragStartNorm_ = t->metaOsc.frames[(size_t)selectedMetaFrame_]
                                             .harmonics[(size_t)selectedMetaHarmonic_].amp;
            }
            return true;
        }
        if(metaHarmonicPhaseRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
            {
                pushMetaUndoSnapshot();
                const float ph = t->metaOsc.frames[(size_t)selectedMetaFrame_]
                                              .harmonics[(size_t)selectedMetaHarmonic_].phase;
                dragTarget_    = DragTarget::MetaHarmonicPhase;
                dragStartY_    = y;
                dragStartNorm_ = (ph + kPi) / (2.0f * kPi);
            }
            return true;
        }
        return harmonicEditorPanelRect_.contains(x, y);
    }

    bool handleToolbarClick(float x, float y)
    {
        if(presetSelectRect_.contains(x, y))
        {
            presetMenuOpen_ = !presetMenuOpen_;
            optionsMenuOpen_ = false;
            return true;
        }
        if(presetPrevRect_.contains(x, y) || presetNextRect_.contains(x, y))
        {
            if(!presetNames_.empty())
            {
                if(selectedPresetIndex_ < 0)
                    selectedPresetIndex_ = 0;
                else if(presetPrevRect_.contains(x, y))
                    selectedPresetIndex_ = (selectedPresetIndex_ + int(presetNames_.size()) - 1) % int(presetNames_.size());
                else
                    selectedPresetIndex_ = (selectedPresetIndex_ + 1) % int(presetNames_.size());
                presetNameBuffer_ = presetNames_[(size_t)selectedPresetIndex_];
                presetLabel_ = presetNameBuffer_;
            }
            return true;
        }
        if(presetMenuOpen_)
        {
            if(presetSearchRect_.contains(x, y))
            {
                beginSynthPresetRename();
                return true;
            }
            if(presetMenuNewRect_.contains(x, y))
            {
                selectedPresetIndex_ = -1;
                presetNameBuffer_.clear();
                presetNameEditing_ = true;
                presetNameEditTarget_ = PresetNameEditTarget::Synth;
                skipNextPresetCharacterInput_ = false;
                return true;
            }
            const int visible = std::min<int>(int(presetRowRects_.size()), int(presetNames_.size()));
            const int first = selectedPresetIndex_ >= 0
                                  ? clampi(selectedPresetIndex_ - visible / 2, 0, std::max(0, int(presetNames_.size()) - visible))
                                  : 0;
            for(int i = 0; i < int(presetRowRects_.size()); ++i)
            {
                const int presetIndex = first + i;
                if(presetRowRects_[(size_t)i].contains(x, y) && presetIndex < int(presetNames_.size()))
                {
                    selectedPresetIndex_ = presetIndex;
                    presetNameBuffer_ = presetNames_[(size_t)presetIndex];
                    presetLabel_ = presetNameBuffer_;
                    presetNameEditing_ = false;
                    presetNameEditTarget_ = PresetNameEditTarget::None;
                    skipNextPresetCharacterInput_ = false;
                    if(currentClickIsDouble_)
                    {
                        releaseAllUiNotes();
                        if(auto *p = plugin())
                            p->loadUserPreset(presetNameBuffer_.c_str());
                        pullFromPlugin();
                        presetMenuOpen_ = false;
                    }
                    return true;
                }
            }
            if(presetMenuSaveRect_.contains(x, y))
            {
                if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Synth)
                    commitPresetNameEdit();
                else
                    beginSynthPresetRename();
                return true;
            }
            if(presetMenuDeleteRect_.contains(x, y))
            {
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                skipNextPresetCharacterInput_ = false;
                releaseAllUiNotes();
                const char *name = nullptr;
                if(!presetNameBuffer_.empty())
                    name = presetNameBuffer_.c_str();
                else if(selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
                    name = presetNames_[(size_t)selectedPresetIndex_].c_str();
                if(auto *p = plugin())
                    p->deleteUserPreset(name);
                selectedPresetIndex_ = -1;
                presetNameBuffer_ = "user_kapibara";
                pullFromPlugin();
                return true;
            }
            if(presetMenuResetRect_.contains(x, y))
            {
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                skipNextPresetCharacterInput_ = false;
                releaseAllUiNotes();
                if(auto *p = plugin())
                    p->resetUserPreset();
                pullFromPlugin();
                return true;
            }
            if(presetMenuPanelRect_.contains(x, y))
                return true;

            presetMenuOpen_ = false;
        }
        if(panicRect_.contains(x, y))
        {
            if(auto *p = plugin())
                p->panic();
            clearUiNoteState();
            return true;
        }
        if(menuRect_.contains(x, y))
        {
            optionsMenuOpen_ = !optionsMenuOpen_;
            presetMenuOpen_ = false;
            return true;
        }
        if(optionsMenuOpen_)
        {
            if(optionsMenuPanelRect_.contains(x, y))
                return false;
            optionsMenuOpen_ = false;
        }
        if(aboutRect_.contains(x, y))
            return true;
        if(gainRect_.contains(x, y))
        {
            dragTarget_    = DragTarget::Gain;
            dragStartY_    = y;
            dragStartNorm_ = clampf(gain_, 0.0f, 1.0f);
            return true;
        }
        return false;
    }

    // Direct gain/pan/send drag on ANY strip (no need to select the strip first).
    bool handleStripFaderPress(float x, float y)
    {
        const int n = int(generator_.tracks.size());
        for(int i = 0; i < n && i < int(synth::kMaxSourceTracks); ++i)
        {
            DragTarget tgt = DragTarget::None;
            float norm = 0.0f;
            const auto &t = generator_.tracks[(size_t)i];
            if(stripGainRects_[(size_t)i].w > 0.0f && stripGainRects_[(size_t)i].contains(x, y))
            { tgt = DragTarget::TrackGain; norm = t.gain * 0.5f; }
            else if(stripPanRects_[(size_t)i].w > 0.0f && stripPanRects_[(size_t)i].contains(x, y))
            { tgt = DragTarget::TrackPan; norm = (t.pan + 1.0f) * 0.5f; }
            else if(stripSendRects_[(size_t)i].w > 0.0f && stripSendRects_[(size_t)i].contains(x, y))
            { tgt = DragTarget::TrackSend; norm = t.send; }
            if(tgt == DragTarget::None) continue;
            dragTrackIndex_ = i;           // adjust this strip directly; editor view unchanged
            dragTarget_ = tgt;
            dragStartY_ = y;
            dragStartNorm_ = clampf(norm, 0.0f, 1.0f);
            return true;
        }
        return false;
    }

    bool handlePageClick(float x, float y)
    {
        // Editor tab switch (SOURCE / SHAPE / VOICE / MAPPING).
        for(int i = 0; i < int(editorTabRects_.size()); ++i)
            if(editorTabRects_[(size_t)i].contains(x, y))
            {
                editorTab_ = i;
                repaint();
                return true;
            }
        // Grid axis picker (open): pick an item, or click outside to dismiss.
        if(gridPickerMode_ != 0)
        {
            for(size_t k = 0; k < gridPickerItemRects_.size(); ++k)
                if(gridPickerItemRects_[k].contains(x, y))
                {
                    const int idx = gridPickerPoolIdx_[k];
                    if(gridPickerMode_ == 1) gridSources_.push_back(kGridSourcePool[idx]);
                    else                     gridDests_.push_back(kGridDestPool[idx]);
                    gridPickerMode_ = 0;
                    repaint();
                    return true;
                }
            gridPickerMode_ = 0;
            repaint();
            return true;
        }
        // Matrix dashboard tab switch (GRID / MODULATORS / AMP ENV).
        for(int i = 0; i < int(matrixTabRects_.size()); ++i)
            if(matrixTabRects_[(size_t)i].contains(x, y))
            {
                matrixTab_ = i;
                repaint();
                return true;
            }
        // Grid axis "+" add buttons.
        if(gridAddSrcRect_.contains(x, y))
        {
            gridPickerMode_ = 1;
            gridPickerX_ = gridAddSrcRect_.x;
            gridPickerY_ = gridAddSrcRect_.y + 22.0f;
            repaint();
            return true;
        }
        if(gridAddDstRect_.contains(x, y))
        {
            gridPickerMode_ = 2;
            gridPickerX_ = gridAddDstRect_.x;
            gridPickerY_ = gridAddDstRect_.y + 18.0f;
            repaint();
            return true;
        }
        // Matrix grid node create / depth-drag.
        if(handleMatrixGridPress(x, y))
            return true;
        // Direct strip fader/pan/send drag takes priority over strip selection.
        if(handleStripFaderPress(x, y))
            return true;
        // Insert-slot buttons must be checked before strip selection logic
        if(handleInsertButtonClick(x, y))
            return true;
        if(handleModColumnClick(x, y))
            return true;
        if(handleRouteFxClick(x, y))
            return true;
        if(handleModEditorClick(x, y))
            return true;
        return handleButtonClick(x, y) || handleControlPress(x, y);
    }

    bool handleButtonClick(float x, float y)
    {
        if(addTrackRect_.contains(x, y))
        {
            addTrackMenuOpen_ = !addTrackMenuOpen_;
            return true;
        }
        if(removeTrackRect_.contains(x, y) && !generator_.tracks.empty())
        {
            const uint32_t id = generator_.tracks[(size_t)selectedTrack_].id;
            if(auto *p = plugin())
            {
                p->removeSourceTrack(id);
                pullFromPlugin();
                selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
            }
            return true;
        }
        if(addTrackMenuOpen_)
        {
            for(int i = 0; i < 4; ++i)
            {
                if(addTrackTypeRects_[(size_t)i].contains(x, y))
                {
                    const auto type = static_cast<synth::SourceTrackType>(i);
                    if(auto *p = plugin())
                    {
                        p->addSourceTrack(type, synth::sourceTrackTypeName(type));
                        pullFromPlugin();
                        selectedTrack_ = int(generator_.tracks.size()) - 1;
                    }
                    addTrackMenuOpen_ = false;
                    return true;
                }
            }
        }
        for(size_t i = 0; i < trackRowRects_.size(); ++i)
        {
            if(trackRowRects_[i].contains(x, y) && i < generator_.tracks.size())
            {
                if(selectedTrack_ != int(i))
                {
                    selectedMetaFrame_ = 0;
                    metaFrameSelected_.fill(false);
                    metaFrameRangeAnchor_ = -1;
                    wavetablePresetLabel_ = "Select Wavetable";
                }
                selectedTrack_ = int(i);
                selectedAmpEnv_ = clampi(generator_.tracks[i].ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
                return true;
            }
        }
        for(size_t i = 0; i < stripRects_.size(); ++i)
        {
            const auto &s = stripRects_[i];
            if(s.contains(x, y) && i < generator_.tracks.size())
            {
                if(shiftDown_)
                {
                    // Shift+click: toggle strip in multi-selection without changing editor focus
                    selectedStrips_[i] = !selectedStrips_[i];
                    if(selectedStrips_[i])
                        selectedTrack_ = int(i);
                    return true;
                }
                // Normal click: clear multi-selection, select this strip
                selectedStrips_.fill(false);
                selectedStrips_[i] = true;
                selectedGroupView_ = -1;  // leave group view
                if(selectedTrack_ != int(i))
                {
                    selectedMetaFrame_ = 0;
                    metaFrameSelected_.fill(false);
                    metaFrameRangeAnchor_ = -1;
                    wavetablePresetLabel_ = "Select Wavetable";
                }
                selectedTrack_ = int(i);
                auto &track = generator_.tracks[i];
                selectedAmpEnv_ = clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);

                // Gain/Pan/Send 在 selectedTrack 上且 rects 已在 drawStripRack 里设好 → 交给 handleControlPress
                if(trackGainRect_.contains(x, y) || trackPanRect_.contains(x, y) || trackSendRect_.contains(x, y))
                    return false;

                // ADSR route: cycle which amp env this strip uses.
                if(stripEnvRect_.contains(x, y))
                {
                    track.ampEnvIndex = (clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) + 1) % synth::kMaxAmpEnvs;
                    selectedAmpEnv_ = track.ampEnvIndex;
                    pushCurrentTrack();
                    return true;
                }
                // Duplicate this strip's amp env into a free slot and use it.
                if(stripDupRect_.contains(x, y))
                {
                    const int src = clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
                    int dst = (src + 1) % synth::kMaxAmpEnvs;
                    for(int e = 0; e < synth::kMaxAmpEnvs; ++e)
                        if(e != src && envUseCount(e) == 0) { dst = e; break; }
                    ampEnvs_[(size_t)dst] = ampEnvs_[(size_t)src];
                    track.ampEnvIndex = dst;
                    selectedAmpEnv_ = dst;
                    pushAmpEnv();
                    pushCurrentTrack();
                    return true;
                }

                if(stripMuteRects_[i].contains(x, y))
                {
                    track.mute = !track.mute;
                    pushCurrentTrack();
                }
                else if(stripSoloRects_[i].contains(x, y))
                {
                    track.solo = !track.solo;
                    pushCurrentTrack();
                }
                return true;
            }
        }
        // Left-click a group bus → show its OSC/effect view in the editor
        for(size_t gi = 0; gi < stripGroupBusRects_.size() && gi < stripGroups_.size(); ++gi)
        {
            if(stripGroupBusRects_[gi].contains(x, y))
            {
                selectedGroupView_ = int(gi);
                return true;
            }
        }
        if(auto *track = currentTrack())
        {
            if(ampEnvSelectRect_.contains(x, y))
            {
                track->ampEnvIndex = (clampi(track->ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) + 1) % synth::kMaxAmpEnvs;
                selectedAmpEnv_ = track->ampEnvIndex;
                pushCurrentTrack();
                return true;
            }
            if(duplicateEnvRect_.contains(x, y))
            {
                const int src = clampi(track->ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
                int dst = (src + 1) % synth::kMaxAmpEnvs;
                for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
                {
                    if(i != src && envUseCount(i) == 0)
                    {
                        dst = i;
                        break;
                    }
                }
                ampEnvs_[(size_t)dst] = ampEnvs_[(size_t)src];
                track->ampEnvIndex = dst;
                selectedAmpEnv_ = dst;
                pushAmpEnv();
                pushCurrentTrack();
                return true;
            }
            if(inharmonicModeRect_.contains(x, y) && track->type == synth::SourceTrackType::PartialBank)
            {
                track->partialBank.freqShape = static_cast<synth::FreqShape>((int(track->partialBank.freqShape) + 1) % 3);
                pushCurrentTrack();
                return true;
            }
            if(metaFrameScrollRect_.contains(x, y)
               && (track->type == synth::SourceTrackType::MetaOscillator
                   || track->type == synth::SourceTrackType::PartialBank))
            {
                dragTarget_ = DragTarget::MetaFrameScroll;
                dragScrollStartX_ = x;
                dragScrollStartVal_ = metaFrameScrollStart_;
                return true;
            }
            if(metaWavetableNameRect_.contains(x, y)
               && (track->type == synth::SourceTrackType::MetaOscillator
                   || track->type == synth::SourceTrackType::PartialBank))
            {
                wavetablePresetMenuOpen_ = !wavetablePresetMenuOpen_;
                presetMenuOpen_ = false;
                if(wavetablePresetMenuOpen_)
                    refreshWavetablePresets();
                return true;
            }
            if((metaWavetablePrevRect_.contains(x, y) || metaWavetableNextRect_.contains(x, y))
               && (track->type == synth::SourceTrackType::MetaOscillator
                   || track->type == synth::SourceTrackType::PartialBank))
            {
                if(wavetablePresets_.empty())
                    refreshWavetablePresets();
                if(!wavetablePresets_.empty())
                {
                    if(selectedWavetablePresetIndex_ < 0)
                        selectedWavetablePresetIndex_ = 0;
                    else if(metaWavetablePrevRect_.contains(x, y))
                        selectedWavetablePresetIndex_ = (selectedWavetablePresetIndex_ + int(wavetablePresets_.size()) - 1)
                                                         % int(wavetablePresets_.size());
                    else
                        selectedWavetablePresetIndex_ = (selectedWavetablePresetIndex_ + 1)
                                                         % int(wavetablePresets_.size());
                    loadWavetablePreset(selectedWavetablePresetIndex_);
                }
                return true;
            }
            if(metaWarpModeRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                track->metaOsc.warpMode = static_cast<synth::WavetableWarpMode>((int(track->metaOsc.warpMode) + 1) % 4);
                pushCurrentTrack();
                return true;
            }
            if(basicShapeRect_.contains(x, y) && track->type == synth::SourceTrackType::BasicOscillator)
            {
                track->basicShape = static_cast<synth::BasicOscillatorShape>((int(track->basicShape) + 1) % 5);
                pushCurrentTrack();
                return true;
            }
            if(noiseModeRect_.contains(x, y) && track->type == synth::SourceTrackType::SampleNoise)
            {
                track->sampleNoiseMode = synth::SampleNoiseMode::Noise;
                return true;
            }
            if(metaHarmonicEditRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                harmonicEditorOpen_ = true;
                return true;
            }
            if(metaHarmonicEditRect_.contains(x, y) && track->type == synth::SourceTrackType::PartialBank)
            {
                harmonicEditorOpen_ = true;
                metaProcessContextMenuOpen_ = false;
                selectedMetaFrame_ = partialBankMorphFrameIndex(track->partialBank);
                return true;
            }
            if(metaLoadRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                beginWavetableImport();
                return true;
            }
            if(metaSaveRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                openWavetableSaveBrowser();
                return true;
            }
            for(int i = 0; i < int(metaFramePresetRects_.size()); ++i)
            {
                if(metaFramePresetRects_[(size_t)i].contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    applyFramePreset(i);
                    return true;
                }
            }
            for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
            {
                if(metaFrameRects_[(size_t)local].contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    selectMetaFrameAt(metaFramePageStart_ + local, track->metaOsc.frameCount);
                    return true;
                }
                if(metaFrameRects_[(size_t)local].contains(x, y) && track->type == synth::SourceTrackType::PartialBank)
                {
                    selectMetaFrameAt(metaFramePageStart_ + local, track->partialBank.frameCount);
                    return true;
                }
            }
        }
        const int sourceOptions[4] = { 1, 2, 4, 8 };
        for(int i = 0; i < 4; ++i)
        {
            if(sourceCountRects_[(size_t)i].contains(x, y))
            {
                generator_.sourceCount = sourceOptions[i];
                generator_.wavetableSeed.partialCount = synth::kMaxWavetablePartials;
                for(int p = 0; p < synth::kMaxWavetablePartials; ++p)
                    generator_.wavetableSeed.partials[(size_t)p].enabled = true;
                selectedSource_ = clampi(selectedSource_, 0, generator_.sourceCount - 1);
                const int metas = synth::metaPartialsPerGeneratorSource(generator_.sourceCount);
                selectedMetaPartial_ = selectedSource_ * metas;
                pushGenerator();
                return true;
            }
        }
        for(int i = 0; i < generator_.sourceCount; ++i)
        {
            if(sourceChainRects_[(size_t)i].contains(x, y))
            {
                selectedSource_ = i;
                selectedMetaPartial_ = selectedSource_ * synth::metaPartialsPerGeneratorSource(generator_.sourceCount);
                selectedMetaFrame_ = 0;
                return true;
            }
        }
        for(int i = 0; i < synth::kEditableMetaPartials; ++i)
        {
            if(metaSelectRects_[(size_t)i].w > 0.0f && metaSelectRects_[(size_t)i].contains(x, y))
            {
                selectedMetaPartial_ = i;
                selectedMetaFrame_ = 0;
                return true;
            }
        }
        auto &metaSlot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        if(inharmonicModeRect_.contains(x, y))
        {
            generator_.wavetableSeed.freqShape = static_cast<synth::FreqShape>((int(generator_.wavetableSeed.freqShape) + 1) % 3);
            pushGenerator();
            return true;
        }
        if(sourceFilterEnableRect_.contains(x, y))
        {
            auto &filter = generator_.sources[(size_t)selectedSource_].filter;
            filter.enabled = !filter.enabled;
            pushSource();
            return true;
        }
        if(sourceFilterTopologyRect_.contains(x, y))
        {
            auto &filter = generator_.sources[(size_t)selectedSource_].filter;
            filter.topology = static_cast<synth::SourceFilterTopology>(
                (int(filter.topology) + 1) % (int(synth::SourceFilterTopology::FeedbackLadder) + 1));
            if(filter.topology == synth::SourceFilterTopology::Bypass)
                filter.enabled = false;
            else
                filter.enabled = true;
            pushSource();
            return true;
        }
        if(metaWarpModeRect_.contains(x, y))
        {
            metaSlot.warpMode = static_cast<synth::WavetableWarpMode>((int(metaSlot.warpMode) + 1) % 4);
            pushMetaPartialRuntime();
            return true;
        }
        if(metaFrameButtonRect_.contains(x, y))
        {
            selectedMetaFrame_ = (selectedMetaFrame_ + 1) % std::max(1, metaSlot.frameCount);
            return true;
        }
        if(metaHarmonicEditRect_.contains(x, y) || metaWaveformRect_.contains(x, y))
        {
            harmonicEditorOpen_ = true;
            return true;
        }
        if(metaLoadRect_.contains(x, y))
        {
            beginWavetableImport();
            return true;
        }
        if(metaSaveRect_.contains(x, y))
        {
            openWavetableSaveBrowser();
            return true;
        }
        for(int i = 0; i < int(metaFramePresetRects_.size()); ++i)
        {
            if(metaFramePresetRects_[(size_t)i].contains(x, y))
            {
                applyFramePreset(i);
                return true;
            }
        }
        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            if(metaFrameRects_[(size_t)local].contains(x, y))
            {
                selectMetaFrameAt(metaFramePageStart_ + local, metaSlot.frameCount);
                return true;
            }
        }

        if(opEnableRect_.contains(x, y))
        {
            operatorChain_.ops[(size_t)selectedOp_].enabled = !operatorChain_.ops[(size_t)selectedOp_].enabled;
            pushOperator();
            return true;
        }
        if(opTypeRect_.contains(x, y))
        {
            auto &op = operatorChain_.ops[(size_t)selectedOp_];
            op.type = static_cast<synth::OperatorType>((int(op.type) + 1) % 5);
            pushOperator();
            return true;
        }
        for(int i = 0; i < 5; ++i)
            if(opSelectRects_[(size_t)i].contains(x, y)) { selectedOp_ = i; return true; }

        auto &op = operatorChain_.ops[(size_t)selectedOp_];
        if(op.type == synth::OperatorType::PartialMask)
        {
            if(opParamCRect_.contains(x, y)) { op.maskGroupLow = !op.maskGroupLow; pushOperator(); return true; }
            if(opParamDRect_.contains(x, y)) { op.maskGroupMid = !op.maskGroupMid; pushOperator(); return true; }
        }

        for(int i = 0; i < synth::kMaxLfos; ++i)
            if(lfoSelectRects_[(size_t)i].contains(x, y))
            {
                selectedLfo_ = i;
                selectedMatrixModSlot_ = i;
                beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Lfo1) + i),
                                  lfoSelectRects_[(size_t)i], x, y);
                return true;
            }
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
            if(envSelectRects_[(size_t)i].contains(x, y))
            {
                selectedEnv_ = i;
                selectedMatrixModSlot_ = synth::kMaxLfos + i;
                beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Env1) + i),
                                  envSelectRects_[(size_t)i], x, y);
                return true;
            }
        for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
            if(ampEnvTabRects_[(size_t)i].contains(x, y))
            {
                selectedAmpEnv_ = i;
                beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Adsr1) + i),
                                  ampEnvTabRects_[(size_t)i], x, y);
                return true;
            }

        auto &lfo = lfos_[(size_t)selectedLfo_];
        if(lfoShapeRect_.contains(x, y)) { lfo.shape = static_cast<synth::LfoShape>((int(lfo.shape) + 1) % 5); pushLfoOnly(); return true; }

        if(eqEnableRect_.contains(x, y)) { effects_.eq.enabled = !effects_.eq.enabled; pushEffects(); return true; }
        if(eqModeRect_.contains(x, y)) { effects_.eq.mode = static_cast<synth::EffectProcessMode>((int(effects_.eq.mode) + 1) % 3); pushEffects(); return true; }
        if(filterEnableRect_.contains(x, y)) { effects_.filter.enabled = !effects_.filter.enabled; pushEffects(); return true; }
        if(filterModeRect_.contains(x, y)) { effects_.filter.mode = static_cast<synth::EffectProcessMode>((int(effects_.filter.mode) + 1) % 3); pushEffects(); return true; }
        if(filterTypeRect_.contains(x, y)) { effects_.filter.type = static_cast<synth::FilterType>((int(effects_.filter.type) + 1) % 3); pushEffects(); return true; }

        return false;
    }

    bool handleControlPress(float x, float y)
    {
        // Absolute-position controls (waveform/spectrum editors, scrollbars, bar charts)
        const auto setDragAbs = [&](DragTarget target) -> bool {
            dragTarget_ = target;
            applyDragValue(x, y);
            return true;
        };
        // Delta-based knob controls: drag up = increase, drag down = decrease (Vital-style)
        const auto setDragKnob = [&](DragTarget target, float currentNorm) -> bool {
            dragTarget_ = target;
            dragStartY_    = y;
            dragStartNorm_ = clampf(currentNorm, 0.0f, 1.0f);
            return true;
        };

        auto *track    = currentTrack();
        auto &metaSlot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &lfo      = lfos_[(size_t)selectedLfo_];
        auto &env      = envs_[(size_t)selectedEnv_];
        auto &source   = generator_.sources[(size_t)selectedSource_];
        auto &ampEnv   = ampEnvs_[(size_t)selectedAmpEnv_];

        // Track strip knobs
        if(trackGainRect_.contains(x, y))
            return setDragKnob(DragTarget::TrackGain, track ? track->gain * 0.5f : 0.5f);
        if(trackPanRect_.contains(x, y))
            return setDragKnob(DragTarget::TrackPan, track ? (track->pan + 1.0f) * 0.5f : 0.5f);
        if(trackSendRect_.contains(x, y))
            return setDragKnob(DragTarget::TrackSend, track ? track->send : 0.0f);

        // Partial bank harmonic knobs.
        if(track && track->type == synth::SourceTrackType::PartialBank)
        {
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                if(partialKnobRects_[(size_t)i].w > 0.0f && partialKnobRects_[(size_t)i].contains(x, y))
                {
                    selectedPartialIndex_ = i;
                    auto &seed = track->partialBank;
                    seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    const float amp = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)i].amp;
                    return setDragKnob(DragTarget::PartialAmp, amp);
                }
            }
        }
        if(partialAmpRect_.contains(x, y)) {
            auto *ptrack = track;
            float n = 0.0f;
            if(ptrack && ptrack->type == synth::SourceTrackType::PartialBank)
            {
                auto &seed = ptrack->partialBank;
                seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                n = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_].amp;
            }
            return setDragKnob(DragTarget::PartialAmp, n);
        }
        if(partialRatioRect_.contains(x, y)) {
            float ratio = 1.0f;
            if(track && track->type == synth::SourceTrackType::PartialBank)
                ratio = track->partialBank.partials[(size_t)selectedPartialIndex_].ratio;
            return setDragKnob(DragTarget::PartialRatio, std::min(1.0f, ratio / 64.0f));
        }

        if(basicPulseRect_.contains(x, y))
            return setDragKnob(DragTarget::BasicPulse, track ? track->pulseWidth : 0.5f);
        if(basicSubRect_.contains(x, y))
            return setDragKnob(DragTarget::BasicSub, track ? track->subLevel : 0.0f);
        if(noiseColorRect_.contains(x, y))
            return setDragKnob(DragTarget::NoiseColor, track ? track->noiseColor : 0.5f);
        if(partialCountRect_.contains(x, y)) {
            lastTrackRealtimeDragPushMs_ = 0u;
            lastGenRealtimeDragPushMs_ = 0u;
            int cnt = track ? track->partialBank.partialCount : generator_.wavetableSeed.partialCount;
            return setDragKnob(DragTarget::PartialCount, float(cnt - 1) / 63.0f);
        }
        if(inharmonicRect_.contains(x, y)) {
            lastTrackRealtimeDragPushMs_ = 0u;
            lastGenRealtimeDragPushMs_ = 0u;
            float inh = track ? track->partialBank.inharmonicAmount : generator_.wavetableSeed.inharmonicAmount;
            return setDragKnob(DragTarget::Inharmonic, inh);
        }
        if(gainRect_.contains(x, y)) return setDragKnob(DragTarget::Gain, gain_);
        if(sourceGainRect_.contains(x, y)) return setDragKnob(DragTarget::SourceGain, source.gain * 0.5f);
        if(sourcePanRect_.contains(x, y)) return setDragKnob(DragTarget::SourcePan, (source.pan + 1.0f) * 0.5f);
        if(sourceFilterCutoffRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterCutoff, cutoffToNorm(source.filter.cutoffHz));
        if(sourceFilterResRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterResonance, source.filter.resonance);
        if(sourceFilterDriveRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterDrive, source.filter.drive / 8.0f);
        if(sourceFilterFeedbackRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterFeedback, source.filter.feedback);
        if(sourceFilterMixRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterMix, source.filter.mix);

        // Unison knobs
        {
            int uv = track ? track->unison.voices : generator_.unison.voices;
            float ud = track ? track->unison.detuneCents : generator_.unison.detuneCents;
            float uw = track ? track->unison.widthStereo : generator_.unison.widthStereo;
            float up = track ? track->unison.phaseSpread : generator_.unison.phaseSpread;
            if(unisonVoicesRect_.contains(x, y)) return setDragKnob(DragTarget::UnisonVoices, float(uv - 1) / 15.0f);
            if(unisonDetuneRect_.contains(x, y)) return setDragKnob(DragTarget::UnisonDetune, ud / 80.0f);
            if(unisonWidthRect_.contains(x, y))  return setDragKnob(DragTarget::UnisonWidth, uw);
            if(unisonPhaseRect_.contains(x, y))  return setDragKnob(DragTarget::UnisonPhase, up);
        }

        // ADSR knobs
        if(attackRect_.contains(x, y))  return setDragKnob(DragTarget::Attack,  ampEnv.attack / 5.0f);
        if(decayRect_.contains(x, y))   return setDragKnob(DragTarget::Decay,   ampEnv.decay  / 5.0f);
        if(sustainRect_.contains(x, y)) return setDragKnob(DragTarget::Sustain, ampEnv.sustain);
        if(releaseRect_.contains(x, y)) return setDragKnob(DragTarget::Release, ampEnv.release / 8.0f);
        if(curveRect_.contains(x, y))   return setDragKnob(DragTarget::Curve,   adsr_.curve);

        // MetaOsc — drag 开始时记录 undo 快照（每次按下只记一次）
        const auto setDragMetaAbs = [&](DragTarget tgt) -> bool {
            pushMetaUndoSnapshot();
            return setDragAbs(tgt);
        };
        const auto setDragMetaKnob = [&](DragTarget tgt, float norm) -> bool {
            pushMetaUndoSnapshot();
            return setDragKnob(tgt, norm);
        };
        if(metaWaveformRect_.contains(x, y)) return setDragMetaAbs(DragTarget::MetaWaveform);

        // Pitch controls (OCT/SEM/FIN/CRS) — MetaOscillator or whole PartialBank group.
        if(track && (track->type == synth::SourceTrackType::MetaOscillator
                     || track->type == synth::SourceTrackType::PartialBank))
        {
            auto &ms = track->type == synth::SourceTrackType::MetaOscillator
                           ? track->metaOsc
                           : track->partialBank.partials[0];
            if(metaOctRect_.contains(x, y))
            {
                if(track->type == synth::SourceTrackType::MetaOscillator)
                    pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchOct;
                dragStartY_   = y;
                dragStartOct_ = ms.pitchOct;
                return true;
            }
            if(metaSemRect_.contains(x, y))
            {
                if(track->type == synth::SourceTrackType::MetaOscillator)
                    pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchSem;
                dragStartY_   = y;
                dragStartSem_ = ms.pitchSem;
                return true;
            }
            if(metaFinRect_.contains(x, y))
            {
                if(track->type == synth::SourceTrackType::MetaOscillator)
                    pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchFin;
                dragStartY_   = y;
                dragStartFin_ = ms.pitchFin;
                return true;
            }
            if(metaCrsRect_.contains(x, y))
            {
                if(track->type == synth::SourceTrackType::MetaOscillator)
                    pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchCrs;
                dragStartY_   = y;
                dragStartCrs_ = ms.pitchCrs;
                return true;
            }
        }

        if(track && track->type == synth::SourceTrackType::PartialBank)
        {
            auto &seed = track->partialBank;
            if(metaFrameCountRect_.contains(x, y))
                return setDragKnob(DragTarget::MetaFrameCount,
                                   float(seed.frameCount - 1) / float(synth::kMaxWavetableFrames - 1));
            if(metaMorphRect_.contains(x, y))
                return setDragKnob(DragTarget::MetaMorph, seed.morph);
        }

        {
            bool isMeta = track && track->type == synth::SourceTrackType::MetaOscillator;
            auto &ms = isMeta ? track->metaOsc : metaSlot;
            if(metaRatioRect_.contains(x, y))
                return setDragMetaKnob(DragTarget::MetaRatio, std::min(1.0f, std::log2(std::max(0.01f, ms.ratio)) / 7.0f));
            if(metaAmpRect_.contains(x, y))         return setDragMetaKnob(DragTarget::MetaAmp,        ms.amp);
            if(metaPhaseRect_.contains(x, y))       return setDragMetaKnob(DragTarget::MetaPhase,      (ms.phase + kPi) / (2.0f * kPi));
            if(metaPanRect_.contains(x, y))         return setDragMetaKnob(DragTarget::MetaPan,        (ms.pan + 1.0f) * 0.5f);
            if(metaFrameCountRect_.contains(x, y))   return setDragMetaKnob(DragTarget::MetaFrameCount,
                                                                            float(ms.frameCount - 1) / float(synth::kMaxWavetableFrames - 1));
            if(metaMorphRect_.contains(x, y))       return setDragKnob(DragTarget::MetaMorph,      ms.morph); // morph不修改帧数据
            if(metaWarpAmountRect_.contains(x, y))  return setDragMetaKnob(DragTarget::MetaWarpAmount, (ms.warpAmount + 1.0f) * 0.5f);
        }
        if(metaFrameStripRect_.contains(x, y)) return setDragAbs(DragTarget::MetaFrameScan);
        if(metaHarmonicRatioRect_.contains(x, y)) {
            float n = float(selectedMetaHarmonic_) / float(synth::kEditableWavetableHarmonics - 1);
            return setDragMetaKnob(DragTarget::MetaHarmonicRatio, n);
        }
        {
            auto &hFrame = metaSlot.frames[(size_t)selectedMetaFrame_];
            auto &hh = hFrame.harmonics[(size_t)selectedMetaHarmonic_];
            if(metaHarmonicAmpRect_.contains(x, y))   return setDragMetaKnob(DragTarget::MetaHarmonicAmp,   hh.amp);
            if(metaHarmonicPhaseRect_.contains(x, y)) return setDragMetaKnob(DragTarget::MetaHarmonicPhase, (hh.phase + kPi) / (2.0f * kPi));
        }

        // Matrix / LFO / ENV / Rules
        if(modModeRect_.contains(x, y))
        {
            env.loop = !env.loop;   // ENV slot: toggle one-shot <-> loop
            pushEnvOnly();
            return true;
        }
        if(modEnvRateRect_.contains(x, y)) return setDragKnob(DragTarget::ModEnvRate, env.loopRateHz / 20.0f);
        if(lfoFreqRect_.contains(x, y))  return setDragKnob(DragTarget::LfoFreq,  lfo.frequencyHz / 20.0f);
        if(lfoPhaseRect_.contains(x, y)) return setDragKnob(DragTarget::LfoPhase, lfo.phase0);
        if(lfoRhoRect_.contains(x, y))   return setDragKnob(DragTarget::LfoRho,   lfo.rhoLfo);
        if(envPointARect_.contains(x, y)) return setDragKnob(DragTarget::EnvPointA, env.points[1].y);
        if(envPointBRect_.contains(x, y)) return setDragKnob(DragTarget::EnvPointB, env.points[2].y);
        if(envCurveARect_.contains(x, y)) return setDragKnob(DragTarget::EnvCurveA, (env.points[1].curve + 1.0f) * 0.5f);
        if(matrixEnvCurveRect_.contains(x, y))
        {
            auto *pts = curCurvePoints();
            int &countRef = curCurveCount();
            countRef = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
            const int hit = matrixEnvPointAt(x, y);
            // Double-click: remove a middle point, or add one on empty curve.
            if(currentClickIsDouble_)
            {
                if(hit > 0 && hit < countRef - 1)
                    deleteMatrixEnvPoint(hit);
                else if(hit < 0)
                    addMatrixEnvPoint(x, y);
                matrixEnvDirty_ = true;
                pushCurCurve();
                return true;
            }
            // Ctrl-drag a segment → bend (per-segment curvature).
            if(ctrlDown_)
            {
                const int seg = matrixEnvSegmentAt(x);
                if(seg >= 0)
                {
                    envDragSeg_ = seg;
                    selectedEnvPoint_ = seg;
                    dragTarget_ = DragTarget::MatrixEnvSeg;
                    dragStartY_ = y;
                    dragStartDepth_ = pts[seg].curve;
                    return true;
                }
            }
            // Single click selects the point under the cursor; empty space just
            // deselects (no teleport — drag an existing point to move it).
            selectedEnvPoint_ = hit;
            if(hit < 0)
                return true;
            return setDragAbs(DragTarget::MatrixEnvCurve);
        }
        // Route-FX insert knobs (flattened editor)
        for(const auto &h : fxKnobHits_)
            if(h.rect.w > 0.0f && h.rect.contains(x, y))
            {
                auto *chain = insertChainFor(h.trackId, h.groupIdx);
                if(chain != nullptr && h.insertIdx < int(chain->size()))
                {
                    fxDragHit_ = h;
                    return setDragKnob(DragTarget::FxInsertKnob, fxKnobNorm((*chain)[(size_t)h.insertIdx], h.knob));
                }
            }
        if(chaosRateRect_.contains(x, y))   return setDragKnob(DragTarget::ChaosRate,   chaos_.frequencyHz / 60.0f);
        if(chaosAmountRect_.contains(x, y)) return setDragKnob(DragTarget::ChaosAmount, chaos_.amount);
        if(shapePhaseRect_.contains(x, y))  return setDragKnob(DragTarget::ShapePhase,  shape_.phase0);
        if(shapeRhoRect_.contains(x, y))    return setDragKnob(DragTarget::ShapeRho,    shape_.rho);
        if(shapeUpRect_.contains(x, y))     return setDragKnob(DragTarget::ShapeUp,     shape_.pUp / 8.0f);
        if(shapeDownRect_.contains(x, y))   return setDragKnob(DragTarget::ShapeDown,   shape_.pDown / 8.0f);
        if(eqLowRect_.contains(x, y))    return setDragKnob(DragTarget::EqLow,       (effects_.eq.lowGainDb  + 24.0f) / 48.0f);
        if(eqMidRect_.contains(x, y))    return setDragKnob(DragTarget::EqMid,       (effects_.eq.midGainDb  + 24.0f) / 48.0f);
        if(eqHighRect_.contains(x, y))   return setDragKnob(DragTarget::EqHigh,      (effects_.eq.highGainDb + 24.0f) / 48.0f);
        if(eqDriveRect_.contains(x, y))  return setDragKnob(DragTarget::EqDrive,     effects_.eq.drive / 6.0f);
        if(filterCutoffRect_.contains(x, y))  return setDragKnob(DragTarget::FilterCutoff,    cutoffToNorm(effects_.filter.cutoffHz));
        if(filterResRect_.contains(x, y))     return setDragKnob(DragTarget::FilterResonance, effects_.filter.resonance);
        if(filterDriveRect_.contains(x, y))   return setDragKnob(DragTarget::FilterDrive,     effects_.filter.drive / 6.0f);
        if(uiScaleRect_.contains(x, y))  return setDragKnob(DragTarget::UiScale, (uiScale_ - 0.75f) / 0.75f);
        if(opParamARect_.contains(x, y)) return setDragAbs(DragTarget::OpParamA);
        if(opParamBRect_.contains(x, y)) return setDragAbs(DragTarget::OpParamB);
        if(opParamCRect_.contains(x, y)) return setDragAbs(DragTarget::OpParamC);
        if(opParamDRect_.contains(x, y)) return setDragAbs(DragTarget::OpParamD);

        // Mod-entry depth sliders (absolute horizontal)
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
            if(modDepthRects_[(size_t)i].w > 0.0f && modDepthRects_[(size_t)i].contains(x, y))
            {
                selectedModSlot_ = i;
                return setDragAbs(DragTarget::ModEntryDepth);
            }

        // Strip scrollbar drag
        if(stripScrollbarRect_.w > 0.0f && stripScrollbarRect_.contains(x, y))
        {
            dragTarget_   = DragTarget::StripScroll;
            dragStartY_   = x;
            dragScrollStartX_  = x;
            dragScrollStartValF_ = stripScrollF_;
            return true;
        }

        return false;
    }

    void applyDragValue(float x, float y)
    {
        // Absolute horizontal position (scrollbars, waveform editors, bar charts)
        const auto normIn = [&](const Rect &r) { return clampf((x - r.x) / std::max(1.0f, r.w), 0.0f, 1.0f); };
        // Vertical delta-based knob: drag up = increase (200 px = full range, fine with modifier)
        // Sensitivity is in screen pixels (×uiRenderScale_) so knobs feel the same
        // regardless of window size — ~200 screen px for the full range.
        const auto knobNorm = [&]() -> float {
            return clampf(dragStartNorm_ + (dragStartY_ - y) * uiRenderScale_ / 200.0f, 0.0f, 1.0f);
        };
        auto &lfo = lfos_[(size_t)selectedLfo_];
        auto &env = envs_[(size_t)selectedEnv_];
        auto &rule = rules_[(size_t)selectedRule_];
        auto &op = operatorChain_.ops[(size_t)selectedOp_];
        auto &source = generator_.sources[(size_t)selectedSource_];
        auto &sourceFilter = source.filter;
        auto &metaSlot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &metaFrame = metaSlot.frames[(size_t)selectedMetaFrame_];
        auto &metaHarmonic = metaFrame.harmonics[(size_t)selectedMetaHarmonic_];

        switch(dragTarget_)
        {
            case DragTarget::TrackGain:
                if(auto *t = dragTrack()) { t->gain = knobNorm() * 2.0f; pushTrackById(t->id); }
                break;
            case DragTarget::TrackPan:
                if(auto *t = dragTrack()) { t->pan = knobNorm() * 2.0f - 1.0f; pushTrackById(t->id); }
                break;
            case DragTarget::TrackSend:
                if(auto *t = dragTrack()) { t->send = knobNorm(); pushTrackById(t->id); }
                break;
            case DragTarget::PartialAmp:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    auto &slot = seed.partials[(size_t)selectedPartialIndex_];
                    auto &harm = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_];
                    slot.enabled = true;
                    const float amp = knobNorm();
                    slot.amp = amp;
                    harm.ratio = float(selectedPartialIndex_ + 1);
                    harm.amp = amp;
                    if(selectedPartialIndex_ + 1 > seed.partialCount)
                        seed.partialCount = selectedPartialIndex_ + 1;
                    deferTrackPush_ = true;
                }
                break;
            case DragTarget::PartialRatio:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &slot = track->partialBank.partials[(size_t)selectedPartialIndex_];
                    slot.ratio = 0.01f + knobNorm() * 63.99f;
                    deferTrackPush_ = true;
                }
                break;
            case DragTarget::BasicPulse:
                if(auto *track = currentTrack()) { track->pulseWidth = knobNorm(); pushCurrentTrack(); }
                break;
            case DragTarget::BasicSub:
                if(auto *track = currentTrack()) { track->subLevel = knobNorm(); pushCurrentTrack(); }
                break;
            case DragTarget::NoiseColor:
                if(auto *track = currentTrack()) { track->noiseColor = knobNorm(); pushCurrentTrack(); }
                break;
            case DragTarget::PartialCount:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.partialCount = clampi(1 + int(std::round(knobNorm() * 63.0f)), 1, 64);
                    for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
                        track->partialBank.partials[(size_t)i].enabled = i < track->partialBank.partialCount;
                    deferTrackPush_ = true;
                    pushCurrentTrackDuringRealtimeDrag();
                }
                else
                {
                    generator_.wavetableSeed.partialCount = clampi(1 + int(std::round(knobNorm() * 63.0f)), 1, 64);
                    deferGenPush_ = true;
                    pushGeneratorDuringRealtimeDrag();
                }
                break;
            case DragTarget::Inharmonic:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.inharmonicAmount = knobNorm();
                    deferTrackPush_ = true;
                    pushCurrentTrackDuringRealtimeDrag();
                }
                else
                {
                    generator_.wavetableSeed.inharmonicAmount = knobNorm();
                    deferGenPush_ = true;
                    pushGeneratorDuringRealtimeDrag();
                }
                break;
            case DragTarget::Gain: gain_ = knobNorm(); pushGain(); break;
            case DragTarget::SourceGain: source.gain = knobNorm() * 2.0f; pushSource(); break;
            case DragTarget::SourcePan: source.pan = knobNorm() * 2.0f - 1.0f; pushSource(); break;
            case DragTarget::SourceFilterCutoff:
                sourceFilter.cutoffHz = normToCutoff(knobNorm());
                sourceFilter.enabled = true;
                pushSource();
                break;
            case DragTarget::SourceFilterResonance:
                sourceFilter.resonance = knobNorm() * 0.95f;
                sourceFilter.enabled = true;
                pushSource();
                break;
            case DragTarget::SourceFilterDrive:
                sourceFilter.drive = 0.1f + knobNorm() * 7.9f;
                sourceFilter.enabled = true;
                pushSource();
                break;
            case DragTarget::SourceFilterFeedback:
                sourceFilter.feedback = knobNorm() * 0.95f;
                sourceFilter.enabled = true;
                pushSource();
                break;
            case DragTarget::SourceFilterMix:
                sourceFilter.mix = knobNorm();
                sourceFilter.enabled = true;
                pushSource();
                break;
            case DragTarget::UnisonVoices:
                if(auto *track = currentTrack()) { track->unison.voices = clampi(1 + int(std::round(knobNorm() * 15.0f)), 1, 16); pushCurrentTrack(); }
                else { generator_.unison.voices = clampi(1 + int(std::round(knobNorm() * 15.0f)), 1, 16); pushGenerator(); }
                break;
            case DragTarget::UnisonDetune:
                if(auto *track = currentTrack()) { track->unison.detuneCents = knobNorm() * 80.0f; pushCurrentTrack(); }
                else { generator_.unison.detuneCents = knobNorm() * 80.0f; pushGenerator(); }
                break;
            case DragTarget::UnisonWidth:
                if(auto *track = currentTrack()) { track->unison.widthStereo = knobNorm(); pushCurrentTrack(); }
                else { generator_.unison.widthStereo = knobNorm(); pushGenerator(); }
                break;
            case DragTarget::UnisonPhase:
                if(auto *track = currentTrack()) { track->unison.phaseSpread = knobNorm(); pushCurrentTrack(); }
                else { generator_.unison.phaseSpread = knobNorm(); pushGenerator(); }
                break;
            case DragTarget::Attack:
                ampEnvs_[(size_t)selectedAmpEnv_].attack = knobNorm() * 5.0f; pushAmpEnv();
                break;
            case DragTarget::Decay:
                ampEnvs_[(size_t)selectedAmpEnv_].decay = knobNorm() * 5.0f; pushAmpEnv();
                break;
            case DragTarget::Sustain:
                ampEnvs_[(size_t)selectedAmpEnv_].sustain = knobNorm(); pushAmpEnv();
                break;
            case DragTarget::Release:
                ampEnvs_[(size_t)selectedAmpEnv_].release = knobNorm() * 8.0f; pushAmpEnv();
                break;
            case DragTarget::Curve:
                adsr_.curve = knobNorm(); pushAdsr();
                break;
            case DragTarget::ManualCycleLength:
                manualCycleLength_ = clampi(32 + int(std::round(normIn(manualCycleValueRect_) * 65504.0f)), 32, 65536);
                break;
            case DragTarget::MetaWaveform:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    auto &slot = track->metaOsc;
                    auto &frame = slot.frames[(size_t)selectedMetaFrame_];
                    const int h = clampi(int((x - metaWaveformRect_.x) / std::max(1.0f, metaWaveformRect_.w)
                                             * float(synth::kEditableWavetableHarmonics)),
                                         0, synth::kEditableWavetableHarmonics - 1);
                    selectedMetaHarmonic_ = h;
                    auto &harm = frame.harmonics[(size_t)h];
                    harm.ratio = float(h + 1);
                    harm.amp = clampf(1.0f - (y - metaWaveformRect_.y) / std::max(1.0f, metaWaveformRect_.h), 0.0f, 1.0f);
                    frame.useImportedWaveform = false;
                    frame.waveform.reset();
                    frame.spectrum.reset();
                    pushCurrentTrack();
                }
                else editMetaWaveform(x, y);
                break;
            case DragTarget::MetaRatio:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.ratio = std::pow(2.0f, knobNorm() * 7.0f); pushCurrentTrack(); }
                else { metaSlot.ratio = std::pow(2.0f, knobNorm() * 7.0f); pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaAmp:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.amp = knobNorm(); pushCurrentTrack(); }
                else { metaSlot.amp = knobNorm(); pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaPhase:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.phase = knobNorm() * 2.0f * kPi - kPi; pushCurrentTrack(); }
                else { metaSlot.phase = knobNorm() * 2.0f * kPi - kPi; pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaPan:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.pan = knobNorm() * 2.0f - 1.0f; pushCurrentTrack(); }
                else { metaSlot.pan = knobNorm() * 2.0f - 1.0f; pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaFrameCount:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    track->metaOsc.frameCount = clampi(1 + int(std::round(knobNorm() * float(synth::kMaxWavetableFrames - 1))),
                                                       1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = std::min(selectedMetaFrame_, track->metaOsc.frameCount - 1);
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.frameCount = clampi(1 + int(std::round(knobNorm() * float(synth::kMaxWavetableFrames - 1))),
                                                           1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = std::min(selectedMetaFrame_, track->partialBank.frameCount - 1);
                    ensurePartialBankFrameDefaults(track->partialBank, selectedMetaFrame_);
                    pushCurrentTrack();
                }
                else
                {
                    metaSlot.frameCount = clampi(1 + int(std::round(knobNorm() * float(synth::kMaxWavetableFrames - 1))),
                                                 1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = std::min(selectedMetaFrame_, metaSlot.frameCount - 1);
                    pushMetaPartial();
                }
                break;
            case DragTarget::MetaMorph:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.morph = knobNorm(); pushCurrentTrackMorphOnly(); }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.morph = knobNorm();
                    selectedMetaFrame_ = partialBankMorphFrameIndex(track->partialBank);
                    pushCurrentTrack();
                }
                else { metaSlot.morph = knobNorm(); pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaWarpAmount:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.warpAmount = knobNorm() * 2.0f - 1.0f; pushCurrentTrack(); }
                else { metaSlot.warpAmount = knobNorm() * 2.0f - 1.0f; pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaPitchOct:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const int delta = int((dragStartY_ - y) / 22.0f);
                    track->metaOsc.pitchOct = std::max(-4, std::min(4, dragStartOct_ + delta));
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const int delta = int((dragStartY_ - y) / 22.0f);
                    const int oct = std::max(-4, std::min(4, dragStartOct_ + delta));
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, oct, base.pitchSem, base.pitchFin, base.pitchCrs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaPitchSem:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const int delta = int((dragStartY_ - y) / 12.0f);
                    track->metaOsc.pitchSem = std::max(-12, std::min(12, dragStartSem_ + delta));
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const int delta = int((dragStartY_ - y) / 12.0f);
                    const int sem = std::max(-12, std::min(12, dragStartSem_ + delta));
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, base.pitchOct, sem, base.pitchFin, base.pitchCrs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaPitchFin:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const float delta = (dragStartY_ - y) * 0.6f;
                    track->metaOsc.pitchFin = clampf(dragStartFin_ + delta, -100.0f, 100.0f);
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const float delta = (dragStartY_ - y) * 0.6f;
                    const float fin = clampf(dragStartFin_ + delta, -100.0f, 100.0f);
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, base.pitchOct, base.pitchSem, fin, base.pitchCrs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaPitchCrs:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const float delta = (dragStartY_ - y) * 0.1f;
                    track->metaOsc.pitchCrs = clampf(dragStartCrs_ + delta, -100.0f, 100.0f);
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const float delta = (dragStartY_ - y) * 0.1f;
                    const float crs = clampf(dragStartCrs_ + delta, -100.0f, 100.0f);
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, base.pitchOct, base.pitchSem, base.pitchFin, crs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaFrameScan:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    selectedMetaFrame_ = clampi(int(normIn(metaFrameStripRect_) * float(std::max(1, seed.frameCount))),
                                                0, std::max(0, seed.frameCount - 1));
                    seed.morph = seed.frameCount > 1 ? float(selectedMetaFrame_) / float(seed.frameCount - 1) : 0.0f;
                    pushCurrentTrack();
                }
                else
                {
                    selectedMetaFrame_ = clampi(int(normIn(metaFrameStripRect_) * float(std::max(1, metaSlot.frameCount))),
                                                0, std::max(0, metaSlot.frameCount - 1));
                    metaSlot.morph = metaSlot.frameCount > 1
                                         ? float(selectedMetaFrame_) / float(metaSlot.frameCount - 1)
                                         : 0.0f;
                    pushMetaPartialRuntime();
                }
                break;
            case DragTarget::PartialTableAmp:
                editPartialTable(x, y, false);
                break;
            case DragTarget::PartialTablePhase:
                editPartialTable(x, y, true);
                break;
            case DragTarget::MetaHarmonicRatio:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    selectedPartialIndex_ = clampi(int(std::round(knobNorm()
                                                         * float(synth::kMaxWavetablePartials - 1))),
                                                   0, synth::kMaxWavetablePartials - 1);
                    selectedMetaHarmonic_ = selectedPartialIndex_;
                    break;
                }
                if(harmonicEditorOpen_)
                {
                    selectedMetaHarmonic_ = clampi(int(std::round(knobNorm()
                                                         * float(synth::kEditableWavetableHarmonics - 1))),
                                                   0, synth::kEditableWavetableHarmonics - 1);
                    break;
                }
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    auto &h = track->metaOsc.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedMetaHarmonic_];
                    h.ratio = 0.01f + knobNorm() * (float(synth::kEditableWavetableHarmonics) - 0.01f);
                    pushCurrentTrack();
                }
                else { metaHarmonic.ratio = 0.01f + knobNorm() * (float(synth::kEditableWavetableHarmonics) - 0.01f); pushMetaPartial(); }
                break;
            case DragTarget::MetaHarmonicAmp:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    auto &h = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_];
                    h.ratio = float(selectedPartialIndex_ + 1);
                    h.amp = knobNorm();
                    seed.partials[(size_t)selectedPartialIndex_].amp = h.amp;
                    pushCurrentTrack();
                    break;
                }
                if(harmonicEditorOpen_)
                {
                    editSelectedSpectrumControl(knobNorm(), false);
                    break;
                }
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    track->metaOsc.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedMetaHarmonic_].amp = knobNorm();
                    pushCurrentTrack();
                }
                else { metaHarmonic.amp = knobNorm(); pushMetaPartial(); }
                break;
            case DragTarget::MetaHarmonicPhase:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    auto &h = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_];
                    h.ratio = float(selectedPartialIndex_ + 1);
                    h.phase = knobNorm() * 2.0f * kPi - kPi;
                    pushCurrentTrack();
                    break;
                }
                if(harmonicEditorOpen_)
                {
                    editSelectedSpectrumControl(knobNorm(), true);
                    break;
                }
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    track->metaOsc.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedMetaHarmonic_].phase =
                        knobNorm() * 2.0f * kPi - kPi;
                    pushCurrentTrack();
                }
                else { metaHarmonic.phase = knobNorm() * 2.0f * kPi - kPi; pushMetaPartial(); }
                break;
            case DragTarget::HarmonicEditor: editHarmonicEditor(x, y); break;
            case DragTarget::MetaTimeEditor:
            case DragTarget::MetaSpectrumEditor: editMetaDomain(x, y); break;
            case DragTarget::MetaFrameScroll:
                if(auto *t2 = currentTrack(); t2 != nullptr
                   && (t2->type == synth::SourceTrackType::MetaOscillator
                       || t2->type == synth::SourceTrackType::PartialBank))
                {
                    const int frameCount = t2->type == synth::SourceTrackType::PartialBank
                                               ? t2->partialBank.frameCount
                                               : t2->metaOsc.frameCount;
                    const int maxScroll = std::max(0, frameCount - synth::kVisibleWavetableFrames);
                    if(maxScroll > 0)
                    {
                        const float pixPerStep = std::max(1.0f, metaFrameScrollRect_.w / float(maxScroll));
                        const int delta = int((x - dragScrollStartX_) / pixPerStep + 0.5f);
                        metaFrameScrollStart_ = clampi(dragScrollStartVal_ + delta, 0, maxScroll);
                    }
                }
                break;
            case DragTarget::LfoFreq: lfo.frequencyHz = knobNorm() * 20.0f; pushLfoOnly(); break;
            case DragTarget::ModEnvRate: env.loopRateHz = std::max(0.05f, knobNorm() * 20.0f); pushEnvOnly(); break;
            case DragTarget::LfoPhase: lfo.phase0 = knobNorm(); pushLfoOnly(); break;
            case DragTarget::LfoRho: lfo.rhoLfo = knobNorm(); pushLfoOnly(); break;
            case DragTarget::EnvPointA: env.points[1].y = knobNorm(); pushEnvOnly(); break;
            case DragTarget::EnvPointB: env.points[2].y = knobNorm(); pushEnvOnly(); break;
            case DragTarget::EnvCurveA: env.points[1].curve = knobNorm() * 2.0f - 1.0f; pushEnvOnly(); break;
            case DragTarget::MatrixEnvCurve: editMatrixEnvCurve(x, y); break;
            case DragTarget::MatrixEnvSeg:
            {
                auto *pts = curCurvePoints();
                if(envDragSeg_ >= 0 && envDragSeg_ < curCurveCount())
                {
                    pts[envDragSeg_].curve = clampf(dragStartDepth_ + (dragStartY_ - y) * uiRenderScale_ / 90.0f, -1.0f, 1.0f);
                    matrixEnvDirty_ = true;
                }
                break;
            }
            case DragTarget::RuleDepth: rule.depth = knobNorm() * 24.0f - 12.0f; pushRuleOnly(); break;
            case DragTarget::ModDepth:
                rule.depth = clampf(dragStartDepth_ + (dragStartY_ - y) * dragDepthLimit_ / 80.0f,
                                    -dragDepthLimit_, dragDepthLimit_);
                pushRuleOnly();
                break;
            case DragTarget::RuleBandLo: rule.bandLo = clampi(int(knobNorm() * synth::kMaxPartials), 0, rule.bandHi - 1); pushRuleOnly(); break;
            case DragTarget::RuleBandHi: rule.bandHi = clampi(int(knobNorm() * synth::kMaxPartials), rule.bandLo + 1, synth::kMaxPartials); pushRuleOnly(); break;
            case DragTarget::ChaosRate: chaos_.frequencyHz = knobNorm() * 60.0f; pushChaosOnly(); break;
            case DragTarget::ChaosAmount: chaos_.amount = knobNorm(); pushChaosOnly(); break;
            case DragTarget::ShapePhase: shape_.phase0 = knobNorm(); pushShapeOnly(); break;
            case DragTarget::ShapeRho: shape_.rho = knobNorm(); pushShapeOnly(); break;
            case DragTarget::ShapeUp: shape_.pUp = 0.1f + knobNorm() * 7.9f; pushShapeOnly(); break;
            case DragTarget::ShapeDown: shape_.pDown = 0.1f + knobNorm() * 7.9f; pushShapeOnly(); break;
            case DragTarget::EqLow: effects_.eq.lowGainDb = knobNorm() * 48.0f - 24.0f; pushEffects(); break;
            case DragTarget::EqMid: effects_.eq.midGainDb = knobNorm() * 48.0f - 24.0f; pushEffects(); break;
            case DragTarget::EqHigh: effects_.eq.highGainDb = knobNorm() * 48.0f - 24.0f; pushEffects(); break;
            case DragTarget::EqDrive: effects_.eq.drive = 0.1f + knobNorm() * 5.9f; pushEffects(); break;
            case DragTarget::FilterCutoff: effects_.filter.cutoffHz = normToCutoff(knobNorm()); pushEffects(); break;
            case DragTarget::FilterResonance: effects_.filter.resonance = knobNorm(); pushEffects(); break;
            case DragTarget::FilterDrive: effects_.filter.drive = 0.1f + knobNorm() * 5.9f; pushEffects(); break;
            case DragTarget::UiScale: uiScale_ = 0.75f + knobNorm() * 0.75f; break;
            case DragTarget::OpParamA: applyOperatorDrag(op, 0, normIn(opParamARect_)); break;
            case DragTarget::OpParamB: applyOperatorDrag(op, 1, normIn(opParamBRect_)); break;
            case DragTarget::OpParamC: applyOperatorDrag(op, 2, normIn(opParamCRect_)); break;
            case DragTarget::OpParamD: applyOperatorDrag(op, 3, normIn(opParamDRect_)); break;
            case DragTarget::FxInsertKnob:
                if(auto *chain = insertChainFor(fxDragHit_.trackId, fxDragHit_.groupIdx);
                   chain != nullptr && fxDragHit_.insertIdx < int(chain->size()))
                {
                    fxKnobSetNorm((*chain)[(size_t)fxDragHit_.insertIdx], fxDragHit_.knob, knobNorm());
                    commitChainChange(fxDragHit_.trackId, fxDragHit_.groupIdx);
                }
                break;
            case DragTarget::ModEntryDepth:
                if(auto *t = currentTrack(); t != nullptr && selectedModSlot_ >= 0 && selectedModSlot_ < synth::kMaxTrackMods
                   && modEntryActive(t->mods[(size_t)selectedModSlot_]))
                {
                    t->mods[(size_t)selectedModSlot_].depth = normIn(modDepthRects_[(size_t)selectedModSlot_]);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::StripScroll:
            {
                const int totalTracks = int(generator_.tracks.size());
                const int totalCols   = totalTracks + int(stripGroups_.size());
                const float sbW       = std::max(1.0f, stripScrollbarRect_.w);
                constexpr float kMinStripW = 90.0f;
                const float gap2 = 6.0f;
                const float panelW = clampf(stripScrollbarRect_.w + 28.0f, kMinStripW, 4096.0f);
                const int maxVis2 = std::max(1, int((panelW - 28.0f + gap2) / (kMinStripW + gap2)));
                const int totalScroll2 = std::max(0, totalCols - maxVis2);
                if(totalScroll2 > 0)
                {
                    const float usableW = std::max(1.0f, sbW - std::max(16.0f, sbW * float(maxVis2) / float(totalCols)));
                    const float delta = ((x - dragScrollStartX_) / usableW) * float(totalScroll2);
                    stripScrollF_ = clampf(dragScrollStartValF_ + delta, 0.0f, float(totalScroll2));
                }
                break;
            }
            case DragTarget::LayoutVSplit:
            {
                const float pageH = static_cast<float>(uiH()) - 184.0f;
                const float delta = (dragStartY_ - y) / std::max(1.0f, pageH);
                layoutBottomRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.18f, 0.72f);
                break;
            }
            case DragTarget::LayoutRackSplit:
            {
                const float pageW = static_cast<float>(uiW()) - 32.0f;
                const float delta = (x - dragStartY_) / std::max(1.0f, pageW);
                layoutRackRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.11f, 0.38f);
                break;
            }
            case DragTarget::LayoutStripSplit:
            {
                const float pageW = static_cast<float>(uiW()) - 32.0f;
                const float delta = (dragStartY_ - x) / std::max(1.0f, pageW);
                layoutStripRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.13f, 0.42f);
                break;
            }
            case DragTarget::None: break;
        }
    }

    void applyOperatorDrag(synth::OperatorBase &op, int param, float norm)
    {
        switch(op.type)
        {
            case synth::OperatorType::PartialMask:
                if(param == 0) op.maskLow = clampi(int(norm * synth::kMaxPartials), 0, op.maskHigh - 1);
                if(param == 1) op.maskHigh = clampi(int(norm * synth::kMaxPartials), op.maskLow + 1, synth::kMaxPartials);
                break;
            case synth::OperatorType::AmpScalePerGroup:
                if(param == 0) op.gainLow = norm * 2.0f;
                if(param == 1) op.gainMid = norm * 2.0f;
                if(param == 2) op.gainHigh = norm * 2.0f;
                break;
            case synth::OperatorType::FrequencyJitter:
                if(param == 0) op.jitterAmount = norm;
                if(param == 1) op.jitterSeed = uint32_t(std::round(norm * 999.0f));
                break;
            case synth::OperatorType::SpectralTilt:
                if(param == 0) op.extraTilt = norm * 4.0f - 2.0f;
                break;
            case synth::OperatorType::HarmonicLock:
                if(param == 0) op.lockAmount = norm;
                break;
        }
        pushOperator();
    }

    void applyFramePreset(int preset)
    {
        auto *track = currentTrack();
        if(track != nullptr && track->type != synth::SourceTrackType::MetaOscillator)
            return;
        auto &slot = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                         ? track->metaOsc
                         : generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        slot.frameCount = std::max(slot.frameCount, selectedMetaFrame_ + 1);
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, synth::kMaxWavetableFrames - 1);
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        frame.useImportedWaveform = false;
        frame.waveform.reset();
        frame.spectrum.reset();
        for(auto &h : frame.harmonics)
            h = {};

        const auto wrapPhase = [](float phase) {
            while(phase > kPi)
                phase -= 2.0f * kPi;
            while(phase < -kPi)
                phase += 2.0f * kPi;
            return phase;
        };
        const auto setH = [&](int index, float amp, float phase = 0.0f) {
            if(index < 0 || index >= synth::kMaxWavetableHarmonics)
                return;
            auto &h = frame.harmonics[(size_t)index];
            h.ratio = float(index + 1);
            h.amp = clampf(std::abs(amp), 0.0f, 1.0f);
            h.phase = wrapPhase(amp < 0.0f ? phase + kPi : phase);
        };

        switch(preset)
        {
            case 0: // sine
                setH(0, 1.0f);
                break;
            case 1: // saw
                for(int h = 0; h < synth::kMaxWavetableHarmonics; ++h)
                    setH(h, 1.0f / float(h + 1));
                break;
            case 2: // square
                for(int h = 0; h < synth::kMaxWavetableHarmonics; h += 2)
                    setH(h, 1.0f / float(h + 1));
                break;
            case 3: // triangle
                for(int h = 0; h < synth::kMaxWavetableHarmonics; h += 2)
                {
                    const int n = h + 1;
                    const float sign = ((n - 1) / 2) & 1 ? -1.0f : 1.0f;
                    setH(h, sign / float(n * n));
                }
                break;
            default:
                break;
        }
        synth::materializeWavetableFrame(frame);
        selectedMetaHarmonic_ = 0;
        if(track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
            pushCurrentTrack();
        else
            pushMetaPartial();
    }

    // ---- Modulator point-curve editor helpers ----
    // The MODULATORS graph edits whichever slot is selected: an ENV (envs_) or a
    // custom-curve LFO (lfos_). These accessors resolve to the active slot's curve.
    bool curCurveIsLfo() const { return selectedMatrixModSlot_ < synth::kMaxLfos; }
    synth::MatrixEnvPoint *curCurvePoints()
    {
        if(curCurveIsLfo())
            return lfos_[(size_t)clampi(selectedMatrixModSlot_, 0, synth::kMaxLfos - 1)].points.data();
        return envs_[(size_t)clampi(selectedMatrixModSlot_ - synth::kMaxLfos, 0, synth::kMaxModEnvs - 1)].points.data();
    }
    int &curCurveCount()
    {
        if(curCurveIsLfo())
            return lfos_[(size_t)clampi(selectedMatrixModSlot_, 0, synth::kMaxLfos - 1)].pointCount;
        return envs_[(size_t)clampi(selectedMatrixModSlot_ - synth::kMaxLfos, 0, synth::kMaxModEnvs - 1)].pointCount;
    }
    void pushCurCurve()
    {
        if(curCurveIsLfo()) pushLfoOnly();
        else                pushEnvOnly();
    }
    static float snapEnvValue(float v, bool isX)
    {
        const float step = isX ? (1.0f / 16.0f) : (1.0f / 8.0f);
        return clampf(std::round(v / step) * step, 0.0f, 1.0f);
    }
    float matrixEnvPx(float nx) const
    {
        return matrixEnvCurveRect_.x + 8.0f + clampf(nx, 0.0f, 1.0f) * (matrixEnvCurveRect_.w - 16.0f);
    }
    float matrixEnvPy(float ny) const
    {
        return matrixEnvCurveRect_.y + matrixEnvCurveRect_.h - 5.0f - clampf(ny, 0.0f, 1.0f) * (matrixEnvCurveRect_.h - 10.0f);
    }
    float matrixEnvNx(float x) const
    {
        return clampf((x - (matrixEnvCurveRect_.x + 8.0f)) / std::max(1.0f, matrixEnvCurveRect_.w - 16.0f), 0.0f, 1.0f);
    }
    float matrixEnvNy(float y) const
    {
        return clampf(1.0f - (y - (matrixEnvCurveRect_.y + 5.0f)) / std::max(1.0f, matrixEnvCurveRect_.h - 10.0f), 0.0f, 1.0f);
    }
    int matrixEnvPointAt(float x, float y)
    {
        const auto *pts = curCurvePoints();
        const int count = clampi(curCurveCount(), 2, synth::kMaxMatrixEnvPoints);
        for(int i = 0; i < count; ++i)
            if(std::hypot(x - matrixEnvPx(pts[i].x), y - matrixEnvPy(pts[i].y)) <= 8.0f)
                return i;
        return -1;
    }
    int matrixEnvSegmentAt(float x)
    {
        const auto *pts = curCurvePoints();
        const int count = clampi(curCurveCount(), 2, synth::kMaxMatrixEnvPoints);
        const float nx = matrixEnvNx(x);
        for(int i = 0; i + 1 < count; ++i)
            if(nx <= pts[i + 1].x || i + 2 == count)
                return i;
        return -1;
    }
    void addMatrixEnvPoint(float x, float y)
    {
        auto *pts = curCurvePoints();
        int &countRef = curCurveCount();
        int count = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
        if(count >= synth::kMaxMatrixEnvPoints)
            return;
        float nx = matrixEnvNx(x);
        float ny = matrixEnvNy(y);
        if(!shiftDown_) { nx = snapEnvValue(nx, true); ny = snapEnvValue(ny, false); }
        int idx = 1;
        while(idx < count && pts[idx].x < nx)
            ++idx;
        idx = clampi(idx, 1, count - 1);
        for(int i = count; i > idx; --i)
            pts[i] = pts[i - 1];
        pts[idx] = synth::MatrixEnvPoint { nx, ny, 0.0f };
        countRef = count + 1;
        selectedEnvPoint_ = idx;
    }
    void deleteMatrixEnvPoint(int idx)
    {
        auto *pts = curCurvePoints();
        int &countRef = curCurveCount();
        int count = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
        if(idx <= 0 || idx >= count - 1 || count <= 2)
            return;  // keep the two endpoints
        for(int i = idx; i < count - 1; ++i)
            pts[i] = pts[i + 1];
        countRef = count - 1;
        selectedEnvPoint_ = -1;
    }

    void editMatrixEnvCurve(float x, float y)
    {
        auto *pts = curCurvePoints();
        int &countRef = curCurveCount();
        countRef = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
        if(selectedEnvPoint_ < 0 || selectedEnvPoint_ >= countRef)
            return;  // only an explicitly-grabbed point moves (no teleport)
        float nx = matrixEnvNx(x);
        float ny = matrixEnvNy(y);
        if(!shiftDown_) { nx = snapEnvValue(nx, true); ny = snapEnvValue(ny, false); }

        auto &pt = pts[selectedEnvPoint_];
        if(selectedEnvPoint_ == 0)
        {
            pt.x = 0.0f;
            pt.y = ny;
        }
        else if(selectedEnvPoint_ == countRef - 1)
        {
            pt.x = 1.0f;
            pt.y = ny;
        }
        else
        {
            const float lo = pts[selectedEnvPoint_ - 1].x + 0.01f;
            const float hi = pts[selectedEnvPoint_ + 1].x - 0.01f;
            pt.x = clampf(nx, lo, hi);
            pt.y = ny;
        }
        matrixEnvDirty_ = true; // published on mouse release, not every motion
    }

    void editPartialTable(float x, float y, bool phaseMode)
    {
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::PartialBank)
            return;
        auto &seed = track->partialBank;
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
        ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
        Rect rect = phaseMode ? harmonicEditorPhaseRect_ : harmonicEditorSpectrumRect_;
        selectedPartialIndex_ = clampi(int((x - rect.x) / std::max(1.0f, rect.w)
                                           * float(synth::kMaxWavetablePartials)),
                                      0, synth::kMaxWavetablePartials - 1);
        selectedMetaHarmonic_ = selectedPartialIndex_;
        auto &frame = seed.frames[(size_t)selectedMetaFrame_];
        auto &h = frame.harmonics[(size_t)selectedPartialIndex_];
        h.ratio = float(selectedPartialIndex_ + 1);
        if(phaseMode)
        {
            const float normY = clampf((y - rect.y) / std::max(1.0f, rect.h), 0.0f, 1.0f);
            h.phase = (0.5f - normY) * 2.0f * kPi;
        }
        else
        {
            h.amp = clampf(1.0f - (y - rect.y) / std::max(1.0f, rect.h), 0.0f, 1.0f);
            seed.partials[(size_t)selectedPartialIndex_].amp = h.amp;
        }
        pushCurrentTrack();
    }

    void editHarmonicEditor(float x, float y)
    {
        auto *track = currentTrack();
        if(track != nullptr && track->type != synth::SourceTrackType::MetaOscillator)
            return;
        auto &slot = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                         ? track->metaOsc
                         : generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        frame.useImportedWaveform = false;
        frame.waveform.reset();
        frame.spectrum.reset();
        selectedMetaHarmonic_ = clampi(int((x - harmonicEditorBarsRect_.x) / std::max(1.0f, harmonicEditorBarsRect_.w)
                                           * float(synth::kEditableWavetableHarmonics)),
                                      0, synth::kEditableWavetableHarmonics - 1);
        auto &h = frame.harmonics[(size_t)selectedMetaHarmonic_];
        h.ratio = float(selectedMetaHarmonic_ + 1);
        h.amp = clampf(1.0f - (y - harmonicEditorBarsRect_.y) / std::max(1.0f, harmonicEditorBarsRect_.h), 0.0f, 1.0f);
        if(track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
            pushCurrentTrack();
        else
            pushMetaPartial();
    }

    bool performMetaFrameAction(int action)
    {
        auto *track = currentTrack();
        if(track != nullptr && track->type == synth::SourceTrackType::PartialBank)
        {
            auto &seed = track->partialBank;
            seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
            selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
            synth::WavetablePartialSlot slot;
            slot.frameCount = seed.frameCount;
            slot.morph = seed.morph;
            slot.frames = seed.frames;

            if(action == 2)
            {
                int selectedCount = 0;
                for(int i = 0; i < slot.frameCount; ++i)
                    selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
                if(selectedCount == 0)
                {
                    metaEditorStatus_ = "select frames to delete";
                    return true;
                }
                if(slot.frameCount <= 1)
                {
                    metaEditorStatus_ = "one frame required";
                    return true;
                }
                const int deleted = synth::deleteSelectedWavetableFrames(
                    slot, metaFrameSelected_.data(), slot.frameCount);
                seed.frameCount = slot.frameCount;
                seed.frames = slot.frames;
                seed.morph = slot.morph;
                selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
                metaFrameSelected_.fill(false);
                metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
                metaFrameRangeAnchor_ = selectedMetaFrame_;
                metaEditorStatus_ = deleted > 0 ? "selected partial frames deleted" : "no frames deleted";
                pushCurrentTrack();
                return true;
            }

            bool changed = false;
            switch(action)
            {
                case 0:
                    changed = synth::addWavetableFrame(slot, selectedMetaFrame_);
                    if(changed) ++selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "blank partial frame added" : "frame limit reached";
                    break;
                case 1:
                    changed = synth::duplicateWavetableFrame(slot, selectedMetaFrame_);
                    if(changed) ++selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "partial frame duplicated" : "frame limit reached";
                    break;
                case 3:
                    changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ - 1);
                    if(changed) --selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "partial frame moved left" : "already first frame";
                    break;
                case 4:
                    changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ + 1);
                    if(changed) ++selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "partial frame moved right" : "already last frame";
                    break;
                default:
                    break;
            }
            if(changed)
            {
                seed.frameCount = slot.frameCount;
                seed.frames = slot.frames;
                seed.morph = slot.morph;
                seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                metaFrameSelected_.fill(false);
                metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
                metaFrameRangeAnchor_ = selectedMetaFrame_;
                pushCurrentTrack();
            }
            return true;
        }
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return true;
        auto &slot = track->metaOsc;
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));

        if(action == 2)
        {
            int selectedCount = 0;
            for(int i = 0; i < slot.frameCount; ++i)
                selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
            if(selectedCount == 0)
            {
                metaEditorStatus_ = "select frames to delete";
                return true;
            }
            if(slot.frameCount <= 1)
            {
                metaEditorStatus_ = "one frame required";
                return true;
            }

            pushMetaUndoSnapshot();
            const int deleted = synth::deleteSelectedWavetableFrames(
                slot, metaFrameSelected_.data(), slot.frameCount);
            selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
            metaFrameSelected_.fill(false);
            metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
            metaFrameRangeAnchor_ = selectedMetaFrame_;
            metaEditorStatus_ = deleted < selectedCount
                                  ? "selected frames deleted; one frame retained"
                                  : "selected frames deleted";
            if(deleted > 0)
                pushCurrentTrack();
            return true;
        }

        pushMetaUndoSnapshot();
        bool changed = false;
        switch(action)
        {
            case 0:
                changed = synth::addWavetableFrame(slot, selectedMetaFrame_);
                if(changed) ++selectedMetaFrame_;
                metaEditorStatus_ = changed ? "blank frame added" : "frame limit reached";
                break;
            case 1:
                changed = synth::duplicateWavetableFrame(slot, selectedMetaFrame_);
                if(changed) ++selectedMetaFrame_;
                metaEditorStatus_ = changed ? "frame duplicated" : "frame limit reached";
                break;
            case 3:
                changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ - 1);
                if(changed) --selectedMetaFrame_;
                metaEditorStatus_ = changed ? "frame moved left" : "already first";
                break;
            case 4:
                changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ + 1);
                if(changed) ++selectedMetaFrame_;
                metaEditorStatus_ = changed ? "frame moved right" : "already last";
                break;
            case 5:
            {
                const int reference = selectedMetaFrame_ > 0 ? selectedMetaFrame_ - 1 : 1;
                changed = slot.frameCount > 1
                       && synth::alignWavetableFramePhase(slot, selectedMetaFrame_, reference);
                metaEditorStatus_ = changed ? "phase aligned" : "needs another frame";
                break;
            }
            case 6:
            case 7:
                if(selectedMetaFrame_ > 0 && selectedMetaFrame_ + 1 < slot.frameCount)
                    changed = synth::morphWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ - 1,
                                                         selectedMetaFrame_ + 1, 0.5f,
                                                         action == 6 ? synth::WavetableMorphMode::Linear
                                                                     : synth::WavetableMorphMode::Spectral);
                metaEditorStatus_ = changed ? (action == 6 ? "linear midpoint" : "spectral midpoint")
                                            : "select an interior frame";
                break;
            default:
                break;
        }
        if(changed)
        {
            metaFrameSelected_.fill(false);
            metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
            metaFrameRangeAnchor_ = selectedMetaFrame_;
            pushCurrentTrack();
        }
        return true;
    }

    // 手动波表处理操作（0=DC/Norm 1=Crossfade 2=ZeroAlign 3=PhaseAlign 4=EnergySmooth）
    bool applyWavetableProcess(int op)
    {
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return true;
        auto &slot = track->metaOsc;

        int selectedCount = 0;
        for(int i = 0; i < slot.frameCount; ++i)
            selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
        if(selectedCount == 0)
        {
            metaEditorStatus_ = "select frames to process";
            return true;
        }
        pushMetaUndoSnapshot();
        const bool *sel = metaFrameSelected_.data();

        switch(op)
        {
            case 0:
                synth::processWavetableRemoveDC(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: DC removed + RMS normalized";
                break;
            case 1:
                synth::processWavetableCrossfade(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: crossfade applied";
                break;
            case 2:
                synth::processWavetableZeroAlign(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: zero-crossing aligned";
                break;
            case 3:
                synth::processWavetableAlignPhases(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: phases aligned";
                break;
            case 4:
                synth::processWavetableEnergySmooth(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: energy smoothed";
                break;
            default: break;
        }
        pushCurrentTrack();
        return true;
    }

    bool applySelectedFrameMorph(int targetFrameCount)
    {
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return false;

        int selectedCount = 0;
        for(int i = 0; i < track->metaOsc.frameCount; ++i)
            selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
        if(selectedCount < 2)
        {
            metaEditorStatus_ = "select at least two frames to morph";
            return true;
        }

        pushMetaUndoSnapshot();
        if(!synth::expandSelectedWavetableFrames(track->metaOsc, metaFrameSelected_.data(),
                                                 selectedCount, targetFrameCount))
        {
            if(!metaUndoStack_.empty())
                metaUndoStack_.pop_back();
            metaEditorStatus_ = "morph expansion failed";
            return true;
        }

        selectedMetaFrame_ = 0;
        metaFramePageStart_ = 0;
        metaFrameScrollStart_ = 0;
        metaFrameSelected_.fill(false);
        metaFrameSelected_[0] = true;
        metaFrameRangeAnchor_ = 0;
        metaEditorStatus_ = targetFrameCount == 256 ? "selection morphed to 256 frames"
                                                     : "selection morphed to 512 frames";
        pushCurrentTrack();
        return true;
    }

    void editMetaDomain(float x, float y)
    {
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        auto &frame = track->metaOsc.frames[(size_t)selectedMetaFrame_];
        synth::materializeWavetableFrame(frame);
        if(dragTarget_ == DragTarget::MetaTimeEditor)
        {
            const Rect &wr = harmonicEditorBarsRect_;
            if(!frame.waveform)
                return;
            auto wave = std::make_shared<std::array<float, synth::kWavetableSize>>(*frame.waveform);

            // 用上一次事件位置做线段插值，消除快速拖动时的锯齿空白
            const float px0 = prevTimeEditX_ >= 0.0f ? prevTimeEditX_ : x;
            const float py0 = prevTimeEditY_ >= 0.0f ? prevTimeEditY_ : y;
            const int s0 = clampi(int((px0 - wr.x) / std::max(1.0f, wr.w) * float(synth::kWavetableSize)),
                                  0, synth::kWavetableSize - 1);
            const int s1 = clampi(int((x   - wr.x) / std::max(1.0f, wr.w) * float(synth::kWavetableSize)),
                                  0, synth::kWavetableSize - 1);
            const float v0 = clampf(1.0f - 2.0f * (py0 - wr.y) / std::max(1.0f, wr.h), -1.0f, 1.0f);
            const float v1 = clampf(1.0f - 2.0f * (y   - wr.y) / std::max(1.0f, wr.h), -1.0f, 1.0f);
            const int lo = std::min(s0, s1), hi = std::max(s0, s1);
            for(int s = lo; s <= hi; ++s)
            {
                const float t = (hi > lo) ? float(s - lo) / float(hi - lo) : 0.5f;
                const float val = (s0 <= s1) ? (v0 + (v1 - v0) * t) : (v1 + (v0 - v1) * t);
                (*wave)[(size_t)s] = val;
            }
            prevTimeEditX_ = x;
            prevTimeEditY_ = y;
            frame.waveform = std::move(wave);
            frame.useImportedWaveform = true;
            synth::analyzeWavetableFrame(frame);
            metaEditorStatus_ = "time edited";
        }
        else
        {
            const Rect &sr = harmonicEditorSpectrumRect_;
            selectedMetaHarmonic_ = clampi(int((x - sr.x) / std::max(1.0f, sr.w)
                                               * float(synth::kEditableWavetableHarmonics)),
                                           0, synth::kEditableWavetableHarmonics - 1);
            if(!frame.spectrum)
                synth::analyzeWavetableFrame(frame);
            if(!frame.spectrum)
                return;
            auto spectrum = std::make_shared<synth::WavetableFrame::Spectrum>(*frame.spectrum);
            const size_t bin = (size_t)(selectedMetaHarmonic_ + 1);
            const float amplitude = clampf(1.0f - (y - sr.y) / std::max(1.0f, sr.h), 0.0f, 1.0f);
            const float phase = std::arg((*spectrum)[bin]);
            (*spectrum)[bin] = std::polar(amplitude * float(synth::kWavetableSize) * 0.5f, phase);
            frame.spectrum = std::move(spectrum);
            synth::rebuildWavetableFrameFromSpectrum(frame);
            metaEditorStatus_ = "spectrum edited";
        }
        metaEditorDirty_ = true;
    }

    void editSelectedSpectrumControl(float normalized, bool phaseControl)
    {
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        auto &frame = track->metaOsc.frames[(size_t)selectedMetaFrame_];
        synth::materializeWavetableFrame(frame);
        if(!frame.spectrum)
            return;
        auto spectrum = std::make_shared<synth::WavetableFrame::Spectrum>(*frame.spectrum);
        const size_t bin = (size_t)(clampi(selectedMetaHarmonic_, 0,
                                           synth::kEditableWavetableHarmonics - 1) + 1);
        const float amplitude = phaseControl ? std::abs((*spectrum)[bin])
                                             : clampf(normalized, 0.0f, 1.0f)
                                                   * float(synth::kWavetableSize) * 0.5f;
        const float phase = phaseControl ? clampf(normalized, 0.0f, 1.0f) * 2.0f * kPi - kPi
                                         : std::arg((*spectrum)[bin]);
        (*spectrum)[bin] = std::polar(amplitude, phase);
        frame.spectrum = std::move(spectrum);
        synth::rebuildWavetableFrameFromSpectrum(frame);
        metaEditorDirty_ = true;
        metaEditorStatus_ = phaseControl ? "phase edited" : "amplitude edited";
    }

    void editMetaWaveform(float x, float y)
    {
        auto *track = currentTrack();
        auto &slot = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                         ? track->metaOsc
                         : generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        frame.useImportedWaveform = false;
        frame.waveform.reset();
        frame.spectrum.reset();
        selectedMetaHarmonic_ = clampi(int((x - metaWaveformRect_.x) / std::max(1.0f, metaWaveformRect_.w) *
                                           float(synth::kMaxWavetableHarmonics)),
                                      0, synth::kMaxWavetableHarmonics - 1);
        auto &harmonic = frame.harmonics[(size_t)selectedMetaHarmonic_];
        harmonic.ratio = std::max(0.01f, float(selectedMetaHarmonic_ + 1));
        harmonic.amp = clampf(1.0f - (y - metaWaveformRect_.y) / std::max(1.0f, metaWaveformRect_.h), 0.0f, 1.0f);
        if(track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
            pushCurrentTrack();
        else
            pushMetaPartial();
    }

    void beginWavetableImport()
    {
        droppedWavPending_ = false;
        wavetablePresetMenuOpen_ = false;
        wavetableImportMenuOpen_ = true;
    }

    void openWavetableFileBrowser()
    {
#if DISTRHO_UI_FILE_BROWSER
        FileBrowserOptions options;
        options.title = "Import wavetable WAV";
        if(!lastLoadPath_.empty())
        {
            const size_t slash = lastLoadPath_.find_last_of("/\\");
            browserStartDir_ = slash == std::string::npos ? std::string {} : lastLoadPath_.substr(0, slash);
            if(!browserStartDir_.empty())
                options.startDir = browserStartDir_.c_str();
        }
        if(openFileBrowser(options))
            return;
#endif
        loadPathEditing_ = true;
        if(loadPathBuffer_.empty())
            loadPathBuffer_ = lastLoadPath_;
    }

    bool commitWavetableLoad()
    {
        loadPathEditing_ = false;
        if(loadPathBuffer_.empty())
            return false;

        lastLoadPath_ = loadPathBuffer_;
        bool ok = false;
        // Kapibara native harmonic/phase wavetable round-trip.
        if(loadPathBuffer_.size() >= 4
           && loadPathBuffer_.compare(loadPathBuffer_.size() - 4, 4, ".kwt") == 0)
        {
            ok = loadWavetableHarmonicFile(loadPathBuffer_);
            if(ok)
                pullFromPlugin();
            return ok;
        }
        if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
        {
            pushMetaUndoSnapshot();
            synth::WavetableImportOptions options;
            options.mode = wavetableImportMode_;
            options.frameLength = synth::kWavetableSize;
            options.manualCycleLength = manualCycleLength_;
            options.maxFrames = importFrameLimit_;
            const auto result = synth::importWavetableFramesFromWav(loadPathBuffer_, track->metaOsc, 0, options);
            ok = result.success;
            if(ok)
            {
                selectedMetaFrame_ = 0;
                metaFrameSelected_.fill(false);
                metaFrameSelected_[0] = true;
                metaFrameRangeAnchor_ = 0;
                pushCurrentTrack();
                loadStatus_ = result.message;
            }
            else
                loadStatus_ = "load failed: " + result.message;
        }
        else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
        {
            (void)track;
            loadStatus_ = "PartialBank loads .kwt harmonic tables";
            ok = false;
        }
        else if(auto *p = plugin())
        {
            ok = p->loadWavetableFrame(selectedMetaPartial_, selectedMetaFrame_, loadPathBuffer_.c_str());
        }
        if(currentTrack() == nullptr || currentTrack()->type != synth::SourceTrackType::MetaOscillator)
            loadStatus_ = ok ? ("loaded " + loadPathBuffer_) : ("load failed: " + loadPathBuffer_);
        if(ok)
        {
            const size_t slash = loadPathBuffer_.find_last_of("/\\");
            const size_t nameStart = slash == std::string::npos ? 0 : slash + 1;
            const size_t dot = loadPathBuffer_.find_last_of('.');
            wavetablePresetLabel_ = loadPathBuffer_.substr(
                nameStart, dot == std::string::npos || dot < nameStart ? std::string::npos : dot - nameStart);
            pullFromPlugin();
            auto *track = currentTrack();
            auto &slot = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                             ? track->metaOsc
                             : generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
        }
        return ok;
    }

    // ---- IR convolution reverb: impulse files live in presets/irs (.wav) ----
    void refreshIrFiles()
    {
        irFiles_.clear();
        std::error_code ec;
        const std::filesystem::path dir("presets/irs");
        std::filesystem::create_directories(dir, ec);
        for(const auto &entry : std::filesystem::directory_iterator(dir, ec))
        {
            if(ec) break;
            if(!entry.is_regular_file(ec)) continue;
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return char(std::tolower(c)); });
            if(ext != ".wav") continue;
            irFiles_.push_back({ entry.path().stem().string(), entry.path().string() });
        }
        std::sort(irFiles_.begin(), irFiles_.end(),
                  [](const auto &a, const auto &b){ return a.first < b.first; });
    }

    void loadImpulseIntoInsert(InsertEffect &e, const std::string &name, const std::string &path)
    {
        std::vector<float> samples;
        uint32_t sr = 48000;
        if(!synth::loadImpulseResponseMono(path, samples, sr))
        {
            metaEditorStatus_ = "IR load failed: " + name;
            return;
        }
        const double engineSr = getSampleRate() > 1000.0 ? getSampleRate() : 48000.0;
        // hop=512 → ~11ms latency; cap IR at 3 seconds.
        e.conv.ir = synth::fx::buildConvIR(samples, 512, 3.0f, engineSr, name);
        e.conv.irName = name;
        metaEditorStatus_ = "IR loaded: " + name;
    }

    // Save the current Meta Oscillator's harmonic wavetable straight into the
    // presets/wavetables folder (no OS file dialog — those are unreliable under
    // some Wayland compositors). The saved .kwt then appears in the preset list.
    void openWavetableSaveBrowser()
    {
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank))
        {
            metaEditorStatus_ = "select Meta or PartialBank to save";
            return;
        }
        std::string dir = "presets/wavetables";
        if(auto *p = plugin())
            dir = p->wavetableUserDir();
        std::string base = wavetablePresetLabel_.empty() ? std::string("wavetable") : wavetablePresetLabel_;
        // Strip any directory part the label may carry.
        const size_t slash = base.find_last_of("/\\");
        if(slash != std::string::npos)
            base = base.substr(slash + 1);
        // Pick a unique filename so saves don't silently overwrite.
        std::error_code ec;
        std::string path = dir + "/" + base + ".kwt";
        int suffix = 2;
        while(std::filesystem::exists(path, ec))
            path = dir + "/" + base + "_" + std::to_string(suffix++) + ".kwt";
        saveWavetableToFile(path);
        refreshWavetablePresets();
    }

    void saveOrRenameWavetablePreset(const std::string &newStem)
    {
        std::string dir = "presets/wavetables";
        if(auto *p = plugin())
            dir = p->wavetableUserDir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const bool hasSelection = selectedWavetablePresetIndex_ >= 0
                                  && selectedWavetablePresetIndex_ < int(wavetablePresets_.size());
        if(hasSelection)
        {
            const auto oldPath = std::filesystem::path(wavetablePresets_[(size_t)selectedWavetablePresetIndex_].path);
            const auto ext = oldPath.extension().empty() ? std::filesystem::path(".kwt") : oldPath.extension();
            auto newPath = oldPath.parent_path() / (newStem + ext.string());
            if(newPath != oldPath)
            {
                int suffix = 2;
                while(std::filesystem::exists(newPath, ec))
                    newPath = oldPath.parent_path() / (newStem + "_" + std::to_string(suffix++) + ext.string());
                std::filesystem::rename(oldPath, newPath, ec);
                if(ec)
                {
                    metaEditorStatus_ = "rename failed: " + oldPath.string();
                    loadStatus_ = metaEditorStatus_;
                    return;
                }
                lastLoadPath_ = newPath.string();
            }
            wavetablePresetLabel_ = newStem;
            metaEditorStatus_ = "renamed wavetable: " + newStem;
            loadStatus_ = metaEditorStatus_;
            refreshWavetablePresets();
            for(int i = 0; i < int(wavetablePresets_.size()); ++i)
                if(wavetablePresets_[(size_t)i].name == newStem)
                    selectedWavetablePresetIndex_ = i;
            return;
        }

        std::filesystem::path path = std::filesystem::path(dir) / (newStem + ".kwt");
        int suffix = 2;
        while(std::filesystem::exists(path, ec))
            path = std::filesystem::path(dir) / (newStem + "_" + std::to_string(suffix++) + ".kwt");
        saveWavetableToFile(path.string());
        refreshWavetablePresets();
        for(int i = 0; i < int(wavetablePresets_.size()); ++i)
            if(wavetablePresets_[(size_t)i].path == path.string())
                selectedWavetablePresetIndex_ = i;
    }

    // Serializes the current Meta Oscillator's per-frame harmonics (ratio/amp/phase)
    // to a plain-text .kwt file. Not a WAV: only the additive spectrum is stored.
    void saveWavetableToFile(const std::string &path)
    {
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank))
        {
            metaEditorStatus_ = "select Meta or PartialBank to save";
            return;
        }
        const bool isBank = track->type == synth::SourceTrackType::PartialBank;
        const int frameCount = isBank
                                   ? clampi(track->partialBank.frameCount, 1, synth::kMaxWavetableFrames)
                                   : clampi(track->metaOsc.frameCount, 1, synth::kMaxWavetableFrames);
        const auto &frameStorage = isBank ? track->partialBank.frames : track->metaOsc.frames;
        const int harmonicLimit = isBank ? synth::kMaxWavetablePartials : synth::kMaxWavetableHarmonics;
        std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if(!out)
        {
            metaEditorStatus_ = "save failed: " + path;
            loadStatus_ = metaEditorStatus_;
            return;
        }
        Kwt2Header header;
        header.frameCount = uint32_t(frameCount);
        header.binCount = uint32_t(harmonicLimit);
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
        const auto &frames = frameStorage.get();
        for(int f = 0; f < frameCount; ++f)
        {
            const auto &fp = frames[(size_t)f];
            const synth::WavetableFrame *frame = fp ? fp.get() : nullptr;
            for(int h = 0; h < harmonicLimit; ++h)
            {
                const auto &hm = frame != nullptr ? frame->harmonics[(size_t)h] : synth::WavetableHarmonic {};
                const auto packed = packKwtBin(hm.amp, hm.phase);
                out.write(reinterpret_cast<const char *>(&packed), sizeof(packed));
            }
        }
        out.close();
        const size_t slash = path.find_last_of("/\\");
        lastLoadPath_ = path;
        wavetablePresetLabel_ = path.substr(slash == std::string::npos ? 0 : slash + 1);
        const size_t dot = wavetablePresetLabel_.find_last_of('.');
        if(dot != std::string::npos)
            wavetablePresetLabel_ = wavetablePresetLabel_.substr(0, dot);
        metaEditorStatus_ = "saved " + path;
        loadStatus_ = metaEditorStatus_;
    }

    // Loads a .kwt harmonic/phase wavetable into the current Meta Oscillator or
    // PartialBank. PartialBank uses only harmonics[0..63] as frame amp/phase.
    bool loadWavetableHarmonicFile(const std::string &path)
    {
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank))
            return false;
        const bool isBank = track->type == synth::SourceTrackType::PartialBank;
        {
            std::ifstream bin(path, std::ios::binary);
            Kwt2Header header;
            if(bin.read(reinterpret_cast<char *>(&header), sizeof(header))
               && std::memcmp(header.magic, "KWT2", 4) == 0)
            {
                const int frameCount = clampi(int(header.frameCount), 1, synth::kMaxWavetableFrames);
                const int binCount = clampi(int(header.binCount), 1, synth::kMaxWavetableHarmonics);
                if(!isBank)
                    pushMetaUndoSnapshot();
                auto &metaSlot = track->metaOsc;
                auto &bank = track->partialBank;
                if(isBank)
                {
                    bank.frameCount = frameCount;
                    bank.morph = 0.0f;
                    bank.partialCount = synth::kMaxWavetablePartials;
                    for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
                        bank.partials[(size_t)i].enabled = true;
                }
                else
                {
                    metaSlot.frameCount = frameCount;
                }
                auto &frames = isBank ? bank.frames.ensure() : metaSlot.frames.ensure();
                const int limit = isBank ? synth::kMaxWavetablePartials : synth::kMaxWavetableHarmonics;
                for(int f = 0; f < frameCount; ++f)
                {
                    if(!frames[(size_t)f])
                        frames[(size_t)f] = std::make_shared<synth::WavetableFrame>();
                    auto &frame = *frames[(size_t)f];
                    frame.useImportedWaveform = false;
                    frame.waveform.reset();
                    frame.spectrum.reset();
                    frame.harmonics.fill(synth::WavetableHarmonic {});
                    for(int b = 0; b < binCount; ++b)
                    {
                        Kwt2PackedBin packed;
                        if(!bin.read(reinterpret_cast<char *>(&packed), sizeof(packed)))
                        {
                            loadStatus_ = "bad KWT2: " + path;
                            return false;
                        }
                        if(b < limit)
                            frame.harmonics[(size_t)b] = unpackKwtBin(packed, b);
                    }
                }
                selectedMetaFrame_ = 0;
                metaFrameSelected_.fill(false);
                metaFrameSelected_[0] = true;
                metaFrameRangeAnchor_ = 0;
                pushCurrentTrack();
                loadStatus_ = "loaded " + path;
                return true;
            }
        }

        std::ifstream in(path);
        if(!in)
        {
            loadStatus_ = "load failed: " + path;
            return false;
        }
        std::string tag;
        int version = 0;
        in >> tag >> version;
        if(tag != "KAPIBARA_WT")
        {
            loadStatus_ = "not a Kapibara wavetable: " + path;
            return false;
        }
        std::string key;
        int frameCount = 1;
        in >> key >> frameCount;
        frameCount = clampi(frameCount, 1, synth::kMaxWavetableFrames);

        if(!isBank)
            pushMetaUndoSnapshot();
        auto &metaSlot = track->metaOsc;
        auto &bank = track->partialBank;
        if(isBank)
        {
            bank.frameCount = frameCount;
            bank.morph = 0.0f;
            bank.partialCount = synth::kMaxWavetablePartials;
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
                bank.partials[(size_t)i].enabled = true;
        }
        else
        {
            metaSlot.frameCount = frameCount;
        }
        auto &frames = isBank ? bank.frames.ensure() : metaSlot.frames.ensure();
        for(int f = 0; f < frameCount; ++f)
        {
            std::string ftag;
            int idx = 0, used = 0;
            in >> ftag >> idx >> used;
            if(!frames[(size_t)f])
                frames[(size_t)f] = std::make_shared<synth::WavetableFrame>();
            auto &frame = *frames[(size_t)f];
            frame.useImportedWaveform = false;
            frame.waveform.reset();
            frame.spectrum.reset();
            frame.harmonics.fill(synth::WavetableHarmonic {});
            used = clampi(used, 0, synth::kMaxWavetableHarmonics);
            for(int h = 0; h < used; ++h)
            {
                float ratio = 1.0f, amp = 0.0f, phase = 0.0f;
                in >> ratio >> amp >> phase;
                const int dst = isBank ? clampi(int(std::round(ratio)) - 1, 0, synth::kMaxWavetablePartials)
                                       : h;
                if(dst >= (isBank ? synth::kMaxWavetablePartials : synth::kMaxWavetableHarmonics))
                    continue;
                frame.harmonics[(size_t)dst].ratio = isBank ? float(dst + 1) : ratio;
                frame.harmonics[(size_t)dst].amp = amp;
                frame.harmonics[(size_t)dst].phase = phase;
            }
            if(isBank)
                for(int h = 0; h < synth::kMaxWavetablePartials; ++h)
                    if(frame.harmonics[(size_t)h].ratio <= 0.0f)
                        frame.harmonics[(size_t)h].ratio = float(h + 1);
        }
        selectedMetaFrame_ = 0;
        metaFrameSelected_.fill(false);
        metaFrameSelected_[0] = true;
        metaFrameRangeAnchor_ = 0;
        pushCurrentTrack();
        loadStatus_ = "loaded " + path;
        return true;
    }

    void ensureDefaultOperatorChain()
    {
        if(operatorChain_.ops.size() >= 5)
            return;

        const size_t oldSize = operatorChain_.ops.size();
        operatorChain_.ops.resize(5);
        for(size_t i = oldSize; i < operatorChain_.ops.size(); ++i)
        {
            auto &op = operatorChain_.ops[i];
            op.enabled = false;
            op.type = static_cast<synth::OperatorType>(std::min<int>(int(i), 4));
            op.maskLow = 0;
            op.maskHigh = synth::kMaxPartials;
            op.maskGroupLow = true;
            op.maskGroupMid = true;
            op.maskGroupHigh = true;
            op.gainLow = 1.0f;
            op.gainMid = 1.0f;
            op.gainHigh = 1.0f;
            op.jitterAmount = 0.0f;
            op.jitterSeed = 7u + uint32_t(i);
            op.extraTilt = 0.0f;
            op.lockAmount = 0.0f;
        }
        if(oldSize == 0)
            pushOperator();
    }

    float cutoffToNorm(float hz) const
    {
        const float lo = std::log(20.0f);
        const float hi = std::log(20000.0f);
        return clampf((std::log(clampf(hz, 20.0f, 20000.0f)) - lo) / (hi - lo), 0.0f, 1.0f);
    }

    float normToCutoff(float norm) const
    {
        const float lo = std::log(20.0f);
        const float hi = std::log(20000.0f);
        return std::exp(lo + clampf(norm, 0.0f, 1.0f) * (hi - lo));
    }

    int keyAt(float x, float y) const
    {
        if(!keyboardRect_.contains(x, y))
            return -1;
        constexpr int first = 36;
        constexpr int keys = 61;
        return first + clampi(int((x - keyboardRect_.x) / keyboardRect_.w * float(keys)), 0, keys - 1);
    }

    bool handleKeyboardPress(float x, float y)
    {
        const int note = keyAt(x, y);
        if(note < 0)
            return false;
        pressMouseKey(note);
        return true;
    }

    void pressMouseKey(int note)
    {
        if(mouseKey_ == note)
            return;
        releaseMouseKey();
        mouseKey_ = note;
        if(auto *p = plugin())
            p->previewNoteOn(note, 0.85f);
        else
            sendNote(0, static_cast<uint8_t>(note), 105);
    }

    void releaseMouseKey()
    {
        if(mouseKey_ < 0)
            return;
        if(auto *p = plugin())
            p->previewNoteOff(mouseKey_);
        else
            sendNote(0, static_cast<uint8_t>(mouseKey_), 0);
        mouseKey_ = -1;
        repaint();
    }

    int keycodeSlot(uint keycode) const
    {
        return keycode < pressedKeycodeNotes_.size() ? int(keycode) : -1;
    }

    void releaseComputerNote(int note, int keySlot)
    {
        if(note < 0 || note >= int(computerKeys_.size()))
            return;
        if(computerKeys_[(size_t)note])
        {
            computerKeys_[(size_t)note] = false;
            if(auto *p = plugin())
                p->previewNoteOff(note);
        }
        if(keySlot >= 0 && keySlot < int(pressedKeycodeNotes_.size()))
            pressedKeycodeNotes_[(size_t)keySlot] = -1;
    }

    void releaseAllUiNotes()
    {
        if(auto *p = plugin())
        {
            for(size_t note = 0; note < computerKeys_.size(); ++note)
            {
                if(computerKeys_[note])
                    p->previewNoteOff(int(note));
            }
            if(mouseKey_ >= 0)
                p->previewNoteOff(mouseKey_);
        }
        clearUiNoteState();
    }

    void clearUiNoteState()
    {
        computerKeys_.fill(false);
        pressedKeycodeNotes_.fill(-1);
        mouseKey_ = -1;
    }

    int noteForComputerKey(uint key) const
    {
        if(key >= 'A' && key <= 'Z')
            key += 'a' - 'A';
        static constexpr char keys[] = "zsxdcvgbhnjmq2w3er5t6y7ui";
        for(size_t i = 0; i + 1 < sizeof(keys); ++i)
            if(uint(keys[i]) == key)
                return 48 + int(i);
        return -1;
    }

    synth::SourceGenParams generator_ {};
    synth::AdsrParams adsr_ {};
    std::array<synth::AdsrParams, synth::kMaxAmpEnvs> ampEnvs_ {};
    synth::OperatorChain operatorChain_ {};
    std::array<synth::LfoParams, synth::kMaxLfos> lfos_ {};
    std::array<synth::MatrixEnvParams, synth::kMaxModEnvs> envs_ {};
    std::array<synth::MatrixRule, synth::kMaxMatrixRules> rules_ {};
    synth::ChaosParams chaos_ {};
    synth::ShapeSourceParams shape_ {};
    synth::EffectsChainParams effects_ {};
    float gain_ = 0.3f;

    // MetaOsc 专用 undo 栈（UI 层），最多保留 32 步
    static constexpr int kMetaUndoMax = 32;
    std::vector<synth::WavetablePartialSlot> metaUndoStack_;
    bool metaUndoPending_ = false; // drag 期间只记一次快照
    int activeVoices_ = 0;
    int selectedLfo_ = 0;
    int selectedAmpEnv_ = 0;
    int selectedEnv_ = 0;
    int selectedRule_ = 0;
    int selectedOp_ = 0;
    int selectedSource_ = 0;
    int selectedTrack_ = 0;
    int selectedPartialIndex_ = 0;
    int selectedMetaPartial_ = 0;
    int selectedMetaFrame_ = 0;
    std::array<bool, synth::kMaxWavetableFrames> metaFrameSelected_ {};
    int metaFrameRangeAnchor_ = -1;
    int frameRangeCount_ = 0;
    bool matrixEnvDirty_ = false;
    static constexpr uint64_t kRealtimeDragPushIntervalMs = 8u;
    uint64_t lastTrackRealtimeDragPushMs_ = 0u;
    uint64_t lastGenRealtimeDragPushMs_ = 0u;
    // Heavy partial-bank edits still flush on release. Realtime-safe runtime
    // controls such as Partials/Inharmonic are additionally throttled while dragging.
    bool deferTrackPush_ = false;
    bool deferGenPush_ = false;
    bool ctrlDown_ = false;
    int selectedMetaHarmonic_ = 0;
    int selectedEnvPoint_ = -1;
    int selectedMatrixModSlot_ = 0;  // unified matrix LFO1-4 (0-3) / ENV1-4 (4-7) selection
    int envDragSeg_ = -1;            // segment whose curvature is being Ctrl-dragged
    Rect modModeRect_ {}, modEnvRateRect_ {};
    int mouseKey_ = -1;
    std::array<bool, 128> computerKeys_ {};
    std::array<int, 512> pressedKeycodeNotes_ {};
    DragTarget dragTarget_ = DragTarget::None;
    float dragStartY_    = 0.0f;
    float dragStartNorm_ = 0.0f;
    float dragStartDepth_ = 0.0f;
    float dragDepthLimit_ = 1.0f;
    float dragStartLayoutRatio_ = 0.0f;
    float layoutBottomRatio_ = 0.45f;   // bottom row = strips
    float layoutMatrixRatio_ = 0.42f;   // top-right column = matrix
    float layoutRackRatio_   = 0.20f;
    float layoutStripRatio_  = 0.25f;
    Rect layoutVSplitHandle_ {}, layoutRackSplitHandle_ {}, layoutStripSplitHandle_ {};
    bool modRouteDragActive_ = false;
    bool modRouteDragMoved_ = false;
    synth::ModSource modRouteSource_ = synth::ModSource::None;
    Rect modRouteSourceRect_ {};
    ModRouteTarget modRouteHover_ {};
    float modRouteStartX_ = 0.0f;
    float modRouteStartY_ = 0.0f;
    float modRouteMouseX_ = 0.0f;
    float modRouteMouseY_ = 0.0f;
    bool loadPathEditing_ = false;
    bool wavetableImportMenuOpen_ = false;
    bool wavetablePresetMenuOpen_ = false;
    bool droppedWavPending_ = false;
    bool presetMenuOpen_ = false;
    bool optionsMenuOpen_ = false;
    bool harmonicEditorOpen_ = false;
    bool metaProcessContextMenuOpen_ = false;
    bool routeContextMenuOpen_ = false;
    float routeContextX_ = 0.0f, routeContextY_ = 0.0f;
    int  routeContextRuleIndex_ = -1;
    std::array<Rect, 2> routeContextRects_ {};
    std::array<Rect, 2> stripRouteRects_ {};
    std::array<int,  2> stripRouteRuleIndices_ { -1, -1 };
    bool metaEditorDirty_ = false;
    MetaEditorDomain metaEditorDomain_ = MetaEditorDomain::Time;
    synth::WavetableImportMode wavetableImportMode_ = synth::WavetableImportMode::AutoDetect;
    int manualCycleLength_ = synth::kWavetableSize;
    int importFrameLimit_ = 128;
    bool shiftDown_ = false;
    std::array<bool, synth::kMaxSourceTracks> selectedStrips_ {};
    float stripScrollF_ = 0.0f;   // fractional column scroll for smooth panning
    Rect stripScrollbarRect_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripMuteRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripSoloRects_ {};
    std::vector<StripGroup> stripGroups_;
    bool stripGroupContextMenuOpen_ = false;
    float stripGroupContextX_ = 0.0f, stripGroupContextY_ = 0.0f;
    std::array<Rect, 2> stripGroupContextRects_ {};
    int  groupContextTargetGroup_ = -1;  // >=0 => ungroup menu for this group; -1 => create-group menu
    std::array<Rect, synth::kMaxSourceTracks> stripGroupBusRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripGainRects_ {}, stripPanRects_ {}, stripSendRects_ {};
    int dragTrackIndex_ = -1; // strip whose gain/pan/send is being dragged
    std::vector<InsertHit> insertHits_;
    std::vector<FxKnobHit> fxKnobHits_;
    std::vector<FxBtnHit> fxBypassHits_, fxDeleteHits_, fxModeHits_;
    FxKnobHit fxDragHit_ {};
    int modeMenuTrackId_ = -1, modeMenuGroup_ = -1, modeMenuInsertIdx_ = 0;
    // Source-modulation editing
    int selectedModSlot_ = -1;  // -1 = nothing highlighted by default
    struct ModHit { Rect rect {}; int trackId = -1; int slot = 0; };  // slot = -1 means "add"
    std::vector<ModHit> modHits_;
    std::array<Rect, synth::kMaxTrackMods> modSrcRects_ {}, modTypeRects_ {}, modDepthRects_ {}, modDeleteRects_ {};
    // Mod source picker (right-click the strip MOD area)
    bool modSourceMenuOpen_ = false;
    float modSourceMenuX_ = 0.0f, modSourceMenuY_ = 0.0f;
    int modSourceMenuTrackId_ = -1, modSourceMenuSlot_ = -1;  // slot -1 = new entry
    std::array<Rect, synth::kMaxSourceTracks + 1> modSourceMenuRects_ {};  // remove + candidate tracks
    // Insert slot press → click-to-edit or drag-to-reorder
    bool insertPending_ = false;
    bool insertDragActive_ = false;
    int insertPendTrackId_ = -1, insertPendGroup_ = -1, insertPendSlot_ = 0;
    float insertPendX_ = 0.0f, insertPendY_ = 0.0f;
    float insertDragX_ = 0.0f, insertDragY_ = 0.0f;
    // Insert picker menu
    bool insertMenuOpen_ = false;
    float insertMenuX_ = 0.0f, insertMenuY_ = 0.0f;
    int insertMenuTrackId_ = -1, insertMenuGroup_ = -1, insertMenuSlot_ = 0;
    std::array<Rect, 49> insertMenuRects_ {};  // None + 6 types × 8 slots (grid)
    // Effect algorithm/mode picker menu (right-click the mode button)
    bool modeMenuOpen_ = false;
    int modeMenuKind_ = 0;   // 1 = filter, 2 = distortion
    int modeMenuSlot_ = 0;   // bank slot 0..7
    int modeMenuSelectedIndex_ = 0;
    float modeMenuX_ = 0.0f, modeMenuY_ = 0.0f;
    std::array<Rect, 12> modeMenuRects_ {};
    bool addTrackMenuOpen_ = false;
    bool presetNameEditing_ = false;
    PresetNameEditTarget presetNameEditTarget_ = PresetNameEditTarget::None;
    bool skipNextPresetCharacterInput_ = false;
    std::string presetLabel_ = "Select preset";
    std::string presetNameBuffer_ = "user_kapibara";
    std::vector<std::string> presetNames_ {};
    int selectedPresetIndex_ = -1;
    std::vector<WavetablePresetEntry> wavetablePresets_ {};
    std::vector<std::pair<std::string, std::string>> irFiles_ {}; // (name, path) for IR reverb
    int selectedWavetablePresetIndex_ = -1;
    std::string wavetablePresetLabel_ = "Select Wavetable";
    std::string loadPathBuffer_ {};
    std::string lastLoadPath_ {};
    std::string browserStartDir_ {};
    std::string loadStatus_ = "type wav path after Load";
    std::string metaEditorStatus_ = "ready";
    float uiScale_ = 1.0f;
    // Letterbox state (fixed-aspect canvas centered in the real window).
    float realW_ = float(DISTRHO_UI_DEFAULT_WIDTH);
    float realH_ = float(DISTRHO_UI_DEFAULT_HEIGHT);
    float lbX_ = 0.0f, lbY_ = 0.0f;
    float lbW_ = float(DISTRHO_UI_DEFAULT_WIDTH), lbH_ = float(DISTRHO_UI_DEFAULT_HEIGHT);
    float uiRenderScale_ = 1.0f;  // uniform window-fit scale applied to the whole canvas
    char scratch_[64] {};

    Rect toolbar_ {}, panicRect_ {}, statusRect_ {}, keyboardRect_ {};
    Rect presetPrevRect_ {}, presetSelectRect_ {}, presetNextRect_ {}, presetSaveRect_ {}, presetLoadRect_ {};
    Rect menuRect_ {}, aboutRect_ {}, presetMenuPanelRect_ {}, presetSearchRect_ {}, presetListRect_ {};
    Rect presetMenuNewRect_ {}, presetMenuSaveRect_ {}, presetMenuLoadRect_ {}, presetMenuDeleteRect_ {}, presetMenuResetRect_ {};
    Rect optionsMenuPanelRect_ {}, uiScaleRect_ {};
    Rect wavetableImportPanelRect_ {}, wavetableImportCancelRect_ {};
    Rect wavetablePresetPanelRect_ {}, wavetablePresetListRect_ {};
    Rect wavetablePresetLoadRect_ {}, wavetablePresetNameRect_ {}, wavetablePresetImportRect_ {}, wavetablePresetRefreshRect_ {}, wavetablePresetCloseRect_ {};
    Rect wavetablePresetSaveRect_ {};
    std::array<Rect, 5> wavetableBuiltinRects_ {};
    std::array<Rect, 8> wavetablePresetRowRects_ {};
    bool currentClickIsDouble_ = false;
    Rect metaProcessContextPanelRect_ {};
    std::array<Rect, 7> metaProcessContextRects_ {};
    float metaProcessContextX_ = 0.0f;
    float metaProcessContextY_ = 0.0f;
    Rect manualCycleMinusRect_ {}, manualCyclePlusRect_ {}, manualCycleValueRect_ {};
    std::array<Rect, 3> importFrameLimitRects_ {};
    std::array<Rect, 5> wavetableImportModeRects_ {};
    std::array<Rect, 6> presetRowRects_ {};
    Rect partialCountRect_ {}, inharmonicModeRect_ {}, inharmonicRect_ {}, gainRect_ {}, masterMeterRect_ {};
    int editorTab_ = 0;  // 0=SOURCE 1=SHAPE 2=VOICE 3=MAPPING
    std::array<Rect, 4> editorTabRects_ {};
    int matrixTab_ = 0;  // 0=GRID 1=MODULATORS 2=AMP ENV
    std::array<Rect, 3> matrixTabRects_ {};
    struct MatrixCell { Rect rect; synth::ModSource src; synth::ModDestination dst; };
    std::vector<MatrixCell> matrixGridCells_;
    // User-chosen grid axes (start with a small default; add/remove via the grid).
    std::vector<synth::ModSource> gridSources_ { synth::ModSource::Lfo1, synth::ModSource::Env1 };
    std::vector<synth::ModDestination> gridDests_ { synth::ModDestination::Amp, synth::ModDestination::Freq };
    std::vector<Rect> gridSrcLabelRects_, gridDestLabelRects_, gridPickerItemRects_;
    std::vector<int> gridPickerPoolIdx_;
    Rect gridAddSrcRect_ {}, gridAddDstRect_ {};
    int gridPickerMode_ = 0;  // 0=closed 1=pick source 2=pick destination
    float gridPickerX_ = 0.0f, gridPickerY_ = 0.0f;
    std::array<Rect, synth::kMaxWavetablePartials> partialKnobRects_ {};
    std::array<Rect, 4> sourceCountRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> sourceChainRects_ {};
    Rect addTrackRect_ {};
    Rect removeTrackRect_ {};
    std::array<Rect, 4> addTrackTypeRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> trackRowRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripRects_ {};
    Rect trackOutputModeRect_ {}, trackGainRect_ {}, trackPanRect_ {}, trackSendRect_ {};
    Rect stripEnvRect_ {}, stripDupRect_ {};
    Rect ampEnvSelectRect_ {}, duplicateEnvRect_ {};
    Rect partialSpectrumRect_ {}, partialAmpRect_ {}, partialRatioRect_ {};
    Rect basicShapeRect_ {}, basicPulseRect_ {}, basicSubRect_ {};
    Rect noiseModeRect_ {}, noiseColorRect_ {};
    Rect sourceGainRect_ {}, sourcePanRect_ {}, sourceFilterEnableRect_ {}, sourceFilterTopologyRect_ {};
    Rect sourceFilterCutoffRect_ {}, sourceFilterResRect_ {}, sourceFilterDriveRect_ {}, sourceFilterFeedbackRect_ {};
    Rect sourceFilterMixRect_ {};
    Rect unisonVoicesRect_ {}, unisonDetuneRect_ {}, unisonWidthRect_ {}, unisonPhaseRect_ {};
    Rect attackRect_ {}, decayRect_ {}, sustainRect_ {}, releaseRect_ {}, curveRect_ {};
    std::array<Rect, synth::kEditableMetaPartials> metaSelectRects_ {};
    Rect metaEnableRect_ {}, metaWarpModeRect_ {}, metaFrameButtonRect_ {};
    Rect metaWavetableNameRect_ {}, metaWavetablePrevRect_ {}, metaWavetableNextRect_ {};
    Rect metaHarmonicEditRect_ {};
    Rect metaLoadRect_ {}, metaSaveRect_ {}, metaLoadPathRect_ {}, metaFrameStripRect_ {};
    bool metaFramesShown_ = false;
    bool fileBrowserSaving_ = false;
    std::array<Rect, 5> metaFramePresetRects_ {};
    std::array<Rect, synth::kVisibleWavetableFrames> metaFrameRects_ {};
    int metaFramePageStart_ = 0;
    int metaFrameScrollStart_ = 0;
    float dragScrollStartX_ = 0.0f;
    int dragScrollStartVal_ = 0;
    float dragScrollStartValF_ = 0.0f;
    // OCT/SEM/FIN/CRS pitch 控件
    Rect metaOctRect_ {}, metaSemRect_ {}, metaFinRect_ {}, metaCrsRect_ {};
    int  dragStartOct_ = 0, dragStartSem_ = 0;
    float dragStartFin_ = 0.0f, dragStartCrs_ = 0.0f;
    // 时域波形绘制连线插值用
    float prevTimeEditX_ = -1.0f, prevTimeEditY_ = -1.0f;
    // 双击重置检测
    uint32_t lastClickTime_ = 0;
    float lastClickX_ = -9999.0f, lastClickY_ = -9999.0f;
    // Route-FX editor: identity of the chain currently shown in the OSC editor.
    int selectedGroupView_ = -1;       // >=0 => OSC editor shows this group
    int routeFxChainTrackId_ = -1;
    int routeFxChainGroup_ = -1;
    // 动画相位（uiIdle 驱动，给 LFO 曲线做时变效果）
    float animPhase_ = 0.0f;
    // Legacy meta-partial ratio/amp（drawMetaPartialEditor 仍在用）
    Rect metaRatioRect_ {}, metaAmpRect_ {}, metaPhaseRect_ {}, metaPanRect_ {};
    Rect metaFrameCountRect_ {}, metaMorphRect_ {}, metaWarpAmountRect_ {};
    Rect metaWaveformRect_ {};
    Rect metaHarmonicRatioRect_ {}, metaHarmonicAmpRect_ {}, metaHarmonicPhaseRect_ {};
    Rect harmonicEditorPanelRect_ {}, harmonicEditorCloseRect_ {}, harmonicEditorBarsRect_ {};
    Rect harmonicEditorSpectrumRect_ {}, harmonicEditorPhaseRect_ {};
    Rect metaFrameScrollRect_ {};
    Rect metaEditorTimeRect_ {}, metaEditorSpectrumRect_ {}, metaEditorFrameStripRect_ {};
    Rect metaEditorImportRect_ {}, metaEditorAddRect_ {}, metaEditorDuplicateRect_ {}, metaEditorDeleteRect_ {};
    Rect metaSelAllRect_ {};
    Rect metaEditorLeftRect_ {}, metaEditorRightRect_ {}, metaEditorAlignRect_ {};
    Rect metaEditorLinearRect_ {}, metaEditorSpectralMorphRect_ {};
    std::array<Rect, 5> opSelectRects_ {};
    Rect opEnableRect_ {}, opTypeRect_ {}, opParamARect_ {}, opParamBRect_ {}, opParamCRect_ {}, opParamDRect_ {};
    std::array<Rect, synth::kMaxLfos> lfoSelectRects_ {};
    std::array<Rect, synth::kMaxModEnvs> envSelectRects_ {};
    std::array<Rect, synth::kMaxMatrixRules> ruleSelectRects_ {};
    std::array<Rect, synth::kMaxAmpEnvs> ampEnvTabRects_ {};
    Rect adsrSourceRect_ {};
    Rect lfoEnableRect_ {}, lfoShapeRect_ {}, lfoFreqRect_ {}, lfoPhaseRect_ {}, lfoRhoRect_ {};
    Rect envEnableRect_ {}, envPointARect_ {}, envPointBRect_ {}, envCurveARect_ {};
    Rect matrixEnvCurveRect_ {};
    Rect ruleEnableRect_ {}, ruleSourceRect_ {}, ruleDestRect_ {}, ruleWeightRect_ {}, ruleDepthRect_ {}, ruleBandLoRect_ {}, ruleBandHiRect_ {};
    Rect chaosEnableRect_ {}, chaosRateRect_ {}, chaosAmountRect_ {}, shapeAxisRect_ {}, shapePhaseRect_ {}, shapeRhoRect_ {}, shapeUpRect_ {}, shapeDownRect_ {};
    Rect eqEnableRect_ {}, eqModeRect_ {}, eqLowRect_ {}, eqMidRect_ {}, eqHighRect_ {}, eqDriveRect_ {};
    Rect filterEnableRect_ {}, filterModeRect_ {}, filterTypeRect_ {}, filterCutoffRect_ {}, filterResRect_ {}, filterDriveRect_ {};
    // UI filter bank rects
    std::array<Rect, 8> filterTabRects_ {};
    Rect filterEnabledRect_ {}, filterAlgoRect_ {};
    Rect filterCutoffKnobRect_ {}, filterResKnobRect_ {}, filterGainKnobRect_ {}, filterMixKnobRect_ {}, filterDriveKnobRect_ {};
    Rect filterEqPreviewRect_ {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KapibaraUI)
};

UI *createUI()
{
    return new KapibaraUI();
}

END_NAMESPACE_DISTRHO
