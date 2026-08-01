    std::array<Rect, synth::kMaxWavetablePartials> partialKnobRects_ {};
    std::array<Rect, 4> sourceCountRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> sourceChainRects_ {};
    Rect addTrackRect_ {};
    Rect removeTrackRect_ {};
    std::array<Rect, 4> addTrackTypeRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> trackRowRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripRects_ {};
    Rect trackOutputModeRect_ {}, trackGainRect_ {}, trackPanRect_ {}, trackSendRect_ {};
    Rect stripEnvRect_ {}, stripDupRect_ {};
    Rect ampEnvSelectRect_ {}, duplicateEnvRect_ {};
    Rect partialSpectrumRect_ {}, partialAmpRect_ {}, partialRatioRect_ {};
    // Basic Oscillator hosts kBasicOscUnits oscillators side by side, so each
    // control is per unit. basicDragUnit_ carries which column a drag belongs to.
    std::array<Rect, synth::kBasicOscUnits> basicUnitEnableRects_ {}, basicShapeRects_ {},
        basicLevelRects_ {}, basicPulseRects_ {}, basicSubRects_ {};
    std::array<std::array<Rect, 4>, synth::kBasicOscUnits> basicPitchRects_ {};
    int basicDragUnit_ = -1;
    std::array<Rect, synth::kBasicOscUnits> basicWaveRects_ {};   // per-unit mini preview
    // Source-local cross-unit modulation module (not a matrix route).
    Rect basicModModeRect_ {}, basicModSrcRect_ {}, basicModDstRect_ {}, basicModDepthRect_ {};
    Rect noiseModeRect_ {}, noiseColorRect_ {}, noiseTypeRect_ {};
    // Sampler (Sample / Noise track, File mode).
    Rect samplerLoadRect_ {}, samplerRootRect_ {}, samplerKeyTrackRect_ {}, samplerLoopRect_ {};
    Rect samplerSliceRect_ {}, samplerRevRect_ {}, samplerWaveRect_ {};
    Rect samplerStartRect_ {}, samplerEndRect_ {};
    Rect samplerLoopStartRect_ {}, samplerLoopEndRect_ {}, samplerGainRect_ {};
    bool samplerLoadPending_ = false;   // file browser is opening for the sampler
    Rect sourceGainRect_ {}, sourcePanRect_ {}, sourceFilterEnableRect_ {}, sourceFilterTopologyRect_ {};
    Rect sourceFilterCutoffRect_ {}, sourceFilterResRect_ {}, sourceFilterDriveRect_ {}, sourceFilterFeedbackRect_ {};
    Rect sourceFilterMixRect_ {};
    Rect unisonVoicesRect_ {}, unisonDetuneRect_ {}, unisonWidthRect_ {}, unisonPhaseRect_ {};
    Rect unisonVoicesDownRect_ {}, unisonVoicesUpRect_ {};  // Voices [-]/[+] stepper
    Rect metaMorphSliderRect_ {};        // full-height left-gutter hit area for the Morph scrubber
    float morphGrooveTop_ = 0.0f;        // y of morph=0 (groove top) — value mapping range
    float morphGrooveH_ = 1.0f;          // groove height (morph 0..1 span)
    float oscBodyBottomY_ = 0.0f;        // y where the oscillator body ends; UNISON sits just below
    float oscBodyLeftW_ = 0.0f;          // width of the left column (wave view); UNISON spans this, not PAN's column
    Rect attackRect_ {}, decayRect_ {}, sustainRect_ {}, releaseRect_ {}, curveRect_ {};
