#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawPartialBankLayerPreview(const Rect &r, synth::WavetableSeedParams &seed)
{
        drawPlotBackground(r, 8, 4);
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        const int morphFrame = partialBankMorphFrameIndex(seed);
        constexpr int kMaxShow = 18;
        const int stride = std::max(1, seed.frameCount / kMaxShow);
        std::vector<int> frames;
        frames.reserve(kMaxShow + 1);
        for(int f = 0; f < seed.frameCount; f += stride)
            frames.push_back(f);
        if(std::find(frames.begin(), frames.end(), morphFrame) == frames.end())
            frames.push_back(morphFrame);
        std::sort(frames.begin(), frames.end());

        const float plotX = r.x + 12.0f;
        const float plotW = r.w - 24.0f;
        const float baseY = r.y + r.h - 16.0f;
        const float depthY = std::min(r.h * 0.38f, float(frames.size()) * 5.0f);
        const float barW = plotW / float(synth::kMaxWavetablePartials);
        scissor(r.x + 2.0f, r.y + 2.0f, r.w - 4.0f, r.h - 4.0f);
        for(size_t di = 0; di < frames.size(); ++di)
        {
            const int frameIndex = frames[di];
            ensurePartialBankFrameDefaults(seed, frameIndex);
            const auto &frame = seed.frames[(size_t)frameIndex];
            const float depthT = frames.size() > 1 ? float(di) / float(frames.size() - 1) : 1.0f;
            const float y0 = baseY - depthY * (1.0f - depthT);
            const float height = (r.h - depthY - 28.0f) * (0.50f + depthT * 0.50f);
            const float alpha = frameIndex == morphFrame ? 0.95f : 0.18f + 0.35f * depthT;
            const Color col = frameIndex == morphFrame
                                  ? DesignTokens::accentGreen()
                                  : DesignTokens::accentCyan().withAlpha(alpha);
            strokeLine(plotX, y0, plotX + plotW, y0, DesignTokens::divider().withAlpha(0.35f), 0.6f);
            for(int i = 0; i < synth::kMaxWavetablePartials; ++i)
            {
                const float amp = clampf(frame.harmonics[(size_t)i].amp, 0.0f, 1.0f);
                if(amp <= 0.001f)
                    continue;
                const float x = plotX + float(i) * barW;
                const float bh = amp * height;
                beginPath();
                rect(x + 0.5f, y0 - bh, std::max(1.0f, barW - 1.0f), bh);
                fillColor(col);
                fill();
            }
            if(frameIndex == morphFrame)
            {
                uiFontSize(9.0f);
                fillColor(DesignTokens::accentGreen());
                textAlign(ALIGN_LEFT | ALIGN_TOP);
                text(r.x + 8.0f, y0 - height - 10.0f, buttonText("F%d", frameIndex + 1), nullptr);
            }
        }
        resetScissor();

        uiFontSize(10.0f);
        fillColor(rgba(0x7f9aabff));
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(r.x + 8.0f, r.y + 6.0f, "PARTIAL TABLE PREVIEW  amp/phase loaded per frame", nullptr);
    }

void KapibaraUI::drawPartialBankTrackEditor(const Rect &r, synth::SourceTrackParams &track)
{
        auto &seed = track.partialBank;
        seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
        ensurePartialBankFrameDefaults(seed, selectedMetaFrame_);
        for(auto &rect : partialKnobRects_)
            rect = {};

        metaWavetableNameRect_ = { r.x, r.y, std::max(120.0f, r.w - 66.0f), 28.0f };
        metaWavetablePrevRect_ = { r.x + r.w - 60.0f, r.y, 28.0f, 28.0f };
        metaWavetableNextRect_ = { r.x + r.w - 28.0f, r.y, 28.0f, 28.0f };
        const char *tableTitle = wavetablePresetLabel_.empty() ? "Select Table" : wavetablePresetLabel_.c_str();
        drawDropdown(metaWavetableNameRect_, tableTitle, wavetablePresetMenuOpen_);
        drawButton(metaWavetablePrevRect_, "<", false);
        drawButton(metaWavetableNextRect_, ">", false);

        const float topY = r.y + 40.0f;
        const float knobW = std::min(86.0f, std::max(58.0f, (r.w - 170.0f) / 4.0f));
        partialCountRect_ = { r.x, topY, knobW, 54.0f };
        inharmonicModeRect_ = { r.x + knobW + 8.0f, topY + 10.0f, 104.0f, 30.0f };
        inharmonicRect_ = { inharmonicModeRect_.x + inharmonicModeRect_.w + 8.0f, topY, knobW, 54.0f };
        metaFrameCountRect_ = {};
        metaMorphRect_ = { inharmonicRect_.x + knobW + 8.0f, topY, knobW, 54.0f };
        metaHarmonicEditRect_ = {};
        drawKnob(partialCountRect_, "Partials", float(seed.partialCount - 1) / 63.0f, float(seed.partialCount));
        drawButton(inharmonicModeRect_, freqShapeName(seed.freqShape), seed.freqShape != synth::FreqShape::Harmonic);
        drawKnob(inharmonicRect_, "Harmonize", seed.inharmonicAmount, seed.inharmonicAmount);
        drawKnob(metaMorphRect_, "Morph", seed.morph, seed.morph);

        metaFrameStripRect_ = {};
        metaFrameScrollRect_ = {};

        // Pitch sits ABOVE the preview, which then takes whatever is left. It used
        // to be pinned to the panel bottom with the preview floored at 120 px, so
        // the moment the panel got short — every OSC MOD entry takes 24 px out of
        // this editor — the preview grew straight over the OCT/SEM/FIN/CRS row.
        // Placed inline with the knob row when the panel is wide enough, which
        // keeps a whole row of height for the preview on cramped patches.
        constexpr float pitchRowH = 30.0f;
        selectedPartialIndex_ = clampi(selectedPartialIndex_, 0, std::max(0, seed.partialCount - 1));
        partialAmpRect_ = {};
        partialRatioRect_ = {};
        const auto &groupPitch = seed.partials[0];
        const float pitchGap = 6.0f;
        const float inlineX = metaMorphRect_.x + knobW + 12.0f;
        const float inlineAvail = (r.x + r.w) - inlineX;
        const bool inlinePitch = (inlineAvail - pitchGap * 3.0f) * 0.25f >= 52.0f;
        const float pitchW = inlinePitch
                                 ? std::min(96.0f, (inlineAvail - pitchGap * 3.0f) * 0.25f)
                                 : std::min(96.0f, std::max(56.0f, (r.w - pitchGap * 3.0f) * 0.25f));
        const float pitchX = inlinePitch ? inlineX : r.x;
        // Never past the panel bottom: on a very short panel (small window plus a
        // full OSC MOD zone) the stacked row would otherwise draw over the UNISON
        // divider below — the same collision, one element further down.
        const float pitchY = std::min(inlinePitch ? topY + 12.0f : topY + 60.0f,
                                      r.y + r.h - pitchRowH);
        metaOctRect_ = { pitchX, pitchY, pitchW, pitchRowH };
        metaSemRect_ = { metaOctRect_.x + pitchW + pitchGap, pitchY, pitchW, pitchRowH };
        metaFinRect_ = { metaSemRect_.x + pitchW + pitchGap, pitchY, pitchW, pitchRowH };
        metaCrsRect_ = { metaFinRect_.x + pitchW + pitchGap, pitchY, pitchW, pitchRowH };
        drawPitchControl(metaOctRect_, "OCT", groupPitch.pitchOct, false);
        drawPitchControl(metaSemRect_, "SEM", groupPitch.pitchSem, false);
        drawPitchControl(metaFinRect_, "FIN", int(std::round(groupPitch.pitchFin)), false);
        drawPitchControl(metaCrsRect_, "CRS", int(std::round(groupPitch.pitchCrs)), false);

        const float spectrumTop = inlinePitch ? topY + 66.0f : pitchY + pitchRowH + 8.0f;
        const float spectrumH = (r.y + r.h) - spectrumTop;
        partialSpectrumRect_ = {};
        if(spectrumH >= 48.0f)
        {
            const Rect spectrum { r.x, spectrumTop, r.w, spectrumH };
            partialSpectrumRect_ = spectrum;
            drawPartialBankLayerPreview(spectrum, seed);
        }
    }

END_NAMESPACE_DISTRHO
