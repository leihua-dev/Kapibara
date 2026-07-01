#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::handleDoubleClickReset(float x, float y)
{
        // Strip rack controls
        if(trackGainRect_.contains(x, y))
        {
            if(auto *t = currentTrack()) { t->gain = 1.0f; pushCurrentTrack(); return true; }
        }
        if(trackPanRect_.contains(x, y))
        {
            if(auto *t = currentTrack()) { t->pan = 0.0f; pushCurrentTrack(); return true; }
        }
        if(trackSendRect_.contains(x, y))
        {
            if(auto *t = currentTrack()) { t->send = 0.0f; pushCurrentTrack(); return true; }
        }
        // Meta OCT/SEM/FIN/CRS
        if(metaOctRect_.contains(x, y) || metaSemRect_.contains(x, y)
           || metaFinRect_.contains(x, y) || metaCrsRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
            {
                pushMetaUndoSnapshot();
                t->metaOsc.pitchOct = 0; t->metaOsc.pitchSem = 0;
                t->metaOsc.pitchFin = 0.0f; t->metaOsc.pitchCrs = 0.0f;
                t->metaOsc.syncRatioFromPitch();
                pushCurrentTrack();
                return true;
            }
        }
        // Meta knobs (morph, phase, pan, warpAmount)
        if(metaMorphRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.morph = 0.0f; pushCurrentTrack(); return true; }
        }
        if(metaPhaseRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.phase = 0.0f; pushCurrentTrack(); return true; }
        }
        if(metaPhaseRandRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.phaseRandom = 0.0f; pushCurrentTrack(); return true; }
        }
        if(metaPanRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.pan = 0.0f; pushCurrentTrack(); return true; }
        }
        if(metaWarpAmountRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
                { pushMetaUndoSnapshot(); t->metaOsc.warpAmount = 0.0f; pushCurrentTrack(); return true; }
        }
        // Legacy meta partial knobs
        if(metaAmpRect_.contains(x, y))
        {
            auto &s = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            s.amp = 1.0f; pushMetaPartialRuntime(); return true;
        }
        if(metaPhaseRect_.contains(x, y))
        {
            auto &s = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            s.phase = 0.0f; pushMetaPartialRuntime(); return true;
        }
        if(metaPhaseRandRect_.contains(x, y))
        {
            auto &s = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            s.phaseRandom = 0.0f; pushMetaPartialRuntime(); return true;
        }
        if(metaPanRect_.contains(x, y))
        {
            auto &s = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
            s.pan = 0.0f; pushMetaPartialRuntime(); return true;
        }
        // Route-FX insert knobs → reset to a sensible default for that param
        for(const auto &h : fxKnobHits_)
            if(h.rect.w > 0.0f && h.rect.contains(x, y))
            {
                auto *chain = insertChainFor(h.trackId, h.mergeIdx);
                if(chain != nullptr && h.insertIdx < int(chain->size()))
                {
                    InsertEffect def; def.kind = (*chain)[(size_t)h.insertIdx].kind;
                    fxKnobSetNorm((*chain)[(size_t)h.insertIdx], h.knob, fxKnobNorm(def, h.knob));
                    commitChainChange(h.trackId, h.mergeIdx);
                }
                return true;
            }
        return false;
    }

bool KapibaraUI::handleHarmonicEditorClick(float x, float y)
{
        if(!harmonicEditorOpen_)
            return false;
        auto *activeTrack = currentTrack();
        if(harmonicEditorCloseRect_.contains(x, y))
        {
            harmonicEditorOpen_ = false;
            metaProcessContextMenuOpen_ = false;
            dragTarget_ = DragTarget::None;
            return true;
        }
        if(activeTrack != nullptr && activeTrack->type == synth::SourceTrackType::PartialBank)
        {
            if(metaFrameScrollRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaFrameScroll;
                dragScrollStartX_ = x;
                dragScrollStartVal_ = metaFrameScrollStart_;
                return true;
            }
            if(metaEditorImportRect_.contains(x, y))
            {
                beginWavetableImport();
                return true;
            }
            if(metaEditorAddRect_.contains(x, y)) return performMetaFrameAction(0);
            if(metaEditorDuplicateRect_.contains(x, y)) return performMetaFrameAction(1);
            if(metaEditorDeleteRect_.contains(x, y)) return performMetaFrameAction(2);
            if(metaEditorLeftRect_.contains(x, y)) return performMetaFrameAction(3);
            if(metaEditorRightRect_.contains(x, y)) return performMetaFrameAction(4);
            for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
            {
                if(metaFrameRects_[(size_t)local].contains(x, y))
                {
                    selectMetaFrameAt(metaFramePageStart_ + local, activeTrack->partialBank.frameCount);
                    return true;
                }
            }
            if(harmonicEditorPhaseRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::PartialTablePhase;
                editPartialTable(x, y, true);
                return true;
            }
            if(harmonicEditorSpectrumRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::PartialTableAmp;
                editPartialTable(x, y, false);
                return true;
            }
            if(metaHarmonicRatioRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaHarmonicRatio;
                dragStartY_ = y;
                dragStartNorm_ = float(selectedPartialIndex_) / float(synth::kMaxWavetablePartials - 1);
                return true;
            }
            if(metaHarmonicAmpRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaHarmonicAmp;
                dragStartY_ = y;
                const auto &frame = activeTrack->partialBank.frames[(size_t)selectedMetaFrame_];
                dragStartNorm_ = frame.harmonics[(size_t)selectedPartialIndex_].amp;
                return true;
            }
            if(metaHarmonicPhaseRect_.contains(x, y))
            {
                dragTarget_ = DragTarget::MetaHarmonicPhase;
                dragStartY_ = y;
                const auto &frame = activeTrack->partialBank.frames[(size_t)selectedMetaFrame_];
                dragStartNorm_ = (frame.harmonics[(size_t)selectedPartialIndex_].phase + kPi) / (2.0f * kPi);
                return true;
            }
            return harmonicEditorPanelRect_.contains(x, y);
        }
        if(metaFrameScrollRect_.contains(x, y))
        {
            dragTarget_ = DragTarget::MetaFrameScroll;
            dragScrollStartX_ = x;
            dragScrollStartVal_ = metaFrameScrollStart_;
            return true;
        }
        if(metaEditorImportRect_.contains(x, y))
        {
            beginWavetableImport();
            return true;
        }
        if(metaEditorAddRect_.contains(x, y)) return performMetaFrameAction(0);
        if(metaEditorDuplicateRect_.contains(x, y)) return performMetaFrameAction(1);
        if(metaEditorDeleteRect_.contains(x, y)) return performMetaFrameAction(2);
        if(metaEditorLeftRect_.contains(x, y)) return performMetaFrameAction(3);
        if(metaEditorRightRect_.contains(x, y)) return performMetaFrameAction(4);

        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            if(metaFrameRects_[(size_t)local].contains(x, y))
            {
                if(auto *track = currentTrack(); track != nullptr)
                    selectMetaFrameAt(metaFramePageStart_ + local, track->metaOsc.frameCount);
                return true;
            }
        }
        if(harmonicEditorBarsRect_.contains(x, y))
        {
            prevTimeEditX_ = x;
            prevTimeEditY_ = y;
            dragTarget_ = DragTarget::MetaTimeEditor;
            editMetaDomain(x, y);
            return true;
        }
        if(harmonicEditorSpectrumRect_.contains(x, y))
        {
            dragTarget_ = DragTarget::MetaSpectrumEditor;
            editMetaDomain(x, y);
            return true;
        }
        if(metaHarmonicRatioRect_.contains(x, y))
        {
            pushMetaUndoSnapshot();
            dragTarget_    = DragTarget::MetaHarmonicRatio;
            dragStartY_    = y;
            dragStartNorm_ = float(selectedMetaHarmonic_) / float(synth::kEditableWavetableHarmonics - 1);
            return true;
        }
        if(metaHarmonicAmpRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
            {
                pushMetaUndoSnapshot();
                dragTarget_    = DragTarget::MetaHarmonicAmp;
                dragStartY_    = y;
                dragStartNorm_ = t->metaOsc.frames[(size_t)selectedMetaFrame_]
                                             .harmonics[(size_t)selectedMetaHarmonic_].amp;
            }
            return true;
        }
        if(metaHarmonicPhaseRect_.contains(x, y))
        {
            if(auto *t = currentTrack(); t && t->type == synth::SourceTrackType::MetaOscillator)
            {
                pushMetaUndoSnapshot();
                const float ph = t->metaOsc.frames[(size_t)selectedMetaFrame_]
                                              .harmonics[(size_t)selectedMetaHarmonic_].phase;
                dragTarget_    = DragTarget::MetaHarmonicPhase;
                dragStartY_    = y;
                dragStartNorm_ = (ph + kPi) / (2.0f * kPi);
            }
            return true;
        }
        return harmonicEditorPanelRect_.contains(x, y);
    }

END_NAMESPACE_DISTRHO
