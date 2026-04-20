#include "bsp2rbx/SolidWorldspawnFilter.h"

#include <cstring>

namespace bsp2rbx {

namespace {

bool texnameStartsWith(const char* tex, const char* prefix) {
    const size_t n = strnlen(prefix, 32);
    return std::strncmp(tex, prefix, n) == 0;
}

bool sideHasRejectedTex(const Bsp& bsp, const dbrushside_t& side) {
    if (side.texinfo < 0 || static_cast<size_t>(side.texinfo) >= bsp.texinfos.size()) {
        return false;
    }
    const texinfo_t& ti = bsp.texinfos[static_cast<size_t>(side.texinfo)];
    if (ti.flags & SURF_SKY) return true;
    if (ti.flags & SURF_NODRAW) return true;
    const char* name = ti.texture;
    if (texnameStartsWith(name, "sky"))      return true;
    if (texnameStartsWith(name, "trigger"))  return true;
    if (texnameStartsWith(name, "clip"))     return true;
    if (texnameStartsWith(name, "origin"))   return true;
    if (texnameStartsWith(name, "hint"))     return true;
    if (texnameStartsWith(name, "skip"))     return true;
    return false;
}

} // namespace

bool SolidWorldspawnFilter::keep(const Bsp& bsp, int brushIndex) {
    if (brushIndex < 0 || static_cast<size_t>(brushIndex) >= bsp.brushes.size()) {
        return false;
    }
    const dbrush_t& brush = bsp.brushes[static_cast<size_t>(brushIndex)];
    if ((brush.contents & CONTENTS_SOLID) == 0) return false;
    if (brush.contents & (CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP)) return false;

    for (int s = 0; s < brush.numsides; ++s) {
        const int bsi = brush.firstside + s;
        if (bsi < 0 || static_cast<size_t>(bsi) >= bsp.brushsides.size()) continue;
        if (sideHasRejectedTex(bsp, bsp.brushsides[static_cast<size_t>(bsi)])) {
            return false;
        }
    }
    return true;
}

} // namespace bsp2rbx
