#include "../../KapibaraUI.hpp"

START_NAMESPACE_DISTRHO

synth::MatrixEnvPoint *KapibaraUI::curCurvePoints()
{
    const int idx = clampi(selectedMatrixModSlot_, 0, synth::kMaxModSlots - 1);
    return modSlots_[(size_t)idx].points.data();
}

int &KapibaraUI::curCurveCount()
{
    const int idx = clampi(selectedMatrixModSlot_, 0, synth::kMaxModSlots - 1);
    return modSlots_[(size_t)idx].pointCount;
}

void KapibaraUI::pushCurCurve()
{ pushCurModSlot(); }

bool &KapibaraUI::curCurveLoop()
{
    const int idx = clampi(selectedMatrixModSlot_, 0, synth::kMaxModSlots - 1);
    return modSlots_[(size_t)idx].loop;
}

float &KapibaraUI::curCurveRate()
{
    const int idx = clampi(selectedMatrixModSlot_, 0, synth::kMaxModSlots - 1);
    return modSlots_[(size_t)idx].rateHz;
}

float KapibaraUI::snapEnvValue(float v, bool isX)
{
        const float step = isX ? (1.0f / 16.0f) : (1.0f / 8.0f);
        return clampf(std::round(v / step) * step, 0.0f, 1.0f);
    }

float KapibaraUI::matrixEnvPx(float nx) const
{
        return matrixEnvCurveRect_.x + 8.0f + clampf(nx, 0.0f, 1.0f) * (matrixEnvCurveRect_.w - 16.0f);
    }

float KapibaraUI::matrixEnvPy(float ny) const
{
        return matrixEnvCurveRect_.y + matrixEnvCurveRect_.h - 5.0f - clampf(ny, 0.0f, 1.0f) * (matrixEnvCurveRect_.h - 10.0f);
    }

float KapibaraUI::matrixEnvNx(float x) const
{
        return clampf((x - (matrixEnvCurveRect_.x + 8.0f)) / std::max(1.0f, matrixEnvCurveRect_.w - 16.0f), 0.0f, 1.0f);
    }

float KapibaraUI::matrixEnvNy(float y) const
{
        return clampf(1.0f - (y - (matrixEnvCurveRect_.y + 5.0f)) / std::max(1.0f, matrixEnvCurveRect_.h - 10.0f), 0.0f, 1.0f);
    }

int KapibaraUI::matrixEnvPointAt(float x, float y)
{
        const auto *pts = curCurvePoints();
        const int count = clampi(curCurveCount(), 2, synth::kMaxMatrixEnvPoints);
        for(int i = 0; i < count; ++i)
            if(std::hypot(x - matrixEnvPx(pts[i].x), y - matrixEnvPy(pts[i].y)) <= 8.0f)
                return i;
        return -1;
    }

int KapibaraUI::matrixEnvSegmentAt(float x)
{
        const auto *pts = curCurvePoints();
        const int count = clampi(curCurveCount(), 2, synth::kMaxMatrixEnvPoints);
        const float nx = matrixEnvNx(x);
        for(int i = 0; i + 1 < count; ++i)
            if(nx <= pts[i + 1].x || i + 2 == count)
                return i;
        return -1;
    }

void KapibaraUI::addMatrixEnvPoint(float x, float y)
{
        auto *pts = curCurvePoints();
        int &countRef = curCurveCount();
        int count = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
        if(count >= synth::kMaxMatrixEnvPoints)
            return;
        float nx = matrixEnvNx(x);
        float ny = matrixEnvNy(y);
        if(!shiftDown_) { nx = snapEnvValue(nx, true); ny = snapEnvValue(ny, false); }
        int idx = 1;
        while(idx < count && pts[idx].x < nx)
            ++idx;
        idx = clampi(idx, 1, count - 1);
        for(int i = count; i > idx; --i)
            pts[i] = pts[i - 1];
        pts[idx] = synth::MatrixEnvPoint { nx, ny, 0.0f };
        countRef = count + 1;
        selectedEnvPoint_ = idx;
    }

void KapibaraUI::deleteMatrixEnvPoint(int idx)
{
        auto *pts = curCurvePoints();
        int &countRef = curCurveCount();
        int count = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
        if(idx <= 0 || idx >= count - 1 || count <= 2)
            return;  // keep the two endpoints
        for(int i = idx; i < count - 1; ++i)
            pts[i] = pts[i + 1];
        countRef = count - 1;
        selectedEnvPoint_ = -1;
    }

void KapibaraUI::editMatrixEnvCurve(float x, float y)
{
        auto *pts = curCurvePoints();
        int &countRef = curCurveCount();
        countRef = clampi(countRef, 2, synth::kMaxMatrixEnvPoints);
        if(selectedEnvPoint_ < 0 || selectedEnvPoint_ >= countRef)
            return;  // only an explicitly-grabbed point moves (no teleport)
        float nx = matrixEnvNx(x);
        float ny = matrixEnvNy(y);
        if(!shiftDown_) { nx = snapEnvValue(nx, true); ny = snapEnvValue(ny, false); }

        auto &pt = pts[selectedEnvPoint_];
        if(selectedEnvPoint_ == 0)
        {
            pt.x = 0.0f;
            pt.y = ny;
        }
        else if(selectedEnvPoint_ == countRef - 1)
        {
            pt.x = 1.0f;
            pt.y = ny;
        }
        else
        {
            const float lo = pts[selectedEnvPoint_ - 1].x + 0.01f;
            const float hi = pts[selectedEnvPoint_ + 1].x - 0.01f;
            pt.x = clampf(nx, lo, hi);
            pt.y = ny;
        }
        matrixEnvDirty_ = true; // published on mouse release, not every motion
    }

END_NAMESPACE_DISTRHO
