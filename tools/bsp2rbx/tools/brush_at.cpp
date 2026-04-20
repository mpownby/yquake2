// One-off diagnostic: find all worldspawn brushes whose AABB contains (or
// is near) a given point, and for each one report its vertex/face
// signature plus which conversion path it takes (wedge / chamferedBeam /
// cornerChamfer / OBB fallback). Helps localize "this yellow thing in
// Studio corresponds to which brush?" problems.
//
// Usage: brush_at <bsp> <x> <y> <z> [padding]

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "bsp2rbx/BrushGeometry.h"
#include "bsp2rbx/BspParser.h"
#include "bsp2rbx/FileReader.h"
#include "bsp2rbx/SolidWorldspawnFilter.h"
#include "bsp2rbx/WorldspawnBrushSet.h"

using namespace bsp2rbx;

namespace {

constexpr float kEps = 0.01f;

std::vector<std::array<float, 3>>
dedup(const std::vector<std::array<float, 3>>& in) {
    std::vector<std::array<float, 3>> out;
    for (const auto& v : in) {
        bool dup = false;
        for (const auto& e : out) {
            if (std::fabs(e[0] - v[0]) < kEps &&
                std::fabs(e[1] - v[1]) < kEps &&
                std::fabs(e[2] - v[2]) < kEps) { dup = true; break; }
        }
        if (!dup) out.push_back(v);
    }
    return out;
}

std::string sigStr(const std::map<int, int>& sig) {
    std::string s;
    for (const auto& kv : sig) {
        if (kv.first < 3) continue;
        if (!s.empty()) s += "+";
        s += std::to_string(kv.second) + "*" + std::to_string(kv.first) + "v";
    }
    return s;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: brush_at <bsp> <x> <y> <z> [padding=4.0]\n");
        return 1;
    }
    const float px  = std::strtof(argv[2], nullptr);
    const float py  = std::strtof(argv[3], nullptr);
    const float pz  = std::strtof(argv[4], nullptr);
    const float pad = (argc >= 6) ? std::strtof(argv[5], nullptr) : 4.0f;

    FileReader r;
    BspParser  p;
    auto bsp = p.parse(r.read(argv[1]));

    WorldspawnBrushSet ws;
    auto wsSet = ws.compute(*bsp);

    SolidWorldspawnFilter filter;
    BrushGeometry         geom;

    std::printf("Searching for worldspawn brushes near (%.2f, %.2f, %.2f) pad=%.2f\n",
                px, py, pz, pad);

    int hits = 0;
    for (size_t i = 0; i < bsp->brushes.size(); ++i) {
        if (!wsSet.count(static_cast<int>(i))) continue;
        if (!filter.keep(*bsp, static_cast<int>(i))) continue;

        const auto verts = dedup(geom.brushVertices(*bsp, static_cast<int>(i)));
        if (verts.empty()) continue;
        float mn[3] = {  1e30f,  1e30f,  1e30f };
        float mx[3] = { -1e30f, -1e30f, -1e30f };
        for (const auto& v : verts) {
            for (int c = 0; c < 3; ++c) {
                if (v[c] < mn[c]) mn[c] = v[c];
                if (v[c] > mx[c]) mx[c] = v[c];
            }
        }
        if (px < mn[0] - pad || px > mx[0] + pad) continue;
        if (py < mn[1] - pad || py > mx[1] + pad) continue;
        if (pz < mn[2] - pad || pz > mx[2] + pad) continue;
        ++hits;

        const dbrush_t& b = bsp->brushes[i];
        std::map<int, int> histo;
        for (int s = 0; s < b.numsides; ++s) {
            const int bsi = b.firstside + s;
            if (bsi < 0 || (size_t)bsi >= bsp->brushsides.size()) continue;
            const auto pn = bsp->brushsides[bsi].planenum;
            if (pn >= bsp->planes.size()) continue;
            const auto& pl = bsp->planes[pn];
            int onF = 0;
            for (const auto& v : verts) {
                const float d = pl.normal[0]*v[0]+pl.normal[1]*v[1]+pl.normal[2]*v[2]-pl.dist;
                if (std::fabs(d) < kEps) ++onF;
            }
            ++histo[onF];
        }
        const std::string sig = std::to_string(verts.size()) + "v: " + sigStr(histo);

        // Which conversion path does this brush take?
        const char* path = "OBB-fallback";
        if (geom.brushWedge(*bsp, static_cast<int>(i)).has_value())              path = "brushWedge (phase 1)";
        else if (geom.brushChamferedBeam(*bsp, static_cast<int>(i)).has_value()) path = "brushChamferedBeam (phase 2)";
        else if (geom.brushCornerChamfer(*bsp, static_cast<int>(i)).has_value()) path = "brushCornerChamfer (phase 3)";

        std::printf("\n brush %4zu: %s\n", i, sig.c_str());
        std::printf("   bbox: (%.2f..%.2f, %.2f..%.2f, %.2f..%.2f)\n",
                    mn[0], mx[0], mn[1], mx[1], mn[2], mx[2]);
        std::printf("   conv: %s\n", path);
        std::printf("   sides:\n");
        for (int s = 0; s < b.numsides; ++s) {
            const int bsi = b.firstside + s;
            if (bsi < 0 || (size_t)bsi >= bsp->brushsides.size()) continue;
            const auto pn = bsp->brushsides[bsi].planenum;
            if (pn >= bsp->planes.size()) continue;
            const auto& pl = bsp->planes[pn];
            int onF = 0;
            for (const auto& v : verts) {
                const float d = pl.normal[0]*v[0]+pl.normal[1]*v[1]+pl.normal[2]*v[2]-pl.dist;
                if (std::fabs(d) < kEps) ++onF;
            }
            if (onF < 3) continue;
            std::printf("     n=(%+.3f,%+.3f,%+.3f) d=%+.3f onF=%d\n",
                pl.normal[0], pl.normal[1], pl.normal[2], pl.dist, onF);
        }
    }
    std::printf("\n%d brush(es) found.\n", hits);
    return 0;
}
