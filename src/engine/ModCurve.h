#pragma once

#include "dsp/SpectralFrame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace synth
{

// -----------------------------------------------------------------------------
// Breakpoint curve — used by unified modulator slots
// -----------------------------------------------------------------------------
static constexpr int kMaxMatrixEnvPoints = 16;

struct MatrixEnvPoint
{
    float x = 0.0f;
    float y = 0.0f;
    float curve = 0.0f;

    constexpr MatrixEnvPoint() = default;
    constexpr MatrixEnvPoint(float xIn, float yIn, float curveIn)
        : x(xIn), y(yIn), curve(curveIn)
    {
    }
};

inline float matrixEnvSegmentValue(const MatrixEnvPoint &a, const MatrixEnvPoint &b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float curve = std::clamp(a.curve, -1.0f, 1.0f);
    const float shaped = curve >= 0.0f ? std::pow(t, 1.0f + curve * 4.0f)
                                       : 1.0f - std::pow(1.0f - t, 1.0f - curve * 4.0f);
    return a.y + (b.y - a.y) * shaped;
}

inline float pointCurveEval(const MatrixEnvPoint *points, int pointCount, float x)
{
    const int count = std::clamp(pointCount, 2, kMaxMatrixEnvPoints);
    x = std::clamp(x, 0.0f, 1.0f);
    if(x <= points[0].x)
        return std::clamp(points[0].y, 0.0f, 1.0f);
    for(int i = 0; i + 1 < count; ++i)
    {
        const auto &a = points[(size_t)i];
        const auto &b = points[(size_t)i + 1];
        if(x <= b.x || i + 2 == count)
        {
            const float span = std::max(0.0001f, b.x - a.x);
            return std::clamp(matrixEnvSegmentValue(a, b, (x - a.x) / span), 0.0f, 1.0f);
        }
    }
    return std::clamp(points[(size_t)count - 1].y, 0.0f, 1.0f);
}

// -----------------------------------------------------------------------------
// Unified modulator slot
// loop=true  → continuous phase (LFO behavior, retriggers on note-on)
// loop=false → one-shot from note-on, holds at end
// Output:  pointCurveEval(phase) * 2 - 1  →  -1..+1
// -----------------------------------------------------------------------------
struct ModSlotParams
{
    bool enabled = false;
    bool loop = true;
    float rateHz = 1.0f;
    int pointCount = 4;
    std::array<MatrixEnvPoint, kMaxMatrixEnvPoints> points {
        MatrixEnvPoint { 0.0f, 0.5f, 0.0f },
        MatrixEnvPoint { 0.25f, 1.0f, 0.0f },
        MatrixEnvPoint { 0.5f, 0.5f, 0.0f },
        MatrixEnvPoint { 1.0f, 0.5f, 0.0f }
    };
};

// -----------------------------------------------------------------------------
// Chaos source
// -----------------------------------------------------------------------------
enum class ChaosNoiseType : uint8_t
{
    White = 0,
    Smooth = 1,
    Crackle = 2
};

struct ChaosParams
{
    bool enabled = false;
    ChaosNoiseType type = ChaosNoiseType::Smooth;
    float frequencyHz = 8.0f;
    float amount = 1.0f;
};

// -----------------------------------------------------------------------------
// Shape source (spectral-domain static modulator)
// LfoShape kept because ShapeSourceParams still uses it.
// -----------------------------------------------------------------------------
enum class LfoShape : uint8_t
{
    Asymmetric = 0,
    Sine = 1,
    Square = 2,
    Triangle = 3,
    SampleHold = 4
};

struct ShapeSourceParams
{
    LfoShape shape = LfoShape::Asymmetric;
    float phase0 = 0.0f;
    float rho = 0.5f;
    float pUp = 1.0f;
    float pDown = 1.0f;
    bool useSpectralX = false;
};

} // namespace synth
