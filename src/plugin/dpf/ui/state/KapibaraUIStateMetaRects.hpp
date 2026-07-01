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
    // OCT/SEM/FIN/CRS pitch controls.
    Rect metaOctRect_ {}, metaSemRect_ {}, metaFinRect_ {}, metaCrsRect_ {};
    int  dragStartOct_ = 0, dragStartSem_ = 0;
    float dragStartFin_ = 0.0f, dragStartCrs_ = 0.0f;
    // Time-domain waveform draw interpolation.
    float prevTimeEditX_ = -1.0f, prevTimeEditY_ = -1.0f;
    // Double-click reset detection.
    uint32_t lastClickTime_ = 0;
    float lastClickX_ = -9999.0f, lastClickY_ = -9999.0f;
    // Animation phase driven by uiIdle for animated modulation curves.
    float animPhase_ = 0.0f;
    // Legacy meta-partial ratio/amp (still used by drawMetaPartialEditor).
    Rect metaRatioRect_ {}, metaAmpRect_ {}, metaPhaseRect_ {}, metaPanRect_ {};
    Rect metaFrameCountRect_ {}, metaMorphRect_ {}, metaWarpAmountRect_ {};
    Rect metaPhaseRandRect_ {};
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
