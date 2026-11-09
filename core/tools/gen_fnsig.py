#!/usr/bin/env python3
"""gen_fnsig.py <out.c> [--report] [--strict] -- the registered core functions whose C signature is plain integers (rx_addr / uint32_t /
int32_t / uint16_t / uint8_t / int / unsigned / bool parameters, a void / integer / rx_addr result): the product's rx_call_original
(src/rx_mem.c) may call those by address (a virtual's target, a callee another unit ported) instead of stopping at an "unported game
function". A function with a float / double / struct / pointer parameter or result, and a registration whose definition the scan does not
find, are not in the table (the product still stops there). The table is rebuilt by every product build (core/build.sh,
tools/rx_api_test.sh, tools/build_event_test.sh), so it follows the sources.

Every game C file is PREPROCESSED with -DRX_FNSIG_SCAN before the scan (rx_fn.h turns RX_FUNCTION into a plain RX_FNSIG_REG(address, fn)
marker in that mode). That is what makes registrations produced by a macro family visible: build_event_proxy_equals.c PROXY_EQ*,
build_scene_start.c START_CTOR, race_ai_params.c AI_PARAM, build_vcall_thunks.c and any future family. A literal-only scan silently
dropped them from the table (first hit: the 00592b60 vcall thunk, builds/stuntaudio). If the preprocessor is unavailable or a file does
not preprocess (mid-edit), that file falls back to the literal `RX_FUNCTION(0x...)` scan and the tool names the file; --strict turns
that into an error, --report prints the counts and every excluded registration with its reason.

The helpers and INT below are also exec'd by builds/dispatchfix/scan_dispatch.py (it splits at the first `addrs` assignment); keep that boundary.
"""
import re, sys, glob, os, shutil, subprocess

here = os.path.dirname(os.path.abspath(__file__))
game = os.path.join(here, "..", "game")
INT = {"rx_addr", "uint32_t", "int32_t", "uint16_t", "int16_t", "uint8_t", "int8_t", "int", "unsigned", "unsigned int", "bool", "_Bool", "size_t",
       "unsigned char", "unsigned short", "char", "short"}

def strip(s):
    s = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), s, flags=re.S)
    s = re.sub(r"//[^\n]*", "", s)
    s = re.sub(r'"(?:\\.|[^"\\])*"', '""', s)
    return s

def norm(t):
    t = re.sub(r"\b(const|volatile|register)\b", "", t)
    return " ".join(t.split())

addrs = set()

# ---- the registrations ------------------------------------------------------------------------------------------------------------
REG_RE = re.compile(r"RX_FNSIG_REG\(\s*(0x[0-9a-fA-F]+)\s*,\s*(\w+)\s*\)")   # rx_fn.h's RX_FNSIG_SCAN form, after macro expansion
LIT_RE = re.compile(r"^[ \t]*RX_FUNCTION\(\s*(0x[0-9a-fA-F]+)\s*,\s*(\w+)\s*\)", re.M)   # the literal form (fallback / no preprocessor)

# every definition in a (preprocessed) file in one pass: the per-name search made the scan quadratic in the headers
DEF_RE = re.compile(r"(?m)^[ \t]*((?:(?:static|inline|extern|__attribute__\(\([^)]*\)\))\s+)*)([A-Za-z_][\w \t\*]*?)[ \t\n\*]([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{")

def definitions(src):
    """name -> (return type, parameters, declaration head) for every definition in `src`."""
    out = {}
    for d in DEF_RE.finditer(src):
        name = d.group(3)
        if name not in out:
            out[name] = (norm(d.group(2)), d.group(4).strip(), d.group(0).split(name)[0])
    return out

def int_only(ret, params, head):
    """(ok, reason): the current integer-signature rule."""
    if "*" in head and "rx_addr" not in ret:
        return False, "pointer result"
    if ret != "void" and ret not in INT:
        return False, "result " + ret
    if params and params != "void":
        for p in params.split(","):
            p = norm(p)
            if "*" in p or "[" in p or "." in p:
                return False, "parameter " + p
            toks = p.split()
            if len(toks) >= 2 and " ".join(toks[:-1]) in INT:
                continue
            if p in INT:
                continue
            return False, "parameter " + p
    return True, ""

def compiler():
    for c in (os.environ.get("RX_FNSIG_CC"), "clang", "cc"):
        if c and shutil.which(c):
            return c
    return None

CORE = os.path.abspath(os.path.join(here, ".."))
INCS = [os.path.join(CORE, d) for d in ("include", "src", "game", "api")]

# Match the per-TU native compilation view from the same canonical map used
# by both product builders. RX_FNSIG_SCAN remains an additional scan-only marker.
TU_FLAGS = {}
_tu_map = os.path.join(CORE, "tools", "native_tu_flags.tsv")
if os.path.isfile(_tu_map):
    for _line in open(_tu_map):
        if not _line.strip():
            continue
        _source, _define = _line.rstrip("\n").split("\t")
        if not re.fullmatch(r"game/[A-Za-z0-9_]+\.c", _source) or not re.fullmatch(r"-D[A-Za-z_][A-Za-z0-9_]*", _define):
            raise ValueError("Invalid native per-TU flag row: " + _line)
        if _source in TU_FLAGS:
            raise ValueError("Duplicate native per-TU flag row: " + _source)
        TU_FLAGS[_source] = [_define]

def preprocess(cc, path):
    """the file with every macro expanded (rx_fn.h's RX_FNSIG_SCAN marker), or None if the preprocessor refuses it."""
    try:
        r = subprocess.run([cc, "-E", "-P", "-std=c11", "-DRX_FNSIG_SCAN"] + TU_FLAGS.get(os.path.relpath(path, CORE), []) + ["-I" + d for d in INCS] + [path],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return None
    return r.stdout.decode("utf-8", "replace") if r.returncode == 0 else None

def scan(path, cc):
    """(registrations, literal count, preprocessed?, definitions): registrations as (addr, name, resolved-or-None) from one file."""
    raw = strip(open(path, encoding="utf-8", errors="replace").read())
    literals = [(int(m.group(1), 16), m.group(2)) for m in LIT_RE.finditer(raw)]
    src = preprocess(cc, path) if cc else None
    if src is not None:
        src = strip(src)
        found = [(int(m.group(1), 16), m.group(2)) for m in REG_RE.finditer(src)]
        preprocessed = True
    else:
        found, src, preprocessed = literals, raw, False
    defs = definitions(src)
    out = []
    for addr, name in found:
        sig = defs.get(name)
        if sig is None:
            out.append((addr, name, None))     # resolved from another file (an aliased registration), or nowhere
            continue
        ok, why = int_only(*sig)
        out.append((addr, name, (ok, why)))
    return out, len(literals), preprocessed, defs

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if not args:
        sys.exit("usage: gen_fnsig.py <out.c> [--report] [--strict]")
    out = args[0]
    cc = compiler()
    files = sorted(glob.glob(os.path.join(game, "*.c")))
    regs = {}            # addr -> (name, ok, reason)
    fallback = []        # files scanned literally (preprocessor unavailable / refused)
    literal_total = 0
    from concurrent.futures import ThreadPoolExecutor
    # Signature preprocessing is part of the build too. Respect the same worker
    # cap as compilation instead of spawning up to sixteen extra clang jobs.
    try:
        workers = int(os.environ.get("RX_BUILD_JOBS", "2"))
        if workers <= 0:
            raise ValueError
    except ValueError:
        sys.exit("RX_BUILD_JOBS must be a positive integer")
    with ThreadPoolExecutor(max_workers=workers) as ex:
        results = list(ex.map(lambda p: (p, scan(p, cc)), files))
    gdefs = {}           # name -> (signature, file): an aliased registration (0080f790) is defined in its owner's file
    for path, (found, literals, preprocessed, defs) in results:
        if not preprocessed:
            fallback.append(os.path.basename(path))
        literal_total += literals
        for name, sig in defs.items():
            gdefs.setdefault(name, (sig, path))
    for path, (found, literals, preprocessed, defs) in results:
        for addr, name, res in found:
            if addr in regs and regs[addr][0] != name:
                print("gen_fnsig: warning: %08x registered as %s and %s" % (addr, regs[addr][0], name), file=sys.stderr)
            if res is None:
                g = gdefs.get(name)
                res = int_only(*g[0]) if g else (False, "definition not found")
            regs[addr] = (name, res[0], res[1])
    nonint = {a: (n, why) for a, (n, ok, why) in regs.items() if not ok}
    addrs.update(a for a, (n, ok, why) in regs.items() if ok)
    if flags & {"--report", "--strict"}:
        print("gen_fnsig: %d registrations in %d game files (%d from macro families), %d integer-signature entries, %d excluded"
              % (len(regs), len(files), len(regs) - literal_total, len(addrs), len(nonint)), file=sys.stderr)
        for addr in sorted(nonint):
            print("gen_fnsig:   non-integer %08x %-40s %s" % (addr, nonint[addr][0], nonint[addr][1]), file=sys.stderr)
        for f in fallback:
            print("gen_fnsig:   literal-only scan (not preprocessed): %s" % f, file=sys.stderr)
    if fallback:
        print("gen_fnsig: warning: %d file(s) not preprocessed, macro registrations there are invisible: %s"
              % (len(fallback), ", ".join(fallback)), file=sys.stderr)
    if "--strict" in flags and fallback:
        sys.exit(1)

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    lines = ["/* generated by tools/gen_fnsig.py: registered core functions with an integer-only signature (see src/rx_mem.c);",
             " * %d registrations, %d integer-only, %d not (macro-expanded scan, rx_fn.h -DRX_FNSIG_SCAN) */" % (len(regs), len(addrs), len(nonint)),
             "#include <stdint.h>", "const uint32_t rx_fnsig_int_addr[] = {"]
    a = sorted(addrs)
    lines += ["    " + ", ".join("0x%08x" % x for x in a[i:i + 8]) + "," for i in range(0, len(a), 8)] or ["    0,"]
    lines += ["};", "const uint32_t rx_fnsig_int_count = %d;" % len(a)]
    # the registered functions the product may not call this way: their signature is not integer-only, or its definition was not found.
    # The product does not use this list; builds/fnsig/fnsig_check uses it to prove the two lists partition rx_fn.
    lines += ["const uint32_t rx_fnsig_skip_addr[] = {"]
    s = sorted(nonint)
    lines += ["    " + ", ".join("0x%08x" % x for x in s[i:i + 8]) + "," for i in range(0, len(s), 8)] or ["    0,"]
    lines += ["};", "const uint32_t rx_fnsig_skip_count = %d;" % len(s)]
    new = "\n".join(lines) + "\n"
    if not os.path.exists(out) or open(out).read() != new:
        open(out, "w").write(new)

if __name__ == "__main__":
    main()
