#!/usr/bin/env python3
"""rxtrack.py -- a dependency-free reference writer / reader of Reflex track archives (.rxtrack version 5: an RXAR container of kind TRAK).

The format is core/plan/RXARCHIVE.md; the game's reader is core/src/fmt_rxar.c. This file is meant to be READ by tool authors (a track
builder that outputs .rxtrack) as much as run: everything a writer must get right is here, in plain Python 3 standard library.

  python3 rxtrack.py pack <folder> <out.rxtrack> [--stem STEM] [--name NAME]   a Reflex track folder (<stem>.dx9.database / .level /
                                                                              .package / .scene + optional mod.ini, mod.json, pictures,
                                                                              .txt) into one .rxtrack
  python3 rxtrack.py list <file.rxtrack>                                       the files, their codecs and sizes, the metadata
  python3 rxtrack.py verify <file.rxtrack>                                     every file read back: CRC-32 and SHA-256 checked
  python3 rxtrack.py extract <file.rxtrack> <dir>                              every file to <dir>/<path>

Compression: files are written STORED (codec 0). A Reflex package (.dx9.package) is LZX-compressed inside already, so zstd gains almost
nothing on it; the game's own tools store it too. If the `zstandard` module is installed, the small files (database, level, scene) are
written as zstd frames (codec 1), like core/tools/rxpack.c does. The index is always a zstd frame: without `zstandard` this writes a valid
zstd frame of raw (uncompressed) blocks, which every zstd decoder reads.
"""
import hashlib, json, os, struct, sys, time, zlib

try:
    import zstandard  # optional
except ImportError:
    zstandard = None

MAGIC, INDEX_MAGIC = b"RXAR", b"RXIX"
KIND_DATA, KIND_TRACK = 0x41544144, 0x4B415254          # "DATA" / "TRAK" as little-endian u32
CODEC_STORE, CODEC_ZSTD, CODEC_POOL = 0, 1, 2
FLAG_SHA = 1
HEADER = 64
EXTS = (".dx9.level", ".dx9.database", ".dx9.package", ".dx9.scene")   # the four files of a track, in the content id's order
DRESS = ("mod.ini", "mod.json")
DRESS_EXT = (".png", ".jpg", ".jpeg", ".txt")


# ------------------------------------------------------------------------------------------------ zstd (index frame without a library)

def zstd_raw_frame(data):
    """A valid zstd frame holding `data` uncompressed: magic, a frame header with the content size (single segment, 8-byte size field),
    then raw blocks of at most 128 KiB (block header: bit 0 last block, bits 1-2 block type 0 = raw, bits 3-23 size)."""
    out = bytearray(b"\x28\xb5\x2f\xfd")                     # 0xFD2FB528, little endian
    out.append(0xE0)                                         # frame header descriptor: content size field 8 bytes, single segment
    out += struct.pack("<Q", len(data))
    if not data:
        out += (1).to_bytes(3, "little")                     # one empty last raw block
    for at in range(0, len(data), 128 * 1024):
        chunk = data[at:at + 128 * 1024]
        last = at + len(chunk) >= len(data)
        out += ((len(chunk) << 3) | (0 << 1) | (1 if last else 0)).to_bytes(3, "little")
        out += chunk
    return bytes(out)


def zstd_compress(data):
    if zstandard is None:
        return zstd_raw_frame(data)
    return zstandard.ZstdCompressor(level=19, write_checksum=True, write_content_size=True).compress(data)


def zstd_decompress(frame, size):
    if zstandard is not None:
        return zstandard.ZstdDecompressor().decompress(frame, max_output_size=size)
    # (reading without the library: only raw-block frames, as this file writes them)
    assert frame[:4] == b"\x28\xb5\x2f\xfd" and frame[4] == 0xE0, "install `zstandard` to read zstd-compressed data"
    at, out = 13, bytearray()
    while True:
        h = int.from_bytes(frame[at:at + 3], "little"); at += 3
        kind, n = (h >> 1) & 3, h >> 3
        assert kind == 0, "install `zstandard` to read zstd-compressed data"
        out += frame[at:at + n]; at += n
        if h & 1:
            return bytes(out)


# ------------------------------------------------------------------------------------------------ writing

def path_ok(p):
    """What an archive path may be: relative, '/' separators, no empty / '.' / '..' component, no '\\', ':' or control byte, <= 1023 bytes."""
    b = p.encode("utf-8")
    if not b or len(b) > 1023 or p.startswith("/") or p.endswith("/"):
        return False
    if any(c < 0x20 or c == 0x7F or c in b"\\:" for c in b):
        return False
    return all(part not in ("", ".", "..") for part in p.split("/"))


def write_archive(out_path, files, meta, kind=KIND_TRACK):
    """files: list of (archive path, bytes). meta: a dict (written as UTF-8 JSON). Writes <out>.partial, then renames it."""
    seen = set()
    for p, _ in files:
        assert path_ok(p), "bad archive path: " + p
        assert p.lower() not in seen, "two files named " + p + " (paths are case-insensitive)"
        seen.add(p.lower())
    tmp = out_path + ".partial"
    entries = []
    with open(tmp, "wb") as f:
        f.write(b"\0" * HEADER)                              # the header is written LAST: an unfinished file never reads as an archive
        stored_once = {}                                     # sha256 -> (offset, stored, codec): the same bytes are stored once
        for p, data in files:
            sha = hashlib.sha256(data).digest()
            crc = zlib.crc32(data) & 0xFFFFFFFF
            if sha in stored_once:
                offset, stored, codec = stored_once[sha]
            else:
                codec, payload = CODEC_STORE, data
                small_and_compressible = zstandard is not None and 0 < len(data) <= 4 << 20 and not p.lower().endswith(".dx9.package")
                if small_and_compressible:
                    z = zstd_compress(data)
                    if len(z) <= len(data) * 0.97:
                        codec, payload = CODEC_ZSTD, z
                offset, stored = f.tell(), len(payload)
                f.write(payload)
                stored_once[sha] = (offset, stored, codec)
            entries.append((p, codec, len(data), offset, stored, crc, sha))
        index_offset = f.tell()
        meta_bytes = json.dumps(meta, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        raw = bytearray(INDEX_MAGIC + struct.pack("<II", len(entries), len(meta_bytes)) + meta_bytes)
        for p, codec, size, offset, stored, crc, sha in sorted(entries, key=lambda e: e[0].encode("utf-8")):   # sorted by path bytes
            pb = p.encode("utf-8")
            raw += struct.pack("<H", len(pb)) + pb
            raw += struct.pack("<BBHQQQI", codec, 0, FLAG_SHA, size, offset, stored, crc)       # 32 bytes (chunk_log2 0: one frame)
            raw += sha                                                                          # flags bit 0: the SHA-256 follows
        packed = zstd_compress(bytes(raw))
        f.write(packed)
        header = bytearray(MAGIC + struct.pack("<III", 2, kind, 0))                             # container version 2, kind, flags 0
        header += struct.pack("<QQQ", index_offset, len(packed), len(raw))
        header += struct.pack("<II", len(entries), zlib.crc32(bytes(raw)) & 0xFFFFFFFF)
        header += b"\0" * 12
        header += struct.pack("<I", zlib.crc32(bytes(header)) & 0xFFFFFFFF)                     # CRC-32 of bytes 0..59
        assert len(header) == HEADER
        f.seek(0); f.write(header)
        f.flush(); os.fsync(f.fileno())
    os.replace(tmp, out_path)


# ------------------------------------------------------------------------------------------------ reading

def read_archive(path):
    """(kind, version, meta dict, [entry dicts]) of an archive; an entry: path, codec, chunk_log2, raw, offset, stored, crc, sha, chunks."""
    with open(path, "rb") as f:
        h = f.read(HEADER)
        assert h[:4] == MAGIC, "not a Reflex archive (a gzip RXPK .rxtrack is the older MTX terrain package)"
        version, kind, flags = struct.unpack_from("<III", h, 4)
        assert version in (1, 2), "unknown container version %d" % version
        assert flags == 0, "unknown archive flags %d" % flags
        assert zlib.crc32(h[:60]) & 0xFFFFFFFF == struct.unpack_from("<I", h, 60)[0], "damaged header"
        index_offset, stored, raw_size = struct.unpack_from("<QQQ", h, 16)
        count, index_crc = struct.unpack_from("<II", h, 40)
        f.seek(index_offset)
        raw = zstd_decompress(f.read(stored), raw_size)
    assert len(raw) == raw_size and zlib.crc32(raw) & 0xFFFFFFFF == index_crc, "damaged index"
    assert raw[:4] == INDEX_MAGIC and struct.unpack_from("<I", raw, 4)[0] == count
    meta_len = struct.unpack_from("<I", raw, 8)[0]
    meta = json.loads(raw[12:12 + meta_len] or b"{}")
    at, entries = 12 + meta_len, []
    for _ in range(count):
        pl = struct.unpack_from("<H", raw, at)[0]; at += 2
        p = raw[at:at + pl].decode("utf-8"); at += pl
        codec, chunk_log2, eflags, size, offset, st, crc = struct.unpack_from("<BBHQQQI", raw, at); at += 32
        sha = None
        if eflags & FLAG_SHA:
            sha = raw[at:at + 32]; at += 32
        chunks = None
        if chunk_log2:
            n = struct.unpack_from("<I", raw, at)[0]; at += 4
            chunks = list(struct.unpack_from("<%dI" % n, raw, at)); at += 4 * n
        entries.append(dict(path=p, codec=codec, chunk_log2=chunk_log2, raw=size, offset=offset, stored=st, crc=crc, sha=sha, chunks=chunks))
    return kind, version, meta, entries


def read_entry(path, e):
    assert e["codec"] != CODEC_POOL, e["path"] + " is a pool reference (only inside an imported game library, never in a shared track)"
    with open(path, "rb") as f:
        f.seek(e["offset"])
        payload = f.read(e["stored"])
    if e["codec"] == CODEC_STORE:
        return payload
    if not e["chunk_log2"]:
        return zstd_decompress(payload, e["raw"])
    out, at = bytearray(), 0
    for n in e["chunks"]:                                    # independent zstd frames of 2^chunk_log2 raw bytes each
        out += zstd_decompress(payload[at:at + n], 1 << e["chunk_log2"]); at += n
    return bytes(out)


# ------------------------------------------------------------------------------------------------ a track folder

def track_files(folder, stem=None):
    names = sorted(n for n in os.listdir(folder) if os.path.isfile(os.path.join(folder, n)) and not n.startswith("."))
    if stem is None:
        stems = [n[:-len(".dx9.level")] for n in names if n.lower().endswith(".dx9.level")]
        assert len(stems) == 1, "%s: %d tracks here (name one with --stem)" % (folder, len(stems))
        stem = stems[0]
    by_lower = {n.lower(): n for n in names}
    four = []
    for ext in EXTS:
        real = by_lower.get((stem + ext).lower())
        assert real, "%s: %s%s is missing (a track is its .dx9.database, .level, .package and .scene)" % (folder, stem, ext)
        four.append(os.path.join(folder, real))
    dress = [os.path.join(folder, n) for n in names if n.lower() in DRESS or n.lower().endswith(DRESS_EXT)]
    return stem, four, dress


def pack(folder, out, stem=None, name=None):
    stem, four, dress = track_files(folder, stem)
    files, listed = [], []
    for p, ext in zip(four, EXTS):
        data = open(p, "rb").read()
        files.append(("Database/" + stem + ext, data))      # the stem as the set names it, the extension lower case
        listed.append({"name": stem + ext, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    for p in dress:
        files.append(("Mod/" + os.path.basename(p), open(p, "rb").read()))
    meta = {"format": "reflex-track-archive", "version": 1, "level": stem, "name": name or os.path.basename(os.path.normpath(folder)),
            "source": "mod", "layout": "packed", "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "files": sorted(listed, key=lambda x: ["database", "level", "scene", "package"].index(x["name"].rsplit(".", 1)[1]))}
    write_archive(out, files, meta)
    return stem, len(files)


def main(argv):
    if len(argv) < 3:
        print(__doc__); return 2
    cmd = argv[1]
    opt = lambda k: argv[argv.index(k) + 1] if k in argv else None
    if cmd == "pack":
        stem, n = pack(argv[2], argv[3], opt("--stem"), opt("--name"))
        print("PACK %s: %s, %d files, %d bytes" % (argv[3], stem, n, os.path.getsize(argv[3])))
    elif cmd == "list":
        kind, version, meta, entries = read_archive(argv[2])
        print("%s: %s, container version %d, %d files\nmeta %s" % (argv[2], struct.pack("<I", kind).decode(), version, len(entries), json.dumps(meta)))
        for e in entries:
            print("%-5s %12d %12d  %s" % (("store", "zstd", "pool")[e["codec"]], e["raw"], e["stored"], e["path"]))
    elif cmd == "verify":
        kind, version, meta, entries = read_archive(argv[2])
        bad = 0
        for e in entries:
            data = read_entry(argv[2], e)
            ok = len(data) == e["raw"] and zlib.crc32(data) & 0xFFFFFFFF == e["crc"] and (e["sha"] is None or hashlib.sha256(data).digest() == e["sha"])
            if not ok:
                bad += 1; print("BAD", e["path"])
        print("VERIFY %s: %d files, %d bad" % (argv[2], len(entries), bad))
        return 1 if bad else 0
    elif cmd == "extract":
        kind, version, meta, entries = read_archive(argv[2])
        for e in entries:
            dest = os.path.join(argv[3], *e["path"].split("/"))
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            open(dest, "wb").write(read_entry(argv[2], e))
        print("EXTRACT %d files to %s" % (len(entries), argv[3]))
    else:
        print(__doc__); return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
