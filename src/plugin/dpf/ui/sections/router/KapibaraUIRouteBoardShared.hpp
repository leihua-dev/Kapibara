#pragma once

#include "../../KapibaraUI.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

START_NAMESPACE_DISTRHO

namespace routeui
{
inline uint32_t stripNodeId(uint32_t trackId, int local)
{
    return 0x20000000u | (trackId << 8u) | uint32_t(local & 0xff);
}

inline uint32_t masterNodeId()
{
    return 0x2fffffffu;
}

inline uint32_t sourceRouterNodeId(uint32_t trackId)
{
    return 0x08000000u | (trackId & 0x00ffffffu);
}

inline uint32_t perVoiceNodeId(uint32_t trackId, int local)
{
    (void)trackId;
    return 0x10000000u | uint32_t(local & 0x0f);
}

inline bool isPerVoiceNode(uint32_t nodeId)
{
    return (nodeId & 0xf0000000u) == 0x10000000u;
}

inline bool isSourceRouterNode(uint32_t nodeId)
{
    return (nodeId & 0xff000000u) == 0x08000000u;
}

inline bool isStripNode(uint32_t nodeId)
{
    return (nodeId & 0xf0000000u) == 0x20000000u;
}

inline bool isAmpEnvNode(uint32_t nodeId)
{
    return (nodeId & 0xf0000000u) == 0x30000000u;
}

inline int ampEnvSlotId(uint32_t nodeId)
{
    const int encoded = int((nodeId >> 8u) & 0xffu);
    if(encoded > 0)
        return encoded - 1;
    return int(nodeId & 0x0fu) - 1; // legacy AE1..AE4 singleton ids
}

inline int ampEnvInstanceId(uint32_t nodeId)
{
    const int encoded = int((nodeId >> 8u) & 0xffu);
    if(encoded > 0)
        return int(nodeId & 0xffu) - 1;
    return int(nodeId & 0x0fu) - 1; // legacy maps to instance 0..3
}

inline uint32_t sourceRouterTrackId(uint32_t nodeId)
{
    return nodeId & 0x00ffffffu;
}

inline uint32_t stripTrackId(uint32_t nodeId)
{
    return (nodeId & 0x0fffff00u) >> 8u;
}

inline int stripLocalId(uint32_t nodeId)
{
    return int(nodeId & 0xffu);
}

inline int perVoiceLocalId(uint32_t nodeId)
{
    return int(nodeId & 0x0fu);
}

inline bool samePort(const synth::GridPortRef &a, const synth::GridPortRef &b)
{
    return a.nodeId == b.nodeId && a.port == b.port;
}

inline bool samePoint(const synth::GridPoint &a, const synth::GridPoint &b)
{
    return a.x == b.x && a.y == b.y;
}

inline void appendConstrainedPath(std::vector<synth::GridPoint> &points, const synth::GridPoint &end)
{
    if(points.empty()) { points.push_back(end); return; }
    const auto start = points.back();
    const int dx = end.x - start.x;
    const int dy = end.y - start.y;
    const int adx = std::abs(dx);
    const int ady = std::abs(dy);
    if(adx == 0 || ady == 0 || adx == ady)
    {
        if(!samePoint(start, end))
            points.push_back(end);
        return;
    }
    // 45° diagonal first, then straight: shorter diagonal leg, then axis-aligned.
    const int d = std::min(adx, ady);
    const synth::GridPoint diag { start.x + (dx < 0 ? -d : d), start.y + (dy < 0 ? -d : d) };
    if(!samePoint(points.back(), diag)) points.push_back(diag);
    if(!samePoint(points.back(), end)) points.push_back(end);
}

inline std::vector<synth::GridPoint> cleanRoutePath(const synth::GridPoint &start, const synth::GridPoint &end)
{
    std::vector<synth::GridPoint> points { start };
    appendConstrainedPath(points, end);
    return points;
}

// When several wires feed one input port we must keep them apart so they only
// touch at the port itself (never share a trunk segment in front of it). Each
// wire is given a distinct final-approach direction so the last leg into the
// port is unique per wire. `lane` is the wire's index (top→bottom by source Y)
// among `total` wires reaching the same port. Returns the staging point that the
// wire routes to before a short, direction-unique segment into `port`.
inline synth::GridPoint laneStubStart(const synth::GridPoint &port, int lane, int total, int len)
{
    // Approach directions ordered top→bottom (stub offset relative to the port):
    //   0:N  1:NW  2:W  3:SW  4:S
    static const synth::GridPoint kDirs[5] = {
        {  0, -1 }, { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, 1 }
    };
    // Symmetric, diagonal-preferring fan windows so the common 2/3-wire cases
    // approach cleanly from the upper-left / lower-left rather than vertically.
    static const int kT2[] = { 1, 3 };
    static const int kT3[] = { 1, 2, 3 };
    static const int kT4[] = { 0, 1, 3, 4 };
    static const int kT5[] = { 0, 1, 2, 3, 4 };
    if(total <= 1) return port;
    int dirIdx;
    if(total == 2)      dirIdx = kT2[lane % 2];
    else if(total == 3) dirIdx = kT3[lane % 3];
    else if(total == 4) dirIdx = kT4[lane % 4];
    else if(total == 5) dirIdx = kT5[lane % 5];
    else { dirIdx = lane % 5; len += (lane / 5) * len; } // rare: >5 wires to one port
    const auto &d = kDirs[dirIdx];
    return synth::GridPoint { port.x + d.x * len, port.y + d.y * len };
}


inline bool segmentIntersection(const synth::GridPoint &a, const synth::GridPoint &b,
                         const synth::GridPoint &c, const synth::GridPoint &d,
                         synth::GridPoint &out)
{
    const float x1 = float(a.x), y1 = float(a.y), x2 = float(b.x), y2 = float(b.y);
    const float x3 = float(c.x), y3 = float(c.y), x4 = float(d.x), y4 = float(d.y);
    const float den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if(std::abs(den) < 1.0e-4f)
        return false;
    const float px = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / den;
    const float py = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / den;
    const auto between = [](float v, float a0, float b0) {
        return v >= std::min(a0, b0) - 0.5f && v <= std::max(a0, b0) + 0.5f;
    };
    if(!between(px, x1, x2) || !between(py, y1, y2) || !between(px, x3, x4) || !between(py, y3, y4))
        return false;
    out = { int(std::round(px)), int(std::round(py)) };
    return true;
}

inline synth::GridPoint constrainedRoutePoint(const synth::GridPoint &from, float px, float py)
{
    const float dx = px - float(from.x);
    const float dy = py - float(from.y);
    const float adx = std::abs(dx);
    const float ady = std::abs(dy);
    if(adx < 1.0f && ady < 1.0f) return from;
    if(ady < 1.0f) return synth::GridPoint { int(std::round(px)), from.y };
    if(adx < 1.0f) return synth::GridPoint { from.x, int(std::round(py)) };
    // 45° snapping: snap to diagonal direction from last point.
    const float d = std::min(adx, ady);
    return synth::GridPoint {
        int(std::round(float(from.x) + (dx < 0.0f ? -d : d))),
        int(std::round(float(from.y) + (dy < 0.0f ? -d : d)))
    };
}

inline float pointToSegmentDist2(float px, float py, float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    const float len2 = dx * dx + dy * dy;
    if(len2 < 0.0001f)
        return (px - ax) * (px - ax) + (py - ay) * (py - ay);
    const float t = std::max(0.0f, std::min(1.0f, ((px - ax) * dx + (py - ay) * dy) / len2));
    const float cx = ax + t * dx, cy = ay + t * dy;
    return (px - cx) * (px - cx) + (py - cy) * (py - cy);
}

inline bool pointNearPolyline(float px, float py, const std::vector<synth::GridPoint> &path, float threshold)
{
    const float t2 = threshold * threshold;
    for(size_t i = 1; i < path.size(); ++i)
        if(pointToSegmentDist2(px, py, float(path[i - 1].x), float(path[i - 1].y),
                               float(path[i].x), float(path[i].y)) <= t2)
            return true;
    return false;
}

// Returns (segIdx, t-within-seg) of the closest point on path to (px, py).
inline std::pair<int, float> findPathPos(float px, float py, const std::vector<synth::GridPoint> &path)
{
    int bestSeg = 0;
    float bestT = 0.0f;
    float bestDist2 = 1e18f;
    for(int i = 0; i + 1 < int(path.size()); ++i)
    {
        const float ax = float(path[i].x), ay = float(path[i].y);
        const float bx = float(path[i + 1].x), by = float(path[i + 1].y);
        const float dx = bx - ax, dy = by - ay;
        const float len2 = dx * dx + dy * dy;
        float t = 0.0f;
        if(len2 > 0.0f)
            t = std::max(0.0f, std::min(1.0f, ((px - ax) * dx + (py - ay) * dy) / len2));
        const float cx = ax + t * dx, cy = ay + t * dy;
        const float d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy);
        if(d2 < bestDist2) { bestDist2 = d2; bestSeg = i; bestT = t; }
    }
    return { bestSeg, bestT };
}

// Returns sub-path from path start up to (and including) pt.
inline std::vector<synth::GridPoint> pathUpTo(const std::vector<synth::GridPoint> &path,
                                       const synth::GridPoint &pt)
{
    for(int i = 0; i + 1 < int(path.size()); ++i)
    {
        if(pointToSegmentDist2(float(pt.x), float(pt.y),
                               float(path[i].x), float(path[i].y),
                               float(path[i + 1].x), float(path[i + 1].y)) < 1.0f)
        {
            std::vector<synth::GridPoint> result(path.begin(), path.begin() + i + 1);
            if(!samePoint(result.back(), pt)) result.push_back(pt);
            return result;
        }
    }
    return path;
}

// Returns sub-path from pt (inclusive) to path end.
inline std::vector<synth::GridPoint> pathFrom(const std::vector<synth::GridPoint> &path,
                                       const synth::GridPoint &pt)
{
    for(int i = 0; i + 1 < int(path.size()); ++i)
    {
        if(pointToSegmentDist2(float(pt.x), float(pt.y),
                               float(path[i].x), float(path[i].y),
                               float(path[i + 1].x), float(path[i + 1].y)) < 1.0f)
        {
            std::vector<synth::GridPoint> result;
            result.push_back(pt);
            result.insert(result.end(), path.begin() + i + 1, path.end());
            return result;
        }
    }
    return { path.empty() ? pt : path.back() };
}

// Returns sub-path between ptA and ptB (both inclusive) along the polyline.
inline std::vector<synth::GridPoint> extractSubpath(const std::vector<synth::GridPoint> &path,
                                              const synth::GridPoint &ptA,
                                              const synth::GridPoint &ptB)
{
    // Find which segment ptA and ptB lie on (take the first match in path order).
    int segA = 0; float tA = 0.0f;
    int segB = int(path.size()) - 2; float tB = 1.0f;
    for(int i = 0; i + 1 < int(path.size()); ++i)
    {
        if(pointToSegmentDist2(float(ptA.x), float(ptA.y),
                               float(path[i].x), float(path[i].y),
                               float(path[i + 1].x), float(path[i + 1].y)) < 1.0f)
        {
            segA = i;
            const float dx = float(path[i + 1].x - path[i].x), dy = float(path[i + 1].y - path[i].y);
            const float len2 = dx * dx + dy * dy;
            tA = len2 > 0.0f ? ((float(ptA.x - path[i].x)) * dx + (float(ptA.y - path[i].y)) * dy) / len2 : 0.0f;
            tA = std::max(0.0f, std::min(1.0f, tA));
            break;
        }
    }
    for(int i = 0; i + 1 < int(path.size()); ++i)
    {
        if(pointToSegmentDist2(float(ptB.x), float(ptB.y),
                               float(path[i].x), float(path[i].y),
                               float(path[i + 1].x), float(path[i + 1].y)) < 1.0f)
        {
            segB = i;
            const float dx = float(path[i + 1].x - path[i].x), dy = float(path[i + 1].y - path[i].y);
            const float len2 = dx * dx + dy * dy;
            tB = len2 > 0.0f ? ((float(ptB.x - path[i].x)) * dx + (float(ptB.y - path[i].y)) * dy) / len2 : 0.0f;
            tB = std::max(0.0f, std::min(1.0f, tB));
        }
    }
    std::vector<synth::GridPoint> result;
    result.push_back(ptA);
    for(int i = segA + 1; i <= segB; ++i)
        result.push_back(path[i]);
    if(!samePoint(result.back(), ptB)) result.push_back(ptB);
    return result;
}

// Builds a new wire path with the section [ptA..ptB] translated by (dx, dy).
inline std::vector<synth::GridPoint> computeMovedPath(const std::vector<synth::GridPoint> &origPath,
                                                const synth::GridPoint &ptA,
                                                const synth::GridPoint &ptB, int dx, int dy)
{
    const synth::GridPoint movedA { ptA.x + dx, ptA.y + dy };
    const synth::GridPoint movedB { ptB.x + dx, ptB.y + dy };

    const auto appendUniq = [](std::vector<synth::GridPoint> &v, const synth::GridPoint &p) {
        if(!v.empty() && samePoint(v.back(), p)) return;
        v.push_back(p);
    };

    std::vector<synth::GridPoint> result;
    // Prefix: path up to (and including) ptA.
    for(const auto &p : pathUpTo(origPath, ptA)) appendUniq(result, p);
    // ptA → movedA using 45° routing.
    { std::vector<synth::GridPoint> conn { ptA }; appendConstrainedPath(conn, movedA);
      for(size_t i = 1; i < conn.size(); ++i) appendUniq(result, conn[i]); }
    // Translated interior of the section (excluding ptA and ptB themselves).
    { const auto sec = extractSubpath(origPath, ptA, ptB);
      for(size_t i = 1; i + 1 < sec.size(); ++i)
          appendUniq(result, { sec[i].x + dx, sec[i].y + dy }); }
    appendUniq(result, movedB);
    // Suffix: skip ptB itself; connect movedB → successor of ptB → rest.
    // This avoids the "return leg" back to ptB which looks like an unmoved ghost segment.
    { const auto suf = pathFrom(origPath, ptB); // suf[0] == ptB, suf[1...] is real suffix
      if(suf.size() >= 2)
      {
          std::vector<synth::GridPoint> conn { movedB };
          appendConstrainedPath(conn, suf[1]);
          for(size_t i = 1; i < conn.size(); ++i) appendUniq(result, conn[i]);
          for(size_t i = 2; i < suf.size(); ++i) appendUniq(result, suf[i]);
      }
    }
    return result;
}
} // namespace routeui

END_NAMESPACE_DISTRHO
