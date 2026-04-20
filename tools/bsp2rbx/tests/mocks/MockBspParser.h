#pragma once

#include <gmock/gmock.h>

#include "bsp2rbx/IBspParser.h"

namespace bsp2rbx {

class MockBspParser : public IBspParser {
public:
    MOCK_METHOD(std::unique_ptr<Bsp>, parse,
                (const std::vector<uint8_t>& bytes),
                (override));
};

} // namespace bsp2rbx
