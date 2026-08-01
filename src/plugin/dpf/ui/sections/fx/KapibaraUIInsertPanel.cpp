#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// Magnitude (linear) of a biquad cascade at digital frequency w.



    // Source-modulation editor inside the OSC editor (rows of enable/source/type/depth).
    // Does routing track[srcIdx] → track[targetIdx] create a modulation cycle?
void KapibaraUI::drawRouteFxEditor(const Rect &region, std::vector<InsertEffect> *chain,
                       int chainTrackId, int chainGroup)
{
        routeFxChainTrackId_ = chainTrackId;
        routeFxChainMerge_ = chainGroup;
        fxKnobHits_.clear(); fxBypassHits_.clear(); fxDeleteHits_.clear(); fxModeHits_.clear();
        drawSectionTitle(region.x, region.y, "ROUTE FX (signal flows left -> right)");
        if(chain == nullptr)
            return;
        if(chain->empty())
        {
            drawLabelBox({ region.x, region.y + 24.0f, region.w, 24.0f },
                         "No FX. Add via a strip ROUTE  + chip.");
            return;
        }
        const float panelW = 150.0f, panelH = std::max(96.0f, region.h - 28.0f);
        const float arrowW = 16.0f;
        const float top = region.y + 24.0f;
        float x = region.x;
        const int n = int(chain->size());
        for(int i = 0; i < n; ++i)
        {
            const Rect p { x, top, panelW, panelH };
            drawInsertPanel(p, (*chain)[(size_t)i], chainTrackId, chainGroup, i);
            x += panelW;
            if(i + 1 < n)
            {
                // arrow to next
                const float ay = top + panelH * 0.5f;
                strokeLine(x + 2.0f, ay, x + arrowW - 2.0f, ay, rgba(0x8aa0b0ff), 2.0f);
                beginPath();
                moveTo(x + arrowW - 2.0f, ay); lineTo(x + arrowW - 7.0f, ay - 4.0f); lineTo(x + arrowW - 7.0f, ay + 4.0f);
                closePath(); fillColor(rgba(0x8aa0b0ff)); fill();
                x += arrowW;
            }
            if(x > region.x + region.w - panelW) break; // clip overflow (no horizontal scroll yet)
        }
    }

void KapibaraUI::drawInsertPanel(const Rect &p, InsertEffect &e, int trackId, int mergeIdx, int insertIdx)
{
        const bool byp = e.bypass;
        // Only the focused panel below can own these; a rack panel drawn after
        // it must not leave live rects behind.
        clearDisperserRects();
        drawPanel(p, rgba(byp ? 0x10141aff : 0x10171bff), rgba(0x3b5560ff));
        // header
        fontSize(8.5f); fillColor(rgba(byp ? 0x6a7884ff : 0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(p.x + 5.0f, p.y + 3.0f, insertTypeName(e.kind), nullptr);
        const Rect bypR { p.x + p.w - 38.0f, p.y + 2.0f, 18.0f, 14.0f };
        const Rect delR { p.x + p.w - 18.0f, p.y + 2.0f, 16.0f, 14.0f };
        drawButton(bypR, "b", !byp);
        fxBypassHits_.push_back(FxBtnHit { bypR, trackId, mergeIdx, insertIdx });
        if(!fxPanelHideDelete_)
        {
            drawButton(delR, "x", false);
            fxDeleteHits_.push_back(FxBtnHit { delR, trackId, mergeIdx, insertIdx });
        }
        float knobsY = p.y + 20.0f;
        if(fxHasMode(e.kind))
        {
            const Rect modeR { p.x + 5.0f, p.y + 18.0f, p.w - 10.0f, 16.0f };
            const char *algo = e.kind == InsertConvReverb
                                   ? (e.conv.irName.empty() ? "Load IR..." : e.conv.irName.c_str())
                                   : fxModeName(e.kind, fxCurrentMode(e, e.kind));
            drawButton(modeR, algo, false);
            fxModeHits_.push_back(FxBtnHit { modeR, trackId, mergeIdx, insertIdx });
            knobsY = p.y + 38.0f;
        }
        const float kw = (p.w - 12.0f) * 0.25f;
        const float knobH = 44.0f;
        for(int i = 0; i < 4; ++i)
        {
            const Rect kr { p.x + 4.0f + float(i) * (kw + 1.0f), knobsY, kw, knobH };
            drawKnob(kr, fxKnobName(e.kind, i), fxKnobNorm(e, i), fxKnobDisp(e, i));
            fxKnobHits_.push_back(FxKnobHit { kr, trackId, mergeIdx, insertIdx, i });
        }
        // Disperser row: the allpass algos have four more controls, and only
        // they do — drawing them for a lowpass would be four dead knobs.
        float extraH = 0.0f;
        if(e.kind == InsertFilter && synth::isAllpassAlgo(e.filter.algo))
        {
            const float row2Y = knobsY + knobH + 4.0f;
            for(int i = 4; i < 8; ++i)
            {
                const Rect kr { p.x + 4.0f + float(i - 4) * (kw + 1.0f), row2Y, kw, knobH };
                drawKnob(kr, fxKnobName(e.kind, i), fxKnobNorm(e, i), fxKnobDisp(e, i));
                fxKnobHits_.push_back(FxKnobHit { kr, trackId, mergeIdx, insertIdx, i });
            }
            extraH = knobH + 4.0f;
        }

        // Response/transfer graph below the knobs (filter / eq / dist / comp).
        const float graphTop = knobsY + knobH + 6.0f + extraH;
        const float graphBot = p.y + p.h - 5.0f;
        // Focused allpass: the whole pane is free, so the stage distribution
        // gets drawn as an editable lane instead of a thumbnail response.
        if(fxPanelFocused_ && e.kind == InsertFilter && synth::isAllpassAlgo(e.filter.algo)
           && graphBot - graphTop > 150.0f)
        {
            drawDisperserEditor({ p.x + 5.0f, graphTop + 8.0f, p.w - 10.0f, graphBot - graphTop - 8.0f },
                                e, trackId, mergeIdx, insertIdx);
            return;
        }
        if(fxHasGraph(e.kind) && graphBot - graphTop > 22.0f)
            drawInsertGraph({ p.x + 5.0f, graphTop, p.w - 10.0f, graphBot - graphTop }, e);
    }

bool KapibaraUI::fxHasGraph(int kind)
{
        return kind == InsertFilter || kind == InsertEq || kind == InsertDist || kind == InsertComp;
    }

float KapibaraUI::biquadMagnitude(const synth::BiquadCoeffs &c, float w)
{
        const float cw = std::cos(w), sw = std::sin(w);
        const float c2 = std::cos(2.0f * w), s2 = std::sin(2.0f * w);
        const float nRe = c.b0 + c.b1 * cw + c.b2 * c2;
        const float nIm = -(c.b1 * sw + c.b2 * s2);
        const float dRe = 1.0f + c.a1 * cw + c.a2 * c2;
        const float dIm = -(c.a1 * sw + c.a2 * s2);
        const float den = std::sqrt(dRe * dRe + dIm * dIm);
        const float num = std::sqrt(nRe * nRe + nIm * nIm);
        const float m = den > 1e-9f ? num / den : 0.0f;
        return std::pow(m, float(std::max(1, c.stages)));
    }

float KapibaraUI::biquadPhase(const synth::BiquadCoeffs &c, float w)
{
        const float cw = std::cos(w), sw = std::sin(w);
        const float c2 = std::cos(2.0f * w), s2 = std::sin(2.0f * w);
        const float nRe = c.b0 + c.b1 * cw + c.b2 * c2;
        const float nIm = -(c.b1 * sw + c.b2 * s2);
        const float dRe = 1.0f + c.a1 * cw + c.a2 * c2;
        const float dIm = -(c.a1 * sw + c.a2 * s2);
        const float ph = std::atan2(nIm, nRe) - std::atan2(dIm, dRe);
        return ph * float(std::max(1, c.stages));
    }

synth::BiquadCoeffs KapibaraUI::sourceFilterBiquad(const synth::SourceFilterParams &, double)
{
        return {}; // (kept for the declaration; the response now uses the real recurrence)
    }

namespace
{
bool sameSourceFilterShape(const synth::SourceFilterParams &a, const synth::SourceFilterParams &b)
{
        return a.topology == b.topology && a.enabled == b.enabled
               && std::abs(a.cutoffHz - b.cutoffHz) < 0.01f
               && std::abs(a.resonance - b.resonance) < 1e-4f
               && std::abs(a.feedback - b.feedback) < 1e-4f;
}
}

// Sample-accurate response: run the *actual* per-topology recurrence (the same
// math as Voice::processSourceFilterParams, linearised — no drive/tanh) as an
// impulse response, then DFT it. Cached so it only recomputes on a param change.
void KapibaraUI::drawSourceFilterGraph(const Rect &g, const synth::SourceFilterParams &f, bool phase)
{
        if(!pvGraphCacheValid_ || !sameSourceFilterShape(f, pvGraphCacheParams_))
        {
            const float sr = 48000.0f;
            const float cutoff = clampf(f.cutoffHz, 20.0f, sr * 0.45f);
            const float gc = clampf(1.0f - std::exp(-2.0f * kPi * cutoff / sr), 0.001f, 0.98f);
            const float res = clampf(f.resonance, 0.0f, 0.95f);
            const float fb = clampf(f.feedback, 0.0f, 0.95f);
            const bool bypass = (!f.enabled || f.topology == synth::SourceFilterTopology::Bypass);

            constexpr int N = 512;
            std::array<float, N> h {};
            float lp1 = 0, lp2 = 0, lp3 = 0, lp4 = 0, bp = 0;
            for(int n = 0; n < N; ++n)
            {
                const float x = (n == 0) ? 1.0f : 0.0f;
                float y = x;
                if(!bypass) switch(f.topology)
                {
                    case synth::SourceFilterTopology::OnePoleLowPass:
                        lp1 += gc * (x - lp1); y = lp1; break;
                    case synth::SourceFilterTopology::TwoPoleStateVariable:
                    { const float hp = x - lp1 - res * bp; bp += gc * hp; lp1 += gc * bp; y = lp1; break; }
                    case synth::SourceFilterTopology::FourPoleCascade:
                    { const float in = x - res * lp4; lp1 += gc * (in - lp1); lp2 += gc * (lp1 - lp2);
                      lp3 += gc * (lp2 - lp3); lp4 += gc * (lp3 - lp4); y = lp4; break; }
                    case synth::SourceFilterTopology::FeedbackLadder:
                    { const float in = x - (res + fb) * lp4; lp1 += gc * (in - lp1); lp2 += gc * (lp1 - lp2);
                      lp3 += gc * (lp2 - lp3); lp4 += gc * (lp3 - lp4); y = lp4; break; }
                    default: y = x; break;
                }
                h[(size_t)n] = y;
            }
            const float logLo = std::log10(20.0f), logHi = std::log10(20000.0f);
            const int M = int(pvGraphMag_.size());
            for(int i = 0; i < M; ++i)
            {
                const float t = float(i) / float(M - 1);
                const float fr = std::pow(10.0f, logLo + t * (logHi - logLo));
                const float w = 2.0f * kPi * fr / sr;
                // Goertzel-style oscillator recurrence for e^{-j w n} → 2 trig per freq.
                const float cw = std::cos(w), sw = std::sin(w);
                float cn = 1.0f, sn = 0.0f, re = 0.0f, im = 0.0f;
                for(int n = 0; n < N; ++n)
                {
                    re += h[(size_t)n] * cn;
                    im -= h[(size_t)n] * sn; // e^{-jwn} = cos - j sin
                    const float c2 = cn * cw - sn * sw;
                    sn = sn * cw + cn * sw;
                    cn = c2;
                }
                const float mag = std::sqrt(re * re + im * im);
                const float db = 20.0f * std::log10(std::max(1e-4f, mag));
                pvGraphMag_[(size_t)i] = clampf((db + 24.0f) / 48.0f, 0.0f, 1.0f);
                const float p = std::atan2(im, re);
                pvGraphPhase_[(size_t)i] = clampf(0.5f - p / (2.0f * kPi), 0.0f, 1.0f);
            }
            pvGraphCacheParams_ = f;
            pvGraphCacheValid_ = true;
        }

        beginPath();
        roundedRect(g.x, g.y, g.w, g.h, 3.0f);
        fillColor(rgba(0x0a0f13ff));
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();
        scissor(g.x + 1.0f, g.y + 1.0f, g.w - 2.0f, g.h - 2.0f);
        const float midY = g.y + g.h * 0.5f;
        strokeLine(g.x + 1.0f, midY, g.x + g.w - 1.0f, midY, DesignTokens::divider(), 1.0f);
        useUiFont(); uiFontSize(6.5f); fillColor(DesignTokens::textSecondary());
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(g.x + 3.0f, g.y + 2.0f, phase ? "phase" : "mag", nullptr);

        const auto &curve = phase ? pvGraphPhase_ : pvGraphMag_;
        const int M = int(curve.size());
        const int steps = std::max(8, int(g.w));
        beginPath();
        for(int i = 0; i <= steps; ++i)
        {
            const float t = float(i) / float(steps);
            const int idx = clampi(int(t * float(M - 1) + 0.5f), 0, M - 1);
            const float py = g.y + g.h - curve[(size_t)idx] * g.h;
            const float px = g.x + t * g.w;
            if(i == 0) moveTo(px, py); else lineTo(px, py);
        }
        strokeColor(phase ? DesignTokens::accentGreen() : DesignTokens::accentCyan());
        strokeWidth(1.4f);
        stroke();
        resetScissor();
    }

void KapibaraUI::drawInsertGraph(const Rect &g, const InsertEffect &e)
{
        // Backing panel + center line.
        beginPath();
        roundedRect(g.x, g.y, g.w, g.h, 3.0f);
        fillColor(rgba(0x0a0f13ff));
        fill();
        strokeColor(DesignTokens::divider());
        strokeWidth(1.0f);
        stroke();
        scissor(g.x + 1.0f, g.y + 1.0f, g.w - 2.0f, g.h - 2.0f);
        const float midY = g.y + g.h * 0.5f;
        strokeLine(g.x + 1.0f, midY, g.x + g.w - 1.0f, midY, DesignTokens::divider(), 1.0f);

        const Color line = DesignTokens::accentCyan();
        const int steps = std::max(8, int(g.w));
        const double sr = 48000.0;

        if(e.kind == InsertFilter && synth::isAllpassAlgo(e.filter.algo))
        {
            // An allpass is flat by construction — drawing its magnitude would
            // be a straight line saying nothing. Group delay is the dispersion.
            resetScissor();
            drawDisperserStageGraph(g, e.filter, -1);
            return;
        }
        if(e.kind == InsertFilter || e.kind == InsertEq)
        {
            // Log-frequency magnitude response, +/-24 dB window.
            synth::BiquadCoeffs eqc[3];
            int nb = 1;
            synth::BiquadCoeffs single;
            if(e.kind == InsertFilter) { single = synth::designInsertBiquad(e.filter, sr); }
            else { synth::designEqBiquads(e.eq, sr, eqc); nb = 3; }
            const float fLo = 20.0f, fHi = 20000.0f;
            const float logLo = std::log10(fLo), logHi = std::log10(fHi);
            beginPath();
            for(int i = 0; i <= steps; ++i)
            {
                const float t = float(i) / float(steps);
                const float f = std::pow(10.0f, logLo + t * (logHi - logLo));
                const float w = 2.0f * kPi * f / float(sr);
                float mag = 1.0f;
                if(e.kind == InsertFilter) mag = biquadMagnitude(single, w);
                else for(int b = 0; b < nb; ++b) mag *= biquadMagnitude(eqc[b], w);
                const float db = 20.0f * std::log10(std::max(1e-4f, mag));
                const float yn = clampf((db + 24.0f) / 48.0f, 0.0f, 1.0f);
                const float px = g.x + t * g.w;
                const float py = g.y + g.h - yn * g.h;
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line); strokeWidth(1.4f); stroke();
        }
        else if(e.kind == InsertDist)
        {
            // Input/output transfer curve over x in [-1, 1].
            beginPath();
            for(int i = 0; i <= steps; ++i)
            {
                const float xin = -1.0f + 2.0f * float(i) / float(steps);
                float yo = synth::distShape(e.dist.algo, xin, e.dist.drive, e.dist.bias) * e.dist.outGain;
                yo = clampf(yo, -1.2f, 1.2f) / 1.2f;
                const float px = g.x + (xin * 0.5f + 0.5f) * g.w;
                const float py = midY - yo * (g.h * 0.5f - 2.0f);
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line); strokeWidth(1.4f); stroke();
        }
        else if(e.kind == InsertComp)
        {
            // Static compression curve: input dB (-60..0) -> output dB.
            const float thr = e.comp.threshDb;
            const float ratio = std::max(1.0f, e.comp.ratio);
            const float makeup = e.comp.makeupDb;
            beginPath();
            for(int i = 0; i <= steps; ++i)
            {
                const float inDb = -60.0f + 60.0f * float(i) / float(steps);
                float outDb = inDb <= thr ? inDb : thr + (inDb - thr) / ratio;
                outDb += makeup;
                const float px = g.x + (inDb + 60.0f) / 60.0f * g.w;
                const float py = g.y + g.h - clampf((outDb + 60.0f) / 60.0f, 0.0f, 1.0f) * g.h;
                if(i == 0) moveTo(px, py); else lineTo(px, py);
            }
            strokeColor(line); strokeWidth(1.4f); stroke();
        }
        resetScissor();
    }

bool KapibaraUI::handleRouteFxClick(float x, float y)
{
        if(multibandEditorTrackId_ >= 0 && multibandEditorInsertIdx_ >= 0)
        {
            auto *outer = trackInsertsFor(uint32_t(multibandEditorTrackId_));
            if(outer != nullptr && multibandEditorInsertIdx_ >= 0 && multibandEditorInsertIdx_ < int(outer->size()))
            {
                auto &owner = (*outer)[(size_t)multibandEditorInsertIdx_];
                if(owner.kind == InsertMultiband)
                {
                    auto &mb = ensureMultibandParams(owner);
                    for(int band = 0; band < 3; ++band)
                    {
                        if(multibandBandMuteRects_[(size_t)band].contains(x, y))
                        {
                            mb.bandMute[band] = !mb.bandMute[band];
                            commitChainChange(multibandEditorTrackId_, -1);
                            return true;
                        }
                        if(multibandBandSoloRects_[(size_t)band].contains(x, y))
                        {
                            mb.bandSolo[band] = !mb.bandSolo[band];
                            commitChainChange(multibandEditorTrackId_, -1);
                            return true;
                        }
                    }
                }
            }
        }
        // Resolve the chain per hit (FX in a source's chain may live on another track).
        for(const auto &h : fxBypassHits_)
            if(h.rect.contains(x, y))
            {
                auto *chain = insertChainFor(h.trackId, h.mergeIdx);
                if(chain && h.insertIdx < int(chain->size()))
                { (*chain)[(size_t)h.insertIdx].bypass = !(*chain)[(size_t)h.insertIdx].bypass; commitChainChange(h.trackId, h.mergeIdx); return true; }
            }
        for(const auto &h : fxDeleteHits_)
            if(h.rect.contains(x, y))
            {
                auto *chain = insertChainFor(h.trackId, h.mergeIdx);
                if(chain && h.insertIdx < int(chain->size()))
                { chain->erase(chain->begin() + h.insertIdx); commitChainChange(h.trackId, h.mergeIdx); return true; }
            }
        for(const auto &h : fxModeHits_)
            if(h.rect.contains(x, y))
            {
                auto *chain = insertChainFor(h.trackId, h.mergeIdx);
                if(chain && h.insertIdx < int(chain->size()))
                { openModeMenu(h.trackId, h.mergeIdx, h.insertIdx, (*chain)[(size_t)h.insertIdx].kind, x, y); return true; }
            }
        return false;
    }

END_NAMESPACE_DISTRHO
