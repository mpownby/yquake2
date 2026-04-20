#include <gtest/gtest.h>

#include <cstring>

#include "bsp2rbx/SolidWorldspawnFilter.h"

namespace bsp2rbx {
namespace {

Bsp makeBspWithOneBrush(int contents, const char* texName, int surfFlags = 0) {
    Bsp bsp;
    texinfo_t ti{};
    std::strncpy(ti.texture, texName, sizeof(ti.texture) - 1);
    ti.flags = surfFlags;
    bsp.texinfos.push_back(ti);

    dbrushside_t s{};
    s.planenum = 0;
    s.texinfo = 0;
    bsp.brushsides.push_back(s);

    dbrush_t b{};
    b.firstside = 0;
    b.numsides  = 1;
    b.contents  = contents;
    bsp.brushes.push_back(b);
    return bsp;
}

TEST(SolidWorldspawnFilterTest, KeepsSolidWorldBrush) {
    auto bsp = makeBspWithOneBrush(CONTENTS_SOLID, "walls/m1_1");
    SolidWorldspawnFilter f;
    EXPECT_TRUE(f.keep(bsp, 0));
}

TEST(SolidWorldspawnFilterTest, RejectsNonSolid) {
    auto bsp = makeBspWithOneBrush(CONTENTS_WATER, "walls/m1_1");
    SolidWorldspawnFilter f;
    EXPECT_FALSE(f.keep(bsp, 0));
}

TEST(SolidWorldspawnFilterTest, RejectsClip) {
    auto bsp = makeBspWithOneBrush(CONTENTS_SOLID | CONTENTS_PLAYERCLIP, "walls/m1_1");
    SolidWorldspawnFilter f;
    EXPECT_FALSE(f.keep(bsp, 0));
}

TEST(SolidWorldspawnFilterTest, RejectsSkyByTexname) {
    auto bsp = makeBspWithOneBrush(CONTENTS_SOLID, "sky1");
    SolidWorldspawnFilter f;
    EXPECT_FALSE(f.keep(bsp, 0));
}

TEST(SolidWorldspawnFilterTest, RejectsSkyBySurfFlag) {
    auto bsp = makeBspWithOneBrush(CONTENTS_SOLID, "walls/m1_1", SURF_SKY);
    SolidWorldspawnFilter f;
    EXPECT_FALSE(f.keep(bsp, 0));
}

TEST(SolidWorldspawnFilterTest, RejectsTriggerAndClipTexnames) {
    for (const char* name : {"trigger", "clip", "origin", "hint", "skip"}) {
        auto bsp = makeBspWithOneBrush(CONTENTS_SOLID, name);
        SolidWorldspawnFilter f;
        EXPECT_FALSE(f.keep(bsp, 0)) << "expected reject for texname " << name;
    }
}

TEST(SolidWorldspawnFilterTest, RejectsOutOfRangeIndex) {
    auto bsp = makeBspWithOneBrush(CONTENTS_SOLID, "walls/m1_1");
    SolidWorldspawnFilter f;
    EXPECT_FALSE(f.keep(bsp, 5));
    EXPECT_FALSE(f.keep(bsp, -1));
}

} // namespace
} // namespace bsp2rbx
