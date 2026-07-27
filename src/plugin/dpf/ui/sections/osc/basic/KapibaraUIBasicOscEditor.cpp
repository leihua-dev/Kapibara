#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
constexpr float kTwoPiF = 6.28318530717958647692f;

// One unit's partial series, resolved once and evaluated many times. Sharing
// basicOscPartials with the seed builder is the point: a preview drawn from its
// own formula would drift the moment a shape changed.
struct UnitWave
{
    float ratio[synth::kMaxWavetablePartials];
    float amp[synth::kMaxWavetablePartials];
    float phase[synth::kMaxWavetablePartials];
    int n = 0;
    float level = 1.0f;
    float peak = 0.0f;

    void bind(const synth::BasicOscUnit &u)
    {
        n = synth::basicOscPartials(u, synth::kMaxWavetablePartials, ratio, amp, phase);
        level = clampf(u.level, 0.0f, 1.0f);
        // Pitch scales the series here exactly as buildBasicSeed does, so a
        // detuned unit visibly runs at its own rate against the others.
        const float semis = float(u.pitchOct) * 12.0f + float(u.pitchSem)
                            + u.pitchFin / 100.0f + u.pitchCrs / 100.0f;
        const float pitchRatio = std::pow(2.0f, semis / 12.0f);
        peak = 0.0f;
        for(int i = 0; i < n; ++i)
        {
            ratio[i] *= pitchRatio;
            peak += amp[i];
        }
        peak *= level;
    }

    float at(float t) const
    {
        float v = 0.0f;
        for(int i = 0; i < n; ++i)
            v += amp[i] * std::sin(kTwoPiF * t * ratio[i] + phase[i]);
        return v * level;
    }
};
} // namespace

// The Basic Oscillator editor is a rack of kBasicOscUnits oscillators side by
// side: one basic shape is four controls, and stacking three is what makes the
// track worth having.
void KapibaraUI::drawBasicTrackEditor(const Rect &r, synth::SourceTrackParams &track)
{
        basicUnitEnableRects_.fill({});
        basicShapeRects_.fill({});
        basicLevelRects_.fill({});
        basicPulseRects_.fill({});
        basicSubRects_.fill({});
        for(auto &row : basicPitchRects_)
            row.fill({});

        constexpr float kColGap = 10.0f;
        const int units = synth::kBasicOscUnits;
        const float colW = (r.w - kColGap * float(units - 1)) / float(units);
        // Only Pulse and Sub have an extra parameter. When no unit is on one of
        // those shapes the row isn't reserved at all, which is height the
        // waveform plot below gets to keep.
        bool anyShapeExtra = false;
        for(const auto &u : track.basicUnits)
            if(u.shape == synth::BasicOscillatorShape::Pulse
               || u.shape == synth::BasicOscillatorShape::Sub)
                anyShapeExtra = true;
        // Controls on top, one shared waveform plot underneath.
        constexpr float rowH = 22.0f;
        constexpr float pitchH = 24.0f;
        const float extraH = anyShapeExtra ? rowH + 4.0f : 0.0f;
        const float controlsH = 20.0f + rowH + 4.0f + rowH + 4.0f + extraH + pitchH * 2.0f + 4.0f;
        const float plotTop = r.y + controlsH + 18.0f;
        const float plotH = (r.y + r.h) - plotTop;

        for(int u = 0; u < units; ++u)
        {
            auto &unit = track.basicUnits[(size_t)u];
            const float cx = r.x + float(u) * (colW + kColGap);
            float y = r.y;

            // Header: OSC n + on/off. A disabled unit dims but keeps its settings.
            basicUnitEnableRects_[(size_t)u] = { cx, y, colW, 16.0f };
            drawButton(basicUnitEnableRects_[(size_t)u],
                       buttonText("OSC %d", u + 1), unit.enabled);
            y += 20.0f;

            basicShapeRects_[(size_t)u] = { cx, y, colW, rowH };
            drawButton(basicShapeRects_[(size_t)u],
                       synth::basicOscillatorShapeName(unit.shape), false);
            y += rowH + 4.0f;

            basicLevelRects_[(size_t)u] = { cx, y, colW, rowH };
            drawSlider(basicLevelRects_[(size_t)u], "Level", unit.level, unit.level);
            y += rowH + 4.0f;

            // Shape-specific control only where it means something: Pulse Width on
            // a sine is a dead slider, so it is simply not drawn (and its rect
            // stays zeroed, so it cannot take clicks either).
            if(unit.shape == synth::BasicOscillatorShape::Pulse)
            {
                basicPulseRects_[(size_t)u] = { cx, y, colW, rowH };
                drawSlider(basicPulseRects_[(size_t)u], "Pulse Width",
                           unit.pulseWidth, unit.pulseWidth);
            }
            else if(unit.shape == synth::BasicOscillatorShape::Sub)
            {
                basicSubRects_[(size_t)u] = { cx, y, colW, rowH };
                drawSlider(basicSubRects_[(size_t)u], "Sub Level",
                           unit.subLevel, unit.subLevel);
            }
            y += extraH;

            // Pitch as 2x2 so a column stays narrow.
            const float pw = (colW - 6.0f) * 0.5f;
            basicPitchRects_[(size_t)u][0] = { cx, y, pw, pitchH };
            basicPitchRects_[(size_t)u][1] = { cx + pw + 6.0f, y, pw, pitchH };
            basicPitchRects_[(size_t)u][2] = { cx, y + pitchH + 4.0f, pw, pitchH };
            basicPitchRects_[(size_t)u][3] = { cx + pw + 6.0f, y + pitchH + 4.0f, pw, pitchH };
            drawPitchControl(basicPitchRects_[(size_t)u][0], "OCT", unit.pitchOct, false);
            drawPitchControl(basicPitchRects_[(size_t)u][1], "SEM", unit.pitchSem, false);
            drawPitchControl(basicPitchRects_[(size_t)u][2], "FIN",
                             int(std::round(unit.pitchFin)), false);
            drawPitchControl(basicPitchRects_[(size_t)u][3], "CRS",
                             int(std::round(unit.pitchCrs)), false);
        }

        // A short strip still reads; only give up when there is genuinely no room.
        if(plotH < 32.0f)
            return;

        // Real waveform, not a stand-in: each active unit faint, their sum bright.
        // The old preview drew a fixed two-cycle sine whatever the shape was.
        const Rect plot { r.x, plotTop, r.w, plotH };
        drawSectionTitle(r.x, plotTop - 16.0f, "Waveform");
        drawPlotBackground(plot, 8, 4);
        const float midY = plot.y + plot.h * 0.5f;
        const int steps = clampi(int(plot.w), 96, 512);
        scissor(plot.x + 2.0f, plot.y + 2.0f, plot.w - 4.0f, plot.h - 4.0f);

        // Resolve each unit's series once — the point loops below run per pixel.
        std::array<UnitWave, synth::kBasicOscUnits> waves;
        std::array<bool, synth::kBasicOscUnits> live {};
        float sumPeak = 0.0f;
        for(int u = 0; u < units; ++u)
        {
            live[(size_t)u] = track.basicUnits[(size_t)u].enabled;
            if(!live[(size_t)u])
                continue;
            waves[(size_t)u].bind(track.basicUnits[(size_t)u]);
            sumPeak += waves[(size_t)u].peak;
        }
        // One shared vertical scale, so the units' relative loudness is visible
        // instead of each being normalized to look the same size.
        const float scale = plot.h * 0.42f / std::max(0.25f, sumPeak);

        for(int u = 0; u < units; ++u)
        {
            if(!live[(size_t)u])
                continue;
            beginPath();
            for(int s = 0; s <= steps; ++s)
            {
                const float t = float(s) / float(steps);
                const float px = plot.x + t * plot.w;
                const float py = midY - waves[(size_t)u].at(t) * scale;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(DesignTokens::accentCyan().withAlpha(0.30f));
            strokeWidth(1.0f);
            stroke();
        }

        beginPath();
        for(int s = 0; s <= steps; ++s)
        {
            const float t = float(s) / float(steps);
            float sum = 0.0f;
            for(int u = 0; u < units; ++u)
                if(live[(size_t)u])
                    sum += waves[(size_t)u].at(t);
            const float px = plot.x + t * plot.w;
            const float py = midY - sum * scale;
            if(s == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(DesignTokens::accentGreen());
        strokeWidth(1.8f);
        stroke();
        resetScissor();
    }

END_NAMESPACE_DISTRHO
