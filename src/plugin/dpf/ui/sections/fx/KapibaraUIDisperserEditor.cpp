#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
constexpr double kGraphSr = 48000.0;
constexpr float kQLo = synth::kDisperserMinQMul;
constexpr float kQHi = synth::kDisperserMaxQMul;

// Lane <-> value mappings, still used by the slot rows' pitch and Q cells.
float octFromY(const Rect &lane, float y)
{
    const float t = clampf((y - lane.y) / std::max(1.0f, lane.h), 0.0f, 1.0f);
    return (1.0f - 2.0f * t) * synth::kDisperserMaxOct;
}
float qFromY(const Rect &lane, float y)
{
    const float t = 1.0f - clampf((y - lane.y) / std::max(1.0f, lane.h), 0.0f, 1.0f);
    return std::exp(std::log(kQLo) + t * (std::log(kQHi) - std::log(kQLo)));
}
// Which bar is under x. Clamped, not rejected: a drag that runs off the end
// keeps painting the last stage instead of dropping the gesture.
int stageFromX(const Rect &lane, int stages, float x)
{
    if(stages <= 0)
        return -1;
    const float t = (x - lane.x) / std::max(1.0f, lane.w);
    return clampi(int(t * float(stages)), 0, stages - 1);
}

const char *hzLabel(char *buf, size_t n, float hz)
{
    if(hz >= 1000.0f) std::snprintf(buf, n, "%.2fk", hz / 1000.0f);
    else              std::snprintf(buf, n, "%.0f", hz);
    return buf;
}

// Column split of a slot row, as fractions of the list width. Draw and hit-test
// both read these, so a click always lands on the control it is under.
constexpr float kSlotColNum   = 0.00f;
constexpr float kSlotColType  = 0.07f;
constexpr float kSlotColDist  = 0.29f;
constexpr float kSlotColDrive = 0.49f;
constexpr float kSlotColFb    = 0.62f;
constexpr float kSlotColOct   = 0.75f;   // pitch offset, same array the lane draws
constexpr float kSlotColQ     = 0.88f;
constexpr float kSlotRowH = 22.0f;      // readable, and big enough to aim at
constexpr float kSlotBarW = 8.0f;       // scrollbar gutter

// Rows sit on a fixed pitch and the list scrolls, so `scroll` (px) is part of
// the geometry. Both the draw and the hit-test go through here.
Rect slotCell(const Rect &list, float scroll, int row, float f0, float f1)
{
    const float w = std::max(10.0f, list.w - kSlotBarW);
    return Rect { list.x + w * f0, list.y + float(row) * kSlotRowH - scroll,
                  w * (f1 - f0) - 2.0f, kSlotRowH - 2.0f };
}
int slotRowFromY(const Rect &list, float scroll, int rows, float y)
{
    if(rows <= 0) return -1;
    const int k = int((y - list.y + scroll) / kSlotRowH);
    return (k < 0 || k >= rows) ? -1 : k;
}
} // namespace

// Group delay against log frequency, with a tick for every section: this is the
// "where is each stage" view.
void KapibaraUI::drawDisperserStageGraph(const Rect &g, const synth::FilterSlotParams &fs, int selStage)
{
        drawPanel(g, rgba(0x0a0f13ff), DesignTokens::divider());
        scissor(g.x + 1.0f, g.y + 1.0f, g.w - 2.0f, g.h - 2.0f);
        const float fLo = 20.0f, fHi = 20000.0f;
        const float logLo = std::log10(fLo), logHi = std::log10(fHi);
        // One point per pixel, capped: the curve is redrawn every repaint, so it
        // must not allocate.
        constexpr int kMaxSteps = 1024;
        const int steps = clampi(int(g.w), 32, kMaxSteps);

        // Decade grid.
        for(int dec = 2; dec <= 4; ++dec)
        {
            const float px = g.x + (float(dec) - logLo) / (logHi - logLo) * g.w;
            strokeLine(px, g.y + 1.0f, px, g.y + g.h - 1.0f, DesignTokens::divider(), 1.0f);
        }

        // Auto-range: dispersion spans microseconds to tens of ms depending on
        // the stage count, so a fixed axis would be blank or clipped most of the
        // time.
        float peak = 0.0f;
        float gd[kMaxSteps + 1];
        for(int i = 0; i <= steps; ++i)
        {
            const float t = float(i) / float(steps);
            const float f = std::pow(10.0f, logLo + t * (logHi - logLo));
            gd[(size_t)i] = synth::disperserGroupDelayMs(fs, f, kGraphSr);
            peak = std::max(peak, gd[(size_t)i]);
        }
        const float span = std::max(0.5f, peak * 1.15f);

        const int sections = synth::disperserSections(fs);
        for(int k = 0; k < sections; ++k)
        {
            const float f = synth::disperserStageHz(fs, sections, k, kGraphSr);
            const float px = g.x + (std::log10(f) - logLo) / (logHi - logLo) * g.w;
            const bool sel = k == selStage;
            strokeLine(px, g.y + 2.0f, px, g.y + g.h - 2.0f,
                       sel ? DesignTokens::accentGreen() : rgba(0x3f6b74aa), sel ? 1.6f : 1.0f);
        }

        beginPath();
        for(int i = 0; i <= steps; ++i)
        {
            const float px = g.x + float(i) / float(steps) * g.w;
            const float py = g.y + g.h - clampf(gd[(size_t)i] / span, 0.0f, 1.0f) * (g.h - 3.0f) - 1.5f;
            if(i == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(DesignTokens::accentCyan());
        strokeWidth(1.4f);
        stroke();

        char buf[48];
        fontSize(8.0f);
        fillColor(DesignTokens::textSecondary());
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        std::snprintf(buf, sizeof buf, "%.2f ms", span);
        text(g.x + 4.0f, g.y + 3.0f, buf, nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
        text(g.x + g.w - 4.0f, g.y + g.h - 2.0f, "20 Hz - 20 kHz  group delay", nullptr);
        resetScissor();
}

void KapibaraUI::clearDisperserRects()
{
        disperserFreqLaneRect_ = {};
        disperserQLaneRect_ = {};
        disperserShapeRects_.fill({});
        disperserVoiceStripRect_ = {};
        disperserSlotListRect_ = {};
        disperserPageTabs_.fill({});
        disperserEditorRect_ = {};
        disperserLaneStages_ = 0;
}

// SLOTS page: one row per live slot, no graph. This is where a slot is DEFINED —
// type, distortion, drive, feedback — while SHAPE decides where the slots sit.
void KapibaraUI::drawDisperserSlotList(const Rect &rWhole, synth::FilterSlotParams &fs, int sections)
{
        // Header, so the two right-hand columns read as what they are: offsets
        // FROM the filter's own cutoff and Q, not absolute values. Move the master
        // Cutoff and every slot moves with it, keeping its interval.
        const float headH = 11.0f;
        const Rect r { rWhole.x, rWhole.y + headH, rWhole.w, std::max(1.0f, rWhole.h - headH) };
        {
            useUiFont();
            uiFontSize(7.5f);
            fillColor(DesignTokens::textSecondary());
            const auto head = [&](float f0, float f1, const char *s, int align) {
                const float x0 = rWhole.x + rWhole.w * f0, x1 = rWhole.x + rWhole.w * f1;
                textAlign(align | ALIGN_BOTTOM);
                text(align == ALIGN_CENTER ? (x0 + x1) * 0.5f : x0 + 2.0f, rWhole.y + headH - 1.0f, s, nullptr);
            };
            head(kSlotColType,  kSlotColDist,  "TYPE",        ALIGN_CENTER);
            head(kSlotColDist,  kSlotColDrive, "DIST",        ALIGN_CENTER);
            head(kSlotColDrive, kSlotColFb,    "DRIVE",       ALIGN_CENTER);
            head(kSlotColFb,    kSlotColOct,   "FEEDBACK",    ALIGN_CENTER);
            head(kSlotColOct,   kSlotColQ,     "PITCH  oct from cutoff", ALIGN_CENTER);
            head(kSlotColQ,     1.0f,          "Q  x master", ALIGN_CENTER);
        }
        disperserSlotListRect_ = r;
        drawPanel(r, rgba(0x0a0f13ff), DesignTokens::divider());
        if(sections <= 0)
            return;
        disperserSlotRowH_ = kSlotRowH;
        const float contentH = float(sections) * kSlotRowH;
        disperserSlotMaxScroll_ = std::max(0.0f, contentH - r.h);
        disperserSlotScroll_ = clampf(disperserSlotScroll_, 0.0f, disperserSlotMaxScroll_);
        const float scroll = disperserSlotScroll_;
        const float listW = std::max(10.0f, r.w - kSlotBarW);

        const auto bar = [&](const Rect &br, float norm, const char *label, Color col) {
            drawPanel(br, rgba(0x070b0eff), DesignTokens::border());
            if(norm > 0.0f)
            {
                beginPath();
                rect(br.x + 1.0f, br.y + 1.0f, std::max(0.0f, (br.w - 2.0f) * norm), br.h - 2.0f);
                fillColor(col);
                fill();
            }
            useUiFont();
            uiFontSize(8.0f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(DesignTokens::textPrimary());
            text(br.x + br.w * 0.5f, br.y + br.h * 0.5f, label, nullptr);
        };
        // Thumbnails: the row says what a slot IS in words, these say it in shape.
        // Small enough to sit inside the button next to its label.
        const auto glyphFilter = [&](const Rect &g, const synth::FilterSlotParams &slot) {
            const synth::BiquadCoeffs c = synth::designInsertBiquad(slot, kGraphSr);
            constexpr int N = 14;
            beginPath();
            for(int i = 0; i <= N; ++i)
            {
                const float t = float(i) / float(N);
                const float f = std::pow(10.0f, 1.3f + t * 2.7f);      // 20 Hz .. 20 kHz
                const float db = 20.0f * std::log10(std::max(1e-3f, biquadMagnitude(c, 6.2831853f * f / float(kGraphSr))));
                const float yy = g.y + g.h - clampf((db + 24.0f) / 36.0f, 0.0f, 1.0f) * g.h;
                if(i == 0) moveTo(g.x, yy); else lineTo(g.x + t * g.w, yy);
            }
            strokeColor(DesignTokens::accentCyan());
            strokeWidth(1.0f);
            stroke();
        };
        const auto glyphDist = [&](const Rect &g, synth::InsertDistAlgo algo, float drive) {
            constexpr int N = 14;
            beginPath();
            for(int i = 0; i <= N; ++i)
            {
                const float t = float(i) / float(N);
                const float xin = t * 2.0f - 1.0f;
                const float yo = clampf(synth::distShape(algo, xin, drive, 0.0f), -1.0f, 1.0f);
                const float yy = g.y + g.h * 0.5f - yo * (g.h * 0.5f - 1.0f);
                if(i == 0) moveTo(g.x, yy); else lineTo(g.x + t * g.w, yy);
            }
            strokeColor(rgba(0xffc857ff));
            strokeWidth(1.0f);
            stroke();
        };

        scissor(r.x + 1.0f, r.y + 1.0f, r.w - 2.0f, r.h - 2.0f);
        char lbl[48];
        const int first = clampi(int(scroll / kSlotRowH), 0, std::max(0, sections - 1));
        const int last  = clampi(first + int(r.h / kSlotRowH) + 1, 0, sections - 1);
        for(int k = first; k <= last; ++k)
        {
            const float rowY = r.y + float(k) * kSlotRowH - scroll;
            if(k == disperserSelStage_)
            {
                beginPath();
                rect(r.x + 1.0f, rowY, listW - 2.0f, kSlotRowH);
                fillColor(rgba(0x8bea6222));
                fill();
            }
            const Rect numR = slotCell(r, scroll, k, kSlotColNum, kSlotColType);
            useUiFont();
            uiFontSize(8.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(k == disperserSelStage_ ? DesignTokens::accentGreen() : DesignTokens::textSecondary());
            std::snprintf(lbl, sizeof lbl, "%d", k + 1);
            text(numR.x + 3.0f, numR.y + numR.h * 0.5f, lbl, nullptr);

            const uint8_t av = fs.apAlgo[(size_t)k];
            const Rect typeR = slotCell(r, scroll, k, kSlotColType, kSlotColDist);
            if(av == 0) std::snprintf(lbl, sizeof lbl, "(%s)", kFilterAlgoNames[int(fs.algo)]);
            else        std::snprintf(lbl, sizeof lbl, "%s", kFilterAlgoNames[av - 1]);
            drawButton(typeR, lbl, av != 0);
            {
                // Drawn from the slot AS PLAYED, so the shape reflects this row's
                // own pitch offset and Q, not the master setting.
                synth::FilterSlotParams show = synth::disperserSectionSlot(fs, sections, k, kGraphSr);
                glyphFilter({ typeR.x + 3.0f, typeR.y + 3.0f, 20.0f, typeR.h - 6.0f }, show);
            }

            const uint8_t dv = fs.apDist[(size_t)k];
            const Rect distR = slotCell(r, scroll, k, kSlotColDist, kSlotColDrive);
            std::snprintf(lbl, sizeof lbl, "%s", dv == 0 ? "clean" : kDistAlgoNames[dv - 1]);
            drawButton(distR, lbl, dv != 0);
            if(dv != 0)
                glyphDist({ distR.x + 3.0f, distR.y + 3.0f, 18.0f, distR.h - 6.0f },
                          synth::disperserSlotDist(fs, k), synth::disperserSlotDrive(fs, k));

            const float dn = float(fs.apDrive[(size_t)k]) / 255.0f;
            std::snprintf(lbl, sizeof lbl, "%.1f", double(synth::disperserSlotDrive(fs, k)));
            bar(slotCell(r, scroll, k, kSlotColDrive, kSlotColFb), dn, lbl,
                dv != 0 ? rgba(0xffc857ff) : rgba(0x55636dff));

            const float fn = float(fs.apFb[(size_t)k]) / 255.0f;
            std::snprintf(lbl, sizeof lbl, "%.2f", double(synth::disperserSlotFeedback(fs, k)));
            bar(slotCell(r, scroll, k, kSlotColFb, kSlotColOct), fn, lbl,
                fn > 0.0f ? rgba(0xff7a4dff) : rgba(0x55636dff));

            float oct = 0.0f, qm = 1.0f;
            synth::disperserStage(fs, sections, k, oct, qm);
            const Rect octR = slotCell(r, scroll, k, kSlotColOct, kSlotColQ);
            drawPanel(octR, rgba(0x070b0eff), DesignTokens::border());
            {
                const float mid = octR.x + octR.w * 0.5f;
                const float w = octR.w * 0.5f * clampf(oct / synth::kDisperserMaxOct, -1.0f, 1.0f);
                beginPath();
                rect(std::min(mid, mid + w), octR.y + 1.0f, std::max(1.0f, std::abs(w)), octR.h - 2.0f);
                fillColor(DesignTokens::accentCyan());
                fill();
            }
            {
                char hz[24];
                useUiFont(); uiFontSize(8.0f);
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(DesignTokens::textPrimary());
                std::snprintf(lbl, sizeof lbl, "%+.2f  %s", double(oct),
                              hzLabel(hz, sizeof hz, synth::disperserStageHz(fs, sections, k, kGraphSr)));
                text(octR.x + octR.w * 0.5f, octR.y + octR.h * 0.5f, lbl, nullptr);
            }
            const float qn = clampf((std::log(clampf(qm, kQLo, kQHi)) - std::log(kQLo))
                                        / (std::log(kQHi) - std::log(kQLo)), 0.0f, 1.0f);
            std::snprintf(lbl, sizeof lbl, "x%.2f", double(qm));
            bar(slotCell(r, scroll, k, kSlotColQ, 1.0f), qn, lbl, DesignTokens::accentBlue());
        }
        resetScissor();

        if(disperserSlotMaxScroll_ > 0.0f)
        {
            const Rect gutter { r.x + listW + 1.0f, r.y + 1.0f, kSlotBarW - 2.0f, r.h - 2.0f };
            drawPanel(gutter, rgba(0x070b0eff), DesignTokens::divider());
            const float frac = r.h / contentH;
            const float thumbH = std::max(12.0f, gutter.h * frac);
            const float ty = gutter.y + (gutter.h - thumbH) * (scroll / disperserSlotMaxScroll_);
            beginPath();
            rect(gutter.x + 1.0f, ty, gutter.w - 2.0f, thumbH);
            fillColor(rgba(0x3f6b74ff));
            fill();
        }

}

void KapibaraUI::drawDisperserEditor(const Rect &rFull, InsertEffect &e, int trackId, int mergeIdx, int insertIdx)
{
        auto &fs = e.filter;
        const int sections = synth::disperserSections(fs);
        disperserEditorRect_ = rFull;
        disperserTarget_ = FxKnobHit { {}, trackId, mergeIdx, insertIdx, 0 };
        disperserLaneStages_ = sections;
        if(disperserSelStage_ >= sections)
            disperserSelStage_ = -1;

        // No tabs here any more: CONTROL and SLOTS are top-level, drawn by the
        // focus header. This function just renders whichever one is selected.
        disperserPageTabs_.fill({});
        const Rect body = rFull;

        if(disperserPage_ == 1)
        {
            drawDisperserSlotList(body, fs, sections);
            disperserFreqLaneRect_ = {};
            disperserQLaneRect_ = {};
            disperserVoiceStripRect_ = {};
            disperserShapeRects_.fill({});
            return;
        }
        disperserSlotListRect_ = {};
        disperserFreqLaneRect_ = {};
        disperserQLaneRect_ = {};
        disperserVoiceStripRect_ = {};

        // CONTROL owns every knob: the filter's own cutoff/Q/drive/mix, the slot
        // count, and the three distribution generators.
        const float kw = (rFull.w - 3.0f) * 0.25f;
        const float knobH = clampf(body.h * 0.22f, 30.0f, 44.0f);
        const int knobRows = body.h > 190.0f ? 2 : 1;
        for(int i = 0; i < knobRows * 4; ++i)
        {
            const Rect kr { body.x + float(i % 4) * (kw + 1.0f),
                            body.y + float(i / 4) * (knobH + 2.0f), kw, knobH };
            drawKnob(kr, fxKnobName(InsertFilter, i), fxKnobNorm(e, i), fxKnobDisp(e, i));
            fxKnobHits_.push_back(FxKnobHit { kr, trackId, mergeIdx, insertIdx, i });
        }
        float usedH = float(knobRows) * (knobH + 2.0f);
        if(knobRows == 1)
        {
            const float sh = 14.0f;
            for(int i = 4; i < 8; ++i)
            {
                const Rect br { body.x + float(i - 4) * (kw + 1.0f), body.y + usedH, kw, sh };
                char lb[24];
                std::snprintf(lb, sizeof lb, "%s %.2f", fxKnobName(InsertFilter, i), double(fxKnobDisp(e, i)));
                drawPanel(br, rgba(0x070b0eff), DesignTokens::border());
                beginPath();
                rect(br.x + 1.0f, br.y + 1.0f, std::max(0.0f, (br.w - 2.0f) * fxKnobNorm(e, i)), br.h - 2.0f);
                fillColor(rgba(0x3f6b7488));
                fill();
                useUiFont(); uiFontSize(8.0f);
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(DesignTokens::textPrimary());
                text(br.x + br.w * 0.5f, br.y + br.h * 0.5f, lb, nullptr);
                fxKnobHits_.push_back(FxKnobHit { br, trackId, mergeIdx, insertIdx, i });
            }
            usedH += sh + 2.0f;
        }

        // The two stage lanes are gone: per-slot pitch and Q are edited on the
        // SLOTS row that owns them. What belongs here is what the whole filter
        // does — magnitude on the left, phase on the right.
        const float btnH = 15.0f, btnGap = 4.0f;
        const Rect graphs { body.x, body.y + usedH + 4.0f, body.w,
                            std::max(30.0f, body.h - usedH - 8.0f - btnH - btnGap) };
        drawFilterResponsePair(graphs, fs);

        static const char *const kShapeNames[4] = { "FLAT", "MACRO", "RANDOM", "SMOOTH" };
        const float sw = (rFull.w - 9.0f) * 0.25f;
        const float shapeY = graphs.y + graphs.h + btnGap;
        for(int i = 0; i < 4; ++i)
        {
            const Rect br { rFull.x + float(i) * (sw + 3.0f), shapeY, sw, btnH };
            disperserShapeRects_[(size_t)i] = br;
            drawButton(br, kShapeNames[i], false);
        }
}

// Magnitude and phase of the whole cascade, side by side. Both are computed the
// way the audio path builds it — per section, summed as vectors when the slots
// run in parallel and multiplied when they chain — so the picture is the sound.
void KapibaraUI::drawFilterResponsePair(const Rect &r, const synth::FilterSlotParams &fs)
{
        const float gw = (r.w - 4.0f) * 0.5f;
        const Rect gm { r.x, r.y, gw, r.h };
        const Rect gp { r.x + gw + 4.0f, r.y, gw, r.h };
        const int sections = synth::disperserSections(fs);
        synth::BiquadCoeffs sect[synth::kMaxDisperserStages];
        float gain[synth::kMaxDisperserStages];
        for(int k = 0; k < sections; ++k)
        {
            sect[k] = synth::designInsertBiquad(synth::disperserSectionSlot(fs, sections, k, kGraphSr), kGraphSr);
            sect[k].stages = 1;   // the cascade counts its own depth
            gain[k] = synth::disperserSectionGain(fs, k);
        }
        const float pnorm = synth::disperserParallelNorm(sections);
        const bool par = fs.apParallel != 0;

        // Tick opacity carries each slot's Q, normalised against THIS filter's own
        // spread rather than the absolute 0.05..10 range. PINCH only moves the
        // sections over a couple of octaves of Q; on an absolute scale that was a
        // few percent of alpha and invisible. Relative, the same knob drives the
        // full contrast range, which is the point of looking at it.
        //
        // Contrast is itself scaled by how wide the spread is, so this does not
        // turn a hair of PINCH into a full gradient: no spread draws every tick
        // at the same mid opacity, and the picture only separates as the knob
        // actually separates the sections. Uses the REALISED Q (post guard clamp).
        uint32_t tickAlpha[synth::kMaxDisperserStages];
        {
            float qv[synth::kMaxDisperserStages];
            float qLo = 1e9f, qHi = -1e9f;
            for(int k = 0; k < sections; ++k)
            {
                qv[k] = std::max(0.05f, synth::disperserSectionSlot(fs, sections, k, kGraphSr).resonance);
                qLo = std::min(qLo, qv[k]);
                qHi = std::max(qHi, qv[k]);
            }
            const float lgLo = std::log(qLo), lgHi = std::log(qHi);
            const float spanLg = lgHi - lgLo;
            // Full contrast once the sharpest section is 4x the gentlest.
            const float contrast = clampf(spanLg / std::log(4.0f), 0.0f, 1.0f);
            constexpr float kLo = 56.0f, kHi = 255.0f;      // 0x38 .. 0xff
            constexpr float kMid = (kLo + kHi) * 0.5f;
            for(int k = 0; k < sections; ++k)
            {
                const float t = spanLg > 1.0e-4f ? (std::log(qv[k]) - lgLo) / spanLg : 0.5f;
                tickAlpha[k] = uint32_t(clampf(kMid + contrast * (t - 0.5f) * (kHi - kLo),
                                               kLo, kHi));
            }
        }

        constexpr int kSteps = 256;
        float dbv[kSteps + 1], phv[kSteps + 1];
        float phLo = 1e9f, phHi = -1e9f;
        for(int i = 0; i <= kSteps; ++i)
        {
            const float t = float(i) / float(kSteps);
            const float f = std::pow(10.0f, 1.301f + t * 3.0f);          // 20 Hz .. 20 kHz
            const float w = 6.2831853f * f / float(kGraphSr);
            float mag = 1.0f, ph = 0.0f;
            if(par)
            {
                float re = 0.0f, im = 0.0f;
                for(int k = 0; k < sections; ++k)
                {
                    float r1, i1;
                    biquadResponse(sect[k], w, r1, i1);
                    re += r1 * gain[k];
                    im += i1 * gain[k];
                }
                mag = std::sqrt(re * re + im * im) * pnorm;
                ph = std::atan2(im, re);
            }
            else
            {
                for(int k = 0; k < sections; ++k)
                {
                    mag *= biquadMagnitude(sect[k], w);
                    ph  += biquadPhase(sect[k], w);   // chained phase accumulates
                }
            }
            dbv[i] = 20.0f * std::log10(std::max(1e-4f, mag));
            phv[i] = ph * 57.29578f;                  // degrees
            phLo = std::min(phLo, phv[i]);
            phHi = std::max(phHi, phv[i]);
        }

        const auto frame = [&](const Rect &g, const char *label) {
            drawPanel(g, rgba(0x0a0f13ff), DesignTokens::divider());
            for(int dec = 2; dec <= 4; ++dec)
            {
                const float px = g.x + (float(dec) - 1.301f) / 3.0f * g.w;
                strokeLine(px, g.y + 1.0f, px, g.y + g.h - 1.0f, DesignTokens::divider(), 1.0f);
            }
            // Where every slot actually sits. Both curves carry them: reading the
            // response without knowing which stage put a feature there is half a
            // picture, and the ticks are the only thing tying the two pages
            // together now that the stage lanes are gone.
            for(int k = 0; k < sections; ++k)
            {
                const float f = synth::disperserStageHz(fs, sections, k, kGraphSr);
                const float px = g.x + (std::log10(std::max(20.0f, f)) - 1.301f) / 3.0f * g.w;
                if(px < g.x || px > g.x + g.w)
                    continue;
                const bool sel = k == disperserSelStage_;
                // The selected tick keeps a floor so it stays findable even when
                // its Q is low — it is the selection marker as well as a reading.
                const uint32_t a = sel ? std::max(tickAlpha[k], 0xa0u) : tickAlpha[k];
                strokeLine(px, g.y + 2.0f, px, g.y + g.h - 2.0f,
                           rgba((sel ? 0x8bea6200u : 0x3f6b7400u) | a),
                           sel ? 1.6f : 1.0f);
            }
            fontSize(8.0f);
            fillColor(DesignTokens::textSecondary());
            textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
            text(g.x + g.w - 4.0f, g.y + g.h - 2.0f, label, nullptr);
        };

        frame(gm, "MAGNITUDE  20 Hz - 20 kHz");
        strokeLine(gm.x + 1.0f, gm.y + gm.h * 0.5f, gm.x + gm.w - 1.0f, gm.y + gm.h * 0.5f,
                   rgba(0x35505aff), 1.0f);
        scissor(gm.x + 1.0f, gm.y + 1.0f, gm.w - 2.0f, gm.h - 2.0f);
        beginPath();
        for(int i = 0; i <= kSteps; ++i)
        {
            const float px = gm.x + float(i) / float(kSteps) * gm.w;
            const float py = gm.y + gm.h - clampf((dbv[i] + 24.0f) / 48.0f, 0.0f, 1.0f) * gm.h;
            if(i == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(DesignTokens::accentCyan());
        strokeWidth(1.4f);
        stroke();
        resetScissor();

        // Auto-ranged: a 32-stage allpass winds through thousands of degrees, and
        // a fixed +/-180 axis would be a solid block of wrapped lines.
        frame(gp, "PHASE  degrees");
        const float span = std::max(30.0f, phHi - phLo);
        scissor(gp.x + 1.0f, gp.y + 1.0f, gp.w - 2.0f, gp.h - 2.0f);
        beginPath();
        for(int i = 0; i <= kSteps; ++i)
        {
            const float px = gp.x + float(i) / float(kSteps) * gp.w;
            const float py = gp.y + gp.h - clampf((phv[i] - phLo) / span, 0.0f, 1.0f) * (gp.h - 3.0f) - 1.5f;
            if(i == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(rgba(0xffc857ff));
        strokeWidth(1.4f);
        stroke();
        resetScissor();
        char buf[40];
        fontSize(8.0f);
        fillColor(DesignTokens::textSecondary());
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        std::snprintf(buf, sizeof buf, "%.0f deg", double(phHi));
        text(gp.x + 4.0f, gp.y + 3.0f, buf, nullptr);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        std::snprintf(buf, sizeof buf, "%.0f", double(phLo));
        text(gp.x + 4.0f, gp.y + gp.h - 2.0f, buf, nullptr);
}

bool KapibaraUI::handleDisperserEditorPress(float x, float y)
{
        if(disperserLaneStages_ <= 0)
            return false;
        auto *chain = insertChainFor(disperserTarget_.trackId, disperserTarget_.mergeIdx);
        if(chain == nullptr || disperserTarget_.insertIdx < 0
           || disperserTarget_.insertIdx >= int(chain->size()))
            return false;
        auto &fs = (*chain)[(size_t)disperserTarget_.insertIdx].filter;
        const int sections = disperserLaneStages_;

        for(int i = 0; i < 4; ++i)
        {
            if(disperserShapeRects_[(size_t)i].w <= 0.0f || !disperserShapeRects_[(size_t)i].contains(x, y))
                continue;
            if(i == 1)
            {
                // Back to the macro shape, then frozen again so the lanes stay
                // editable — MACRO is a generator, not a mode.
                fs.apCustom = 0;
                synth::disperserMaterialize(fs);
            }
            else
            {
                synth::disperserMaterialize(fs);
                if(i == 0)
                {
                    fs.apOct.fill(0.0f);
                    fs.apQMul.fill(1.0f);
                }
                else if(i == 2)
                {
                    // UI-thread only, so a plain LCG is fine; it just has to
                    // differ from press to press.
                    static uint32_t seed = 0x9e3779b9u;
                    for(int k = 0; k < sections; ++k)
                    {
                        seed = seed * 1664525u + 1013904223u;
                        const float u = float((seed >> 8) & 0xffffu) / 65535.0f;
                        seed = seed * 1664525u + 1013904223u;
                        const float v = float((seed >> 8) & 0xffffu) / 65535.0f;
                        fs.apOct[(size_t)k] = (u * 2.0f - 1.0f) * 2.0f;
                        fs.apQMul[(size_t)k] = 0.4f + v * 2.6f;
                    }
                }
                else
                {
                    std::array<float, synth::kMaxDisperserStages> so {}, sq {};
                    for(int k = 0; k < sections; ++k)
                    {
                        const int a = std::max(0, k - 1), b = std::min(sections - 1, k + 1);
                        so[(size_t)k] = (fs.apOct[(size_t)a] + fs.apOct[(size_t)k] + fs.apOct[(size_t)b]) / 3.0f;
                        sq[(size_t)k] = (fs.apQMul[(size_t)a] + fs.apQMul[(size_t)k] + fs.apQMul[(size_t)b]) / 3.0f;
                    }
                    for(int k = 0; k < sections; ++k)
                    { fs.apOct[(size_t)k] = so[(size_t)k]; fs.apQMul[(size_t)k] = sq[(size_t)k]; }
                }
            }
            commitChainChange(disperserTarget_.trackId, disperserTarget_.mergeIdx);
            return true;
        }

        for(int i = 0; i < 2; ++i)
            if(disperserPageTabs_[(size_t)i].w > 0.0f && disperserPageTabs_[(size_t)i].contains(x, y))
            {
                disperserPage_ = i;
                repaint();
                return true;
            }

        // ---- SLOTS page: one row per slot, columns are the controls ----------
        if(disperserSlotListRect_.w > 0.0f && disperserSlotListRect_.contains(x, y))
        {
            const Rect &list = disperserSlotListRect_;
            const int k = slotRowFromY(list, disperserSlotScroll_, sections, y);
            if(k < 0)
                return true;
            disperserSelStage_ = k;
            const float f = (x - list.x) / std::max(10.0f, list.w - kSlotBarW);
            if(f >= kSlotColType && f < kSlotColDist)
            {
                // Cycles through "as the filter is set" plus every algo. Opening a
                // menu per row would cost more clicks than it saves.
                const int last = int(synth::InsertFilterAlgo::HP4) + 1;
                fs.apAlgo[(size_t)k] = uint8_t((int(fs.apAlgo[(size_t)k]) + 1) % (last + 1));
            }
            else if(f >= kSlotColDist && f < kSlotColDrive)
            {
                const int last = int(synth::InsertDistAlgo::Tanh) + 1;
                uint8_t &d = fs.apDist[(size_t)k];
                d = uint8_t((int(d) + 1) % (last + 1));
                // Switching a clean slot on with no drive yet would look broken,
                // so give it something audible to start from.
                if(d != 0 && fs.apDrive[(size_t)k] == 0)
                    fs.apDrive[(size_t)k] = 96;
            }
            else if(f >= kSlotColDrive && f < kSlotColFb)
            {
                dragTarget_ = DragTarget::DisperserSlotDrive;
                applyDisperserDrag(x, y);
                return true;
            }
            else if(f >= kSlotColFb && f < kSlotColOct)
            {
                dragTarget_ = DragTarget::DisperserSlotFb;
                applyDisperserDrag(x, y);
                return true;
            }
            else if(f >= kSlotColOct)
            {
                // Same freeze the lanes do: pitch and Q are macro-generated until
                // something is edited by hand, and the edit must start from what
                // was being heard rather than from a flat default.
                synth::disperserMaterialize(fs);
                dragTarget_ = f < kSlotColQ ? DragTarget::DisperserSlotOct
                                            : DragTarget::DisperserSlotQ;
                applyDisperserDrag(x, y);
                return true;
            }
            else
            {
                repaint();   // the row-number column just selects
                return true;
            }
            commitChainChange(disperserTarget_.trackId, disperserTarget_.mergeIdx);
            return true;
        }

        if(disperserVoiceStripRect_.w > 0.0f && disperserVoiceStripRect_.contains(x, y))
        {
            disperserSelStage_ = stageFromX(disperserVoiceStripRect_, sections, x);
            return true;
        }

        const bool inFreq = disperserFreqLaneRect_.w > 0.0f && disperserFreqLaneRect_.contains(x, y);
        const bool inQ    = disperserQLaneRect_.w > 0.0f && disperserQLaneRect_.contains(x, y);
        if(!inFreq && !inQ)
        {
            // Nothing of ours. The caller decides what happens next — the CONTROL
            // page hosts real knobs inside this same rectangle, so swallowing
            // everything here would leave them dead.
            return false;
        }
        // Freeze the macro shape before the first hand edit, so the drag starts
        // from exactly what was being heard rather than from a flat line.
        synth::disperserMaterialize(fs);
        disperserSelStage_ = stageFromX(inFreq ? disperserFreqLaneRect_ : disperserQLaneRect_, sections, x);
        dragTarget_ = inFreq ? DragTarget::DisperserStageFreq : DragTarget::DisperserStageQ;
        applyDisperserDrag(x, y);
        return true;
}

void KapibaraUI::applyDisperserDrag(float x, float y)
{
        if(disperserLaneStages_ <= 0)
            return;
        auto *chain = insertChainFor(disperserTarget_.trackId, disperserTarget_.mergeIdx);
        if(chain == nullptr || disperserTarget_.insertIdx < 0
           || disperserTarget_.insertIdx >= int(chain->size()))
            return;
        auto &fs = (*chain)[(size_t)disperserTarget_.insertIdx].filter;
        // The two slot bars are horizontal 0..1 drags on the SELECTED slot, not
        // per-stage painting like the lanes — there is one value, not 32.
        const bool slotDrag = dragTarget_ == DragTarget::DisperserSlotDrive
                              || dragTarget_ == DragTarget::DisperserSlotFb
                              || dragTarget_ == DragTarget::DisperserSlotOct
                              || dragTarget_ == DragTarget::DisperserSlotQ;
        if(slotDrag)
        {
            const Rect &list = disperserSlotListRect_;
            const int k = disperserSelStage_;
            if(list.w <= 0.0f || k < 0 || k >= disperserLaneStages_)
                return;
            float c0 = kSlotColDrive, c1 = kSlotColFb;
            if(dragTarget_ == DragTarget::DisperserSlotFb)  { c0 = kSlotColFb;  c1 = kSlotColOct; }
            if(dragTarget_ == DragTarget::DisperserSlotOct) { c0 = kSlotColOct; c1 = kSlotColQ; }
            if(dragTarget_ == DragTarget::DisperserSlotQ)   { c0 = kSlotColQ;   c1 = 1.0f; }
            const Rect br = slotCell(list, disperserSlotScroll_, k, c0, c1);
            const float t = clampf((x - br.x) / std::max(1.0f, br.w), 0.0f, 1.0f);
            switch(dragTarget_)
            {
                case DragTarget::DisperserSlotDrive:
                    fs.apDrive[(size_t)k] = uint8_t(std::lround(double(t) * 255.0));
                    break;
                case DragTarget::DisperserSlotFb:
                    fs.apFb[(size_t)k] = uint8_t(std::lround(double(t) * 255.0));
                    break;
                case DragTarget::DisperserSlotOct:
                {
                    // Bipolar across the cell, centre is the cutoff itself.
                    float v = (t * 2.0f - 1.0f) * synth::kDisperserMaxOct;
                    if(std::abs(v) < 0.06f) v = 0.0f;
                    fs.apOct[(size_t)k] = v;
                    break;
                }
                default:
                {
                    float v = std::exp(std::log(kQLo) + t * (std::log(kQHi) - std::log(kQLo)));
                    if(std::abs(v - 1.0f) < 0.06f) v = 1.0f;
                    fs.apQMul[(size_t)k] = v;
                    break;
                }
            }
            commitChainChange(disperserTarget_.trackId, disperserTarget_.mergeIdx);
            return;
        }
        const bool freq = dragTarget_ == DragTarget::DisperserStageFreq;
        const Rect &lane = freq ? disperserFreqLaneRect_ : disperserQLaneRect_;
        if(lane.w <= 0.0f)
            return;
        // The stage follows x, so one sweep draws the whole distribution the way
        // the partial bar chart does.
        const int k = stageFromX(lane, disperserLaneStages_, x);
        if(k < 0)
            return;
        disperserSelStage_ = k;
        if(freq)
        {
            float v = octFromY(lane, y);
            if(std::abs(v) < 0.06f)   // snap to the cutoff: the row is one pixel wide otherwise
                v = 0.0f;
            fs.apOct[(size_t)k] = v;
        }
        else
        {
            float v = qFromY(lane, y);
            if(std::abs(v - 1.0f) < 0.06f)
                v = 1.0f;
            fs.apQMul[(size_t)k] = v;
        }
        commitChainChange(disperserTarget_.trackId, disperserTarget_.mergeIdx);
}

END_NAMESPACE_DISTRHO
