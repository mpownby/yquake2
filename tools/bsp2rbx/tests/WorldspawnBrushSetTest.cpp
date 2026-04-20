#include <gtest/gtest.h>

#include "bsp2rbx/WorldspawnBrushSet.h"

namespace bsp2rbx {
namespace {

// Helpers for hand-building a Bsp in tests.
dmodel_t makeModel(int headnode) {
    dmodel_t m{};
    m.headnode = headnode;
    return m;
}

dnode_t makeNode(int c0, int c1) {
    dnode_t n{};
    n.children[0] = c0;
    n.children[1] = c1;
    return n;
}

dleaf_t makeLeaf(unsigned short firstLeafBrush, unsigned short numLeafBrushes) {
    dleaf_t l{};
    l.contents = CONTENTS_SOLID;
    l.firstleafbrush = firstLeafBrush;
    l.numleafbrushes = numLeafBrushes;
    return l;
}

TEST(WorldspawnBrushSetTest, EmptyModelsReturnsEmpty) {
    Bsp bsp;
    WorldspawnBrushSet sut;
    EXPECT_TRUE(sut.compute(bsp).empty());
}

TEST(WorldspawnBrushSetTest, SingleLeafUnderModelZeroReturnsItsBrushes) {
    // Model 0 headnode points directly at leaf 0, which references brushes 0 and 1.
    Bsp bsp;
    bsp.brushes.resize(2);
    bsp.leafbrushes = { 0, 1 };
    bsp.leaves = { makeLeaf(0, 2) };
    // Headnode of -1 means "leaf index -(-1+1) = 0".
    bsp.models = { makeModel(-1) };

    WorldspawnBrushSet sut;
    auto result = sut.compute(bsp);
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result.count(0), 1u);
    EXPECT_EQ(result.count(1), 1u);
}

TEST(WorldspawnBrushSetTest, WalksNodeTreeCollectingAllLeafBrushes) {
    // node0: children {leaf0, node1}  -> leaf0 refs brush 0; node1 children {leaf1, leaf2}
    // leaf1 refs brush 1, leaf2 refs brush 2.
    Bsp bsp;
    bsp.brushes.resize(3);
    bsp.leafbrushes = { 0, 1, 2 };
    bsp.leaves = {
        makeLeaf(0, 1),  // leaf 0 -> brush 0
        makeLeaf(1, 1),  // leaf 1 -> brush 1
        makeLeaf(2, 1),  // leaf 2 -> brush 2
    };
    bsp.nodes = {
        makeNode(-1 /*leaf 0*/, 1 /*node 1*/),  // node 0
        makeNode(-2 /*leaf 1*/, -3 /*leaf 2*/), // node 1
    };
    bsp.models = { makeModel(0) };

    WorldspawnBrushSet sut;
    auto result = sut.compute(bsp);
    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result.count(0), 1u);
    EXPECT_EQ(result.count(1), 1u);
    EXPECT_EQ(result.count(2), 1u);
}

TEST(WorldspawnBrushSetTest, ExcludesBrushEntityBrushesFromOtherModels) {
    // Model 0 owns brushes {0, 1}; model 1 owns brush 2.
    // The critical invariant: the result MUST exclude brush 2.
    Bsp bsp;
    bsp.brushes.resize(3);
    bsp.leafbrushes = { 0, 1, 2 };
    bsp.leaves = {
        makeLeaf(0, 2),  // leaf 0 -> brushes 0, 1  (model 0)
        makeLeaf(2, 1),  // leaf 1 -> brush 2       (model 1)
    };
    bsp.nodes.clear();
    bsp.models = {
        makeModel(-1),  // model 0 headnode = -1 => leaf 0
        makeModel(-2),  // model 1 headnode = -2 => leaf 1
    };

    WorldspawnBrushSet sut;
    auto result = sut.compute(bsp);
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result.count(0), 1u);
    EXPECT_EQ(result.count(1), 1u);
    EXPECT_EQ(result.count(2), 0u) << "brush 2 belongs to model 1, must not leak into worldspawn set";
}

TEST(WorldspawnBrushSetTest, SilentlySkipsOutOfRangeLeafBrushIndices) {
    // leaf claims 3 brushes starting at firstleafbrush=0, but leafbrushes
    // only has 2 entries. We must not read past the end.
    Bsp bsp;
    bsp.brushes.resize(2);
    bsp.leafbrushes = { 0, 1 };
    bsp.leaves = { makeLeaf(0, 3) };
    bsp.models = { makeModel(-1) };

    WorldspawnBrushSet sut;
    auto result = sut.compute(bsp);
    EXPECT_EQ(result.size(), 2u);
}

TEST(WorldspawnBrushSetTest, SilentlySkipsOutOfRangeBrushIndex) {
    // leafbrushes references brush 99 which doesn't exist.
    Bsp bsp;
    bsp.brushes.resize(1);
    bsp.leafbrushes = { 0, 99 };
    bsp.leaves = { makeLeaf(0, 2) };
    bsp.models = { makeModel(-1) };

    WorldspawnBrushSet sut;
    auto result = sut.compute(bsp);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result.count(0), 1u);
}

TEST(WorldspawnBrushSetTest, HandlesCycleInNodesWithoutInfiniteLoop) {
    // Pathological (should not happen in a valid BSP) cycle: node 0 -> node 0.
    // We must terminate.
    Bsp bsp;
    bsp.brushes.resize(1);
    bsp.leafbrushes = { 0 };
    bsp.leaves = { makeLeaf(0, 1) };
    bsp.nodes = { makeNode(0, -1) };  // self-reference + leaf 0
    bsp.models = { makeModel(0) };

    WorldspawnBrushSet sut;
    auto result = sut.compute(bsp);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result.count(0), 1u);
}

} // namespace
} // namespace bsp2rbx
