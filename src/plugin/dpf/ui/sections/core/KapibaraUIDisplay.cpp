#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Floating label that follows the cursor while reordering a strip insert.


    // 周期性刷新，驱动 LFO / 失真曲线的时变动画与实时波形预览
void KapibaraUI::onNanoDisplay()
{
        metaFramesShown_ = false; // set true by any frame strip drawn this frame
        updateLetterbox();
        // Fill the whole real window with the letterbox border colour.
        beginPath();
        rect(0.0f, 0.0f, realW_, realH_);
        fillColor(rgba(0x05070aff));
        fill();
        // Draw the fixed-aspect UI inside the centered letterbox region.
        save();
        translate(lbX_, lbY_);
        scale(uiRenderScale_, uiRenderScale_);
        drawBackground();
        drawToolbar();
        drawCurrentPage();
        drawModulationOverlays();
        drawPresetMenu();
        drawOptionsMenu();
        drawHarmonicEditor();
        drawKeyboard();
        drawWavetablePresetMenu();
        drawMetaProcessContextMenu();
        drawRouteContextMenu();
        drawInsertMenu();
        drawModeMenu();
        drawModSourceMenu();
        drawWarpModeMenu();
        drawOscModTypeMenu();
        drawBasicOscModMenu();
        drawWavetableImportMenu();
        drawGridAxisPicker();
        drawInsertDragGhost();
        restore();
    }

void KapibaraUI::drawInsertDragGhost()
{
        if(!insertDragActive_ || insertPendSlot_ < 0)
            return;
        auto *chain = insertChainFor(insertPendTrackId_, insertPendMerge_);
        if(chain == nullptr || insertPendSlot_ >= int(chain->size()))
            return;
        const auto &ins = (*chain)[(size_t)insertPendSlot_];
        const Rect g { insertDragX_ + 8.0f, insertDragY_ - 8.0f, 70.0f, 16.0f };
        drawPanel(g, rgba(0x17242cf0), rgba(0x9eff50ffU));
        fontSize(8.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(g.x + g.w * 0.5f, g.y + g.h * 0.5f, insertTypeName(ins.kind), nullptr);
    }

void KapibaraUI::onResize(const ResizeEvent &ev)
{
        UI::onResize(ev);
        realW_ = float(ev.size.getWidth());
        realH_ = float(ev.size.getHeight());
        updateLetterbox();
        repaint();
    }

void KapibaraUI::uiIdle()
{
        animPhase_ += 0.12f;
        if(animPhase_ > 1.0e6f) animPhase_ = 0.0f;
        repaint();
    }

void KapibaraUI::uiFocus(bool focus, DGL_NAMESPACE::CrossingMode mode)
{
        (void)mode;
        if(!focus)
        {
            releaseAllUiNotes();
            modRouteDragActive_ = false;
            modRouteDragMoved_ = false;
            modRouteHover_ = {};
            dragTarget_ = DragTarget::None;
            repaint();
        }
    }

END_NAMESPACE_DISTRHO
