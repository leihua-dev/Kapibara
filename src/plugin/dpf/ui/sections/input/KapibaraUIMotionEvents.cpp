#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::onMotion(const MotionEvent &ev)
{
        const float x = (static_cast<float>(ev.pos.getX()) - lbX_) / uiRenderScale_;
        const float y = (static_cast<float>(ev.pos.getY()) - lbY_) / uiRenderScale_;
        // Keep snap modifier live during a drag (Shift to disable grid snap).
        shiftDown_ = (ev.mod & kModifierShift) != 0;

        if(insertPending_)
        {
            const float dx = x - insertPendX_, dy = y - insertPendY_;
            if(dx * dx + dy * dy > 25.0f)
                insertDragActive_ = true;
            insertDragX_ = x; insertDragY_ = y;
            repaint();
            return true;
        }

        if(fxRackDragPending_)
        {
            const float dx = x - fxRackDragStartX_, dy = y - fxRackDragStartY_;
            if(dx * dx + dy * dy > 25.0f)
                fxRackDragActive_ = true;
            fxRackDragX_ = x; fxRackDragY_ = y;
            repaint();
            return true;
        }

        if(focusedNodeId_ != 0 && structUtilParamDrag_ >= 0 && structSelectedUtil_ >= 30)
        {
            const int u = structSelectedUtil_ - 30;
            auto &up = structUtilParams_[(uint64_t(focusedNodeId_) << 8) | uint32_t(u)];
            const float nn = std::clamp(structUtilParamStartVal_ + (structUtilParamStartY_ - y) / 200.0f, 0.0f, 1.0f);
            switch(structUtilParamDrag_)
            {
                case 0: up.level = nn * 2.0f; break;
                case 1: up.pan = nn * 2.0f - 1.0f; break;
                case 2: up.bandLoHz = normToCutoff(nn); break;
                case 3: up.bandHiHz = normToCutoff(nn); break;
            }
            if(auto *p = plugin()) p->updateCompiledRoute(buildCompiledRoute());
            repaint();
            return true;
        }
        if(focusedNodeId_ != 0 && structDragLocal_ >= 0)
        {
            // Snap to the 12px grid (Shift disables snap), like the main router.
            const float nx = x - structDragOffX_, ny = y - structDragOffY_;
            structNodePos_[focusedNodeId_][structDragLocal_] = shiftDown_
                ? synth::GridPoint { int(nx), int(ny) }
                : synth::GridPoint { int(std::round(nx / 12.0f)) * 12, int(std::round(ny / 12.0f)) * 12 };
            repaint();
            return true;
        }
        if(focusedNodeId_ != 0 && structDraftActive_)
        {
            structDraftX_ = x; structDraftY_ = y;
            repaint();
            return true;
        }

        if(routeWireDraft_.active)
        {
            routeWireDraft_.mouseX = x;
            routeWireDraft_.mouseY = y;
            repaint();
            return true;
        }

        if(segmentDragPending_)
        {
            const float ddx = x - segmentDragStartX_;
            const float ddy = y - segmentDragStartY_;
            if(ddx * ddx + ddy * ddy > 16.0f)
            {
                segmentDragPending_ = false;
                segmentDragActive_ = true;
                // Fall through to drag handler.
            }
            else
            {
                repaint();
                return true;
            }
        }

        if(segmentDragActive_)
        {
            const int dx = int(std::round(x - segmentDragStartX_));
            const int dy = int(std::round(y - segmentDragStartY_));
            const size_t wi = segmentDragWireIdx_ >= 0
                              ? renderedWireIndices_[(size_t)segmentDragWireIdx_] : size_t(-1);
            if(wi < routeWires_.size())
            {
                routeWires_[wi].points = computeMovedPathSection(segmentDragOrigPath_,
                                                                 segmentDragPtA_, segmentDragPtB_, dx, dy);
            }
            repaint();
            return true;
        }

        if(routeNodeDragActive_)
        {
            const auto isPerVoiceRouteNode = [](uint32_t nodeId) {
                return (nodeId & 0xf0000000u) == 0x10000000u;
            };
            const auto isStripRouteNode = [](uint32_t nodeId) {
                return (nodeId & 0xf0000000u) == 0x20000000u;
            };
            const auto isAmpEnvRouteNode = [](uint32_t nodeId) {
                return (nodeId & 0xf0000000u) == 0x30000000u;
            };
            const float nodeW = isAmpEnvRouteNode(routeNodeDragId_) ? 32.0f
                              : (isPerVoiceRouteNode(routeNodeDragId_) ? 46.0f : 56.0f);
            const float nodeH = 18.0f;
            const auto snapCoord = [&](float v) {
                return shiftDown_ ? v : std::round(v / 12.0f) * 12.0f;
            };
            const Rect zone = (isPerVoiceRouteNode(routeNodeDragId_) || isAmpEnvRouteNode(routeNodeDragId_)) ? perVoiceRouteRect_
                            : (isStripRouteNode(routeNodeDragId_) ? stripRouteRect_ : routeBoardRect_);
            const float minX = zone.w > 0.0f ? zone.x + 8.0f : 0.0f;
            const float minY = zone.h > 0.0f ? zone.y + 34.0f : 0.0f;
            const float maxX = zone.w > 0.0f ? zone.x + zone.w - nodeW - 8.0f : float(uiW());
            const float maxY = zone.h > 0.0f ? zone.y + zone.h - nodeH - 8.0f : float(uiH());
            float nx = clampf(snapCoord(x - routeNodeDragOffsetX_), minX, maxX);
            float ny = clampf(snapCoord(y - routeNodeDragOffsetY_), minY, maxY);

            // Connected nodes must not overlap other nodes.
            const auto nodeHasConn = [&](uint32_t nodeId) {
                for(const auto &w : routeWires_)
                    if(w.from.nodeId == nodeId || w.to.nodeId == nodeId)
                        return true;
                return false;
            };
            if(nodeHasConn(routeNodeDragId_))
            {
                constexpr float margin = 6.0f;
                for(const auto &hit : routeNodeHits_)
                {
                    if(hit.nodeId == routeNodeDragId_) continue;
                    const float ox1 = hit.rect.x - margin, oy1 = hit.rect.y - margin;
                    const float ox2 = hit.rect.x + hit.rect.w + margin, oy2 = hit.rect.y + hit.rect.h + margin;
                    if(nx < ox2 && nx + nodeW > ox1 && ny < oy2 && ny + nodeH > oy1)
                    {
                        const float ovX = std::min(ox2 - nx, (nx + nodeW) - ox1);
                        const float ovY = std::min(oy2 - ny, (ny + nodeH) - oy1);
                        if(ovX <= ovY)
                        { if(ox2 - nx < (nx + nodeW) - ox1) nx = ox2; else nx = ox1 - nodeW; }
                        else
                        { if(oy2 - ny < (ny + nodeH) - oy1) ny = oy2; else ny = oy1 - nodeH; }
                    }
                }
            }

            routeNodePositions_[routeNodeDragId_] = synth::GridPoint {
                int(std::round(nx)),
                int(std::round(ny))
            };
            repaint();
            return true;
        }

        if(modRouteDragActive_)
        {
            modRouteMouseX_ = x;
            modRouteMouseY_ = y;
            const float dx = x - modRouteStartX_;
            const float dy = y - modRouteStartY_;
            modRouteDragMoved_ = modRouteDragMoved_ || dx * dx + dy * dy > 16.0f;
            modRouteHover_ = modRouteTargetAt(x, y);
            repaint();
            return true;
        }

        if(mouseKey_ >= 0)
        {
            const int key = keyAt(x, y);
            if(key != mouseKey_)
            {
                releaseMouseKey();
                if(key >= 0)
                    pressMouseKey(key);
            }
            return true;
        }

        if(dragTarget_ == DragTarget::None)
            return false;

        applyDragValue(x, y);
        repaint();
        return true;
    }

END_NAMESPACE_DISTRHO
