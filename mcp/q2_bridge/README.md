# q2_bridge — MCP bridge to yquake2

Stdio MCP server that lets Claude Code (or any MCP client) drive a running
yquake2 instance: send console commands, capture their output, and (later)
take screenshots over the wire.

## Architecture

```
Claude Code ──stdio MCP──▶ q2_bridge.server ──TCP──▶ yquake2 (mcp_server.c)
```

The bridge is stateless — every MCP tool call opens a fresh TCP connection,
sends one JSON-RPC line, reads one response line, and closes.

## Install

Requires Python 3.10+.

```bash
cd mcp/q2_bridge
pip install -e .
```

## Run yquake2 with the MCP server enabled

The MCP server is gated by a cvar (`mcp_server_port`) and disabled by
default. Launch with:

```bash
yquake2 +set mcp_server_port 28910
```

Optionally bind to a non-loopback address (only do this on a trusted
network — there is no auth):

```bash
yquake2 +set mcp_server_port 28910 +set mcp_server_bind 0.0.0.0
```

You should see `MCP: listening on 127.0.0.1:28910` in the console.

## Configure Claude Code

Add to your project `.mcp.json` (or `~/.claude/mcp.json`):

```json
{
  "mcpServers": {
    "yquake2": {
      "command": "python",
      "args": ["-m", "q2_bridge.server"],
      "env": {
        "Q2_MCP_HOST": "127.0.0.1",
        "Q2_MCP_PORT": "28910"
      }
    }
  }
}
```

## Tools exposed

- `q2_console(cmd: str) -> str` — run a console command, return captured output.
- `q2_screenshot() -> Image` — capture the current frame as an inline PNG.
  Requires the gl1 renderer (the default).

## Manual smoke test

```bash
# In one terminal: launch yquake2 with MCP enabled.
# In another:
python -c "from q2_bridge.server import _rpc; print(_rpc('q2_console', {'cmd': 'where'}))"
```
