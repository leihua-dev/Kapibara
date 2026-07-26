#pragma once

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

#include "KapibaraUIShared.h"
#include "KapibaraUIDrawing.h"

class KapibaraUI final : public KapibaraUIDrawing
{
  public:
    // KapibaraUILifecycle
    KapibaraUI();

  protected:
    // KapibaraUIDisplay
    void onNanoDisplay() override;
    void drawInsertDragGhost();
    void onResize(const ResizeEvent &ev) override;
    void uiIdle() override;
    void uiFocus(bool focus, DGL_NAMESPACE::CrossingMode mode) override;

    // input/KapibaraUIEvents
    bool onKeyboard(const KeyboardEvent &ev) override;
    bool onCharacterInput(const CharacterInputEvent &ev) override;
    bool onMouse(const MouseEvent &ev) override;
    bool onMotion(const MotionEvent &ev) override;
    void uiFileBrowserSelected(const char *filename) override;
    uint32_t uiClipboardDataOffer() override;
    void uiClipboardData(const char *mimeType, const void *data, size_t dataSize) override;

  private:
    // sync/KapibaraUIPluginSync
    KapibaraPlugin *plugin() const;
    void pullFromPlugin();
    void pushGenerator();
    void pushSource();
    void pushCurrentTrack();
    void pushAllTracks();
    uint64_t uiNowMs() const;
    bool realtimeDragPushDue(uint64_t &lastPushMs, bool force);
    void pushCurrentTrackDuringRealtimeDrag(bool force = false);
    void pushGeneratorDuringRealtimeDrag(bool force = false);
    synth::SourceTrackParams *dragTrack();
    void pushCurrentTrackMorphOnly();
    void pushTrackById(uint32_t id);
    void deleteSelectedTrack();
    void pushGroups();
    void pushMetaUndoSnapshot();
    bool undoMeta();
    void pushMetaPartial();
    void pushMetaPartialRuntime();
    void pushAdsr();
    void pushGain();
    void pushMatrix();
    void pushCurModSlot();
    void pushRuleOnly();
    void pushRule(int idx);
    void pushGroup(int idx);
    void pushChaosOnly();
    void pushShapeOnly();
    void pushAmpEnv();
    void pushEffects();

    // KapibaraUIToolbar
    float masterLevel();
    void drawMasterMeter(const Rect &r);
    void drawBackground();
    void drawToolbar();

    // input/KapibaraUIKeyboardInput
    int keyAt(float x, float y) const;
    bool handleKeyboardPress(float x, float y);
    void pressMouseKey(int note);
    void releaseMouseKey();
    int keycodeSlot(uint keycode) const;
    void releaseComputerNote(int note, int keySlot);
    void releaseAllUiNotes();
    void clearUiNoteState();

    // menus/KapibaraUIPresetMenu
    void drawPresetMenu();

    // menus/KapibaraUIWavetablePresetMenu
    void drawWavetablePresetMenu();
    void refreshWavetablePresets();
    bool loadWavetablePreset(int index);
    bool handleWavetablePresetMenuClick(float x, float y);

    // menus/KapibaraUIOptionsMenu
    void drawOptionsMenu();

    // menus/KapibaraUIWavetableImportMenu
    void drawWavetableImportMenu();
    bool handleWavetableImportMenuClick(float x, float y);

    // fx/KapibaraUIInsertMenus
    std::vector<InsertEffect> *insertChainFor(int trackId, int mergeIdx);
    void commitChainChange(int trackId, int group);
    void openInsertMenu(int trackId, int mergeIdx, float x, float y);
    void drawInsertMenu();
    bool handleInsertMenuClick(float x, float y);
    int fxModeCount(int kind) const;
    const char *fxModeName(int kind, int i);
    int fxCurrentMode(const InsertEffect &ins, int kind);
    void openModeMenu(int trackId, int mergeIdx, int insertIdx, int kind, float x, float y);
    void drawModeMenu();
    void commitModeMenuSelection();
    bool handleModeMenuClick(float x, float y);
    bool handleInsertButtonClick(float x, float y);
    void finishInsertInteraction(float x, float y);

    // menus/KapibaraUIRouteContextMenu
    void drawRouteContextMenu();
    bool handleRouteContextMenuClick(float x, float y);

    // menus/KapibaraUIMetaProcessMenu
    void drawMetaProcessContextMenu();
    void openMetaProcessContextMenu(float x, float y);
    bool handleMetaProcessContextMenuClick(float x, float y);

    // source/KapibaraUISourceList
    void drawSourceRack(const Rect &r);
    synth::SourceTrackParams *currentTrack();
    static float modulationDepthLimit(synth::ModDestination destination);
    static float defaultModulationDepth(synth::ModDestination destination);
    static Color modulationSourceColor(synth::ModSource source);
    ModRouteTarget modRouteTargetAt(float x, float y) const;
    Rect modulationDestinationRect(const synth::MatrixRule &rule) const;

    // KapibaraUIModRoute
    void beginModRouteDrag(synth::ModSource source, const Rect &sourceRect, float x, float y);
    void finishModRouteDrag(float x, float y);
    bool handleModDepthPress(float x, float y);
    void drawModulationOverlays();

    // fx/KapibaraUIInsertPanel
    void drawRouteFxEditor(const Rect &region, std::vector<InsertEffect> *chain,
                           int chainTrackId, int chainGroup);
    void drawInsertPanel(const Rect &p, InsertEffect &e, int trackId, int mergeIdx, int insertIdx);
    static bool fxHasGraph(int kind);
    static float biquadMagnitude(const synth::BiquadCoeffs &c, float w);
    static float biquadPhase(const synth::BiquadCoeffs &c, float w);
    static synth::BiquadCoeffs sourceFilterBiquad(const synth::SourceFilterParams &f, double sampleRate);
    void drawSourceFilterGraph(const Rect &g, const synth::SourceFilterParams &f, bool phase);
    void drawInsertGraph(const Rect &g, const InsertEffect &e);
    bool handleRouteFxClick(float x, float y);

    // KapibaraUIModEditor
    bool modSourceCausesCycle(int targetIdx, int srcIdx) const;
    static bool trackHasAnyMod(const synth::SourceTrackParams &t);
    void drawModEditor(const Rect &region, synth::SourceTrackParams &track);
    bool handleModColumnClick(float x, float y);
    bool handleModEditorClick(float x, float y);
    uint32_t adoptStripInsert(int fromTrackIdx, int insIdx, synth::SourceTrackParams &to);
    void openModSourceMenu(int trackId, int slot, float x, float y);
    void drawModSourceMenu();
    bool modSourceCausesCycleFor(int selfIdx, int cand) const;
    bool handleModSourceMenuClick(float x, float y);

    // menus/KapibaraUIWarpModeMenu
    void openWarpModeMenu(uint32_t trackId, float x, float y);
    void drawWarpModeMenu();
    bool handleWarpModeMenuClick(float x, float y);

    // menus/KapibaraUIOscModTypeMenu
    void openOscModTypeMenu(int trackId, int slot, float x, float y);
    void drawOscModTypeMenu();
    bool handleOscModTypeMenuClick(float x, float y);
    bool handleOscModDotRightClick(float x, float y);
    bool handleModWireDrop(float x, float y);
    void drawOscModDiagram(const Rect &r);
    void drawMatrixView(const Rect &r);
    void oscModSourceLabel(const synth::SourceModEntry &m, char *buf, size_t n) const;

    // source/KapibaraUISourceEditor
    void drawGroupEditor(const Rect &r, int gi);
    void drawTrackEditor(const Rect &r);
    void drawVoiceTab(const Rect &r, synth::SourceTrackParams &track);
    void clearTrackEditorRects();
    std::vector<InsertEffect> *trackInsertsFor(uint32_t trackId);
    static bool trackHasRoutedFx(const synth::SourceTrackParams &t);
    static const char *insertKindShort(uint8_t kind);

    // source/KapibaraUISourceVisuals
    void drawInsertColumn(const Rect &region, const std::vector<InsertEffect> &inserts,
                          int trackId, int mergeIdx);
    static bool modEntryActive(const synth::SourceModEntry &m);
    void drawModColumn(const Rect &region, const synth::SourceTrackParams &track, int trackId);
    int trackIndexOfId(uint32_t id) const;
    int matrixRouteCountForTrack(const synth::SourceTrackParams &track) const;
    int activeModCountForTrack(const synth::SourceTrackParams &track) const;
    void drawFader(const Rect &r, float norm, const char *label, float value, bool active);
    void drawLevelMeter(const Rect &r, float level);
    void drawMeterScale(const Rect &meter);
    void drawStripThumbnail(const Rect &r, const synth::SourceTrackParams &track, int trackIndex, bool selected);
    void drawModulationMatrixPreview(const Rect &r);

    // router/KapibaraUIRouteBoardLegacy
    void drawStripRack(const Rect &r);

    // router/KapibaraUISourceRouter / KapibaraUIPerVoiceGrid / KapibaraUIRouteBoard / KapibaraUIRouteInspector
    void drawSourceRouter(const Rect &r);
    void drawPerVoiceGrid(const Rect &r);
    void drawStripGrid(const Rect &r);
    void drawInspector(const Rect &r);
    bool handleRouteGraphClick(float x, float y);
    void rebuildSelectedPerVoiceRouteFromWires();
    // Compiles the route wires into the per-voice DAG the engine evaluates
    // (filter nodes sum their inputs; track buses sum the nodes feeding them).
    synth::CompiledPerVoiceRoute buildCompiledRoute() const;
    // Walks the selected source forward through the graph, filling
    // selectedChainFilters_ / selectedChainInserts_ (crossing track boundaries).
    void computeSelectedSourceChain();
    // Per-voice filter params are globally shared across all source tracks (a
    // route-graph filter node feeds every signal passing through it). This
    // mirrors the edited track's filter slots onto every track and pushes them.
    void commitPerVoiceFilterEdit(synth::SourceTrackParams *editedTrack);
    void cleanupRouteGraphForCurrentTracks();
    bool handleRouteNodeContextClick(float x, float y);
    bool openRouteNodeContext(float x, float y);
    bool deleteRouteNode(uint32_t nodeId);
    bool deleteSelectedWire();
    void tryMergeNodeIntoWire(uint32_t nodeId);
    std::vector<synth::GridPoint> computeMovedPathSection(const std::vector<synth::GridPoint> &orig,
                                                          const synth::GridPoint &ptA,
                                                          const synth::GridPoint &ptB, int dx, int dy);

    // KapibaraUIEditorTracks
    void drawPartialBankLayerPreview(const Rect &r, synth::WavetableSeedParams &seed);
    void drawPartialBankTrackEditor(const Rect &r, synth::SourceTrackParams &track);
    void drawMetaTrackEditor(const Rect &r, synth::SourceTrackParams &track);
    void drawBasicTrackEditor(const Rect &r, synth::SourceTrackParams &track);
    void drawNoiseTrackEditor(const Rect &r, synth::SourceTrackParams &track);

    // source/KapibaraUISourceEnv
    int envUseCount(int envIndex) const;

    // KapibaraUIMatrix
    void drawMatrixDashboard(const Rect &r);
    void drawMatrixModulators(const Rect &r);
    void drawMatrixAmpEnv(const Rect &r);
    void drawGridAxisPicker();
    void enableModSource(synth::ModSource src);
    void drawMatrixRoutes(const Rect &r);
    void drawMaskGroups(const Rect &r);
    void drawMaskGroupPreview(const Rect &r, const synth::MaskGroup &g);
    bool handleMaskGroupsPress(float x, float y);
    void clearMatrixRects();
    // The matrix view accepts input only while it is actually the drawn top-row
    // branch (multiband / focused detail take priority and overdraw it).
    bool matrixViewInteractive() const
    {
        return matrixViewOpen_ && multibandEditorTrackId_ < 0 && focusedNodeId_ == 0;
    }
    void drawXferCurve(const Rect &r, float curve);
    bool handleMatrixRoutesPress(float x, float y);
    bool onScroll(const ScrollEvent &ev) override;

    // osc/partialbank/KapibaraUIHarmonicEditor
    void drawPartialTableEditor(synth::SourceTrackParams &track);
    void drawHarmonicEditor();

    // page/KapibaraUIPage
    void drawCurrentPage();
    void drawModSourceStrip(const Rect &r);
    void drawChainEditorPlaceholder(const Rect &r, const char *title, const char *hint);
    void drawBottomWorkspace(const Rect &full);
    bool handleBottomLayoutPress(float x, float y);

    // fx/KapibaraUIFocusedDetail
    void drawFocusedNodeDetail(const Rect &r);
    bool handleFocusedDetailPress(float x, float y);
    bool focusedNodeValid() const;

    // router/KapibaraUIRouteFocusedStructure
    void drawFocusedStructure(const Rect &r);
    int  componentOutPortCount(uint32_t nodeId) const;
    void drawComponentOutputPorts(const Rect &node, uint32_t nodeId);
    void ensureStructureDefault(uint32_t comp);
    bool handleStructurePress(float x, float y);
    void finishStructureNodeDrag(float x, float y);

    // fx/KapibaraUIFxRackEditor
    void drawFxRackEditor(const Rect &r);
    bool handleFxRackPress(float x, float y);
    void finishFxRackDrag(float x, float y);
    void drawMultibandFxEditor(const Rect &r);

    // pervoice/KapibaraUIPerVoiceChainEditor
    void drawPerVoiceChainEditor(const Rect &r);
    bool handlePerVoiceChainPress(float x, float y);
    int  globalPerVoiceFilterCount() const;
    void addPerVoiceFilterSlot();
    void addAmpEnvRouteNode(int index);

    // KapibaraUIMetaFrameVisuals
    void drawMetaPartialEditor(const Rect &r);
    void drawFrameScrollbar(const Rect &r, const synth::WavetablePartialSlot &slot);
    bool morphIsModulated(const synth::SourceTrackParams &t) const;
    void selectMetaFrameAt(int frameIndex, int frameCount);
    void selectAllMetaFrames();
    void drawMetaFrameStrip(const Rect &r, const synth::WavetablePartialSlot &slot);
    void drawPitchControl(const Rect &r, const char *label, int value, bool);
    void drawPitchControlF(const Rect &r, const char *label, float value);
    static float warpPhase01(synth::WavetableWarpMode mode, float amount, float x);
    static float sampleFrameWarped(const synth::WavetableFrame &frame, float t,
                                   synth::WavetableWarpMode mode, float amount);
    static float sampleFrame(const synth::WavetableFrame &frame, float t);

    // KapibaraUIWaveformVisuals
    void drawPlotBackground(const Rect &r, int columns = 6, int rows = 4);
    void drawMeta3DWaveform(const Rect &r, const synth::WavetablePartialSlot &slot, int trackIndex = -1);
    void drawMetaWaveformEditor(const Rect &r, const synth::WavetableFrame &frame);

    // KapibaraUIKeyboardVisuals
    void drawKeyboard();

    // KapibaraUIEnvelopeVisuals
    void drawAdsrCurve(const Rect &r);
    void drawAdsrCurve(const Rect &r, const synth::AdsrParams &env);
    void drawMatrixEnvCurve(const Rect &r, const synth::MatrixEnvPoint *points, int pointCount,
                            Color lineColor = DesignTokens::accentGreen());

    // KapibaraUIInputResetMeta
    bool handleDoubleClickReset(float x, float y);
    bool handleHarmonicEditorClick(float x, float y);

    // KapibaraUIInputNavigation
    bool handleToolbarClick(float x, float y);
    bool handleStripFaderPress(float x, float y);
    bool handlePageClick(float x, float y);
    bool handleButtonClick(float x, float y);

    // KapibaraUIInputControls
    bool handleControlPress(float x, float y);

    // KapibaraUIInputDrag
    void applyDragValue(float x, float y);
    bool applySourceDragValue(float x, float y);
    bool applyOscillatorDragValue(float x, float y);
    bool applyModFxLayoutDragValue(float x, float y);

    // KapibaraUIEditMatrixEnv
    synth::MatrixEnvPoint *curCurvePoints();
    int &curCurveCount();
    void pushCurCurve();
    bool &curCurveLoop();
    float &curCurveRate();
    static float snapEnvValue(float v, bool isX);
    float matrixEnvPx(float nx) const;
    float matrixEnvPy(float ny) const;
    float matrixEnvNx(float x) const;
    float matrixEnvNy(float y) const;
    int matrixEnvPointAt(float x, float y);
    int matrixEnvSegmentAt(float x);
    void addMatrixEnvPoint(float x, float y);
    void deleteMatrixEnvPoint(int idx);
    void editMatrixEnvCurve(float x, float y);

    // KapibaraUIEditPartials
    void editPartialTable(float x, float y, bool phaseMode);
    void editHarmonicEditor(float x, float y);

    // KapibaraUIEditMetaFrames
    bool performMetaFrameAction(int action);
    bool applyWavetableProcess(int op);
    bool applySelectedFrameMorph(int targetFrameCount);

    // KapibaraUIEditMetaDomain
    void editMetaDomain(float x, float y);
    void editSelectedSpectrumControl(float normalized, bool phaseControl);
    void editMetaWaveform(float x, float y);

    // KapibaraUIEditWavetableFiles
    void beginWavetableImport();
    void openWavetableFileBrowser();
    bool commitWavetableLoad();
    void refreshIrFiles();
    void loadImpulseIntoInsert(InsertEffect &e, const std::string &name, const std::string &path);
    void openWavetableSaveBrowser();
    void saveOrRenameWavetablePreset(const std::string &newStem);
    void saveWavetableToFile(const std::string &path);
    bool loadWavetableHarmonicFile(const std::string &path);

    // presets/KapibaraUIPresetState
    void appendPresetNameChar(char ch);
    static std::string trimPresetName(std::string s);
    static std::string safeFileStem(std::string s, const char *fallback);
    void beginSynthPresetRename();
    void beginWavetablePresetRename();
    void commitPresetNameEdit();
    void saveModernState(const std::string &path); // append tracks + routing + structure
    void loadModernState(const std::string &path); // parse + apply modern state

    // osc/partialbank/KapibaraUIPartialBankHelpers
    static void ensurePartialBankFrameDefaults(synth::WavetableSeedParams &seed, int frameIndex);
    static int partialBankMorphFrameIndex(const synth::WavetableSeedParams &seed);
    static float partialBankPitchRatio(int oct, int sem, float fin, float crs);
    static void applyPartialBankGroupPitch(synth::WavetableSeedParams &seed,
                                           int oct, int sem, float fin, float crs);
    static Kwt2PackedBin packKwtBin(float amp, float phase);
    static synth::WavetableHarmonic unpackKwtBin(const Kwt2PackedBin &bin, int index);
    void applyFramePreset(int preset);

    #include "state/KapibaraUIState.hpp"

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KapibaraUI)
};

END_NAMESPACE_DISTRHO
