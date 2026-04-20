#include "bsp2rbx/BrushGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

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

Vec3 sub(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
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

namespace {

struct FaceVerts {
    Vec3              normal;   // outward, normalized
    float             dist;     // plane distance: dot(normal, p) <= dist inside
    std::vector<int>  vertIdx;  // indices into the brush vertex cloud
};

// For each brush side, build a FaceVerts entry listing which of the brush's
// computed vertices lie on that face's plane (within kEpsilon). Returns empty
// if any side references invalid plane / brushside indices.
std::vector<FaceVerts> collectFaceVertSets(const Bsp& bsp,
                                           const dbrush_t& brush,
                                           const std::vector<std::array<float, 3>>& verts) {
    std::vector<FaceVerts> faces;
    for (int s = 0; s < brush.numsides; ++s) {
        const int bsi = brush.firstside + s;
        if (bsi < 0 || static_cast<size_t>(bsi) >= bsp.brushsides.size()) return {};
        const unsigned short pn = bsp.brushsides[static_cast<size_t>(bsi)].planenum;
        if (pn >= bsp.planes.size()) return {};
        const dplane_t& pl = bsp.planes[pn];
        FaceVerts fv;
        fv.normal = normalize({ pl.normal[0], pl.normal[1], pl.normal[2] });
        fv.dist   = pl.dist;
        if (std::fabs(fv.normal.x) + std::fabs(fv.normal.y) + std::fabs(fv.normal.z) < 1e-6f) {
            return {};
        }
        for (int v = 0; v < static_cast<int>(verts.size()); ++v) {
            const Vec3 p{ verts[static_cast<size_t>(v)][0],
                          verts[static_cast<size_t>(v)][1],
                          verts[static_cast<size_t>(v)][2] };
            const float d = dot(fv.normal, p) - fv.dist;
            if (std::fabs(d) < kEpsilon) fv.vertIdx.push_back(v);
        }
        faces.push_back(fv);
    }
    return faces;
}

Vec3 vertAt(const std::vector<std::array<float, 3>>& verts, int i) {
    return { verts[static_cast<size_t>(i)][0],
             verts[static_cast<size_t>(i)][1],
             verts[static_cast<size_t>(i)][2] };
}

// brushVertices returns one entry per accepted plane-triple intersection, so
// a hull vertex shared by N>=3 planes appears C(N,3) times. Collapse those to
// unique points within kEpsilon for the prism analysis.
std::vector<std::array<float, 3>>
dedupVerts(const std::vector<std::array<float, 3>>& in) {
    std::vector<std::array<float, 3>> out;
    out.reserve(in.size());
    for (const auto& v : in) {
        bool dup = false;
        for (const auto& e : out) {
            if (std::fabs(e[0] - v[0]) < kEpsilon &&
                std::fabs(e[1] - v[1]) < kEpsilon &&
                std::fabs(e[2] - v[2]) < kEpsilon) {
                dup = true;
                break;
            }
        }
        if (!dup) out.push_back(v);
    }
    return out;
}

} // namespace

std::optional<BrushWedge> BrushGeometry::brushWedge(const Bsp& bsp, int brushIndex) {
    validateBrushIndex(bsp, brushIndex);
    const dbrush_t& brush = bsp.brushes[static_cast<size_t>(brushIndex)];

    // A right-triangular prism has 6 unique hull vertices (3 per triangle
    // face). The brush can have *more* than 5 sides — Q2's qbsp tacks on
    // axis-aligned bevel planes for collision, so a 5-defining-plane prism
    // commonly has 7+ sides total. Bevel planes are tangent to the hull and
    // contain at most 2 hull vertices, so they're filtered out below by
    // the face-vertex-count check.
    const auto verts = dedupVerts(brushVertices(bsp, brushIndex));
    if (verts.size() != 6) return std::nullopt;

    const auto faces = collectFaceVertSets(bsp, brush, verts);
    if (faces.empty()) return std::nullopt;

    int triFace[2] = { -1, -1 };
    int triCount  = 0;
    int rectCount = 0;
    for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
        if (faces[i].vertIdx.size() == 3) {
            if (triCount < 2) triFace[triCount] = i;
            ++triCount;
        } else if (faces[i].vertIdx.size() == 4) {
            ++rectCount;
        }
    }
    if (triCount != 2 || rectCount != 3) return std::nullopt;

    const FaceVerts& fa = faces[triFace[0]];
    const FaceVerts& fb = faces[triFace[1]];

    // The two triangles must partition the 6-vertex hull (3+3, no overlap).
    // Without this, two coplanar 3-vertex faces could pass the count check
    // for a degenerate brush.
    {
        bool seen[6] = { false, false, false, false, false, false };
        int  total   = 0;
        for (int v : fa.vertIdx) if (v >= 0 && v < 6 && !seen[v]) { seen[v] = true; ++total; }
        for (int v : fb.vertIdx) if (v >= 0 && v < 6 && !seen[v]) { seen[v] = true; ++total; }
        if (total != 6) return std::nullopt;
    }
    if (dot(fa.normal, fb.normal) > -1.0f + kAxisDotEps) return std::nullopt;

    const Vec3 t0 = vertAt(verts, fa.vertIdx[0]);
    const Vec3 t1 = vertAt(verts, fa.vertIdx[1]);
    const Vec3 t2 = vertAt(verts, fa.vertIdx[2]);
    const Vec3 tri[3] = { t0, t1, t2 };

    int rightIdx = -1;
    for (int i = 0; i < 3; ++i) {
        const Vec3 e1 = sub(tri[(i + 1) % 3], tri[i]);
        const Vec3 e2 = sub(tri[(i + 2) % 3], tri[i]);
        const float n1 = std::sqrt(dot(e1, e1));
        const float n2 = std::sqrt(dot(e2, e2));
        if (n1 < 1e-4f || n2 < 1e-4f) return std::nullopt;
        const float c = dot(e1, e2) / (n1 * n2);
        if (std::fabs(c) < 1e-2f) { rightIdx = i; break; }
    }
    if (rightIdx < 0) return std::nullopt;

    Vec3 axisX = scale(fa.normal, -1.0f);
    const Vec3 otherCenter = scale(add(add(vertAt(verts, fb.vertIdx[0]),
                                           vertAt(verts, fb.vertIdx[1])),
                                       vertAt(verts, fb.vertIdx[2])),
                                   1.0f / 3.0f);
    if (dot(axisX, sub(otherCenter, tri[rightIdx])) < 0.0f) {
        axisX = scale(axisX, -1.0f);
    }
    const float prismLength = std::fabs(dot(axisX, sub(otherCenter, tri[rightIdx])));
    if (prismLength < 1e-4f) return std::nullopt;

    Vec3 e1raw = sub(tri[(rightIdx + 1) % 3], tri[rightIdx]);
    Vec3 e2raw = sub(tri[(rightIdx + 2) % 3], tri[rightIdx]);
    float legA = std::sqrt(dot(e1raw, e1raw));
    float legB = std::sqrt(dot(e2raw, e2raw));
    Vec3 axisY = scale(e1raw, 1.0f / legA);
    Vec3 axisZ = scale(e2raw, 1.0f / legB);

    if (dot(cross(axisX, axisY), axisZ) < 0.0f) {
        std::swap(axisY, axisZ);
        std::swap(legA,  legB);
    }

    const Vec3 c = add(add(add(tri[rightIdx],
                               scale(axisX, prismLength * 0.5f)),
                           scale(axisY, legA * 0.5f)),
                       scale(axisZ, legB * 0.5f));

    BrushWedge w{};
    w.center = { c.x, c.y, c.z };
    w.size   = { prismLength, legA, legB };
    w.rotation = {
        axisX.x, axisY.x, axisZ.x,
        axisX.y, axisY.y, axisZ.y,
        axisX.z, axisY.z, axisZ.z,
    };
    w.modelIndex = 0;
    w.texname    = textureNameForBrush(bsp, brush);
    return w;
}

namespace {

// Walk a face's vertex set and return them ordered around the face perimeter
// (counter-clockwise when viewed from outside, i.e. against the face normal).
// Falls back to empty if the input doesn't form a simple polygon.
std::vector<Vec3> orderFacePerimeter(const std::vector<std::array<float, 3>>& verts,
                                     const FaceVerts& face) {
    const size_t n = face.vertIdx.size();
    if (n < 3) return {};

    std::vector<Vec3> p;
    p.reserve(n);
    for (int i : face.vertIdx) p.push_back(vertAt(verts, i));

    // Compute centroid.
    Vec3 c{ 0, 0, 0 };
    for (const Vec3& v : p) c = add(c, v);
    c = scale(c, 1.0f / static_cast<float>(n));

    // Build a 2D basis on the face plane from the first edge direction.
    Vec3 u = normalize(sub(p[0], c));
    if (std::fabs(u.x) + std::fabs(u.y) + std::fabs(u.z) < 1e-6f) return {};
    Vec3 v = normalize(cross(face.normal, u));

    // Sort by polar angle around the centroid in (u, v) — gives CCW order
    // when viewed against the face normal.
    std::vector<std::pair<float, Vec3>> withAngle;
    withAngle.reserve(n);
    for (const Vec3& q : p) {
        const Vec3 d = sub(q, c);
        const float du = dot(d, u);
        const float dv = dot(d, v);
        withAngle.emplace_back(std::atan2(dv, du), q);
    }
    std::sort(withAngle.begin(), withAngle.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<Vec3> ordered;
    ordered.reserve(n);
    for (const auto& wa : withAngle) ordered.push_back(wa.second);
    return ordered;
}

struct PentagonAnalysis {
    std::array<Vec3, 5>  verts;          // ordered around perimeter
    int                  missingCorner;  // index 0..3 of the absent bbox corner
    Vec3                 axisU, axisV;   // 2D basis on the pentagon plane
    float                bbMinU, bbMaxU, bbMinV, bbMaxV;
    int                  chamferAtV0;    // pentagon vertex on the U-side of missing corner
    int                  chamferAtV1;    // pentagon vertex on the V-side of missing corner
};

// Given a 5-vertex polygon ordered around its perimeter, decide whether it
// looks like an axis-aligned rectangle with one corner cut off by a single
// chamfer plane. If yes, fill out PentagonAnalysis with the basis, bbox
// corners, and the indices (0..4 in `pent.verts`) of the two vertices that
// flank the chamfer edge. Returns false otherwise.
bool analyzePentagon(const std::array<Vec3, 5>& pent,
                     const Vec3& planeNormal,
                     PentagonAnalysis& out) {
    out.verts = pent;

    // Compute the 5 edge directions (unit vectors).
    std::array<Vec3, 5> dirs;
    std::array<float, 5> lens;
    for (int i = 0; i < 5; ++i) {
        const Vec3 e = sub(pent[(i + 1) % 5], pent[i]);
        lens[i] = std::sqrt(dot(e, e));
        if (lens[i] < 1e-4f) return false;
        dirs[i] = scale(e, 1.0f / lens[i]);
    }

    // The rectangle's two axes are the two distinct edge directions that
    // each appear on at least 2 of the 5 pentagon edges (since every
    // rectangle side has a parallel partner). The chamfer is the lone
    // edge with no parallel partner. Picking axisU/axisV from parallel
    // pairs is essential when the chamfer happens to be the longest
    // pentagon edge — using the longest edge directly would land axisU on
    // the chamfer and produce a rotated bbox unrelated to the rectangle's
    // natural orientation.
    constexpr float kParallelEps = 1e-2f;
    int chamferEdge = -1;
    int axisAEdge = -1, axisBEdge = -1;
    for (int i = 0; i < 5; ++i) {
        bool hasPartner = false;
        for (int j = 0; j < 5; ++j) {
            if (i == j) continue;
            if (std::fabs(dot(dirs[i], dirs[j])) >= 1.0f - kParallelEps) {
                hasPartner = true;
                break;
            }
        }
        if (!hasPartner) {
            if (chamferEdge >= 0) return false;  // 2+ unpaired edges
            chamferEdge = i;
        }
    }
    if (chamferEdge < 0) return false;

    axisAEdge = (chamferEdge + 1) % 5;  // first edge after the chamfer
    // Find any edge perpendicular to axisAEdge (and not the chamfer).
    for (int i = 0; i < 5; ++i) {
        if (i == chamferEdge || i == axisAEdge) continue;
        if (std::fabs(dot(dirs[i], dirs[axisAEdge])) < kParallelEps) {
            axisBEdge = i;
            break;
        }
    }
    if (axisBEdge < 0) return false;

    out.axisU = dirs[axisAEdge];
    out.axisV = dirs[axisBEdge];
    // Make axisV the perpendicular-in-plane direction; flip if needed so
    // the basis (axisU, axisV, planeNormal) is right-handed.
    const Vec3 perp = normalize(cross(planeNormal, out.axisU));
    if (dot(perp, out.axisV) < 0.0f) out.axisV = scale(out.axisV, -1.0f);

    // Project every vertex into 2D (U, V) about pent[0].
    std::array<float, 5> pu{}, pv{};
    for (int i = 0; i < 5; ++i) {
        const Vec3 d = sub(pent[i], pent[0]);
        pu[i] = dot(d, out.axisU);
        pv[i] = dot(d, out.axisV);
    }

    // Bounding box in local 2D.
    float uMin =  std::numeric_limits<float>::infinity();
    float uMax = -std::numeric_limits<float>::infinity();
    float vMin =  std::numeric_limits<float>::infinity();
    float vMax = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < 5; ++i) {
        if (pu[i] < uMin) uMin = pu[i];
        if (pu[i] > uMax) uMax = pu[i];
        if (pv[i] < vMin) vMin = pv[i];
        if (pv[i] > vMax) vMax = pv[i];
    }
    out.bbMinU = uMin; out.bbMaxU = uMax;
    out.bbMinV = vMin; out.bbMaxV = vMax;

    // The 4 bbox corners. Three should coincide with pentagon vertices;
    // the 4th is the chamfer-removed corner.
    const float corners[4][2] = {
        { uMin, vMin }, { uMax, vMin }, { uMax, vMax }, { uMin, vMax }
    };
    int matchCount[4] = { 0, 0, 0, 0 };
    int matchedVert[4] = { -1, -1, -1, -1 };
    constexpr float kEps2D = 0.05f;
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i < 5; ++i) {
            if (std::fabs(pu[i] - corners[c][0]) < kEps2D &&
                std::fabs(pv[i] - corners[c][1]) < kEps2D) {
                ++matchCount[c];
                matchedVert[c] = i;
            }
        }
    }

    int matched = 0, missing = -1;
    for (int c = 0; c < 4; ++c) {
        if (matchCount[c] == 1) ++matched;
        else if (matchCount[c] == 0) missing = c;
        else return false;  // duplicate match — degenerate pentagon
    }
    if (matched != 3 || missing < 0) return false;
    out.missingCorner = missing;

    // The two pentagon vertices that aren't matched to a bbox corner are
    // the chamfer endpoints. They must be adjacent in the pentagon order.
    bool isCorner[5] = { false, false, false, false, false };
    for (int c = 0; c < 4; ++c) {
        if (matchedVert[c] >= 0) isCorner[matchedVert[c]] = true;
    }
    int cham[2] = { -1, -1 };
    int ci = 0;
    for (int i = 0; i < 5; ++i) {
        if (!isCorner[i]) {
            if (ci >= 2) return false;
            cham[ci++] = i;
        }
    }
    if (ci != 2) return false;
    if (!((cham[0] + 1) % 5 == cham[1] || (cham[1] + 1) % 5 == cham[0])) {
        return false;  // chamfer endpoints not adjacent — not a single-cut pentagon
    }

    // Each chamfer endpoint lies on the rectangle side adjacent to the
    // missing corner. Identify which is on the U-side vs V-side.
    const float missU = corners[missing][0];
    const float missV = corners[missing][1];
    auto onUSide = [&](int i) {
        return std::fabs(pv[i] - missV) < kEps2D;  // same V as missing corner -> on U-side edge
    };
    auto onVSide = [&](int i) {
        return std::fabs(pu[i] - missU) < kEps2D;  // same U as missing corner -> on V-side edge
    };
    if (onUSide(cham[0]) && onVSide(cham[1])) {
        out.chamferAtV0 = cham[0]; out.chamferAtV1 = cham[1];
    } else if (onUSide(cham[1]) && onVSide(cham[0])) {
        out.chamferAtV0 = cham[1]; out.chamferAtV1 = cham[0];
    } else {
        return false;
    }
    return true;
}

} // namespace

std::optional<BrushDecomposition>
BrushGeometry::brushChamferedBeam(const Bsp& bsp, int brushIndex) {
    validateBrushIndex(bsp, brushIndex);
    const dbrush_t& brush = bsp.brushes[static_cast<size_t>(brushIndex)];

    // A pentagonal-prism brush with one chamfered edge has 10 unique hull
    // vertices (5 per pentagon) and 7 faces (2 pentagons + 5 rectangles).
    // Bevel planes from qbsp may add extra sides with at most 2 hull
    // vertices, which the face-classification below ignores.
    const auto verts = dedupVerts(brushVertices(bsp, brushIndex));
    if (verts.size() != 10) return std::nullopt;

    const auto faces = collectFaceVertSets(bsp, brush, verts);
    if (faces.empty()) return std::nullopt;

    int pentFace[2] = { -1, -1 };
    int pentCount = 0;
    int rectCount = 0;
    for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
        const size_t n = faces[i].vertIdx.size();
        if (n == 5) { if (pentCount < 2) pentFace[pentCount] = i; ++pentCount; }
        else if (n == 4) { ++rectCount; }
    }
    if (pentCount != 2 || rectCount != 5) return std::nullopt;

    const FaceVerts& fa = faces[pentFace[0]];
    const FaceVerts& fb = faces[pentFace[1]];
    if (dot(fa.normal, fb.normal) > -1.0f + kAxisDotEps) return std::nullopt;

    // The two pentagons must partition the 10 hull vertices (5 + 5, no
    // overlap). Otherwise we have something other than a clean prism.
    {
        bool seen[10] = { false, false, false, false, false,
                          false, false, false, false, false };
        int total = 0;
        for (int v : fa.vertIdx) if (v >= 0 && v < 10 && !seen[v]) { seen[v] = true; ++total; }
        for (int v : fb.vertIdx) if (v >= 0 && v < 10 && !seen[v]) { seen[v] = true; ++total; }
        if (total != 10) return std::nullopt;
    }

    // Order pentagon A's vertices around its perimeter and analyze.
    auto orderedA = orderFacePerimeter(verts, fa);
    if (orderedA.size() != 5) return std::nullopt;
    std::array<Vec3, 5> pentA{ orderedA[0], orderedA[1], orderedA[2], orderedA[3], orderedA[4] };

    PentagonAnalysis pa{};
    if (!analyzePentagon(pentA, fa.normal, pa)) return std::nullopt;

    // Prism axis points from pentagon A toward pentagon B (face A's outward
    // normal is fa.normal — flip toward fb.normal which is the opposite end).
    Vec3 prismAxis = scale(fa.normal, -1.0f);
    // Length is the perpendicular distance between the two pentagon planes.
    const float prismLength = std::fabs(fa.dist + fb.dist);
    if (prismLength < 1e-4f) return std::nullopt;

    // 2D pentagon (in U/V on pentagon A) → three pieces:
    //   piece 0 = lower box  spanning the full bbox in U, partial in V
    //   piece 1 = upper box  spanning partial in U, partial in V
    //   piece 2 = wedge      filling the chamfer slope
    // The geometry depends on which corner of the bbox is missing. Treat
    // the missing corner as (uHi, vHi) and reflect at the end.
    const float uMin = pa.bbMinU, uMax = pa.bbMaxU;
    const float vMin = pa.bbMinV, vMax = pa.bbMaxV;
    const float uExt = uMax - uMin;
    const float vExt = vMax - vMin;
    if (uExt < 1e-4f || vExt < 1e-4f) return std::nullopt;

    // The chamfer endpoints in 2D.
    auto to2D = [&](const Vec3& p) {
        const Vec3 d = sub(p, pentA[0]);
        return std::pair<float, float>(dot(d, pa.axisU), dot(d, pa.axisV));
    };
    const auto chU = to2D(pentA[pa.chamferAtV0]);  // on U-side edge: shares V with missing corner
    const auto chV = to2D(pentA[pa.chamferAtV1]);  // on V-side edge: shares U with missing corner

    // Map missing-corner location to a canonical (uHi, vHi) in [0..uExt] x [0..vExt],
    // measuring the chamfer cut sizes:
    //   cutU = how far in U the chamfer reaches from the missing corner
    //   cutV = how far in V the chamfer reaches from the missing corner
    float cutU = 0.0f, cutV = 0.0f;
    bool missAtUMax = false, missAtVMax = false;
    switch (pa.missingCorner) {
        case 0: missAtUMax = false; missAtVMax = false; break;  // (uMin, vMin)
        case 1: missAtUMax = true;  missAtVMax = false; break;  // (uMax, vMin)
        case 2: missAtUMax = true;  missAtVMax = true;  break;  // (uMax, vMax)
        case 3: missAtUMax = false; missAtVMax = true;  break;  // (uMin, vMax)
    }
    {
        const float missU = missAtUMax ? uMax : uMin;
        const float missV = missAtVMax ? vMax : vMin;
        cutU = std::fabs(chU.first  - missU);
        cutV = std::fabs(chV.second - missV);
    }
    if (cutU < 1e-4f || cutV < 1e-4f) return std::nullopt;
    if (cutU > uExt - 1e-4f || cutV > vExt - 1e-4f) {
        // Cut consumes the whole side -> the brush is actually a wedge or
        // smaller box; let the wedge detector or OBB handle it.
        return std::nullopt;
    }

    // Build the three pieces in local (U, V) coords. Anchor the 2D layout
    // at the missing corner so signs work uniformly: signU/signV point from
    // the missing corner inward.
    const float signU = missAtUMax ? -1.0f : +1.0f;
    const float signV = missAtVMax ? -1.0f : +1.0f;
    const float anchU = missAtUMax ? uMax : uMin;
    const float anchV = missAtVMax ? vMax : vMin;

    // Pentagon shape (relative to anchor, with signs absorbed):
    //   full box = [0..uExt] x [0..vExt]
    //   chamfer cuts a triangle at the (0, 0) corner with legs (cutU, cutV)
    //
    // Decomposition:
    //   Piece A (box, "far in V"): u in [0..uExt],   v in [cutV..vExt]
    //   Piece B (box, "near in V"): u in [cutU..uExt], v in [0..cutV]
    //   Piece C (wedge): right triangle with legs (cutU, cutV) at the chamfer
    //                    -- right-angle vertex at (cutU, cutV), legs going
    //                    along -U (length cutU) and -V (length cutV)
    struct Box2D { float uCenter, vCenter, uSize, vSize; };
    struct Wedge2D { float uRight, vRight, uLegSign, vLegSign, uLeg, vLeg; };

    Box2D pieceA{ uExt * 0.5f,
                  cutV + (vExt - cutV) * 0.5f,
                  uExt,
                  vExt - cutV };
    Box2D pieceB{ cutU + (uExt - cutU) * 0.5f,
                  cutV * 0.5f,
                  uExt - cutU,
                  cutV };
    // Wedge: in local 2D, right-angle vertex at (cutU, cutV); chamfer goes
    // from (0, cutV) to (cutU, 0). That's a triangle with vertices
    // (0, cutV), (cutU, cutV), (cutU, 0). Right angle at (cutU, cutV).
    Wedge2D pieceC{ cutU, cutV, -1.0f, -1.0f, cutU, cutV };

    // Lift each piece into world coordinates.
    // World-space center of pentagon A's anchor:
    const Vec3 anchorA = add(pentA[0],
                             add(scale(pa.axisU, anchU),
                                 scale(pa.axisV, anchV)));
    // Apply the (signU, signV) reflections by flipping basis vectors.
    const Vec3 axU = scale(pa.axisU, signU);
    const Vec3 axV = scale(pa.axisV, signV);

    auto liftBox = [&](const Box2D& b) {
        BrushPiece bp{};
        bp.kind = BrushPiece::Kind::Part;
        // Center of the box on pentagon A:
        const Vec3 ca = add(anchorA, add(scale(axU, b.uCenter), scale(axV, b.vCenter)));
        // The piece spans the full prism length along prismAxis. Center
        // sits halfway along the prism.
        const Vec3 c = add(ca, scale(prismAxis, prismLength * 0.5f));
        bp.center = { c.x, c.y, c.z };
        bp.size   = { prismLength, b.uSize, b.vSize };
        // Local axes: prism axis -> X, axU -> Y, axV -> Z. Right-handed if
        // det(prismAxis, axU, axV) > 0; otherwise swap U/V to fix.
        Vec3 X = prismAxis, Y = axU, Z = axV;
        if (dot(cross(X, Y), Z) < 0.0f) {
            std::swap(Y, Z);
            std::swap(bp.size[1], bp.size[2]);
        }
        bp.rotation = {
            X.x, Y.x, Z.x,
            X.y, Y.y, Z.y,
            X.z, Y.z, Z.z,
        };
        return bp;
    };

    auto liftWedge = [&](const Wedge2D& w) {
        BrushPiece bp{};
        bp.kind = BrushPiece::Kind::Wedge;
        // Right-angle vertex on pentagon A in 2D = (uRight, vRight); in world =
        const Vec3 raA = add(anchorA, add(scale(axU, w.uRight), scale(axV, w.vRight)));
        // Wedge axes (matching BrushWedge convention from phase 1):
        //   local +X = prism axis (length = prismLength)
        //   local +Y = leg direction along U (signed by w.uLegSign)
        //   local +Z = leg direction along V (signed by w.vLegSign)
        // Right-angle vertex on the -X triangle face is at local (-X/2, -Y/2, -Z/2).
        Vec3 axisX = prismAxis;
        Vec3 axisY = scale(axU, w.uLegSign);
        Vec3 axisZ = scale(axV, w.vLegSign);
        float legY = w.uLeg;
        float legZ = w.vLeg;
        if (dot(cross(axisX, axisY), axisZ) < 0.0f) {
            std::swap(axisY, axisZ);
            std::swap(legY,  legZ);
        }
        // Center = right-angle vertex + axisX*L/2 + axisY*legY/2 + axisZ*legZ/2.
        const Vec3 c = add(add(add(raA,
                                   scale(axisX, prismLength * 0.5f)),
                               scale(axisY, legY * 0.5f)),
                           scale(axisZ, legZ * 0.5f));
        bp.center = { c.x, c.y, c.z };
        bp.size   = { prismLength, legY, legZ };
        bp.rotation = {
            axisX.x, axisY.x, axisZ.x,
            axisX.y, axisY.y, axisZ.y,
            axisX.z, axisY.z, axisZ.z,
        };
        return bp;
    };

    BrushDecomposition d{};
    d.modelIndex = 0;
    d.texname    = textureNameForBrush(bsp, brush);
    d.pieces.push_back(liftBox(pieceA));
    d.pieces.push_back(liftBox(pieceB));
    d.pieces.push_back(liftWedge(pieceC));
    return d;
}

} // namespace bsp2rbx
