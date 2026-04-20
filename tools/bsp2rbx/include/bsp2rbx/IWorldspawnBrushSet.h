#pragma once

#include <unordered_set>

#include "bsp2rbx/Bsp.h"

namespace bsp2rbx {

// Computes the set of brush indices reachable from model 0 (worldspawn) of a
// parsed BSP, by walking its node tree and collecting leafbrush references.
//
// Brush indices in models 1+ belong to brush-entities (doors, buttons, movers)
// and must not be emitted as static world geometry: their compiled-in-place
// position rarely matches where they sit at runtime, and they represent
// dynamic objects, not walls.
class IWorldspawnBrushSet {
public:
    virtual ~IWorldspawnBrushSet() = default;

    // Returns the set of brush indices belonging to model 0. Implementations
    // must not throw on malformed input — out-of-range node/leaf/brush refs
    // should be silently skipped so one bad BSP does not crash the converter.
    virtual std::unordered_set<int> compute(const Bsp& bsp) = 0;
};

} // namespace bsp2rbx
