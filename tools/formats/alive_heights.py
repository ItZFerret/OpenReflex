"""Terrain height range of a converted / installed Alive level (heightfield R32F x TerrainStrength), for test windows."""
import json
import struct
from pathlib import Path

import numpy as np

from scene_read import read_sections, entity_fields


def terrain_entity(scene_bytes, level):
    section = level + "Base"
    for e in read_sections(scene_bytes, [section])[section]["entities"]:
        f = entity_fields(e)
        if f["namespace"].endswith("_HFTerrainRSData"):
            return f["fields"]
    raise ValueError(f"{level}: no HFTerrain entity")


def heights_from_texture(data):
    """Converted heightfield (texture v4, D3DFMT_R32F 114) -> float array of the base mip."""
    h = struct.unpack_from("<14I", data)
    if h[13] != 114:
        raise ValueError("Heightfield is not R32F")
    levels, width, height = h[10:13]
    offset = 56 + h[4] * 8
    return np.frombuffer(data, dtype="<f4", count=width * height, offset=offset).reshape(height, width)


def terrain_height_range(root, level):
    """(min, max) terrain height in feet. `root` is a conversion folder or an overlay install."""
    root = Path(root)
    fields = terrain_entity((root / "Database" / f"{level}.dx9.scene").read_bytes(), level)
    name = fields["HeightMapAsset"][0]["attributes"]["name"]
    strength = fields["TerrainStrength"][0]["value"] if "TerrainStrength" in fields else 2048.0
    matches = sorted((root / "Assets/texture" / level).rglob(name + ".texture"))
    if not matches:
        raise ValueError(f"{level}: heightfield {name} not converted")
    values = heights_from_texture(matches[0].read_bytes()) * strength
    return float(values.min()), float(values.max())


def zone_maps(data):
    """The four terrain zone maps of a converted (PC schema, not interleaved) zones packdata, as (h, w) int arrays."""
    import zlib
    from bxml import HEADER, NODE
    magic, flags, ns, ss, bs, na, nn, res, comp = HEADER.unpack_from(data)
    pay = zlib.decompress(data[36:]); strings = pay[:ss].split(b"\0"); blob = pay[ss:ss + bs]
    vals = {}
    for i in range(1, nn):
        name, ref, count, kind, *_ = NODE.unpack_from(pay, ss + bs + na * 12 + i * 32)
        vals[strings[name].decode()] = np.frombuffer(blob, dtype="<u4", count=count, offset=ref)
    out = []
    for k in range(1, 5):
        w, h, b = int(vals[f"Width{k}"][0]), int(vals[f"Height{k}"][0]), int(vals[f"NumBits{k}"][0])
        words = np.append(vals[f"Data{k}"].astype(np.uint64), np.uint64(0))
        pos = np.arange(w * h, dtype=np.uint64) * np.uint64(b)
        s = ((words[pos // np.uint64(32)] >> (pos % np.uint64(32))) |
             (words[pos // np.uint64(32) + np.uint64(1)] << (np.uint64(32) - pos % np.uint64(32)))) & np.uint64((1 << b) - 1)
        out.append(s.reshape(h, w).astype(int))
    return out


def zone_value_at(root, level, x, z, layer=0):
    """The terrain material (zone value) the core reads at world (x, z): phys_terrain_material_at 007c5120 with
    u = (size / 2 + x) / size, v = (size / 2 - z) / size and the packed grid's index floor(w u) * stride + w v (u picks the row);
    size = heightfield samples x the GridResolution spacing (rx_hfterrain_entity_load). Layer 0 value 7 is the reset material."""
    root = Path(root)
    fields = terrain_entity((root / "Database" / f"{level}.dx9.scene").read_bytes(), level)
    name = fields["HeightMapAsset"][0]["attributes"]["name"]
    hm = sorted((root / "Assets/texture" / level).rglob(name + ".texture"))[0].read_bytes()
    width, height = struct.unpack_from("<14I", hm)[11:13]
    spacing = 1 << int(fields["GridResolution"][1]["value"]) if "GridResolution" in fields else 1
    zname = fields["FrictionZones"][0]["attributes"]["name"]
    maps = zone_maps(sorted((root / "Assets/packdata" / level).rglob(zname + ".packdata"))[0].read_bytes())
    m = maps[layer]
    sx, sz = width * spacing, height * spacing
    u = min(max((sx / 2 + x) / sx, 0.0), 1.0); v = min(max((sz / 2 - z) / sz, 0.0), 1.0)
    row = min(int(m.shape[0] * u), m.shape[0] - 1); col = min(int(m.shape[1] * v), m.shape[1] - 1)
    return int(m[row, col])
