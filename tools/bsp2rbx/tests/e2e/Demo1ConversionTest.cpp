#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

#include "bsp2rbx/BspConverter.h"
#include "bsp2rbx/BspParser.h"
#include "bsp2rbx/BrushGeometry.h"
#include "bsp2rbx/FileReader.h"
#include "bsp2rbx/FileWriter.h"
#include "bsp2rbx/RobloxXmlWriter.h"
#include "bsp2rbx/SolidWorldspawnFilter.h"
#include "bsp2rbx/WorldspawnBrushSet.h"

namespace bsp2rbx {
namespace {

TEST(Demo1ConversionTest, ConvertsDemo1WhenBaseq2Available) {
    const char* base = std::getenv("YQ2_TEST_BASEQ2");
    if (!base || !*base) {
        GTEST_SKIP() << "YQ2_TEST_BASEQ2 not set";
    }
    const std::filesystem::path bsp = std::filesystem::path(base) / "maps" / "demo1.bsp";
    if (!std::filesystem::exists(bsp)) {
        GTEST_SKIP() << "demo1.bsp not found at " << bsp.string();
    }
    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "bsp2rbx_demo1.rbxlx";

    auto reader     = std::make_shared<FileReader>();
    auto parser     = std::make_shared<BspParser>();
    auto worldspawn = std::make_shared<WorldspawnBrushSet>();
    auto geom       = std::make_shared<BrushGeometry>();
    auto filter     = std::make_shared<SolidWorldspawnFilter>();
    auto xml        = std::make_shared<RobloxXmlWriter>();
    auto writer     = std::make_shared<FileWriter>();

    BspConverter c(reader, parser, worldspawn, geom, filter, xml, writer);
    c.convert(bsp, out, 1.0f / 12.0f);

    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 100u);
}

} // namespace
} // namespace bsp2rbx
