#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawMetaTrackEditor(const Rect &r, synth::SourceTrackParams &track)
{
        auto &slot = track.metaOsc;
        metaEnableRect_ = {};  // ON/OFF button removed
        metaWavetablePrevRect_ = { r.x + r.w - 60.0f, r.y, 28.0f, 28.0f };
        metaWavetableNextRect_ = { r.x + r.w - 28.0f, r.y, 28.0f, 28.0f };
        metaWavetableNameRect_ = { r.x, r.y, std::max(80.0f, r.w - 66.0f), 28.0f };
        const char *wavetableTitle = wavetablePresetLabel_.empty() ? "Select Wavetable" : wavetablePresetLabel_.c_str();
        drawDropdown(metaWavetableNameRect_, wavetableTitle, wavetablePresetMenuOpen_);
        drawButton(metaWavetablePrevRect_, "<", false);
        drawButton(metaWavetableNextRect_, ">", false);

        const float pitchY = r.y + 32.0f;
        const float pitchGap = 6.0f;
        const float pitchW = (r.w - pitchGap * 3.0f) * 0.25f;
        metaOctRect_ = { r.x, pitchY, pitchW, 26.0f };
        metaSemRect_ = { metaOctRect_.x + pitchW + pitchGap, pitchY, pitchW, 26.0f };
        metaFinRect_ = { metaSemRect_.x + pitchW + pitchGap, pitchY, pitchW, 26.0f };
        metaCrsRect_ = { metaFinRect_.x + pitchW + pitchGap, pitchY, pitchW, 26.0f };
        drawPitchControl(metaOctRect_, "OCT", slot.pitchOct, false);
        drawPitchControl(metaSemRect_, "SEM", slot.pitchSem, false);
        drawPitchControl(metaFinRect_, "FIN", int(slot.pitchFin), false);
        drawPitchControlF(metaCrsRect_, "CRS", slot.pitchCrs);

        const float colW = 116.0f;
        const float waveformTop = r.y + 62.0f;
        const float waveformBottom = r.y + r.h;
        const float waveformW = std::max(120.0f, r.w - colW - 12.0f);
        metaWaveformRect_ = { r.x, waveformTop, waveformW, std::max(60.0f, waveformBottom - waveformTop) };
        drawMeta3DWaveform(metaWaveformRect_, slot, selectedTrack_);

        const float cx = r.x + waveformW + 12.0f;
        float ky = waveformTop;
        metaFrameCountRect_ = {};  // Frames 控件从主界面移除
        metaWarpModeRect_   = { cx, ky, colW, 26.0f }; ky += 32.0f;
        metaMorphRect_      = { cx, ky, colW, 38.0f }; ky += 42.0f;
        metaWarpAmountRect_ = { cx, ky, colW, 38.0f }; ky += 42.0f;
        metaPhaseRect_      = { cx, ky, colW, 38.0f }; ky += 42.0f;
        metaPhaseRandRect_  = { cx, ky, colW, 38.0f }; ky += 42.0f;
        metaPanRect_        = { cx, ky, colW, 38.0f };
        drawButton(metaWarpModeRect_, warpModeName(slot.warpMode), false);
        drawKnob(metaMorphRect_, "Morph", slot.morph, slot.morph);
        drawKnob(metaWarpAmountRect_, "Warp", (slot.warpAmount + 1.0f) * 0.5f, slot.warpAmount);
        drawKnob(metaPhaseRect_, "Phase", (slot.phase + kPi) / (2.0f * kPi), slot.phase);
        drawKnob(metaPhaseRandRect_, "Rand", slot.phaseRandom, slot.phaseRandom);
        drawKnob(metaPanRect_, "Pan", (slot.pan + 1.0f) * 0.5f, slot.pan);

        metaLoadRect_ = {};
        for(auto &presetRect : metaFramePresetRects_)
            presetRect = {};
        metaHarmonicEditRect_ = {};
    }

END_NAMESPACE_DISTRHO
