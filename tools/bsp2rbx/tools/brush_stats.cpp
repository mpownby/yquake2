// One-off diagnostic: report unique-vertex count and face-by-vertex-count
// signature for every worldspawn brush in a BSP. Used to figure out why
// phase 3's corner-chamfer detector matches zero brushes in battle.bsp.
//
// Build: linked as a separate exe in CMake just like bsp2rbx.

#include <cstdio>
#include <cstring>
#include <map>
#include <set>
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
        if (kv.first < 3) continue;  // ignore bevels
        if (!s.empty()) s += "+";
        s += std::to_string(kv.second) + "*" + std::to_string(kv.first) + "v";
    }
    return s;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: brush_stats <bsp>\n"); return 1; }
    FileReader r;
    BspParser  p;
    auto bsp = p.parse(r.read(argv[1]));

    WorldspawnBrushSet ws;
    auto wsSet = ws.compute(*bsp);

    SolidWorldspawnFilter filter;
    BrushGeometry         geom;

    std::map<std::string, int> sigCounts;
    int totalKept = 0;

    for (size_t i = 0; i < bsp->brushes.size(); ++i) {
        if (!wsSet.count(static_cast<int>(i))) continue;
        if (!filter.keep(*bsp, static_cast<int>(i))) continue;
        ++totalKept;

        auto verts = dedup(geom.brushVertices(*bsp, static_cast<int>(i)));
        const dbrush_t& b = bsp->brushes[i];

        std::map<int, int> faceVertHisto;
        for (int s = 0; s < b.numsides; ++s) {
            const int bsi = b.firstside + s;
            if (bsi < 0 || static_cast<size_t>(bsi) >= bsp->brushsides.size()) continue;
            const auto pn = bsp->brushsides[static_cast<size_t>(bsi)].planenum;
            if (pn >= bsp->planes.size()) continue;
            const auto& pl = bsp->planes[pn];
            int onFace = 0;
            for (const auto& v : verts) {
                const float d = pl.normal[0]*v[0] + pl.normal[1]*v[1] + pl.normal[2]*v[2] - pl.dist;
                if (std::fabs(d) < kEps) ++onFace;
            }
            ++faceVertHisto[onFace];
        }

        const std::string sig = std::to_string(verts.size()) + "v: " + sigStr(faceVertHisto);
        ++sigCounts[sig];
    }

    std::printf("Total worldspawn brushes kept: %d\n", totalKept);
    std::printf("Brush signature counts (verts: face-shape histogram, ignoring <3-vert bevels):\n");
    for (const auto& kv : sigCounts) {
        std::printf("  %5d  %s\n", kv.second, kv.first.c_str());
    }

    // Phase 3 audit: list every brush with 3 pent + 3 rect + 1 tri faces
    // and whether brushCornerChamfer accepts it.
    std::printf("\n3 pent + 3 rect + 1 tri brushes:\n");
    int corn3pent3rect1tri = 0;
    int detected3 = 0;
    for (size_t i = 0; i < bsp->brushes.size(); ++i) {
        if (!wsSet.count(static_cast<int>(i))) continue;
        if (!filter.keep(*bsp, static_cast<int>(i))) continue;
        const auto vv = dedup(geom.brushVertices(*bsp, static_cast<int>(i)));
        if (vv.size() < 10) continue;
        const dbrush_t& bb = bsp->brushes[i];
        int t=0, r=0, p=0;
        for (int s = 0; s < bb.numsides; ++s) {
            const int bsi = bb.firstside + s;
            if (bsi < 0 || (size_t)bsi >= bsp->brushsides.size()) continue;
            const auto pn = bsp->brushsides[bsi].planenum;
            if (pn >= bsp->planes.size()) continue;
            const auto& pl = bsp->planes[pn];
            int onF = 0;
            for (const auto& v : vv) {
                const float d = pl.normal[0]*v[0]+pl.normal[1]*v[1]+pl.normal[2]*v[2]-pl.dist;
                if (std::fabs(d) < kEps) ++onF;
            }
            if (onF == 3) ++t;
            else if (onF == 4) ++r;
            else if (onF == 5) ++p;
        }
        if (t == 1 && r == 3 && p == 3) {
            ++corn3pent3rect1tri;
            const bool det = geom.brushCornerChamfer(*bsp, static_cast<int>(i)).has_value();
            if (det) ++detected3;
            if (!det && corn3pent3rect1tri <= 3) {
                std::printf("  brush %4zu: NO  (uniqueV=%zu)\n", i, vv.size());
                std::printf("    verts:");
                for (const auto& v : vv) std::printf(" (%.2f,%.2f,%.2f)", v[0], v[1], v[2]);
                std::printf("\n    face normals (n=, dist, vert-count):\n");
                for (int s = 0; s < bb.numsides; ++s) {
                    const int bsi = bb.firstside + s;
                    if (bsi < 0 || (size_t)bsi >= bsp->brushsides.size()) continue;
                    const auto pn = bsp->brushsides[bsi].planenum;
                    if (pn >= bsp->planes.size()) continue;
                    const auto& pl = bsp->planes[pn];
                    int onF = 0;
                    for (const auto& v : vv) {
                        const float d = pl.normal[0]*v[0]+pl.normal[1]*v[1]+pl.normal[2]*v[2]-pl.dist;
                        if (std::fabs(d) < kEps) ++onF;
                    }
                    if (onF >= 3)
                    std::printf("      n=(%.3f,%.3f,%.3f) d=%.3f onF=%d\n",
                        pl.normal[0], pl.normal[1], pl.normal[2], pl.dist, onF);
                }
            }
        }
    }
    std::printf("3p+3r+1t total: %d, corner-chamfer detected: %d\n",
                corn3pent3rect1tri, detected3);

    // Phase 2 audit — disabled by default; uncomment for chamfered-beam debugging.
    if (false) {
    std::printf("\n10v + 5 rect + 2 pent brushes:\n");
    for (size_t i = 0; i < bsp->brushes.size(); ++i) {
        if (!wsSet.count(static_cast<int>(i))) continue;
        if (!filter.keep(*bsp, static_cast<int>(i))) continue;
        const auto verts = dedup(geom.brushVertices(*bsp, static_cast<int>(i)));
        if (verts.size() != 10) continue;
        const dbrush_t& b = bsp->brushes[i];
        int rect = 0, pent = 0, other = 0;
        for (int s = 0; s < b.numsides; ++s) {
            const int bsi = b.firstside + s;
            if (bsi < 0 || (size_t)bsi >= bsp->brushsides.size()) continue;
            const auto pn = bsp->brushsides[bsi].planenum;
            if (pn >= bsp->planes.size()) continue;
            const auto& pl = bsp->planes[pn];
            int onFace = 0;
            for (const auto& v : verts) {
                const float d = pl.normal[0]*v[0]+pl.normal[1]*v[1]+pl.normal[2]*v[2]-pl.dist;
                if (std::fabs(d) < kEps) ++onFace;
            }
            if (onFace == 4) ++rect;
            else if (onFace == 5) ++pent;
            else if (onFace >= 3) ++other;
        }
        if (rect != 5 || pent != 2) continue;
        const bool detected = geom.brushChamferedBeam(*bsp, static_cast<int>(i)).has_value();
        std::printf("  brush %4zu: detected=%s", i, detected ? "YES" : "NO ");
        if (!detected) {
            // Print the 10 vertex coords + face/plane info to figure out
            // why analyzePentagon rejects this brush.
            std::printf("\n    verts:");
            for (const auto& v : verts) {
                std::printf(" (%.2f,%.2f,%.2f)", v[0], v[1], v[2]);
            }
            std::printf("\n    faces:");
            for (int s = 0; s < b.numsides; ++s) {
                const int bsi = b.firstside + s;
                if (bsi < 0 || (size_t)bsi >= bsp->brushsides.size()) continue;
                const auto pn = bsp->brushsides[bsi].planenum;
                if (pn >= bsp->planes.size()) continue;
                const auto& pl = bsp->planes[pn];
                int onFace = 0;
                for (const auto& v : verts) {
                    const float d = pl.normal[0]*v[0]+pl.normal[1]*v[1]+pl.normal[2]*v[2]-pl.dist;
                    if (std::fabs(d) < kEps) ++onFace;
                }
                std::printf(" n=(%.2f,%.2f,%.2f) d=%.2f #v=%d;",
                    pl.normal[0], pl.normal[1], pl.normal[2], pl.dist, onFace);
            }
        }
        std::printf("\n");
    }
    } // if (false)
    return 0;
}
