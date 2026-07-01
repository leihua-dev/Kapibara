#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawFxRackEditor(const Rect &r)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "FX RACK (current chain)");
        fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
        routeFxChainTrackId_ = -1; // hits carry their own trackId
        routeFxChainMerge_ = -1;
        // FX in this source's chain, in order — each may live on a different track.
        const int oc = int(selectedChainInserts_.size());
        if(oc == 0)
        {
            useUiFont(); uiFontSize(8.0f); fillColor(DesignTokens::textSecondary());
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, "no FX in this chain", nullptr);
            return;
        }
        const float top = r.y + 28.0f;
        const float rowH = 72.0f;
        const float panelW = r.w - 20.0f;
        float y = top;
        fxRackPanelRects_.assign((size_t)oc, Rect {});
        for(int k = 0; k < oc; ++k)
        {
            const uint32_t tid = selectedChainInserts_[(size_t)k].first;
            const int idx = selectedChainInserts_[(size_t)k].second;
            auto *chain = insertChainFor(int(tid), -1);
            if(chain == nullptr || idx < 0 || idx >= int(chain->size())) continue;
            const Rect p { r.x + 10.0f, y, panelW, rowH };
            fxRackPanelRects_[(size_t)k] = p;
            drawInsertPanel(p, (*chain)[(size_t)idx], int(tid), -1, idx);
            const bool isDragSrc = (fxRackDragActive_ && fxRackDragFrom_ == k);
            const bool isDropTgt = (fxRackDragActive_ && fxRackDragFrom_ != k && p.contains(fxRackDragX_, fxRackDragY_));
            if(isDragSrc) { beginPath(); rect(p.x, p.y, p.w, p.h); fillColor(rgba(0x0b121799)); fill(); }
            if(isDropTgt) { beginPath(); roundedRect(p.x, p.y, p.w, p.h, 3.0f); strokeColor(rgba(0x9eff50ffU)); strokeWidth(2.0f); stroke(); }
            y += rowH + 6.0f;
            if(y > r.y + r.h - 18.0f) break;
        }
        // Drag ghost following the cursor.
        if(fxRackDragActive_ && fxRackDragFrom_ >= 0 && fxRackDragFrom_ < oc)
        {
            const uint32_t tid = selectedChainInserts_[(size_t)fxRackDragFrom_].first;
            const int idx = selectedChainInserts_[(size_t)fxRackDragFrom_].second;
            auto *chain = insertChainFor(int(tid), -1);
            if(chain && idx >= 0 && idx < int(chain->size()))
            {
                const Rect ghost { r.x + 10.0f, fxRackDragY_ - rowH * 0.5f, panelW, rowH };
                beginPath(); roundedRect(ghost.x, ghost.y, ghost.w, ghost.h, 4.0f);
                fillColor(rgba(0x17242cdd)); fill();
                strokeColor(rgba(0x9eff50ffU)); strokeWidth(1.6f); stroke();
                useUiFont(); uiFontSize(9.0f); fillColor(rgba(0xc8d6dcff));
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                text(ghost.x + 8.0f, ghost.y + rowH * 0.5f, insertTypeName((*chain)[(size_t)idx].kind), nullptr);
            }
        }
    }

bool KapibaraUI::handleFxRackPress(float x, float y)
{
        for(size_t i = 0; i < fxRackPanelRects_.size(); ++i)
            if(fxRackPanelRects_[i].w > 0.0f && fxRackPanelRects_[i].contains(x, y))
            {
                // Let the panel's knobs drag-edit; only the panel body starts a swap.
                for(const auto &h : fxKnobHits_) if(h.rect.contains(x, y)) return false;
                fxRackDragPending_ = true;
                fxRackDragActive_ = false;
                fxRackDragFrom_ = int(i);
                fxRackDragStartX_ = fxRackDragX_ = x;
                fxRackDragStartY_ = fxRackDragY_ = y;
                return true;
            }
        return false;
    }

void KapibaraUI::finishFxRackDrag(float x, float y)
{
        const bool wasDrag = fxRackDragActive_;
        const int from = fxRackDragFrom_;
        fxRackDragPending_ = false;
        fxRackDragActive_ = false;
        fxRackDragFrom_ = -1;
        if(!wasDrag || from < 0 || from >= int(selectedChainInserts_.size()))
            return;
        int to = -1;
        for(size_t i = 0; i < fxRackPanelRects_.size(); ++i)
            if(fxRackPanelRects_[i].w > 0.0f && fxRackPanelRects_[i].contains(x, y)) { to = int(i); break; }
        if(to < 0 || to == from || to >= int(selectedChainInserts_.size()))
            return;
        // Swap only the effect CONTENT between the two chain positions (wiring stays;
        // the router nodes display whatever insert lives at their slot).
        const uint32_t tidA = selectedChainInserts_[(size_t)from].first;
        const int idxA = selectedChainInserts_[(size_t)from].second;
        const uint32_t tidB = selectedChainInserts_[(size_t)to].first;
        const int idxB = selectedChainInserts_[(size_t)to].second;
        auto *cA = insertChainFor(int(tidA), -1);
        auto *cB = insertChainFor(int(tidB), -1);
        if(cA == nullptr || cB == nullptr) return;
        if(idxA < 0 || idxA >= int(cA->size()) || idxB < 0 || idxB >= int(cB->size())) return;
        std::swap((*cA)[(size_t)idxA], (*cB)[(size_t)idxB]);
        commitChainChange(int(tidA), -1);
        if(tidB != tidA) commitChainChange(int(tidB), -1);
    }

void KapibaraUI::drawMultibandFxEditor(const Rect &r)
{
        fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
        multibandBandAddRects_.fill({});
        multibandBandMuteRects_.fill({});
        multibandBandSoloRects_.fill({});
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        auto *outer = trackInsertsFor(uint32_t(multibandEditorTrackId_));
        if(outer == nullptr || multibandEditorInsertIdx_ < 0 || multibandEditorInsertIdx_ >= int(outer->size()))
            return;
        auto &owner = (*outer)[(size_t)multibandEditorInsertIdx_];
        auto &mb = ensureMultibandParams(owner);

        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "XOVER FX");
        const Rect lowSlider { r.x + r.w * 0.33f - 42.0f, r.y + 11.0f, 70.0f, 14.0f };
        const Rect highSlider { r.x + r.w * 0.66f - 42.0f, r.y + 11.0f, 70.0f, 14.0f };
        drawButton(lowSlider, "LOW/MID", false);
        drawButton(highSlider, "MID/HIGH", false);
        fxKnobHits_.push_back(FxKnobHit { lowSlider, int(multibandEditorTrackId_), -1, multibandEditorInsertIdx_, 0 });
        fxKnobHits_.push_back(FxKnobHit { highSlider, int(multibandEditorTrackId_), -1, multibandEditorInsertIdx_, 1 });
        useUiFont();
        uiFontSize(8.0f);
        fillColor(DesignTokens::textSecondary());
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        char hz[32];
        std::snprintf(hz, sizeof(hz), "%.0f Hz", mb.lowXoverHz);
        text(lowSlider.x + lowSlider.w * 0.5f, lowSlider.y + 16.0f, hz, nullptr);
        std::snprintf(hz, sizeof(hz), "%.0f Hz", mb.highXoverHz);
        text(highSlider.x + highSlider.w * 0.5f, highSlider.y + 16.0f, hz, nullptr);

        static const char *bandNames[3] = { "LOW", "MID", "HIGH" };
        const float gap = 10.0f;
        const float top = r.y + 42.0f;
        const float bandW = (r.w - gap * 4.0f) / 3.0f;
        for(int band = 0; band < 3; ++band)
        {
            const Rect pane { r.x + gap + float(band) * (bandW + gap), top, bandW, r.y + r.h - top - 10.0f };
            drawPanel(pane, rgba(0x10171bff), rgba(0x293842ff));
            drawSectionTitle(pane.x + 10.0f, pane.y + 8.0f, bandNames[band]);
            const Rect muteR { pane.x + pane.w - 48.0f, pane.y + 8.0f, 18.0f, 14.0f };
            const Rect soloR { pane.x + pane.w - 26.0f, pane.y + 8.0f, 18.0f, 14.0f };
            drawButton(muteR, "M", mb.bandMute[band]);
            drawButton(soloR, "S", mb.bandSolo[band]);
            multibandBandMuteRects_[(size_t)band] = muteR;
            multibandBandSoloRects_[(size_t)band] = soloR;
            float rowY = pane.y + 30.0f;
            auto &chain = mb.bands[band];
            for(int i = 0; i < int(chain.size()); ++i)
            {
                const Rect row { pane.x + 8.0f, rowY, pane.w - 16.0f, 62.0f };
                drawInsertPanel(row, chain[(size_t)i], -2, band, i);
                rowY += 68.0f;
                if(rowY > pane.y + pane.h - 34.0f) break;
            }
            const Rect add { pane.x + 8.0f, pane.y + pane.h - 28.0f, std::min(92.0f, pane.w - 16.0f), 18.0f };
            drawButton(add, "+ FX", false);
            multibandBandAddRects_[(size_t)band] = add;
            insertHits_.push_back(InsertHit { add, -2, band, -1 });
        }
    }

END_NAMESPACE_DISTRHO
