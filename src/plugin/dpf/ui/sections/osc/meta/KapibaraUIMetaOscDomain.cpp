#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::editHarmonicEditor(float x, float y)
{
        auto *track = currentTrack();
        if(track != nullptr && track->type != synth::SourceTrackType::MetaOscillator)
            return;
        auto &slot = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                         ? track->metaOsc
                         : generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        frame.useImportedWaveform = false;
        frame.waveform.reset();
        frame.spectrum.reset();
        selectedMetaHarmonic_ = clampi(int((x - harmonicEditorBarsRect_.x) / std::max(1.0f, harmonicEditorBarsRect_.w)
                                           * float(synth::kEditableWavetableHarmonics)),
                                      0, synth::kEditableWavetableHarmonics - 1);
        auto &h = frame.harmonics[(size_t)selectedMetaHarmonic_];
        h.ratio = float(selectedMetaHarmonic_ + 1);
        h.amp = clampf(1.0f - (y - harmonicEditorBarsRect_.y) / std::max(1.0f, harmonicEditorBarsRect_.h), 0.0f, 1.0f);
        if(track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
            pushCurrentTrack();
        else
            pushMetaPartial();
    }

void KapibaraUI::editMetaDomain(float x, float y)
{
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        auto &frame = track->metaOsc.frames[(size_t)selectedMetaFrame_];
        synth::materializeWavetableFrame(frame);
        if(dragTarget_ == DragTarget::MetaTimeEditor)
        {
            const Rect &wr = harmonicEditorBarsRect_;
            if(!frame.waveform)
                return;
            auto wave = std::make_shared<std::array<float, synth::kWavetableSize>>(*frame.waveform);

            const float px0 = prevTimeEditX_ >= 0.0f ? prevTimeEditX_ : x;
            const float py0 = prevTimeEditY_ >= 0.0f ? prevTimeEditY_ : y;
            const int s0 = clampi(int((px0 - wr.x) / std::max(1.0f, wr.w) * float(synth::kWavetableSize)),
                                  0, synth::kWavetableSize - 1);
            const int s1 = clampi(int((x   - wr.x) / std::max(1.0f, wr.w) * float(synth::kWavetableSize)),
                                  0, synth::kWavetableSize - 1);
            const float v0 = clampf(1.0f - 2.0f * (py0 - wr.y) / std::max(1.0f, wr.h), -1.0f, 1.0f);
            const float v1 = clampf(1.0f - 2.0f * (y   - wr.y) / std::max(1.0f, wr.h), -1.0f, 1.0f);
            const int lo = std::min(s0, s1), hi = std::max(s0, s1);
            for(int s = lo; s <= hi; ++s)
            {
                const float t = (hi > lo) ? float(s - lo) / float(hi - lo) : 0.5f;
                const float val = (s0 <= s1) ? (v0 + (v1 - v0) * t) : (v1 + (v0 - v1) * t);
                (*wave)[(size_t)s] = val;
            }
            prevTimeEditX_ = x;
            prevTimeEditY_ = y;
            frame.waveform = std::move(wave);
            frame.useImportedWaveform = true;
            synth::analyzeWavetableFrame(frame);
            metaEditorStatus_ = "time edited";
        }
        else
        {
            const Rect &sr = harmonicEditorSpectrumRect_;
            selectedMetaHarmonic_ = clampi(int((x - sr.x) / std::max(1.0f, sr.w)
                                               * float(synth::kEditableWavetableHarmonics)),
                                           0, synth::kEditableWavetableHarmonics - 1);
            if(!frame.spectrum)
                synth::analyzeWavetableFrame(frame);
            if(!frame.spectrum)
                return;
            auto spectrum = std::make_shared<synth::WavetableFrame::Spectrum>(*frame.spectrum);
            const size_t bin = (size_t)(selectedMetaHarmonic_ + 1);
            const float amplitude = clampf(1.0f - (y - sr.y) / std::max(1.0f, sr.h), 0.0f, 1.0f);
            const float phase = std::arg((*spectrum)[bin]);
            (*spectrum)[bin] = std::polar(amplitude * float(synth::kWavetableSize) * 0.5f, phase);
            frame.spectrum = std::move(spectrum);
            synth::rebuildWavetableFrameFromSpectrum(frame);
            metaEditorStatus_ = "spectrum edited";
        }
        metaEditorDirty_ = true;
    }

void KapibaraUI::editSelectedSpectrumControl(float normalized, bool phaseControl)
{
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return;
        auto &frame = track->metaOsc.frames[(size_t)selectedMetaFrame_];
        synth::materializeWavetableFrame(frame);
        if(!frame.spectrum)
            return;
        auto spectrum = std::make_shared<synth::WavetableFrame::Spectrum>(*frame.spectrum);
        const size_t bin = (size_t)(clampi(selectedMetaHarmonic_, 0,
                                           synth::kEditableWavetableHarmonics - 1) + 1);
        const float amplitude = phaseControl ? std::abs((*spectrum)[bin])
                                             : clampf(normalized, 0.0f, 1.0f)
                                                   * float(synth::kWavetableSize) * 0.5f;
        const float phase = phaseControl ? clampf(normalized, 0.0f, 1.0f) * 2.0f * kPi - kPi
                                         : std::arg((*spectrum)[bin]);
        (*spectrum)[bin] = std::polar(amplitude, phase);
        frame.spectrum = std::move(spectrum);
        synth::rebuildWavetableFrameFromSpectrum(frame);
        metaEditorDirty_ = true;
        metaEditorStatus_ = phaseControl ? "phase edited" : "amplitude edited";
    }

void KapibaraUI::editMetaWaveform(float x, float y)
{
        auto *track = currentTrack();
        auto &slot = (track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
                         ? track->metaOsc
                         : generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        frame.useImportedWaveform = false;
        frame.waveform.reset();
        frame.spectrum.reset();
        selectedMetaHarmonic_ = clampi(int((x - metaWaveformRect_.x) / std::max(1.0f, metaWaveformRect_.w) *
                                           float(synth::kMaxWavetableHarmonics)),
                                      0, synth::kMaxWavetableHarmonics - 1);
        auto &harmonic = frame.harmonics[(size_t)selectedMetaHarmonic_];
        harmonic.ratio = std::max(0.01f, float(selectedMetaHarmonic_ + 1));
        harmonic.amp = clampf(1.0f - (y - metaWaveformRect_.y) / std::max(1.0f, metaWaveformRect_.h), 0.0f, 1.0f);
        if(track != nullptr && track->type == synth::SourceTrackType::MetaOscillator)
            pushCurrentTrack();
        else
            pushMetaPartial();
    }

END_NAMESPACE_DISTRHO
