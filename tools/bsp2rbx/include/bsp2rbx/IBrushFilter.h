#pragma once

#include "bsp2rbx/Bsp.h"

namespace bsp2rbx {

class IBrushFilter {
public:
    virtual ~IBrushFilter() = default;
    virtual bool keep(const Bsp& bsp, int brushIndex) = 0;
};

} // namespace bsp2rbx
