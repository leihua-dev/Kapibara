    bool addTrackMenuOpen_ = false;
    bool presetNameEditing_ = false;
    PresetNameEditTarget presetNameEditTarget_ = PresetNameEditTarget::None;
    bool skipNextPresetCharacterInput_ = false;
    std::string presetLabel_ = "Select preset";
    std::string presetNameBuffer_ = "user_kapibara";
    std::vector<std::string> presetNames_ {};
    int selectedPresetIndex_ = -1;
    std::vector<WavetablePresetEntry> wavetablePresets_ {};
    std::vector<std::pair<std::string, std::string>> irFiles_ {}; // (name, path) for IR reverb
    int selectedWavetablePresetIndex_ = -1;
    std::string wavetablePresetLabel_ = "Select Wavetable";
    std::string loadPathBuffer_ {};
    std::string lastLoadPath_ {};
    std::string browserStartDir_ {};
    std::string loadStatus_ = "type wav path after Load";
    std::string metaEditorStatus_ = "ready";
