    // Double-click a route node -> the top half shows that component's detail.
    uint32_t focusedNodeId_ = 0; // 0 = normal 3-panel top
    Rect focusedCloseRect_ {};
    bool fxPanelHideDelete_ = false; // hide the insert delete "x" (focus detail)
    int focusPage_ = 0;             // 0 = detail, 1 = structure (output router), 2 = osc-mod diagram (sources only)
    std::array<Rect, 3> focusPageTabRects_ {};
    Rect structAddOutRect_ {};
    Rect structRemoveOutRect_ {};
    Rect structAddUtilRect_ {};
    // Per route node: number of output ports (>=1). Extra OUT ports let a component
    // fan out (each port still carries one wire). Derived/extended in the structure page.
    std::unordered_map<uint32_t, int> nodeOutPortCount_;
    // Per-component output-router structure (free-drag + free-wire). Local node ids:
    // 1 = IN, 2 = component, 10+k = OUT k, 30+u = utility u. Keyed by component node id.
    std::unordered_map<uint32_t, std::vector<synth::GridWire>> structWires_;
    std::unordered_map<uint32_t, int> structUtilCount_;
    std::unordered_map<uint32_t, std::unordered_map<int, synth::GridPoint>> structNodePos_;
    // Per (component<<8 | utilLocalIndex) utility params.
    std::unordered_map<uint64_t, synth::RouteUtilParams> structUtilParams_;
    std::vector<std::pair<Rect, int>> structNodeRects_;          // (rect, localId)
    std::vector<std::tuple<Rect, int, bool>> structPortRects_;   // (rect, localId, isOutput)
    bool structDraftActive_ = false;
    int  structDraftFromLocal_ = -1;
    float structDraftX_ = 0.0f, structDraftY_ = 0.0f;
    int  structDragLocal_ = -1;
    float structDragOffX_ = 0.0f, structDragOffY_ = 0.0f;
    int  structSelectedWire_ = -1; // selected structure wire index (Delete removes it)
    int  structSelectedUtil_ = -1; // selected utility local id (param editor)
    std::array<Rect, 5> structUtilParamRects_ {}; // level, pan, bandLo, bandHi, bandToggle
    // In-node utility controls: (rect, utility index u, control 0=level 1=pan 2=lo 3=hi 4=band).
    std::vector<std::tuple<Rect, int, int>> structUtilCtrlHits_;
    int  structUtilParamDrag_ = -1; // 0=level 1=pan 2=lo 3=hi
    float structUtilParamStartY_ = 0.0f, structUtilParamStartVal_ = 0.0f;
    std::array<Rect, 4> ampFocusKnobRects_ {};
    // OSC MOD diagram (focus page 2): clickable modulator boxes, one per mod slot.
    std::array<Rect, synth::kMaxTrackMods> oscModDiagRects_ {};
