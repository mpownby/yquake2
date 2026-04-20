"""Parse a Q2Map rbxlx produced by bsp2rbx and emit chunked Luau scripts
for import into a running Roblox Studio via MCP execute_luau.

The rbxlx has a single top-level Model "Q2Map" with Part/WedgePart children.
Each part encodes: Name, Anchored, size(X,Y,Z), CFrame(X,Y,Z,R00..R22),
Color3uint8 (packed 0xAARRGGBB uint32).

Studio's execute_luau has a payload limit (~200 KB), so we split the data
array into chunks. Chunks 1..N-1 append into _G.QMapD; chunk N instantiates.

Run:  python rbxlx_to_luau_chunks.py <input.rbxlx> <out_dir>
Emits: <out_dir>/import_chunk1.lua ... import_chunkN.lua
"""
import argparse
import pathlib
import re
import sys
import xml.etree.ElementTree as ET


def unpack_color(u: int) -> tuple[int, int, int]:
    return ((u >> 16) & 0xFF, (u >> 8) & 0xFF, u & 0xFF)


def parse_rbxlx(path: pathlib.Path) -> list[dict]:
    tree = ET.parse(path)
    root = tree.getroot()
    items = []
    for item in root.iter("Item"):
        cls = item.get("class")
        if cls not in ("Part", "WedgePart"):
            continue
        props = item.find("Properties")
        if props is None:
            continue
        name = props.findtext("string[@name='Name']") or ""
        size = props.find("Vector3[@name='size']")
        cf = props.find("CoordinateFrame[@name='CFrame']")
        col = props.findtext("Color3uint8[@name='Color3uint8']") or "4294967295"
        if size is None or cf is None:
            continue
        sx = float(size.findtext("X") or 0)
        sy = float(size.findtext("Y") or 0)
        sz = float(size.findtext("Z") or 0)
        x = float(cf.findtext("X") or 0)
        y = float(cf.findtext("Y") or 0)
        z = float(cf.findtext("Z") or 0)
        r = [float(cf.findtext(f"R{i}{j}") or 0) for i in range(3) for j in range(3)]
        r8, g8, b8 = unpack_color(int(col))
        items.append(dict(cls=cls, name=name, size=(sx, sy, sz),
                          pos=(x, y, z), rot=r, col=(r8, g8, b8)))
    return items


def fmt_num(x: float) -> str:
    s = f"{x:.6f}".rstrip("0").rstrip(".")
    return s if s else "0"


def row_literal(it: dict) -> str:
    sx, sy, sz = it["size"]
    px, py, pz = it["pos"]
    r = it["rot"]
    cr, cg, cb = it["col"]
    cls_tag = "W" if it["cls"] == "WedgePart" else "P"
    name = it["name"].replace("'", "\\'")
    nums = ",".join(fmt_num(v) for v in [sx, sy, sz, px, py, pz, *r])
    return f"{{'{cls_tag}',{nums},{cr},{cg},{cb},'{name}'}}"


def chunk_rows(rows: list[str], budget: int) -> list[list[str]]:
    chunks, cur, cur_len = [], [], 0
    for r in rows:
        add = len(r) + 2
        if cur and cur_len + add > budget:
            chunks.append(cur)
            cur, cur_len = [], 0
        cur.append(r)
        cur_len += add
    if cur:
        chunks.append(cur)
    return chunks


def emit(out_dir: pathlib.Path, rows: list[str], budget: int) -> int:
    out_dir.mkdir(parents=True, exist_ok=True)
    for old in out_dir.glob("import_chunk*.lua"):
        old.unlink()
    chunks = chunk_rows(rows, budget)
    n = len(chunks)
    for i, ch in enumerate(chunks, 1):
        is_first = (i == 1)
        is_last = (i == n)
        buf = []
        if is_first:
            buf.append("_G.QMapD = _G.QMapD or {}")
        buf.append("local D = _G.QMapD")
        buf.append("local chunk = {")
        buf.extend(r + "," for r in ch)
        buf.append("}")
        buf.append("for _, r in ipairs(chunk) do D[#D+1] = r end")
        if is_last:
            buf.append("local ws = game:GetService('Workspace')")
            buf.append("local existing = ws:FindFirstChild('Q2Map')")
            buf.append("if existing then existing:Destroy() end")
            buf.append("local model = Instance.new('Model')")
            buf.append("model.Name = 'Q2Map'")
            buf.append("model.Parent = ws")
            buf.append("for _, d in ipairs(D) do")
            buf.append("  local p = Instance.new(d[1] == 'W' and 'WedgePart' or 'Part')")
            buf.append("  p.Anchored = true")
            buf.append("  p.TopSurface = Enum.SurfaceType.Smooth")
            buf.append("  p.BottomSurface = Enum.SurfaceType.Smooth")
            buf.append("  p.Size = Vector3.new(d[2], d[3], d[4])")
            buf.append("  p.CFrame = CFrame.new(d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15],d[16])")
            buf.append("  p.Color = Color3.fromRGB(d[17], d[18], d[19])")
            buf.append("  p.Name = d[20]")
            buf.append("  p.Parent = model")
            buf.append("end")
            buf.append("_G.QMapD = nil")
            buf.append(f"return 'imported ' .. tostring(model:GetAttribute('_') or #model:GetChildren()) .. ' parts'")
        else:
            buf.append(f"return 'chunk{i}/{n} accum=' .. #D")
        (out_dir / f"import_chunk{i}.lua").write_text("\n".join(buf), encoding="utf-8")
    return n


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=pathlib.Path)
    ap.add_argument("out_dir", type=pathlib.Path)
    ap.add_argument("--budget", type=int, default=80000,
                    help="approx chars per chunk (execute_luau payload limit)")
    args = ap.parse_args(argv)

    items = parse_rbxlx(args.input)
    rows = [row_literal(it) for it in items]
    n = emit(args.out_dir, rows, args.budget)
    print(f"parsed {len(items)} parts, wrote {n} chunks to {args.out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
