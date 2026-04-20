#pragma once

#include <array>
#include <optional>
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
    // Right-angled triangular prism, oriented for emission as a Roblox
    // WedgePart. Returns nullopt when the brush isn't a triangular prism
    // with a right-angle leg — caller falls back to brushObb.
    virtual std::optional<BrushWedge>
        brushWedge(const Bsp& bsp, int brushIndex) = 0;
    // Pentagonal-prism brush (a box with one edge chamfered) decomposed
    // into two axis-aligned boxes plus one right-angled wedge filling the
    // chamfer slope. Returns nullopt for anything that isn't a pentagonal
    // prism with three right-angle corners and one chamfer edge.
    virtual std::optional<BrushDecomposition>
        brushChamferedBeam(const Bsp& bsp, int brushIndex) = 0;
};

} // namespace bsp2rbx
