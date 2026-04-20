#pragma once

#include "bsp2rbx/IFileWriter.h"

namespace bsp2rbx {

class FileWriter : public IFileWriter {
public:
    void write(const std::filesystem::path& path, std::string_view content) override;
};

} // namespace bsp2rbx
