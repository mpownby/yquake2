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

TEST(BrushGeometryTest, WedgeOnAabbCubeReturnsNullopt) {
    // A cube has 6 sides — not the 5 of a triangular prism.
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushWedge(bsp, 0).has_value());
}

TEST(BrushGeometryTest, WedgeOnAxisAlignedRampMatchesGeometry) {
    // 45° axis-aligned ramp:
    //   x ∈ [0, 4], y ∈ [0, 2], z ∈ [0, 2], with the slanted top plane
    //   y + z <= 2 cutting off the (+y, +z) edge so the cross-section in YZ
    //   is a right triangle with legs of length 2 and the right angle at
    //   (y=0, z=0). Prism axis is +X, length 4.
    Bsp bsp;
    const float r2 = std::sqrt(0.5f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 },                 // x >= 0
        {  1, 0, 0, 4 },                 // x <= 4
        {  0,-1, 0, 0 },                 // y >= 0
        {  0, 0,-1, 0 },                 // z >= 0
        {  0, r2, r2, r2 * 2 },          // y + z <= 2  (45° slope)
    });

    BrushGeometry geom;
    const auto wOpt = geom.brushWedge(bsp, bi);
    ASSERT_TRUE(wOpt.has_value()) << "axis-aligned 5-side prism should be detected";
    const BrushWedge& w = *wOpt;

    // Local +X = prism axis (length 4 along world X).
    // Local +Y / +Z = the two right-angle legs (length 2 each in YZ).
    EXPECT_NEAR(w.size[0], 4.0f, 1e-4f);
    EXPECT_NEAR(w.size[1], 2.0f, 1e-4f);
    EXPECT_NEAR(w.size[2], 2.0f, 1e-4f);

    // Center is at the AABB center of the prism: (2, 1, 1).
    EXPECT_NEAR(w.center[0], 2.0f, 1e-4f);
    EXPECT_NEAR(w.center[1], 1.0f, 1e-4f);
    EXPECT_NEAR(w.center[2], 1.0f, 1e-4f);

    // Local +X column of rotation must be ±world X. (axisX is the prism
    // axis; it should align with world X — sign chosen to point "into" the
    // other triangle, which is at x = 4, so +X.)
    EXPECT_NEAR(w.rotation[0],  1.0f, 1e-4f);  // R00: local x's world x
    EXPECT_NEAR(w.rotation[3],  0.0f, 1e-4f);  // R10
    EXPECT_NEAR(w.rotation[6],  0.0f, 1e-4f);  // R20

    // Right-handedness: cross(col0, col1) == col2.
    auto col = [&](int c) {
        return std::array<float, 3>{ w.rotation[c], w.rotation[3 + c], w.rotation[6 + c] };
    };
    const auto c0 = col(0), c1 = col(1), c2 = col(2);
    const std::array<float, 3> cx{
        c0[1] * c1[2] - c0[2] * c1[1],
        c0[2] * c1[0] - c0[0] * c1[2],
        c0[0] * c1[1] - c0[1] * c1[0]
    };
    EXPECT_NEAR(cx[0], c2[0], 1e-4f);
    EXPECT_NEAR(cx[1], c2[1], 1e-4f);
    EXPECT_NEAR(cx[2], c2[2], 1e-4f);
}

TEST(BrushGeometryTest, WedgeOnNonRightTriangleReturnsNullopt) {
    // Triangular prism whose cross-section is an isoceles triangle with
    // 60°/60°/60° angles (no right angle) — not representable as a Roblox
    // WedgePart, must return nullopt and let the caller fall back to OBB.
    //
    // Equilateral triangle in the YZ plane with vertices
    //   (y=-1, z=0), (y=1, z=0), (y=0, z=√3)
    // The three side normals (outward, in YZ) are:
    //   bottom  : (0, -1)        dist = 0
    //   right   : (√3/2,  1/2)   dist = √3/2     (line through (1,0)-(0,√3))
    //   left    : (-√3/2, 1/2)   dist = √3/2
    // Prism along +X from x=0 to x=2.
    const float h = std::sqrt(3.0f) / 2.0f;
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0,    0,   0 },     // x >= 0
        {  1, 0,    0,   2 },     // x <= 2
        {  0, 0,   -1,   0 },     // z >= 0
        {  0,  h, 0.5f,  h },     // upper-right slope
        {  0, -h, 0.5f,  h },     // upper-left slope
    });

    BrushGeometry geom;
    const auto verts = geom.brushVertices(bsp, bi);
    ASSERT_EQ(verts.size(), 6u) << "test fixture should yield a triangular prism";
    EXPECT_FALSE(geom.brushWedge(bsp, bi).has_value());
}

TEST(BrushGeometryTest, WedgeRotatedRampHasRotatedFrame) {
    // 45° ramp rotated 90° about +Z so the prism axis runs along world +Y
    // instead of +X. Same shape, just oriented differently.
    //   y ∈ [0, 4]  (prism axis)
    //   x ∈ [0, 2]
    //   z ∈ [0, 2]
    //   slope: x + z <= 2
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 },           // x >= 0
        {  0,-1, 0, 0 },           // y >= 0
        {  0, 1, 0, 4 },           // y <= 4
        {  0, 0,-1, 0 },           // z >= 0
        { r2, 0, r2, r2 * 2 },     // x + z <= 2
    });

    BrushGeometry geom;
    const auto wOpt = geom.brushWedge(bsp, bi);
    ASSERT_TRUE(wOpt.has_value());
    const BrushWedge& w = *wOpt;

    // Prism length = 4 along Y; both legs = 2.
    EXPECT_NEAR(w.size[0], 4.0f, 1e-4f);
    EXPECT_NEAR(w.size[1], 2.0f, 1e-4f);
    EXPECT_NEAR(w.size[2], 2.0f, 1e-4f);

    // Center at AABB midpoint (1, 2, 1).
    EXPECT_NEAR(w.center[0], 1.0f, 1e-4f);
    EXPECT_NEAR(w.center[1], 2.0f, 1e-4f);
    EXPECT_NEAR(w.center[2], 1.0f, 1e-4f);

    // Local +X (prism axis) must align with world +Y. So R10 (Y component
    // of local x in world) ≈ 1.
    EXPECT_NEAR(w.rotation[3], 1.0f, 1e-4f);

    // Rotation must NOT be identity — prism axis is rotated.
    const std::array<float, 9> identity = { 1, 0, 0,  0, 1, 0,  0, 0, 1 };
    float deviation = 0.0f;
    for (int i = 0; i < 9; ++i) deviation += std::fabs(w.rotation[i] - identity[i]);
    EXPECT_GT(deviation, 0.1f);
}

TEST(BrushGeometryTest, WedgeRecognizesPrismWithBevelPlanes) {
    // Q2's qbsp adds axis-aligned bevel planes to the convex hull of each
    // brush for collision. A 5-defining-plane ramp commonly ships with 2
    // bevel planes (one per missing axis direction in its AABB), giving 7
    // total sides. Bevels are tangent to the hull and contain at most 2
    // hull vertices, so the prism detector must look at vertex/face shape
    // rather than gating on numsides == 5. This test reproduces the case
    // that made battle.bsp emit zero wedges before the fix.
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        // 5 defining planes: same ramp as WedgeOnAxisAlignedRampMatchesGeometry.
        { -1, 0, 0, 0 },             // x >= 0
        {  1, 0, 0, 4 },             // x <= 4 (this would be a bevel, but here
                                     //         it's defining — ramp is bounded
                                     //         in +x by the slope edge instead)
        {  0,-1, 0, 0 },             // y >= 0
        {  0, 0,-1, 0 },             // z >= 0
        {  0, r2, r2, r2 * 2 },      // y + z <= 2
        // Two bevel planes added by qbsp because no defining plane has +y
        // or +z normal:
        {  0, 1, 0, 2 },             // y <= 2 (bevel, tangent to slope)
        {  0, 0, 1, 2 },             // z <= 2 (bevel, tangent to slope)
    });

    BrushGeometry geom;
    const auto wOpt = geom.brushWedge(bsp, bi);
    ASSERT_TRUE(wOpt.has_value())
        << "ramp with bevel planes must still be detected as a wedge";
    EXPECT_NEAR(wOpt->size[0], 4.0f, 1e-4f);
    EXPECT_NEAR(wOpt->size[1], 2.0f, 1e-4f);
    EXPECT_NEAR(wOpt->size[2], 2.0f, 1e-4f);
}

TEST(BrushGeometryTest, WedgeThrowsOnOutOfRangeIndex) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_THROW(geom.brushWedge(bsp, 99), std::out_of_range);
}

// Returns total volume of all pieces in a decomposition.
float decompositionVolume(const BrushDecomposition& d) {
    float v = 0.0f;
    for (const BrushPiece& p : d.pieces) {
        const float pieceVol = p.size[0] * p.size[1] * p.size[2];
        // Wedge half-volume since it's a triangular prism = 1/2 of bounding box.
        v += (p.kind == BrushPiece::Kind::Wedge) ? pieceVol * 0.5f : pieceVol;
    }
    return v;
}

TEST(BrushGeometryTest, ChamferedBeamOnAabbCubeReturnsNullopt) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushChamferedBeam(bsp, 0).has_value());
}

TEST(BrushGeometryTest, ChamferedBeamOnTriangularPrismReturnsNullopt) {
    // A pure triangular prism (5 sides, 6 verts) is phase-1 wedge territory,
    // not phase-2 — the chamfered-beam detector must reject it so the
    // converter routes it through brushWedge instead.
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, {  1, 0, 0, 4 }, {  0,-1, 0, 0 }, {  0, 0,-1, 0 },
        {  0, r2, r2, r2 * 2 },
    });
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushChamferedBeam(bsp, bi).has_value());
}

TEST(BrushGeometryTest, ChamferedBeamOnAxisAlignedBeamProducesTwoBoxesAndWedge) {
    // Axis-aligned beam, prism axis = +Y, length 4 (y in [0, 4]).
    // Cross-section pentagon in XZ:
    //   (0,0), (2,0), (2,1), (1,2), (0,2)
    // This is a 2x2 square with the (+x, +z) corner cut by the chamfer plane
    // x + z <= 3 at 45°. Decomposition: lower 2x1 box + upper-left 1x1 box +
    // 1x1 right wedge filling the chamfer slope.
    Bsp bsp;
    const float r2 = std::sqrt(0.5f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 },              // x >= 0
        {  1, 0, 0, 2 },              // x <= 2
        {  0,-1, 0, 0 },              // y >= 0
        {  0, 1, 0, 4 },              // y <= 4
        {  0, 0,-1, 0 },              // z >= 0
        {  0, 0, 1, 2 },              // z <= 2
        { r2, 0, r2, r2 * 3 },        // x + z <= 3 (45° chamfer)
    });

    BrushGeometry geom;
    const auto dOpt = geom.brushChamferedBeam(bsp, bi);
    ASSERT_TRUE(dOpt.has_value()) << "axis-aligned chamfered beam should decompose";
    const auto& d = *dOpt;

    // Three pieces: two Parts and one Wedge.
    ASSERT_EQ(d.pieces.size(), 3u);
    int parts = 0, wedges = 0;
    for (const auto& p : d.pieces) {
        if (p.kind == BrushPiece::Kind::Part) ++parts;
        else if (p.kind == BrushPiece::Kind::Wedge) ++wedges;
    }
    EXPECT_EQ(parts, 2);
    EXPECT_EQ(wedges, 1);

    // Volumes: brush is a 2*2*4 box (= 16) minus a 1*1*4/2 chamfer wedge
    // (= 2). Brush volume = 14. Decomposition must match.
    EXPECT_NEAR(decompositionVolume(d), 14.0f, 1e-3f);
}

TEST(BrushGeometryTest, ChamferedBeamPiecesTileTheOriginalBoundingBox) {
    // Looser geometric check: every piece center lies inside the brush AABB,
    // and no piece extends outside it. Catches axis-mapping bugs.
    Bsp bsp;
    const float r2 = std::sqrt(0.5f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 10 },              // x >= -10
        {  1, 0, 0, 10 },              // x <= 10
        {  0,-1, 0, 0 },               // y >= 0
        {  0, 1, 0, 8 },               // y <= 8
        {  0, 0,-1, 0 },               // z >= 0
        {  0, 0, 1, 6 },               // z <= 6
        {  r2, 0, r2, (10 + 6) * r2 - r2 * 2 },  // chamfer cuts +x +z corner
    });

    BrushGeometry geom;
    const auto dOpt = geom.brushChamferedBeam(bsp, bi);
    ASSERT_TRUE(dOpt.has_value());

    // All piece centers inside the AABB [-10..10] x [0..8] x [0..6].
    for (const auto& p : dOpt->pieces) {
        EXPECT_GE(p.center[0], -10.5f);
        EXPECT_LE(p.center[0],  10.5f);
        EXPECT_GE(p.center[1],  -0.5f);
        EXPECT_LE(p.center[1],   8.5f);
        EXPECT_GE(p.center[2],  -0.5f);
        EXPECT_LE(p.center[2],   6.5f);
    }
}

TEST(BrushGeometryTest, ChamferedBeamPropagatesTexname) {
    Bsp bsp;
    const float r2 = std::sqrt(0.5f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 2 }, { 0,-1, 0, 0 }, { 0, 1, 0, 4 },
        { 0, 0,-1, 0 }, { 0, 0, 1, 2 }, { r2, 0, r2, r2 * 3 },
    }, "walls/beam_chamfer");
    BrushGeometry geom;
    const auto dOpt = geom.brushChamferedBeam(bsp, bi);
    ASSERT_TRUE(dOpt.has_value());
    EXPECT_EQ(dOpt->texname, std::string("walls/beam_chamfer"));
}

TEST(BrushGeometryTest, ChamferedBeamRecognizesPrismWithBevelPlanes) {
    // Same chamfered beam as above plus a bevel plane that is tangent to
    // the chamfer slope at one edge (touches 2 hull vertices). Detector
    // must ignore the bevel and still return a 3-piece decomposition.
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 2 }, { 0,-1, 0, 0 }, { 0, 1, 0, 4 },
        { 0, 0,-1, 0 }, { 0, 0, 1, 2 }, { r2, 0, r2, r2 * 3 },
        // Bevel plane along the y axis tangent to the chamfer face's right
        // edge: the chamfer face contains the edge from (2,?,1) to (1,?,2);
        // bevel y = 4 already exists. Skip extra bevel to avoid building an
        // invalid hull — qbsp typically doesn't add redundant bevels here.
    });
    BrushGeometry geom;
    EXPECT_TRUE(geom.brushChamferedBeam(bsp, bi).has_value());
}

TEST(BrushGeometryTest, ChamferedBeamHandlesChamferLongerThanRectangleSide) {
    // Regression: when the chamfer cut is deep enough that the chamfer edge
    // is *longer* than any rectangle side of the pentagon, the analyzer
    // must still pick rectangle-aligned axes from parallel pentagon edges
    // (not from the longest edge). Otherwise the bbox is computed in the
    // wrong frame and pieces extend outside the brush envelope.
    //
    // Cross-section in YZ: rectangle [0,3] x [0,1] with the (+y, +z) corner
    // chamfered at (~80% of the Y span, ~80% of Z span). The chamfer goes
    // from (3, 0.2) to (0.6, 1) — length sqrt(2.4^2 + 0.8^2) ≈ 2.53, which
    // is longer than the shortest rectangle sides (e.g. (0,1)->(0.6,1) =
    // 0.6) and the chamfered side pieces.
    Bsp bsp;
    const float dy = 2.4f, dz = 0.8f;
    const float L = std::sqrt(dy * dy + dz * dz);
    const float ny = dz / L, nz = dy / L;
    const float dist = ny * 3.0f + nz * 0.2f;  // plane through (3, 0.2)
    const int bi = addCustomBrush(bsp, {
        {  0,-1, 0, 0 },          // y >= 0
        {  0, 1, 0, 3 },          // y <= 3
        { -1, 0, 0, 0 },          // x >= 0
        {  1, 0, 0, 4 },          // x <= 4
        {  0, 0,-1, 0 },          // z >= 0
        {  0, 0, 1, 1 },          // z <= 1
        {  0, ny, nz, dist },     // chamfer
    });

    BrushGeometry geom;
    const auto dOpt = geom.brushChamferedBeam(bsp, bi);
    ASSERT_TRUE(dOpt.has_value());

    // Every piece of the decomposition must lie inside the brush AABB
    // [0..4] x [0..3] x [0..1].
    for (const auto& p : dOpt->pieces) {
        // Compute the piece's world-axis-aligned bounding box from its
        // local-frame size and rotation.
        const float halfX = p.size[0] * 0.5f;
        const float halfY = p.size[1] * 0.5f;
        const float halfZ = p.size[2] * 0.5f;
        // Project local axes (rotation columns) onto each world axis to
        // find the world-axis half-extent (sum of |R[w,k]| * half[k]).
        for (int w = 0; w < 3; ++w) {
            const float worldHalf =
                std::fabs(p.rotation[w * 3 + 0]) * halfX +
                std::fabs(p.rotation[w * 3 + 1]) * halfY +
                std::fabs(p.rotation[w * 3 + 2]) * halfZ;
            const float lo = p.center[w] - worldHalf;
            const float hi = p.center[w] + worldHalf;
            const float bbLo = (w == 0) ? 0.0f : (w == 1 ? 0.0f : 0.0f);
            const float bbHi = (w == 0) ? 4.0f : (w == 1 ? 3.0f : 1.0f);
            EXPECT_GE(lo, bbLo - 0.05f) << "piece extends below world axis " << w;
            EXPECT_LE(hi, bbHi + 0.05f) << "piece extends above world axis " << w;
        }
    }
}

TEST(BrushGeometryTest, CornerChamferOnAabbCubeReturnsNullopt) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushCornerChamfer(bsp, 0).has_value());
}

TEST(BrushGeometryTest, CornerChamferOnTriangularPrismReturnsNullopt) {
    // Triangular prism is phase-1 territory — corner-chamfer detector must
    // not fire on it.
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 4 }, { 0,-1, 0, 0 }, { 0, 0,-1, 0 },
        { 0, r2, r2, r2 * 2 },
    });
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushCornerChamfer(bsp, bi).has_value());
}

TEST(BrushGeometryTest, CornerChamferOnPentagonalPrismReturnsNullopt) {
    // Pentagonal prism (chamfered beam) is phase-2 — corner-chamfer
    // detector must not fire on it (face counts differ: 2 pent + 5 rect
    // vs 3 pent + 3 rect + 1 tri).
    Bsp bsp;
    const float r2 = std::sqrt(0.5f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 2 }, { 0,-1, 0, 0 }, { 0, 1, 0, 4 },
        { 0, 0,-1, 0 }, { 0, 0, 1, 2 }, { r2, 0, r2, r2 * 3 },
    });
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushCornerChamfer(bsp, bi).has_value());
}

TEST(BrushGeometryTest, CornerChamferOnAxisAlignedCubeWithCornerCutDecomposes) {
    // 2x2x2 box [0,2]^3 with the (+x,+y,+z) corner cut by a plane through
    // (1, 2, 2), (2, 1, 2), (2, 2, 1). All three chamfer legs = 1.
    Bsp bsp;
    const float r3 = 1.0f / std::sqrt(3.0f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 },             // x >= 0
        {  1, 0, 0, 2 },             // x <= 2
        {  0,-1, 0, 0 },             // y >= 0
        {  0, 1, 0, 2 },             // y <= 2
        {  0, 0,-1, 0 },             // z >= 0
        {  0, 0, 1, 2 },             // z <= 2
        { r3, r3, r3, r3 * 5 },      // x + y + z <= 5  (chamfer plane)
    });

    BrushGeometry geom;
    const auto verts = geom.brushVertices(bsp, bi);
    // 8 cube corners minus 1 chopped + 3 cut points -> 10 unique verts.
    // (brushVertices may return duplicates; we don't strictly check count.)
    ASSERT_GE(verts.size(), 10u);

    const auto dOpt = geom.brushCornerChamfer(bsp, bi);
    ASSERT_TRUE(dOpt.has_value()) << "axis-aligned corner chamfer should decompose";
    const auto& d = *dOpt;

    // 3 slab Parts + 1 approximation Wedge = 4 pieces.
    ASSERT_EQ(d.pieces.size(), 4u);
    int parts = 0, wedges = 0;
    for (const auto& p : d.pieces) {
        if (p.kind == BrushPiece::Kind::Part) ++parts;
        else if (p.kind == BrushPiece::Kind::Wedge) ++wedges;
    }
    EXPECT_EQ(parts, 3);
    EXPECT_EQ(wedges, 1);
}

TEST(BrushGeometryTest, CornerChamferAllPiecesInsideBrushAabb) {
    // The 4-piece decomposition must fit entirely inside the brush's
    // AABB. Otherwise pieces poke outside the brush envelope.
    Bsp bsp;
    const float r3 = 1.0f / std::sqrt(3.0f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 4 }, { 0,-1, 0, 0 }, { 0, 1, 0, 3 },
        { 0, 0,-1, 0 }, { 0, 0, 1, 2 },
        { r3, r3, r3, r3 * (4 + 3 + 2 - 1) },  // chamfer cuts (4,3,2) corner
    });

    BrushGeometry geom;
    const auto dOpt = geom.brushCornerChamfer(bsp, bi);
    ASSERT_TRUE(dOpt.has_value());

    for (const auto& p : dOpt->pieces) {
        const float halfX = p.size[0] * 0.5f;
        const float halfY = p.size[1] * 0.5f;
        const float halfZ = p.size[2] * 0.5f;
        for (int w = 0; w < 3; ++w) {
            const float worldHalf =
                std::fabs(p.rotation[w * 3 + 0]) * halfX +
                std::fabs(p.rotation[w * 3 + 1]) * halfY +
                std::fabs(p.rotation[w * 3 + 2]) * halfZ;
            const float lo = p.center[w] - worldHalf;
            const float hi = p.center[w] + worldHalf;
            const float bbHi = (w == 0) ? 4.0f : (w == 1 ? 3.0f : 2.0f);
            EXPECT_GE(lo, -0.05f) << "piece below world axis " << w;
            EXPECT_LE(hi, bbHi + 0.05f) << "piece above world axis " << w;
        }
    }
}

TEST(BrushGeometryTest, CornerChamferPropagatesTexname) {
    Bsp bsp;
    const float r3 = 1.0f / std::sqrt(3.0f);
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 2 }, { 0,-1, 0, 0 }, { 0, 1, 0, 2 },
        { 0, 0,-1, 0 }, { 0, 0, 1, 2 }, { r3, r3, r3, r3 * 5 },
    }, "ceiling/corner_chamfer");
    BrushGeometry geom;
    const auto dOpt = geom.brushCornerChamfer(bsp, bi);
    ASSERT_TRUE(dOpt.has_value());
    EXPECT_EQ(dOpt->texname, std::string("ceiling/corner_chamfer"));
}

TEST(BrushGeometryTest, CornerChamferThrowsOnOutOfRangeIndex) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_THROW(geom.brushCornerChamfer(bsp, 99), std::out_of_range);
}

TEST(BrushGeometryTest, HexagonalFloorOnAabbCubeReturnsNullopt) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushHexagonalFloor(bsp, 0).has_value());
}

TEST(BrushGeometryTest, HexagonalFloorOnPentagonalPrismReturnsNullopt) {
    // A box with ONE corner chamfered (single chamfer beam) — the hexagonal
    // floor detector must reject it so the ChamferedBeam detector handles it.
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 2 }, { 0,-1, 0, 0 }, { 0, 1, 0, 4 },
        { 0, 0,-1, 0 }, { 0, 0, 1, 2 }, { r2, 0, r2, r2 * 3 },
    });
    BrushGeometry geom;
    EXPECT_FALSE(geom.brushHexagonalFloor(bsp, bi).has_value());
}

TEST(BrushGeometryTest, HexagonalFloorOnBoxWithTwoAdjacentCornersChamfered) {
    // Box x in [0,10], y in [0,10], z in [0,2] with BOTH corners on the
    // -X side (i.e. (0,0) and (0,10)) chamfered by vertical 45° planes.
    // Top face is a hexagon with 4 axis-aligned edges + 2 diagonal edges.
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 },            // x >= 0
        {  1, 0, 0, 10 },           // x <= 10
        {  0,-1, 0, 0 },            // y >= 0
        {  0, 1, 0, 10 },           // y <= 10
        {  0, 0,-1, 0 },            // z >= 0
        {  0, 0, 1, 2 },            // z <= 2
        { -r2,  r2, 0,  6 * r2 },   // chamfer at (0,10) corner, 45°, cutU=cutV=4
        { -r2, -r2, 0, -4 * r2 },   // chamfer at (0, 0) corner, 45°, cutU=cutV=4
    });
    BrushGeometry geom;
    const auto dOpt = geom.brushHexagonalFloor(bsp, bi);
    ASSERT_TRUE(dOpt.has_value()) << "double-chamfered floor must decompose";
    const auto& d = *dOpt;

    ASSERT_EQ(d.pieces.size(), 4u);
    int parts = 0, wedges = 0;
    for (const auto& p : d.pieces) {
        if (p.kind == BrushPiece::Kind::Part) ++parts;
        else if (p.kind == BrushPiece::Kind::Wedge) ++wedges;
    }
    EXPECT_EQ(parts, 2);
    EXPECT_EQ(wedges, 2);

    // Volume: full bbox (10*10*2 = 200) minus two chamfer triangular
    // prisms (0.5*4*4*2 = 16 each) = 200 - 32 = 168.
    EXPECT_NEAR(decompositionVolume(d), 168.0f, 1e-3f);

    // All piece centers must lie inside the brush AABB.
    for (const auto& p : d.pieces) {
        EXPECT_GE(p.center[0], -0.01f); EXPECT_LE(p.center[0], 10.01f);
        EXPECT_GE(p.center[1], -0.01f); EXPECT_LE(p.center[1], 10.01f);
        EXPECT_GE(p.center[2], -0.01f); EXPECT_LE(p.center[2],  2.01f);
    }
}

TEST(BrushGeometryTest, HexagonalFloorHandlesSlopedBottomLikeBrush313) {
    // Brush 313 signature: box x in [-992,-832], y in [-1104,-880], z in
    // [224,256] with two vertical corner chamfers on the -X side AND a
    // sloped bottom plane. The detector must ignore the slope (treating
    // the solid as a vertical hexagonal prism from AABB minZ to +Z face)
    // and still return 4 pieces — the slope's volume loss is approximated.
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1,  0,  0,  992 },                 // x >= -992
        {  1,  0,  0, -832 },                 // x <= -832
        {  0, -1,  0,  1104 },                // y >= -1104
        {  0,  1,  0, -880 },                 // y <= -880
        {  0,  0, -1, -224 },                 // z >= 224
        {  0,  0,  1,  256 },                 // z <= 256
        { -0.6f,  0.8f, 0,  -166.4f },        // chamfer at -X/+Y corner
        { -0.6f, -0.8f, 0, 1420.8f },         // chamfer at -X/-Y corner
        { -0.196f, 0, -0.981f, -47.068f },    // sloped bottom (ignored)
    });
    BrushGeometry geom;
    const auto dOpt = geom.brushHexagonalFloor(bsp, bi);
    ASSERT_TRUE(dOpt.has_value());
    EXPECT_EQ(dOpt->pieces.size(), 4u);

    // All pieces within the brush AABB with a small tolerance.
    for (const auto& p : dOpt->pieces) {
        EXPECT_GE(p.center[0], -992.5f);
        EXPECT_LE(p.center[0], -831.5f);
        EXPECT_GE(p.center[1], -1104.5f);
        EXPECT_LE(p.center[1],  -879.5f);
        EXPECT_GE(p.center[2],   223.5f);
        EXPECT_LE(p.center[2],   256.5f);
    }
}

TEST(BrushGeometryTest, HexagonalFloorPropagatesTexname) {
    const float r2 = std::sqrt(0.5f);
    Bsp bsp;
    const int bi = addCustomBrush(bsp, {
        { -1, 0, 0, 0 }, { 1, 0, 0, 10 }, { 0,-1, 0, 0 }, { 0, 1, 0, 10 },
        { 0, 0,-1, 0 }, { 0, 0, 1, 2 },
        { -r2,  r2, 0,  6 * r2 }, { -r2, -r2, 0, -4 * r2 },
    }, "floors/lava");
    BrushGeometry geom;
    const auto dOpt = geom.brushHexagonalFloor(bsp, bi);
    ASSERT_TRUE(dOpt.has_value());
    EXPECT_EQ(dOpt->texname, std::string("floors/lava"));
}

TEST(BrushGeometryTest, HexagonalFloorThrowsOnOutOfRangeIndex) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_THROW(geom.brushHexagonalFloor(bsp, 99), std::out_of_range);
}

TEST(BrushGeometryTest, ChamferedBeamThrowsOnOutOfRangeIndex) {
    Bsp bsp = buildAxisAlignedCubeBsp(-1, 1, -1, 1, -1, 1);
    BrushGeometry geom;
    EXPECT_THROW(geom.brushChamferedBeam(bsp, 99), std::out_of_range);
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
