#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "bsp2rbx/BspConverter.h"
#include "bsp2rbx/BspParser.h"
#include "bsp2rbx/BrushGeometry.h"
#include "bsp2rbx/FileReader.h"
#include "bsp2rbx/FileWriter.h"
#include "bsp2rbx/RobloxXmlWriter.h"
#include "bsp2rbx/SolidWorldspawnFilter.h"
#include "bsp2rbx/WorldspawnBrushSet.h"

namespace {

struct CliArgs {
    std::filesystem::path input;
    std::filesystem::path output;
    float                 scale = 1.0f / 12.0f;
    std::string           mode  = "aabb";
    bool                  worldspawnOnly = true;
};

void printUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [--mode=aabb|mesh] [--scale=<float>] "
           "[--worldspawn-only] input.bsp output.rbxlx\n"
           "Defaults: --mode=aabb --scale=0.0833 (1 Q2 unit = 1 inch, 1 stud = 1 foot)\n";
}

bool parseCli(int argc, char** argv, CliArgs& out) {
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a{argv[i]};
        if (a == "--worldspawn-only") {
            out.worldspawnOnly = true;
        } else if (a.rfind("--mode=", 0) == 0) {
            out.mode = std::string(a.substr(7));
            if (out.mode != "aabb" && out.mode != "mesh") {
                std::cerr << "bsp2rbx: unsupported --mode=" << out.mode << "\n";
                return false;
            }
        } else if (a.rfind("--scale=", 0) == 0) {
            try {
                out.scale = std::stof(std::string(a.substr(8)));
            } catch (...) {
                std::cerr << "bsp2rbx: invalid --scale\n";
                return false;
            }
        } else if (a == "-h" || a == "--help") {
            return false;
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "bsp2rbx: unknown flag " << a << "\n";
            return false;
        } else {
            positional.emplace_back(a);
        }
    }
    if (positional.size() != 2) return false;
    out.input  = positional[0];
    out.output = positional[1];
    return true;
}

} // namespace

int main(int argc, char** argv) {
    CliArgs args;
    if (!parseCli(argc, argv, args)) {
        printUsage(argc > 0 ? argv[0] : "bsp2rbx");
        return EXIT_FAILURE;
    }
    if (args.mode == "mesh") {
        std::cerr << "bsp2rbx: --mode=mesh not yet implemented (milestone 2)\n";
        return EXIT_FAILURE;
    }

    try {
        auto reader    = std::make_shared<bsp2rbx::FileReader>();
        auto parser    = std::make_shared<bsp2rbx::BspParser>();
        auto worldspawn= std::make_shared<bsp2rbx::WorldspawnBrushSet>();
        auto geometry  = std::make_shared<bsp2rbx::BrushGeometry>();
        auto filter    = std::make_shared<bsp2rbx::SolidWorldspawnFilter>();
        auto xml       = std::make_shared<bsp2rbx::RobloxXmlWriter>();
        auto writer    = std::make_shared<bsp2rbx::FileWriter>();

        bsp2rbx::BspConverter converter(reader, parser, worldspawn, geometry, filter, xml, writer);
        converter.convert(args.input, args.output, args.scale,
                          bsp2rbx::BspConverter::kQ2ToRobloxAxisTransform);
    } catch (const std::exception& e) {
        std::cerr << "bsp2rbx: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "bsp2rbx: wrote " << args.output.string() << "\n";
    return EXIT_SUCCESS;
}
