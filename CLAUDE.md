# CLAUDE.md

Entry point for Claude Code working in this repo. Keep this file short — it
is auto-loaded into every session. Detailed context lives in the sibling
`CLAUDE_*.md` files; pull them in only when relevant to the current task.

## What this repo is

Yamagi Quake II (yquake2) — an open-source Quake 2 engine — plus a small
set of additions that turn it into a tooling host for an autonomous Claude
workflow. Custom additions:

1. **Console commands** for inspecting/manipulating player position:
   `where`, `teleport_exact`, `setangles`, `setforward` (in
   [src/game/g_cmds.c](src/game/g_cmds.c)).
2. **MCP bridge** — TCP JSON-RPC server inside the engine plus a Python
   stdio bridge, so Claude can run console commands and capture screenshots
   without manual intervention. See [CLAUDE_MCP.md](CLAUDE_MCP.md).
3. **BSP → Roblox converter** (planned) — a standalone C++ tool at
   `tools/bsp2rbx/` for converting Quake 2 maps to other engines. See
   [CLAUDE_CONVERTER.md](CLAUDE_CONVERTER.md).

The end goal: Claude can iterate on the converter, run it, drive the real
Quake 2 client to compare reference views, and verify the output in Roblox
Studio — all autonomously, with the unit-test suite catching regressions.

## Where to look

- **Resuming work / current state**: [CLAUDE_STATUS.md](CLAUDE_STATUS.md)
- **Coding conventions for new code**: [CLAUDE_CONVENTIONS.md](CLAUDE_CONVENTIONS.md)
- **MCP bridge** (architecture, files, how to use): [CLAUDE_MCP.md](CLAUDE_MCP.md)
- **bsp2rbx converter** (design, DI graph, milestones): [CLAUDE_CONVERTER.md](CLAUDE_CONVERTER.md)

## Building (Windows, MSVC)

The repo's `CMakeLists.txt` requires CMake 3.31. The system CMake is too
old; use the one bundled with Visual Studio 2022:

```
"C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
```

Configure once (replace the libs path with wherever dhewm3-libs is on this
machine):

```bash
cmake -G "Visual Studio 17 2022" -A x64 -DSDL3_SUPPORT=OFF \
  -DYQUAKE2LIBS="<path-to-dhewm3-libs>/x86_64-w64-mingw32/" \
  -S . -B build
```

`-DSDL3_SUPPORT=OFF` is required because dhewm3-libs ships SDL2.

Build:

```bash
"<vs-cmake>" --build build --config RelWithDebInfo
```

Outputs land in `build/release/RelWithDebInfo/`. Runtime DLLs (SDL2.dll,
OpenAL32.dll, libcurl-4.dll renamed to curl.dll) must be next to the exe —
copy them once from `<dhewm3-libs>/x86_64-w64-mingw32/bin/`.

## Running with the MCP bridge enabled

```bash
yquake2 +set mcp_server_port 28910
```

Default bind is 127.0.0.1. The MCP subsystem is fully gated by this cvar —
when unset, none of it runs. See [CLAUDE_MCP.md](CLAUDE_MCP.md) for the
Python bridge install + Claude Code config.

## Upstream relationship

Custom additions are kept additive: the engine still builds and runs as a
plain yquake2 if no MCP cvars are set and no `tools/` build is requested.
Don't refactor existing engine code unless a feature genuinely needs it.
