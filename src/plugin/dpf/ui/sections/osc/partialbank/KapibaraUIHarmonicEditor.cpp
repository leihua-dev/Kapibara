#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawPartialTableEditor(synth::SourceTrackParams &track)
{
        auto &seed = track.partialBank;
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
        selectedPartialIndex_ = clampi(selectedPartialIndex_, 0, synth::kMaxWavetablePartials - 1);
        selectedMetaHarmonic_ = selectedPartialIndex_;
        ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
        auto &frame = seed.frames[(size_t)selectedMetaFrame_];
        auto &h = frame.harmonics[(size_t)selectedPartialIndex_];

        const Rect r { 42.0f, 92.0f, static_cast<float>(uiW()) - 84.0f,
                       static_cast<float>(uiH()) - 190.0f };
        harmonicEditorPanelRect_ = r;
        drawPanel(r, rgba(0x0b1117f7), rgba(0x4a6470ff));
        drawSectionTitle(r.x + 18.0f, r.y + 16.0f, "Partial Table Editor");
        harmonicEditorCloseRect_ = { r.x + r.w - 86.0f, r.y + 14.0f, 68.0f, 28.0f };
        drawButton(harmonicEditorCloseRect_, "Close", false);

        const float toolY = r.y + 50.0f;
        metaEditorImportRect_ = { r.x + 18.0f, toolY, 88.0f, 24.0f };
        metaEditorAddRect_ = { r.x + 112.0f, toolY, 52.0f, 24.0f };
        metaEditorDuplicateRect_ = { r.x + 170.0f, toolY, 70.0f, 24.0f };
        metaEditorDeleteRect_ = { r.x + 246.0f, toolY, 64.0f, 24.0f };
        metaEditorLeftRect_ = { r.x + 316.0f, toolY, 36.0f, 24.0f };
        metaEditorRightRect_ = { r.x + 358.0f, toolY, 36.0f, 24.0f };
        drawButton(metaEditorImportRect_, "Import KWT", false);
        drawButton(metaEditorAddRect_, "Add", false);
        drawButton(metaEditorDuplicateRect_, "Duplicate", false);
        drawButton(metaEditorDeleteRect_, "Delete", false);
        drawButton(metaEditorLeftRect_, "<", false);
        drawButton(metaEditorRightRect_, ">", false);
        fontSize(8.0f);
        fillColor(rgba(0x6a8090ff));
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 404.0f, toolY + 12.0f, "Ctrl+click=range  Ctrl+A=all  drag phase/level bars", nullptr);

        synth::WavetablePartialSlot frameStripSlot;
        frameStripSlot.frameCount = seed.frameCount;
        frameStripSlot.morph = seed.morph;
        frameStripSlot.frames = seed.frames;
        metaEditorFrameStripRect_ = { r.x + 18.0f, r.y + 82.0f, r.w - 36.0f, 48.0f };
        drawMetaFrameStrip(metaEditorFrameStripRect_, frameStripSlot);
        metaFrameScrollRect_ = { r.x + 18.0f, r.y + 134.0f, r.w - 36.0f, 12.0f };
        drawFrameScrollbar(metaFrameScrollRect_, frameStripSlot);

        char title[176];
        std::snprintf(title, sizeof(title), "Frame %03d/%03d  Partial %02d  Level %.3g  Phase %.3g  %s",
                      selectedMetaFrame_ + 1, seed.frameCount, selectedPartialIndex_ + 1,
                      h.amp, h.phase, metaEditorStatus_.c_str());
        drawLabelBox({ r.x + 18.0f, r.y + 150.0f, r.w - 36.0f, 26.0f }, title);

        const float totalDispH = r.h - 230.0f;
        const float phaseH = std::max(42.0f, totalDispH * 0.36f);
        const float ampH = std::max(72.0f, totalDispH - phaseH - 8.0f);
        const float dispY = r.y + 180.0f;

        harmonicEditorPhaseRect_ = { r.x + 18.0f, dispY, r.w - 36.0f, phaseH };
        drawPanel(harmonicEditorPhaseRect_, rgba(0x0d1620ff), rgba(0x1e3040ff));
        {
            const Rect &pr = harmonicEditorPhaseRect_;
            const float barW = pr.w / float(synth::kMaxWavetablePartials);
            const float midY = pr.y + pr.h * 0.5f;
            strokeLine(pr.x + 2.0f, midY, pr.x + pr.w - 2.0f, midY, rgba(0x2b3f48ff), 0.8f);
            scissor(pr.x + 2.0f, pr.y + 2.0f, pr.w - 4.0f, pr.h - 4.0f);
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                const float ph = frame.harmonics[(size_t)i].phase;
                const float norm = clampf(ph / kPi, -1.0f, 1.0f);
                const float bx = pr.x + float(i) * barW + 0.5f;
                const float bw = std::max(1.0f, barW - 1.0f);
                const float bh = norm * (pr.h * 0.44f);
                const float by = bh >= 0.0f ? midY - bh : midY;
                beginPath();
                rect(bx, by, bw, std::abs(bh));
                fillColor(i == selectedPartialIndex_ ? rgba(0xffa23add) : rgba(0x7f68b0aa));
                fill();
            }
            resetScissor();
            fontSize(8.0f);
            fillColor(rgba(0x6080a0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(pr.x + 4.0f, pr.y + 2.0f, "PHASE PER PARTIAL", nullptr);
        }

        harmonicEditorSpectrumRect_ = { r.x + 18.0f, dispY + phaseH + 8.0f, r.w - 36.0f, ampH };
        harmonicEditorBarsRect_ = harmonicEditorSpectrumRect_;
        drawPanel(harmonicEditorSpectrumRect_, rgba(0x101820ff), rgba(0x263842ff));
        {
            const Rect &sr = harmonicEditorSpectrumRect_;
            const float barW = sr.w / float(synth::kMaxWavetablePartials);
            scissor(sr.x + 2.0f, sr.y + 2.0f, sr.w - 4.0f, sr.h - 4.0f);
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                const auto &hm = frame.harmonics[(size_t)i];
                const float amp = clampf(hm.amp, 0.0f, 1.0f);
                const float bx = sr.x + float(i) * barW;
                const float bh = amp * (sr.h - 12.0f);
                beginPath();
                rect(bx + 0.5f, sr.y + sr.h - 6.0f - bh, std::max(1.0f, barW - 1.0f), bh);
                fillColor(i == selectedPartialIndex_ ? rgba(0x8be87dff) : rgba(0x4d8a80dd));
                fill();
            }
            resetScissor();
            fontSize(8.0f);
            fillColor(rgba(0x6080a0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(sr.x + 4.0f, sr.y + 2.0f, "PARTIAL LEVEL RATIO", nullptr);
        }

        const float sliderY = r.y + r.h - 50.0f;
        const float third = (r.w - 52.0f) / 3.0f;
        metaHarmonicRatioRect_ = { r.x + 18.0f, sliderY, third, 24.0f };
        metaHarmonicAmpRect_ = { metaHarmonicRatioRect_.x + third + 8.0f, sliderY, third, 24.0f };
        metaHarmonicPhaseRect_ = { metaHarmonicAmpRect_.x + third + 8.0f, sliderY, third, 24.0f };
        drawSlider(metaHarmonicRatioRect_, "Partial", float(selectedPartialIndex_) / float(synth::kMaxWavetablePartials - 1),
                   float(selectedPartialIndex_ + 1));
        drawSlider(metaHarmonicAmpRect_, "Level", h.amp, h.amp);
        drawSlider(metaHarmonicPhaseRect_, "Phase", (h.phase + kPi) / (2.0f * kPi), h.phase);
    }

void KapibaraUI::drawHarmonicEditor()
{
        if(!harmonicEditorOpen_)
            return;
        auto *track = currentTrack();
        if(track != nullptr && track->type == synth::SourceTrackType::PartialBank)
        {
            drawPartialTableEditor(*track);
            return;
        }
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
        {
            harmonicEditorOpen_ = false;
            harmonicEditorPanelRect_ = {};
            return;
        }

        const Rect r { 42.0f, 92.0f, static_cast<float>(uiW()) - 84.0f,
                       static_cast<float>(uiH()) - 190.0f };
        harmonicEditorPanelRect_ = r;
        drawPanel(r, rgba(0x0b1117f7), rgba(0x4a6470ff));
        drawSectionTitle(r.x + 18.0f, r.y + 16.0f, "Meta Wavetable Editor");
        harmonicEditorCloseRect_ = { r.x + r.w - 86.0f, r.y + 14.0f, 68.0f, 28.0f };
        drawButton(harmonicEditorCloseRect_, "Close", false);

        auto &slot = track->metaOsc;
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
        selectedMetaHarmonic_ = clampi(selectedMetaHarmonic_, 0, synth::kEditableWavetableHarmonics - 1);
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        auto &h = frame.harmonics[(size_t)selectedMetaHarmonic_];

        // Tool buttons row (no TIME/SPECTRUM tabs — both panels always visible)
        const float toolY = r.y + 50.0f;
        const float toolW = 64.0f;
        metaEditorImportRect_ = { r.x + 18.0f, toolY, 88.0f, 24.0f };
        metaEditorAddRect_ = { r.x + 112.0f, toolY, 52.0f, 24.0f };
        metaEditorDuplicateRect_ = { r.x + 170.0f, toolY, 70.0f, 24.0f };
        metaEditorDeleteRect_ = { r.x + 246.0f, toolY, toolW, 24.0f };
        metaEditorLeftRect_ = { r.x + 316.0f, toolY, 36.0f, 24.0f };
        metaEditorRightRect_ = { r.x + 358.0f, toolY, 36.0f, 24.0f };
        metaSelAllRect_ = {};
        drawButton(metaEditorImportRect_, "Import WAV", false);
        drawButton(metaEditorAddRect_, "Add", false);
        drawButton(metaEditorDuplicateRect_, "Duplicate", false);
        drawButton(metaEditorDeleteRect_, "Delete", false);
        drawButton(metaEditorLeftRect_, "<", false);
        drawButton(metaEditorRightRect_, ">", false);
        // Selection hint: Ctrl+click = range, Ctrl+A = all.
        fontSize(8.0f); fillColor(rgba(0x6a8090ff)); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 404.0f, toolY + 12.0f, "Ctrl+click=range  Ctrl+A=all", nullptr);

        // Frame strip (48 px tall — room for mini waveform preview)
        metaEditorFrameStripRect_ = { r.x + 18.0f, r.y + 82.0f, r.w - 36.0f, 48.0f };
        drawMetaFrameStrip(metaEditorFrameStripRect_, slot);

        // Continuous scroll bar below frame strip
        metaFrameScrollRect_ = { r.x + 18.0f, r.y + 134.0f, r.w - 36.0f, 12.0f };
        drawFrameScrollbar(metaFrameScrollRect_, slot);

        // Status bar
        char title[160];
        std::snprintf(title, sizeof(title), "Frame %02d/%02d  Bin %03d  Amp %.3g  Phase %.3g  %s",
                      selectedMetaFrame_ + 1, slot.frameCount, selectedMetaHarmonic_ + 1,
                      h.amp, h.phase, metaEditorStatus_.c_str());
        drawLabelBox({ r.x + 18.0f, r.y + 150.0f, r.w - 36.0f, 26.0f }, title);

        // 三分布局: 相位图(22%) + 振幅谱(28%) + 波形(50%)
        const float totalDispH = r.h - 230.0f;
        const float phaseH = std::max(30.0f, totalDispH * 0.22f);
        const float specH  = std::max(30.0f, totalDispH * 0.28f);
        const float waveH  = totalDispH - phaseH - specH - 12.0f;
        const float dispY  = r.y + 180.0f;

        // --- 相位图面板 (上方) ---
        harmonicEditorPhaseRect_ = { r.x + 18.0f, dispY, r.w - 36.0f, phaseH };
        drawPanel(harmonicEditorPhaseRect_, rgba(0x0d1620ff), rgba(0x1e3040ff));
        {
            const Rect &pr = harmonicEditorPhaseRect_;
            const float barW = pr.w / float(synth::kEditableWavetableHarmonics);
            const float midY = pr.y + pr.h * 0.5f;
            strokeLine(pr.x + 2.0f, midY, pr.x + pr.w - 2.0f, midY, rgba(0x2b3f48ff), 0.8f);
            scissor(pr.x + 2.0f, pr.y + 2.0f, pr.w - 4.0f, pr.h - 4.0f);
            for(int i = 0; i < synth::kEditableWavetableHarmonics; ++i)
            {
                const float ph = frame.harmonics[(size_t)i].phase; // -π..+π
                const float norm = ph / kPi; // -1..+1
                const float bx = pr.x + float(i) * barW + 0.5f;
                const float bw = std::max(1.0f, barW - 1.0f);
                const float bh = norm * (pr.h * 0.44f);
                const float by = bh >= 0.0f ? midY - bh : midY;
                beginPath();
                rect(bx, by, bw, std::abs(bh));
                fillColor(i == selectedMetaHarmonic_ ? rgba(0xff9030dd) : rgba(0x7060a0aa));
                fill();
            }
            resetScissor();
            // 标签
            fontSize(8.0f);
            fillColor(rgba(0x6080a0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(pr.x + 4.0f, pr.y + 2.0f, "PHASE", nullptr);
        }

        // --- Spectrum panel (harmonic amplitude) ---
        harmonicEditorSpectrumRect_ = { r.x + 18.0f, dispY + phaseH + 6.0f, r.w - 36.0f, specH };
        drawPanel(harmonicEditorSpectrumRect_, rgba(0x101820ff), rgba(0x263842ff));
        {
            const Rect &sr = harmonicEditorSpectrumRect_;
            const float barW = sr.w / float(synth::kEditableWavetableHarmonics);
            scissor(sr.x + 2.0f, sr.y + 2.0f, sr.w - 4.0f, sr.h - 4.0f);
            for(int i = 0; i < synth::kEditableWavetableHarmonics; ++i)
            {
                const float amp = clampf(frame.harmonics[(size_t)i].amp, 0.0f, 1.0f);
                const float bx = sr.x + float(i) * barW;
                const float bh = amp * (sr.h - 10.0f);
                beginPath();
                rect(bx + 0.5f, sr.y + sr.h - 5.0f - bh, std::max(1.0f, barW - 1.0f), bh);
                fillColor(i == selectedMetaHarmonic_ ? rgba(0x8be87dff) : rgba(0x4d7780dd));
                fill();
            }
            resetScissor();
        }

        // --- Waveform panel (time domain) ---
        harmonicEditorBarsRect_ = { r.x + 18.0f, harmonicEditorSpectrumRect_.y + specH + 6.0f, r.w - 36.0f, waveH };
        drawPanel(harmonicEditorBarsRect_, rgba(0x101820ff), rgba(0x263842ff));
        {
            const Rect &wr = harmonicEditorBarsRect_;
            strokeLine(wr.x + 4.0f, wr.y + wr.h * 0.5f,
                       wr.x + wr.w - 4.0f, wr.y + wr.h * 0.5f, rgba(0x2b3f48ff), 1.0f);
            scissor(wr.x + 2.0f, wr.y + 2.0f, wr.w - 4.0f, wr.h - 4.0f);
            beginPath();
            for(int i = 0; i < 512; ++i)
            {
                const float t = float(i) / 511.0f;
                float value = 0.0f;
                if(frame.waveform)
                {
                    const int sample = clampi(int(t * float(synth::kWavetableSize - 1)), 0, synth::kWavetableSize - 1);
                    value = (*frame.waveform)[(size_t)sample];
                }
                else
                {
                    for(const auto &harmonic : frame.harmonics)
                        if(harmonic.amp > 0.0f && harmonic.ratio > 0.0f)
                            value += harmonic.amp * std::sin(2.0f * kPi * t * harmonic.ratio + harmonic.phase);
                    value = clampf(value, -1.0f, 1.0f);
                }
                const float px = wr.x + 4.0f + t * (wr.w - 8.0f);
                const float py = wr.y + wr.h * 0.5f - value * (wr.h * 0.42f);
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(rgba(0x63d2ffff));
            strokeWidth(1.5f);
            stroke();
            resetScissor();
        }

        const float sliderY = r.y + r.h - 50.0f;
        const float third = (r.w - 52.0f) / 3.0f;
        metaHarmonicRatioRect_ = { r.x + 18.0f, sliderY, third, 24.0f };
        metaHarmonicAmpRect_ = { metaHarmonicRatioRect_.x + third + 8.0f, sliderY, third, 24.0f };
        metaHarmonicPhaseRect_ = { metaHarmonicAmpRect_.x + third + 8.0f, sliderY, third, 24.0f };
        drawSlider(metaHarmonicRatioRect_, "Bin", float(selectedMetaHarmonic_) / float(synth::kEditableWavetableHarmonics - 1), float(selectedMetaHarmonic_ + 1));
        drawSlider(metaHarmonicAmpRect_, "H Amp", h.amp, h.amp);
        drawSlider(metaHarmonicPhaseRect_, "H Phase", (h.phase + kPi) / (2.0f * kPi), h.phase);
    }

END_NAMESPACE_DISTRHO
