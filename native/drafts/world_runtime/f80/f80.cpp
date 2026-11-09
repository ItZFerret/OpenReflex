// See f80.h. Reference semantics: _re/trace/x87.py (and x86world.py for fscale / fprem).
#include "f80.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace reflex_f80 {
namespace {

typedef unsigned __int128 u128;

const int NEAREST = 0, DOWN = 1, UP = 2;
const int64_t EMIN_EXT = -16382;

inline bool iszero(const F80& a) { return a.kind == 0 && a.mant == 0; }
inline F80 mk(int kind, int sign) {
  F80 r;
  r.kind = (uint8_t)kind;
  r.sign = (uint8_t)sign;
  r.mant = 0;
  r.exp = 0;
  return r;
}
inline F80 mk_zero(int s) { return mk(0, s); }
// NaNs keep their significand in mant (bit 63 the integer bit, bit 62 the quiet bit, below it the payload), as the x87 holds them.
const uint64_t NAN_IBIT = 1ULL << 63, NAN_QBIT = 1ULL << 62, NAN_INDEFINITE = NAN_IBIT | NAN_QBIT;
// the default NaN of an invalid operation (the x87's "real indefinite": negative, significand 0xc000000000000000)
inline F80 mk_nan(int s) { F80 r = mk(2, s); r.mant = NAN_INDEFINITE; return r; }
// a NaN made quiet (an SNaN operand / source; a NaN built without a significand is the indefinite)
inline F80 quiet(F80 a) { a.mant |= NAN_INDEFINITE; return a; }
inline bool is_snan(const F80& a) { return a.kind == 2 && a.mant != 0 && !(a.mant & NAN_QBIT); }

inline int bl128(u128 x) {
  uint64_t hi = (uint64_t)(x >> 64);
  if (hi) return 128 - __builtin_clzll(hi);
  uint64_t lo = (uint64_t)x;
  return lo ? 64 - __builtin_clzll(lo) : 0;
}

// ---------------------------------------------------------------- rounding core
struct Rnd {
  u128 n;
  int64_t L;
};

// Round  (m + eps) * 2^e2  (eps in (0,1) iff sticky) to `bits` significand bits, with gradual
// underflow below 2^emin (mirrors x87.py round_fraction). Returns n * 2^L, n <= 2^bits.
// When sticky is set the caller guarantees at least 2 bits are dropped.
Rnd round_bits(u128 m, int64_t e2, bool sticky, int bits, int rc, int sign, int64_t emin) {
  if (m == 0) return Rnd{0, 0};
  int p = bl128(m) - 1;
  int64_t e = p + e2;
  int64_t ee = e > emin ? e : emin;
  int64_t L = ee - (bits - 1);
  int64_t shift = L - e2;
  if (shift <= 0) return Rnd{m << (-shift), L};
  u128 n;
  int cmp;
  bool inexact;
  if (shift > 128) {
    n = 0;
    cmp = -1;
    inexact = true;
  } else {
    u128 rem, half;
    if (shift == 128) {
      n = 0;
      rem = m;
      half = (u128)1 << 127;
    } else {
      n = m >> shift;
      rem = m & (((u128)1 << shift) - 1);
      half = (u128)1 << (shift - 1);
    }
    inexact = rem != 0 || sticky;
    if (rem > half) cmp = 1;
    else if (rem < half) cmp = -1;
    else cmp = sticky ? 1 : 0;
  }
  if (!inexact) return Rnd{n, L};
  bool inc = false;
  if (rc == NEAREST) inc = cmp > 0 || (cmp == 0 && (n & 1));
  else if (rc == UP) inc = !sign;
  else if (rc == DOWN) inc = sign != 0;
  if (inc) n += 1;
  return Rnd{n, L};
}

// value n * 2^L (n <= 2^64) -> normalized F80 (exact).
F80 make_exact(int sign, u128 n, int64_t L) {
  if (n == 0) return mk_zero(sign);
  int p = bl128(n) - 1;
  F80 r;
  r.kind = 0;
  r.sign = (uint8_t)sign;
  r.mant = p >= 63 ? (uint64_t)(n >> (p - 63)) : (uint64_t)(n << (63 - p));
  int64_t e = p + L;
  if (e > 0x3fffffff) e = 0x3fffffff;
  if (e < -0x3fffffff) e = -0x3fffffff;
  r.exp = (int32_t)e;
  return r;
}

F80 rounded(int sign, u128 m, int64_t e2, bool sticky, int bits, int rc) {
  Rnd r = round_bits(m, e2, sticky, bits, rc, sign, EMIN_EXT);
  return make_exact(sign, r.n, r.L);
}

// The NaN result of a two-operand x87 operation with a NaN operand (Intel SDM vol. 1 table 4-7, x87 column): one NaN: it, made quiet;
// an SNaN and a QNaN: the QNaN; two SNaNs or two QNaNs: the one with the larger significand (quieted); equal significands: the
// positive one (Bochs softfloat propagateFloatx80NaN, the same rule).
inline F80 nan_from(const F80& a, const F80& b) {
  if (a.kind != 2) return quiet(b);
  if (b.kind != 2) return quiet(a);
  const bool as = is_snan(a), bs = is_snan(b);
  const F80 qa = quiet(a), qb = quiet(b);
  if (as != bs) return as ? qb : qa;
  if (qa.mant != qb.mant) return qa.mant > qb.mant ? qa : qb;
  return qa.sign <= qb.sign ? qa : qb;
}

}  // namespace

// ---------------------------------------------------------------- constructors
F80 from_f32(uint32_t bits) {
  int sign = bits >> 31, ex = (bits >> 23) & 0xff;
  uint32_t frac = bits & 0x7fffff;
  if (ex == 0xff) {   // fld m32 of a NaN: the payload kept, an SNaN made quiet
    F80 r = mk(frac ? 2 : 1, sign);
    if (frac) r.mant = NAN_INDEFINITE | ((uint64_t)frac << 40);
    return r;
  }
  if (ex == 0) return make_exact(sign, frac, -149);
  return make_exact(sign, (1u << 23) | frac, ex - 150);
}

F80 from_f64(uint64_t bits) {
  int sign = (int)(bits >> 63), ex = (int)((bits >> 52) & 0x7ff);
  uint64_t frac = bits & ((1ULL << 52) - 1);
  if (ex == 0x7ff) {   // fld m64 of a NaN: the payload kept, an SNaN made quiet
    F80 r = mk(frac ? 2 : 1, sign);
    if (frac) r.mant = NAN_INDEFINITE | (frac << 11);
    return r;
  }
  if (ex == 0) return make_exact(sign, frac, -1074);
  return make_exact(sign, (1ULL << 52) | frac, ex - 1075);
}

F80 from_f80(const uint8_t raw[10]) {
  uint64_t mant = 0;
  for (int i = 7; i >= 0; i--) mant = (mant << 8) | raw[i];
  int se = raw[8] | (raw[9] << 8);
  int sign = se >> 15, ex = se & 0x7fff;
  if (ex == 0x7fff) {   // fld m80 of a NaN: the significand as it is (an SNaN stays signalling)
    F80 r = mk((mant << 1) ? 2 : 1, sign);
    if (mant << 1) r.mant = mant;
    return r;
  }
  if (ex == 0 && mant == 0) return mk_zero(sign);
  return make_exact(sign, mant, (int64_t)(ex > 1 ? ex : 1) - 16383 - 63);
}

F80 from_int(int64_t v) {
  uint64_t m = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
  return make_exact(v < 0, m, 0);
}

// ---------------------------------------------------------------- conversions out
namespace {
uint64_t to_bits(const F80& a, int mbits, int ebits, int rc) {
  int width = mbits + ebits;
  int bias = (1 << (ebits - 1)) - 1;
  uint64_t sign = (uint64_t)a.sign << (width - 1);
  if (a.kind == 2) {   // fstp of a NaN: the significand's top bits below the integer bit, the quiet bit set (an SNaN made quiet)
    const uint64_t m = a.mant ? a.mant : NAN_INDEFINITE;
    const uint64_t frac = ((m >> (64 - mbits)) & ((1ULL << (mbits - 1)) - 1)) | (1ULL << (mbits - 2));
    return sign | (((1ULL << ebits) - 1) << (mbits - 1)) | frac;
  }
  uint64_t infbits = sign | (((1ULL << ebits) - 1) << (mbits - 1));
  if (a.kind == 1) return infbits;
  if (a.mant == 0) return sign;
  Rnd r = round_bits(a.mant, (int64_t)a.exp - 63, false, mbits, rc, a.sign, 1 - bias);
  if (r.n == 0) return sign;
  u128 n = r.n;
  int64_t L = r.L;
  if (bl128(n) <= mbits - 1) return sign | (uint64_t)n;  // subnormal: n * 2^(emin-mbits+1)
  if (n == ((u128)1 << mbits)) {
    n >>= 1;
    L += 1;
  }
  int64_t e = L + mbits - 1;
  if (e > bias) return infbits;  // overflow -> inf (x87.py never produces this itself, see notes)
  uint64_t frac = (uint64_t)n - (1ULL << (mbits - 1));
  return sign | ((uint64_t)(e + bias) << (mbits - 1)) | frac;
}
}  // namespace

uint32_t to_f32(const F80& a, int rc) { return (uint32_t)to_bits(a, 24, 8, rc); }
uint64_t to_f64(const F80& a, int rc) { return to_bits(a, 53, 11, rc); }

void to_f80(const F80& a, uint8_t out[10]) {
  uint64_t mant;
  uint32_t se = (uint32_t)a.sign << 15;
  if (a.kind == 2) {
    mant = a.mant ? a.mant : NAN_INDEFINITE;
    se |= 0x7fff;
  } else if (a.kind == 1) {
    mant = 1ULL << 63;
    se |= 0x7fff;
  } else if (a.mant == 0) {
    mant = 0;
  } else {
    int64_t biased = (int64_t)a.exp + 16383;
    if (biased >= 1) {
      mant = a.mant;
      se |= (uint32_t)(biased & 0x7fff);  // out-of-range exponents wrap (x87.py would emit garbage)
    } else {  // denormal encoding (x87.py cannot represent these: raises); truncating
      int64_t sh = 1 - biased;
      mant = sh >= 64 ? 0 : a.mant >> sh;
    }
  }
  for (int i = 0; i < 8; i++) out[i] = (uint8_t)(mant >> (8 * i));
  out[8] = (uint8_t)se;
  out[9] = (uint8_t)(se >> 8);
}

// ---------------------------------------------------------------- arithmetic
F80 add(const F80& a, const F80& b, int bits, int rc, bool negate_b) {
  if (a.kind == 2 || b.kind == 2) return nan_from(a, b);
  int bsign = b.sign ^ (negate_b ? 1 : 0);
  if (a.kind == 1 || b.kind == 1) {
    if (a.kind == 1 && b.kind == 1 && a.sign != bsign) return mk_nan(1);
    return mk(1, a.kind == 1 ? a.sign : bsign);
  }
  bool az = a.mant == 0, bz = b.mant == 0;
  if (az && bz) {
    if (a.sign == bsign) return mk_zero(a.sign);
    return mk_zero(rc == DOWN ? 1 : 0);
  }
  if (az) return rounded(bsign, b.mant, (int64_t)b.exp - 63, false, bits, rc);
  if (bz) return rounded(a.sign, a.mant, (int64_t)a.exp - 63, false, bits, rc);
  // pick the larger magnitude as "big"
  bool a_big = a.exp > b.exp || (a.exp == b.exp && a.mant >= b.mant);
  const F80& big = a_big ? a : b;
  const F80& small = a_big ? b : a;
  int sbig = a_big ? a.sign : bsign, ssmall = a_big ? bsign : a.sign;
  int64_t d = (int64_t)big.exp - small.exp;
  u128 X = (u128)big.mant << 62;
  u128 Ys = (u128)small.mant << 62;
  u128 Y;
  bool st;
  if (d >= 128) {
    Y = 0;
    st = true;
  } else {
    Y = Ys >> d;
    st = d > 0 && (Ys & (((u128)1 << d) - 1)) != 0;
  }
  u128 m;
  if (sbig == ssmall) {
    m = X + Y;
  } else {
    m = X - Y;
    if (st) m -= 1;
  }
  if (m == 0 && !st) return mk_zero(rc == DOWN ? 1 : 0);
  return rounded(sbig, m, (int64_t)big.exp - 125, st, bits, rc);
}

F80 mul(const F80& a, const F80& b, int bits, int rc) {
  if (a.kind == 2 || b.kind == 2) return nan_from(a, b);
  int sign = a.sign ^ b.sign;
  if (a.kind == 1 || b.kind == 1) {
    if (iszero(a) || iszero(b)) return mk_nan(1);
    return mk(1, sign);
  }
  if (a.mant == 0 || b.mant == 0) return mk_zero(sign);
  return rounded(sign, (u128)a.mant * b.mant, (int64_t)a.exp + b.exp - 126, false, bits, rc);
}

namespace {
// ma/mb (both normalized 64-bit) = (Q + eps) * 2^-95, eps>0 iff st.
void divcore(uint64_t ma, uint64_t mb, u128& Q, bool& st) {
  u128 N = (u128)ma << 63;
  u128 q1 = N / mb;
  uint64_t r1 = (uint64_t)(N % mb);
  u128 N2 = (u128)r1 << 32;
  u128 q2 = N2 / mb;
  uint64_t r2 = (uint64_t)(N2 % mb);
  Q = (q1 << 32) | q2;
  st = r2 != 0;
}
}  // namespace

F80 div(const F80& a, const F80& b, int bits, int rc) {
  if (a.kind == 2 || b.kind == 2) return nan_from(a, b);
  int sign = a.sign ^ b.sign;
  if (a.kind == 1) return b.kind == 1 ? mk_nan(1) : mk(1, sign);
  if (b.kind == 1) return mk_zero(sign);
  if (b.mant == 0) return a.mant == 0 ? mk_nan(1) : mk(1, sign);
  if (a.mant == 0) return mk_zero(sign);
  u128 Q;
  bool st;
  divcore(a.mant, b.mant, Q, st);
  return rounded(sign, Q, (int64_t)a.exp - b.exp - 95, st, bits, rc);
}

F80 sqrt(const F80& a, int bits, int rc) {
  if (a.kind == 2) return quiet(a);
  if (iszero(a)) return a;
  if (a.sign) return mk_nan(1);
  if (a.kind == 1) return a;
  int64_t E = (int64_t)a.exp - 63;
  u128 M = a.mant;
  if (E & 1) {
    M <<= 1;
    E -= 1;
  }
  u128 P = M << 62;  // radicand R = M << 126 as 3x64-bit limbs
  uint64_t limb[3] = {0, (uint64_t)P, (uint64_t)(P >> 64)};
  u128 rem = 0, root = 0;
  for (int i = 95; i >= 0; i--) {
    int pos = 2 * i;
    unsigned two = (unsigned)((limb[pos >> 6] >> (pos & 63)) & 3);
    rem = (rem << 2) | two;
    u128 trial = (root << 2) | 1;
    if (rem >= trial) {
      rem -= trial;
      root = (root << 1) | 1;
    } else {
      root <<= 1;
    }
  }
  return rounded(0, root, (E - 126) / 2, rem != 0, bits, rc);
}

int compare(const F80& a, const F80& b) {
  if (a.kind == 2 || b.kind == 2) return 2;
  auto key = [](const F80& f) { return f.kind == 1 ? (f.sign ? -2 : 2) : 0; };
  int ka = key(a), kb = key(b);
  if (ka != 0 || kb != 0) {
    if (ka == kb) return 0;
    return ka > kb ? 1 : -1;
  }
  auto sg = [](const F80& f) { return f.mant == 0 ? 0 : (f.sign ? -1 : 1); };
  int sa = sg(a), sb = sg(b);
  if (sa != sb) return sa > sb ? 1 : -1;
  if (sa == 0) return 0;
  int c;
  if (a.exp != b.exp) c = a.exp > b.exp ? 1 : -1;
  else if (a.mant != b.mant) c = a.mant > b.mant ? 1 : -1;
  else c = 0;
  return sa < 0 ? -c : c;
}

F80 round_to_integer(const F80& a, int rc) {
  if (a.kind == 2) return quiet(a);
  if (a.kind != 0 || a.mant == 0) return a;
  int64_t shift = 63 - (int64_t)a.exp;
  if (shift <= 0) return a;
  if (shift > 65) shift = 65;
  u128 m = a.mant;
  u128 ip = m >> shift;
  u128 rem = m & (((u128)1 << shift) - 1);
  u128 half = (u128)1 << (shift - 1);
  bool inc = false;
  if (rem) {
    if (rc == NEAREST) inc = rem > half || (rem == half && (ip & 1));
    else if (rc == DOWN) inc = a.sign != 0;
    else if (rc == UP) inc = !a.sign;
  }
  return make_exact(a.sign, ip + (inc ? 1 : 0), 0);
}

F80 scale(const F80& a, int64_t n) {
  if (a.kind == 2) return quiet(a);
  if (a.kind != 0 || a.mant == 0) return a;  // x86world.py raises for inf/nan; returned unchanged here
  int64_t e = (int64_t)a.exp + (n > (1LL << 40) ? (1LL << 40) : n < -(1LL << 40) ? -(1LL << 40) : n);
  if (e > 0x3fffffff) e = 0x3fffffff;
  if (e < -0x3fffffff) e = -0x3fffffff;
  F80 r = a;
  r.exp = (int32_t)e;
  return r;
}

F80 remainder(const F80& x, const F80& y, bool nearest, int* quotient_low3) {
  if (quotient_low3) *quotient_low3 = 0;
  if (x.kind == 2 || y.kind == 2) return nan_from(x, y);
  if (x.kind != 0 || y.kind != 0 || iszero(y)) return mk_nan(1);
  if (iszero(x)) return mk_zero(x.sign);
  uint64_t mx = x.mant, my = y.mant;
  int64_t d = (int64_t)x.exp - y.exp;
  uint64_t q = 0;
  u128 r;
  int64_t unit;  // result magnitude = r * 2^unit
  bool flip = false;
  if (d >= 0) {
    uint64_t rr = mx % my;
    q = mx / my;
    int64_t rem = d;
    while (rem > 0) {
      int k = rem > 60 ? 60 : (int)rem;
      u128 t = (u128)rr << k;
      q = (q << k) + (uint64_t)(t / my);
      rr = (uint64_t)(t % my);
      rem -= k;
    }
    r = rr;
    unit = (int64_t)y.exp - 63;
    if (nearest) {
      u128 tr = (u128)rr * 2;
      if (tr > my || (tr == my && (q & 1))) {
        q += 1;
        r = (u128)my - rr;
        flip = true;
      }
    }
  } else {
    r = mx;
    unit = (int64_t)x.exp - 63;
    int64_t nd = -d;
    if (nearest && nd <= 3) {
      u128 Y = (u128)my << nd;
      if ((u128)mx * 2 > Y) {
        q = 1;
        r = Y - mx;
        flip = true;
      }
    }
  }
  if (quotient_low3) *quotient_low3 = (int)(q & 7);
  return make_exact(x.sign ^ (flip ? 1 : 0), r, unit);
}

// ================================================================ transcendentals
namespace {

typedef std::vector<uint32_t> Big;  // little-endian magnitude, no leading zero limbs

inline void btrim(Big& a) {
  while (!a.empty() && a.back() == 0) a.pop_back();
}
inline int bbl(const Big& a) { return a.empty() ? 0 : 32 * (int)(a.size() - 1) + (32 - __builtin_clz(a.back())); }
Big bfrom(u128 v) {
  Big r;
  while (v) {
    r.push_back((uint32_t)v);
    v >>= 32;
  }
  return r;
}
u128 bto128(const Big& a) {
  u128 v = 0;
  for (size_t i = std::min<size_t>(a.size(), 4); i-- > 0;) v = (v << 32) | a[i];
  return v;
}
int bcmp(const Big& a, const Big& b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  for (size_t i = a.size(); i-- > 0;)
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  return 0;
}
Big badd(const Big& a, const Big& b) {
  const Big& x = a.size() >= b.size() ? a : b;
  const Big& y = a.size() >= b.size() ? b : a;
  Big r(x.size() + 1);
  uint64_t c = 0;
  for (size_t i = 0; i < x.size(); i++) {
    c += (uint64_t)x[i] + (i < y.size() ? y[i] : 0);
    r[i] = (uint32_t)c;
    c >>= 32;
  }
  r[x.size()] = (uint32_t)c;
  btrim(r);
  return r;
}
Big bsub(const Big& a, const Big& b) {  // a >= b
  Big r(a.size());
  int64_t br = 0;
  for (size_t i = 0; i < a.size(); i++) {
    int64_t d = (int64_t)a[i] - (i < b.size() ? b[i] : 0) - br;
    if (d < 0) {
      d += (1LL << 32);
      br = 1;
    } else {
      br = 0;
    }
    r[i] = (uint32_t)d;
  }
  btrim(r);
  return r;
}
Big bshl(const Big& a, int64_t n) {
  if (a.empty() || n <= 0) return a;
  size_t w = (size_t)(n / 32);
  int b = (int)(n % 32);
  Big r(a.size() + w + 1, 0);
  for (size_t i = 0; i < a.size(); i++) {
    uint64_t v = (uint64_t)a[i] << b;
    r[i + w] |= (uint32_t)v;
    r[i + w + 1] |= (uint32_t)(v >> 32);
  }
  btrim(r);
  return r;
}
Big bshr(const Big& a, int64_t n) {
  if (n <= 0) return a;
  size_t w = (size_t)(n / 32);
  int b = (int)(n % 32);
  if (w >= a.size()) return Big();
  Big r(a.size() - w);
  for (size_t i = w; i < a.size(); i++) {
    uint64_t v = a[i];
    if (i + 1 < a.size()) v |= (uint64_t)a[i + 1] << 32;
    r[i - w] = (uint32_t)(v >> b);
  }
  btrim(r);
  return r;
}
Big bmul(const Big& a, const Big& b) {
  if (a.empty() || b.empty()) return Big();
  Big r(a.size() + b.size(), 0);
  for (size_t i = 0; i < a.size(); i++) {
    uint64_t c = 0, ai = a[i];
    for (size_t j = 0; j < b.size(); j++) {
      c += ai * b[j] + r[i + j];
      r[i + j] = (uint32_t)c;
      c >>= 32;
    }
    r[i + b.size()] = (uint32_t)c;
  }
  btrim(r);
  return r;
}
void bmul32(Big& a, uint32_t m) {
  uint64_t c = 0;
  for (auto& x : a) {
    c += (uint64_t)x * m;
    x = (uint32_t)c;
    c >>= 32;
  }
  if (c) a.push_back((uint32_t)c);
  btrim(a);
}
uint32_t bdiv32(Big& a, uint32_t d) {
  uint64_t r = 0;
  for (size_t i = a.size(); i-- > 0;) {
    uint64_t cur = (r << 32) | a[i];
    a[i] = (uint32_t)(cur / d);
    r = cur % d;
  }
  btrim(a);
  return (uint32_t)r;
}
// floor(a / b) (b nonzero), remainder optionally returned: Knuth's algorithm D on 32-bit limbs.
Big bdiv(const Big& a, const Big& b, Big* remout) {
  if (bcmp(a, b) < 0) {
    if (remout) *remout = a;
    return Big();
  }
  if (b.size() == 1) {
    Big q = a;
    uint32_t r = bdiv32(q, b[0]);
    if (remout) *remout = r ? Big{r} : Big();
    return q;
  }
  const int s = __builtin_clz(b.back());
  Big v = bshl(b, s), u = bshl(a, s);
  const size_t n = v.size(), m = a.size() - n;
  u.resize(a.size() + 1, 0);
  Big q(m + 1, 0);
  const uint64_t B = 1ull << 32;
  for (size_t j = m + 1; j-- > 0;) {
    uint64_t num = ((uint64_t)u[j + n] << 32) | u[j + n - 1];
    uint64_t qhat = num / v[n - 1], rhat = num - qhat * v[n - 1];
    while (qhat >= B || qhat * v[n - 2] > ((rhat << 32) | u[j + n - 2])) {
      qhat--;
      rhat += v[n - 1];
      if (rhat >= B) break;
    }
    int64_t k = 0, t;
    for (size_t i = 0; i < n; i++) {
      uint64_t p = qhat * v[i];
      t = (int64_t)u[i + j] - k - (int64_t)(p & 0xffffffffu);
      u[i + j] = (uint32_t)t;
      k = (int64_t)(p >> 32) - (t >> 32);
    }
    t = (int64_t)u[j + n] - k;
    u[j + n] = (uint32_t)t;
    if (t < 0) {  // qhat was one too large: add v back
      qhat--;
      uint64_t c = 0;
      for (size_t i = 0; i < n; i++) {
        uint64_t sum = (uint64_t)u[i + j] + v[i] + c;
        u[i + j] = (uint32_t)sum;
        c = sum >> 32;
      }
      u[j + n] += (uint32_t)c;
    }
    q[j] = (uint32_t)qhat;
  }
  btrim(q);
  if (remout) {
    u.resize(n);
    btrim(u);
    *remout = bshr(u, s);
  }
  return q;
}

// ---- fixed point with g_P fractional bits
const int P_DEF = 576;  // working precision (>= 500 usable bits)
const int PC = 1088;    // constants / argument-reduction precision
thread_local int g_P = P_DEF;
// Fast attempt (Ziv): the same computation at P_FAST bits, accepted only when every value within 2^g_check_bits units
// of the approximation's last bit rounds alike (the approximation's error bound is far below that radius), so the result
// equals the P_DEF result; otherwise the caller recomputes at P_DEF.
const int P_FAST = 256;
thread_local int g_Pw = P_DEF;
thread_local int g_check_bits = 0;
thread_local bool g_amb = false;

struct SF {
  bool neg;
  Big m;
};
Big fone() { return bshl(bfrom(1), g_P); }
Big fmulb(const Big& a, const Big& b) { return bshr(bmul(a, b), g_P); }
Big fdivb(const Big& a, const Big& b) { return bdiv(bshl(a, g_P), b, nullptr); }
SF sfadd(const SF& a, const SF& b) {
  if (a.neg == b.neg) return SF{a.neg, badd(a.m, b.m)};
  int c = bcmp(a.m, b.m);
  if (c == 0) return SF{false, Big()};
  if (c > 0) return SF{a.neg, bsub(a.m, b.m)};
  return SF{b.neg, bsub(b.m, a.m)};
}
SF sfneg(SF a) {
  a.neg = !a.neg;
  return a;
}

// atan(t), 0 < t <= 1 (Euler series, ratio <= 1/2)
Big fatan_pos(const Big& t) {
  Big one = fone();
  Big t2 = fmulb(t, t);
  Big den = badd(one, t2);
  Big w = fdivb(t2, den);
  Big term = fdivb(t, den);
  Big sum = term;
  for (uint32_t n = 1;; n++) {
    term = fmulb(term, w);
    bmul32(term, 2 * n);
    bdiv32(term, 2 * n + 1);
    if (term.empty()) break;
    sum = badd(sum, term);
  }
  return sum;
}
// atanh(z), 0 <= z < 1/2
Big fatanh_pos(const Big& z) {
  Big z2 = fmulb(z, z);
  Big pw = z, sum = z;
  for (uint32_t k = 1;; k++) {
    pw = fmulb(pw, z2);
    if (pw.empty()) break;
    Big t = pw;
    bdiv32(t, 2 * k + 1);
    sum = badd(sum, t);
  }
  return sum;
}

struct Consts {
  Big pi, ln2, l2e;
};
const Consts& consts() {
  static const Consts c = [] {
    int save = g_P;
    g_P = PC;
    Consts k;
    Big one = fone();
    k.pi = fatan_pos(one);
    bmul32(k.pi, 4);
    Big third = bdiv(one, bfrom(3), nullptr);
    k.ln2 = fatanh_pos(third);
    bmul32(k.ln2, 2);
    k.l2e = fdivb(one, k.ln2);
    g_P = save;
    return k;
  }();
  return c;
}
Big cst(const Big& c) { return bshr(c, PC - g_P); }

// round an approximate fixed value  M * 2^e2 (nonzero) to `bits`
F80 round_big(int sign, const Big& M, int64_t e2, int bits, int rc) {
  if (g_check_bits) {
    int cb = g_check_bits;
    g_check_bits = 0;
    Big E = bshl(bfrom(1), cb);
    F80 lo, hi;
    if (bcmp(M, E) <= 0) {
      g_amb = true;
      lo = hi = round_big(sign, M, e2, bits, rc);
    } else {
      lo = round_big(sign, bsub(M, E), e2, bits, rc);
      hi = round_big(sign, badd(M, E), e2, bits, rc);
    }
    g_check_bits = cb;
    if (lo.kind != hi.kind || lo.sign != hi.sign || lo.mant != hi.mant || lo.exp != hi.exp) g_amb = true;
    return lo;
  }
  int bl = bbl(M);
  if (bl == 0) return mk_zero(sign);
  u128 m;
  if (bl > 126) {
    int sh = bl - 126;
    m = bto128(bshr(M, sh));
    e2 += sh;
  } else {
    m = bto128(M) << (126 - bl);
    e2 -= 126 - bl;
  }
  return rounded(sign, m, e2, true, bits, rc);
}
F80 round_sf(const SF& v, int rc) { return round_big(v.neg, v.m, -(int64_t)g_P, 64, rc); }

// log2(X * 2^-s), X > 0 not a power of two (or any positive value; exactness handled by caller)
SF log2fix(const Big& X, int64_t s) {
  int bl = bbl(X);
  int64_t e = (int64_t)bl - 1 - s;
  Big F = (bl - 1 >= g_P) ? bshr(X, bl - 1 - g_P) : bshl(X, g_P - (bl - 1));
  Big thr = bshl(bfrom(0x16A09E667F3BCDULL), g_P - 52);  // ~sqrt(2)
  if (bcmp(F, thr) > 0) {
    F = bshr(F, 1);
    e += 1;
  }
  Big one = fone();
  bool zneg = bcmp(F, one) < 0;
  Big num = zneg ? bsub(one, F) : bsub(F, one);
  Big z = fdivb(num, badd(F, one));
  Big ah = fatanh_pos(z);
  bmul32(ah, 2);  // |ln f|
  SF l2{zneg, fmulb(ah, cst(consts().l2e))};
  SF ev{e < 0, bshl(bfrom((u128)(e < 0 ? -e : e)), g_P)};
  return sfadd(ev, l2);
}

// expm1(z) for |z| <= ~0.36
SF fexpm1(SF z) {
  const int S = 6;
  z.m = bshr(z.m, S);
  SF term = z, sum = z;
  for (uint32_t k = 2;; k++) {
    term.m = fmulb(term.m, z.m);
    term.neg = term.neg != z.neg;
    bdiv32(term.m, k);
    if (term.m.empty()) break;
    sum = sfadd(sum, term);
  }
  for (int i = 0; i < S; i++) {
    SF dbl{sum.neg, bshl(sum.m, 1)};
    SF sq{false, fmulb(sum.m, sum.m)};
    sum = sfadd(dbl, sq);
  }
  return sum;
}

F80 pi_rounded(int sign, int rc) {
  g_P = P_DEF;
  return round_big(sign, cst(consts().pi), -(int64_t)g_P, 64, rc);
}

inline F80 NAN1() { return mk_nan(1); }

// x*k for integer k (exact), x finite nonzero
F80 mul_int_exact(const F80& y, int64_t k, int rc) {
  uint64_t ak = k < 0 ? (uint64_t)0 - (uint64_t)k : (uint64_t)k;
  return rounded(y.sign ^ (k < 0 ? 1 : 0), (u128)y.mant * ak, (int64_t)y.exp - 63, false, 64, rc);
}
// y * L  (L signed fixed, nonzero), approximate
F80 mul_y_fixed(const F80& y, const SF& L, int rc) {
  Big M = bmul(bfrom(y.mant), L.m);
  return round_big(y.sign ^ (L.neg ? 1 : 0), M, (int64_t)y.exp - 63 - g_P, 64, rc);
}

F80 t_fyl2x(const F80& a, const F80& b, int rc, int* err) {
  if (a.kind != 0 || b.kind != 0 || a.sign || a.mant == 0) {
    *err = 1;
    return NAN1();
  }
  bool one = a.mant == (1ULL << 63) && a.exp == 0;
  if (one) return mk_zero(a.sign ^ b.sign);
  if (b.mant == 0) return mk_zero(0);
  if (a.mant == (1ULL << 63)) return mul_int_exact(b, a.exp, rc);
  g_P = g_Pw;
  SF L = log2fix(bfrom(a.mant), 63 - (int64_t)a.exp);
  return mul_y_fixed(b, L, rc);
}

F80 t_fyl2xp1(const F80& a, const F80& b, int rc, int* err) {
  if (a.kind != 0 || b.kind != 0 || (a.sign && a.mant != 0 && a.exp >= 0)) {
    *err = 1;
    return NAN1();
  }
  if (a.mant == 0 || b.mant == 0) return mk_zero(a.sign ^ b.sign);
  g_P = P_DEF;
  if (a.exp < -100) {  // log2(1+a) = a/ln2 * (1 - a/2 + ...); correction never decides rounding
    Big M = bmul(bmul(bfrom(a.mant), bfrom(b.mant)), cst(consts().l2e));
    return round_big(a.sign ^ b.sign, M, (int64_t)a.exp - 63 + (int64_t)b.exp - 63 - g_P, 64, rc);
  }
  Big X;
  int64_t s = 0;
  bool have_x = true;
  if (a.exp > 700) {
    X = bfrom(a.mant);
    s = 63 - (int64_t)a.exp;
    have_x = false;
  } else if (a.exp >= 63) {
    X = badd(bshl(bfrom(a.mant), (int64_t)a.exp - 63), bfrom(1));
  } else {
    int64_t sh = 63 - (int64_t)a.exp;  // a = +-mant / 2^sh
    Big one = bshl(bfrom(1), sh);
    X = a.sign ? bsub(one, bfrom(a.mant)) : badd(one, bfrom(a.mant));
    s = sh;
  }
  if (have_x) {
    int pc = 0;
    for (uint32_t w : X) pc += __builtin_popcount(w);
    if (pc == 1) return mul_int_exact(b, (int64_t)bbl(X) - 1 - s, rc);  // log2(1+a) integer
  }
  SF L;
  if (a.exp <= -2) {  // |a| < 1/2: ln(1+a) = 2 atanh(a/(2+a))
    Big A = bshl(bfrom(a.mant), g_P + (int64_t)a.exp - 63);
    Big two = bshl(fone(), 1);
    Big den = a.sign ? bsub(two, A) : badd(two, A);
    Big z = fdivb(A, den);
    Big ah = fatanh_pos(z);
    bmul32(ah, 2);
    L = SF{(bool)a.sign, fmulb(ah, cst(consts().l2e))};
  } else {
    L = log2fix(X, s);
  }
  return mul_y_fixed(b, L, rc);
}

F80 t_f2xm1(const F80& a, int rc, int* err) {
  if (a.kind != 0) {
    *err = 1;
    return NAN1();
  }
  if (a.mant == 0) return mk_zero(a.sign);
  g_P = P_DEF;
  if (a.exp < -100) {  // x*ln2 (1 + x ln2/2 + ...): correction never decides rounding
    Big M = bmul(bfrom(a.mant), cst(consts().ln2));
    return round_big(a.sign, M, (int64_t)a.exp - 63 - g_P, 64, rc);
  }
  if (a.exp > 28) {
    *err = 2;
    return NAN1();
  }
  // integer argument -> exact 2^n - 1
  bool isint = a.exp >= 63 || (a.exp >= 0 && (a.mant & ((1ULL << (63 - a.exp)) - 1)) == 0);
  if (isint) {
    int64_t N = (int64_t)(a.mant >> (63 - a.exp));
    const u128 ALL = ~(u128)0;
    if (!a.sign) {
      if (N <= 128) return rounded(0, N == 128 ? ALL : (((u128)1 << N) - 1), 0, false, 64, rc);
      return rounded(0, ALL, N - 128, true, 64, rc);
    }
    if (N <= 128) return rounded(1, N == 128 ? ALL : (((u128)1 << N) - 1), -N, false, 64, rc);
    return rounded(1, ALL, -128, true, 64, rc);
  }
  Big Xa = bshl(bfrom(a.mant), g_P + (int64_t)a.exp - 63);  // |x| fixed
  Big half = bshr(fone(), 1);
  Big Nb = bshr(badd(Xa, half), g_P);  // floor(|x| + 1/2)
  int64_t N = (int64_t)bto128(Nb);
  Big Nfix = bshl(Nb, g_P);
  SF f = bcmp(Xa, Nfix) >= 0 ? SF{false, bsub(Xa, Nfix)} : SF{true, bsub(Nfix, Xa)};
  if (a.sign) f.neg = !f.neg;
  int64_t n = a.sign ? -N : N;
  SF z{f.neg, fmulb(f.m, cst(consts().ln2))};
  SF E = fexpm1(z);
  Big one = fone();
  if (n == 0) return round_sf(E, rc);
  SF opE = sfadd(SF{false, one}, E);  // 2^f, positive
  if (n > 0) {
    if (n >= 400) return round_big(0, opE.m, n - (int64_t)g_P, 64, rc);
    SF V = sfadd(SF{false, bshl(opE.m, n)}, SF{true, one});
    return round_sf(V, rc);
  }
  if (-n >= 400) return rounded(1, ~(u128)0, -128, true, 64, rc);
  SF V = sfadd(SF{false, bshr(opE.m, -n)}, SF{true, one});
  return round_sf(V, rc);
}

// sin and cos of x (finite, |x| >= 2^-100 in magnitude, < 2^255). Signed fixed results.
bool sincos_fixed(const F80& x, SF& S, SF& C) {
  if (x.exp >= 255) return false;
  Big Xp = bshl(bfrom(x.mant), (int64_t)x.exp - 63 + PC);
  Big hp = bshr(consts().pi, 1);
  Big h2 = bshr(hp, 1);
  Big rem;
  Big K = bdiv(badd(Xp, h2), hp, &rem);
  SF R = bcmp(rem, h2) >= 0 ? SF{false, bsub(rem, h2)} : SF{true, bsub(h2, rem)};
  Big Rm = bshr(R.m, PC - g_P);
  Big R2 = fmulb(Rm, Rm);
  Big one = fone();
  Big term = Rm, sp = Rm, sn;
  for (uint32_t k = 1;; k++) {
    term = fmulb(term, R2);
    bdiv32(term, (2 * k) * (2 * k + 1));
    if (term.empty()) break;
    if (k & 1) sn = badd(sn, term);
    else sp = badd(sp, term);
  }
  Big sinv = bsub(sp, sn);
  term = one;
  Big cp = one, cn;
  for (uint32_t k = 1;; k++) {
    term = fmulb(term, R2);
    bdiv32(term, (2 * k - 1) * (2 * k));
    if (term.empty()) break;
    if (k & 1) cn = badd(cn, term);
    else cp = badd(cp, term);
  }
  Big cosv = bsub(cp, cn);
  SF s{R.neg, sinv}, c{false, cosv};
  int k = K.empty() ? 0 : (int)(K[0] & 3);
  switch (k) {
    case 0: S = s; C = c; break;
    case 1: S = c; C = sfneg(s); break;
    case 2: S = sfneg(s); C = sfneg(c); break;
    default: S = sfneg(c); C = s; break;
  }
  if (x.sign) S = sfneg(S);
  return true;
}

F80 t_trig(const char* op, const F80& a, int rc, int* err) {
  if (a.kind != 0) {
    *err = 1;
    return NAN1();
  }
  bool isc = !strcmp(op, "fcos"), ist = !strcmp(op, "fptan");
  if (a.mant == 0) return isc ? rounded(0, 1, 0, false, 64, rc) : mk_zero(a.sign);
  if (a.exp < -100) {
    if (isc) return rounded(0, ((u128)1 << 100) - 1, -100, true, 64, rc);
    u128 m = (u128)a.mant << 64;
    if (!ist) m -= 1;  // sin x = x (1 - x^2/6 ...) strictly below x
    return rounded(a.sign, m, (int64_t)a.exp - 127, true, 64, rc);
  }
  g_P = P_DEF;
  SF S, C;
  if (!sincos_fixed(a, S, C)) {
    *err = 2;
    return NAN1();
  }
  if (isc) return round_sf(C, rc);
  if (!ist) return round_sf(S, rc);
  SF T{S.neg != C.neg, fdivb(S.m, C.m)};
  return round_sf(T, rc);
}

F80 t_fpatan(const F80& a, const F80& b, int rc, int* err) {  // atan2(y=b, x=a)
  if (a.kind != 0 || b.kind != 0) {
    *err = 1;
    return NAN1();
  }
  bool xz = a.mant == 0, yz = b.mant == 0;
  if (yz) {
    // x86 semantic atan2(+-0, -0) = +-pi. x87.py returns an UNROUNDED pi for that one case
    // (F80.num(Fraction(_pi()))); we return the 64-bit rounded value.
    if (!a.sign) return mk_zero(b.sign);
    return pi_rounded(b.sign, rc);
  }
  g_P = g_Pw;
  if (xz) {
    Big hp = bshr(cst(consts().pi), 1);
    return round_big(b.sign, hp, -(int64_t)g_P, 64, rc);
  }
  bool ylex = b.exp < a.exp || (b.exp == a.exp && b.mant <= a.mant);
  int64_t d = (int64_t)b.exp - a.exp;
  if (!a.sign && ylex && d <= -100) {  // atan t = t (1 - t^2/3 ...), strictly below t
    u128 Q;
    bool st;
    divcore(b.mant, a.mant, Q, st);
    if (!st) Q -= 1;
    return rounded(b.sign, Q, d - 95, true, 64, rc);
  }
  const F80& nu = ylex ? b : a;
  const F80& de = ylex ? a : b;
  int64_t sh = g_P + (int64_t)nu.exp - de.exp;
  Big Num = bshl(bfrom(nu.mant), sh > 0 ? sh : 0);
  Big Den = sh < 0 ? bshl(bfrom(de.mant), -sh) : bfrom(de.mant);
  Big T = bdiv(Num, Den, nullptr);
  Big A;
  if (!T.empty()) A = fatan_pos(T);
  Big pi = cst(consts().pi);
  if (!ylex) A = bsub(bshr(pi, 1), A);
  Big val = a.sign ? bsub(pi, A) : A;
  return round_big(b.sign, val, -(int64_t)g_P, 64, rc);
}

F80 t_const(const char* op, int rc) {
  g_P = P_DEF;
  const Consts& c = consts();
  if (!strcmp(op, "pi")) return round_big(0, cst(c.pi), -(int64_t)g_P, 64, rc);
  if (!strcmp(op, "l2e")) return round_big(0, cst(c.l2e), -(int64_t)g_P, 64, rc);
  if (!strcmp(op, "ln2")) return round_big(0, cst(c.ln2), -(int64_t)g_P, 64, rc);
  SF l2t = log2fix(bfrom(10), 0);
  if (!strcmp(op, "l2t")) return round_sf(l2t, rc);
  SF lg2{false, fdivb(fone(), l2t.m)};
  return round_sf(lg2, rc);
}

}  // namespace

F80 transcendental_ex(const char* op, const F80* a, const F80* b, int rc, int* err) {
  int dummy = 0;
  if (!err) err = &dummy;
  *err = 0;
  if (!strcmp(op, "pi") || !strcmp(op, "l2e") || !strcmp(op, "ln2") || !strcmp(op, "l2t") ||
      !strcmp(op, "lg2"))
    return t_const(op, rc);
  if (!a) {
    *err = 1;
    return mk_nan(1);
  }
  if (!strcmp(op, "fpatan") || !strcmp(op, "fyl2x") || !strcmp(op, "fyl2xp1")) {
    if (!b) {
      *err = 1;
      return mk_nan(1);
    }
    if (a->kind == 2 || b->kind == 2) return nan_from(*a, *b);   // a NaN operand propagates (quieted)
    bool atan = !strcmp(op, "fpatan");
    if (atan || !strcmp(op, "fyl2x")) {
      // fast attempt; radius 2^32 units of 2^-P_FAST (atan), times the 64-bit y mantissa (y * log2 x)
      g_Pw = P_FAST; g_check_bits = atan ? 32 : 32 + 64; g_amb = false;
      F80 r = atan ? t_fpatan(*a, *b, rc, err) : t_fyl2x(*a, *b, rc, err);
      g_Pw = P_DEF; g_check_bits = 0;
      if (!g_amb) return r;
      *err = 0;
      return atan ? t_fpatan(*a, *b, rc, err) : t_fyl2x(*a, *b, rc, err);
    }
    return t_fyl2xp1(*a, *b, rc, err);
  }
  if (a->kind == 2) return quiet(*a);   // f2xm1 / fsin / fcos / fptan of a NaN: the NaN, quieted
  if (!strcmp(op, "f2xm1")) return t_f2xm1(*a, rc, err);
  if (!strcmp(op, "fsin") || !strcmp(op, "fcos") || !strcmp(op, "fptan")) return t_trig(op, *a, rc, err);
  *err = 1;
  return mk_nan(1);
}

F80 transcendental(const char* op, const F80* a, const F80* b, int rc) {
  return transcendental_ex(op, a, b, rc, nullptr);
}

}  // namespace reflex_f80
