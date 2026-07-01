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
        // Large waveform preview fills the rest of the panel (no dead space).
        const float previewTop = r.y + 104.0f;
        const float previewH = (r.y + r.h) - previewTop;
        if(previewH > 70.0f)
        {
            drawSectionTitle(r.x, previewTop - 18.0f, "Waveform");
            drawStripThumbnail({ r.x, previewTop, r.w, previewH }, track, selectedTrack_, false);
        }
    }

END_NAMESPACE_DISTRHO
