#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

// ---- Insert TYPE picker (append a new effect to a chain) ----



    // ---- Filter/dist algorithm picker for a specific insert ----
std::vector<InsertEffect> *KapibaraUI::insertChainFor(int trackId, int mergeIdx)
{
        if(trackId == -2)
        {
            auto *outer = trackInsertsFor(uint32_t(multibandEditorTrackId_));
            if(outer == nullptr || multibandEditorInsertIdx_ < 0 || multibandEditorInsertIdx_ >= int(outer->size()))
                return nullptr;
            auto &owner = (*outer)[(size_t)multibandEditorInsertIdx_];
            if(owner.kind != InsertMultiband)
                return nullptr;
            auto &mb = ensureMultibandParams(owner);
            if(mergeIdx < 0 || mergeIdx >= 3)
                return nullptr;
            return &mb.bands[mergeIdx];
        }
        (void)mergeIdx;
        if(trackId >= 0)
            return trackInsertsFor(uint32_t(trackId));
        return nullptr;
    }

void KapibaraUI::commitChainChange(int trackId, int group)
{
        (void)group;
        if(trackId == -2)
        {
            if(multibandEditorTrackId_ >= 0)
                pushTrackById(uint32_t(multibandEditorTrackId_));
            return;
        }
        if(trackId >= 0)
            pushTrackById(uint32_t(trackId));
    }

void KapibaraUI::openInsertMenu(int trackId, int mergeIdx, float x, float y)
{
        insertMenuTrackId_ = trackId;
        insertMenuMerge_   = mergeIdx;
        constexpr float rowH = 20.0f;
        constexpr float menuW = 120.0f;
        const float menuH = 22.0f + rowH * float(kInsertTypeCount);
        if(y + menuH + 8.0f > float(uiH()))
            y -= menuH;
        insertMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - menuW - 8.0f));
        insertMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - menuH - 8.0f));
        insertMenuOpen_ = true;
    }

void KapibaraUI::drawInsertMenu()
{
        if(!insertMenuOpen_)
            return;
        constexpr float rowH = 20.0f;
        const float menuW = 120.0f;
        const Rect panel { insertMenuX_, insertMenuY_, menuW, 22.0f + rowH * float(kInsertTypeCount) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 4.0f, "Add effect", nullptr);
        for(int c = 0; c < kInsertTypeCount; ++c)
        {
            insertMenuRects_[(size_t)c] = { panel.x + 6.0f, panel.y + 20.0f + float(c) * rowH, menuW - 12.0f, rowH - 2.0f };
            drawButton(insertMenuRects_[(size_t)c], insertTypeName(c + 1), false);
        }
    }

bool KapibaraUI::handleInsertMenuClick(float x, float y)
{
        if(!insertMenuOpen_)
            return false;
        insertMenuOpen_ = false;
        auto *chain = insertChainFor(insertMenuTrackId_, insertMenuMerge_);
        if(chain == nullptr)
            return true;
        for(int c = 0; c < kInsertTypeCount; ++c)
            if(insertMenuRects_[(size_t)c].contains(x, y))
            {
                InsertEffect e;
                e.kind = uint8_t(c + 1);
                if(e.kind == InsertMultiband)
                    e.multiband = std::make_shared<synth::MultibandSlotParams>();
                chain->push_back(e);
                commitChainChange(insertMenuTrackId_, insertMenuMerge_);
                return true;
            }
        return true;
    }

int KapibaraUI::fxModeCount(int kind) const
{
        if(kind == InsertFilter) return kFilterAlgoCount;
        if(kind == InsertDist)   return kDistAlgoCount;
        if(kind == InsertDelay)  return 2; // Stereo / PingPong
        if(kind == InsertConvReverb) return int(irFiles_.size());
        return 0;
    }

const char *KapibaraUI::fxModeName(int kind, int i)
{
        if(kind == InsertFilter) return kFilterAlgoNames[i];
        if(kind == InsertDist)   return kDistAlgoNames[i];
        if(kind == InsertDelay)  { static const char *D[2] = { "Stereo", "PingPong" }; return D[i]; }
        if(kind == InsertConvReverb) return (i >= 0 && i < int(irFiles_.size())) ? irFiles_[(size_t)i].first.c_str() : "";
        return "";
    }

int KapibaraUI::fxCurrentMode(const InsertEffect &ins, int kind)
{
        if(kind == InsertFilter) return int(ins.filter.algo);
        if(kind == InsertDist)   return int(ins.dist.algo);
        if(kind == InsertDelay)  return ins.delay.pingpong ? 1 : 0;
        if(kind == InsertConvReverb)
            for(int i = 0; i < int(irFiles_.size()); ++i)
                if(irFiles_[(size_t)i].first == ins.conv.irName) return i;
        return 0;
    }

void KapibaraUI::openModeMenu(int trackId, int mergeIdx, int insertIdx, int kind, float x, float y)
{
        modeMenuTrackId_ = trackId; modeMenuMerge_ = mergeIdx; modeMenuInsertIdx_ = insertIdx; modeMenuKind_ = kind;
        if(kind == InsertConvReverb) refreshIrFiles();
        modeMenuSelectedIndex_ = 0;
        if(auto *chain = insertChainFor(trackId, mergeIdx);
           chain != nullptr && insertIdx >= 0 && insertIdx < int(chain->size()))
            modeMenuSelectedIndex_ = fxCurrentMode((*chain)[(size_t)insertIdx], kind);
        const int rows = std::min(fxModeCount(kind), int(modeMenuRects_.size()));
        modeMenuX_ = clampf(x, 4.0f, std::max(4.0f, float(uiW()) - 130.0f));
        modeMenuY_ = clampf(y, 4.0f, std::max(4.0f, float(uiH()) - (28.0f + float(rows) * 18.0f)));
        modeMenuOpen_ = true;
    }

void KapibaraUI::drawModeMenu()
{
        if(!modeMenuOpen_)
            return;
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuMerge_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size())) { modeMenuOpen_ = false; return; }
        const int rows = std::min(fxModeCount(modeMenuKind_), int(modeMenuRects_.size()));
        constexpr float rowH = 18.0f;
        const float menuW = 122.0f;
        const Rect panel { modeMenuX_, modeMenuY_, menuW, 22.0f + rowH * float(rows) };
        drawPanel(panel, rgba(0x10171df8), rgba(0x5b7380ff));
        fontSize(9.0f); fillColor(rgba(0xc8d6dcff)); textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(panel.x + 8.0f, panel.y + 5.0f, fxModeTitle(modeMenuKind_), nullptr);
        const auto &ins = (*chain)[(size_t)modeMenuInsertIdx_];
        const int cur = fxCurrentMode(ins, modeMenuKind_);
        modeMenuSelectedIndex_ = clampi(modeMenuSelectedIndex_, 0, std::max(0, rows - 1));
        for(int i = 0; i < rows; ++i)
        {
            modeMenuRects_[(size_t)i] = { panel.x + 6.0f, panel.y + 20.0f + float(i) * rowH, menuW - 12.0f, rowH - 2.0f };
            drawButton(modeMenuRects_[(size_t)i], fxModeName(modeMenuKind_, i), cur == i || modeMenuSelectedIndex_ == i);
        }
    }

void KapibaraUI::commitModeMenuSelection()
{
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuMerge_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size()))
        {
            modeMenuOpen_ = false;
            return;
        }
        const int rows = std::min(fxModeCount(modeMenuKind_), int(modeMenuRects_.size()));
        const int i = clampi(modeMenuSelectedIndex_, 0, std::max(0, rows - 1));
        auto &ins = (*chain)[(size_t)modeMenuInsertIdx_];
        if(modeMenuKind_ == InsertFilter)     ins.filter.algo = static_cast<synth::InsertFilterAlgo>(i);
        else if(modeMenuKind_ == InsertDist)  ins.dist.algo = static_cast<synth::InsertDistAlgo>(i);
        else if(modeMenuKind_ == InsertDelay) ins.delay.pingpong = (i == 1);
        else if(modeMenuKind_ == InsertConvReverb && i < int(irFiles_.size()))
            loadImpulseIntoInsert(ins, irFiles_[(size_t)i].first, irFiles_[(size_t)i].second);
        commitChainChange(modeMenuTrackId_, modeMenuMerge_);
        modeMenuOpen_ = false;
    }

bool KapibaraUI::handleModeMenuClick(float x, float y)
{
        if(!modeMenuOpen_)
            return false;
        auto *chain = insertChainFor(modeMenuTrackId_, modeMenuMerge_);
        if(chain == nullptr || modeMenuInsertIdx_ < 0 || modeMenuInsertIdx_ >= int(chain->size()))
        {
            modeMenuOpen_ = false;
            return true;
        }
        const int rows = std::min(fxModeCount(modeMenuKind_), int(modeMenuRects_.size()));
        for(int i = 0; i < rows; ++i)
            if(modeMenuRects_[(size_t)i].contains(x, y))
            {
                modeMenuSelectedIndex_ = i;
                if(currentClickIsDouble_)
                    commitModeMenuSelection();
                return true;
            }
        modeMenuOpen_ = false;
        return true;
    }

bool KapibaraUI::handleInsertButtonClick(float x, float y)
{
        if(multibandEditorTrackId_ >= 0 && multibandEditorInsertIdx_ >= 0)
        {
            for(int band = 0; band < 3; ++band)
                if(multibandBandAddRects_[(size_t)band].w > 0.0f
                   && multibandBandAddRects_[(size_t)band].contains(x, y))
                {
                    openInsertMenu(-2, band, x, y);
                    return true;
                }
        }
        // Press on a strip insert chip: pend (click = add-menu for "+", drag = reorder).
        for(const auto &hit : insertHits_)
            if(hit.rect.contains(x, y))
            {
                insertPending_ = true;
                insertDragActive_ = false;
                insertPendTrackId_ = hit.trackId;
                insertPendMerge_ = hit.mergeIdx;
                insertPendSlot_ = hit.slot;  // -1 = the "+ add" chip
                insertPendX_ = x;
                insertPendY_ = y;
                return true;
            }
        return false;
    }

void KapibaraUI::finishInsertInteraction(float x, float y)
{
        if(!insertPending_)
            return;
        const int fromSlot = insertPendSlot_;
        const int trackId = insertPendTrackId_;
        const int merge = insertPendMerge_;
        const bool wasDrag = insertDragActive_;
        insertPending_ = false;
        insertDragActive_ = false;
        auto *chain = insertChainFor(trackId, merge);
        if(chain == nullptr)
            return;
        if(fromSlot < 0)  // the "+ add" chip
        {
            if(!wasDrag) openInsertMenu(trackId, merge, x, y);
            return;
        }
        if(!wasDrag)
        {
            if(fromSlot >= 0 && fromSlot < int(chain->size()) && (*chain)[(size_t)fromSlot].kind == InsertMultiband
               && trackId >= 0)
            {
                multibandEditorTrackId_ = trackId;
                multibandEditorInsertIdx_ = fromSlot;
                ensureMultibandParams((*chain)[(size_t)fromSlot]);
                repaint();
            }
            return;
        }
        // Drag → reorder within the same chain (move fromSlot to the chip under cursor).
        for(const auto &hit : insertHits_)
            if(hit.trackId == trackId && hit.mergeIdx == merge && hit.slot >= 0 && hit.rect.contains(x, y))
            {
                int toSlot = hit.slot;
                if(toSlot != fromSlot && fromSlot < int(chain->size()) && toSlot < int(chain->size()))
                {
                    InsertEffect moved = (*chain)[(size_t)fromSlot];
                    chain->erase(chain->begin() + fromSlot);
                    chain->insert(chain->begin() + toSlot, moved);
                    commitChainChange(trackId, merge);
                }
                break;
            }
    }

END_NAMESPACE_DISTRHO
