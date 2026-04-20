#pragma once

#include <array>
#include <vector>

#include "bsp2rbx/Bsp.h"

namespace bsp2rbx {

class IBrushGeometry {
public:
    virtual ~IBrushGeometry() = default;
    virtual std::vector<std::array<float, 3>>
        brushVertices(const Bsp& bsp, int brushIndex) = 0;
    virtual BrushAabb brushAabb(const Bsp& bsp, int brushIndex) = 0;
    // Oriented bounding box. For brushes whose faces include 3 mutually
    // orthogonal plane normals (i.e. "tilted boxes"), the OBB is aligned
    // with those face normals — giving a correct fit for rotated wall
    // segments, trim, non-axis-aligned doorways, etc. For degenerate
    // brushes (no orthogonal triple of faces), falls back to the
    // axis-aligned AABB with identity rotation.
    virtual BrushObb  brushObb (const Bsp& bsp, int brushIndex) = 0;
};

} // namespace bsp2rbx
