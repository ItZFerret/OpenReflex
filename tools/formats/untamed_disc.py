#!/usr/bin/env python3
"""Read-only inventory of an MX vs ATV Untamed (Xbox 360) disc: the disc image (.iso, XDVDFS) or a folder the disc was extracted into.

The reference for the client's Untamed disc check (Unity/Reflex/Assets/Scripts/Content/UntamedImport/UntamedImporter.cs, plan:
core/plan/UNTAMED_TRACKS.md): prints the title id from default.xex and, for each stadium level, the sizes of its four
Database/<level>.xenon.{level,database,package,scene} files and its AI/<level>-<mode>.tdf07 race line, with the SHA-256 of the files the
converter reads (database, scene, tdf07; --package also hashes the .package, about 600 MB for the 12 tracks), then the disc's other
stadium levels. --csharp prints the rows of UntamedImporter.KnownTracks: another revision of the disc is supported by adding its rows after
the ones there (the check takes the first row a level's files match). Standard library only; nothing is written.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys

SECTOR = 2048
# (name, game partition offset): the order DiscVolume (AliveContainers.cs) tries
PARTITIONS = (("XGD2", 0x0FD90000), ("XGD3", 0x02080000), ("XGD1", 0x18300000), ("game partition", 0))
MAGIC = b"MICROSOFT*XBOX*MEDIA"
SUFFIXES = ("level", "database", "package", "scene")

# (level, short name, race mode = the tdf07 suffix, the location name: MX07Tables' Free Ride name). The 10 supercross and 2 EnduroCross
# stadiums the Python Untamed converter (the reference) builds; the disc's other *_STADIUM levels are listed as not supported.
TRACKS = (
    ("Norton_STADIUM", "norton", "sx", "Norton Coliseum"),
    ("Capital_City_STADIUM", "capitalcity", "sx", "Capital City Stadium"),
    ("Columbus_STADIUM", "columbus", "sx", "Columbus Stadium"),
    ("Denton_STADIUM", "denton", "sx", "Denton Stadium"),
    ("HarrisPark_STADIUM", "harrispark", "sx", "Harris Park Stadium"),
    ("Foothills_STADIUM", "foothills", "sx", "Foothills Stadium"),
    ("WestStone_STADIUM", "weststone", "sx", "West Stone Gardens Stadium"),
    ("Jefferson_STADIUM", "jefferson", "sx", "Jefferson Stadium"),
    ("Wilford_STADIUM", "wilford", "sx", "Wilford Stadium"),
    ("BridgePort_STADIUM", "bridgeport", "sx", "Bridgeport Stadium"),
    ("Fairview_STADIUM", "fairview", "ec", "Fairview Arena"),
    ("Lincoln_STADIUM", "lincoln", "ec", "Lincoln Memorial Arena"),
)


class Disc:
    """An XDVDFS image: files by '/'-separated path (case kept), read in place."""

    def __init__(self, path: Path):
        self.f = open(path, "rb")
        size = path.stat().st_size
        self.base, self.layout = -1, None
        for name, off in PARTITIONS:
            if off + 33 * SECTOR > size:
                continue
            self.f.seek(off + 32 * SECTOR)
            head = self.f.read(SECTOR)
            if head.startswith(MAGIC) and head[0x7EC:0x7EC + len(MAGIC)] == MAGIC:
                self.base, self.layout = off, name
                break
        if self.base < 0:
            raise SystemExit(f"{path}: not an Xbox 360 game disc image (no XDVDFS volume descriptor)")
        root, root_size = struct.unpack_from("<II", head, 20)
        self.files: dict[str, tuple[int, int]] = {}
        self._walk(root, root_size, "", 0)

    def _walk(self, sector: int, size: int, prefix: str, depth: int) -> None:
        if size == 0 or depth > 64:
            return
        self.f.seek(self.base + sector * SECTOR)
        data = self.f.read(size)
        stack, seen = [0], set()
        while stack:
            off = stack.pop()
            if off in seen or off + 14 > len(data):
                continue
            seen.add(off)
            left, right, first, length, attr, nl = struct.unpack_from("<HHIIBB", data, off)
            if left == 0xFFFF and right == 0xFFFF:
                continue
            name = data[off + 14:off + 14 + nl].decode("ascii")
            if right:
                stack.append(right * 4)
            if left:
                stack.append(left * 4)
            if attr & 0x10:
                self._walk(first, length, prefix + name + "/", depth + 1)
            else:
                self.files[prefix + name] = (first, length)

    def size(self, path: str) -> int | None:
        hit = self._find(path)
        return None if hit is None else self.files[hit][1]

    def read(self, path: str, limit: int | None = None) -> bytes:
        first, length = self.files[self._find(path)]
        self.f.seek(self.base + first * SECTOR)
        return self.f.read(length if limit is None else min(limit, length))

    def _find(self, path: str) -> str | None:
        low = path.lower()
        return next((p for p in self.files if p.lower() == low), None)


class Folder:
    """An extracted disc folder (the one holding Database and AI)."""

    def __init__(self, path: Path):
        self.root, self.layout = path, "folder"

    def _file(self, path: str) -> Path | None:
        p = self.root
        for part in path.split("/"):
            hit = next((c for c in p.iterdir() if c.name.lower() == part.lower()), None) if p.is_dir() else None
            if hit is None:
                return None
            p = hit
        return p

    @property
    def files(self) -> dict[str, None]:
        """The Database folder's files (the other stadiums are listed from them)."""
        db = self._file("Database")
        return {} if db is None or not db.is_dir() else {f"Database/{c.name}": None for c in db.iterdir() if c.is_file()}

    def size(self, path: str) -> int | None:
        p = self._file(path)
        return None if p is None or not p.is_file() else p.stat().st_size

    def read(self, path: str, limit: int | None = None) -> bytes:
        with open(self._file(path), "rb") as f:
            return f.read() if limit is None else f.read(limit)


def title_id(src) -> int | None:
    """default.xex: the title id of the execution info optional header (key 0x00040006)."""
    if src.size("default.xex") is None:
        return None
    xex = src.read("default.xex", 0x10000)
    if xex[:4] != b"XEX2":
        return None
    count = struct.unpack_from(">I", xex, 0x14)[0]
    for i in range(min(count, 64)):
        key, value = struct.unpack_from(">II", xex, 0x18 + i * 8)
        if key == 0x00040006 and value + 16 <= len(xex):
            return struct.unpack_from(">I", xex, value + 12)[0]
    return None


def sha256(src, path: str) -> str:
    return hashlib.sha256(src.read(path)).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("source", type=Path, help="the disc image (.iso) or the extracted disc folder")
    ap.add_argument("--package", action="store_true", help="also hash each level's .package")
    ap.add_argument("--csharp", action="store_true", help="print UntamedImporter.KnownTracks rows (implies --package)")
    a = ap.parse_args()
    src = Folder(a.source) if a.source.is_dir() else Disc(a.source)
    tid = title_id(src)
    print(f"// {a.source.name}: layout {src.layout}, title {tid:08X}" if tid is not None else f"// {a.source.name}: layout {src.layout}, no default.xex")
    missing = 0
    for level, short, mode, name in TRACKS:
        paths = [f"Database/{level}.xenon.{s}" for s in SUFFIXES]
        tdf = f"AI/{level}-{mode}.tdf07"
        sizes = [src.size(p) for p in paths + [tdf]]
        if any(s is None for s in sizes):
            print(f"// {level}: missing " + ", ".join(p for p, s in zip(paths + [tdf], sizes) if s is None))
            missing += 1
            continue
        db, scene, tdf_sha = sha256(src, paths[1]), sha256(src, paths[3]), sha256(src, tdf)
        pkg = sha256(src, paths[2]) if a.package or a.csharp else None
        if a.csharp:
            print(f'            new UntamedKnown("{level}", "{short}", "{mode}", "{name}", {sizes[0]}, {sizes[1]}, {sizes[2]}, {sizes[3]}, {sizes[4]},\n'
                  f'                "{db}", "{scene}", "{tdf_sha}", "{pkg}"),')
        else:
            print(f"{level} {short} {mode} sizes {' '.join(map(str, sizes))} database {db} scene {scene} tdf07 {tdf_sha}" + (f" package {pkg}" if pkg else ""))
    others = sorted({p.split("/")[-1].split(".")[0] for p in src.files
                     if p.lower().startswith("database/") and p.lower().endswith("_stadium.xenon.level")} - {t[0] for t in TRACKS})
    if others:
        print("// not supported: " + ", ".join(others))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
