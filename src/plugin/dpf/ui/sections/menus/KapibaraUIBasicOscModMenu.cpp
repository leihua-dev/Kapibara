#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Mode picker for a Basic Oscillator rack's own cross-unit modulation. A menu
// rather than a cycling button: with six modes, stepping through them to reach
// the one you want means hearing every mode in between.

namespace
{
constexpr synth::BasicOscModMode kBasicModModes[synth::kBasicOscModModes] = {
    synth::BasicOscModMode::Off,  synth::BasicOscModMode::Ring,
    synth::BasicOscModMode::AM,   synth::BasicOscModMode::Sync,
    synth::BasicOscModMode::FM,   synth::BasicOscModMode::PM
};
} // namespace

const char *KapibaraUI::basicOscModModeName(synth::BasicOscModMode m)
{
        switch(m)
        {
            case synth::BasicOscModMode::Ring: return "RING";
            case synth::BasicOscModMode::AM:   return "AM";
            case synth::BasicOscModMode::Sync: return "SYNC";
            case synth::BasicOscModMode::FM:   return "FM";
            case synth::BasicOscModMode::PM:   return "PM";
            case synth::BasicOscModMode::Off:  break;
        }
        return "OFF";
    }

void KapibaraUI::openBasicOscModMenu(uint32_t trackId, float x, float y)
{
        constexpr float rowH = 20.0f;
        constexpr float menuW = 120.0f;
        const float h = 22.0f + rowH * float(synth::kBasicOscModModes);
        basicModMenuTrackId_ = trackId;
        basicModMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - menuW));
        basicModMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - h));
        basicModMenuOpen_ = true;
    }

void KapibaraUI::drawBasicOscModMenu()
{
        if(!basicModMenuOpen_)
        {
            basicModMenuRects_.fill({});
            return;
        }
        constexpr float rowH = 20.0f;
        constexpr float menuW = 120.0f;
        const Rect panel { basicModMenuX_, basicModMenuY_, menuW,
                           22.0f + rowH * float(synth::kBasicOscModModes) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f);
        fillColor(rgba(0xc8d6dcff));
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, "Osc mod mode", nullptr);

        synth::BasicOscModMode current = synth::BasicOscModMode::Off;
        const int ti = trackIndexOfId(basicModMenuTrackId_);
        if(ti >= 0)
            current = generator_.tracks[(size_t)ti].basicMod.mode;

        for(int i = 0; i < synth::kBasicOscModModes; ++i)
        {
            basicModMenuRects_[(size_t)i] = { panel.x + 6.0f, panel.y + 20.0f + float(i) * rowH,
                                              menuW - 12.0f, rowH - 2.0f };
            drawButton(basicModMenuRects_[(size_t)i], basicOscModModeName(kBasicModModes[i]),
                       kBasicModModes[i] == current);
        }
    }

bool KapibaraUI::handleBasicOscModMenuClick(float x, float y)
{
        if(!basicModMenuOpen_)
            return false;
        basicModMenuOpen_ = false;
        const int ti = trackIndexOfId(basicModMenuTrackId_);
        if(ti < 0)
            return true;
        auto &track = generator_.tracks[(size_t)ti];
        for(int i = 0; i < synth::kBasicOscModModes; ++i)
        {
            if(!basicModMenuRects_[(size_t)i].contains(x, y))
                continue;
            track.basicMod.mode = kBasicModModes[i];
            // A mode with nothing dialled in would look active but do nothing.
            if(track.basicMod.mode != synth::BasicOscModMode::Off && track.basicMod.depth <= 0.0f)
                track.basicMod.depth = 0.5f;
            pushTrackById(track.id);
            return true;
        }
        return true;  // clicking off the menu just closes it
    }

END_NAMESPACE_DISTRHO
