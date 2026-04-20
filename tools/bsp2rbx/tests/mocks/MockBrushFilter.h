#pragma once

#include <gmock/gmock.h>

#include "bsp2rbx/IBrushFilter.h"

namespace bsp2rbx {

class MockBrushFilter : public IBrushFilter {
public:
    MOCK_METHOD(bool, keep, (const Bsp& bsp, int brushIndex), (override));
};

} // namespace bsp2rbx
