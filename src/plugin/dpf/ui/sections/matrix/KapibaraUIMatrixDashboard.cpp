#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Top-row MATRIX view (opened from the toolbar). GRID tab = the per-track cell
// grid; ROUTES tab = the full rule pool as component-style cards. Modulator /
// amp-env editors stay in the bottom dashboard.
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

        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "MATRIX");
        matrixViewTabRects_[0] = { r.x + 96.0f, r.y + 8.0f, 56.0f, 16.0f };
        matrixViewTabRects_[1] = { r.x + 156.0f, r.y + 8.0f, 64.0f, 16.0f };
        drawButton(matrixViewTabRects_[0], "GRID", matrixViewTab_ == 0);
        drawButton(matrixViewTabRects_[1], "ROUTES", matrixViewTab_ == 1);
        matrixViewCloseRect_ = { r.x + r.w - 26.0f, r.y + 8.0f, 18.0f, 16.0f };
        drawButton(matrixViewCloseRect_, "x", false);

        const Rect body { r.x + 16.0f, r.y + 32.0f, r.w - 32.0f, r.h - 42.0f };
        if(matrixViewTab_ == 0)
        {
            // The grid is scoped to the selected track — say so, and count the
            // rules it cannot show (other tracks / FX-param routes).
            matrixMoreRect_ = {};
            const auto *track = currentTrack();
            useUiFont();
            uiFontSize(8.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            if(track != nullptr)
            {
                char scope[80];
                std::snprintf(scope, sizeof(scope), "— %s", track->name.c_str());
                text(r.x + 232.0f, r.y + 16.0f, scope, nullptr);
                int hidden = 0;
                for(const auto &ru : rules_)
                    if(ru.enabled
                       && (ru.targetTrackId != track->id || synth::insertModParamForDest(ru.dest) >= 0))
                        ++hidden;
                if(hidden > 0)
                {
                    char more[32];
                    std::snprintf(more, sizeof(more), "%d more >", hidden);
                    matrixMoreRect_ = { r.x + r.w - 110.0f, r.y + 8.0f, 74.0f, 16.0f };
                    drawButton(matrixMoreRect_, more, false);
                }
            }
            // ROUTES-tab rects must not eat clicks while GRID is up.
            matrixCardHits_.clear();
            matrixRoutesScrollbarRect_ = {};
            matrixRoutesAddRect_ = {};
            if(gridPickerMode_ >= 3) gridPickerMode_ = 0;  // card pickers close with their tab
            drawMatrixGrid(body);
        }
        else
        {
            // GRID rects must not eat clicks while ROUTES is up.
            matrixGridCells_.clear();
            gridSrcLabelRects_.clear();
            gridDestLabelRects_.clear();
            gridAddSrcRect_ = {};
            gridAddDstRect_ = {};
            if(gridPickerMode_ == 1 || gridPickerMode_ == 2) gridPickerMode_ = 0;  // axis pickers close with GRID
            ruleXferRect_ = {}; ruleEditRect_ = {};
            ruleWeightRect_ = {}; ruleMaskRect_ = {}; ruleMaskAxisRect_ = {};
            matrixMoreRect_ = {};
            drawMatrixRoutes(body);
        }
    }

void KapibaraUI::drawMatrixDashboard(const Rect &r)
{
        drawPanel(r, rgba(0x0d151aff), rgba(0x4b6972ff));
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, "Modulators");

        // The grid lives in the toolbar MATRIX view now; this bottom panel hosts
        // only the modulator curve editors and the amp envelopes.
        if(matrixTab_ == 0) matrixTab_ = 1;
        matrixTabRects_.fill({});
        const Rect body { r.x + 16.0f, r.y + 38.0f, r.w - 32.0f, r.h - 50.0f };

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

        if(matrixTab_ == 2) drawMatrixAmpEnv(body);
        else                drawMatrixModulators(body);
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
        const float dim = rule->muted ? 0.35f : 1.0f;  // muted rules read as ghosted
        Color col = rule->depth >= 0.0f ? DesignTokens::accentCyan() : DesignTokens::accentGreen();
        col = col.withAlpha(col.alpha * dim);
        beginPath();
        circle(ncx, ncy, nr);
        fillColor(col.withAlpha(0.16f * dim));
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
        // Shaped rules (mask / narrowed weight / bent transfer) get a second thin
        // outer ring — the "there's more on this route" cue.
        const bool shaped = rule->maskSlot >= 0 || rule->weight != synth::WeightMode::All
                            || std::abs(rule->transferCurve) > 1.0e-3f;
        if(shaped)
        {
            beginPath();
            circle(ncx, ncy, nr + 3.5f);
            strokeColor(col.withAlpha(0.55f * dim));
            strokeWidth(1.0f);
            stroke();
        }
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%+.1f", double(rule->depth));
        uiFontSize(8.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary().withAlpha(dim));
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

        // Selected-rule inspector row (basic tier): the Serum-style transfer bend
        // widget, a read-only summary of any advanced config, and EDIT > which
        // jumps to the rule's card on the ROUTES tab.
        ruleWeightRect_ = {}; ruleMaskRect_ = {}; ruleMaskAxisRect_ = {};
        ruleXferRect_ = {}; ruleEditRect_ = {};
        if(selectedRule_ >= 0 && selectedRule_ < synth::kMaxMatrixRules
           && rules_[(size_t)selectedRule_].enabled)
        {
            const auto &ru = rules_[(size_t)selectedRule_];
            const float iy = r.y + r.h - 18.0f;
            useUiFont();
            uiFontSize(8.0f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            char hdr[64];
            std::snprintf(hdr, sizeof(hdr), "RULE  %s > %s", sourceName(ru.source), destName(ru.dest));
            text(r.x, iy + 8.0f, hdr, nullptr);

            text(r.x + 148.0f, iy + 8.0f, "XFER", nullptr);
            ruleXferRect_ = { r.x + 176.0f, iy, 44.0f, 16.0f };
            drawXferCurve(ruleXferRect_, ru.transferCurve);

            // Read-only summary of the advanced config (edited on ROUTES).
            char sum[96] = "";
            static const char *kWeightNames[] = { "", "LOW", "HIGH", "GRP LO", "GRP MID", "GRP HI", "BAND" };
            static const char *kAxisNames[5] = { "IDX", "SPEC X", "FREQ", "IDX+PH", "TRACK" };
            int sp = 0;
            if(ru.muted)
                sp += std::snprintf(sum + sp, sizeof(sum) - size_t(sp), "MUTED  ");
            if(ru.weight == synth::WeightMode::BandIndex)
                sp += std::snprintf(sum + sp, sizeof(sum) - size_t(sp), "BAND %d..%d  ", ru.bandLo, ru.bandHi);
            else if(ru.weight != synth::WeightMode::All)
                sp += std::snprintf(sum + sp, sizeof(sum) - size_t(sp), "%s  ",
                                    kWeightNames[std::min<int>(int(ru.weight), 6)]);
            if(ru.maskSlot >= 0)
                std::snprintf(sum + sp, sizeof(sum) - size_t(sp), "MASK MOD%d/%s",
                              int(ru.maskSlot) + 1, kAxisNames[std::min<int>(int(ru.maskAxis), 4)]);
            fillColor(DesignTokens::textSecondary().withAlpha(0.8f));
            text(ruleXferRect_.x + ruleXferRect_.w + 12.0f, iy + 8.0f, sum, nullptr);

            ruleEditRect_ = { r.x + r.w - 60.0f, iy, 56.0f, 16.0f };
            drawButton(ruleEditRect_, "EDIT >", false);
        }
    }

// Small transfer-bend widget: a groove box with the bend curve; drag vertically
// to adjust concavity (0 = straight diagonal).
void KapibaraUI::drawXferCurve(const Rect &r, float curve)
{
        drawPanel(r, DesignTokens::groove(), DesignTokens::border());
        const float c = clampf(curve, -1.0f, 1.0f);
        beginPath();
        constexpr int kSteps = 16;
        for(int k = 0; k <= kSteps; ++k)
        {
            const float x = float(k) / float(kSteps);
            const float y = std::abs(c) < 1.0e-4f
                                ? x
                                : (c >= 0.0f ? std::pow(x, 1.0f + c * 4.0f)
                                             : 1.0f - std::pow(1.0f - x, 1.0f - c * 4.0f));
            const float px = r.x + 3.0f + x * (r.w - 6.0f);
            const float py = r.y + r.h - 3.0f - y * (r.h - 6.0f);
            if(k == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(std::abs(c) < 1.0e-4f ? DesignTokens::textSecondary() : DesignTokens::accentCyan());
        strokeWidth(1.3f);
        stroke();
    }

// ROUTES tab: the whole rule pool as component-style cards.
void KapibaraUI::drawMatrixRoutes(const Rect &r)
{
        matrixCardHits_.clear();
        matrixRoutesScrollbarRect_ = {};

        int used = 0;
        for(const auto &ru : rules_)
            if(ru.enabled) ++used;
        useUiFont();
        uiFontSize(8.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary());
        char cnt[32];
        std::snprintf(cnt, sizeof(cnt), "ROUTES %d/%d", used, synth::kMaxMatrixRules);
        text(r.x, r.y + 8.0f, cnt, nullptr);
        matrixRoutesAddRect_ = { r.x + 96.0f, r.y, 74.0f, 16.0f };
        drawButton(matrixRoutesAddRect_, "+ ROUTE",
                   gridPickerMode_ == 3 && gridPickerRuleIdx_ == -2);
        fillColor(DesignTokens::textSecondary().withAlpha(0.55f));
        text(r.x + 184.0f, r.y + 8.0f,
             "click chip = edit - drag depth/curve - right-click card = clear - drop a MOD chip here to add",
             nullptr);

        const Rect list { r.x, r.y + 22.0f, r.w - 14.0f, r.h - 24.0f };
        matrixRoutesListRect_ = list;
        constexpr float cardH = 42.0f, gap = 6.0f;
        const float contentH = float(used) * (cardH + gap);
        matrixRoutesMaxScroll_ = std::max(0.0f, contentH - list.h);

        // Scroll-to (EDIT > / chip drop / picker commit) resolved at draw time.
        if(matrixRoutesScrollTo_ >= 0)
        {
            int ord = 0;
            for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            {
                if(!rules_[(size_t)i].enabled) continue;
                if(i == matrixRoutesScrollTo_)
                {
                    const float top = float(ord) * (cardH + gap);
                    if(top < matrixRoutesScroll_)
                        matrixRoutesScroll_ = top;
                    else if(top + cardH > matrixRoutesScroll_ + list.h)
                        matrixRoutesScroll_ = top + cardH - list.h;
                    break;
                }
                ++ord;
            }
            matrixRoutesScrollTo_ = -1;
        }
        matrixRoutesScroll_ = clampf(matrixRoutesScroll_, 0.0f, matrixRoutesMaxScroll_);

        // Hit rects are clipped to the visible list so half-scrolled cards can't
        // be clicked outside the panel.
        const auto pushHit = [&](const Rect &rc, int rule, uint8_t kind) {
            const float x0 = std::max(rc.x, list.x), y0 = std::max(rc.y, list.y);
            const float x1 = std::min(rc.x + rc.w, list.x + list.w);
            const float y1 = std::min(rc.y + rc.h, list.y + list.h);
            if(x1 <= x0 || y1 <= y0) return;
            matrixCardHits_.push_back(MatrixCardHit { { x0, y0, x1 - x0, y1 - y0 }, rule, kind });
        };
        static const char *kWeightNames[7] = { "ALL", "LOW", "HIGH", "GRP LO", "GRP MID", "GRP HI", "BAND" };
        static const char *kAxisNames[5] = { "IDX", "SPEC X", "FREQ", "IDX+PH", "TRACK" };

        scissor(list.x, list.y, list.w, list.h);
        float cy = list.y - matrixRoutesScroll_;
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
        {
            auto &ru = rules_[(size_t)i];
            if(!ru.enabled) continue;
            const Rect card { list.x, cy, list.w, cardH };
            cy += cardH + gap;
            if(card.y + card.h < list.y || card.y > list.y + list.h)
                continue;  // fully clipped: no draw, no hits

            const bool sel = selectedRule_ == i;
            const float dim = ru.muted ? 0.45f : 1.0f;
            drawPanel(card, sel ? rgba(0x14202aff) : rgba(0x0f171dff),
                      ru.muted ? rgba(0x3a4750ff)
                               : (sel ? DesignTokens::accentCyan() : rgba(0x2c3c46ff)));

            // --- line 1: mute dot | SRC -> DEST | depth bar | x ---------------
            const float l1 = card.y + 4.0f;
            const Rect mute { card.x + 6.0f, l1 + 2.0f, 12.0f, 12.0f };
            beginPath();
            circle(mute.x + 6.0f, mute.y + 6.0f, 4.5f);
            if(ru.muted)
            {
                strokeColor(DesignTokens::textSecondary());
                strokeWidth(1.2f);
                stroke();
            }
            else
            {
                fillColor(DesignTokens::accentGreen());
                fill();
            }
            pushHit(mute, i, MatrixCardHit::Mute);

            const Rect src { card.x + 24.0f, l1, 84.0f, 15.0f };
            drawButton(src, sourceName(ru.source), false);
            pushHit(src, i, MatrixCardHit::Source);
            useUiFont();
            uiFontSize(9.0f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary().withAlpha(dim));
            text(src.x + src.w + 8.0f, l1 + 7.5f, ">", nullptr);

            const bool fxDest = synth::insertModParamForDest(ru.dest) >= 0;
            const Rect dst { src.x + src.w + 16.0f, l1, 96.0f, 15.0f };
            char dlbl[32];
            if(fxDest)
                std::snprintf(dlbl, sizeof(dlbl), "%s s%d", destName(ru.dest), ru.targetSlot + 1);
            else
                std::snprintf(dlbl, sizeof(dlbl), "%s", destName(ru.dest));
            drawButton(dst, dlbl, false);
            if(!fxDest)  // FX dests keep their targetSlot; re-pick via chip drag
                pushHit(dst, i, MatrixCardHit::Dest);

            const Rect del { card.x + card.w - 22.0f, l1, 16.0f, 15.0f };
            drawButton(del, "x", false);
            pushHit(del, i, MatrixCardHit::Delete);

            // Depth: centre-zero bar, same relative drag semantics as grid nodes.
            const Rect depth { dst.x + dst.w + 10.0f, l1, del.x - 12.0f - (dst.x + dst.w + 10.0f), 15.0f };
            drawPanel(depth, DesignTokens::groove(), DesignTokens::border());
            const float limit = modulationDepthLimit(ru.dest);
            const float norm = clampf(ru.depth / std::max(1.0e-6f, limit), -1.0f, 1.0f);
            const float cx0 = depth.x + depth.w * 0.5f;
            const Color dcol = (ru.depth >= 0.0f ? DesignTokens::accentCyan()
                                                 : DesignTokens::accentGreen()).withAlpha(0.7f * dim);
            beginPath();
            rect(std::min(cx0, cx0 + norm * depth.w * 0.5f), depth.y + 2.0f,
                 std::abs(norm) * depth.w * 0.5f, depth.h - 4.0f);
            fillColor(dcol);
            fill();
            char dv[16];
            std::snprintf(dv, sizeof(dv), "%+.1f", double(ru.depth));
            useMonoFont();
            uiFontSize(8.5f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(DesignTokens::textPrimary().withAlpha(dim));
            text(cx0, depth.y + depth.h * 0.5f, dv, nullptr);
            pushHit(depth, i, MatrixCardHit::Depth);

            // --- line 2: @target | XFER | WEIGHT (+band) | MASK (+axis) -------
            const float l2 = card.y + 23.0f;
            const Rect tgt { card.x + 24.0f, l2, 96.0f, 15.0f };
            char tlbl[40];
            if(ru.targetTrackId == 0)
                std::snprintf(tlbl, sizeof(tlbl), "@GLOBAL");
            else
            {
                const int ti = trackIndexOfId(ru.targetTrackId);
                std::snprintf(tlbl, sizeof(tlbl), "@%s",
                              ti >= 0 ? generator_.tracks[(size_t)ti].name.c_str() : "?");
            }
            drawButton(tgt, tlbl, false);
            if(!fxDest)  // FX routes are bound to their insert's track
                pushHit(tgt, i, MatrixCardHit::Target);

            const Rect xf { tgt.x + tgt.w + 8.0f, l2, 40.0f, 15.0f };
            drawXferCurve(xf, ru.transferCurve);
            pushHit(xf, i, MatrixCardHit::Xfer);

            if(!fxDest)  // weight/mask don't apply on the insert-param path
            {
                const Rect wt { xf.x + xf.w + 8.0f, l2, 58.0f, 15.0f };
                drawButton(wt, kWeightNames[std::min<int>(int(ru.weight), 6)],
                           ru.weight != synth::WeightMode::All);
                pushHit(wt, i, MatrixCardHit::Weight);
                float mx = wt.x + wt.w + 6.0f;
                if(ru.weight == synth::WeightMode::BandIndex)
                {
                    const Rect lo { mx, l2, 34.0f, 15.0f };
                    const Rect hi { mx + 38.0f, l2, 34.0f, 15.0f };
                    char bb[12];
                    drawPanel(lo, DesignTokens::groove(), DesignTokens::border());
                    std::snprintf(bb, sizeof(bb), "%d", ru.bandLo);
                    useMonoFont(); uiFontSize(8.5f);
                    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                    fillColor(DesignTokens::textPrimary());
                    text(lo.x + lo.w * 0.5f, lo.y + lo.h * 0.5f, bb, nullptr);
                    drawPanel(hi, DesignTokens::groove(), DesignTokens::border());
                    std::snprintf(bb, sizeof(bb), "%d", ru.bandHi);
                    text(hi.x + hi.w * 0.5f, hi.y + hi.h * 0.5f, bb, nullptr);
                    pushHit(lo, i, MatrixCardHit::BandLo);
                    pushHit(hi, i, MatrixCardHit::BandHi);
                    mx += 78.0f;
                }
                char mlbl[20];
                if(ru.maskSlot >= 0)
                    std::snprintf(mlbl, sizeof(mlbl), "MASK MOD%d", int(ru.maskSlot) + 1);
                else
                    std::snprintf(mlbl, sizeof(mlbl), "MASK OFF");
                const Rect mk { mx, l2, 78.0f, 15.0f };
                drawButton(mk, mlbl, ru.maskSlot >= 0);
                pushHit(mk, i, MatrixCardHit::Mask);
                if(ru.maskSlot >= 0)
                {
                    const Rect ax { mx + 82.0f, l2, 52.0f, 15.0f };
                    drawButton(ax, kAxisNames[std::min<int>(int(ru.maskAxis), 4)], false);
                    pushHit(ax, i, MatrixCardHit::MaskAxis);
                }
            }

            // Whole-row select LAST so specific controls win the hit scan.
            pushHit(card, i, MatrixCardHit::Row);
        }
        resetScissor();

        if(matrixRoutesMaxScroll_ > 0.0f)
        {
            matrixRoutesScrollbarRect_ = { r.x + r.w - 10.0f, list.y, 8.0f, list.h };
            drawPanel(matrixRoutesScrollbarRect_, DesignTokens::groove(), DesignTokens::border());
            const float thumbH = std::max(18.0f, list.h * (list.h / contentH));
            const float ty = list.y + (matrixRoutesScroll_ / matrixRoutesMaxScroll_) * (list.h - thumbH);
            beginPath();
            roundedRect(matrixRoutesScrollbarRect_.x + 1.5f, ty, 5.0f, thumbH, 2.0f);
            fillColor(DesignTokens::textSecondary().withAlpha(0.5f));
            fill();
        }
    }

bool KapibaraUI::handleMatrixRoutesPress(float x, float y)
{
        if(matrixRoutesAddRect_.w > 0.0f && matrixRoutesAddRect_.contains(x, y))
        {
            gridPickerMode_ = 3;
            gridPickerRuleIdx_ = -2;  // staged: rule allocated on picker commit
            gridPickerX_ = x;
            gridPickerY_ = y + 12.0f;
            repaint();
            return true;
        }
        if(matrixRoutesScrollbarRect_.w > 0.0f && matrixRoutesScrollbarRect_.contains(x, y))
        {
            dragTarget_ = DragTarget::MatrixRoutesScroll;
            return true;
        }
        for(const auto &h : matrixCardHits_)
        {
            if(!h.rect.contains(x, y))
                continue;
            if(h.rule < 0 || h.rule >= synth::kMaxMatrixRules)
                return true;
            auto &ru = rules_[(size_t)h.rule];
            selectedRule_ = h.rule;
            switch(h.kind)
            {
                case MatrixCardHit::Mute:
                    ru.muted = ru.muted ? 0 : 1;
                    pushRule(h.rule);
                    break;
                case MatrixCardHit::Source:
                    gridPickerMode_ = 3;
                    gridPickerRuleIdx_ = h.rule;
                    gridPickerX_ = x;
                    gridPickerY_ = y + 12.0f;
                    break;
                case MatrixCardHit::Dest:
                    gridPickerMode_ = 4;
                    gridPickerRuleIdx_ = h.rule;
                    gridPickerX_ = x;
                    gridPickerY_ = y + 12.0f;
                    break;
                case MatrixCardHit::Depth:
                    dragTarget_ = DragTarget::ModDepth;
                    dragStartY_ = y;
                    dragStartDepth_ = ru.depth;
                    dragDepthLimit_ = modulationDepthLimit(ru.dest);
                    break;
                case MatrixCardHit::Delete:
                    ru = synth::MatrixRule {};
                    pushRule(h.rule);
                    break;
                case MatrixCardHit::Xfer:
                    dragTarget_ = DragTarget::RuleXfer;
                    dragStartY_ = y;
                    dragStartDepth_ = ru.transferCurve;
                    break;
                case MatrixCardHit::Weight:
                    ru.weight = synth::WeightMode((int(ru.weight) + 1) % 7);
                    pushRule(h.rule);
                    break;
                case MatrixCardHit::BandLo:
                    dragTarget_ = DragTarget::RuleBandLo;
                    dragStartY_ = y;
                    dragStartNorm_ = float(ru.bandLo) / float(synth::kMaxPartials);
                    break;
                case MatrixCardHit::BandHi:
                    dragTarget_ = DragTarget::RuleBandHi;
                    dragStartY_ = y;
                    dragStartNorm_ = float(ru.bandHi) / float(synth::kMaxPartials);
                    break;
                case MatrixCardHit::Mask:
                    ru.maskSlot = ru.maskSlot >= synth::kMaxModSlots - 1 ? int8_t(-1)
                                                                         : int8_t(ru.maskSlot + 1);
                    pushRule(h.rule);
                    break;
                case MatrixCardHit::MaskAxis:
                    ru.maskAxis = uint8_t((ru.maskAxis + 1) % 5);
                    pushRule(h.rule);
                    break;
                case MatrixCardHit::Target:
                {
                    // @GLOBAL -> track 1 -> ... -> track n -> @GLOBAL
                    const int n = int(generator_.tracks.size());
                    int ti = ru.targetTrackId == 0 ? -1 : trackIndexOfId(ru.targetTrackId);
                    ++ti;
                    ru.targetTrackId = (ti < 0 || ti >= n) ? 0u : generator_.tracks[(size_t)ti].id;
                    pushRule(h.rule);
                    break;
                }
                case MatrixCardHit::Row:
                default:
                    break;  // selection already updated
            }
            repaint();
            return true;
        }
        return false;
    }

void KapibaraUI::drawGridAxisPicker()
{
        gridPickerItemRects_.clear();
        gridPickerPoolIdx_.clear();
        if(gridPickerMode_ == 0)
            return;
        // Modes: 1 = grid source axis, 2 = grid dest axis (taken-filtered),
        //        3 = card source, 4 = card dest (full pools, no filter).
        const bool srcMode = gridPickerMode_ == 1 || gridPickerMode_ == 3;
        const bool cardMode = gridPickerMode_ >= 3;
        const int poolN = cardMode
                              ? (srcMode ? int(sizeof(kCardSourcePool) / sizeof(kCardSourcePool[0]))
                                         : int(sizeof(kCardDestPool) / sizeof(kCardDestPool[0])))
                              : (srcMode ? int(sizeof(kGridSourcePool) / sizeof(kGridSourcePool[0]))
                                         : int(sizeof(kGridDestPool) / sizeof(kGridDestPool[0])));
        std::vector<int> avail;
        for(int i = 0; i < poolN; ++i)
        {
            const bool taken = !cardMode
                               && (srcMode
                                       ? std::find(gridSources_.begin(), gridSources_.end(),
                                                   kGridSourcePool[i]) != gridSources_.end()
                                       : std::find(gridDests_.begin(), gridDests_.end(),
                                                   kGridDestPool[i]) != gridDests_.end());
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
            const char *nm = cardMode
                                 ? (srcMode ? sourceName(kCardSourcePool[avail[k]]) : destName(kCardDestPool[avail[k]]))
                                 : (srcMode ? sourceName(kGridSourcePool[avail[k]]) : destName(kGridDestPool[avail[k]]));
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
        // "N more >" chip — routes the grid can't show live on the ROUTES tab.
        if(matrixMoreRect_.w > 0.0f && matrixMoreRect_.contains(x, y))
        {
            matrixViewTab_ = 1;
            repaint();
            return true;
        }
        // Inspector row: transfer-bend drag + EDIT > jump to the rule's card.
        if(selectedRule_ >= 0 && selectedRule_ < synth::kMaxMatrixRules
           && rules_[(size_t)selectedRule_].enabled)
        {
            if(ruleXferRect_.w > 0.0f && ruleXferRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::RuleXfer;
                dragStartY_ = y;
                dragStartDepth_ = rules_[(size_t)selectedRule_].transferCurve;
                return true;
            }
            if(ruleEditRect_.w > 0.0f && ruleEditRect_.contains(x, y))
            {
                matrixViewTab_ = 1;
                matrixRoutesScrollTo_ = selectedRule_;
                repaint();
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
                // A freed slot keeps its old mask/band/curve fields — reset to
                // defaults so a fresh route never inherits stale config.
                ru = synth::MatrixRule {};
                ru.enabled = true;
                ru.source = c.src;
                ru.dest = c.dst;
                ru.targetTrackId = track->id;
                ru.depth = defaultModulationDepth(c.dst);
                enableModSource(c.src);
                // pushMatrix() sends rules_[selectedRule_]; select BEFORE pushing
                // or a click-create (no drag) never reaches the engine.
                selectedRule_ = idx;
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
