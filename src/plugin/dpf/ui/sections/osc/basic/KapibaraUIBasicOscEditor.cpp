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

// Cross-unit modulation, local to this source. Deliberately NOT a matrix route:
// it wires one oscillator of the rack into another inside the voice, the way a
// classic two-oscillator synth does. Its DEPTH is a matrix destination though,
// so an LFO can still sweep it (ModDestination::OscModDepth).
void KapibaraUI::drawBasicOscModModule(const Rect &r, synth::SourceTrackParams &track)
{
        drawPanel(r, DesignTokens::groove(), DesignTokens::border());
        auto &mod = track.basicMod;
        const char *modeName = "OFF";
        switch(mod.mode)
        {
            case synth::BasicOscModMode::Ring: modeName = "RING"; break;
            case synth::BasicOscModMode::AM:   modeName = "AM";   break;
            case synth::BasicOscModMode::Sync: modeName = "SYNC"; break;
            default: break;
        }
        const float pad = 8.0f;
        const float w = r.w - pad * 2.0f;
        const float gap = 4.0f;
        // The module's height is whatever the waveform plot leaves it, which on a
        // busy patch is not much, so what survives is decided by priority rather
        // than by draw order: Depth first (it is what the module exists for, and
        // the LFO target), then MODE, then SRC/DST (one-click cycles with usable
        // defaults), and the header last.
        constexpr float kMinRow = 13.0f;
        const auto rowFor = [&](float top, int rows) {
            return ((r.y + r.h) - top - 2.0f - gap * float(rows - 1)) / float(rows);
        };
        const bool showRouting = rowFor(r.y + 2.0f, 3) >= kMinRow;
        const int rows = showRouting ? 3 : 2;
        const bool showLabel = rowFor(r.y + 20.0f, rows) >= kMinRow;
        if(showLabel)
            drawGroupLabel(r.x + 8.0f, r.y + 6.0f, "OSC MOD");

        float y = r.y + (showLabel ? 20.0f : 2.0f);
        const float rowH = std::min(rowFor(y, rows), 22.0f);

        basicModModeRect_ = { r.x + pad, y, w, rowH };
        drawButton(basicModModeRect_, modeName, mod.mode != synth::BasicOscModMode::Off);
        y += rowH + gap;

        if(showRouting)
        {
            // Which unit drives which. Two chips rather than a matrix row — this
            // routing never leaves the source.
            const float half = (w - 6.0f) * 0.5f;
            basicModSrcRect_ = { r.x + pad, y, half, rowH };
            basicModDstRect_ = { r.x + pad + half + 6.0f, y, half, rowH };
            drawButton(basicModSrcRect_, buttonText("SRC %d", int(mod.source) + 1), false);
            drawButton(basicModDstRect_, buttonText("DST %d", int(mod.target) + 1), false);
            y += rowH + gap;
        }

        basicModDepthRect_ = { r.x + pad, y, w, rowH };
        drawSlider(basicModDepthRect_, "Depth", mod.depth, mod.depth);
        y += rowH + gap;

        if((r.y + r.h) - y >= 11.0f)
        {
            useUiFont();
            uiFontSize(7.5f);
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary().withAlpha(0.6f));
            text(r.x + pad, y, showRouting ? "Depth is an LFO target"
                                           : buttonText("SRC %d -> DST %d",
                                                        int(mod.source) + 1, int(mod.target) + 1),
                 nullptr);
        }
    }

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

        basicWaveRects_.fill({});
        basicModModeRect_ = {}; basicModSrcRect_ = {};
        basicModDstRect_ = {};  basicModDepthRect_ = {};

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
        constexpr float miniH = 30.0f;   // per-unit waveform strip
        const float extraH = anyShapeExtra ? rowH + 4.0f : 0.0f;
        const float controlsH = 20.0f + rowH + 4.0f + miniH + 4.0f + rowH + 4.0f
                                + extraH + pitchH * 2.0f + 4.0f;
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

            // This unit's own waveform, one cycle, at its own pitch.
            basicWaveRects_[(size_t)u] = { cx, y, colW, miniH };
            {
                const Rect &mr = basicWaveRects_[(size_t)u];
                beginPath();
                roundedRect(mr.x, mr.y, mr.w, mr.h, 2.0f);
                fillColor(DesignTokens::appBackground().withAlpha(0.35f));
                fill();
                UnitWave w;
                w.bind(unit);
                const float my = mr.y + mr.h * 0.5f;
                const float ms = mr.h * 0.40f / std::max(0.25f, w.peak);
                const int mn = clampi(int(mr.w), 48, 256);
                scissor(mr.x + 1.0f, mr.y + 1.0f, mr.w - 2.0f, mr.h - 2.0f);
                beginPath();
                for(int sIdx = 0; sIdx <= mn; ++sIdx)
                {
                    const float t = float(sIdx) / float(mn);
                    const float px = mr.x + t * mr.w;
                    const float py = my - w.at(t) * ms;
                    if(sIdx == 0) moveTo(px, py); else lineTo(px, py);
                }
                strokeColor(unit.enabled ? DesignTokens::accentCyan()
                                         : DesignTokens::textSecondary().withAlpha(0.35f));
                strokeWidth(1.3f);
                stroke();
                resetScissor();
            }
            y += miniH + 4.0f;

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
        const float modW = std::min(230.0f, std::max(150.0f, r.w * 0.30f));
        const Rect plot { r.x, plotTop, r.w - modW - 12.0f, plotH };
        drawBasicOscModModule({ r.x + plot.w + 12.0f, plotTop, modW, plotH }, track);
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
