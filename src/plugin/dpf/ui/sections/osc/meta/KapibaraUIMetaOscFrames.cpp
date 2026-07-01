#include "../../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// 手动波表处理操作（0=DC/Norm 1=Crossfade 2=ZeroAlign 3=PhaseAlign 4=EnergySmooth）
bool KapibaraUI::performMetaFrameAction(int action)
{
        auto *track = currentTrack();
        if(track != nullptr && track->type == synth::SourceTrackType::PartialBank)
        {
            auto &seed = track->partialBank;
            seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
            selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
            synth::WavetablePartialSlot slot;
            slot.frameCount = seed.frameCount;
            slot.morph = seed.morph;
            slot.frames = seed.frames;

            if(action == 2)
            {
                int selectedCount = 0;
                for(int i = 0; i < slot.frameCount; ++i)
                    selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
                if(selectedCount == 0)
                {
                    metaEditorStatus_ = "select frames to delete";
                    return true;
                }
                if(slot.frameCount <= 1)
                {
                    metaEditorStatus_ = "one frame required";
                    return true;
                }
                const int deleted = synth::deleteSelectedWavetableFrames(
                    slot, metaFrameSelected_.data(), slot.frameCount);
                seed.frameCount = slot.frameCount;
                seed.frames = slot.frames;
                seed.morph = slot.morph;
                selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, seed.frameCount - 1));
                metaFrameSelected_.fill(false);
                metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
                metaFrameRangeAnchor_ = selectedMetaFrame_;
                metaEditorStatus_ = deleted > 0 ? "selected partial frames deleted" : "no frames deleted";
                pushCurrentTrack();
                return true;
            }

            bool changed = false;
            switch(action)
            {
                case 0:
                    changed = synth::addWavetableFrame(slot, selectedMetaFrame_);
                    if(changed) ++selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "blank partial frame added" : "frame limit reached";
                    break;
                case 1:
                    changed = synth::duplicateWavetableFrame(slot, selectedMetaFrame_);
                    if(changed) ++selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "partial frame duplicated" : "frame limit reached";
                    break;
                case 3:
                    changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ - 1);
                    if(changed) --selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "partial frame moved left" : "already first frame";
                    break;
                case 4:
                    changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ + 1);
                    if(changed) ++selectedMetaFrame_;
                    metaEditorStatus_ = changed ? "partial frame moved right" : "already last frame";
                    break;
                default:
                    break;
            }
            if(changed)
            {
                seed.frameCount = slot.frameCount;
                seed.frames = slot.frames;
                seed.morph = slot.morph;
                seed.frameCount = clampi(seed.frameCount, 1, synth::kMaxWavetableFrames);
                selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, seed.frameCount - 1);
                metaFrameSelected_.fill(false);
                metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
                metaFrameRangeAnchor_ = selectedMetaFrame_;
                pushCurrentTrack();
            }
            return true;
        }
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return true;
        auto &slot = track->metaOsc;
        selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));

        if(action == 2)
        {
            int selectedCount = 0;
            for(int i = 0; i < slot.frameCount; ++i)
                selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
            if(selectedCount == 0)
            {
                metaEditorStatus_ = "select frames to delete";
                return true;
            }
            if(slot.frameCount <= 1)
            {
                metaEditorStatus_ = "one frame required";
                return true;
            }

            pushMetaUndoSnapshot();
            const int deleted = synth::deleteSelectedWavetableFrames(
                slot, metaFrameSelected_.data(), slot.frameCount);
            selectedMetaFrame_ = clampi(selectedMetaFrame_, 0, std::max(0, slot.frameCount - 1));
            metaFrameSelected_.fill(false);
            metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
            metaFrameRangeAnchor_ = selectedMetaFrame_;
            metaEditorStatus_ = deleted < selectedCount
                                  ? "selected frames deleted; one frame retained"
                                  : "selected frames deleted";
            if(deleted > 0)
                pushCurrentTrack();
            return true;
        }

        pushMetaUndoSnapshot();
        bool changed = false;
        switch(action)
        {
            case 0:
                changed = synth::addWavetableFrame(slot, selectedMetaFrame_);
                if(changed) ++selectedMetaFrame_;
                metaEditorStatus_ = changed ? "blank frame added" : "frame limit reached";
                break;
            case 1:
                changed = synth::duplicateWavetableFrame(slot, selectedMetaFrame_);
                if(changed) ++selectedMetaFrame_;
                metaEditorStatus_ = changed ? "frame duplicated" : "frame limit reached";
                break;
            case 3:
                changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ - 1);
                if(changed) --selectedMetaFrame_;
                metaEditorStatus_ = changed ? "frame moved left" : "already first";
                break;
            case 4:
                changed = synth::moveWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ + 1);
                if(changed) ++selectedMetaFrame_;
                metaEditorStatus_ = changed ? "frame moved right" : "already last";
                break;
            case 5:
            {
                const int reference = selectedMetaFrame_ > 0 ? selectedMetaFrame_ - 1 : 1;
                changed = slot.frameCount > 1
                       && synth::alignWavetableFramePhase(slot, selectedMetaFrame_, reference);
                metaEditorStatus_ = changed ? "phase aligned" : "needs another frame";
                break;
            }
            case 6:
            case 7:
                if(selectedMetaFrame_ > 0 && selectedMetaFrame_ + 1 < slot.frameCount)
                    changed = synth::morphWavetableFrame(slot, selectedMetaFrame_, selectedMetaFrame_ - 1,
                                                         selectedMetaFrame_ + 1, 0.5f,
                                                         action == 6 ? synth::WavetableMorphMode::Linear
                                                                     : synth::WavetableMorphMode::Spectral);
                metaEditorStatus_ = changed ? (action == 6 ? "linear midpoint" : "spectral midpoint")
                                            : "select an interior frame";
                break;
            default:
                break;
        }
        if(changed)
        {
            metaFrameSelected_.fill(false);
            metaFrameSelected_[(size_t)selectedMetaFrame_] = true;
            metaFrameRangeAnchor_ = selectedMetaFrame_;
            pushCurrentTrack();
        }
        return true;
    }

bool KapibaraUI::applyWavetableProcess(int op)
{
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return true;
        auto &slot = track->metaOsc;

        int selectedCount = 0;
        for(int i = 0; i < slot.frameCount; ++i)
            selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
        if(selectedCount == 0)
        {
            metaEditorStatus_ = "select frames to process";
            return true;
        }
        pushMetaUndoSnapshot();
        const bool *sel = metaFrameSelected_.data();

        switch(op)
        {
            case 0:
                synth::processWavetableRemoveDC(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: DC removed + RMS normalized";
                break;
            case 1:
                synth::processWavetableCrossfade(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: crossfade applied";
                break;
            case 2:
                synth::processWavetableZeroAlign(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: zero-crossing aligned";
                break;
            case 3:
                synth::processWavetableAlignPhases(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: phases aligned";
                break;
            case 4:
                synth::processWavetableEnergySmooth(slot, sel, slot.frameCount);
                metaEditorStatus_ = "selected frames: energy smoothed";
                break;
            default: break;
        }
        pushCurrentTrack();
        return true;
    }

bool KapibaraUI::applySelectedFrameMorph(int targetFrameCount)
{
        auto *track = currentTrack();
        if(track == nullptr || track->type != synth::SourceTrackType::MetaOscillator)
            return false;

        int selectedCount = 0;
        for(int i = 0; i < track->metaOsc.frameCount; ++i)
            selectedCount += metaFrameSelected_[(size_t)i] ? 1 : 0;
        if(selectedCount < 2)
        {
            metaEditorStatus_ = "select at least two frames to morph";
            return true;
        }

        pushMetaUndoSnapshot();
        if(!synth::expandSelectedWavetableFrames(track->metaOsc, metaFrameSelected_.data(),
                                                 selectedCount, targetFrameCount))
        {
            if(!metaUndoStack_.empty())
                metaUndoStack_.pop_back();
            metaEditorStatus_ = "morph expansion failed";
            return true;
        }

        selectedMetaFrame_ = 0;
        metaFramePageStart_ = 0;
        metaFrameScrollStart_ = 0;
        metaFrameSelected_.fill(false);
        metaFrameSelected_[0] = true;
        metaFrameRangeAnchor_ = 0;
        metaEditorStatus_ = targetFrameCount == 256 ? "selection morphed to 256 frames"
                                                     : "selection morphed to 512 frames";
        pushCurrentTrack();
        return true;
    }

END_NAMESPACE_DISTRHO
