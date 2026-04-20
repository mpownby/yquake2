#pragma once

#include "bsp2rbx/IBrushFilter.h"

namespace bsp2rbx {

class SolidWorldspawnFilter : public IBrushFilter {
public:
    bool keep(const Bsp& bsp, int brushIndex) override;
};

} // namespace bsp2rbx
