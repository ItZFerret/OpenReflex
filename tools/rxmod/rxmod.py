#!/usr/bin/env python3
"""rxmod.py -- packs, checks and reads Open Reflex bike mods (.rxmod: an RXAR container of kind MODS). Plain Python 3, no packages.

The format is core/plan/RXMOD.md (the container: core/plan/RXARCHIVE.md); the game's reader is
Unity/Reflex/Assets/Scripts/Content/RxModBikes.cs. A mod is made as a folder (mod.json + .rxmesh + maps), which the game also loads as it
is (drop the folder in Mods/Bikes while you work on it), then packed into one .rxmod file to share.

  python3 rxmod.py check   <folder | file.rxmod>          mod.json and every bike's mesh and maps read back, with a summary
  python3 rxmod.py pack    <folder> <out.rxmod>            the folder (mod.json and the files it names + any .txt / pictures) into one file
  python3 rxmod.py list    <file.rxmod>                    the files, their sizes, the metadata
  python3 rxmod.py extract <file.rxmod> <dir>              every file to <dir>/<path> (a folder the game loads, and the next pack's input)
  python3 rxmod.py new-skin <bike .rxmod | folder> <out folder> [--bike B] [--name N] [--author A]
                                                           a skin mod to paint: the bike's own diffuse map(s) copied in as the starting
                                                           point and a mod.json that maps them over the bike's (core/plan/RXMOD.md "Skins")
  python3 rxmod.py pack-kit <kit pack folder> <out.rxmod> [--name N] [--author A]
                                                           a classic kit pack (MXTables_DLC0NN + its graphicskit / MXUI .dx9 files, byte for
                                                           byte) as one .rxmod for Mods/Gear (RXMOD.md "Kit packs")
  check / pack also take a skins mod ("type": "skins"); check --base <bike .rxmod> also checks that the maps it replaces are the bike's.
"""
import json, os, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "rxtrack"))   # (the repository)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))                                    # (a kit: rxtrack.py beside it)
from rxtrack import write_archive, read_archive, read_entry, path_ok, HEADER   # noqa: E402  (the RXAR reference writer / reader)

KIND_MODS = 0x53444F4D          # "MODS" as a little-endian u32
FORMAT = "reflex-mod"
TEMPLATES = ("plastic", "frame", "engine", "wheel", "tire", "chain")
NODES = ("OBJ_Frame", "OBJ_UPR_Forks", "OBJ_LWR_Forks", "OBJ_FRT_Tire", "OBJ_FRT_Fender", "OBJ_Swingarm", "OBJ_BCK_Tire", "OBJ_BCK_Fender",
         "OBJ_Throttle", "OBJ_ClutchLVR", "OBJ_BrakeLVR")
PICTURES = (".png", ".jpg", ".jpeg")

class Source:
    """A mod folder or a .rxmod file, read the same way."""
    def __init__(self, path):
        self.path = path
        self.archive = os.path.isfile(path)
        if self.archive:
            kind, self.version, self.meta, entries = read_archive(path)
            assert kind == KIND_MODS, "%s: not a mod archive (kind %08x)" % (path, kind)
            self.entries = {e["path"].lower(): e for e in entries}
            self.names = [e["path"] for e in entries]
        else:
            self.names = []
            for d, _, files in os.walk(path):
                for n in files:
                    if n.startswith(".") or n in ("Thumbs.db", "desktop.ini"): continue
                    self.names.append(os.path.relpath(os.path.join(d, n), path).replace(os.sep, "/"))
            self.names.sort()

    def has(self, name):
        return name is not None and (name.lower() in self.entries if self.archive else os.path.isfile(os.path.join(self.path, name)))

    def read(self, name):
        if self.archive:
            return read_entry(self.path, self.entries[name.lower()])
        with open(os.path.join(self.path, name), "rb") as f:
            return f.read()

    def size(self, name):
        return self.entries[name.lower()]["raw"] if self.archive else os.path.getsize(os.path.join(self.path, name))


def read_mesh(data):
    """The .rxmesh summary: nodes (name, parent, world 4x4 row-major), materials, surfaces (material, node, flags, vertices, triangles, bounds)."""
    at = 0
    def take(fmt):
        nonlocal at
        v = struct.unpack_from("<" + fmt, data, at); at += struct.calcsize("<" + fmt); return v
    def string():
        nonlocal at
        n, = take("H"); s = data[at:at + n].decode("utf-8"); at += n; return s
    assert data[:4] == b"RXMS", "not an .rxmesh"
    at = 4
    version, flags = take("II"); assert version == 1, "rxmesh version %d" % version
    nodes = []
    for _ in range(take("I")[0]):
        name = string(); parent, = take("i"); world = take("16f")
        assert -1 <= parent < len(nodes), "node order"
        nodes.append(dict(name=name, parent=parent, world=world))
    materials = [string() for _ in range(take("I")[0])]
    surfaces = []
    for _ in range(take("I")[0]):
        mat, node, sflags, v, ix = take("IIIII")
        assert mat < len(materials) and node < len(nodes) and ix % 3 == 0 and v > 0, "surface header"
        pos = struct.unpack_from("<%df" % (3 * v), data, at)
        lo = [min(pos[k::3]) for k in range(3)]; hi = [max(pos[k::3]) for k in range(3)]
        at += 4 * (3 * v + 3 * v + 2 * v)
        if sflags & 2: at += 8 * v
        if sflags & 4: at += 16 * v
        if sflags & 1: at += 4 * v + 16 * v
        idx = struct.unpack_from("<%dI" % ix, data, at); at += 4 * ix
        assert max(idx) < v, "index past the vertices"
        surfaces.append(dict(material=materials[mat], node=nodes[node]["name"], flags=sflags, vertices=v, triangles=ix // 3, lo=lo, hi=hi))
    assert at == len(data), "trailing bytes in the mesh"
    return dict(nodes=nodes, materials=materials, surfaces=surfaces)


def png_size(data):
    if data[:8] == b"\x89PNG\r\n\x1a\n": return struct.unpack(">II", data[16:24])
    if data[:2] == b"\xff\xd8":                          # JPEG: the first SOFn marker
        i = 2
        while i + 9 < len(data):
            if data[i] != 0xFF: i += 1; continue
            m = data[i + 1]; ln = struct.unpack(">H", data[i + 2:i + 4])[0]
            if m in (0xC0, 0xC1, 0xC2): return struct.unpack(">HH", data[i + 7:i + 9])[::-1]
            i += 2 + ln
    return None


def check_map(src, name, bad, what):
    if not src.has(name): bad("%s: %s is missing" % (what, name)); return
    sz = png_size(src.read(name))
    if sz is None: bad("%s: %s is not a PNG / JPG" % (what, name))
    elif max(sz) > 4096 or sz[0] & (sz[0] - 1) or sz[1] & (sz[1] - 1): bad("%s: %s is %dx%d (a power of two, at most 4096)" % (what, name, sz[0], sz[1]))


def check_skins(src, m, bad, base=None):
    out = ["%s by %s: %d skin(s)%s" % (m.get("name", "?"), m.get("author", "?"), len(m.get("skins", [])), " [archive]" if src.archive else "")]
    bm = None
    if base:
        bs = Source(base); bm = json.loads(bs.read("mod.json"))
    for i, k in enumerate(m.get("skins", [])):
        if not k.get("mod") and k.get("original") is None: bad("skin %d: no \"mod\" (the bike mod's id or name) nor \"original\" (an original bike's VehicleModelId)" % i)
        if not k.get("maps") and not k.get("materials"): bad("skin %d replaces no map" % i)
        for key, e in (k.get("maps") or {}).items(): check_map(src, e, bad, "skin %d map %s" % (i, key))
        for mat, x in (k.get("materials") or {}).items():
            for f in ("diffuse", "spec", "normal", "sfx"):
                if x.get(f): check_map(src, x[f], bad, "skin %d material %s %s" % (i, mat, f))
        if bm:
            n = lambda v: "".join(c for c in str(v or "").lower() if c.isalnum())
            if n(k.get("mod")) not in (n(bm.get("id")), n(bm.get("name"))): bad("skin %d: \"mod\" %s is not %s (id %s)" % (i, k.get("mod"), bm.get("name"), bm.get("id")))
            used = {x.get(f) for b in bm.get("bikes", []) for x in b.get("materials", {}).values() for f in ("diffuse", "spec", "normal", "sfx") if x.get(f)}
            for key in (k.get("maps") or {}):
                if key not in used: bad("skin %d: map %s is not a map of the bike mod (it has %s)" % (i, key, ", ".join(sorted(used))))
        what = ("the original bike #%s" % k["original"]) if k.get("original") is not None else "%s / %s" % (k.get("mod"), k.get("bike"))
        out.append("  skin %d \"%s\" paints %s: %d map(s), %d material(s)" % (i, k.get("name", ""), what, len(k.get("maps") or {}), len(k.get("materials") or {})))
    return out


def check(path, quiet=False, base=None):
    """Reads everything a game reads; returns (manifest, problems)."""
    src = Source(path)
    problems = []
    def bad(msg): problems.append(msg)
    if not src.has("mod.json"):
        return None, ["no mod.json"]
    m = json.loads(src.read("mod.json").decode("utf-8"))
    if m.get("format") != FORMAT: bad('mod.json "format" is not "%s"' % FORMAT)
    if m.get("type") == "kits":
        names = [n for n in src.names if n.lower().endswith((".dx9.database", ".dx9.package"))]
        tables = [n for n in names if n.split("/")[-1].lower().startswith("mxtables_") and n.lower().endswith(".dx9.database")]
        for t in tables:
            if t[:-len(".dx9.database")].lower() + ".dx9.package" not in [x.lower() for x in names]: bad("%s has no .dx9.package beside it" % t)
        if not tables: bad("no MXTables_<slot>.dx9.database (a kit pack's table)")
        if not quiet:
            print("%s by %s: kit pack, %d .dx9 files (%s)" % (m.get("name"), m.get("author"), len(names), ", ".join(t.split("/")[-1][:-13] for t in tables)))
            for p in problems: print("  PROBLEM: " + p)
            print("ok" if not problems else "%d problem(s)" % len(problems))
        return m, problems
    if m.get("type", "bikes") in ("skins", "skin"):
        if not m.get("author"): bad("no author (the garage credits the skin's author)")
        out = check_skins(src, m, bad, base)
        if not quiet:
            print("\n".join(out))
            for p in problems: print("  PROBLEM: " + p)
            print("ok" if not problems else "%d problem(s)" % len(problems))
        return m, problems
    if m.get("type", "bikes") not in ("bikes", "bike"): bad('mod.json "type" is not "bikes" or "skins"')
    if not m.get("author"): bad("no author (the garage credits the mod's author)")
    out = ["%s by %s: %d bike(s)%s" % (m.get("name", "?"), m.get("author", "?"), len(m.get("bikes", [])), " [archive]" if src.archive else "")]
    if m.get("thumbnail") and not src.has(m["thumbnail"]): bad("thumbnail %s is missing" % m["thumbnail"])
    for i, b in enumerate(m.get("bikes", [])):
        bike = b.get("bike")
        if not (isinstance(bike, int) or (isinstance(bike, str) and bike.isdigit())): bad("bike %d: \"bike\" must be the original's VehicleModelId" % i)
        if not src.has(b.get("mesh")): bad("bike %d: mesh %s is missing" % (i, b.get("mesh"))); continue
        mesh = read_mesh(src.read(b["mesh"]))
        names = [n["name"] for n in mesh["nodes"]]
        for need in ("OBJ_FRT_Tire", "OBJ_BCK_Tire"):
            if need not in names: bad("bike %d: the mesh has no %s node (its axle)" % (i, need))
        for n in names:
            if n not in NODES: bad("bike %d: node %s is not an original hierarchy node (drawn on its parent)" % (i, n))
        tri = sum(s["triangles"] for s in mesh["surfaces"]); ver = sum(s["vertices"] for s in mesh["surfaces"])
        world = {n["name"]: n["world"] for n in mesh["nodes"]}
        wb = None
        if "OBJ_FRT_Tire" in world and "OBJ_BCK_Tire" in world:
            f, r = world["OBJ_FRT_Tire"], world["OBJ_BCK_Tire"]
            wb = ((f[3] - r[3]) ** 2 + (f[7] - r[7]) ** 2 + (f[11] - r[11]) ** 2) ** 0.5
            if not 3.5 < wb < 6.0: bad("bike %d: the axles are %.2f ft apart (a bike's are ~4.8 ft: feet, Z forward?)" % (i, wb))
            if f[11] < r[11]: bad("bike %d: the front axle is behind the rear one (Z must point forward)" % i)
        mats = b.get("materials", {})
        for s in mesh["surfaces"]:
            if s["material"] not in mats: bad("bike %d: material %s of the mesh is not in mod.json" % (i, s["material"]))
        for k, x in mats.items():
            if x.get("template", "plastic") not in TEMPLATES: bad("bike %d: material %s: template %s" % (i, k, x.get("template")))
            for key in ("diffuse", "spec", "normal", "sfx"):
                if x.get(key) is None: continue
                if not src.has(x[key]): bad("bike %d: material %s: %s %s is missing" % (i, k, key, x[key])); continue
                sz = png_size(src.read(x[key]))
                if sz is None: bad("bike %d: %s is not a PNG / JPG" % (i, x[key]))
                elif max(sz) > 4096 or sz[0] & (sz[0] - 1) or sz[1] & (sz[1] - 1): bad("bike %d: %s is %dx%d (a power of two, at most 4096)" % (i, x[key], sz[0], sz[1]))
        out.append("  #%s %s -> %s %s | %s: %d nodes, %d surfaces, %d vertices, %d triangles, wheelbase %s ft, materials %s"
                   % (bike, b.get("original", ""), b.get("brand", ""), b.get("model", b.get("name", "")), b["mesh"], len(names), len(mesh["surfaces"]),
                      ver, tri, "%.3f" % wb if wb else "?", ", ".join("%s:%s" % (k, v.get("template", "plastic")) for k, v in mats.items())))
    if not quiet:
        print("\n".join(out))
        for p in problems: print("  PROBLEM: " + p)
        print("ok" if not problems else "%d problem(s)" % len(problems))
    return m, problems


def pack(folder, out):
    m, problems = check(folder, quiet=True)
    assert m is not None and not problems, "the folder does not check out: " + "; ".join(problems)
    src = Source(folder)
    wanted = {"mod.json"}
    if m.get("thumbnail"): wanted.add(m["thumbnail"])
    for k in m.get("skins", []):
        if k.get("logo"): wanted.add(k["logo"])
        wanted.update((k.get("maps") or {}).values())
        for x in (k.get("materials") or {}).values():
            for key in ("diffuse", "spec", "normal", "sfx"):
                if x.get(key): wanted.add(x[key])
    for b in m.get("bikes", []):
        wanted.add(b["mesh"])
        if b.get("logo"): wanted.add(b["logo"])
        for x in b.get("materials", {}).values():
            for key in ("diffuse", "spec", "normal", "sfx"):
                if x.get(key): wanted.add(x[key])
    extra = [n for n in src.names if n.lower().endswith((".txt", ".md")) and "/" not in n]
    files = []
    for n in sorted(wanted | set(extra)):
        assert path_ok(n), "bad path " + n
        files.append((n, src.read(n)))
    meta = {"format": FORMAT, "type": m.get("type", "bikes"), "name": m.get("name"), "author": m.get("author"),
            "bikes": [b.get("bike") for b in m.get("bikes", [])], "skins": [[k.get("mod"), k.get("bike"), k.get("name")] for k in m.get("skins", [])]}
    write_archive(out, files, meta, kind=KIND_MODS)
    print("%s: %d files, %.1f MB" % (out, len(files), os.path.getsize(out) / 1048576))


def pack_kit(folder, out, name=None, author=""):
    """A classic kit pack folder (its .dx9 files byte for byte; a mod.ini / pictures / .txt beside them kept) as one .rxmod."""
    files = []
    for d, _, fs in os.walk(folder):
        for n in sorted(fs):
            if n.startswith("."): continue
            low = n.lower()
            if low.endswith((".dx9.database", ".dx9.package", ".png", ".jpg", ".txt", ".ini")):
                rel = os.path.relpath(os.path.join(d, n), folder).replace(os.sep, "/")
                with open(os.path.join(d, n), "rb") as f: files.append((rel, f.read()))   # (at the root: the unpacked folder is named after the .rxmod)
    assert any(p.split("/")[-1].lower().startswith("mxtables_") for p, _ in files), folder + " holds no MXTables_<slot>.dx9.database: not a kit pack"
    name = name or os.path.basename(os.path.normpath(folder))
    m = {"format": FORMAT, "version": 1, "type": "kits", "name": name, "author": author,
         "description": "a classic kit pack (bike graphics kits / rider gear), its .dx9 files as downloaded"}
    files.insert(0, ("mod.json", json.dumps(m, indent=1).encode("utf-8")))
    write_archive(out, files, {"format": FORMAT, "type": "kits", "name": name, "author": author}, kind=KIND_MODS)
    print("%s: %d files, %.1f MB" % (out, len(files), os.path.getsize(out) / 1048576))


def new_skin(base, out, bike=None, name="My skin", author=""):
    """A skins mod folder to paint: the bike's diffuse map(s) copied in (the starting point) and a mod.json mapping them over the bike's."""
    src = Source(base); bm = json.loads(src.read("mod.json"))
    bikes = bm.get("bikes", [])
    n = lambda v: "".join(c for c in str(v or "").lower() if c.isalnum())
    if bike: bikes = [b for b in bikes if n(bike) in (n(b.get("id")), n(b.get("model")), n(b.get("bike")), n(str(b.get("brand", "")) + str(b.get("model", ""))))]
    assert bikes, "no bike %s in %s (it has %s)" % (bike, base, ", ".join(str(b.get("model", b.get("bike"))) for b in bm.get("bikes", [])))
    b = bikes[0]
    diffuse = sorted({x["diffuse"] for x in b.get("materials", {}).values() if x.get("diffuse")})
    os.makedirs(out, exist_ok=True)
    maps = {}
    for d in diffuse:
        dst = "skin/" + d.split("/")[-1]
        os.makedirs(os.path.join(out, "skin"), exist_ok=True)
        with open(os.path.join(out, *dst.split("/")), "wb") as f: f.write(src.read(d))
        maps[d] = dst
    m = {"format": FORMAT, "version": 1, "type": "skins", "name": name, "author": author, "description": "",
         "skins": [{"mod": bm.get("id") or bm.get("name"), "bike": b.get("id") or b.get("model") or b.get("bike"), "name": name, "maps": maps}]}
    with open(os.path.join(out, "mod.json"), "w") as f: json.dump(m, f, indent=1)
    print("%s: paint %s, then: rxmod.py check %s --base %s; rxmod.py pack %s <name>.rxmod" % (out, ", ".join(maps.values()), out, base, out))


def main(argv):
    if len(argv) < 2:
        print(__doc__); return 2
    cmd = argv[1]
    if cmd == "check" and len(argv) in (3, 5) and (len(argv) == 3 or argv[3] == "--base"):
        return 1 if check(argv[2], base=argv[4] if len(argv) == 5 else None)[1] else 0
    if cmd == "pack-kit" and len(argv) >= 4:
        opts = dict(zip(argv[4::2], argv[5::2]))
        pack_kit(argv[2], argv[3], opts.get("--name"), opts.get("--author", "")); return 0
    if cmd == "new-skin" and len(argv) >= 4:
        opts = dict(zip(argv[4::2], argv[5::2]))
        new_skin(argv[2], argv[3], opts.get("--bike"), opts.get("--name", "My skin"), opts.get("--author", ""))
        return 0
    if cmd == "pack" and len(argv) == 4:
        pack(argv[2], argv[3]); return 0
    if cmd == "list" and len(argv) == 3:
        kind, version, meta, entries = read_archive(argv[2])
        print("kind %08x, container version %d, metadata %s" % (kind, version, json.dumps(meta)))
        for e in entries: print("%12d  %s" % (e["raw"], e["path"]))
        return 0
    if cmd == "extract" and len(argv) == 4:
        src = Source(argv[2])
        for n in src.names:
            p = os.path.join(argv[3], *n.split("/"))
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as f: f.write(src.read(n))
        print("%d files to %s" % (len(src.names), argv[3])); return 0
    print(__doc__); return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
