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

        // The bottom is always the full router now — the Matrix moved to the
        // toolbar-opened top-row view, so the collapse/expand arrows are gone.
        const float sgap = 10.0f;
        const float sourceW = clampf(full.w * 0.13f, 110.0f, 155.0f);
        const Rect sourceCol { full.x + 8.0f, full.y + 8.0f, sourceW, full.h - 16.0f };
        drawSourceRouter(sourceCol); // the Source column is ALWAYS present

        bottomExpandArrowRect_ = {};
        bottomCollapseArrowRect_ = {};
        const float routerX = sourceCol.x + sourceCol.w + sgap;
        const float routerRight = full.x + full.w - 8.0f;
        const float perVoiceW = clampf(full.w * 0.22f, 180.0f, 250.0f);
        const Rect perVoice { routerX, full.y + 8.0f, perVoiceW, full.h - 16.0f };
        const Rect stripGrid { perVoice.x + perVoice.w + sgap, full.y + 8.0f,
                               routerRight - (perVoice.x + perVoice.w + sgap), full.h - 16.0f };
        perVoiceRouteRect_ = perVoice;
        stripRouteRect_ = stripGrid;
        drawPerVoiceGrid(perVoice);
        drawStripGrid(stripGrid);
    }

END_NAMESPACE_DISTRHO
