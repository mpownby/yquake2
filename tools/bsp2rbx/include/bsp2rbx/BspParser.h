#pragma once

#include "bsp2rbx/IBspParser.h"

namespace bsp2rbx {

class BspParser : public IBspParser {
public:
    std::unique_ptr<Bsp> parse(const std::vector<uint8_t>& bytes) override;
};

} // namespace bsp2rbx
