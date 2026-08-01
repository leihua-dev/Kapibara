#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

#if DISTRHO_UI_FILE_BROWSER
#endif
bool KapibaraUI::onKeyboard(const KeyboardEvent &ev)
{
        if(aiPromptEditing_)
        {
            if(!ev.press)
                return true;
            if(ev.key == kKeyEnter)
            {
                aiPromptEditing_ = false;
                if(auto *t = currentTrack();
                   t != nullptr && t->type == synth::SourceTrackType::SampleNoise)
                    startAiSampleGeneration(*t, aiPromptBuffer_);
                repaint();
                return true;
            }
            if(ev.key == kKeyEscape)
            {
                aiPromptEditing_ = false;
                repaint();
                return true;
            }
            if(ev.key == kKeyBackspace)
            {
                if(!aiPromptBuffer_.empty())
                    aiPromptBuffer_.pop_back();
                repaint();
                return true;
            }
            if(ev.key >= 32 && ev.key <= 126 && aiPromptBuffer_.size() < 200)
            {
                aiPromptBuffer_.push_back(char(ev.key));
                repaint();
                return true;
            }
            return true;
        }
        if(loadPathEditing_)
        {
            if(!ev.press)
                return true;
            if(ev.key == kKeyEnter)
            {
                commitWavetableLoad();
                repaint();
                return true;
            }
            if(ev.key == kKeyEscape)
            {
                loadPathEditing_ = false;
                repaint();
                return true;
            }
            if(ev.key == kKeyBackspace)
            {
                if(!loadPathBuffer_.empty())
                    loadPathBuffer_.pop_back();
                repaint();
                return true;
            }
            return true;
        }
        if(presetNameEditing_)
        {
            if(!ev.press)
                return true;
            if(ev.key == kKeyEnter)
            {
                commitPresetNameEdit();
                repaint();
                return true;
            }
            if(ev.key == kKeyEscape)
            {
                presetNameEditing_ = false;
                presetNameEditTarget_ = PresetNameEditTarget::None;
                repaint();
                return true;
            }
            if(ev.key == kKeyBackspace)
            {
                if(!presetNameBuffer_.empty())
                    presetNameBuffer_.pop_back();
                repaint();
                return true;
            }
            if(ev.key >= 32 && ev.key <= 126)
            {
                appendPresetNameChar(char(ev.key));
                skipNextPresetCharacterInput_ = true;
                repaint();
                return true;
            }
            return true;
        }
        if(ev.press && ev.key == kKeyEnter)
        {
            if(presetMenuOpen_)
            {
                beginSynthPresetRename();
                repaint();
                return true;
            }
            if(wavetablePresetMenuOpen_)
            {
                beginWavetablePresetRename();
                repaint();
                return true;
            }
            if(modeMenuOpen_ && modeMenuKind_ == InsertConvReverb)
            {
                commitModeMenuSelection();
                repaint();
                return true;
            }
        }

        // Control is held? Match 'a'/'A' and the control-code (Ctrl+A -> 0x01).
        const bool ctrlHeld = (ev.mod & kModifierControl) != 0;
        const auto ctrlKey = [&](char base) {
            const uint b = uint(base);
            return ev.key == b || ev.key == uint(base - 'a' + 'A') || ev.key == uint(base - 'a' + 1);
        };
        // Ctrl+A selects all frames; Ctrl+click defines a range (mouse) — both in the
        // meta wavetable editor / wherever a frame strip is visible.
        const bool metaFrameCtx = harmonicEditorOpen_ || metaFramesShown_;
        if(ev.press && metaFrameCtx && ctrlHeld && ctrlKey('a'))
        {
            selectAllMetaFrames();
            repaint();
            return true;
        }
        if(ev.press && metaFrameCtx && ctrlHeld && ctrlKey('z'))
        {
            if(undoMeta())
                repaint();
            return true;
        }
        if(ev.press && harmonicEditorOpen_
           && (ev.key == kKeyDelete || ev.key == kKeyBackspace))
        {
            performMetaFrameAction(2);
            repaint();
            return true;
        }
        // Delete / Backspace removes selected wire, node, or strip (in that priority order).
        if(ev.press && !harmonicEditorOpen_ && !loadPathEditing_ && !presetNameEditing_
           && (ev.key == kKeyDelete || ev.key == kKeyBackspace))
        {
            // Structure editor: remove the selected structure wire first.
            if(focusedNodeId_ != 0 && focusPage_ == 1 && structSelectedWire_ >= 0)
            {
                auto &wires = structWires_[focusedNodeId_];
                if(structSelectedWire_ < int(wires.size()))
                    wires.erase(wires.begin() + structSelectedWire_);
                structSelectedWire_ = -1;
                if(auto *p = plugin()) p->updateCompiledRoute(buildCompiledRoute());
                repaint();
                return true;
            }
            if(deleteSelectedWire())
            {
                repaint();
                return true;
            }
            // While a component's detail/structure view is focused, Del must not
            // delete the focused effector (or its track); only wire deletes apply.
            if(focusedNodeId_ == 0)
            {
                if(selectedRouteNodeId_ != 0 && deleteRouteNode(selectedRouteNodeId_))
                {
                    repaint();
                    return true;
                }
                deleteSelectedTrack();
            }
            repaint();
            return true;
        }

        // Disable the computer-keyboard MIDI piano while the meta wavetable editor is
        // open, so letter keys (and Ctrl combos) drive editing, not notes.
        if(harmonicEditorOpen_)
            return true;

        const int note = noteForComputerKey(ev.key);
        if(note < 0)
            return false;
        const int keySlot = keycodeSlot(ev.keycode);

        if(ev.press)
        {
            if(keySlot >= 0 && pressedKeycodeNotes_[(size_t)keySlot] == note)
                return true;

            if(keySlot >= 0 && pressedKeycodeNotes_[(size_t)keySlot] >= 0)
                releaseComputerNote(pressedKeycodeNotes_[(size_t)keySlot], keySlot);

            if(!computerKeys_[(size_t)note])
            {
                computerKeys_[(size_t)note] = true;
                if(auto *p = plugin())
                    p->previewNoteOn(note, 0.82f);
            }
            if(keySlot >= 0)
                pressedKeycodeNotes_[(size_t)keySlot] = note;
        }
        else
        {
            if(keySlot >= 0)
            {
                if(pressedKeycodeNotes_[(size_t)keySlot] == note)
                    releaseComputerNote(note, keySlot);
            }
            else if(computerKeys_[(size_t)note])
            {
                releaseComputerNote(note, -1);
            }
        }
        repaint();
        return true;
    }

bool KapibaraUI::onCharacterInput(const CharacterInputEvent &ev)
{
        if(presetNameEditing_)
        {
            if(skipNextPresetCharacterInput_)
            {
                skipNextPresetCharacterInput_ = false;
                return true;
            }
            if(ev.string[0] != '\0')
            {
                for(const char *p = ev.string; *p != '\0'; ++p)
                    appendPresetNameChar(*p);
                repaint();
            }
            return true;
        }
        if(!loadPathEditing_)
            return false;
        if(ev.string[0] != '\0')
        {
            loadPathBuffer_ += ev.string;
            if(loadPathBuffer_.size() > 512)
                loadPathBuffer_.resize(512);
            repaint();
        }
        return true;
    }

END_NAMESPACE_DISTRHO
