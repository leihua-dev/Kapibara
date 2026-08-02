#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::focusedNodeValid() const
{
        const uint32_t id = focusedNodeId_;
        if(id == 0) return false;
        if((id & 0xff000000u) == 0x08000000u)
        {
            const uint32_t tid = id & 0x00ffffffu;
            for(const auto &t : generator_.tracks) if(t.id == tid) return true;
            return false;
        }
        if((id & 0xf0000000u) == 0x10000000u)
        {
            const int local = int(id & 0xfu);
            return local > 0 && local <= globalPerVoiceFilterCount();
        }
        if((id & 0xf0000000u) == 0x30000000u)
        {
            const int slot = (int((id >> 8) & 0xffu) > 0) ? int((id >> 8) & 0xffu) - 1 : int(id & 0xfu) - 1;
            return slot >= 0 && slot < synth::kMaxAmpEnvs;
        }
        if((id & 0xf0000000u) == 0x20000000u && id != 0x2fffffffu)
        {
            const uint32_t tid = (id & 0x0fffff00u) >> 8u;
            const int local = int(id & 0xffu);
            for(const auto &t : generator_.tracks)
                if(t.id == tid) return local > 0 && local <= int(t.inserts.size());
            return false;
        }
        return false;
    }

bool KapibaraUI::focusedInsertIsFilter()
{
        const uint32_t id = focusedNodeId_;
        if((id & 0xf0000000u) != 0x20000000u || id == 0x2fffffffu)
            return false;
        const int tid = int((id & 0x0fffff00u) >> 8u);
        const int idx = int(id & 0xffu) - 1;
        auto *chain = insertChainFor(tid, -1);
        return chain != nullptr && idx >= 0 && idx < int(chain->size())
               && (*chain)[(size_t)idx].kind == InsertFilter;
    }

void KapibaraUI::drawFocusedNodeDetail(const Rect &r)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        focusedCloseRect_ = { r.x + r.w - 26.0f, r.y + 8.0f, 18.0f, 16.0f };
        drawButton(focusedCloseRect_, "x", false);
        const bool isSourceNode = (focusedNodeId_ & 0xff000000u) == 0x08000000u;
        if(!isSourceNode && focusPage_ == 2)
            focusPage_ = 0;
        // A focused filter has no generic "detail" — it has a CONTROL page and a
        // SLOTS page, and those are top-level, not tabs nested inside a tab.
        const bool isFilterFx = (focusedNodeId_ & 0xf0000000u) == 0x20000000u
                                && focusedInsertIsFilter();
        const float tabW = 78.0f, tabH = 16.0f;
        focusPageTabRects_[0] = { r.x + 10.0f, r.y + 8.0f, tabW, tabH };
        focusPageTabRects_[1] = { focusPageTabRects_[0].x + tabW + 4.0f, r.y + 8.0f, tabW, tabH };
        focusPageTabRects_[2] = {};
        if(isFilterFx)
        {
            drawButton(focusPageTabRects_[0], "CONTROL", focusPage_ == 0 && disperserPage_ == 0);
            drawButton(focusPageTabRects_[1], "SLOTS", focusPage_ == 0 && disperserPage_ == 1);
            focusPageTabRects_[2] = { focusPageTabRects_[1].x + tabW + 4.0f, r.y + 8.0f, tabW, tabH };
            drawButton(focusPageTabRects_[2], "STRUCTURE", focusPage_ == 1);
        }
        else
        {
            drawButton(focusPageTabRects_[0], "DETAIL", focusPage_ == 0);
            drawButton(focusPageTabRects_[1], "STRUCTURE", focusPage_ == 1);
            if(isSourceNode)
            {
                focusPageTabRects_[2] = { focusPageTabRects_[1].x + tabW + 4.0f, r.y + 8.0f, tabW, tabH };
                drawButton(focusPageTabRects_[2], "OSC MOD", focusPage_ == 2);
            }
        }
        ampFocusKnobRects_.fill({});
        clearTrackEditorRects();
        pvChainTabRects_.fill({}); pvChainEnableRect_ = {}; pvChainTypeRect_ = {};
        pvChainKnobRects_.fill({}); pvChainAddRect_ = {}; pvChainCount_ = 0;
        fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
        fxRackPanelRects_.clear();
        clearDisperserRects();

        const uint32_t id = focusedNodeId_;
        const Rect content { r.x + 10.0f, r.y + 30.0f, r.w - 20.0f, r.h - 40.0f };
        if(focusPage_ == 1)
        {
            drawFocusedStructure(content);
            return;
        }
        if(focusPage_ == 2 && isSourceNode)
        {
            drawSectionTitle(r.x + 268.0f, r.y + 10.0f, "OSC MOD SYSTEM");
            drawOscModDiagram(content);
            return;
        }
        structAddOutRect_ = {}; structRemoveOutRect_ = {}; structAddUtilRect_ = {};
        if((id & 0xff000000u) == 0x08000000u)
        {
            drawSectionTitle(r.x + 184.0f, r.y + 10.0f, "SOURCE DETAIL");
            drawTrackEditor(content);
        }
        else if((id & 0xf0000000u) == 0x10000000u)
        {
            drawSectionTitle(r.x + 184.0f, r.y + 10.0f, "FILTER DETAIL");
            drawPerVoiceChainEditor(content);
        }
        else if((id & 0xf0000000u) == 0x30000000u)
        {
            const int e = clampi((int((id >> 8) & 0xffu) > 0) ? int((id >> 8) & 0xffu) - 1 : int(id & 0xfu) - 1, 0, synth::kMaxAmpEnvs - 1);
            selectedAmpEnv_ = e;
            char title[24]; std::snprintf(title, sizeof title, "AMP ENV %d", e + 1);
            drawSectionTitle(r.x + 184.0f, r.y + 10.0f, title);
            auto &ae = ampEnvs_[(size_t)e];
            const float kw = (content.w - 18.0f) / 4.0f;
            const float ky = content.y + 14.0f, kh = std::min(96.0f, content.h - 24.0f);
            const float norms[4] = { ae.attack / 5.0f, ae.decay / 5.0f, ae.sustain, ae.release / 8.0f };
            const float disps[4] = { ae.attack, ae.decay, ae.sustain, ae.release };
            static const char *const knames[4] = { "Attack", "Decay", "Sustain", "Release" };
            for(int k = 0; k < 4; ++k)
            {
                const Rect kr { content.x + float(k) * kw, ky, kw - 6.0f, kh };
                ampFocusKnobRects_[(size_t)k] = kr;
                drawKnob(kr, knames[k], norms[k], disps[k]);
            }
        }
        else if((id & 0xf0000000u) == 0x20000000u)
        {
            drawSectionTitle(r.x + 268.0f, r.y + 10.0f, "FX DETAIL");
            const uint32_t tid = (id & 0x0fffff00u) >> 8u;
            const int idx = int(id & 0xffu) - 1;
            auto *chain = insertChainFor(int(tid), -1);
            if(chain != nullptr && idx >= 0 && idx < int(chain->size()))
            {
                fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
                routeFxChainTrackId_ = -1; routeFxChainMerge_ = -1;
                fxPanelHideDelete_ = true;
                fxPanelFocused_ = true;
                drawInsertPanel(content, (*chain)[(size_t)idx], int(tid), -1, idx);
                fxPanelFocused_ = false;
                fxPanelHideDelete_ = false;
            }
        }
    }

bool KapibaraUI::handleFocusedDetailPress(float x, float y)
{
        if(focusedNodeId_ == 0)
            return false;
        if(routeBoardRect_.w > 0.0f && routeBoardRect_.contains(x, y))
            return false;
        if(focusedCloseRect_.contains(x, y)) { focusedNodeId_ = 0; repaint(); return true; }
        const bool isFilterFx = (focusedNodeId_ & 0xf0000000u) == 0x20000000u
                                && focusedInsertIsFilter();
        for(int i = 0; i < 3; ++i)
            if(focusPageTabRects_[(size_t)i].w > 0.0f && focusPageTabRects_[(size_t)i].contains(x, y))
            {
                // CONTROL and SLOTS are both the detail page; they differ only in
                // which half of the filter editor is showing.
                if(isFilterFx)
                {
                    focusPage_ = (i == 2) ? 1 : 0;
                    if(i < 2) disperserPage_ = i;
                }
                else
                {
                    focusPage_ = i;
                }
                repaint();
                return true;
            }
        // OSC MOD diagram page: click a mode chip → mode / remove menu.
        if(focusPage_ == 2 && (focusedNodeId_ & 0xff000000u) == 0x08000000u)
        {
            for(const auto &hit : oscModDiagHits_)
                if(hit.rect.contains(x, y) && hit.track >= 0
                   && hit.track < int(generator_.tracks.size()))
                {
                    selectedTrack_ = hit.track;
                    selectedModSlot_ = hit.slot;
                    openOscModTypeMenu(int(generator_.tracks[(size_t)hit.track].id), hit.slot, x, y);
                    repaint();
                    return true;
                }
        }
        if(focusPage_ == 1)
        {
            if(structAddOutRect_.w > 0.0f && structAddOutRect_.contains(x, y))
            {
                nodeOutPortCount_[focusedNodeId_] = std::min(8, componentOutPortCount(focusedNodeId_) + 1);
                repaint();
                return true;
            }
            if(structRemoveOutRect_.w > 0.0f && structRemoveOutRect_.contains(x, y))
            {
                const int n = componentOutPortCount(focusedNodeId_);
                if(n > 1)
                {
                    routeWires_.erase(std::remove_if(routeWires_.begin(), routeWires_.end(),
                                                     [&](const synth::GridWire &w) {
                                                         return w.from.nodeId == focusedNodeId_ && int(w.from.port) >= n;
                                                     }),
                                      routeWires_.end());
                    nodeOutPortCount_[focusedNodeId_] = n - 1;
                    auto &sw = structWires_[focusedNodeId_];
                    sw.erase(std::remove_if(sw.begin(), sw.end(),
                                            [&](const synth::GridWire &w) {
                                                return int(w.to.nodeId) == 10 + n || int(w.from.nodeId) == 10 + n;
                                            }),
                             sw.end());
                    rebuildSelectedPerVoiceRouteFromWires();
                }
                repaint();
                return true;
            }
            if(structAddUtilRect_.w > 0.0f && structAddUtilRect_.contains(x, y))
            {
                structUtilCount_[focusedNodeId_] = std::min(6, (structUtilCount_.count(focusedNodeId_) ? structUtilCount_[focusedNodeId_] : 0) + 1);
                repaint();
                return true;
            }
            if(handleStructurePress(x, y))
                return true;
            return true;
        }
        const uint32_t id = focusedNodeId_;
        // Disperser stage lanes: they exist only while this pane draws them, and
        // claiming the press here keeps it away from the generic control chain.
        if((id & 0xf0000000u) == 0x20000000u && handleDisperserEditorPress(x, y))
            return true;
        if((id & 0xf0000000u) == 0x30000000u)
        {
            static const DragTarget tg[4] = { DragTarget::Attack, DragTarget::Decay,
                                              DragTarget::Sustain, DragTarget::Release };
            const int e = clampi((int((id >> 8) & 0xffu) > 0) ? int((id >> 8) & 0xffu) - 1 : int(id & 0xfu) - 1, 0, synth::kMaxAmpEnvs - 1);
            auto &ae = ampEnvs_[(size_t)e];
            const float cur[4] = { ae.attack / 5.0f, ae.decay / 5.0f, ae.sustain, ae.release / 8.0f };
            for(int k = 0; k < 4; ++k)
                if(ampFocusKnobRects_[(size_t)k].contains(x, y))
                {
                    selectedAmpEnv_ = e;
                    dragTarget_ = tg[k];
                    dragStartY_ = y;
                    dragStartNorm_ = clampf(cur[k], 0.0f, 1.0f);
                    return true;
                }
        }
        return false;
    }

END_NAMESPACE_DISTRHO
