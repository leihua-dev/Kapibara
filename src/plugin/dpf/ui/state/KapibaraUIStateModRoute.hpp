    // Source-modulation editing.
    int selectedModSlot_ = -1;  // -1 = nothing highlighted by default
    std::vector<ModHit> modHits_;
    std::array<Rect, synth::kMaxTrackMods> modSrcRects_ {}, modTypeRects_ {}, modDepthRects_ {}, modDeleteRects_ {};
    // Mod source picker (right-click the source MOD area).
    bool modSourceMenuOpen_ = false;
    float modSourceMenuX_ = 0.0f, modSourceMenuY_ = 0.0f;
    int modSourceMenuTrackId_ = -1, modSourceMenuSlot_ = -1;  // slot -1 = new entry
    std::array<Rect, synth::kMaxSourceTracks + 1> modSourceMenuRects_ {};  // remove + candidate tracks
