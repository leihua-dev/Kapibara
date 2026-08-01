#include "../../KapibaraUI.hpp"

#include <fstream>
#include <sstream>

START_NAMESPACE_DISTRHO

// Router ("architecture") presets: the source rack and its wiring, saved apart
// from the sound. They use the same line format as the modern preset section —
// a strict subset of it — so loading one goes through readModernState, which
// leaves matrix rules, mask groups and MOD curves untouched when a file has no
// such lines. That is what makes swapping architectures under a patch work.

namespace
{
constexpr const char *kRouterDir = "presets/routers";
constexpr const char *kRouterExt = ".krt";

std::string sanitizeName(const std::string &in)
{
        std::string out;
        for(const unsigned char ch : in)
            if(std::isalnum(ch) || ch == '_' || ch == '-' || ch == ' ')
                out.push_back(char(ch));
        while(!out.empty() && out.front() == ' ') out.erase(out.begin());
        while(!out.empty() && out.back() == ' ') out.pop_back();
        if(out.size() > 48) out.resize(48);
        return out;
    }
} // namespace

std::string KapibaraUI::routerPresetDir() const
{
        return kRouterDir;
    }

void KapibaraUI::refreshRouterPresets()
{
        routerPresetNames_.clear();
        std::error_code ec;
        const std::filesystem::path root(kRouterDir);
        if(!std::filesystem::exists(root, ec))
            return;
        for(const auto &entry : std::filesystem::directory_iterator(root, ec))
        {
            if(ec) break;
            if(!entry.is_regular_file(ec)) continue;
            if(entry.path().extension().string() != kRouterExt) continue;
            routerPresetNames_.push_back(entry.path().stem().string());
        }
        std::sort(routerPresetNames_.begin(), routerPresetNames_.end());
    }

std::vector<std::string> KapibaraUI::routerPresetNames() const
{
        return routerPresetNames_;
    }

bool KapibaraUI::saveRouterPreset(const std::string &name)
{
        const std::string clean = sanitizeName(name);
        if(clean.empty())
            return false;
        std::error_code ec;
        std::filesystem::create_directories(kRouterDir, ec);
        std::ofstream out(std::filesystem::path(kRouterDir) / (clean + kRouterExt), std::ios::trunc);
        if(!out)
        {
            metaEditorStatus_ = "router save failed";
            return false;
        }
        writeModernState(out, /*structureOnly=*/true);
        routerPresetLabel_ = clean;
        refreshRouterPresets();
        metaEditorStatus_ = "router saved: " + clean;
        return true;
    }

bool KapibaraUI::loadRouterPreset(const std::string &name)
{
        const std::string clean = sanitizeName(name);
        std::ifstream in(std::filesystem::path(kRouterDir) / (clean + kRouterExt));
        if(!in)
        {
            metaEditorStatus_ = "router load failed";
            return false;
        }
        readModernState(in);
        routerPresetLabel_ = clean;
        // The rack changed underneath the editors; a stale selection would index
        // a track that no longer exists.
        selectedTrack_ = clampi(selectedTrack_, 0, std::max(0, int(generator_.tracks.size()) - 1));
        selectedStrips_.fill(false);
        selectedGroupView_ = -1;
        focusedNodeId_ = 0;
        hostStateDirty_ = true;
        metaEditorStatus_ = "router loaded: " + clean;
        return true;
    }

void KapibaraUI::drawRouterPresetMenu()
{
        routerPresetItemRects_.clear();
        if(!routerPresetMenuOpen_)
            return;
        constexpr float rowH = 20.0f;
        constexpr float menuW = 168.0f;
        const int rows = int(routerPresetNames_.size()) + 1;   // + "save as new"
        const float h = 22.0f + rowH * float(std::max(1, rows));
        const float px = clampf(routerPresetMenuX_, 4.0f, std::max(4.0f, float(uiW()) - menuW - 4.0f));
        const float py = clampf(routerPresetMenuY_, 4.0f, std::max(4.0f, float(uiH()) - h - 4.0f));
        drawPanel({ px, py, menuW, h }, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f);
        fillColor(rgba(0xc8d6dcff));
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(px + 8.0f, py + 5.0f, "Architecture", nullptr);

        float y = py + 20.0f;
        for(const auto &n : routerPresetNames_)
        {
            const Rect it { px + 6.0f, y, menuW - 12.0f, rowH - 2.0f };
            routerPresetItemRects_.push_back(it);
            drawButton(it, n.c_str(), n == routerPresetLabel_);
            y += rowH;
        }
        const Rect save { px + 6.0f, y, menuW - 12.0f, rowH - 2.0f };
        routerPresetItemRects_.push_back(save);
        drawButton(save, routerPresetNames_.empty() ? "(save current as...)" : "+ save current as...", false);
    }

bool KapibaraUI::handleRouterPresetMenuClick(float x, float y)
{
        if(!routerPresetMenuOpen_)
            return false;
        routerPresetMenuOpen_ = false;
        const int n = int(routerPresetNames_.size());
        for(int i = 0; i < int(routerPresetItemRects_.size()); ++i)
        {
            if(!routerPresetItemRects_[(size_t)i].contains(x, y))
                continue;
            if(i < n)
                loadRouterPreset(routerPresetNames_[(size_t)i]);
            else
            {
                // Reuse the shared inline name editor rather than growing a
                // second one; commitPresetNameEdit dispatches on the target.
                presetNameEditing_ = true;
                presetNameEditTarget_ = PresetNameEditTarget::Router;
                presetNameBuffer_ = routerPresetLabel_ == "ARCH" ? std::string() : routerPresetLabel_;
                skipNextPresetCharacterInput_ = false;
            }
            return true;
        }
        return true;   // clicking off the menu just closes it
    }

END_NAMESPACE_DISTRHO
