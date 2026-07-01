#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::applyOscillatorDragValue(float x, float y)
{
        // Absolute horizontal position (scrollbars, waveform editors, bar charts)
        const auto normIn = [&](const Rect &r) { return clampf((x - r.x) / std::max(1.0f, r.w), 0.0f, 1.0f); };
        // Vertical delta-based knob: drag up = increase (200 px = full range, fine with modifier)
        // Sensitivity is in screen pixels (×uiRenderScale_) so knobs feel the same
        // regardless of window size — ~200 screen px for the full range.
        const auto knobNorm = [&]() -> float {
            return clampf(dragStartNorm_ + (dragStartY_ - y) * uiRenderScale_ / 200.0f, 0.0f, 1.0f);
        };
        auto &metaSlot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &metaFrame = metaSlot.frames[(size_t)selectedMetaFrame_];
        auto &metaHarmonic = metaFrame.harmonics[(size_t)selectedMetaHarmonic_];

        switch(dragTarget_)
        {
            case DragTarget::MetaWaveform:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    auto &slot = track->metaOsc;
                    auto &frame = slot.frames[(size_t)selectedMetaFrame_];
                    const int h = clampi(int((x - metaWaveformRect_.x) / std::max(1.0f, metaWaveformRect_.w)
                                             * float(synth::kEditableWavetableHarmonics)),
                                         0, synth::kEditableWavetableHarmonics - 1);
                    selectedMetaHarmonic_ = h;
                    auto &harm = frame.harmonics[(size_t)h];
                    harm.ratio = float(h + 1);
                    harm.amp = clampf(1.0f - (y - metaWaveformRect_.y) / std::max(1.0f, metaWaveformRect_.h), 0.0f, 1.0f);
                    frame.useImportedWaveform = false;
                    frame.waveform.reset();
                    frame.spectrum.reset();
                    pushCurrentTrack();
                }
                else editMetaWaveform(x, y);
                break;
            case DragTarget::MetaRatio:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.ratio = std::pow(2.0f, knobNorm() * 7.0f); pushCurrentTrack(); }
                else { metaSlot.ratio = std::pow(2.0f, knobNorm() * 7.0f); pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaAmp:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.amp = knobNorm(); pushCurrentTrack(); }
                else { metaSlot.amp = knobNorm(); pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaPhase:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.phase = knobNorm() * 2.0f * kPi - kPi; pushCurrentTrack(); }
                else { metaSlot.phase = knobNorm() * 2.0f * kPi - kPi; pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaPhaseRand:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.phaseRandom = knobNorm(); pushCurrentTrack(); }
                else { metaSlot.phaseRandom = knobNorm(); pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaPan:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.pan = knobNorm() * 2.0f - 1.0f; pushCurrentTrack(); }
                else { metaSlot.pan = knobNorm() * 2.0f - 1.0f; pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaFrameCount:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    track->metaOsc.frameCount = clampi(1 + int(std::round(knobNorm() * float(synth::kMaxWavetableFrames - 1))),
                                                       1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = std::min(selectedMetaFrame_, track->metaOsc.frameCount - 1);
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.frameCount = clampi(1 + int(std::round(knobNorm() * float(synth::kMaxWavetableFrames - 1))),
                                                           1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = std::min(selectedMetaFrame_, track->partialBank.frameCount - 1);
                    ensurePartialBankFrameDefaults(track->partialBank, selectedMetaFrame_);
                    pushCurrentTrack();
                }
                else
                {
                    metaSlot.frameCount = clampi(1 + int(std::round(knobNorm() * float(synth::kMaxWavetableFrames - 1))),
                                                 1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = std::min(selectedMetaFrame_, metaSlot.frameCount - 1);
                    pushMetaPartial();
                }
                break;
            case DragTarget::MetaMorph:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.morph = knobNorm(); pushCurrentTrackMorphOnly(); }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.morph = knobNorm();
                    selectedMetaFrame_ = partialBankMorphFrameIndex(track->partialBank);
                    pushCurrentTrack();
                }
                else { metaSlot.morph = knobNorm(); pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaWarpAmount:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                { track->metaOsc.warpAmount = knobNorm() * 2.0f - 1.0f; pushCurrentTrack(); }
                else { metaSlot.warpAmount = knobNorm() * 2.0f - 1.0f; pushMetaPartialRuntime(); }
                break;
            case DragTarget::MetaPitchOct:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const int delta = int((dragStartY_ - y) / 22.0f);
                    track->metaOsc.pitchOct = std::max(-4, std::min(4, dragStartOct_ + delta));
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const int delta = int((dragStartY_ - y) / 22.0f);
                    const int oct = std::max(-4, std::min(4, dragStartOct_ + delta));
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, oct, base.pitchSem, base.pitchFin, base.pitchCrs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaPitchSem:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const int delta = int((dragStartY_ - y) / 12.0f);
                    track->metaOsc.pitchSem = std::max(-12, std::min(12, dragStartSem_ + delta));
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const int delta = int((dragStartY_ - y) / 12.0f);
                    const int sem = std::max(-12, std::min(12, dragStartSem_ + delta));
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, base.pitchOct, sem, base.pitchFin, base.pitchCrs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaPitchFin:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const float delta = (dragStartY_ - y) * 0.6f;
                    track->metaOsc.pitchFin = clampf(dragStartFin_ + delta, -100.0f, 100.0f);
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const float delta = (dragStartY_ - y) * 0.6f;
                    const float fin = clampf(dragStartFin_ + delta, -100.0f, 100.0f);
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, base.pitchOct, base.pitchSem, fin, base.pitchCrs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaPitchCrs:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    const float delta = (dragStartY_ - y) * 0.1f;
                    track->metaOsc.pitchCrs = clampf(dragStartCrs_ + delta, -100.0f, 100.0f);
                    track->metaOsc.syncRatioFromPitch();
                    pushCurrentTrack();
                }
                else if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    const float delta = (dragStartY_ - y) * 0.1f;
                    const float crs = clampf(dragStartCrs_ + delta, -100.0f, 100.0f);
                    const auto &base = track->partialBank.partials[0];
                    applyPartialBankGroupPitch(track->partialBank, base.pitchOct, base.pitchSem, base.pitchFin, crs);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::MetaFrameScan:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    selectedMetaFrame_ = clampi(int(normIn(metaFrameStripRect_) * float(std::max(1, seed.frameCount))),
                                                0, std::max(0, seed.frameCount - 1));
                    seed.morph = seed.frameCount > 1 ? float(selectedMetaFrame_) / float(seed.frameCount - 1) : 0.0f;
                    pushCurrentTrack();
                }
                else
                {
                    selectedMetaFrame_ = clampi(int(normIn(metaFrameStripRect_) * float(std::max(1, metaSlot.frameCount))),
                                                0, std::max(0, metaSlot.frameCount - 1));
                    metaSlot.morph = metaSlot.frameCount > 1
                                         ? float(selectedMetaFrame_) / float(metaSlot.frameCount - 1)
                                         : 0.0f;
                    pushMetaPartialRuntime();
                }
                break;
            case DragTarget::PartialTableAmp:
                editPartialTable(x, y, false);
                break;
            case DragTarget::PartialTablePhase:
                editPartialTable(x, y, true);
                break;
            case DragTarget::MetaHarmonicRatio:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    selectedPartialIndex_ = clampi(int(std::round(knobNorm()
                                                         * float(synth::kMaxWavetablePartials - 1))),
                                                   0, synth::kMaxWavetablePartials - 1);
                    selectedMetaHarmonic_ = selectedPartialIndex_;
                    break;
                }
                if(harmonicEditorOpen_)
                {
                    selectedMetaHarmonic_ = clampi(int(std::round(knobNorm()
                                                         * float(synth::kEditableWavetableHarmonics - 1))),
                                                   0, synth::kEditableWavetableHarmonics - 1);
                    break;
                }
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    auto &h = track->metaOsc.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedMetaHarmonic_];
                    h.ratio = 0.01f + knobNorm() * (float(synth::kEditableWavetableHarmonics) - 0.01f);
                    pushCurrentTrack();
                }
                else { metaHarmonic.ratio = 0.01f + knobNorm() * (float(synth::kEditableWavetableHarmonics) - 0.01f); pushMetaPartial(); }
                break;
            case DragTarget::MetaHarmonicAmp:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    auto &h = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_];
                    h.ratio = float(selectedPartialIndex_ + 1);
                    h.amp = knobNorm();
                    seed.partials[(size_t)selectedPartialIndex_].amp = h.amp;
                    pushCurrentTrack();
                    break;
                }
                if(harmonicEditorOpen_)
                {
                    editSelectedSpectrumControl(knobNorm(), false);
                    break;
                }
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    track->metaOsc.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedMetaHarmonic_].amp = knobNorm();
                    pushCurrentTrack();
                }
                else { metaHarmonic.amp = knobNorm(); pushMetaPartial(); }
                break;
            case DragTarget::MetaHarmonicPhase:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    auto &h = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_];
                    h.ratio = float(selectedPartialIndex_ + 1);
                    h.phase = knobNorm() * 2.0f * kPi - kPi;
                    pushCurrentTrack();
                    break;
                }
                if(harmonicEditorOpen_)
                {
                    editSelectedSpectrumControl(knobNorm(), true);
                    break;
                }
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                {
                    track->metaOsc.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedMetaHarmonic_].phase =
                        knobNorm() * 2.0f * kPi - kPi;
                    pushCurrentTrack();
                }
                else { metaHarmonic.phase = knobNorm() * 2.0f * kPi - kPi; pushMetaPartial(); }
                break;
            case DragTarget::HarmonicEditor: editHarmonicEditor(x, y); break;
            case DragTarget::MetaTimeEditor:
            case DragTarget::MetaSpectrumEditor: editMetaDomain(x, y); break;
            case DragTarget::MetaFrameScroll:
                if(auto *t2 = currentTrack(); t2 != nullptr
                   && (t2->type == synth::SourceTrackType::MetaOscillator
                       || t2->type == synth::SourceTrackType::PartialBank))
                {
                    const int frameCount = t2->type == synth::SourceTrackType::PartialBank
                                               ? t2->partialBank.frameCount
                                               : t2->metaOsc.frameCount;
                    const int maxScroll = std::max(0, frameCount - synth::kVisibleWavetableFrames);
                    if(maxScroll > 0)
                    {
                        const float pixPerStep = std::max(1.0f, metaFrameScrollRect_.w / float(maxScroll));
                        const int delta = int((x - dragScrollStartX_) / pixPerStep + 0.5f);
                        metaFrameScrollStart_ = clampi(dragScrollStartVal_ + delta, 0, maxScroll);
                    }
                }
                break;
            default:
                return false;
        }
        return true;
    }

END_NAMESPACE_DISTRHO
