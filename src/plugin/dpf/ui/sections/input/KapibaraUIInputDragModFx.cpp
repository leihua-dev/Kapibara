#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

bool KapibaraUI::applyModFxLayoutDragValue(float x, float y)
{
        // Absolute horizontal position for strip/matrix depth bars.
        const auto normIn = [&](const Rect &r) { return clampf((x - r.x) / std::max(1.0f, r.w), 0.0f, 1.0f); };
        // Vertical delta-based knob: drag up = increase (200 px = full range, fine with modifier)
        // Sensitivity is in screen pixels (×uiRenderScale_) so knobs feel the same
        // regardless of window size — ~200 screen px for the full range.
        const auto knobNorm = [&]() -> float {
            return clampf(dragStartNorm_ + (dragStartY_ - y) * uiRenderScale_ / 200.0f, 0.0f, 1.0f);
        };
        auto &rule = rules_[(size_t)selectedRule_];

        switch(dragTarget_)
        {
            case DragTarget::ModEnvRate: curCurveRate() = std::max(0.05f, knobNorm() * 20.0f); pushCurCurve(); break;
            case DragTarget::AmpAdsrSeg:
            {
                auto &ae = ampEnvs_[(size_t)selectedAmpEnv_];
                const float c = clampf(dragStartDepth_ + (dragStartY_ - y) * uiRenderScale_ / 160.0f, 0.0f, 1.0f);
                if(ampAdsrDragSeg_ == 0)      ae.curveA = c;
                else if(ampAdsrDragSeg_ == 1) ae.curveD = c;
                else if(ampAdsrDragSeg_ == 2) ae.curveR = c;
                pushAmpEnv();
                break;
            }
            case DragTarget::MatrixEnvCurve: editMatrixEnvCurve(x, y); break;
            case DragTarget::MatrixEnvSeg:
            {
                auto *pts = curCurvePoints();
                if(envDragSeg_ >= 0 && envDragSeg_ < curCurveCount())
                {
                    pts[envDragSeg_].curve = clampf(dragStartDepth_ + (dragStartY_ - y) * uiRenderScale_ / 90.0f, -1.0f, 1.0f);
                    matrixEnvDirty_ = true;
                }
                break;
            }
            case DragTarget::RuleDepth: rule.depth = knobNorm() * 24.0f - 12.0f; pushRuleOnly(); break;
            case DragTarget::RuleXfer:
                // Serum-style bend: drag up = convex, down = concave.
                rule.transferCurve = clampf(dragStartDepth_ + (dragStartY_ - y) * uiRenderScale_ / 120.0f,
                                            -1.0f, 1.0f);
                pushRuleOnly();
                break;
            case DragTarget::GroupFreqSpread:
            case DragTarget::GroupPhaseSpread:
            case DragTarget::GroupSpreadCurve:
            {
                auto &g = maskGroups_[(size_t)clampi(selectedMaskGroup_, 0, synth::kMaxMaskGroups - 1)];
                float v = clampf(dragStartDepth_ + (dragStartY_ - y) * uiRenderScale_ / 120.0f,
                                 -1.0f, 1.0f);
                // Deadzone: the drag maps 120px onto ±1, so exact zero is one
                // pixel wide. FREQ SPRD especially must be reachable — a residual
                // 0.008 still reads "+0.00" but drifts the fan apart over minutes.
                if(std::abs(v) < 0.01f)
                    v = 0.0f;
                if(dragTarget_ == DragTarget::GroupFreqSpread)       g.freqSpread = v;
                else if(dragTarget_ == DragTarget::GroupPhaseSpread) g.phaseSpread = v;
                else                                                 g.spreadCurve = v;
                pushGroup(selectedMaskGroup_);
                break;
            }
            case DragTarget::GroupFamilyDepth:
            {
                auto &g = maskGroups_[(size_t)clampi(selectedMaskGroup_, 0, synth::kMaxMaskGroups - 1)];
                g.familyDepth = clampf(dragStartDepth_ + (dragStartY_ - y) * dragDepthLimit_ / 80.0f,
                                       -dragDepthLimit_, dragDepthLimit_);
                pushGroup(selectedMaskGroup_);
                break;
            }
            case DragTarget::GroupSlotDepth:
            {
                auto &g = maskGroups_[(size_t)clampi(selectedMaskGroup_, 0, synth::kMaxMaskGroups - 1)];
                if(groupDragSlot_ >= 0 && groupDragSlot_ < synth::kMaskGroupSlots)
                {
                    g.targets[(size_t)groupDragSlot_].depth =
                        clampf(dragStartDepth_ + (dragStartY_ - y) * dragDepthLimit_ / 80.0f,
                               -dragDepthLimit_, dragDepthLimit_);
                    pushGroup(selectedMaskGroup_);
                }
                break;
            }
            case DragTarget::MatrixRoutesScroll:
                // Absolute thumb position within the scrollbar groove.
                if(matrixRoutesScrollbarRect_.h > 1.0f)
                    matrixRoutesScroll_ = clampf((y - matrixRoutesScrollbarRect_.y)
                                                     / matrixRoutesScrollbarRect_.h,
                                                 0.0f, 1.0f)
                                          * matrixRoutesMaxScroll_;
                break;
            case DragTarget::ModDepth:
                rule.depth = clampf(dragStartDepth_ + (dragStartY_ - y) * dragDepthLimit_ / 80.0f,
                                    -dragDepthLimit_, dragDepthLimit_);
                pushRuleOnly();
                break;
            case DragTarget::RuleBandLo: rule.bandLo = clampi(int(knobNorm() * synth::kMaxPartials), 0, rule.bandHi - 1); pushRuleOnly(); break;
            case DragTarget::RuleBandHi: rule.bandHi = clampi(int(knobNorm() * synth::kMaxPartials), rule.bandLo + 1, synth::kMaxPartials); pushRuleOnly(); break;
            case DragTarget::ChaosRate: chaos_.frequencyHz = knobNorm() * 60.0f; pushChaosOnly(); break;
            case DragTarget::ChaosAmount: chaos_.amount = knobNorm(); pushChaosOnly(); break;
            case DragTarget::ShapePhase: shape_.phase0 = knobNorm(); pushShapeOnly(); break;
            case DragTarget::ShapeRho: shape_.rho = knobNorm(); pushShapeOnly(); break;
            case DragTarget::ShapeUp: shape_.pUp = 0.1f + knobNorm() * 7.9f; pushShapeOnly(); break;
            case DragTarget::ShapeDown: shape_.pDown = 0.1f + knobNorm() * 7.9f; pushShapeOnly(); break;
            case DragTarget::EqLow: effects_.eq.lowGainDb = knobNorm() * 48.0f - 24.0f; pushEffects(); break;
            case DragTarget::EqMid: effects_.eq.midGainDb = knobNorm() * 48.0f - 24.0f; pushEffects(); break;
            case DragTarget::EqHigh: effects_.eq.highGainDb = knobNorm() * 48.0f - 24.0f; pushEffects(); break;
            case DragTarget::EqDrive: effects_.eq.drive = 0.1f + knobNorm() * 5.9f; pushEffects(); break;
            case DragTarget::FilterCutoff: effects_.filter.cutoffHz = normToCutoff(knobNorm()); pushEffects(); break;
            case DragTarget::FilterResonance: effects_.filter.resonance = knobNorm(); pushEffects(); break;
            case DragTarget::FilterDrive: effects_.filter.drive = 0.1f + knobNorm() * 5.9f; pushEffects(); break;
            case DragTarget::UiScale: uiScale_ = 0.75f + knobNorm() * 0.75f; break;
            case DragTarget::FxInsertKnob:
                if(auto *chain = insertChainFor(fxDragHit_.trackId, fxDragHit_.mergeIdx);
                   chain != nullptr && fxDragHit_.insertIdx < int(chain->size()))
                {
                    fxKnobSetNorm((*chain)[(size_t)fxDragHit_.insertIdx], fxDragHit_.knob, knobNorm());
                    commitChainChange(fxDragHit_.trackId, fxDragHit_.mergeIdx);
                }
                break;
            case DragTarget::ModEntryDepth:
                if(auto *t = currentTrack(); t != nullptr && selectedModSlot_ >= 0 && selectedModSlot_ < synth::kMaxTrackMods
                   && modEntryActive(t->mods[(size_t)selectedModSlot_]))
                {
                    t->mods[(size_t)selectedModSlot_].depth = normIn(modDepthRects_[(size_t)selectedModSlot_]);
                    pushCurrentTrack();
                }
                break;
            case DragTarget::StripScroll:
            {
                const int totalTracks = int(generator_.tracks.size());
                const int totalCols   = totalTracks + int(stripGroups_.size());
                const float sbW       = std::max(1.0f, stripScrollbarRect_.w);
                constexpr float kMinStripW = 90.0f;
                const float gap2 = 6.0f;
                const float panelW = clampf(stripScrollbarRect_.w + 28.0f, kMinStripW, 4096.0f);
                const int maxVis2 = std::max(1, int((panelW - 28.0f + gap2) / (kMinStripW + gap2)));
                const int totalScroll2 = std::max(0, totalCols - maxVis2);
                if(totalScroll2 > 0)
                {
                    const float usableW = std::max(1.0f, sbW - std::max(16.0f, sbW * float(maxVis2) / float(totalCols)));
                    const float delta = ((x - dragScrollStartX_) / usableW) * float(totalScroll2);
                    stripScrollF_ = clampf(dragScrollStartValF_ + delta, 0.0f, float(totalScroll2));
                }
                break;
            }
            case DragTarget::LayoutVSplit:
            {
                const float pageH = static_cast<float>(uiH()) - 184.0f;
                const float delta = (dragStartY_ - y) / std::max(1.0f, pageH);
                layoutBottomRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.18f, 0.72f);
                break;
            }
            case DragTarget::LayoutRackSplit:
            {
                const float pageW = static_cast<float>(uiW()) - 32.0f;
                const float delta = (x - dragStartY_) / std::max(1.0f, pageW);
                layoutRackRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.11f, 0.38f);
                break;
            }
            case DragTarget::LayoutStripSplit:
            {
                const float pageW = static_cast<float>(uiW()) - 32.0f;
                const float delta = (dragStartY_ - x) / std::max(1.0f, pageW);
                layoutStripRatio_ = clampf(dragStartLayoutRatio_ + delta, 0.13f, 0.42f);
                break;
            }
            case DragTarget::None: break;
            default:
                return false;
        }
        return true;
    }

END_NAMESPACE_DISTRHO
