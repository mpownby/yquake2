# CLAUDE_ROBLOX_IMPORT.md

How to import a bsp2rbx-generated `.rbxlx` into the user's running
Roblox Studio without asking the user to do anything. The user has a
Studio open with the `roblox_studio` MCP server attached (see
`mcp__roblox_studio__execute_luau`, `mcp__roblox_studio__screen_capture`).
Do NOT ask the user to import via File > Open — do it yourself.

## Constraint: payload limits

`execute_luau` rejects payloads above roughly 25 KB (observed). And
`Read` of a >25k-token file fails, so a single giant literal also
breaks the authoring path. Conclusion: split the part array into
≤~18 KB Luau chunks and send them sequentially. Accumulate into
`_G.QMapD`, then the final chunk instantiates.

## Pipeline

1. **Reconvert** the BSP to `.rbxlx` via `bsp2rbx.exe`.
2. **Chunk** the rbxlx into Luau scripts with the helper:
   ```
   python tools/bsp2rbx/scripts/rbxlx_to_luau_chunks.py \
       battle.rbxlx . --budget 18000
   ```
   Emits `import_chunk1.lua` ... `import_chunkN.lua`.
3. **Execute each chunk in order** via `mcp__roblox_studio__execute_luau`.
   Each chunk's full content fits under the payload cap; Claude can
   `Read` each ≤18 KB chunk file and paste its contents verbatim as the
   `code` argument.
4. Final chunk destroys any existing `Workspace.Q2Map` and rebuilds it
   from the accumulated `_G.QMapD`, then clears the global.
5. **Verify** by calling `mcp__roblox_studio__screen_capture`. If the
   camera is now inside the enlarged map, pull it back with
   `model:GetBoundingBox()` (the scale change fix happens in the
   converter, not in Studio — see the memory rule
   `feedback_converter_source_of_truth`).

## Row format used by the chunk helper

Each part is a Lua table literal:

```
{cls, sx, sy, sz, px, py, pz, R00..R22, r8, g8, b8, name}
```

- `cls` is `'P'` for Part, `'W'` for WedgePart. Both exist in bsp2rbx
  output (triangular faces become WedgePart).
- Rotation is 9 floats in row-major order matching `CFrame.new(x,y,z,
  R00,R01,R02,R10,R11,R12,R20,R21,R22)`.
- Color is `Color3uint8` unpacked: `(u>>16)&0xFF, (u>>8)&0xFF, u&0xFF`.
- The final chunk's loop dispatches: `Instance.new(d[1] == 'W' and
  'WedgePart' or 'Part')`.

## Do not

- Do not hand-edit Studio instances to fix visual bugs — fix the
  converter and reconvert (memory rule
  `feedback_converter_source_of_truth`). Importing the reconverted
  rbxlx via the pipeline above is the legitimate downstream step.
- Do not ask the user to import the `.rbxlx` via Studio's File menu.
  The MCP bridge can drive Studio directly; the user's time is
  valuable.
