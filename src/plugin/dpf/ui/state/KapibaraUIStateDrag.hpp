    DragTarget dragTarget_ = DragTarget::None;
    float dragStartY_    = 0.0f;
    float dragStartNorm_ = 0.0f;
    float dragStartDepth_ = 0.0f;
    float dragDepthLimit_ = 1.0f;
    // FX Rack drag-to-swap: dragging a panel swaps its effect content with the
    // drop target (wiring untouched -> the router below just shows the swap).
    bool fxRackDragPending_ = false;
    bool fxRackDragActive_ = false;
    int  fxRackDragFrom_ = -1;
    float fxRackDragStartX_ = 0.0f, fxRackDragStartY_ = 0.0f;
    float fxRackDragX_ = 0.0f, fxRackDragY_ = 0.0f;
    // Thin draggable MOD-source strip (between the top editors and the bottom panel).
    std::array<Rect, 24> modStripChipRects_ {};
    std::array<synth::ModSource, 24> modStripChipSources_ {};
    int modStripChipCount_ = 0;
    bool modRouteDragActive_ = false;
    bool modRouteDragMoved_ = false;
    synth::ModSource modRouteSource_ = synth::ModSource::None;
    Rect modRouteSourceRect_ {};
    ModRouteTarget modRouteHover_ {};
    float modRouteStartX_ = 0.0f;
    float modRouteStartY_ = 0.0f;
    float modRouteMouseX_ = 0.0f;
    float modRouteMouseY_ = 0.0f;
