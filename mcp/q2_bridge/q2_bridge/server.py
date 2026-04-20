"""
MCP stdio bridge that forwards tool calls to a running yquake2 instance.

Connection model: each MCP tool call opens a fresh TCP connection to the
yquake2 MCP server (defaults to 127.0.0.1:28910), sends one JSON-RPC line,
reads one response line, and closes. Stateless, so the bridge process can be
restarted independently of the game.
"""

from __future__ import annotations

import base64
import json
import os
import socket
from typing import Any

from mcp.server.fastmcp import FastMCP
from mcp.server.fastmcp.utilities.types import Image

DEFAULT_HOST = os.environ.get("Q2_MCP_HOST", "127.0.0.1")
DEFAULT_PORT = int(os.environ.get("Q2_MCP_PORT", "28910"))
RECV_TIMEOUT = float(os.environ.get("Q2_MCP_TIMEOUT", "10.0"))

mcp = FastMCP("yquake2")


def _rpc(method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    """Send one JSON-RPC line to the yquake2 MCP server and return the parsed response."""
    payload = {"method": method, "params": params or {}}
    request = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")

    with socket.create_connection((DEFAULT_HOST, DEFAULT_PORT), timeout=RECV_TIMEOUT) as s:
        s.sendall(request)
        s.shutdown(socket.SHUT_WR)
        chunks: list[bytes] = []
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
        raw = b"".join(chunks).decode("utf-8", errors="replace").strip()

    if not raw:
        raise RuntimeError(f"yquake2 MCP server returned empty response for method '{method}'")

    response = json.loads(raw.splitlines()[0])
    if "error" in response:
        raise RuntimeError(f"yquake2 MCP server error: {response['error']}")
    return response.get("result", {})


@mcp.tool()
def q2_console(cmd: str) -> str:
    """
    Execute a console command inside the running yquake2 client.

    Examples: 'where' (prints player position/angles), 'teleport_exact 100 200 50',
    'setangles 0 90 0', 'map demo1'.

    Returns the captured Com_Printf output produced while the command ran.
    """
    result = _rpc("q2_console", {"cmd": cmd})
    output = result.get("output", "")
    return output if output else "(no output)"


@mcp.tool()
def q2_screenshot() -> Image:
    """
    Capture a PNG screenshot of the running yquake2 client's current frame
    and return it inline as an MCP image. No file is written to disk.

    Requires the gl1 (default) renderer; other renderers don't yet implement
    framebuffer readback for the MCP bridge.
    """
    result = _rpc("q2_screenshot")
    png = base64.b64decode(result["png_base64"])
    return Image(data=png, format="png")


def main() -> None:
    """Entry point used by the `q2-bridge` console script."""
    mcp.run()


if __name__ == "__main__":
    main()
