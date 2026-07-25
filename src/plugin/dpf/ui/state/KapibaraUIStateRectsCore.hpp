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
    int editorTab_ = 0;  // 0=SOURCE 1=SHAPE
    std::array<Rect, 2> editorTabRects_ {};
    int matrixTab_ = 0;  // 0=GRID 1=MODULATORS 2=AMP ENV
    std::array<Rect, 3> matrixTabRects_ {};
    std::vector<Rect> gridPickerItemRects_;
    std::vector<int> gridPickerPoolIdx_;
    int gridPickerMode_ = 0;  // 0=closed 3=card source 4=card destination
    float gridPickerX_ = 0.0f, gridPickerY_ = 0.0f;
