// rx_x87.cpp -- the C API of rx_x87.h over the exact F80 library (src/f80, our own code, namespace rxf80).
#include "rx_x87.h"
#include <cstring>
#include "f80/f80.h"

static rxf80::F80 in(rx_f80 a) { rxf80::F80 r; r.kind = a.kind; r.sign = a.sign; r.mant = a.mant; r.exp = a.exp; return r; }
static rx_f80 out(const rxf80::F80& a) { rx_f80 r; r.kind = a.kind; r.sign = a.sign; r.mant = a.mant; r.exp = a.exp; return r; }
extern "C" {
rx_f80 rx_f80_from_f32(float x) { uint32_t b; std::memcpy(&b, &x, 4); return out(rxf80::from_f32(b)); }
rx_f80 rx_f80_from_f64(double x) { uint64_t b; std::memcpy(&b, &x, 8); return out(rxf80::from_f64(b)); }
rx_f80 rx_f80_from_int(int64_t v) { return out(rxf80::from_int(v)); }
float rx_f80_to_f32(rx_f80 a, int rc) { uint32_t b = rxf80::to_f32(in(a), rc); float f; std::memcpy(&f, &b, 4); return f; }
double rx_f80_to_f64(rx_f80 a, int rc) { uint64_t b = rxf80::to_f64(in(a), rc); double d; std::memcpy(&d, &b, 8); return d; }
rx_f80 rx_f80_add(rx_f80 a, rx_f80 b, int bits, int rc) { return out(rxf80::add(in(a), in(b), bits, rc)); }
rx_f80 rx_f80_sub(rx_f80 a, rx_f80 b, int bits, int rc) { return out(rxf80::add(in(a), in(b), bits, rc, true)); }
rx_f80 rx_f80_mul(rx_f80 a, rx_f80 b, int bits, int rc) { return out(rxf80::mul(in(a), in(b), bits, rc)); }
rx_f80 rx_f80_div(rx_f80 a, rx_f80 b, int bits, int rc) { return out(rxf80::div(in(a), in(b), bits, rc)); }
rx_f80 rx_f80_sqrt(rx_f80 a, int bits, int rc) { return out(rxf80::sqrt(in(a), bits, rc)); }
int rx_f80_cmp(rx_f80 a, rx_f80 b) { return rxf80::compare(in(a), in(b)); }
rx_f80 rx_f80_round_int(rx_f80 a, int rc) { return out(rxf80::round_to_integer(in(a), rc)); }
rx_f80 rx_f80_rem(rx_f80 a, rx_f80 b, int nearest) { int q = 0; return out(rxf80::remainder(in(a), in(b), nearest != 0, &q)); }
int64_t rx_f80_to_int(rx_f80 a, int rc) {
    rxf80::F80 r = rxf80::round_to_integer(in(a), rc);
    if (r.kind != 0) return INT64_MIN;
    if (r.mant == 0) return 0;
    if (r.exp > 62) return INT64_MIN;
    if (r.exp < 0) return 0;
    uint64_t mag = r.mant >> (63 - r.exp);
    return r.sign ? -(int64_t)mag : (int64_t)mag;
}
rx_f80 rx_f80_transcendental(enum rx_f80_op op, rx_f80 a, rx_f80 b, int rc) {
    static const char* names[] = {"fsin", "fcos", "fptan", "fpatan", "fyl2x", "fyl2xp1", "f2xm1"};
    rxf80::F80 x = in(a), y = in(b);
    if (op == RX_FPATAN) return out(rxf80::fpatan(x, y, rc));
    return out(rxf80::transcendental(names[op], &x, &y, rc));
}
void rx_x87_warm(void) { rxf80::transcendental("pi", nullptr, nullptr, 0); }
}
