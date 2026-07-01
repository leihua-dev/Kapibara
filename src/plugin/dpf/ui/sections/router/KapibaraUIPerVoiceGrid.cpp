#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
uint32_t perVoiceNodeId(uint32_t trackId, int local)
{
    (void)trackId;
    return 0x10000000u | uint32_t(local & 0x0f);
}
uint32_t ampEnvNodeId(int slot, int instance)
{
    return 0x30000000u | (uint32_t((slot + 1) & 0xff) << 8u) | uint32_t((instance + 1) & 0xff);
}
}

void KapibaraUI::drawPerVoiceGrid(const Rect &r)
{
        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "PER-VOICE");

        const float gridTop = r.y + 38.0f;
        const float gridH = r.h - 50.0f;
        for(int i = 0; i < 9; ++i)
        {
            const float gx = r.x + 12.0f + float(i) * (r.w - 24.0f) / 8.0f;
            strokeLine(gx, gridTop, gx, gridTop + gridH, rgba(0x20313a66), 0.7f);
        }
        for(int i = 0; i < 7; ++i)
        {
            const float gy = gridTop + float(i) * gridH / 6.0f;
            strokeLine(r.x + 12.0f, gy, r.x + r.w - 12.0f, gy, rgba(0x20313a66), 0.7f);
        }

        if(generator_.tracks.empty())
        {
            drawLabelBox({ r.x + 18.0f, r.y + 56.0f, r.w - 36.0f, 22.0f }, "no source");
            return;
        }

        const uint32_t tid = 0;
        int count = 0;
        for(const auto &track : generator_.tracks)
            count = std::max(count, clampi(track.perVoiceFilterCount, 0, synth::kMaxPerVoiceFilters));
        selectedPerVoiceFilter_ = clampi(selectedPerVoiceFilter_, 0, std::max(0, count - 1));

        const auto drawNode = [&](const Rect &node, const char *label, bool active) {
            drawPanel(node, active ? rgba(0x17242cff) : rgba(0x101820ff),
                      active ? DesignTokens::accentGreen() : DesignTokens::border());
            useUiFont();
            uiFontSize(8.5f);
            fillColor(active ? DesignTokens::accentGreen() : DesignTokens::textPrimary());
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            text(node.x + node.w * 0.5f, node.y + node.h * 0.5f, label, nullptr);
        };

        const auto dot = [&](const Rect &p, Color c) {
            beginPath();
            ellipse(p.x + p.w * 0.5f, p.y + p.h * 0.5f, 4.0f, 4.0f);
            fillColor(c);
            fill();
        };
        const float availableW = std::max(80.0f, r.w - 44.0f);
        constexpr float nodeH = 18.0f;
        constexpr float nodeW = 46.0f;
        const float chainW = count > 0 ? nodeW * float(count) + 12.0f * float(count - 1) : 0.0f;
        float nodeX = r.x + 22.0f;
        if(count > 0)
            nodeX = r.x + 22.0f + std::max(0.0f, (availableW - chainW) * 0.5f);
        for(int i = 0; i < count; ++i)
        {
            const uint32_t nodeId = perVoiceNodeId(tid, i + 1);
            const synth::GridPoint defaultPos {
                int(std::round(nodeX + float(i) * (nodeW + 12.0f))),
                int(std::round(r.y + 82.0f + float(i % 2) * 54.0f))
            };
            auto [posIt, inserted] = routeNodePositions_.try_emplace(nodeId, defaultPos);
            const Rect node { float(posIt->second.x), float(posIt->second.y), nodeW, nodeH };
            perVoiceFilterNodeRects_[(size_t)i] = node;
            drawNode(node, "FILTER", selectedPerVoiceFilter_ == i);
            routeNodeHits_.push_back(RouteNodeHit { node, nodeId });
            const Rect inP { node.x - 4.0f, node.y + node.h * 0.5f - 4.0f, 8.0f, 8.0f };
            routePortHits_.push_back(RoutePortHit { inP, synth::GridPortRef { nodeId, 0 }, false });
            dot(inP, DesignTokens::accentCyan());
            drawComponentOutputPorts(node, nodeId); // one or more output ports
        }

        // Amp-env nodes are explicit components. They only appear after being
        // added from + COMPONENT; source tracks do not get an implicit ADSR.
        const auto drawAmpNode = [&](const Rect &node, const char *label) {
            drawPanel(node, rgba(0x241a10ff), rgba(0xffc05799));
            useUiFont();
            uiFontSize(8.0f);
            fillColor(rgba(0xffc857ff));
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            text(node.x + node.w * 0.5f, node.y + node.h * 0.5f, label, nullptr);
        };
        int visibleAe = 0;
        for(int i = 0; i < synth::kMaxAmpEnvRouteNodes; ++i)
        {
            const int slot = int(ampEnvRouteNodeSlots_[(size_t)i]);
            if(slot < 0 || slot >= synth::kMaxAmpEnvs)
                continue;
            const uint32_t nodeId = ampEnvNodeId(slot, i);
            constexpr float aeNodeW = 32.0f;
            const synth::GridPoint defaultPos {
                int(std::round(r.x + 20.0f + float(visibleAe % 5) * (aeNodeW + 10.0f))),
                int(std::round(r.y + r.h - 70.0f - float(visibleAe / 5) * 28.0f))
            };
            ++visibleAe;
            auto [posIt, inserted] = routeNodePositions_.try_emplace(nodeId, defaultPos);
            const Rect node { float(posIt->second.x), float(posIt->second.y), aeNodeW, nodeH };
            char lbl[8]; std::snprintf(lbl, sizeof lbl, "AE%d", slot + 1);
            drawAmpNode(node, lbl);
            routeNodeHits_.push_back(RouteNodeHit { node, nodeId });
            const Rect inP { node.x - 4.0f, node.y + node.h * 0.5f - 4.0f, 8.0f, 8.0f };
            routePortHits_.push_back(RoutePortHit { inP, synth::GridPortRef { nodeId, 0 }, false });
            dot(inP, DesignTokens::accentCyan());
            drawComponentOutputPorts(node, nodeId);
        }

        perVoiceFilterAddRect_ = { r.x + 18.0f, r.y + r.h - 34.0f, 112.0f, 20.0f };
        if(perVoiceFilterAddRect_.w > 0.0f)
            drawButton(perVoiceFilterAddRect_, "+ COMPONENT", perVoiceComponentMenuOpen_);

        perVoiceComponentMenuRects_.fill({});
        if(perVoiceComponentMenuOpen_)
        {
            static const char *const labels[1 + synth::kMaxAmpEnvs] = { "FILTER", "AE1", "AE2", "AE3", "AE4" };
            for(int i = 0; i < 1 + synth::kMaxAmpEnvs; ++i)
            {
                const Rect item {
                    perVoiceFilterAddRect_.x,
                    perVoiceFilterAddRect_.y - 4.0f - float(1 + synth::kMaxAmpEnvs - i) * 20.0f,
                    86.0f,
                    18.0f
                };
                perVoiceComponentMenuRects_[(size_t)i] = item;
                const bool disabled = (i == 0 && count >= synth::kMaxPerVoiceFilters);
                char label[16] {};
                if(disabled)
                    std::snprintf(label, sizeof(label), "%s on", labels[i]);
                else
                    std::snprintf(label, sizeof(label), "%s", labels[i]);
                drawButton(item, label, disabled);
            }
        }
    }

END_NAMESPACE_DISTRHO
