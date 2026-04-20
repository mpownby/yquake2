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

// Builds a brush by appending planes + sides to an existing Bsp, returning
// the new brush index.
int addCustomBrush(Bsp& bsp, const std::vector<std::array<float, 4>>& planes,
                   const char* texture = "walls/m1_1") {
    const unsigned short firstPlane = static_cast<unsigned short>(bsp.planes.size());
    const int firstSide = static_cast<int>(bsp.brushsides.size());
    for (const auto& pl : planes) {
        dplane_t p{};
        p.normal[0] = pl[0]; p.normal[1] = pl[1]; p.normal[2] = pl[2];
        p.dist = pl[3];
        bsp.planes.push_back(p);
    }
    for (size_t i = 0; i < planes.size(); ++i) {
        dbrushside_t s{};
        s.planenum = static_cast<unsigned short>(firstPlane + i);
        s.texinfo = 0;
        bsp.brushsides.push_back(s);
    }
    if (bsp.texinfos.empty()) {
        texinfo_t ti{};
        std::strncpy(ti.texture, texture, sizeof(ti.texture) - 1);
        bsp.texinfos.push_back(ti);
    }
    dbrush_t b{};
    b.firstside = firstSide;
    b.numsides  = static_cast<int>(planes.size());
    b.contents  = CONTENTS_SOLID;
    bsp.brushes.push_back(b);
    return static_cast<int>(bsp.brushes.size() - 1);
}

TEST(BrushGeometryTest, ObbAxisAlignedCubeHasIdentityRotation) {
    // A cube centered at origin, extents [-1, 1] x [-2, 2] x [-3, 3].
    // OBB should coincide with AABB, rotation = identity.
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -2, 2, -3, 3);
    BrushGeometry geom;
    const BrushObb obb = geom.brushObb(bsp, 0);

    EXPECT_FLOAT_EQ(obb.center[0], 0.0f);
    EXPECT_FLOAT_EQ(obb.center[1], 0.0f);
    EXPECT_FLOAT_EQ(obb.center[2], 0.0f);
    EXPECT_FLOAT_EQ(obb.size[0], 2.0f);
    EXPECT_FLOAT_EQ(obb.size[1], 4.0f);
    EXPECT_FLOAT_EQ(obb.size[2], 6.0f);

    const std::array<float, 9> identity = { 1, 0, 0,  0, 1, 0,  0, 0, 1 };
    for (int i = 0; i < 9; ++i) {
        EXPECT_NEAR(obb.rotation[i], identity[i], 1e-5f) << "rotation[" << i << "]";
    }
    EXPECT_EQ(obb.texname, std::string("walls/m1_1"));
}

TEST(BrushGeometryTest, ObbTranslatedAxisAlignedCubeHasCorrectCenter) {
    // Cube from (10, 20, 30) to (14, 26, 40).
    Bsp bsp = buildAxisAlignedCubeBsp(10, 14, 20, 26, 30, 40);
    BrushGeometry geom;
    const BrushObb obb = geom.brushObb(bsp, 0);

    EXPECT_FLOAT_EQ(obb.center[0], 12.0f);
    EXPECT_FLOAT_EQ(obb.center[1], 23.0f);
    EXPECT_FLOAT_EQ(obb.center[2], 35.0f);
    EXPECT_FLOAT_EQ(obb.size[0],  4.0f);
    EXPECT_FLOAT_EQ(obb.size[1],  6.0f);
    EXPECT_FLOAT_EQ(obb.size[2], 10.0f);
}

TEST(BrushGeometryTest, Obb45DegRotatedBoxProducesRotatedFrame) {
    // Box rotated 45° around +Z. Half-extents along local x̂' = 2, ŷ' = 1, ẑ = 3.
    // Local axes: x̂' = (√2/2,  √2/2, 0), ŷ' = (-√2/2, √2/2, 0).
    // Face plane normals (outward) and distances:
    //   ±x̂' at distance 2 (so `dist = 2` for +x̂', `dist = 2` for -x̂' — since
    //   for plane n·p <= d to keep point inside, the -x̂' plane is (-x̂')·p <= 2)
    //   ±ŷ' at distance 1
    //   ±ẑ at distance 3
    const float s = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -s, -s, 0, 2 },  // -x̂' face outward -> (-x̂')
        {  s,  s, 0, 2 },  // +x̂' face outward ->  (+x̂')
        {  s, -s, 0, 1 },  // -ŷ' face outward ->  (-ŷ' because ŷ' = (-s, s, 0))
        { -s,  s, 0, 1 },  // +ŷ' face outward ->  (+ŷ')
        {  0,  0,-1, 3 },
        {  0,  0, 1, 3 },
    });

    BrushGeometry geom;
    const BrushObb obb = geom.brushObb(bsp, bi);

    // Center should be at the origin.
    EXPECT_NEAR(obb.center[0], 0.0f, 1e-4f);
    EXPECT_NEAR(obb.center[1], 0.0f, 1e-4f);
    EXPECT_NEAR(obb.center[2], 0.0f, 1e-4f);

    // Sizes should be the oriented extents (4, 2, 6), assigned to the
    // closest-world-axis slot. The rotated x̂' and ŷ' both have |x|=|y|=√2/2
    // so they tie on alignment — the result just needs to be consistent.
    // We assert total extent and that Z extent comes out on axis 2.
    EXPECT_NEAR(obb.size[2], 6.0f, 1e-4f);
    const float perimXy = obb.size[0] + obb.size[1];
    EXPECT_NEAR(perimXy, 6.0f, 1e-4f);  // 4 + 2 regardless of assignment

    // Z row of rotation should be (0, 0, 1).
    EXPECT_NEAR(obb.rotation[2], 0.0f, 1e-4f);  // R02: local z's x
    EXPECT_NEAR(obb.rotation[5], 0.0f, 1e-4f);  // R12: local z's y
    EXPECT_NEAR(obb.rotation[8], 1.0f, 1e-4f);  // R22: local z's z

    // X/Y columns should be rotated ~45° in xy plane: |R00|=|R10|=|R01|=|R11|=√2/2.
    EXPECT_NEAR(std::fabs(obb.rotation[0]), s, 1e-4f);
    EXPECT_NEAR(std::fabs(obb.rotation[3]), s, 1e-4f);
    EXPECT_NEAR(std::fabs(obb.rotation[1]), s, 1e-4f);
    EXPECT_NEAR(std::fabs(obb.rotation[4]), s, 1e-4f);
}

TEST(BrushGeometryTest, ObbTriangularPrismPicksHypotenuseAlignedSlab) {
    // 45° triangular prism (ramp / corner-chamfer): base rectangle z=0, back
    // wall at x=0, side walls at y ∈ [-1, 1], slanted top (x + z)/√2 ≤ √2.
    //
    // The AABB (axis-aligned) has size (2, 2, 2) -> volume 8, min extent 2.
    // The hypotenuse-aligned OBB has size (2√2, 2, √2) -> same volume 8 but
    // min extent √2 ≈ 1.414. With equal volume, the tie-breaker prefers the
    // thinner-axis orientation because visually it represents the chamfer as
    // a thin slanted slab instead of a fat rectangular filler — which is what
    // the human observer sees in Q2's rendering of the cut corner.
    Bsp bsp;
    const float r2 = std::sqrt(2.0f) / 2.0f;
    const int bi = addCustomBrush(bsp, {
        {  0,  0, -1, 0 },
        { -1,  0,  0, 0 },
        {  0, -1,  0, 1 },
        {  0,  1,  0, 1 },
        { r2,  0, r2, r2*2 },
    });

    BrushGeometry geom;
    const BrushObb obb = geom.brushObb(bsp, bi);

    // Min extent must be √2 (the slab thickness), not 2 (AABB).
    const float minSize = std::min({ obb.size[0], obb.size[1], obb.size[2] });
    EXPECT_NEAR(minSize, std::sqrt(2.0f), 1e-3f)
        << "expected slab-thickness √2, got sizes "
        << obb.size[0] << ", " << obb.size[1] << ", " << obb.size[2];

    // Rotation must NOT be identity — tie-breaker chose rotated orientation.
    const std::array<float, 9> identity = { 1, 0, 0,  0, 1, 0,  0, 0, 1 };
    float deviation = 0.0f;
    for (int i = 0; i < 9; ++i) deviation += std::fabs(obb.rotation[i] - identity[i]);
    EXPECT_GT(deviation, 0.1f)
        << "rotation is identity — tie-breaker failed to pick the slanted slab";

    // Y axis extent stays 2 (the prism length; unaffected by the XZ rotation).
    // Verify y is one of the three sizes.
    bool yAxisPresent =
        std::fabs(obb.size[0] - 2.0f) < 1e-3f ||
        std::fabs(obb.size[1] - 2.0f) < 1e-3f ||
        std::fabs(obb.size[2] - 2.0f) < 1e-3f;
    EXPECT_TRUE(yAxisPresent) << "expected one axis of extent 2 (prism length)";

    // Volume must still match: 2√2 * 2 * √2 = 8.
    const float vol = obb.size[0] * obb.size[1] * obb.size[2];
    EXPECT_NEAR(vol, 8.0f, 1e-2f);
}

TEST(BrushGeometryTest, ObbRotationIsOrthonormalAndRightHanded) {
    // For a 45° rotated box, verify the rotation matrix columns are unit
    // vectors and form a right-handed basis (det = +1).
    const float s = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -s, -s, 0, 2 }, {  s,  s, 0, 2 },
        {  s, -s, 0, 1 }, { -s,  s, 0, 1 },
        {  0,  0,-1, 3 }, {  0,  0, 1, 3 },
    });

    BrushGeometry geom;
    const BrushObb obb = geom.brushObb(bsp, bi);

    // Columns of R (each column is a local axis in world space).
    auto col = [&](int c) {
        return std::array<float, 3>{ obb.rotation[c], obb.rotation[3 + c], obb.rotation[6 + c] };
    };
    auto dot3 = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    };
    const auto c0 = col(0), c1 = col(1), c2 = col(2);
    EXPECT_NEAR(dot3(c0, c0), 1.0f, 1e-4f);
    EXPECT_NEAR(dot3(c1, c1), 1.0f, 1e-4f);
    EXPECT_NEAR(dot3(c2, c2), 1.0f, 1e-4f);
    EXPECT_NEAR(dot3(c0, c1), 0.0f, 1e-4f);
    EXPECT_NEAR(dot3(c0, c2), 0.0f, 1e-4f);
    EXPECT_NEAR(dot3(c1, c2), 0.0f, 1e-4f);

    // Right-handed: cross(c0, c1) should equal c2.
    std::array<float, 3> cx{ c0[1]*c1[2] - c0[2]*c1[1],
                              c0[2]*c1[0] - c0[0]*c1[2],
                              c0[0]*c1[1] - c0[1]*c1[0] };
    EXPECT_NEAR(cx[0], c2[0], 1e-4f);
    EXPECT_NEAR(cx[1], c2[1], 1e-4f);
    EXPECT_NEAR(cx[2], c2[2], 1e-4f);
}

TEST(BrushGeometryTest, ObbTiltedSlabFindsRotatedFitViaCrossProductSynthesis) {
    // Regression for bridge-support / chamfered-beam brushes in battle.bsp:
    // 6 axis-aligned faces form a loose outer bounding box, and 2 antiparallel
    // tilted faces clip the brush into a thin slanted slab. The only orthogonal
    // triple of *face normals* is {x̂, ŷ, ẑ} -> produces an AABB far larger
    // than the true brush. The algorithm must synthesize the third axis as
    // cross(x̂, tilted_normal) and pick the minimum-volume OBB, which is the
    // tilted-slab orientation.
    //
    // Build a slab that spans x in [-8, 8], with the slab oriented along a
    // normal tilted 30° from +Z toward +Y:  n_slab = (0, sin30, cos30).
    // The slab's extent along n_slab is ±1 (thin), and along its two
    // in-plane axes it is ±50 (long, perpendicular to x̂) and ±8 (x̂ wide).
    const float s30 = 0.5f;
    const float c30 = std::sqrt(3.0f) / 2.0f;

    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        // Axis-aligned outer bounds — deliberately loose.
        { -1,  0,  0, 8   },  // x >= -8
        {  1,  0,  0, 8   },  // x <=  8
        {  0, -1,  0, 100 },  // y >= -100
        {  0,  1,  0, 100 },  // y <=  100
        {  0,  0, -1, 100 },  // z >= -100
        {  0,  0,  1, 100 },  // z <=  100
        // The defining slab normals (antiparallel pair) — these clip into
        // a thin 2-thick slab along n_slab.
        {  0,  s30,  c30, 1 },
        {  0, -s30, -c30, 1 },
    });

    BrushGeometry geom;
    const BrushObb obb = geom.brushObb(bsp, bi);

    // The tilted OBB should have: one axis = x̂ (16 wide),
    // one axis ≈ n_slab (2 thin), one axis ≈ perpendicular in YZ plane (big).
    // The slab is thin (2), so one of the three sizes must be ~2.
    const float minSize = std::min({ obb.size[0], obb.size[1], obb.size[2] });
    EXPECT_NEAR(minSize, 2.0f, 0.1f) << "expected thin-slab extent, got sizes "
        << obb.size[0] << ", " << obb.size[1] << ", " << obb.size[2];

    // One axis should still be aligned with world x.
    const bool xAligned =
        (std::fabs(obb.rotation[0]) > 0.99f) ||  // R00: local x's world x
        (std::fabs(obb.rotation[1]) > 0.99f) ||  // R01
        (std::fabs(obb.rotation[2]) > 0.99f);    // R02
    EXPECT_TRUE(xAligned) << "expected one OBB axis parallel to world X";

    // Rotation must NOT be identity — the slab is tilted, the algorithm must
    // have found it.
    const std::array<float, 9> identity = { 1, 0, 0,  0, 1, 0,  0, 0, 1 };
    float deviation = 0.0f;
    for (int i = 0; i < 9; ++i) deviation += std::fabs(obb.rotation[i] - identity[i]);
    EXPECT_GT(deviation, 0.1f)
        << "rotation was identity — algorithm failed to find the tilted OBB";

    // And the tight OBB should have much smaller volume than the axis AABB.
    const float obbVol = obb.size[0] * obb.size[1] * obb.size[2];
    const float aabbVol = 16.0f * 200.0f * 200.0f;  // loose axis bounds
    EXPECT_LT(obbVol, aabbVol * 0.1f)
        << "tilted OBB should be <10% the volume of the axis-aligned bounds";
}

TEST(BrushGeometryTest, ObbThrowsOnOutOfRangeIndex) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_THROW(geom.brushObb(bsp, 99), std::out_of_range);
}

TEST(BrushGeometryTest, ObbEmptyBrushReturnsZeroSizeIdentity) {
    Bsp bsp;
    dbrush_t b{};
    b.firstside = 0;
    b.numsides  = 2;  // degenerate: no vertices
    bsp.brushes.push_back(b);
    BrushGeometry geom;
    const BrushObb obb = geom.brushObb(bsp, 0);
    EXPECT_EQ(obb.size[0], 0.0f);
    EXPECT_EQ(obb.size[1], 0.0f);
    EXPECT_EQ(obb.size[2], 0.0f);
}

} // namespace
} // namespace bsp2rbx
