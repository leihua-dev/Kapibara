#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

int KapibaraUI::keyAt(float x, float y) const
{
        if(!keyboardRect_.contains(x, y))
            return -1;
        constexpr int first = 36;
        constexpr int keys = 61;
        return first + clampi(int((x - keyboardRect_.x) / keyboardRect_.w * float(keys)), 0, keys - 1);
    }

bool KapibaraUI::handleKeyboardPress(float x, float y)
{
        const int note = keyAt(x, y);
        if(note < 0)
            return false;
        pressMouseKey(note);
        return true;
    }

void KapibaraUI::pressMouseKey(int note)
{
        if(mouseKey_ == note)
            return;
        releaseMouseKey();
        mouseKey_ = note;
        if(auto *p = plugin())
            p->previewNoteOn(note, 0.85f);
        else
            sendNote(0, static_cast<uint8_t>(note), 105);
    }

void KapibaraUI::releaseMouseKey()
{
        if(mouseKey_ < 0)
            return;
        if(auto *p = plugin())
            p->previewNoteOff(mouseKey_);
        else
            sendNote(0, static_cast<uint8_t>(mouseKey_), 0);
        mouseKey_ = -1;
        repaint();
    }

int KapibaraUI::keycodeSlot(uint keycode) const
{
        return keycode < pressedKeycodeNotes_.size() ? int(keycode) : -1;
    }

void KapibaraUI::releaseComputerNote(int note, int keySlot)
{
        if(note < 0 || note >= int(computerKeys_.size()))
            return;
        if(computerKeys_[(size_t)note])
        {
            computerKeys_[(size_t)note] = false;
            if(auto *p = plugin())
                p->previewNoteOff(note);
        }
        if(keySlot >= 0 && keySlot < int(pressedKeycodeNotes_.size()))
            pressedKeycodeNotes_[(size_t)keySlot] = -1;
    }

void KapibaraUI::releaseAllUiNotes()
{
        if(auto *p = plugin())
        {
            for(size_t note = 0; note < computerKeys_.size(); ++note)
            {
                if(computerKeys_[note])
                    p->previewNoteOff(int(note));
            }
            if(mouseKey_ >= 0)
                p->previewNoteOff(mouseKey_);
        }
        clearUiNoteState();
    }

void KapibaraUI::clearUiNoteState()
{
        computerKeys_.fill(false);
        pressedKeycodeNotes_.fill(-1);
        mouseKey_ = -1;
    }

END_NAMESPACE_DISTRHO
