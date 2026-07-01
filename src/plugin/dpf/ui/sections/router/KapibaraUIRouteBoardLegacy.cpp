#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawStripRack(const Rect &r)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        sourceRouterRects_.fill({});
        sourceRouterOutputRects_.fill({});
        perVoiceFilterNodeRects_.fill({});
        perVoiceFilterAddRect_ = {};
        perVoiceComponentMenuRects_.fill({});
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
        routeBoardRect_ = r;
        cleanupRouteGraphForCurrentTracks();

        const float gridTop = r.y + 36.0f;
        const float gridBottom = r.y + r.h - 12.0f;
        for(int i = 0; i < 24; ++i)
        {
            const float gx = r.x + 14.0f + float(i) * (r.w - 28.0f) / 23.0f;
            strokeLine(gx, gridTop, gx, gridBottom, rgba(0x20313a44), 0.6f);
        }
        for(int i = 0; i < 10; ++i)
        {
            const float gy = gridTop + float(i) * (gridBottom - gridTop) / 9.0f;
            strokeLine(r.x + 14.0f, gy, r.x + r.w - 14.0f, gy, rgba(0x20313a44), 0.6f);
        }

        constexpr float gap = 10.0f;
        const float sourceW = clampf(r.w * 0.13f, 110.0f, 155.0f);
        const float perVoiceW = clampf(r.w * 0.22f, 180.0f, 250.0f);
        const Rect source { r.x + 8.0f, r.y + 28.0f, sourceW, r.h - 38.0f };
        const Rect perVoice { source.x + source.w + gap, r.y + 28.0f, perVoiceW, r.h - 38.0f };
        const Rect stripGrid { perVoice.x + perVoice.w + gap, r.y + 28.0f,
                               r.x + r.w - (perVoice.x + perVoice.w + gap) - 8.0f, r.h - 38.0f };
        perVoiceRouteRect_ = perVoice;
        stripRouteRect_ = stripGrid;

        strokeLine(source.x + source.w + gap * 0.5f, r.y + 28.0f,
                   source.x + source.w + gap * 0.5f, r.y + r.h - 10.0f,
                   rgba(0x70d77a44), 1.0f);
        strokeLine(stripGrid.x - gap * 0.5f, r.y + 28.0f,
                   stripGrid.x - gap * 0.5f, r.y + r.h - 10.0f,
                   rgba(0xffc85744), 1.0f);

        drawSourceRouter(source);
        drawPerVoiceGrid(perVoice);
        drawStripGrid(stripGrid);
    }

END_NAMESPACE_DISTRHO
