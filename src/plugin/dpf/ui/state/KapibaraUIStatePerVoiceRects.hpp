    // Per-Voice Chain editor (top-middle): tabs over the selected source's chain
    // filters, plus the selected filter's detail (knobs + response graphs).
    std::array<Rect, synth::kMaxPerVoiceFilters> pvChainTabRects_ {};
    std::array<int, synth::kMaxPerVoiceFilters> pvChainTabSlots_ {};
    Rect pvChainEnableRect_ {};
    Rect pvChainTypeRect_ {};
    std::array<Rect, 4> pvChainKnobRects_ {};
    Rect pvChainAddRect_ {};
    int pvChainCount_ = 0;
    // Selected source's chain, traversed forward through the route graph.
    std::vector<int> selectedChainFilters_ {};                 // per-voice filter slots, in order
    std::vector<std::pair<uint32_t, int>> selectedChainInserts_ {}; // (trackId, insertIdx), in order
    // Cached impulse-response-derived filter response (recomputed on param change).
    bool pvGraphCacheValid_ = false;
    synth::SourceFilterParams pvGraphCacheParams_ {};
    std::array<float, 256> pvGraphMag_ {};
    std::array<float, 256> pvGraphPhase_ {};
