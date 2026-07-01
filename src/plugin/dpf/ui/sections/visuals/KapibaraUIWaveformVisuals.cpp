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
        drawPlotBackground(r, 6, 4);
        if(slot.frameCount <= 0) return;

        // 用调制后的实时 morph（来自音频引擎），fallback 为静态 slot.morph
        float liveMorph = slot.morph;
        if(trackIndex >= 0)
        {
            if(const auto *p = plugin())
                liveMorph = p->sourceLiveMorph(trackIndex);
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

        // 透视深度：帧越靠后y越高（向上偏移）
        const float perspH = std::min(r.h * 0.50f, float(N) * 5.0f);
        const float waveH  = r.h - perspH - 8.0f;
        // front frame(idx N-1) 绘制在 r 底部居中
        const float frontY = r.y + r.h - 8.0f - waveH * 0.5f;

        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);

        const auto lerpColor = [](Color a, Color b, float t) {
            a.red   += (b.red   - a.red)   * t;
            a.green += (b.green - a.green) * t;
            a.blue  += (b.blue  - a.blue)  * t;
            return a;
        };

        // Geometry of the live (morph) frame, captured to draw the phase-start
        // marker on top of everything after the frame stack is laid down.
        float morphXL = 0.0f, morphXR = 0.0f, morphYC = 0.0f, morphAmp = 0.0f;
        bool haveMorphGeom = false;

        constexpr int kPts = 80;
        for(int di = 0; di < N; ++di)
        {
            const int frameIdx = frames[(size_t)di];
            const bool isMorph = (frameIdx == morphFrame);
            const float depthT = float(di) / float(std::max(1, N - 1)); // 0=back,1=front
            const float yCenter = frontY - perspH * (1.0f - depthT);

            // x 范围随深度缩小（透视）
            const float xScale = 0.65f + 0.35f * depthT;
            const float xL = r.x + r.w * (1.0f - xScale) * 0.5f + 4.0f;
            const float xR = r.x + r.w * (1.0f + xScale) * 0.5f - 4.0f;
            const float ampScale = (0.45f + 0.55f * depthT) * waveH * 0.45f;
            if(isMorph)
            {
                morphXL = xL; morphXR = xR; morphYC = yCenter; morphAmp = ampScale;
                haveMorphGeom = true;
            }

            // Depth-graded colour: back frames blue, fading to cyan toward the
            // front; the live morph frame stays solid green.
            const Color depthCol = lerpColor(DesignTokens::accentBlue(), DesignTokens::accentCyan(), depthT);
            const Color lineCol = isMorph ? DesignTokens::accentGreen()
                                          : depthCol.withAlpha(0.22f + 0.5f * depthT);
            const float lineW = isMorph ? 2.4f : (0.7f + 0.9f * depthT);

            auto &frame = slot.frames[(size_t)frameIdx];

            // Faint translucent ribbon under every frame builds a layered surface.
            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yCenter - sampleFrameWarped(frame, t, slot.warpMode, slot.warpAmount) * ampScale;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            lineTo(xR, yCenter);
            lineTo(xL, yCenter);
            closePath();
            fillColor((isMorph ? DesignTokens::accentGreen() : depthCol)
                          .withAlpha(isMorph ? 0.10f : 0.03f + 0.05f * depthT));
            fill();

            // Waveform line.
            lineCap(ROUND);
            beginPath();
            for(int s = 0; s <= kPts; ++s)
            {
                const float t = float(s) / float(kPts);
                const float px = xL + t * (xR - xL);
                const float py = yCenter - sampleFrameWarped(frame, t, slot.warpMode, slot.warpAmount) * ampScale;
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(lineCol);
            strokeWidth(lineW);
            stroke();
            lineCap(BUTT);

            // 基线（morph 帧用亮色）
            if(isMorph)
            {
                strokeLine(xL, yCenter, xR, yCenter, DesignTokens::accentGreen().withAlpha(0.18f), 0.5f);
                // 帧号标注
                uiFontSize(9.0f);
                fillColor(DesignTokens::accentGreen());
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                char buf[16];
                std::snprintf(buf, sizeof(buf), "F%d", morphFrame + 1);
                text(r.x + 4.0f, yCenter - ampScale * 0.85f, buf, nullptr);
            }
        }

        // Phase start marker on the live frame: one cycle spans morphXL..morphXR, so
        // the note-on read position sits at fraction phase/2pi. When Rand > 0 the start
        // can land anywhere in [phase, phase + phaseRandom*2pi), shown as a shaded band.
        if(haveMorphGeom)
        {
            const float twoPi = 2.0f * kPi;
            float startFrac = slot.phase / twoPi;   // [-0.5, 0.5]
            startFrac -= std::floor(startFrac);     // wrap to [0, 1)
            const float span = morphXR - morphXL;
            const float x0 = morphXL + startFrac * span;
            const float hMark = morphAmp * 1.15f + 4.0f;
            const Color phaseCol = rgba(0xffc044ffu);

            const float randFrac = clampf(slot.phaseRandom, 0.0f, 1.0f);
            if(randFrac > 1.0e-3f)
            {
                const auto fillBand = [&](float bx, float bw) {
                    if(bw <= 0.0f) return;
                    beginPath();
                    roundedRect(bx, morphYC - hMark, bw, hMark * 2.0f, 2.0f);
                    fillColor(phaseCol.withAlpha(0.15f));
                    fill();
                };
                const float endX = x0 + randFrac * span;
                if(endX <= morphXR + 0.5f)
                    fillBand(x0, endX - x0);
                else // range wraps past the cycle end
                {
                    fillBand(x0, morphXR - x0);
                    fillBand(morphXL, endX - morphXR);
                }
            }

            strokeLine(x0, morphYC - hMark, x0, morphYC + hMark, phaseCol.withAlpha(0.9f), 1.6f);
            beginPath();
            roundedRect(x0 - 2.2f, morphYC - 2.2f, 4.4f, 4.4f, 2.2f);
            fillColor(phaseCol);
            fill();
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
