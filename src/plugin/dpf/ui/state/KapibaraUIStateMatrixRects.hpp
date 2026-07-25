    std::array<Rect, synth::kMaxModSlots> modSlotSelectRects_ {};
    std::array<Rect, synth::kMaxMatrixRules> ruleSelectRects_ {};
    std::array<Rect, synth::kMaxAmpEnvs> ampEnvTabRects_ {};
    Rect adsrSourceRect_ {};
    Rect lfoEnableRect_ {};
    Rect envEnableRect_ {};
    Rect matrixEnvCurveRect_ {};
    Rect ruleEnableRect_ {}, ruleSourceRect_ {}, ruleDestRect_ {}, ruleWeightRect_ {}, ruleDepthRect_ {}, ruleBandLoRect_ {}, ruleBandHiRect_ {};
    Rect ruleMaskRect_ {}, ruleMaskAxisRect_ {};  // spatial-mask controls (ROUTES cards)
    Rect ruleXferRect_ {}, ruleEditRect_ {};      // grid inspector: transfer bend + EDIT > jump
    std::vector<MatrixCardHit> matrixCardHits_;   // ROUTES card controls
    int gridPickerRuleIdx_ = -1;                  // rule a card picker (mode 3/4) edits; -2 = staged + ROUTE
    Rect matrixToolbarRect_ {};    // toolbar MATRIX entry button
    Rect matrixViewCloseRect_ {};  // close X of the top-row matrix view
    Rect chaosEnableRect_ {}, chaosRateRect_ {}, chaosAmountRect_ {}, shapeAxisRect_ {}, shapePhaseRect_ {}, shapeRhoRect_ {}, shapeUpRect_ {}, shapeDownRect_ {};
