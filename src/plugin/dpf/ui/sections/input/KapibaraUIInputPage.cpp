#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::handlePageClick(float x, float y)
{
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
        // Grid axis picker (open): pick an item, or click outside to dismiss.
        if(gridPickerMode_ != 0)
        {
            for(size_t k = 0; k < gridPickerItemRects_.size(); ++k)
                if(gridPickerItemRects_[k].contains(x, y))
                {
                    const int idx = gridPickerPoolIdx_[k];
                    if(gridPickerMode_ == 1) gridSources_.push_back(kGridSourcePool[idx]);
                    else                     gridDests_.push_back(kGridDestPool[idx]);
                    gridPickerMode_ = 0;
                    repaint();
                    return true;
                }
            gridPickerMode_ = 0;
            repaint();
            return true;
        }
        // Bottom panel handlers are gated by which workspace is showing so stale
        // rects from the hidden panel can't trigger phantom clicks.
        const bool modMode    = (bottomPanelMode_ == 0); // Modulation / Matrix
        const bool structMode = (bottomPanelMode_ == 1); // Source Structure
        if(multibandEditorTrackId_ >= 0 && routeBoardRect_.contains(x, y))
        {
            multibandEditorTrackId_ = -1;
            multibandEditorInsertIdx_ = -1;
        }
        // Matrix dashboard tab switch (GRID / MODULATORS / AMP ENV).
        if(modMode)
        for(int i = 0; i < int(matrixTabRects_.size()); ++i)
            if(matrixTabRects_[(size_t)i].contains(x, y))
            {
                matrixTab_ = i;
                repaint();
                return true;
            }
        // Grid axis "+" add buttons.
        if(modMode && gridAddSrcRect_.contains(x, y))
        {
            gridPickerMode_ = 1;
            gridPickerX_ = gridAddSrcRect_.x;
            gridPickerY_ = gridAddSrcRect_.y + 22.0f;
            repaint();
            return true;
        }
        if(modMode && gridAddDstRect_.contains(x, y))
        {
            gridPickerMode_ = 2;
            gridPickerX_ = gridAddDstRect_.x;
            gridPickerY_ = gridAddDstRect_.y + 18.0f;
            repaint();
            return true;
        }
        // Matrix grid node create / depth-drag.
        if(modMode && handleMatrixGridPress(x, y))
            return true;
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
