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

void KapibaraUI::drawModEditor(const Rect &region, synth::SourceTrackParams &track)
{
        drawSectionTitle(region.x, region.y, "MODULATION (source → this)");
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        const float rowH = std::max(20.0f, (region.h - 24.0f) / float(synth::kMaxTrackMods) - 4.0f);
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
        {
            auto &m = track.mods[(size_t)i];
            if(!modEntryActive(m) || m.sourceTrack >= int(generator_.tracks.size()))
                continue;
            const float ry = region.y + 24.0f + float(i) * (rowH + 4.0f);
            modSrcRects_[(size_t)i]    = { region.x, ry, region.w * 0.30f, rowH };
            modTypeRects_[(size_t)i]   = { modSrcRects_[(size_t)i].x + modSrcRects_[(size_t)i].w + 4.0f, ry, region.w * 0.18f, rowH };
            modDeleteRects_[(size_t)i] = { region.x + region.w - 22.0f, ry, 22.0f, rowH };
            modDepthRects_[(size_t)i]  = { modTypeRects_[(size_t)i].x + modTypeRects_[(size_t)i].w + 4.0f, ry,
                                          modDeleteRects_[(size_t)i].x - 4.0f - (modTypeRects_[(size_t)i].x + modTypeRects_[(size_t)i].w + 4.0f), rowH };
            std::snprintf(scratch_, sizeof(scratch_), "%s", generator_.tracks[(size_t)m.sourceTrack].name.c_str());
            drawButton(modSrcRects_[(size_t)i], scratch_, selectedModSlot_ == i);
            drawButton(modTypeRects_[(size_t)i], synth::sourceModTypeName(m.type), false);
            drawSlider(modDepthRects_[(size_t)i], "Depth", m.depth, m.depth);
            drawButton(modDeleteRects_[(size_t)i], "x", false);
        }
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
                m.type = static_cast<synth::SourceModType>((int(m.type) + 1) % synth::kSourceModTypeCount);
                pushCurrentTrack();
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
