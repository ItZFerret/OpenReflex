"""Read-only bounded scene-container/BXML extensions observed in Reflex assets.

The existing standalone BXML patcher's accepted corpus is not broadened. This
reader adds raw BXML, opaque byte values, typed arrays, vec4 and matrix4 solely
for scene/material inspection; it has no mutation or scene-writing API.
"""
import math
import struct
import zlib

from bxml import Document, HEADER, PACKING, require


class SceneDocument(Document):
    def patch(self, *args, **kwargs):
        raise ValueError("SceneDocument is a read-only inspection type")

    def to_bytes(self, *args, **kwargs):
        raise ValueError("SceneDocument does not serialize scene/material files")

    def __init__(self, raw):
        require(len(raw) >= 32, "Truncated scene BXML")
        magic, flags = struct.unpack_from("<4sI", raw)
        require(magic == b"BXML", "Invalid scene BXML magic")
        if flags == 0x3ea:
            header = list(struct.unpack_from("<4s7I", raw))
            _, _, ns, ss, bs, na, nn, reserved = header
            require(reserved == 0 and ss + bs + na * 12 + nn * 32 == len(raw) - 32,
                    "Raw BXML table sizes differ from resource size")
            require(len(raw) <= 32 * 1024 * 1024, "Raw BXML exceeds inspection limit")
            payload = zlib.compress(raw[32:])
            header[1] = 0x103ea
            raw = HEADER.pack(*header, len(payload)) + payload
        super().__init__(raw, max_decoded=32 * 1024 * 1024)

    def value(self, item):
        if item.kind in (0, 1):
            return super().value(item)
        if item.kind == 2:
            require(item.ref + item.count <= len(self.blob), "Opaque BXML bytes exceed blob")
            return {"opaque_bytes": self.blob[item.ref:item.ref + item.count].hex()}
        formats = {**PACKING, 7: "<4f", 8: "<16f"}
        require(item.kind in formats, f"Unsupported scene BXML type {item.kind}")
        fmt = formats[item.kind]
        size = struct.calcsize(fmt)
        require(item.count <= 65536 and item.ref + item.count * size <= len(self.blob),
                "Scene typed array exceeds blob or inspection limit")
        values = []
        for index in range(item.count):
            value = struct.unpack_from(fmt, self.blob, item.ref + index * size)
            require(all(not isinstance(v, float) or math.isfinite(v) for v in value), "Nonfinite scene value")
            if item.kind == 11:
                require(value[0] in (0, 1), "Invalid scene bool")
            values.append(list(value) if len(value) > 1 else value[0])
        return values[0] if item.count == 1 else values

    def attributes_at(self, index):
        return {self.strings[self.attributes[a].name]: self.value(self.attributes[a]) for a in self.attrs(index)}

    def as_record(self, index=0):
        item = self.nodes[index]
        return {"name": self.strings[item.name], "value": self.value(item),
                "attributes": self.attributes_at(index),
                "children": [self.as_record(child) for child in self.children(index)]}


def read_sections(data, selected):
    require(8 <= len(data) <= 128 * 1024 * 1024, "Scene container outside inspection bounds")
    magic, count = struct.unpack_from("<II", data)
    require(magic == 0xc0fffeee and 0 < count <= 128, "Invalid scene container header")
    table_end = 8 + count * 264
    require(table_end <= len(data), "Truncated scene section table")
    table = []
    for index in range(count):
        name, reserved, offset = struct.unpack_from("<256sII", data, 8 + index * 264)
        zero = name.find(b"\0")
        require(zero > 0 and name[zero:] == b"\0" * (256 - zero), "Invalid scene section name padding")
        name = name[:zero].decode("utf-8")
        require(reserved == 0 and table_end <= offset < len(data), "Invalid scene section pointer")
        table.append((name, offset))
    require(len({name for name, _ in table}) == count, "Duplicate scene section name")
    offsets = [offset for _, offset in table]
    require(offsets == sorted(set(offsets)), "Scene sections are duplicated or out of order")
    require(set(selected) <= {name for name, _ in table}, "Requested scene section is absent")
    result = {}
    for index, (name, start) in enumerate(table):
        if name not in selected:
            continue
        end = table[index + 1][1] if index + 1 < count else len(data)
        require(start + 8 <= end, "Truncated scene section header")
        chunks, entities = struct.unpack_from("<II", data, start)
        require(0 < chunks <= 4096 and entities <= 100000, "Scene section counts exceed bounds")
        cursor, records, actual_entities = start + 8, [], 0
        for _ in range(chunks):
            require(cursor + 36 <= end, "Truncated embedded BXML header")
            header = HEADER.unpack_from(data, cursor)
            require(header[0] == b"BXML" and header[1] == 0x103ea, "Unsupported embedded scene BXML")
            size = 36 + header[-1]
            require(cursor + size <= end, "Embedded BXML crosses section boundary")
            document = SceneDocument(data[cursor:cursor + size])
            for node, item in enumerate(document.nodes):
                if document.strings[item.name] == "Entity":
                    records.append(document.as_record(node))
                    actual_entities += 1
            cursor += size
        require(cursor == end and actual_entities == entities, "Scene section extent/entity count mismatch")
        result[name] = {"offset": start, "bytes": end - start, "chunks": chunks,
                        "entity_count": entities, "entities": records}
    return result


def entity_fields(record):
    require(record["name"] == "Entity" and len(record["children"]) == 1, "Unsupported entity wrapper")
    root = record["children"][0]
    require(root["name"] == "RSData", "Missing entity RSData")
    fields = {}
    for child in root["children"]:
        name = child["attributes"].get("name")
        require(name and name not in fields, "Unnamed or duplicate entity field")
        fields[name] = child["children"]
    return {"entity_id": record["attributes"]["entityId"],
            "namespace": root["attributes"]["namespace"], "fields": fields}
