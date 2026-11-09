#!/usr/bin/env python3
"""Build isolated native decoder; --fetch explicitly downloads pinned LGPL source."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

from bxml import HERE, OUT, require, write_output


COMMIT = "55d501976171397ccd5d5a7a1ca7da065b1d9a06"
HASHES = {
    "lzxd.c": "bc4a14a5938d99cb1e2521a0fcc16fa44e8ea262ab0bc072e3f9003e19c816da",
    "lzx.h": "22b1ffa9ddc15aa5fac95d4f4f0c60486f590e55636a17ab94f3ad1b770f907f",
    "readbits.h": "7c849edb9b939b4eb1e9e2b895b7fee3eb19e23d0a1dab87c6c504cf007010ab",
    "readhuff.h": "446c7ca1cfb11a80cd1b4e86006abea3f6052059636361ddb1e227f29ccee560",
    "system.h": "98997127296c7baccd61b5ade9362cab509c9664b00259f9f62e6008b7369e42",
    "mspack.h": "df9e9e2b18cc145e6cf652a43ffbc88bde21bfd6fd3024318fbc2140a4efd059",
    "macros.h": "29a9628cd7a8881bd36defb811209c2a48852e0bf6899d579d6cb80d65337b0d",
    "COPYING.LIB": "a190dc9c8043755d90f8b0a75fa66b9e42d4af4c980bf5ddc633f0124db3cee7",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fetch", action="store_true", help="download missing pinned sources into out only")
    args = parser.parse_args()
    directory = OUT / "libmspack"
    directory.mkdir(parents=True, exist_ok=True)
    for filename, expected in HASHES.items():
        path = directory / filename
        if not path.exists():
            require(args.fetch, f"Missing {filename}; rerun with --fetch")
            upstream = "libmspack/" + ("" if filename == "COPYING.LIB" else "mspack/") + filename
            url = f"https://raw.githubusercontent.com/kyz/libmspack/{COMMIT}/{upstream}"
            with urllib.request.urlopen(url, timeout=30) as response:
                data = response.read(2 * 1024 * 1024)
            require(hashlib.sha256(data).hexdigest() == expected, f"Source hash mismatch: {filename}")
            write_output(path, data)
        require(hashlib.sha256(path.read_bytes()).hexdigest() == expected, f"Cached source hash mismatch: {filename}")
    source = {"repo": "https://github.com/kyz/libmspack", "commit": COMMIT,
              "license": "LGPL-2.1", "sha256": HASHES}
    write_output(directory / "SOURCE.json", (json.dumps(source, indent=2) + "\n").encode())
    destination = directory / "libreflex_lzx.dylib"
    require(destination.resolve().is_relative_to(OUT.resolve()), "Build output escapes out")
    subprocess.run(["clang", "-shared", "-fPIC", "-O2", "-I" + str(directory),
                    str(HERE / "lzx_bridge.c"), str(directory / "lzxd.c"),
                    "-o", str(destination)], check=True, timeout=60)
    print(destination)


if __name__ == "__main__":
    main()
