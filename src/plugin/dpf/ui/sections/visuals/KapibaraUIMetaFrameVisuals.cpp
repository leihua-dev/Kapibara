#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Shared frame-selection logic (single, or ctrl/shift range) for every frame strip.
    // Is morph being modulated (matrix rule targeting this track's MetaMorph)?

    // Frame click. Ctrl/Shift held: extend a range from the anchor. Plain click:
    // single-select + (if morph isn't modulated) auto-jump morph to that frame.





    // ------ Serum 风格 3D 多帧波形预览 ---------------------------
    // 复刻引擎 warpTablePhase 的相位重映射（归一化 0..1），用于波表预览显示 warp/bend。
void KapibaraUI::drawMetaPartialEditor(const Rect &r)
{
        drawSectionTitle(r.x, r.y, "MetaPartial");
        const int sourceCount = synth::sanitizeGeneratorSourceCount(generator_.sourceCount);
        const int metas = synth::metaPartialsPerGeneratorSource(sourceCount);
        selectedSource_ = clampi(selectedSource_, 0, sourceCount - 1);
        const int firstMeta = selectedSource_ * metas;
        selectedMetaPartial_ = clampi(selectedMetaPartial_, firstMeta, firstMeta + metas - 1);
        for(auto &rect : metaSelectRects_)
            rect = {};
        const float buttonW = std::max(42.0f, (r.w - float(std::max(0, metas - 1)) * 5.0f) / float(metas));
        for(int local = 0; local < metas; ++local)
        {
            const int i = firstMeta + local;
            metaSelectRects_[(size_t)i] = { r.x + float(local) * (buttonW + 5.0f), r.y + 26.0f, buttonW, 20.0f };
            drawButton(metaSelectRects_[(size_t)i], buttonText("MP%d", i + 1), selectedMetaPartial_ == i);
        }

        auto &slot = generator_.wavetableSeed.partials[(size_t)selectedMetaPartial_];
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
        selectedMetaHarmonic_ = clampi(selectedMetaHarmonic_, 0, synth::kEditableWavetableHarmonics - 1);
        auto &frame = slot.frames[(size_t)selectedMetaFrame_];
        auto &harmonic = frame.harmonics[(size_t)selectedMetaHarmonic_];

        const float gap = 8.0f;
        const float colW = (r.w - gap) * 0.5f;
        metaEnableRect_ = { r.x, r.y + 54.0f, 70.0f, 20.0f };
        metaWarpModeRect_ = { r.x + 78.0f, r.y + 54.0f, 110.0f, 20.0f };
        metaFrameButtonRect_ = { r.x + 196.0f, r.y + 54.0f, std::max(68.0f, r.w - 196.0f), 20.0f };
        drawButton(metaEnableRect_, slot.enabled ? "On" : "Off", slot.enabled);
        drawButton(metaWarpModeRect_, warpModeName(slot.warpMode), false);
        drawButton(metaFrameButtonRect_, buttonText("Frame %d", selectedMetaFrame_ + 1), false);

        metaLoadRect_ = { r.x, r.y + 82.0f, 44.0f, 20.0f };
        metaSaveRect_ = { r.x + 48.0f, r.y + 82.0f, 44.0f, 20.0f };
        const float presetBase = 96.0f;
        const float presetW = std::min(46.0f, std::max(32.0f, (r.w - presetBase - 84.0f) / 5.0f));
        for(int i = 0; i < 5; ++i)
            metaFramePresetRects_[(size_t)i] = { r.x + presetBase + float(i) * (presetW + 5.0f), r.y + 82.0f, presetW, 20.0f };
        metaLoadPathRect_ = { r.x + presetBase + 5.0f * (presetW + 5.0f), r.y + 82.0f,
                              std::max(60.0f, r.w - presetBase - 5.0f * (presetW + 5.0f)), 20.0f };
        drawButton(metaLoadRect_, "Load", loadPathEditing_);
        drawButton(metaSaveRect_, "Save", false);
        drawButton(metaFramePresetRects_[0], "Sin", false);
        drawButton(metaFramePresetRects_[1], "Saw", false);
        drawButton(metaFramePresetRects_[2], "Sqr", false);
        drawButton(metaFramePresetRects_[3], "Tri", false);
        drawButton(metaFramePresetRects_[4], "Clr", false);
        const std::string pathText = loadPathEditing_ ? ("> " + loadPathBuffer_) : loadStatus_;
        drawLabelBox(metaLoadPathRect_, pathText.empty() ? "type wav path after Load" : pathText.c_str());

        metaFrameStripRect_ = { r.x, r.y + 108.0f, r.w, 22.0f };
        drawMetaFrameStrip(metaFrameStripRect_, slot);

        const float waveformH = clampf(r.h - 230.0f, 68.0f, 128.0f);
        metaWaveformRect_ = { r.x, r.y + 136.0f, r.w, waveformH };
        drawMetaWaveformEditor(metaWaveformRect_, frame);

        const float controlY = metaWaveformRect_.y + metaWaveformRect_.h + 8.0f;
        metaRatioRect_ = { r.x, controlY, colW, 20.0f };
        metaAmpRect_ = { r.x + colW + gap, controlY, colW, 20.0f };
        metaPhaseRect_ = { r.x, controlY + 26.0f, colW, 20.0f };
        metaPanRect_ = { r.x + colW + gap, controlY + 26.0f, colW, 20.0f };
        metaFrameCountRect_ = { r.x, controlY + 52.0f, colW, 20.0f };
        metaMorphRect_ = { r.x + colW + gap, controlY + 52.0f, colW, 20.0f };
        drawSlider(metaRatioRect_, "Ratio", std::log2(std::max(0.01f, slot.ratio)) / 7.0f, slot.ratio);
        drawSlider(metaAmpRect_, "Amp", slot.amp, slot.amp);
        drawSlider(metaPhaseRect_, "Phase", (slot.phase + kPi) / (2.0f * kPi), slot.phase);
        drawSlider(metaPanRect_, "Pan", (slot.pan + 1.0f) * 0.5f, slot.pan);
        drawSlider(metaFrameCountRect_, "Frames", float(slot.frameCount - 1) / float(synth::kMaxWavetableFrames - 1), float(slot.frameCount));
        drawSlider(metaMorphRect_, "Morph", slot.morph, slot.morph);

        metaWarpAmountRect_ = { r.x, controlY + 78.0f, colW, 20.0f };
        drawSlider(metaWarpAmountRect_, "Warp Amt", (slot.warpAmount + 1.0f) * 0.5f, slot.warpAmount);

        const float thirdW = (colW - gap * 2.0f) / 3.0f;
        metaHarmonicEditRect_ = { r.x + colW + gap, controlY + 78.0f, thirdW, 20.0f };
        metaHarmonicRatioRect_ = { metaHarmonicEditRect_.x + thirdW + gap, controlY + 78.0f, thirdW, 20.0f };
        metaHarmonicAmpRect_ = { metaHarmonicRatioRect_.x + thirdW + gap, controlY + 78.0f, thirdW, 20.0f };
        drawButton(metaHarmonicEditRect_, "Edit H", harmonicEditorOpen_);
        drawSlider(metaHarmonicRatioRect_, "H", float(selectedMetaHarmonic_ + 1) / float(synth::kEditableWavetableHarmonics), float(selectedMetaHarmonic_ + 1));
        drawSlider(metaHarmonicAmpRect_, "Amp", harmonic.amp, harmonic.amp);
        metaHarmonicPhaseRect_ = {};
    }

void KapibaraUI::drawFrameScrollbar(const Rect &r, const synth::WavetablePartialSlot &slot)
{
        drawPanel(r, rgba(0x0c1318ff), rgba(0x1e2c34ff));
        if(slot.frameCount <= synth::kVisibleWavetableFrames)
            return;
        const int maxScroll = slot.frameCount - synth::kVisibleWavetableFrames;
        const float visibleFrac = float(synth::kVisibleWavetableFrames) / float(slot.frameCount);
        const float thumbW = std::max(24.0f, (r.w - 8.0f) * visibleFrac);
        const float thumbX = r.x + 4.0f + (r.w - 8.0f - thumbW)
                           * float(metaFrameScrollStart_) / float(maxScroll);
        beginPath();
        roundedRect(thumbX, r.y + 2.0f, thumbW, r.h - 4.0f, 3.0f);
        fillColor(rgba(0x4a6c7cff));
        fill();
    }

bool KapibaraUI::morphIsModulated(const synth::SourceTrackParams &t) const
{
        for(const auto &r : rules_)
            if(r.enabled && r.targetTrackId == t.id && r.dest == synth::ModDestination::MetaMorph)
                return true;
        return false;
    }

void KapibaraUI::selectMetaFrameAt(int frameIndex, int frameCount)
{
        if(frameIndex < 0 || frameIndex >= frameCount)
            return;
        if(ctrlDown_ || shiftDown_)
        {
            if(metaFrameRangeAnchor_ < 0 || metaFrameRangeAnchor_ >= frameCount)
            {
                metaFrameRangeAnchor_ = frameIndex;
                metaFrameSelected_.fill(false);
                metaFrameSelected_[(size_t)frameIndex] = true;
                selectedMetaFrame_ = frameIndex;
                return;
            }
            const int a = std::min(metaFrameRangeAnchor_, frameIndex);
            const int b = std::max(metaFrameRangeAnchor_, frameIndex);
            metaFrameSelected_.fill(false);
            for(int i = a; i <= b; ++i)
                metaFrameSelected_[(size_t)i] = true;
            selectedMetaFrame_ = frameIndex;
            return;
        }
        // Plain single select.
        metaFrameSelected_.fill(false);
        metaFrameSelected_[(size_t)frameIndex] = true;
        selectedMetaFrame_ = frameIndex;
        metaFrameRangeAnchor_ = frameIndex;
        if(auto *track = currentTrack();
           track != nullptr && track->type == synth::SourceTrackType::MetaOscillator
           && frameCount >= 2 && !morphIsModulated(*track))
        {
            track->metaOsc.morph = float(frameIndex) / float(frameCount - 1);
            pushCurrentTrack();
        }
        else if(auto *track = currentTrack();
                track != nullptr && track->type == synth::SourceTrackType::PartialBank
                && frameCount >= 2)
        {
            track->partialBank.morph = float(frameIndex) / float(frameCount - 1);
            pushCurrentTrack();
        }
    }

void KapibaraUI::selectAllMetaFrames()
{
        auto *track = currentTrack();
        const int fc = track && track->type == synth::SourceTrackType::PartialBank
                           ? track->partialBank.frameCount
                           : (track ? track->metaOsc.frameCount : 0);
        for(int i = 0; i < synth::kMaxWavetableFrames; ++i)
            metaFrameSelected_[(size_t)i] = i < fc;
        metaFrameRangeAnchor_ = fc > 0 ? 0 : -1;
    }

void KapibaraUI::drawMetaFrameStrip(const Rect &r, const synth::WavetablePartialSlot &slot)
{
        metaFramesShown_ = true; // a clickable frame strip is on screen this frame
        metaFrameScrollStart_ = clampi(metaFrameScrollStart_, 0,
                                       std::max(0, slot.frameCount - synth::kVisibleWavetableFrames));
        metaFramePageStart_ = metaFrameScrollStart_;

        const float cellW = r.w / float(synth::kVisibleWavetableFrames);
        const float waveAreaH = r.h - 15.0f;  // top area for mini waveform
        for(int local = 0; local < synth::kVisibleWavetableFrames; ++local)
        {
            const int frameIndex = metaFrameScrollStart_ + local;
            const Rect btn { r.x + float(local) * cellW + 1.0f, r.y, cellW - 2.0f, r.h };
            metaFrameRects_[(size_t)local] = btn;
            const bool active = frameIndex < slot.frameCount;
            const bool multiSel = active && (size_t)frameIndex < metaFrameSelected_.size() && metaFrameSelected_[(size_t)frameIndex];
            const bool focused = frameIndex == selectedMetaFrame_;
            const bool selected = multiSel || focused;

            // Selection is a real multi-frame set; every selected frame gets the
            // same bright treatment, while the focused edit frame gets an extra cap.
            const uint32_t bgCol  = selected ? 0x263840ff : 0x151d22ff;
            const uint32_t brdCol = selected ? 0x70d77aff : 0x354851ff;
            drawPanel(btn, rgba(bgCol), rgba(brdCol));
            if(selected && active)
            {
                beginPath();
                roundedRect(btn.x + 3.0f, btn.y + 3.0f, btn.w - 6.0f, 3.0f, 1.5f);
                fillColor(rgba(0x8df7aaff));
                fill();
            }

            // Mini waveform preview in upper area
            if(active)
            {
                const auto &frm = slot.frames[(size_t)frameIndex];
                scissor(btn.x + 2.0f, btn.y + 2.0f, btn.w - 4.0f, waveAreaH - 2.0f);
                beginPath();
                const int steps = 28;
                for(int i = 0; i < steps; ++i)
                {
                    const float t = float(i) / float(steps - 1);
                    float v = 0.0f;
                    if(frm.waveform)
                    {
                        const int s = clampi(int(t * float(synth::kWavetableSize - 1)), 0, synth::kWavetableSize - 1);
                        v = (*frm.waveform)[(size_t)s];
                    }
                    else
                    {
                        for(const auto &hm : frm.harmonics)
                            if(hm.amp > 0.0f && hm.ratio > 0.0f)
                                v += hm.amp * std::sin(2.0f * kPi * t * hm.ratio + hm.phase);
                        v = clampf(v, -1.0f, 1.0f);
                    }
                    const float px = btn.x + 3.0f + t * (btn.w - 6.0f);
                    const float py = btn.y + 2.0f + waveAreaH * 0.5f - v * (waveAreaH * 0.42f);
                    if(i == 0) moveTo(px, py); else lineTo(px, py);
                }
                strokeColor(selected ? rgba(0x63d2ffff) : rgba(0x3a7a90cc));
                strokeWidth(1.0f);
                stroke();
                resetScissor();
            }

            // Frame number label at bottom
            useUiFont();
            uiFontSize(11.0f);
            textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
            fillColor(selected ? rgba(0xf4fff4ff) : rgba(0x8aaabcff));
            char numBuf[16];
            std::snprintf(numBuf, sizeof(numBuf), "%03d", frameIndex + 1);
            text(btn.x + btn.w * 0.5f, btn.y + btn.h - 1.0f, numBuf, nullptr);

            if(!active)
            {
                beginPath();
                rect(btn.x, btn.y, btn.w, btn.h);
                fillColor(rgba(0x00000099));
                fill();
            }
        }
    }

void KapibaraUI::drawPitchControl(const Rect &r, const char *label, int value, bool)
{
        drawPanel(r, rgba(0x0d1920ff), rgba(0x243240ff));
        useUiFont();
        uiFontSize(10.0f);
        fillColor(rgba(0x5a8aa8ff));
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 7.0f, r.y + r.h * 0.5f, label, nullptr);

        char buf[16];
        std::snprintf(buf, sizeof(buf), value > 0 ? "+%d" : "%d", value);
        uiFontSize(13.0f);
        fillColor(rgba(0x9be7a1ff));
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        text(r.x + r.w - 7.0f, r.y + r.h * 0.5f, buf, nullptr);
    }

void KapibaraUI::drawPitchControlF(const Rect &r, const char *label, float value)
{
        drawPanel(r, rgba(0x0d1920ff), rgba(0x243240ff));
        useUiFont();
        uiFontSize(10.0f);
        fillColor(rgba(0x5a8aa8ff));
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(r.x + 7.0f, r.y + r.h * 0.5f, label, nullptr);

        char buf[16];
        std::snprintf(buf, sizeof(buf), value >= 0.0f ? "+%.2f" : "%.2f", value);
        uiFontSize(12.0f);
        fillColor(rgba(0x9be7a1ff));
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        text(r.x + r.w - 7.0f, r.y + r.h * 0.5f, buf, nullptr);
    }

float KapibaraUI::warpPhase01(synth::WavetableWarpMode mode, float amount, float x)
{
        if(mode == synth::WavetableWarpMode::None || std::fabs(amount) < 1.0e-5f)
            return x;
        x -= std::floor(x);
        const float centered = x * 2.0f - 1.0f;
        float y = x;
        switch(mode)
        {
            case synth::WavetableWarpMode::Bend:
            {
                const float bend = 1.0f + 3.0f * std::fabs(amount);
                y = amount >= 0.0f ? std::pow(x, bend) : 1.0f - std::pow(1.0f - x, bend);
                break;
            }
            case synth::WavetableWarpMode::Squeeze:
            {
                const float squeeze = 1.0f - 0.85f * std::fabs(amount);
                y = 0.5f + centered * 0.5f * squeeze;
                break;
            }
            case synth::WavetableWarpMode::Skew:
                y = x + amount * x * (1.0f - x) * (x < 0.5f ? 1.0f : -1.0f);
                break;
            case synth::WavetableWarpMode::None:
                break;
        }
        y -= std::floor(y);
        return y;
    }

float KapibaraUI::sampleFrameWarped(const synth::WavetableFrame &frame, float t,
                               synth::WavetableWarpMode mode, float amount)
{
        return sampleFrame(frame, warpPhase01(mode, amount, t));
    }

float KapibaraUI::sampleFrame(const synth::WavetableFrame &frame, float t)
{
        float v = 0.0f;
        if(frame.useImportedWaveform && frame.waveform)
        {
            const float pos = t * float(synth::kWavetableSize - 1);
            const int a = std::max(0, std::min(synth::kWavetableSize - 1, int(pos)));
            const int b = std::min(a + 1, synth::kWavetableSize - 1);
            v = (*frame.waveform)[(size_t)a] * (1.0f - (pos - float(a)))
              + (*frame.waveform)[(size_t)b] * (pos - float(a));
        }
        else
        {
            for(const auto &h : frame.harmonics)
            {
                if(h.amp <= 0.0f || h.ratio <= 0.0f) continue;
                v += h.amp * std::sin(2.0f * kPi * t * h.ratio + h.phase);
            }
            v = clampf(v, -1.0f, 1.0f);
        }
        return v;
    }

END_NAMESPACE_DISTRHO
