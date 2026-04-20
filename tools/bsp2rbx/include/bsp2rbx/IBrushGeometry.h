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
};

} // namespace bsp2rbx
