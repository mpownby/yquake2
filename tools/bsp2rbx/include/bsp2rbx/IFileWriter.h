#pragma once

#include <filesystem>
#include <string_view>

namespace bsp2rbx {

class IFileWriter {
public:
    virtual ~IFileWriter() = default;
    virtual void write(const std::filesystem::path& path, std::string_view content) = 0;
};

} // namespace bsp2rbx
