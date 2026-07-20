#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Top-row MATRIX view (opened from the toolbar): tabs + close X + the dashboard.
void KapibaraUI::drawMatrixView(const Rect &r)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        // The three top editors aren't drawn while the matrix view is up — their
        // stale rects must not eat clicks.
        clearTrackEditorRects();
        ampFocusKnobRects_.fill({});
        pvChainTabRects_.fill({}); pvChainEnableRect_ = {}; pvChainTypeRect_ = {};
        pvChainKnobRects_.fill({}); pvChainAddRect_ = {}; pvChainCount_ = 0;
        fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
        fxRackPanelRects_.clear();

        matrixViewCloseRect_ = { r.x + r.w - 26.0f, r.y + 8.0f, 18.0f, 16.0f };
        drawButton(matrixViewCloseRect_, "x", false);

        static const char *kTabs[3] = { "GRID", "MODULATORS", "AMP ENV" };
        const float tabW = 84.0f, tabH = 16.0f;
        for(int i = 0; i < 3; ++i)
        {
            matrixTabRects_[(size_t)i] = { r.x + 10.0f + float(i) * (tabW + 4.0f), r.y + 8.0f, tabW, tabH };
            drawButton(matrixTabRects_[(size_t)i], kTabs[i], matrixTab_ == i);
        }

        drawMatrixDashboard({ r.x + 8.0f, r.y + 30.0f, r.w - 16.0f, r.h - 38.0f });
    }

void KapibaraUI::drawMatrixDashboard(const Rect &r)
{
        drawPanel(r, rgba(0x0d151aff), rgba(0x4b6972ff));
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, "Matrix");

        // (Tabs are drawn by drawMatrixView, which owns matrixTabRects_.)
        const Rect body { r.x + 16.0f, r.y + 38.0f, r.w - 32.0f, r.h - 50.0f };

        matrixGridCells_.clear();
        if(matrixTab_ != 0)
        {
            gridSrcLabelRects_.clear();
            gridDestLabelRects_.clear();
            gridAddSrcRect_ = {};
            gridAddDstRect_ = {};
            gridPickerMode_ = 0;
        }
        for(auto &rc : modSlotSelectRects_) rc = {};
        for(auto &rc : ampEnvTabRects_) rc = {};
        lfoEnableRect_ = {}; envEnableRect_ = {}; adsrSourceRect_ = {};
        modModeRect_ = {}; modEnvRateRect_ = {};
        if(matrixTab_ != 2) ampAdsrRect_ = {};
        matrixEnvCurveRect_ = {};
        attackRect_ = {}; decayRect_ = {}; sustainRect_ = {}; releaseRect_ = {};
        ruleEnableRect_ = {}; ruleSourceRect_ = {}; ruleDestRect_ = {}; ruleWeightRect_ = {};
        ruleDepthRect_ = {}; ruleBandLoRect_ = {}; ruleBandHiRect_ = {};
        for(auto &rc : ruleSelectRects_) rc = {};
        chaosEnableRect_ = {}; shapeAxisRect_ = {};
        chaosRateRect_ = {}; chaosAmountRect_ = {};
        shapePhaseRect_ = {}; shapeRhoRect_ = {}; shapeUpRect_ = {}; shapeDownRect_ = {};

        if(matrixTab_ == 1)      drawMatrixModulators(body);
        else if(matrixTab_ == 2) drawMatrixAmpEnv(body);
        else                     drawMatrixGrid(body);
    }

void KapibaraUI::drawMatrixModulators(const Rect &r)
{
        const int nSlots = synth::kMaxModSlots;
        selectedMatrixModSlot_ = clampi(selectedMatrixModSlot_, 0, nSlots - 1);
        for(auto &rc : modSlotSelectRects_) rc = {};

        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        const bool selectedEnv = selectedMatrixModSlot_ >= synth::kMaxLfos;
        std::snprintf(scratch_, sizeof scratch_, "%s %d",
                      selectedEnv ? "ENV" : "MOD",
                      selectedEnv ? selectedMatrixModSlot_ - synth::kMaxLfos + 1 : selectedMatrixModSlot_ + 1);
        text(r.x, r.y, scratch_, nullptr);
        text(r.x + 54.0f, r.y, "double-click = add/remove point   Ctrl-drag = bend   Shift = no snap", nullptr);

        bool &loopRef = curCurveLoop();
        float &rateRef = curCurveRate();
        modModeRect_ = { r.x + r.w - 132.0f, r.y, 132.0f, 20.0f };
        drawButton(modModeRect_, loopRef ? "Mode: LOOP" : "Mode: ENV", loopRef);

        const float knobH = 54.0f;
        const float curveBottom = (r.y + r.h) - knobH - 8.0f;
        matrixEnvCurveRect_ = { r.x, r.y + 24.0f, r.w, std::max(60.0f, curveBottom - (r.y + 24.0f)) };
        drawMatrixEnvCurve(matrixEnvCurveRect_, curCurvePoints(), curCurveCount(),
                           loopRef ? DesignTokens::accentCyan() : DesignTokens::accentGreen());
        modEnvRateRect_ = { r.x, r.y + r.h - knobH, (r.w - 8.0f) / 2.0f, 48.0f };
        drawKnob(modEnvRateRect_, "Rate", rateRef / 20.0f, rateRef);
    }

void KapibaraUI::drawMatrixAmpEnv(const Rect &r)
{
        for(auto &rc : ampEnvTabRects_) rc = {};
        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(r.x, r.y + 11.0f, buttonText("AE%d", selectedAmpEnv_ + 1), nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary());
        text(r.x + r.w, r.y + 11.0f, "Ctrl-drag a segment = bend", nullptr);
        auto &ampEnv = ampEnvs_[(size_t)selectedAmpEnv_];
        const float kw = (r.w - 18.0f) * 0.25f;
        attackRect_  = { r.x,                      r.y + 30.0f, kw, 44.0f };
        decayRect_   = { r.x + kw + 6.0f,          r.y + 30.0f, kw, 44.0f };
        sustainRect_ = { r.x + (kw + 6.0f) * 2.0f, r.y + 30.0f, kw, 44.0f };
        releaseRect_ = { r.x + (kw + 6.0f) * 3.0f, r.y + 30.0f, kw, 44.0f };
        drawKnob(attackRect_,  "A", ampEnv.attack  / 5.0f, ampEnv.attack);
        drawKnob(decayRect_,   "D", ampEnv.decay   / 5.0f, ampEnv.decay);
        drawKnob(sustainRect_, "S", ampEnv.sustain, ampEnv.sustain);
        drawKnob(releaseRect_, "R", ampEnv.release / 8.0f, ampEnv.release);
        drawAdsrCurve({ r.x, r.y + 84.0f, r.w, std::max(60.0f, (r.y + r.h) - (r.y + 84.0f)) }, ampEnv);
    }

void KapibaraUI::drawMatrixGridNode(const Rect &cell, const synth::MatrixRule *rule)
{
        const float ncx = cell.x + cell.w * 0.5f;
        const float ncy = cell.y + cell.h * 0.5f;
        if(rule == nullptr)
        {
            beginPath();
            circle(ncx, ncy, 1.6f);
            fillColor(DesignTokens::divider());
            fill();
            return;
        }
        const float nr = std::min(cell.w, cell.h) * 0.36f;
        const float amt = clampf(std::abs(rule->depth) / modulationDepthLimit(rule->dest), 0.0f, 1.0f);
        const Color col = rule->depth >= 0.0f ? DesignTokens::accentCyan() : DesignTokens::accentGreen();
        beginPath();
        circle(ncx, ncy, nr);
        fillColor(col.withAlpha(0.16f));
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(2.0f);
        stroke();
        lineCap(ROUND);
        beginPath();
        const float a0 = -kPi * 0.5f;
        arc(ncx, ncy, nr, a0, a0 + 2.0f * kPi * std::max(0.02f, amt), CW);
        strokeColor(col);
        strokeWidth(2.4f);
        stroke();
        lineCap(BUTT);
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%+.1f", double(rule->depth));
        uiFontSize(8.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(ncx, ncy, buf, nullptr);
    }

void KapibaraUI::drawMatrixGrid(const Rect &r)
{
        const auto *track = currentTrack();
        const int nS = int(gridSources_.size());
        const int nD = int(gridDests_.size());

        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        fillColor(DesignTokens::textSecondary());
        text(r.x, r.y, "click = route  ·  drag = depth  ·  right-click node = clear  ·  right-click label = remove axis", nullptr);

        const float labelW = 58.0f;
        const float headH = 16.0f;
        const float gridX = r.x + labelW;
        const float topY = r.y + 16.0f;
        const float gridY = topY + headH;
        const float addW = 26.0f;
        const float availW = (r.x + r.w) - gridX - addW;
        const float availH = (r.y + r.h) - gridY - 22.0f;
        const float cellW = clampf(availW / float(std::max(1, nD)), 32.0f, 110.0f);
        const float cellH = clampf(availH / float(std::max(1, nS)), 24.0f, 46.0f);

        gridSrcLabelRects_.clear();
        gridDestLabelRects_.clear();

        uiFontSize(8.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        for(int d = 0; d < nD; ++d)
        {
            const Rect hr { gridX + cellW * float(d), topY, cellW, headH };
            gridDestLabelRects_.push_back(hr);
            fillColor(DesignTokens::textPrimary());
            text(hr.x + hr.w * 0.5f, hr.y + hr.h * 0.5f, destName(gridDests_[(size_t)d]), nullptr);
        }
        gridAddDstRect_ = { gridX + cellW * float(nD) + 2.0f, topY, addW - 4.0f, headH };
        drawButton(gridAddDstRect_, "+", gridPickerMode_ == 2);

        for(int s = 0; s < nS; ++s)
        {
            const Rect lr { r.x, gridY + cellH * float(s), labelW - 4.0f, cellH };
            gridSrcLabelRects_.push_back(lr);
            uiFontSize(8.0f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(lr.x + 2.0f, lr.y + lr.h * 0.5f, sourceName(gridSources_[(size_t)s]), nullptr);
        }
        gridAddSrcRect_ = { r.x, gridY + cellH * float(nS) + 2.0f, labelW - 4.0f, 20.0f };
        drawButton(gridAddSrcRect_, "+ src", gridPickerMode_ == 1);

        matrixGridCells_.clear();
        for(int s = 0; s < nS; ++s)
            for(int d = 0; d < nD; ++d)
            {
                const Rect cell { gridX + cellW * float(d), gridY + cellH * float(s), cellW, cellH };
                matrixGridCells_.push_back(MatrixCell { cell, gridSources_[(size_t)s], gridDests_[(size_t)d] });
                beginPath();
                rect(cell.x + 1.0f, cell.y + 1.0f, cell.w - 2.0f, cell.h - 2.0f);
                strokeColor(DesignTokens::divider().withAlpha(0.5f));
                strokeWidth(1.0f);
                stroke();
                const synth::MatrixRule *rule = nullptr;
                if(track != nullptr)
                    for(const auto &ru : rules_)
                        if(ru.enabled && ru.source == gridSources_[(size_t)s] && ru.dest == gridDests_[(size_t)d]
                           && ru.targetTrackId == track->id) { rule = &ru; break; }
                drawMatrixGridNode(cell, rule);
            }

        // Selected-rule inspector row: weight mode + spatial mask (a MOD slot's
        // curve mapped over the partial axis instead of time).
        ruleWeightRect_ = {}; ruleMaskRect_ = {}; ruleMaskAxisRect_ = {};
        if(selectedRule_ >= 0 && selectedRule_ < synth::kMaxMatrixRules
           && rules_[(size_t)selectedRule_].enabled)
        {
            const auto &ru = rules_[(size_t)selectedRule_];
            static const char *kWeightNames[] = { "ALL", "LOW", "HIGH", "GRP LO", "GRP MID", "GRP HI", "BAND" };
            const float iy = r.y + r.h - 18.0f;
            useUiFont();
            uiFontSize(8.0f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            char hdr[64];
            std::snprintf(hdr, sizeof(hdr), "RULE  %s > %s", sourceName(ru.source), destName(ru.dest));
            text(r.x, iy + 8.0f, hdr, nullptr);

            ruleWeightRect_ = { r.x + 150.0f, iy, 62.0f, 16.0f };
            drawButton(ruleWeightRect_, kWeightNames[std::min<int>(int(ru.weight), 6)], false);

            char mlbl[16];
            if(ru.maskSlot >= 0)
                std::snprintf(mlbl, sizeof(mlbl), "MASK MOD%d", int(ru.maskSlot) + 1);
            else
                std::snprintf(mlbl, sizeof(mlbl), "MASK OFF");
            ruleMaskRect_ = { ruleWeightRect_.x + 68.0f, iy, 84.0f, 16.0f };
            drawButton(ruleMaskRect_, mlbl, ru.maskSlot >= 0);
            if(ru.maskSlot >= 0)
            {
                ruleMaskAxisRect_ = { ruleMaskRect_.x + 90.0f, iy, 58.0f, 16.0f };
                drawButton(ruleMaskAxisRect_, ru.maskAxis == 1 ? "SPEC X" : "IDX", false);
            }
        }
    }

void KapibaraUI::drawGridAxisPicker()
{
        gridPickerItemRects_.clear();
        gridPickerPoolIdx_.clear();
        if(gridPickerMode_ == 0)
            return;
        const bool srcMode = gridPickerMode_ == 1;
        const int poolN = srcMode ? int(sizeof(kGridSourcePool) / sizeof(kGridSourcePool[0]))
                                  : int(sizeof(kGridDestPool) / sizeof(kGridDestPool[0]));
        std::vector<int> avail;
        for(int i = 0; i < poolN; ++i)
        {
            const bool taken = srcMode
                ? std::find(gridSources_.begin(), gridSources_.end(), kGridSourcePool[i]) != gridSources_.end()
                : std::find(gridDests_.begin(), gridDests_.end(), kGridDestPool[i]) != gridDests_.end();
            if(!taken)
                avail.push_back(i);
        }
        const float rowH = 20.0f;
        const float w = 140.0f;
        const float h = std::max(rowH, rowH * float(avail.size())) + 8.0f;
        const float px = clampf(gridPickerX_, 4.0f, float(uiW()) - w - 4.0f);
        const float py = clampf(gridPickerY_, 4.0f, float(uiH()) - h - 4.0f);
        drawPanel({ px, py, w, h }, rgba(0x10171df8), rgba(0x5b7380ff));
        useUiFont();
        uiFontSize(11.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for(size_t k = 0; k < avail.size(); ++k)
        {
            const Rect it { px + 4.0f, py + 4.0f + rowH * float(k), w - 8.0f, rowH - 2.0f };
            gridPickerItemRects_.push_back(it);
            gridPickerPoolIdx_.push_back(avail[k]);
            fillColor(DesignTokens::textPrimary());
            const char *nm = srcMode ? sourceName(kGridSourcePool[avail[k]]) : destName(kGridDestPool[avail[k]]);
            text(it.x + 8.0f, it.y + it.h * 0.5f, nm, nullptr);
        }
        if(avail.empty())
        {
            fillColor(DesignTokens::textSecondary());
            text(px + 8.0f, py + 4.0f + rowH * 0.5f, "(all added)", nullptr);
        }
    }

void KapibaraUI::enableModSource(synth::ModSource src)
{
        if(src >= synth::ModSource::Lfo1 && src <= synth::ModSource::Lfo4)
        { int s = int(src) - int(synth::ModSource::Lfo1); selectedMatrixModSlot_ = s; modSlots_[(size_t)s].enabled = true; }
        else if(src >= synth::ModSource::Env1 && src <= synth::ModSource::Env4)
        { int s = synth::kMaxLfos + int(src) - int(synth::ModSource::Env1); selectedMatrixModSlot_ = s; modSlots_[(size_t)s].enabled = true; }
    }

bool KapibaraUI::handleMatrixGridPress(float x, float y)
{
        auto *track = currentTrack();
        if(track == nullptr)
            return false;
        // Selected-rule inspector row (weight / spatial mask / mask axis).
        if(selectedRule_ >= 0 && selectedRule_ < synth::kMaxMatrixRules
           && rules_[(size_t)selectedRule_].enabled)
        {
            auto &ru = rules_[(size_t)selectedRule_];
            if(ruleWeightRect_.w > 0.0f && ruleWeightRect_.contains(x, y))
            {
                ru.weight = synth::WeightMode((int(ru.weight) + 1) % 7);
                pushRuleOnly();
                return true;
            }
            if(ruleMaskRect_.w > 0.0f && ruleMaskRect_.contains(x, y))
            {
                // OFF → MOD1 … MOD8 → OFF
                ru.maskSlot = ru.maskSlot >= synth::kMaxModSlots - 1 ? int8_t(-1)
                                                                     : int8_t(ru.maskSlot + 1);
                pushRuleOnly();
                return true;
            }
            if(ruleMaskAxisRect_.w > 0.0f && ruleMaskAxisRect_.contains(x, y))
            {
                ru.maskAxis = ru.maskAxis == 0 ? 1 : 0;
                pushRuleOnly();
                return true;
            }
        }
        for(const auto &c : matrixGridCells_)
        {
            if(!c.rect.contains(x, y))
                continue;
            int idx = -1, freeIdx = -1;
            for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            {
                auto &ru = rules_[(size_t)i];
                if(ru.enabled && ru.source == c.src && ru.dest == c.dst && ru.targetTrackId == track->id)
                { idx = i; break; }
                if(freeIdx < 0 && !ru.enabled)
                    freeIdx = i;
            }
            if(idx < 0)
            {
                if(freeIdx < 0)
                    return true;
                idx = freeIdx;
                auto &ru = rules_[(size_t)idx];
                ru.enabled = true;
                ru.source = c.src;
                ru.dest = c.dst;
                ru.targetTrackId = track->id;
                ru.targetSlot = 0;
                ru.weight = synth::WeightMode::All;
                ru.depth = defaultModulationDepth(c.dst);
                enableModSource(c.src);
                pushMatrix();
            }
            selectedRule_ = idx;
            dragTarget_ = DragTarget::ModDepth;
            dragStartY_ = y;
            dragStartDepth_ = rules_[(size_t)idx].depth;
            dragDepthLimit_ = modulationDepthLimit(rules_[(size_t)idx].dest);
            return true;
        }
        return false;
    }

bool KapibaraUI::handleMatrixGridDelete(float x, float y)
{
        auto *track = currentTrack();
        if(track == nullptr)
            return false;
        for(const auto &c : matrixGridCells_)
        {
            if(!c.rect.contains(x, y))
                continue;
            for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            {
                auto &ru = rules_[(size_t)i];
                if(ru.enabled && ru.source == c.src && ru.dest == c.dst && ru.targetTrackId == track->id)
                {
                    ru.enabled = false;
                    pushMatrix();
                    break;
                }
            }
            return true;
        }
        return false;
    }

END_NAMESPACE_DISTRHO
