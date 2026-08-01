    synth::SourceGenParams generator_ {};
    synth::AdsrParams adsr_ {};
    std::array<synth::AdsrParams, synth::kMaxAmpEnvs> ampEnvs_ {};
    std::array<synth::ModSlotParams, synth::kMaxModSlots> modSlots_ {};
    std::array<synth::MatrixRule, synth::kMaxMatrixRules> rules_ {};
    synth::ChaosParams chaos_ {};
    synth::ShapeSourceParams shape_ {};
    synth::MasterEffectsParams effects_ {};
    float gain_ = 0.3f;

    // MetaOsc 专用 undo 栈（UI 层），最多保留 32 步
    static constexpr int kMetaUndoMax = 32;
    std::vector<synth::WavetablePartialSlot> metaUndoStack_;
    bool metaUndoPending_ = false; // drag 期间只记一次快照
    int activeVoices_ = 0;
    int selectedAmpEnv_ = 0;
    int selectedRule_ = 0;
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
    // Tempo sync row of the selected modulator slot.
    Rect modSyncToggleRect_ {}, modSyncDivDownRect_ {}, modSyncDivUpRect_ {}, modBpmRect_ {};
    float uiTempoBpm_ = synth::kFallbackBpm;   // used when the host reports none
    Rect ampAdsrRect_ {};
    float ampAdsrXA_ = 0.0f, ampAdsrXD_ = 0.0f, ampAdsrXS_ = 0.0f, ampAdsrXR_ = 0.0f;
    int ampAdsrDragSeg_ = -1;  // 0=attack 1=decay 2=release
