#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawMetaTrackEditor(const Rect &r, synth::SourceTrackParams &track)
{
        auto &slot = track.metaOsc;
        // Legacy controls not used by this layout.
        metaEnableRect_ = {};
        metaFrameCountRect_ = {};
        metaMorphRect_ = {};
        metaLoadRect_ = {};
        for(auto &presetRect : metaFramePresetRects_)
            presetRect = {};
        metaHarmonicEditRect_ = {};

        const float rightW = 158.0f;
        const float gap = 12.0f;
        const float waveW = std::max(140.0f, r.w - rightW - gap);
        const float px = r.x + waveW + gap;

        // --- Row: wavetable selector (slim) ------------------------------------
        const float selH = 24.0f;
        metaWavetablePrevRect_ = { r.x + r.w - 58.0f, r.y, 27.0f, selH };
        metaWavetableNextRect_ = { r.x + r.w - 27.0f, r.y, 27.0f, selH };
        metaWavetableNameRect_ = { r.x, r.y, std::max(80.0f, r.w - 64.0f), selH };
        const char *wavetableTitle = wavetablePresetLabel_.empty() ? "Select Wavetable" : wavetablePresetLabel_.c_str();
        drawDropdown(metaWavetableNameRect_, wavetableTitle, wavetablePresetMenuOpen_);
        drawButton(metaWavetablePrevRect_, "<", false);
        drawButton(metaWavetableNextRect_, ">", false);

        // --- PITCH cells: slim, only above the wave view (no silk-screen label) --
        const float topRow = r.y + selH + 6.0f;
        const float pitchH = 20.0f;
        const float pitchGap = 6.0f;
        const float pitchW = (waveW - pitchGap * 3.0f) * 0.25f;
        metaOctRect_ = { r.x, topRow, pitchW, pitchH };
        metaSemRect_ = { metaOctRect_.x + pitchW + pitchGap, topRow, pitchW, pitchH };
        metaFinRect_ = { metaSemRect_.x + pitchW + pitchGap, topRow, pitchW, pitchH };
        metaCrsRect_ = { metaFinRect_.x + pitchW + pitchGap, topRow, pitchW, pitchH };
        drawPitchControl(metaOctRect_, "OCT", slot.pitchOct, false);
        drawPitchControl(metaSemRect_, "SEM", slot.pitchSem, false);
        drawPitchControl(metaFinRect_, "FIN", int(slot.pitchFin), false);
        drawPitchControlF(metaCrsRect_, "CRS", slot.pitchCrs);

        // Wave view ends level with the WAVETABLE panel bottom (the PAN row) so the
        // UNISON row can tuck right beneath it at PAN height. The area below UNISON
        // is left empty on purpose — reserved for the future source-mod module.
        const float waveTop = topRow + pitchH + 8.0f;
        const float rowH = 30.0f;
        const float rowPitch = 34.0f;
        const float panelBottom = (topRow + 20.0f) + 32.0f + 3.0f * rowPitch + rowH; // = PAN row bottom
        oscBodyBottomY_ = panelBottom;
        oscBodyLeftW_ = waveW;  // UNISON spans only the wave column, not the PAN column

        metaWaveformRect_ = { r.x, waveTop, waveW, std::max(60.0f, panelBottom - waveTop) };
        // drawMeta3DWaveform lays out the vertical Morph scrubber and stores its rect.
        drawMeta3DWaveform(metaWaveformRect_, slot, selectedTrack_);
        metaMorphRect_ = metaMorphSliderRect_;  // keep morph reachable as a Matrix mod target

        // --- WAVETABLE panel (right) starts up at the pitch row, reclaiming the
        //     space the full-width pitch row used to occupy. -----------------------
        drawGroupLabel(px, topRow, "WAVETABLE");
        strokeLine(px, topRow + 13.0f, px + rightW, topRow + 13.0f,
                   DesignTokens::divider(), 1.0f);
        float ky = topRow + 20.0f;

        // MODE row: a labelled bar (right-click anywhere on it to pick a warp mode).
        metaWarpModeRect_ = { px, ky, rightW, 26.0f };
        drawButton(metaWarpModeRect_, "", false);
        useUiFont();
        uiFontSize(11.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textSecondary());
        text(px + 10.0f, ky + 13.0f, "MODE", nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        fillColor(DesignTokens::textPrimary());
        text(px + rightW - 10.0f, ky + 13.0f, warpModeName(slot.warpMode) + 5, nullptr); // strip "Warp "
        ky += 32.0f;

        // AMT / PHASE / RANDOM / PAN — strict aligned instrument column: knob, a
        // condensed label, and a right-aligned monospace value with fixed decimals.
        char vbuf[24];
        metaWarpAmountRect_ = { px, ky, rightW, rowH };
        std::snprintf(vbuf, sizeof(vbuf), "%.2f", slot.warpAmount);
        drawParamKnobRow(metaWarpAmountRect_, "AMT", (slot.warpAmount + 1.0f) * 0.5f, vbuf);
        ky += rowPitch;
        metaPhaseRect_ = { px, ky, rightW, rowH };
        std::snprintf(vbuf, sizeof(vbuf), "%.2f", slot.phase);
        drawParamKnobRow(metaPhaseRect_, "PHASE", (slot.phase + kPi) / (2.0f * kPi), vbuf);
        ky += rowPitch;
        metaPhaseRandRect_ = { px, ky, rightW, rowH };
        std::snprintf(vbuf, sizeof(vbuf), "%.2f", slot.phaseRandom);
        drawParamKnobRow(metaPhaseRandRect_, "RANDOM", slot.phaseRandom, vbuf);
        ky += rowPitch;
        metaPanRect_ = { px, ky, rightW, rowH };
        std::snprintf(vbuf, sizeof(vbuf), "%.2f", slot.pan);
        drawParamKnobRow(metaPanRect_, "PAN", (slot.pan + 1.0f) * 0.5f, vbuf);
    }

END_NAMESPACE_DISTRHO
