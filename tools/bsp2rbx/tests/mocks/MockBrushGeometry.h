#pragma once

#include <gmock/gmock.h>

#include "bsp2rbx/IBrushGeometry.h"

namespace bsp2rbx {

class MockBrushGeometry : public IBrushGeometry {
public:
    MOCK_METHOD((std::vector<std::array<float, 3>>), brushVertices,
                (const Bsp& bsp, int brushIndex), (override));
    MOCK_METHOD(BrushAabb, brushAabb,
                (const Bsp& bsp, int brushIndex), (override));
    MOCK_METHOD(BrushObb, brushObb,
                (const Bsp& bsp, int brushIndex), (override));
    MOCK_METHOD(std::optional<BrushWedge>, brushWedge,
                (const Bsp& bsp, int brushIndex), (override));
    MOCK_METHOD(std::optional<BrushDecomposition>, brushChamferedBeam,
                (const Bsp& bsp, int brushIndex), (override));
    MOCK_METHOD(std::optional<BrushDecomposition>, brushCornerChamfer,
                (const Bsp& bsp, int brushIndex), (override));
    MOCK_METHOD(std::optional<BrushDecomposition>, brushHexagonalFloor,
                (const Bsp& bsp, int brushIndex), (override));
    MOCK_METHOD(std::optional<BrushDecomposition>, brushBeveledBottomBrick,
                (const Bsp& bsp, int brushIndex), (override));
};

} // namespace bsp2rbx
