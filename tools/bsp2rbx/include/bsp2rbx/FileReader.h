#pragma once

#include "bsp2rbx/IFileReader.h"

namespace bsp2rbx {

class FileReader : public IFileReader {
public:
    std::vector<uint8_t> read(const std::filesystem::path& path) override;
};

} // namespace bsp2rbx
