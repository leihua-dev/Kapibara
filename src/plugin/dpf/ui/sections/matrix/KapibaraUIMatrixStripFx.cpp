#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::modEntryActive(const synth::SourceModEntry &m)
{
        return m.sourceTrack >= 0;
    }

void KapibaraUI::drawModColumn(const Rect &region, const synth::SourceTrackParams &track, int trackId)
{
        const float rowH = 14.0f;
        const float gap = 2.0f;
        int row = 0;
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
        {
            const auto &m = track.mods[(size_t)i];
            if(!modEntryActive(m) || m.sourceTrack >= int(generator_.tracks.size()))
                continue;
            const Rect b { region.x, region.y + float(row) * (rowH + gap), region.w, rowH };
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "%s %s %d%%",
                          generator_.tracks[(size_t)m.sourceTrack].name.c_str(),
                          synth::sourceModTypeName(m.type), int(m.depth * 100.0f + 0.5f));
            const bool active = selectedTrack_ == trackIndexOfId(uint32_t(trackId)) && selectedModSlot_ == i;
            drawButton(b, lbl, active);
            modHits_.push_back(ModHit { b, trackId, i });
            ++row;
        }
        if(row < synth::kMaxTrackMods)
        {
            const Rect b { region.x, region.y + float(row) * (rowH + gap), region.w, rowH };
            drawButton(b, "+ add (R-click)", false);
            modHits_.push_back(ModHit { b, trackId, -1 });
        }
    }

int KapibaraUI::trackIndexOfId(uint32_t id) const
{
        for(size_t i = 0; i < generator_.tracks.size(); ++i)
            if(generator_.tracks[i].id == id) return int(i);
        return -1;
    }

int KapibaraUI::matrixRouteCountForTrack(const synth::SourceTrackParams &track) const
{
        int count = 0;
        for(const auto &rule : rules_)
            if(rule.enabled && rule.targetTrackId == track.id)
                ++count;
        return count;
    }

int KapibaraUI::activeModCountForTrack(const synth::SourceTrackParams &track) const
{
        int count = 0;
        for(const auto &mod : track.mods)
            if(mod.enabled && mod.sourceTrack >= 0)
                ++count;
        return count;
    }

void KapibaraUI::drawModulationMatrixPreview(const Rect &r)
{
        drawPlotBackground(r, 6, 5);
        static constexpr synth::ModSource sources[] = {
            synth::ModSource::Lfo1, synth::ModSource::Lfo2, synth::ModSource::Env1,
            synth::ModSource::Env2, synth::ModSource::Adsr, synth::ModSource::KeyTrack
        };
        static constexpr synth::ModDestination dests[] = {
            synth::ModDestination::Amp, synth::ModDestination::Freq, synth::ModDestination::MetaMorph,
            synth::ModDestination::MetaWarp, synth::ModDestination::TrackPan
        };
        constexpr int sourceCount = int(sizeof(sources) / sizeof(sources[0]));
        constexpr int destCount = int(sizeof(dests) / sizeof(dests[0]));
        const float labelW = 52.0f;
        const float headH = 14.0f;
        const float cellW = (r.w - 12.0f - labelW) / float(destCount);
        const float cellH = (r.h - 14.0f - headH) / float(sourceCount);

        useUiFont();
        uiFontSize(7.5f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary());
        for(int d = 0; d < destCount; ++d)
            text(r.x + 6.0f + labelW + cellW * (float(d) + 0.5f), r.y + 8.0f, destName(dests[d]), nullptr);

        for(int s = 0; s < sourceCount; ++s)
        {
            const float cy = r.y + headH + 7.0f + cellH * (float(s) + 0.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(DesignTokens::textSecondary());
            text(r.x + 8.0f, cy, sourceName(sources[s]), nullptr);
            for(int d = 0; d < destCount; ++d)
            {
                const Rect cell { r.x + 6.0f + labelW + cellW * float(d), r.y + headH + 7.0f + cellH * float(s),
                                  cellW, cellH };
                beginPath();
                rect(cell.x, cell.y, cell.w, cell.h);
                strokeColor(DesignTokens::divider().withAlpha(0.55f));
                strokeWidth(1.0f);
                stroke();
                for(const auto &rule : rules_)
                {
                    if(!rule.enabled || rule.source != sources[s] || rule.dest != dests[d])
                        continue;
                    const float amount = clampf(std::abs(rule.depth) / modulationDepthLimit(rule.dest), 0.0f, 1.0f);
                    const float radius = 4.0f + 5.0f * amount;
                    beginPath();
                    circle(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, radius);
                    fillColor(DesignTokens::accentBlue().withAlpha(0.28f));
                    fill();
                    strokeColor(DesignTokens::accentCyan());
                    strokeWidth(1.0f);
                    stroke();
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f", double(rule.depth));
                    uiFontSize(7.0f);
                    textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                    fillColor(DesignTokens::textPrimary());
                    text(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, buf, nullptr);
                    break;
                }
            }
        }
    }

END_NAMESPACE_DISTRHO
