#include "bsp2rbx/FileWriter.h"

#include <fstream>
#include <stdexcept>

namespace bsp2rbx {

void FileWriter::write(const std::filesystem::path& path, std::string_view content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("FileWriter: cannot open " + path.string());
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
        throw std::runtime_error("FileWriter: write failed for " + path.string());
    }
}

} // namespace bsp2rbx
