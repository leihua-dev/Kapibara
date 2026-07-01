#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// LFO shape preview with animated playhead.

    // Distortion slot editor (controls + curves) for uiDist_[selectedDist_].
void KapibaraUI::drawOptionsMenu()
{
        if(!optionsMenuOpen_)
            return;
        const Rect r { menuRect_.x - 230.0f, toolbar_.y + toolbar_.h + 8.0f, 320.0f, 118.0f };
        optionsMenuPanelRect_ = r;
        drawPanel(r, rgba(0x181d24f5), rgba(0x39404dff));
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, "Menu");
        uiScaleRect_ = { r.x + 16.0f, r.y + 48.0f, r.w - 32.0f, 26.0f };
        drawSlider(uiScaleRect_, "UI Scale", (uiScale_ - 0.75f) / 0.75f, uiScale_);
        drawLabelBox({ r.x + 16.0f, r.y + 82.0f, r.w - 32.0f, 24.0f }, "Affects panel text and control labels");
    }


END_NAMESPACE_DISTRHO
