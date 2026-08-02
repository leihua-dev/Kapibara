#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::handlePageClick(float x, float y)
{
        // Card picker (modal): pick an item, or click outside to dismiss. Runs
        // FIRST — nothing may steal its clicks (the MOD chip strip sits under it).
        if(gridPickerMode_ != 0)
        {
            for(size_t k = 0; k < gridPickerItemRects_.size(); ++k)
                if(gridPickerItemRects_[k].contains(x, y))
                {
                    const int idx = gridPickerPoolIdx_[k];
                    if(gridPickerMode_ == 3)
                    {
                        // Card source pick. -2 = staged "+ ROUTE": allocate only now,
                        // so a dismissed picker never leaves a ghost rule behind.
                        int ri = gridPickerRuleIdx_;
                        if(ri == -2)
                        {
                            ri = -1;
                            for(int i = 0; i < synth::kMaxMatrixRules; ++i)
                                if(!rules_[(size_t)i].enabled) { ri = i; break; }
                            if(ri >= 0)
                            {
                                auto &ru = rules_[(size_t)ri];
                                ru = synth::MatrixRule {};
                                ru.enabled = true;
                                ru.dest = synth::ModDestination::Amp;
                                if(const auto *t = currentTrack()) ru.targetTrackId = t->id;
                                ru.depth = defaultModulationDepth(ru.dest);
                            }
                        }
                        if(ri >= 0 && ri < synth::kMaxMatrixRules)
                        {
                            rules_[(size_t)ri].source = kCardSourcePool[idx];
                            enableModSource(kCardSourcePool[idx]);
                            selectedRule_ = ri;
                            matrixRoutesScrollTo_ = ri;
                            pushRule(ri);
                        }
                    }
                    else if(gridPickerMode_ == 4 && gridPickerRuleIdx_ >= 0
                            && gridPickerRuleIdx_ < synth::kMaxMatrixRules)
                    {
                        auto &ru = rules_[(size_t)gridPickerRuleIdx_];
                        ru.dest = kCardDestPool[idx];
                        if(std::abs(ru.depth) < 1.0e-6f)
                            ru.depth = defaultModulationDepth(ru.dest);
                        selectedRule_ = gridPickerRuleIdx_;
                        pushRule(gridPickerRuleIdx_);
                    }
                    else if(gridPickerMode_ == 5 && gridPickerRuleIdx_ >= 0
                            && gridPickerRuleIdx_ < synth::kMaskGroupSlots)
                    {
                        // Mask-group slot destination pick: enables the slot.
                        auto &t = maskGroups_[(size_t)clampi(selectedMaskGroup_, 0, synth::kMaxMaskGroups - 1)]
                                      .targets[(size_t)gridPickerRuleIdx_];
                        t.dest = kCardDestPool[idx];
                        t.enabled = true;
                        if(t.targetTrackId == 0)
                            if(const auto *ct = currentTrack()) t.targetTrackId = ct->id;
                        if(std::abs(t.depth) < 1.0e-6f)
                            t.depth = defaultModulationDepth(t.dest);
                        pushGroup(selectedMaskGroup_);
                    }
                    gridPickerMode_ = 0;
                    gridPickerRuleIdx_ = -1;
                    repaint();
                    return true;
                }
            gridPickerMode_ = 0;
            gridPickerRuleIdx_ = -1;
            repaint();
            return true;
        }
        // The filter slot/stage editor owns its pane outright, and it is claimed
        // HERE — ahead of every other handler except the modal picker above.
        // Several handlers further down keep rects that can sit under it (the
        // focus page tabs, the insert panel's mode button, the bottom layout
        // strip), and a press that reached one of those switched the whole view
        // out from under the user instead of editing the slot they clicked.
        // Nothing inside this rectangle belongs to anyone else.
        if(disperserEditorRect_.w > 0.0f && disperserEditorRect_.contains(x, y))
        {
            // Its own controls first, then the knobs the CONTROL page hosts in
            // the same rectangle, and only then swallow. Consuming outright left
            // every knob dead; letting everything through sent stray presses to
            // handlers that switched the whole view.
            if(!handleDisperserEditorPress(x, y))
                handleControlPress(x, y);
            repaint();
            return true;
        }
        // Bottom expand/collapse arrows + draggable MOD-source strip.
        if(handleBottomLayoutPress(x, y))
            return true;
        // Focused-component detail (close button + amp-env ADSR knobs).
        if(focusedNodeId_ != 0 && handleFocusedDetailPress(x, y))
            return true;
        // Per-Voice Chain editor (top-middle, or the filter-focus detail). Its rects
        // are cleared when not shown, so this is safe to always run (XOver excepted).
        if(multibandEditorTrackId_ < 0 && handlePerVoiceChainPress(x, y))
            return true;
        // Editor tab switch (SOURCE / SHAPE / VOICE / MAPPING).
        for(int i = 0; i < int(editorTabRects_.size()); ++i)
            if(editorTabRects_[(size_t)i].contains(x, y))
            {
                editorTab_ = i;
                repaint();
                return true;
            }
        // Handlers are gated by which view is showing so stale rects from a
        // hidden panel can't trigger phantom clicks. The bottom collapsed panel
        // hosts the modulator/amp-env editors (modMode); MATRIX route-card rects
        // are cleared by whichever top-row branch overdraws the view.
        const bool modMode    = (bottomPanelMode_ == 0);
        const bool structMode = (bottomPanelMode_ == 1);
        if(matrixViewOpen_ && matrixViewCloseRect_.w > 0.0f && matrixViewCloseRect_.contains(x, y))
        {
            matrixViewOpen_ = false;
            repaint();
            return true;
        }
        if(matrixViewInteractive() && matrixViewTab_ == 0 && handleMatrixRoutesPress(x, y))
            return true;
        if(matrixViewInteractive() && matrixViewTab_ == 1 && handleMaskGroupsPress(x, y))
            return true;
        if(matrixViewOpen_)
            for(int i = 0; i < 2; ++i)
                if(matrixViewTabRects_[(size_t)i].w > 0.0f
                   && matrixViewTabRects_[(size_t)i].contains(x, y))
                {
                    matrixViewTab_ = i;
                    repaint();
                    return true;
                }
        if(multibandEditorTrackId_ >= 0 && routeBoardRect_.contains(x, y))
        {
            multibandEditorTrackId_ = -1;
            multibandEditorInsertIdx_ = -1;
        }
        // A pending wire draft dropped on a source ROW creates an osc-mod entry —
        // must run before row selection consumes the click.
        if(handleModWireDrop(x, y))
            return true;
        // Source column is always present → its faders/selection are never gated.
        if(handleStripFaderPress(x, y))
            return true;
        if(structMode && handleRouteGraphClick(x, y))
            return true;
        // Insert-slot buttons must be checked before strip selection logic
        if(structMode && handleInsertButtonClick(x, y))
            return true;
        if(modMode && handleModColumnClick(x, y))
            return true;
        // FX Rack editor (top-right) is always present.
        if(handleRouteFxClick(x, y))
            return true;
        // FX Rack panel body → start a drag-to-swap.
        if(handleFxRackPress(x, y))
            return true;
        // OSC MOD rows live in the always-visible source editor, so no mode gate.
        if(handleModEditorClick(x, y))
            return true;
        return handleButtonClick(x, y) || handleControlPress(x, y);
    }

END_NAMESPACE_DISTRHO
