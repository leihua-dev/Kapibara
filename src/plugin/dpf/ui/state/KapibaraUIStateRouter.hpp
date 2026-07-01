    bool shiftDown_ = false;
    std::array<bool, synth::kMaxSourceTracks> selectedStrips_ {};
    float stripScrollF_ = 0.0f;   // fractional column scroll for smooth panning
    Rect stripScrollbarRect_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripMuteRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripSoloRects_ {};
    std::vector<StripGroup> stripGroups_;
    bool stripGroupContextMenuOpen_ = false;
    float stripGroupContextX_ = 0.0f, stripGroupContextY_ = 0.0f;
    std::array<Rect, 2> stripGroupContextRects_ {};
    int  groupContextTargetGroup_ = -1;  // >=0 => ungroup menu for this group; -1 => create-group menu
    std::array<Rect, synth::kMaxSourceTracks> stripGroupBusRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> stripGainRects_ {}, stripPanRects_ {}, stripSendRects_ {};
    int dragTrackIndex_ = -1; // source row whose gain/pan/send is being dragged
    std::array<Rect, synth::kMaxSourceTracks> sourceRouterRects_ {};
    std::array<Rect, synth::kMaxSourceTracks> sourceRouterOutputRects_ {};
    std::array<Rect, synth::kMaxPerVoiceFilters> perVoiceFilterNodeRects_ {};
    Rect perVoiceFilterAddRect_ {};
    bool perVoiceComponentMenuOpen_ = false;
    std::array<Rect, 1 + synth::kMaxAmpEnvs> perVoiceComponentMenuRects_ {};
    std::array<uint8_t, synth::kMaxAmpEnvRouteNodes> ampEnvRouteNodeSlots_ {};
    int selectedPerVoiceFilter_ = 0;
    Rect stripGridMasterRect_ {};
    std::vector<RoutePortHit> routePortHits_;
    std::vector<RouteNodeHit> routeNodeHits_;
    std::vector<synth::GridWire> routeWires_;
    RouteWireDraft routeWireDraft_ {};
    std::unordered_map<uint32_t, synth::GridPoint> routeNodePositions_;
    bool routeNodeDragActive_ = false;
    uint32_t routeNodeDragId_ = 0;
    float routeNodeDragOffsetX_ = 0.0f;
    float routeNodeDragOffsetY_ = 0.0f;
    Rect routeBoardRect_ {};
    Rect perVoiceRouteRect_ {};
    Rect stripRouteRect_ {};
    uint32_t selectedRouteNodeId_ = 0;
    bool routeNodeContextOpen_ = false;
    Rect routeNodeDeleteRect_ {};
    std::vector<std::vector<synth::GridPoint>> renderedWirePaths_;
    std::vector<size_t> renderedWireIndices_;
    std::vector<std::vector<WireIntersectionInfo>> renderedWireIntersections_;
    // Selected section (between two consecutive nodes/intersection-dots on one wire)
    int selectedWireIdx_ = -1;
    bool selectedSectionValid_ = false;
    synth::GridPoint selectedSectionPtA_ {};
    synth::GridPoint selectedSectionPtB_ {};
    // Segment drag (click selected section then hold + drag to slide without rerouting)
    bool segmentDragPending_ = false;
    bool segmentDragActive_ = false;
    int segmentDragWireIdx_ = -1;
    synth::GridPoint segmentDragPtA_ {};
    synth::GridPoint segmentDragPtB_ {};
    std::vector<synth::GridPoint> segmentDragOrigPath_ {};
    float segmentDragStartX_ = 0.0f, segmentDragStartY_ = 0.0f;
