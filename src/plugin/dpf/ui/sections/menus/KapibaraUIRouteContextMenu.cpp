#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawRouteContextMenu()
{
        if(!routeContextMenuOpen_)
            return;
        static constexpr const char *labels[] = { "Mute Route (zero depth)", "Delete Route" };
        constexpr float rowH = 28.0f;
        const Rect panel { routeContextX_, routeContextY_, 200.0f, 30.0f + rowH * 2.0f };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        drawSectionTitle(panel.x + 12.0f, panel.y + 8.0f, "Route");
        for(int i = 0; i < 2; ++i)
        {
            routeContextRects_[(size_t)i] = { panel.x + 8.0f, panel.y + 26.0f + float(i) * rowH,
                                               panel.w - 16.0f, rowH - 2.0f };
            drawButton(routeContextRects_[(size_t)i], labels[i], false);
        }
    }

bool KapibaraUI::handleRouteContextMenuClick(float x, float y)
{
        if(!routeContextMenuOpen_)
            return false;
        routeContextMenuOpen_ = false;
        for(int i = 0; i < 2; ++i)
        {
            if(!routeContextRects_[(size_t)i].contains(x, y))
                continue;
            const int ri = routeContextRuleIndex_;
            if(ri < 0 || ri >= synth::kMaxMatrixRules)
                break;
            auto &rule = rules_[(size_t)ri];
            if(i == 0)
                rule.depth = 0.0f;    // mute
            else
                rule.enabled = false; // delete
            if(auto *p = plugin())
                p->updateMatrixRule(ri, rule);
            break;
        }
        return true;
    }


END_NAMESPACE_DISTRHO
