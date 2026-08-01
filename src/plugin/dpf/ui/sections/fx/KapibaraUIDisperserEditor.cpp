#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
constexpr double kGraphSr = 48000.0;
constexpr float kQLo = synth::kDisperserMinQMul;
constexpr float kQHi = synth::kDisperserMaxQMul;

// Lane <-> value mappings. One place, so a click lands on the bar it drew.
float octFromY(const Rect &lane, float y)
{
    const float t = clampf((y - lane.y) / std::max(1.0f, lane.h), 0.0f, 1.0f);
    return (1.0f - 2.0f * t) * synth::kDisperserMaxOct;
}
float yFromOct(const Rect &lane, float oct)
{
    const float t = clampf(0.5f - oct / (2.0f * synth::kDisperserMaxOct), 0.0f, 1.0f);
    return lane.y + t * lane.h;
}
float qFromY(const Rect &lane, float y)
{
    const float t = 1.0f - clampf((y - lane.y) / std::max(1.0f, lane.h), 0.0f, 1.0f);
    return std::exp(std::log(kQLo) + t * (std::log(kQHi) - std::log(kQLo)));
}
float yFromQ(const Rect &lane, float q)
{
    const float t = clampf((std::log(clampf(q, kQLo, kQHi)) - std::log(kQLo))
                               / (std::log(kQHi) - std::log(kQLo)),
                           0.0f, 1.0f);
    return lane.y + (1.0f - t) * lane.h;
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
        disperserLaneStages_ = 0;
}

void KapibaraUI::drawDisperserEditor(const Rect &r, InsertEffect &e, int trackId, int mergeIdx, int insertIdx)
{
        auto &fs = e.filter;
        const int sections = synth::disperserSections(fs);
        disperserTarget_ = FxKnobHit { {}, trackId, mergeIdx, insertIdx, 0 };
        disperserLaneStages_ = sections;
        if(disperserSelStage_ >= sections)
            disperserSelStage_ = -1;

        // Laid out from the height actually available: graph, then the two
        // editable lanes, then the shape row — the stack has to land exactly on
        // r's bottom edge or the buttons fall outside the panel.
        const float btnH = 15.0f, labelFreq = 11.0f, labelQ = 10.0f, btnGap = 4.0f;
        const float qH = clampf(r.h * 0.14f, 20.0f, 34.0f);
        const float rem = r.h - (btnH + labelFreq + labelQ + btnGap) - qH;
        const float graphH = clampf(rem * 0.45f, 40.0f, std::max(40.0f, rem * 0.6f));
        const Rect graph { r.x, r.y, r.w, graphH };
        const Rect freqLane { r.x, graph.y + graph.h + labelFreq, r.w, std::max(20.0f, rem - graphH) };
        const Rect qLane { r.x, freqLane.y + freqLane.h + labelQ, r.w, qH };
        disperserFreqLaneRect_ = freqLane;
        disperserQLaneRect_ = qLane;

        drawDisperserStageGraph(graph, fs, disperserSelStage_);

        // ---- frequency lane -------------------------------------------------
        fontSize(8.0f);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textSecondary());
        text(freqLane.x + 2.0f, freqLane.y - 3.0f, "STAGE PITCH  (octaves off cutoff, drag to draw)", nullptr);
        char info[64];
        if(disperserSelStage_ >= 0 && disperserSelStage_ < sections)
        {
            float oct = 0.0f, qm = 1.0f;
            synth::disperserStage(fs, sections, disperserSelStage_, oct, qm);
            char hz[24];
            std::snprintf(info, sizeof info, "S%d  %s Hz  %+0.2f oct  Q x%.2f",
                          disperserSelStage_ + 1,
                          hzLabel(hz, sizeof hz, synth::disperserStageHz(fs, sections, disperserSelStage_, kGraphSr)),
                          double(oct), double(qm));
        }
        else
        {
            std::snprintf(info, sizeof info, "%d stages  %s", sections,
                          fs.apCustom != 0 ? "custom" : "macro");
        }
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
        fillColor(fs.apCustom != 0 ? DesignTokens::accentGreen() : DesignTokens::textSecondary());
        text(freqLane.x + freqLane.w - 2.0f, freqLane.y - 3.0f, info, nullptr);

        drawPanel(freqLane, rgba(0x0a0f13ff), DesignTokens::divider());
        scissor(freqLane.x + 1.0f, freqLane.y + 1.0f, freqLane.w - 2.0f, freqLane.h - 2.0f);
        const float zeroY = yFromOct(freqLane, 0.0f);
        strokeLine(freqLane.x + 1.0f, zeroY, freqLane.x + freqLane.w - 1.0f, zeroY,
                   rgba(0x35505aff), 1.0f);
        for(int oct = -2; oct <= 2; oct += 2)
        {
            if(oct == 0) continue;
            const float gy = yFromOct(freqLane, float(oct));
            strokeLine(freqLane.x + 1.0f, gy, freqLane.x + freqLane.w - 1.0f, gy,
                       DesignTokens::divider(), 1.0f);
        }
        const float bw = freqLane.w / float(std::max(1, sections));
        for(int k = 0; k < sections; ++k)
        {
            float oct = 0.0f, qm = 1.0f;
            synth::disperserStage(fs, sections, k, oct, qm);
            const float bx = freqLane.x + float(k) * bw;
            const float by = yFromOct(freqLane, oct);
            const bool sel = k == disperserSelStage_;
            beginPath();
            rect(bx + 1.0f, std::min(by, zeroY), std::max(1.0f, bw - 2.0f), std::abs(by - zeroY));
            fillColor(sel ? rgba(0x8bea6266) : rgba(0x62d7df44));
            fill();
            // The head is the handle: always visible, even at 0 oct where the
            // bar has no height at all.
            beginPath();
            rect(bx + 1.0f, by - 1.5f, std::max(1.0f, bw - 2.0f), 3.0f);
            fillColor(sel ? DesignTokens::accentGreen() : DesignTokens::accentCyan());
            fill();
        }
        resetScissor();

        // ---- Q lane ---------------------------------------------------------
        fontSize(8.0f);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textSecondary());
        text(qLane.x + 2.0f, qLane.y - 2.0f, "STAGE Q  (x resonance)", nullptr);
        drawPanel(qLane, rgba(0x0a0f13ff), DesignTokens::divider());
        scissor(qLane.x + 1.0f, qLane.y + 1.0f, qLane.w - 2.0f, qLane.h - 2.0f);
        const float unityY = yFromQ(qLane, 1.0f);
        strokeLine(qLane.x + 1.0f, unityY, qLane.x + qLane.w - 1.0f, unityY, rgba(0x35505aff), 1.0f);
        for(int k = 0; k < sections; ++k)
        {
            float oct = 0.0f, qm = 1.0f;
            synth::disperserStage(fs, sections, k, oct, qm);
            const float bx = qLane.x + float(k) * bw;
            const float by = yFromQ(qLane, qm);
            const bool sel = k == disperserSelStage_;
            beginPath();
            rect(bx + 1.0f, std::min(by, unityY), std::max(1.0f, bw - 2.0f), std::abs(by - unityY));
            fillColor(sel ? rgba(0x8bea6255) : rgba(0x4aa8e844));
            fill();
            beginPath();
            rect(bx + 1.0f, by - 1.0f, std::max(1.0f, bw - 2.0f), 2.0f);
            fillColor(sel ? DesignTokens::accentGreen() : DesignTokens::accentBlue());
            fill();
        }
        resetScissor();

        // ---- shape buttons --------------------------------------------------
        static const char *const kShapeNames[4] = { "FLAT", "MACRO", "RANDOM", "SMOOTH" };
        const float sw = (r.w - 9.0f) * 0.25f;
        for(int i = 0; i < 4; ++i)
        {
            const Rect br { r.x + float(i) * (sw + 3.0f), qLane.y + qLane.h + btnGap, sw, btnH };
            disperserShapeRects_[(size_t)i] = br;
            drawButton(br, kShapeNames[i], false);
        }
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

        const bool inFreq = disperserFreqLaneRect_.w > 0.0f && disperserFreqLaneRect_.contains(x, y);
        const bool inQ    = disperserQLaneRect_.w > 0.0f && disperserQLaneRect_.contains(x, y);
        if(!inFreq && !inQ)
            return false;
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
