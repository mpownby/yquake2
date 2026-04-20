#include "bsp2rbx/WorldspawnBrushSet.h"

#include <vector>

namespace bsp2rbx {

std::unordered_set<int> WorldspawnBrushSet::compute(const Bsp& bsp) {
    std::unordered_set<int> out;
    if (bsp.models.empty()) return out;

    const int head = bsp.models[0].headnode;
    std::vector<int> stack;
    stack.push_back(head);
    std::unordered_set<int> visitedNodes;

    while (!stack.empty()) {
        const int idx = stack.back();
        stack.pop_back();

        if (idx >= 0) {
            if (static_cast<size_t>(idx) >= bsp.nodes.size()) continue;
            if (!visitedNodes.insert(idx).second) continue;
            const dnode_t& n = bsp.nodes[static_cast<size_t>(idx)];
            stack.push_back(n.children[0]);
            stack.push_back(n.children[1]);
        } else {
            const int leafIdx = -(idx + 1);
            if (leafIdx < 0 || static_cast<size_t>(leafIdx) >= bsp.leaves.size()) continue;
            const dleaf_t& leaf = bsp.leaves[static_cast<size_t>(leafIdx)];
            for (int k = 0; k < leaf.numleafbrushes; ++k) {
                const size_t lbIdx = static_cast<size_t>(leaf.firstleafbrush) + static_cast<size_t>(k);
                if (lbIdx >= bsp.leafbrushes.size()) continue;
                const int brushIdx = static_cast<int>(bsp.leafbrushes[lbIdx]);
                if (brushIdx < 0 || static_cast<size_t>(brushIdx) >= bsp.brushes.size()) continue;
                out.insert(brushIdx);
            }
        }
    }

    return out;
}

} // namespace bsp2rbx
