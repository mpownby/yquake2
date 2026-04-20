# CLAUDE_MCP.md

The MCP (Model Context Protocol) bridge that lets Claude drive the running
yquake2 client. Status: working end-to-end on Windows + gl1 renderer.

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
