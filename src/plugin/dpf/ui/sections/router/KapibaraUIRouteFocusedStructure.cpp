#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
constexpr int kStructIn = 1, kStructComp = 2; // OUT k = 10+k, utility u = 30+u

bool structSamePt(const synth::GridPoint &a, const synth::GridPoint &b) { return a.x == b.x && a.y == b.y; }

// 45°-then-straight routing, same shape as the main router.
void structAppendPath(std::vector<synth::GridPoint> &pts, const synth::GridPoint &end)
{
        if(pts.empty()) { pts.push_back(end); return; }
        const auto s = pts.back();
        const int dx = end.x - s.x, dy = end.y - s.y;
        const int adx = std::abs(dx), ady = std::abs(dy);
        if(adx == 0 || ady == 0 || adx == ady) { if(!structSamePt(s, end)) pts.push_back(end); return; }
        const int d = std::min(adx, ady);
        const synth::GridPoint diag { s.x + (dx < 0 ? -d : d), s.y + (dy < 0 ? -d : d) };
        if(!structSamePt(pts.back(), diag)) pts.push_back(diag);
        if(!structSamePt(pts.back(), end)) pts.push_back(end);
}

std::vector<synth::GridPoint> structPath(const synth::GridPoint &a, const synth::GridPoint &b)
{
        std::vector<synth::GridPoint> pts { a };
        structAppendPath(pts, b);
        return pts;
}

float structPtSegDist2(float px, float py, float ax, float ay, float bx, float by)
{
        const float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
        if(l2 < 1e-4f) return (px - ax) * (px - ax) + (py - ay) * (py - ay);
        float t = ((px - ax) * dx + (py - ay) * dy) / l2;
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        const float cx = ax + t * dx, cy = ay + t * dy;
        return (px - cx) * (px - cx) + (py - cy) * (py - cy);
}

bool structNearPath(float px, float py, const std::vector<synth::GridPoint> &path, float thr)
{
        const float t2 = thr * thr;
        for(size_t i = 1; i < path.size(); ++i)
            if(structPtSegDist2(px, py, float(path[i - 1].x), float(path[i - 1].y),
                                float(path[i].x), float(path[i].y)) <= t2) return true;
        return false;
}

const char *structNodeLabel(int local, uint32_t comp, char *buf, int bufN)
{
        if(local == kStructIn) return "IN";
        if(local == kStructComp)
        {
            if((comp & 0xf0000000u) == 0x10000000u) { std::snprintf(buf, (size_t)bufN, "FILTER %d", int(comp & 0xfu)); return buf; }
            if((comp & 0xf0000000u) == 0x30000000u) return "AMP ENV";
            if((comp & 0xf0000000u) == 0x20000000u) return "FX";
            return "COMPONENT";
        }
        if(local >= 30) { std::snprintf(buf, (size_t)bufN, "UTIL %d", local - 30 + 1); return buf; }
        std::snprintf(buf, (size_t)bufN, "OUT %d", local - 10); return buf;
}

// A node's available ports: IN has only output; OUT nodes only input; others both.
bool structHasIn(int local) { return local != kStructIn; }
bool structHasOut(int local) { return !(local >= 10 && local < 30); }
// Utility nodes (local >= 30) are larger boxes that host their own param controls.
inline float structNodeW(int local) { return local >= 30 ? 138.0f : 84.0f; }
inline float structNodeH(int local) { return local >= 30 ? 68.0f  : 28.0f; }
}

int KapibaraUI::componentOutPortCount(uint32_t nodeId) const
{
        int n = 1;
        auto it = nodeOutPortCount_.find(nodeId);
        if(it != nodeOutPortCount_.end()) n = std::max(1, it->second);
        for(const auto &w : routeWires_)
            if(w.from.nodeId == nodeId) n = std::max(n, int(w.from.port)); // ports 1..N
        return std::min(n, 8);
    }

void KapibaraUI::drawComponentOutputPorts(const Rect &node, uint32_t nodeId)
{
        const int n = componentOutPortCount(nodeId);
        const float px = node.x + node.w - 4.0f;
        const float spacing = 9.0f;
        const float startY = node.y + node.h * 0.5f - float(n - 1) * spacing * 0.5f - 4.0f;
        for(int k = 0; k < n; ++k)
        {
            const Rect outP { px, startY + float(k) * spacing, 8.0f, 8.0f };
            routePortHits_.push_back(RoutePortHit { outP, synth::GridPortRef { nodeId, uint8_t(k + 1) }, true });
            beginPath();
            ellipse(outP.x + outP.w * 0.5f, outP.y + outP.h * 0.5f, 4.0f, 4.0f);
            fillColor(DesignTokens::accentGreen());
            fill();
        }
    }

void KapibaraUI::ensureStructureDefault(uint32_t comp)
{
        auto &wires = structWires_[comp];
        if(!wires.empty()) return;
        // Default: IN -> component -> OUT 1.
        synth::GridWire w1; w1.from = { kStructIn, 1 };  w1.to = { kStructComp, 0 };
        synth::GridWire w2; w2.from = { kStructComp, 1 }; w2.to = { 11u, 0 };
        wires.push_back(w1);
        wires.push_back(w2);
    }

void KapibaraUI::drawFocusedStructure(const Rect &r)
{
        const uint32_t C = focusedNodeId_;
        ensureStructureDefault(C);
        drawSectionTitle(r.x, r.y, "STRUCTURE  (output router - drag nodes, drag ports to wire)");
        structAddOutRect_ = {}; structRemoveOutRect_ = {}; structAddUtilRect_ = {};
        structNodeRects_.clear();
        structPortRects_.clear();
        structUtilCtrlHits_.clear();

        const int n = componentOutPortCount(C);
        const int M = structUtilCount_.count(C) ? structUtilCount_[C] : 0;

        struct SNode { int local; synth::GridPoint def; };
        std::vector<SNode> nodes;
        nodes.push_back({ kStructIn,   { int(r.x + 24.0f),         int(r.y + 46.0f) } });
        nodes.push_back({ kStructComp, { int(r.x + r.w * 0.30f),   int(r.y + 46.0f) } });
        for(int k = 1; k <= n; ++k)
            nodes.push_back({ 10 + k, { int(r.x + r.w * 0.80f), int(r.y + 40.0f + float(k - 1) * 44.0f) } });
        for(int u = 0; u < M; ++u)
            nodes.push_back({ 30 + u, { int(r.x + r.w * 0.44f), int(r.y + 58.0f + float(u) * 78.0f) } });

        auto &pos = structNodePos_[C];
        const auto portCenter = [&](int local, bool isOut) -> synth::GridPoint {
            const auto it = pos.find(local);
            const synth::GridPoint p = it != pos.end() ? it->second : synth::GridPoint { 0, 0 };
            return { p.x + (isOut ? int(structNodeW(local)) : 0), p.y };
        };

        for(int wi = 0; wi < int(structWires_[C].size()); ++wi)
        {
            const auto &w = structWires_[C][(size_t)wi];
            if(!pos.count(int(w.from.nodeId)) || !pos.count(int(w.to.nodeId))) continue;
            const auto path = structPath(portCenter(int(w.from.nodeId), true), portCenter(int(w.to.nodeId), false));
            if(path.size() < 2) continue;
            const bool sel = (wi == structSelectedWire_);
            beginPath();
            moveTo(float(path.front().x), float(path.front().y));
            for(size_t i = 1; i < path.size(); ++i) lineTo(float(path[i].x), float(path[i].y));
            strokeColor(sel ? rgba(0xffff50ffU) : rgba(0x9eff50cc));
            strokeWidth(sel ? 2.4f : 1.5f);
            stroke();
        }

        char buf[24];
        for(const auto &sn : nodes)
        {
            auto it = pos.find(sn.local);
            if(it == pos.end()) { pos[sn.local] = sn.def; it = pos.find(sn.local); }
            const synth::GridPoint p = it->second;
            const float nodeW = structNodeW(sn.local), nodeH = structNodeH(sn.local);
            const Rect node { float(p.x), float(p.y) - nodeH * 0.5f, nodeW, nodeH };
            const bool isComp = sn.local == kStructComp;
            const bool isOut = sn.local >= 10 && sn.local < 30;
            const bool isUtil = sn.local >= 30;
            drawPanel(node, rgba(0x101820ff),
                      isComp ? DesignTokens::accentGreen() : (sn.local == kStructIn ? rgba(0x6ea8ffaa)
                              : (isOut ? rgba(0xffc857aa) : rgba(0xb088ffaa))));
            if(isUtil)
            {
                const int u = sn.local - 30;
                auto &up = structUtilParams_[(uint64_t(C) << 8) | uint32_t(u)];
                const float pad = 5.0f;
                useUiFont(); uiFontSize(7.5f); fillColor(rgba(0xb088ffff));
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                text(node.x + pad, node.y + 8.0f, structNodeLabel(sn.local, C, buf, sizeof buf), nullptr);
                const Rect bandR { node.x + node.w - 42.0f, node.y + 3.0f, 38.0f, 11.0f };
                drawButton(bandR, "BAND", up.bandOn);
                structUtilCtrlHits_.push_back({ bandR, u, 4 });
                const float cw = node.w - pad * 2.0f;
                float yy = node.y + 16.0f;
                const Rect lvl { node.x + pad, yy, cw, 12.0f };
                drawSlider(lvl, "Lvl", up.level / 2.0f, up.level); structUtilCtrlHits_.push_back({ lvl, u, 0 }); yy += 13.0f;
                const Rect pan { node.x + pad, yy, cw, 12.0f };
                drawSlider(pan, "Pan", (up.pan + 1.0f) * 0.5f, up.pan); structUtilCtrlHits_.push_back({ pan, u, 1 }); yy += 13.0f;
                const float hw = (cw - 4.0f) * 0.5f;
                const Rect lo { node.x + pad, yy, hw, 12.0f };
                drawSlider(lo, "Lo", cutoffToNorm(up.bandLoHz), up.bandLoHz); structUtilCtrlHits_.push_back({ lo, u, 2 });
                const Rect hi { node.x + pad + hw + 4.0f, yy, hw, 12.0f };
                drawSlider(hi, "Hi", cutoffToNorm(up.bandHiHz), up.bandHiHz); structUtilCtrlHits_.push_back({ hi, u, 3 });
            }
            else
            {
                useUiFont(); uiFontSize(8.0f); fillColor(DesignTokens::textPrimary());
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                text(node.x + node.w * 0.5f, node.y + node.h * 0.5f, structNodeLabel(sn.local, C, buf, sizeof buf), nullptr);
            }
            structNodeRects_.push_back({ node, sn.local });
            if(structHasIn(sn.local))
            {
                const Rect ip { node.x - 4.0f, node.y + node.h * 0.5f - 4.0f, 8.0f, 8.0f };
                structPortRects_.push_back({ ip, sn.local, false });
                beginPath(); ellipse(ip.x + 4.0f, ip.y + 4.0f, 4.0f, 4.0f); fillColor(DesignTokens::accentCyan()); fill();
            }
            if(structHasOut(sn.local))
            {
                const Rect op { node.x + node.w - 4.0f, node.y + node.h * 0.5f - 4.0f, 8.0f, 8.0f };
                structPortRects_.push_back({ op, sn.local, true });
                beginPath(); ellipse(op.x + 4.0f, op.y + 4.0f, 4.0f, 4.0f); fillColor(DesignTokens::accentGreen()); fill();
            }
        }

        if(structDraftActive_ && structDraftFromLocal_ >= 0)
        {
            const auto a = portCenter(structDraftFromLocal_, true);
            const auto path = structPath(a, { int(structDraftX_), int(structDraftY_) });
            beginPath();
            moveTo(float(path.front().x), float(path.front().y));
            for(size_t i = 1; i < path.size(); ++i) lineTo(float(path[i].x), float(path[i].y));
            strokeColor(rgba(0x9eff5088)); strokeWidth(1.2f); stroke();
        }

        const float btnY = r.y + r.h - 24.0f;
        structAddOutRect_ = { r.x + 8.0f, btnY, 54.0f, 18.0f };
        drawButton(structAddOutRect_, "+ OUT", false);
        if(n > 1)
        {
            structRemoveOutRect_ = { r.x + 66.0f, btnY, 54.0f, 18.0f };
            drawButton(structRemoveOutRect_, "- OUT", false);
        }
        structAddUtilRect_ = { r.x + 130.0f, btnY, 84.0f, 18.0f };
        drawButton(structAddUtilRect_, "+ UTILITY", false);
        structUtilParamRects_.fill({}); // params now live inside each utility node
    }

bool KapibaraUI::handleStructurePress(float x, float y)
{
        const uint32_t C = focusedNodeId_;
        for(const auto &ch : structUtilCtrlHits_)
        {
            if(!std::get<0>(ch).contains(x, y)) continue;
            const int u = std::get<1>(ch), ctrl = std::get<2>(ch);
            auto &up = structUtilParams_[(uint64_t(C) << 8) | uint32_t(u)];
            structSelectedUtil_ = 30 + u;
            structSelectedWire_ = -1;
            if(ctrl == 4)
            {
                up.bandOn = !up.bandOn;
                if(auto *p = plugin()) p->updateCompiledRoute(buildCompiledRoute());
                return true;
            }
            structUtilParamDrag_ = ctrl;
            structUtilParamStartY_ = y;
            structUtilParamStartVal_ = (ctrl == 0) ? up.level / 2.0f
                                     : (ctrl == 1) ? (up.pan + 1.0f) * 0.5f
                                     : (ctrl == 2) ? cutoffToNorm(up.bandLoHz)
                                                   : cutoffToNorm(up.bandHiHz);
            return true;
        }
        for(const auto &pr : structPortRects_)
        {
            const Rect &rect = std::get<0>(pr);
            const int local = std::get<1>(pr);
            const bool isOut = std::get<2>(pr);
            if(!rect.contains(x, y)) continue;
            if(isOut)
            {
                structDraftActive_ = true;
                structDraftFromLocal_ = local;
                structDraftX_ = x; structDraftY_ = y;
                return true;
            }
            if(structDraftActive_ && structDraftFromLocal_ != local)
            {
                auto &wires = structWires_[C];
                if(structDraftFromLocal_ != kStructComp)
                    wires.erase(std::remove_if(wires.begin(), wires.end(),
                                               [&](const synth::GridWire &w) { return int(w.from.nodeId) == structDraftFromLocal_; }),
                                wires.end());
                const bool dup = std::any_of(wires.begin(), wires.end(), [&](const synth::GridWire &w) {
                    return int(w.from.nodeId) == structDraftFromLocal_ && int(w.to.nodeId) == local;
                });
                if(!dup)
                {
                    synth::GridWire w; w.from = { uint32_t(structDraftFromLocal_), 1 }; w.to = { uint32_t(local), 0 };
                    wires.push_back(w);
                    if(auto *p = plugin()) p->updateCompiledRoute(buildCompiledRoute());
                }
                structDraftActive_ = false;
                structDraftFromLocal_ = -1;
                return true;
            }
            structDraftActive_ = false;
            return true;
        }
        for(const auto &nr : structNodeRects_)
            if(nr.first.contains(x, y))
            {
                structDragLocal_ = nr.second;
                structDragOffX_ = x - nr.first.x;
                structDragOffY_ = y - (nr.first.y + nr.first.h * 0.5f);
                structSelectedWire_ = -1;
                structSelectedUtil_ = (nr.second >= 30) ? nr.second : -1;
                return true;
            }
        {
            auto &pos = structNodePos_[C];
            const auto pc = [&](int local, bool isOut) -> synth::GridPoint {
                const auto p = pos[local]; return { p.x + (isOut ? int(structNodeW(local)) : 0), p.y };
            };
            auto &wires = structWires_[C];
            for(int i = 0; i < int(wires.size()); ++i)
            {
                const int f = int(wires[i].from.nodeId), t = int(wires[i].to.nodeId);
                if(!pos.count(f) || !pos.count(t)) continue;
                const auto path = structPath(pc(f, true), pc(t, false));
                if(structNearPath(x, y, path, 5.0f)) { structSelectedWire_ = i; structDraftActive_ = false; return true; }
            }
        }
        structSelectedWire_ = -1;
        structDraftActive_ = false;
        return true;
    }

void KapibaraUI::finishStructureNodeDrag(float x, float y)
{
        (void)x; (void)y;
        const uint32_t C = focusedNodeId_;
        const int node = structDragLocal_;
        structDragLocal_ = -1;
        if(node < 0 || !structHasIn(node) || !structHasOut(node)) return;
        auto &pos = structNodePos_[C];
        if(!pos.count(node)) return;
        const float nodeW = 84.0f;
        const auto pc = [&](int local, bool isOut) -> synth::GridPoint {
            const auto p = pos[local];
            return { p.x + (isOut ? int(nodeW) : 0), p.y };
        };
        const synth::GridPoint nc = pos[node];
        const float cx = float(nc.x) + nodeW * 0.5f, cy = float(nc.y);
        auto &wires = structWires_[C];
        for(size_t i = 0; i < wires.size(); ++i)
        {
            const int f = int(wires[i].from.nodeId), t = int(wires[i].to.nodeId);
            if(f == node || t == node) continue;
            if(!pos.count(f) || !pos.count(t)) continue;
            const auto path = structPath(pc(f, true), pc(t, false));
            if(!structNearPath(cx, cy, path, 16.0f)) continue;
            const auto exists = [&](int a, int b) {
                return std::any_of(wires.begin(), wires.end(), [&](const synth::GridWire &w) {
                    return int(w.from.nodeId) == a && int(w.to.nodeId) == b; });
            };
            wires.erase(wires.begin() + long(i));
            if(!exists(f, node)) { synth::GridWire w; w.from = { uint32_t(f), 1 }; w.to = { uint32_t(node), 0 }; wires.push_back(w); }
            if(!exists(node, t)) { synth::GridWire w; w.from = { uint32_t(node), 1 }; w.to = { uint32_t(t), 0 }; wires.push_back(w); }
            if(auto *p = plugin()) p->updateCompiledRoute(buildCompiledRoute());
            break;
        }
    }

END_NAMESPACE_DISTRHO
