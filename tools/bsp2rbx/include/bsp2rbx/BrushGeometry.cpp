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

} // namespace bsp2rbx
