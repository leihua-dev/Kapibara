    // Route-FX editor: identity of the chain currently shown in the editor.
    int selectedGroupView_ = -1;       // >=0 => editor shows this merge/group
    int routeFxChainTrackId_ = -1;
    int routeFxChainMerge_ = -1;
    // FX Rack panel rects (one per chain insert), for drag-to-swap hit-testing.
    std::vector<Rect> fxRackPanelRects_ {};
