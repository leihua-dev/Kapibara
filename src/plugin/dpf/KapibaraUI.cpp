#include "DistrhoUI.hpp"

#include "KapibaraPlugin.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    LfoFreq, LfoPhase, LfoRho,
    EnvPointA, EnvPointB, EnvCurveA, MatrixEnvCurve, HarmonicEditor, MetaTimeEditor, MetaSpectrumEditor,
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
                         InsertDelay = synth::InsertDelay, InsertReverb = synth::InsertReverb;
    static constexpr int kInsertTypeCount = 6; // filter,dist,eq,comp,delay,reverb (kind 1..6)
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
        setGeometryConstraints(1040, 820, true);
        getWindow().setIgnoringKeyRepeat(true);
        computerKeys_.fill(false);
        pressedKeycodeNotes_.fill(-1);
        pullFromPlugin();
        pushGroups();
    }

  protected:
    void onNanoDisplay() override
    {
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
        drawInsertDragGhost();
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
        repaint();
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
                presetNameEditing_ = false;
                repaint();
                return true;
            }
            if(ev.key == kKeyEscape)
            {
                presetNameEditing_ = false;
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

        if(ev.press && harmonicEditorOpen_ && (ev.mod & kModifierControl) && ev.key == 'a')
        {
            auto *track = currentTrack();
            const int fc = track ? track->metaOsc.frameCount : 0;
            for(int i = 0; i < synth::kMaxWavetableFrames; ++i)
                metaFrameSelected_[(size_t)i] = i < fc;
            metaFrameRangeAnchor_ = fc > 0 ? 0 : -1;
            repaint();
            return true;
        }
        if(ev.press && harmonicEditorOpen_ && (ev.mod & kModifierControl) && ev.key == 'z')
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
        const float x = static_cast<float>(ev.pos.getX());
        const float y = static_cast<float>(ev.pos.getY());

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
                    finishModRouteDrag(static_cast<float>(ev.pos.getX()), static_cast<float>(ev.pos.getY()));
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
            dragTarget_ = DragTarget::None;
            prevTimeEditX_ = -1.0f;
            prevTimeEditY_ = -1.0f;
            return true;
        }

        if(ev.button == kMouseButtonRight && harmonicEditorOpen_)
        {
            openMetaProcessContextMenu(x, y);
            repaint();
            return true;
        }
        if(ev.button == kMouseButtonRight)
        {
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
                    routeContextX_ = clampf(x, 4.0f, std::max(4.0f, float(getWidth())  - 208.0f));
                    routeContextY_ = clampf(y, 4.0f, std::max(4.0f, float(getHeight()) - 90.0f));
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
                    stripGroupContextX_ = clampf(x, 4.0f, std::max(4.0f, float(getWidth())  - 220.0f));
                    stripGroupContextY_ = clampf(y, 4.0f, std::max(4.0f, float(getHeight()) - 100.0f));
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
                    stripGroupContextX_ = clampf(x, 4.0f, std::max(4.0f, float(getWidth())  - 220.0f));
                    stripGroupContextY_ = clampf(y, 4.0f, std::max(4.0f, float(getHeight()) - 100.0f));
                    stripGroupContextMenuOpen_ = true;
                    repaint();
                    return true;
                }
            }
        }

        if(ev.button != 1)
            return false;

        ctrlDown_  = (ev.mod & kModifierControl) != 0;
        shiftDown_ = (ev.mod & kModifierShift) != 0;

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
        const bool isDblClick = (ev.time - lastClickTime_) < 400u
                                && std::abs(x - lastClickX_) < 8.0f
                                && std::abs(y - lastClickY_) < 8.0f;
        lastClickTime_ = ev.time;
        lastClickX_ = x;
        lastClickY_ = y;
        if(isDblClick && handleDoubleClickReset(x, y)) { repaint(); return true; }

        if(handleToolbarClick(x, y) || handlePageClick(x, y) || handleKeyboardPress(x, y))
        {
            repaint();
            return true;
        }

        return false;
    }

    bool onMotion(const MotionEvent &ev) override
    {
        const float x = static_cast<float>(ev.pos.getX());
        const float y = static_cast<float>(ev.pos.getY());

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
            return;
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

    void drawBackground()
    {
        beginPath();
        rect(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()));
        fillColor(rgba(0x0f1418ff));
        fill();
    }

    void drawToolbar()
    {
        useUiFont();
        toolbar_ = { 0.0f, 0.0f, static_cast<float>(getWidth()), 64.0f };
        beginPath();
        rect(toolbar_.x, toolbar_.y, toolbar_.w, toolbar_.h);
        fillColor(rgba(0x171c22ff));
        fill();

        uiFontSize(18.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(rgba(0x9db0baff));
        text(18.0f, 32.0f, "Kapibara", nullptr);

        const float right = static_cast<float>(getWidth()) - 14.0f;
        aboutRect_ = { right - 88.0f, 12.0f, 88.0f, 36.0f };
        menuRect_ = { aboutRect_.x - 96.0f, 12.0f, 88.0f, 36.0f };
        panicRect_ = { menuRect_.x - 82.0f, 12.0f, 74.0f, 36.0f };
        const Rect abRect { panicRect_.x - 112.0f, 12.0f, 104.0f, 36.0f };
        presetPrevRect_ = { 220.0f, 14.0f, 38.0f, 34.0f };
        presetNextRect_ = { abRect.x - 48.0f, 14.0f, 38.0f, 34.0f };
        presetSelectRect_ = { 264.0f, 8.0f, std::max(220.0f, presetNextRect_.x - 272.0f), 46.0f };
        presetSaveRect_ = {};
        presetLoadRect_ = {};

        drawButton(presetPrevRect_, "<", false);
        drawButton(presetSelectRect_, presetLabel_.empty() ? "Select preset" : presetLabel_.c_str(), presetMenuOpen_);
        drawButton(presetNextRect_, ">", false);
        drawButton(abRect, "A -> B", false);
        drawButton(panicRect_, "Panic", false);
        drawButton(menuRect_, "MENU", false);
        drawButton(aboutRect_, "ABOUT", false);

        statusRect_ = { 18.0f, 48.0f, 520.0f, 14.0f };
        char status[128];
        std::snprintf(status, sizeof(status), "%d voices | keys: ZSXDCV... / Q2W3E...", activeVoices_);
        uiFontSize(11.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(rgba(0x70828dff));
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
        presetMenuLoadRect_ = { r.x + r.w - 156.0f, r.y + 72.0f, 140.0f, 44.0f };
        presetMenuDeleteRect_ = { r.x + r.w - 156.0f, r.y + 128.0f, 140.0f, 44.0f };
        presetMenuResetRect_ = { r.x + r.w - 156.0f, r.y + 184.0f, 140.0f, 44.0f };
        presetListRect_ = { r.x + 16.0f, r.y + 62.0f, r.w - 188.0f, 156.0f };
        const std::string nameText = presetNameEditing_ ? ("> " + presetNameBuffer_) : (presetNameBuffer_.empty() ? "Name..." : presetNameBuffer_);
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
        drawButton(presetMenuSaveRect_, "SAVE AS", false);
        drawButton(presetMenuLoadRect_, "LOAD", false);
        drawButton(presetMenuDeleteRect_, "DELETE", false);
        drawButton(presetMenuResetRect_, "RESET", false);
        drawLabelBox({ r.x + 16.0f, r.y + 282.0f, 88.0f, 22.0f }, "All");
        drawLabelBox({ r.x + 108.0f, r.y + 282.0f, 104.0f, 22.0f }, "User");
        drawLabelBox({ r.x + 216.0f, r.y + 282.0f, 122.0f, 22.0f }, "Favourites");
    }

    void drawWavetablePresetMenu()
    {
        if(!wavetablePresetMenuOpen_)
            return;

        const float menuWidth = std::min(520.0f, float(getWidth()) - 24.0f);
        const float menuX = clampf(metaWavetableNameRect_.x, 12.0f, float(getWidth()) - menuWidth - 12.0f);
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
                         "No WAV files in presets/wavetables");

        wavetablePresetLoadRect_ = { panel.x + 16.0f, panel.y + 266.0f, 112.0f, 34.0f };
        wavetablePresetImportRect_ = { panel.x + 136.0f, panel.y + 266.0f, 132.0f, 34.0f };
        wavetablePresetRefreshRect_ = { panel.x + 276.0f, panel.y + 266.0f, 104.0f, 34.0f };
        wavetablePresetCloseRect_ = { panel.x + panel.w - 96.0f, panel.y + 266.0f, 80.0f, 34.0f };
        drawButton(wavetablePresetLoadRect_, "LOAD", false);
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
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator
           || index < 0 || index >= int(wavetablePresets_.size()))
            return false;
        selectedWavetablePresetIndex_ = index;
        loadPathBuffer_ = wavetablePresets_[(size_t)index].path;
        wavetableImportMode_ = synth::WavetableImportMode::AutoDetect;
        importFrameLimit_ = synth::kMaxWavetableFrames;
        if(!commitWavetableLoad())
            return false;
        wavetablePresetLabel_ = wavetablePresets_[(size_t)index].name;
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
                return true;
            }
        }
        if(wavetablePresetLoadRect_.contains(x, y))
        {
            if(loadWavetablePreset(selectedWavetablePresetIndex_))
                wavetablePresetMenuOpen_ = false;
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
        const Rect r { (float(getWidth()) - 440.0f) * 0.5f, (float(getHeight()) - 410.0f) * 0.5f,
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

    static bool fxHasMode(int kind) { return kind == InsertFilter || kind == InsertDist; }

    static const char *fxKnobName(int kind, int i)
    {
        static const char *F[4]={"Cutoff","Q","Drive","Mix"};
        static const char *D[4]={"Drive","Bias","Mix","Out"};
        static const char *E[4]={"Low","Mid","High","MidHz"};
        static const char *C[4]={"Thr","Ratio","Atk","Makeup"};
        static const char *L[4]={"Time","FB","Mix","Tone"};
        static const char *R[4]={"Size","Decay","Mix","Damp"};
        switch(kind){case InsertFilter:return F[i];case InsertDist:return D[i];case InsertEq:return E[i];
                     case InsertComp:return C[i];case InsertDelay:return L[i];default:return R[i];}
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
            default:{auto&r=e.reverb;switch(i){case 0:r.size=n;break;case 1:r.decay=n*0.92f;break;case 2:r.mix=n;break;default:r.damp=n;break;}break;}
        }
    }

    // ---- Insert TYPE picker (append a new effect to a chain) ----
    void openInsertMenu(int trackId, int groupIdx, float x, float y)
    {
        insertMenuTrackId_ = trackId;
        insertMenuGroup_   = groupIdx;
        insertMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(getWidth())  - 130.0f));
        insertMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(getHeight()) - 160.0f));
        insertMenuOpen_ = true;
    }

    static const char *insertTypeName(int kind)
    {
        static const char *n[7] = { "", "Filter", "Distortion", "EQ", "Compressor", "Delay", "Reverb" };
        return (kind >= 1 && kind <= 6) ? n[kind] : "";
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
    void openModeMenu(int trackId, int groupIdx, int insertIdx, int kind, float x, float y)
    {
        modeMenuTrackId_ = trackId; modeMenuGroup_ = groupIdx; modeMenuInsertIdx_ = insertIdx; modeMenuKind_ = kind;
        const int rows = kind == InsertFilter ? kFilterAlgoCount : kDistAlgoCount;
        modeMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(getWidth()) - 130.0f));
        modeMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(getHeight()) - (28.0f + float(rows) * 18.0f)));
        modeMenuOpen_ = true;
    }

    void drawModeMenu()
    {
        if(!modeMenuOpen_)
            return;
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuGroup_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size())) { modeMenuOpen_ = false; return; }
        const int rows = modeMenuKind_ == InsertFilter ? kFilterAlgoCount : kDistAlgoCount;
        constexpr float rowH = 18.0f;
        const float menuW = 122.0f;
        const Rect panel { modeMenuX_, modeMenuY_, menuW, 22.0f + rowH * float(rows) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, modeMenuKind_ == InsertFilter ? "Filter mode" : "Dist mode", nullptr);
        const auto &ins = (*chain)[(size_t)modeMenuInsertIdx_];
        const int cur = modeMenuKind_ == InsertFilter ? int(ins.filter.algo) : int(ins.dist.algo);
        for(int i = 0; i < rows; ++i)
        {
            modeMenuRects_[(size_t)i] = { panel.x + 6.0f, panel.y + 20.0f + float(i) * rowH, menuW - 12.0f, rowH - 2.0f };
            drawButton(modeMenuRects_[(size_t)i], modeMenuKind_ == InsertFilter ? kFilterAlgoNames[i] : kDistAlgoNames[i], cur == i);
        }
    }

    bool handleModeMenuClick(float x, float y)
    {
        if(!modeMenuOpen_)
            return false;
        modeMenuOpen_ = false;
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuGroup_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size()))
            return true;
        const int rows = modeMenuKind_ == InsertFilter ? kFilterAlgoCount : kDistAlgoCount;
        for(int i = 0; i < rows; ++i)
            if(modeMenuRects_[(size_t)i].contains(x, y))
            {
                auto &ins = (*chain)[(size_t)modeMenuInsertIdx_];
                if(modeMenuKind_ == InsertFilter) ins.filter.algo = static_cast<synth::InsertFilterAlgo>(i);
                else                              ins.dist.algo = static_cast<synth::InsertDistAlgo>(i);
                commitChainChange(modeMenuTrackId_, modeMenuGroup_);
                return true;
            }
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
        metaProcessContextX_ = clampf(x, 4.0f, std::max(4.0f, float(getWidth()) - menuWidth - 4.0f));
        metaProcessContextY_ = clampf(y, 4.0f, std::max(4.0f, float(getHeight()) - menuHeight - 4.0f));
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

    void drawHarmonicEditor()
    {
        if(!harmonicEditorOpen_)
            return;
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
        {
            harmonicEditorOpen_ = false;
            harmonicEditorPanelRect_ = {};
            return;
        }

        const Rect r { 42.0f, 92.0f, static_cast<float>(getWidth()) - 84.0f,
                       static_cast<float>(getHeight()) - 190.0f };
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
        drawButton(metaEditorImportRect_, "Import WAV", false);
        drawButton(metaEditorAddRect_, "Add", false);
        drawButton(metaEditorDuplicateRect_, "Duplicate", false);
        drawButton(metaEditorDeleteRect_, "Delete", false);
        drawButton(metaEditorLeftRect_, "<", false);
        drawButton(metaEditorRightRect_, ">", false);

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
        const Rect page { 16.0f, 82.0f, static_cast<float>(getWidth()) - 32.0f,
                          static_cast<float>(getHeight()) - 184.0f };
        drawPanel(page, rgba(0x10171bff), rgba(0x293842ff));

        const float gap     = 14.0f;
        const float bottomH = clampf(page.h * layoutBottomRatio_, 200.0f, page.h - 160.0f);
        const float topH    = page.h - bottomH - gap * 2.0f;
        const float stripW  = clampf(page.w * layoutStripRatio_, 240.0f, page.w * 0.65f);

        const Rect strip  { page.x + page.w - gap - stripW, page.y + gap, stripW, topH };
        const Rect editor { page.x + gap, page.y + gap,
                            std::max(200.0f, strip.x - page.x - gap * 2.0f), topH };
        const Rect matrix { page.x + gap, strip.y + strip.h + gap * 2.0f,
                            page.w - gap * 2.0f, bottomH - gap };

        drawTrackEditor(editor);
        drawStripRack(strip);
        drawMatrixDashboard(matrix);

        // ---- Divider handles ----
        constexpr float kDivW = 6.0f;
        // Horizontal divider: between top panels and matrix
        const float hDivY = strip.y + strip.h + gap * 0.5f;
        layoutVSplitHandle_     = { page.x + gap, hDivY, page.w - gap * 2.0f, gap };
        layoutRackSplitHandle_  = {};
        // Vertical divider: between editor and strip rack
        const float svDivX = strip.x - gap * 0.5f - kDivW * 0.5f;
        layoutStripSplitHandle_ = { svDivX, page.y + gap, kDivW, topH };

        const auto drawDivHandle = [&](const Rect &r, bool horiz) {
            beginPath();
            rect(r.x, r.y, r.w, r.h);
            fillColor(rgba(0x3a5060a0));
            fill();
            const int dots = horiz ? 5 : 3;
            const float step = horiz ? r.w / float(dots + 1) : r.h / float(dots + 1);
            fillColor(rgba(0x6a9ab0cc));
            for(int i = 1; i <= dots; ++i)
            {
                const float cx = horiz ? r.x + step * float(i) : r.x + r.w * 0.5f;
                const float cy = horiz ? r.y + r.h * 0.5f      : r.y + step * float(i);
                beginPath();
                circle(cx, cy, 2.0f);
                fill();
            }
        };
        drawDivHandle(layoutVSplitHandle_,     true);
        drawDivHandle(layoutStripSplitHandle_, false);
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

        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            const auto source = static_cast<synth::ModSource>(int(synth::ModSource::Lfo1) + i);
            const bool routed = std::any_of(rules_.begin(), rules_.end(),
                                            [source](const auto &r) { return r.enabled && r.source == source; });
            if(routed || (modRouteDragActive_ && modRouteSource_ == source))
                drawPanel(lfoSelectRects_[(size_t)i], rgba(0x17384a70), modulationSourceColor(source));
        }
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
        {
            const auto source = static_cast<synth::ModSource>(int(synth::ModSource::Env1) + i);
            const bool routed = std::any_of(rules_.begin(), rules_.end(),
                                            [source](const auto &r) { return r.enabled && r.source == source; });
            if(routed || (modRouteDragActive_ && modRouteSource_ == source))
                drawPanel(envSelectRects_[(size_t)i], rgba(0x4a301770), modulationSourceColor(source));
        }
        if(modRouteDragActive_ && modRouteDragMoved_)
        {
            const Color color = modulationSourceColor(modRouteSource_);
            strokeLine(modRouteSourceRect_.x + modRouteSourceRect_.w * 0.5f,
                       modRouteSourceRect_.y + modRouteSourceRect_.h * 0.5f,
                       modRouteMouseX_, modRouteMouseY_, color, 2.5f);
            if(modRouteHover_.valid)
                drawPanel(modRouteHover_.rect, rgba(0x17384a70), color);
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
            const char *algo = e.kind == InsertFilter ? kFilterAlgoNames[int(e.filter.algo)]
                                                       : kDistAlgoNames[int(e.dist.algo)];
            drawButton(modeR, algo, false);
            fxModeHits_.push_back(FxBtnHit { modeR, trackId, groupIdx, insertIdx });
            knobsY = p.y + 38.0f;
        }
        const float kw = (p.w - 12.0f) * 0.25f;
        for(int i = 0; i < 4; ++i)
        {
            const Rect kr { p.x + 4.0f + float(i) * (kw + 1.0f), knobsY, kw, std::min(46.0f, p.y + p.h - knobsY - 4.0f) };
            drawKnob(kr, fxKnobName(e.kind, i), fxKnobNorm(e, i), fxKnobDisp(e, i));
            fxKnobHits_.push_back(FxKnobHit { kr, trackId, groupIdx, insertIdx, i });
        }
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
        modSourceMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(getWidth()) - 150.0f));
        modSourceMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(getHeight()) - (26.0f + float(rows) * 18.0f)));
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
        drawLabelBox({ r.x + 16.0f, r.y + 42.0f, 170.0f, 24.0f }, synth::sourceTrackTypeName(track->type));
        trackOutputModeRect_ = {};  // output mode 选择从 UI 移除，默认 AudioAndMod
        track->outputMode = synth::SourceTrackOutputMode::AudioAndMod;
        track->ampEnvIndex = clampi(track->ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
        ampEnvSelectRect_ = { r.x + 196.0f, r.y + 42.0f, 112.0f, 24.0f };
        duplicateEnvRect_ = { r.x + 318.0f, r.y + 42.0f, 96.0f, 24.0f };
        drawButton(ampEnvSelectRect_, buttonText("ADSR ENV %d", track->ampEnvIndex + 1), selectedAmpEnv_ == track->ampEnvIndex);
        drawButton(duplicateEnvRect_, "Duplicate", false);

        const float uniY = r.y + 76.0f;
        const float uniKw = 46.0f;
        unisonVoicesRect_ = { r.x + 16.0f,                       uniY, uniKw, uniKw };
        unisonDetuneRect_ = { r.x + 16.0f + (uniKw + 8.0f),     uniY, uniKw, uniKw };
        unisonWidthRect_  = { r.x + 16.0f + (uniKw + 8.0f) * 2, uniY, uniKw, uniKw };
        unisonPhaseRect_  = { r.x + 16.0f + (uniKw + 8.0f) * 3, uniY, uniKw, uniKw };
        drawKnob(unisonVoicesRect_, "Unison",  float(track->unison.voices - 1) / 15.0f, float(track->unison.voices));
        drawKnob(unisonDetuneRect_, "Detune",  track->unison.detuneCents / 80.0f,        track->unison.detuneCents);
        drawKnob(unisonWidthRect_,  "Width",   track->unison.widthStereo,                track->unison.widthStereo);
        drawKnob(unisonPhaseRect_,  "Rnd Ph",  track->unison.phaseSpread,                track->unison.phaseSpread);

        // Reserve the lower portion: routed-effect editor (if any) at the bottom,
        // and a modulation editor strip above it.
        const bool hasFx = trackHasRoutedFx(*track);
        const bool hasMod = trackHasAnyMod(*track);  // only show MOD editor once a source is routed
        const float fxH = hasFx ? std::min(230.0f, std::max(130.0f, (r.h - 152.0f) * 0.36f)) : 0.0f;
        const float modH = hasMod ? 104.0f : 0.0f;
        const Rect body { r.x + 16.0f, r.y + 136.0f, r.w - 32.0f, r.h - 152.0f - fxH - modH };
        if(track->type == synth::SourceTrackType::PartialBank)
            drawPartialBankTrackEditor(body, *track);
        else if(track->type == synth::SourceTrackType::MetaOscillator)
            drawMetaTrackEditor(body, *track);
        else if(track->type == synth::SourceTrackType::BasicOscillator)
            drawBasicTrackEditor(body, *track);
        else
            drawNoiseTrackEditor(body, *track);

        if(hasMod)
        {
            const Rect modRegion { r.x + 16.0f, r.y + r.h - fxH - modH, r.w - 32.0f, modH - 6.0f };
            drawModEditor(modRegion, *track);
        }
        else
        {
            modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        }

        if(hasFx)
        {
            const Rect fx { r.x + 16.0f, r.y + r.h - fxH - 4.0f, r.w - 32.0f, fxH - 8.0f };
            drawRouteFxEditor(fx, &track->inserts, int(track->id), -1);
        }
        else
        {
            fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
            routeFxChainTrackId_ = -1;
            routeFxChainGroup_ = -1;
        }
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
        stripScrollOffset_ = clampi(stripScrollOffset_, 0, totalScroll);
        const int visCols = std::min(totalCols - stripScrollOffset_, maxVis);
        const float w = visCols > 0
            ? (r.w - 28.0f - gap * float(visCols - 1)) / float(visCols)
            : r.w - 28.0f;

        // ---- Clear hit rects ----
        trackGainRect_ = {}; trackPanRect_ = {}; trackSendRect_ = {};
        stripRouteRects_.fill({}); stripRouteRuleIndices_.fill(-1);
        for(auto &rr : stripRects_) rr = {};
        for(auto &rr : stripGroupBusRects_) rr = {};
        for(auto &rr : stripMuteRects_) rr = {};
        for(auto &rr : stripSoloRects_) rr = {};
        insertHits_.clear();
        modHits_.clear();

        // ---- Draw group brackets (behind strips, for member ranges) ----
        for(int gi = 0; gi < totalGroups; ++gi)
        {
            const auto &grp = stripGroups_[(size_t)gi];
            if(grp.memberIndices.empty()) continue;
            int firstCol = 99999, lastCol = -1;
            for(int mi : grp.memberIndices)
            {
                const int col = mi - stripScrollOffset_;
                if(col >= 0 && col < maxVis) { firstCol = std::min(firstCol, col); lastCol = std::max(lastCol, col); }
            }
            if(lastCol < 0 || firstCol >= 99999) continue;
            const float bx = r.x + 14.0f + float(firstCol) * (w + gap) - 2.0f;
            const float bw = float(lastCol - firstCol) * (w + gap) + w + 4.0f;
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
        for(int col = 0; col < visCols; ++col)
        {
            const int globalIdx = col + stripScrollOffset_;
            const bool isGroup  = globalIdx >= totalTracks;
            const float sx = r.x + 14.0f + float(col) * (w + gap);
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

            const uint32_t borderCol = isPrimary  ? 0x70d77affU
                                     : isMultiSel ? 0xe0a030ffU
                                     : grpIdx >= 0 ? 0xc070e066U
                                     :               0x344852ffU;
            drawPanel(s, isPrimary ? rgba(0x17242cff) : rgba(0x101820ff), rgba(borderCol));

            // ---- OSC header (mini preview + type label) ----
            const float kOscH = std::min(58.0f, stripH * 0.22f);
            const Rect oscHdr { s.x, s.y, s.w, kOscH };
            drawPanel(oscHdr, rgba(0x0d1822ff), isPrimary ? rgba(0x405060ddU) : rgba(0x1e2c38ddU));
            fontSize(7.5f);
            fillColor(rgba(isPrimary ? 0x9eff50ffU : 0x6080a0ffU));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(oscHdr.x + 4.0f, oscHdr.y + 2.0f, synth::sourceTrackTypeName(track.type), nullptr);
            if(track.type == synth::SourceTrackType::MetaOscillator && track.metaOsc.frameCount > 0)
            {
                float liveMorph = track.metaOsc.morph;
                if(const auto *p = plugin()) liveMorph = p->sourceLiveMorph(globalIdx);
                const int fIdx = clampi(int(liveMorph * float(track.metaOsc.frameCount - 1) + 0.5f), 0, track.metaOsc.frameCount - 1);
                const auto &frm = track.metaOsc.frames[(size_t)fIdx];
                const Rect wr { oscHdr.x + 2.0f, oscHdr.y + 14.0f, oscHdr.w - 4.0f, kOscH - 16.0f };
                scissor(wr.x, wr.y, wr.w, wr.h);
                const float midY2 = wr.y + wr.h * 0.5f;
                beginPath();
                for(int sp = 0; sp < int(wr.w); ++sp)
                {
                    const float t2 = float(sp) / std::max(1.0f, wr.w);
                    const float v  = sampleFrameWarped(frm, t2, track.metaOsc.warpMode, track.metaOsc.warpAmount);
                    const float px2 = wr.x + float(sp);
                    const float py2 = midY2 - v * wr.h * 0.42f;
                    if(sp == 0) moveTo(px2, py2); else lineTo(px2, py2);
                }
                strokeColor(isPrimary ? rgba(0x9eff50ccU) : rgba(0x4d7780bbU));
                strokeWidth(1.0f); stroke();
                resetScissor();
            }

            // ---- Controls below OSC header ----
            const float cy = s.y + kOscH + 4.0f;
            drawButton({ s.x + 4.0f, cy,        s.w - 8.0f, 22.0f }, track.name.c_str(), isPrimary || isMultiSel);
            const Rect muteR { s.x + 4.0f, cy + 26.0f, 24.0f, 22.0f };
            const Rect soloR { s.x + s.w - 28.0f, cy + 26.0f, 24.0f, 22.0f };
            stripMuteRects_[(size_t)globalIdx] = muteR;
            stripSoloRects_[(size_t)globalIdx] = soloR;
            drawButton(muteR, "M", track.mute);
            drawButton(soloR, "S", track.solo);

            // Group tag
            if(grpIdx >= 0)
            {
                fontSize(7.0f); fillColor(rgba(0xc070e0ccU));
                textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
                text(s.x + s.w - 5.0f, cy + 36.0f, stripGroups_[(size_t)grpIdx].name.c_str(), nullptr);
            }

            // Route inserts (filter / distortion chain) — mixer-style, one per row
            const float routeY0 = cy + 52.0f;
            fontSize(7.0f); fillColor(rgba(0x6a8090ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(s.x + 4.0f, routeY0 - 9.0f, "ROUTE (drag to reorder)", nullptr);
            drawInsertColumn({ s.x + 4.0f, routeY0, s.w - 8.0f, 74.0f }, track.inserts, int(track.id), -1);

            // MOD slots (modulation entries) — click to edit in the OSC editor
            const float modY = routeY0 + 80.0f;
            fontSize(7.0f); fillColor(rgba(0x6a8090ff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(s.x + 4.0f, modY - 9.0f, "MOD", nullptr);
            drawModColumn({ s.x + 4.0f, modY, s.w - 8.0f, 56.0f }, track, int(track.id));

            // MATRIX routes that target this track (created from the matrix dashboard)
            const float mtxY = modY + 60.0f;
            fontSize(7.0f); fillColor(rgba(0x6a8090ff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(s.x + 4.0f, mtxY - 9.0f, "MATRIX", nullptr);
            int mrow = 0;
            for(int ri = 0; ri < synth::kMaxMatrixRules && mrow < 2; ++ri)
            {
                const auto &rule = rules_[(size_t)ri];
                if(!rule.enabled || rule.targetTrackId != track.id)
                    continue;
                const Rect rr { s.x + 4.0f, mtxY + float(mrow) * 20.0f, s.w - 8.0f, 18.0f };
                const bool isEff = synth::insertModParamForDest(rule.dest) >= 0;
                if(isEff)
                    std::snprintf(scratch_, sizeof(scratch_), "%s>%s%d %+.1f",
                                  sourceName(rule.source), destName(rule.dest), rule.targetSlot + 1, double(rule.depth));
                else
                    std::snprintf(scratch_, sizeof(scratch_), "%s>%s %+.1f",
                                  sourceName(rule.source), destName(rule.dest), double(rule.depth));
                drawLabelBox(rr, scratch_);
                if(isPrimary)  // only the focused strip registers right-click delete targets
                {
                    stripRouteRects_[(size_t)mrow] = rr;
                    stripRouteRuleIndices_[(size_t)mrow] = ri;
                }
                ++mrow;
            }
            if(mrow == 0)
                drawLabelBox({ s.x + 4.0f, mtxY, s.w - 8.0f, 18.0f }, "no matrix");

            drawLabelBox({ s.x + 4.0f, mtxY + 44.0f, s.w - 8.0f, 18.0f },
                         buttonText("ENV%d", clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) + 1));
            drawLabelBox({ s.x + 4.0f, mtxY + 64.0f, s.w - 8.0f, 18.0f },
                         buttonText("UNI×%d", clampi(track.unison.voices, 1, 16)));

            // Gain / Pan / Send (selected strip only, at bottom)
            if(isPrimary)
            {
                trackGainRect_ = { s.x + 4.0f, s.y + s.h - 82.0f, s.w - 8.0f, 22.0f };
                trackPanRect_  = { s.x + 4.0f, s.y + s.h - 56.0f, s.w - 8.0f, 22.0f };
                trackSendRect_ = { s.x + 4.0f, s.y + s.h - 30.0f, s.w - 8.0f, 22.0f };
                drawSlider(trackGainRect_, "Gain", track.gain * 0.5f, track.gain);
                drawSlider(trackPanRect_,  "Pan",  (track.pan + 1.0f) * 0.5f, track.pan);
                drawSlider(trackSendRect_, "Send", track.send, track.send);
            }
        }

        // ---- Horizontal scrollbar ----
        const float sbY = startY + stripH + 4.0f;
        stripScrollbarRect_ = { r.x + 14.0f, sbY, r.w - 28.0f, kScrollH };
        drawPanel(stripScrollbarRect_, rgba(0x0d141aff), rgba(0x2a3840ff));
        if(totalCols > maxVis && totalScroll > 0)
        {
            const float thumbW = std::max(16.0f, stripScrollbarRect_.w * float(maxVis) / float(totalCols));
            const float thumbX = stripScrollbarRect_.x
                               + (stripScrollbarRect_.w - thumbW) * float(stripScrollOffset_) / float(totalScroll);
            beginPath();
            rect(thumbX, sbY + 2.0f, thumbW, kScrollH - 4.0f);
            fillColor(rgba(0x5080a0ccU));
            fill();
        }
    }

    void drawPartialBankTrackEditor(const Rect &r, synth::SourceTrackParams &track)
    {
        auto &seed = track.partialBank;
        partialCountRect_ = { r.x, r.y, 220.0f, 24.0f };
        inharmonicModeRect_ = { r.x + 230.0f, r.y, 120.0f, 24.0f };
        inharmonicRect_ = { r.x + 360.0f, r.y, 180.0f, 24.0f };
        drawSlider(partialCountRect_, "Partials", float(seed.partialCount - 1) / 63.0f, float(seed.partialCount));
        drawButton(inharmonicModeRect_, freqShapeName(seed.freqShape), seed.freqShape != synth::FreqShape::Harmonic);
        drawSlider(inharmonicRect_, "Inharmonic", seed.inharmonicAmount, seed.inharmonicAmount);

        const Rect spectrum { r.x, r.y + 40.0f, r.w, std::min(170.0f, r.h * 0.42f) };
        drawPanel(spectrum, rgba(0x101820ff), rgba(0x263842ff));
        const float barW = (spectrum.w - 16.0f) / float(synth::kMaxWavetablePartials);
        for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
        {
            const auto &p = seed.partials[(size_t)i];
            const float amp = i < seed.partialCount && p.enabled ? clampf(p.amp, 0.0f, 1.0f) : 0.0f;
            const float h = amp * (spectrum.h - 22.0f);
            beginPath();
            rect(spectrum.x + 8.0f + float(i) * barW, spectrum.y + spectrum.h - 10.0f - h,
                 std::max(1.0f, barW - 2.0f), h);
            fillColor(i == selectedPartialIndex_ ? rgba(0x8be87dff) : rgba(0x8c5dffcc));
            fill();
        }
        partialSpectrumRect_ = spectrum;
        selectedPartialIndex_ = clampi(selectedPartialIndex_, 0, std::max(0, seed.partialCount - 1));
        auto &slot = seed.partials[(size_t)selectedPartialIndex_];
        partialAmpRect_ = { r.x, spectrum.y + spectrum.h + 12.0f, r.w * 0.5f - 6.0f, 24.0f };
        partialRatioRect_ = { r.x + r.w * 0.5f + 6.0f, spectrum.y + spectrum.h + 12.0f, r.w * 0.5f - 6.0f, 24.0f };
        drawSlider(partialAmpRect_, buttonText("P%d Amp", selectedPartialIndex_ + 1), slot.amp, slot.amp);
        drawSlider(partialRatioRect_, "Ratio", std::min(1.0f, slot.ratio / 64.0f), slot.ratio);
    }

    void drawMetaTrackEditor(const Rect &r, synth::SourceTrackParams &track)
    {
        auto &slot = track.metaOsc;
        metaEnableRect_ = {};  // ON/OFF button removed
        metaWavetablePrevRect_ = { r.x + r.w - 60.0f, r.y, 28.0f, 28.0f };
        metaWavetableNextRect_ = { r.x + r.w - 28.0f, r.y, 28.0f, 28.0f };
        metaWavetableNameRect_ = { r.x, r.y, std::max(80.0f, r.w - 66.0f), 28.0f };
        const std::string wavetableTitle = (wavetablePresetLabel_.empty() ? "Select Wavetable" : wavetablePresetLabel_) + "  v";
        drawButton(metaWavetableNameRect_, wavetableTitle.c_str(), wavetablePresetMenuOpen_);
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

        const float waveformHeight = clampf(r.h - 104.0f, 28.0f, 150.0f);
        metaWaveformRect_ = { r.x, r.y + 62.0f, r.w, waveformHeight };
        drawMeta3DWaveform(metaWaveformRect_, slot, selectedTrack_);

        const float controlY = metaWaveformRect_.y + metaWaveformRect_.h + 6.0f;
        const float controlGap = 5.0f;
        const float controlW = (r.w - controlGap * 4.0f) / 5.0f;
        metaFrameCountRect_ = {};  // Frames 控件从主界面移除
        metaMorphRect_ = { r.x, controlY, controlW, 36.0f };
        metaWarpModeRect_ = { metaMorphRect_.x + controlW + controlGap, controlY, controlW, 36.0f };
        metaWarpAmountRect_ = { metaWarpModeRect_.x + controlW + controlGap, controlY, controlW, 36.0f };
        metaPhaseRect_ = { metaWarpAmountRect_.x + controlW + controlGap, controlY, controlW, 36.0f };
        metaPanRect_ = { metaPhaseRect_.x + controlW + controlGap, controlY, controlW, 36.0f };
        drawKnob(metaMorphRect_, "Morph", slot.morph, slot.morph);
        drawButton(metaWarpModeRect_, warpModeName(slot.warpMode), false);
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
    }

    void drawNoiseTrackEditor(const Rect &r, synth::SourceTrackParams &track)
    {
        noiseModeRect_ = { r.x, r.y, 180.0f, 26.0f };
        noiseColorRect_ = { r.x, r.y + 36.0f, 260.0f, 24.0f };
        drawButton(noiseModeRect_, synth::sampleNoiseModeName(track.sampleNoiseMode), false);
        drawSlider(noiseColorRect_, "Noise Color", track.noiseColor, track.noiseColor);
        drawLabelBox({ r.x, r.y + 70.0f, 260.0f, 24.0f }, "File/Capture unavailable in v1");
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

        // Effects live in the OSC editor (per-strip insert chains), not here.
        const float colGap = 18.0f;
        const float colW   = (r.w - 32.0f - colGap) * 0.5f;
        const float c0 = r.x + 16.0f;
        const float c1 = c0 + colW + colGap;

        // -------- Column 0: LFO / Matrix ENV (with LFO curve) --------
        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            lfoSelectRects_[(size_t)i] = { c0 + float(i) * (colW / synth::kMaxLfos), r.y + 44.0f, colW / synth::kMaxLfos - 4.0f, 24.0f };
            drawButton(lfoSelectRects_[(size_t)i], buttonText("LFO%d", i + 1), selectedLfo_ == i);
        }
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
        {
            envSelectRects_[(size_t)i] = { c0 + float(i) * (colW / synth::kMaxModEnvs), r.y + 72.0f, colW / synth::kMaxModEnvs - 4.0f, 24.0f };
            drawButton(envSelectRects_[(size_t)i], buttonText("ENV%d", i + 1), selectedEnv_ == i);
        }
        auto &lfo = lfos_[(size_t)selectedLfo_];
        auto &env = envs_[(size_t)selectedEnv_];
        lfoEnableRect_ = { c0, r.y + 100.0f, colW * 0.48f - 4.0f, 24.0f };
        lfoShapeRect_  = { c0 + colW * 0.52f, r.y + 100.0f, colW * 0.48f - 4.0f, 24.0f };
        drawButton(lfoEnableRect_, lfo.enabled ? "LFO On"  : "LFO Off",  lfo.enabled);
        drawButton(lfoShapeRect_,  lfoShapeName(lfo.shape), false);
        // LFO curve preview
        drawLfoCurve({ c0, r.y + 128.0f, colW, 72.0f }, lfo);
        lfoFreqRect_   = { c0, r.y + 208.0f, colW * 0.5f - 4.0f, 22.0f };
        lfoPhaseRect_  = { c0 + colW * 0.5f + 4.0f, r.y + 208.0f, colW * 0.5f - 4.0f, 22.0f };
        lfoRhoRect_    = { c0, r.y + 234.0f, colW, 22.0f };
        drawSlider(lfoFreqRect_,   "LFO Rate",  lfo.frequencyHz / 20.0f, lfo.frequencyHz);
        drawSlider(lfoPhaseRect_,  "LFO Phase", lfo.phase0, lfo.phase0);
        drawSlider(lfoRhoRect_,    "LFO Rho",   lfo.rhoLfo, lfo.rhoLfo);
        envEnableRect_ = { c0, r.y + 262.0f, colW, 24.0f };
        drawButton(envEnableRect_, env.enabled ? "ENV On (drag curve)" : "ENV Off", env.enabled);
        envPointARect_ = {}; envPointBRect_ = {}; envCurveARect_ = {};
        matrixEnvCurveRect_ = { c0, r.y + 290.0f, colW, std::max(40.0f, r.h - 298.0f) };
        drawMatrixEnvCurve(matrixEnvCurveRect_, env);

        // Clear rule/chaos/shape rects (legacy, unused in this layout)
        ruleEnableRect_ = {}; ruleSourceRect_ = {}; ruleDestRect_ = {}; ruleWeightRect_ = {};
        ruleDepthRect_ = {}; ruleBandLoRect_ = {}; ruleBandHiRect_ = {};
        for(auto &rc : ruleSelectRects_) rc = {};
        chaosEnableRect_ = {}; shapeAxisRect_ = {};
        chaosRateRect_ = {}; chaosAmountRect_ = {};
        shapePhaseRect_ = {}; shapeRhoRect_ = {}; shapeUpRect_ = {}; shapeDownRect_ = {};

        // -------- Column 1: Amp ADSR ENV --------
        drawSectionTitle(c1, r.y + 14.0f, "Amp ADSR");
        // Draggable ADSR modulation source (drag onto any knob to route)
        adsrSourceRect_ = { c1 + colW - 86.0f, r.y + 12.0f, 86.0f, 20.0f };
        drawButton(adsrSourceRect_, "ADSR src →", false);
        const float tabW = colW / synth::kMaxAmpEnvs;
        for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
        {
            ampEnvTabRects_[(size_t)i] = { c1 + float(i) * tabW, r.y + 44.0f, tabW - 4.0f, 24.0f };
            drawButton(ampEnvTabRects_[(size_t)i], buttonText("ENV%d", i + 1), selectedAmpEnv_ == i);
        }
        auto &ampEnv = ampEnvs_[(size_t)selectedAmpEnv_];
        drawLabelBox({ c1, r.y + 74.0f, colW, 22.0f },
                     buttonText("Used by %d tracks", envUseCount(selectedAmpEnv_)));
        const float kw = (colW - 18.0f) * 0.25f;
        attackRect_  = { c1,                      r.y + 104.0f, kw, 44.0f };
        decayRect_   = { c1 + kw + 6.0f,          r.y + 104.0f, kw, 44.0f };
        sustainRect_ = { c1 + (kw + 6.0f) * 2.0f, r.y + 104.0f, kw, 44.0f };
        releaseRect_ = { c1 + (kw + 6.0f) * 3.0f, r.y + 104.0f, kw, 44.0f };
        drawKnob(attackRect_,  "A", ampEnv.attack  / 5.0f, ampEnv.attack);
        drawKnob(decayRect_,   "D", ampEnv.decay   / 5.0f, ampEnv.decay);
        drawKnob(sustainRect_, "S", ampEnv.sustain, ampEnv.sustain);
        drawKnob(releaseRect_, "R", ampEnv.release / 8.0f, ampEnv.release);
        drawAdsrCurve({ c1, r.y + 158.0f, colW, std::max(60.0f, r.h - 166.0f) }, ampEnv);
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

        metaLoadRect_ = { r.x, r.y + 82.0f, 70.0f, 20.0f };
        const float presetW = std::min(46.0f, std::max(34.0f, (r.w - 322.0f) / 5.0f));
        for(int i = 0; i < 5; ++i)
            metaFramePresetRects_[(size_t)i] = { r.x + 78.0f + float(i) * (presetW + 5.0f), r.y + 82.0f, presetW, 20.0f };
        metaLoadPathRect_ = { r.x + 78.0f + 5.0f * (presetW + 5.0f), r.y + 82.0f,
                              std::max(80.0f, r.w - 78.0f - 5.0f * (presetW + 5.0f)), 20.0f };
        drawButton(metaLoadRect_, "Load", loadPathEditing_);
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

    void drawMetaFrameStrip(const Rect &r, const synth::WavetablePartialSlot &slot)
    {
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
            const bool selected = frameIndex == selectedMetaFrame_;

            // Button background + border; multi-selected frames get cyan accent
            const uint32_t bgCol  = selected ? 0x263840ff : (multiSel ? 0x1a3028ff : 0x151d22ff);
            const uint32_t brdCol = selected ? 0x70d77aff : (multiSel ? 0x3ec87aff : 0x354851ff);
            drawPanel(btn, rgba(bgCol), rgba(brdCol));

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

    void drawMeta3DWaveform(const Rect &r, const synth::WavetablePartialSlot &slot, int trackIndex = -1)
    {
        drawPanel(r, rgba(0x080e14ff), rgba(0x1a2830ff));
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

            // 线条颜色：morph 帧亮黄绿，其他暗青
            const uint32_t lineCol = isMorph
                ? 0x9eff50ff
                : uint32_t(0x1a4830ff | (uint32_t(80 + int(80.0f * depthT)) << 8));
            const float lineW = isMorph ? 2.0f : (0.7f + 0.6f * depthT);

            auto &frame = slot.frames[(size_t)frameIdx];

            // 绘制填充区（仅 morph 帧做半透明填充）
            if(isMorph)
            {
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
                fillColor(rgba(0x9eff5014));
                fill();
            }

            // 波形线
            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yCenter - sampleFrameWarped(frame, t, slot.warpMode, slot.warpAmount) * ampScale;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(rgba(lineCol));
            strokeWidth(lineW);
            stroke();

            // 基线（morph 帧用亮色）
            if(isMorph)
            {
                strokeLine(xL, yCenter, xR, yCenter, rgba(0x9eff5030), 0.5f);
                // 帧号标注
                fontSize(9.0f);
                fillColor(rgba(0x9eff50c0));
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
        drawPanel(r, rgba(0x101820ff), rgba(0x263842ff));
        strokeLine(r.x + 6.0f, r.y + r.h * 0.5f, r.x + r.w - 6.0f, r.y + r.h * 0.5f, rgba(0x2b3f48ff), 1.0f);

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
        strokeColor(rgba(0x63d2ffff));
        strokeWidth(1.5f);
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
            fillColor(h == selectedMetaHarmonic_ ? rgba(0x8be87dff) : rgba(0x4d7780bb));
            fill();
        }
    }

    void drawKeyboard()
    {
        keyboardRect_ = { 16.0f, static_cast<float>(getHeight()) - 86.0f, static_cast<float>(getWidth()) - 32.0f, 70.0f };
        drawPanel(keyboardRect_, rgba(0x11181dff), rgba(0x293842ff));

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
            rect(r.x, r.y, r.w, r.h);
            fillColor(pressed ? rgba(0x67d36dff) : (black ? rgba(0x101010ff) : rgba(0xf1f4f0ff)));
            fill();
        }
    }

    void drawAdsrCurve(const Rect &r)
    {
        drawPanel(r, rgba(0x0c1115ff), rgba(0x344852ff));
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
        strokeColor(rgba(0x8be87dff));
        strokeWidth(2.0f);
        stroke();
    }

    void drawAdsrCurve(const Rect &r, const synth::AdsrParams &env)
    {
        drawPanel(r, rgba(0x0c1115ff), rgba(0x344852ff));
        const float a = std::max(0.01f, env.attack);
        const float d = std::max(0.01f, env.decay);
        const float rr = std::max(0.01f, env.release);
        const float sum = a + d + rr + 0.5f;
        const float x0 = r.x + 10.0f;
        const float xA = r.x + r.w * (a / sum);
        const float xD = r.x + r.w * ((a + d) / sum);
        const float xS = r.x + r.w * ((a + d + 0.5f) / sum);
        const float xR = r.x + r.w - 10.0f;
        const float y0 = r.y + r.h - 8.0f;
        const float y1 = r.y + 8.0f;
        const float yS = r.y + r.h - 8.0f - env.sustain * (r.h - 16.0f);

        beginPath();
        moveTo(x0, y0);
        lineTo(xA, y1);
        lineTo(xD, yS);
        lineTo(xS, yS);
        lineTo(xR, y0);
        strokeColor(rgba(0x8be87dff));
        strokeWidth(2.0f);
        stroke();
    }

    void drawMatrixEnvCurve(const Rect &r, const synth::MatrixEnvParams &env)
    {
        drawPanel(r, rgba(0x101820ff), rgba(0x263842ff));
        beginPath();
        const int count = clampi(env.pointCount, 2, synth::kMaxMatrixEnvPoints);
        for(int s = 0; s < 80; ++s)
        {
            const float x = float(s) / 79.0f;
            const float yv = synth::matrixEnvBreakpointEval(env, x);
            const float px = r.x + 8.0f + x * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - yv * (r.h - 10.0f);
            if(s == 0) moveTo(px, py); else lineTo(px, py);
        }
        (void)count;
        strokeColor(rgba(0x8be87dff));
        strokeWidth(2.0f);
        stroke();
        for(int i = 0; i < count; ++i)
        {
            const auto &pt = env.points[(size_t)i];
            const float px = r.x + 8.0f + clampf(pt.x, 0.0f, 1.0f) * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - clampf(pt.y, 0.0f, 1.0f) * (r.h - 10.0f);
            beginPath();
            circle(px, py, i == selectedEnvPoint_ ? 5.0f : 3.5f);
            fillColor(i == selectedEnvPoint_ ? rgba(0xffd166ff) : rgba(0x63d2ffff));
            fill();
        }
    }

    void drawSectionTitle(float x, float y, const char *title)
    {
        useUiFont();
        uiFontSize(15.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(rgba(0xc8d6dcff));
        text(x, y, title, nullptr);
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
        const float rad  = sz * 0.5f - 3.5f;
        const float cx   = tall ? r.x + r.w * 0.5f : r.x + sz * 0.5f;
        const float cy   = tall ? r.y + sz * 0.5f + 2.0f : r.y + r.h * 0.5f;

        // Background circle
        beginPath();
        circle(cx, cy, rad + 3.5f);
        fillColor(rgba(0x12202aff));
        fill();
        strokeColor(rgba(0x2a3e4aff));
        strokeWidth(1.0f);
        stroke();

        const float kStart = kPi * 0.75f;
        const float kEnd   = kPi * 2.25f;
        const float kAngle = kStart + clampf(norm, 0.0f, 1.0f) * (kEnd - kStart);
        const float sw     = rad >= 14.0f ? 3.0f : 2.0f;

        // Track arc
        beginPath();
        arc(cx, cy, rad, kStart, kEnd, CCW);
        strokeColor(rgba(0x243340ff));
        strokeWidth(sw);
        stroke();

        // Fill arc
        if(norm > 0.001f)
        {
            beginPath();
            arc(cx, cy, rad, kStart, kAngle, CCW);
            strokeColor(rgba(0x63d2ffff));
            strokeWidth(sw);
            stroke();
        }

        // Pointer line
        const float pLen = rad * 0.60f;
        beginPath();
        moveTo(cx, cy);
        lineTo(cx + std::cos(kAngle) * pLen, cy + std::sin(kAngle) * pLen);
        strokeColor(rgba(0xe8f4f8ff));
        strokeWidth(1.5f);
        stroke();

        // Center dot
        beginPath();
        circle(cx, cy, rad >= 14.0f ? 2.5f : 1.5f);
        fillColor(rgba(0xe8f4f8ff));
        fill();

        // Label and value
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3g", value);
        useUiFont();
        if(tall)
        {
            uiFontSize(10.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(rgba(0x8aaab8ff));
            text(cx, r.y + sz + 2.0f, label, nullptr);
            uiFontSize(11.5f);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(rgba(0xd0e0e8ff));
            text(cx, r.y + sz + 14.0f, buf, nullptr);
        }
        else
        {
            const float tx = r.x + sz + 5.0f;
            uiFontSize(10.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(rgba(0x8aaab8ff));
            text(tx, cy - 5.5f, label, nullptr);
            uiFontSize(12.0f);
            fillColor(rgba(0xd0e0e8ff));
            text(tx, cy + 6.0f, buf, nullptr);
        }
    }

    void drawSlider(const Rect &r, const char *label, float norm, float value)
    {
        drawKnob(r, label, norm, value);
    }

    void drawLabelBox(const Rect &r, const char *textValue)
    {
        drawPanel(r, rgba(0x162027ff), rgba(0x354851ff));
        useUiFont();
        uiFontSize(13.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(rgba(0xd8e3e6ff));
        text(r.x + 10.0f, r.y + r.h * 0.5f, textValue, nullptr);
    }

    void drawButton(const Rect &r, const char *label, bool active)
    {
        drawPanel(r, active ? rgba(0x263840ff) : rgba(0x151d22ff), active ? rgba(0x70d77aff) : rgba(0x354851ff));
        useUiFont();
        uiFontSize(13.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(active ? rgba(0xf4fff4ff) : rgba(0xd5e1e6ff));
        text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, label, nullptr);
    }

    void drawPanel(const Rect &r, Color fillValue, Color strokeValue)
    {
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 6.0f);
        fillColor(fillValue);
        fill();
        strokeColor(strokeValue);
        strokeWidth(1.0f);
        stroke();
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
        if(harmonicEditorCloseRect_.contains(x, y))
        {
            harmonicEditorOpen_ = false;
            metaProcessContextMenuOpen_ = false;
            dragTarget_ = DragTarget::None;
            return true;
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
                const int frameIndex = metaFramePageStart_ + local;
                if(auto *track = currentTrack(); track != nullptr && frameIndex < track->metaOsc.frameCount)
                {
                    if(ctrlDown_)
                    {
                        if(metaFrameRangeAnchor_ < 0 || metaFrameRangeAnchor_ >= track->metaOsc.frameCount)
                        {
                            metaFrameRangeAnchor_ = frameIndex;
                            metaFrameSelected_.fill(false);
                            metaFrameSelected_[(size_t)frameIndex] = true;
                            selectedMetaFrame_ = frameIndex;
                            return true;
                        }
                        const int first = std::min(metaFrameRangeAnchor_, frameIndex);
                        const int last = std::max(metaFrameRangeAnchor_, frameIndex);
                        metaFrameSelected_.fill(false);
                        for(int i = first; i <= last; ++i)
                            metaFrameSelected_[(size_t)i] = true;
                        selectedMetaFrame_ = frameIndex;
                    }
                    else
                    {
                        metaFrameSelected_.fill(false);
                        selectedMetaFrame_ = frameIndex;
                        metaFrameSelected_[(size_t)frameIndex] = true;
                        metaFrameRangeAnchor_ = frameIndex;
                    }
                }
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
                presetNameEditing_ = true;
                skipNextPresetCharacterInput_ = false;
                return true;
            }
            if(presetMenuNewRect_.contains(x, y))
            {
                selectedPresetIndex_ = -1;
                presetNameBuffer_.clear();
                presetNameEditing_ = true;
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
                    presetNameEditing_ = false;
                    skipNextPresetCharacterInput_ = false;
                    return true;
                }
            }
            if(presetMenuSaveRect_.contains(x, y))
            {
                presetNameEditing_ = false;
                skipNextPresetCharacterInput_ = false;
                releaseAllUiNotes();
                if(auto *p = plugin())
                    p->saveUserPreset(presetNameBuffer_.empty() ? nullptr : presetNameBuffer_.c_str());
                pullFromPlugin();
                for(int i = 0; i < int(presetNames_.size()); ++i)
                    if(presetNames_[(size_t)i] == presetNameBuffer_)
                        selectedPresetIndex_ = i;
                return true;
            }
            if(presetMenuLoadRect_.contains(x, y))
            {
                presetNameEditing_ = false;
                skipNextPresetCharacterInput_ = false;
                releaseAllUiNotes();
                const char *name = nullptr;
                if(!presetNameBuffer_.empty())
                    name = presetNameBuffer_.c_str();
                else if(selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
                    name = presetNames_[(size_t)selectedPresetIndex_].c_str();
                if(auto *p = plugin())
                    p->loadUserPreset(name);
                pullFromPlugin();
                return true;
            }
            if(presetMenuDeleteRect_.contains(x, y))
            {
                presetNameEditing_ = false;
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
        return false;
    }

    bool handlePageClick(float x, float y)
    {
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
            if(metaWavetableNameRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                wavetablePresetMenuOpen_ = !wavetablePresetMenuOpen_;
                presetMenuOpen_ = false;
                if(wavetablePresetMenuOpen_)
                    refreshWavetablePresets();
                return true;
            }
            if((metaWavetablePrevRect_.contains(x, y) || metaWavetableNextRect_.contains(x, y))
               && track->type == synth::SourceTrackType::MetaOscillator)
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
            if(metaLoadRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                beginWavetableImport();
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
                    const int frameIndex = metaFramePageStart_ + local;
                    if(frameIndex < track->metaOsc.frameCount)
                        selectedMetaFrame_ = frameIndex;
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
                const int frameIndex = metaFramePageStart_ + local;
                if(frameIndex < metaSlot.frameCount)
                    selectedMetaFrame_ = frameIndex;
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
                beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Lfo1) + i),
                                  lfoSelectRects_[(size_t)i], x, y);
                return true;
            }
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
            if(envSelectRects_[(size_t)i].contains(x, y))
            {
                selectedEnv_ = i;
                beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Env1) + i),
                                  envSelectRects_[(size_t)i], x, y);
                return true;
            }
        if(adsrSourceRect_.w > 0.0f && adsrSourceRect_.contains(x, y))
        {
            beginModRouteDrag(synth::ModSource::Adsr, adsrSourceRect_, x, y);
            return true;
        }
        for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
            if(ampEnvTabRects_[(size_t)i].contains(x, y)) { selectedAmpEnv_ = i; return true; }

        auto &lfo = lfos_[(size_t)selectedLfo_];
        auto &env = envs_[(size_t)selectedEnv_];
        if(lfoEnableRect_.contains(x, y)) { lfo.enabled = !lfo.enabled; pushLfoOnly(); return true; }
        if(lfoShapeRect_.contains(x, y)) { lfo.shape = static_cast<synth::LfoShape>((int(lfo.shape) + 1) % 5); pushLfoOnly(); return true; }
        if(envEnableRect_.contains(x, y)) { env.enabled = !env.enabled; pushEnvOnly(); return true; }

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

        // Partial bank bar chart (absolute position click)
        if(partialSpectrumRect_.contains(x, y)) return setDragAbs(DragTarget::PartialAmp);
        if(partialAmpRect_.contains(x, y)) {
            auto *ptrack = track;
            float n = 0.0f;
            if(ptrack && ptrack->type == synth::SourceTrackType::PartialBank)
                n = ptrack->partialBank.partials[(size_t)selectedPartialIndex_].amp;
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
            int cnt = track ? track->partialBank.partialCount : generator_.wavetableSeed.partialCount;
            return setDragKnob(DragTarget::PartialCount, float(cnt - 1) / 63.0f);
        }
        if(inharmonicRect_.contains(x, y)) {
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

        // Pitch controls (OCT/SEM/FIN/CRS) — 只在 MetaOscillator source track 里
        if(track && track->type == synth::SourceTrackType::MetaOscillator)
        {
            auto &ms = track->metaOsc;
            if(metaOctRect_.contains(x, y))
            {
                pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchOct;
                dragStartY_   = y;
                dragStartOct_ = ms.pitchOct;
                return true;
            }
            if(metaSemRect_.contains(x, y))
            {
                pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchSem;
                dragStartY_   = y;
                dragStartSem_ = ms.pitchSem;
                return true;
            }
            if(metaFinRect_.contains(x, y))
            {
                pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchFin;
                dragStartY_   = y;
                dragStartFin_ = ms.pitchFin;
                return true;
            }
            if(metaCrsRect_.contains(x, y))
            {
                pushMetaUndoSnapshot();
                dragTarget_   = DragTarget::MetaPitchCrs;
                dragStartY_   = y;
                dragStartCrs_ = ms.pitchCrs;
                return true;
            }
        }

        {
            bool isMeta = track && track->type == synth::SourceTrackType::MetaOscillator;
            auto &ms = isMeta ? track->metaOsc : metaSlot;
            if(metaRatioRect_.contains(x, y))
                return setDragMetaKnob(DragTarget::MetaRatio, std::min(1.0f, std::log2(std::max(0.01f, ms.ratio)) / 7.0f));
            if(metaAmpRect_.contains(x, y))         return setDragMetaKnob(DragTarget::MetaAmp,        ms.amp);
            if(metaPhaseRect_.contains(x, y))       return setDragMetaKnob(DragTarget::MetaPhase,      (ms.phase + kPi) / (2.0f * kPi));
            if(metaPanRect_.contains(x, y))         return setDragMetaKnob(DragTarget::MetaPan,        (ms.pan + 1.0f) * 0.5f);
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
        if(lfoFreqRect_.contains(x, y))  return setDragKnob(DragTarget::LfoFreq,  lfo.frequencyHz / 20.0f);
        if(lfoPhaseRect_.contains(x, y)) return setDragKnob(DragTarget::LfoPhase, lfo.phase0);
        if(lfoRhoRect_.contains(x, y))   return setDragKnob(DragTarget::LfoRho,   lfo.rhoLfo);
        if(envPointARect_.contains(x, y)) return setDragKnob(DragTarget::EnvPointA, env.points[1].y);
        if(envPointBRect_.contains(x, y)) return setDragKnob(DragTarget::EnvPointB, env.points[2].y);
        if(envCurveARect_.contains(x, y)) return setDragKnob(DragTarget::EnvCurveA, (env.points[1].curve + 1.0f) * 0.5f);
        if(matrixEnvCurveRect_.contains(x, y))
        {
            selectedEnvPoint_ = -1;
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
            dragScrollStartVal_ = stripScrollOffset_;
            return true;
        }

        // Layout divider drags
        if(layoutVSplitHandle_.contains(x, y))
        {
            dragTarget_ = DragTarget::LayoutVSplit;
            dragStartY_ = y;
            dragStartLayoutRatio_ = layoutBottomRatio_;
            return true;
        }
        if(layoutRackSplitHandle_.contains(x, y))
        {
            dragTarget_ = DragTarget::LayoutRackSplit;
            dragStartY_ = x;  // repurpose dragStartY_ as startX
            dragStartLayoutRatio_ = layoutRackRatio_;
            return true;
        }
        if(layoutStripSplitHandle_.contains(x, y))
        {
            dragTarget_ = DragTarget::LayoutStripSplit;
            dragStartY_ = x;
            dragStartLayoutRatio_ = layoutStripRatio_;
            return true;
        }
        return false;
    }

    void applyDragValue(float x, float y)
    {
        // Absolute horizontal position (scrollbars, waveform editors, bar charts)
        const auto normIn = [&](const Rect &r) { return clampf((x - r.x) / std::max(1.0f, r.w), 0.0f, 1.0f); };
        // Vertical delta-based knob: drag up = increase (200 px = full range, fine with modifier)
        const auto knobNorm = [&]() -> float {
            return clampf(dragStartNorm_ + (dragStartY_ - y) / 200.0f, 0.0f, 1.0f);
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
                if(auto *track = currentTrack()) { track->gain = knobNorm() * 2.0f; pushCurrentTrack(); }
                break;
            case DragTarget::TrackPan:
                if(auto *track = currentTrack()) { track->pan = knobNorm() * 2.0f - 1.0f; pushCurrentTrack(); }
                break;
            case DragTarget::TrackSend:
                if(auto *track = currentTrack()) { track->send = knobNorm(); pushCurrentTrack(); }
                break;
            case DragTarget::PartialAmp:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    if(partialSpectrumRect_.contains(x, y))
                        selectedPartialIndex_ = clampi(int((x - partialSpectrumRect_.x) / std::max(1.0f, partialSpectrumRect_.w)
                                                           * float(synth::kMaxWavetablePartials)),
                                                       0, synth::kMaxWavetablePartials - 1);
                    auto &slot = track->partialBank.partials[(size_t)selectedPartialIndex_];
                    slot.enabled = true;
                    slot.amp = partialSpectrumRect_.contains(x, y)
                                   ? clampf(1.0f - (y - partialSpectrumRect_.y) / std::max(1.0f, partialSpectrumRect_.h), 0.0f, 1.0f)
                                   : knobNorm();
                    if(selectedPartialIndex_ + 1 > track->partialBank.partialCount)
                        track->partialBank.partialCount = selectedPartialIndex_ + 1;
                    pushCurrentTrack();
                }
                break;
            case DragTarget::PartialRatio:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &slot = track->partialBank.partials[(size_t)selectedPartialIndex_];
                    slot.ratio = 0.01f + knobNorm() * 63.99f;
                    pushCurrentTrack();
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
                    pushCurrentTrack();
                }
                else
                {
                    generator_.wavetableSeed.partialCount = clampi(1 + int(std::round(knobNorm() * 63.0f)), 1, 64);
                    pushGenerator();
                }
                break;
            case DragTarget::Inharmonic:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.inharmonicAmount = knobNorm();
                    pushCurrentTrack();
                }
                else { generator_.wavetableSeed.inharmonicAmount = knobNorm(); pushGenerator(); }
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
                break;
            case DragTarget::MetaPitchSem:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const int delta = int((dragStartY_ - y) / 12.0f);
                    track->metaOsc.pitchSem = std::max(-12, std::min(12, dragStartSem_ + delta));
                    track->metaOsc.syncRatioFromPitch();
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
                break;
            case DragTarget::MetaPitchCrs:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const float delta = (dragStartY_ - y) * 0.1f;
                    track->metaOsc.pitchCrs = clampf(dragStartCrs_ + delta, -100.0f, 100.0f);
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaFrameScan:
                selectedMetaFrame_ = clampi(int(normIn(metaFrameStripRect_) * float(std::max(1, metaSlot.frameCount))),
                                            0, std::max(0, metaSlot.frameCount - 1));
                metaSlot.morph = metaSlot.frameCount > 1
                                     ? float(selectedMetaFrame_) / float(metaSlot.frameCount - 1)
                                     : 0.0f;
                pushMetaPartialRuntime();
                break;
            case DragTarget::MetaHarmonicRatio:
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
                if(auto *t2 = currentTrack(); t2 != nullptr && t2->type == synth::SourceTrackType::MetaOscillator)
                {
                    const int maxScroll = std::max(0, t2->metaOsc.frameCount - synth::kVisibleWavetableFrames);
                    if(maxScroll > 0)
                    {
                        const float pixPerStep = std::max(1.0f, metaFrameScrollRect_.w / float(maxScroll));
                        const int delta = int((x - dragScrollStartX_) / pixPerStep + 0.5f);
                        metaFrameScrollStart_ = clampi(dragScrollStartVal_ + delta, 0, maxScroll);
                    }
                }
                break;
            case DragTarget::LfoFreq: lfo.frequencyHz = knobNorm() * 20.0f; pushLfoOnly(); break;
            case DragTarget::LfoPhase: lfo.phase0 = knobNorm(); pushLfoOnly(); break;
            case DragTarget::LfoRho: lfo.rhoLfo = knobNorm(); pushLfoOnly(); break;
            case DragTarget::EnvPointA: env.points[1].y = knobNorm(); pushEnvOnly(); break;
            case DragTarget::EnvPointB: env.points[2].y = knobNorm(); pushEnvOnly(); break;
            case DragTarget::EnvCurveA: env.points[1].curve = knobNorm() * 2.0f - 1.0f; pushEnvOnly(); break;
            case DragTarget::MatrixEnvCurve: editMatrixEnvCurve(x, y); break;
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
                    const float pixPerStep = std::max(1.0f, sbW / float(totalScroll2));
                    const int delta = int((x - dragScrollStartX_) / pixPerStep + 0.5f);
                    stripScrollOffset_ = clampi(dragScrollStartVal_ + delta, 0, totalScroll2);
                }
                break;
            }
            case DragTarget::LayoutVSplit:
            {
                const float pageH = static_cast<float>(getHeight()) - 184.0f;
                const float delta = (dragStartY_ - y) / std::max(1.0f, pageH);
                layoutBottomRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.18f, 0.72f);
                break;
            }
            case DragTarget::LayoutRackSplit:
            {
                const float pageW = static_cast<float>(getWidth()) - 32.0f;
                const float delta = (x - dragStartY_) / std::max(1.0f, pageW);
                layoutRackRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.11f, 0.38f);
                break;
            }
            case DragTarget::LayoutStripSplit:
            {
                const float pageW = static_cast<float>(getWidth()) - 32.0f;
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

    void editMatrixEnvCurve(float x, float y)
    {
        auto &env = envs_[(size_t)selectedEnv_];
        env.pointCount = clampi(env.pointCount, 2, synth::kMaxMatrixEnvPoints);
        const float nx = clampf((x - (matrixEnvCurveRect_.x + 8.0f)) / std::max(1.0f, matrixEnvCurveRect_.w - 16.0f), 0.0f, 1.0f);
        const float ny = clampf(1.0f - (y - (matrixEnvCurveRect_.y + 5.0f)) / std::max(1.0f, matrixEnvCurveRect_.h - 10.0f), 0.0f, 1.0f);

        if(selectedEnvPoint_ < 0 || selectedEnvPoint_ >= env.pointCount)
        {
            float best = 1.0e9f;
            selectedEnvPoint_ = 0;
            for(int i = 0; i < env.pointCount; ++i)
            {
                const float dx = env.points[(size_t)i].x - nx;
                const float dy = env.points[(size_t)i].y - ny;
                const float d = dx * dx + dy * dy;
                if(d < best)
                {
                    best = d;
                    selectedEnvPoint_ = i;
                }
            }
        }

        auto &pt = env.points[(size_t)selectedEnvPoint_];
        if(selectedEnvPoint_ == 0)
        {
            pt.x = 0.0f;
            pt.y = ny;
        }
        else if(selectedEnvPoint_ == env.pointCount - 1)
        {
            pt.x = 1.0f;
            pt.y = ny;
        }
        else
        {
            const float lo = env.points[(size_t)selectedEnvPoint_ - 1].x + 0.01f;
            const float hi = env.points[(size_t)selectedEnvPoint_ + 1].x - 0.01f;
            pt.x = clampf(nx, lo, hi);
            pt.y = ny;
        }
        pushEnvOnly();
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
    bool ctrlDown_ = false;
    int selectedMetaHarmonic_ = 0;
    int selectedEnvPoint_ = -1;
    int mouseKey_ = -1;
    std::array<bool, 128> computerKeys_ {};
    std::array<int, 512> pressedKeycodeNotes_ {};
    DragTarget dragTarget_ = DragTarget::None;
    float dragStartY_    = 0.0f;
    float dragStartNorm_ = 0.0f;
    float dragStartDepth_ = 0.0f;
    float dragDepthLimit_ = 1.0f;
    float dragStartLayoutRatio_ = 0.0f;
    float layoutBottomRatio_ = 0.48f;
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
    int  stripScrollOffset_ = 0;
    Rect stripScrollbarRect_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripMuteRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripSoloRects_ {};
    std::vector<StripGroup> stripGroups_;
    bool stripGroupContextMenuOpen_ = false;
    float stripGroupContextX_ = 0.0f, stripGroupContextY_ = 0.0f;
    std::array<Rect, 2> stripGroupContextRects_ {};
    int  groupContextTargetGroup_ = -1;  // >=0 => ungroup menu for this group; -1 => create-group menu
    std::array<Rect, synth::kMaxSourceTracks> stripGroupBusRects_ {};
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
    float modeMenuX_ = 0.0f, modeMenuY_ = 0.0f;
    std::array<Rect, 12> modeMenuRects_ {};
    bool addTrackMenuOpen_ = false;
    bool presetNameEditing_ = false;
    bool skipNextPresetCharacterInput_ = false;
    std::string presetLabel_ = "Select preset";
    std::string presetNameBuffer_ = "user_kapibara";
    std::vector<std::string> presetNames_ {};
    int selectedPresetIndex_ = -1;
    std::vector<WavetablePresetEntry> wavetablePresets_ {};
    int selectedWavetablePresetIndex_ = -1;
    std::string wavetablePresetLabel_ = "Select Wavetable";
    std::string loadPathBuffer_ {};
    std::string lastLoadPath_ {};
    std::string browserStartDir_ {};
    std::string loadStatus_ = "type wav path after Load";
    std::string metaEditorStatus_ = "ready";
    float uiScale_ = 1.0f;
    char scratch_[64] {};

    Rect toolbar_ {}, panicRect_ {}, statusRect_ {}, keyboardRect_ {};
    Rect presetPrevRect_ {}, presetSelectRect_ {}, presetNextRect_ {}, presetSaveRect_ {}, presetLoadRect_ {};
    Rect menuRect_ {}, aboutRect_ {}, presetMenuPanelRect_ {}, presetSearchRect_ {}, presetListRect_ {};
    Rect presetMenuNewRect_ {}, presetMenuSaveRect_ {}, presetMenuLoadRect_ {}, presetMenuDeleteRect_ {}, presetMenuResetRect_ {};
    Rect optionsMenuPanelRect_ {}, uiScaleRect_ {};
    Rect wavetableImportPanelRect_ {}, wavetableImportCancelRect_ {};
    Rect wavetablePresetPanelRect_ {}, wavetablePresetListRect_ {};
    Rect wavetablePresetLoadRect_ {}, wavetablePresetImportRect_ {}, wavetablePresetRefreshRect_ {}, wavetablePresetCloseRect_ {};
    std::array<Rect, 5> wavetableBuiltinRects_ {};
    std::array<Rect, 8> wavetablePresetRowRects_ {};
    Rect metaProcessContextPanelRect_ {};
    std::array<Rect, 7> metaProcessContextRects_ {};
    float metaProcessContextX_ = 0.0f;
    float metaProcessContextY_ = 0.0f;
    Rect manualCycleMinusRect_ {}, manualCyclePlusRect_ {}, manualCycleValueRect_ {};
    std::array<Rect, 3> importFrameLimitRects_ {};
    std::array<Rect, 5> wavetableImportModeRects_ {};
    std::array<Rect, 6> presetRowRects_ {};
    Rect partialCountRect_ {}, inharmonicModeRect_ {}, inharmonicRect_ {}, gainRect_ {};
    std::array<Rect, 4> sourceCountRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> sourceChainRects_ {};
    Rect addTrackRect_ {};
    Rect removeTrackRect_ {};
    std::array<Rect, 4> addTrackTypeRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> trackRowRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripRects_ {};
    Rect trackOutputModeRect_ {}, trackGainRect_ {}, trackPanRect_ {}, trackSendRect_ {};
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
    Rect metaLoadRect_ {}, metaLoadPathRect_ {}, metaFrameStripRect_ {};
    std::array<Rect, 5> metaFramePresetRects_ {};
    std::array<Rect, synth::kVisibleWavetableFrames> metaFrameRects_ {};
    int metaFramePageStart_ = 0;
    int metaFrameScrollStart_ = 0;
    float dragScrollStartX_ = 0.0f;
    int dragScrollStartVal_ = 0;
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
