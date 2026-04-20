# CLAUDE_STATUS.md

Snapshot of what's done and what's next. Update this file as work moves
forward so a fresh session can pick up where the last one left off.

Last updated: 2026-04-19.

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

## Not yet done — bsp2rbx converter (track 2)

Track 1 is complete; track 2 hasn't been started. The plan is fully
designed in [CLAUDE_CONVERTER.md](CLAUDE_CONVERTER.md).

**Next concrete steps** when picking this up:
1. Scaffold `tools/bsp2rbx/` with `CMakeLists.txt` and `FetchContent` for
   GoogleTest+GoogleMock.
2. Wire `option(YQ2_BUILD_TOOLS "Build helper tools" ON)` in root
   `CMakeLists.txt` and `add_subdirectory(tools/bsp2rbx)`.
3. Implement the interfaces, production impls, and one mock+test per
   class in DI order (start with `IFileReader` — simplest, then build up).
4. CLI parser in `main.cpp` constructs the dependency graph and invokes
   `BspConverter::convert`.
5. Convert `baseq2/maps/demo1.bsp` to `demo1.rbxlx`, eyeball-import in
   Roblox Studio.

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
