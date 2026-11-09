#!/usr/bin/env python3
"""gen.py: regenerate native/game/ports/<file> for a group with the unified lifter (lift.py / lift_fl.py), emitting
lift_rt.h.  Two ways to drive it:

  gen.py --from-ports <file.cpp> [<file.cpp> ...]
      Re-lift every port already in those committed files, preserving each port's name, group string, declared outputs and
      the comment block above it (so names / OUTS / hand-tuned annotations are kept).  The file's #include of a per-group
      *_util.h is rewritten to lift_rt.h.  Hand-written (non-lifter) ports are detected (no LIFT/BIKE/.._ENTRY and no
      generator signature) and copied through verbatim.

  gen.py --list <list.tsv> --out <file.cpp> [--group g] [--backend captured|fl]
      Lift a fresh TSV of `addr<TAB>name<TAB>group<TAB>file` (the loading-path wave: native/game/plan/boot-alpine-functions.tsv
      enriched with group/file).  Outputs each addr as a port; refuses (prints, with the address) anything it cannot
      translate exactly, never guessing.

Backends: "captured" (lift.py) is the default (flags resolved statically to the defining instruction + fused branches, a
materialized EFLAGS word only where flags are read on several paths).  "fl" (lift_fl.py) keeps a single EFLAGS word `fl`
updated only where a consumer reads it (needed where flags come from sahf or complex multi-source paths)."""
import argparse, os, re, sys
import lift, abi
try:
    import lift_fl
except Exception:
    lift_fl = None

HERE = os.path.dirname(os.path.abspath(__file__))
PORTS = os.path.abspath(os.path.join(HERE, "..", "..", "ports"))

SIG = re.compile(r'REFLEX_PORT_FN\(0x([0-9a-fA-F]+),\s*([A-Za-z0-9_]+),\s*"([A-Za-z0-9_]+)",\s*(.+?)\)\s*\{')
ENTRY = re.compile(r'\b(LIFT|BIKE|ENG|FX|ANIM|UI|AI)_ENTRY\(\)')


def body_of(code):
    """the lines of a lifter-emitted port between its LIFT_ENTRY(); line and the final closing brace."""
    lines = code.rstrip("\n").split("\n")
    i = next(k for k, l in enumerate(lines) if "LIFT_ENTRY();" in l)
    assert lines[-1] == "}"
    return "\n".join(lines[i + 1:-1])


def lift_body(addr, name, group, backend):
    lift.GROUP = group
    if backend == "fl":
        if lift_fl is None:
            raise lift.LiftError("fl backend unavailable")
        return lift_fl.lift_body(addr, name, group)
    code = lift.lift(addr, name, "")
    return body_of(code)


def split_ports(text):
    """yield (pre_comment, sig_line, header_through_entry, body, addr, name, group, outs, is_lifted) per REFLEX_PORT_FN."""
    lines = text.split("\n")
    i = 0
    # file preamble: up to the first comment line that introduces the first port
    while i < len(lines):
        m = SIG.search(lines[i])
        if m:
            break
        i += 1
    preamble_end = i
    # walk back over the contiguous comment block that belongs to the first port
    j = preamble_end
    while j > 0 and (lines[j - 1].startswith("//") or lines[j - 1] == ""):
        j -= 1
    # the real preamble is everything before the first port's comment block
    yield ("PREAMBLE", "\n".join(lines[:j]))
    k = j
    while k < len(lines):
        m = SIG.search(lines[k]) if k < len(lines) else None
        if not m:
            k += 1
            continue
        # comment block above this sig
        c = k
        while c > j and (lines[c - 1].startswith("//") or lines[c - 1] == "") and not SIG.search(lines[c - 1]):
            c -= 1
        comment = "\n".join(lines[c:k])
        # find the matching closing brace (first line that is exactly "}")
        e = k + 1
        while e < len(lines) and lines[e] != "}":
            e += 1
        chunk = "\n".join(lines[k:e + 1])
        is_lifted = bool(ENTRY.search(chunk))
        yield ("PORT", comment, lines[k], chunk, int(m.group(1), 16), m.group(2), m.group(3), m.group(4).strip(), is_lifted)
        j = e + 1
        k = e + 1


def regen_file(path, backend, out_path=None, check_only=False):
    text = open(path).read()
    out = []
    fails = []
    reused = 0
    lifted = 0
    handwritten = 0
    for item in split_ports(text):
        if item[0] == "PREAMBLE":
            pre = item[1]
            pre = re.sub(r'#include "(?:bike|engine|fx|anim|ui|ai)_util\.h"', '#include "lift_rt.h"', pre)
            out.append(pre)
            continue
        _, comment, sig, chunk, addr, name, group, outs, is_lifted = item
        if not is_lifted:
            handwritten += 1
            out.append((comment + "\n" if comment.strip() else "") + chunk)
            continue
        try:
            body = lift_body(addr, name, group, backend)
        except Exception as e:
            fails.append("%08x %s: %s" % (addr, name, e))
            # keep the committed port on failure (never drop a port)
            out.append((comment + "\n" if comment.strip() else "") + chunk)
            continue
        lifted += 1
        outs = re.sub(r'\b(?:BIKE|ENG|FX|ANIM|UI|AI)_OUTS\b', 'LIFT_OUTS', outs)
        newsig = 'REFLEX_PORT_FN(0x%08x, %s, "%s", %s) {' % (addr, name, group, outs)
        port = (comment + "\n" if comment.strip() else "") + newsig + "\n    LIFT_ENTRY();\n" + body + "\n}"
        out.append(port)
    result = "\n".join(out).rstrip("\n") + "\n"
    if not check_only:
        open(out_path or path, "w").write(result)
    return lifted, handwritten, fails, result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from-ports", nargs="+")
    ap.add_argument("--backend", default="captured", choices=["captured", "fl"])
    ap.add_argument("--check", action="store_true", help="do not write; just report lift counts and failures")
    ap.add_argument("--list")
    ap.add_argument("--out")
    ap.add_argument("--group", default="")
    args = ap.parse_args()
    if args.from_ports:
        allfail = []
        for f in args.from_ports:
            p = f if os.path.isabs(f) else os.path.join(PORTS, f)
            lifted, hw, fails, _ = regen_file(p, args.backend, check_only=args.check)
            print("%s: %d lifted, %d hand-written kept, %d failed" % (os.path.basename(p), lifted, hw, len(fails)))
            allfail += fails
        if allfail:
            print("\nFAILURES (kept committed port, never dropped):")
            print("\n".join(allfail))
            sys.exit(1 if not args.check else 0)
        return
    if args.list:
        import csv
        rows = [l.rstrip("\n").split("\t") for l in open(args.list) if l.strip() and not l.startswith("#")]
        out = ['// Generated by native/game/tools/lift (gen.py --list); see tools/lift/README.md.', '#include <stdexcept>',
               '#include <utility>', '#include "lift_rt.h"', '']
        fails = []
        for c in rows:
            addr = int(c[0], 16); name = c[1]; group = c[2] if len(c) > 2 and c[2] else (args.group or "lifted")
            try:
                body = lift_body(addr, name, group, args.backend)
                regs, rets = abi.abi(addr)
                out.append("//   original %s; register inputs: %s; ret %s" % (lift.ENTS.get(addr, "?"), " ".join(regs) or "none", "/".join(str(r) for r in rets)))
                out.append('REFLEX_PORT_FN(0x%08x, %s, "%s", LIFT_OUTS) {\n    LIFT_ENTRY();\n%s\n}' % (addr, name, group, body))
            except Exception as e:
                fails.append("%08x %s: %s" % (addr, name, e))
        open(args.out, "w").write("\n".join(out) + "\n")
        print("wrote %s: %d ports, %d refused" % (args.out, len(rows) - len(fails), len(fails)))
        if fails:
            print("REFUSED (not translated exactly, left out):\n" + "\n".join(fails))
        return
    ap.error("give --from-ports or --list")


if __name__ == "__main__":
    main()
