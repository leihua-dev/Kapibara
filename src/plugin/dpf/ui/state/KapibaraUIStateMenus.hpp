    bool loadPathEditing_ = false;
    bool wavetableImportMenuOpen_ = false;
    bool wavetablePresetMenuOpen_ = false;
    bool droppedWavPending_ = false;
    bool presetMenuOpen_ = false;
    bool optionsMenuOpen_ = false;
    bool harmonicEditorOpen_ = false;
    bool metaProcessContextMenuOpen_ = false;
    // Warp mode picker (right-click the warp mode chip). trackId 0 = legacy
    // metaSlot (generator_.wavetableSeed.partials[selectedMetaPartial_]).
    bool warpModeMenuOpen_ = false;
    float warpModeMenuX_ = 0.0f, warpModeMenuY_ = 0.0f;
    uint32_t warpModeMenuTrackId_ = 0;
    std::array<Rect, 4> warpModeMenuRects_ {};
    // OSC MOD type picker (mode button / mod-wire dot / wire-drop onto a source).
    bool oscModTypeMenuOpen_ = false;
    float oscModTypeMenuX_ = 0.0f, oscModTypeMenuY_ = 0.0f;
    int oscModTypeMenuTrackId_ = -1, oscModTypeMenuSlot_ = -1;
    std::array<Rect, 6> oscModTypeMenuRects_ {};  // 5 modes + remove
    // Basic Oscillator rack cross-unit modulation mode picker.
    // Router (architecture) preset menu, from the SOURCE column.
    bool routerPresetMenuOpen_ = false;
    float routerPresetMenuX_ = 0.0f, routerPresetMenuY_ = 0.0f;
    std::vector<std::string> routerPresetNames_;
    std::vector<Rect> routerPresetItemRects_;
    Rect routerPresetSaveRect_ {}, routerPresetBarRect_ {};
    std::string routerPresetLabel_ { "ARCH" };
    bool basicModMenuOpen_ = false;
    float basicModMenuX_ = 0.0f, basicModMenuY_ = 0.0f;
    uint32_t basicModMenuTrackId_ = 0;
    std::array<Rect, synth::kBasicOscModModes> basicModMenuRects_ {};
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
