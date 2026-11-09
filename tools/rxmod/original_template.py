#!/usr/bin/env python3
"""original_template.py -- an ORIGINAL bike's graphics kit textures as PNG templates to paint a .rxmod skin on (core/plan/RXMOD.md "Skins").

Reads the player's own Reflex install (its Database folder, or the Open Reflex game library's "game files/Archive/Database"); writes PNGs
only into the output folder. Nothing of the game goes into a skin you share except what you painted over it: share your own art.

  python3 original_template.py <Database dir> --list                 the bikes' chassis (MarsF_Lite, VenusF ...) and their kits
  python3 original_template.py <Database dir> <chassis> <out dir>    every kit of that chassis: its plastics (and seat) textures as PNG,
                                                                     the high-resolution twin too (HR_*, the garage's) - paint on the HR one

The original bikes' chassis (VehicleModelId: chassis): 11 Blaze 250F / 1 Blaze 450F: MarsF_Lite / MarsF; 13 / 14 Dakumoto: NeptuneF_Lite /
NeptuneF; 18 / 28 KTM SX-F: MercuryF_Lite / MercuryF; 17 / 10 Shima: EarthF_Lite / EarthF; 31 / 32 Takashi: VenusF_Lite / VenusF.
A skin's "maps" keys for an original bike: "plastics", "seat" (the roles: the seat samples the FULL-SIZE bike's plastics atlas, the second
file of a 250's kit), or a texture's name as written here (without HR_).
Needs the repository's tools/formats readers (and their LZX bridge for the compressed packages: python3 tools/formats/build_lzx.py --fetch).
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "formats"))
from bxml import Document                                    # noqa: E402
from package_extract import extract                          # noqa: E402
from texture_decode import parse_texture, decode_dxt1, rgba_png   # noqa: E402

SKIP = ("_SFX", "Numbers", "Mud_Noise", "Hguard", "CrewChief", "NRML", "_Norm", "dirtmask", "Mud_WET")


def packages(db):
    return [p.get("name") for p in Document.read(db).to_element().iter("Package")]


def textures(db, pkg):
    for p in Document.read(db).to_element().iter("Package"):
        if p.get("name") == pkg: return [a.get("name") for a in p.findall("Asset") if a.get("type") == "texture"]
    return []


def kits_of(dbdir, chassis):
    """The MX_GraphicKits packages of a chassis: MX_<chassis>_<kit> (MarsF's kits are not MarsF_Lite's)."""
    all_ = packages(os.path.join(dbdir, "MX_GraphicKits.dx9.database"))
    pre = "MX_" + chassis + "_"
    return [k for k in all_ if k.startswith(pre) and (chassis.endswith("_Lite") or not k[len(pre):].startswith("Lite_"))]


def main(a):
    if len(a) >= 2 and a[1] == "--list":
        for c in ("MarsF_Lite", "MarsF", "NeptuneF_Lite", "NeptuneF", "MercuryF_Lite", "MercuryF", "EarthF_Lite", "EarthF", "VenusF_Lite", "VenusF"):
            print("%-14s %s" % (c, ", ".join(k[len(c) + 4:] for k in kits_of(a[0], c))))
        return 0
    if len(a) != 3: print(__doc__); return 2
    dbdir, chassis, out = a
    os.makedirs(out, exist_ok=True)
    n = 0
    for kit in kits_of(dbdir, chassis):
        for db, pkg in (("MX_GraphicKits", kit), ("HR_MX_GraphicKits", "HR_" + kit)):
            dbf = os.path.join(dbdir, db + ".dx9.database")
            for t in textures(dbf, pkg):
                if any(s.lower() in t.lower() for s in SKIP): continue
                data = extract(dbf, pkg, t, "texture")[0]
                try:
                    tex = parse_texture(data); px, w, h = decode_dxt1(data, tex)
                except Exception as e:   # (a format this reader lacks: DXT5 ...)
                    print("  skipped %s/%s: %s" % (pkg, t, e)); continue
                f = os.path.join(out, "%s__%s.png" % (pkg, t))
                with open(f, "wb") as o: o.write(rgba_png(px, w, h))
                print("%s %dx%d" % (f, w, h)); n += 1
    print("%d template(s) in %s" % (n, out))
    return 0 if n else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
