#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

namespace
{
uint32_t inspMasterNodeId()          { return 0x2fffffffu; }
bool inspIsPerVoiceNode(uint32_t id) { return (id & 0xf0000000u) == 0x10000000u; }
bool inspIsStripNode(uint32_t id)    { return (id & 0xf0000000u) == 0x20000000u; }
bool inspIsSourceRouter(uint32_t id) { return (id & 0xff000000u) == 0x08000000u; }
int  inspPerVoiceLocal(uint32_t id)  { return int(id & 0x0fu); }
uint32_t inspStripTrackId(uint32_t id) { return (id & 0x0fffff00u) >> 8u; }
int  inspStripLocal(uint32_t id)     { return int(id & 0xffu); }
}

void KapibaraUI::drawInspector(const Rect &r)
{
        drawPanel(r, rgba(0x0d151aff), rgba(0x3b5560ff));
        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "INSPECTOR");

        const uint32_t id = selectedRouteNodeId_;
        const Rect content { r.x + 10.0f, r.y + 32.0f, r.w - 20.0f, r.h - 42.0f };

        if(id == 0)
        {
            useUiFont();
            uiFontSize(8.5f);
            fillColor(rgba(0x40505aff));
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, "click a route node", nullptr);
            // Clear filter rects so they don't catch clicks when inspector is idle.
            sourceFilterEnableRect_ = {};
            sourceFilterTopologyRect_ = {};
            sourceFilterCutoffRect_ = {};
            sourceFilterResRect_ = {};
            sourceFilterDriveRect_ = {};
            sourceFilterFeedbackRect_ = {};
            sourceFilterMixRect_ = {};
            return;
        }

        if(id == inspMasterNodeId())
        {
            drawSectionTitle(content.x, content.y, "MASTER OUTPUT");
            useUiFont();
            uiFontSize(8.0f);
            fillColor(rgba(0xffd77aff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(content.x, content.y + 22.0f, "All connected sources mix here.", nullptr);
            sourceFilterEnableRect_ = {};
            sourceFilterTopologyRect_ = {};
            sourceFilterCutoffRect_ = {};
            sourceFilterResRect_ = {};
            sourceFilterDriveRect_ = {};
            sourceFilterFeedbackRect_ = {};
            sourceFilterMixRect_ = {};
            return;
        }

        if(inspIsSourceRouter(id))
        {
            const uint32_t tid = id & 0x00ffffffu;
            for(const auto &t : generator_.tracks)
            {
                if(t.id != tid) continue;
                drawSectionTitle(content.x, content.y, t.name.c_str());
                useUiFont();
                uiFontSize(8.0f);
                fillColor(rgba(0x9eff50ccU));
                textAlign(ALIGN_LEFT | ALIGN_TOP);
                text(content.x, content.y + 22.0f, synth::sourceTrackTypeName(t.type), nullptr);
                break;
            }
            sourceFilterEnableRect_ = {};
            sourceFilterTopologyRect_ = {};
            sourceFilterCutoffRect_ = {};
            sourceFilterResRect_ = {};
            sourceFilterDriveRect_ = {};
            sourceFilterFeedbackRect_ = {};
            sourceFilterMixRect_ = {};
            return;
        }

        if(inspIsPerVoiceNode(id))
        {
            const int local = inspPerVoiceLocal(id);
            const int filterIdx = local - 1;
            auto *track = currentTrack();
            if(track == nullptr || filterIdx < 0 || filterIdx >= int(track->perVoiceFilters.size()))
            {
                sourceFilterEnableRect_ = {};
                sourceFilterTopologyRect_ = {};
                sourceFilterCutoffRect_ = {};
                sourceFilterResRect_ = {};
                sourceFilterDriveRect_ = {};
                sourceFilterFeedbackRect_ = {};
                sourceFilterMixRect_ = {};
                return;
            }
            selectedPerVoiceFilter_ = clampi(filterIdx, 0, synth::kMaxPerVoiceFilters - 1);
            auto &filt = track->perVoiceFilters[(size_t)filterIdx];
            drawSectionTitle(content.x, content.y, buttonText("PER-VOICE FILTER %d", filterIdx + 1));
            const float colW = (content.w - 8.0f) * 0.5f;
            float fy = content.y + 22.0f;
            sourceFilterEnableRect_   = { content.x,           fy,       colW, 20.0f };
            sourceFilterTopologyRect_ = { content.x + colW + 8.0f, fy,   colW, 20.0f };
            fy += 26.0f;
            sourceFilterCutoffRect_   = { content.x,           fy,       colW, 20.0f };
            sourceFilterResRect_      = { content.x + colW + 8.0f, fy,   colW, 20.0f };
            fy += 26.0f;
            sourceFilterDriveRect_    = { content.x,           fy,       colW, 20.0f };
            sourceFilterFeedbackRect_ = { content.x + colW + 8.0f, fy,   colW, 20.0f };
            fy += 26.0f;
            sourceFilterMixRect_      = { content.x,           fy, content.w, 20.0f };
            drawButton(sourceFilterEnableRect_,   filt.enabled ? "Filter On" : "Filter Off", filt.enabled);
            drawButton(sourceFilterTopologyRect_,  synth::sourceFilterTopologyName(filt.topology), false);
            drawSlider(sourceFilterCutoffRect_,   "Cutoff",   cutoffToNorm(filt.cutoffHz), filt.cutoffHz);
            drawSlider(sourceFilterResRect_,      "Resonance", filt.resonance, filt.resonance);
            drawSlider(sourceFilterDriveRect_,    "Drive",     filt.drive / 8.0f, filt.drive);
            drawSlider(sourceFilterFeedbackRect_, "Feedback",  filt.feedback, filt.feedback);
            drawSlider(sourceFilterMixRect_,      "Mix",       filt.mix, filt.mix);
            return;
        }

        if(inspIsStripNode(id))
        {
            const uint32_t tid = inspStripTrackId(id);
            const int slot = inspStripLocal(id) - 1;
            // Clear filter rects since we show insert panel here.
            sourceFilterEnableRect_ = {};
            sourceFilterTopologyRect_ = {};
            sourceFilterCutoffRect_ = {};
            sourceFilterResRect_ = {};
            sourceFilterDriveRect_ = {};
            sourceFilterFeedbackRect_ = {};
            sourceFilterMixRect_ = {};
            for(auto &track : generator_.tracks)
            {
                if(track.id != tid) continue;
                if(slot < 0 || slot >= int(track.inserts.size()))
                {
                    drawSectionTitle(content.x, content.y, "STRIP FX");
                    break;
                }
                auto &ins = track.inserts[(size_t)slot];
                drawSectionTitle(content.x, content.y, insertTypeName(ins.kind));
                const Rect panel { content.x, content.y + 22.0f,
                                   std::min(content.w, 160.0f), std::max(96.0f, content.h - 28.0f) };
                drawInsertPanel(panel, ins, int(tid), -1, slot);
                break;
            }
            return;
        }
    }

END_NAMESPACE_DISTRHO
