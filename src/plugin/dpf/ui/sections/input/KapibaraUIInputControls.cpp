#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::handleControlPress(float x, float y)
{
        // Absolute-position controls (waveform/spectrum editors, scrollbars, bar charts)
        const auto setDragAbs = [&](DragTarget target) -> bool {
            dragTarget_ = target;
            applyDragValue(x, y);
            return true;
        };
        // Delta-based knob controls: drag up = increase, drag down = decrease (Vital-style)
        const auto setDragKnob = [&](DragTarget target, float currentNorm) -> bool {
            dragTarget_ = target;
            dragStartY_    = y;
            dragStartNorm_ = clampf(currentNorm, 0.0f, 1.0f);
            return true;
        };

        auto *track    = currentTrack();
        auto &metaSlot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &source   = track != nullptr ? track->strip : generator_.sources[(size_t)selectedSource_];
        auto &sourceFilter = (track != nullptr && track->perVoiceFilterCount > 0)
            ? track->perVoiceFilters[(size_t)clampi(selectedPerVoiceFilter_, 0, track->perVoiceFilterCount - 1)]
            : source.filter;
        auto &ampEnv   = ampEnvs_[(size_t)selectedAmpEnv_];

        // Track strip knobs
        if(trackGainRect_.contains(x, y))
            return setDragKnob(DragTarget::TrackGain, track ? track->gain * 0.5f : 0.5f);
        if(trackPanRect_.contains(x, y))
            return setDragKnob(DragTarget::TrackPan, track ? (track->pan + 1.0f) * 0.5f : 0.5f);
        if(trackSendRect_.contains(x, y))
            return setDragKnob(DragTarget::TrackSend, track ? track->send : 0.0f);

        // Partial bank harmonic knobs.
        if(track && track->type == synth::SourceTrackType::PartialBank)
        {
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                if(partialKnobRects_[(size_t)i].w > 0.0f && partialKnobRects_[(size_t)i].contains(x, y))
                {
                    selectedPartialIndex_ = i;
                    auto &seed = track->partialBank;
                    seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    const float amp = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)i].amp;
                    return setDragKnob(DragTarget::PartialAmp, amp);
                }
            }
        }
        if(partialAmpRect_.contains(x, y)) {
            auto *ptrack = track;
            float n = 0.0f;
            if(ptrack && ptrack->type == synth::SourceTrackType::PartialBank)
            {
                auto &seed = ptrack->partialBank;
                seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                n = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_].amp;
            }
            return setDragKnob(DragTarget::PartialAmp, n);
        }
        if(partialRatioRect_.contains(x, y)) {
            float ratio = 1.0f;
            if(track && track->type == synth::SourceTrackType::PartialBank)
                ratio = track->partialBank.partials[(size_t)selectedPartialIndex_].ratio;
            return setDragKnob(DragTarget::PartialRatio, std::min(1.0f, ratio / 64.0f));
        }

        // Basic Oscillator rack: every control is per unit, so the press records
        // which column it belongs to. Rects of controls a shape does not use are
        // left zeroed by the editor, so they never match here.
        if(track != nullptr && track->type == synth::SourceTrackType::BasicOscillator)
        {
            if(basicModDepthRect_.w > 0.0f && basicModDepthRect_.contains(x, y))
                return setDragKnob(DragTarget::BasicModDepth, track->basicMod.depth);
            for(int u = 0; u < synth::kBasicOscUnits; ++u)
            {
                auto &unit = track->basicUnits[(size_t)u];
                const auto grabKnob = [&](DragTarget tgt, float norm) {
                    basicDragUnit_ = u;
                    return setDragKnob(tgt, norm);
                };
                const auto grabPitch = [&](DragTarget tgt) {
                    basicDragUnit_ = u;
                    dragTarget_   = tgt;
                    dragStartY_   = y;
                    dragStartOct_ = unit.pitchOct;
                    dragStartSem_ = unit.pitchSem;
                    dragStartFin_ = unit.pitchFin;
                    dragStartCrs_ = unit.pitchCrs;
                    return true;
                };
                if(basicLevelRects_[(size_t)u].w > 0.0f && basicLevelRects_[(size_t)u].contains(x, y))
                    return grabKnob(DragTarget::BasicLevel, unit.level);
                if(basicPulseRects_[(size_t)u].w > 0.0f && basicPulseRects_[(size_t)u].contains(x, y))
                    return grabKnob(DragTarget::BasicPulse, unit.pulseWidth);
                if(basicSubRects_[(size_t)u].w > 0.0f && basicSubRects_[(size_t)u].contains(x, y))
                    return grabKnob(DragTarget::BasicSub, unit.subLevel);
                const auto &pr = basicPitchRects_[(size_t)u];
                if(pr[0].w > 0.0f && pr[0].contains(x, y)) return grabPitch(DragTarget::BasicPitchOct);
                if(pr[1].w > 0.0f && pr[1].contains(x, y)) return grabPitch(DragTarget::BasicPitchSem);
                if(pr[2].w > 0.0f && pr[2].contains(x, y)) return grabPitch(DragTarget::BasicPitchFin);
                if(pr[3].w > 0.0f && pr[3].contains(x, y)) return grabPitch(DragTarget::BasicPitchCrs);
            }
        }
        if(track != nullptr && track->type == synth::SourceTrackType::SampleNoise)
        {
            auto &sp = track->sampler;
            if(samplerStartRect_.w > 0.0f && samplerStartRect_.contains(x, y))
                return setDragKnob(DragTarget::SamplerStart, sp.startNorm);
            if(samplerEndRect_.w > 0.0f && samplerEndRect_.contains(x, y))
                return setDragKnob(DragTarget::SamplerEnd, sp.endNorm);
            if(samplerLoopStartRect_.w > 0.0f && samplerLoopStartRect_.contains(x, y))
                return setDragKnob(DragTarget::SamplerLoopStart, sp.loopStartNorm);
            if(samplerLoopEndRect_.w > 0.0f && samplerLoopEndRect_.contains(x, y))
                return setDragKnob(DragTarget::SamplerLoopEnd, sp.loopEndNorm);
            if(samplerGainRect_.w > 0.0f && samplerGainRect_.contains(x, y))
                return setDragKnob(DragTarget::SamplerGain, sp.gain * 0.5f);
        }
        if(noiseColorRect_.contains(x, y))
            return setDragKnob(DragTarget::NoiseColor, track ? track->noiseColor : 0.5f);
        if(partialCountRect_.contains(x, y)) {
            lastTrackRealtimeDragPushMs_ = 0u;
            lastGenRealtimeDragPushMs_ = 0u;
            int cnt = track ? track->partialBank.partialCount : generator_.wavetableSeed.partialCount;
            return setDragKnob(DragTarget::PartialCount, float(cnt - 1) / 63.0f);
        }
        if(inharmonicRect_.contains(x, y)) {
            lastTrackRealtimeDragPushMs_ = 0u;
            lastGenRealtimeDragPushMs_ = 0u;
            float inh = track ? track->partialBank.inharmonicAmount : generator_.wavetableSeed.inharmonicAmount;
            return setDragKnob(DragTarget::Inharmonic, inh);
        }
        if(gainRect_.contains(x, y)) return setDragKnob(DragTarget::Gain, gain_);
        if(sourceGainRect_.contains(x, y)) return setDragKnob(DragTarget::SourceGain, source.gain * 0.5f);
        if(sourcePanRect_.contains(x, y)) return setDragKnob(DragTarget::SourcePan, (source.pan + 1.0f) * 0.5f);
        if(sourceFilterCutoffRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterCutoff, cutoffToNorm(sourceFilter.cutoffHz));
        if(sourceFilterResRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterResonance, sourceFilter.resonance);
        if(sourceFilterDriveRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterDrive, sourceFilter.drive / 8.0f);
        if(sourceFilterFeedbackRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterFeedback, sourceFilter.feedback);
        if(sourceFilterMixRect_.contains(x, y))
            return setDragKnob(DragTarget::SourceFilterMix, sourceFilter.mix);

        // Unison knobs
        {
            int uv = track ? track->unison.voices : generator_.unison.voices;
            float ud = track ? track->unison.detuneCents : generator_.unison.detuneCents;
            float uw = track ? track->unison.widthStereo : generator_.unison.widthStereo;
            float up = track ? track->unison.phaseSpread : generator_.unison.phaseSpread;
            if(unisonVoicesRect_.contains(x, y)) return setDragKnob(DragTarget::UnisonVoices, float(uv - 1) / 15.0f);
            if(unisonDetuneRect_.contains(x, y)) return setDragKnob(DragTarget::UnisonDetune, ud / 80.0f);
            if(unisonWidthRect_.contains(x, y))  return setDragKnob(DragTarget::UnisonWidth, uw);
            if(unisonPhaseRect_.contains(x, y))  return setDragKnob(DragTarget::UnisonPhase, up);
        }

        // ADSR knobs
        if(attackRect_.contains(x, y))  return setDragKnob(DragTarget::Attack,  ampEnv.attack / 5.0f);
        if(decayRect_.contains(x, y))   return setDragKnob(DragTarget::Decay,   ampEnv.decay  / 5.0f);
        if(sustainRect_.contains(x, y)) return setDragKnob(DragTarget::Sustain, ampEnv.sustain);
        if(releaseRect_.contains(x, y)) return setDragKnob(DragTarget::Release, ampEnv.release / 8.0f);
        if(curveRect_.contains(x, y))   return setDragKnob(DragTarget::Curve,   adsr_.curve);

        // MetaOsc — drag 开始时记录 undo 快照（每次按下只记一次）
        const auto setDragMetaAbs = [&](DragTarget tgt) -> bool {
            pushMetaUndoSnapshot();
            return setDragAbs(tgt);
        };
        const auto setDragMetaKnob = [&](DragTarget tgt, float norm) -> bool {
            pushMetaUndoSnapshot();
            return setDragKnob(tgt, norm);
        };
        // Vertical Morph scrubber occupies the left edge of the wave view, so it
        // must be tested before the waveform-edit region it overlaps.
        if(metaMorphSliderRect_.contains(x, y)) return setDragAbs(DragTarget::MetaMorphSlider);
        if(metaWaveformRect_.contains(x, y)) return setDragMetaAbs(DragTarget::MetaWaveform);

        // Pitch controls (OCT/SEM/FIN/CRS) — Meta, the whole PartialBank group, or
        // a Basic oscillator's harmonic series. trackGroupPitch says where the
        // offset lives for this track type; the rects and drag cases are shared.
        if(TrackPitch tp; track != nullptr && trackGroupPitch(*track, tp))
        {
            const auto startPitchDrag = [&](DragTarget tgt) {
                if(track->type == synth::SourceTrackType::MetaOscillator)
                    pushMetaUndoSnapshot();
                dragTarget_   = tgt;
                dragStartY_   = y;
                dragStartOct_ = tp.oct;
                dragStartSem_ = tp.sem;
                dragStartFin_ = tp.fin;
                dragStartCrs_ = tp.crs;
                return true;
            };
            if(metaOctRect_.contains(x, y)) return startPitchDrag(DragTarget::MetaPitchOct);
            if(metaSemRect_.contains(x, y)) return startPitchDrag(DragTarget::MetaPitchSem);
            if(metaFinRect_.contains(x, y)) return startPitchDrag(DragTarget::MetaPitchFin);
            if(metaCrsRect_.contains(x, y)) return startPitchDrag(DragTarget::MetaPitchCrs);
        }

        if(track && track->type == synth::SourceTrackType::PartialBank)
        {
            auto &seed = track->partialBank;
            if(metaFrameCountRect_.contains(x, y))
                return setDragKnob(DragTarget::MetaFrameCount,
                                   float(seed.frameCount - 1) / float(synth::kMaxWavetableFrames - 1));
            if(metaMorphRect_.contains(x, y))
                return setDragKnob(DragTarget::MetaMorph, seed.morph);
        }

        {
            bool isMeta = track && track->type == synth::SourceTrackType::MetaOscillator;
            auto &ms = isMeta ? track->metaOsc : metaSlot;
            if(metaRatioRect_.contains(x, y))
                return setDragMetaKnob(DragTarget::MetaRatio, std::min(1.0f, std::log2(std::max(0.01f, ms.ratio)) / 7.0f));
            if(metaAmpRect_.contains(x, y))         return setDragMetaKnob(DragTarget::MetaAmp,        ms.amp);
            if(metaPhaseRect_.contains(x, y))       return setDragMetaKnob(DragTarget::MetaPhase,      (ms.phase + kPi) / (2.0f * kPi));
            if(metaPhaseRandRect_.contains(x, y))   return setDragMetaKnob(DragTarget::MetaPhaseRand,  ms.phaseRandom);
            if(metaPanRect_.contains(x, y))         return setDragMetaKnob(DragTarget::MetaPan,        (ms.pan + 1.0f) * 0.5f);
            if(metaFrameCountRect_.contains(x, y))   return setDragMetaKnob(DragTarget::MetaFrameCount,
                                                                            float(ms.frameCount - 1) / float(synth::kMaxWavetableFrames - 1));
            if(metaMorphRect_.contains(x, y))       return setDragKnob(DragTarget::MetaMorph,      ms.morph); // morph不修改帧数据
            if(metaWarpAmountRect_.contains(x, y))  return setDragMetaKnob(DragTarget::MetaWarpAmount, (ms.warpAmount + 1.0f) * 0.5f);
        }
        if(metaFrameStripRect_.contains(x, y)) return setDragAbs(DragTarget::MetaFrameScan);
        if(metaHarmonicRatioRect_.contains(x, y)) {
            float n = float(selectedMetaHarmonic_) / float(synth::kEditableWavetableHarmonics - 1);
            return setDragMetaKnob(DragTarget::MetaHarmonicRatio, n);
        }
        {
            auto &hFrame = metaSlot.frames[(size_t)selectedMetaFrame_];
            auto &hh = hFrame.harmonics[(size_t)selectedMetaHarmonic_];
            if(metaHarmonicAmpRect_.contains(x, y))   return setDragMetaKnob(DragTarget::MetaHarmonicAmp,   hh.amp);
            if(metaHarmonicPhaseRect_.contains(x, y)) return setDragMetaKnob(DragTarget::MetaHarmonicPhase, (hh.phase + kPi) / (2.0f * kPi));
        }

        // Matrix / LFO / ENV / Rules
        // Ctrl-drag a segment of the AMP ENV curve to bend it.
        if(ctrlDown_ && ampAdsrRect_.contains(x, y))
        {
            auto &ae = ampEnvs_[(size_t)selectedAmpEnv_];
            int seg = -1;
            float startCurve = 0.5f;
            if(x < ampAdsrXA_)        { seg = 0; startCurve = ae.curveA; }
            else if(x < ampAdsrXD_)   { seg = 1; startCurve = ae.curveD; }
            else if(x >= ampAdsrXS_)  { seg = 2; startCurve = ae.curveR; }
            if(seg >= 0)
            {
                ampAdsrDragSeg_ = seg;
                dragTarget_ = DragTarget::AmpAdsrSeg;
                dragStartY_ = y;
                dragStartDepth_ = startCurve;
                return true;
            }
        }
        if(modModeRect_.contains(x, y))
        {
            curCurveLoop() = !curCurveLoop();   // unified slot: toggle loop <-> env
            pushCurCurve();
            return true;
        }
        if(modEnvRateRect_.contains(x, y)) return setDragKnob(DragTarget::ModEnvRate, clampf(curCurveRate() / 20.0f, 0.0f, 1.0f));
        if(matrixEnvCurveRect_.contains(x, y))
        {
            auto *pts = curCurvePoints();
            int &countRef = curCurveCount();
            countRef = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
            const int hit = matrixEnvPointAt(x, y);
            // Double-click: remove a middle point, or add one on empty curve.
            if(currentClickIsDouble_)
            {
                if(hit > 0 && hit < countRef - 1)
                    deleteMatrixEnvPoint(hit);
                else if(hit < 0)
                    addMatrixEnvPoint(x, y);
                matrixEnvDirty_ = true;
                pushCurCurve();
                return true;
            }
            // Ctrl-drag a segment → bend (per-segment curvature).
            if(ctrlDown_)
            {
                const int seg = matrixEnvSegmentAt(x);
                if(seg >= 0)
                {
                    envDragSeg_ = seg;
                    selectedEnvPoint_ = seg;
                    dragTarget_ = DragTarget::MatrixEnvSeg;
                    dragStartY_ = y;
                    dragStartDepth_ = pts[seg].curve;
                    return true;
                }
            }
            // Single click selects the point under the cursor; empty space just
            // deselects (no teleport — drag an existing point to move it).
            selectedEnvPoint_ = hit;
            if(hit < 0)
                return true;
            return setDragAbs(DragTarget::MatrixEnvCurve);
        }
        // Route-FX insert knobs (flattened editor)
        for(const auto &h : fxKnobHits_)
            if(h.rect.w > 0.0f && h.rect.contains(x, y))
            {
                auto *chain = insertChainFor(h.trackId, h.mergeIdx);
                if(chain != nullptr && h.insertIdx < int(chain->size()))
                {
                    fxDragHit_ = h;
                    return setDragKnob(DragTarget::FxInsertKnob, fxKnobNorm((*chain)[(size_t)h.insertIdx], h.knob));
                }
            }
        if(chaosRateRect_.contains(x, y))   return setDragKnob(DragTarget::ChaosRate,   chaos_.frequencyHz / 60.0f);
        if(chaosAmountRect_.contains(x, y)) return setDragKnob(DragTarget::ChaosAmount, chaos_.amount);
        if(shapePhaseRect_.contains(x, y))  return setDragKnob(DragTarget::ShapePhase,  shape_.phase0);
        if(shapeRhoRect_.contains(x, y))    return setDragKnob(DragTarget::ShapeRho,    shape_.rho);
        if(shapeUpRect_.contains(x, y))     return setDragKnob(DragTarget::ShapeUp,     (shape_.pUp - 0.1f) / 7.9f);
        if(shapeDownRect_.contains(x, y))   return setDragKnob(DragTarget::ShapeDown,   (shape_.pDown - 0.1f) / 7.9f);
        if(eqLowRect_.contains(x, y))    return setDragKnob(DragTarget::EqLow,       (effects_.eq.lowGainDb  + 24.0f) / 48.0f);
        if(eqMidRect_.contains(x, y))    return setDragKnob(DragTarget::EqMid,       (effects_.eq.midGainDb  + 24.0f) / 48.0f);
        if(eqHighRect_.contains(x, y))   return setDragKnob(DragTarget::EqHigh,      (effects_.eq.highGainDb + 24.0f) / 48.0f);
        if(eqDriveRect_.contains(x, y))  return setDragKnob(DragTarget::EqDrive,     effects_.eq.drive / 6.0f);
        if(filterCutoffRect_.contains(x, y))  return setDragKnob(DragTarget::FilterCutoff,    cutoffToNorm(effects_.filter.cutoffHz));
        if(filterResRect_.contains(x, y))     return setDragKnob(DragTarget::FilterResonance, effects_.filter.resonance);
        if(filterDriveRect_.contains(x, y))   return setDragKnob(DragTarget::FilterDrive,     effects_.filter.drive / 6.0f);
        if(uiScaleRect_.contains(x, y))  return setDragKnob(DragTarget::UiScale, (uiScale_ - 0.75f) / 0.75f);

        // Mod-entry depth sliders (absolute horizontal)
        for(int i = 0; i < synth::kMaxTrackMods; ++i)
            if(modDepthRects_[(size_t)i].w > 0.0f && modDepthRects_[(size_t)i].contains(x, y))
            {
                selectedModSlot_ = i;
                return setDragAbs(DragTarget::ModEntryDepth);
            }

        // Strip scrollbar drag
        if(stripScrollbarRect_.w > 0.0f && stripScrollbarRect_.contains(x, y))
        {
            dragTarget_   = DragTarget::StripScroll;
            dragStartY_   = x;
            dragScrollStartX_  = x;
            dragScrollStartValF_ = stripScrollF_;
            return true;
        }

        return false;
    }

END_NAMESPACE_DISTRHO
