#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawBasicTrackEditor(const Rect &r, synth::SourceTrackParams &track)
{
        basicShapeRect_ = { r.x, r.y, 180.0f, 26.0f };
        basicPulseRect_ = { r.x, r.y + 36.0f, 260.0f, 24.0f };
        basicSubRect_ = { r.x, r.y + 66.0f, 260.0f, 24.0f };
        drawButton(basicShapeRect_, synth::basicOscillatorShapeName(track.basicShape), false);
        drawSlider(basicPulseRect_, "Pulse Width", track.pulseWidth, track.pulseWidth);
        drawSlider(basicSubRect_, "Sub Level", track.subLevel, track.subLevel);

        // Same OCT/SEM/FIN/CRS module the other oscillators have; it shifts the
        // whole harmonic series, so the shape is unchanged and only the
        // fundamental moves. Shares the meta pitch rects — one editor is drawn
        // at a time and the handlers resolve the target by track type.
        constexpr float pitchRowH = 30.0f;
        // Clamped so a short panel never pushes the row past the panel bottom.
        const float pitchY = std::min(r.y + 98.0f, r.y + r.h - pitchRowH);
        const float pitchGap = 6.0f;
        const float pitchW = std::min(96.0f, std::max(56.0f, (r.w - pitchGap * 3.0f) * 0.25f));
        metaOctRect_ = { r.x, pitchY, pitchW, pitchRowH };
        metaSemRect_ = { metaOctRect_.x + pitchW + pitchGap, pitchY, pitchW, pitchRowH };
        metaFinRect_ = { metaSemRect_.x + pitchW + pitchGap, pitchY, pitchW, pitchRowH };
        metaCrsRect_ = { metaFinRect_.x + pitchW + pitchGap, pitchY, pitchW, pitchRowH };
        drawPitchControl(metaOctRect_, "OCT", track.basicPitchOct, false);
        drawPitchControl(metaSemRect_, "SEM", track.basicPitchSem, false);
        drawPitchControl(metaFinRect_, "FIN", int(std::round(track.basicPitchFin)), false);
        drawPitchControl(metaCrsRect_, "CRS", int(std::round(track.basicPitchCrs)), false);

        // Large waveform preview fills the rest of the panel (no dead space).
        const float previewTop = pitchY + pitchRowH + 22.0f;
        const float previewH = (r.y + r.h) - previewTop;
        if(previewH > 70.0f)
        {
            drawSectionTitle(r.x, previewTop - 18.0f, "Waveform");
            drawStripThumbnail({ r.x, previewTop, r.w, previewH }, track, selectedTrack_, false);
        }
    }

END_NAMESPACE_DISTRHO
