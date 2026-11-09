#!/usr/bin/env python3
"""Bit-exact verification of f80.cpp against _re/trace/x87.py.

Usage: _re/.venv/bin/python verify.py [--seed N] [--scale F] [--archs arm64,x86_64]
Generates >200k cases, computes the expected result with x87.py (multiprocessing), runs the
C++ driver for every arch and compares line by line. Writes report.json.
"""
import argparse, json, os, random, subprocess, sys, time
from fractions import Fraction
from multiprocessing import Pool

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, "/Users/payton/Documents/Development/REFLEX/_re/trace")
import x87  # noqa: E402
from x87 import F80  # noqa: E402

M64 = (1 << 64) - 1


# ------------------------------------------------------------------ token <-> F80
def tok(v):
    if v.kind == "nan": return "q%d" % v.sign
    if v.kind == "inf": return "i%d" % v.sign
    if v.mag == 0: return "z%d" % v.sign
    m, e = x87.frexp_fraction(v.mag)
    mm = m * (1 << 63)
    assert mm.denominator == 1, "more than 64 significant bits: %r" % (v.mag,)
    return "n%d:%016x:%d" % (v.sign, int(mm), e)


def untok(t):
    s = int(t[1])
    if t[0] == "z": return F80("num", s)
    if t[0] == "i": return F80("inf", s)
    if t[0] == "q": return F80("nan", s)
    _, mh, e = t.split(":")
    sh = int(e) - 63
    mag = Fraction(int(mh, 16) << sh) if sh >= 0 else Fraction(int(mh, 16), 1 << -sh)
    return F80("num", s, mag)


def mktok(sign, mant, exp):
    return "n%d:%016x:%d" % (sign, mant, exp)


# ------------------------------------------------------------------ expected values
def bits_overflow_aware(a, width, rc):
    """x87.py to_bits, except that overflow yields infinity (x87.py's `r.kind == "inf"` branch is dead
    code: round_fraction never returns inf, so for finite values >= 2^emax+1 it emits garbage)."""
    mbits, ebits = (24, 8) if width == 32 else (53, 11)
    bias = (1 << (ebits - 1)) - 1
    if a.kind == "num" and a.mag != 0:
        r = x87.round_fraction(a.mag, mbits, rc, a.sign, emin=1 - bias)
        if r.mag != 0:
            _, e = x87.frexp_fraction(r.mag)
            if e > bias:
                return (a.sign << (width - 1)) | (((1 << ebits) - 1) << (mbits - 1))
    return a.to_bits(width, rc)



def hp_transcendental(op, a, b, rc, prec=420):
    """x87.py's own formulas evaluated at `prec` digits instead of 90 (high-precision oracle used where the
    90-digit evaluation is not trustworthy: operands tiny enough that x87.py's absolute series tolerance
    matters, or very large trig arguments)."""
    from decimal import Decimal, localcontext
    D = x87._dec
    with localcontext() as ctx:
        ctx.prec = prec
        if op == "fpatan":
            x, y = D(a.value()), D(b.value())
            if x > 0: r = x87._atan(y / x)
            else: r = x87._atan(y / x) + (x87._pi() if (y > 0 or (y == 0 and not b.sign)) else -x87._pi())
            return x87.rounded(Fraction(r), 64, rc) if r != 0 else F80("num", b.sign)
        if op == "fyl2xp1":
            x, y = D(a.value()), D(b.value())
            r = y * ((x + 1).ln() / Decimal(2).ln())
            return x87.rounded(Fraction(r), 64, rc) if r != 0 else F80("num", a.sign ^ b.sign)
        if op == "f2xm1":
            x = D(a.value())
            r = (x * Decimal(2).ln()).exp() - 1
            return x87.rounded(Fraction(r), 64, rc) if r != 0 else F80("num", a.sign)
        s_, c_ = x87._sin_cos(D(a.value()))
        r = s_ if op == "fsin" else c_ if op == "fcos" else s_ / c_
        return x87.rounded(Fraction(r), 64, rc) if r != 0 else F80("num", a.sign)

def py_rem(x, y, nearest):
    if x.kind != "num" or y.kind != "num" or y.is_zero():
        return F80("nan", 1), 0
    q = x.value() / y.value()
    q = round(q) if nearest else int(q)
    r = x.value() - q * y.value()
    res = F80.num(r) if r != 0 else F80("num", x.sign)
    return res, abs(q) & 7


def exact_oracle(cat, name, a, b, rc):
    """True mathematically-exact result (the true value is exactly representable before rounding to
    64 bits, or -- for exact_fyl2xp1_hugepow2 -- exactly k plus an infinitesimal)."""
    y = b.value() if b is not None else None
    if name == "f2xm1":
        n = int(a.value())
        return tok(x87.rounded(Fraction(2) ** n - 1, 64, rc))
    if name == "fyl2x":
        k = (a.mag.numerator.bit_length() - 1) - (a.mag.denominator.bit_length() - 1)
        if k == 0: return tok(F80("num", a.sign ^ b.sign))
        if y == 0: return tok(F80("num", 0))
        return tok(x87.rounded(y * k, 64, rc))
    v = a.value() + 1                       # fyl2xp1
    if a.mag == 0 or y == 0: return tok(F80("num", a.sign ^ b.sign))
    if cat == "exact_fyl2xp1_hugepow2":
        k = a.mag.numerator.bit_length() - 1
        return tok(x87.rounded(y * k * (1 + Fraction(1, 2 ** (k + 64))), 64, rc))
    k = (v.numerator.bit_length() - 1) - (v.denominator.bit_length() - 1)
    return tok(x87.rounded(y * k, 64, rc))


def expected(case):
    cat, line = case
    p = line.split()
    op = p[0]
    if op in ("add", "sub"):
        return tok(x87.add(untok(p[4]), untok(p[5]), int(p[1]), int(p[2]), negate_b=bool(int(p[3]))))
    if op == "mul": return tok(x87.mul(untok(p[3]), untok(p[4]), int(p[1]), int(p[2])))
    if op == "div": return tok(x87.div(untok(p[3]), untok(p[4]), int(p[1]), int(p[2])))
    if op == "sqrt": return tok(x87.sqrt(untok(p[3]), int(p[1]), int(p[2])))
    if op == "cmp":
        c = x87.compare(untok(p[1]), untok(p[2]))
        return "2" if c is None else str(c)
    if op == "rint": return tok(x87.round_to_integer(untok(p[2]), int(p[1])))
    if op == "scale":
        a = untok(p[1]); n = int(p[2])
        return tok(F80(a.kind, a.sign, a.mag * Fraction(2) ** n))
    if op == "rem":
        r, q = py_rem(untok(p[2]), untok(p[3]), int(p[1]))
        return "%s %d" % (tok(r), q)
    if op == "to_f32": return "%08x" % bits_overflow_aware(untok(p[2]), 32, int(p[1]))
    if op == "to_f64": return "%016x" % bits_overflow_aware(untok(p[2]), 64, int(p[1]))
    if op == "to_f80": return untok(p[1]).to_f80().hex()
    if op == "from_f32": return tok(F80.from_f32(int(p[1], 16)))
    if op == "from_f64": return tok(F80.from_f64(int(p[1], 16)))
    if op == "from_f80": return tok(F80.from_f80(bytes.fromhex(p[1])))
    if op == "from_int": return tok(F80.from_int(int(p[1])))
    if op == "tr":
        name, rc = p[1], int(p[2])
        a = None if p[3] == "-" else untok(p[3])
        b = None if p[4] == "-" else untok(p[4])
        if cat == "fpatan_negzero_corrected":
            # x87.py returns the UNROUNDED pi here (bug); expectation = pi rounded to 64 bits.
            from decimal import localcontext
            with localcontext() as ctx:
                ctx.prec = 90
                r = x87.round_fraction(Fraction(x87._pi()), 64, rc, b.sign)
            return tok(F80("num", b.sign, r.mag))
        if cat.startswith("hp_"):
            return tok(hp_transcendental(name, a, b, rc))
        try:
            py = tok(x87.transcendental(name, a, b, bits=64, rc=rc))
        except Exception:
            py = "ERR"
        if cat.startswith("exact"):
            return exact_oracle(cat, name, a, b, rc) + "\t" + py
        return py
    raise ValueError(line)


# ------------------------------------------------------------------ generators
class Gen:
    def __init__(self, seed):
        self.r = random.Random(seed)
        self.cases = []

    def add(self, cat, line):
        p = line.split()
        if p[0] == "tr" and p[1] in ("fyl2x", "fyl2xp1", "f2xm1") and not cat.startswith("exact"):
            a = untok(p[3])
            if a.kind == "num" and a.mag != 0:
                if p[1] == "fyl2x": ex = a.sign == 0 and (a.mag.numerator & (a.mag.numerator - 1)) == 0 and (a.mag.denominator & (a.mag.denominator - 1)) == 0
                elif p[1] == "fyl2xp1":
                    v = a.value() + 1
                    ex = v > 0 and (v.numerator & (v.numerator - 1)) == 0 and (v.denominator & (v.denominator - 1)) == 0
                    # a = 2^k with k > ~297: x87.py's 90-digit "x + 1" drops the +1, so it sees an exact integer log2
                    if a.sign == 0 and (a.mag.numerator & (a.mag.numerator - 1)) == 0 and a.mag.numerator.bit_length() > 290:
                        ex = False; cat = "exact_fyl2xp1_hugepow2"
                else: ex = a.mag.denominator == 1
                if ex: cat = "exact_" + p[1] + "_misc"
        self.cases.append((cat, line))

    # --- mantissa / exponent / value
    def mant(self):
        r = self.r
        k = r.random()
        if k < 0.30: return (1 << 63) | r.getrandbits(63)
        if k < 0.38: return 1 << 63
        if k < 0.46: return (1 << 63) | r.getrandbits(r.randint(1, 8))
        if k < 0.50: return M64
        if k < 0.54: return (1 << 63) | ((M64 >> r.randint(1, 20)) & ~(1 << 63)) | 0
        w = r.choice([1, 2, 5, 11, 12, 24, 25, 26, 32, 52, 53, 54, 55, 63, r.randint(1, 64)])
        top = (1 << (w - 1)) | r.getrandbits(w - 1)
        return top << (64 - w)

    def exp(self, wide=True, small=40):
        r = self.r
        k = r.random()
        if k < 0.70: return r.randint(-small, small)
        if k < 0.90 or not wide: return r.randint(-300, 300)
        return r.randint(-16000, 16000)

    def num(self, wide=True, small=40, zero=0.03):
        r = self.r
        s = r.getrandbits(1)
        if r.random() < zero: return "z%d" % s
        return mktok(s, self.mant(), self.exp(wide, small))

    def any(self, spec=0.05):
        r = self.r
        if r.random() < spec:
            return r.choice(["i0", "i1", "q0", "q1", "z0", "z1"])
        return self.num()

    def rc(self): return self.r.randint(0, 3)
    def bits(self): return self.r.choice([24, 53, 64])


def parse_num(t):
    _, mh, e = t.split(":")
    return int(t[1]), int(mh, 16), int(e)


def gen_arith(g, n_add, n_mul, n_div, n_sqrt):
    r = g.r
    for i in range(n_add):
        bits, rc, neg = g.bits(), g.rc(), r.randint(0, 1)
        k = r.random()
        a = g.any(0.04)
        if k < 0.40:
            b = g.any(0.04)
        elif k < 0.70 and a[0] == "n":  # nearby exponent
            s, m, e = parse_num(a)
            b = mktok(r.getrandbits(1), g.mant(), e + r.randint(-70, 70))
        elif k < 0.80 and a[0] == "n":  # exact cancellation / near cancellation
            s, m, e = parse_num(a)
            m2 = m if r.random() < 0.6 else (m ^ (1 << r.randint(0, 62)))
            b = mktok(s ^ 1 ^ neg, m2, e)
            if r.random() < 0.3: b = mktok(s ^ 1 ^ neg, m, e - 1) if m == (1 << 63) else b
        elif k < 0.92 and a[0] == "n":  # exact tie at precision `bits`
            s, m, e = parse_num(a)
            m = (m >> (64 - bits)) << (64 - bits)
            m |= (1 << 63)
            a = mktok(s, m, e)
            sb = r.getrandbits(1)
            b = mktok(sb, 1 << 63, e - bits)  # half ulp
            if r.random() < 0.4: b = mktok(sb, (1 << 63) | (1 << r.randint(0, 62)), e - bits) if False else b
            if bits == 64 and r.random() < 0.5: a = mktok(s, m | 1, e)
        else:  # huge exponent gap
            if a[0] == "n":
                s, m, e = parse_num(a)
                b = mktok(r.getrandbits(1), g.mant(), e - r.randint(60, 17000) * r.choice([1, -1]))
            else:
                b = g.any()
        g.add("add" if not neg else "sub", "add %d %d %d %s %s" % (bits, rc, neg, a, b))
    for i in range(n_mul):
        bits, rc = g.bits(), g.rc()
        k = r.random()
        if k < 0.5: a, b = g.any(0.04), g.any(0.04)
        elif k < 0.8:  # small integer mantissas -> ties / exact products
            def small():
                w = r.randint(1, 40)
                return mktok(r.getrandbits(1), ((1 << (w - 1)) | r.getrandbits(w - 1)) << (64 - w), r.randint(-60, 60))
            a, b = small(), small()
        else:  # extreme exponents -> gradual underflow regime of x87.py
            a = mktok(r.getrandbits(1), g.mant(), r.randint(-17000, 17000))
            b = mktok(r.getrandbits(1), g.mant(), r.randint(-17000, 17000))
        g.add("mul", "mul %d %d %s %s" % (bits, rc, a, b))
    for i in range(n_div):
        bits, rc = g.bits(), g.rc()
        k = r.random()
        if k < 0.45: a, b = g.any(0.04), g.any(0.04)
        elif k < 0.75:  # exact tie: a = Q*b with Q having bits+1 significant bits ending in 1
            qb = min(bits + 1, 63)
            Q = (1 << (qb - 1)) | r.getrandbits(qb - 1) | 1
            bw = r.randint(1, max(1, 64 - qb))
            B = (1 << (bw - 1)) | r.getrandbits(bw - 1)
            A = Q * B
            sh = 64 - A.bit_length()
            s1, s2 = r.getrandbits(1), r.getrandbits(1)
            eb = r.randint(-40, 40)
            ea = (A.bit_length() - 1) - (B.bit_length() - 1) + eb - (Q.bit_length() - 1)
            # value A/B = Q ; as F80: a = A * 2^0, b = B * 2^0 (exact)
            a = mktok(s1, A << sh, A.bit_length() - 1)
            b = mktok(s2, B << (64 - B.bit_length()), B.bit_length() - 1)
        elif k < 0.9:
            a = mktok(r.getrandbits(1), g.mant(), r.randint(-17000, 17000))
            b = mktok(r.getrandbits(1), g.mant(), r.randint(-17000, 17000))
        else:
            a, b = g.any(0.3), g.any(0.3)
        g.add("div", "div %d %d %s %s" % (bits, rc, a, b))
    for i in range(n_sqrt):
        bits, rc = g.bits(), g.rc()
        k = r.random()
        if k < 0.5:
            s, m, e = 0, g.mant(), g.exp()
            a = mktok(0 if r.random() < 0.97 else 1, m, e)
        elif k < 0.7:  # perfect squares
            w = r.randint(1, 32)
            kk = (1 << (w - 1)) | r.getrandbits(w - 1)
            sq = kk * kk
            e2 = 2 * r.randint(-100, 100)
            a = mktok(0, sq << (64 - sq.bit_length()), sq.bit_length() - 1 + e2)
        elif k < 0.85:
            a = mktok(0, g.mant(), r.randint(-16000, 16000))
        else:
            a = g.any(0.5)
        g.add("sqrt", "sqrt %d %d %s" % (bits, rc, a))


def gen_misc(g, n_cmp, n_rint, n_scale, n_rem):
    r = g.r
    for i in range(n_cmp):
        k = r.random()
        if k < 0.3: a, b = g.num(), g.num()
        elif k < 0.5:
            a = g.num(); b = a
            if r.random() < 0.5 and a[0] == "n":
                s, m, e = parse_num(a); b = mktok(s ^ 1, m, e)
        elif k < 0.7 and True:  # same exponent / mantissa neighbours
            s, m, e = r.getrandbits(1), g.mant(), r.randint(-50, 50)
            a = mktok(s, m, e); b = mktok(r.getrandbits(1), m ^ (1 << r.randint(0, 62)), e)
        elif k < 0.85:
            a, b = r.choice(["z0", "z1", "i0", "i1", "q0", "q1"]), r.choice(["z0", "z1", "i0", "i1", "q0", "q1"])
        else:  # inf/nan vs moderate finite (x87.py float() overflows beyond ~1e308)
            a = r.choice(["i0", "i1", "q0", "z0", "z1"]); b = mktok(r.getrandbits(1), g.mant(), r.randint(-1000, 1000))
            if r.random() < 0.5: a, b = b, a
        g.add("cmp", "cmp %s %s" % (a, b))
    for i in range(n_rint):
        rc = g.rc(); k = r.random()
        if k < 0.35:
            a = mktok(r.getrandbits(1), g.mant(), r.randint(-70, 70))
        elif k < 0.60:  # exact halves n + 1/2
            n = r.getrandbits(r.randint(0, 40)); v = Fraction(2 * n + 1, 2) * (1 if r.random() < 0.5 else -1)
            a = tok(F80.num(v))
        elif k < 0.75:
            n = r.getrandbits(r.randint(0, 62)); a = tok(F80.num(Fraction(n) * (1 if r.random() < .5 else -1)))
        elif k < 0.85:
            a = g.any(0.5)
        else:
            a = mktok(r.getrandbits(1), g.mant(), r.randint(-300, 300))
        g.add("rint", "rint %d %s" % (rc, a))
    for i in range(n_scale):
        a = g.num(zero=0.05)
        g.add("scale", "scale %s %d" % (a, r.choice([r.randint(-40, 40), r.randint(-40000, 40000), 0])))
    for i in range(n_rem):
        k = r.random()
        near = r.getrandbits(1)
        if k < 0.04:
            x, y = g.any(0.6), g.any(0.6)
        elif k < 0.10:
            x = g.num(); y = mktok(r.getrandbits(1), g.mant(), r.randint(-60, 60)); x = "z%d" % r.getrandbits(1) if r.random() < 0.5 else x
        else:
            ey = r.randint(-60, 60)
            kk = r.random()
            if kk < 0.4: ex = ey + r.randint(-3, 3)
            elif kk < 0.8: ex = ey + r.randint(0, 80)
            elif kk < 0.95: ex = ey + r.randint(0, 3000)
            else: ex = ey + r.randint(-16000, 16000)
            x = mktok(r.getrandbits(1), g.mant(), ex); y = mktok(r.getrandbits(1), g.mant(), ey)
            if r.random() < 0.08: y = mktok(parse_num(y)[0], parse_num(x)[1], ey)         # equal mantissas
            if r.random() < 0.05:  # exact multiples / half multiples
                s, m, e = parse_num(y); q = r.randint(1, 20)
                v = Fraction(q * 2 + (1 if r.random() < 0.5 else 0), 2) * untok(y).mag * (-1 if parse_num(x)[0] else 1)
                try: x = tok(F80.num(v))
                except AssertionError: pass
        g.add("rem", "rem %d %s %s" % (near, x, y))


def gen_conv(g, n32, n64, n80, nf32, nf64, nf80, nint):
    r = g.r
    def val(lo, hi, spec=0.05):
        if r.random() < spec: return r.choice(["z0", "z1", "i0", "i1", "q0", "q1"])
        return mktok(r.getrandbits(1), g.mant(), r.randint(lo, hi))
    for i in range(n32):
        k = r.random()
        rng = (-160, -100) if k < 0.4 else (-12, 12) if k < 0.6 else (100, 135) if k < 0.8 else (-300, 300)
        g.add("to_f32", "to_f32 %d %s" % (g.rc(), val(*rng)))
    for i in range(n64):
        k = r.random()
        rng = (-1080, -1000) if k < 0.4 else (-12, 12) if k < 0.6 else (1000, 1035) if k < 0.8 else (-3000, 3000)
        g.add("to_f64", "to_f64 %d %s" % (g.rc(), val(*rng)))
    for i in range(n80):
        g.add("to_f80", "to_f80 %s" % val(-16000, 16000))
    for i in range(nf32):
        e = r.choice([0, 1, 254, 255, r.randint(0, 255)])
        fr = r.choice([0, 1, 0x7fffff, r.getrandbits(23)])
        g.add("from_f32", "from_f32 %08x" % ((r.getrandbits(1) << 31) | (e << 23) | fr))
    for i in range(nf64):
        e = r.choice([0, 1, 2046, 2047, r.randint(0, 2047)])
        fr = r.choice([0, 1, (1 << 52) - 1, r.getrandbits(52)])
        g.add("from_f64", "from_f64 %016x" % ((r.getrandbits(1) << 63) | (e << 52) | fr))
    for i in range(nf80):
        e = r.choice([0, 1, 0x7ffe, 0x7fff, 0x3fff, 0x3ffe, r.randint(0, 0x7fff)])
        m = r.choice([0, 1 << 63, 3 << 62, M64, r.getrandbits(64), r.getrandbits(64) | (1 << 63), r.getrandbits(30)])
        raw = m.to_bytes(8, "little") + ((r.getrandbits(1) << 15) | e).to_bytes(2, "little")
        g.add("from_f80", "from_f80 %s" % raw.hex())
    for i in range(nint):
        k = r.random()
        if k < 0.1: v = r.choice([0, 1, -1, (1 << 63) - 1, -(1 << 63), -(1 << 63) + 1])
        else: v = r.choice([-1, 1]) * r.getrandbits(r.randint(0, 63))
        g.add("from_int", "from_int %d" % v)


def pi_frac():
    from decimal import localcontext
    with localcontext() as ctx:
        ctx.prec = 90
        return Fraction(x87._pi())


def f80tok_of(v):  # Fraction (signed) -> nearest 64-bit F80 token
    r = x87.round_fraction(abs(v), 64, 0, 1 if v < 0 else 0)
    return tok(r)


def gen_trans(g, scale):
    r = g.r
    S = lambda n: max(1, int(n * scale))
    def signed(ex_lo, ex_hi, zero=0.0):
        if r.random() < zero: return "z%d" % r.getrandbits(1)
        return mktok(r.getrandbits(1), g.mant(), r.randint(ex_lo, ex_hi))
    # constants
    for op in ("pi", "l2e", "ln2", "l2t", "lg2"):
        for rc in range(4): g.add("const", "tr %s %d - -" % (op, rc))
    # fpatan: atan2(y=b, x=a)
    pi = pi_frac()
    for i in range(S(4000)):
        k = r.random()
        rc = g.rc()
        if k < 0.80:
            ex = r.randint(-20, 20)
            x = mktok(r.getrandbits(1), g.mant(), ex)
            y = mktok(r.getrandbits(1), g.mant(), ex + r.randint(-60, 60))
        elif k < 0.88:   # tiny ratio, nearest mode only (x87.py series has an absolute tolerance, see notes)
            rc = 0; ex = r.randint(-20, 20)
            x = mktok(r.getrandbits(1), g.mant(), ex); y = mktok(r.getrandbits(1), g.mant(), ex - r.randint(61, 250))
        elif k < 0.94:   # zero operands (except x = -0 with y = +-0, handled separately)
            x = r.choice(["z0", "z0", "z1", signed(-10, 10)]); y = signed(-10, 10, 0.4)
            if x == "z1" and y[0] == "z": y = signed(-10, 10)
        else:            # simple values
            x = mktok(r.getrandbits(1), r.choice([1 << 63, 3 << 62]), r.randint(-3, 3))
            y = mktok(r.getrandbits(1), r.choice([1 << 63, 3 << 62]), r.randint(-3, 3))
        g.add("fpatan", "tr fpatan %d %s %s" % (rc, x, y))
    for rc in range(4):
        for ys in ("z0", "z1"):
            g.add("fpatan_negzero_corrected", "tr fpatan %d z1 %s" % (rc, ys))
    # fyl2x: b*log2(a), a>0
    for i in range(S(3000)):
        rc = g.rc(); k = r.random()
        if k < 0.70: a = mktok(0, g.mant(), r.randint(-30, 30))
        elif k < 0.80: a = mktok(0, g.mant(), r.randint(-16000, 16000))
        elif k < 0.95:  # near 1
            j = r.randint(2, 60); d = r.getrandbits(r.randint(1, 8)) + 1; sgn = r.choice([1, -1])
            a = f80tok_of(Fraction(1) + sgn * Fraction(d, 1 << j))
        else: a = mktok(0, r.choice([1 << 63, 3 << 62, M64]), r.randint(-3, 3))
        if a == mktok(0, 1 << 63, 0): a = mktok(0, 3 << 62, 0)
        b = signed(-30, 20, 0.02)
        g.add("fyl2x", "tr fyl2x %d %s %s" % (rc, a, b))
    # fyl2xp1: b*log2(a+1), a>-1
    for i in range(S(3000)):
        rc = g.rc(); k = r.random()
        if k < 0.55: a = signed(-30, 3)
        elif k < 0.70: a = signed(-200, -31)
        elif k < 0.80:  # close to -1
            j = r.randint(2, 60); d = r.getrandbits(r.randint(1, 8)) + 1
            a = f80tok_of(Fraction(-1) + Fraction(d, 1 << j))
        elif k < 0.90: a = mktok(r.getrandbits(1), g.mant(), r.randint(-1, 0)) if True else a
        else: a = signed(0, 5000)
        if a[0] == "n" and a[1] == "1" and parse_num(a)[2] >= 0: a = mktok(0, parse_num(a)[1], parse_num(a)[2])  # a <= -1 is invalid
        if a[0] == "n" and a[1] == "1" and a.endswith(":0") is False and parse_num(a)[2] == 0: pass
        g.add("fyl2xp1", "tr fyl2xp1 %d %s %s" % (rc, a, signed(-30, 20, 0.02)))
    # f2xm1
    def nonint(t):
        if t[0] != "n": return True
        s, m, e = parse_num(t)
        return not (e >= 63 or (e >= 0 and (m & ((1 << (63 - e)) - 1)) == 0))
    n = 0
    while n < S(3000):
        k = r.random()
        if k < 0.55: x = signed(-12, -1)
        elif k < 0.70: x = signed(-190, -13)
        elif k < 0.85: x = signed(0, 7)
        elif k < 0.93: x = signed(0, 3)
        else:
            x = signed(8, 19)
            if x[0] == "n" and x[1] == "1": x = x[:1] + "0" + x[2:]   # large negative x is outside x87.py's accuracy
        if not nonint(x): continue
        g.add("f2xm1", "tr f2xm1 %d %s -" % (g.rc(), x)); n += 1
    # trig
    def trig_arg():
        k = r.random()
        if k < 0.55: return signed(-30, 6, 0.01)
        if k < 0.70: return signed(-70, -31)
        if k < 0.80: return signed(7, 20)
        if k < 0.83: return signed(21, 40)
        # near a multiple of pi/2
        kk = r.randint(1, 5000); v = pi * kk / 2
        v += Fraction(r.randint(-3, 3), 1 << 62) * (1 if True else 1) * (1 if kk < 50 else 1)
        return f80tok_of(v * (1 if r.random() < 0.5 else -1))
    for name in ("fsin", "fcos", "fptan"):
        for i in range(S(3000)):
            g.add(name, "tr %s %d %s -" % (name, g.rc(), trig_arg()))
    # high-precision-oracle buckets: tiny operands (exercise the sign-of-deviation shortcuts) and huge trig args
    for name in ("fsin", "fcos", "fptan"):
        for i in range(S(1200)):
            m = r.choice([1 << 63, g.mant(), g.mant(), 3 << 62])
            g.add("hp_" + name + "_tiny", "tr %s %d %s -" % (name, g.rc(), mktok(r.getrandbits(1), m, r.randint(-390, -95))))
        for i in range(S(600)):
            g.add("hp_" + name + "_huge", "tr %s %d %s -" % (name, g.rc(), mktok(r.getrandbits(1), g.mant(), r.randint(41, 250))))
    for i in range(S(1200)):
        m = r.choice([1 << 63, g.mant(), g.mant()])
        g.add("hp_f2xm1_tiny", "tr f2xm1 %d %s -" % (g.rc(), mktok(r.getrandbits(1), m, r.randint(-390, -95))))
    for i in range(S(1200)):
        m = r.choice([1 << 63, g.mant(), g.mant()])
        g.add("hp_fyl2xp1_tiny", "tr fyl2xp1 %d %s %s" % (g.rc(), mktok(r.getrandbits(1), m, r.randint(-390, -95)), signed(-30, 20)))
    for i in range(S(1500)):
        ex = r.randint(-20, 20)
        xm = r.choice([1 << 63, g.mant(), g.mant()])
        x = mktok(r.getrandbits(1), xm, ex)
        y = mktok(r.getrandbits(1), r.choice([1 << 63, g.mant(), g.mant()]), ex - r.randint(95, 380))
        g.add("hp_fpatan_tiny", "tr fpatan %d %s %s" % (g.rc(), x, y))
    # exact-result buckets (x87.py evaluates at 90 digits, so exact cases are not guaranteed to match)
    for i in range(S(200)):
        k = r.choice([-1, 1]) * r.randint(1, 40)
        a = mktok(0, 1 << 63, k)
        g.add("exact_fyl2x_pow2", "tr fyl2x %d %s %s" % (g.rc(), a, signed(-30, 20)))
    for i in range(S(200)):
        k = r.randint(-8, 40)
        v = Fraction(2) ** k - 1
        if v == 0: continue
        g.add("exact_fyl2xp1_pow2", "tr fyl2xp1 %d %s %s" % (g.rc(), f80tok_of(v), signed(-30, 20)))
    for i in range(S(200)):
        nn = r.randint(-60, 60)
        if nn == 0: continue
        g.add("exact_f2xm1_int", "tr f2xm1 %d %s -" % (g.rc(), f80tok_of(Fraction(nn))))


def build_cases(seed, scale):
    g = Gen(seed)
    S = lambda n: max(1, int(n * scale))
    gen_arith(g, S(50000), S(40000), S(40000), S(25000))
    gen_misc(g, S(8000), S(12000), S(6000), S(14000))
    gen_conv(g, S(8000), S(8000), S(3000), S(3000), S(3000), S(4000), S(3000))
    gen_trans(g, scale)
    return g.cases


# ------------------------------------------------------------------ driver
def run_arch(arch, lines):
    binary = os.path.join(HERE, "f80_test_" + arch)
    cmd = [binary] if arch == "arm64" else ["arch", "-x86_64", binary]
    p = subprocess.run(cmd, input="\n".join(lines) + "\n", capture_output=True, text=True)
    if p.returncode != 0: raise RuntimeError("driver failed: %s %s" % (arch, p.stderr[:500]))
    out = p.stdout.split("\n")
    if out and out[-1] == "": out.pop()
    assert len(out) == len(lines), (len(out), len(lines))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=20260101)
    ap.add_argument("--scale", type=float, default=1.0)
    ap.add_argument("--archs", default="arm64,x86_64")
    ap.add_argument("--report", default=os.path.join(HERE, "report.json"))
    ap.add_argument("--cache", default="")
    args = ap.parse_args()
    t0 = time.time()
    cases = build_cases(args.seed, args.scale)
    print("cases:", len(cases), flush=True)
    exp = None
    if args.cache and os.path.exists(args.cache):
        exp = json.load(open(args.cache))
        if len(exp) != len(cases): exp = None
    if exp is None:
        with Pool() as pool:
            exp = pool.map(expected_safe, cases, chunksize=200)
        if args.cache: json.dump(exp, open(args.cache, "w"))
    print("expected computed in %.1fs" % (time.time() - t0), flush=True)
    py_dis = {}
    for i, (c, e) in enumerate(zip(cases, exp)):
        if "\t" in e:
            o, py = e.split("\t")
            exp[i] = o
            if o != py: py_dis[c[0]] = py_dis.get(c[0], 0) + 1
    lines = [c[1] for c in cases]
    report = {"seed": args.seed, "total_cases": len(cases), "categories": {}, "archs": args.archs.split(","),
              "total_mismatches": {}, "examples": {},
              "x87py_disagrees_with_exact_math": py_dis}
    cats = {}
    for c in cases: cats[c[0]] = cats.get(c[0], 0) + 1
    for cat, n in sorted(cats.items()): report["categories"][cat] = {"cases": n, "mismatches": {}}
    for arch in report["archs"]:
        t1 = time.time()
        got = run_arch(arch, lines)
        mism = {}
        ex = []
        for c, l, e, gt in zip(cases, lines, exp, got):
            if e != gt:
                mism[c[0]] = mism.get(c[0], 0) + 1
                if len(ex) < 40: ex.append({"cat": c[0], "case": l, "expected": e, "got": gt})
        for cat in cats: report["categories"][cat]["mismatches"][arch] = mism.get(cat, 0)
        report["total_mismatches"][arch] = sum(mism.values())
        report["examples"][arch] = ex
        print("%s: %d mismatches (%.1fs)" % (arch, sum(mism.values()), time.time() - t1), flush=True)
        for e in ex[:10]: print("   ", e)
    trans = sum(v["cases"] for k, v in report["categories"].items()
                if k.startswith(("f", "hp_", "exact_", "const")) and not k.startswith(("from_",)))
    report["transcendental_cases_total"] = trans
    report["notes"] = [
        "Categories exact_* : true result is exactly representable (2^k, integer log2, 2^n-1); expectation is an exact-rational oracle, "
        "x87py_disagrees_with_exact_math counts cases where x87.py's 90-digit evaluation itself differs from exact math (x87.py imprecision, not a C++ mismatch).",
        "Categories hp_* : expectation is x87.py's own formulas evaluated at 420 digits (x87.py at 90 digits is unreliable for tiny operands because of its absolute series tolerance).",
        "fpatan_negzero_corrected: x87.py returns an UNROUNDED pi for atan2(+-0, -0); expectation is pi rounded to 64 bits.",
        "to_f32/to_f64: overflow expectation is infinity (x87.py to_bits has a dead overflow branch).",
    ]
    json.dump(report, open(args.report, "w"), indent=1)
    print("transcendental cases total:", trans)
    for cat, v in sorted(report["categories"].items()):
        print("%-28s %7d  %s" % (cat, v["cases"], v["mismatches"]))
    return 0


def expected_safe(case):
    try:
        return expected(case)
    except Exception as ex:
        return "PYEXC:%s" % type(ex).__name__


if __name__ == "__main__":
    sys.exit(main())
