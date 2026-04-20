#include "bsp2rbx/FileReader.h"

#include <fstream>
#include <stdexcept>

namespace bsp2rbx {

std::vector<uint8_t> FileReader::read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error("FileReader: cannot open " + path.string());
    }
    const std::streamsize size = in.tellg();
    if (size < 0) {
        throw std::runtime_error("FileReader: tellg failed for " + path.string());
    }
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error("FileReader: read failed for " + path.string());
    }
    return bytes;
}

} // namespace bsp2rbx
