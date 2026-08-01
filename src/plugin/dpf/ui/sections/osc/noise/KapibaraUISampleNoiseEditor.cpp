#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

const char *KapibaraUI::sampleLoopModeName(synth::SampleLoopMode m)
{
        switch(m)
        {
            case synth::SampleLoopMode::Forward:  return "LOOP: FWD";
            case synth::SampleLoopMode::PingPong: return "LOOP: P-PONG";
            case synth::SampleLoopMode::Off:      break;
        }
        return "LOOP: OFF";
    }

void KapibaraUI::openSamplerFileBrowser()
{
        samplerLoadPending_ = true;
#if DISTRHO_UI_FILE_BROWSER
        FileBrowserOptions options;
        options.title = "Load sample WAV";
        if(!lastLoadPath_.empty())
        {
            const size_t slash = lastLoadPath_.find_last_of("/\\");
            browserStartDir_ = slash == std::string::npos ? std::string {} : lastLoadPath_.substr(0, slash);
            if(!browserStartDir_.empty())
                options.startDir = browserStartDir_.c_str();
        }
        if(openFileBrowser(options))
            return;
#endif
        samplerLoadPending_ = false;
        metaEditorStatus_ = "file browser unavailable";
    }

bool KapibaraUI::loadSampleIntoTrack(synth::SourceTrackParams &track, const std::string &path)
{
        std::vector<float> left, right;
        uint32_t sr = 48000;
        if(!synth::loadSampleFile(path, left, right, sr) || left.empty())
        {
            metaEditorStatus_ = "sample load failed";
            return false;
        }
        auto data = std::make_shared<synth::SampleData>();
        data->left = std::move(left);
        data->right = std::move(right);
        data->sampleRate = sr > 0 ? sr : 48000;
        const size_t slash = path.find_last_of("/\\");
        data->name = slash == std::string::npos ? path : path.substr(slash + 1);

        auto &sp = track.sampler;
        sp.sampleName = data->name;
        sp.samplePath = path;
        sp.sample = data;
        // A freshly loaded file plays whole; the previous file's window would
        // otherwise silently crop it.
        sp.startNorm = 0.0f;
        sp.endNorm = 1.0f;
        sp.loopStartNorm = 0.0f;
        sp.loopEndNorm = 1.0f;
        track.sampleNoiseMode = synth::SampleNoiseMode::File;
        lastLoadPath_ = path;
        metaEditorStatus_ = std::string("sample loaded: ") + data->name
                            + (data->stereo() ? " (stereo)" : " (mono)");
        return true;
    }

void KapibaraUI::drawNoiseTrackEditor(const Rect &r, synth::SourceTrackParams &track)
{
        samplerLoadRect_ = {}; samplerRootRect_ = {}; samplerKeyTrackRect_ = {};
        samplerLoopRect_ = {}; samplerSliceRect_ = {}; samplerRevRect_ = {};
        samplerWaveRect_ = {}; samplerStartRect_ = {}; samplerEndRect_ = {};
        samplerLoopStartRect_ = {}; samplerLoopEndRect_ = {}; samplerGainRect_ = {};
        noiseTypeRect_ = {};

        auto &sp = track.sampler;
        constexpr float rowH = 22.0f;
        float y = r.y;

        const bool sampling = track.sampleNoiseMode != synth::SampleNoiseMode::Noise;

        // Row 1: mode, and — only when sampling — the file.
        noiseModeRect_ = { r.x, y, 96.0f, rowH };
        drawButton(noiseModeRect_, synth::sampleNoiseModeName(track.sampleNoiseMode), sampling);
        if(sampling)
        {
            samplerLoadRect_ = { r.x + 102.0f, y, 74.0f, rowH };
            drawButton(samplerLoadRect_, "LOAD", false);
            useUiFont();
            uiFontSize(8.5f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(sp.sample ? DesignTokens::textPrimary() : DesignTokens::textSecondary());
            const float tx = r.x + 182.0f;
            scissor(tx, y, std::max(0.0f, r.x + r.w - tx), rowH);
            text(tx, y + rowH * 0.5f,
                 sp.sample ? sp.sampleName.c_str() : "(no sample - LOAD a WAV)", nullptr);
            resetScissor();
        }
        y += rowH + 6.0f;

        if(!sampling)
        {
            // Noise has no file and no waveform worth drawing — a noise plot is
            // the same fuzz whatever the settings are. Type and colour is all it
            // needs, so they get the room instead.
            noiseTypeRect_ = { r.x, y, 120.0f, rowH };
            drawButton(noiseTypeRect_, synth::noiseTypeName(track.noiseType), true);
            noiseColorRect_ = { r.x + 128.0f, y, std::max(120.0f, r.w - 128.0f), rowH };
            drawSlider(noiseColorRect_, "Tone", track.noiseColor, track.noiseColor);
            y += rowH + 10.0f;
            useUiFont();
            uiFontSize(8.0f);
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary().withAlpha(0.65f));
            text(r.x, y, "Tone tilts the chosen noise: dark at 0, flat at 0.5, bright at 1", nullptr);
            oscBodyBottomY_ = y + 16.0f;
            return;
        }
        noiseColorRect_ = {};
        noiseTypeRect_ = {};

        // Row 2: how a note maps onto the file.
        const float bw = std::min(104.0f, (r.w - 24.0f) * 0.2f);
        samplerRootRect_ = { r.x, y, bw, rowH };
        samplerKeyTrackRect_ = { r.x + (bw + 6.0f), y, bw, rowH };
        samplerLoopRect_ = { r.x + (bw + 6.0f) * 2.0f, y, bw, rowH };
        samplerSliceRect_ = { r.x + (bw + 6.0f) * 3.0f, y, bw, rowH };
        samplerRevRect_ = { r.x + (bw + 6.0f) * 4.0f, y, bw, rowH };
        static const char *kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                              "F#", "G", "G#", "A", "A#", "B" };
        const int rn = clampi(sp.rootNote, 0, 127);
        std::snprintf(scratch_, sizeof scratch_, "ROOT %s%d", kNoteNames[rn % 12], rn / 12 - 1);
        drawButton(samplerRootRect_, scratch_, false);
        drawButton(samplerKeyTrackRect_, sp.keyTrack ? "KEY TRK" : "FIXED", sp.keyTrack);
        drawButton(samplerLoopRect_, sampleLoopModeName(sp.loopMode),
                   sp.loopMode != synth::SampleLoopMode::Off);
        drawButton(samplerSliceRect_, buttonText("SLICES %d", clampi(sp.sliceCount, 1, 64)),
                   sp.sliceCount > 1);
        drawButton(samplerRevRect_, "REV", sp.reverse);
        y += rowH + 6.0f;

        // The same pitch module every other source has, on top of the root-note
        // transposition. Shares the meta pitch rects — one editor is drawn at a
        // time and the handlers resolve the target by track type.
        {
            constexpr float pitchH = 24.0f;
            const float pgap = 6.0f;
            const float pw = std::min(96.0f, std::max(56.0f, (r.w - pgap * 3.0f) * 0.25f));
            metaOctRect_ = { r.x, y, pw, pitchH };
            metaSemRect_ = { r.x + pw + pgap, y, pw, pitchH };
            metaFinRect_ = { r.x + (pw + pgap) * 2.0f, y, pw, pitchH };
            metaCrsRect_ = { r.x + (pw + pgap) * 3.0f, y, pw, pitchH };
            drawPitchControl(metaOctRect_, "OCT", sp.pitchOct, false);
            drawPitchControl(metaSemRect_, "SEM", sp.pitchSem, false);
            drawPitchControl(metaFinRect_, "FIN", int(std::round(sp.pitchFin)), false);
            drawPitchControl(metaCrsRect_, "CRS", int(std::round(sp.pitchCrs)), false);
            y += pitchH + 8.0f;
        }

        // Waveform with the playback window, slice divisions and loop markers.
        const float sliders = rowH * 2.0f + 6.0f + 8.0f;
        const float waveH = (r.y + r.h) - y - sliders;
        if(waveH >= 48.0f)
        {
            samplerWaveRect_ = { r.x, y, r.w, waveH };
            const Rect &w = samplerWaveRect_;
            drawPlotBackground(w, 8, 4);
            scissor(w.x + 2.0f, w.y + 2.0f, w.w - 4.0f, w.h - 4.0f);
            const float mid = w.y + w.h * 0.5f;
            if(sp.sample && sp.sample->frames() > 1)
            {
                const auto &sd = *sp.sample;
                const int n = sd.frames();
                const int cols = clampi(int(w.w), 64, 1024);
                // Min/max envelope per column. Point-sampling a long file would
                // draw its aliasing, not its shape.
                for(int c = 0; c < cols; ++c)
                {
                    const int a = int(int64_t(c) * n / cols);
                    const int b = std::max(a + 1, int(int64_t(c + 1) * n / cols));
                    float lo = 1.0f, hi = -1.0f;
                    for(int i = a; i < b && i < n; ++i)
                    {
                        lo = std::min(lo, sd.left[(size_t)i]);
                        hi = std::max(hi, sd.left[(size_t)i]);
                    }
                    if(hi < lo) continue;
                    const float px = w.x + (float(c) + 0.5f) * w.w / float(cols);
                    strokeLine(px, mid - hi * w.h * 0.45f, px, mid - lo * w.h * 0.45f,
                               DesignTokens::accentCyan().withAlpha(0.75f), 1.0f);
                }
                // Playback window: what falls outside it is dimmed.
                const float a = clampf(std::min(sp.startNorm, sp.endNorm), 0.0f, 1.0f);
                const float b = clampf(std::max(sp.startNorm, sp.endNorm), 0.0f, 1.0f);
                beginPath();
                rect(w.x, w.y, a * w.w, w.h);
                rect(w.x + b * w.w, w.y, (1.0f - b) * w.w, w.h);
                fillColor(DesignTokens::appBackground().withAlpha(0.6f));
                fill();
                strokeLine(w.x + a * w.w, w.y, w.x + a * w.w, w.y + w.h,
                           DesignTokens::accentGreen(), 1.4f);
                strokeLine(w.x + b * w.w, w.y, w.x + b * w.w, w.y + w.h,
                           DesignTokens::accentGreen(), 1.4f);
                // Slice divisions inside the window — the note picks one of these.
                const int slices = clampi(sp.sliceCount, 1, 64);
                for(int i = 1; i < slices; ++i)
                {
                    const float sx = w.x + (a + (b - a) * float(i) / float(slices)) * w.w;
                    strokeLine(sx, w.y, sx, w.y + w.h,
                               DesignTokens::divider().withAlpha(0.8f), 1.0f);
                }
                if(sp.loopMode != synth::SampleLoopMode::Off)
                {
                    const float la = a + (b - a) * clampf(std::min(sp.loopStartNorm, sp.loopEndNorm), 0.0f, 1.0f);
                    const float lb = a + (b - a) * clampf(std::max(sp.loopStartNorm, sp.loopEndNorm), 0.0f, 1.0f);
                    beginPath();
                    rect(w.x + la * w.w, w.y, (lb - la) * w.w, w.h);
                    fillColor(DesignTokens::accentGreen().withAlpha(0.10f));
                    fill();
                }
            }
            else
            {
                useUiFont();
                uiFontSize(9.0f);
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                fillColor(DesignTokens::textSecondary().withAlpha(0.7f));
                text(w.x + w.w * 0.5f, mid, "LOAD a WAV to play it back", nullptr);
            }
            resetScissor();
            y += waveH + 8.0f;
        }

        const float sw = (r.w - 12.0f) * 0.5f;
        samplerStartRect_ = { r.x, y, sw, rowH };
        samplerEndRect_ = { r.x + sw + 12.0f, y, sw, rowH };
        drawSlider(samplerStartRect_, "Start", sp.startNorm, sp.startNorm);
        drawSlider(samplerEndRect_, "End", sp.endNorm, sp.endNorm);
        y += rowH + 6.0f;
        const float tw = (r.w - 24.0f) / 3.0f;
        samplerLoopStartRect_ = { r.x, y, tw, rowH };
        samplerLoopEndRect_ = { r.x + tw + 12.0f, y, tw, rowH };
        samplerGainRect_ = { r.x + (tw + 12.0f) * 2.0f, y, tw, rowH };
        drawSlider(samplerLoopStartRect_, "Loop A", sp.loopStartNorm, sp.loopStartNorm);
        drawSlider(samplerLoopEndRect_, "Loop B", sp.loopEndNorm, sp.loopEndNorm);
        drawSlider(samplerGainRect_, "Gain", sp.gain * 0.5f, sp.gain);
    }

END_NAMESPACE_DISTRHO
