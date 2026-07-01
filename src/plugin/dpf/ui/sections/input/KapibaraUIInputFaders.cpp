#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::handleStripFaderPress(float x, float y)
{
        const int n = int(generator_.tracks.size());
        for(int i = 0; i < n && i < int(synth::kMaxSourceTracks); ++i)
        {
            DragTarget tgt = DragTarget::None;
            float norm = 0.0f;
            const auto &t = generator_.tracks[(size_t)i];
            if(stripGainRects_[(size_t)i].w > 0.0f && stripGainRects_[(size_t)i].contains(x, y))
            { tgt = DragTarget::TrackGain; norm = t.gain * 0.5f; }
            else if(stripPanRects_[(size_t)i].w > 0.0f && stripPanRects_[(size_t)i].contains(x, y))
            { tgt = DragTarget::TrackPan; norm = (t.pan + 1.0f) * 0.5f; }
            else if(stripSendRects_[(size_t)i].w > 0.0f && stripSendRects_[(size_t)i].contains(x, y))
            { tgt = DragTarget::TrackSend; norm = t.send; }
            if(tgt == DragTarget::None) continue;
            dragTrackIndex_ = i;           // adjust this strip directly; editor view unchanged
            dragTarget_ = tgt;
            dragStartY_ = y;
            dragStartNorm_ = clampf(norm, 0.0f, 1.0f);
            return true;
        }
        return false;
    }

END_NAMESPACE_DISTRHO
