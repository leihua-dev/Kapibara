#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// 取得某 track 的 insert 链（存储在 SourceTrackParams 内）



    // Strip insert list: one chip per effect (no number), a trailing "+" to add. Drag to reorder.
void KapibaraUI::drawGroupEditor(const Rect &r, int gi)
{
        drawPanel(r, rgba(0x140d1aff), rgba(0xc070e0aaU));
        clearTrackEditorRects();
        modSrcRects_.fill({}); modTypeRects_.fill({}); modDepthRects_.fill({}); modDeleteRects_.fill({});
        oscModAddRect_ = {};
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
        // Section title (accent bar + name) matching the PER-VOICE CHAIN / FX RACK
        // panels to the right.
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

        // Oscillator body + a dedicated UNISON zone + the OSC MOD zone, separated by
        // dividers (not boxed cards). The editor may report its real body bottom via
        // oscBodyBottomY_ (e.g. the meta editor ends level with the WAVETABLE panel /
        // PAN row) so the UNISON row tucks right under it, with OSC MOD below that.
        const Rect content { r.x + 16.0f, r.y + 44.0f, r.w - 32.0f, r.h - 56.0f };
        // A Basic Oscillator rack is already three oscillators: stacking unison
        // lanes on top of that is redundant voicing, so the zone is not shown and
        // its rects stay zeroed (no stale click targets). The engine is told to
        // render one lane per partial in pushCurrentTrack.
        const bool showUnison = track->type != synth::SourceTrackType::BasicOscillator;
        const float unisonH = showUnison ? 52.0f : 0.0f;  // label + control row
        // OSC MOD zone grows with the number of active source-mod entries.
        int activeMods = 0;
        for(const auto &m : track->mods)
            if(modEntryActive(m)) ++activeMods;
        const float modZoneH = 18.0f + float(activeMods) * 24.0f;
        const Rect srcContent { content.x, content.y, content.w,
                                content.h - unisonH - modZoneH - 16.0f };
        oscBodyBottomY_ = content.y + srcContent.h;  // default: UNISON at the panel bottom
        oscBodyLeftW_ = content.w;                    // default: UNISON spans the full width
        if(track->type == synth::SourceTrackType::PartialBank)
            drawPartialBankTrackEditor(srcContent, *track);
        else if(track->type == synth::SourceTrackType::MetaOscillator)
            drawMetaTrackEditor(srcContent, *track);
        else if(track->type == synth::SourceTrackType::BasicOscillator)
            drawBasicTrackEditor(srcContent, *track);
        else
            drawNoiseTrackEditor(srcContent, *track);

        // UNISON zone directly below the oscillator body. Clamp so it always fits,
        // leaving room for the OSC MOD zone beneath.
        const float uy = clampf(oscBodyBottomY_ + 10.0f, content.y,
                                content.y + content.h - unisonH - modZoneH - 6.0f);
        const float unisonW = oscBodyLeftW_ > 0.0f ? oscBodyLeftW_ : content.w;
        if(!showUnison)
        {
            unisonVoicesRect_ = {}; unisonDetuneRect_ = {};
            unisonWidthRect_ = {};  unisonPhaseRect_ = {};
            unisonVoicesDownRect_ = {}; unisonVoicesUpRect_ = {};
            const float modOnlyY = uy + 6.0f;
            drawModEditor({ content.x, modOnlyY, unisonW, modZoneH }, *track);
            return;
        }
        strokeLine(content.x, uy - 2.0f, content.x + unisonW, uy - 2.0f,
                   DesignTokens::divider(), 1.0f);
        drawGroupLabel(content.x, uy + 2.0f, "UNISON");
        const float rowY = uy + 16.0f;
        const float rowH = unisonH - 18.0f;
        const float cellGap = 10.0f;
        const float cellW = (unisonW - cellGap * 3.0f) * 0.25f;
        unisonVoicesRect_ = { content.x,                       rowY, cellW, rowH };
        unisonDetuneRect_ = { content.x + (cellW + cellGap),   rowY, cellW, rowH };
        unisonWidthRect_  = { content.x + (cellW + cellGap) * 2.0f, rowY, cellW, rowH };
        unisonPhaseRect_  = { content.x + (cellW + cellGap) * 3.0f, rowY, cellW, rowH };
        // Voices: drag up/down to change the count (no +/- buttons).
        unisonVoicesDownRect_ = {};
        unisonVoicesUpRect_ = {};
        {
            useUiFont();
            uiFontSize(11.5f);
            textAlign(ALIGN_LEFT | ALIGN_TOP);
            fillColor(DesignTokens::textSecondary());
            text(unisonVoicesRect_.x, unisonVoicesRect_.y, "VOICES", nullptr);
            useMonoFont();
            uiFontSize(17.0f);
            textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
            fillColor(DesignTokens::textPrimary());
            char vb[8];
            std::snprintf(vb, sizeof(vb), "%d", track->unison.voices);
            text(unisonVoicesRect_.x, unisonVoicesRect_.y + unisonVoicesRect_.h, vb, nullptr);
        }
        char ub[24];
        std::snprintf(ub, sizeof(ub), "%.1f ct", track->unison.detuneCents);
        drawKnobLabeled(unisonDetuneRect_, "DETUNE", track->unison.detuneCents / 80.0f, ub);
        std::snprintf(ub, sizeof(ub), "%.2f", track->unison.widthStereo);
        drawKnobLabeled(unisonWidthRect_, "WIDTH", track->unison.widthStereo, ub);
        std::snprintf(ub, sizeof(ub), "%.2f", track->unison.phaseSpread);
        drawKnobLabeled(unisonPhaseRect_, "RND PH", track->unison.phaseSpread, ub);

        // OSC MOD zone: this track as a carrier, other tracks as audio-rate
        // modulators (AM / Ring / FM / PM / Sync), rendered per voice in Voice.
        const float modY = uy + unisonH + 6.0f;
        drawModEditor({ content.x, modY, unisonW, modZoneH }, *track);
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
        basicWaveRects_.fill({});
        basicModModeRect_ = {}; basicModSrcRect_ = {};
        basicModDstRect_ = {};  basicModDepthRect_ = {};
        basicUnitEnableRects_.fill({});
        basicShapeRects_.fill({});
        basicLevelRects_.fill({});
        basicPulseRects_.fill({});
        basicSubRects_.fill({});
        for(auto &row : basicPitchRects_)
            row.fill({});
        noiseModeRect_ = {};
        noiseTypeRect_ = {};
        samplerLoadRect_ = {}; samplerRootRect_ = {}; samplerKeyTrackRect_ = {};
        samplerLoopRect_ = {}; samplerSliceRect_ = {}; samplerRevRect_ = {};
        samplerWaveRect_ = {}; samplerStartRect_ = {}; samplerEndRect_ = {};
        samplerLoopStartRect_ = {}; samplerLoopEndRect_ = {}; samplerGainRect_ = {};
        noiseColorRect_ = {};
        attackRect_ = {};
        decayRect_ = {};
        sustainRect_ = {};
        releaseRect_ = {};
        curveRect_ = {};
        ampEnvSelectRect_ = {};
        duplicateEnvRect_ = {};
        unisonVoicesRect_ = {};
        unisonVoicesDownRect_ = {};
        unisonVoicesUpRect_ = {};
        unisonDetuneRect_ = {};
        unisonWidthRect_ = {};
        unisonPhaseRect_ = {};
        metaMorphSliderRect_ = {};
        oscModAddRect_ = {};
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
