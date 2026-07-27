    std::array<Rect, synth::kMaxModSlots> modSlotSelectRects_ {};
    std::array<Rect, synth::kMaxMatrixRules> ruleSelectRects_ {};
    std::array<Rect, synth::kMaxAmpEnvs> ampEnvTabRects_ {};
    Rect adsrSourceRect_ {};
    Rect lfoEnableRect_ {};
    Rect envEnableRect_ {};
    Rect matrixEnvCurveRect_ {};
    Rect ruleEnableRect_ {}, ruleSourceRect_ {}, ruleDestRect_ {}, ruleWeightRect_ {}, ruleDepthRect_ {}, ruleBandLoRect_ {}, ruleBandHiRect_ {};
    std::vector<MatrixCardHit> matrixCardHits_;   // ROUTES card controls
    int gridPickerRuleIdx_ = -1;                  // rule a card picker (mode 3/4) edits; -2 = staged + ROUTE
    // MASK GROUPS page (advanced tier).
    std::array<synth::MaskGroup, synth::kMaxMaskGroups> maskGroups_ {};
    int selectedMaskGroup_ = 0;
    std::vector<MatrixCardHit> groupSlotHits_;    // slot column (rule field = slot index)
    std::array<Rect, synth::kMaxMaskGroups> groupSelRects_ {};
    Rect groupEnableRect_ {}, groupBaseRect_ {};
    Rect groupRateRect_ {}, groupFreqRect_ {}, groupPhaseRect_ {}, groupCurveRect_ {};
    Rect groupFamilyRect_ {}, groupFamilyDestRect_ {}, groupFamilyTrackRect_ {}, groupFamilyDepthRect_ {};
    Rect groupPreviewRect_ {};
    int groupDragSlot_ = -1;
    // Preview mirror of the engine's baked lane waveforms, so the 3D fan draws
    // the exact shapes the audio thread plays without re-summing harmonics per
    // point per repaint. Keyed by the wavetable's COW identity.
    const void *maskPreviewFramesKey_ = nullptr;
    uint32_t maskPreviewTrackKey_ = 0;
    int maskPreviewFrameCount_ = 0;
    // Must match synth::kMaskWaveLut: bakeModWaveLut band-limits to the LUT it is
    // given, so a smaller preview LUT would draw a different (aliased) waveform
    // than the fan plays.
    static constexpr int kMaskPreviewLut = synth::kMaskWaveLut;
    std::vector<std::array<float, kMaskPreviewLut + 1>> maskPreviewLut_;
    Rect matrixToolbarRect_ {};    // toolbar MATRIX entry button
    Rect matrixViewCloseRect_ {};  // close X of the top-row matrix view
    Rect chaosEnableRect_ {}, chaosRateRect_ {}, chaosAmountRect_ {}, shapeAxisRect_ {}, shapePhaseRect_ {}, shapeRhoRect_ {}, shapeUpRect_ {}, shapeDownRect_ {};
