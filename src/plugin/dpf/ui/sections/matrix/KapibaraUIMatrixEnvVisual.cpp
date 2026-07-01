#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawMatrixEnvCurve(const Rect &r, const synth::MatrixEnvPoint *points, int pointCount,
                        Color lineColor)
{
        drawPanel(r, DesignTokens::controlBackground(), DesignTokens::border());
        beginPath();
        roundedRect(r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, r.h - 8.0f, DesignTokens::controlRadius);
        fillColor(DesignTokens::appBackground().withAlpha(0.35f));
        fill();
        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        const Color gridCol = DesignTokens::divider().withAlpha(0.35f);
        for(int i = 1; i < 16; ++i)
        {
            const float gx = r.x + 8.0f + (float(i) / 16.0f) * (r.w - 16.0f);
            strokeLine(gx, r.y + 5.0f, gx, r.y + r.h - 5.0f, gridCol, 1.0f);
        }
        for(int i = 1; i < 8; ++i)
        {
            const float gy = r.y + r.h - 5.0f - (float(i) / 8.0f) * (r.h - 10.0f);
            strokeLine(r.x + 8.0f, gy, r.x + r.w - 8.0f, gy, gridCol, 1.0f);
        }
        resetScissor();
        beginPath();
        const int count = clampi(pointCount, 2, synth::kMaxMatrixEnvPoints);
        for(int s = 0; s < 80; ++s)
        {
            const float x = float(s) / 79.0f;
            const float yv = synth::pointCurveEval(points, count, x);
            const float px = r.x + 8.0f + x * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - yv * (r.h - 10.0f);
            if(s == 0) moveTo(px, py); else lineTo(px, py);
        }
        lineTo(r.x + r.w - 8.0f, r.y + r.h - 5.0f);
        lineTo(r.x + 8.0f, r.y + r.h - 5.0f);
        closePath();
        fillColor(lineColor.withAlpha(0.08f));
        fill();
        beginPath();
        for(int s = 0; s < 80; ++s)
        {
            const float x = float(s) / 79.0f;
            const float yv = synth::pointCurveEval(points, count, x);
            const float px = r.x + 8.0f + x * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - yv * (r.h - 10.0f);
            if(s == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(lineColor);
        strokeWidth(2.0f);
        stroke();
        for(int i = 0; i < count; ++i)
        {
            const auto &pt = points[(size_t)i];
            const float px = r.x + 8.0f + clampf(pt.x, 0.0f, 1.0f) * (r.w - 16.0f);
            const float py = r.y + r.h - 5.0f - clampf(pt.y, 0.0f, 1.0f) * (r.h - 10.0f);
            beginPath();
            circle(px, py, i == selectedEnvPoint_ ? 5.0f : 3.5f);
            fillColor(i == selectedEnvPoint_ ? DesignTokens::accentBlue() : DesignTokens::accentCyan());
            fill();
            beginPath();
            circle(px, py, i == selectedEnvPoint_ ? 5.0f : 3.5f);
            strokeColor(DesignTokens::appBackground());
            strokeWidth(1.0f);
            stroke();
        }
    }

END_NAMESPACE_DISTRHO
