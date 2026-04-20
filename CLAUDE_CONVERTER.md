# CLAUDE_CONVERTER.md

Plan and design for `tools/bsp2rbx/` — a standalone C++ utility that
converts Quake 2 `.bsp` map files to other engine formats. First target:
Roblox (`.rbxlx`). Status: **not yet implemented**, design only.

Read [CLAUDE_CONVENTIONS.md](CLAUDE_CONVENTIONS.md) before starting — the
DI architecture and GoogleMock matcher rules are non-negotiable.

## Goal

The user wants Claude to be able to iterate on this converter
autonomously: run the converter, drive the real Quake 2 client to compare
reference views (via the MCP bridge — see [CLAUDE_MCP.md](CLAUDE_MCP.md)),
and verify output in Roblox Studio. The unit tests catch regressions as
the converter grows in complexity.

## Approach summary

- **Standalone C++17 executable** at `tools/bsp2rbx/`. No link to any
  yquake2 source. Includes only the on-disk struct definitions from
  [src/common/header/files.h](src/common/header/files.h) (plain POD).
- **Do not** refactor [src/common/collision.c](src/common/collision.c)
  into a shared library. It's interleaved with the engine (`Hunk_Alloc`,
  `Com_Error`, server entity strings, area-portal/PVS state, vis
  decompression). Pulling it out is days of churn for zero engine
  benefit. A fresh ~400-line `std::ifstream`/`std::vector` parser is
  simpler.
- **GoogleTest + GoogleMock via CMake `FetchContent`**. No vendoring.
- **Dependency Inversion everywhere** — every collaborator behind an
  interface, all wired in `main()`.

## Layout

```
tools/bsp2rbx/
  CMakeLists.txt
  include/bsp2rbx/
    Bsp.h                          # struct Bsp { vertices, planes, brushes, ... }
    IFileReader.h                  # interface: read raw bytes from a path
    FileReader.h/.cpp              # production impl using std::ifstream
    IBspParser.h                   # interface: bytes -> Bsp
    BspParser.h/.cpp
    IBrushGeometry.h               # interface: brush -> vertices, AABB
    BrushGeometry.h/.cpp           # plane-intersection
    IBrushFilter.h                 # interface: should this brush be exported?
    SolidWorldspawnFilter.h/.cpp   # solid + non-sky/trigger/clip
    IRobloxXmlWriter.h             # interface: build .rbxlx XML
    RobloxXmlWriter.h/.cpp
    IFileWriter.h                  # interface: write string to path
    FileWriter.h/.cpp
    BspConverter.h/.cpp            # orchestrator: takes all interfaces in ctor
  src/main.cpp                     # CLI parse + DI wiring + invoke BspConverter
  tests/
    mocks/
      MockFileReader.h
      MockBspParser.h
      MockBrushGeometry.h
      MockBrushFilter.h
      MockRobloxXmlWriter.h
      MockFileWriter.h
    FileReaderTest.cpp             # only test that hits real disk (uses tmp file)
    BspParserTest.cpp
    BrushGeometryTest.cpp
    SolidWorldspawnFilterTest.cpp
    RobloxXmlWriterTest.cpp
    FileWriterTest.cpp
    BspConverterTest.cpp           # all deps mocked — interaction-only
    e2e/Demo1ConversionTest.cpp    # real wiring on a real BSP, env-gated
  fixtures/
    cube.bsp                       # tiny hand-crafted minimal BSP
```

## Public types

```cpp
namespace bsp2rbx {

struct Bsp {
    std::vector<dvertex_t>    vertices;
    std::vector<dplane_t>     planes;
    std::vector<dbrush_t>     brushes;
    std::vector<dbrushside_t> brushsides;
    std::vector<dmodel_t>     models;
    std::vector<dface_t>      faces;
    std::vector<dedge_t>      edges;
    std::vector<int>          surfedges;
    std::vector<texinfo_t>    texinfos;
    std::string               entityString;
};

struct BrushAabb {
    std::array<float,3>   mins, maxs;
    int                   modelIndex;
    std::string           texname;
};

struct RobloxPart {
    std::array<float,3>   position, size;
    std::array<uint8_t,3> color;
    std::string           name;
};

} // namespace bsp2rbx
```

## Interfaces

```cpp
class IFileReader {
public:
    virtual ~IFileReader() = default;
    virtual std::vector<uint8_t> read(const std::filesystem::path&) = 0;
};

class IBspParser {
public:
    virtual ~IBspParser() = default;
    virtual std::unique_ptr<Bsp> parse(const std::vector<uint8_t>&) = 0;
};

class IBrushGeometry {
public:
    virtual ~IBrushGeometry() = default;
    virtual std::vector<std::array<float,3>>
            brushVertices(const Bsp&, int brushIndex) = 0;
    virtual BrushAabb brushAabb(const Bsp&, int brushIndex) = 0;
};

class IBrushFilter {
public:
    virtual ~IBrushFilter() = default;
    virtual bool keep(const Bsp&, int brushIndex) = 0;
};

class IRobloxXmlWriter {
public:
    virtual ~IRobloxXmlWriter() = default;
    virtual void beginDocument() = 0;
    virtual void emitPart(const RobloxPart&) = 0;
    virtual std::string endDocument() = 0;  // returns final XML string
};

class IFileWriter {
public:
    virtual ~IFileWriter() = default;
    virtual void write(const std::filesystem::path&, std::string_view) = 0;
};
```

## Orchestrator

```cpp
class BspConverter {
public:
    BspConverter(std::shared_ptr<IFileReader>      reader,
                 std::shared_ptr<IBspParser>       parser,
                 std::shared_ptr<IBrushGeometry>   geometry,
                 std::shared_ptr<IBrushFilter>     filter,
                 std::shared_ptr<IRobloxXmlWriter> xml,
                 std::shared_ptr<IFileWriter>      writer);

    void convert(const std::filesystem::path& inBsp,
                 const std::filesystem::path& outRbxlx,
                 float scale);
};
```

`convert()` is short — it reads, parses, iterates brushes, asks the
filter, asks the geometry, calls the XML writer, writes the output. Each
collaborator is mocked independently in `BspConverterTest.cpp`.

## CLI

```
bsp2rbx [--mode=aabb|mesh] [--scale=<float>] [--worldspawn-only]
        input.bsp output.rbxlx
```

Defaults: `--mode=aabb`, `--scale=0.0254`. Quake units are roughly 1 inch;
Roblox studs are ~1 ft. Document and let it be tuned.

`main()` is the only function exempt from unit testing — it parses args
and constructs the concrete dependency graph:

```cpp
int main(int argc, char** argv) {
    auto args   = parseCli(argc, argv);
    auto reader = std::make_shared<FileReader>();
    auto parser = std::make_shared<BspParser>();
    auto geom   = std::make_shared<BrushGeometry>();
    auto filter = std::make_shared<SolidWorldspawnFilter>();
    auto xml    = std::make_shared<RobloxXmlWriter>();
    auto writer = std::make_shared<FileWriter>();
    BspConverter c(reader, parser, geom, filter, xml, writer);
    c.convert(args.input, args.output, args.scale);
}
```

## Roblox emitter — milestone 1: AABB per brush

Filter to solid worldspawn brushes (`contents & CONTENTS_SOLID`, skip
`sky/trigger/clip` texnames). For each kept brush, intersect its
half-spaces (planes from `brushside_t`) to enumerate vertices, take the
AABB, emit one `<Item class="Part">` per brush in a `<Item class="Model"
Name="Q2Map">` inside `<Item class="Workspace">`. `Color3uint8` hashed
from texname for visual debugging.

Correct for the ~70% of brushes that are already axis-aligned boxes;
conservative-but-visible for the rest. Produces a loadable `.rbxlx`
immediately.

**Milestone 2** (deferred until milestone 1 round-trips a real map):
emit `MeshPart` with the convex hull mesh for non-AABB brushes.

## CMake integration

Root [CMakeLists.txt](CMakeLists.txt):
```cmake
option(YQ2_BUILD_TOOLS "Build helper tools" ON)
if(YQ2_BUILD_TOOLS)
    add_subdirectory(tools/bsp2rbx)
endif()
```

`tools/bsp2rbx/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(bsp2rbx LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0)
FetchContent_MakeAvailable(googletest)

add_library(bsp2rbx_lib OBJECT
    include/bsp2rbx/FileReader.cpp
    include/bsp2rbx/BspParser.cpp
    include/bsp2rbx/BrushGeometry.cpp
    include/bsp2rbx/SolidWorldspawnFilter.cpp
    include/bsp2rbx/RobloxXmlWriter.cpp
    include/bsp2rbx/FileWriter.cpp
    include/bsp2rbx/BspConverter.cpp)
target_include_directories(bsp2rbx_lib
    PUBLIC  include
    PRIVATE ${CMAKE_SOURCE_DIR}/src/common/header)  # for files.h

add_executable(bsp2rbx src/main.cpp $<TARGET_OBJECTS:bsp2rbx_lib>)
target_link_libraries(bsp2rbx PRIVATE bsp2rbx_lib)

enable_testing()
include(GoogleTest)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
     tests/*Test.cpp tests/e2e/*Test.cpp)
foreach(src ${TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} $<TARGET_OBJECTS:bsp2rbx_lib>)
  target_include_directories(${name} PRIVATE tests/mocks)
  target_link_libraries(${name} PRIVATE bsp2rbx_lib gmock_main)
  gtest_discover_tests(${name})
endforeach()
```

Run: `cmake --build build --target bsp2rbx && ctest --test-dir build -R bsp2rbx`.

## Test patterns

Example orchestrator test (note: no `testing::_`):

```cpp
using ::testing::Return;
using ::testing::Eq;
using ::testing::ByMove;

TEST(BspConverterTest, ReadsParsesIteratesAndWrites) {
    auto reader  = std::make_shared<MockFileReader>();
    auto parser  = std::make_shared<MockBspParser>();
    auto geom    = std::make_shared<MockBrushGeometry>();
    auto filter  = std::make_shared<MockBrushFilter>();
    auto xml     = std::make_shared<MockRobloxXmlWriter>();
    auto writer  = std::make_shared<MockFileWriter>();

    std::vector<uint8_t> bytes = {0x49, 0x42, 0x53, 0x50};  // "IBSP"
    auto bsp = std::make_unique<Bsp>();
    bsp->brushes.resize(2);
    auto bspPtr = bsp.get();

    EXPECT_CALL(*reader, read(std::filesystem::path("in.bsp")))
        .WillOnce(Return(bytes));
    EXPECT_CALL(*parser, parse(Eq(bytes)))
        .WillOnce(Return(ByMove(std::move(bsp))));
    EXPECT_CALL(*filter, keep(::testing::Ref(*bspPtr), 0)).WillOnce(Return(true));
    EXPECT_CALL(*filter, keep(::testing::Ref(*bspPtr), 1)).WillOnce(Return(false));
    EXPECT_CALL(*geom, brushAabb(::testing::Ref(*bspPtr), 0))
        .WillOnce(Return(BrushAabb{{0,0,0},{1,1,1},0,"wall1"}));
    EXPECT_CALL(*xml, beginDocument());
    EXPECT_CALL(*xml, emitPart(Field(&RobloxPart::name, Eq("brush_0"))));
    EXPECT_CALL(*xml, endDocument()).WillOnce(Return(std::string("<roblox/>")));
    EXPECT_CALL(*writer, write(std::filesystem::path("out.rbxlx"),
                               std::string_view("<roblox/>")));

    BspConverter c(reader, parser, geom, filter, xml, writer);
    c.convert("in.bsp", "out.rbxlx", 1.0f);
}
```

Every concrete class has its own `*Test.cpp` testing all of its methods.
The orchestrator test is short because it only verifies wiring; the
geometry/XML logic is tested in their own files.

## Fixtures

`fixtures/cube.bsp` (a few KB hand-crafted minimal BSP) committed to repo
for deterministic CI. Real BSPs from `baseq2/maps/*.bsp` (or unpacked
from `pak0.pak`) are gated by `YQ2_TEST_BASEQ2` env var; if unset,
`e2e/Demo1ConversionTest.cpp` calls `GTEST_SKIP() << "YQ2_TEST_BASEQ2 not set"`.

## Verification end-to-end

1. `ctest -R bsp2rbx` — all tests green.
2. `bsp2rbx baseq2/maps/demo1.bsp demo1.rbxlx` — converts a real map.
3. Open `demo1.rbxlx` in Roblox Studio — see geometry.
4. (With MCP bridge running) From Claude: `q2_console("map demo1");
   q2_console("teleport_exact <x> <y> <z>"); q2_screenshot()` —
   compare screenshot to the Roblox view, iterate.
