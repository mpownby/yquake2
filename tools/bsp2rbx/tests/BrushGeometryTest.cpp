#include <gtest/gtest.h>

#include <cstring>

#include "bsp2rbx/BrushGeometry.h"

namespace bsp2rbx {
namespace {

// Assembles an axis-aligned cube brush defined by six half-spaces:
// x >= minX, x <= maxX, y >= minY, y <= maxY, z >= minZ, z <= maxZ
Bsp buildAxisAlignedCubeBsp(float minX, float maxX, float minY, float maxY, float minZ, float maxZ) {
    Bsp bsp;
    auto addPlane = [&](float nx, float ny, float nz, float dist) {
        dplane_t p{};
        p.normal[0] = nx; p.normal[1] = ny; p.normal[2] = nz;
        p.dist = dist;
        bsp.planes.push_back(p);
    };
    // Planes store outward normals; dot(normal, p) <= dist keeps p inside.
    addPlane(-1, 0, 0, -minX);  // x >= minX
    addPlane( 1, 0, 0,  maxX);  // x <= maxX
    addPlane( 0,-1, 0, -minY);
    addPlane( 0, 1, 0,  maxY);
    addPlane( 0, 0,-1, -minZ);
    addPlane( 0, 0, 1,  maxZ);

    for (unsigned short i = 0; i < 6; ++i) {
        dbrushside_t s{};
        s.planenum = i;
        s.texinfo  = 0;
        bsp.brushsides.push_back(s);
    }

    texinfo_t ti{};
    std::strncpy(ti.texture, "walls/m1_1", sizeof(ti.texture) - 1);
    bsp.texinfos.push_back(ti);

    dbrush_t b{};
    b.firstside = 0;
    b.numsides  = 6;
    b.contents  = CONTENTS_SOLID;
    bsp.brushes.push_back(b);
    return bsp;
}

TEST(BrushGeometryTest, CubeProducesEightVertices) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -2, 2, -3, 3);
    BrushGeometry geom;
    auto verts = geom.brushVertices(bsp, 0);
    EXPECT_EQ(verts.size(), 8u);
}

TEST(BrushGeometryTest, CubeAabbMatchesInput) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -2, 2, -3, 3);
    BrushGeometry geom;
    auto aabb = geom.brushAabb(bsp, 0);

    EXPECT_FLOAT_EQ(aabb.mins[0], -1.0f);
    EXPECT_FLOAT_EQ(aabb.mins[1], -2.0f);
    EXPECT_FLOAT_EQ(aabb.mins[2], -3.0f);
    EXPECT_FLOAT_EQ(aabb.maxs[0],  1.0f);
    EXPECT_FLOAT_EQ(aabb.maxs[1],  2.0f);
    EXPECT_FLOAT_EQ(aabb.maxs[2],  3.0f);
    EXPECT_EQ(aabb.texname, std::string("walls/m1_1"));
    EXPECT_EQ(aabb.modelIndex, 0);
}

TEST(BrushGeometryTest, ThrowsOnOutOfRangeIndex) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_THROW(geom.brushVertices(bsp, 99), std::out_of_range);
    EXPECT_THROW(geom.brushAabb(bsp, -1),    std::out_of_range);
}

TEST(BrushGeometryTest, DegenerateBrushReturnsEmpty) {
    Bsp bsp;
    dbrush_t b{};
    b.firstside = 0;
    b.numsides  = 2;
    bsp.brushes.push_back(b);
    BrushGeometry geom;
    EXPECT_TRUE(geom.brushVertices(bsp, 0).empty());
}

} // namespace
} // namespace bsp2rbx
