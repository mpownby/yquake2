#include <gtest/gtest.h>

#include <cstring>

#include "bsp2rbx/BspParser.h"

namespace bsp2rbx {
namespace {

class BspParserTest : public ::testing::Test {
protected:
    // Build a minimal valid BSP byte buffer with one vertex, one plane, one brush,
    // one brushside, and an entities string.
    std::vector<uint8_t> buildMinimalBsp() {
        std::vector<uint8_t> buf;
        buf.resize(sizeof(dheader_t));

        dheader_t hdr{};
        hdr.ident   = IDBSPHEADER;
        hdr.version = BSPVERSION;

        auto appendLump = [&](int lumpId, const void* data, size_t len) {
            const size_t ofs = buf.size();
            buf.insert(buf.end(),
                       static_cast<const uint8_t*>(data),
                       static_cast<const uint8_t*>(data) + len);
            hdr.lumps[lumpId].fileofs = static_cast<int>(ofs);
            hdr.lumps[lumpId].filelen = static_cast<int>(len);
        };

        dvertex_t v{}; v.point[0] = 1.5f; v.point[1] = 2.5f; v.point[2] = 3.5f;
        appendLump(LUMP_VERTEXES, &v, sizeof(v));

        dplane_t p{}; p.normal[0] = 1.0f; p.dist = 64.0f; p.type = PLANE_X;
        appendLump(LUMP_PLANES, &p, sizeof(p));

        dbrush_t b{}; b.firstside = 0; b.numsides = 1; b.contents = CONTENTS_SOLID;
        appendLump(LUMP_BRUSHES, &b, sizeof(b));

        dbrushside_t bs{}; bs.planenum = 0; bs.texinfo = 0;
        appendLump(LUMP_BRUSHSIDES, &bs, sizeof(bs));

        const char entities[] = "{ \"classname\" \"worldspawn\" }";
        appendLump(LUMP_ENTITIES, entities, sizeof(entities) - 1);

        std::memcpy(buf.data(), &hdr, sizeof(hdr));
        return buf;
    }
};

TEST_F(BspParserTest, ThrowsOnTooSmall) {
    BspParser parser;
    EXPECT_THROW(parser.parse(std::vector<uint8_t>(10)), std::runtime_error);
}

TEST_F(BspParserTest, ThrowsOnBadMagic) {
    std::vector<uint8_t> buf(sizeof(dheader_t));
    dheader_t hdr{};
    hdr.ident = 0xDEADBEEF;
    hdr.version = BSPVERSION;
    std::memcpy(buf.data(), &hdr, sizeof(hdr));

    BspParser parser;
    EXPECT_THROW(parser.parse(buf), std::runtime_error);
}

TEST_F(BspParserTest, ThrowsOnBadVersion) {
    std::vector<uint8_t> buf(sizeof(dheader_t));
    dheader_t hdr{};
    hdr.ident = IDBSPHEADER;
    hdr.version = 99;
    std::memcpy(buf.data(), &hdr, sizeof(hdr));

    BspParser parser;
    EXPECT_THROW(parser.parse(buf), std::runtime_error);
}

TEST_F(BspParserTest, ParsesMinimalBsp) {
    auto buf = buildMinimalBsp();
    BspParser parser;
    auto bsp = parser.parse(buf);

    ASSERT_NE(bsp, nullptr);
    ASSERT_EQ(bsp->vertices.size(), 1u);
    EXPECT_FLOAT_EQ(bsp->vertices[0].point[0], 1.5f);
    EXPECT_FLOAT_EQ(bsp->vertices[0].point[1], 2.5f);
    EXPECT_FLOAT_EQ(bsp->vertices[0].point[2], 3.5f);

    ASSERT_EQ(bsp->planes.size(), 1u);
    EXPECT_FLOAT_EQ(bsp->planes[0].dist, 64.0f);

    ASSERT_EQ(bsp->brushes.size(), 1u);
    EXPECT_EQ(bsp->brushes[0].contents, CONTENTS_SOLID);
    EXPECT_EQ(bsp->brushes[0].numsides, 1);

    ASSERT_EQ(bsp->brushsides.size(), 1u);
    EXPECT_EQ(bsp->brushsides[0].planenum, 0);

    EXPECT_EQ(bsp->entityString, "{ \"classname\" \"worldspawn\" }");
}

TEST_F(BspParserTest, ThrowsOnLumpSizeMismatch) {
    std::vector<uint8_t> buf(sizeof(dheader_t) + 5, 0);
    dheader_t hdr{};
    hdr.ident = IDBSPHEADER;
    hdr.version = BSPVERSION;
    hdr.lumps[LUMP_VERTEXES].fileofs = static_cast<int>(sizeof(dheader_t));
    hdr.lumps[LUMP_VERTEXES].filelen = 5;  // not a multiple of sizeof(dvertex_t)==12
    std::memcpy(buf.data(), &hdr, sizeof(hdr));

    BspParser parser;
    EXPECT_THROW(parser.parse(buf), std::runtime_error);
}

} // namespace
} // namespace bsp2rbx
