#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawMetaProcessContextMenu()
{
        if(!metaProcessContextMenuOpen_)
            return;

        static constexpr const char *labels[] = {
            "Remove DC + Normalize",
            "Crossfade Boundaries",
            "Zero-Crossing Align",
            "Align Frame Phases",
            "Smooth Frame Energy",
            "Morph Selection to 256",
            "Morph Selection to 512"
        };
        constexpr float rowHeight = 28.0f;
        metaProcessContextPanelRect_ = { metaProcessContextX_, metaProcessContextY_, 224.0f,
                                         34.0f + rowHeight * float(metaProcessContextRects_.size()) };
        drawPanel(metaProcessContextPanelRect_, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(metaProcessContextPanelRect_.x + 12.0f,
                         metaProcessContextPanelRect_.y + 8.0f, "Frame Operations");
        for(size_t i = 0; i < metaProcessContextRects_.size(); ++i)
        {
            metaProcessContextRects_[i] = { metaProcessContextPanelRect_.x + 8.0f,
                                             metaProcessContextPanelRect_.y + 30.0f + float(i) * rowHeight,
                                             metaProcessContextPanelRect_.w - 16.0f, rowHeight - 2.0f };
            drawButton(metaProcessContextRects_[i], labels[i], false);
        }
    }

void KapibaraUI::openMetaProcessContextMenu(float x, float y)
{
        if(!harmonicEditorPanelRect_.contains(x, y))
        {
            metaProcessContextMenuOpen_ = false;
            return;
        }

        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            if(!metaFrameRects_[(size_t)local].contains(x, y))
                continue;
            const int frameIndex = metaFramePageStart_ + local;
            if(auto *track = currentTrack(); track != nullptr && frameIndex < track->metaOsc.frameCount
               && !metaFrameSelected_[(size_t)frameIndex])
            {
                metaFrameSelected_.fill(false);
                selectedMetaFrame_ = frameIndex;
                metaFrameSelected_[(size_t)frameIndex] = true;
                metaFrameRangeAnchor_ = frameIndex;
            }
            break;
        }

        constexpr float menuWidth = 224.0f;
        constexpr float menuHeight = 230.0f;
        metaProcessContextX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - menuWidth - 4.0f));
        metaProcessContextY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - menuHeight - 4.0f));
        metaProcessContextMenuOpen_ = true;
    }

bool KapibaraUI::handleMetaProcessContextMenuClick(float x, float y)
{
        if(!metaProcessContextMenuOpen_)
            return false;
        metaProcessContextMenuOpen_ = false;
        for(size_t i = 0; i < metaProcessContextRects_.size(); ++i)
        {
            if(!metaProcessContextRects_[i].contains(x, y))
                continue;
            if(i < 5)
                applyWavetableProcess(int(i));
            else
                applySelectedFrameMorph(i == 5 ? 256 : 512);
            break;
        }
        return true;
    }

END_NAMESPACE_DISTRHO
