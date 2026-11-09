#!/usr/bin/env python3
"""vtrace_check.py -- the runtime half of sigcheck_vt.py (w-vtlog).

Why. sigcheck_vt.py checks the typed calls through vtable slots statically, but it cannot tell which classes reach a generic receiver
(`obj`, `p`, a wrapper's argument: 293 "heterogeneous" sites, 31 unresolved conversions). A build with -DRX_FN_TRACE (core/src/rx_fn.h,
core/tools/vtrace_build.sh) records every (call site file:line, the C type as written, target address) a run actually produces. This
script compares each record's C type with the target port's real prototype, with the rules sigcheck_vt.compare uses:
  return kind (void / integer / 64-bit / float / double / rx_f80 / struct), the number of arguments (a port parameter the body never
  reads does not count) and float / double / struct / integer at each position both have; a void port whose value the call uses.

usage:  vtrace_check.py <static.json> <trace file>... [--lib <librxcore.dylib>] [--list-reached] [--quiet]
  static.json   _re/.venv/bin/python core/tools/sigcheck.py --vt-dump=<static.json>      (the ports' prototypes, the typedef kinds, every
                typed conversion and every resolved call site, hetero / unresolved flags)
  trace file    what a run with RX_FN_TRACE_OUT=<file> wrote (several files are merged)
  --lib         the traced library (built with -g): the return addresses of the records are resolved with atos (inline frames included) so a
                call site that goes through a wrapper (vcall0, loop_tc0, anim_vcall0 ...) is attributed to the caller that passed the slot.
                Without it only the conversion's own file:line is known and the coverage numbers count conversions, not call sites.
Output: MISMATCH lines (one per distinct conversion / C type / target), then the coverage of the static checker's heterogeneous sites and
unresolved conversions. Exit status 1 when there is a mismatch.
"""
import os, re, sys, json, subprocess, collections, argparse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sigcheck_vt import parse_fnptr, kind_name, param_kind, split_top   # the prototype parsing sigcheck_vt uses

LINE_TOL = 3     # a multi-line statement: the debug line of a call and the line sigcheck_vt records for it can differ by a few lines


def type_kinds(tstr, conv_file, conv_line, typedefs):
    """the (ret kind, [arg kinds]) of a traced C type string, or None"""
    m = re.fullmatch(r"rx_tcall/(\d)", tstr)
    if m:
        return "I", ["I"] * (int(m.group(1)) + 1)     # rx_tcall(target, obj, n, args): the integer path (obj + n integers), integer result
    m = re.fullmatch(r"rx_ints/(\d)", tstr)
    if m:
        return "I", ["I"] * int(m.group(1))        # a generic integer dispatcher: that many integer parameters, integer result
    if tstr in ("rx_tcall/N", "rx_ints/N"):
        return None
    k = parse_fnptr(" ".join(tstr.split()))
    if k:
        return k
    if re.fullmatch(r"\w+", tstr):
        c = typedefs.get(tstr)
        if c:
            same = [x for x in c if x[0] == conv_file]
            before = [x for x in same if x[1] <= conv_line]     # a function-local `typedef ... fn_t;` name is reused in one file: the last one above the call
            x = (before[-1:] or same[:1] or [x for x in c if x[0].endswith(".h")][:1] or c[:1])[0]
            return x[2], x[3]
    return None


def compare(cr, cargs, port, used):
    """sigcheck_vt.compare for one (call type, port): the list of messages"""
    pr, pargs = port["ret"], port["args"]
    msg = []
    if cr != pr:
        if cr == "I" and pr == "L":
            pass
        elif cr == "V" and pr in ("I", "L", "F", "D"):
            pass
        elif cr == "V":
            msg.append("the call ignores the result but the port returns %s (a hidden result pointer the call does not pass)" % kind_name(pr))
        elif pr == "V":
            if used:
                msg.append("the call uses a %s result but the port returns void" % kind_name(cr))
        else:
            msg.append("the call returns %s, the port returns %s" % (kind_name(cr), kind_name(pr)))
    pused = port["used"]
    if len(pargs) > len(cargs) and any(pused[i] for i in range(len(cargs), len(pargs))):
        msg.append("the port takes %d parameters, the call passes %d" % (len(pargs), len(cargs)))
    for i in range(min(len(pargs), len(cargs))):
        if pargs[i] != cargs[i] and pused[i]:
            msg.append("argument %d is %s in the call, %s in the port" % (i, kind_name(cargs[i]), kind_name(pargs[i])))
    return msg


def severity(msg):
    """HARD: the register classes or the hidden result pointer differ (float vs integer vs rx_f80 / struct); ARGS: the port reads more integer
    parameters than the call passes; VOIDUSE: the call uses a result the void port does not produce"""
    if msg.startswith("the pointer has the prototype"):
        return "TYPEOF"
    if msg.startswith("the port takes"):
        return "ARGS"
    if msg.startswith("the call uses"):
        return "VOIDUSE"
    return "HARD"


def split_loc(s):
    f, _, l = s.rpartition(":")
    return os.path.basename(f), int(l)


def resolve_frames(addrs, libs):
    """'librxcore.dylib+0x1234' -> [(function, file, line)] innermost first (inline frames included), by atos on addr-1"""
    out = {}
    for lib in libs:
        out.update(_resolve_one(addrs, lib))
    return out


def _resolve_one(addrs, lib):
    out = {}
    todo = sorted({a for a in addrs if a != "-" and a.split("+")[0] == os.path.basename(lib)})
    BASE = 0x100000000
    for i in range(0, len(todo), 400):
        chunk = todo[i:i + 400]
        args = ["atos", "-o", lib, "-arch", "arm64", "-l", hex(BASE), "-i"] + [hex(BASE + int(a.split("+")[1], 16) - 1) for a in chunk]
        txt = subprocess.run(args, capture_output=True, text=True).stdout
        groups, cur = [], []
        for ln in txt.split("\n"):
            if ln.strip() == "":
                if cur:
                    groups.append(cur)
                cur = []
            else:
                cur.append(ln)
        if cur:
            groups.append(cur)
        if len(groups) != len(chunk):
            sys.stderr.write("atos: %d answers for %d addresses (chunk %d); frames of this chunk dropped\n" % (len(groups), len(chunk), i))
            continue
        for a, g in zip(chunk, groups):
            fr = []
            for ln in g:
                m = re.match(r"^(.*?) \(in [^)]*\) \((.*?):(\d+)\)$", ln)
                if m:
                    fr.append((m.group(1), m.group(2), int(m.group(3))))
            out[a] = fr
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("static")
    ap.add_argument("traces", nargs="+")
    ap.add_argument("--lib", action="append", help="a traced image built with -g (the dylib, or a statically linked driver); repeatable")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--args", action="store_true", help="list the ARGS class (the port reads more parameters than the call passes)")
    ap.add_argument("--voiduse", action="store_true", help="list the VOIDUSE class (the call uses a result of a void port)")
    ap.add_argument("--list-reached", action="store_true")
    ap.add_argument("--list-unreached", action="store_true")
    a = ap.parse_args()
    st = json.load(open(a.static))
    for i_, s_ in enumerate(st["sites"]):
        s_["_i"] = i_
    ports, typedefs = st["ports"], st["typedefs"]
    convs = collections.defaultdict(list)       # (file, how) -> [conv]
    for c in st["conversions"]:
        convs[(c["file"], c["how"])].append(c)
    conv_by_loc = collections.defaultdict(list)
    for c in st["conversions"]:
        conv_by_loc[(c["file"], c["line"])].append(c)

    # ---- the records: (conv file, conv line, type, target) -> {ras: hits, files: set}
    recs = collections.OrderedDict()
    for tf in a.traces:
        for ln in open(tf, encoding="utf-8", errors="replace"):
            f = ln.rstrip("\n").split("\t")
            if len(f) < 5:
                continue
            fl, line = split_loc(f[0])
            key = (fl, line, f[1], f[2])
            r = recs.setdefault(key, dict(ras=collections.Counter(), runs=set(), hits=0))
            r["ras"][tuple(f[3].split(","))] += int(f[5]) if len(f) > 5 else 1
            r["runs"].add(os.path.basename(tf))
            r["hits"] += int(f[5]) if len(f) > 5 else 1
    frames = {}
    if a.lib:
        frames = resolve_frames({x for r in recs.values() for ras in r["ras"] for x in ras}, a.lib)

    # ---- which static call sites each record belongs to
    # a record belongs to a static site when the conversion matches and (with frames) the site's caller line is among the record's frames
    by_conv = collections.defaultdict(list)       # conv (file, line) -> records
    for key, r in recs.items():
        by_conv[(key[0], key[1])].append((key, r))

    def conv_records(cf, cl):
        out = by_conv.get((cf, cl), [])
        if not out:
            for dl in range(1, LINE_TOL + 1):
                out = by_conv.get((cf, cl + dl), []) or by_conv.get((cf, cl - dl), [])
                if out:
                    break
        return out

    # Attribution: a record (conversion / type / target / return addresses) belongs to the static sites whose caller line is among the frames of
    # its return addresses. Without a wrapper chain the conversion is the site. With a chain, the site is the wrapper's caller: its file and
    # line (within LINE_TOL: a call split over lines) must be a frame, and per return-address tuple only the nearest line counts (two
    # neighbouring calls of one function are told apart by the exact line).
    sites_of_conv = collections.defaultdict(list)       # conv (file, line) -> static sites
    for i_, s_ in enumerate(st["sites"]):
        sites_of_conv[tuple(s_["conv"])].append(i_)
    site_recs = collections.defaultdict(list)           # site index -> [(key, record)]
    rec_sites = collections.defaultdict(list)           # record key -> static sites
    loose = set()                                       # site indices attributed by the conversion alone (no frames)
    for key, r in recs.items():
        cand = []
        for dl in [0] + [d for k_ in range(1, LINE_TOL + 1) for d in (k_, -k_)]:
            cand = sites_of_conv.get((key[0], key[1] + dl), [])
            if cand:
                break
        picked = set()
        for i_ in cand:
            s_ = st["sites"][i_]
            if not s_["chain"]:
                picked.add(i_)
            elif not frames:
                picked.add(i_)
                loose.add(i_)
        by_chain = [i_ for i_ in cand if st["sites"][i_]["chain"]]
        if frames and by_chain:
            for ras in r["ras"]:
                best = None
                for i_ in by_chain:
                    s_ = st["sites"][i_]
                    for x in ras:
                        for (fn, f_, l_) in frames.get(x, []):
                            if os.path.basename(f_) == s_["file"] and abs(l_ - s_["line"]) <= LINE_TOL:
                                d_ = abs(l_ - s_["line"])
                                if best is None or d_ < best[0]:
                                    best = (d_, [i_])
                                elif d_ == best[0] and i_ not in best[1]:
                                    best[1].append(i_)
                if best:
                    picked.update(best[1])
        for i_ in picked:
            site_recs[i_].append((key, r))
            rec_sites[key].append(st["sites"][i_])

    def reached(s):
        i_ = s["_i"]
        return site_recs.get(i_, []), i_ not in loose

    # ---- the comparison
    mism = []
    n_typed = n_static_typed = n_unregistered = n_untyped = n_typeof = 0
    seen_targets = collections.defaultdict(set)       # conv (file, line) -> targets
    for (fl, line, ts, tg), r in recs.items():
        seen_targets[(fl, line)].add(tg)
        port = ports.get(tg)
        kinds, used, src, used_src = None, True, "traced type", "conversion"
        mt = re.fullmatch(r"__typeof__\(&(\w+)\)", ts)
        if mt:
            # PL_CORE / LOOP_CORE / RX_FN(addr, __typeof__(&fn)): the pointer has the prototype of the named function `fn`. The prototype is the
            # compiler's, so the only way it can be wrong is that the port registered at the address is another function.
            n_typeof += 1
            if port is None:
                n_unregistered += 1
            elif port["name"] != mt.group(1):
                mism.append(dict(site="%s:%d" % (fl, line), type=ts, target=tg, port=port, msgs=["the pointer has the prototype of %s, the port registered at the address is %s" % (mt.group(1), port["name"])],
                                 r=r, kinds=None, src="typeof", used=True, used_src="-"))
            continue
        if ts != "-":
            kinds = type_kinds(ts, fl, line, typedefs)
            cs = conv_by_loc.get((fl, line)) or [c for dl in range(-LINE_TOL, LINE_TOL + 1) for c in conv_by_loc.get((fl, line + dl), [])]
            if cs:
                used = any(c["used"] for c in cs)
            used_src = "conversion"
            if rec_sites.get((fl, line, ts, tg)):
                used = any(x["used"] for x in rec_sites[(fl, line, ts, tg)])      # the callers that passed this slot: is the value used there?
                used_src = "call site"
            n_typed += 1
            cands = [(kinds, ts)] if kinds else []
        else:
            cs = [c for c in conv_by_loc.get((fl, line), []) if c["how"] == "rx_fn"]
            if not cs:
                n_untyped += 1
                continue
            n_static_typed += 1
            src = "cast type from the static scan"
            cands = [((c["ret"], c["args"]), "%s (cast)" % kind_name(c["ret"])) for c in cs]
            used = any(c["used"] for c in cs)
        if not cands:
            n_untyped += 1
            continue
        if port is None:
            n_unregistered += 1       # rx_fn returned NULL: the call site's own fallback (rx_call_original) takes it
            continue
        for kinds, label in cands:
            msgs = compare(kinds[0], kinds[1], port, used)
            if msgs:
                if len(cands) > 1 and any(not compare(k[0], k[1], port, used) for k, _ in cands):
                    continue        # several casts of one rx_fn result (a dispatcher): one of them fits this target
                mism.append(dict(site="%s:%d" % (fl, line), type=ts if ts != "-" else label, target=tg, port=port, msgs=msgs, r=r, kinds=kinds, src=src, used=used, used_src=used_src))

    def who(r):
        out = []
        for ras, n in r["ras"].most_common(3):
            fr = []
            for x in ras:
                for (fn, f_, l_) in frames.get(x, [])[:3]:
                    fr.append("%s(%s:%d)" % (fn, f_, l_))
                    if len(fr) >= 4:
                        break
                if len(fr) >= 4:
                    break
            out.append("%s x%d" % (" < ".join(fr) if fr else "(no frames)", n))
        return "; ".join(out)

    for m in mism:
        m["sev"] = sorted({severity(x) for x in m["msgs"]}, key=["HARD", "TYPEOF", "ARGS", "VOIDUSE"].index)[0]
    for sev in ("HARD", "TYPEOF", "ARGS", "VOIDUSE"):
        if sev not in ("HARD", "TYPEOF") and not (a.args if sev == "ARGS" else a.voiduse):
            continue
        for m in [x for x in mism if x["sev"] == sev]:
            print("MISMATCH %s\t%s\t%s\ttarget %s %s\t%s\t[%d hits, runs: %s]\n\tcallers: %s" % (
                sev, m["site"], m["type"], m["target"], m["port"]["text"][:160], " | ".join(m["msgs"]) + (" (value use read off the %s)" % m["used_src"] if sev == "VOIDUSE" else ""), m["r"]["hits"], ",".join(sorted(m["r"]["runs"])), who(m["r"])))
    for m in []:
        print("MISMATCH\t%s\t%s\ttarget %s %s\t%s\t[%d hits, runs: %s]\n\tcallers: %s" % (
            m["site"], m["type"], m["target"], m["port"]["text"][:160], " | ".join(m["msgs"]), m["r"]["hits"], ",".join(sorted(m["r"]["runs"])), who(m["r"])))
    print("records %d (distinct conversion / type / target): %d with a traced C type, %d typed by the static scan (rx_fn casts), %d skipped (no type: presence tests, typeof casts, "
          "variable rx_tcall counts), %d targets not registered (the call site's fallback); %d __typeof__(&function) pointers (name checked); MISMATCH: %d HARD, %d TYPEOF, %d ARGS, %d VOIDUSE" % (
              len(recs), n_typed, n_static_typed, n_untyped, n_unregistered, n_typeof, *[sum(1 for m in mism if m["sev"] == k) for k in ("HARD", "TYPEOF", "ARGS", "VOIDUSE")]))

    # ---- coverage
    het = [s for s in st["sites"] if s["hetero"]]
    het_hit = [(s, reached(s)) for s in het]
    n_exact = sum(1 for s, (rs, ex) in het_hit if rs and ex)
    n_loose = sum(1 for s, (rs, ex) in het_hit if rs and not ex)
    print("\nCOVERAGE heterogeneous generic-receiver sites (sigcheck --vt-hetero): %d of %d reached by the runs%s" % (
        n_exact + n_loose, len(het), "" if frames else " (no --lib: counted by the conversion only)"))
    if n_loose:
        print("  (%d of them by the conversion's file:line only: the wrapper's caller could not be matched in the frames)" % n_loose)
    tg_by_site = {}
    for s, (rs, ex) in het_hit:
        tg_by_site[(s["file"], s["line"], s["func"], s["slot"], s["how"], tuple(s["chain"]))] = sorted({k[3] for k, _ in rs})
    if a.list_reached:
        for s, (rs, ex) in het_hit:
            if rs:
                print("  reached\t%s:%d\t%s\tslot %s\t%s\ttargets %s" % (s["file"], s["line"], s["func"], s["slot"], ">".join(s["chain"]) or "-", ",".join(sorted({k[3] for k, _ in rs}))))
    if a.list_unreached or not a.quiet:
        for s, (rs, ex) in het_hit:
            if not rs:
                print("  unreached\t%s:%d\t%s\tslot %s\t%s%s" % (s["file"], s["line"], s["func"], s["slot"], s["how"], (" via " + ">".join(s["chain"])) if s["chain"] else ""))
    # every resolved site, for the record
    allr = [(s, reached(s)) for s in st["sites"]]
    print("all %d resolved call sites: %d reached" % (len(st["sites"]), sum(1 for s, (rs, ex) in allr if rs)))
    # the conversions the static check could not resolve
    un = st["unresolved"]
    print("\nCOVERAGE unresolved conversions (sigcheck --vt-unresolved): %d" % len(un))
    nun = 0
    for u in un:
        cf, cl = u["conv"] if u["conv"] else (u["file"], u["line"])
        rs = conv_records(cf, cl)
        tgs = sorted({k[3] for k, _ in rs})
        nun += 1 if rs else 0
        if rs:
            names = ["%s %s" % (t, ports[t]["name"] if t in ports else "(not registered)") for t in tgs[:6]]
            print("  reached\t%s:%d\t%s\ttargets %d: %s%s" % (cf, cl, u["text"][:70], len(tgs), "; ".join(names), " ..." if len(tgs) > 6 else ""))
        else:
            print("  unreached\t%s:%d\t%s" % (cf, cl, u["text"][:90]))
    print("unresolved conversions reached: %d of %d" % (nun, len(un)))
    return 1 if any(m["sev"] in ("HARD", "TYPEOF") for m in mism) else 0


if __name__ == "__main__":
    sys.exit(main())
