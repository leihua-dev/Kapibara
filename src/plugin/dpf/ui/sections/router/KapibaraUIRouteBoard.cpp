#include "KapibaraUIRouteBoardShared.hpp"

START_NAMESPACE_DISTRHO

using namespace routeui;

void KapibaraUI::drawStripGrid(const Rect &r)
{
        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "STRIP FX");

        const float gridTop = r.y + 38.0f;
        const float gridH = r.h - 50.0f;
        for(int i = 0; i < 11; ++i)
        {
            const float gx = r.x + 12.0f + float(i) * (r.w - 24.0f) / 10.0f;
            strokeLine(gx, gridTop, gx, gridTop + gridH, rgba(0x20313a66), 0.7f);
        }
        for(int i = 0; i < 8; ++i)
        {
            const float gy = gridTop + float(i) * gridH / 7.0f;
            strokeLine(r.x + 12.0f, gy, r.x + r.w - 12.0f, gy, rgba(0x20313a66), 0.7f);
        }

        const int n = int(generator_.tracks.size());
        const float masterW = 78.0f;
        stripGridMasterRect_ = { r.x + r.w - masterW - 16.0f, r.y + r.h * 0.5f - 26.0f, masterW, 52.0f };
        drawPanel(stripGridMasterRect_, rgba(0x1b1820ff), rgba(0xffc857cc));
        useUiFont();
        uiFontSize(8.5f);
        fillColor(rgba(0xffd77aff));
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(stripGridMasterRect_.x + stripGridMasterRect_.w * 0.5f,
             stripGridMasterRect_.y + stripGridMasterRect_.h * 0.5f, "MASTER", nullptr);
        const Rect masterIn { stripGridMasterRect_.x - 4.0f, stripGridMasterRect_.y + stripGridMasterRect_.h * 0.5f - 4.0f, 8.0f, 8.0f };
        routePortHits_.push_back(RoutePortHit { masterIn, synth::GridPortRef { masterNodeId(), 0 }, false });

        constexpr float nodeW = 56.0f;
        constexpr float nodeH = 18.0f;
        const float startX = r.x + 18.0f;
        const float startY = r.y + 54.0f;
        int drawnFx = 0;
        for(int i = 0; i < n; ++i)
        {
            auto &track = generator_.tracks[(size_t)i];
            for(int fx = 0; fx < int(track.inserts.size()); ++fx)
            {
                const int localNode = fx + 1;
                const uint32_t nodeId = stripNodeId(track.id, localNode);
                const int col = drawnFx % 2;
                const int row = drawnFx / 2;
                const synth::GridPoint defaultPos {
                    int(std::round(startX + float(col) * (nodeW + 14.0f))),
                    int(std::round(startY + float(row) * (nodeH + 12.0f)))
                };
                auto [posIt, inserted] = routeNodePositions_.try_emplace(nodeId, defaultPos);
                const Rect node { float(posIt->second.x), float(posIt->second.y), nodeW, nodeH };
                const auto &ins = track.inserts[(size_t)fx];
                drawPanel(node, ins.bypass ? rgba(0x15191cff) : rgba(0x14201cff),
                          ins.bypass ? rgba(0x506070ff) : DesignTokens::accentCyan());
                uiFontSize(7.0f);
                fillColor(ins.bypass ? DesignTokens::textSecondary() : DesignTokens::textPrimary());
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                text(node.x + node.w * 0.5f, node.y + node.h * 0.5f, insertTypeName(ins.kind), nullptr);
                insertHits_.push_back(InsertHit { node, int(track.id), -1, fx });
                routeNodeHits_.push_back(RouteNodeHit { node, nodeId });

                const Rect inP { node.x - 4.0f, node.y + node.h * 0.5f - 4.0f, 8.0f, 8.0f };
                routePortHits_.push_back(RoutePortHit { inP, synth::GridPortRef { stripNodeId(track.id, localNode), 0 }, false });
                beginPath(); ellipse(inP.x + 4.0f, inP.y + 4.0f, 4.0f, 4.0f); fillColor(DesignTokens::accentCyan()); fill();
                drawComponentOutputPorts(node, stripNodeId(track.id, localNode));
                ++drawnFx;
            }
        }

        if(selectedTrack_ >= 0 && selectedTrack_ < n)
        {
            const Rect add { r.x + 18.0f, r.y + r.h - 34.0f, 86.0f, 20.0f };
            drawButton(add, "+ STRIP FX", false);
            insertHits_.push_back(InsertHit { add, int(generator_.tracks[(size_t)selectedTrack_].id), -1, -1 });
        }

        beginPath(); ellipse(masterIn.x + 4.0f, masterIn.y + 4.0f, 4.0f, 4.0f);
        fillColor(rgba(0xffc857ff)); fill();

        const auto portCenterFor = [&](const synth::GridPortRef &port, synth::GridPoint fallback) {
            for(const auto &hit : routePortHits_)
                if(samePort(hit.port, port))
                    return synth::GridPoint {
                        int(std::round(hit.rect.x + hit.rect.w * 0.5f)),
                        int(std::round(hit.rect.y + hit.rect.h * 0.5f))
                    };
            return fallback;
        };
        renderedWirePaths_.clear();
        renderedWireIndices_.clear();
        renderedWireIntersections_.clear();

        // Group auto-routed wires by destination port so several sources feeding
        // one input converge only at the port. Each wire in a group gets a
        // distinct final-approach lane (see laneStubStart) — they may cross but
        // never share a trunk segment in front of the input.
        std::vector<int> laneOf(routeWires_.size(), -1);
        std::vector<int> laneTotal(routeWires_.size(), 1);
        {
            // Destination port -> list of (wireIdx, sourceY), only for auto wires.
            std::vector<std::pair<synth::GridPortRef, std::vector<std::pair<size_t, int>>>> groups;
            for(size_t i = 0; i < routeWires_.size(); ++i)
            {
                const auto &w = routeWires_[i];
                if(w.points.size() != 2) continue; // empty or manually-routed → leave alone
                const auto start = portCenterFor(w.from, w.points.front());
                auto it = std::find_if(groups.begin(), groups.end(),
                                       [&](const auto &g) { return samePort(g.first, w.to); });
                if(it == groups.end()) groups.push_back({ w.to, { { i, start.y } } });
                else it->second.push_back({ i, start.y });
            }
            for(auto &g : groups)
            {
                auto &members = g.second;
                if(members.size() <= 1) continue;
                std::sort(members.begin(), members.end(),
                          [](const auto &a, const auto &b) { return a.second < b.second; });
                for(size_t k = 0; k < members.size(); ++k)
                {
                    laneOf[members[k].first]    = int(k);
                    laneTotal[members[k].first] = int(members.size());
                }
            }
        }

        // === Pass 1: Compute all wire paths ===
        size_t wireListIdx = 0;
        for(const auto &w : routeWires_)
        {
            if(w.points.empty()) { ++wireListIdx; continue; }
            const auto start = portCenterFor(w.from, w.points.front());
            const auto end   = portCenterFor(w.to,   w.points.back());

            std::vector<synth::GridPoint> points;
            if(w.points.size() > 2)
            {
                points = w.points;
                points.front() = start;
                points.back()  = end;
            }
            else if(laneTotal[wireListIdx] > 1)
            {
                // Route to a per-lane staging point, then a short unique segment
                // into the port so the merge happens only at the port pixel.
                const synth::GridPoint stub =
                    laneStubStart(end, laneOf[wireListIdx], laneTotal[wireListIdx], 16);
                points = cleanRoutePath(start, stub);
                if(!samePoint(points.back(), end)) points.push_back(end);
            }
            else
            {
                points = cleanRoutePath(start, end);
            }
            renderedWirePaths_.push_back(std::move(points));
            renderedWireIndices_.push_back(wireListIdx);
            renderedWireIntersections_.push_back({});
            ++wireListIdx;
        }

        // Which wires carry the currently-selected source's signal? (forward
        // reachability from its source node) — those are highlighted; the rest dim.
        std::vector<bool> wireInChain(renderedWirePaths_.size(), false);
        if(selectedTrack_ >= 0 && selectedTrack_ < int(generator_.tracks.size()))
        {
            std::vector<uint32_t> reach { sourceRouterNodeId(generator_.tracks[(size_t)selectedTrack_].id) };
            bool grew = true;
            while(grew)
            {
                grew = false;
                for(const auto &w : routeWires_)
                    if(std::find(reach.begin(), reach.end(), w.from.nodeId) != reach.end()
                       && std::find(reach.begin(), reach.end(), w.to.nodeId) == reach.end())
                    { reach.push_back(w.to.nodeId); grew = true; }
            }
            for(size_t ri = 0; ri < renderedWirePaths_.size() && ri < renderedWireIndices_.size(); ++ri)
            {
                const size_t wi = renderedWireIndices_[ri];
                if(wi < routeWires_.size()
                   && std::find(reach.begin(), reach.end(), routeWires_[wi].from.nodeId) != reach.end())
                    wireInChain[ri] = true;
            }
        }

        // === Pass 2: Draw each wire path directly ===
        // Wires that share identical segments at the same pixel coordinates
        // naturally appear as one line when drawn on top of each other.
        for(size_t ri = 0; ri < renderedWirePaths_.size(); ++ri)
        {
            const bool wsel = (selectedWireIdx_ == int(ri));
            const bool inChain = (ri < wireInChain.size() && wireInChain[ri]);
            const auto &path = renderedWirePaths_[ri];
            if(path.size() < 2) continue;
            beginPath();
            moveTo(float(path.front().x), float(path.front().y));
            for(size_t i = 1; i < path.size(); ++i)
                lineTo(float(path[i].x), float(path[i].y));
            strokeColor(wsel ? rgba(0xc8ff80ffU) : (inChain ? rgba(0x9eff50ffU) : rgba(0x9eff5040U)));
            strokeWidth(wsel ? 1.9f : (inChain ? 1.7f : 1.0f));
            stroke();
        }
        // Crossing wires do NOT merge — each goes its own way — so no junction
        // marker is drawn at intersections. We still record crossings per wire
        // because they act as section boundaries for segment dragging.
        for(size_t wi = 0; wi < renderedWirePaths_.size(); ++wi)
        {
            for(size_t wj = wi + 1; wj < renderedWirePaths_.size(); ++wj)
            {
                for(size_t ai = 1; ai < renderedWirePaths_[wi].size(); ++ai)
                {
                    for(size_t bi = 1; bi < renderedWirePaths_[wj].size(); ++bi)
                    {
                        synth::GridPoint p;
                        if(!segmentIntersection(renderedWirePaths_[wi][ai - 1], renderedWirePaths_[wi][ai],
                                                renderedWirePaths_[wj][bi - 1], renderedWirePaths_[wj][bi], p))
                            continue;

                        // Record intersection on each wire with its position parameter.
                        const auto computeT = [](const synth::GridPoint &seg0, const synth::GridPoint &seg1,
                                                  const synth::GridPoint &pt) {
                            const float dx = float(seg1.x - seg0.x), dy = float(seg1.y - seg0.y);
                            const float len2 = dx * dx + dy * dy;
                            if(len2 < 0.0001f) return 0.0f;
                            return std::max(0.0f, std::min(1.0f,
                                ((float(pt.x - seg0.x)) * dx + (float(pt.y - seg0.y)) * dy) / len2));
                        };
                        if(wi < renderedWireIntersections_.size())
                            renderedWireIntersections_[wi].push_back({p, int(ai - 1), computeT(renderedWirePaths_[wi][ai - 1], renderedWirePaths_[wi][ai], p)});
                        if(wj < renderedWireIntersections_.size())
                            renderedWireIntersections_[wj].push_back({p, int(bi - 1), computeT(renderedWirePaths_[wj][bi - 1], renderedWirePaths_[wj][bi], p)});
                    }
                }
            }
        }
        // Draw selected section highlight on top.
        if(selectedWireIdx_ >= 0 && selectedWireIdx_ < int(renderedWirePaths_.size())
           && selectedSectionValid_)
        {
            const auto sec = extractSubpath(renderedWirePaths_[(size_t)selectedWireIdx_],
                                            selectedSectionPtA_, selectedSectionPtB_);
            if(sec.size() >= 2)
            {
                beginPath();
                moveTo(float(sec.front().x), float(sec.front().y));
                for(size_t i = 1; i < sec.size(); ++i)
                    lineTo(float(sec[i].x), float(sec[i].y));
                strokeColor(segmentDragActive_ ? rgba(0xff8800ffU) : rgba(0xffff50ffU));
                strokeWidth(2.4f);
                stroke();
                // Draw small squares at section endpoints to show boundaries.
                for(const auto &ep : { sec.front(), sec.back() })
                {
                    beginPath();
                    rect(float(ep.x) - 3.0f, float(ep.y) - 3.0f, 6.0f, 6.0f);
                    fillColor(rgba(0xffc857ffU));
                    fill();
                }
            }
        }
        if(routeWireDraft_.active && !routeWireDraft_.points.empty())
        {
            std::vector<synth::GridPoint> previewPoints = routeWireDraft_.points;
            appendConstrainedPath(previewPoints, synth::GridPoint {
                int(std::round(routeWireDraft_.mouseX)),
                int(std::round(routeWireDraft_.mouseY))
            });
            beginPath();
            moveTo(float(previewPoints.front().x), float(previewPoints.front().y));
            for(size_t i = 1; i < previewPoints.size(); ++i)
                lineTo(float(previewPoints[i].x), float(previewPoints[i].y));
            strokeColor(rgba(0x9eff5088));
            strokeWidth(1.2f);
            stroke();
        }
        if(routeNodeContextOpen_)
            drawButton(routeNodeDeleteRect_, "DELETE", false);
    }
END_NAMESPACE_DISTRHO
