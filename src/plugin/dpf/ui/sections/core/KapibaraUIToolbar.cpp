#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Approximate master output level from the per-source live levels, scaled by
    // the master gain — good enough to drive the toolbar meter.
float KapibaraUI::masterLevel()
{
        float m = 0.0f;
        if(const auto *p = plugin())
            for(int i = 0; i < int(generator_.tracks.size()); ++i)
                if(generator_.tracks[(size_t)i].connectedToMaster)
                    m += p->sourceLiveLevel(i);
        return clampf(m * gain_, 0.0f, 1.0f);
    }

void KapibaraUI::drawMasterMeter(const Rect &r)
{
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 3.0f);
        fillColor(DesignTokens::controlBackground());
        fill();
        const float lvl = masterLevel();
        const float fillW = clampf(lvl, 0.0f, 1.0f) * (r.w - 2.0f);
        if(fillW > 1.0f)
        {
            beginPath();
            roundedRect(r.x + 1.0f, r.y + 1.0f, fillW, r.h - 2.0f, 2.0f);
            fillPaint(linearGradient(r.x, r.y, r.x + r.w, r.y,
                                     DesignTokens::accentGreen(), rgba(0xff5a4effU)));
            fill();
        }
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, 3.0f);
        strokeColor(DesignTokens::border());
        strokeWidth(1.0f);
        stroke();
    }

void KapibaraUI::drawBackground()
{
        const float w = static_cast<float>(uiW());
        const float h = static_cast<float>(uiH());
        beginPath();
        rect(0.0f, 0.0f, w, h);
        fillColor(DesignTokens::appBackground());
        fill();
    }

void KapibaraUI::drawToolbar()
{
        useUiFont();
        toolbar_ = { 0.0f, 0.0f, static_cast<float>(uiW()), 64.0f };
        beginPath();
        rect(toolbar_.x, toolbar_.y, toolbar_.w, toolbar_.h);
        fillColor(DesignTokens::panelBackground());
        fill();
        beginPath();
        rect(0.0f, toolbar_.h - 1.0f, toolbar_.w, 1.0f);
        fillColor(DesignTokens::divider());
        fill();

        // Logo mark.
        beginPath();
        roundedRect(16.0f, 16.0f, 8.0f, 32.0f, 3.0f);
        fillColor(DesignTokens::accentCyan());
        fill();

        uiFontSize(20.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(34.0f, 26.0f, "Kapibara", nullptr);
        uiFontSize(8.5f);
        fillColor(DesignTokens::textSecondary());
        text(35.0f, 42.0f, "ADDITIVE  SYNTH", nullptr);

        const float edge = static_cast<float>(uiW()) - 14.0f;
        // ---- Master section (far right): output knob + horizontal level meter ----
        gainRect_        = { edge - 96.0f, 12.0f, 96.0f, 40.0f };
        masterMeterRect_ = { gainRect_.x - 66.0f, 27.0f, 60.0f, 9.0f };
        const float right = masterMeterRect_.x - 16.0f;
        aboutRect_ = { right - 78.0f, 12.0f, 78.0f, 36.0f };
        menuRect_ = { aboutRect_.x - 86.0f, 12.0f, 78.0f, 36.0f };
        panicRect_ = { menuRect_.x - 76.0f, 12.0f, 68.0f, 36.0f };
        const Rect abRect { panicRect_.x - 102.0f, 12.0f, 94.0f, 36.0f };
        // MATRIX entry sits between the preset selector and A->B; the preset
        // selector gives up the width for it.
        matrixToolbarRect_ = { abRect.x - 96.0f, 12.0f, 88.0f, 36.0f };
        presetPrevRect_ = { 220.0f, 14.0f, 38.0f, 34.0f };
        presetNextRect_ = { matrixToolbarRect_.x - 48.0f, 14.0f, 38.0f, 34.0f };
        presetSelectRect_ = { 264.0f, 8.0f, std::max(140.0f, presetNextRect_.x - 272.0f), 46.0f };
        presetSaveRect_ = {};
        presetLoadRect_ = {};

        drawButton(presetPrevRect_, "<", false);
        drawButton(presetSelectRect_, presetLabel_.empty() ? "Select preset" : presetLabel_.c_str(), presetMenuOpen_);
        drawButton(presetNextRect_, ">", false);
        drawButton(matrixToolbarRect_, "MATRIX", matrixViewOpen_);
        drawButton(abRect, "A -> B", false);
        drawButton(panicRect_, "Panic", false);
        drawButton(menuRect_, "MENU", false);
        drawButton(aboutRect_, "ABOUT", false);

        // "MASTER" caption above the meter, then the meter and the output dial.
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        text(masterMeterRect_.x, masterMeterRect_.y - 11.0f, "MASTER", nullptr);
        drawMasterMeter(masterMeterRect_);
        drawKnob(gainRect_, "Master", gain_, gain_);

        statusRect_ = { 160.0f, 50.0f, 56.0f, 14.0f };
        char status[128];
        std::snprintf(status, sizeof(status), "%d voices", activeVoices_);
        uiFontSize(10.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(rgba(0x6a7d88ff));
        text(statusRect_.x, statusRect_.y + statusRect_.h * 0.5f, status, nullptr);

    }

END_NAMESPACE_DISTRHO
