#!/usr/bin/env python3
"""Reference side of the file reader test (core/tools/fmt_test.sh).

Prints the same text lines as core/tools/fmt_test.c (`fmt_test dump`) from the Python parsers in _re/formats (bxml.py,
scene_read.py, package_extract.py + the libmspack bridge, texture_decode.py, bike_asset_export.py, alpine_collision_objects.py,
level_export.py, terrain_deform_reference.py, ui_export.py) so that `diff` shows any difference between the C readers and the
reference. Read-only on the install; writes only the files named on the command line (under the scratchpad / build folder).

    fmt_test.py ARCHIVE OUT.txt SAMPLES.txt [DUMPDIR]

SAMPLES.txt lists the model / surface / texture assets the Python parsers could read (the C side then reads exactly those).
With DUMPDIR the full per-document dumps are written there (for diffing a failing hash)."""
import hashlib
import json
import math
import os
import struct
import sys
import zlib
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
FORMATS = HERE.parents[1] / "_re" / "formats"
sys.path.insert(0, str(FORMATS))

from bxml import Document  # noqa: E402
from package_extract import LzxDecoder, decode_resource  # noqa: E402
from scene_read import SceneDocument, read_sections  # noqa: E402
import texture_decode  # noqa: E402
import bike_asset_export as bae  # noqa: E402
import alpine_collision_objects as aco  # noqa: E402
import level_export as lex  # noqa: E402
import terrain_deform_reference as tdr  # noqa: E402
import ui_export  # noqa: E402

LEVELS = ["FR_Alpine", "NAT_Concord", "SX_Kingston"]
FULL_DBS = ["MXTables_Default", "EngineDependencies", "RiderPhysics", "FR_Alpine", "NAT_Concord", "SX_Kingston"]
COLLIS_DBS = ["FR_Alpine", "NAT_Concord", "SX_Kingston", "MXs", "ATVs", "EngineDependencies", "RiderPhysics"]
MODEL_DBS = ["FR_Alpine", "NAT_Concord", "SX_Kingston", "MXs"]
MODEL_LIMIT = 60
TEX_LIMIT = 40
TEX_DECODE_PIXELS = 256 * 256

OUTL = []
DUMPDIR = None
DEC = None


def emit(line):
    OUTL.append(line)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def hx(f):
    return "%08x" % struct.unpack("<I", struct.pack("<f", f))[0]


def js(s):
    return json.dumps(s, ensure_ascii=False)


def dump_file(name, text):
    if DUMPDIR:
        p = Path(DUMPDIR) / (name.replace("/", "_") + ".txt")
        p.write_text(text, encoding="utf-8")


class TestDoc(SceneDocument):
    """SceneDocument without the 65536-element cap (the packdata maps are bigger)."""

    def value(self, item):
        if item.kind in (0, 1):
            return Document.value(self, item)
        if item.kind == 2:
            return {"opaque_bytes": self.blob[item.ref:item.ref + item.count].hex()}
        formats = {3: "<i", 4: "<I", 5: "<f", 10: "<3f", 11: "<I", 7: "<4f", 8: "<16f", 9: "<2f"}
        fmt = formats[item.kind]
        size = struct.calcsize(fmt)
        assert item.ref + item.count * size <= len(self.blob)
        values = []
        for index in range(item.count):
            value = struct.unpack_from(fmt, self.blob, item.ref + index * size)
            values.append(list(value) if len(value) > 1 else value[0])
        return values[0] if item.count == 1 else values


def scalar_text(kind, v):
    if kind == 5:
        return hx(v)
    return "%d" % v


def rv(doc, item):
    kind = item.kind
    if kind == 0:
        return "-"
    value = doc.value(item)
    if kind == 1:
        return js(value)
    if kind == 2:
        return "x" + value["opaque_bytes"]
    elems = [value] if item.count == 1 else value
    parts = []
    for e in elems:
        if isinstance(e, list):
            parts.append(" ".join(scalar_text(kind if kind in (3, 4, 11) else 5, c) for c in e))
        else:
            parts.append(scalar_text(kind, e))
    return "T%d:%d:%s" % (kind, item.count, ",".join(parts))


def dump_tree(doc, index, depth, out):
    n = doc.nodes[index]
    attrs = " ".join("%s=%s" % (doc.strings[doc.attributes[a].name], rv(doc, doc.attributes[a])) for a in doc.attrs(index))
    out.append("%d %s {%s} %s\n" % (depth, doc.strings[n.name], attrs, rv(doc, n)))
    for c in doc.children(index):
        dump_tree(doc, c, depth + 1, out)


def tree_text(doc, index=0):
    out = []
    stack = [(index, 0)]
    # iterative preorder (deep trees)
    while stack:
        i, d = stack.pop()
        n = doc.nodes[i]
        attrs = " ".join("%s=%s" % (doc.strings[doc.attributes[a].name], rv(doc, doc.attributes[a])) for a in doc.attrs(i))
        out.append("%d %s {%s} %s\n" % (d, doc.strings[n.name], attrs, rv(doc, n)))
        for c in reversed(list(doc.children(i))):
            stack.append((c, d + 1))
    return "".join(out)


# ------------------------------------------------------------------------------------------------ install access

class Install:
    def __init__(self, root):
        self.root = Path(root)
        self.db = self.root / "Database"
        self.dbs = {}
        self.files = {}

    def element(self, name):
        if name not in self.dbs:
            self.dbs[name] = Document.read(self.db / (name + ".dx9.database")).to_element()
        return self.dbs[name]

    def assets(self, name):
        """(package element, asset element) for every asset with a heap region, in file order."""
        out = []
        for p in self.element(name).iter("Package"):
            for a in p.findall("Asset"):
                if a.find("Heap") is not None:
                    out.append((p, a))
        return out

    def read(self, name, p, a):
        heap, block, comp = a.find("Heap"), a.find("Block"), a.find("Compress")
        if block is not None and int(block.get("size")[6:]) != 0:
            raise ValueError("block resource")
        off = int(p.get("offset")[6:]) + int(heap.get("offset")[6:])
        size = int(heap.get("size")[6:])
        path = self.db / (p.get("file") + ".dx9.package")
        f = self.files.get(path)
        if f is None:
            f = self.files[path] = open(path, "rb")
        f.seek(off)
        raw = f.read(size)
        if len(raw) != size:
            raise ValueError("short read")
        if comp is not None and comp.get("enabled") == "_bool:true":
            if comp.get("codec") != "LZX":
                raise ValueError("codec")
            data, _ = decode_resource(raw, DEC)
            return data
        return raw

    def find(self, dbname, name, typ, package=None):
        for p, a in self.assets(dbname):
            if package and p.get("name") != package:
                continue
            if a.get("name") == name and a.get("type") == typ:
                return self.read(dbname, p, a)
        raise KeyError((dbname, name, typ))


# ------------------------------------------------------------------------------------------------ sections

def sec_vfs(inst):
    files = []
    for dp, dn, fn in os.walk(inst.root):
        for f in fn:
            full = Path(dp) / f
            if full.is_file():
                files.append((str(full.relative_to(inst.root)).replace(os.sep, "/"), full.stat().st_size))
    files.sort(key=lambda x: x[0].encode())
    emit("vfs files %d sha %s" % (len(files), sha("".join("%s\t%d\n" % x for x in files).encode())))


def uint(s):
    assert s.startswith("_uint:")
    return int(s[6:])


def sec_db(inst):
    for name in FULL_DBS:
        root = inst.element(name)
        pk = list(root.iter("Package"))
        handles = len(root.find("Handles")) if root.find("Handles") is not None else 0
        emit("db %s packages %d handles %d" % (name, len(pk), handles))
        lines = []
        for p in pk:
            seen, dups, n = set(), 0, 0
            for a in p.findall("Asset"):
                heap = a.find("Heap")
                if heap is None:
                    continue
                n += 1
                key = (a.get("name").lower(), a.get("type").lower())
                if key in seen:
                    dups += 1
                seen.add(key)
                comp, block = a.find("Compress"), a.find("Block")
                codec = "raw"
                if comp is not None and comp.get("enabled") == "_bool:true":
                    codec = "lzx" if comp.get("codec") == "LZX" else "other"
                lines.append("%s\t%s\t%d\t%d\t%d\t%s\n" % (a.get("type"), a.get("name"), uint(p.get("offset")) + uint(heap.get("offset")),
                                                              uint(heap.get("size")), uint(block.get("size")) if block is not None else 0, codec))
            emit("pkg %s file %s offset %d block %d heap %d assets %d dups %d" % (
                p.get("name"), p.get("file"), uint(p.get("offset")), uint(p.get("blockOffset")), uint(p.get("heapOffset")), n, dups))
        emit("assets sha %s" % sha("".join(lines).encode()))


def sec_assets(inst):
    for name in FULL_DBS:
        for p, a in inst.assets(name):
            try:
                d = inst.read(name, p, a)
                emit("a %s/%s/%s/%s %d %s" % (name, p.get("name"), a.get("type"), a.get("name"), len(d), sha(d)))
            except Exception:
                emit("a %s/%s/%s/%s ERR" % (name, p.get("name"), a.get("type"), a.get("name")))


def doc_line(label, doc, text):
    emit("bxml %s nodes %d attrs %d strings %d blob %d sha %s" % (label, len(doc.nodes), len(doc.attributes), len(doc.strings), len(doc.blob), sha(text.encode())))
    dump_file("bxml_" + label, text)


def sec_bxml(inst):
    for f in sorted(p for p in inst.root.iterdir() if p.suffix.lower() == ".bxml"):
        doc = TestDoc(f.read_bytes())
        doc_line(f.name, doc, tree_text(doc))
    for fname, path in [("MXRegistry.bxml", "/Registry/MXGame/StartInFrontEnd"), ("MXRegistry.bxml", "/Registry/MXGame/@startMode"),
                        ("MXRegistry.bxml", "/Registry/MXGame/UISceneName"), ("MXRegistry.bxml", "/Registry/MXGame/Nonexistent"),
                        ("RendererRegistry.bxml", "/Registry/Renderer/Startup/Windowed")]:
        d = Document.read(inst.root / fname)
        try:
            group, index = d.resolve(path)
            emit("resolve %s %s %s %d" % (fname, path, "attribute" if group == "attribute" else "node", index))
        except Exception:
            emit("resolve %s %s none" % (fname, path))
    for lv in LEVELS:
        for ext in ("level", "database"):
            doc = TestDoc((inst.db / ("%s.dx9.%s" % (lv, ext))).read_bytes())
            doc_line("%s.%s" % (lv, ext), doc, tree_text(doc))
        for p, a in inst.assets(lv):
            if a.get("type") == "material":
                doc = TestDoc(inst.read(lv, p, a))
                doc_line("%s/%s/%s.material" % (lv, p.get("name"), a.get("name")), doc, tree_text(doc))


def entity_fields(doc, node):
    """RSData node and {field name: [child node indices]} of an Entity node."""
    kids = list(doc.children(node))
    if len(kids) != 1 or doc.strings[doc.nodes[kids[0]].name] != "RSData":
        return None, {}
    rs = kids[0]
    fields = {}
    for f in doc.children(rs):
        name = None
        for a in doc.attrs(f):
            if doc.strings[doc.attributes[a].name] == "name":
                name = doc.value(doc.attributes[a])
        fields[name] = list(doc.children(f))
    return rs, fields


def scene_sections(data):
    magic, count = struct.unpack_from("<II", data)
    table = []
    for i in range(count):
        name, reserved, off = struct.unpack_from("<256sII", data, 8 + i * 264)
        table.append((name[:name.find(b"\0")].decode(), off))
    result = []
    for i, (name, start) in enumerate(table):
        end = table[i + 1][1] if i + 1 < count else len(data)
        chunks, entities = struct.unpack_from("<II", data, start)
        cursor, docs = start + 8, []
        for _ in range(chunks):
            size = 36 + struct.unpack_from("<I", data, cursor + 32)[0]
            docs.append(TestDoc(data[cursor:cursor + size]))
            cursor += size
        assert cursor == end
        result.append({"name": name, "offset": start, "bytes": end - start, "chunks": chunks, "entities": entities, "docs": docs})
    return result


def sec_scene(inst):
    for lv in LEVELS:
        data = (inst.db / (lv + ".dx9.scene")).read_bytes()
        secs = scene_sections(data)
        emit("scene %s size %d subscenes %d" % (lv, len(data), len(secs)))
        reference = read_sections(data, [s["name"] for s in secs])      # the Python reader's own validation and counts
        for s in secs:
            text, count = [], 0
            for doc in s["docs"]:
                for i, n in enumerate(doc.nodes):
                    if doc.strings[n.name] == "Entity":
                        count += 1
                        text.append("E\n")
                        text.append(tree_text(doc, i))
            assert count == reference[s["name"]]["entity_count"] == s["entities"]
            text = "".join(text)
            emit("sub %s offset %d bytes %d chunks %d entities %d sha %s" % (s["name"], s["offset"], s["bytes"], s["chunks"], count, sha(text.encode())))
            dump_file("scene_" + s["name"], text)
        by = {s["name"]: s for s in secs}
        fr = by.get(lv + "FreeRide")
        anchors = []
        for doc in fr["docs"]:
            for i, n in enumerate(doc.nodes):
                if doc.strings[n.name] != "Entity":
                    continue
                rs, fields = entity_fields(doc, i)
                ns = [doc.value(doc.attributes[a]) for a in doc.attrs(rs) if doc.strings[doc.attributes[a].name] == "namespace"][0]
                if not ns.endswith("StartEntityFreeRideRSData"):
                    continue
                eid = [doc.value(doc.attributes[a]) for a in doc.attrs(i) if doc.strings[doc.attributes[a].name] == "entityId"][0]
                m = doc.value(doc.nodes[fields["Matrix"][0]])
                anchors.append((doc.value(doc.nodes[fields["Index"][0]]), eid, doc.value(doc.nodes[fields["Name"][0]]), m))
        anchors.sort(key=lambda a: a[0])
        for idx, eid, name, m in anchors:
            emit("anchor %s id %d index %d name %s matrix %s" % (lv, eid, idx, js(name), " ".join(hx(x) for x in m)))


def table_rows(doc):
    """[(table name, [(col name, type, default)], [row cell lists])] of a tbldata document (strict reader, typed cells)."""
    tables = []
    for i, n in enumerate(doc.nodes):
        if doc.strings[n.name] != "Table":
            continue
        name, cols, rows = None, [], []
        for c in doc.children(i):
            tag = doc.strings[doc.nodes[c].name]
            if tag == "Name":
                name = doc.value(doc.nodes[c])
            elif tag == "Columns":
                for ch in doc.children(c):
                    a = {doc.strings[doc.attributes[x].name]: doc.value(doc.attributes[x]) for x in doc.attrs(ch)}
                    cols.append((a["name"], a.get("type", "string"), a.get("default", "")))
            elif tag == "Rows":
                for r in doc.children(c):
                    a = {doc.strings[doc.attributes[x].name]: (doc.attributes[x].kind, doc.value(doc.attributes[x])) for x in doc.attrs(r)}
                    cells = []
                    for cname, _, _ in cols:
                        if cname not in a:
                            cells.append("-")
                            continue
                        k, v = a[cname]
                        cells.append("i:%d" % v if k in (3, 4, 11) else "f:" + hx(v) if k == 5 else "s:" + js(v))
                    rows.append(cells)
        tables.append((name, cols, rows))
    return tables


def table_text(cols, rows, origins=None):
    t = ["col\t%s\t%s\t%s\n" % c for c in cols]
    for i, r in enumerate(rows):
        t.append("r\t%s%s\n" % (origins[i] + "\t" if origins else "", "\t".join(r)))
    return "".join(t)


def sec_tables(inst):
    default = Document(inst.find("MXTables_Default", "MXTables_Default", "tbldata"))
    for name, cols, rows in table_rows(default):
        emit("table %s cols %d rows %d sha %s" % (name, len(cols), len(rows), sha(table_text(cols, rows).encode())))
    # merged with the DLC databases
    merged, order = {}, []
    def add(tables, origin):
        for name, cols, rows in tables:
            if name not in merged:
                merged[name] = [cols, [], []]
                order.append(name)
            merged[name][1].extend(rows)
            merged[name][2].extend([origin] * len(rows))
    add(table_rows(default), "MXTables_Default")
    dlcs = sorted(f[:-len(".dx9.database")] for f in os.listdir(inst.db) if f.lower().endswith(".dx9.database") and "dlc" in f[:-len(".dx9.database")].lower())
    emit("dlc %s" % ",".join(dlcs))
    for dname in dlcs:
        pk = sorted(inst.element(dname).iter("Package"), key=lambda p: p.get("name").encode())
        for p in pk:
            for a in p.findall("Asset"):
                if a.get("type") == "tbldata" and a.find("Heap") is not None:
                    try:
                        add(table_rows(Document(inst.read(dname, p, a))), dname)
                    except Exception:
                        pass
    for name in order:
        cols, rows, origins = merged[name]
        hist = {}
        for o in origins:
            hist[o] = hist.get(o, 0) + 1
        emit("mtable %s cols %d rows %d origins %s sha %s" % (name, len(cols), len(rows), ",".join("%s:%d" % kv for kv in hist.items()), sha(table_text(cols, rows, origins).encode())))


def sec_loc(inst):
    for p, a in inst.assets("MXUI"):
        if a.get("type") != "localiz":
            continue
        d = inst.read("MXUI", p, a)
        r = ui_export.parse_localiz(d)
        keys = list(r["strings"].keys())
        t = []
        for k in keys:
            t.append("%s\t%s\t%s\t%d\n" % (k, js(r["strings"][k]), js(r["embedded"][k]) if k in r["embedded"] else "-", r["capacity"][k]))
        emit("loc %s/%s n %d sha %s" % (p.get("name"), a.get("name"), len(keys), sha("".join(t).encode())))


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def u8r_decode(data, w, h, mode5=False):
    """DXT1 mip 0 -> RGB bytes with the rounded palette of terrain_deform_reference.Dxt1('u8r'); whole image."""
    out = bytearray(w * h * 3)
    bw = (w + 3) // 4
    off = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            c0, c1, idx = struct.unpack_from("<HHI", data, off)
            off += 8
            a, b = tdr._rgb565(c0, "u8r"), tdr._rgb565(c1, "u8r")
            if c0 > c1:
                pal = [a, b, tuple((2 * a[i] + b[i] + 1) // 3 for i in range(3)), tuple((a[i] + 2 * b[i] + 1) // 3 for i in range(3))]
            else:
                pal = [a, b, tuple((a[i] + b[i] + 1) // 2 for i in range(3)), (0, 0, 0)]
            for k in range(16):
                x, y = bx + (k & 3), by + (k >> 2)
                if x < w and y < h:
                    out[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes(pal[(idx >> (2 * k)) & 3])
    return bytes(out)


def tex_header(data):
    h = struct.unpack_from("<14I", data)
    assert h[:4] == (0x77ddcc00, 4, 0, 1)
    levels, w, hh, fmt = h[10:14]
    return levels, w, hh, fmt


FMT_NAMES = {21: "A8R8G8B8", 22: "X8R8G8B8", 112: "G16R16F", 113: "A16B16G16R16F", 114: "R32F",
             int.from_bytes(b"DXT1", "little"): "DXT1", int.from_bytes(b"DXT3", "little"): "DXT3", int.from_bytes(b"DXT5", "little"): "DXT5"}


def material_summary(doc):
    """tmat lines from a material document (generic walk of the record tree)."""
    root = 0
    lines = []
    def tag(i):
        return doc.strings[doc.nodes[i].name]
    def attr(i, name):
        for a in doc.attrs(i):
            if doc.strings[doc.attributes[a].name] == name:
                return doc.value(doc.attributes[a])
        return None
    def pairs(i):
        n = doc.nodes[i]
        v = doc.value(n)
        raw = bytes.fromhex(v["opaque_bytes"])
        return " ".join("%d:%d" % kv for kv in struct.iter_unpack("<II", raw))
    pas = [c for c in doc.children(root) if tag(c) == "Pass"][0]
    shader, vs, ve, se, ps, pe = None, None, None, None, None, None
    textures, params, states = [], [], ""
    num_passes = attr(root, "numPasses")
    for c in doc.children(pas):
        t = tag(c)
        if t == "Shaders":
            shader = attr(c, "asset")
            for s in doc.children(c):
                if tag(s) == "VertexShader":
                    vs, ve, se = attr(s, "asset"), attr(s, "entryPoint"), attr(s, "skinningEntryPoint")
                elif tag(s) == "PixelShader":
                    ps, pe = attr(s, "asset"), attr(s, "entryPoint")
                    if shader is None:
                        shader = ps
        elif t == "Textures":
            for tx in doc.children(c):
                if tag(tx) != "Texture":
                    continue
                st = ""
                for s in doc.children(tx):
                    if tag(s) == "XenonSamplerStates":
                        st = pairs(s)
                textures.append((attr(tx, "textureName") or "", st))
        elif t == "Parameters":
            for p in doc.children(c):
                if tag(p) != "Parameter" or attr(p, "name") is None:
                    continue
                v = doc.nodes[p]
                val = doc.value(v)
                if v.kind == 7:
                    comp = " ".join(hx(x) for x in val)
                elif v.kind == 5:
                    comp = hx(val)
                else:
                    comp = "-"
                params.append((attr(p, "name"), attr(p, "type"), comp))
        elif t == "XenonStates":
            states = pairs(c)
    lines.append("mat passes %s shader %s vs %s/%s/%s ps %s/%s ntex %d nparam %d" % (num_passes, js(shader), js(vs), js(ve), js(se), js(ps), js(pe), len(textures), len(params)))
    for i, (n, st) in enumerate(textures):
        lines.append("mat tex %d %s %s" % (i, js(n), st))
    for n, t, c in params:
        lines.append("mat param %s %s %s" % (js(n), js(t), c))
    lines.append("mat states %s" % states)
    return lines


def sec_terrain(inst):
    for lv in LEVELS:
        data = (inst.db / (lv + ".dx9.scene")).read_bytes()
        secs = scene_sections(data)
        base = [s for s in secs if s["name"] == lv + "Base"][0]
        terr = None
        shadow = None
        for doc in base["docs"]:
            for i, n in enumerate(doc.nodes):
                if doc.strings[n.name] != "Entity":
                    continue
                rs, fields = entity_fields(doc, i)
                ns = [doc.value(doc.attributes[a]) for a in doc.attrs(rs) if doc.strings[doc.attributes[a].name] == "namespace"][0]
                eid = [doc.value(doc.attributes[a]) for a in doc.attrs(i) if doc.strings[doc.attributes[a].name] == "entityId"][0]
                if ns.endswith("_HFTerrainRSData"):
                    terr = (doc, fields, eid)
                elif ns.endswith("_ShadowMapRSData"):
                    shadow = (doc, fields)
        doc, fields, eid = terr
        def asset(name):
            if name not in fields:
                return "-"
            n = fields[name][0]
            for a in doc.attrs(n):
                if doc.strings[doc.attributes[a].name] == "name":
                    return js(doc.value(doc.attributes[a]))
            return "-"
        def val(name, tag=None):
            for c in fields[name]:
                if tag is None or doc.strings[doc.nodes[c].name] == tag:
                    return doc.value(doc.nodes[c])
        grid = val("GridResolution", "EnumValue")
        sh = "-"
        if shadow:
            sdoc, sf = shadow
            n = sf["VolumeShadowMapAsset"][0]
            for a in sdoc.attrs(n):
                if sdoc.strings[sdoc.attributes[a].name] == "name":
                    sh = js(sdoc.value(sdoc.attributes[a]))
        emit("tparams %s id %d name %s matrix %s height %s material %s softness %s deform %s tracks %s gen %s mx %s atv %s utv %s pro4 %s prolite %s buggy %s zones %s shadow %s grid %d spacing %d edge %s" % (
            lv, eid, js(val("Name")), " ".join(hx(x) for x in val("Matrix")), asset("HeightMapAsset"), asset("MaterialAsset"), asset("SoftnessMask"),
            asset("TerrainDeformation"), asset("TerrainTracks"), asset("GenericTrack"), asset("MXTrack"), asset("ATVTrack"), asset("UTVTrack"),
            asset("Pro4Track"), asset("ProLiteTrack"), asset("BuggyTrack"), asset("FrictionZones"), sh, grid, 1 << grid, hx(val("EdgeBuffer"))))
        hname = json.loads(asset("HeightMapAsset"))
        tex = inst.find(lv, hname, "texture")
        info = texture_decode.parse_texture(tex)
        raw = texture_decode.height_bytes(tex, info)
        from array import array
        heights = array("f")
        heights.frombytes(raw)
        size = info["width"]
        emit("height %s %d sha %s" % (lv, size, sha(raw)))
        coarse = lex.coarse_bounds(heights, size, 2048.0)
        emit("coarse %s %d sha %s" % (lv, size // 16, sha(coarse)))
        sname = json.loads(asset("SoftnessMask"))
        st = inst.find(lv, sname, "texture")
        levels, w, h, fmt = tex_header(st)
        if fmt == texture_decode.DXT1:
            off = 56 + 8 * levels
            rgb = u8r_decode(st[off:], w, h)
            ref = tdr.Dxt1(st, "u8r")
            for (x, y) in [(0, 0), (w - 1, h - 1), (w // 3, h // 7), (1023, 513), (5, 2000), (777, 31), (w // 2, h // 2)]:
                x, y = x % w, y % h
                c = ref.texel(0, x, y)
                assert tuple(round(v * 255) for v in c) == tuple(rgb[(y * w + x) * 3:(y * w + x) * 3 + 3]), "u8r decoder disagrees with terrain_deform_reference"
            emit("soft %s %d %d DXT1 sha %s" % (lv, w, h, sha(rgb)))
        else:
            emit("soft %s %d %d %s" % (lv, w, h, FMT_NAMES.get(fmt, "?")))
        mname = json.loads(asset("MaterialAsset"))
        mdoc = TestDoc(inst.find(lv, mname, "material"))
        for l in material_summary(mdoc):
            emit("t%s %s" % (l, lv))
        zname = json.loads(asset("FrictionZones"))
        zd = TestDoc(inst.find(lv, zname, "packdata"))
        zf = {zd.strings[zd.nodes[c].name]: zd.value(zd.nodes[c]) for c in zd.children(0)}
        parts = ["inter %d" % zf["Interleaved"]]
        words_all = hashlib.sha256()
        samples = []
        for k in range(1, 5):
            wds = zf["Data%d" % k]
            nb, wd, ht = zf["NumBits%d" % k], zf["Width%d" % k], zf["Height%d" % k]
            assert zf["NumWords%d" % k] == len(wds)
            raw_words = struct.pack("<%dI" % len(wds), *wds)
            big = int.from_bytes(raw_words, "little")
            sm = []
            for (x, y) in [(0, 0), (wd - 1, ht - 1), (100, 200), (511, 512), (1000, 3), (3, 1000), (640, 640), (123, 456)]:
                x, y = x % wd, y % ht
                i = y * wd + x
                sm.append("%d" % ((big >> (i * nb)) & ((1 << nb) - 1)))
            parts.append("map%d %dx%d bits %d words %d sha %s samples %s" % (k, wd, ht, nb, len(wds), sha(raw_words), ",".join(sm)))
        emit("zones %s %s" % (lv, " | ".join(parts)))


def collis_summary(data):
    magic, tag, version, kind, zero, count = struct.unpack_from("<I4sIIII", data)
    if kind == 22:
        k, chunks = aco.parse_collis(data)
        return kind, count, "tris %d" % sum(len(c["triangles"]) for c in chunks)
    if kind == 15:
        pool = struct.unpack_from("<I", data, 24)[0]
        pool_bytes = data[28:28 + pool]
        cur, hist, names, h = 28 + pool, {}, [], hashlib.sha256()
        for _ in range(count):
            t = struct.unpack_from("<I", data, cur)[0]
            size = {5: 88, 7: 76, 8: 80}[t]
            link = struct.unpack_from("<i", data, cur + 68)[0]
            if 0 <= link < pool:
                names.append(pool_bytes[link:pool_bytes.index(b"\0", link)].decode())
            params = list(struct.unpack_from("<%df" % {5: 3, 7: 1, 8: 2}[t], data, cur + 72)) + [0.0] * (3 - {5: 3, 7: 1, 8: 2}[t])
            tail = struct.unpack_from("<I", data, cur + 84)[0] if t == 5 else 0
            h.update(data[cur + 4:cur + 68] + b"".join(struct.pack("<f", x) for x in params) + struct.pack("<i", link) + struct.pack("<I", tail))
            hist[t] = hist.get(t, 0) + 1
            cur += size
        assert cur == len(data)
        return kind, count, "pool %d types %s names %s params %s" % (pool, ",".join("%d:%d" % kv for kv in sorted(hist.items())), ",".join(names) or "-", h.hexdigest())
    return kind, count, "raw %d" % len(data)


def sec_collis(inst):
    for name in COLLIS_DBS:
        for p, a in inst.assets(name):
            if a.get("type") != "collis":
                continue
            try:
                d = inst.read(name, p, a)
                kind, count, summary = collis_summary(d)
                emit("collis %s/%s/%s kind %d count %d %s" % (name, p.get("name"), a.get("name"), kind, count, summary))
            except Exception as e:
                emit("collis %s/%s/%s ERR" % (name, p.get("name"), a.get("name")))
    for lv in LEVELS:
        data = (inst.db / (lv + ".dx9.scene")).read_bytes()
        secs = {s["name"]: s for s in scene_sections(data)}
        for sname in (lv + "Base", lv + "FreeRide"):
            for doc in secs[sname]["docs"]:
                for i, n in enumerate(doc.nodes):
                    if doc.strings[n.name] != "Entity":
                        continue
                    rs, fields = entity_fields(doc, i)
                    ns = [doc.value(doc.attributes[a]) for a in doc.attrs(rs) if doc.strings[doc.attributes[a].name] == "namespace"][0]
                    if not ns.endswith("_CollisionRSData"):
                        continue
                    eid = [doc.value(doc.attributes[a]) for a in doc.attrs(i) if doc.strings[doc.attributes[a].name] == "entityId"][0]
                    asset = None
                    for a in doc.attrs(fields["FemaleCollisionAsset"][0]):
                        if doc.strings[doc.attributes[a].name] == "name":
                            asset = doc.value(doc.attributes[a])
                    nm = doc.value(doc.nodes[fields["Name"][0]]) if "Name" in fields else None
                    sm = doc.value(doc.nodes[fields["Matrix"][0]])
                    data_c = None
                    for p, a in inst.assets(lv):
                        if a.get("type") == "collis" and a.get("name") == asset:
                            data_c = inst.read(lv, p, a)
                            break
                    em = aco.entity_matrix(sm)
                    kind = struct.unpack_from("<I", data_c, 12)[0]
                    emit("place %s sub %s id %d name %s asset %s kind %d smat %s emat %s" % (
                        lv, sname, eid, js(nm) if nm is not None else "-", asset, kind, " ".join(hx(x) for x in sm), " ".join(hx(x) for x in em)))
                    if kind == 22:
                        _, chunks = aco.parse_collis(data_c)
                        for ci, c in enumerate(chunks):
                            pos = aco.transform(c["center"], em)
                            emit("chunk %d center %s n %d sha %s pos %s" % (ci, " ".join(hx(x) for x in c["center"]), len(c["triangles"]), sha(c["raw"]), " ".join(hx(x) for x in pos)))


def model_candidates(inst):
    out = []
    for name in MODEL_DBS:
        n = 0
        for p, a in inst.assets(name):
            if a.get("type") != "model":
                continue
            if n >= MODEL_LIMIT:
                break
            n += 1
            out.append((name, p, a))
    return out


def sec_model(inst, samples):
    for name, p, a in model_candidates(inst):
        key = "%s/%s/%s" % (name, p.get("name"), a.get("name"))
        try:
            m = bae.parse_model(inst.read(name, p, a), lenient_bindings=True)
            sdata = None
            for p2, a2 in inst.assets(name):
                if p2 is p and a2.get("type") == "surface" and a2.get("name") == a.get("name"):
                    sdata = inst.read(name, p2, a2)
            surfs = bae.parse_surfaces(sdata, fallback_normals=True) if sdata else None
        except Exception:
            continue
        samples.append("model %s %s %s %s" % (name, p.get("name"), a.get("name"), "1" if surfs else "0"))
        lines = ["name %s flags %d nodes %d lods %d bindings %d refs %d\n" % (js(m["name"]), m["flags"], len(m["nodes"]), len(m["lods"]), len(m["bindings"]), m["surface_references"])]
        # the record floats are not in the parser's output: read them again from the resource
        data = inst.read(name, p, a)
        count = len(m["nodes"])
        records_at = 172 + count * 64
        for i, nd in enumerate(m["nodes"]):
            extra = struct.unpack_from("<9f", data, records_at + i * 44 + 8)
            lines.append("node %d %s %d L %s W %s X %s\n" % (i, js(nd["name"]), -1 if nd["parent"] is None else nd["parent"], " ".join(hx(x) for x in nd["local"]),
                                                          " ".join(hx(x) for x in nd["world"]), " ".join(hx(x) for x in extra)))
        for l in m["lods"]:
            lines.append("lod %d %d %d\n" % tuple(l))
        for b in m["bindings"]:
            lines.append("bind %s %d %d\n" % (js(b["material"]), b["surface"], b["node"]))
        text = "".join(lines)
        emit("model %s sha %s" % (key, sha(text.encode())))
        dump_file("model_" + key, text)
        if surfs:
            for i, s in enumerate(surfs):
                pos = struct.pack("<%df" % (3 * len(s["positions"])), *[c for v in s["positions"] for c in v])
                nrm = struct.pack("<%df" % (3 * len(s["normals"])), *[c for v in s["normals"] for c in v])
                uv = struct.pack("<%df" % (2 * len(s["uvs"])), *[c for v in s["uvs"] for c in v])
                tri = struct.pack("<%dI" % (3 * len(s["triangles"])), *[c for t in s["triangles"] for c in t])
                emit("surf %s %d skinned %d verts %d tris %d strip %d repaired %d palette %s pos %s nrm %s uv %s tri %s" % (
                    key, i, 1 if s["skinned"] else 0, len(s["positions"]), len(s["triangles"]), s["strip_indices"], len(s["repaired_normal_vertices"]),
                    bytes(s["bone_palette"]).hex() or "-", sha(pos), sha(nrm), sha(uv), sha(tri)))


def sec_tex(inst, samples):
    for name in MODEL_DBS[:3]:
        n = 0
        for p, a in inst.assets(name):
            if a.get("type") != "texture":
                continue
            if n >= TEX_LIMIT:
                break
            try:
                d = inst.read(name, p, a)
                levels, w, h, fmt = tex_header(d)
            except Exception:
                continue
            n += 1
            sizes = struct.unpack_from("<%dI" % (2 * levels), d, 56)
            line = "tex %s/%s/%s fmt %s %dx%d levels %d mips %s" % (name, p.get("name"), a.get("name"), FMT_NAMES.get(fmt, "?"), w, h, levels, sha(struct.pack("<%dI" % len(sizes), *sizes)))
            decoded = 0
            if w * h <= TEX_DECODE_PIXELS and fmt == texture_decode.DXT1:
                try:
                    px, _, _ = texture_decode.decode_dxt1(d)
                    line += " rgba %s" % sha(px)
                    decoded = 1
                except Exception:
                    pass
            elif w * h <= TEX_DECODE_PIXELS and fmt == 21:
                decoded = 1
                size, pitch = struct.unpack_from("<II", d, 56)
                src = d[56 + 8 * levels:56 + 8 * levels + size]
                out = bytearray(size)
                out[0::4], out[1::4], out[2::4], out[3::4] = src[2::4], src[1::4], src[0::4], src[3::4]
                line += " rgba %s" % sha(bytes(out))
            samples.append("texture %s %s %s %d" % (name, p.get("name"), a.get("name"), decoded))
            emit(line)


def main():
    global DEC, DUMPDIR
    root, out, samples_path = sys.argv[1], sys.argv[2], sys.argv[3]
    DUMPDIR = sys.argv[4] if len(sys.argv) > 4 else None
    if DUMPDIR:
        Path(DUMPDIR).mkdir(parents=True, exist_ok=True)
    DEC = LzxDecoder()
    inst = Install(root)
    samples = []
    for fn in (sec_vfs, sec_db, sec_assets, sec_bxml, sec_scene, sec_tables, sec_loc, sec_terrain, sec_collis):
        fn(inst)
    sec_model(inst, samples)
    sec_tex(inst, samples)
    Path(out).write_text("\n".join(OUTL) + "\n", encoding="utf-8")
    Path(samples_path).write_text("\n".join(samples) + "\n", encoding="utf-8")
    print("python reference: %d lines, %d samples" % (len(OUTL), len(samples)))


if __name__ == "__main__":
    main()
