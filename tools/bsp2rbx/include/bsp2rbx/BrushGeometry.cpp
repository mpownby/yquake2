#include "bsp2rbx/BrushGeometry.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace bsp2rbx {

namespace {

constexpr float kEpsilon = 0.01f;

struct Vec3 {
    float x, y, z;
};

Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 scale(const Vec3& a, float s) {
    return { a.x * s, a.y * s, a.z * s };
}

Vec3 add(const Vec3& a, const Vec3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

bool intersectPlanes(const dplane_t& a, const dplane_t& b, const dplane_t& c, Vec3& out) {
    const Vec3 na{ a.normal[0], a.normal[1], a.normal[2] };
    const Vec3 nb{ b.normal[0], b.normal[1], b.normal[2] };
    const Vec3 nc{ c.normal[0], c.normal[1], c.normal[2] };
    const Vec3 nbxnc = cross(nb, nc);
    const float denom = dot(na, nbxnc);
    if (std::fabs(denom) < 1e-6f) {
        return false;
    }
    const Vec3 ncxna = cross(nc, na);
    const Vec3 naxnb = cross(na, nb);
    const Vec3 sum = add(add(scale(nbxnc, a.dist), scale(ncxna, b.dist)), scale(naxnb, c.dist));
    out = scale(sum, 1.0f / denom);
    return true;
}

bool pointInsideAllPlanes(const Vec3& p,
                          const dbrush_t& brush,
                          const Bsp& bsp,
                          size_t skipA, size_t skipB, size_t skipC) {
    for (int s = 0; s < brush.numsides; ++s) {
        if (static_cast<size_t>(s) == skipA ||
            static_cast<size_t>(s) == skipB ||
            static_cast<size_t>(s) == skipC) {
            continue;
        }
        const int bsi = brush.firstside + s;
        if (bsi < 0 || static_cast<size_t>(bsi) >= bsp.brushsides.size()) {
            return false;
        }
        const dbrushside_t& side = bsp.brushsides[static_cast<size_t>(bsi)];
        if (side.planenum >= bsp.planes.size()) {
            return false;
        }
        const dplane_t& plane = bsp.planes[side.planenum];
        const float d = plane.normal[0] * p.x + plane.normal[1] * p.y + plane.normal[2] * p.z - plane.dist;
        if (d > kEpsilon) {
            return false;
        }
    }
    return true;
}

void validateBrushIndex(const Bsp& bsp, int brushIndex) {
    if (brushIndex < 0 || static_cast<size_t>(brushIndex) >= bsp.brushes.size()) {
        throw std::out_of_range("BrushGeometry: brush index out of range");
    }
}

std::string textureNameForBrush(const Bsp& bsp, const dbrush_t& brush) {
    for (int s = 0; s < brush.numsides; ++s) {
        const int bsi = brush.firstside + s;
        if (bsi < 0 || static_cast<size_t>(bsi) >= bsp.brushsides.size()) {
            continue;
        }
        const short ti = bsp.brushsides[static_cast<size_t>(bsi)].texinfo;
        if (ti < 0 || static_cast<size_t>(ti) >= bsp.texinfos.size()) {
            continue;
        }
        const char* name = bsp.texinfos[static_cast<size_t>(ti)].texture;
        if (name[0] != '\0') {
            return std::string(name, strnlen(name, sizeof(bsp.texinfos[0].texture)));
        }
    }
    return {};
}

} // namespace

std::vector<std::array<float, 3>>
BrushGeometry::brushVertices(const Bsp& bsp, int brushIndex) {
    validateBrushIndex(bsp, brushIndex);
    const dbrush_t& brush = bsp.brushes[static_cast<size_t>(brushIndex)];

    std::vector<std::array<float, 3>> out;
    if (brush.numsides < 3) {
        return out;
    }

    for (int i = 0; i < brush.numsides; ++i) {
        const int bsiA = brush.firstside + i;
        if (bsiA < 0 || static_cast<size_t>(bsiA) >= bsp.brushsides.size()) continue;
        const unsigned short pA = bsp.brushsides[static_cast<size_t>(bsiA)].planenum;
        if (pA >= bsp.planes.size()) continue;
        for (int j = i + 1; j < brush.numsides; ++j) {
            const int bsiB = brush.firstside + j;
            if (bsiB < 0 || static_cast<size_t>(bsiB) >= bsp.brushsides.size()) continue;
            const unsigned short pB = bsp.brushsides[static_cast<size_t>(bsiB)].planenum;
            if (pB >= bsp.planes.size()) continue;
            for (int k = j + 1; k < brush.numsides; ++k) {
                const int bsiC = brush.firstside + k;
                if (bsiC < 0 || static_cast<size_t>(bsiC) >= bsp.brushsides.size()) continue;
                const unsigned short pC = bsp.brushsides[static_cast<size_t>(bsiC)].planenum;
                if (pC >= bsp.planes.size()) continue;

                Vec3 pt{};
                if (!intersectPlanes(bsp.planes[pA], bsp.planes[pB], bsp.planes[pC], pt)) continue;
                if (!pointInsideAllPlanes(pt, brush, bsp,
                                          static_cast<size_t>(i),
                                          static_cast<size_t>(j),
                                          static_cast<size_t>(k))) continue;
                out.push_back({ pt.x, pt.y, pt.z });
            }
        }
    }
    return out;
}

BrushAabb BrushGeometry::brushAabb(const Bsp& bsp, int brushIndex) {
    validateBrushIndex(bsp, brushIndex);
    const dbrush_t& brush = bsp.brushes[static_cast<size_t>(brushIndex)];

    auto verts = brushVertices(bsp, brushIndex);

    BrushAabb aabb{};
    aabb.modelIndex = 0;
    aabb.texname = textureNameForBrush(bsp, brush);

    if (verts.empty()) {
        aabb.mins = { 0.0f, 0.0f, 0.0f };
        aabb.maxs = { 0.0f, 0.0f, 0.0f };
        return aabb;
    }

    constexpr float kInf = std::numeric_limits<float>::infinity();
    aabb.mins = {  kInf,  kInf,  kInf };
    aabb.maxs = { -kInf, -kInf, -kInf };
    for (const auto& v : verts) {
        for (int c = 0; c < 3; ++c) {
            if (v[c] < aabb.mins[c]) aabb.mins[c] = v[c];
            if (v[c] > aabb.maxs[c]) aabb.maxs[c] = v[c];
        }
    }
    return aabb;
}

namespace {

// Epsilon for treating two direction vectors as antiparallel / orthogonal.
constexpr float kAxisDotEps = 1e-3f;

Vec3 normalize(const Vec3& v) {
    const float len = std::sqrt(dot(v, v));
    if (len < 1e-9f) return { 0.0f, 0.0f, 0.0f };
    return { v.x / len, v.y / len, v.z / len };
}

// Collect each face's outward plane normal as a unit vector, deduplicated up
// to sign (n and -n are the same axis direction). Treats any two normals
// with |n1 . n2| >= 1 - kAxisDotEps as the same direction.
std::vector<Vec3> collectFaceAxes(const Bsp& bsp, const dbrush_t& brush) {
    std::vector<Vec3> out;
    for (int s = 0; s < brush.numsides; ++s) {
        const int bsi = brush.firstside + s;
        if (bsi < 0 || static_cast<size_t>(bsi) >= bsp.brushsides.size()) continue;
        const unsigned short pn = bsp.brushsides[static_cast<size_t>(bsi)].planenum;
        if (pn >= bsp.planes.size()) continue;
        const dplane_t& pl = bsp.planes[pn];
        Vec3 n = normalize({ pl.normal[0], pl.normal[1], pl.normal[2] });
        if (std::fabs(n.x) + std::fabs(n.y) + std::fabs(n.z) < 1e-6f) continue;

        bool dup = false;
        for (const Vec3& e : out) {
            if (std::fabs(dot(e, n)) >= 1.0f - kAxisDotEps) { dup = true; break; }
        }
        if (!dup) out.push_back(n);
    }
    return out;
}

// Given a vertex cloud and an orthonormal frame, compute the OBB extent
// (size per local axis) and the world-space center (midpoint of the
// projected min/max along each axis, mapped back to world).
void fitObbToVerts(const std::vector<std::array<float, 3>>& verts,
                   const Vec3& ax0, const Vec3& ax1, const Vec3& ax2,
                   std::array<float, 3>& outCenter,
                   std::array<float, 3>& outSize) {
    constexpr float kInf = std::numeric_limits<float>::infinity();
    float mn[3] = {  kInf,  kInf,  kInf };
    float mx[3] = { -kInf, -kInf, -kInf };
    const Vec3 axes[3] = { ax0, ax1, ax2 };
    for (const auto& v : verts) {
        const Vec3 p{ v[0], v[1], v[2] };
        for (int k = 0; k < 3; ++k) {
            const float d = dot(axes[k], p);
            if (d < mn[k]) mn[k] = d;
            if (d > mx[k]) mx[k] = d;
        }
    }
    for (int k = 0; k < 3; ++k) {
        outSize[k] = mx[k] - mn[k];
    }
    // world center = sum_k ((mn[k]+mx[k])/2) * axes[k]
    Vec3 c{ 0.0f, 0.0f, 0.0f };
    for (int k = 0; k < 3; ++k) {
        const float cMid = (mn[k] + mx[k]) * 0.5f;
        c = add(c, scale(axes[k], cMid));
    }
    outCenter = { c.x, c.y, c.z };
}

// Put an orthogonal triple into a canonical order: assign each to the world
// axis it aligns with best, flip signs so each axis . world_i >= 0, and
// enforce right-handedness.
void canonicalizeTriple(const Vec3& a, const Vec3& b, const Vec3& c,
                        Vec3& o0, Vec3& o1, Vec3& o2) {
    const Vec3 worldAxes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    const Vec3 cand[3] = { a, b, c };
    int assigned[3] = { -1, -1, -1 };
    bool usedCand[3] = { false, false, false };
    for (int w = 0; w < 3; ++w) {
        int best = -1;
        float bestScore = -1.0f;
        for (int ci = 0; ci < 3; ++ci) {
            if (usedCand[ci]) continue;
            const float s = std::fabs(dot(cand[ci], worldAxes[w]));
            if (s > bestScore) { bestScore = s; best = ci; }
        }
        assigned[w] = best;
        if (best >= 0) usedCand[best] = true;
    }

    Vec3 out[3];
    for (int w = 0; w < 3; ++w) {
        Vec3 v = cand[assigned[w]];
        if (dot(v, worldAxes[w]) < 0.0f) v = scale(v, -1.0f);
        out[w] = v;
    }
    const Vec3 expectedZ = cross(out[0], out[1]);
    if (dot(expectedZ, out[2]) < 0.0f) out[2] = scale(out[2], -1.0f);
    o0 = out[0]; o1 = out[1]; o2 = out[2];
}

// Volume of the OBB fit to verts with the given orthonormal basis.
float obbVolume(const std::vector<std::array<float, 3>>& verts,
                const Vec3& ax0, const Vec3& ax1, const Vec3& ax2) {
    std::array<float, 3> c{};
    std::array<float, 3> sz{};
    fitObbToVerts(verts, ax0, ax1, ax2, c, sz);
    return sz[0] * sz[1] * sz[2];
}

// Generate all plausible OBB basis candidates from the deduped face normals.
// For a brush with 3 mutually orthogonal face normals (like an axis-aligned
// cube or a 45°-rotated box), those three form one candidate. For a brush
// with 2 perpendicular face normals but no third collinear with their cross
// product (like a tilted slab: 6 axis + 2 antiparallel tilted faces), we
// synthesize the third axis via cross product — this is the key case that
// makes bridge-support and chamfered-beam brushes produce a tight rotated
// fit instead of a bloated AABB.
std::vector<std::array<Vec3, 3>> candidateTriples(const std::vector<Vec3>& axes) {
    std::vector<std::array<Vec3, 3>> out;
    const size_t n = axes.size();

    auto alreadyPresent = [&](const Vec3& a0, const Vec3& a1, const Vec3& a2) {
        for (const auto& t : out) {
            auto sameDir = [](const Vec3& u, const Vec3& v) {
                return std::fabs(dot(u, v)) >= 1.0f - kAxisDotEps;
            };
            const Vec3 want[3] = { a0, a1, a2 };
            bool matched[3] = { false, false, false };
            int hits = 0;
            for (const Vec3& w : want) {
                for (int k = 0; k < 3; ++k) {
                    if (matched[k]) continue;
                    if (sameDir(w, t[k])) { matched[k] = true; ++hits; break; }
                }
            }
            if (hits == 3) return true;
        }
        return false;
    };

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (std::fabs(dot(axes[i], axes[j])) > kAxisDotEps) continue;
            // Synthesize third axis via cross product.
            const Vec3 synth = normalize(cross(axes[i], axes[j]));
            if (std::fabs(synth.x) + std::fabs(synth.y) + std::fabs(synth.z) < 1e-6f) continue;

            // Prefer a real face normal for the third axis if one exists
            // aligned with the cross product — this produces nicer output
            // for axis-aligned brushes (identity rotation instead of a
            // permuted/flipped variant).
            Vec3 third = synth;
            for (size_t k = 0; k < n; ++k) {
                if (k == i || k == j) continue;
                if (std::fabs(dot(axes[k], synth)) >= 1.0f - kAxisDotEps) {
                    third = axes[k];
                    if (dot(third, synth) < 0.0f) third = scale(third, -1.0f);
                    break;
                }
            }

            Vec3 c0, c1, c2;
            canonicalizeTriple(axes[i], axes[j], third, c0, c1, c2);
            if (!alreadyPresent(c0, c1, c2)) {
                out.push_back({ c0, c1, c2 });
            }
        }
    }

    if (out.empty()) {
        // Fallback: world axes.
        Vec3 c0, c1, c2;
        canonicalizeTriple({1,0,0}, {0,1,0}, {0,0,1}, c0, c1, c2);
        out.push_back({ c0, c1, c2 });
    }
    return out;
}

} // namespace

BrushObb BrushGeometry::brushObb(const Bsp& bsp, int brushIndex) {
    validateBrushIndex(bsp, brushIndex);
    const dbrush_t& brush = bsp.brushes[static_cast<size_t>(brushIndex)];

    BrushObb obb{};
    obb.modelIndex = 0;
    obb.texname = textureNameForBrush(bsp, brush);
    obb.rotation = { 1, 0, 0,  0, 1, 0,  0, 0, 1 };

    const auto verts = brushVertices(bsp, brushIndex);
    if (verts.empty()) {
        obb.center = { 0.0f, 0.0f, 0.0f };
        obb.size   = { 0.0f, 0.0f, 0.0f };
        return obb;
    }

    // Collect face-normal axes and always include the world axes as a
    // degenerate fallback candidate — guarantees we consider AABB as an
    // option even for brushes with few face normals.
    auto axes = collectFaceAxes(bsp, brush);
    auto pushIfMissing = [&](const Vec3& n) {
        for (const Vec3& e : axes) {
            if (std::fabs(dot(e, n)) >= 1.0f - kAxisDotEps) return;
        }
        axes.push_back(n);
    };
    pushIfMissing({1, 0, 0});
    pushIfMissing({0, 1, 0});
    pushIfMissing({0, 0, 1});

    const auto candidates = candidateTriples(axes);

    // Pick the best candidate: smallest volume, tie-broken by thinnest axis.
    // The tie-breaker matters for triangular-prism / chamfer brushes, where
    // the AABB and the hypotenuse-aligned OBB have the same volume but the
    // rotated OBB has a thinner slab axis — which visually represents the
    // chamfer cut in Studio instead of the AABB's fat rectangular filler.
    float bestVol  = std::numeric_limits<float>::infinity();
    float bestThin = std::numeric_limits<float>::infinity();
    Vec3 ax0{1,0,0}, ax1{0,1,0}, ax2{0,0,1};
    for (const auto& t : candidates) {
        std::array<float, 3> c{};
        std::array<float, 3> sz{};
        fitObbToVerts(verts, t[0], t[1], t[2], c, sz);
        const float vol  = sz[0] * sz[1] * sz[2];
        const float thin = std::min({ sz[0], sz[1], sz[2] });

        const float volRatio = bestVol > 0 ? vol / bestVol : 1.0f;
        const bool strictlySmaller = vol < bestVol - 1e-4f;
        const bool tied = !strictlySmaller && volRatio < 1.01f;
        if (strictlySmaller || (tied && thin < bestThin - 1e-4f)) {
            bestVol  = vol;
            bestThin = thin;
            ax0 = t[0]; ax1 = t[1]; ax2 = t[2];
        }
    }

    fitObbToVerts(verts, ax0, ax1, ax2, obb.center, obb.size);
    obb.rotation = {
        ax0.x, ax1.x, ax2.x,
        ax0.y, ax1.y, ax2.y,
        ax0.z, ax1.z, ax2.z,
    };
    return obb;
}

} // namespace bsp2rbx
