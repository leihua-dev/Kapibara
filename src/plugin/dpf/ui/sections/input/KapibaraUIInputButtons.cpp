#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::handleButtonClick(float x, float y)
{
        if(addTrackRect_.contains(x, y))
        {
            if(generator_.tracks.size() >= size_t(synth::kMaxSourceTracks))
                return true;
            addTrackMenuOpen_ = !addTrackMenuOpen_;
            return true;
        }
        if(addTrackMenuOpen_)
        {
            for(int i = 0; i < 4; ++i)
            {
                if(addTrackTypeRects_[(size_t)i].contains(x, y))
                {
                    if(generator_.tracks.size() >= size_t(synth::kMaxSourceTracks))
                    {
                        addTrackMenuOpen_ = false;
                        return true;
                    }
                    const auto type = static_cast<synth::SourceTrackType>(i);
                    if(auto *p = plugin())
                    {
                        p->addSourceTrack(type, synth::sourceTrackTypeName(type));
                        pullFromPlugin();
                        selectedTrack_ = int(generator_.tracks.size()) - 1;
                    }
                    addTrackMenuOpen_ = false;
                    return true;
                }
            }
        }
        for(size_t i = 0; i < trackRowRects_.size(); ++i)
        {
            if(trackRowRects_[i].contains(x, y) && i < generator_.tracks.size())
            {
                if(selectedTrack_ != int(i))
                {
                    selectedMetaFrame_ = 0;
                    metaFrameSelected_.fill(false);
                    metaFrameRangeAnchor_ = -1;
                    wavetablePresetLabel_ = "Select Wavetable";
                }
                selectedTrack_ = int(i);
                selectedAmpEnv_ = clampi(generator_.tracks[i].ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
                return true;
            }
        }
        for(size_t i = 0; i < stripRects_.size(); ++i)
        {
            const auto &s = stripRects_[i];
            if(s.contains(x, y) && i < generator_.tracks.size())
            {
                auto &track = generator_.tracks[i];
                // Double-click a source row → focus its detail; jump straight to the
                // OSC MOD diagram page when the track has source mods.
                if(currentClickIsDouble_ && !stripMuteRects_[i].contains(x, y)
                   && !stripSoloRects_[i].contains(x, y))
                {
                    const uint32_t nodeId = 0x08000000u | (track.id & 0x00ffffffu); // sourceRouterNodeId
                    focusedNodeId_ = (focusedNodeId_ == nodeId) ? 0u : nodeId;
                    selectedTrack_ = int(i);
                    selectedGroupView_ = -1;
                    if(focusedNodeId_ != 0)
                        focusPage_ = trackHasAnyMod(track) ? 2 : 0;
                    repaint();
                    return true;
                }
                // Clicking the source column exits a focused component detail view.
                if(focusedNodeId_ != 0) { focusedNodeId_ = 0; repaint(); }
                if(stripMuteRects_[i].contains(x, y))
                {
                    track.mute = !track.mute;
                    pushTrackById(track.id);
                    return true;
                }
                if(stripSoloRects_[i].contains(x, y))
                {
                    track.solo = !track.solo;
                    pushTrackById(track.id);
                    return true;
                }
                if(shiftDown_)
                {
                    // Shift+click: toggle strip in multi-selection without changing editor focus
                    selectedStrips_[i] = !selectedStrips_[i];
                    if(selectedStrips_[i])
                        selectedTrack_ = int(i);
                    return true;
                }
                // Normal click: clear multi-selection, select this strip
                selectedStrips_.fill(false);
                selectedStrips_[i] = true;
                selectedGroupView_ = -1;  // leave group view
                if(selectedTrack_ != int(i))
                {
                    selectedMetaFrame_ = 0;
                    metaFrameSelected_.fill(false);
                    metaFrameRangeAnchor_ = -1;
                    wavetablePresetLabel_ = "Select Wavetable";
                }
                selectedTrack_ = int(i);
                selectedAmpEnv_ = clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);

                // Gain/Pan/Send 在 selectedTrack 上且 rects 已在 drawStripRack 里设好 → 交给 handleControlPress
                if(trackGainRect_.contains(x, y) || trackPanRect_.contains(x, y) || trackSendRect_.contains(x, y))
                    return false;

                // ADSR route: cycle which amp env this strip uses.
                if(stripEnvRect_.contains(x, y))
                {
                    track.ampEnvIndex = (clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) + 1) % synth::kMaxAmpEnvs;
                    selectedAmpEnv_ = track.ampEnvIndex;
                    pushCurrentTrack();
                    return true;
                }
                // Duplicate this strip's amp env into a free slot and use it.
                if(stripDupRect_.contains(x, y))
                {
                    const int src = clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
                    int dst = (src + 1) % synth::kMaxAmpEnvs;
                    for(int e = 0; e < synth::kMaxAmpEnvs; ++e)
                        if(e != src && envUseCount(e) == 0) { dst = e; break; }
                    ampEnvs_[(size_t)dst] = ampEnvs_[(size_t)src];
                    track.ampEnvIndex = dst;
                    selectedAmpEnv_ = dst;
                    pushAmpEnv();
                    pushCurrentTrack();
                    return true;
                }

                return true;
            }
        }
        // Left-click a group bus → show its OSC/effect view in the editor
        for(size_t gi = 0; gi < stripGroupBusRects_.size() && gi < stripGroups_.size(); ++gi)
        {
            if(stripGroupBusRects_[gi].contains(x, y))
            {
                selectedGroupView_ = int(gi);
                return true;
            }
        }
        if(auto *track = currentTrack())
        {
            if(ampEnvSelectRect_.contains(x, y))
            {
                track->ampEnvIndex = (clampi(track->ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) + 1) % synth::kMaxAmpEnvs;
                selectedAmpEnv_ = track->ampEnvIndex;
                pushCurrentTrack();
                return true;
            }
            if(duplicateEnvRect_.contains(x, y))
            {
                const int src = clampi(track->ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
                int dst = (src + 1) % synth::kMaxAmpEnvs;
                for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
                {
                    if(i != src && envUseCount(i) == 0)
                    {
                        dst = i;
                        break;
                    }
                }
                ampEnvs_[(size_t)dst] = ampEnvs_[(size_t)src];
                track->ampEnvIndex = dst;
                selectedAmpEnv_ = dst;
                pushAmpEnv();
                pushCurrentTrack();
                return true;
            }
            if(inharmonicModeRect_.contains(x, y) && track->type == synth::SourceTrackType::PartialBank)
            {
                track->partialBank.freqShape = static_cast<synth::FreqShape>((int(track->partialBank.freqShape) + 1) % 3);
                pushCurrentTrack();
                return true;
            }
            if(metaFrameScrollRect_.contains(x, y)
               && (track->type == synth::SourceTrackType::MetaOscillator
                   || track->type == synth::SourceTrackType::PartialBank))
            {
                dragTarget_ = DragTarget::MetaFrameScroll;
                dragScrollStartX_ = x;
                dragScrollStartVal_ = metaFrameScrollStart_;
                return true;
            }
            if(metaWavetableNameRect_.contains(x, y)
               && (track->type == synth::SourceTrackType::MetaOscillator
                   || track->type == synth::SourceTrackType::PartialBank))
            {
                wavetablePresetMenuOpen_ = !wavetablePresetMenuOpen_;
                presetMenuOpen_ = false;
                if(wavetablePresetMenuOpen_)
                    refreshWavetablePresets();
                return true;
            }
            if((metaWavetablePrevRect_.contains(x, y) || metaWavetableNextRect_.contains(x, y))
               && (track->type == synth::SourceTrackType::MetaOscillator
                   || track->type == synth::SourceTrackType::PartialBank))
            {
                if(wavetablePresets_.empty())
                    refreshWavetablePresets();
                if(!wavetablePresets_.empty())
                {
                    if(selectedWavetablePresetIndex_ < 0)
                        selectedWavetablePresetIndex_ = 0;
                    else if(metaWavetablePrevRect_.contains(x, y))
                        selectedWavetablePresetIndex_ = (selectedWavetablePresetIndex_ + int(wavetablePresets_.size()) - 1)
                                                         % int(wavetablePresets_.size());
                    else
                        selectedWavetablePresetIndex_ = (selectedWavetablePresetIndex_ + 1)
                                                         % int(wavetablePresets_.size());
                    loadWavetablePreset(selectedWavetablePresetIndex_);
                }
                return true;
            }
            // Warp mode is chosen via right-click menu (openWarpModeMenu), not by
            // left-click cycling, so no left-click handling here.
            if(basicShapeRect_.contains(x, y) && track->type == synth::SourceTrackType::BasicOscillator)
            {
                track->basicShape = static_cast<synth::BasicOscillatorShape>((int(track->basicShape) + 1) % 5);
                pushCurrentTrack();
                return true;
            }
            if(noiseModeRect_.contains(x, y) && track->type == synth::SourceTrackType::SampleNoise)
            {
                track->sampleNoiseMode = synth::SampleNoiseMode::Noise;
                return true;
            }
            if(metaHarmonicEditRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                harmonicEditorOpen_ = true;
                return true;
            }
            if((metaHarmonicEditRect_.contains(x, y) || partialSpectrumRect_.contains(x, y))
               && track->type == synth::SourceTrackType::PartialBank)
            {
                harmonicEditorOpen_ = true;
                metaProcessContextMenuOpen_ = false;
                selectedMetaFrame_ = partialBankMorphFrameIndex(track->partialBank);
                return true;
            }
            if(metaLoadRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                beginWavetableImport();
                return true;
            }
            if(metaSaveRect_.contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
            {
                openWavetableSaveBrowser();
                return true;
            }
            for(int i = 0; i < int(metaFramePresetRects_.size()); ++i)
            {
                if(metaFramePresetRects_[(size_t)i].contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    applyFramePreset(i);
                    return true;
                }
            }
            for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
            {
                if(metaFrameRects_[(size_t)local].contains(x, y) && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    selectMetaFrameAt(metaFramePageStart_ + local, track->metaOsc.frameCount);
                    return true;
                }
                if(metaFrameRects_[(size_t)local].contains(x, y) && track->type == synth::SourceTrackType::PartialBank)
                {
                    selectMetaFrameAt(metaFramePageStart_ + local, track->partialBank.frameCount);
                    return true;
                }
            }
        }
        const int sourceOptions[4] = { 1, 2, 4, 8 };
        for(int i = 0; i < 4; ++i)
        {
            if(sourceCountRects_[(size_t)i].contains(x, y))
            {
                generator_.sourceCount = sourceOptions[i];
                generator_.wavetableSeed.partialCount = synth::kMaxWavetablePartials;
                for(int p = 0; p < synth::kMaxWavetablePartials; ++p)
                    generator_.wavetableSeed.partials[(size_t)p].enabled = true;
                selectedSource_ = clampi(selectedSource_, 0, generator_.sourceCount - 1);
                const int metas = synth::metaPartialsPerGeneratorSource(generator_.sourceCount);
                selectedMetaPartial_ = selectedSource_ * metas;
                pushGenerator();
                return true;
            }
        }
        for(int i = 0; i < generator_.sourceCount; ++i)
        {
            if(sourceChainRects_[(size_t)i].contains(x, y))
            {
                selectedSource_ = i;
                selectedMetaPartial_ = selectedSource_ * synth::metaPartialsPerGeneratorSource(generator_.sourceCount);
                selectedMetaFrame_ = 0;
                return true;
            }
        }
        for(int i = 0; i < synth::kEditableMetaPartials; ++i)
        {
            if(metaSelectRects_[(size_t)i].w > 0.0f && metaSelectRects_[(size_t)i].contains(x, y))
            {
                selectedMetaPartial_ = i;
                selectedMetaFrame_ = 0;
                return true;
            }
        }
        auto &metaSlot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        if(inharmonicModeRect_.contains(x, y))
        {
            generator_.wavetableSeed.freqShape = static_cast<synth::FreqShape>((int(generator_.wavetableSeed.freqShape) + 1) % 3);
            pushGenerator();
            return true;
        }
        if(sourceFilterEnableRect_.contains(x, y))
        {
            auto *track = currentTrack();
            auto &filter = (track != nullptr && track->perVoiceFilterCount > 0)
                ? track->perVoiceFilters[(size_t)clampi(selectedPerVoiceFilter_, 0, track->perVoiceFilterCount - 1)]
                : generator_.sources[(size_t)selectedSource_].filter;
            filter.enabled = !filter.enabled;
            commitPerVoiceFilterEdit(track);
            return true;
        }
        if(sourceFilterTopologyRect_.contains(x, y))
        {
            auto *track = currentTrack();
            auto &filter = (track != nullptr && track->perVoiceFilterCount > 0)
                ? track->perVoiceFilters[(size_t)clampi(selectedPerVoiceFilter_, 0, track->perVoiceFilterCount - 1)]
                : generator_.sources[(size_t)selectedSource_].filter;
            filter.topology = static_cast<synth::SourceFilterTopology>(
                (int(filter.topology) + 1) % (int(synth::SourceFilterTopology::FeedbackLadder) + 1));
            if(filter.topology == synth::SourceFilterTopology::Bypass)
                filter.enabled = false;
            else
                filter.enabled = true;
            commitPerVoiceFilterEdit(track);
            return true;
        }
        if(!generator_.tracks.empty())
        {
            int globalFilterCount = 0;
            for(const auto &t : generator_.tracks)
                globalFilterCount = std::max(globalFilterCount, clampi(t.perVoiceFilterCount, 0, synth::kMaxPerVoiceFilters));
            if(perVoiceComponentMenuOpen_)
            {
                for(int i = 0; i < 1 + synth::kMaxAmpEnvs; ++i)
                {
                    if(!perVoiceComponentMenuRects_[(size_t)i].contains(x, y))
                        continue;
                    perVoiceComponentMenuOpen_ = false;
                    if(i == 0)
                    {
                        addPerVoiceFilterSlot();
                        return true;
                    }
                    addAmpEnvRouteNode(i - 1);
                    return true;
                }
                perVoiceComponentMenuOpen_ = false;
            }
            for(int i = 0; i < synth::kMaxPerVoiceFilters; ++i)
            {
                if(perVoiceFilterNodeRects_[(size_t)i].contains(x, y) && i < globalFilterCount)
                {
                    selectedPerVoiceFilter_ = i;
                    return true;
                }
            }
            if(perVoiceFilterAddRect_.contains(x, y))
            {
                perVoiceComponentMenuOpen_ = !perVoiceComponentMenuOpen_;
                return true;
            }
        }
        // Warp mode is chosen via right-click menu (openWarpModeMenu), not by
        // left-click cycling, so no left-click handling here.
        if(metaFrameButtonRect_.contains(x, y))
        {
            selectedMetaFrame_ = (selectedMetaFrame_ + 1) % std::max(1, metaSlot.frameCount);
            return true;
        }
        // Opening the full editor from the wave view, except over the Morph
        // scrubber gutter, which is a drag control handled in handleControlPress.
        if(metaHarmonicEditRect_.contains(x, y)
           || (metaWaveformRect_.contains(x, y) && !metaMorphSliderRect_.contains(x, y)))
        {
            harmonicEditorOpen_ = true;
            return true;
        }
        if(metaLoadRect_.contains(x, y))
        {
            beginWavetableImport();
            return true;
        }
        if(metaSaveRect_.contains(x, y))
        {
            openWavetableSaveBrowser();
            return true;
        }
        for(int i = 0; i < int(metaFramePresetRects_.size()); ++i)
        {
            if(metaFramePresetRects_[(size_t)i].contains(x, y))
            {
                applyFramePreset(i);
                return true;
            }
        }
        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            if(metaFrameRects_[(size_t)local].contains(x, y))
            {
                selectMetaFrameAt(metaFramePageStart_ + local, metaSlot.frameCount);
                return true;
            }
        }

        // Matrix source chips (only present while the MATRIX view is showing).
        if(matrixViewOpen_)
        {
            for(int i = 0; i < synth::kMaxLfos; ++i)
                if(modSlotSelectRects_[(size_t)i].contains(x, y))
                {
                    selectedMatrixModSlot_ = i;
                    beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Lfo1) + i),
                                      modSlotSelectRects_[(size_t)i], x, y);
                    return true;
                }
            for(int i = 0; i < synth::kMaxModEnvs; ++i)
                if(modSlotSelectRects_[(size_t)(synth::kMaxLfos + i)].contains(x, y))
                {
                    selectedMatrixModSlot_ = synth::kMaxLfos + i;
                    beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Env1) + i),
                                      modSlotSelectRects_[(size_t)(synth::kMaxLfos + i)], x, y);
                    return true;
                }
            for(int i = 0; i < synth::kMaxAmpEnvs; ++i)
                if(ampEnvTabRects_[(size_t)i].contains(x, y))
                {
                    selectedAmpEnv_ = i;
                    beginModRouteDrag(static_cast<synth::ModSource>(int(synth::ModSource::Adsr1) + i),
                                      ampEnvTabRects_[(size_t)i], x, y);
                    return true;
                }
        }

        if(eqEnableRect_.contains(x, y)) { effects_.eq.enabled = !effects_.eq.enabled; pushEffects(); return true; }
        if(eqModeRect_.contains(x, y)) { effects_.eq.mode = static_cast<synth::MasterEffectProcessMode>((int(effects_.eq.mode) + 1) % 3); pushEffects(); return true; }
        if(filterEnableRect_.contains(x, y)) { effects_.filter.enabled = !effects_.filter.enabled; pushEffects(); return true; }
        if(filterModeRect_.contains(x, y)) { effects_.filter.mode = static_cast<synth::MasterEffectProcessMode>((int(effects_.filter.mode) + 1) % 3); pushEffects(); return true; }
        if(filterTypeRect_.contains(x, y)) { effects_.filter.type = static_cast<synth::MasterFilterType>((int(effects_.filter.type) + 1) % 3); pushEffects(); return true; }

        return false;
    }

END_NAMESPACE_DISTRHO
