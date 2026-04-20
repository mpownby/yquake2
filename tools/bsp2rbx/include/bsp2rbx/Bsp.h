#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

typedef unsigned char byte;
#include "files.h"

namespace bsp2rbx {

struct Bsp {
    std::vector<dvertex_t>    vertices;
    std::vector<dplane_t>     planes;
    std::vector<dbrush_t>     brushes;
    std::vector<dbrushside_t> brushsides;
    std::vector<dmodel_t>     models;
    std::vector<dface_t>      faces;
    std::vector<dedge_t>      edges;
    std::vector<int>          surfedges;
    std::vector<texinfo_t>    texinfos;
    std::vector<dnode_t>      nodes;
    std::vector<dleaf_t>      leaves;
    std::vector<uint16_t>     leafbrushes;
    std::string               entityString;
};

struct BrushAabb {
    std::array<float, 3>  mins;
    std::array<float, 3>  maxs;
    int                   modelIndex;
    std::string           texname;
};

// Oriented bounding box for a brush. The rotation is a row-major 3x3 matrix
// whose columns are the world-space directions of the box's local +x, +y, +z
// axes (unit vectors). The box has `size[k]` extent along its local k-th axis,
// centered at `center` in world space.
//
// For an axis-aligned brush, `rotation` is the identity and `size` is the
// AABB extent — so BrushObb degenerates to BrushAabb for common cases.
struct BrushObb {
    std::array<float, 3>  center;
    std::array<float, 3>  size;
    std::array<float, 9>  rotation;  // R00 R01 R02 R10 R11 R12 R20 R21 R22
    int                   modelIndex;
    std::string           texname;
};

// Right-angled triangular prism brush, ready to emit as a Roblox WedgePart.
// Convention matches WedgePart's local geometry:
//   - prism axis  -> local +X (length `size[0]`)
//   - one leg     -> local +Y (length `size[1]`)
//   - other leg   -> local +Z (length `size[2]`)
//   - the right-angle vertex on the -X triangle face is at local
//     (-X/2, -Y/2, -Z/2); the chopped-off corner of the bounding box is at
//     (+Y/2, +Z/2) on each ±X face.
// `rotation` columns are the world-space directions of the local +X, +Y, +Z
// axes, forming a right-handed orthonormal basis.
struct BrushWedge {
    std::array<float, 3>  center;
    std::array<float, 3>  size;
    std::array<float, 9>  rotation;
    int                   modelIndex;
    std::string           texname;
};

struct RobloxPart {
    std::array<float, 3>   position;
    std::array<float, 3>   size;
    std::array<float, 9>   rotation;  // identity by default; row-major 3x3
    std::array<uint8_t, 3> color;
    std::string            name;
};

// Same payload as RobloxPart; kept distinct for type-safe routing through
// IRobloxXmlWriter::emitWedge so callers cannot accidentally emit a wedge as
// a plain Part (they have different default geometry in Studio).
struct RobloxWedge {
    std::array<float, 3>   position;
    std::array<float, 3>   size;
    std::array<float, 9>   rotation;
    std::array<uint8_t, 3> color;
    std::string            name;
};

// One piece of a multi-instance decomposition of a brush. `kind` chooses
// whether the converter emits a Roblox Part or WedgePart for this piece.
// Coordinates are in Q2 units (the converter scales when emitting).
struct BrushPiece {
    enum class Kind { Part, Wedge };
    std::array<float, 3>  center;
    std::array<float, 3>  size;
    std::array<float, 9>  rotation;
    Kind                  kind;
};

// Multi-instance decomposition of a brush whose convex hull is *not* a single
// box or wedge — typically a chamfered beam (pentagonal-prism cross-section)
// returned as 2 boxes + 1 wedge. All pieces share the brush's texture.
struct BrushDecomposition {
    std::vector<BrushPiece>  pieces;
    int                      modelIndex;
    std::string              texname;
};

} // namespace bsp2rbx
