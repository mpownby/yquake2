#include "bsp2rbx/BspParser.h"

#include <cstring>
#include <stdexcept>

namespace bsp2rbx {

namespace {

static_assert(sizeof(dheader_t)    == 4 + 4 + 19 * 8, "dheader_t layout");
static_assert(sizeof(dvertex_t)    == 12,              "dvertex_t layout");
static_assert(sizeof(dplane_t)     == 20,              "dplane_t layout");
static_assert(sizeof(dbrush_t)     == 12,              "dbrush_t layout");
static_assert(sizeof(dbrushside_t) == 4,               "dbrushside_t layout");
static_assert(sizeof(dmodel_t)     == 48,              "dmodel_t layout");
static_assert(sizeof(dface_t)      == 20,              "dface_t layout");
static_assert(sizeof(dedge_t)      == 4,               "dedge_t layout");
static_assert(sizeof(texinfo_t)    == 76,              "texinfo_t layout");

template <class T>
std::vector<T> copyLump(const std::vector<uint8_t>& bytes, const lump_t& lump, const char* name) {
    if (lump.filelen < 0 || lump.fileofs < 0) {
        throw std::runtime_error(std::string("BspParser: negative lump bounds for ") + name);
    }
    if (static_cast<size_t>(lump.fileofs) + static_cast<size_t>(lump.filelen) > bytes.size()) {
        throw std::runtime_error(std::string("BspParser: lump overflow for ") + name);
    }
    if (lump.filelen % sizeof(T) != 0) {
        throw std::runtime_error(std::string("BspParser: lump size not a multiple of element for ") + name);
    }
    const size_t count = static_cast<size_t>(lump.filelen) / sizeof(T);
    std::vector<T> out(count);
    if (count > 0) {
        std::memcpy(out.data(), bytes.data() + lump.fileofs, static_cast<size_t>(lump.filelen));
    }
    return out;
}

} // namespace

std::unique_ptr<Bsp> BspParser::parse(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < sizeof(dheader_t)) {
        throw std::runtime_error("BspParser: file smaller than BSP header");
    }

    dheader_t header{};
    std::memcpy(&header, bytes.data(), sizeof(header));

    if (header.ident != IDBSPHEADER) {
        throw std::runtime_error("BspParser: bad magic (expected IBSP)");
    }
    if (header.version != BSPVERSION) {
        throw std::runtime_error("BspParser: unsupported BSP version " + std::to_string(header.version));
    }

    auto bsp = std::make_unique<Bsp>();
    bsp->vertices   = copyLump<dvertex_t>   (bytes, header.lumps[LUMP_VERTEXES],   "vertexes");
    bsp->planes     = copyLump<dplane_t>    (bytes, header.lumps[LUMP_PLANES],     "planes");
    bsp->brushes    = copyLump<dbrush_t>    (bytes, header.lumps[LUMP_BRUSHES],    "brushes");
    bsp->brushsides = copyLump<dbrushside_t>(bytes, header.lumps[LUMP_BRUSHSIDES], "brushsides");
    bsp->models     = copyLump<dmodel_t>    (bytes, header.lumps[LUMP_MODELS],     "models");
    bsp->faces      = copyLump<dface_t>     (bytes, header.lumps[LUMP_FACES],      "faces");
    bsp->edges      = copyLump<dedge_t>     (bytes, header.lumps[LUMP_EDGES],      "edges");
    bsp->surfedges  = copyLump<int>         (bytes, header.lumps[LUMP_SURFEDGES],  "surfedges");
    bsp->texinfos   = copyLump<texinfo_t>   (bytes, header.lumps[LUMP_TEXINFO],    "texinfo");

    const lump_t& ents = header.lumps[LUMP_ENTITIES];
    if (ents.filelen < 0 || ents.fileofs < 0 ||
        static_cast<size_t>(ents.fileofs) + static_cast<size_t>(ents.filelen) > bytes.size()) {
        throw std::runtime_error("BspParser: entity lump out of bounds");
    }
    bsp->entityString.assign(
        reinterpret_cast<const char*>(bytes.data() + ents.fileofs),
        static_cast<size_t>(ents.filelen));
    if (!bsp->entityString.empty() && bsp->entityString.back() == '\0') {
        bsp->entityString.pop_back();
    }

    return bsp;
}

} // namespace bsp2rbx
