    std::vector<InsertHit> insertHits_;
    std::vector<FxKnobHit> fxKnobHits_;
    std::vector<FxBtnHit> fxBypassHits_, fxDeleteHits_, fxModeHits_;
    FxKnobHit fxDragHit_ {};
    int multibandEditorTrackId_ = -1;
    int multibandEditorInsertIdx_ = -1;
    std::array<Rect, 3> multibandBandAddRects_ {};
    std::array<Rect, 3> multibandBandMuteRects_ {};
    std::array<Rect, 3> multibandBandSoloRects_ {};
    int modeMenuTrackId_ = -1, modeMenuMerge_ = -1, modeMenuInsertIdx_ = 0;
    // Insert slot press -> click-to-edit or drag-to-reorder.
    bool insertPending_ = false;
    bool insertDragActive_ = false;
    int insertPendTrackId_ = -1, insertPendMerge_ = -1, insertPendSlot_ = 0;
    float insertPendX_ = 0.0f, insertPendY_ = 0.0f;
    float insertDragX_ = 0.0f, insertDragY_ = 0.0f;
    // Insert picker menu.
    bool insertMenuOpen_ = false;
    float insertMenuX_ = 0.0f, insertMenuY_ = 0.0f;
    int insertMenuTrackId_ = -1, insertMenuMerge_ = -1, insertMenuSlot_ = 0;
    std::array<Rect, 49> insertMenuRects_ {};  // None + 6 types x 8 slots (grid)
    // Effect algorithm/mode picker menu (right-click the mode button).
    bool modeMenuOpen_ = false;
    int modeMenuKind_ = 0;   // 1 = filter, 2 = distortion
    int modeMenuSlot_ = 0;   // bank slot 0..7
    int modeMenuSelectedIndex_ = 0;
    float modeMenuX_ = 0.0f, modeMenuY_ = 0.0f;
    std::array<Rect, 12> modeMenuRects_ {};
