# CLAUDE_STATUS.md

Snapshot of what's done and what's next. Update this file as work moves
forward so a fresh session can pick up where the last one left off.

Last updated: 2026-04-20.

## Done — bsp2rbx milestone 3 phase 2 (chamfered-beam decomposition)

Phase 2 detects **pentagonal-prism brushes** (a box with one edge chamfered —
7 faces, 10 hull vertices) and decomposes each into **2 axis-aligned Parts +
1 WedgePart** that exactly tile the pentagonal cross-section. Was the right
fit for `brush_345` in `battle.bsp`, the long ceiling beam visible in the
corner near (-1076, -1070, 280) in Q2 — previously emitted as a fat OBB
Part that filled the entire bbox of the (un-chamfered) cube.

**Algorithm** ([BrushGeometry.cpp:`brushChamferedBeam`](tools/bsp2rbx/include/bsp2rbx/BrushGeometry.cpp)):
1. Vertex/face shape check: 10 unique hull vertices + 2 pentagon faces + 5
   rectangle faces (other faces with ≤ 2 verts are ignored — handles qbsp
   bevels).
2. Pentagon perimeter walked; rectangle axes identified by **parallel-edge
   pairs** (4 of 5 pentagon edges come in 2 parallel pairs; the chamfer is
   the lone unpaired edge). Important: an earlier "longest pentagon edge"
   heuristic broke when the chamfer was the longest side (true for
   `brush_345`, where the chamfer cuts ~67% of the cross-section).
3. Local 2D bbox + missing-corner identification.
4. Decompose into lower-strip Box + upper-corner Box + slope WedgePart
   that share the brush's texture.

**Result on stock maps** (vs phase 1 totals):
- battle:    374 → **382** (3 chamfered-beam brushes detected; +6 instances)
- base64:   5277 → **5331**
- city64:   4254 → **4284**
- arena7dm:  487 → **495**
- bldstorm: 1343 → **1381**

Tests: **72 total, 100% pass** (71 + 1 skipped e2e). 8 new tests covering
detection, volume, AABB containment, texname propagation, bevel resilience,
out-of-range, and the chamfer-as-longest-edge regression.

**Visually verified in Studio** at the Q2 corner the user flagged:
`brush_345` decomposes into 3 pieces that together form the chamfered
ceiling-beam cross-section (long thin top strip + small lower-left box +
gentle ~23° slope wedge). The "fat rectangle" artifact for that brush is
gone.

**Still not handled** — the prominent triangular *corner* fold visible in
the same Q2 view (yellow inverted-pyramid in Studio) is a corner-chamfer
brush (one plane lops off a single box vertex → 7 vertices, not 10). That's
**phase 3** — a Part + CornerWedgePart decomposition.

## Done — bsp2rbx milestone 3 phase 1 (WedgePart for triangular prisms)

Empirical milestone 2 finding: stock Q2 maps have no truly rotated brushes, but
they have *lots* of chamfered/triangular-prism brushes (ramps, trim, edge
chamfers) that get rendered as fat AABBs. Phase 1 of milestone 3 plugs that
gap by detecting right-angled triangular prisms and emitting them as Roblox
**WedgePart** primitives (no external mesh asset needed — Studio renders these
natively).

**Result on stock maps** (parts → parts + wedges; total preserved):
- battle.bsp:   374 → 360 + 14
- base64.bsp:  5424 → 5159 + 118
- city64.bsp:  ~~~  → 4080 + 174
- arena7dm:    ~~~  → 465  + 22
- bldstorm:    ~~~  → 1274 + 69

Tests: **62 total, 100% pass** (61 + 1 skipped e2e).

**New code**:
- `BrushWedge` / `RobloxWedge` structs in [tools/bsp2rbx/include/bsp2rbx/Bsp.h](tools/bsp2rbx/include/bsp2rbx/Bsp.h)
- `IBrushGeometry::brushWedge` — returns `std::optional<BrushWedge>`; non-empty
  only when the brush is a right-angled triangular prism. Resilient to qbsp's
  bevel planes (vertex-shape based, not `numsides`-gated) and to the duplicate
  vertices `brushVertices` produces from over-determined plane triples.
- `IRobloxXmlWriter::emitWedge` — writes `<Item class="WedgePart">`. Shared
  XML formatting helper extracted from the existing `emitPart`.
- `BspConverter::convert` now tries `brushWedge` first per brush; falls back
  to `brushObb` when the optional is empty. Existing 6-arg ctor unchanged.

**WedgePart orientation convention** chosen but not yet eyeball-verified in
Studio: prism axis = local +X, the two triangle legs map to local +Y and +Z,
right-angle vertex on the −X triangle face sits at local (−X/2, −Y/2, −Z/2).
If Studio draws the wedge mirrored or rotated, the axis assignment in
[BrushGeometry::brushWedge](tools/bsp2rbx/include/bsp2rbx/BrushGeometry.cpp)
is the single place to flip.

**Phase 2** (next): non-right-triangle prisms (must return nullopt today —
Roblox WedgePart can't represent a non-right triangle). Phase 3: corner
chamfers — boxes with a single tetrahedral cut, decomposable into Part +
CornerWedgePart.

## Done — bsp2rbx milestone 2 (OBB / rotated Parts, option 1)

Milestone 2 implemented and green: **49 unit tests pass** (40 from
milestone 1 + 7 OBB tests on `BrushGeometry` + 1 rotation-propagation
test in `BspConverter` + 1 rotation-XML test in `RobloxXmlWriter`).
`BrushGeometry::brushObb` computes a face-aligned OBB by finding three
mutually orthogonal face-normal pairs and projecting vertices onto that
basis; falls back to identity-rotation AABB when no orthogonal triple
exists.

**Surprising empirical finding**: none of the ~50 stock `baseq2/maps/*.bsp`
files have any truly rotated brushes. Every non-axis-aligned face is a
*chamfer cut* on an otherwise axis-aligned box (e.g. brush 34 in
`battle.bsp` has the 6 ±x/±y/±z faces plus a 45° corner-chamfer plane).
For such brushes the correct OBB is still the axis-aligned one, so the
rbxlx output is byte-identical to milestone 1: `battle.rbxlx` still has
374 parts, all with identity rotation. The OBB code path is correct and
tested; it just doesn't fire on the available data. Improving visual
fidelity on chamfered brushes is fundamentally a **convex-hull** problem
(milestone 3, option 3), not an OBB one.

## Done — bsp2rbx milestone 1 (track 2)

Scaffolded and green on Windows + MSVC. Tool runs end-to-end on
`battle.bsp` (0.4 MB) -> `battle.rbxlx` (203 KB, 374 solid worldspawn
parts) and `base64.bsp` (5.8 MB) -> `base64.rbxlx` (2.9 MB, 5424 parts).

**Tool code** at [tools/bsp2rbx/](tools/bsp2rbx/):
- Interfaces: `IFileReader`, `IFileWriter`, `IBspParser`,
  `IWorldspawnBrushSet`, `IBrushGeometry`, `IBrushFilter`,
  `IRobloxXmlWriter`.
- Production: `FileReader`, `FileWriter`, `BspParser`,
  `WorldspawnBrushSet` (walks model 0's node tree to collect owned brush
  indices — prevents brush-entity brushes from leaking into worldspawn),
  `BrushGeometry` (plane-intersection, AABB), `SolidWorldspawnFilter`,
  `RobloxXmlWriter`.
- Orchestrator: `BspConverter` (constructor-injected, all collaborators
  behind interfaces).
- [tools/bsp2rbx/src/main.cpp](tools/bsp2rbx/src/main.cpp) — CLI parse +
  DI wiring, flags `--mode=aabb|mesh`, `--scale=<f>`, `--worldspawn-only`.
- Mocks in [tools/bsp2rbx/tests/mocks/](tools/bsp2rbx/tests/mocks/).
- Per-class tests in [tools/bsp2rbx/tests/](tools/bsp2rbx/tests/).

## Fixed defects (from recon passes)

1. **Brush-entity brushes emitted as static worldspawn parts.** The
   converter iterated all brushes in the BSP, but a BSP's brushes lump
   is shared across model 0 (worldspawn) and models 1+ (brush-entities
   like doors, movers, platforms). Those were being emitted at their
   compiled positions as static walls. Fix: new `IWorldspawnBrushSet` /
   `WorldspawnBrushSet` walks model 0's node tree (via the newly parsed
   nodes, leaves, leafbrushes lumps) and returns the set of brush
   indices reachable from it; `BspConverter` skips anything outside that
   set. On `battle.bsp`, dropped 5 parts (4 doors + 1 orphan brush with
   no model reference): 379 -> 374. Regression test:
   `BspConverterTest.SkipsBrushesNotInWorldspawnSetEvenIfFilterWouldKeep`.

**Root wiring** ([CMakeLists.txt](CMakeLists.txt)):
`option(YQ2_BUILD_TOOLS ON)` → `enable_testing()` +
`add_subdirectory(tools/bsp2rbx)`. Project language now `C CXX`.

**Build / run**:
```
<vs-cmake> -G "Visual Studio 17 2022" -A x64 -DSDL3_SUPPORT=OFF \
  -DYQUAKE2LIBS=<...>/x86_64-w64-mingw32/ -S . -B build
<vs-cmake> --build build --config RelWithDebInfo --target bsp2rbx
<vs-ctest> --test-dir build --output-on-failure -C RelWithDebInfo
```

**Next** (milestone 2): emit `MeshPart` with convex-hull mesh for
non-AABB brushes once milestone 1 has been eyeballed in Roblox Studio
against a Q2 reference screenshot via the MCP bridge.

## Done — MCP autonomy loop (track 1)

End-to-end verified on Windows + MSVC. Claude can drive a running yquake2
client and receive screenshots without writing any temp files.

**Engine side** (C, additive — builds clean, all renderer DLLs rebuilt for
the `refexport_t` ABI bump):
- [src/common/header/mcp_server.h](src/common/header/mcp_server.h),
  [src/common/header/mcp_json.h](src/common/header/mcp_json.h)
- [src/common/mcp_server.c](src/common/mcp_server.c) — TCP listener,
  JSON-RPC dispatch, `q2_console` and `q2_screenshot` methods, base64
  encoder, output capture buffer.
- [src/common/mcp_json.c](src/common/mcp_json.c) — minimal JSON helpers.
- [src/common/frame.c](src/common/frame.c) — calls `MCP_Init`,
  `MCP_RunFrame`, `MCP_Shutdown`.
- [src/common/clientserver.c](src/common/clientserver.c) — `Com_VPrintf`
  calls `MCP_CaptureAppend(msg)` (one-instruction no-op when capture
  inactive).
- [src/client/vid/header/ref.h](src/client/vid/header/ref.h) — added
  `RetrieveScreenshot` to `refexport_t` (ABI change).
- [src/client/refresh/gl1/gl1_misc.c](src/client/refresh/gl1/gl1_misc.c) +
  [gl1_main.c](src/client/refresh/gl1/gl1_main.c) — `R_RetrieveScreenshot`
  reads framebuffer via `glReadPixels`; registered in `GetRefAPI`.
- [src/client/mcp_client.c](src/client/mcp_client.c) — PNG-encodes via
  `stb_image_write`, registers as MCP screenshot provider; called from
  `CL_Init` in [src/client/cl_main.c](src/client/cl_main.c).
- [CMakeLists.txt](CMakeLists.txt) — new sources added to `Common-Source`,
  `Server-Source`, client headers; `mcp_client.c` added to client sources.

**Bridge** (Python, in [mcp/q2_bridge/](mcp/q2_bridge/)):
- `pyproject.toml`, `q2_bridge/server.py`, `q2_bridge/__init__.py`,
  `README.md`. Stateless — each MCP tool call opens a fresh TCP connection.

**Verified working**:
- `q2_console("echo hello")` → `{"output":"hello\n","commands_executed":1}`
- `q2_console("version")` → version string captured
- `q2_screenshot()` → real 640×480 PNG of the demo loop returned inline
- Error paths (unknown method, missing param, dedicated build) return
  proper JSON errors

See [CLAUDE_MCP.md](CLAUDE_MCP.md) for full details.

## Known follow-ups (small, not blocking)

- Other renderers (gl3, gl4/gles3, soft, vk) leave
  `re.RetrieveScreenshot` NULL; `q2_screenshot` returns
  `"screenshot not available"` for those. Mechanical to add — copy the
  glReadPixels logic, adjust for each backend's framebuffer format. Track
  in [CLAUDE_MCP.md](CLAUDE_MCP.md) if it becomes a priority.
- `q2_get_state` MCP method is stubbed (returns "not implemented"). Easy
  to add by running `where` internally and parsing the captured output.
- The existing `R_ScreenShot` in gl1 still uses a VLA + duplicated row-flip
  loop. Could be DRY'd by routing it through `R_RetrieveScreenshot` plus
  the existing file writer — but only worth it if touching that code for
  another reason.
