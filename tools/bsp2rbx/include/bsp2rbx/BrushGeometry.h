#pragma once

#include "bsp2rbx/IBrushGeometry.h"

namespace bsp2rbx {

class BrushGeometry : public IBrushGeometry {
public:
    std::vector<std::array<float, 3>>
        brushVertices(const Bsp& bsp, int brushIndex) override;
    BrushAabb brushAabb(const Bsp& bsp, int brushIndex) override;
    BrushObb  brushObb (const Bsp& bsp, int brushIndex) override;
    std::optional<BrushWedge> brushWedge(const Bsp& bsp, int brushIndex) override;
    std::optional<BrushDecomposition>
        brushChamferedBeam(const Bsp& bsp, int brushIndex) override;
    std::optional<BrushDecomposition>
        brushCornerChamfer(const Bsp& bsp, int brushIndex) override;
};

} // namespace bsp2rbx
