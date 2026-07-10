#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
constexpr synth::WavetableWarpMode kWarpModes[4] = {
    synth::WavetableWarpMode::None, synth::WavetableWarpMode::Bend,
    synth::WavetableWarpMode::Squeeze, synth::WavetableWarpMode::Skew
};
constexpr const char *kWarpModeShortNames[4] = { "None", "Bend", "Squeeze", "Skew" };
} // namespace

void KapibaraUI::openWarpModeMenu(uint32_t trackId, float x, float y)
{
        warpModeMenuTrackId_ = trackId;
        constexpr float rowH = 20.0f;
        constexpr float menuW = 120.0f;
        constexpr int rows = 4;
        warpModeMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - menuW));
        warpModeMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - (22.0f + rowH * float(rows))));
        warpModeMenuOpen_ = true;
    }

void KapibaraUI::drawWarpModeMenu()
{
        if(!warpModeMenuOpen_)
            return;
        constexpr float rowH = 20.0f;
        constexpr float menuW = 120.0f;
        const Rect panel { warpModeMenuX_, warpModeMenuY_, menuW, 22.0f + rowH * 4.0f };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, "Warp mode", nullptr);

        synth::WavetableWarpMode current = synth::WavetableWarpMode::None;
        if(warpModeMenuTrackId_ == 0)
        {
            current = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_].warpMode;
        }
        else
        {
            const int ti = trackIndexOfId(warpModeMenuTrackId_);
            if(ti >= 0)
                current = generator_.tracks[(size_t)ti].metaOsc.warpMode;
        }

        for(int i = 0; i < 4; ++i)
        {
            warpModeMenuRects_[(size_t)i] = { panel.x + 6.0f, panel.y + 20.0f + float(i) * rowH,
                                               menuW - 12.0f, rowH - 2.0f };
            drawButton(warpModeMenuRects_[(size_t)i], kWarpModeShortNames[i], kWarpModes[i] == current);
        }
    }

bool KapibaraUI::handleWarpModeMenuClick(float x, float y)
{
        if(!warpModeMenuOpen_)
            return false;
        warpModeMenuOpen_ = false;
        for(int i = 0; i < 4; ++i)
        {
            if(!warpModeMenuRects_[(size_t)i].contains(x, y))
                continue;
            if(warpModeMenuTrackId_ == 0)
            {
                generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_].warpMode = kWarpModes[i];
                pushMetaPartialRuntime();
            }
            else
            {
                const int ti = trackIndexOfId(warpModeMenuTrackId_);
                if(ti >= 0)
                {
                    generator_.tracks[(size_t)ti].metaOsc.warpMode = kWarpModes[i];
                    pushTrackById(warpModeMenuTrackId_);
                }
            }
            return true;
        }
        return true;
    }

END_NAMESPACE_DISTRHO
