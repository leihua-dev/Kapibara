    std::array<Rect, synth::kMaxModSlots> modSlotSelectRects_ {};
    std::array<Rect, synth::kMaxMatrixRules> ruleSelectRects_ {};
    std::array<Rect, synth::kMaxAmpEnvs> ampEnvTabRects_ {};
    Rect adsrSourceRect_ {};
    Rect lfoEnableRect_ {};
    Rect envEnableRect_ {};
    Rect matrixEnvCurveRect_ {};
    Rect ruleEnableRect_ {}, ruleSourceRect_ {}, ruleDestRect_ {}, ruleWeightRect_ {}, ruleDepthRect_ {}, ruleBandLoRect_ {}, ruleBandHiRect_ {};
    Rect ruleMaskRect_ {}, ruleMaskAxisRect_ {};  // spatial-mask controls in the grid inspector row
    Rect matrixToolbarRect_ {};    // toolbar MATRIX entry button
    Rect matrixViewCloseRect_ {};  // close X of the top-row matrix view
    Rect chaosEnableRect_ {}, chaosRateRect_ {}, chaosAmountRect_ {}, shapeAxisRect_ {}, shapePhaseRect_ {}, shapeRhoRect_ {}, shapeUpRect_ {}, shapeDownRect_ {};
