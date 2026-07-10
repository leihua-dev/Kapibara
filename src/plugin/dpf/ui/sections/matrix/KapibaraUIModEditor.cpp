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

// Focused-detail "OSC MOD" page: a signal diagram of how this carrier is
// modulated. Phase-domain entries (FM/PM) sum into a Σ node feeding the PHASE
// input, amp-domain entries (AM/Ring) combine multiplicatively at the AMP
// stage, and a Sync entry resets phase — matching how Voice actually renders.
void KapibaraUI::drawOscModDiagram(const Rect &r, synth::SourceTrackParams &track)
{
        // The generic OSC MOD row handlers must not misfire from stale rects while
        // the diagram page is up.
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        oscModAddRect_ = {};
        oscModDiagRects_.fill({});

        const auto arrowHead = [&](float ax, float ay, Color c) {
            beginPath();
            moveTo(ax, ay);
            lineTo(ax - 7.0f, ay - 4.0f);
            lineTo(ax - 7.0f, ay + 4.0f);
            closePath();
            fillColor(c);
            fill();
        };

        // Partition active entries by domain.
        std::array<int, synth::kMaxTrackMods> phaseSlots {}, ampSlots {}, syncSlots {};
        int nPhase = 0, nAmp = 0, nSync = 0, nAll = 0;
        for(int k = 0; k < synth::kMaxTrackMods; ++k)
        {
            const auto &m = track.mods[(size_t)k];
            if(!modEntryActive(m) || m.sourceTrack < 0 || m.sourceTrack >= int(generator_.tracks.size()))
                continue;
            ++nAll;
            if(m.type == synth::SourceModType::FM || m.type == synth::SourceModType::PM)
                phaseSlots[(size_t)nPhase++] = k;
            else if(m.type == synth::SourceModType::HardSync)
                syncSlots[(size_t)nSync++] = k;
            else
                ampSlots[(size_t)nAmp++] = k;
        }

        if(nAll == 0)
        {
            useUiFont();
            uiFontSize(10.0f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(r.x + r.w * 0.5f, r.y + r.h * 0.5f,
                 "no osc mods — drag a wire from another source onto this track's row,"
                 " or use + MOD in the source editor", nullptr);
            return;
        }

        // Carrier box on the right with OUT arrow and the three input stages.
        const Rect carrier { r.x + r.w - 200.0f, r.y + r.h * 0.5f - 27.0f, 150.0f, 54.0f };
        drawPanel(carrier, rgba(0x17242cff), DesignTokens::accentGreen());
        useUiFont();
        uiFontSize(10.5f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        scissor(carrier.x + 2.0f, carrier.y, carrier.w - 4.0f, carrier.h);
        text(carrier.x + carrier.w * 0.5f, carrier.y + carrier.h * 0.5f, track.name.c_str(), nullptr);
        resetScissor();
        strokeLine(carrier.x + carrier.w, carrier.y + carrier.h * 0.5f,
                   r.x + r.w - 22.0f, carrier.y + carrier.h * 0.5f,
                   DesignTokens::accentGreen(), 1.5f);
        arrowHead(r.x + r.w - 22.0f, carrier.y + carrier.h * 0.5f, DesignTokens::accentGreen());
        uiFontSize(8.5f);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textSecondary());
        text(carrier.x + carrier.w + 6.0f, carrier.y + carrier.h * 0.5f - 4.0f, "OUT", nullptr);

        const float phaseY = carrier.y + 12.0f;
        const float ampY   = carrier.y + 27.0f;
        const float syncY  = carrier.y + 42.0f;
        uiFontSize(7.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(nPhase > 0 ? DesignTokens::textPrimary() : DesignTokens::textSecondary().withAlpha(0.4f));
        text(carrier.x + 5.0f, phaseY, "PHASE", nullptr);
        fillColor(nAmp > 0 ? DesignTokens::textPrimary() : DesignTokens::textSecondary().withAlpha(0.4f));
        text(carrier.x + 5.0f, ampY, "AMP", nullptr);
        fillColor(nSync > 0 ? DesignTokens::textPrimary() : DesignTokens::textSecondary().withAlpha(0.4f));
        text(carrier.x + 5.0f, syncY, "SYNC", nullptr);

        // Modulator boxes on the left (slot order), coloured by mode.
        constexpr float boxW = 150.0f, boxH = 26.0f, boxGap = 12.0f;
        const float listH = float(nAll) * boxH + float(nAll - 1) * boxGap;
        float by = r.y + std::max(6.0f, r.h * 0.5f - listH * 0.5f);
        std::array<float, synth::kMaxTrackMods> boxMidY {};
        for(int k = 0; k < synth::kMaxTrackMods; ++k)
        {
            const auto &m = track.mods[(size_t)k];
            if(!modEntryActive(m) || m.sourceTrack < 0 || m.sourceTrack >= int(generator_.tracks.size()))
                continue;
            const Color col = oscModTypeColor(m.type);
            const Rect box { r.x + 8.0f, by, boxW, boxH };
            oscModDiagRects_[(size_t)k] = box;
            boxMidY[(size_t)k] = box.y + box.h * 0.5f;
            drawPanel(box, rgba(0x101820ff), col.withAlpha(0.8f));
            useUiFont();
            uiFontSize(9.0f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textPrimary());
            char srcLbl[32];
            oscModSourceLabel(m, srcLbl, sizeof(srcLbl));
            scissor(box.x + 4.0f, box.y, box.w * 0.55f, box.h);
            text(box.x + 6.0f, box.y + box.h * 0.5f, srcLbl, nullptr);
            resetScissor();
            char lbl[24];
            std::snprintf(lbl, sizeof(lbl), "%s %.2f", synth::sourceModTypeName(m.type), m.depth);
            useMonoFont();
            uiFontSize(9.0f);
            textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
            fillColor(col);
            text(box.x + box.w - 6.0f, box.y + box.h * 0.5f, lbl, nullptr);
            by += boxH + boxGap;
        }

        // Combine nodes between the boxes and the carrier inputs.
        const float leftEdge = r.x + 8.0f + boxW;
        const float combineX = leftEdge + (carrier.x - leftEdge) * 0.52f;
        const auto wireTo = [&](int slot, float nx, float ny, Color c) {
            const float sy = boxMidY[(size_t)slot];
            beginPath();
            moveTo(leftEdge, sy);
            bezierTo(leftEdge + (nx - leftEdge) * 0.5f, sy, leftEdge + (nx - leftEdge) * 0.5f, ny, nx, ny);
            strokeColor(c.withAlpha(0.85f));
            strokeWidth(1.5f);
            stroke();
        };
        const auto combineNode = [&](float cx, float cy, const char *sym, Color c) {
            beginPath();
            circle(cx, cy, 10.0f);
            fillColor(rgba(0x101820ff));
            fill();
            strokeColor(c);
            strokeWidth(1.5f);
            stroke();
            useUiFont();
            uiFontSize(11.0f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(c);
            text(cx, cy, sym, nullptr);
        };
        if(nPhase > 0)
        {
            const Color c = rgba(0xe8b34aff); // phase-domain amber
            for(int p = 0; p < nPhase; ++p)
                wireTo(phaseSlots[(size_t)p], combineX - 10.0f, phaseY, oscModTypeColor(track.mods[(size_t)phaseSlots[(size_t)p]].type));
            combineNode(combineX, phaseY, "+", c); // Σ: FM/PM offsets sum into the phase input
            strokeLine(combineX + 10.0f, phaseY, carrier.x - 8.0f, phaseY, c, 1.5f);
            arrowHead(carrier.x - 1.0f, phaseY, c);
        }
        if(nAmp > 0)
        {
            const Color c = DesignTokens::accentCyan();
            for(int p = 0; p < nAmp; ++p)
                wireTo(ampSlots[(size_t)p], combineX - 10.0f, ampY, oscModTypeColor(track.mods[(size_t)ampSlots[(size_t)p]].type));
            combineNode(combineX, ampY, "x", c); // AM/Ring apply multiplicatively in series
            strokeLine(combineX + 10.0f, ampY, carrier.x - 8.0f, ampY, c, 1.5f);
            arrowHead(carrier.x - 1.0f, ampY, c);
        }
        if(nSync > 0)
        {
            const Color c = oscModTypeColor(synth::SourceModType::HardSync);
            for(int p = 0; p < nSync; ++p)
                wireTo(syncSlots[(size_t)p], carrier.x - 8.0f, syncY, c);
            arrowHead(carrier.x - 1.0f, syncY, c);
        }

        // Honest footer: the mod graph is acyclic today.
        useUiFont();
        uiFontSize(8.0f);
        textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(DesignTokens::textSecondary().withAlpha(0.6f));
        text(r.x + 8.0f, r.y + r.h - 4.0f,
             "click a box to change mode / remove - depth edits in the source editor - feedback loops not supported yet",
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
