#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Route-FX editor: shows the controls for whichever strip-grid insert is active.
// Effects are always on unless the per-insert bypass toggle is set.
    // Flattened route-FX editor: all inserts laid out left→right, arrows showing order.
    // Each panel: header (type + mode + bypass + delete) and 4 generic knobs.
void KapibaraUI::beginModRouteDrag(synth::ModSource source, const Rect &sourceRect, float x, float y)
{
        modRouteDragActive_ = true;
        modRouteDragMoved_ = false;
        modRouteSource_ = source;
        modRouteSourceRect_ = sourceRect;
        modRouteStartX_ = modRouteMouseX_ = x;
        modRouteStartY_ = modRouteMouseY_ = y;
        modRouteHover_ = {};
    }

void KapibaraUI::finishModRouteDrag(float x, float y)
{
        // While the MATRIX view is up, the knob targets are hidden — dropping a
        // chip routes via the grid / card list instead.
        if(matrixViewOpen_)
        {
            const auto *track = currentTrack();
            if(matrixViewTab_ == 0 && track != nullptr)
            {
                // Drop on a cell or a dest column header = route chip source
                // to that column's destination for the current track.
                synth::ModDestination dest {};
                bool haveDest = false;
                for(const auto &c : matrixGridCells_)
                    if(c.rect.contains(x, y)) { dest = c.dst; haveDest = true; break; }
                if(!haveDest)
                    for(size_t d = 0; d < gridDestLabelRects_.size() && d < gridDests_.size(); ++d)
                        if(gridDestLabelRects_[d].contains(x, y))
                        { dest = gridDests_[d]; haveDest = true; break; }
                if(haveDest)
                {
                    int idx = -1, freeIdx = -1;
                    for(int i = 0; i < synth::kMaxMatrixRules; ++i)
                    {
                        const auto &ru = rules_[(size_t)i];
                        if(ru.enabled && ru.source == modRouteSource_ && ru.dest == dest
                           && ru.targetTrackId == track->id) { idx = i; break; }
                        if(freeIdx < 0 && !ru.enabled) freeIdx = i;
                    }
                    if(idx < 0 && freeIdx >= 0)
                    {
                        idx = freeIdx;
                        auto &ru = rules_[(size_t)idx];
                        ru = synth::MatrixRule {};
                        ru.enabled = true;
                        ru.source = modRouteSource_;
                        ru.dest = dest;
                        ru.targetTrackId = track->id;
                        ru.depth = defaultModulationDepth(dest);
                        enableModSource(modRouteSource_);
                    }
                    if(idx >= 0)
                    {
                        selectedRule_ = idx;
                        pushRule(idx);
                    }
                }
            }
            else if(matrixViewTab_ == 1 && matrixRoutesListRect_.w > 0.0f
                    && matrixRoutesListRect_.contains(x, y))
            {
                // Drop on the card list = new card with the chip as source; pick
                // the destination right away.
                int freeIdx = -1;
                for(int i = 0; i < synth::kMaxMatrixRules; ++i)
                    if(!rules_[(size_t)i].enabled) { freeIdx = i; break; }
                if(freeIdx >= 0)
                {
                    auto &ru = rules_[(size_t)freeIdx];
                    ru = synth::MatrixRule {};
                    ru.enabled = true;
                    ru.source = modRouteSource_;
                    ru.dest = synth::ModDestination::Amp;
                    if(track != nullptr) ru.targetTrackId = track->id;
                    ru.depth = defaultModulationDepth(ru.dest);
                    enableModSource(modRouteSource_);
                    selectedRule_ = freeIdx;
                    matrixRoutesScrollTo_ = freeIdx;
                    pushRule(freeIdx);
                    gridPickerMode_ = 4;
                    gridPickerRuleIdx_ = freeIdx;
                    gridPickerX_ = x;
                    gridPickerY_ = y;
                }
            }
            return;
        }
        const auto target = modRouteTargetAt(x, y);
        if(!target.valid)
            return;
        const bool isEffectDest = synth::insertModParamForDest(target.destination) >= 0;
        int ruleIndex = -1, freeIndex = -1;
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
        {
            const auto &rule = rules_[(size_t)i];
            // Only ENABLED rules count as a match — matching a disabled slot
            // would resurrect whatever stale config it still carries.
            if(rule.enabled && rule.source == modRouteSource_ && rule.dest == target.destination
               && rule.targetTrackId == target.trackId
               && (!isEffectDest || rule.targetSlot == target.slot))
            {
                ruleIndex = i;
                break;
            }
            if(freeIndex < 0 && !rule.enabled)
                freeIndex = i;
        }
        if(ruleIndex < 0 && freeIndex >= 0)
        {
            ruleIndex = freeIndex;
            rules_[(size_t)ruleIndex] = synth::MatrixRule {};  // fresh slot, no stale fields
        }
        if(ruleIndex < 0)
            return;
        selectedRule_ = ruleIndex;
        auto &rule = rules_[(size_t)ruleIndex];
        rule.enabled = true;
        rule.source = modRouteSource_;
        rule.dest = target.destination;
        rule.targetTrackId = target.trackId;
        rule.targetSlot = target.slot;
        rule.weight = synth::WeightMode::All;
        if(std::abs(rule.depth) < 1.0e-6f)
            rule.depth = defaultModulationDepth(rule.dest);
        if(modRouteSource_ >= synth::ModSource::Lfo1 && modRouteSource_ <= synth::ModSource::Lfo4)
        {
            int s = int(modRouteSource_) - int(synth::ModSource::Lfo1);
            selectedMatrixModSlot_ = s;
            modSlots_[(size_t)s].enabled = true;
        }
        else if(modRouteSource_ >= synth::ModSource::Env1 && modRouteSource_ <= synth::ModSource::Env4)
        {
            int s = synth::kMaxLfos + int(modRouteSource_) - int(synth::ModSource::Env1);
            selectedMatrixModSlot_ = s;
            modSlots_[(size_t)s].enabled = true;
        }
        else if(modRouteSource_ >= synth::ModSource::Adsr1 && modRouteSource_ <= synth::ModSource::Adsr4)
        {
            selectedAmpEnv_ = int(modRouteSource_) - int(synth::ModSource::Adsr1);
        }
        pushMatrix();
    }

bool KapibaraUI::handleModDepthPress(float x, float y)
{
        const auto *track = currentTrack();
        if(track == nullptr)
            return false;
        for(int offset = 0; offset < synth::kMaxMatrixRules; ++offset)
        {
            const int index = (selectedRule_ + offset) % synth::kMaxMatrixRules;
            const auto &rule = rules_[(size_t)index];
            if(!rule.enabled || rule.targetTrackId != track->id)
                continue;
            const Rect rect = modulationDestinationRect(rule);
            if(rect.w <= 0.0f)
                continue;
            const float cx = rect.x + rect.w * 0.5f;
            const float cy = rect.y + rect.h * 0.5f;
            const float radius = std::min(rect.w, rect.h) * 0.5f + 4.0f;
            const float distance = std::hypot(x - cx, y - cy);
            if(std::abs(distance - radius) > 5.0f)
                continue;
            selectedRule_ = index;
            dragTarget_ = DragTarget::ModDepth;
            dragStartY_ = y;
            dragStartDepth_ = rule.depth;
            dragDepthLimit_ = modulationDepthLimit(rule.dest);
            return true;
        }
        return false;
    }

void KapibaraUI::drawModulationOverlays()
{
        const auto *track = currentTrack();
        if(track != nullptr)
        {
            for(const auto &rule : rules_)
            {
                if(!rule.enabled || rule.targetTrackId != track->id)
                    continue;
                const Rect rect = modulationDestinationRect(rule);
                if(rect.w <= 0.0f)
                    continue;
                const Color color = modulationSourceColor(rule.source);
                const float cx = rect.x + rect.w * 0.5f;
                const float cy = rect.y + rect.h * 0.5f;
                const float radius = std::min(rect.w, rect.h) * 0.5f + 4.0f;
                beginPath();
                arc(cx, cy, radius, -0.75f * kPi, 0.75f * kPi, CW);
                strokeColor(rgba(0x263842ff));
                strokeWidth(3.0f);
                stroke();
                const float amount = clampf(std::abs(rule.depth) / modulationDepthLimit(rule.dest), 0.0f, 1.0f);
                beginPath();
                arc(cx, cy, radius, -0.75f * kPi, (-0.75f + 1.5f * amount) * kPi, CW);
                strokeColor(color);
                strokeWidth(3.0f);
                stroke();
            }
        }

        // Highlight routed/dragging sources with a colored BORDER only, so the
        // button label underneath stays visible (no opaque overlay).
        const auto highlightSource = [&](const Rect &rc, Color col) {
            beginPath();
            roundedRect(rc.x + 1.0f, rc.y + 1.0f, rc.w - 2.0f, rc.h - 2.0f, 6.0f);
            strokeColor(col);
            strokeWidth(2.0f);
            stroke();
        };
        for(int i = 0; i < synth::kMaxLfos; ++i)
        {
            const auto source = static_cast<synth::ModSource>(int(synth::ModSource::Lfo1) + i);
            const bool routed = std::any_of(rules_.begin(), rules_.end(),
                                            [source](const auto &r) { return r.enabled && r.source == source; });
            if(routed || (modRouteDragActive_ && modRouteSource_ == source))
                highlightSource(modSlotSelectRects_[(size_t)i], modulationSourceColor(source));
        }
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
        {
            const auto source = static_cast<synth::ModSource>(int(synth::ModSource::Env1) + i);
            const bool routed = std::any_of(rules_.begin(), rules_.end(),
                                            [source](const auto &r) { return r.enabled && r.source == source; });
            if(routed || (modRouteDragActive_ && modRouteSource_ == source))
                highlightSource(modSlotSelectRects_[(size_t)(synth::kMaxLfos + i)], modulationSourceColor(source));
        }
        if(modRouteDragActive_ && modRouteDragMoved_)
        {
            const Color color = modulationSourceColor(modRouteSource_);
            strokeLine(modRouteSourceRect_.x + modRouteSourceRect_.w * 0.5f,
                       modRouteSourceRect_.y + modRouteSourceRect_.h * 0.5f,
                       modRouteMouseX_, modRouteMouseY_, color, 2.5f);
            if(modRouteHover_.valid)
                highlightSource(modRouteHover_.rect, color);
        }
    }

END_NAMESPACE_DISTRHO
