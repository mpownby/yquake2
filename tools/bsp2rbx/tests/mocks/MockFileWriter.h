#pragma once

#include <gmock/gmock.h>

#include "bsp2rbx/IFileWriter.h"

namespace bsp2rbx {

class MockFileWriter : public IFileWriter {
public:
    MOCK_METHOD(void, write,
                (const std::filesystem::path& path, std::string_view content),
                (override));
};

} // namespace bsp2rbx
