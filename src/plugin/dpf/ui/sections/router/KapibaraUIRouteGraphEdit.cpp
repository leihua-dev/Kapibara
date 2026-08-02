#include "KapibaraUIRouteBoardShared.hpp"

START_NAMESPACE_DISTRHO

using namespace routeui;

// A preset with no `modern` section carries no router at all. Inheriting the
// previous patch's graph left the rack incoherent — wires into components this
// patch does not have, output ports held by references nothing draws — so the
// components that DID load could not be wired until some unrelated edit
// rewrote the leftovers. Reset it and lay down the path the pre-router engine
// actually ran, so nothing is inherited and nothing comes up silent.
void KapibaraUI::resetRouterGraphToDefaultChains()
{
        routeWires_.clear();
        routeNodePositions_.clear();
        nodeOutPortCount_.clear();
        structUtilCount_.clear();
        structUtilParams_.clear();
        structNodePos_.clear();
        structWires_.clear();
        stripGroups_.clear();
        ampEnvRouteNodeSlots_.fill(255);
        routeWireDraft_ = {};
        selectedWireIdx_ = -1;
        selectedSectionValid_ = false;
        selectedGroupView_ = -1;
        focusedNodeId_ = 0;

        const auto link = [&](const synth::GridPortRef &from, const synth::GridPortRef &to) {
            synth::GridWire w;
            w.from = from;
            w.to = to;
            // No points: the visual path is recomputed from live port positions
            // every frame, the same as wires parsed out of a preset.
            routeWires_.push_back(std::move(w));
        };
        // Per-voice filter nodes are shared by the whole rack and the linear chain
        // walk only ever leaves one by output port 1, so they cannot fan out to a
        // separate strip chain per track. With a single source there is no
        // contention and the filter the plugin migrates out of a legacy
        // `strip.filter` belongs in the path; past that the default runs straight
        // into each track's own inserts and the filters are left to be wired
        // deliberately.
        const bool useFilters = generator_.tracks.size() == 1;
        for(const auto &t : generator_.tracks)
        {
            synth::GridPortRef cursor { sourceRouterNodeId(t.id), 0 };
            if(useFilters)
                for(int fi = 0; fi < clampi(t.perVoiceFilterCount, 0, synth::kMaxPerVoiceFilters); ++fi)
                {
                    const uint32_t node = perVoiceNodeId(t.id, fi + 1);
                    link(cursor, { node, 0 });
                    cursor = { node, 1 };
                }
            for(size_t ii = 0; ii < t.inserts.size() && ii < size_t(synth::kMaxStripInserts); ++ii)
            {
                const uint32_t node = stripNodeId(t.id, int(ii) + 1);
                link(cursor, { node, 0 });
                cursor = { node, 1 };
            }
            link(cursor, { masterNodeId(), 0 });
        }
        pushGroups();
        rebuildSelectedPerVoiceRouteFromWires();
    }

void KapibaraUI::cleanupRouteGraphForCurrentTracks()
{
        for(const auto &wire : routeWires_)
        {
            if(isAmpEnvNode(wire.from.nodeId))
            {
                const int slot = ampEnvSlotId(wire.from.nodeId);
                const int inst = ampEnvInstanceId(wire.from.nodeId);
                if(slot >= 0 && slot < synth::kMaxAmpEnvs
                   && inst >= 0 && inst < synth::kMaxAmpEnvRouteNodes)
                {
                    ampEnvRouteNodeSlots_[(size_t)inst] = uint8_t(slot);
                }
            }
            if(isAmpEnvNode(wire.to.nodeId))
            {
                const int slot = ampEnvSlotId(wire.to.nodeId);
                const int inst = ampEnvInstanceId(wire.to.nodeId);
                if(slot >= 0 && slot < synth::kMaxAmpEnvs
                   && inst >= 0 && inst < synth::kMaxAmpEnvRouteNodes)
                {
                    ampEnvRouteNodeSlots_[(size_t)inst] = uint8_t(slot);
                }
            }
        }
        const auto trackExists = [&](uint32_t trackId) {
            return std::any_of(generator_.tracks.begin(), generator_.tracks.end(),
                               [trackId](const synth::SourceTrackParams &track) { return track.id == trackId; });
        };
        const auto insertExists = [&](uint32_t trackId, int local) {
            if(local <= 0)
                return false;
            for(const auto &track : generator_.tracks)
                if(track.id == trackId)
                    return local <= int(track.inserts.size());
            return false;
        };
        int globalFilterCount = 0;
        for(const auto &track : generator_.tracks)
            globalFilterCount = std::max(globalFilterCount, clampi(track.perVoiceFilterCount, 0, synth::kMaxPerVoiceFilters));
        if(globalFilterCount <= 0)
            globalFilterCount = generator_.tracks.empty() ? 0 : 1;

        const auto nodeValid = [&](uint32_t nodeId) {
            if(nodeId == masterNodeId())
                return true;
            if(isSourceRouterNode(nodeId))
                return trackExists(sourceRouterTrackId(nodeId));
            if(isPerVoiceNode(nodeId))
            {
                const int local = perVoiceLocalId(nodeId);
                return local > 0 && local <= globalFilterCount;
            }
            if(isStripNode(nodeId))
                return insertExists(stripTrackId(nodeId), stripLocalId(nodeId));
            if(isAmpEnvNode(nodeId))
            {
                const int slot = ampEnvSlotId(nodeId);
                const int inst = ampEnvInstanceId(nodeId);
                return slot >= 0 && slot < synth::kMaxAmpEnvs
                       && inst >= 0 && inst < synth::kMaxAmpEnvRouteNodes
                       && ampEnvRouteNodeSlots_[(size_t)inst] == uint8_t(slot);
            }
            return false;
        };

        // Strip nodes are user-managed: their wires are only removed when the node is
        // explicitly deleted, NOT when the source track disappears. This prevents a source
        // deletion from silently disconnecting the downstream effects chain.
        //
        // That exemption must not extend to a strip node whose INSERT is gone, which
        // is what a blanket `return false` did. Nothing draws such a node, so the wire
        // is invisible — while it still holds its upstream node's single output port,
        // still feeds createsCycle, and still counts toward componentOutPortCount,
        // which stacks phantom output dots and shifts every real one off the pixel the
        // user is aiming at. That is the "component will not connect until I wire
        // something else first" case: the other edit rewrote the stale wire and the
        // dots snapped back. Deleting a source still leaves other tracks' inserts
        // alone, so the original intent survives — only genuinely dangling refs go.
        //
        // MASTER is tested first: it satisfies isStripNode, and putting it through
        // insertExists resolves to "gone" and would erase every wire reaching output.
        const auto shouldAutoClean = [&](uint32_t nodeId) {
            if(nodeId == masterNodeId())
                return false;
            if(isStripNode(nodeId))
                return !insertExists(stripTrackId(nodeId), stripLocalId(nodeId));
            return !nodeValid(nodeId);
        };
        routeWires_.erase(std::remove_if(routeWires_.begin(), routeWires_.end(),
                                         [&](const synth::GridWire &wire) {
                                             return shouldAutoClean(wire.from.nodeId)
                                                    || shouldAutoClean(wire.to.nodeId);
                                         }),
                          routeWires_.end());

        for(auto it = routeNodePositions_.begin(); it != routeNodePositions_.end(); )
        {
            const uint32_t nodeId = it->first;
            // Strip nodes persist in routeNodePositions_ until explicitly deleted.
            const bool keep = isStripNode(nodeId)
                              || ((isPerVoiceNode(nodeId) || isAmpEnvNode(nodeId))
                                  && nodeValid(nodeId));
            if(keep)
                ++it;
            else
                it = routeNodePositions_.erase(it);
        }

        if(routeWireDraft_.active && !nodeValid(routeWireDraft_.from.nodeId))
            routeWireDraft_ = {};
        if(routeNodeDragActive_ && !nodeValid(routeNodeDragId_))
        {
            routeNodeDragActive_ = false;
            routeNodeDragId_ = 0;
        }
    }

bool KapibaraUI::deleteRouteNode(uint32_t nodeId)
{
        if(nodeId == 0 || isSourceRouterNode(nodeId) || nodeId == masterNodeId())
            return false;

        routeWires_.erase(std::remove_if(routeWires_.begin(), routeWires_.end(),
                                         [&](const synth::GridWire &wire) {
                                             return wire.from.nodeId == nodeId || wire.to.nodeId == nodeId;
                                         }),
                          routeWires_.end());
        routeNodePositions_.erase(nodeId);
        selectedWireIdx_ = -1;

        if(isPerVoiceNode(nodeId))
        {
            const int local = perVoiceLocalId(nodeId);
            if(local <= 0 || local > synth::kMaxPerVoiceFilters)
                return false;
            const int filterIdx = local - 1;
            for(auto &track : generator_.tracks)
            {
                if(filterIdx >= track.perVoiceFilterCount)
                    continue;
                for(int i = filterIdx; i + 1 < track.perVoiceFilterCount && i + 1 < synth::kMaxPerVoiceFilters; ++i)
                    track.perVoiceFilters[(size_t)i] = track.perVoiceFilters[(size_t)i + 1];
                track.perVoiceFilterCount = std::max(0, track.perVoiceFilterCount - 1);
                std::array<uint8_t, synth::kMaxPerVoiceFilters> newOrder {};
                int kept = 0;
                for(int i = 0; i < track.perVoiceFilterOrderCount; ++i)
                {
                    int idx = int(track.perVoiceFilterOrder[(size_t)i]);
                    if(idx == filterIdx)
                        continue;
                    if(idx > filterIdx)
                        --idx;
                    if(kept < synth::kMaxPerVoiceFilters)
                        newOrder[(size_t)kept++] = uint8_t(idx);
                }
                track.perVoiceFilterOrder = newOrder;
                track.perVoiceFilterOrderCount = kept;
                if(track.perVoiceFilterCount > 0)
                    track.strip.filter = track.perVoiceFilters[0];
            }
            std::unordered_map<uint32_t, synth::GridPoint> shifted;
            for(auto &[id, pos] : routeNodePositions_)
            {
                if(isPerVoiceNode(id))
                {
                    const int otherLocal = perVoiceLocalId(id);
                    if(otherLocal > local)
                        shifted[perVoiceNodeId(0, otherLocal - 1)] = pos;
                    else
                        shifted[id] = pos;
                }
                else
                    shifted[id] = pos;
            }
            routeNodePositions_ = std::move(shifted);
            for(auto &wire : routeWires_)
            {
                if(isPerVoiceNode(wire.from.nodeId) && perVoiceLocalId(wire.from.nodeId) > local)
                    wire.from.nodeId = perVoiceNodeId(0, perVoiceLocalId(wire.from.nodeId) - 1);
                if(isPerVoiceNode(wire.to.nodeId) && perVoiceLocalId(wire.to.nodeId) > local)
                    wire.to.nodeId = perVoiceNodeId(0, perVoiceLocalId(wire.to.nodeId) - 1);
            }
            selectedPerVoiceFilter_ = clampi(selectedPerVoiceFilter_, 0, synth::kMaxPerVoiceFilters - 1);
            selectedRouteNodeId_ = 0;
            routeNodeContextOpen_ = false;
            pushGenerator();
            return true;
        }

        if(isStripNode(nodeId))
        {
            const uint32_t trackId = stripTrackId(nodeId);
            const int local = stripLocalId(nodeId);
            if(local <= 0)
                return false;
            for(auto &track : generator_.tracks)
            {
                if(track.id != trackId || local > int(track.inserts.size()))
                    continue;
                track.inserts.erase(track.inserts.begin() + (local - 1));
                break;
            }
            std::unordered_map<uint32_t, synth::GridPoint> shifted;
            for(auto &[id, pos] : routeNodePositions_)
            {
                if(isStripNode(id) && stripTrackId(id) == trackId)
                {
                    const int otherLocal = stripLocalId(id);
                    if(otherLocal > local)
                        shifted[stripNodeId(trackId, otherLocal - 1)] = pos;
                    else
                        shifted[id] = pos;
                }
                else
                    shifted[id] = pos;
            }
            routeNodePositions_ = std::move(shifted);
            for(auto &wire : routeWires_)
            {
                if(isStripNode(wire.from.nodeId) && stripTrackId(wire.from.nodeId) == trackId
                   && stripLocalId(wire.from.nodeId) > local)
                    wire.from.nodeId = stripNodeId(trackId, stripLocalId(wire.from.nodeId) - 1);
                if(isStripNode(wire.to.nodeId) && stripTrackId(wire.to.nodeId) == trackId
                   && stripLocalId(wire.to.nodeId) > local)
                    wire.to.nodeId = stripNodeId(trackId, stripLocalId(wire.to.nodeId) - 1);
            }
            selectedRouteNodeId_ = 0;
            routeNodeContextOpen_ = false;
            pushGenerator();
            return true;
        }
        if(isAmpEnvNode(nodeId))
        {
            const int inst = ampEnvInstanceId(nodeId);
            if(inst < 0 || inst >= synth::kMaxAmpEnvRouteNodes)
                return false;
            ampEnvRouteNodeSlots_[(size_t)inst] = 255;
            selectedRouteNodeId_ = 0;
            routeNodeContextOpen_ = false;
            rebuildSelectedPerVoiceRouteFromWires();
            return true;
        }
        return false;
    }

bool KapibaraUI::openRouteNodeContext(float x, float y)
{
        for(auto it = routeNodeHits_.rbegin(); it != routeNodeHits_.rend(); ++it)
        {
            if(!it->rect.contains(x, y))
                continue;
            selectedRouteNodeId_ = it->nodeId;
            routeNodeContextOpen_ = true;
            routeNodeDeleteRect_ = {
                clampf(x, 4.0f, std::max(4.0f, float(uiW()) - 58.0f)),
                clampf(y, 4.0f, std::max(4.0f, float(uiH()) - 22.0f)),
                54.0f,
                18.0f
            };
            return true;
        }
        routeNodeContextOpen_ = false;
        return false;
    }

void KapibaraUI::tryMergeNodeIntoWire(uint32_t nodeId)
{
    if(nodeId == 0)
        return;
    // Only splice if the node currently has no connections.
    for(const auto &w : routeWires_)
        if(w.from.nodeId == nodeId || w.to.nodeId == nodeId)
            return;

    // Find the node's input and output port centers from the last rendered frame.
    synth::GridPortRef inPort {}, outPort {};
    synth::GridPoint inPt {}, outPt {};
    for(const auto &hit : routePortHits_)
    {
        if(hit.port.nodeId != nodeId)
            continue;
        const synth::GridPoint center {
            int(std::round(hit.rect.x + hit.rect.w * 0.5f)),
            int(std::round(hit.rect.y + hit.rect.h * 0.5f))
        };
        if(!hit.output) { inPort = hit.port; inPt = center; }
        else            { outPort = hit.port; outPt = center; }
    }
    if(inPort.nodeId == 0 || outPort.nodeId == 0)
        return;

    // Find a rendered wire whose path passes near BOTH the input and output ports.
    constexpr float kMergeThreshold = 12.0f;
    for(size_t ri = 0; ri < renderedWirePaths_.size() && ri < renderedWireIndices_.size(); ++ri)
    {
        const auto &path = renderedWirePaths_[ri];
        if(path.size() < 2)
            continue;
        if(!pointNearPolyline(float(inPt.x), float(inPt.y), path, kMergeThreshold))
            continue;
        if(!pointNearPolyline(float(outPt.x), float(outPt.y), path, kMergeThreshold))
            continue;

        const size_t wi = renderedWireIndices_[ri];
        if(wi >= routeWires_.size())
            continue;
        const auto srcPort = routeWires_[wi].from;
        const auto dstPort = routeWires_[wi].to;
        const auto pathFront = path.front();
        const auto pathBack  = path.back();

        routeWires_.erase(routeWires_.begin() + long(wi));

        synth::GridWire w1, w2;
        w1.from = srcPort;
        w1.to   = inPort;
        w1.points = { pathFront, inPt };
        w2.from = outPort;
        w2.to   = dstPort;
        w2.points = { outPt, pathBack };
        routeWires_.push_back(std::move(w1));
        routeWires_.push_back(std::move(w2));
        selectedWireIdx_ = -1;
        rebuildSelectedPerVoiceRouteFromWires();
        return;
    }
}

std::vector<synth::GridPoint> KapibaraUI::computeMovedPathSection(
    const std::vector<synth::GridPoint> &orig,
    const synth::GridPoint &ptA, const synth::GridPoint &ptB, int dx, int dy)
{
    return computeMovedPath(orig, ptA, ptB, dx, dy);
}

bool KapibaraUI::deleteSelectedWire()
{
    if(selectedWireIdx_ < 0 || selectedWireIdx_ >= int(renderedWireIndices_.size()))
        return false;
    const size_t wi = renderedWireIndices_[(size_t)selectedWireIdx_];
    if(wi >= routeWires_.size())
        return false;
    routeWires_.erase(routeWires_.begin() + long(wi));
    selectedWireIdx_ = -1;
    rebuildSelectedPerVoiceRouteFromWires();
    return true;
}

bool KapibaraUI::handleRouteNodeContextClick(float x, float y)
{
        if(!routeNodeContextOpen_)
            return false;
        if(routeNodeDeleteRect_.contains(x, y))
            return deleteRouteNode(selectedRouteNodeId_);
        routeNodeContextOpen_ = false;
        return false;
    }

bool KapibaraUI::handleRouteGraphClick(float x, float y)
{
        const auto snapPoint = [](float px, float py) {
            constexpr float grid = 12.0f;
            return synth::GridPoint { int(std::round(px / grid) * grid), int(std::round(py / grid) * grid) };
        };
        const auto portCenter = [](const Rect &r) {
            return synth::GridPoint { int(std::round(r.x + r.w * 0.5f)), int(std::round(r.y + r.h * 0.5f)) };
        };
        // Ports are checked before nodes, but NOT simply in registration order.
        // The drag collision resolver parks a node's left edge exactly on its
        // neighbour's right edge, which drops that neighbour's OUTPUT dot pixel
        // for pixel onto this node's INPUT dot. First-hit-wins then restarted the
        // draft instead of completing the wire, so the two nodes could not be
        // connected at all until one was dragged away — the same click succeeding
        // or failing purely on where the nodes had come to rest. With a draft in
        // flight an input port outranks any output port sharing the pixel.
        const RoutePortHit *sel = nullptr;
        for(const auto &hit : routePortHits_)
        {
            if(!hit.rect.contains(x, y))
                continue;
            if(sel == nullptr)
                sel = &hit;
            if(routeWireDraft_.active && !hit.output)
            {
                sel = &hit;
                break;
            }
        }
        if(sel != nullptr)
        {
            const RoutePortHit &hit = *sel;
            if(hit.output)
            {
                routeWireDraft_.active = true;
                routeWireDraft_.from = hit.port;
                routeWireDraft_.points.clear();
                routeWireDraft_.points.push_back(portCenter(hit.rect));
                routeWireDraft_.mouseX = x;
                routeWireDraft_.mouseY = y;
                return true;
            }
            if(!routeWireDraft_.active)
                return false;
            const auto createsCycle = [&]() {
                std::vector<uint32_t> stack { hit.port.nodeId };
                std::vector<uint32_t> visited;
                while(!stack.empty())
                {
                    const uint32_t node = stack.back();
                    stack.pop_back();
                    if(node == routeWireDraft_.from.nodeId)
                        return true;
                    if(std::find(visited.begin(), visited.end(), node) != visited.end())
                        continue;
                    visited.push_back(node);
                    for(const auto &w : routeWires_)
                        if(w.from.nodeId == node)
                            stack.push_back(w.to.nodeId);
                }
                return false;
            };
            const bool duplicateWire = std::any_of(routeWires_.begin(), routeWires_.end(), [&](const synth::GridWire &w) {
                return samePort(w.from, routeWireDraft_.from) && samePort(w.to, hit.port);
            });
            if(duplicateWire || samePort(routeWireDraft_.from, hit.port) || createsCycle())
            {
                routeWireDraft_ = {};
                return true;
            }
            // Each output port carries at most ONE wire (inputs still sum many):
            // drop any existing wire leaving this output before adding the new one.
            routeWires_.erase(std::remove_if(routeWires_.begin(), routeWires_.end(),
                                             [&](const synth::GridWire &w) {
                                                 return samePort(w.from, routeWireDraft_.from);
                                             }),
                              routeWires_.end());
            synth::GridWire wire;
            wire.from = routeWireDraft_.from;
            wire.to = hit.port;
            const auto end = portCenter(hit.rect);
            // Store only the two port endpoints; visual path is computed each frame.
            wire.points = { routeWireDraft_.points.front(), end };
            routeWires_.push_back(std::move(wire));
            routeWireDraft_ = {};
            selectedWireIdx_ = -1;
            selectedSectionValid_ = false;
            rebuildSelectedPerVoiceRouteFromWires();
            return true;
        }
        if(routeWireDraft_.active)
        {
            const auto snapped = snapPoint(x, y);
            routeWireDraft_.points.push_back(constrainedRoutePoint(routeWireDraft_.points.back(),
                                                                   float(snapped.x), float(snapped.y)));
            routeWireDraft_.mouseX = x;
            routeWireDraft_.mouseY = y;
            selectedWireIdx_ = -1;
            return true;
        }
        for(auto it = routeNodeHits_.rbegin(); it != routeNodeHits_.rend(); ++it)
        {
            if(!it->rect.contains(x, y))
                continue;
            routeNodeContextOpen_ = false;
            selectedWireIdx_ = -1;
            if(currentClickIsDouble_)
            {
                // Double-click → focus this component's detail in the top half
                // (double-clicking the focused node again exits focus).
                focusedNodeId_ = (focusedNodeId_ == it->nodeId) ? 0u : it->nodeId;
                selectedRouteNodeId_ = it->nodeId;
                if(isPerVoiceNode(it->nodeId))
                {
                    const int local = perVoiceLocalId(it->nodeId);
                    if(local > 0) selectedPerVoiceFilter_ = clampi(local - 1, 0, synth::kMaxPerVoiceFilters - 1);
                }
                else if(isSourceRouterNode(it->nodeId))
                {
                    const uint32_t tid = sourceRouterTrackId(it->nodeId);
                    for(int i = 0; i < int(generator_.tracks.size()); ++i)
                        if(generator_.tracks[(size_t)i].id == tid) { selectedTrack_ = i; break; }
                }
                else if(isAmpEnvNode(it->nodeId))
                {
                    selectedAmpEnv_ = clampi(ampEnvSlotId(it->nodeId), 0, synth::kMaxAmpEnvs - 1);
                }
                repaint();
                return true;
            }
            // Single-clicking a different router component (source included) while
            // focused returns to the normal 3-panel top view. The bottom router
            // otherwise stays fully live (select/drag/wire/add/delete all work).
            if(focusedNodeId_ != 0 && it->nodeId != focusedNodeId_)
            {
                focusedNodeId_ = 0;
                repaint();
            }
            routeNodeDragActive_ = true;
            routeNodeDragId_ = it->nodeId;
            selectedRouteNodeId_ = it->nodeId;
            routeNodeDragOffsetX_ = x - it->rect.x;
            routeNodeDragOffsetY_ = y - it->rect.y;
            if(isPerVoiceNode(it->nodeId))
            {
                const int local = perVoiceLocalId(it->nodeId);
                if(local > 0)
                    selectedPerVoiceFilter_ = clampi(local - 1, 0, synth::kMaxPerVoiceFilters - 1);
            }
            return true;
        }
        // Check wire/section hit.
        for(size_t ri = 0; ri < renderedWirePaths_.size(); ++ri)
        {
            const auto &path = renderedWirePaths_[ri];
            if(!pointNearPolyline(x, y, path, 5.0f))
                continue;
            routeNodeContextOpen_ = false;

            // Click on already-selected section → start pending drag (drag activates on mouse move).
            if(selectedWireIdx_ == int(ri) && selectedSectionValid_)
            {
                const auto sec = extractSubpath(path, selectedSectionPtA_, selectedSectionPtB_);
                if(pointNearPolyline(x, y, sec, 6.0f))
                {
                    segmentDragPending_ = true;
                    segmentDragWireIdx_ = int(ri);
                    segmentDragPtA_ = selectedSectionPtA_;
                    segmentDragPtB_ = selectedSectionPtB_;
                    segmentDragOrigPath_ = path;
                    segmentDragStartX_ = x;
                    segmentDragStartY_ = y;
                    return true;
                }
            }

            // Find the section (between adjacent intersection nodes) that was clicked.
            // Build boundary list: wire endpoints + intersection points, sorted by position.
            struct Boundary { synth::GridPoint pt; float param; };
            std::vector<Boundary> boundaries;
            boundaries.push_back({ path.front(), 0.0f });
            if(ri < renderedWireIntersections_.size())
            {
                for(const auto &intr : renderedWireIntersections_[ri])
                    boundaries.push_back({ intr.pt, float(intr.segIdx) + intr.segT });
            }
            boundaries.push_back({ path.back(), float(int(path.size()) - 1) });
            std::sort(boundaries.begin(), boundaries.end(),
                      [](const Boundary &a, const Boundary &b) { return a.param < b.param; });

            // Remove near-duplicate boundaries.
            boundaries.erase(std::unique(boundaries.begin(), boundaries.end(),
                                         [](const Boundary &a, const Boundary &b) {
                                             return std::abs(a.param - b.param) < 0.01f;
                                         }), boundaries.end());

            const auto [clickSeg, clickT] = findPathPos(x, y, path);
            const float clickParam = float(clickSeg) + clickT;

            synth::GridPoint ptA = boundaries.front().pt, ptB = boundaries.back().pt;
            for(size_t b = 0; b + 1 < boundaries.size(); ++b)
            {
                if(clickParam >= boundaries[b].param && clickParam <= boundaries[b + 1].param)
                {
                    ptA = boundaries[b].pt;
                    ptB = boundaries[b + 1].pt;
                    break;
                }
            }

            selectedWireIdx_ = int(ri);
            selectedSectionValid_ = true;
            selectedSectionPtA_ = ptA;
            selectedSectionPtB_ = ptB;
            return true;
        }
        selectedWireIdx_ = -1;
        selectedSectionValid_ = false;
        return false;
    }
END_NAMESPACE_DISTRHO
