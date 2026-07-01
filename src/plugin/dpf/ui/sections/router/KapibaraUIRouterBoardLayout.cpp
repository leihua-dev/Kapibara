#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawBottomWorkspace(const Rect &full)
{
        // Replicate the route-board setup drawStripRack used to do.
        cleanupRouteGraphForCurrentTracks();
        sourceRouterRects_.fill({});
        sourceRouterOutputRects_.fill({});
        perVoiceFilterNodeRects_.fill({});
        perVoiceFilterAddRect_ = {};
        stripGridMasterRect_ = {};
        stripRects_.fill({});
        stripGroupBusRects_.fill({});
        stripGainRects_.fill({});
        stripPanRects_.fill({});
        stripSendRects_.fill({});
        stripMuteRects_.fill({});
        stripSoloRects_.fill({});
        insertHits_.clear();
        routePortHits_.clear();
        routeNodeHits_.clear();
        routeBoardRect_ = full;

        drawPanel(full, rgba(0x0b1217ff), rgba(0x344852ff));

        const bool expanded = (bottomPanelMode_ == 1);
        const float sgap = 10.0f;
        const float arrowW = 18.0f;
        const float sourceW = clampf(full.w * 0.13f, 110.0f, 155.0f);
        const Rect sourceCol { full.x + 8.0f, full.y + 8.0f, sourceW, full.h - 16.0f };
        drawSourceRouter(sourceCol); // the Source column is ALWAYS present

        const float arrowY = sourceCol.y + sourceCol.h * 0.5f - 14.0f;
        const auto drawArrow = [&](const Rect &a, bool pointRight) {
            drawPanel(a, rgba(0x101820ff), rgba(0x70d77a99));
            const float cx = a.x + a.w * 0.5f, cy = a.y + a.h * 0.5f;
            beginPath();
            if(pointRight) { moveTo(cx - 3.0f, cy - 5.0f); lineTo(cx + 4.0f, cy); lineTo(cx - 3.0f, cy + 5.0f); }
            else           { moveTo(cx + 3.0f, cy - 5.0f); lineTo(cx - 4.0f, cy); lineTo(cx + 3.0f, cy + 5.0f); }
            closePath();
            fillColor(DesignTokens::accentGreen());
            fill();
        };

        if(!expanded)
        {
            // Collapsed: a right-arrow at the source edge expands; Modulation fills the rest.
            const Rect expandArrow { sourceCol.x + sourceCol.w + 2.0f, arrowY, arrowW, 28.0f };
            bottomExpandArrowRect_ = expandArrow;
            bottomCollapseArrowRect_ = {};
            const float mx = expandArrow.x + arrowW + sgap;
            const Rect matrixRegion { mx, full.y + 8.0f, full.x + full.w - 8.0f - mx, full.h - 16.0f };
            drawMatrixDashboard(matrixRegion);
            drawArrow(expandArrow, true);
        }
        else
        {
            // Expanded: the full router; a left-arrow at the far right collapses.
            const Rect collapseArrow { full.x + full.w - arrowW - 6.0f, arrowY, arrowW, 28.0f };
            bottomCollapseArrowRect_ = collapseArrow;
            bottomExpandArrowRect_ = {};
            const float routerX = sourceCol.x + sourceCol.w + sgap;
            const float routerRight = collapseArrow.x - sgap;
            const float perVoiceW = clampf(full.w * 0.22f, 180.0f, 250.0f);
            const Rect perVoice { routerX, full.y + 8.0f, perVoiceW, full.h - 16.0f };
            const Rect stripGrid { perVoice.x + perVoice.w + sgap, full.y + 8.0f,
                                   routerRight - (perVoice.x + perVoice.w + sgap), full.h - 16.0f };
            perVoiceRouteRect_ = perVoice;
            stripRouteRect_ = stripGrid;
            drawPerVoiceGrid(perVoice);
            drawStripGrid(stripGrid);
            drawArrow(collapseArrow, false);
        }
    }

END_NAMESPACE_DISTRHO
