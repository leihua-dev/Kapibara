#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawKeyboard()
{
        const float top = static_cast<float>(uiH()) - 86.0f;
        constexpr float wheelW = 26.0f;
        constexpr float wheelGap = 8.0f;
        const float wheelsW = wheelW * 2.0f + wheelGap + 12.0f;
        keyboardRect_ = { 16.0f + wheelsW, top,
                          static_cast<float>(uiW()) - 32.0f - wheelsW, 70.0f };
        drawPanel(keyboardRect_, DesignTokens::panelRaised(), DesignTokens::border());

        // Bend / Mod wheels, where a hardware synth puts them.
        pitchWheelRect_ = { 16.0f, top, wheelW, 70.0f };
        modWheelRect_ = { 16.0f + wheelW + wheelGap, top, wheelW, 70.0f };
        const auto drawWheel = [&](const Rect &w, const char *label, float pos01, bool centred) {
            drawPanel(w, DesignTokens::panelRaised(), DesignTokens::border());
            const float trackTop = w.y + 8.0f;
            const float trackH = w.h - 22.0f;
            beginPath();
            roundedRect(w.x + 7.0f, trackTop, w.w - 14.0f, trackH, 4.0f);
            fillColor(DesignTokens::appBackground().withAlpha(0.6f));
            fill();
            if(centred)
                strokeLine(w.x + 5.0f, trackTop + trackH * 0.5f, w.x + w.w - 5.0f,
                           trackTop + trackH * 0.5f, DesignTokens::divider(), 1.0f);
            // Handle: pos01 0 = bottom, 1 = top.
            const float hy = trackTop + (1.0f - clampf(pos01, 0.0f, 1.0f)) * (trackH - 12.0f);
            beginPath();
            roundedRect(w.x + 4.0f, hy, w.w - 8.0f, 12.0f, 3.0f);
            fillColor(DesignTokens::accentCyan());
            fill();
            useUiFont();
            uiFontSize(7.5f);
            textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
            fillColor(DesignTokens::textSecondary());
            text(w.x + w.w * 0.5f, w.y + w.h - 2.0f, label, nullptr);
        };
        drawWheel(pitchWheelRect_, "BEND", pitchWheelValue_ * 0.5f + 0.5f, true);
        drawWheel(modWheelRect_, "MOD", modWheelValue_, false);

        constexpr int first = 36;
        constexpr int keys = 61;
        const float keyW = keyboardRect_.w / float(keys);
        for(int i = 0; i < keys; ++i)
        {
            const int note = first + i;
            const bool black = isBlackKey(note);
            const bool pressed = note == mouseKey_ || computerKeys_[(size_t)note];
            const Rect r { keyboardRect_.x + i * keyW + 1.0f, keyboardRect_.y + 6.0f, keyW - 2.0f,
                           black ? keyboardRect_.h * 0.58f : keyboardRect_.h - 12.0f };
            beginPath();
            roundedRect(r.x, r.y, r.w, r.h, black ? 2.0f : 3.0f);
            fillColor(pressed ? DesignTokens::accentGreen()
                              : (black ? DesignTokens::appBackground() : rgba(0xdde3e5ff)));
            fill();
            beginPath();
            roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, black ? 2.0f : 3.0f);
            strokeColor(black ? DesignTokens::divider() : DesignTokens::border());
            strokeWidth(1.0f);
            stroke();
        }
    }

END_NAMESPACE_DISTRHO
