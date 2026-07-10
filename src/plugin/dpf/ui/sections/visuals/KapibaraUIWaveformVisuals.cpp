#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawPlotBackground(const Rect &r, int columns, int rows)
{
        drawPanel(r, DesignTokens::controlBackground(), DesignTokens::border());
        beginPath();
        roundedRect(r.x + 4.0f, r.y + 4.0f, r.w - 8.0f, r.h - 8.0f, DesignTokens::controlRadius);
        fillColor(DesignTokens::appBackground().withAlpha(0.35f));
        fill();
        for(int i = 1; i < columns; ++i)
        {
            const float x = r.x + 6.0f + (r.w - 12.0f) * float(i) / float(columns);
            strokeLine(x, r.y + 6.0f, x, r.y + r.h - 6.0f, DesignTokens::divider().withAlpha(0.65f), 1.0f);
        }
        for(int i = 1; i < rows; ++i)
        {
            const float y = r.y + 6.0f + (r.h - 12.0f) * float(i) / float(rows);
            strokeLine(r.x + 6.0f, y, r.x + r.w - 6.0f, y, DesignTokens::divider().withAlpha(0.65f), 1.0f);
        }
    }

void KapibaraUI::drawMeta3DWaveform(const Rect &r, const synth::WavetablePartialSlot &slot, int trackIndex)
{
        // No framed box — the stacked waves blend directly into the module panel.
        // A faint inset darkening gives just enough depth for the lines to read.
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 2.0f);
        fillColor(DesignTokens::appBackground().withAlpha(0.30f));
        fill();
        if(slot.frameCount <= 0) return;

        // Follow the knob (slot.morph) whenever no voice is actively playing this
        // track, so dragging Morph previews live even with no note held; once a
        // voice is active, switch to the engine's real (modulated) morph value.
        float liveMorph = slot.morph;
        if(trackIndex >= 0)
        {
            if(const auto *p = plugin())
            {
                const float live = p->sourceLiveMorph(trackIndex);
                if(live >= 0.0f)
                    liveMorph = live;
            }
        }
        const int morphFrame = slot.frameCount > 1
            ? clampi(int(liveMorph * float(slot.frameCount - 1) + 0.5f), 0, slot.frameCount - 1)
            : 0;

        // 最多显示 16 帧（step 抽样）
        constexpr int kMaxShow = 16;
        int stride = std::max(1, slot.frameCount / kMaxShow);
        std::vector<int> frames;
        frames.reserve(kMaxShow + 1);
        for(int i = 0; i < slot.frameCount; i += stride)
            frames.push_back(i);
        // 确保 morphFrame 包含在内（不重复添加）
        if(frames.empty() || frames.back() != morphFrame)
        {
            // 若 morphFrame 还不在列表里则插入
            bool found = false;
            for(int f : frames) if(f == morphFrame) { found = true; break; }
            if(!found) frames.push_back(morphFrame);
        }
        // 按帧号排序（保持 back→front 顺序）
        std::sort(frames.begin(), frames.end());

        const int N = int(frames.size());
        if(N == 0) return;

        // Perspective depth: farther frames sit higher and smaller in amplitude.
        // The horizontal span (one full wave cycle) is depth-independent, so the
        // wavelength reads the same regardless of how far back a frame is stacked.
        const float perspH = std::min(r.h * 0.50f, float(N) * 5.0f);
        const float waveH  = r.h - perspH - 8.0f;
        // front frame(idx N-1) 绘制在 r 底部居中
        const float frontY = r.y + r.h - 8.0f - waveH * 0.5f;
        // Left gutter reserved for the vertical Morph scrubber, so the groove/handle
        // have a clean column instead of hiding behind the frame baselines.
        const float morphGutterW = 18.0f;
        const float xL = r.x + morphGutterW + 6.0f;
        const float xR = r.x + r.w - 8.0f;
        // Amplitude scale: raised from the old 0.45/0.45 pair (frames read too flat/
        // small) — back frames now show more of their shape too, not just the front.
        constexpr float kAmpBaseMin = 0.55f;
        constexpr float kAmpFactor  = 0.58f;

        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);

        // (No vertical grid lines — the per-frame horizontal baselines below are the
        // only "grid", and they carry the perspective on their own.)

        // Geometry of the live morph playhead, captured for the phase-start marker.
        float morphYC = 0.0f, morphAmp = 0.0f;
        bool haveMorphGeom = false;

        constexpr int kPts = 80;
        // Background stack: every sampled frame drawn faint and receding by depth.
        // The bright "current" waveform is a separate crossfading playhead below.
        for(int di = 0; di < N; ++di)
        {
            const int frameIdx = frames[(size_t)di];
            const float depthT = float(di) / float(std::max(1, N - 1)); // 0=back,1=front
            const float yCenter = frontY - perspH * (1.0f - depthT);
            const float ampScale = (kAmpBaseMin + (1.0f - kAmpBaseMin) * depthT) * waveH * kAmpFactor;

            strokeLine(xL, yCenter, xR, yCenter,
                       DesignTokens::divider().withAlpha(0.14f + 0.18f * depthT), 0.6f);

            const auto &frame = slot.frames[(size_t)frameIdx];
            lineCap(ROUND);
            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yCenter - sampleFrameWarped(frame, t, slot.warpMode, slot.warpAmount) * ampScale;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(DesignTokens::accentBlue().withAlpha(0.10f + 0.16f * depthT));
            strokeWidth(0.7f + 0.4f * depthT);
            stroke();
            lineCap(BUTT);
        }

        // Crossfading morph playhead: linear blend of the two neighbouring frames by
        // the fractional morph position, drawn at a continuous depth so scrubbing
        // fades smoothly frame-to-frame instead of snapping.
        {
            const float framePos = clampf(liveMorph, 0.0f, 1.0f) * float(std::max(0, slot.frameCount - 1));
            const int fA = clampi(int(framePos), 0, slot.frameCount - 1);
            const int fB = std::min(fA + 1, slot.frameCount - 1);
            const float frac = framePos - float(fA);
            const auto &frA = slot.frames[(size_t)fA];
            const auto &frB = slot.frames[(size_t)fB];
            const float depthT = clampf(liveMorph, 0.0f, 1.0f);
            const float yC  = frontY - perspH * (1.0f - depthT);
            const float amp = (kAmpBaseMin + (1.0f - kAmpBaseMin) * depthT) * waveH * kAmpFactor;
            morphYC = yC; morphAmp = amp; haveMorphGeom = true;

            const auto sampleBlend = [&](float t) {
                const float a = sampleFrameWarped(frA, t, slot.warpMode, slot.warpAmount);
                const float b = sampleFrameWarped(frB, t, slot.warpMode, slot.warpAmount);
                return a + (b - a) * frac;
            };

            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yC - sampleBlend(t) * amp;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            lineTo(xR, yC);
            lineTo(xL, yC);
            closePath();
            fillColor(DesignTokens::accentGreen().withAlpha(0.10f));
            fill();

            lineCap(ROUND);
            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yC - sampleBlend(t) * amp;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(DesignTokens::accentGreen());
            strokeWidth(2.6f);
            stroke();
            lineCap(BUTT);
            strokeLine(xL, yC, xR, yC, DesignTokens::accentGreen().withAlpha(0.18f), 0.5f);
        }

        // Phase start marker on the live frame: one cycle spans xL..xR, so the
        // note-on read position sits at fraction phase/2pi. When Rand > 0 the start
        // can land anywhere in [phase, phase + phaseRandom*2pi), shown as a shaded band.
        // Only drawn when phase/rand are actually engaged — at defaults it would sit
        // on the left edge and read as a stray debug line.
        const float randFrac = clampf(slot.phaseRandom, 0.0f, 1.0f);
        const bool phaseEngaged = std::abs(slot.phase) > 1.0e-3f || randFrac > 1.0e-3f;
        if(haveMorphGeom && phaseEngaged)
        {
            const float twoPi = 2.0f * kPi;
            float startFrac = slot.phase / twoPi;   // [-0.5, 0.5]
            startFrac -= std::floor(startFrac);     // wrap to [0, 1)
            const float span = xR - xL;
            const float x0 = xL + startFrac * span;
            const float hMark = morphAmp * 1.05f + 2.0f;
            const Color phaseCol = rgba(0xffc044ffu);

            if(randFrac > 1.0e-3f)
            {
                const auto fillBand = [&](float bx, float bw) {
                    if(bw <= 0.0f) return;
                    beginPath();
                    roundedRect(bx, morphYC - hMark, bw, hMark * 2.0f, 2.0f);
                    fillColor(phaseCol.withAlpha(0.12f));
                    fill();
                };
                const float endX = x0 + randFrac * span;
                if(endX <= xR + 0.5f)
                    fillBand(x0, endX - x0);
                else // range wraps past the cycle end
                {
                    fillBand(x0, xR - x0);
                    fillBand(xL, endX - xR);
                }
            }

            strokeLine(x0, morphYC - hMark, x0, morphYC + hMark, phaseCol.withAlpha(0.7f), 1.2f);
        }

        // Vertical Morph scrubber in the left gutter, mapped to the frame-stack
        // depth: morph 0 = back (top) frame, morph 1 = front (bottom) frame, so the
        // handle always sits at the same height as the highlighted frame's baseline.
        {
            const float sTop = frontY - perspH;
            const float sBot = frontY;
            const float sh = std::max(6.0f, sBot - sTop);
            const float sx = r.x + morphGutterW * 0.5f;
            // Hit area = the whole left gutter (full height), so the handle is easy
            // to grab even at the extremes where it overhangs the groove. Value maps
            // to the groove sub-range [sTop, sBot] (stored separately), clamped.
            metaMorphSliderRect_ = { r.x + 1.0f, r.y + 2.0f, morphGutterW + 3.0f, r.h - 4.0f };
            morphGrooveTop_ = sTop;
            morphGrooveH_ = sh;

            // Recessed groove: dark core with a light left highlight so it reads as
            // milled metal against the panel.
            strokeLine(sx, sTop, sx, sBot, rgba(0x05080aff), 5.0f);
            strokeLine(sx, sTop, sx, sBot, rgba(0x33434dff), 1.0f);

            const float hy = sTop + clampf(liveMorph, 0.0f, 1.0f) * sh;
            // Handle: a chunky rounded cap that clearly rides the groove.
            beginPath();
            roundedRect(r.x + 3.0f, hy - 4.0f, morphGutterW - 6.0f, 8.0f, 2.0f);
            fillColor(DesignTokens::accentGreen());
            fill();

            uiFontSize(8.5f);
            textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
            fillColor(DesignTokens::textSecondary());
            char fbuf[16];
            std::snprintf(fbuf, sizeof(fbuf), "%d/%d", morphFrame + 1, slot.frameCount);
            text(r.x + 3.0f, sTop - 3.0f, fbuf, nullptr);
        }
        resetScissor();
    }

void KapibaraUI::drawMetaWaveformEditor(const Rect &r, const synth::WavetableFrame &frame)
{
        drawPlotBackground(r, 8, 4);
        strokeLine(r.x + 6.0f, r.y + r.h * 0.5f, r.x + r.w - 6.0f, r.y + r.h * 0.5f,
                   DesignTokens::divider(), 1.0f);

        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        beginPath();
        for(int i = 0; i < 160; ++i)
        {
            const float t = float(i) / 159.0f;
            float v = 0.0f;
            if(frame.useImportedWaveform && frame.waveform)
            {
                const float pos = t * float(synth::kWavetableSize - 1);
                const int a = clampi(int(pos), 0, synth::kWavetableSize - 1);
                const int b = std::min(a + 1, synth::kWavetableSize - 1);
                const float frac = pos - float(a);
                v = (*frame.waveform)[(size_t)a] + ((*frame.waveform)[(size_t)b] - (*frame.waveform)[(size_t)a]) * frac;
            }
            else
            {
                for(const auto &h : frame.harmonics)
                {
                    if(h.amp <= 0.0f || h.ratio <= 0.0f)
                        continue;
                    v += h.amp * std::sin(2.0f * kPi * t * h.ratio + h.phase);
                }
                v = clampf(v, -1.0f, 1.0f);
            }
            const float px = r.x + 6.0f + t * (r.w - 12.0f);
            const float py = r.y + r.h * 0.5f - v * (r.h * 0.40f);
            if(i == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(DesignTokens::accentCyan());
        strokeWidth(2.0f);
        stroke();
        resetScissor();

        const float barW = (r.w - 12.0f) / float(synth::kEditableWavetableHarmonics);
        for(int h = 0; h < synth::kEditableWavetableHarmonics; ++h)
        {
            const auto &harmonic = frame.harmonics[(size_t)h];
            const float amp = clampf(harmonic.amp, 0.0f, 1.0f);
            const float x = r.x + 6.0f + float(h) * barW + 1.0f;
            const float y = r.y + r.h - 5.0f - amp * (r.h - 14.0f);
            beginPath();
            roundedRect(x, y, std::max(1.0f, barW - 2.0f), r.y + r.h - 5.0f - y, 1.0f);
            fillColor(h == selectedMetaHarmonic_ ? DesignTokens::accentGreen() : DesignTokens::accentBlue().withAlpha(0.62f));
            fill();
        }
    }

END_NAMESPACE_DISTRHO
