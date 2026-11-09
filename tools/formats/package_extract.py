#!/usr/bin/env python3
"""Read-only extraction of observed DX9 heap resources using original metadata.

Supports the LZX resource wrapper verified against default tables and frontend
Lua. Uses the separate LGPL libmspack decoder in out/libmspack; see README.
All CLI outputs are restricted to formats/out. No game/runtime dependency.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import struct

from bxml import BxmlError, Document, HERE, OUT, require, write_output

MAX_RESOURCE = 64 * 1024 * 1024


def metadata_uint(value):
    require(value is not None and value.startswith("_uint:"), "Expected typed unsigned metadata")
    try:
        number = int(value[6:])
    except ValueError as exc:
        raise BxmlError("Invalid unsigned metadata") from exc
    require(0 <= number <= 0xFFFFFFFF, "Unsigned metadata out of range")
    return number


def split_frames(data):
    """Remove XMem frame headers, preserving one continuous LZX chunk stream."""
    position, decoded, frames = 0, 0, []
    while position < len(data) - 5:
        if data[position] == 0xFF:
            require(position + 5 <= len(data), "Truncated long XMem frame header")
            output_size, input_size = struct.unpack_from(">HH", data, position + 1)
            position += 5
        else:
            require(position + 2 <= len(data), "Truncated XMem frame header")
            input_size = struct.unpack_from(">H", data, position)[0]
            output_size = 32768
            position += 2
        require(0 < output_size <= 32768 and input_size > 0, "Invalid XMem frame sizes")
        require(position + input_size <= len(data) - 5, "Truncated XMem frame data")
        frames.append(data[position:position + input_size])
        position += input_size
        decoded += output_size
        require(decoded <= 65536, "Unverified chunk larger than 64 KiB")
    require(frames and data[position:] == b"\0" * 5, "Expected five-byte XMem terminator")
    return b"".join(frames), decoded


class LzxDecoder:
    def __init__(self, library=None):
        path = Path(library) if library else OUT / "libmspack/libreflex_lzx.dylib"
        require(path.is_file(), "Missing native LZX bridge; see README build command")
        self.library = ctypes.CDLL(str(path.resolve()))
        self.decode = self.library.reflex_lzx_decode
        self.decode.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                                ctypes.c_size_t, ctypes.c_int]
        self.decode.restype = ctypes.c_int

    def __call__(self, source, length):
        require(0 < length <= 65536, "LZX output exceeds observed chunk limit")
        destination = ctypes.create_string_buffer(length)
        result = self.decode(source, len(source), destination, length, 17)
        require(result == 0, f"libmspack LZX error {result}")
        return destination.raw


def decode_resource(raw, decoder):
    require(16 <= len(raw) <= MAX_RESOURCE, "Invalid resource size")
    region_end = struct.unpack_from("<I", raw)[0]
    require(region_end + 4 == len(raw), "Resource wrapper length differs from heap metadata")
    last_length = struct.unpack_from("<I", raw, region_end)[0]
    require(0 < last_length <= 65536, "Invalid final chunk logical length")
    position, chunks, descriptions = 4, [], []
    while position < region_end:
        require(position + 8 <= region_end, "Truncated resource chunk header")
        capacity, compressed = struct.unpack_from("<II", raw, position)
        position += 8
        require(capacity == 65536, "Unverified resource chunk allocation size")
        require(0 < compressed and position + compressed <= region_end, "Invalid compressed chunk size")
        data = raw[position:position + compressed]
        position += compressed
        # Do not guess about the uncompressed branch of the game's loader.
        require(compressed != capacity, "Unverified raw resource chunk")
        stream, length = split_frames(data)
        expected = last_length if position == region_end else capacity
        require(length == expected, "XMem length differs from resource chunk length")
        require(sum(len(c) for c in chunks) + length <= MAX_RESOURCE, "Resource output exceeds limit")
        decoded = decoder(stream, length)
        require(len(decoded) == length, "Decoder returned the wrong length")
        chunks.append(decoded)
        descriptions.append({"allocated_bytes": capacity, "compressed_bytes": compressed,
                             "decoded_bytes": length})
    require(position == region_end and chunks, "Resource chunk boundary mismatch")
    return b"".join(chunks), descriptions


def extract(database, package_name, asset_name, asset_type, decoder=None):
    database = Path(database).resolve()
    require(database.name.endswith(".dx9.database"), "Only observed DX9 metadata is supported")
    root = Document.read(database).to_element()
    packages = [p for p in root.iter("Package") if p.get("name") == package_name]
    require(len(packages) == 1, "Package name missing or ambiguous")
    package = packages[0]
    assets = [a for a in package.findall("Asset")
              if a.get("name") == asset_name and a.get("type") == asset_type]
    require(len(assets) == 1, "Asset name/type missing or ambiguous")
    asset = assets[0]
    heap, block, compress = asset.find("Heap"), asset.find("Block"), asset.find("Compress")
    require(heap is not None and block is not None and compress is not None, "Missing resource metadata")
    require(metadata_uint(block.get("size")) == 0, "Block resources are not supported")
    require(compress.get("enabled") == "_bool:true" and compress.get("codec") == "LZX",
            "Only observed LZX resources are supported")
    filename = package.get("file", "")
    require(filename and Path(filename).name == filename and "\\" not in filename,
            "Package file must be a sibling basename")
    package_path = database.parent / (filename + ".dx9.package")
    offset = metadata_uint(package.get("offset")) + metadata_uint(heap.get("offset"))
    size = metadata_uint(heap.get("size"))
    require(0 < size <= MAX_RESOURCE and offset + size <= package_path.stat().st_size,
            "Heap region exceeds source package")
    with package_path.open("rb") as stream:
        stream.seek(offset)
        raw = stream.read(size)
    decoded, chunks = decode_resource(raw, decoder or LzxDecoder())
    validation = "raw resource decoded; content not interpreted"
    if asset_type == "script":
        require(len(decoded) >= 4 and struct.unpack_from("<I", decoded)[0] == len(decoded) - 4,
                "Lua script length prefix mismatch")
        decoded = decoded[4:]
        decoded.decode("utf-8")
        validation = "Lua source: exact length prefix and UTF-8 validated"
    elif asset_type in ("tbldata", "bxml"):
        Document(decoded)
        validation = "BXML decoded and structurally validated"
    report = {"database": str(database), "package_file": str(package_path),
              "package": package_name, "asset": asset_name, "type": asset_type,
              "source_offset": offset, "source_bytes": size,
              "source_region_sha256": hashlib.sha256(raw).hexdigest(),
              "output_bytes": len(decoded), "output_sha256": hashlib.sha256(decoded).hexdigest(),
              "chunks": chunks, "validation": validation}
    return decoded, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("--package", required=True)
    parser.add_argument("--asset", required=True)
    parser.add_argument("--type", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data, report = extract(args.database, args.package, args.asset, args.type)
    require(args.output.resolve() != Path(report["package_file"]).resolve(), "Refusing to overwrite package input")
    path = write_output(args.output, data, source=args.database)
    write_output(Path(str(path) + ".json"), (json.dumps(report, indent=2) + "\n").encode())
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
