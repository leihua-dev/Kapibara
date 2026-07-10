#include "../router/KapibaraUIRouteBoardShared.hpp"

START_NAMESPACE_DISTRHO

// OSC MOD type picker: choose how one source modulates another (AM / Ring / FM /
// PM / Sync) or remove the entry. Opened from the OSC MOD row's mode button, a
// right-click on a mod-wire dot in the SOURCE column, or right after dropping a
// wire onto a source row.

namespace
{
constexpr synth::SourceModType kOscModTypes[5] = {
    synth::SourceModType::AM, synth::SourceModType::RingMod,
    synth::SourceModType::FM, synth::SourceModType::PM,
    synth::SourceModType::HardSync
};
} // namespace

void KapibaraUI::openOscModTypeMenu(int trackId, int slot, float x, float y)
{
        oscModTypeMenuTrackId_ = trackId;
        oscModTypeMenuSlot_ = slot;
        constexpr float rowH = 20.0f;
        constexpr float menuW = 130.0f;
        constexpr int rows = 6; // 5 modes + remove
        oscModTypeMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - menuW));
        oscModTypeMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - (22.0f + rowH * float(rows))));
        oscModTypeMenuOpen_ = true;
    }

void KapibaraUI::drawOscModTypeMenu()
{
        if(!oscModTypeMenuOpen_)
            return;
        constexpr float rowH = 20.0f;
        constexpr float menuW = 130.0f;
        const Rect panel { oscModTypeMenuX_, oscModTypeMenuY_, menuW, 22.0f + rowH * 6.0f };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, "Osc mod", nullptr);

        synth::SourceModType current = synth::SourceModType::AM;
        const int ti = trackIndexOfId(uint32_t(oscModTypeMenuTrackId_));
        if(ti >= 0 && oscModTypeMenuSlot_ >= 0 && oscModTypeMenuSlot_ < synth::kMaxTrackMods)
            current = generator_.tracks[(size_t)ti].mods[(size_t)oscModTypeMenuSlot_].type;

        for(int i = 0; i < 5; ++i)
        {
            oscModTypeMenuRects_[(size_t)i] = { panel.x + 6.0f, panel.y + 20.0f + float(i) * rowH,
                                                menuW - 12.0f, rowH - 2.0f };
            drawButton(oscModTypeMenuRects_[(size_t)i], synth::sourceModTypeName(kOscModTypes[i]),
                       kOscModTypes[i] == current);
            // Mode colour swatch on the row's left edge.
            const Rect &rr = oscModTypeMenuRects_[(size_t)i];
            beginPath();
            roundedRect(rr.x + 2.0f, rr.y + 3.0f, 3.0f, rr.h - 6.0f, 1.0f);
            fillColor(oscModTypeColor(kOscModTypes[i]));
            fill();
        }
        oscModTypeMenuRects_[5] = { panel.x + 6.0f, panel.y + 20.0f + 5.0f * rowH, menuW - 12.0f, rowH - 2.0f };
        drawButton(oscModTypeMenuRects_[5], "(remove)", false);
    }

bool KapibaraUI::handleOscModTypeMenuClick(float x, float y)
{
        if(!oscModTypeMenuOpen_)
            return false;
        oscModTypeMenuOpen_ = false;
        const int ti = trackIndexOfId(uint32_t(oscModTypeMenuTrackId_));
        if(ti < 0 || oscModTypeMenuSlot_ < 0 || oscModTypeMenuSlot_ >= synth::kMaxTrackMods)
            return true;
        auto &track = generator_.tracks[(size_t)ti];
        auto &m = track.mods[(size_t)oscModTypeMenuSlot_];
        for(int i = 0; i < 5; ++i)
        {
            if(!oscModTypeMenuRects_[(size_t)i].contains(x, y))
                continue;
            m.type = kOscModTypes[i];
            pushTrackById(track.id);
            return true;
        }
        if(oscModTypeMenuRects_[5].contains(x, y))
        {
            m = synth::SourceModEntry {};
            if(selectedModSlot_ == oscModTypeMenuSlot_) selectedModSlot_ = -1;
            pushTrackById(track.id);
        }
        return true;
    }

// Right-click one of the mod-wire input dots in the SOURCE column.
bool KapibaraUI::handleOscModDotRightClick(float x, float y)
{
        const int n = std::min<int>(int(generator_.tracks.size()), synth::kMaxSourceTracks);
        for(int i = 0; i < n; ++i)
            for(int k = 0; k < synth::kMaxTrackMods; ++k)
            {
                const Rect &d = oscModDotRects_[(size_t)i][(size_t)k];
                if(d.w <= 0.0f || !d.contains(x, y))
                    continue;
                selectedTrack_ = i;
                selectedModSlot_ = k;
                openOscModTypeMenu(int(generator_.tracks[(size_t)i].id), k, x, y);
                return true;
            }
        return false;
    }

// A pending route-wire draft dropped onto a SOURCE row body: the draft's source
// component becomes an audio-rate modulator of that row's track. Sources tap the
// track output; per-voice filter and amp-env nodes tap that node's output
// (engine evaluates the node before the carrier renders).
bool KapibaraUI::handleModWireDrop(float x, float y)
{
        if(!routeWireDraft_.active)
            return false;
        const uint32_t fromNode = routeWireDraft_.from.nodeId;
        int kind = -1, node = 0;
        if(routeui::isSourceRouterNode(fromNode))
            kind = 0;
        else if(routeui::isPerVoiceNode(fromNode))
        {
            const int local = routeui::perVoiceLocalId(fromNode);
            if(local <= 0 || local > synth::kMaxPerVoiceFilters)
                return false;
            kind = 1;
            node = local - 1;
        }
        else if(routeui::isAmpEnvNode(fromNode))
        {
            const int inst = routeui::ampEnvInstanceId(fromNode);
            if(inst < 0 || inst >= synth::kMaxAmpEnvRouteNodes)
                return false;
            kind = 2;
            node = inst;
        }
        else
            return false;

        // Forward wire reachability from `start` to `target`.
        const auto reaches = [&](uint32_t start, uint32_t target) {
            std::vector<uint32_t> reach { start };
            bool grew = true;
            while(grew)
            {
                grew = false;
                for(const auto &w : routeWires_)
                    if(std::find(reach.begin(), reach.end(), w.from.nodeId) != reach.end()
                       && std::find(reach.begin(), reach.end(), w.to.nodeId) == reach.end())
                    { reach.push_back(w.to.nodeId); grew = true; }
            }
            return std::find(reach.begin(), reach.end(), target) != reach.end();
        };

        const int n = std::min<int>(int(generator_.tracks.size()), synth::kMaxSourceTracks);
        for(int i = 0; i < n; ++i)
        {
            const Rect &row = sourceRouterRects_[(size_t)i];
            if(row.w <= 0.0f || !row.contains(x, y))
                continue;
            // Not on the row's output port — that continues a normal audio wire.
            if(sourceRouterOutputRects_[(size_t)i].contains(x, y))
                return false;
            auto &carrier = generator_.tracks[(size_t)i];
            routeWireDraft_ = {};  // the draft is consumed either way

            // Home (feeder) track: the tapped node's signal ultimately comes from
            // some source; for track taps it's the track itself.
            int srcIdx = -1;
            if(kind == 0)
                srcIdx = trackIndexOfId(routeui::sourceRouterTrackId(fromNode));
            else
                for(int t = 0; t < n; ++t)
                    if(reaches(routeui::sourceRouterNodeId(generator_.tracks[(size_t)t].id), fromNode))
                    { srcIdx = t; break; }
            if(srcIdx < 0 || srcIdx == i || modSourceCausesCycleFor(i, srcIdx))
                return true;
            // A component fed by the carrier itself would be a feedback loop.
            if(kind != 0 && reaches(routeui::sourceRouterNodeId(carrier.id), fromNode))
                return true;

            // Reuse the entry with the same tap, else the first free slot.
            int slot = -1;
            for(int k = 0; k < synth::kMaxTrackMods; ++k)
            {
                const auto &e = carrier.mods[(size_t)k];
                if(modEntryActive(e) && e.sourceKind == uint8_t(kind)
                   && e.sourceNode == uint8_t(node)
                   && (kind != 0 || e.sourceTrack == srcIdx))
                { slot = k; break; }
            }
            if(slot < 0)
                for(int k = 0; k < synth::kMaxTrackMods; ++k)
                    if(!modEntryActive(carrier.mods[(size_t)k])) { slot = k; break; }
            if(slot < 0)
                return true; // all slots used
            auto &m = carrier.mods[(size_t)slot];
            m.sourceTrack = int8_t(srcIdx);
            m.sourceKind = uint8_t(kind);
            m.sourceNode = uint8_t(node);
            m.enabled = true;
            if(m.depth <= 0.0f) m.depth = 0.5f;
            selectedTrack_ = i;
            selectedGroupView_ = -1;
            selectedModSlot_ = slot;
            pushTrackById(carrier.id);
            openOscModTypeMenu(int(carrier.id), slot, x, y);  // pick the mode right away
            return true;
        }
        return false;
    }

// Short display label for a mod entry's source: track name, "FLT n", or "AE n".
void KapibaraUI::oscModSourceLabel(const synth::SourceModEntry &m, char *buf, size_t n) const
{
        if(m.sourceKind == 1)
        {
            std::snprintf(buf, n, "FLT %d", int(m.sourceNode) + 1);
            return;
        }
        if(m.sourceKind == 2)
        {
            const int inst = int(m.sourceNode);
            const int slot = (inst >= 0 && inst < int(ampEnvRouteNodeSlots_.size()))
                                 ? int(ampEnvRouteNodeSlots_[(size_t)inst]) : 0;
            std::snprintf(buf, n, "AE %d", slot + 1);
            return;
        }
        if(m.sourceTrack >= 0 && m.sourceTrack < int(generator_.tracks.size()))
            std::snprintf(buf, n, "%s", generator_.tracks[(size_t)m.sourceTrack].name.c_str());
        else
            std::snprintf(buf, n, "-");
    }

END_NAMESPACE_DISTRHO
