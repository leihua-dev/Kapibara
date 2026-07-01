#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Track currently being drag-edited from the strip rack (gain/pan/send), or the
    // selected track as a fallback.




    // Build SourceGroupDef list from the UI groups and push to the engine (group buses).

    // 在修改 MetaOsc 帧数据之前调用，记录快照用于 Ctrl+Z
KapibaraPlugin *KapibaraUI::plugin() const
{
        return static_cast<KapibaraPlugin *>(getPluginInstancePointer());
    }

void KapibaraUI::pullFromPlugin()
{
        auto *p = plugin();
        if(p == nullptr)
            return;

        generator_ = p->generatorParams();
        adsr_ = p->adsrParams();
        if(generator_.tracks.empty())
        {
            synth::SourceTrackParams track;
            track.id = 1;
            track.name = "Partial Bank 1";
            track.type = synth::SourceTrackType::PartialBank;
            track.partialBank = generator_.wavetableSeed;
            track.gain = generator_.sources[0].gain;
            track.pan = generator_.sources[0].pan;
            track.strip = generator_.sources[0];
            track.ampEnvIndex = 0;
            track.unison = generator_.unison;
            track.ampEnvelope = adsr_;
            generator_.tracks.push_back(track);
        }
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        for(int i = 0; i < synth::kMaxModSlots; ++i)
            modSlots_[(size_t)i] = p->modSlotParams(i);
        for(int i = 0; i < synth::kMaxModEnvs; ++i)
            ampEnvs_[(size_t)i] = p->ampEnvParams(i);
        for(int i = 0; i < synth::kMaxMatrixRules; ++i)
            rules_[(size_t)i] = p->matrixRule(i);
        chaos_ = p->chaosParams();
        shape_ = p->shapeSourceParams();
        effects_ = p->effectsParams();
        gain_ = p->globalGain();
        activeVoices_ = p->activeVoiceCount();
        presetNames_ = p->presetNames();
        wavetablePresets_ = p->wavetablePresetEntries();
        if(selectedWavetablePresetIndex_ >= int(wavetablePresets_.size()))
            selectedWavetablePresetIndex_ = wavetablePresets_.empty() ? -1 : int(wavetablePresets_.size()) - 1;
        if(selectedPresetIndex_ >= int(presetNames_.size()))
            selectedPresetIndex_ = presetNames_.empty() ? -1 : int(presetNames_.size()) - 1;
        presetLabel_ = (selectedPresetIndex_ >= 0 && selectedPresetIndex_ < int(presetNames_.size()))
                           ? presetNames_[(size_t)selectedPresetIndex_]
                           : p->presetStatus();
        // Per-voice filters are globally shared; collapse any per-track divergence
        // (e.g. from older presets) onto the first track's slots.
        if(!generator_.tracks.empty())
            for(auto &t : generator_.tracks)
            {
                t.perVoiceFilterCount = generator_.tracks.front().perVoiceFilterCount;
                t.perVoiceFilters     = generator_.tracks.front().perVoiceFilters;
            }
        // Recompute route-graph audio routing (connectedToMaster, filterOrder, insertOrder)
        // and push all tracks so the engine always reflects the current wire state.
        rebuildSelectedPerVoiceRouteFromWires();
    }

void KapibaraUI::pushGenerator()
{
        if(auto *p = plugin())
            p->updateGenerator(generator_.wavetableSeed.partialCount, generator_.wavetableSeed.inharmonicAmount,
                               static_cast<int>(generator_.wavetableSeed.freqShape), generator_.sourceCount,
                               generator_.unison.voices, generator_.unison.detuneCents, generator_.unison.widthStereo,
                               generator_.unison.phaseSpread);
    }

void KapibaraUI::pushSource()
{
        if(auto *p = plugin())
            p->updateGeneratorSource(selectedSource_, generator_.sources[(size_t)selectedSource_]);
    }

void KapibaraUI::pushCurrentTrack()
{
        auto *track = currentTrack();
        if(track == nullptr)
            return;
        if(auto *p = plugin())
            p->updateSourceTrack(track->id, *track);
    }

void KapibaraUI::commitPerVoiceFilterEdit(synth::SourceTrackParams *editedTrack)
{
        if(editedTrack == nullptr)
        {
            pushSource();
            return;
        }
        // Legacy strip-filter mirror kept in sync for slot 0.
        if(editedTrack->perVoiceFilterCount > 0)
            editedTrack->strip.filter = editedTrack->perVoiceFilters[0];
        // Globally shared filter slots: mirror the edited track's filter params
        // and count onto every track in the UI model …
        for(auto &t : generator_.tracks)
        {
            t.perVoiceFilterCount = editedTrack->perVoiceFilterCount;
            t.perVoiceFilters     = editedTrack->perVoiceFilters;
        }
        // … then push via the lightweight global path (no wavetable rebake) so
        // realtime knob drags stay smooth.
        if(auto *p = plugin())
            p->updatePerVoiceFiltersGlobal(editedTrack->perVoiceFilters, editedTrack->perVoiceFilterCount);
    }

void KapibaraUI::pushAllTracks()
{
        if(auto *p = plugin())
            p->updateSourceTracks(generator_.tracks);
    }

uint64_t KapibaraUI::uiNowMs() const
{
        using clock = std::chrono::steady_clock;
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now().time_since_epoch()).count());
    }

bool KapibaraUI::realtimeDragPushDue(uint64_t &lastPushMs, bool force)
{
        const uint64_t now = uiNowMs();
        if(!force && lastPushMs != 0u && now - lastPushMs < kRealtimeDragPushIntervalMs)
            return false;
        lastPushMs = now;
        return true;
    }

void KapibaraUI::pushCurrentTrackDuringRealtimeDrag(bool force)
{
        if(!realtimeDragPushDue(lastTrackRealtimeDragPushMs_, force))
            return;
        pushCurrentTrack();
    }

void KapibaraUI::pushGeneratorDuringRealtimeDrag(bool force)
{
        if(!realtimeDragPushDue(lastGenRealtimeDragPushMs_, force))
            return;
        pushGenerator();
    }

synth::SourceTrackParams *KapibaraUI::dragTrack()
{
        const int ti = dragTrackIndex_ >= 0 ? dragTrackIndex_ : selectedTrack_;
        if(ti >= 0 && ti < int(generator_.tracks.size()))
            return &generator_.tracks[(size_t)ti];
        return nullptr;
    }

void KapibaraUI::pushCurrentTrackMorphOnly()
{
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        if(auto *p = plugin())
            p->updateSourceTrackMorphOnly(track->id, track->metaOsc.morph);
    }

void KapibaraUI::pushTrackById(uint32_t id)
{
        for(auto &t : generator_.tracks)
            if(t.id == id)
            {
                if(auto *p = plugin())
                    p->updateSourceTrack(t.id, t);
                return;
            }
    }

void KapibaraUI::deleteSelectedTrack()
{
        if(generator_.tracks.size() <= 1)
            return;
        if(selectedTrack_ < 0 || selectedTrack_ >= int(generator_.tracks.size()))
            return;
        const uint32_t id = generator_.tracks[(size_t)selectedTrack_].id;
        if(auto *p = plugin())
        {
            p->removeSourceTrack(id);
            pullFromPlugin();
        }
        // Drop the deleted index from any groups and clamp selection.
        for(auto &g : stripGroups_)
        {
            std::vector<int> kept;
            for(int mi : g.memberIndices)
                if(mi != selectedTrack_)
                    kept.push_back(mi > selectedTrack_ ? mi - 1 : mi);
            g.memberIndices = std::move(kept);
        }
        stripGroups_.erase(std::remove_if(stripGroups_.begin(), stripGroups_.end(),
                                          [](const StripGroup &g) { return g.memberIndices.size() < 2; }),
                           stripGroups_.end());
        selectedGroupView_ = -1;
        selectedStrips_.fill(false);
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        pushGroups();
    }

void KapibaraUI::pushGroups()
{
        std::vector<synth::SourceGroupDef> defs;
        defs.reserve(stripGroups_.size());
        for(const auto &g : stripGroups_)
        {
            synth::SourceGroupDef d;
            for(int mi : g.memberIndices)
                if(mi >= 0 && mi < int(generator_.tracks.size()))
                    d.memberTrackIds.push_back(generator_.tracks[(size_t)mi].id);
            defs.push_back(std::move(d));
        }
        if(auto *p = plugin())
            p->updateSourceGroups(defs);
    }

void KapibaraUI::pushMetaUndoSnapshot()
{
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        if(int(metaUndoStack_.size()) >= kMetaUndoMax)
            metaUndoStack_.erase(metaUndoStack_.begin());
        metaUndoStack_.push_back(track->metaOsc);
    }

bool KapibaraUI::undoMeta()
{
        if(metaUndoStack_.empty())
            return false;
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return false;
        track->metaOsc = metaUndoStack_.back();
        metaUndoStack_.pop_back();
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, track->metaOsc.frameCount - 1));
        pushCurrentTrack();
        return true;
    }

void KapibaraUI::pushMetaPartial()
{
        if(selectedMetaPartial_ < 0 || selectedMetaPartial_ >= synth::kEditableMetaPartials)
            return;
        if(auto *p = plugin())
            p->updatePartialSlot(selectedMetaPartial_, generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_]);
    }

void KapibaraUI::pushMetaPartialRuntime()
{
        if(selectedMetaPartial_ < 0 || selectedMetaPartial_ >= synth::kEditableMetaPartials)
            return;
        const auto &slot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        if(auto *p = plugin())
            p->updatePartialRuntime(selectedMetaPartial_, slot.enabled, slot.ratio, slot.amp, slot.phase, slot.pan,
                                    slot.morph, int(slot.warpMode), slot.warpAmount);
    }

void KapibaraUI::pushAdsr()
{
        if(auto *p = plugin())
            p->updateAdsr(adsr_.attack, adsr_.decay, adsr_.sustain, adsr_.release, adsr_.curve);
    }

void KapibaraUI::pushGain()
{
        if(auto *p = plugin())
            p->updateGlobalGain(gain_);
    }

void KapibaraUI::pushMatrix()
{
        if(auto *p = plugin())
        {
            p->updateModSlot(selectedMatrixModSlot_, modSlots_[(size_t)selectedMatrixModSlot_]);
            p->updateMatrixRule(selectedRule_, rules_[(size_t)selectedRule_]);
            p->updateChaos(chaos_);
            p->updateShapeSource(shape_);
        }
    }

void KapibaraUI::pushCurModSlot()
{ if(auto *p = plugin()) p->updateModSlot(selectedMatrixModSlot_, modSlots_[(size_t)selectedMatrixModSlot_]); }

void KapibaraUI::pushRuleOnly()
{ if(auto *p = plugin()) p->updateMatrixRule(selectedRule_, rules_[(size_t)selectedRule_]); }

void KapibaraUI::pushChaosOnly()
{ if(auto *p = plugin()) p->updateChaos(chaos_); }

void KapibaraUI::pushShapeOnly()
{ if(auto *p = plugin()) p->updateShapeSource(shape_); }

void KapibaraUI::pushAmpEnv()
{
        if(auto *p = plugin())
            p->updateAmpEnv(selectedAmpEnv_, ampEnvs_[(size_t)selectedAmpEnv_]);
    }

void KapibaraUI::pushEffects()
{
        if(auto *p = plugin())
            p->updateEffects(effects_);
    }

END_NAMESPACE_DISTRHO
