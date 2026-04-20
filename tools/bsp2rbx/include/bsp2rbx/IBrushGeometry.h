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
    // Corner-chamfer brush — a box with a single plane cutting off one
    // vertex (10 hull verts: 7 original cube corners + 3 chamfer cut
    // points; 7 faces: 3 pentagons + 3 rectangles + 1 triangle). Returned
    // decomposition tiles the un-chopped 7/8 of the bbox with three
    // axis-aligned Parts and approximates the chopped corner sub-box with
    // one WedgePart whose hypotenuse aligns with the chamfer plane along
    // its longest leg. The approximation under-fills the chopped sub-box
    // by up to ~1/6 of its volume — a small triangular notch — but
    // captures the chamfer slope direction. Returns nullopt if the brush
    // isn't a corner-chamfer.
    virtual std::optional<BrushDecomposition>
        brushCornerChamfer(const Bsp& bsp, int brushIndex) = 0;
    // Hexagonal-floor brush — a box with two adjacent corners on the same
    // side clipped by vertical chamfer planes, producing a hexagonal top
    // face (4 axis-aligned edges + 2 diagonal edges). Additional cuts on
    // other faces (e.g. a sloped bottom) are ignored: the brush is
    // approximated as a vertical hexagonal prism extruded from AABB minZ
    // to the top face's Z. Decomposition = 2 axis-aligned Parts + 2
    // WedgeParts filling the chamfer corners. Returns nullopt when the
    // top face is not a double-chamfered hexagon.
    virtual std::optional<BrushDecomposition>
        brushHexagonalFloor(const Bsp& bsp, int brushIndex) = 0;
};

} // namespace bsp2rbx
