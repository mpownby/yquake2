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

struct RobloxPart {
    std::array<float, 3>   position;
    std::array<float, 3>   size;
    std::array<float, 9>   rotation;  // identity by default; row-major 3x3
    std::array<uint8_t, 3> color;
    std::string            name;
};

} // namespace bsp2rbx
