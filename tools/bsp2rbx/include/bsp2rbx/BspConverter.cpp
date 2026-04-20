#include "bsp2rbx/BspConverter.h"

#include <cstdint>
#include <string>

namespace bsp2rbx {

namespace {

std::array<uint8_t, 3> colorFromTexname(const std::string& name) {
    uint32_t h = 2166136261u;
    for (char c : name) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    if (name.empty()) h = 0x888888;
    uint8_t r = static_cast<uint8_t>(64 + ((h >> 16) & 0x7F));
    uint8_t g = static_cast<uint8_t>(64 + ((h >>  8) & 0x7F));
    uint8_t b = static_cast<uint8_t>(64 + ( h        & 0x7F));
    return { r, g, b };
}

} // namespace

BspConverter::BspConverter(std::shared_ptr<IFileReader>         reader,
                           std::shared_ptr<IBspParser>          parser,
                           std::shared_ptr<IWorldspawnBrushSet> worldspawn,
                           std::shared_ptr<IBrushGeometry>      geometry,
                           std::shared_ptr<IBrushFilter>        filter,
                           std::shared_ptr<IRobloxXmlWriter>    xml,
                           std::shared_ptr<IFileWriter>         writer)
    : reader_(std::move(reader)),
      parser_(std::move(parser)),
      worldspawn_(std::move(worldspawn)),
      geometry_(std::move(geometry)),
      filter_(std::move(filter)),
      xml_(std::move(xml)),
      writer_(std::move(writer)) {}

void BspConverter::convert(const std::filesystem::path& inBsp,
                           const std::filesystem::path& outRbxlx,
                           float scale) {
    auto bytes = reader_->read(inBsp);
    auto bsp   = parser_->parse(bytes);
    const auto worldspawn = worldspawn_->compute(*bsp);

    xml_->beginDocument();
    const int count = static_cast<int>(bsp->brushes.size());
    for (int i = 0; i < count; ++i) {
        if (worldspawn.count(i) == 0) continue;
        if (!filter_->keep(*bsp, i)) continue;
        const BrushObb o = geometry_->brushObb(*bsp, i);
        RobloxPart part{};
        part.name = "brush_" + std::to_string(i);
        part.color = colorFromTexname(o.texname);
        for (int c = 0; c < 3; ++c) {
            part.size[c]     = o.size[c]   * scale;
            part.position[c] = o.center[c] * scale;
        }
        // Rotation matrix is unit-free — no scaling.
        part.rotation = o.rotation;
        xml_->emitPart(part);
    }
    const std::string xml = xml_->endDocument();
    writer_->write(outRbxlx, xml);
}

} // namespace bsp2rbx
