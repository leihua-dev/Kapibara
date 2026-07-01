    float dragStartLayoutRatio_ = 0.0f;
    float layoutBottomRatio_ = 0.45f;   // bottom workspace height
    float layoutMatrixRatio_ = 0.42f;   // top-right column = matrix
    float layoutRackRatio_   = 0.20f;
    float layoutStripRatio_  = 0.25f;
    Rect layoutVSplitHandle_ {}, layoutRackSplitHandle_ {}, layoutStripSplitHandle_ {};
    // Bottom workspace: the Source column is always shown on the left; the rest
    // shows the Modulation/Matrix editor when collapsed (0) or the full Source
    // Structure router when expanded (1). A right-arrow at the source's edge
    // expands; a left-arrow at the far right collapses.
    int bottomPanelMode_ = 0;
    Rect bottomExpandArrowRect_ {};
    Rect bottomCollapseArrowRect_ {};
