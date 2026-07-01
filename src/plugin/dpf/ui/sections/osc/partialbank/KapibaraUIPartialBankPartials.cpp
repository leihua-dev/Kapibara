#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::editPartialTable(float x, float y, bool phaseMode)
{
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::PartialBank)
            return;
        auto &seed = track->partialBank;
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
        ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
        Rect rect = phaseMode ? harmonicEditorPhaseRect_ : harmonicEditorSpectrumRect_;
        selectedPartialIndex_ = clampi(int((x - rect.x) / std::max(1.0f, rect.w)
                                           * float(synth::kMaxWavetablePartials)),
                                      0, synth::kMaxWavetablePartials - 1);
        selectedMetaHarmonic_ = selectedPartialIndex_;
        auto &frame = seed.frames[(size_t)selectedMetaFrame_];
        auto &h = frame.harmonics[(size_t)selectedPartialIndex_];
        h.ratio = float(selectedPartialIndex_ + 1);
        if(phaseMode)
        {
            const float normY = clampf((y - rect.y) / std::max(1.0f, rect.h), 0.0f, 1.0f);
            h.phase = (0.5f - normY) * 2.0f * kPi;
        }
        else
        {
            h.amp = clampf(1.0f - (y - rect.y) / std::max(1.0f, rect.h), 0.0f, 1.0f);
            seed.partials[(size_t)selectedPartialIndex_].amp = h.amp;
        }
        pushCurrentTrack();
    }

END_NAMESPACE_DISTRHO
