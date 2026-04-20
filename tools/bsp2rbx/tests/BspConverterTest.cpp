#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "bsp2rbx/BspConverter.h"
#include "mocks/MockBrushFilter.h"
#include "mocks/MockBrushGeometry.h"
#include "mocks/MockBspParser.h"
#include "mocks/MockFileReader.h"
#include "mocks/MockFileWriter.h"
#include "mocks/MockRobloxXmlWriter.h"
#include "mocks/MockWorldspawnBrushSet.h"

namespace bsp2rbx {
namespace {

using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Ref;
using ::testing::Return;

TEST(BspConverterTest, ReadsParsesIteratesAndWrites) {
    auto reader    = std::make_shared<MockFileReader>();
    auto parser    = std::make_shared<MockBspParser>();
    auto wsBrushes = std::make_shared<MockWorldspawnBrushSet>();
    auto geom      = std::make_shared<MockBrushGeometry>();
    auto filter    = std::make_shared<MockBrushFilter>();
    auto xml       = std::make_shared<MockRobloxXmlWriter>();
    auto writer    = std::make_shared<MockFileWriter>();

    const std::vector<uint8_t> bytes{0x49, 0x42, 0x53, 0x50};
    auto bsp = std::make_unique<Bsp>();
    bsp->brushes.resize(2);
    Bsp* bspPtr = bsp.get();

    EXPECT_CALL(*reader, read(std::filesystem::path("in.bsp")))
        .WillOnce(Return(bytes));
    EXPECT_CALL(*parser, parse(Eq(bytes)))
        .WillOnce(Return(ByMove(std::move(bsp))));
    EXPECT_CALL(*wsBrushes, compute(Ref(*bspPtr)))
        .WillOnce(Return(std::unordered_set<int>{0, 1}));

    EXPECT_CALL(*filter, keep(Ref(*bspPtr), 0)).WillOnce(Return(true));
    EXPECT_CALL(*filter, keep(Ref(*bspPtr), 1)).WillOnce(Return(false));

    BrushAabb aabb{};
    aabb.mins = { 0.0f, 0.0f, 0.0f };
    aabb.maxs = { 2.0f, 4.0f, 6.0f };
    aabb.modelIndex = 0;
    aabb.texname = "walls/m1_1";
    EXPECT_CALL(*geom, brushAabb(Ref(*bspPtr), 0)).WillOnce(Return(aabb));

    {
        InSequence s;
        EXPECT_CALL(*xml, beginDocument());
        EXPECT_CALL(*xml, emitPart(Field(&RobloxPart::name, Eq("brush_0"))));
        EXPECT_CALL(*xml, endDocument()).WillOnce(Return(std::string("<roblox/>")));
    }

    EXPECT_CALL(*writer,
                write(std::filesystem::path("out.rbxlx"),
                      std::string_view("<roblox/>")));

    BspConverter c(reader, parser, wsBrushes, geom, filter, xml, writer);
    c.convert("in.bsp", "out.rbxlx", 1.0f);
}

TEST(BspConverterTest, AppliesScaleToPosAndSize) {
    auto reader    = std::make_shared<MockFileReader>();
    auto parser    = std::make_shared<MockBspParser>();
    auto wsBrushes = std::make_shared<MockWorldspawnBrushSet>();
    auto geom      = std::make_shared<MockBrushGeometry>();
    auto filter    = std::make_shared<MockBrushFilter>();
    auto xml       = std::make_shared<MockRobloxXmlWriter>();
    auto writer    = std::make_shared<MockFileWriter>();

    auto bsp = std::make_unique<Bsp>();
    bsp->brushes.resize(1);
    Bsp* bspPtr = bsp.get();

    EXPECT_CALL(*reader, read(std::filesystem::path("i")))
        .WillOnce(Return(std::vector<uint8_t>{}));
    EXPECT_CALL(*parser, parse(Eq(std::vector<uint8_t>{})))
        .WillOnce(Return(ByMove(std::move(bsp))));
    EXPECT_CALL(*wsBrushes, compute(Ref(*bspPtr)))
        .WillOnce(Return(std::unordered_set<int>{0}));
    EXPECT_CALL(*filter, keep(Ref(*bspPtr), 0)).WillOnce(Return(true));

    BrushAabb aabb{};
    aabb.mins = { 0.0f, 0.0f, 0.0f };
    aabb.maxs = { 100.0f, 200.0f, 400.0f };
    EXPECT_CALL(*geom, brushAabb(Ref(*bspPtr), 0)).WillOnce(Return(aabb));

    EXPECT_CALL(*xml, beginDocument());
    EXPECT_CALL(*xml, emitPart(Field(&RobloxPart::size,
                                     Eq(std::array<float, 3>{10.0f, 20.0f, 40.0f}))));
    EXPECT_CALL(*xml, endDocument()).WillOnce(Return(std::string()));
    EXPECT_CALL(*writer, write(std::filesystem::path("o"), std::string_view()));

    BspConverter c(reader, parser, wsBrushes, geom, filter, xml, writer);
    c.convert("i", "o", 0.1f);
}

TEST(BspConverterTest, NoBrushesJustEmitsEmptyModel) {
    auto reader    = std::make_shared<MockFileReader>();
    auto parser    = std::make_shared<MockBspParser>();
    auto wsBrushes = std::make_shared<MockWorldspawnBrushSet>();
    auto geom      = std::make_shared<MockBrushGeometry>();
    auto filter    = std::make_shared<MockBrushFilter>();
    auto xml       = std::make_shared<MockRobloxXmlWriter>();
    auto writer    = std::make_shared<MockFileWriter>();

    auto bsp = std::make_unique<Bsp>();
    Bsp* bspPtr = bsp.get();

    EXPECT_CALL(*reader, read(std::filesystem::path("i")))
        .WillOnce(Return(std::vector<uint8_t>{}));
    EXPECT_CALL(*parser, parse(Eq(std::vector<uint8_t>{})))
        .WillOnce(Return(ByMove(std::move(bsp))));
    EXPECT_CALL(*wsBrushes, compute(Ref(*bspPtr)))
        .WillOnce(Return(std::unordered_set<int>{}));
    EXPECT_CALL(*xml, beginDocument());
    EXPECT_CALL(*xml, endDocument()).WillOnce(Return(std::string("x")));
    EXPECT_CALL(*writer, write(std::filesystem::path("o"), std::string_view("x")));

    BspConverter c(reader, parser, wsBrushes, geom, filter, xml, writer);
    c.convert("i", "o", 1.0f);
}

TEST(BspConverterTest, SkipsBrushesNotInWorldspawnSetEvenIfFilterWouldKeep) {
    // Regression: brush-entity brushes (doors, movers) live at the end of the
    // brushes lump with CONTENTS_SOLID and visible texnames, so the content
    // filter passes them. They must be rejected because they don't belong to
    // model 0. If this test fails, the converter will re-emit compiled-in-place
    // doors as static walls in the Roblox output.
    auto reader    = std::make_shared<MockFileReader>();
    auto parser    = std::make_shared<MockBspParser>();
    auto wsBrushes = std::make_shared<MockWorldspawnBrushSet>();
    auto geom      = std::make_shared<MockBrushGeometry>();
    auto filter    = std::make_shared<MockBrushFilter>();
    auto xml       = std::make_shared<MockRobloxXmlWriter>();
    auto writer    = std::make_shared<MockFileWriter>();

    auto bsp = std::make_unique<Bsp>();
    bsp->brushes.resize(3);
    Bsp* bspPtr = bsp.get();

    EXPECT_CALL(*reader, read(std::filesystem::path("i")))
        .WillOnce(Return(std::vector<uint8_t>{}));
    EXPECT_CALL(*parser, parse(Eq(std::vector<uint8_t>{})))
        .WillOnce(Return(ByMove(std::move(bsp))));
    // Only brushes 0 and 2 belong to worldspawn. Brush 1 is a brush-entity
    // (e.g. a door) and must be skipped before even consulting the filter.
    EXPECT_CALL(*wsBrushes, compute(Ref(*bspPtr)))
        .WillOnce(Return(std::unordered_set<int>{0, 2}));

    EXPECT_CALL(*filter, keep(Ref(*bspPtr), 0)).WillOnce(Return(true));
    EXPECT_CALL(*filter, keep(Ref(*bspPtr), 2)).WillOnce(Return(true));
    // filter->keep(_, 1) is NOT called (strict mock) — wsBrushes excluded it.

    BrushAabb a0{};  a0.maxs = { 1.0f, 1.0f, 1.0f }; a0.texname = "walls/a";
    BrushAabb a2{};  a2.maxs = { 2.0f, 2.0f, 2.0f }; a2.texname = "walls/b";
    EXPECT_CALL(*geom, brushAabb(Ref(*bspPtr), 0)).WillOnce(Return(a0));
    EXPECT_CALL(*geom, brushAabb(Ref(*bspPtr), 2)).WillOnce(Return(a2));

    {
        InSequence s;
        EXPECT_CALL(*xml, beginDocument());
        EXPECT_CALL(*xml, emitPart(Field(&RobloxPart::name, Eq("brush_0"))));
        EXPECT_CALL(*xml, emitPart(Field(&RobloxPart::name, Eq("brush_2"))));
        EXPECT_CALL(*xml, endDocument()).WillOnce(Return(std::string()));
    }
    EXPECT_CALL(*writer, write(std::filesystem::path("o"), std::string_view()));

    BspConverter c(reader, parser, wsBrushes, geom, filter, xml, writer);
    c.convert("i", "o", 1.0f);
}

} // namespace
} // namespace bsp2rbx
