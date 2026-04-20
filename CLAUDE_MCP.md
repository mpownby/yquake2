# CLAUDE_MCP.md

The MCP (Model Context Protocol) bridges that let Claude drive the
running yquake2 client and the running Roblox Studio instance. Both are
registered in [.mcp.json](.mcp.json) so they load as project-scoped MCP
servers.

| Server          | What it drives              | Status                                 |
|-----------------|-----------------------------|----------------------------------------|
| `yquake2`       | yquake2 client              | Working end-to-end, gl1 renderer only  |
| `roblox_studio` | Roblox Studio (any running) | Official Roblox MCP, ships with Studio |

---

## Track 1 — yquake2

Working end-to-end on Windows + gl1 renderer.

## Architecture

```
Claude Code ──stdio MCP──▶ q2_bridge.server (Python) ──TCP──▶ yquake2 (mcp_server.c)
```

Two tiers:

1. **Inside yquake2** ([src/common/mcp_server.c](src/common/mcp_server.c)):
   line-delimited JSON-RPC over TCP on `127.0.0.1:<port>`. Single client
   at a time. Polled once per `Qcommon_Frame` — single-threaded,
   synchronous dispatch.
2. **Outside yquake2** ([mcp/q2_bridge/](mcp/q2_bridge/)): Python stdio
   MCP server using the official `mcp` SDK. Stateless — every tool call
   opens a fresh TCP connection.

The Python tier exists because implementing the full MCP protocol
(stdio framing, schemas, image content types) in C would be ~10x the
code; the JSON-RPC over TCP boundary lets the heavy lifting stay in
Python where the SDK is one `pip install` away.

## Cvars

| Cvar              | Default       | Notes                                |
|-------------------|---------------|--------------------------------------|
| `mcp_server_port` | `0`           | Set non-zero to enable. Loopback only by default. |
| `mcp_server_bind` | `127.0.0.1`   | Change to `0.0.0.0` for non-loopback (no auth — trusted networks only). |

Both are `CVAR_NOSET` (must be set on the launch command line):

```
yquake2 +set mcp_server_port 28910
```

## JSON-RPC protocol

Each request is one line:
```json
{"method":"q2_console","params":{"cmd":"echo hello"}}
```

Each response is one line:
```json
{"result":{"output":"hello\n","commands_executed":1}}
```
or
```json
{"error":"description"}
```

### Methods

- **`q2_console`** — `params: {cmd: string}` → `{result: {output: string,
  commands_executed: int}}`. Wraps `Cbuf_AddText(cmd); Cbuf_Execute()` in
  `MCP_CaptureBegin/End`, returning every `Com_Printf` produced during
  execution.
- **`q2_screenshot`** — `params: {}` → `{result: {png_base64: string,
  width: int, height: int, format: "png"}}`. Calls the registered
  screenshot provider (set up by client code), PNG-encodes via
  `stbi_write_png_to_func` to a memory buffer, base64-encodes.
- **`q2_get_state`** — *stubbed* (returns "not implemented"). Easy
  follow-up: run `where` internally, parse, return structured fields.

## Files involved

**Common (engine, both client and dedicated):**
- [src/common/header/mcp_server.h](src/common/header/mcp_server.h) — public API
- [src/common/header/mcp_json.h](src/common/header/mcp_json.h) — JSON helpers API
- [src/common/mcp_server.c](src/common/mcp_server.c) — TCP listener, dispatch, capture, base64
- [src/common/mcp_json.c](src/common/mcp_json.c) — minimal JSON parser/writer

**Wired into engine main loop:**
- [src/common/frame.c](src/common/frame.c) — `MCP_Init` after `NET_Init`,
  `MCP_RunFrame` after `Cbuf_Execute` in both client and dedicated
  `Qcommon_Frame`, `MCP_Shutdown` first in `Qcommon_Shutdown`.
- [src/common/clientserver.c](src/common/clientserver.c) — one-line hook
  in `Com_VPrintf` calling `MCP_CaptureAppend(msg)`.

**Client-only (screenshot path):**
- [src/client/vid/header/ref.h](src/client/vid/header/ref.h) — added
  `RetrieveScreenshot` to `refexport_t` (ABI change — all renderer DLLs
  must be rebuilt together).
- [src/client/refresh/gl1/gl1_misc.c](src/client/refresh/gl1/gl1_misc.c)
  — `R_RetrieveScreenshot` reads framebuffer via `glReadPixels`.
- [src/client/refresh/gl1/gl1_main.c](src/client/refresh/gl1/gl1_main.c)
  — `re.RetrieveScreenshot = R_RetrieveScreenshot` in `GetRefAPI`.
- [src/client/mcp_client.c](src/client/mcp_client.c) — PNG-encodes via
  `stb_image_write` (header-only, already implementation-defined in
  vid.c), registers as MCP screenshot provider.
- [src/client/cl_main.c](src/client/cl_main.c) — `MCP_ClientInit()` call
  added to `CL_Init`.

**Build:**
- [CMakeLists.txt](CMakeLists.txt) — `mcp_server.c` and `mcp_json.c`
  added to both `Common-Source` and `Server-Source`. `mcp_client.c`
  added to client-only sources. Headers added to client and server
  header lists. `ws2_32` was already linked by the existing UDP
  netcode.

**Bridge (Python):**
- [mcp/q2_bridge/pyproject.toml](mcp/q2_bridge/pyproject.toml)
- [mcp/q2_bridge/q2_bridge/server.py](mcp/q2_bridge/q2_bridge/server.py)
- [mcp/q2_bridge/q2_bridge/__init__.py](mcp/q2_bridge/q2_bridge/__init__.py)
- [mcp/q2_bridge/README.md](mcp/q2_bridge/README.md) — install + Claude
  Code config

## Setting it up on a fresh machine

1. Build yquake2 (see [CLAUDE.md](CLAUDE.md#building-windows-msvc)).
2. `pip install -e mcp/q2_bridge/` (Python 3.10+).
3. Add to `~/.claude/mcp.json` or project `.mcp.json`:
   ```json
   {
     "mcpServers": {
       "yquake2": {
         "command": "python",
         "args": ["-m", "q2_bridge.server"],
         "env": { "Q2_MCP_PORT": "28910" }
       }
     }
   }
   ```
4. Launch yquake2 with `+set mcp_server_port 28910`.

## Smoke test (no Claude Code, just Python + TCP)

```python
import socket, json, base64
s = socket.create_connection(('127.0.0.1', 28910), timeout=10)
s.sendall((json.dumps({"method":"q2_console","params":{"cmd":"echo hi"}})+"\n").encode())
s.shutdown(socket.SHUT_WR)
data = b""
while chunk := s.recv(65536):
    data += chunk
print(json.loads(data.decode().splitlines()[0]))
```

## Known limitations / follow-ups

- **gl1 only** for screenshots. Other renderers (gl3, gl4/gles3, soft, vk)
  default `RetrieveScreenshot` to NULL — `q2_screenshot` returns "screenshot
  not available". Adding each is mechanical: copy the `glReadPixels`
  pattern (or per-backend equivalent), respect that GL3 may use RGBA on
  GLES, and software returns palette indices that need conversion.
- **No auth.** Don't bind to non-loopback unless on a trusted network.
- **Single client.** New connection displaces previous. Fine for Claude;
  a second tool would have to wait its turn.
- **Synchronous dispatch.** A long-running console command (e.g.
  `map demo1`) will block the engine for as long as the load takes,
  which also blocks rendering. Acceptable since nothing competes during
  testing.
- **`q2_get_state` is stubbed.** Add by capturing `where` output and
  parsing the printed fields.

---

## Track 2 — Roblox Studio

Roblox ships the official MCP server bundled with Studio:

- Binary: `%LOCALAPPDATA%\Roblox\Versions\version-*\StudioMCP.exe`
- Version-independent launcher: `%LOCALAPPDATA%\Roblox\mcp.bat`
- Companion Studio plugin: `%LOCALAPPDATA%\Roblox\Plugins\MCPStudioPlugin.rbxm`
  (must be enabled inside Studio for tools to respond)

`.mcp.json` invokes the launcher over stdio:
```json
"roblox_studio": {
    "command": "cmd",
    "args": ["/c", "%LOCALAPPDATA%\\Roblox\\mcp.bat", "--stdio"]
}
```

### Available tools (verified via [mcp/probe_roblox.py](mcp/probe_roblox.py))

The server advertises `protocolVersion: 2024-11-05` and the following
tools (names as reported by `tools/list`):

- **Scene inspection**: `search_game_tree`, `inspect_instance`
- **Script work**: `script_search`, `script_read`, `script_grep`,
  `multi_edit`
- **Execution**: `execute_luau`, `start_stop_play`, `subagent`
- **Creation**: `generate_mesh`, `generate_material`,
  `insert_from_creator_store`
- **Camera / input**: `character_navigation`, `user_mouse_input`,
  `user_keyboard_input`
- **Capture**: `screen_capture`, `get_console_output`
- **Multi-Studio**: `list_roblox_studios`, `set_active_studio`

`screen_capture` + `execute_luau` are the two that matter for the
bsp2rbx feedback loop: load a converted `.rbxlx`, screenshot the Studio
viewport, compare against a `q2_screenshot` of the same vantage in
yquake2, iterate.

### Probe / sanity check

Run [mcp/probe_roblox.py](mcp/probe_roblox.py) with Studio open and the
MCPStudioPlugin enabled. It sends `initialize` + `tools/list` and prints
the server's responses. Handy for verifying the pipe is live without
round-tripping through Claude Code.

### Known limitations

- Requires Studio running *and* the MCPStudioPlugin enabled in it. With
  Studio closed or the plugin disabled, `tools/list` still succeeds but
  tool calls error out.
- `mcp.bat` resolves the latest installed Studio version at each invoke;
  fine in practice but means each Claude Code session binds to whatever
  Studio was most recently installed.
