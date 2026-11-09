"""Bounded decoder for observed Reflex v4 DX9 single-image texture resources.

Only the R32F height map and DXT1 color variants used by Alpine are supported.
All mip descriptors and payload boundaries are checked before exposing pixels.
"""
import math
import struct
import zlib

from bxml import require

R32F = 114
DXT1 = int.from_bytes(b"DXT1", "little")


def parse_texture(data):
    require(56 <= len(data) <= 128 * 1024 * 1024, "Texture resource size outside bounds")
    header = struct.unpack_from("<14I", data)
    require(header[:4] == (0x77ddcc00, 4, 0, 1), "Unsupported texture magic/version/image count")
    require(header[5:10] == (0,) * 5, "Unsupported texture flags or reserved fields")
    levels, width, height, format_id = header[10:14]
    require(header[4] == levels and 1 <= levels <= 14, "Texture mip count differs from descriptor count")
    require(1 <= width <= 8192 and 1 <= height <= 8192 and width & (width - 1) == 0
            and height & (height - 1) == 0, "Only bounded power-of-two textures supported")
    require(levels <= max(width, height).bit_length(), "Too many texture mip levels")
    require(format_id in (R32F, DXT1), "Unsupported texture format")
    offset = 56 + 8 * levels
    require(offset <= len(data), "Truncated texture mip descriptors")
    mips = []
    for level in range(levels):
        w, h = max(1, width >> level), max(1, height >> level)
        size, pitch = struct.unpack_from("<II", data, 56 + level * 8)
        expected_pitch = w * 4 if format_id == R32F else max(1, (w + 3) // 4) * 8
        expected_size = expected_pitch * (h if format_id == R32F else max(1, (h + 3) // 4))
        require((size, pitch) == (expected_size, expected_pitch), "Mip byte size/pitch differs from dimensions")
        require(offset + size <= len(data), "Truncated texture mip pixels")
        mips.append({"level": level, "width": w, "height": h, "offset": offset,
                     "bytes": size, "pitch": pitch})
        offset += size
    require(offset == len(data), "Unexpected trailing texture bytes")
    return {"width": width, "height": height, "format_id": format_id,
            "format": "R32F" if format_id == R32F else "DXT1", "mips": mips}


def height_bytes(data, texture=None):
    texture = texture or parse_texture(data)
    require(texture["format_id"] == R32F, "Height map must contain R32F pixels")
    mip = texture["mips"][0]
    raw = data[mip["offset"]:mip["offset"] + mip["bytes"]]
    require(all(math.isfinite(value[0]) for value in struct.iter_unpack("<f", raw)), "Nonfinite height map")
    return raw


def _rgb565(word):
    r, g, b = word >> 11, (word >> 5) & 63, word & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255)


def decode_dxt1(data, texture=None, level=0):
    texture = texture or parse_texture(data)
    require(texture["format_id"] == DXT1, "Color decoder requires DXT1")
    require(type(level) is int and 0 <= level < len(texture["mips"]), "Invalid mip level")
    mip = texture["mips"][level]
    width, height, offset = mip["width"], mip["height"], mip["offset"]
    output = bytearray(width * height * 4)
    for by in range(0, height, 4):
        for bx in range(0, width, 4):
            first, second, indices = struct.unpack_from("<HHI", data, offset)
            offset += 8
            a, b = _rgb565(first), _rgb565(second)
            palette = [a, b]
            if first > second:
                palette.extend([tuple((2 * a[c] + b[c]) // 3 for c in range(3)) + (255,),
                                tuple((a[c] + 2 * b[c]) // 3 for c in range(3)) + (255,)])
            else:
                palette.extend([tuple((a[c] + b[c]) // 2 for c in range(3)) + (255,), (0, 0, 0, 0)])
            for row in range(4):
                for col in range(4):
                    index = indices >> (2 * (row * 4 + col)) & 3
                    if bx + col < width and by + row < height:
                        start = ((by + row) * width + bx + col) * 4
                        output[start:start + 4] = bytes(palette[index])
    return bytes(output), width, height


def rgba_png(pixels, width, height):
    require(0 < width <= 8192 and 0 < height <= 8192 and len(pixels) == width * height * 4,
            "PNG pixels/dimensions differ")
    def chunk(kind, content):
        return struct.pack(">I", len(content)) + kind + content + struct.pack(">I", zlib.crc32(kind + content))
    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))
