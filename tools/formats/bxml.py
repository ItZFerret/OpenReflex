#!/usr/bin/env python3
"""Strict decoder and value patcher for the BXML files shipped with MX Reflex.

Only the observed 0x000103EA variant is accepted. The CLI writes exclusively to
its sibling out/ directory; it never updates an input or a game installation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import tempfile
import os
import xml.etree.ElementTree as ET
import zlib
from dataclasses import dataclass, replace
from collections import Counter


HERE = Path(__file__).resolve().parent
OUT = HERE / "out"
DEFAULT_ARCHIVE = HERE.parents[1] / "game files" / "Archive"
HEADER = struct.Struct("<4s8I")
ATTRIBUTE = struct.Struct("<IIHH")
NODE = struct.Struct("<8I")
VERSION_FLAGS = 0x000103EA
NONE = 0xFFFFFFFF
MAX_DECODED = 256 * 1024 * 1024
TYPES = {0: "empty", 1: "string", 3: "int", 4: "uint", 5: "float", 10: "vector3", 11: "bool"}
PACKING = {3: "<i", 4: "<I", 5: "<f", 10: "<3f", 11: "<I"}
XML_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_.-]*$")


class BxmlError(ValueError):
    """Malformed input or an unverified format feature."""


def require(condition, message):
    if not condition:
        raise BxmlError(message)


@dataclass(frozen=True)
class Attribute:
    name: int
    ref: int
    count: int
    kind: int


@dataclass(frozen=True)
class Node:
    name: int
    ref: int
    count: int
    kind: int
    child_start: int
    child_count: int
    attribute_start: int
    attribute_count: int


def parse_value(kind: int, text: str):
    """Parse a replacement using the existing field's type, without coercion."""
    if kind == 1:
        require("\0" not in text, "Strings cannot contain NUL")
        return text
    text = text.strip()
    if kind in (3, 4):
        require(bool(re.fullmatch(r"[-+]?[0-9]+", text)), "Expected a decimal integer")
        value = int(text, 10)
        low, high = (-2**31, 2**31 - 1) if kind == 3 else (0, 2**32 - 1)
        require(low <= value <= high, f"Value outside {TYPES[kind]}32 range")
        return value
    if kind == 11:
        values = {"true": True, "t": True, "1": True, "false": False, "f": False, "0": False}
        require(text.lower() in values, "Expected true/false, T/F, or 1/0")
        return values[text.lower()]
    if kind in (5, 10):
        try:
            values = [float(x.strip()) for x in text.split(",")]
        except ValueError as exc:
            raise BxmlError("Expected finite floating-point value(s)") from exc
        require(len(values) == (3 if kind == 10 else 1), "Wrong number of float components")
        require(all(math.isfinite(x) for x in values), "Non-finite replacement value")
        try:
            values = struct.unpack(PACKING[kind], struct.pack(PACKING[kind], *values))
        except (OverflowError, struct.error) as exc:
            raise BxmlError("Replacement exceeds float32 range") from exc
        require(all(math.isfinite(x) for x in values), "Replacement exceeds float32 range")
        return list(values) if kind == 10 else values[0]
    raise BxmlError("Cannot patch an empty or unknown typed value")


def format_value(kind: int, value) -> str:
    if kind == 0:
        return ""
    if kind == 11:
        return "true" if value else "false"
    if kind == 5:
        return format(value, ".9g")
    if kind == 10:
        return ",".join(format(v, ".9g") for v in value)
    return str(value)


class Document:
    def __init__(self, raw: bytes, *, max_decoded=MAX_DECODED):
        self.original = raw
        self.changed = False
        require(len(raw) >= HEADER.size, "Truncated BXML header")
        magic, version, ns, ss, bs, na, nn, reserved, compressed = HEADER.unpack_from(raw)
        require(magic == b"BXML", "Invalid BXML magic")
        require(version == VERSION_FLAGS, f"Unsupported version/flags: 0x{version:08x}")
        require(reserved == 0, "Unverified nonzero header field at byte 28")
        require(compressed == len(raw) - HEADER.size, "Compressed length mismatch")
        size = ss + bs + na * ATTRIBUTE.size + nn * NODE.size
        require(0 < size <= max_decoded, "Decoded size exceeds configured limit or is zero")
        inflater = zlib.decompressobj()
        try:
            payload = inflater.decompress(raw[HEADER.size:], size + 1)
        except zlib.error as exc:
            raise BxmlError(f"Invalid zlib stream: {exc}") from exc
        require(len(payload) == size, "Decoded table lengths do not match header")
        require(inflater.eof and not inflater.unused_data and not inflater.unconsumed_tail,
                "Incomplete zlib stream or unexpected trailing stream/data")
        pool = payload[:ss]
        require(pool.endswith(b"\0"), "String pool lacks terminating NUL")
        self.string_bytes = pool.split(b"\0")[:-1]
        require(len(self.string_bytes) == ns, "String count mismatch")
        try:
            self.strings = [s.decode("utf-8") for s in self.string_bytes]
        except UnicodeDecodeError as exc:
            raise BxmlError("Unverified non-UTF-8 string pool") from exc
        self.blob = payload[ss:ss + bs]
        attr_start = ss + bs
        node_start = attr_start + na * ATTRIBUTE.size
        self.attributes = [Attribute(*ATTRIBUTE.unpack_from(payload, attr_start + i * ATTRIBUTE.size)) for i in range(na)]
        self.nodes = [Node(*NODE.unpack_from(payload, node_start + i * NODE.size)) for i in range(nn)]
        self.validate()

    @classmethod
    def read(cls, path):
        return cls(Path(path).read_bytes())

    def value(self, item: Attribute | Node):
        require(item.kind in TYPES, f"Unknown value type {item.kind}")
        if item.kind == 0:
            require(isinstance(item, Node) and item.ref == NONE and item.count == 0, "Invalid empty value")
            return None
        if item.kind == 1:
            require(item.count == 0 and item.ref < len(self.strings), "Invalid string value reference/count")
            return self.strings[item.ref]
        require(item.count == 1, "Unverified typed array count (only scalar/vector values observed)")
        fmt = PACKING[item.kind]
        require(item.ref + struct.calcsize(fmt) <= len(self.blob), "Typed value exceeds blob")
        values = struct.unpack_from(fmt, self.blob, item.ref)
        if item.kind == 11:
            require(values[0] in (0, 1), "Invalid bool32 value")
            return bool(values[0])
        require(all(not isinstance(x, float) or math.isfinite(x) for x in values), "Non-finite typed value")
        return list(values) if item.kind == 10 else values[0]

    def validate(self):
        require(bool(self.nodes), "Missing root node")
        owners = [0] * len(self.nodes)
        attr_owners = [0] * len(self.attributes)
        for i, node in enumerate(self.nodes):
            require(node.name < len(self.strings), f"Invalid node name at {i}")
            self.value(node)
            require(node.child_start <= len(self.nodes) and node.child_start + node.child_count <= len(self.nodes), f"Invalid child range at node {i}")
            require(node.attribute_start <= len(self.attributes) and node.attribute_start + node.attribute_count <= len(self.attributes), f"Invalid attribute range at node {i}")
            for child in range(node.child_start, node.child_start + node.child_count):
                require(child > i, "Child reference is backward/cyclic")
                owners[child] += 1
            names = set()
            for a in range(node.attribute_start, node.attribute_start + node.attribute_count):
                attr_owners[a] += 1
                item = self.attributes[a]
                require(item.name < len(self.strings), f"Invalid attribute name at {a}")
                require(self.strings[item.name] not in names, f"Duplicate attribute at node {i}")
                names.add(self.strings[item.name])
                self.value(item)
        require(owners[0] == 0 and all(n == 1 for n in owners[1:]), "Tree contains orphan/shared nodes")
        require(all(n == 1 for n in attr_owners), "Tree contains orphan/shared attributes")

    def children(self, index):
        n = self.nodes[index]
        return range(n.child_start, n.child_start + n.child_count)

    def attrs(self, index):
        n = self.nodes[index]
        return range(n.attribute_start, n.attribute_start + n.attribute_count)

    def resolve(self, path: str):
        """Resolve /Root/Child or /Root/Child[2]/@attribute; reject ambiguity."""
        require(path.startswith("/") and not path.endswith("/"), "Use an absolute node path")
        parts = path[1:].split("/")
        require(parts[0] == self.strings[self.nodes[0].name], "Path root does not match document")
        index = 0
        for p, part in enumerate(parts[1:], 1):
            if part.startswith("@"):
                require(p == len(parts) - 1, "Attribute must be the last path component")
                matches = [a for a in self.attrs(index) if self.strings[self.attributes[a].name] == part[1:]]
                require(len(matches) == 1, "Attribute does not exist or is ambiguous")
                return "attribute", matches[0]
            match = re.fullmatch(r"([^\[\]/]+)(?:\[([1-9][0-9]*)\])?", part)
            require(match is not None, "Invalid path component")
            tag, occurrence = match.groups()
            matches = [c for c in self.children(index) if self.strings[self.nodes[c].name] == tag]
            if occurrence:
                require(int(occurrence) <= len(matches), "Indexed node does not exist")
                index = matches[int(occurrence) - 1]
            else:
                require(len(matches) == 1, f"Node {tag!r} is missing/ambiguous; use a 1-based [index]")
                index = matches[0]
        return "node", index

    def patch(self, path: str, text: str):
        group, index = self.resolve(path)
        table = self.nodes if group == "node" else self.attributes
        old = table[index]
        value = parse_value(old.kind, text)
        previous = self.value(old)
        if previous == value:
            return {"path": path, "type": TYPES[old.kind], "before": previous, "after": value, "changed": False}
        if old.kind == 1:
            try:
                ref = self.strings.index(value)
            except ValueError:
                ref = len(self.strings)
                self.strings.append(value)
                self.string_bytes.append(value.encode("utf-8"))
        else:
            # Append, never overwrite: a blob value could be shared by another field.
            ref = len(self.blob)
            values = value if old.kind == 10 else [value]
            self.blob += struct.pack(PACKING[old.kind], *values)
        table[index] = replace(old, ref=ref)
        self.changed = True
        return {"path": path, "type": TYPES[old.kind], "before": previous, "after": value, "changed": True}

    def payload(self):
        pool = b"\0".join(self.string_bytes) + b"\0"
        attrs = b"".join(ATTRIBUTE.pack(a.name, a.ref, a.count, a.kind) for a in self.attributes)
        nodes = b"".join(NODE.pack(n.name, n.ref, n.count, n.kind, n.child_start, n.child_count, n.attribute_start, n.attribute_count) for n in self.nodes)
        return pool, pool + self.blob + attrs + nodes

    def to_bytes(self, *, recompress=False):
        self.validate()
        if not self.changed and not recompress:
            return self.original
        pool, payload = self.payload()
        compressed = zlib.compress(payload, 6)
        return HEADER.pack(b"BXML", VERSION_FLAGS, len(self.strings), len(pool), len(self.blob), len(self.attributes), len(self.nodes), 0, len(compressed)) + compressed

    def semantic_records(self):
        """Flat semantic view for whole-document equivalence checks."""
        return [(self.strings[n.name], n.kind, self.value(n), tuple(self.children(i)),
                 tuple((self.strings[self.attributes[a].name], self.attributes[a].kind, self.value(self.attributes[a])) for a in self.attrs(i)))
                for i, n in enumerate(self.nodes)]

    def to_dict(self):
        rows = []
        for i, n in enumerate(self.nodes):
            rows.append({"index": i, "tag": self.strings[n.name], "type": TYPES[n.kind], "value": self.value(n),
                         "attributes": [{"name": self.strings[self.attributes[a].name], "type": TYPES[self.attributes[a].kind], "value": self.value(self.attributes[a])} for a in self.attrs(i)],
                         "children": list(self.children(i))})
        return {"format": "BXML", "version_flags": f"0x{VERSION_FLAGS:08x}", "original_sha256": hashlib.sha256(self.original).hexdigest(),
                "string_count": len(self.strings), "blob_bytes": len(self.blob), "root_index": 0, "nodes": rows}

    def to_element(self):
        elements = []
        for i, n in enumerate(self.nodes):
            tag = self.strings[n.name]
            require(bool(XML_NAME.fullmatch(tag)), f"Unsupported XML element name {tag!r}")
            e = ET.Element(tag)
            for a in self.attrs(i):
                item = self.attributes[a]
                name = self.strings[item.name]
                require(bool(XML_NAME.fullmatch(name)), f"Unsupported XML attribute name {name!r}")
                text = format_value(item.kind, self.value(item))
                if item.kind != 1:
                    text = "_" + TYPES[item.kind] + ":" + text
                e.set(name, text)
            if n.kind not in (0, 1):
                require("_valuetype" not in e.attrib, "Conflicting _valuetype attribute")
                e.set("_valuetype", TYPES[n.kind])
            if n.kind:
                e.text = format_value(n.kind, self.value(n))
            elements.append(e)
        for i, e in enumerate(elements):
            e.extend(elements[c] for c in self.children(i))
        return elements[0]

    def to_xml(self):
        root = self.to_element()
        ET.indent(root, space="  ")
        return ET.tostring(root, encoding="utf-8", xml_declaration=True) + b"\n"


def compare_xml(doc: Document, xml_path: Path):
    """Compare original XML semantics; report differences instead of hiding them."""
    differences = []
    root = ET.parse(xml_path).getroot()
    stack = [(0, root, "/" + root.tag)]
    while stack:
        i, e, path = stack.pop()
        n = doc.nodes[i]
        if e.tag != doc.strings[n.name]:
            differences.append({"path": path, "problem": "tag mismatch"})
        expected_type = e.get("_valuetype")
        text = (e.text or "").strip()
        expected_kind = next((k for k, v in TYPES.items() if v == expected_type), None) if expected_type else (1 if text else 0)
        try:
            expected_value = parse_value(expected_kind, text) if expected_kind else None
        except BxmlError as exc:
            differences.append({"path": path, "problem": "source XML value cannot be parsed strictly", "value": text, "detail": str(exc)})
            expected_value = doc.value(n)
        if (expected_kind, expected_value) != (n.kind, doc.value(n)):
            differences.append({"path": path, "problem": "value mismatch", "xml": [expected_kind, expected_value], "bxml": [n.kind, doc.value(n)]})
        actual_attrs = {doc.strings[doc.attributes[a].name]: (doc.attributes[a].kind, doc.value(doc.attributes[a])) for a in doc.attrs(i)}
        expected_attrs = {}
        for key, text in e.attrib.items():
            if key == "_valuetype":
                continue
            match = re.match(r"^_(int|uint|float|vector3|bool):(.*)$", text, re.S)
            kind = next(k for k, v in TYPES.items() if v == match[1]) if match else 1
            try:
                expected_attrs[key] = (kind, parse_value(kind, match[2] if match else text))
            except BxmlError as exc:
                differences.append({"path": path + "/@" + key, "problem": "source XML attribute cannot be parsed strictly", "value": text, "detail": str(exc)})
                # Keep evaluating the rest of the pair. A malformed source XML
                # literal is evidence about that source, not a binary parse error.
                expected_attrs[key] = actual_attrs.get(key, (kind, None))
        if expected_attrs != actual_attrs:
            differences.append({"path": path, "problem": "attribute mismatch", "xml": expected_attrs, "bxml": actual_attrs})
        children = list(doc.children(i))
        if len(children) != len(e):
            differences.append({"path": path, "problem": "child count mismatch", "xml": len(e), "bxml": len(children)})
        for child, xml_child in zip(children, e):
            stack.append((child, xml_child, path + "/" + xml_child.tag))
    return differences


def write_output(path, data: bytes, *, source=None):
    destination = Path(path).expanduser().resolve()
    require(destination.is_relative_to(OUT.resolve()) and destination != OUT.resolve(), f"Outputs must be within {OUT}")
    require(source is None or destination != Path(source).resolve(), "Refusing to overwrite input")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as f:
        temporary = Path(f.name)
        try:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        os.replace(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    return destination


def inventory(root: Path):
    records, failures, types = [], [], Counter()
    for path in sorted(root.rglob("*")):
        if path.suffix.lower() not in (".bxml", ".database", ".level"):
            continue
        try:
            raw = path.read_bytes()
            doc = Document(raw)
            rebuilt = Document(doc.to_bytes(recompress=True))
            require(doc.payload()[1] == rebuilt.payload()[1], "Recompression changed decoded bytes")
            require(doc.semantic_records() == rebuilt.semantic_records(), "Recompression changed semantics")
            require(doc.to_bytes() == raw, "No-op serialization changed input")
            exported = ET.fromstring(doc.to_xml())
            require(sum(1 for _ in exported.iter()) == len(doc.nodes), "XML export lost nodes")
            for item in doc.nodes + doc.attributes:
                types[TYPES[item.kind]] += 1
            record = {"path": str(path.relative_to(root)), "sha256": hashlib.sha256(raw).hexdigest(), "file_bytes": len(raw),
                      "strings": len(doc.strings), "blob_bytes": len(doc.blob), "nodes": len(doc.nodes), "attributes": len(doc.attributes),
                      "xml_export": "well-formed; element count matches",
                      "roundtrip": "exact decoded bytes and semantics; original compressed bytes preserved on no-op"}
            pair = path.with_suffix(".xml")
            if path.suffix.lower() == ".bxml" and pair.exists():
                differences = compare_xml(doc, pair)
                record["xml_pair"] = {"path": str(pair.relative_to(root)), "differences": differences}
            records.append(record)
        except (BxmlError, OSError, ET.ParseError) as exc:
            failures.append({"path": str(path.relative_to(root)), "error": str(exc)})
    return {"archive": str(root.resolve()), "validated_files": len(records), "failures": failures, "type_occurrences": dict(types),
            "xml_pairs": sum("xml_pair" in r for r in records),
            "xml_pairs_exact": sum("xml_pair" in r and not r["xml_pair"]["differences"] for r in records), "files": records}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    inspect = sub.add_parser("export", help="Export one file to readable XML or typed JSON")
    inspect.add_argument("source", type=Path)
    inspect.add_argument("--format", choices=("xml", "json"), default="xml")
    inspect.add_argument("--output", type=Path, required=True)
    inv = sub.add_parser("inventory", help="Validate, roundtrip, and compare the complete metadata corpus")
    inv.add_argument("root", type=Path, nargs="?", default=DEFAULT_ARCHIVE)
    inv.add_argument("--output", type=Path, default=OUT / "inventory.json")
    patch = sub.add_parser("patch", help="Patch existing typed values into a new output copy")
    patch.add_argument("source", type=Path)
    patch.add_argument("--set", dest="changes", action="append", nargs=2, metavar=("PATH", "VALUE"), required=True)
    patch.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "inventory":
            require(args.root.is_dir(), "Inventory directory does not exist")
            report = inventory(args.root)
            require(report["validated_files"] + len(report["failures"]) > 0, "No BXML files found")
            output = write_output(args.output, (json.dumps(report, indent=2) + "\n").encode())
            print(json.dumps({k: v for k, v in report.items() if k != "files"}, indent=2))
            print(f"Report: {output}")
            return bool(report["failures"])
        doc = Document.read(args.source)
        if args.command == "export":
            data = doc.to_xml() if args.format == "xml" else (json.dumps(doc.to_dict(), indent=2, allow_nan=False) + "\n").encode()
        else:
            changes = [doc.patch(path, value) for path, value in args.changes]
            data = doc.to_bytes()
            verification = Document(data)
            require(doc.semantic_records() == verification.semantic_records(), "Patch verification failed")
            print(json.dumps({"source": str(args.source.resolve()), "source_sha256": hashlib.sha256(doc.original).hexdigest(),
                              "output_sha256": hashlib.sha256(data).hexdigest(), "changes": changes}, indent=2))
        output = write_output(args.output, data, source=args.source)
        print(f"Wrote: {output}")
        return 0
    except (BxmlError, OSError, ET.ParseError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
