#include "KapibaraUIRouteBoardShared.hpp"

START_NAMESPACE_DISTRHO

using namespace routeui;

void KapibaraUI::rebuildSelectedPerVoiceRouteFromWires()
{
        // Nodes from which MASTER is forward-reachable (reverse closure from
        // MASTER over all wires). Computed once, used for every track below.
        std::vector<uint32_t> canReachMaster { masterNodeId() };
        {
            bool grew = true;
            while(grew)
            {
                grew = false;
                for(const auto &w : routeWires_)
                    if(std::find(canReachMaster.begin(), canReachMaster.end(), w.to.nodeId) != canReachMaster.end()
                       && std::find(canReachMaster.begin(), canReachMaster.end(), w.from.nodeId) == canReachMaster.end())
                    { canReachMaster.push_back(w.from.nodeId); grew = true; }
            }
        }
        const auto reachesMaster = [&](uint32_t nodeId) {
            return std::find(canReachMaster.begin(), canReachMaster.end(), nodeId) != canReachMaster.end();
        };

        for(auto &track : generator_.tracks)
        {
            const uint32_t tid = track.id;

            std::array<uint8_t, synth::kMaxPerVoiceFilters> filterOrder {};
            std::array<bool, synth::kMaxPerVoiceFilters> filterSeen {};
            int filterOrderCount = 0;
            std::array<uint8_t, synth::kMaxStripInserts> insertOrder {};
            std::array<bool, synth::kMaxStripInserts> insertSeen {};
            int insertOrderCount = 0;
            bool connectedToMaster = false;
            bool passedFilters = false; // true once we have left the per-voice filter zone

            synth::GridPortRef cursor { sourceRouterNodeId(tid), 0 };

            const int maxSteps = synth::kMaxPerVoiceFilters + synth::kMaxAmpEnvRouteNodes
                                  + synth::kMaxStripInserts + 4;
            for(int guard = 0; guard < maxSteps; ++guard)
            {
                // Find a wire that starts at cursor and belongs to this track's chain.
                const synth::GridWire *next = nullptr;
                for(const auto &w : routeWires_)
                {
                    if(!samePort(w.from, cursor))
                        continue;
                    const bool okSrc    = isSourceRouterNode(w.from.nodeId)
                                          && sourceRouterTrackId(w.from.nodeId) == tid;
                    const bool okFilter = isPerVoiceNode(w.from.nodeId);
                    const bool okAmp    = isAmpEnvNode(w.from.nodeId);
                    const bool okStrip  = isStripNode(w.from.nodeId)
                                          && stripTrackId(w.from.nodeId) == tid;
                    if(okSrc || okFilter || okAmp || okStrip)
                    {
                        next = &w;
                        break;
                    }
                }
                if(next == nullptr)
                    break;

                const uint32_t dest = next->to.nodeId;

                // Reached MASTER — done.
                if(dest == masterNodeId())
                {
                    connectedToMaster = true;
                    break;
                }

                // Per-voice filter node (only valid before we leave the filter zone).
                if(!passedFilters && isPerVoiceNode(dest))
                {
                    const int local = perVoiceLocalId(dest);
                    if(local <= 0) break;
                    const int fi = local - 1;
                    if(fi >= track.perVoiceFilterCount || fi >= synth::kMaxPerVoiceFilters) break;
                    if(filterSeen[(size_t)fi]) break;
                    filterSeen[(size_t)fi] = true;
                    filterOrder[(size_t)filterOrderCount++] = uint8_t(fi);
                    cursor = { perVoiceNodeId(0u, local), 1 }; // filter output port
                    continue;
                }

                // Amp-env nodes are explicit per-voice processing nodes. They
                // do not change filter/insert order, but the linear strip
                // runtime still needs to walk through them to discover the
                // downstream strip/master connection.
                if(!passedFilters && isAmpEnvNode(dest))
                {
                    if(ampEnvSlotId(dest) < 0 || ampEnvSlotId(dest) >= synth::kMaxAmpEnvs) break;
                    cursor = { dest, 1 };
                    continue;
                }

                // Strip FX node belonging to this track.
                if(isStripNode(dest) && stripTrackId(dest) == tid)
                {
                    passedFilters = true;
                    const int local = stripLocalId(dest);
                    if(local <= 0) break;
                    const int ii = local - 1;
                    if(ii >= int(track.inserts.size()) || ii >= synth::kMaxStripInserts) break;
                    if(insertSeen[(size_t)ii]) break;
                    insertSeen[(size_t)ii] = true;
                    insertOrder[(size_t)insertOrderCount++] = uint8_t(ii);
                    cursor = { dest, 1 }; // strip FX output port
                    continue;
                }

                // Anything else (wrong track's strip node, etc.) — dead end.
                break;
            }

            if(!passedFilters && filterOrderCount > 0)
                passedFilters = true; // had filters, then went somewhere non-strip

            // The linear walk only follows one output port; with multi-output
            // components a path to MASTER may use another port. Confirm via
            // reachability so connectedToMaster is robust.
            if(!connectedToMaster)
                connectedToMaster = reachesMaster(sourceRouterNodeId(tid));
            // The compiled per-voice DAG deposits a signal into the bus of whichever
            // track OWNS the strip node it feeds — which may not be the track whose
            // source started the chain (e.g. source B rewired into A's old chain that
            // ends at A's strip FX). That bus must stay audible, so also count this
            // track as connected when anything feeds its strip zone and that strip
            // node still reaches MASTER.
            if(!connectedToMaster)
                for(const auto &w : routeWires_)
                    if(w.to.nodeId != masterNodeId() && isStripNode(w.to.nodeId)
                       && stripTrackId(w.to.nodeId) == tid && reachesMaster(w.to.nodeId))
                    { connectedToMaster = true; break; }

            // Fallback insert order: when another track's chain feeds this track's
            // strip zone, the linear walk above (which starts at this track's own
            // source) never reaches those inserts, leaving insertOrderCount == 0 and
            // the strip FX silently bypassed. Walk the strip zone from its
            // externally-fed entry node instead.
            if(insertOrderCount == 0 && !track.inserts.empty())
            {
                uint32_t entry = 0;
                for(const auto &w : routeWires_)
                {
                    if(w.to.nodeId == masterNodeId() || !isStripNode(w.to.nodeId)
                       || stripTrackId(w.to.nodeId) != tid)
                        continue;
                    const bool fromOwnStrip = w.from.nodeId != masterNodeId()
                                              && isStripNode(w.from.nodeId)
                                              && stripTrackId(w.from.nodeId) == tid;
                    if(!fromOwnStrip) { entry = w.to.nodeId; break; }
                }
                uint32_t node = entry;
                for(int guard = 0; node != 0 && guard < synth::kMaxStripInserts; ++guard)
                {
                    const int local = stripLocalId(node);
                    const int ii = local - 1;
                    if(local <= 0 || ii >= int(track.inserts.size())
                       || ii >= synth::kMaxStripInserts || insertSeen[(size_t)ii])
                        break;
                    insertSeen[(size_t)ii] = true;
                    insertOrder[(size_t)insertOrderCount++] = uint8_t(ii);
                    uint32_t nextNode = 0;
                    const synth::GridPortRef out { node, 1 };
                    for(const auto &w : routeWires_)
                        if(samePort(w.from, out) && w.to.nodeId != masterNodeId()
                           && isStripNode(w.to.nodeId) && stripTrackId(w.to.nodeId) == tid)
                        { nextNode = w.to.nodeId; break; }
                    node = nextNode;
                }
            }

            track.perVoiceFilterOrder      = filterOrder;
            track.perVoiceFilterOrderCount = filterOrderCount;
            track.insertOrder              = insertOrder;
            track.insertOrderCount         = insertOrderCount;
            track.connectedToMaster        = connectedToMaster;
        }
        pushAllTracks(); // sends routing state (connectedToMaster, filterOrder, insertOrder) to engine
        if(auto *p = plugin())
            p->updateCompiledRoute(buildCompiledRoute()); // per-voice DAG (true merges)
    }

void KapibaraUI::computeSelectedSourceChain()
{
        selectedChainFilters_.clear();
        selectedChainInserts_.clear();
        if(selectedTrack_ < 0 || selectedTrack_ >= int(generator_.tracks.size()))
            return;
        const uint32_t srcId = generator_.tracks[(size_t)selectedTrack_].id;
        synth::GridPortRef cursor { sourceRouterNodeId(srcId), 0 };
        std::vector<uint32_t> visited;
        const int maxSteps = synth::kMaxPerVoiceFilters + synth::kMaxStripInserts + 8;
        for(int guard = 0; guard < maxSteps; ++guard)
        {
            const synth::GridWire *next = nullptr;
            for(const auto &w : routeWires_)
                if(samePort(w.from, cursor)) { next = &w; break; }
            if(next == nullptr)
                break;
            const uint32_t dest = next->to.nodeId;
            if(dest == masterNodeId())
                break;
            if(std::find(visited.begin(), visited.end(), dest) != visited.end())
                break; // cycle guard
            visited.push_back(dest);
            if(isPerVoiceNode(dest))
            {
                const int local = perVoiceLocalId(dest);
                if(local > 0) selectedChainFilters_.push_back(local - 1);
                cursor = { perVoiceNodeId(0u, local), 1 };
                continue;
            }
            if(isStripNode(dest))
            {
                const int local = stripLocalId(dest);
                if(local > 0) selectedChainInserts_.push_back({ stripTrackId(dest), local - 1 });
                cursor = { dest, 1 };
                continue;
            }
            if(isAmpEnvNode(dest))
            {
                // Amp-env nodes pass through (they aren't filters/FX); keep walking.
                cursor = { dest, 1 };
                continue;
            }
            break; // unknown node type
        }
    }

synth::CompiledPerVoiceRoute KapibaraUI::buildCompiledRoute() const
{
        using synth::RouteNodeRef;
        synth::CompiledPerVoiceRoute route;
        const int trackN = int(generator_.tracks.size());
        if(trackN <= 0)
            return route; // valid == false

        int filterCount = 0;
        for(const auto &t : generator_.tracks)
            filterCount = std::max(filterCount, clampi(t.perVoiceFilterCount, 0, synth::kMaxPerVoiceFilters));
        route.filterCount = filterCount;
        route.trackCount = std::min(trackN, synth::kMaxSourceTracks);
        for(int i = 0; i < route.trackCount; ++i)
            route.trackId[(size_t)i] = generator_.tracks[(size_t)i].id;

        const auto indexOfTrack = [&](uint32_t tid) -> int {
            for(int i = 0; i < route.trackCount; ++i)
                if(route.trackId[(size_t)i] == tid) return i;
            return -1;
        };
        // Resolve a wire's FROM port to a per-voice node ref (source / filter / amp-env).
        const auto refOfFrom = [&](uint32_t nodeId, RouteNodeRef &out) -> bool {
            if(isSourceRouterNode(nodeId))
            {
                out.kind = 0; out.id = sourceRouterTrackId(nodeId);
                return indexOfTrack(out.id) >= 0;
            }
            if(isPerVoiceNode(nodeId))
            {
                const int slot = perVoiceLocalId(nodeId) - 1;
                if(slot < 0 || slot >= filterCount) return false;
                out.kind = 1; out.id = uint32_t(slot);
                return true;
            }
            if(isAmpEnvNode(nodeId))
            {
                const int slot = ampEnvSlotId(nodeId);
                const int inst = ampEnvInstanceId(nodeId);
                if(slot < 0 || slot >= synth::kMaxAmpEnvs
                   || inst < 0 || inst >= synth::kMaxAmpEnvRouteNodes)
                    return false;
                out.kind = 2; out.id = uint32_t(inst);
                route.ampEnvSlot[(size_t)inst] = uint8_t(slot);
                route.ampEnvNodeCount = std::max(route.ampEnvNodeCount, inst + 1);
                return true;
            }
            return false; // strip / master / unknown
        };
        const auto pushInput = [](std::array<RouteNodeRef, synth::kMaxRouteInputs> &arr, uint8_t &count,
                                  const RouteNodeRef &r) {
            if(count >= synth::kMaxRouteInputs) return;
            for(uint8_t k = 0; k < count; ++k)
                if(arr[(size_t)k].kind == r.kind && arr[(size_t)k].id == r.id) return; // de-dup
            arr[(size_t)count++] = r;
        };

        // --- Compile each component's output-router structure into utility nodes ---
        // A main-router wire from {component C, port K} carries the structure signal
        // at OUT_K (component → utility chain → OUT_K). outFeeder maps that to a ref.
        std::unordered_map<uint64_t, RouteNodeRef> outFeeder;   // key (C<<8)|port
        std::unordered_map<uint64_t, int> utilIdxOf;            // key (C<<8)|utilLocal → util idx
        std::function<RouteNodeRef(uint32_t, int, int)> structRef =
            [&](uint32_t C, int local, int depth) -> RouteNodeRef {
                RouteNodeRef r {};
                if(depth > 24) return r;
                if(local == 1 || local == 2) { refOfFrom(C, r); return r; } // IN/component → component ref
                if(local >= 30)
                {
                    const uint64_t key = (uint64_t(C) << 8) | uint32_t(local);
                    auto it = utilIdxOf.find(key);
                    if(it != utilIdxOf.end()) { r.kind = 3; r.id = uint32_t(it->second); return r; }
                    if(route.utilCount >= synth::kMaxUtilNodes) return r;
                    const int idx = route.utilCount++;
                    utilIdxOf[key] = idx;
                    auto pit = structUtilParams_.find((uint64_t(C) << 8) | uint32_t(local - 30));
                    if(pit != structUtilParams_.end()) route.utilParams[(size_t)idx] = pit->second;
                    auto swit = structWires_.find(C);
                    if(swit != structWires_.end())
                        for(const auto &w : swit->second)
                            if(int(w.to.nodeId) == local)
                                pushInput(route.utilInputs[(size_t)idx], route.utilInputCount[(size_t)idx],
                                          structRef(C, int(w.from.nodeId), depth + 1));
                    r.kind = 3; r.id = uint32_t(idx);
                    return r;
                }
                return r; // OUT/unknown — not a feeder
            };
        for(const auto &w : routeWires_)
        {
            if(!(isPerVoiceNode(w.from.nodeId) || isAmpEnvNode(w.from.nodeId))) continue;
            const uint32_t C = w.from.nodeId;
            const int port = int(w.from.port);
            const uint64_t key = (uint64_t(C) << 8) | uint32_t(port);
            if(outFeeder.count(key)) continue;
            int feederLocal = 2; // default: the component itself feeds this output
            auto swit = structWires_.find(C);
            if(swit != structWires_.end())
                for(const auto &sw : swit->second)
                    if(int(sw.to.nodeId) == 10 + port) feederLocal = int(sw.from.nodeId);
            outFeeder[key] = structRef(C, feederLocal, 0);
        }
        // Resolve a wire FROM (with port) through the component structure.
        const auto resolveFrom = [&](const synth::GridPortRef &p, RouteNodeRef &out) -> bool {
            if(!refOfFrom(p.nodeId, out)) return false;
            auto it = outFeeder.find((uint64_t(p.nodeId) << 8) | uint32_t(p.port));
            if(it != outFeeder.end() && it->second.kind >= 1 && it->second.kind <= 3)
                out = it->second;
            return true;
        };

        // Pass A: filter-node and amp-env-node inputs.
        for(const auto &w : routeWires_)
        {
            RouteNodeRef ref;
            if(!resolveFrom(w.from, ref)) continue;
            if(isPerVoiceNode(w.to.nodeId))
            {
                const int slot = perVoiceLocalId(w.to.nodeId) - 1;
                if(slot < 0 || slot >= filterCount) continue;
                pushInput(route.filterInputs[(size_t)slot], route.filterInputCount[(size_t)slot], ref);
            }
            else if(isAmpEnvNode(w.to.nodeId))
            {
                const int slot = ampEnvSlotId(w.to.nodeId);
                const int e = ampEnvInstanceId(w.to.nodeId);
                if(slot < 0 || slot >= synth::kMaxAmpEnvs
                   || e < 0 || e >= synth::kMaxAmpEnvRouteNodes) continue;
                route.ampEnvSlot[(size_t)e] = uint8_t(slot);
                route.ampEnvNodeCount = std::max(route.ampEnvNodeCount, e + 1);
                pushInput(route.ampEnvInputs[(size_t)e], route.ampEnvInputCount[(size_t)e], ref);
            }
        }

        const auto nodeInputs = [&](const RouteNodeRef &n, const RouteNodeRef *&arr) -> int {
            if(n.kind == 1) { arr = route.filterInputs[(size_t)n.id].data(); return route.filterInputCount[(size_t)n.id]; }
            if(n.kind == 2) { arr = route.ampEnvInputs[(size_t)n.id].data(); return route.ampEnvInputCount[(size_t)n.id]; }
            if(n.kind == 3) { arr = route.utilInputs[(size_t)n.id].data(); return route.utilInputCount[(size_t)n.id]; }
            arr = nullptr; return 0;
        };

        // Lowest track index among a node's transitive source inputs (for master-direct sinks).
        const auto homeTrackIndex = [&](const RouteNodeRef &start) -> int {
            std::array<bool, synth::kMaxPerVoiceFilters> seenF {};
            std::array<bool, synth::kMaxAmpEnvRouteNodes> seenA {};
            std::array<bool, synth::kMaxUtilNodes> seenU {};
            std::vector<RouteNodeRef> stack;
            int best = -1;
            const auto consider = [&](const RouteNodeRef &r) {
                if(r.kind == 0) { const int idx = indexOfTrack(r.id); if(idx >= 0 && (best < 0 || idx < best)) best = idx; }
                else if(r.kind == 1 && r.id < uint32_t(filterCount) && !seenF[(size_t)r.id]) { seenF[(size_t)r.id] = true; stack.push_back(r); }
                else if(r.kind == 2 && r.id < uint32_t(synth::kMaxAmpEnvRouteNodes) && !seenA[(size_t)r.id]) { seenA[(size_t)r.id] = true; stack.push_back(r); }
                else if(r.kind == 3 && r.id < uint32_t(synth::kMaxUtilNodes) && !seenU[(size_t)r.id]) { seenU[(size_t)r.id] = true; stack.push_back(r); }
            };
            consider(start);
            while(!stack.empty())
            {
                const RouteNodeRef n = stack.back(); stack.pop_back();
                const RouteNodeRef *arr = nullptr; const int c = nodeInputs(n, arr);
                for(int k = 0; k < c; ++k) consider(arr[k]);
            }
            return best;
        };

        // Pass B: strip-bus / master feeders.
        for(const auto &w : routeWires_)
        {
            RouteNodeRef ref;
            if(!resolveFrom(w.from, ref)) continue;
            int sinkIdx = -1;
            // MASTER must be checked first: masterNodeId() shares the high nibble
            // of the strip-node id space, so isStripNode(master) is also true.
            if(w.to.nodeId == masterNodeId())  sinkIdx = homeTrackIndex(ref);
            else if(isStripNode(w.to.nodeId))  sinkIdx = indexOfTrack(stripTrackId(w.to.nodeId));
            if(sinkIdx < 0 || sinkIdx >= route.trackCount) continue;
            pushInput(route.busInputs[(size_t)sinkIdx], route.busInputCount[(size_t)sinkIdx], ref);
        }

        // Unified topological eval order over filter (kind 1) + amp-env (kind 2) nodes.
        std::vector<RouteNodeRef> nodes;
        for(int s = 0; s < filterCount; ++s) nodes.push_back({ 1, uint32_t(s) });
        for(int e = 0; e < synth::kMaxAmpEnvRouteNodes; ++e)
            if(route.ampEnvInputCount[(size_t)e] > 0) nodes.push_back({ 2, uint32_t(e) });
        for(int u = 0; u < route.utilCount; ++u) nodes.push_back({ 3, uint32_t(u) });
        std::vector<int> indeg(nodes.size(), 0);
        for(size_t i = 0; i < nodes.size(); ++i)
        {
            const RouteNodeRef *arr = nullptr; const int c = nodeInputs(nodes[i], arr);
            for(int k = 0; k < c; ++k) if(arr[k].kind >= 1 && arr[k].kind <= 3) indeg[i]++;
        }
        std::vector<bool> placed(nodes.size(), false);
        for(size_t iter = 0; iter < nodes.size(); ++iter)
        {
            int pick = -1;
            for(size_t i = 0; i < nodes.size(); ++i)
                if(!placed[i] && indeg[i] == 0) { pick = int(i); break; }
            if(pick < 0) break; // cycle — leftovers appended below
            placed[(size_t)pick] = true;
            route.evalOrder[(size_t)route.evalOrderCount++] = nodes[(size_t)pick];
            for(size_t i = 0; i < nodes.size(); ++i)
                if(!placed[i])
                {
                    const RouteNodeRef *arr = nullptr; const int c = nodeInputs(nodes[i], arr);
                    for(int k = 0; k < c; ++k)
                        if(arr[k].kind == nodes[(size_t)pick].kind && arr[k].id == nodes[(size_t)pick].id) indeg[i]--;
                }
        }
        for(size_t i = 0; i < nodes.size(); ++i)
            if(!placed[i]) route.evalOrder[(size_t)route.evalOrderCount++] = nodes[i];
        // Keep the filters-only order too (legacy paths / fallback).
        for(int i = 0; i < route.evalOrderCount; ++i)
            if(route.evalOrder[(size_t)i].kind == 1)
                route.filterOrder[(size_t)route.filterOrderCount++] = uint8_t(route.evalOrder[(size_t)i].id);

        // Per source: bypass the implicit source amp-env if it reaches an amp-env node.
        for(int ti = 0; ti < route.trackCount; ++ti)
        {
            std::vector<uint32_t> reach { sourceRouterNodeId(route.trackId[(size_t)ti]) };
            bool grew = true;
            while(grew)
            {
                grew = false;
                for(const auto &w : routeWires_)
                    if(std::find(reach.begin(), reach.end(), w.from.nodeId) != reach.end()
                       && std::find(reach.begin(), reach.end(), w.to.nodeId) == reach.end())
                    { reach.push_back(w.to.nodeId); grew = true; }
            }
            bool hasAmp = false;
            for(uint32_t n : reach) if(isAmpEnvNode(n)) { hasAmp = true; break; }
            route.sourceEnvBypass[(size_t)ti] = hasAmp ? 1 : 0;
        }

        route.valid = true;
        return route;
    }
END_NAMESPACE_DISTRHO
