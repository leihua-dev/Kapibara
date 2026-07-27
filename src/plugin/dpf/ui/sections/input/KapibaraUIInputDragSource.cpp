#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::applySourceDragValue(float x, float y)
{
        // Absolute horizontal position (scrollbars, waveform editors, bar charts)
        const auto normIn = [&](const Rect &r) { return clampf((x - r.x) / std::max(1.0f, r.w), 0.0f, 1.0f); };
        // Vertical delta-based knob: drag up = increase (200 px = full range, fine with modifier)
        // Sensitivity is in screen pixels (×uiRenderScale_) so knobs feel the same
        // regardless of window size — ~200 screen px for the full range.
        const auto knobNorm = [&]() -> float {
            return clampf(dragStartNorm_ + (dragStartY_ - y) * uiRenderScale_ / 200.0f, 0.0f, 1.0f);
        };
        auto *selectedTrack = currentTrack();
        auto &source = selectedTrack != nullptr ? selectedTrack->strip : generator_.sources[(size_t)selectedSource_];
        auto &sourceFilter = (selectedTrack != nullptr && selectedTrack->perVoiceFilterCount > 0)
            ? selectedTrack->perVoiceFilters[(size_t)clampi(selectedPerVoiceFilter_, 0, selectedTrack->perVoiceFilterCount - 1)]
            : source.filter;

        switch(dragTarget_)
        {
            case DragTarget::TrackGain:
                if(auto *t = dragTrack()) { t->gain = knobNorm() * 2.0f; pushTrackById(t->id); }
                break;
            case DragTarget::TrackPan:
                if(auto *t = dragTrack()) { t->pan = knobNorm() * 2.0f - 1.0f; pushTrackById(t->id); }
                break;
            case DragTarget::TrackSend:
                if(auto *t = dragTrack()) { t->send = knobNorm(); pushTrackById(t->id); }
                break;
            case DragTarget::PartialAmp:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &seed = track->partialBank;
                    seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                    selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                    ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
                    auto &slot = seed.partials[(size_t)selectedPartialIndex_];
                    auto &harm = seed.frames[(size_t)selectedMetaFrame_].harmonics[(size_t)selectedPartialIndex_];
                    slot.enabled = true;
                    const float amp = knobNorm();
                    slot.amp = amp;
                    harm.ratio = float(selectedPartialIndex_ + 1);
                    harm.amp = amp;
                    if(selectedPartialIndex_ + 1 > seed.partialCount)
                        seed.partialCount = selectedPartialIndex_ + 1;
                    deferTrackPush_ = true;
                }
                break;
            case DragTarget::PartialRatio:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    auto &slot = track->partialBank.partials[(size_t)selectedPartialIndex_];
                    slot.ratio = 0.01f + knobNorm() * 63.99f;
                    deferTrackPush_ = true;
                }
                break;
            case DragTarget::BasicModDepth:
                if(auto *track = currentTrack())
                { track->basicMod.depth = knobNorm(); pushCurrentTrack(); }
                break;
            // Basic Oscillator rack — basicDragUnit_ says which column the press
            // started in; every one of these edits one unit of the stack.
            case DragTarget::BasicPulse:
            case DragTarget::BasicSub:
            case DragTarget::BasicLevel:
            case DragTarget::BasicPitchOct:
            case DragTarget::BasicPitchSem:
            case DragTarget::BasicPitchFin:
            case DragTarget::BasicPitchCrs:
                if(auto *track = currentTrack();
                   track != nullptr && basicDragUnit_ >= 0 && basicDragUnit_ < synth::kBasicOscUnits)
                {
                    auto &unit = track->basicUnits[(size_t)basicDragUnit_];
                    switch(dragTarget_)
                    {
                        case DragTarget::BasicPulse: unit.pulseWidth = knobNorm(); break;
                        case DragTarget::BasicSub:   unit.subLevel = knobNorm(); break;
                        case DragTarget::BasicLevel: unit.level = knobNorm(); break;
                        case DragTarget::BasicPitchOct:
                            unit.pitchOct = clampi(dragStartOct_ + int((dragStartY_ - y) / 22.0f), -4, 4);
                            break;
                        case DragTarget::BasicPitchSem:
                            unit.pitchSem = clampi(dragStartSem_ + int((dragStartY_ - y) / 12.0f), -12, 12);
                            break;
                        case DragTarget::BasicPitchFin:
                            unit.pitchFin = clampf(dragStartFin_ + (dragStartY_ - y) * 0.6f, -100.0f, 100.0f);
                            break;
                        default:
                            unit.pitchCrs = clampf(dragStartCrs_ + (dragStartY_ - y) * 0.1f, -100.0f, 100.0f);
                            break;
                    }
                    pushCurrentTrack();
                }
                break;
            case DragTarget::NoiseColor:
                if(auto *track = currentTrack()) { track->noiseColor = knobNorm(); pushCurrentTrack(); }
                break;
            case DragTarget::PartialCount:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.partialCount = clampi(1 + int(std::round(knobNorm() * 63.0f)), 1, 64);
                    for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
                        track->partialBank.partials[(size_t)i].enabled = i < track->partialBank.partialCount;
                    deferTrackPush_ = true;
                    pushCurrentTrackDuringRealtimeDrag();
                }
                else
                {
                    generator_.wavetableSeed.partialCount = clampi(1 + int(std::round(knobNorm() * 63.0f)), 1, 64);
                    deferGenPush_ = true;
                    pushGeneratorDuringRealtimeDrag();
                }
                break;
            case DragTarget::Inharmonic:
                if(auto *track = currentTrack(); track != nullptr && track->type == synth::SourceTrackType::PartialBank)
                {
                    track->partialBank.inharmonicAmount = knobNorm();
                    deferTrackPush_ = true;
                    pushCurrentTrackDuringRealtimeDrag();
                }
                else
                {
                    generator_.wavetableSeed.inharmonicAmount = knobNorm();
                    deferGenPush_ = true;
                    pushGeneratorDuringRealtimeDrag();
                }
                break;
            case DragTarget::Gain: gain_ = knobNorm(); pushGain(); break;
            case DragTarget::SourceGain:
                source.gain = knobNorm() * 2.0f;
                if(selectedTrack != nullptr) pushCurrentTrack(); else pushSource();
                break;
            case DragTarget::SourcePan:
                source.pan = knobNorm() * 2.0f - 1.0f;
                if(selectedTrack != nullptr) pushCurrentTrack(); else pushSource();
                break;
            case DragTarget::SourceFilterCutoff:
                sourceFilter.cutoffHz = normToCutoff(knobNorm());
                sourceFilter.enabled = true;
                commitPerVoiceFilterEdit(selectedTrack);
                break;
            case DragTarget::SourceFilterResonance:
                sourceFilter.resonance = knobNorm() * 0.95f;
                sourceFilter.enabled = true;
                commitPerVoiceFilterEdit(selectedTrack);
                break;
            case DragTarget::SourceFilterDrive:
                sourceFilter.drive = 0.1f + knobNorm() * 7.9f;
                sourceFilter.enabled = true;
                commitPerVoiceFilterEdit(selectedTrack);
                break;
            case DragTarget::SourceFilterFeedback:
                sourceFilter.feedback = knobNorm() * 0.95f;
                sourceFilter.enabled = true;
                commitPerVoiceFilterEdit(selectedTrack);
                break;
            case DragTarget::SourceFilterMix:
                sourceFilter.mix = knobNorm();
                sourceFilter.enabled = true;
                commitPerVoiceFilterEdit(selectedTrack);
                break;
            case DragTarget::UnisonVoices:
                if(auto *track = currentTrack()) { track->unison.voices = clampi(1 + int(std::round(knobNorm() * 15.0f)), 1, 16); pushCurrentTrack(); }
                else { generator_.unison.voices = clampi(1 + int(std::round(knobNorm() * 15.0f)), 1, 16); pushGenerator(); }
                break;
            case DragTarget::UnisonDetune:
                if(auto *track = currentTrack()) { track->unison.detuneCents = knobNorm() * 80.0f; pushCurrentTrack(); }
                else { generator_.unison.detuneCents = knobNorm() * 80.0f; pushGenerator(); }
                break;
            case DragTarget::UnisonWidth:
                if(auto *track = currentTrack()) { track->unison.widthStereo = knobNorm(); pushCurrentTrack(); }
                else { generator_.unison.widthStereo = knobNorm(); pushGenerator(); }
                break;
            case DragTarget::UnisonPhase:
                if(auto *track = currentTrack()) { track->unison.phaseSpread = knobNorm(); pushCurrentTrack(); }
                else { generator_.unison.phaseSpread = knobNorm(); pushGenerator(); }
                break;
            case DragTarget::Attack:
                ampEnvs_[(size_t)selectedAmpEnv_].attack = knobNorm() * 5.0f; pushAmpEnv();
                break;
            case DragTarget::Decay:
                ampEnvs_[(size_t)selectedAmpEnv_].decay = knobNorm() * 5.0f; pushAmpEnv();
                break;
            case DragTarget::Sustain:
                ampEnvs_[(size_t)selectedAmpEnv_].sustain = knobNorm(); pushAmpEnv();
                break;
            case DragTarget::Release:
                ampEnvs_[(size_t)selectedAmpEnv_].release = knobNorm() * 8.0f; pushAmpEnv();
                break;
            case DragTarget::Curve:
                adsr_.curve = knobNorm(); pushAdsr();
                break;
            case DragTarget::ManualCycleLength:
                manualCycleLength_ = clampi(32 + int(std::round(normIn(manualCycleValueRect_) * 65504.0f)), 32, 65536);
                break;
            default:
                return false;
        }
        return true;
    }

END_NAMESPACE_DISTRHO
