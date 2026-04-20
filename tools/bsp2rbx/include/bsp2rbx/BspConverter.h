#pragma once

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
    BspConverter(std::shared_ptr<IFileReader>         reader,
                 std::shared_ptr<IBspParser>          parser,
                 std::shared_ptr<IWorldspawnBrushSet> worldspawn,
                 std::shared_ptr<IBrushGeometry>      geometry,
                 std::shared_ptr<IBrushFilter>        filter,
                 std::shared_ptr<IRobloxXmlWriter>    xml,
                 std::shared_ptr<IFileWriter>         writer);

    void convert(const std::filesystem::path& inBsp,
                 const std::filesystem::path& outRbxlx,
                 float scale);

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
