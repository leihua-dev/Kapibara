#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawWavetablePresetMenu()
{
        if(!wavetablePresetMenuOpen_)
            return;

        const float menuWidth = std::min(520.0f, float(uiW()) - 24.0f);
        const float menuX = clampf(metaWavetableNameRect_.x, 12.0f, float(uiW()) - menuWidth - 12.0f);
        const Rect panel { menuX, metaWavetableNameRect_.y + metaWavetableNameRect_.h + 6.0f,
                           menuWidth, 316.0f };
        wavetablePresetPanelRect_ = panel;
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(panel.x + 16.0f, panel.y + 12.0f, "Wavetable Presets");

        static constexpr const char *builtins[] = { "Sine", "Saw", "Square", "Triangle", "Clear" };
        const float builtinGap = 6.0f;
        const float builtinW = (panel.w - 32.0f - builtinGap * 4.0f) / 5.0f;
        for(size_t i = 0; i < wavetableBuiltinRects_.size(); ++i)
        {
            wavetableBuiltinRects_[i] = { panel.x + 16.0f + float(i) * (builtinW + builtinGap),
                                           panel.y + 38.0f, builtinW, 28.0f };
            drawButton(wavetableBuiltinRects_[i], builtins[i], false);
        }

        wavetablePresetListRect_ = { panel.x + 16.0f, panel.y + 76.0f, panel.w - 32.0f, 176.0f };
        drawPanel(wavetablePresetListRect_, rgba(0x0c1318ff), rgba(0x354851ff));
        for(auto &row : wavetablePresetRowRects_)
            row = {};
        const int visible = std::min<int>(int(wavetablePresetRowRects_.size()), int(wavetablePresets_.size()));
        const int first = selectedWavetablePresetIndex_ >= 0
                              ? clampi(selectedWavetablePresetIndex_ - visible / 2, 0,
                                       std::max(0, int(wavetablePresets_.size()) - visible))
                              : 0;
        for(int i = 0; i < visible; ++i)
        {
            const int presetIndex = first + i;
            wavetablePresetRowRects_[(size_t)i] = { wavetablePresetListRect_.x + 8.0f,
                                                     wavetablePresetListRect_.y + 7.0f + float(i) * 20.0f,
                                                     wavetablePresetListRect_.w - 16.0f, 18.0f };
            drawButton(wavetablePresetRowRects_[(size_t)i], wavetablePresets_[(size_t)presetIndex].name.c_str(),
                       presetIndex == selectedWavetablePresetIndex_);
        }
        if(visible == 0)
            drawLabelBox({ wavetablePresetListRect_.x + 8.0f, wavetablePresetListRect_.y + 8.0f,
                           wavetablePresetListRect_.w - 16.0f, 24.0f },
                         "No wavetable files in presets/wavetables");

        const float by = panel.y + 266.0f;
        wavetablePresetLoadRect_ = {};
        wavetablePresetNameRect_ = { panel.x + 16.0f, by, 160.0f, 34.0f };
        wavetablePresetSaveRect_ = { panel.x + 184.0f, by, 78.0f, 34.0f };
        wavetablePresetImportRect_ = { panel.x + 270.0f, by, 108.0f, 34.0f };
        wavetablePresetRefreshRect_ = { panel.x + 386.0f, by, 92.0f, 34.0f };
        wavetablePresetCloseRect_ = { panel.x + panel.w - 90.0f, by, 74.0f, 34.0f };
        std::string wtNameText = wavetablePresetLabel_.empty() ? "wavetable" : wavetablePresetLabel_;
        if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Wavetable)
            wtNameText = "> " + presetNameBuffer_;
        else if(selectedWavetablePresetIndex_ >= 0 && selectedWavetablePresetIndex_ < int(wavetablePresets_.size()))
            wtNameText = wavetablePresets_[(size_t)selectedWavetablePresetIndex_].name;
        drawLabelBox(wavetablePresetNameRect_, wtNameText.c_str());
        drawButton(wavetablePresetSaveRect_, presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Wavetable ? "SAVE" : "RENAME", false);
        drawButton(wavetablePresetImportRect_, "IMPORT WAV", false);
        drawButton(wavetablePresetRefreshRect_, "REFRESH", false);
        drawButton(wavetablePresetCloseRect_, "CLOSE", false);
    }

void KapibaraUI::refreshWavetablePresets()
{
        if(auto *p = plugin())
            wavetablePresets_ = p->wavetablePresetEntries();
        selectedWavetablePresetIndex_ = wavetablePresets_.empty()
                                              ? -1
                                              : clampi(selectedWavetablePresetIndex_, 0,
                                                       int(wavetablePresets_.size()) - 1);
    }

bool KapibaraUI::loadWavetablePreset(int index)
{
        auto *track = currentTrack();
        if(track == nullptr
           || (track->type != synth::SourceTrackType::MetaOscillator
               && track->type != synth::SourceTrackType::PartialBank)
           || index < 0 || index >= int(wavetablePresets_.size()))
            return false;
        selectedWavetablePresetIndex_ = index;
        loadPathBuffer_ = wavetablePresets_[(size_t)index].path;
        wavetableImportMode_ = synth::WavetableImportMode::AutoDetect;
        importFrameLimit_ = synth::kMaxWavetableFrames;
        if(!commitWavetableLoad())
            return false;
        wavetablePresetLabel_ = wavetablePresets_[(size_t)index].name;
        presetNameBuffer_ = wavetablePresetLabel_;
        return true;
    }

bool KapibaraUI::handleWavetablePresetMenuClick(float x, float y)
{
        if(!wavetablePresetMenuOpen_)
            return false;
        static constexpr const char *builtins[] = { "Sine", "Saw", "Square", "Triangle", "Clear" };
        for(size_t i = 0; i < wavetableBuiltinRects_.size(); ++i)
        {
            if(!wavetableBuiltinRects_[i].contains(x, y))
                continue;
            if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                pushMetaUndoSnapshot();
            applyFramePreset(int(i));
            wavetablePresetLabel_ = builtins[i];
            wavetablePresetMenuOpen_ = false;
            return true;
        }

        const int visible = std::min<int>(int(wavetablePresetRowRects_.size()), int(wavetablePresets_.size()));
        const int first = selectedWavetablePresetIndex_ >= 0
                              ? clampi(selectedWavetablePresetIndex_ - visible / 2, 0,
                                       std::max(0, int(wavetablePresets_.size()) - visible))
                              : 0;
        for(int i = 0; i < visible; ++i)
        {
            const int presetIndex = first + i;
            if(wavetablePresetRowRects_[(size_t)i].contains(x, y))
            {
                selectedWavetablePresetIndex_ = presetIndex;
                presetNameBuffer_ = wavetablePresets_[(size_t)presetIndex].name;
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                if(currentClickIsDouble_ && loadWavetablePreset(selectedWavetablePresetIndex_))
                    wavetablePresetMenuOpen_ = false;
                return true;
            }
        }
        if(wavetablePresetNameRect_.contains(x, y))
        {
            beginWavetablePresetRename();
            return true;
        }
        if(wavetablePresetSaveRect_.contains(x, y))
        {
            if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Wavetable)
                commitPresetNameEdit();
            else
                beginWavetablePresetRename();
            return true;
        }
        if(wavetablePresetImportRect_.contains(x, y))
        {
            wavetablePresetMenuOpen_ = false;
            beginWavetableImport();
            return true;
        }
        if(wavetablePresetRefreshRect_.contains(x, y))
        {
            refreshWavetablePresets();
            return true;
        }
        if(wavetablePresetCloseRect_.contains(x, y))
        {
            wavetablePresetMenuOpen_ = false;
            return true;
        }
        if(wavetablePresetPanelRect_.contains(x, y))
            return true;
        wavetablePresetMenuOpen_ = false;
        return true;
    }

END_NAMESPACE_DISTRHO
