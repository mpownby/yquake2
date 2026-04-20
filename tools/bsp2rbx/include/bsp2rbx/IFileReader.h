#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace bsp2rbx {

class IFileReader {
public:
    virtual ~IFileReader() = default;
    virtual std::vector<uint8_t> read(const std::filesystem::path& path) = 0;
};

} // namespace bsp2rbx
