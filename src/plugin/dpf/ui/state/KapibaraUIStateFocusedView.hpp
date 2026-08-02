    // Double-click a route node -> the top half shows that component's detail.
    uint32_t focusedNodeId_ = 0; // 0 = normal 3-panel top
    Rect focusedCloseRect_ {};
    bool fxPanelHideDelete_ = false; // hide the insert delete "x" (focus detail)
    bool fxPanelFocused_ = false;    // insert panel has the whole focus pane to itself
    // Disperser stage editor (focused allpass filter). Cleared at the top of every
    // insert panel so a rack panel drawn afterwards can never leave them live.
    Rect disperserFreqLaneRect_ {};
    Rect disperserQLaneRect_ {};
    std::array<Rect, 4> disperserShapeRects_ {}; // FLAT / MACRO / RANDOM / SMOOTH
    int disperserLaneStages_ = 0;
    int disperserSelStage_ = -1;
    FxKnobHit disperserTarget_ {};   // which insert the lanes edit (knob field unused)
    // Two pages, because "what each slot IS" and "where the slots SIT" are
    // different jobs and fight for the same space. SHAPE keeps the response
    // graph and the distribution lanes; SLOTS is a plain list with no graph at
    // all, one row per live slot.
    int disperserPage_ = 0;                  // 0 = shape/distribution, 1 = slots
    std::array<Rect, 2> disperserPageTabs_ {};
    // The editor owns this whole area. Without it a press that misses a control
    // fell through to the generic FX chain, where the insert's mode button
    // answered instead and the algo menu appeared out of nowhere.
    Rect disperserEditorRect_ {};
    Rect disperserVoiceStripRect_ {};        // SHAPE: where something is voiced
    Rect disperserSlotListRect_ {};          // SLOTS: the whole list body
    // 32 rows squeezed into one pane leaves each ~12 px, too short to read or
    // aim at. Rows keep a usable height and the list scrolls instead.
    float disperserSlotScroll_ = 0.0f;       // px
    float disperserSlotMaxScroll_ = 0.0f;    // computed at draw
    float disperserSlotRowH_ = 0.0f;         // computed at draw (hit-test needs it)
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
    // Global OSC MOD diagram (focus page 2): clickable mode chips → (carrier, slot).
    std::vector<OscModDiagHit> oscModDiagHits_;
