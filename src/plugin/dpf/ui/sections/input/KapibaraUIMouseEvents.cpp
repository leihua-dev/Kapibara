#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::onMouse(const MouseEvent &ev)
{
        const float x = (static_cast<float>(ev.pos.getX()) - lbX_) / uiRenderScale_;
        const float y = (static_cast<float>(ev.pos.getY()) - lbY_) / uiRenderScale_;

        if(!ev.press)
        {
            if(segmentDragPending_)
            {
                // Released without moving enough to start drag: just keep the selection.
                segmentDragPending_ = false;
                repaint();
                return true;
            }
            if(segmentDragActive_)
            {
                segmentDragActive_ = false;
                // Section has moved: clear the selection so the user re-picks.
                selectedSectionValid_ = false;
                repaint();
                return true;
            }
            if(insertPending_)
            {
                finishInsertInteraction(x, y);
                repaint();
                return true;
            }
            if(fxRackDragPending_)
            {
                finishFxRackDrag(x, y);
                repaint();
                return true;
            }
            if(structUtilParamDrag_ >= 0)
            {
                structUtilParamDrag_ = -1;
                repaint();
                return true;
            }
            if(structDragLocal_ >= 0)
            {
                finishStructureNodeDrag(x, y);
                repaint();
                return true;
            }
            if(modRouteDragActive_)
            {
                if(modRouteDragMoved_)
                    finishModRouteDrag(x, y);
                modRouteDragActive_ = false;
                modRouteDragMoved_ = false;
                modRouteHover_ = {};
                repaint();
                return true;
            }
            releaseMouseKey();
            if(metaEditorDirty_)
            {
                metaEditorDirty_ = false;
                pushCurrentTrack();
            }
            // Env-curve edits are published once on release (drag stays smooth — the
            // per-motion full-snapshot publish was the source of the lag).
            if((dragTarget_ == DragTarget::MatrixEnvCurve || dragTarget_ == DragTarget::MatrixEnvSeg)
               && matrixEnvDirty_)
            {
                matrixEnvDirty_ = false;
                pushCurCurve();
            }
            // Flush the exact final value. During drag, PartialBank Partials and
            // Inharmonic are already pushed at UI-frame cadence for live notes.
            if(deferTrackPush_) { deferTrackPush_ = false; pushCurrentTrackDuringRealtimeDrag(true); }
            if(deferGenPush_)   { deferGenPush_ = false; pushGeneratorDuringRealtimeDrag(true); }
            const bool wasNodeDrag = routeNodeDragActive_;
            const uint32_t draggedNodeId = routeNodeDragId_;
            routeNodeDragActive_ = false;
            routeNodeDragId_ = 0;
            dragTarget_ = DragTarget::None;
            dragTrackIndex_ = -1;
            prevTimeEditX_ = -1.0f;
            prevTimeEditY_ = -1.0f;
            if(wasNodeDrag && draggedNodeId != 0)
                tryMergeNodeIntoWire(draggedNodeId);
            return true;
        }

        if(ev.button == kMouseButtonRight && harmonicEditorOpen_
           && currentTrack() != nullptr
           && currentTrack()->type == synth::SourceTrackType::MetaOscillator)
        {
            openMetaProcessContextMenu(x, y);
            repaint();
            return true;
        }
        if(ev.button == kMouseButtonRight && metaWarpModeRect_.contains(x, y))
        {
            auto *track = currentTrack();
            const uint32_t tid = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                                      ? track->id : 0u;
            openWarpModeMenu(tid, x, y);
            repaint();
            return true;
        }
        if(ev.button == kMouseButtonRight)
        {
            // Mod-wire dot in the SOURCE column → mode / remove menu.
            if(handleOscModDotRightClick(x, y))
            {
                repaint();
                return true;
            }
            if(openRouteNodeContext(x, y))
            {
                repaint();
                return true;
            }
            // Right-click a matrix grid node → clear that route.
            if(handleMatrixGridDelete(x, y))
            {
                repaint();
                return true;
            }
            // Right-click a grid axis label → remove that source/destination row/col.
            for(size_t s = 0; s < gridSrcLabelRects_.size() && s < gridSources_.size(); ++s)
                if(gridSrcLabelRects_[s].contains(x, y))
                {
                    gridSources_.erase(gridSources_.begin() + long(s));
                    repaint();
                    return true;
                }
            for(size_t d = 0; d < gridDestLabelRects_.size() && d < gridDests_.size(); ++d)
                if(gridDestLabelRects_[d].contains(x, y))
                {
                    gridDests_.erase(gridDests_.begin() + long(d));
                    repaint();
                    return true;
                }
            // Right-click a strip MOD slot → pick / change the modulation source
            for(const auto &hit : modHits_)
                if(hit.rect.contains(x, y))
                {
                    const int ti = trackIndexOfId(uint32_t(hit.trackId));
                    if(ti >= 0) { selectedTrack_ = ti; selectedGroupView_ = -1; }
                    openModSourceMenu(hit.trackId, hit.slot, x, y);
                    repaint();
                    return true;
                }
            for(int slot = 0; slot < 2; ++slot)
            {
                if(stripRouteRects_[(size_t)slot].contains(x, y)
                   && stripRouteRuleIndices_[(size_t)slot] >= 0)
                {
                    routeContextRuleIndex_ = stripRouteRuleIndices_[(size_t)slot];
                    routeContextX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW())  - 208.0f));
                    routeContextY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - 90.0f));
                    routeContextMenuOpen_ = true;
                    repaint();
                    return true;
                }
            }
        }

        if(ev.button != 1)
            return false;

        // Modifier state from the event mask (cleared every press, never sticky).
        ctrlDown_  = (ev.mod & kModifierControl) != 0;
        shiftDown_ = (ev.mod & kModifierShift) != 0;

        currentClickIsDouble_ = (ev.time - lastClickTime_) < 400u
                                && std::abs(x - lastClickX_) < 8.0f
                                && std::abs(y - lastClickY_) < 8.0f;
        lastClickTime_ = ev.time;
        lastClickX_ = x;
        lastClickY_ = y;

        if(handleModDepthPress(x, y))
        {
            repaint();
            return true;
        }

        if(handleWavetablePresetMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleMetaProcessContextMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleRouteContextMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleRouteNodeContextClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleInsertMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleModeMenuClick(x, y))
        {
            repaint();
            return true;
        }
        if(handleModSourceMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleWarpModeMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleOscModTypeMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleWavetableImportMenuClick(x, y))
        {
            repaint();
            return true;
        }

        if(handleHarmonicEditorClick(x, y))
        {
            repaint();
            return true;
        }

        // 双击重置到默认值
        if(currentClickIsDouble_ && handleDoubleClickReset(x, y)) { repaint(); return true; }

        if(handleToolbarClick(x, y) || handlePageClick(x, y) || handleKeyboardPress(x, y))
        {
            repaint();
            return true;
        }

        return false;
    }

END_NAMESPACE_DISTRHO
