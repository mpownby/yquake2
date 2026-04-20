#pragma once

#include <array>
#include <filesystem>
#include <memory>

#include "bsp2rbx/IBrushFilter.h"
#include "bsp2rbx/IBrushGeometry.h"
#include "bsp2rbx/IBspParser.h"
#include "bsp2rbx/IFileReader.h"
#include "bsp2rbx/IFileWriter.h"
#include "bsp2rbx/IRobloxXmlWriter.h"
#include "bsp2rbx/IWorldspawnBrushSet.h"

namespace bsp2rbx {

class BspConverter {
public:
    // 3x3 row-major rotation applied to every emitted position and rotation
    // matrix, mapping BSP-space coordinates into Roblox-space. Quake 2 is
    // Z-up; Roblox is Y-up. The default here is identity (for unit tests that
    // want to check raw BSP-frame values); production main() passes the
    // Q2 -> Roblox 90° rotation about +X so the map stands upright in Studio.
    static constexpr std::array<float, 9> kIdentityAxisTransform = {
        1, 0, 0,  0, 1, 0,  0, 0, 1
    };
    static constexpr std::array<float, 9> kQ2ToRobloxAxisTransform = {
        1,  0,  0,
        0,  0,  1,
        0, -1,  0
    };

    BspConverter(std::shared_ptr<IFileReader>         reader,
                 std::shared_ptr<IBspParser>          parser,
                 std::shared_ptr<IWorldspawnBrushSet> worldspawn,
                 std::shared_ptr<IBrushGeometry>      geometry,
                 std::shared_ptr<IBrushFilter>        filter,
                 std::shared_ptr<IRobloxXmlWriter>    xml,
                 std::shared_ptr<IFileWriter>         writer);

    void convert(const std::filesystem::path& inBsp,
                 const std::filesystem::path& outRbxlx,
                 float scale,
                 const std::array<float, 9>& axisTransform = kIdentityAxisTransform);

private:
    std::shared_ptr<IFileReader>         reader_;
    std::shared_ptr<IBspParser>          parser_;
    std::shared_ptr<IWorldspawnBrushSet> worldspawn_;
    std::shared_ptr<IBrushGeometry>      geometry_;
    std::shared_ptr<IBrushFilter>        filter_;
    std::shared_ptr<IRobloxXmlWriter>    xml_;
    std::shared_ptr<IFileWriter>         writer_;
};

} // namespace bsp2rbx
