#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawNoiseTrackEditor(const Rect &r, synth::SourceTrackParams &track)
{
        noiseModeRect_ = { r.x, r.y, 180.0f, 26.0f };
        noiseColorRect_ = { r.x, r.y + 36.0f, 260.0f, 24.0f };
        drawButton(noiseModeRect_, synth::sampleNoiseModeName(track.sampleNoiseMode), false);
        drawSlider(noiseColorRect_, "Noise Color", track.noiseColor, track.noiseColor);
        drawLabelBox({ r.x, r.y + 70.0f, 260.0f, 24.0f }, "File/Capture unavailable in v1");
        // Large waveform preview fills the rest of the panel (no dead space).
        const float previewTop = r.y + 122.0f;
        const float previewH = (r.y + r.h) - previewTop;
        if(previewH > 70.0f)
        {
            drawSectionTitle(r.x, previewTop - 18.0f, "Signal");
            drawStripThumbnail({ r.x, previewTop, r.w, previewH }, track, selectedTrack_, false);
        }
    }

END_NAMESPACE_DISTRHO
