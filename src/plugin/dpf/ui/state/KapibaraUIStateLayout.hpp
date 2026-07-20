    float dragStartLayoutRatio_ = 0.0f;
    float layoutBottomRatio_ = 0.45f;   // bottom workspace height
    float layoutMatrixRatio_ = 0.42f;   // top-right column = matrix
    float layoutRackRatio_   = 0.20f;
    float layoutStripRatio_  = 0.25f;
    Rect layoutVSplitHandle_ {}, layoutRackSplitHandle_ {}, layoutStripSplitHandle_ {};
    // Bottom workspace: the Source column on the left + the full Source Structure
    // router (the Matrix moved to a top-row view opened from the toolbar).
    int bottomPanelMode_ = 1;  // kept for old saved state; bottom is always the router
    Rect bottomExpandArrowRect_ {};
    Rect bottomCollapseArrowRect_ {};
    // Matrix view: replaces the top editor row while open (toolbar MATRIX button).
    bool matrixViewOpen_ = false;
