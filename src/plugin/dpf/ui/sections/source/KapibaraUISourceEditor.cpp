#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// 取得某 track 的 insert 链（存储在 SourceTrackParams 内）



    // Strip insert list: one chip per effect (no number), a trailing "+" to add. Drag to reorder.
void KapibaraUI::drawGroupEditor(const Rect &r, int gi)
{
        drawPanel(r, rgba(0x140d1aff), rgba(0xc070e0aaU));
        clearTrackEditorRects();
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        if(gi < 0 || gi >= int(stripGroups_.size()))
            return;
        auto &grp = stripGroups_[(size_t)gi];
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, grp.name.c_str());
        drawSectionTitle(r.x + 16.0f, r.y + 44.0f, "Members -> Bus");
        float yy = r.y + 70.0f;
        for(int mi : grp.memberIndices)
        {
            if(mi < 0 || mi >= int(generator_.tracks.size()))
                continue;
            const auto &t = generator_.tracks[(size_t)mi];
            std::snprintf(scratch_, sizeof(scratch_), "%s  ->  %s", t.name.c_str(), grp.name.c_str());
            drawLabelBox({ r.x + 16.0f, yy, r.w - 32.0f, 22.0f }, scratch_);
            yy += 26.0f;
        }
        drawLabelBox({ r.x + 16.0f, std::min(yy + 8.0f, r.y + r.h - 34.0f), r.w - 32.0f, 22.0f },
                     "merge only - strip FX lives in STRIP GRID");
    }

void KapibaraUI::drawTrackEditor(const Rect &r)
{
        // Group view takes over the editor when a group bus is selected.
        if(selectedGroupView_ >= 0 && selectedGroupView_ < int(stripGroups_.size()))
        {
            drawGroupEditor(r, selectedGroupView_);
            return;
        }
        drawPanel(r, rgba(0x0d151aff), rgba(0x3b5560ff));
        clearTrackEditorRects();
        auto *track = currentTrack();
        if(track == nullptr)
            return;
        drawSectionTitle(r.x + 16.0f, r.y + 14.0f, track->name.c_str());
        // Track type / ADSR-route / Duplicate live in the strip now, not here.
        trackOutputModeRect_ = {};  // output mode 选择从 UI 移除，默认 AudioAndMod
        track->outputMode = synth::SourceTrackOutputMode::AudioAndMod;
        track->ampEnvIndex = clampi(track->ampEnvIndex, 0, synth::kMaxAmpEnvs - 1);
        ampEnvSelectRect_ = {};
        duplicateEnvRect_ = {};

        // The FX chain moved to the dedicated top-right FX Rack editor; the source
        // editor is now a single oscillator + unison view (no SOURCE/SHAPE tabs).
        editorTab_ = 0;
        editorTabRects_.fill({});
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});

        // Oscillator body + compact unison row at the bottom.
        const Rect content { r.x + 16.0f, r.y + 50.0f, r.w - 32.0f, r.h - 62.0f };
        const float unisonH = 26.0f;
        const Rect srcContent { content.x, content.y, content.w, content.h - unisonH - 8.0f };
        if(track->type == synth::SourceTrackType::PartialBank)
            drawPartialBankTrackEditor(srcContent, *track);
        else if(track->type == synth::SourceTrackType::MetaOscillator)
            drawMetaTrackEditor(srcContent, *track);
        else if(track->type == synth::SourceTrackType::BasicOscillator)
            drawBasicTrackEditor(srcContent, *track);
        else
            drawNoiseTrackEditor(srcContent, *track);
        // Compact unison row
        const float uy = content.y + content.h - unisonH;
        const float kw = (content.w - 24.0f) * 0.25f;
        unisonVoicesRect_ = { content.x,                uy, kw, unisonH };
        unisonDetuneRect_ = { content.x + kw + 8.0f,   uy, kw, unisonH };
        unisonWidthRect_  = { content.x + (kw + 8.0f) * 2.0f, uy, kw, unisonH };
        unisonPhaseRect_  = { content.x + (kw + 8.0f) * 3.0f, uy, kw, unisonH };
        drawSlider(unisonVoicesRect_, "Voices",  float(track->unison.voices - 1) / 15.0f, float(track->unison.voices));
        drawSlider(unisonDetuneRect_, "Detune",  track->unison.detuneCents / 80.0f, track->unison.detuneCents);
        drawSlider(unisonWidthRect_,  "Width",   track->unison.widthStereo, track->unison.widthStereo);
        drawSlider(unisonPhaseRect_,  "Rnd Ph",  track->unison.phaseSpread, track->unison.phaseSpread);
    }

void KapibaraUI::drawVoiceTab(const Rect &r, synth::SourceTrackParams &track)
{
        drawSectionTitle(r.x, r.y, "Unison / Voicing");
        const float kw = 78.0f;
        const float gap = 22.0f;
        const float ky = r.y + 44.0f;
        unisonVoicesRect_ = { r.x,                    ky, kw, kw };
        unisonDetuneRect_ = { r.x + (kw + gap),       ky, kw, kw };
        unisonWidthRect_  = { r.x + (kw + gap) * 2.0f, ky, kw, kw };
        unisonPhaseRect_  = { r.x + (kw + gap) * 3.0f, ky, kw, kw };
        drawKnob(unisonVoicesRect_, "Voices", float(track.unison.voices - 1) / 15.0f, float(track.unison.voices));
        drawKnob(unisonDetuneRect_, "Detune", track.unison.detuneCents / 80.0f, track.unison.detuneCents);
        drawKnob(unisonWidthRect_,  "Width",  track.unison.widthStereo, track.unison.widthStereo);
        drawKnob(unisonPhaseRect_,  "Rnd Ph", track.unison.phaseSpread, track.unison.phaseSpread);
    }

void KapibaraUI::clearTrackEditorRects()
{
        partialCountRect_ = {};
        inharmonicModeRect_ = {};
        inharmonicRect_ = {};
        partialSpectrumRect_ = {};
        partialAmpRect_ = {};
        partialRatioRect_ = {};
        metaEnableRect_ = {};
        metaWavetableNameRect_ = {};
        metaWavetablePrevRect_ = {};
        metaWavetableNextRect_ = {};
        metaWarpModeRect_ = {};
        metaFrameButtonRect_ = {};
        metaHarmonicEditRect_ = {};
        metaLoadRect_ = {};
        metaLoadPathRect_ = {};
        metaFrameStripRect_ = {};
        for(auto &r : metaFramePresetRects_)
            r = {};
        for(auto &r : metaFrameRects_)
            r = {};
        metaOctRect_ = {};
        metaSemRect_ = {};
        metaFinRect_ = {};
        metaCrsRect_ = {};
        metaRatioRect_ = {};
        metaAmpRect_ = {};
        metaPhaseRect_ = {};
        metaPhaseRandRect_ = {};
        metaPanRect_ = {};
        metaFrameCountRect_ = {};
        metaMorphRect_ = {};
        metaWarpAmountRect_ = {};
        metaWaveformRect_ = {};
        metaHarmonicRatioRect_ = {};
        metaHarmonicAmpRect_ = {};
        metaHarmonicPhaseRect_ = {};
        basicShapeRect_ = {};
        basicPulseRect_ = {};
        basicSubRect_ = {};
        noiseModeRect_ = {};
        noiseColorRect_ = {};
        attackRect_ = {};
        decayRect_ = {};
        sustainRect_ = {};
        releaseRect_ = {};
        curveRect_ = {};
        ampEnvSelectRect_ = {};
        duplicateEnvRect_ = {};
        unisonVoicesRect_ = {};
        unisonDetuneRect_ = {};
        unisonWidthRect_ = {};
        unisonPhaseRect_ = {};
    }

std::vector<InsertEffect> *KapibaraUI::trackInsertsFor(uint32_t trackId)
{
        for(auto &t : generator_.tracks)
            if(t.id == trackId)
                return &t.inserts;
        return nullptr;
    }

bool KapibaraUI::trackHasRoutedFx(const synth::SourceTrackParams &t)
{ return !t.inserts.empty(); }

const char *KapibaraUI::insertKindShort(uint8_t kind)
{
        switch(kind){case InsertFilter:return "F";case InsertDist:return "D";case InsertEq:return "EQ";
                     case InsertComp:return "CP";case InsertDelay:return "DL";case InsertReverb:return "RV";}
        return "?";
    }

END_NAMESPACE_DISTRHO
