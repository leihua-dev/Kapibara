    float dragStartLayoutRatio_ = 0.0f;
    float layoutBottomRatio_ = 0.45f;   // bottom workspace height
    float layoutMatrixRatio_ = 0.42f;   // top-right column = matrix
    float layoutRackRatio_   = 0.20f;
    float layoutStripRatio_  = 0.25f;
    Rect layoutVSplitHandle_ {}, layoutRackSplitHandle_ {}, layoutStripSplitHandle_ {};
    // Bottom workspace: the Source column is always shown on the left; the rest
    // shows the Modulators panel (curve / amp-env editors) when collapsed (0) or
    // the full Source Structure router when expanded (1). The matrix GRID lives
    // in the toolbar-opened top-row MATRIX view.
    int bottomPanelMode_ = 0;
    Rect bottomExpandArrowRect_ {};
    Rect bottomCollapseArrowRect_ {};
    // MATRIX view (the grid): replaces the top editor row while open.
    bool matrixViewOpen_ = false;
