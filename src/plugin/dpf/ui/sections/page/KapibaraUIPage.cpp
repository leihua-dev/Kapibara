#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawCurrentPage()
{
        const Rect page { 8.0f, 70.0f, static_cast<float>(uiW()) - 16.0f,
                          static_cast<float>(uiH()) - 172.0f };
        drawPanel(page, rgba(0x10171bff), rgba(0x293842ff));

        // Grid rects only exist while the MATRIX view draws them; ungated handlers
        // (right-click axis removal etc.) must not see stale ones.
        if(!matrixViewOpen_)
        {
            gridSrcLabelRects_.clear();
            gridDestLabelRects_.clear();
            gridAddSrcRect_ = {};
            gridAddDstRect_ = {};
            matrixGridCells_.clear();
            ruleWeightRect_ = {}; ruleMaskRect_ = {}; ruleMaskAxisRect_ = {};
            gridPickerMode_ = 0;
        }

        // New layout:
        //   Top row:  Source Editor | Per-Voice Chain Editor | FX Rack Editor
        //   Thin strip: draggable MOD sources
        //   Bottom:   toggle  [ Modulation / Matrix ]  or  [ Source Structure ]
        const float gap       = 8.0f;
        const float layoutModStripH = 24.0f;
        const float modStripH = 44.0f;
        const float tabH      = 18.0f;
        const float bottomH   = clampf(page.h * layoutBottomRatio_, 220.0f, page.h - 220.0f);
        const float topH      = page.h - bottomH - layoutModStripH - gap * 4.0f;
        const float topY      = page.y + gap;

        // --- Top row: three focused editors for the current source ---
        const float srcW = clampf(page.w * 0.44f, 300.0f, page.w * 0.52f);
        const float pvW  = clampf(page.w * 0.24f, 190.0f, 300.0f);
        const Rect sourceEditor { page.x + gap, topY, srcW, topH };
        const Rect perVoice     { sourceEditor.x + sourceEditor.w + gap, topY, pvW, topH };
        const Rect fxRack       { perVoice.x + perVoice.w + gap, topY,
                                  page.x + page.w - gap - (perVoice.x + perVoice.w + gap), topH };

        computeSelectedSourceChain(); // before the chain/FX editors read it
        bool drewMultibandEditor = false;
        if(multibandEditorTrackId_ >= 0 && multibandEditorInsertIdx_ >= 0)
        {
            if(auto *chain = trackInsertsFor(uint32_t(multibandEditorTrackId_));
               chain != nullptr && multibandEditorInsertIdx_ < int(chain->size())
               && (*chain)[(size_t)multibandEditorInsertIdx_].kind == InsertMultiband)
            {
                const Rect mbEditor { sourceEditor.x, topY,
                                      fxRack.x + fxRack.w - sourceEditor.x, topH };
                drawMultibandFxEditor(mbEditor);
                drewMultibandEditor = true;
            }
            else
            {
                multibandEditorTrackId_ = -1;
                multibandEditorInsertIdx_ = -1;
            }
        }
        if(focusedNodeId_ != 0 && !focusedNodeValid())
            focusedNodeId_ = 0;
        if(!drewMultibandEditor && focusedNodeId_ != 0)
        {
            // Double-clicked a component → the whole top row shows its detail.
            const Rect detail { sourceEditor.x, topY, fxRack.x + fxRack.w - sourceEditor.x, topH };
            drawFocusedNodeDetail(detail);
        }
        else if(!drewMultibandEditor && matrixViewOpen_)
        {
            // Toolbar MATRIX view: the whole top row becomes the matrix panel.
            const Rect matrixR { sourceEditor.x, topY, fxRack.x + fxRack.w - sourceEditor.x, topH };
            drawMatrixView(matrixR);
        }
        else if(!drewMultibandEditor)
        {
            drawTrackEditor(sourceEditor);
            drawPerVoiceChainEditor(perVoice);
            drawFxRackEditor(fxRack);
        }

        // --- MOD-source strip: aligned to the collapsed Matrix workspace, not the Source column.
        const float bottomFullW = page.w - gap * 2.0f;
        const float sourceWForMatrix = clampf(bottomFullW * 0.13f, 110.0f, 155.0f);
        const float modStripX = page.x + gap + sourceWForMatrix + 38.0f;
        const Rect modStrip { modStripX, topY + topH + gap,
                              page.x + page.w - gap - modStripX, modStripH };
        drawModSourceStrip(modStrip);
        (void)tabH;

        // --- Bottom workspace: Source column (always) + expand/collapse ---
        const float bottomY = modStrip.y + modStrip.h + gap;
        const Rect bottomFull { page.x + gap, bottomY, page.w - gap * 2.0f,
                                page.y + page.h - gap - bottomY };
        drawBottomWorkspace(bottomFull);

        layoutVSplitHandle_     = {};
        layoutRackSplitHandle_  = {};
        layoutStripSplitHandle_ = {};
    }

void KapibaraUI::drawChainEditorPlaceholder(const Rect &r, const char *title, const char *hint)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, title);
        useUiFont();
        uiFontSize(8.0f);
        fillColor(DesignTokens::textSecondary());
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, hint, nullptr);
    }

namespace
{
struct ModChip { const char *label; synth::ModSource src; };
// Only currently engine-supported sources (see ModSource).
const ModChip kModChips[] = {
    { "MOD 1", synth::ModSource::Lfo1 }, { "MOD 2", synth::ModSource::Lfo2 },
    { "MOD 3", synth::ModSource::Lfo3 }, { "MOD 4", synth::ModSource::Lfo4 },
    { "MOD 5", synth::ModSource::Env1 }, { "MOD 6", synth::ModSource::Env2 },
    { "MOD 7", synth::ModSource::Env3 }, { "MOD 8", synth::ModSource::Env4 },
    { "AE 1", synth::ModSource::Adsr1 }, { "AE 2", synth::ModSource::Adsr2 },
    { "AE 3", synth::ModSource::Adsr3 }, { "AE 4", synth::ModSource::Adsr4 },
    { "VEL", synth::ModSource::Velocity }, { "KEY", synth::ModSource::KeyTrack },
    { "RAND", synth::ModSource::Random }, { "CHAOS", synth::ModSource::Chaos },
    { "SHAPE", synth::ModSource::Shape },
};
constexpr int kModChipCount = int(sizeof(kModChips) / sizeof(kModChips[0]));
}

void KapibaraUI::drawModSourceStrip(const Rect &r)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x33495aff));
        const int n = std::min(kModChipCount, int(modStripChipRects_.size()));
        modStripChipCount_ = n;
        const float pad = 4.0f;
        const float chipH = r.h - pad * 2.0f;
        const float chipW = std::max(38.0f, (r.w - pad * 2.0f - float(n - 1) * 4.0f) / float(n));
        const auto slotIndexForSource = [](synth::ModSource src) -> int {
            if(src >= synth::ModSource::Lfo1 && src <= synth::ModSource::Lfo4)
                return int(src) - int(synth::ModSource::Lfo1);
            if(src >= synth::ModSource::Env1 && src <= synth::ModSource::Env4)
                return synth::kMaxLfos + int(src) - int(synth::ModSource::Env1);
            return -1;
        };
        const auto ampEnvIndexForSource = [](synth::ModSource src) -> int {
            if(src >= synth::ModSource::Adsr1 && src <= synth::ModSource::Adsr4)
                return int(src) - int(synth::ModSource::Adsr1);
            return -1;
        };
        const auto isMapped = [&](synth::ModSource src) {
            for(const auto &rule : rules_)
                if(rule.enabled && rule.source == src && std::abs(rule.depth) > 1.0e-6f)
                    return true;
            return false;
        };
        const auto drawPreview = [&](const Rect &pr, synth::ModSource src, bool mapped) {
            const int slot = slotIndexForSource(src);
            const Color col = mapped ? DesignTokens::accentGreen() : DesignTokens::accentCyan().withAlpha(0.72f);
            beginPath();
            roundedRect(pr.x, pr.y, pr.w, pr.h, 3.0f);
            fillColor(rgba(0x071014aa));
            fill();
            strokeColor(DesignTokens::divider().withAlpha(0.55f));
            strokeWidth(1.0f);
            stroke();
            beginPath();
            constexpr int steps = 28;
            for(int s = 0; s < steps; ++s)
            {
                const float t = float(s) / float(steps - 1);
                float v = 0.5f;
                if(slot >= 0)
                    v = synth::pointCurveEval(modSlots_[(size_t)slot].points.data(),
                                              modSlots_[(size_t)slot].pointCount, t);
                else if(const int ae = ampEnvIndexForSource(src); ae >= 0)
                {
                    const auto &env = ampEnvs_[(size_t)ae];
                    const float a = std::max(0.001f, env.attack);
                    const float d = std::max(0.001f, env.decay);
                    const float rel = std::max(0.001f, env.release);
                    const float sum = a + d + rel + 0.35f;
                    const float xA = a / sum;
                    const float xD = (a + d) / sum;
                    const float xS = (a + d + 0.35f) / sum;
                    const float sustain = clampf(env.sustain, 0.0f, 1.0f);
                    if(t <= xA)
                        v = synth::adsrCurveEval(t / std::max(0.001f, xA), env.curveA);
                    else if(t <= xD)
                        v = sustain + (1.0f - sustain)
                            * (1.0f - synth::adsrCurveEval((t - xA) / std::max(0.001f, xD - xA), env.curveD));
                    else if(t <= xS)
                        v = sustain;
                    else
                        v = sustain * (1.0f - synth::adsrCurveEval((t - xS) / std::max(0.001f, 1.0f - xS), env.curveR));
                }
                else if(src == synth::ModSource::Velocity)
                    v = t;
                else if(src == synth::ModSource::KeyTrack)
                    v = 1.0f - t;
                else if(src == synth::ModSource::Random)
                    v = 0.5f + 0.35f * std::sin(t * 31.0f);
                else if(src == synth::ModSource::Chaos)
                    v = 0.5f + 0.28f * std::sin(t * 18.0f) + 0.14f * std::sin(t * 47.0f);
                else if(src == synth::ModSource::Shape)
                    v = 0.5f + 0.38f * std::sin(t * 2.0f * kPi);
                const float px = pr.x + 4.0f + t * (pr.w - 8.0f);
                const float py = pr.y + pr.h - 4.0f - clampf(v, 0.0f, 1.0f) * (pr.h - 8.0f);
                if(s == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(col);
            strokeWidth(mapped ? 1.8f : 1.2f);
            stroke();
            if(mapped)
            {
                float ph = 0.5f;
                if(slot >= 0)
                {
                    const auto &p = modSlots_[(size_t)slot];
                    ph = std::fmod(float(uiNowMs()) * 0.001f * std::max(0.01f, p.rateHz), 1.0f);
                }
                const float yv = slot >= 0
                    ? synth::pointCurveEval(modSlots_[(size_t)slot].points.data(),
                                            modSlots_[(size_t)slot].pointCount, ph)
                    : 0.5f;
                const float px = pr.x + 4.0f + ph * (pr.w - 8.0f);
                const float py = pr.y + pr.h - 4.0f - clampf(yv, 0.0f, 1.0f) * (pr.h - 8.0f);
                strokeLine(px, pr.y + 3.0f, px, pr.y + pr.h - 3.0f, DesignTokens::accentGreen().withAlpha(0.55f), 1.0f);
                beginPath();
                circle(px, py, 2.7f);
                fillColor(DesignTokens::accentGreen());
                fill();
            }
        };
        float cx = r.x + pad;
        for(int i = 0; i < n; ++i)
        {
            const Rect chip { cx, r.y + pad, chipW, chipH };
            modStripChipRects_[(size_t)i]   = chip;
            modStripChipSources_[(size_t)i] = kModChips[i].src;
            const bool dragging = modRouteDragActive_ && modRouteSource_ == kModChips[i].src;
            const bool mapped = isMapped(kModChips[i].src);
            drawPanel(chip, dragging ? rgba(0x17242cff) : rgba(0x101820ff),
                      dragging || mapped ? DesignTokens::accentGreen() : rgba(0x33495aff));
            drawPreview({ chip.x + 4.0f, chip.y + 16.0f, chip.w - 8.0f, chip.h - 20.0f },
                        kModChips[i].src, mapped);
            useUiFont();
            uiFontSize(7.5f);
            fillColor(dragging ? DesignTokens::accentGreen() : DesignTokens::textPrimary());
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            text(chip.x + chip.w * 0.5f, chip.y + 4.0f, kModChips[i].label, nullptr);
            cx += chipW + 4.0f;
        }
    }

bool KapibaraUI::handleBottomLayoutPress(float x, float y)
{
        if(bottomExpandArrowRect_.w > 0.0f && bottomExpandArrowRect_.contains(x, y))
        {
            bottomPanelMode_ = 1; // expand → Source Structure router
            repaint();
            return true;
        }
        if(bottomCollapseArrowRect_.w > 0.0f && bottomCollapseArrowRect_.contains(x, y))
        {
            bottomPanelMode_ = 0; // collapse → back to Modulation, just the Source column
            repaint();
            return true;
        }
        for(int i = 0; i < modStripChipCount_; ++i)
            if(modStripChipRects_[(size_t)i].contains(x, y))
            {
                const auto src = modStripChipSources_[(size_t)i];
                enableModSource(src);
                // Show the matching editor in the collapsed bottom panel.
                if(src >= synth::ModSource::Adsr1 && src <= synth::ModSource::Adsr4)
                {
                    selectedAmpEnv_ = int(src) - int(synth::ModSource::Adsr1);
                    matrixTab_ = 2;
                }
                else
                {
                    matrixTab_ = 1;
                }
                bottomPanelMode_ = 0;
                beginModRouteDrag(src, modStripChipRects_[(size_t)i], x, y);
                return true;
            }
        return false;
    }

END_NAMESPACE_DISTRHO
