#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

void KapibaraUI::drawSourceRack(const Rect &r)
{
        drawPanel(r, rgba(0x0b1217ff), rgba(0x344852ff));
        drawSectionTitle(r.x + 14.0f, r.y + 12.0f, "SOURCE RACK");
        addTrackRect_ = { r.x + 14.0f, r.y + 40.0f, r.w - 88.0f, 28.0f };
        removeTrackRect_ = { r.x + r.w - 66.0f, r.y + 40.0f, 52.0f, 28.0f };
        drawButton(addTrackRect_, addTrackMenuOpen_ ? "Choose Source Type" : "+ Add Source Track", addTrackMenuOpen_);
        drawButton(removeTrackRect_, "DEL", false);
        const char *labels[4] = { "Partial Bank", "Meta Oscillator", "Basic Oscillator", "Sample / Noise" };
        for(int i = 0; i < 4; ++i)
        {
            addTrackTypeRects_[(size_t)i] = addTrackMenuOpen_
                                                ? Rect { r.x + 14.0f, r.y + 74.0f + float(i) * 26.0f, r.w - 28.0f, 22.0f }
                                                : Rect {};
            if(addTrackMenuOpen_)
                drawButton(addTrackTypeRects_[(size_t)i], labels[i], false);
        }

        if(generator_.tracks.empty())
            generator_.tracks.push_back(synth::SourceTrackParams {});
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        const float startY = r.y + (addTrackMenuOpen_ ? 188.0f : 82.0f);
        const float rowH = 60.0f; // taller rows to fit mini waveform preview
        const float rowGap = 4.0f;
        for(auto &rr : trackRowRects_)
            rr = {};
        for(size_t i = 0; i < generator_.tracks.size() && i < trackRowRects_.size(); ++i)
        {
            const auto &track = generator_.tracks[i];
            const Rect row { r.x + 14.0f, startY + float(i) * (rowH + rowGap), r.w - 28.0f, rowH };
            trackRowRects_[i] = row;
            const bool isSelected = selectedTrack_ == int(i);
            const bool refsEnv = clampi(track.ampEnvIndex, 0, synth::kMaxAmpEnvs - 1) == selectedAmpEnv_;
            drawPanel(row, isSelected ? rgba(0x17242cff) : rgba(0x101820ff),
                      isSelected ? rgba(0x70d77aff) : rgba(0x263842ff));
            char label[64] {};
            std::snprintf(label, sizeof(label), "%02zu  %s", i + 1, track.name.c_str());
            fontSize(10.0f);
            fillColor(isSelected || refsEnv ? rgba(0x9eff50ff) : rgba(0xb0c8d0ff));
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            text(row.x + 6.0f, row.y + 4.0f, label, nullptr);
            // Mini waveform preview for MetaOscillator
            if(track.type == synth::SourceTrackType::MetaOscillator && track.metaOsc.frameCount > 0)
            {
                // Follow the knob when idle; switch to the engine's live morph once
                // a voice is actually playing this track.
                float liveMorph = track.metaOsc.morph;
                if(const auto *p = plugin())
                {
                    const float live = p->sourceLiveMorph(int(i));
                    if(live >= 0.0f)
                        liveMorph = live;
                }
                const int frameIdx = clampi(
                    int(liveMorph * float(track.metaOsc.frameCount - 1) + 0.5f),
                    0, track.metaOsc.frameCount - 1);
                const auto &frm = track.metaOsc.frames[(size_t)frameIdx];
                const Rect wr { row.x + 4.0f, row.y + 20.0f, row.w - 8.0f, rowH - 24.0f };
                scissor(wr.x, wr.y, wr.w, wr.h);
                const float midY = wr.y + wr.h * 0.5f;
                beginPath();
                for(int s = 0; s < int(wr.w); ++s)
                {
                    const float t = float(s) / wr.w;
                    const float v = sampleFrame(frm, t);
                    const float px2 = wr.x + float(s);
                    const float py2 = midY - v * wr.h * 0.46f;
                    if(s == 0) moveTo(px2, py2); else lineTo(px2, py2);
                }
                strokeColor(isSelected ? rgba(0x9eff50cc) : rgba(0x4d7780bb));
                strokeWidth(1.0f);
                stroke();
                resetScissor();
            }
            else
            {
                fontSize(9.0f);
                fillColor(rgba(0x607080ff));
                textAlign(ALIGN_LEFT | ALIGN_TOP);
                text(row.x + 6.0f, row.y + 22.0f, synth::sourceTrackTypeName(track.type), nullptr);
            }
        }
        const int metaUsed = int(std::count_if(generator_.tracks.begin(), generator_.tracks.end(), [](const auto &t) {
            return t.type == synth::SourceTrackType::MetaOscillator;
        }));
        int partialUsed = 0;
        int basicUsed = 0;
        int noiseUsed = 0;
        for(const auto &t : generator_.tracks)
        {
            if(t.type == synth::SourceTrackType::PartialBank)
                partialUsed += t.partialBank.partialCount;
            else if(t.type == synth::SourceTrackType::BasicOscillator)
                ++basicUsed;
            else if(t.type == synth::SourceTrackType::SampleNoise)
                ++noiseUsed;
        }
        drawLabelBox({ r.x + 14.0f, r.y + r.h - 86.0f, r.w - 28.0f, 22.0f },
                     buttonText("Meta %d/8   Partials %d/64", metaUsed, partialUsed));
        drawLabelBox({ r.x + 14.0f, r.y + r.h - 58.0f, r.w - 28.0f, 22.0f },
                     buttonText("Basic %d/8   Noise %d/4", basicUsed, noiseUsed));
        drawLabelBox({ r.x + 14.0f, r.y + r.h - 30.0f, r.w - 28.0f, 22.0f },
                     partialUsed > 64 || metaUsed > 8 ? "ENGINE BUDGET OVER" : "ENGINE BUDGET OK");
    }

synth::SourceTrackParams *KapibaraUI::currentTrack()
{
        if(generator_.tracks.empty())
            return nullptr;
        selectedTrack_ = clampi(selectedTrack_, 0, int(generator_.tracks.size()) - 1);
        return &generator_.tracks[(size_t)selectedTrack_];
    }

END_NAMESPACE_DISTRHO
