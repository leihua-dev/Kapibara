#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawPresetMenu()
{
        if(!presetMenuOpen_)
            return;

        const Rect r { presetSelectRect_.x - 245.0f, toolbar_.y + toolbar_.h + 8.0f, 720.0f, 316.0f };
        presetMenuPanelRect_ = r;
        drawPanel(r, rgba(0x181d24f5), rgba(0x39404dff));
        presetSearchRect_ = { r.x + 16.0f, r.y + 16.0f, r.w - 256.0f, 34.0f };
        presetMenuNewRect_ = { r.x + r.w - 224.0f, r.y + 16.0f, 60.0f, 44.0f };
        presetMenuSaveRect_ = { r.x + r.w - 156.0f, r.y + 16.0f, 140.0f, 44.0f };
        presetMenuLoadRect_ = {};
        presetMenuDeleteRect_ = { r.x + r.w - 156.0f, r.y + 72.0f, 140.0f, 44.0f };
        presetMenuResetRect_ = { r.x + r.w - 156.0f, r.y + 128.0f, 140.0f, 44.0f };
        presetListRect_ = { r.x + 16.0f, r.y + 62.0f, r.w - 188.0f, 188.0f };
        std::string synthNameText = "Name...";
        if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Synth)
            synthNameText = "> " + presetNameBuffer_;
        else if(selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
            synthNameText = presetNames_[(size_t)selectedPresetIndex_];
        else if(!presetLabel_.empty())
            synthNameText = presetLabel_;
        const std::string nameText = synthNameText;
        drawLabelBox(presetSearchRect_, nameText.c_str());
        drawPanel(presetListRect_, rgba(0x20252dff), rgba(0x39404dff));
        for(auto &row : presetRowRects_)
            row = {};
        const int visible = std::min<int>(int(presetRowRects_.size()), int(presetNames_.size()));
        const int first = selectedPresetIndex_ >= 0
                              ? clampi(selectedPresetIndex_ - visible / 2, 0, std::max(0, int(presetNames_.size()) - visible))
                              : 0;
        for(int i = 0; i < visible; ++i)
        {
            const int presetIndex = first + i;
            presetRowRects_[(size_t)i] = { presetListRect_.x + 8.0f, presetListRect_.y + 8.0f + float(i) * 24.0f,
                                           presetListRect_.w - 16.0f, 22.0f };
            drawButton(presetRowRects_[(size_t)i], presetNames_[(size_t)presetIndex].c_str(), selectedPresetIndex_ == presetIndex);
        }
        if(visible == 0)
            drawLabelBox({ presetListRect_.x + 8.0f, presetListRect_.y + 8.0f, presetListRect_.w - 16.0f, 24.0f },
                         "No user presets");
        drawButton(presetMenuNewRect_, "NEW", presetNameEditing_ && presetNameBuffer_.empty());
        drawButton(presetMenuSaveRect_, presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Synth ? "SAVE NAME" : "SAVE AS", false);
        drawButton(presetMenuDeleteRect_, "DELETE", false);
        drawButton(presetMenuResetRect_, "RESET", false);
        drawLabelBox({ r.x + r.w - 156.0f, r.y + 184.0f, 140.0f, 44.0f }, "Double-click loads");
        drawLabelBox({ r.x + 16.0f, r.y + 282.0f, 88.0f, 22.0f }, "All");
        drawLabelBox({ r.x + 108.0f, r.y + 282.0f, 104.0f, 22.0f }, "User");
        drawLabelBox({ r.x + 216.0f, r.y + 282.0f, 122.0f, 22.0f }, "Favourites");
    }


END_NAMESPACE_DISTRHO
