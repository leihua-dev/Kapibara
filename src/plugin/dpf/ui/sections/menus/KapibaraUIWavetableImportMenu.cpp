#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawWavetableImportMenu()
{
        if(!wavetableImportMenuOpen_)
            return;
        const Rect r { (float(uiW()) - 440.0f) * 0.5f, (float(uiH()) - 410.0f) * 0.5f,
                       440.0f, 410.0f };
        wavetableImportPanelRect_ = r;
        drawPanel(r, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(r.x + 18.0f, r.y + 16.0f, "Import WAV as Wavetable");
        for(int i = 0; i < 5; ++i)
        {
            wavetableImportModeRects_[(size_t)i] = { r.x + 18.0f, r.y + 52.0f + float(i) * 46.0f,
                                                     r.w - 36.0f, 36.0f };
            const auto mode = static_cast<synth::WavetableImportMode>(i);
            drawButton(wavetableImportModeRects_[(size_t)i], wavetableImportModeName(mode),
                       wavetableImportMode_ == mode);
        }
        drawLabelBox({ r.x + 18.0f, r.y + 282.0f, 104.0f, 30.0f }, "Frame limit");
        const int limits[3] = { 128, 256, 512 };
        for(int i = 0; i < 3; ++i)
        {
            importFrameLimitRects_[(size_t)i] = { r.x + 130.0f + float(i) * 76.0f, r.y + 282.0f, 68.0f, 30.0f };
            drawButton(importFrameLimitRects_[(size_t)i], buttonText("%d", limits[i]), importFrameLimit_ == limits[i]);
        }
        manualCycleMinusRect_ = { r.x + 18.0f, r.y + 322.0f, 42.0f, 30.0f };
        manualCyclePlusRect_ = { r.x + 66.0f, r.y + 322.0f, 42.0f, 30.0f };
        wavetableImportCancelRect_ = { r.x + r.w - 108.0f, r.y + 322.0f, 90.0f, 30.0f };
        drawButton(manualCycleMinusRect_, "-", false);
        drawButton(manualCyclePlusRect_, "+", false);
        manualCycleValueRect_ = { r.x + 116.0f, r.y + 322.0f, 168.0f, 30.0f };
        drawLabelBox(manualCycleValueRect_,
                     buttonText("Cycle %d samples", manualCycleLength_));
        drawButton(wavetableImportCancelRect_, "Cancel", false);
        drawLabelBox({ r.x + 18.0f, r.y + 364.0f, r.w - 36.0f, 22.0f },
                     droppedWavPending_ ? "Dropped WAV ready" : "Choose a mode, then choose a WAV file");
    }


bool KapibaraUI::handleWavetableImportMenuClick(float x, float y)
{
        if(!wavetableImportMenuOpen_)
            return false;
        if(manualCycleMinusRect_.contains(x, y))
        {
            manualCycleLength_ = std::max(32, manualCycleLength_ - 32);
            return true;
        }
        const int limits[3] = { 128, 256, 512 };
        for(int i = 0; i < 3; ++i)
        {
            if(importFrameLimitRects_[(size_t)i].contains(x, y))
            {
                importFrameLimit_ = limits[i];
                return true;
            }
        }
        if(manualCyclePlusRect_.contains(x, y))
        {
            manualCycleLength_ = std::min(65536, manualCycleLength_ + 32);
            return true;
        }
        if(manualCycleValueRect_.contains(x, y))
        {
            dragTarget_ = DragTarget::ManualCycleLength;
            applyDragValue(x, y);
            return true;
        }
        if(wavetableImportCancelRect_.contains(x, y))
        {
            wavetableImportMenuOpen_ = false;
            droppedWavPending_ = false;
            return true;
        }
        for(int i = 0; i < 5; ++i)
        {
            if(!wavetableImportModeRects_[(size_t)i].contains(x, y))
                continue;
            wavetableImportMode_ = static_cast<synth::WavetableImportMode>(i);
            wavetableImportMenuOpen_ = false;
            if(droppedWavPending_)
            {
                droppedWavPending_ = false;
                commitWavetableLoad();
            }
            else
                openWavetableFileBrowser();
            return true;
        }
        return true;
    }

END_NAMESPACE_DISTRHO
