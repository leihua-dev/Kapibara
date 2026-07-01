#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Direct gain/pan/send drag on ANY strip (no need to select the strip first).
bool KapibaraUI::handleToolbarClick(float x, float y)
{
        if(presetSelectRect_.contains(x, y))
        {
            presetMenuOpen_ = !presetMenuOpen_;
            optionsMenuOpen_ = false;
            return true;
        }
        if(presetPrevRect_.contains(x, y) || presetNextRect_.contains(x, y))
        {
            if(!presetNames_.empty())
            {
                if(selectedPresetIndex_ < 0)
                    selectedPresetIndex_ = 0;
                else if(presetPrevRect_.contains(x, y))
                    selectedPresetIndex_ = (selectedPresetIndex_ + int(presetNames_.size()) - 1) % int(presetNames_.size());
                else
                    selectedPresetIndex_ = (selectedPresetIndex_ + 1) % int(presetNames_.size());
                presetNameBuffer_ = presetNames_[(size_t)selectedPresetIndex_];
                presetLabel_ = presetNameBuffer_;
            }
            return true;
        }
        if(presetMenuOpen_)
        {
            if(presetSearchRect_.contains(x, y))
            {
                beginSynthPresetRename();
                return true;
            }
            if(presetMenuNewRect_.contains(x, y))
            {
                selectedPresetIndex_ = -1;
                presetNameBuffer_.clear();
                presetNameEditing_ = true;
                presetNameEditTarget_ = PresetNameEditTarget::Synth;
                skipNextPresetCharacterInput_ = false;
                return true;
            }
            const int visible = std::min<int>(int(presetRowRects_.size()), int(presetNames_.size()));
            const int first = selectedPresetIndex_ >= 0
                                  ? clampi(selectedPresetIndex_ - visible / 2, 0, std::max(0, int(presetNames_.size()) - visible))
                                  : 0;
            for(int i = 0; i < int(presetRowRects_.size()); ++i)
            {
                const int presetIndex = first + i;
                if(presetRowRects_[(size_t)i].contains(x, y) && presetIndex < int(presetNames_.size()))
                {
                    selectedPresetIndex_ = presetIndex;
                    presetNameBuffer_ = presetNames_[(size_t)presetIndex];
                    presetLabel_ = presetNameBuffer_;
                    presetNameEditing_ = false;
                    presetNameEditTarget_ = PresetNameEditTarget::None;
                    skipNextPresetCharacterInput_ = false;
                    if(currentClickIsDouble_)
                    {
                        releaseAllUiNotes();
                        std::string modernPath;
                        if(auto *p = plugin())
                        {
                            p->loadUserPreset(presetNameBuffer_.c_str());
                            modernPath = p->presetFilePath(presetNameBuffer_.c_str());
                        }
                        pullFromPlugin();
                        if(!modernPath.empty())
                            loadModernState(modernPath);
                        presetMenuOpen_ = false;
                    }
                    return true;
                }
            }
            if(presetMenuSaveRect_.contains(x, y))
            {
                if(presetNameEditing_ && presetNameEditTarget_ == PresetNameEditTarget::Synth)
                    commitPresetNameEdit();
                else
                    beginSynthPresetRename();
                return true;
            }
            if(presetMenuDeleteRect_.contains(x, y))
            {
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                skipNextPresetCharacterInput_ = false;
                releaseAllUiNotes();
                const char *name = nullptr;
                if(!presetNameBuffer_.empty())
                    name = presetNameBuffer_.c_str();
                else if(selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
                    name = presetNames_[(size_t)selectedPresetIndex_].c_str();
                if(auto *p = plugin())
                    p->deleteUserPreset(name);
                selectedPresetIndex_ = -1;
                presetNameBuffer_ = "user_kapibara";
                pullFromPlugin();
                return true;
            }
            if(presetMenuResetRect_.contains(x, y))
            {
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                skipNextPresetCharacterInput_ = false;
                releaseAllUiNotes();
                if(auto *p = plugin())
                    p->resetUserPreset();
                pullFromPlugin();
                return true;
            }
            if(presetMenuPanelRect_.contains(x, y))
                return true;

            presetMenuOpen_ = false;
        }
        if(panicRect_.contains(x, y))
        {
            if(auto *p = plugin())
                p->panic();
            clearUiNoteState();
            return true;
        }
        if(menuRect_.contains(x, y))
        {
            optionsMenuOpen_ = !optionsMenuOpen_;
            presetMenuOpen_ = false;
            return true;
        }
        if(optionsMenuOpen_)
        {
            if(optionsMenuPanelRect_.contains(x, y))
                return false;
            optionsMenuOpen_ = false;
        }
        if(aboutRect_.contains(x, y))
            return true;
        if(gainRect_.contains(x, y))
        {
            dragTarget_    = DragTarget::Gain;
            dragStartY_    = y;
            dragStartNorm_ = clampf(gain_, 0.0f, 1.0f);
            return true;
        }
        return false;
    }

END_NAMESPACE_DISTRHO
