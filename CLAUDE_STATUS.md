# CLAUDE_STATUS.md

Snapshot of what's done and what's next. Update this file as work moves
forward so a fresh session can pick up where the last one left off.

Last updated: 2026-04-19.

## Done — bsp2rbx milestone 1 (track 2)

Scaffolded and green on Windows + MSVC. **40 unit tests pass** (31
original + 7 WorldspawnBrushSet + 1 BspConverter regression + BspParser
nodes/leaves/leafbrushes); env-gated demo1 e2e test is wired but skips
unless `YQ2_TEST_BASEQ2` is set. Tool runs end-to-end on
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
