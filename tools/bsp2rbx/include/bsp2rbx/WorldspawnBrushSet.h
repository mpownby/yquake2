#pragma once

#include "bsp2rbx/IWorldspawnBrushSet.h"

namespace bsp2rbx {

class WorldspawnBrushSet : public IWorldspawnBrushSet {
public:
    std::unordered_set<int> compute(const Bsp& bsp) override;
};

} // namespace bsp2rbx
