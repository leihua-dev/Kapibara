#pragma once

#include "dsp/InsertEffects.h"

#include <cstdint>
#include <vector>

namespace synth
{

struct GridPoint
{
    int x = 0;
    int y = 0;
};

struct GridPortRef
{
    uint32_t nodeId = 0;
    uint8_t port = 0;
};

struct GridWire
{
    GridPortRef from {};
    GridPortRef to {};
    std::vector<GridPoint> points {};
};

enum class PerVoiceNodeType : uint8_t
{
    Input = 0,
    Filter,
    Output
};

struct PerVoiceNode
{
    uint32_t id = 0;
    uint32_t trackId = 0;
    PerVoiceNodeType type = PerVoiceNodeType::Input;
    GridPoint grid {};
};

enum class StripNodeType : uint8_t
{
    Input = 0,
    Insert,
    Master
};

struct StripNode
{
    uint32_t id = 0;
    uint32_t trackId = 0;
    StripNodeType type = StripNodeType::Input;
    InsertEffect insert {};
    int insertIndex = -1;
    GridPoint grid {};
};

} // namespace synth
