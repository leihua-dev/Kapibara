#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
uint32_t sourceRouterNodeId(uint32_t trackId)
{
    return 0x08000000u | (trackId & 0x00ffffffu);
}
}

void KapibaraUI::drawSourceRouter(const Rect &r)
{
        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "SOURCE");

        removeTrackRect_ = {};
        const bool sourceLimitReached = generator_.tracks.size() >= size_t(synth::kMaxSourceTracks);
        addTrackRect_ = {};

        const char *labels[4] = { "Partial Bank", "Meta Oscillator", "Basic Oscillator", "Sample / Noise" };
        for(int i = 0; i < 4; ++i)
            addTrackTypeRects_[(size_t)i] = {};

        const float rowTop = r.y + 34.0f;
        constexpr float gap = 4.0f;
        const float bottomY = r.y + r.h - 8.0f;
        const float rowH = clampf((bottomY - rowTop - 8.0f - gap * float(synth::kMaxSourceTracks - 1))
                                      / float(synth::kMaxSourceTracks),
                                  20.0f, 32.0f);
        const int n = int(generator_.tracks.size());
        for(int i = 0; i < n && i < int(sourceRouterRects_.size()); ++i)
        {
            auto &track = generator_.tracks[(size_t)i];
            const Rect row { r.x + 12.0f, rowTop + float(i) * (rowH + gap), r.w - 24.0f, rowH };
            sourceRouterRects_[(size_t)i] = row;
            stripRects_[(size_t)i] = row;
            const bool selected = selectedTrack_ == i;
            const bool multi = selectedStrips_[(size_t)i];
            drawPanel(row, selected ? rgba(0x17242cff) : rgba(0x101820ff),
                      selected ? rgba(0x70d77aff) : (multi ? rgba(0x6ea8ffff) : rgba(0x263842ff)));

            char label[64] {};
            std::snprintf(label, sizeof(label), "%02d", i + 1);
            useUiFont();
            uiFontSize(8.0f);
            fillColor(selected ? DesignTokens::accentGreen() : DesignTokens::textPrimary());
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            text(row.x + 6.0f, row.y + row.h * 0.5f, label, nullptr);

            // Track name clipped tightly.
            uiFontSize(7.0f);
            fillColor(selected ? DesignTokens::accentGreen() : DesignTokens::textSecondary());
            scissor(row.x + 20.0f, row.y + 1.0f, row.w - 44.0f, row.h - 2.0f);
            text(row.x + 20.0f, row.y + row.h * 0.5f, track.name.c_str(), nullptr);
            resetScissor();

            const Rect muteR { row.x + row.w - 28.0f, row.y + (row.h - 12.0f) * 0.5f, 12.0f, 12.0f };
            const Rect soloR { row.x + row.w - 14.0f, row.y + (row.h - 12.0f) * 0.5f, 12.0f, 12.0f };
            stripMuteRects_[(size_t)i] = muteR;
            stripSoloRects_[(size_t)i] = soloR;
            drawButton(muteR, "M", track.mute);
            drawButton(soloR, "S", track.solo);

            const Rect outP { row.x + row.w - 5.0f, row.y + row.h * 0.5f - 5.0f, 10.0f, 10.0f };
            sourceRouterOutputRects_[(size_t)i] = outP;
            routePortHits_.push_back(RoutePortHit { outP, synth::GridPortRef { sourceRouterNodeId(track.id), 0 }, true });
            beginPath();
            ellipse(outP.x + outP.w * 0.5f, outP.y + outP.h * 0.5f, 5.0f, 5.0f);
            fillColor(selected ? DesignTokens::accentGreen() : rgba(0x70d77aaa));
            fill();
        }

        const float addY = rowTop + float(n) * (rowH + gap);
        if(addY + rowH < bottomY - 6.0f)
        {
            addTrackRect_ = { r.x + 12.0f, addY, r.w - 24.0f, rowH };
            drawPanel(addTrackRect_, rgba(0x0f171dff), sourceLimitReached ? rgba(0x38424aff) : rgba(0x70d77a99));
            useUiFont();
            uiFontSize(8.0f);
            fillColor(sourceLimitReached ? DesignTokens::textSecondary() : DesignTokens::accentGreen());
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            text(addTrackRect_.x + addTrackRect_.w * 0.5f, addTrackRect_.y + addTrackRect_.h * 0.5f,
                 sourceLimitReached ? "Max 12 Sources" : "+ Source", nullptr);
        }

        if(addTrackMenuOpen_ && addTrackRect_.w > 0.0f)
        {
            for(int i = 0; i < 4; ++i)
            {
                addTrackTypeRects_[(size_t)i] = Rect {
                    addTrackRect_.x,
                    addTrackRect_.y + addTrackRect_.h + 4.0f + float(i) * 21.0f,
                    addTrackRect_.w,
                    18.0f
                };
                if(addTrackTypeRects_[(size_t)i].y + addTrackTypeRects_[(size_t)i].h < bottomY - 2.0f)
                    drawButton(addTrackTypeRects_[(size_t)i], labels[i], false);
                else
                    addTrackTypeRects_[(size_t)i] = {};
            }
        }
    }

END_NAMESPACE_DISTRHO
