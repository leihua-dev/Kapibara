#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Editor shown only for routed entries: [source] [type] [depth] [x]. One row per mod.


    // Next selectable modulator track after `current` (skips self & cyclic), -1 = none.

    // ---- Mod source picker menu ----


    // cycle check using an explicit self index (menu context)


    // Group OSC view: members → bus diagram + the group's route FX.
bool KapibaraUI::modSourceCausesCycle(int targetIdx, int srcIdx) const
{
        if(srcIdx < 0 || srcIdx == targetIdx)
            return srcIdx == targetIdx;
        // Follow dependency edges (track → its mod sources) from srcIdx; cycle if we reach target.
        std::array<bool, synth::kMaxSourceTracks> visited {};
        std::vector<int> stack { srcIdx };
        while(!stack.empty())
        {
            const int cur = stack.back();
            stack.pop_back();
            if(cur < 0 || cur >= int(generator_.tracks.size()) || visited[(size_t)cur])
                continue;
            visited[(size_t)cur] = true;
            for(const auto &m : generator_.tracks[(size_t)cur].mods)
            {
                if(!m.enabled || m.sourceTrack < 0)
                    continue;
                if(m.sourceTrack == targetIdx)
                    return true;
                stack.push_back(m.sourceTrack);
            }
        }
        return false;
    }

bool KapibaraUI::trackHasAnyMod(const synth::SourceTrackParams &t)
{
        for(const auto &m : t.mods)
            if(modEntryActive(m)) return true;
        return false;
    }

// OSC MOD zone in the source editor: this track is the carrier; each row is one
// audio-rate modulator entry [source track][mode][depth bar][x]. Divider + group
// label styling matches the UNISON zone directly above it.
void KapibaraUI::drawModEditor(const Rect &region, synth::SourceTrackParams &track)
{
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        oscModAddRect_ = {};

        strokeLine(region.x, region.y - 2.0f, region.x + region.w, region.y - 2.0f,
                   DesignTokens::divider(), 1.0f);
        drawGroupLabel(region.x, region.y + 2.0f, "OSC MOD");

        // "+ MOD" chip on the header line while a free slot remains.
        bool hasFree = false;
        for(const auto &m : track.mods)
            if(!modEntryActive(m)) { hasFree = true; break; }
        if(hasFree)
        {
            oscModAddRect_ = { region.x + region.w - 52.0f, region.y, 52.0f, 14.0f };
            drawButton(oscModAddRect_, "+ MOD", false);
        }

        constexpr float rowH = 20.0f;
        constexpr float rowGap = 4.0f;
        float ry = region.y + 18.0f;
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
        {
            auto &m = track.mods[(size_t)i];
            if(!modEntryActive(m) || m.sourceTrack >= int(generator_.tracks.size()))
                continue;
            const float srcW = std::max(70.0f, region.w * 0.30f);
            modSrcRects_[(size_t)i]    = { region.x, ry, srcW, rowH };
            modTypeRects_[(size_t)i]   = { region.x + srcW + 4.0f, ry, 42.0f, rowH };
            modDeleteRects_[(size_t)i] = { region.x + region.w - 16.0f, ry, 16.0f, rowH };
            const float dx = modTypeRects_[(size_t)i].x + modTypeRects_[(size_t)i].w + 6.0f;
            modDepthRects_[(size_t)i]  = { dx, ry, modDeleteRects_[(size_t)i].x - 6.0f - dx, rowH };

            char srcLbl[32];
            oscModSourceLabel(m, srcLbl, sizeof(srcLbl));
            drawButton(modSrcRects_[(size_t)i], srcLbl, selectedModSlot_ == i);
            drawButton(modTypeRects_[(size_t)i], synth::sourceModTypeName(m.type), false);
            // Mode colour swatch matching the wire colour in the SOURCE column.
            beginPath();
            roundedRect(modTypeRects_[(size_t)i].x + 2.0f, ry + 4.0f, 3.0f, rowH - 8.0f, 1.0f);
            fillColor(oscModTypeColor(m.type));
            fill();

            // Depth: recessed groove bar with a cyan fill + monospace value.
            const Rect &dr = modDepthRects_[(size_t)i];
            drawPanel(dr, DesignTokens::groove(), DesignTokens::border());
            const float fillW = clampf(m.depth, 0.0f, 1.0f) * (dr.w - 4.0f);
            if(fillW > 0.5f)
            {
                beginPath();
                roundedRect(dr.x + 2.0f, dr.y + 2.0f, fillW, dr.h - 4.0f, 1.5f);
                fillColor(DesignTokens::accentCyan().withAlpha(0.45f));
                fill();
            }
            char vb[16];
            std::snprintf(vb, sizeof(vb), "%.2f", m.depth);
            useMonoFont();
            uiFontSize(10.5f);
            textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textPrimary());
            text(dr.x + dr.w - 5.0f, dr.y + dr.h * 0.5f, vb, nullptr);

            drawButton(modDeleteRects_[(size_t)i], "x", false);
            ry += rowH + rowGap;
        }
    }

// Focused-detail "OSC MOD" page: the GLOBAL modulation network. Every track and
// component tap that participates in cross-track modulation is a node, laid out
// left→right by dependency depth, so series chains (A → FLT → B → C) and
// parallel fan-ins read directly. Phase-domain inputs (FM/PM) meet at a +
// node, amp-domain inputs (AM/Ring) at a x node — matching Voice's render.
void KapibaraUI::drawOscModDiagram(const Rect &r)
{
        // The generic OSC MOD row handlers must not misfire from stale rects while
        // the diagram page is up.
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        oscModAddRect_ = {};
        oscModDiagHits_.clear();

        const int n = std::min<int>(int(generator_.tracks.size()), synth::kMaxSourceTracks);

        // --- Node & edge collection -----------------------------------------
        struct DiagNode
        {
            int kind = 0;       // 0 = track, 1 = FLT tap, 2 = AE tap
            int track = -1;     // track index (kind 0) or home track (taps)
            int node = 0;       // tap slot / instance
            int depth = 0;
            bool carrier = false;
            Rect box {};
        };
        struct DiagEdge
        {
            int from = -1, to = -1;      // node indices
            int carrier = -1, slot = -1; // mod entry behind the chip (-1 = audio feed)
            synth::SourceModType type = synth::SourceModType::AM;
            float depth = 0.0f;
            bool audioFeed = false;      // dim home → tap feed line
        };
        std::vector<DiagNode> nodes;
        std::vector<DiagEdge> edges;
        const auto ensureNode = [&](int kind, int track, int node) -> int {
            for(size_t idx = 0; idx < nodes.size(); ++idx)
                if(nodes[idx].kind == kind
                   && (kind == 0 ? nodes[idx].track == track : nodes[idx].node == node))
                    return int(idx);
            DiagNode d; d.kind = kind; d.track = track; d.node = node;
            nodes.push_back(d);
            return int(nodes.size()) - 1;
        };
        for(int i = 0; i < n; ++i)
        {
            const auto &carrier = generator_.tracks[(size_t)i];
            for(int k = 0; k < synth::kMaxTrackMods; ++k)
            {
                const auto &m = carrier.mods[(size_t)k];
                if(!modEntryActive(m) || m.sourceTrack < 0 || m.sourceTrack >= n)
                    continue;
                const int carIdx = ensureNode(0, i, 0);
                nodes[(size_t)carIdx].carrier = true;
                int srcIdx;
                if(m.sourceKind == 0)
                    srcIdx = ensureNode(0, m.sourceTrack, 0);
                else
                {
                    const int homeIdx = ensureNode(0, m.sourceTrack, 0);
                    srcIdx = ensureNode(int(m.sourceKind), m.sourceTrack, int(m.sourceNode));
                    bool haveFeed = false;
                    for(const auto &e : edges)
                        if(e.audioFeed && e.from == homeIdx && e.to == srcIdx) { haveFeed = true; break; }
                    if(!haveFeed)
                    {
                        DiagEdge fe; fe.from = homeIdx; fe.to = srcIdx; fe.audioFeed = true;
                        edges.push_back(fe);
                    }
                }
                DiagEdge e;
                e.from = srcIdx; e.to = ensureNode(0, i, 0);
                e.carrier = i; e.slot = k; e.type = m.type; e.depth = m.depth;
                edges.push_back(e);
            }
        }

        if(edges.empty())
        {
            useUiFont();
            uiFontSize(10.0f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(r.x + r.w * 0.5f, r.y + r.h * 0.5f,
                 "no osc mods anywhere — drag a wire from a component output onto a source row,"
                 " or use + MOD in the source editor", nullptr);
            return;
        }

        // --- Longest-path depth (the mod graph is acyclic) -------------------
        for(size_t pass = 0; pass <= nodes.size(); ++pass)
        {
            bool changed = false;
            for(const auto &e : edges)
                if(nodes[(size_t)e.to].depth < nodes[(size_t)e.from].depth + 1)
                { nodes[(size_t)e.to].depth = nodes[(size_t)e.from].depth + 1; changed = true; }
            if(!changed) break;
        }
        int maxDepth = 0;
        for(const auto &d : nodes) maxDepth = std::max(maxDepth, d.depth);

        // --- Layout: one column per depth, rows stacked & centred ------------
        constexpr float trackW = 132.0f, tapW = 92.0f, rowPitch = 60.0f;
        const float colGap = maxDepth > 0
                                 ? std::max(trackW + 40.0f, (r.w - trackW - 52.0f) / float(maxDepth))
                                 : 0.0f;
        std::vector<int> counts((size_t)maxDepth + 1, 0), placed((size_t)maxDepth + 1, 0);
        for(const auto &d : nodes) counts[(size_t)d.depth]++;
        for(auto &d : nodes)
        {
            const float bw = d.kind == 0 ? trackW : tapW;
            const float bh = (d.kind == 0 && d.carrier) ? 46.0f : 26.0f;
            const float colH = float(counts[(size_t)d.depth]) * rowPitch;
            const float y0 = r.y + std::max(4.0f, (r.h - 18.0f) * 0.5f - colH * 0.5f);
            d.box = { r.x + 8.0f + colGap * float(d.depth),
                      y0 + float(placed[(size_t)d.depth]) * rowPitch, bw, bh };
            placed[(size_t)d.depth]++;
        }

        const auto arrowHead = [&](float ax, float ay, Color c) {
            beginPath();
            moveTo(ax, ay);
            lineTo(ax - 7.0f, ay - 4.0f);
            lineTo(ax - 7.0f, ay + 4.0f);
            closePath();
            fillColor(c);
            fill();
        };
        // Port rows on a carrier box (phase / amp / sync).
        const auto portY = [&](const DiagNode &d, int domain) {
            if(!(d.kind == 0 && d.carrier)) return d.box.y + d.box.h * 0.5f;
            return d.box.y + 10.0f + 13.0f * float(domain);
        };
        const auto domainOf = [](synth::SourceModType t) {
            if(t == synth::SourceModType::FM || t == synth::SourceModType::PM) return 0;
            if(t == synth::SourceModType::HardSync) return 2;
            return 1;
        };

        // --- Edges (under the boxes) -----------------------------------------
        const auto drawWire = [&](float x0, float y0, float x1, float y1, Color c, float w) {
            beginPath();
            moveTo(x0, y0);
            const float mx = x0 + (x1 - x0) * 0.5f;
            bezierTo(mx, y0, mx, y1, x1, y1);
            strokeColor(c);
            strokeWidth(w);
            stroke();
        };
        // Combine nodes: carriers with ≥2 inputs in one domain get a +/x circle.
        std::vector<std::array<int, 3>> fanIn(nodes.size(), std::array<int, 3> { 0, 0, 0 });
        for(const auto &e : edges)
            if(!e.audioFeed) fanIn[(size_t)e.to][(size_t)domainOf(e.type)]++;

        for(const auto &e : edges)
        {
            const DiagNode &a = nodes[(size_t)e.from];
            const DiagNode &b = nodes[(size_t)e.to];
            const float x0 = a.box.x + a.box.w;
            const float y0 = a.box.y + a.box.h * 0.5f;
            if(e.audioFeed)
            {
                // Dim feed line: the tap component carries the home track's signal.
                drawWire(x0, y0, b.box.x, b.box.y + b.box.h * 0.5f,
                         DesignTokens::textSecondary().withAlpha(0.35f), 1.0f);
                continue;
            }
            const int dom = domainOf(e.type);
            const Color col = oscModTypeColor(e.type);
            const float py = portY(b, dom);
            const bool viaCombine = fanIn[(size_t)e.to][(size_t)dom] >= 2 && dom != 2;
            const float endX = viaCombine ? b.box.x - 26.0f : b.box.x - 2.0f;
            drawWire(x0, y0, endX - (viaCombine ? 10.0f : 6.0f), py, col.withAlpha(0.85f), 1.5f);
            if(!viaCombine)
                arrowHead(b.box.x - 1.0f, py, col);

            // Clickable mode chip on the wire.
            char lbl[24];
            std::snprintf(lbl, sizeof(lbl), "%s %.2f", synth::sourceModTypeName(e.type), e.depth);
            const float chipW = 66.0f, chipH = 15.0f;
            const Rect chip { (x0 + endX) * 0.5f - chipW * 0.5f,
                              (y0 + py) * 0.5f - chipH * 0.5f, chipW, chipH };
            drawPanel(chip, rgba(0x101820f0), col.withAlpha(0.9f));
            useMonoFont();
            uiFontSize(8.5f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(col);
            text(chip.x + chip.w * 0.5f, chip.y + chip.h * 0.5f, lbl, nullptr);
            oscModDiagHits_.push_back(OscModDiagHit { chip, e.carrier, e.slot });
        }
        // Combine circles + short arrow into the port.
        for(size_t ni = 0; ni < nodes.size(); ++ni)
        {
            const DiagNode &d = nodes[ni];
            if(!(d.kind == 0 && d.carrier)) continue;
            for(int dom = 0; dom < 2; ++dom)
            {
                if(fanIn[ni][(size_t)dom] < 2) continue;
                const float py = portY(d, dom);
                const float cx = d.box.x - 26.0f;
                const Color c = dom == 0 ? rgba(0xe8b34aff) : DesignTokens::accentCyan();
                beginPath();
                circle(cx, py, 9.0f);
                fillColor(rgba(0x101820ff));
                fill();
                strokeColor(c);
                strokeWidth(1.5f);
                stroke();
                useUiFont();
                uiFontSize(10.0f);
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(c);
                text(cx, py, dom == 0 ? "+" : "x", nullptr);
                strokeLine(cx + 9.0f, py, d.box.x - 8.0f, py, c, 1.5f);
                arrowHead(d.box.x - 1.0f, py, c);
            }
        }

        // --- Boxes (over the wires) -------------------------------------------
        for(const auto &d : nodes)
        {
            if(d.kind == 0)
            {
                const auto &t = generator_.tracks[(size_t)d.track];
                const bool isFocused = (focusedNodeId_ & 0xff000000u) == 0x08000000u
                                       && (focusedNodeId_ & 0x00ffffffu) == t.id;
                drawPanel(d.box, rgba(0x111a21ff),
                          isFocused ? DesignTokens::accentGreen() : rgba(0x3b5560ff));
                useUiFont();
                uiFontSize(9.5f);
                textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
                fillColor(DesignTokens::textPrimary());
                scissor(d.box.x + 2.0f, d.box.y, d.box.w - 4.0f, d.box.h);
                text(d.box.x + d.box.w - 7.0f, d.box.y + d.box.h * 0.5f, t.name.c_str(), nullptr);
                resetScissor();
                if(d.carrier)
                {
                    static const char *kPorts[3] = { "PHASE", "AMP", "SYNC" };
                    uiFontSize(7.0f);
                    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                    for(int dom = 0; dom < 3; ++dom)
                    {
                        const bool used = fanIn[(size_t)(&d - nodes.data())][(size_t)dom] > 0;
                        fillColor(used ? DesignTokens::textSecondary()
                                       : DesignTokens::textSecondary().withAlpha(0.35f));
                        text(d.box.x + 5.0f, portY(d, dom), kPorts[dom], nullptr);
                    }
                }
                // Audio-out stub for tracks that reach MASTER.
                if(t.connectedToMaster)
                {
                    const float oy = d.box.y + d.box.h * 0.5f;
                    strokeLine(d.box.x + d.box.w, oy, d.box.x + d.box.w + 16.0f, oy,
                               DesignTokens::accentGreen().withAlpha(0.8f), 1.5f);
                    arrowHead(d.box.x + d.box.w + 21.0f, oy, DesignTokens::accentGreen().withAlpha(0.8f));
                }
            }
            else
            {
                // Component tap chip (FLT n / AE n).
                drawPanel(d.box, rgba(0x101820ff), rgba(0x5b7380ff));
                char lbl[16];
                if(d.kind == 1)
                    std::snprintf(lbl, sizeof(lbl), "FLT %d", d.node + 1);
                else
                {
                    const int slot = (d.node >= 0 && d.node < int(ampEnvRouteNodeSlots_.size()))
                                         ? int(ampEnvRouteNodeSlots_[(size_t)d.node]) : 0;
                    std::snprintf(lbl, sizeof(lbl), "AE %d", slot + 1);
                }
                useUiFont();
                uiFontSize(9.0f);
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(DesignTokens::textSecondary());
                text(d.box.x + d.box.w * 0.5f, d.box.y + d.box.h * 0.5f, lbl, nullptr);
            }
        }

        // Honest footer: the mod graph is acyclic today.
        useUiFont();
        uiFontSize(8.0f);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textSecondary().withAlpha(0.6f));
        text(r.x + 8.0f, r.y + r.h - 4.0f,
             "global osc-mod network - click a chip to change mode / remove - depth edits in the source editor - feedback loops not supported yet",
             nullptr);
    }

bool KapibaraUI::handleModColumnClick(float x, float y)
{
        for(const auto &hit : modHits_)
            if(hit.rect.contains(x, y))
            {
                const int ti = trackIndexOfId(uint32_t(hit.trackId));
                if(ti >= 0)
                {
                    selectedTrack_ = ti;
                    selectedGroupView_ = -1;
                }
                if(hit.slot < 0)          // "+ add" row → pick a source
                    openModSourceMenu(hit.trackId, -1, x, y);
                else
                    selectedModSlot_ = hit.slot;  // select existing entry for OSC editing
                return true;
            }
        return false;
    }

bool KapibaraUI::handleModEditorClick(float x, float y)
{
        auto *track = currentTrack();
        if(track == nullptr)
            return false;
        if(oscModAddRect_.w > 0.0f && oscModAddRect_.contains(x, y))
        {
            openModSourceMenu(int(track->id), -1, x, y);
            return true;
        }
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
        {
            auto &m = track->mods[(size_t)i];
            if(modSrcRects_[(size_t)i].w > 0.0f && modSrcRects_[(size_t)i].contains(x, y))
            {
                // click source name → reselect via the source picker
                openModSourceMenu(int(track->id), i, x, y);
                return true;
            }
            if(modTypeRects_[(size_t)i].w > 0.0f && modTypeRects_[(size_t)i].contains(x, y))
            {
                // Pick from the full menu instead of blind-cycling five modes.
                selectedModSlot_ = i;
                openOscModTypeMenu(int(track->id), i, x, y);
                return true;
            }
            if(modDeleteRects_[(size_t)i].w > 0.0f && modDeleteRects_[(size_t)i].contains(x, y))
            {
                m = synth::SourceModEntry {};  // remove entry
                if(selectedModSlot_ == i) selectedModSlot_ = -1;
                pushCurrentTrack();
                return true;
            }
        }
        return false;
    }

void KapibaraUI::openModSourceMenu(int trackId, int slot, float x, float y)
{
        modSourceMenuTrackId_ = trackId;
        modSourceMenuSlot_ = slot;
        const int rows = int(generator_.tracks.size()) + 1;  // remove + tracks
        modSourceMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - 150.0f));
        modSourceMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - (26.0f + float(rows) * 18.0f)));
        modSourceMenuOpen_ = true;
    }

void KapibaraUI::drawModSourceMenu()
{
        if(!modSourceMenuOpen_)
            return;
        const int self = trackIndexOfId(uint32_t(modSourceMenuTrackId_));
        const int n = int(generator_.tracks.size());
        constexpr float rowH = 18.0f;
        const float menuW = 150.0f;
        const Rect panel { modSourceMenuX_, modSourceMenuY_, menuW, 22.0f + rowH * float(n + 1) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, "Mod source", nullptr);
        modSourceMenuRects_.fill({});
        modSourceMenuRects_[0] = { panel.x + 6.0f, panel.y + 20.0f, menuW - 12.0f, rowH - 2.0f };
        drawButton(modSourceMenuRects_[0], "(remove)", false);
        for(int i = 0; i < n && i + 1 < int(modSourceMenuRects_.size()); ++i)
        {
            modSourceMenuRects_[(size_t)(i + 1)] = { panel.x + 6.0f, panel.y + 20.0f + float(i + 1) * rowH, menuW - 12.0f, rowH - 2.0f };
            const bool disabled = (i == self) || modSourceCausesCycleFor(self, i);
            std::snprintf(scratch_, sizeof(scratch_), "%s%s", generator_.tracks[(size_t)i].name.c_str(),
                          disabled ? "  (n/a)" : "");
            drawButton(modSourceMenuRects_[(size_t)(i + 1)], scratch_, false);
        }
    }

bool KapibaraUI::modSourceCausesCycleFor(int selfIdx, int cand) const
{
        if(cand < 0 || cand == selfIdx) return cand == selfIdx;
        std::array<bool, synth::kMaxSourceTracks> visited {};
        std::vector<int> stack { cand };
        while(!stack.empty())
        {
            const int cur = stack.back(); stack.pop_back();
            if(cur < 0 || cur >= int(generator_.tracks.size()) || visited[(size_t)cur]) continue;
            visited[(size_t)cur] = true;
            for(const auto &m : generator_.tracks[(size_t)cur].mods)
            {
                if(!m.enabled || m.sourceTrack < 0) continue;
                if(m.sourceTrack == selfIdx) return true;
                stack.push_back(m.sourceTrack);
            }
        }
        return false;
    }

bool KapibaraUI::handleModSourceMenuClick(float x, float y)
{
        if(!modSourceMenuOpen_)
            return false;
        modSourceMenuOpen_ = false;
        const int self = trackIndexOfId(uint32_t(modSourceMenuTrackId_));
        if(self < 0)
            return true;
        auto &track = generator_.tracks[(size_t)self];
        // find target slot: existing slot, or first free
        int slot = modSourceMenuSlot_;
        if(slot < 0)
        {
            slot = -1;
            for(int i = 0; i < synth::kMaxTrackMods; ++i)
                if(!modEntryActive(track.mods[(size_t)i])) { slot = i; break; }
        }
        if(modSourceMenuRects_[0].contains(x, y))
        {
            if(modSourceMenuSlot_ >= 0)
            {
                track.mods[(size_t)modSourceMenuSlot_] = synth::SourceModEntry {};
                pushTrackById(track.id);
            }
            return true;
        }
        const int n = int(generator_.tracks.size());
        for(int i = 0; i < n; ++i)
        {
            if(!modSourceMenuRects_[(size_t)(i + 1)].contains(x, y))
                continue;
            if(i == self || modSourceCausesCycleFor(self, i) || slot < 0)
                return true;  // invalid choice / no free slot
            auto &m = track.mods[(size_t)slot];
            m.sourceTrack = int8_t(i);
            m.sourceKind = 0;  // picking a track resets any component tap
            m.sourceNode = 0;
            m.enabled = true;
            if(m.depth <= 0.0f) m.depth = 0.5f;
            selectedTrack_ = self;
            selectedModSlot_ = slot;
            pushTrackById(track.id);
            return true;
        }
        return true;
    }

END_NAMESPACE_DISTRHO
