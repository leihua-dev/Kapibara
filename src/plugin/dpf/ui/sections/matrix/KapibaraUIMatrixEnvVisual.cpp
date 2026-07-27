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
        const int count = clampi(pointCount, 2, synth::kMaxMatrixEnvPoints);
        const float plotX = r.x + 8.0f;
        const float plotW = r.w - 16.0f;
        const float baseY = r.y + r.h - 5.0f;
        const float plotH = r.h - 10.0f;

        // Walk the curve SEGMENT BY SEGMENT rather than on one uniform grid, and
        // land exactly on every breakpoint. A fixed grid cuts corners: at 80
        // points across a wide editor a sample falls every ~19 px, so a sharp
        // breakpoint between two samples is drawn as a diagonal shortcut and the
        // displayed curve is visibly not the one being played. Density inside a
        // segment follows its pixel width, so nothing is ever coarser than ~1 px.
        const auto emit = [&](bool firstMoves) {
            bool first = true;
            const auto put = [&](float x, float yv) {
                const float px = plotX + x * plotW;
                const float py = baseY - yv * plotH;
                if(first && firstMoves) moveTo(px, py); else lineTo(px, py);
                first = false;
            };
            put(0.0f, synth::pointCurveEval(points, count, 0.0f));
            for(int i = 0; i + 1 < count; ++i)
            {
                const float x0 = clampf(points[(size_t)i].x, 0.0f, 1.0f);
                const float x1 = clampf(points[(size_t)(i + 1)].x, 0.0f, 1.0f);
                if(x1 <= x0)
                    continue;
                const int steps = clampi(int((x1 - x0) * plotW), 2, 512);
                for(int s = 1; s <= steps; ++s)
                {
                    // Nudge off the breakpoint so the sample belongs to THIS
                    // segment; the corner itself is the next segment's x0.
                    const float x = x0 + (x1 - x0) * (float(s) / float(steps));
                    put(x, synth::pointCurveEval(points, count,
                                                 s == steps ? x - 1.0e-5f : x));
                }
                put(x1, synth::pointCurveEval(points, count, x1));
            }
            put(1.0f, synth::pointCurveEval(points, count, 1.0f));
        };

        beginPath();
        emit(true);
        lineTo(r.x + r.w - 8.0f, baseY);
        lineTo(plotX, baseY);
        closePath();
        fillColor(lineColor.withAlpha(0.08f));
        fill();
        beginPath();
        emit(true);
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
