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
    std::string               entityString;
};

struct BrushAabb {
    std::array<float, 3>  mins;
    std::array<float, 3>  maxs;
    int                   modelIndex;
    std::string           texname;
};

struct RobloxPart {
    std::array<float, 3>   position;
    std::array<float, 3>   size;
    std::array<uint8_t, 3> color;
    std::string            name;
};

} // namespace bsp2rbx
