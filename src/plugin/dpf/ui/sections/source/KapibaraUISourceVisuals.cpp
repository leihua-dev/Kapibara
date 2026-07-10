#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Mixer-style modulation list in the strip: one mod slot per row (summary + click).





    // Vertical mixer-style fader. norm 0..1 maps bottom->top.

    // Vertical peak level meter (green->yellow->red bottom to top).

    // dB tick scale drawn just right of a vertical level meter (0/-6/-12/-24 dB).
void KapibaraUI::drawInsertColumn(const Rect &region, const std::vector<InsertEffect> &inserts,
                      int trackId, int mergeIdx)
{
        const float gap = 2.0f, rh = 14.0f;
        const bool dragOnThis = insertDragActive_
                                && insertPendTrackId_ == trackId && insertPendMerge_ == mergeIdx;
        int row = 0;
        const int maxRows = std::max(1, int((region.h + gap) / (rh + gap)));
        for(int i = 0; i < int(inserts.size()) && row < maxRows - 1; ++i, ++row)
        {
            const Rect b { region.x, region.y + float(row) * (rh + gap), region.w, rh };
            const auto &ins = inserts[(size_t)i];
            char lbl[24];
            if(fxHasMode(ins.kind))
                std::snprintf(lbl, sizeof(lbl), "%s %s%s", insertKindShort(ins.kind),
                              ins.kind == InsertFilter ? kFilterAlgoNames[int(ins.filter.algo)]
                                                       : kDistAlgoNames[int(ins.dist.algo)],
                              ins.bypass ? " b" : "");
            else
                std::snprintf(lbl, sizeof(lbl), "%s%s", insertTypeName(ins.kind), ins.bypass ? " b" : "");
            const bool isDragSrc = dragOnThis && insertPendSlot_ == i;
            drawButton(b, lbl, !isDragSrc);
            // Drop-target preview: highlight the chip the dragged insert would land on.
            if(dragOnThis && !isDragSrc && b.contains(insertDragX_, insertDragY_))
            {
                beginPath(); rect(b.x, b.y, b.w, b.h);
                strokeColor(rgba(0x9eff50ffU)); strokeWidth(1.6f); stroke();
            }
            insertHits_.push_back(InsertHit { b, trackId, mergeIdx, i });
        }
        // trailing "+ add" chip
        const Rect addR { region.x, region.y + float(row) * (rh + gap), region.w, rh };
        drawButton(addR, "+ add fx", false);
        insertHits_.push_back(InsertHit { addR, trackId, mergeIdx, -1 });
    }

void KapibaraUI::drawFader(const Rect &r, float norm, const char *label, float value, bool active)
{
        norm = clampf(norm, 0.0f, 1.0f);
        const float cx = r.x + r.w * 0.5f;
        const float top = r.y + 6.0f;
        const float bot = r.y + r.h - 16.0f;
        const float span = std::max(1.0f, bot - top);
        // Groove.
        beginPath();
        roundedRect(cx - 2.5f, top, 5.0f, span, 2.5f);
        fillColor(DesignTokens::controlBackground());
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();
        // Filled portion below the handle.
        const float hy = bot - norm * span;
        beginPath();
        roundedRect(cx - 2.5f, hy, 5.0f, bot - hy, 2.5f);
        fillColor(active ? DesignTokens::accentGreen() : DesignTokens::accentCyan());
        fill();
        // Handle.
        beginPath();
        roundedRect(cx - 9.0f, hy - 5.0f, 18.0f, 10.0f, 3.0f);
        fillPaint(linearGradient(cx, hy - 5.0f, cx, hy + 5.0f,
                                 shade(DesignTokens::panelRaised(), 0.22f),
                                 shade(DesignTokens::panelRaised(), -0.12f)));
        fill();
        strokeColor(active ? DesignTokens::accentGreen() : DesignTokens::border());
        strokeWidth(1.0f);
        stroke();
        // Label + value.
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.2f", value);
        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        text(cx, r.y + r.h - 11.0f, label, nullptr);
    }

void KapibaraUI::drawLevelMeter(const Rect &r, float level)
{
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 2.0f);
        fillColor(rgba(0x0a0f13ff));
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();
        const float v = clampf(level, 0.0f, 1.2f) / 1.2f;
        const float fillH = v * (r.h - 2.0f);
        if(fillH > 0.5f)
        {
            const float fy = r.y + r.h - 1.0f - fillH;
            beginPath();
            roundedRect(r.x + 1.0f, fy, r.w - 2.0f, fillH, 1.5f);
            fillPaint(linearGradient(r.x, r.y + r.h, r.x, r.y,
                                     rgba(0x4fe0a0ff), rgba(0xff5a4fff)));
            fill();
        }
    }

void KapibaraUI::drawMeterScale(const Rect &meter)
{
        struct Tick { const char *label; float amp; };
        static const Tick ticks[] = { { "0", 1.0f }, { "-6", 0.5f }, { "-12", 0.25f }, { "-24", 0.063f } };
        const float tx = meter.x + meter.w + 2.0f;
        useUiFont();
        uiFontSize(6.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for(const auto &t : ticks)
        {
            const float yn = clampf(t.amp / 1.2f, 0.0f, 1.0f);
            const float ty = meter.y + meter.h - 1.0f - yn * (meter.h - 2.0f);
            strokeLine(meter.x + meter.w, ty, tx + 1.0f, ty, DesignTokens::divider(), 1.0f);
            fillColor(DesignTokens::textSecondary());
            text(tx + 3.0f, ty, t.label, nullptr);
        }
    }

void KapibaraUI::drawStripThumbnail(const Rect &r, const synth::SourceTrackParams &track, int trackIndex, bool selected)
{
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, DesignTokens::controlRadius);
        fillColor(DesignTokens::controlBackground());
        fill();
        beginPath();
        roundedRect(r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f, DesignTokens::controlRadius);
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();

        const Rect plot { r.x + 5.0f, r.y + 5.0f, r.w - 10.0f, r.h - 10.0f };
        const Color line = selected ? DesignTokens::accentGreen() : DesignTokens::accentCyan();
        scissor(plot.x, plot.y, plot.w, plot.h);

        if(track.type == synth::SourceTrackType::MetaOscillator && track.metaOsc.frameCount > 0)
        {
            float liveMorph = track.metaOsc.morph;
            if(const auto *p = plugin())
            {
                const float live = p->sourceLiveMorph(trackIndex);
                if(live >= 0.0f)
                    liveMorph = live;
            }
            const int fIdx = clampi(int(liveMorph * float(track.metaOsc.frameCount - 1) + 0.5f), 0,
                                    track.metaOsc.frameCount - 1);
            const auto &frm = track.metaOsc.frames[(size_t)fIdx];
            const float midY = plot.y + plot.h * 0.5f;
            beginPath();
            for(int sp = 0; sp < int(plot.w); ++sp)
            {
                const float t = float(sp) / std::max(1.0f, plot.w);
                const float v = sampleFrameWarped(frm, t, track.metaOsc.warpMode, track.metaOsc.warpAmount);
                const float px = plot.x + float(sp);
                const float py = midY - v * plot.h * 0.42f;
                if(sp == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line);
            strokeWidth(1.25f);
            stroke();
        }
        else if(track.type == synth::SourceTrackType::PartialBank)
        {
            const int count = std::max(1, track.partialBank.partialCount);
            const float barW = std::max(1.0f, plot.w / float(count));
            for(int i = 0; i < count; ++i)
            {
                const auto &p = track.partialBank.partials[(size_t)i];
                const float h = clampf(p.amp, 0.0f, 1.0f) * plot.h;
                beginPath();
                roundedRect(plot.x + float(i) * barW, plot.y + plot.h - h, std::max(1.0f, barW - 1.0f), h, 1.0f);
                fillColor(line.withAlpha(0.78f));
                fill();
            }
        }
        else
        {
            const float midY = plot.y + plot.h * 0.5f;
            beginPath();
            for(int sp = 0; sp < int(plot.w); ++sp)
            {
                const float t = float(sp) / std::max(1.0f, plot.w);
                const float v = track.type == synth::SourceTrackType::BasicOscillator
                                  ? std::sin(t * kPi * 4.0f)
                                  : 0.55f * std::sin(t * kPi * 41.0f) * std::sin(t * kPi * 7.0f);
                const float px = plot.x + float(sp);
                const float py = midY - v * plot.h * 0.38f;
                if(sp == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line.withAlpha(track.type == synth::SourceTrackType::BasicOscillator ? 0.85f : 0.55f));
            strokeWidth(1.25f);
            stroke();
        }
        resetScissor();
    }

END_NAMESPACE_DISTRHO
