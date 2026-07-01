#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawAdsrCurve(const Rect &r)
{
        drawPlotBackground(r, 5, 4);
        const float a = std::max(0.01f, adsr_.attack);
        const float d = std::max(0.01f, adsr_.decay);
        const float rr = std::max(0.01f, adsr_.release);
        const float sum = a + d + rr + 0.5f;
        const float x0 = r.x + 10.0f;
        const float xA = r.x + r.w * (a / sum);
        const float xD = r.x + r.w * ((a + d) / sum);
        const float xS = r.x + r.w * ((a + d + 0.5f) / sum);
        const float xR = r.x + r.w - 10.0f;
        const float y0 = r.y + r.h - 10.0f;
        const float y1 = r.y + 10.0f;
        const float yS = r.y + r.h - 10.0f - adsr_.sustain * (r.h - 20.0f);

        beginPath();
        moveTo(x0, y0);
        lineTo(xA, y1);
        lineTo(xD, yS);
        lineTo(xS, yS);
        lineTo(xR, y0);
        lineTo(x0, y0);
        closePath();
        fillColor(DesignTokens::accentGreen().withAlpha(0.09f));
        fill();
        beginPath();
        moveTo(x0, y0);
        lineTo(xA, y1);
        lineTo(xD, yS);
        lineTo(xS, yS);
        lineTo(xR, y0);
        strokeColor(DesignTokens::accentGreen());
        strokeWidth(2.0f);
        stroke();
    }

void KapibaraUI::drawAdsrCurve(const Rect &r, const synth::AdsrParams &env)
{
        drawPlotBackground(r, 5, 4);
        // Times can be exactly 0 (e.g. instant attack). Segment x positions are
        // mapped into [x0, xR] so a 0-length stage sits exactly at its start edge
        // instead of overshooting backwards.
        const float a = std::max(0.0f, env.attack);
        const float d = std::max(0.0f, env.decay);
        const float rel = std::max(0.0f, env.release);
        const float sustainSpan = 0.35f;  // fixed visual width for the held sustain
        const float sum = std::max(1.0e-3f, a + d + rel + sustainSpan);
        const float x0 = r.x + 10.0f;
        const float xR = r.x + r.w - 10.0f;
        const float plotW = xR - x0;
        const float xA = x0 + plotW * (a / sum);
        const float xD = x0 + plotW * ((a + d) / sum);
        const float xS = x0 + plotW * ((a + d + sustainSpan) / sum);
        const float y0 = r.y + r.h - 8.0f;
        const float y1 = r.y + 8.0f;
        const float sustain = clampf(env.sustain, 0.0f, 1.0f);

        // Store hit info for Ctrl-drag segment bending.
        ampAdsrRect_ = r;
        ampAdsrXA_ = xA; ampAdsrXD_ = xD; ampAdsrXS_ = xS; ampAdsrXR_ = xR;

        const auto vy = [&](float v) { return y0 - clampf(v, 0.0f, 1.0f) * (y0 - y1); };
        constexpr int kN = 18;
        // Trace the curved A/D/S/R outline (matches the engine's adsrCurveEval shaping).
        const auto trace = [&]() {
            moveTo(x0, vy(0.0f));
            for(int i = 1; i <= kN; ++i)
            {
                const float t = float(i) / float(kN);
                lineTo(x0 + (xA - x0) * t, vy(synth::adsrCurveEval(t, env.curveA)));
            }
            for(int i = 1; i <= kN; ++i)
            {
                const float t = float(i) / float(kN);
                const float v = sustain + (1.0f - sustain) * (1.0f - synth::adsrCurveEval(t, env.curveD));
                lineTo(xA + (xD - xA) * t, vy(v));
            }
            lineTo(xS, vy(sustain));
            for(int i = 1; i <= kN; ++i)
            {
                const float t = float(i) / float(kN);
                lineTo(xS + (xR - xS) * t, vy(sustain * (1.0f - synth::adsrCurveEval(t, env.curveR))));
            }
        };
        beginPath();
        trace();
        lineTo(xR, y0);
        lineTo(x0, y0);
        closePath();
        fillColor(DesignTokens::accentGreen().withAlpha(0.09f));
        fill();
        beginPath();
        trace();
        strokeColor(DesignTokens::accentGreen());
        strokeWidth(2.0f);
        stroke();

        // Time scale: boundary guide lines + per-stage duration labels (seconds).
        useUiFont();
        uiFontSize(8.0f);
        const Color tcol = DesignTokens::divider().withAlpha(0.65f);
        const auto stageLabel = [&](float xpos, char tag, float secs, bool rightAlign) {
            strokeLine(xpos, r.y + 4.0f, xpos, r.y + r.h - 4.0f, tcol, 1.0f);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%c %.3gs", tag, double(secs));
            textAlign((rightAlign ? ALIGN_RIGHT : ALIGN_LEFT) | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary());
            text(xpos + (rightAlign ? -3.0f : 3.0f), r.y + 4.0f, buf, nullptr);
        };
        stageLabel(xA, 'A', a, false);
        stageLabel(xD, 'D', d, false);
        stageLabel(xR, 'R', rel, true);
    }

END_NAMESPACE_DISTRHO
