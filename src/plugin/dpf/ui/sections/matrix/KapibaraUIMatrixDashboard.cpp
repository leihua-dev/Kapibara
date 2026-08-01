#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Top-row MATRIX view (opened from the toolbar). ROUTES = the basic tier
// (source > dest cards, depth + transfer bend only); GROUPS = the advanced tier
// (mask groups: one base LFO fanned across many targets). Modulator / amp-env
// editors stay in the bottom dashboard.
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
        matrixViewTabRects_[0] = { r.x + 96.0f, r.y + 8.0f, 66.0f, 16.0f };
        matrixViewTabRects_[1] = { r.x + 166.0f, r.y + 8.0f, 96.0f, 16.0f };
        drawButton(matrixViewTabRects_[0], "ROUTES", matrixViewTab_ == 0);
        drawButton(matrixViewTabRects_[1], "MASK GROUPS", matrixViewTab_ == 1);
        matrixViewCloseRect_ = { r.x + r.w - 26.0f, r.y + 8.0f, 18.0f, 16.0f };
        drawButton(matrixViewCloseRect_, "x", false);

        const Rect body { r.x + 16.0f, r.y + 32.0f, r.w - 32.0f, r.h - 42.0f };
        if(matrixViewTab_ == 0)
        {
            // GROUPS rects must not eat clicks while ROUTES is up.
            groupSlotHits_.clear();
            groupSelRects_.fill({});
            groupEnableRect_ = {}; groupBaseRect_ = {};
            groupRateRect_ = {};
            groupFreqRect_ = {}; groupPhaseRect_ = {}; groupCurveRect_ = {};
            groupFamilyRect_ = {}; groupFamilyDestRect_ = {};
            groupFamilyTrackRect_ = {}; groupFamilyDepthRect_ = {};
            groupPreviewRect_ = {};
            if(gridPickerMode_ == 5) gridPickerMode_ = 0;
            drawMatrixRoutes(body);
        }
        else
        {
            // ROUTES rects must not eat clicks while GROUPS is up.
            matrixCardHits_.clear();
            matrixRoutesScrollbarRect_ = {};
            matrixRoutesAddRect_ = {};
            matrixRoutesListRect_ = {};
            if(gridPickerMode_ == 3 || gridPickerMode_ == 4) gridPickerMode_ = 0;
            drawMaskGroups(body);
        }
    }

// Zero every MATRIX-view hit rect. Called by whichever top-row branch draws
// INSTEAD of the matrix (multiband / focused detail / normal editors / closed
// view) so stale rects can never fire underneath another editor.
void KapibaraUI::clearMatrixRects()
{
        matrixViewCloseRect_ = {};
        matrixViewTabRects_.fill({});
        matrixCardHits_.clear();
        matrixRoutesScrollbarRect_ = {};
        matrixRoutesAddRect_ = {};
        matrixRoutesListRect_ = {};
        groupSlotHits_.clear();
        groupSelRects_.fill({});
        groupEnableRect_ = {}; groupBaseRect_ = {};
        groupRateRect_ = {};
        groupFreqRect_ = {}; groupPhaseRect_ = {}; groupCurveRect_ = {};
        groupFamilyRect_ = {}; groupFamilyDestRect_ = {};
        groupFamilyTrackRect_ = {}; groupFamilyDepthRect_ = {};
        groupPreviewRect_ = {};
        if(gridPickerMode_ != 0)
        {
            gridPickerMode_ = 0;
            gridPickerRuleIdx_ = -1;
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
        chaosEnableRect_ = {}; chaosTypeRect_ = {}; shapeTypeRect_ = {}; shapeAxisRect_ = {};
        chaosRateRect_ = {}; chaosAmountRect_ = {};
        shapePhaseRect_ = {}; shapeRhoRect_ = {}; shapeUpRect_ = {}; shapeDownRect_ = {};

        if(matrixTab_ == 2)      drawMatrixAmpEnv(body);
        else if(matrixTab_ == 3) drawMatrixChaosShape(body);
        else                     drawMatrixModulators(body);
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

// Small transfer-bend widget: a groove box with the bend curve; drag vertically
// to adjust concavity (0 = straight diagonal).
// CHAOS + SHAPE. Both are matrix sources with no editor until now: the params,
// the sync calls and all six drag targets already existed, nothing ever drew
// them. Reached from the CHAOS / SHAPE chips in the strip above.
void KapibaraUI::drawMatrixChaosShape(const Rect &r)
{
        chaosEnableRect_ = {}; chaosTypeRect_ = {}; chaosRateRect_ = {}; chaosAmountRect_ = {};
        shapeTypeRect_ = {}; shapeAxisRect_ = {}; shapePhaseRect_ = {};
        shapeRhoRect_ = {}; shapeUpRect_ = {}; shapeDownRect_ = {};

        const float colW = (r.w - 16.0f) * 0.5f;
        const Rect left { r.x, r.y, colW, r.h };
        const Rect right { r.x + colW + 16.0f, r.y, colW, r.h };

        // ---- CHAOS: a noise source, so plot it over TIME -------------------
        drawGroupLabel(left.x, left.y, "CHAOS");
        float y = left.y + 16.0f;
        chaosEnableRect_ = { left.x, y, 52.0f, 20.0f };
        drawButton(chaosEnableRect_, "ON", chaos_.enabled);
        static const char *kChaosNames[3] = { "WHITE", "SMOOTH", "CRACKLE" };
        chaosTypeRect_ = { left.x + 58.0f, y, 92.0f, 20.0f };
        drawButton(chaosTypeRect_, kChaosNames[clampi(int(chaos_.type), 0, 2)], true);
        y += 26.0f;
        const float kw = std::min(78.0f, (colW - 12.0f) * 0.5f);
        chaosRateRect_ = { left.x, y, kw, 46.0f };
        chaosAmountRect_ = { left.x + kw + 12.0f, y, kw, 46.0f };
        drawKnob(chaosRateRect_, "Rate", chaos_.frequencyHz / 60.0f, chaos_.frequencyHz);
        drawKnob(chaosAmountRect_, "Amount", chaos_.amount, chaos_.amount);
        y += 52.0f;

        const float plotH = (left.y + left.h) - y;
        if(plotH >= 40.0f)
        {
            const Rect plot { left.x, y, colW, plotH };
            drawPlotBackground(plot, 8, 4);
            scissor(plot.x + 2.0f, plot.y + 2.0f, plot.w - 4.0f, plot.h - 4.0f);
            const float mid = plot.y + plot.h * 0.5f;
            // Run the engine's own chaos recurrence offline over the plot width.
            // A hand-drawn lookalike would stop matching the moment the DSP moved.
            const int n = clampi(int(plot.w), 64, 512);
            const int interval = std::max(1, int(float(n) / std::max(0.5f, chaos_.frequencyHz * 0.25f)));
            float value = 0.0f, target = 0.0f, crackle = 0.371f;
            int counter = 0;
            uint32_t seed = 1u;
            beginPath();
            for(int i = 0; i < n; ++i)
            {
                if(++counter >= interval)
                {
                    counter = 0;
                    seed = seed * 1664525u + 1013904223u;
                    const float u = float(seed >> 8) * (1.0f / 16777216.0f);
                    target = 2.0f * u - 1.0f;
                    if(chaos_.type == synth::ChaosNoiseType::White)
                        value = target;
                    else if(chaos_.type == synth::ChaosNoiseType::Crackle)
                    {
                        crackle = crackle * 1.997f + 0.217f + 0.07f * target;
                        crackle -= std::floor(crackle);
                        value = (crackle > 0.72f ? 1.0f : -0.35f) * std::abs(target);
                    }
                }
                if(chaos_.type == synth::ChaosNoiseType::Smooth)
                    value += 0.18f * (target - value);
                const float v = clampf(value * chaos_.amount, -1.0f, 1.0f);
                const float px = plot.x + (float(i) + 0.5f) * plot.w / float(n);
                const float py = mid - v * plot.h * 0.42f;
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(chaos_.enabled ? DesignTokens::accentCyan()
                                       : DesignTokens::textSecondary().withAlpha(0.35f));
            strokeWidth(1.3f);
            stroke();
            resetScissor();
        }

        // ---- SHAPE: a spectral distribution, so plot it over the AXIS ------
        drawGroupLabel(right.x, right.y, "SHAPE");
        y = right.y + 16.0f;
        static const char *kShapeNames[5] = { "ASYM", "SINE", "SQUARE", "TRI", "S&H" };
        shapeTypeRect_ = { right.x, y, 80.0f, 20.0f };
        drawButton(shapeTypeRect_, kShapeNames[clampi(int(shape_.shape), 0, 4)], true);
        shapeAxisRect_ = { right.x + 86.0f, y, 108.0f, 20.0f };
        drawButton(shapeAxisRect_, shape_.useSpectralX ? "AXIS: SPECTRAL" : "AXIS: INDEX",
                   shape_.useSpectralX);
        y += 26.0f;
        // Rho/Up/Down only shape the Asymmetric curve; the others ignore them, so
        // they are not drawn (and their rects stay zeroed, so they take no clicks).
        const bool asym = shape_.shape == synth::LfoShape::Asymmetric;
        const int knobs = asym ? 4 : 1;
        const float sw = std::min(78.0f, (colW - float(knobs - 1) * 8.0f) / float(knobs));
        float kx = right.x;
        shapePhaseRect_ = { kx, y, sw, 46.0f };
        drawKnob(shapePhaseRect_, "Phase", shape_.phase0, shape_.phase0);
        kx += sw + 8.0f;
        if(asym)
        {
            shapeRhoRect_ = { kx, y, sw, 46.0f };
            drawKnob(shapeRhoRect_, "Rho", shape_.rho, shape_.rho);
            kx += sw + 8.0f;
            shapeUpRect_ = { kx, y, sw, 46.0f };
            drawKnob(shapeUpRect_, "Up", (shape_.pUp - 0.1f) / 7.9f, shape_.pUp);
            kx += sw + 8.0f;
            shapeDownRect_ = { kx, y, sw, 46.0f };
            drawKnob(shapeDownRect_, "Down", (shape_.pDown - 0.1f) / 7.9f, shape_.pDown);
        }
        y += 52.0f;

        const float sPlotH = (right.y + right.h) - y;
        if(sPlotH >= 40.0f)
        {
            const Rect plot { right.x, y, colW, sPlotH };
            drawPlotBackground(plot, 8, 4);
            scissor(plot.x + 2.0f, plot.y + 2.0f, plot.w - 4.0f, plot.h - 4.0f);
            const float mid = plot.y + plot.h * 0.5f;
            const int n = clampi(int(plot.w), 64, 512);
            beginPath();
            for(int i = 0; i <= n; ++i)
            {
                const float t = float(i) / float(n);
                // The engine's own evaluator, not a copy of its formula.
                const float v = synth::ModMatrix::shapeOutput(shape_, t);
                const float px = plot.x + t * plot.w;
                const float py = mid - v * plot.h * 0.42f;
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(DesignTokens::accentGreen());
            strokeWidth(1.5f);
            stroke();
            resetScissor();
            useUiFont();
            uiFontSize(7.5f);
            textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
            fillColor(DesignTokens::textSecondary().withAlpha(0.6f));
            text(plot.x + 4.0f, plot.y + plot.h - 3.0f,
                 shape_.useSpectralX ? "across spectral x" : "across partial index", nullptr);
        }
    }

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

// Resolve the wavetable a group's fan reads its lane shapes from. Both meta
// oscillators and partial banks carry a frame table; the engine bakes whichever
// one this returns, so the preview must agree on the choice.
bool KapibaraUI::maskGroupWaveFrames(const synth::MaskGroup &g,
                                     const synth::WavetableFrameStorage *&frames, int &count) const
{
        frames = nullptr;
        count = 0;
        if(g.waveSource == 0 || g.waveTrackId == 0)
            return false;
        for(const auto &t : generator_.tracks)
        {
            if(t.id != g.waveTrackId)
                continue;
            if(t.type == synth::SourceTrackType::MetaOscillator)
            {
                frames = &t.metaOsc.frames;
                count = t.metaOsc.frameCount;
            }
            else if(t.type == synth::SourceTrackType::PartialBank)
            {
                frames = &t.partialBank.frames;
                count = t.partialBank.frameCount;
            }
            break;
        }
        // The TRUE frame count, not the lane cap: the caller has to subsample the
        // whole table exactly the way the engine does, which needs the real size.
        count = clampi(count, 0, synth::kMaxWavetableFrames);
        if(frames == nullptr || !frames->data || count <= 0)
        {
            frames = nullptr;
            count = 0;
            return false;
        }
        return true;
    }

// Mirror of SynthCore's lane bake, at preview resolution. Rebuilt only when the
// table's copy-on-write identity changes — sampleFrame() sums up to 1024 sines
// per point, which is far too expensive to run per lane per repaint.
void KapibaraUI::ensureMaskPreviewLut(const synth::MaskGroup &g)
{
        const synth::WavetableFrameStorage *frames = nullptr;
        int count = 0;
        if(!maskGroupWaveFrames(g, frames, count))
        {
            maskPreviewLut_.clear();
            maskPreviewFramesKey_ = nullptr;
            maskPreviewTrackKey_ = 0;
            maskPreviewFrameCount_ = 0;
            return;
        }
        const void *key = static_cast<const void *>(frames->data.get());
        if(key == maskPreviewFramesKey_ && g.waveTrackId == maskPreviewTrackKey_
           && count == maskPreviewFrameCount_)
            return;
        maskPreviewFramesKey_ = key;
        maskPreviewTrackKey_ = g.waveTrackId;
        maskPreviewFrameCount_ = count;
        // Same subsampling AND the same band limit as SynthCore's lane bake — a
        // table with more frames than lanes must show the frames that actually
        // play, spread over the whole table, not just its first 64.
        const int baked = std::min(count, synth::kMaskFanLanes);
        maskPreviewLut_.assign((size_t)baked, {});
        float peak = 0.0f;
        for(int f = 0; f < baked; ++f)
        {
            const int src = baked > 1 ? (f * (count - 1)) / (baked - 1) : 0;
            peak = std::max(peak, synth::bakeModWaveLut((*frames)[(size_t)src],
                                                        maskPreviewLut_[(size_t)f].data(),
                                                        kMaskPreviewLut));
        }
        if(peak <= 1.0e-6f)
        {
            maskPreviewLut_.clear();  // matches the engine's fall back to the MOD curve
            return;
        }
        const float gain = 1.0f / peak;
        for(auto &lut : maskPreviewLut_)
            for(auto &v : lut)
                v *= gain;
    }

int KapibaraUI::maskGroupLaneCount(int gi) const
{
        if(const auto *p = plugin())
            return clampi(p->maskGroupLanes(gi), 2, synth::kMaskFanLanes);
        return synth::kMaskGroupSlots;
    }

// 3D fan preview: the base shape stacked lane-behind-lane with the group's
// successive rate/phase offsets applied — the wavetable-view feel. With a
// wavetable base each lane draws its OWN frame, so the stack really is the
// table morphing across the fan.
void KapibaraUI::drawMaskGroupPreview(const Rect &r, const synth::MaskGroup &g, int lanes)
{
        beginPath();
        roundedRect(r.x, r.y, r.w, r.h, 2.0f);
        fillColor(DesignTokens::appBackground().withAlpha(0.35f));
        fill();

        ensureMaskPreviewLut(g);
        const int waveFrames = int(maskPreviewLut_.size());
        const auto &mp = modSlots_[(size_t)clampi(int(g.baseSlot), 0, synth::kMaxModSlots - 1)];
        // Draw every lane when the fan is small enough to read; beyond that step
        // through it so a 64-partial family still shows its shape.
        const int drawLanes = clampi(lanes, 2, 24);
        // One sample per pixel of lane width (a lane with FREQ SPRD shows more
        // than one cycle, so anything coarser rounds off the curve's corners).
        const int kSteps = clampi(int(r.w), 64, 384);
        const float perspH = std::min(r.h * 0.45f, float(drawLanes) * 6.0f);
        const float waveH = (r.h - perspH - 14.0f) * 0.5f;
        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        for(int L = drawLanes - 1; L >= 0; --L)  // back to front; lane 0 = fan start
        {
            const float x = float(L) / float(drawLanes - 1);
            const float xb = synth::ModMatrix::bend01(x, g.spreadCurve);
            const float base = r.y + r.h - 8.0f - waveH - perspH * x;
            const float amp = waveH * (1.0f - 0.40f * x);
            const float xL = r.x + 8.0f + x * 16.0f;
            const float xR = r.x + r.w - 8.0f - (1.0f - x) * 4.0f;
            const float alpha = 0.30f + 0.70f * (1.0f - x);
            // This lane's frame position in the table (same bend the engine uses).
            const float fpos = xb * float(std::max(0, waveFrames - 1));
            const int fa = clampi(int(fpos), 0, std::max(0, waveFrames - 1));
            const int fb = std::min(fa + 1, std::max(0, waveFrames - 1));
            const float ft = fpos - float(fa);
            // Faint per-lane baseline carries the perspective.
            strokeLine(xL, base, xR, base, DesignTokens::divider().withAlpha(0.5f * alpha), 0.8f);
            beginPath();
            for(int sIdx = 0; sIdx <= kSteps; ++sIdx)
            {
                const float s01 = float(sIdx) / float(kSteps);
                const double ph = double(s01) * (1.0 + double(xb) * double(g.freqSpread))
                                  + double(xb) * double(g.phaseSpread);
                const float t01 = float(ph - std::floor(ph));
                float v;  // bipolar -1..+1
                if(waveFrames > 0)
                {
                    const float pos = t01 * float(kMaskPreviewLut);
                    const int k = std::min(int(pos), kMaskPreviewLut - 1);
                    const float kt = pos - float(k);
                    const auto &la = maskPreviewLut_[(size_t)fa];
                    const auto &lb = maskPreviewLut_[(size_t)fb];
                    const float a = la[(size_t)k] + (la[(size_t)k + 1] - la[(size_t)k]) * kt;
                    const float b = lb[(size_t)k] + (lb[(size_t)k + 1] - lb[(size_t)k]) * kt;
                    v = a + (b - a) * ft;
                }
                else
                {
                    v = synth::pointCurveEval(mp.points.data(), mp.pointCount, t01) * 2.0f - 1.0f;
                }
                const float px = xL + s01 * (xR - xL);
                const float py = base - v * amp;
                if(sIdx == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor((L == 0 ? DesignTokens::accentCyan() : DesignTokens::accentCyan().withAlpha(alpha)));
            strokeWidth(L == 0 ? 1.6f : 1.1f);
            stroke();
        }
        resetScissor();
    }

// MASK GROUPS page: left = fan display + spread params; right = the 16 target
// slots (blacked out while a >16-element family owns the fan).
void KapibaraUI::drawMaskGroups(const Rect &r)
{
        groupSlotHits_.clear();
        selectedMaskGroup_ = clampi(selectedMaskGroup_, 0, synth::kMaxMaskGroups - 1);
        auto &g = maskGroups_[(size_t)selectedMaskGroup_];

        // Header: group chips, ON, base slot.
        for(int i = 0; i < synth::kMaxMaskGroups; ++i)
        {
            groupSelRects_[(size_t)i] = { r.x + float(i) * 34.0f, r.y, 30.0f, 16.0f };
            char gl[8];
            std::snprintf(gl, sizeof(gl), "G%d", i + 1);
            drawButton(groupSelRects_[(size_t)i], gl, selectedMaskGroup_ == i);
            if(maskGroups_[(size_t)i].enabled)
            {
                beginPath();
                circle(groupSelRects_[(size_t)i].x + 25.0f, groupSelRects_[(size_t)i].y + 4.0f, 2.0f);
                fillColor(DesignTokens::accentGreen());
                fill();
            }
        }
        groupEnableRect_ = { r.x + 4.0f * 34.0f + 8.0f, r.y, 36.0f, 16.0f };
        drawButton(groupEnableRect_, "ON", g.enabled);
        // One chip cycles the whole base-shape pool: MOD1..MOD8, then every track
        // that owns a wavetable. A wavetable replaces the lane SHAPES only — the
        // MOD slot still supplies the fan's rate.
        char bl[40];
        const int baseSlot = clampi(int(g.baseSlot), 0, synth::kMaxModSlots - 1);
        if(g.waveSource != 0)
        {
            const int ti = trackIndexOfId(g.waveTrackId);
            std::snprintf(bl, sizeof(bl), "BASE WT:%s",
                          ti >= 0 ? generator_.tracks[(size_t)ti].name.c_str() : "?");
        }
        else
        {
            std::snprintf(bl, sizeof(bl), "BASE MOD%d", baseSlot + 1);
        }
        groupBaseRect_ = { groupEnableRect_.x + 42.0f, r.y, 132.0f, 16.0f };
        drawButton(groupBaseRect_, bl, g.waveSource != 0);

        const int lanes = maskGroupLaneCount(selectedMaskGroup_);
        char ll[24];
        std::snprintf(ll, sizeof(ll), "%d LANES", lanes);
        useMonoFont();
        uiFontSize(8.5f);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(DesignTokens::accentCyan().withAlpha(0.85f));
        text(r.x + r.w - 2.0f, r.y + 8.0f, ll, nullptr);

        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary().withAlpha(0.6f));
        {
            const float hintX = groupBaseRect_.x + groupBaseRect_.w + 10.0f;
            const float hintW = std::max(0.0f, r.x + r.w - 62.0f - hintX);
            scissor(hintX, r.y, hintW, 16.0f);
            text(hintX, r.y + 8.0f,
                 g.waveSource != 0
                     ? "each lane plays its own frame of the table - drag RATE/FREQ/PHASE/CURVE"
                     : "edit the base curve in MODULATORS below - drag RATE/FREQ/PHASE/CURVE",
                 nullptr);
            resetScissor();
        }

        const Rect body { r.x, r.y + 22.0f, r.w, r.h - 22.0f };
        const float leftW = body.w * 0.54f;

        // --- Left: 3D fan preview + spread params ---------------------------
        constexpr float paramH = 18.0f;
        groupPreviewRect_ = { body.x, body.y, leftW - 12.0f, body.h - paramH - 6.0f };
        drawMaskGroupPreview(groupPreviewRect_, g, lanes);
        const float py = body.y + body.h - paramH;
        const float pw = (leftW - 12.0f - 18.0f) / 4.0f;
        const auto paramChip = [&](Rect &rc, float px, const char *label, float value) {
            rc = { px, py, pw, paramH };
            drawPanel(rc, DesignTokens::groove(), DesignTokens::border());
            useUiFont();
            uiFontSize(8.0f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(rc.x + 5.0f, rc.y + rc.h * 0.5f, label, nullptr);
            char vb[16];
            std::snprintf(vb, sizeof(vb), "%+.2f", value);
            useMonoFont();
            uiFontSize(9.0f);
            textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textPrimary());
            text(rc.x + rc.w - 5.0f, rc.y + rc.h * 0.5f, vb, nullptr);
        };
        // RATE is the group's own, not the base MOD slot's — see MaskGroup::rateHz.
        groupRateRect_ = { body.x, py, pw, paramH };
        drawPanel(groupRateRect_, DesignTokens::groove(), DesignTokens::border());
        useUiFont();
        uiFontSize(8.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary());
        text(groupRateRect_.x + 5.0f, groupRateRect_.y + groupRateRect_.h * 0.5f, "RATE", nullptr);
        char rb[16];
        std::snprintf(rb, sizeof(rb), "%.2f Hz", g.rateHz);
        useMonoFont();
        uiFontSize(9.0f);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(DesignTokens::accentCyan());
        text(groupRateRect_.x + groupRateRect_.w - 5.0f, groupRateRect_.y + groupRateRect_.h * 0.5f,
             rb, nullptr);
        paramChip(groupFreqRect_, body.x + pw + 6.0f, "FREQ SPRD", g.freqSpread);
        paramChip(groupPhaseRect_, body.x + (pw + 6.0f) * 2.0f, "PHASE SPRD", g.phaseSpread);
        paramChip(groupCurveRect_, body.x + (pw + 6.0f) * 3.0f, "CURVE", g.spreadCurve);

        // --- Right: family selector + 16 target slots -----------------------
        const float rx = body.x + leftW;
        const float rw = body.x + body.w - rx;
        groupFamilyRect_ = { rx, body.y, 118.0f, 15.0f };
        drawButton(groupFamilyRect_, g.family ? "FAMILY: PARTIALS" : "FAMILY: OFF", g.family != 0);
        groupFamilyDestRect_ = {}; groupFamilyTrackRect_ = {}; groupFamilyDepthRect_ = {};
        if(g.family)
        {
            groupFamilyDestRect_ = { rx + 124.0f, body.y, 74.0f, 15.0f };
            drawButton(groupFamilyDestRect_, destName(g.familyDest), false);
            char tl[40];
            if(g.familyTrackId == 0)
                std::snprintf(tl, sizeof(tl), "@GLOBAL");
            else
            {
                const int ti = trackIndexOfId(g.familyTrackId);
                std::snprintf(tl, sizeof(tl), "@%s",
                              ti >= 0 ? generator_.tracks[(size_t)ti].name.c_str() : "?");
            }
            groupFamilyTrackRect_ = { rx + 202.0f, body.y, 96.0f, 15.0f };
            drawButton(groupFamilyTrackRect_, tl, false);
            groupFamilyDepthRect_ = { rx + 302.0f, body.y, std::max(50.0f, rw - 306.0f), 15.0f };
            drawPanel(groupFamilyDepthRect_, DesignTokens::groove(), DesignTokens::border());
            const float lim = modulationDepthLimit(g.familyDest);
            const float norm = clampf(g.familyDepth / std::max(1.0e-6f, lim), -1.0f, 1.0f);
            const float cx0 = groupFamilyDepthRect_.x + groupFamilyDepthRect_.w * 0.5f;
            beginPath();
            rect(std::min(cx0, cx0 + norm * groupFamilyDepthRect_.w * 0.5f),
                 groupFamilyDepthRect_.y + 2.0f,
                 std::abs(norm) * groupFamilyDepthRect_.w * 0.5f, groupFamilyDepthRect_.h - 4.0f);
            fillColor(DesignTokens::accentCyan().withAlpha(0.7f));
            fill();
            char dv[16];
            std::snprintf(dv, sizeof(dv), "%+.2f", g.familyDepth);
            useMonoFont();
            uiFontSize(8.5f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(DesignTokens::textPrimary());
            text(cx0, groupFamilyDepthRect_.y + groupFamilyDepthRect_.h * 0.5f, dv, nullptr);
        }

        // 16 slots in two 8-row columns. Blacked out while a family owns the fan
        // ("已经选了映射组": lanes are spent on >16 family elements).
        const float slotsY = body.y + 20.0f;
        const float slotH = std::max(13.0f, std::min(16.0f, (body.h - 24.0f) / 8.0f - 2.0f));
        const float colW = (rw - 8.0f) / 2.0f;
        for(int k = 0; k < synth::kMaskGroupSlots; ++k)
        {
            const int col = k / 8, row = k % 8;
            const Rect sr { rx + float(col) * (colW + 8.0f), slotsY + float(row) * (slotH + 2.0f),
                            colW, slotH };
            const auto &t = g.targets[(size_t)k];
            if(g.family)
            {
                // Disabled: the family owns the fan.
                drawPanel(sr, rgba(0x07090bff), rgba(0x1a2228ff));
                useMonoFont();
                uiFontSize(7.5f);
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                fillColor(DesignTokens::textSecondary().withAlpha(0.25f));
                char sl[8];
                std::snprintf(sl, sizeof(sl), "%02d", k + 1);
                text(sr.x + 4.0f, sr.y + sr.h * 0.5f, sl, nullptr);
                continue;
            }
            if(!t.enabled)
            {
                drawPanel(sr, rgba(0x0c1318ff), rgba(0x24313aff));
                useUiFont();
                uiFontSize(8.0f);
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                fillColor(DesignTokens::textSecondary().withAlpha(0.55f));
                char sl[16];
                std::snprintf(sl, sizeof(sl), "%02d  +", k + 1);
                text(sr.x + 4.0f, sr.y + sr.h * 0.5f, sl, nullptr);
                groupSlotHits_.push_back(MatrixCardHit { sr, k, MatrixCardHit::Dest });
                continue;
            }
            drawPanel(sr, rgba(0x101820ff), rgba(0x33495aff));
            // [dest] [@trk] [depth]
            const Rect destR { sr.x, sr.y, sr.w * 0.40f, sr.h };
            const Rect trkR { sr.x + sr.w * 0.40f, sr.y, sr.w * 0.28f, sr.h };
            const Rect depR { sr.x + sr.w * 0.68f, sr.y, sr.w * 0.32f, sr.h };
            useUiFont();
            uiFontSize(7.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textPrimary());
            scissor(destR.x, destR.y, destR.w - 2.0f, destR.h);
            char dl[24];
            std::snprintf(dl, sizeof(dl), "%02d %s", k + 1, destName(t.dest));
            text(destR.x + 3.0f, destR.y + destR.h * 0.5f, dl, nullptr);
            resetScissor();
            fillColor(DesignTokens::textSecondary());
            scissor(trkR.x, trkR.y, trkR.w - 2.0f, trkR.h);
            char tl[32];
            if(t.targetTrackId == 0)
                std::snprintf(tl, sizeof(tl), "@GLB");
            else
            {
                const int ti = trackIndexOfId(t.targetTrackId);
                std::snprintf(tl, sizeof(tl), "@%s",
                              ti >= 0 ? generator_.tracks[(size_t)ti].name.c_str() : "?");
            }
            text(trkR.x + 2.0f, trkR.y + trkR.h * 0.5f, tl, nullptr);
            resetScissor();
            const float lim = modulationDepthLimit(t.dest);
            const float norm = clampf(t.depth / std::max(1.0e-6f, lim), -1.0f, 1.0f);
            beginPath();
            rect(depR.x + 1.0f, depR.y + sr.h * 0.5f - 2.0f, (depR.w - 2.0f), 4.0f);
            fillColor(DesignTokens::groove());
            fill();
            const float cx0 = depR.x + depR.w * 0.5f;
            beginPath();
            rect(std::min(cx0, cx0 + norm * depR.w * 0.5f), depR.y + sr.h * 0.5f - 2.0f,
                 std::abs(norm) * depR.w * 0.5f, 4.0f);
            fillColor(t.depth >= 0.0f ? DesignTokens::accentCyan() : DesignTokens::accentGreen());
            fill();
            groupSlotHits_.push_back(MatrixCardHit { destR, k, MatrixCardHit::Dest });
            groupSlotHits_.push_back(MatrixCardHit { trkR, k, MatrixCardHit::Target });
            groupSlotHits_.push_back(MatrixCardHit { depR, k, MatrixCardHit::Depth });
            groupSlotHits_.push_back(MatrixCardHit { sr, k, MatrixCardHit::Row });
        }
    }

bool KapibaraUI::handleMaskGroupsPress(float x, float y)
{
        selectedMaskGroup_ = clampi(selectedMaskGroup_, 0, synth::kMaxMaskGroups - 1);
        auto &g = maskGroups_[(size_t)selectedMaskGroup_];
        for(int i = 0; i < synth::kMaxMaskGroups; ++i)
            if(groupSelRects_[(size_t)i].w > 0.0f && groupSelRects_[(size_t)i].contains(x, y))
            {
                selectedMaskGroup_ = i;
                repaint();
                return true;
            }
        if(groupEnableRect_.w > 0.0f && groupEnableRect_.contains(x, y))
        {
            g.enabled = !g.enabled;
            pushGroup(selectedMaskGroup_);
            repaint();
            return true;
        }
        if(groupBaseRect_.w > 0.0f && groupBaseRect_.contains(x, y))
        {
            // Cycle MOD1..MOD8 -> every wavetable-owning track -> back to MOD1.
            const auto hasTable = [&](const synth::SourceTrackParams &t) {
                return (t.type == synth::SourceTrackType::MetaOscillator
                        && t.metaOsc.frameCount > 0)
                       || (t.type == synth::SourceTrackType::PartialBank
                           && t.partialBank.frameCount > 0);
            };
            const int n = int(generator_.tracks.size());
            const auto nextTableTrack = [&](int from) {
                for(int i = from; i < n; ++i)
                    if(hasTable(generator_.tracks[(size_t)i]))
                        return i;
                return -1;
            };
            if(g.waveSource == 0)
            {
                const int next = clampi(int(g.baseSlot), 0, synth::kMaxModSlots - 1) + 1;
                if(next < synth::kMaxModSlots)
                {
                    g.baseSlot = int8_t(next);
                    selectedMatrixModSlot_ = int(g.baseSlot);  // show it in MODULATORS below
                }
                else
                {
                    const int ti = nextTableTrack(0);
                    if(ti >= 0)
                    {
                        g.waveSource = 1;
                        g.waveTrackId = generator_.tracks[(size_t)ti].id;
                    }
                    else
                    {
                        g.baseSlot = 0;
                        selectedMatrixModSlot_ = 0;
                    }
                }
            }
            else
            {
                const int cur = trackIndexOfId(g.waveTrackId);
                const int ti = nextTableTrack(cur < 0 ? 0 : cur + 1);
                if(ti >= 0)
                {
                    g.waveTrackId = generator_.tracks[(size_t)ti].id;
                }
                else
                {
                    g.waveSource = 0;
                    g.waveTrackId = 0;
                    g.baseSlot = 0;
                    selectedMatrixModSlot_ = 0;
                }
            }
            pushGroup(selectedMaskGroup_);
            repaint();
            return true;
        }
        const auto startDrag = [&](DragTarget tgt, float startVal) {
            dragTarget_ = tgt;
            dragStartY_ = y;
            dragStartDepth_ = startVal;
            return true;
        };
        if(groupRateRect_.w > 0.0f && groupRateRect_.contains(x, y))
            return startDrag(DragTarget::GroupRate, std::max(0.01f, g.rateHz));
        if(groupFreqRect_.w > 0.0f && groupFreqRect_.contains(x, y))
            return startDrag(DragTarget::GroupFreqSpread, g.freqSpread);
        if(groupPhaseRect_.w > 0.0f && groupPhaseRect_.contains(x, y))
            return startDrag(DragTarget::GroupPhaseSpread, g.phaseSpread);
        if(groupCurveRect_.w > 0.0f && groupCurveRect_.contains(x, y))
            return startDrag(DragTarget::GroupSpreadCurve, g.spreadCurve);
        if(groupFamilyRect_.w > 0.0f && groupFamilyRect_.contains(x, y))
        {
            g.family = g.family ? 0 : 1;
            if(g.family && g.familyTrackId == 0)
                if(const auto *t = currentTrack()) g.familyTrackId = t->id;
            pushGroup(selectedMaskGroup_);
            repaint();
            return true;
        }
        if(groupFamilyDestRect_.w > 0.0f && groupFamilyDestRect_.contains(x, y))
        {
            // Cycle through the card dest pool.
            constexpr int poolN = int(sizeof(kCardDestPool) / sizeof(kCardDestPool[0]));
            int cur = 0;
            for(int i = 0; i < poolN; ++i)
                if(kCardDestPool[i] == g.familyDest) { cur = i; break; }
            g.familyDest = kCardDestPool[(cur + 1) % poolN];
            pushGroup(selectedMaskGroup_);
            repaint();
            return true;
        }
        if(groupFamilyTrackRect_.w > 0.0f && groupFamilyTrackRect_.contains(x, y))
        {
            const int n = int(generator_.tracks.size());
            int ti = g.familyTrackId == 0 ? -1 : trackIndexOfId(g.familyTrackId);
            ++ti;
            g.familyTrackId = (ti < 0 || ti >= n) ? 0u : generator_.tracks[(size_t)ti].id;
            pushGroup(selectedMaskGroup_);
            repaint();
            return true;
        }
        if(groupFamilyDepthRect_.w > 0.0f && groupFamilyDepthRect_.contains(x, y))
        {
            dragDepthLimit_ = modulationDepthLimit(g.familyDest);
            return startDrag(DragTarget::GroupFamilyDepth, g.familyDepth);
        }
        for(const auto &h : groupSlotHits_)
        {
            if(!h.rect.contains(x, y))
                continue;
            if(h.rule < 0 || h.rule >= synth::kMaskGroupSlots)
                return true;
            auto &t = g.targets[(size_t)h.rule];
            switch(h.kind)
            {
                case MatrixCardHit::Dest:
                    gridPickerMode_ = 5;
                    gridPickerRuleIdx_ = h.rule;
                    gridPickerX_ = x;
                    gridPickerY_ = y + 12.0f;
                    break;
                case MatrixCardHit::Target:
                {
                    const int n = int(generator_.tracks.size());
                    int ti = t.targetTrackId == 0 ? -1 : trackIndexOfId(t.targetTrackId);
                    ++ti;
                    t.targetTrackId = (ti < 0 || ti >= n) ? 0u : generator_.tracks[(size_t)ti].id;
                    pushGroup(selectedMaskGroup_);
                    break;
                }
                case MatrixCardHit::Depth:
                    groupDragSlot_ = h.rule;
                    dragDepthLimit_ = modulationDepthLimit(t.dest);
                    return startDrag(DragTarget::GroupSlotDepth, t.depth);
                case MatrixCardHit::Row:
                default:
                    break;
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
        // Modes: 3 = card source, 4 = card dest (full pools).
        const bool srcMode = gridPickerMode_ == 3;
        const int poolN = srcMode ? int(sizeof(kCardSourcePool) / sizeof(kCardSourcePool[0]))
                                  : int(sizeof(kCardDestPool) / sizeof(kCardDestPool[0]));
        std::vector<int> avail;
        for(int i = 0; i < poolN; ++i)
            avail.push_back(i);
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
            const char *nm = srcMode ? sourceName(kCardSourcePool[avail[k]])
                                     : destName(kCardDestPool[avail[k]]);
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

END_NAMESPACE_DISTRHO
