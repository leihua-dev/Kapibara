#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawKeyboard()
{
        keyboardRect_ = { 16.0f, static_cast<float>(uiH()) - 86.0f, static_cast<float>(uiW()) - 32.0f, 70.0f };
        drawPanel(keyboardRect_, DesignTokens::panelRaised(), DesignTokens::border());

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
