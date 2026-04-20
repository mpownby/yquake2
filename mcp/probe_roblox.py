"""Probe the local Roblox Studio MCP server by sending initialize + tools/list."""

import json
import os
import subprocess
import sys


def main() -> int:
    mcp_bat = os.path.expandvars(r"%LOCALAPPDATA%\Roblox\mcp.bat")
    proc = subprocess.Popen(
        ["cmd", "/c", mcp_bat, "--stdio"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    def send(obj: dict) -> None:
        line = json.dumps(obj) + "\n"
        assert proc.stdin is not None
        proc.stdin.write(line)
        proc.stdin.flush()

    def recv() -> dict:
        assert proc.stdout is not None
        line = proc.stdout.readline()
        if not line:
            raise RuntimeError("server closed stdout")
        return json.loads(line)

    send({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2025-03-26",
            "capabilities": {},
            "clientInfo": {"name": "yquake2-probe", "version": "0.0.1"},
        },
    })
    init_resp = recv()
    print("initialize ->", json.dumps(init_resp, indent=2))

    send({"jsonrpc": "2.0", "method": "notifications/initialized", "params": {}})

    send({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
    tools_resp = recv()
    print("tools/list ->", json.dumps(tools_resp, indent=2))

    proc.stdin.close()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
