#pragma once

#include <gmock/gmock.h>

#include "bsp2rbx/IFileReader.h"

namespace bsp2rbx {

class MockFileReader : public IFileReader {
public:
    MOCK_METHOD(std::vector<uint8_t>, read, (const std::filesystem::path& path), (override));
};

} // namespace bsp2rbx
