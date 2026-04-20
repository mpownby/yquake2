#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "bsp2rbx/Bsp.h"

namespace bsp2rbx {

class IBspParser {
public:
    virtual ~IBspParser() = default;
    virtual std::unique_ptr<Bsp> parse(const std::vector<uint8_t>& bytes) = 0;
};

} // namespace bsp2rbx
