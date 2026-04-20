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

// Apply a 3x3 row-major world-axis transform M to a world-space position.
std::array<float, 3> transformPosition(const std::array<float, 9>& M,
                                       const std::array<float, 3>& p) {
    return {
        M[0] * p[0] + M[1] * p[1] + M[2] * p[2],
        M[3] * p[0] + M[4] * p[1] + M[5] * p[2],
        M[6] * p[0] + M[7] * p[1] + M[8] * p[2],
    };
}

// Apply a 3x3 row-major world-axis transform M to a part's rotation matrix
// (also row-major). Result = M * R, where R's columns are the part's local
// axes in world space.
std::array<float, 9> transformRotation(const std::array<float, 9>& M,
                                       const std::array<float, 9>& R) {
    std::array<float, 9> out{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out[row * 3 + col] =
                M[row * 3 + 0] * R[0 * 3 + col] +
                M[row * 3 + 1] * R[1 * 3 + col] +
                M[row * 3 + 2] * R[2 * 3 + col];
        }
    }
    return out;
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
                           float scale,
                           const std::array<float, 9>& axisTransform) {
    auto bytes = reader_->read(inBsp);
    auto bsp   = parser_->parse(bytes);
    const auto worldspawn = worldspawn_->compute(*bsp);

    const auto& M = axisTransform;

    xml_->beginDocument();
    const int count = static_cast<int>(bsp->brushes.size());
    for (int i = 0; i < count; ++i) {
        if (worldspawn.count(i) == 0) continue;
        if (!filter_->keep(*bsp, i)) continue;

        const std::string brushName = "brush_" + std::to_string(i);
        if (auto wOpt = geometry_->brushWedge(*bsp, i)) {
            const BrushWedge& w = *wOpt;
            RobloxWedge wedge{};
            wedge.name  = brushName;
            wedge.color = colorFromTexname(w.texname);
            wedge.size     = { w.size[0] * scale, w.size[1] * scale, w.size[2] * scale };
            wedge.position = transformPosition(M,
                { w.center[0] * scale, w.center[1] * scale, w.center[2] * scale });
            wedge.rotation = transformRotation(M, w.rotation);
            xml_->emitWedge(wedge);
            continue;
        }

        auto emitDecomposition = [&](const BrushDecomposition& d) {
            const auto col = colorFromTexname(d.texname);
            int sub = 0;
            for (const BrushPiece& p : d.pieces) {
                const std::string subName = brushName + "_" + std::to_string(sub++);
                const std::array<float, 3> scaledCenter{
                    p.center[0] * scale, p.center[1] * scale, p.center[2] * scale };
                const std::array<float, 3> scaledSize{
                    p.size[0]   * scale, p.size[1]   * scale, p.size[2]   * scale };
                if (p.kind == BrushPiece::Kind::Part) {
                    RobloxPart part{};
                    part.name     = subName;
                    part.color    = col;
                    part.size     = scaledSize;
                    part.position = transformPosition(M, scaledCenter);
                    part.rotation = transformRotation(M, p.rotation);
                    xml_->emitPart(part);
                } else {
                    RobloxWedge wedge{};
                    wedge.name     = subName;
                    wedge.color    = col;
                    wedge.size     = scaledSize;
                    wedge.position = transformPosition(M, scaledCenter);
                    wedge.rotation = transformRotation(M, p.rotation);
                    xml_->emitWedge(wedge);
                }
            }
        };

        if (auto dOpt = geometry_->brushChamferedBeam(*bsp, i)) {
            emitDecomposition(*dOpt);
            continue;
        }

        if (auto dOpt = geometry_->brushCornerChamfer(*bsp, i)) {
            emitDecomposition(*dOpt);
            continue;
        }

        if (auto dOpt = geometry_->brushHexagonalFloor(*bsp, i)) {
            emitDecomposition(*dOpt);
            continue;
        }

        const BrushObb o = geometry_->brushObb(*bsp, i);
        RobloxPart part{};
        part.name     = brushName;
        part.color    = colorFromTexname(o.texname);
        part.size     = { o.size[0] * scale, o.size[1] * scale, o.size[2] * scale };
        part.position = transformPosition(M,
            { o.center[0] * scale, o.center[1] * scale, o.center[2] * scale });
        // Rotation matrix is unit-free — no scaling, just axis transform.
        part.rotation = transformRotation(M, o.rotation);
        xml_->emitPart(part);
    }
    const std::string xml = xml_->endDocument();
    writer_->write(outRbxlx, xml);
}

} // namespace bsp2rbx
