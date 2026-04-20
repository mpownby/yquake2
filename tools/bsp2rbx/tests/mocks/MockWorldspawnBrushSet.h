#pragma once

#include <gmock/gmock.h>

#include "bsp2rbx/IWorldspawnBrushSet.h"

namespace bsp2rbx {

class MockWorldspawnBrushSet : public IWorldspawnBrushSet {
public:
    MOCK_METHOD(std::unordered_set<int>, compute, (const Bsp& bsp), (override));
};

} // namespace bsp2rbx
