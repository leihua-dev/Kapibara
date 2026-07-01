#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

int KapibaraUI::globalPerVoiceFilterCount() const
{
        int c = 0;
        for(const auto &t : generator_.tracks)
            c = std::max(c, clampi(t.perVoiceFilterCount, 0, synth::kMaxPerVoiceFilters));
        return c;
    }

void KapibaraUI::addPerVoiceFilterSlot()
{
        const int idx = globalPerVoiceFilterCount();
        if(idx >= synth::kMaxPerVoiceFilters)
            return;
        synth::SourceFilterParams f;
        f.enabled = true;
        // Resonant 2-pole by default so the Resonance knob is audible out of the box
        // (the 1-pole topology has no resonance term).
        f.topology = synth::SourceFilterTopology::TwoPoleStateVariable;
        f.cutoffHz = 1200.0f; f.resonance = 0.2f; f.drive = 1.0f; f.mix = 1.0f;
        for(auto &t : generator_.tracks)
        {
            t.perVoiceFilterCount = clampi(std::max(t.perVoiceFilterCount, idx + 1), 0, synth::kMaxPerVoiceFilters);
            t.perVoiceFilters[(size_t)idx] = f;
        }
        rebuildSelectedPerVoiceRouteFromWires();
    }

void KapibaraUI::addAmpEnvRouteNode(int index)
{
        if(index < 0 || index >= synth::kMaxAmpEnvs)
            return;
        int instance = -1;
        for(int i = 0; i < synth::kMaxAmpEnvRouteNodes; ++i)
            if(ampEnvRouteNodeSlots_[(size_t)i] >= synth::kMaxAmpEnvs)
            {
                instance = i;
                break;
            }
        if(instance < 0)
            return;
        ampEnvRouteNodeSlots_[(size_t)instance] = uint8_t(index);
        rebuildSelectedPerVoiceRouteFromWires();
    }

void KapibaraUI::drawPerVoiceChainEditor(const Rect &r)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        drawSectionTitle(r.x + 12.0f, r.y + 10.0f, "PER-VOICE CHAIN");
        pvChainTabRects_.fill({});
        pvChainEnableRect_ = {};
        pvChainTypeRect_ = {};
        pvChainKnobRects_.fill({});
        pvChainAddRect_ = {};

        auto *track = currentTrack();
        // The chain = the filters THIS source's signal passes through, in order
        // (traversed forward through the graph, so it crosses track boundaries).
        int chainN = std::min((int)selectedChainFilters_.size(), synth::kMaxPerVoiceFilters);
        std::array<int, synth::kMaxPerVoiceFilters> chainSlots {};
        for(int i = 0; i < chainN; ++i)
            chainSlots[(size_t)i] = clampi(selectedChainFilters_[(size_t)i], 0, synth::kMaxPerVoiceFilters - 1);
        pvChainCount_ = chainN;
        const float pad = 10.0f;

        if(track == nullptr || chainN == 0)
        {
            useUiFont(); uiFontSize(8.0f);
            fillColor(DesignTokens::textSecondary());
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            text(r.x + r.w * 0.5f, r.y + r.h * 0.5f, "no filters in this source's chain", nullptr);
            return;
        }

        // Keep the selected filter within this chain.
        bool inChain = false;
        for(int i = 0; i < chainN; ++i) if(chainSlots[(size_t)i] == selectedPerVoiceFilter_) inChain = true;
        if(!inChain) selectedPerVoiceFilter_ = chainSlots[0];

        // --- Tabs: one per chain filter ---
        const float tabY = r.y + 30.0f, tabH = 18.0f;
        const float tabW = clampf((r.w - pad * 2.0f) / float(chainN), 34.0f, 64.0f);
        for(int i = 0; i < chainN; ++i)
        {
            const int slot = chainSlots[(size_t)i];
            const Rect t { r.x + pad + float(i) * (tabW + 4.0f), tabY, tabW, tabH };
            pvChainTabRects_[(size_t)i] = t;
            pvChainTabSlots_[(size_t)i] = slot;
            char lbl[16]; std::snprintf(lbl, sizeof lbl, "F%d", slot + 1);
            drawButton(t, lbl, slot == selectedPerVoiceFilter_);
        }

        // --- Selected filter detail ---
        auto &f = track->perVoiceFilters[(size_t)selectedPerVoiceFilter_];
        const float hy = tabY + tabH + 8.0f;
        pvChainEnableRect_ = { r.x + pad, hy, 42.0f, 16.0f };
        pvChainTypeRect_ = { r.x + pad + 48.0f, hy, r.w - pad * 2.0f - 48.0f, 16.0f };
        const bool bypassed = (!f.enabled || f.topology == synth::SourceFilterTopology::Bypass);
        drawButton(pvChainEnableRect_, "BYP", bypassed); // highlighted = bypassed (pass-through)
        drawButton(pvChainTypeRect_, synth::sourceFilterTopologyName(f.topology), false);

        const float ky = hy + 22.0f, knobH = 44.0f;
        const float kw = (r.w - pad * 2.0f) / 4.0f;
        const float norms[4] = { cutoffToNorm(f.cutoffHz), f.resonance, f.drive / 8.0f, f.mix };
        const float disps[4] = { f.cutoffHz, f.resonance, f.drive, f.mix };
        static const char *const knames[4] = { "Cut", "Res", "Drv", "Mix" };
        for(int k = 0; k < 4; ++k)
        {
            const Rect kr { r.x + pad + float(k) * kw, ky, kw - 2.0f, knobH };
            pvChainKnobRects_[(size_t)k] = kr;
            drawKnob(kr, knames[k], norms[k], disps[k]);
        }

        // --- Response graphs: magnitude + phase ---
        const float gTop = ky + knobH + 8.0f;
        const float gBot = r.y + r.h - 8.0f;
        if(gBot - gTop > 24.0f)
        {
            const float gw = (r.w - pad * 2.0f - 6.0f) * 0.5f;
            drawSourceFilterGraph({ r.x + pad, gTop, gw, gBot - gTop }, f, false);
            drawSourceFilterGraph({ r.x + pad + gw + 6.0f, gTop, gw, gBot - gTop }, f, true);
        }
    }

bool KapibaraUI::handlePerVoiceChainPress(float x, float y)
{
        auto *track = currentTrack();
        if(track == nullptr)
            return false;
        // Start a delta-based knob drag (mirrors handleControlPress's setDragKnob).
        const auto startKnob = [&](DragTarget target, float currentNorm) -> bool {
            dragTarget_    = target;
            dragStartY_    = y;
            dragStartNorm_ = clampf(currentNorm, 0.0f, 1.0f);
            return true;
        };
        // Tabs select which chain filter is shown.
        for(int i = 0; i < pvChainCount_; ++i)
            if(pvChainTabRects_[(size_t)i].contains(x, y))
            {
                selectedPerVoiceFilter_ = pvChainTabSlots_[(size_t)i];
                repaint();
                return true;
            }
        const int slot = clampi(selectedPerVoiceFilter_, 0, synth::kMaxPerVoiceFilters - 1);
        if(slot >= int(track->perVoiceFilters.size())) return false;
        auto &f = track->perVoiceFilters[(size_t)slot];
        if(pvChainEnableRect_.contains(x, y))
        {
            f.enabled = !f.enabled;
            commitPerVoiceFilterEdit(track);
            return true;
        }
        if(pvChainTypeRect_.contains(x, y))
        {
            f.topology = static_cast<synth::SourceFilterTopology>(
                (int(f.topology) + 1) % (int(synth::SourceFilterTopology::FeedbackLadder) + 1));
            f.enabled = (f.topology != synth::SourceFilterTopology::Bypass);
            commitPerVoiceFilterEdit(track);
            return true;
        }
        for(int k = 0; k < 4; ++k)
            if(pvChainKnobRects_[(size_t)k].contains(x, y))
                switch(k)
                {
                    case 0: return startKnob(DragTarget::SourceFilterCutoff, cutoffToNorm(f.cutoffHz));
                    case 1: return startKnob(DragTarget::SourceFilterResonance, f.resonance);
                    case 2: return startKnob(DragTarget::SourceFilterDrive, f.drive / 8.0f);
                    case 3: return startKnob(DragTarget::SourceFilterMix, f.mix);
                }
        return false;
    }

END_NAMESPACE_DISTRHO
