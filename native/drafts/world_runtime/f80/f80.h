// Exact x87 extended-precision (F80) arithmetic, bit-for-bit equal to _re/trace/x87.py.
//
// Value model: kind 0 = finite number (zero when mant == 0), 1 = infinity, 2 = NaN with its x87 significand in mant
// (bit 63 integer bit, bit 62 quiet bit, payload below; the hardware's NaN rules: loads / stores keep the payload and quiet an SNaN,
// operations propagate the NaN operand, invalid operations give the indefinite 0xc000000000000000 with the sign set).
// Finite nonzero: value = mant * 2^(exp-63), mant normalized (bit 63 set), exp is the unbounded
// (int32) unbiased exponent -- like x87.py there is NO overflow/underflow trap on the exponent;
// results are only rounded to the significand width, with gradual underflow below 2^-16382
// (x87.py round_fraction default emin).
//
// Transcendental error convention: operands that x87.py rejects (it raises ValueError /
// InvalidOperation / OverflowError, or silently mis-handles inf/nan) make transcendental_ex()
// set *err (1 = invalid operand, 2 = out of supported range) and return NaN(sign=1);
// transcendental() just returns that NaN.
#pragma once
#include <cstdint>

namespace reflex_f80 {

struct F80 {
  uint8_t kind;   // 0 num, 1 inf, 2 nan
  uint8_t sign;
  uint64_t mant;  // normalized, bit 63 set; 0 for zero/inf/nan
  int32_t exp;
};

F80 from_f32(uint32_t bits);
F80 from_f64(uint64_t bits);
F80 from_f80(const uint8_t raw[10]);
F80 from_int(int64_t v);

uint32_t to_f32(const F80&, int rc);
uint64_t to_f64(const F80&, int rc);
void to_f80(const F80&, uint8_t out[10]);

F80 add(const F80& a, const F80& b, int bits, int rc, bool negate_b = false);
F80 mul(const F80& a, const F80& b, int bits, int rc);
F80 div(const F80& a, const F80& b, int bits, int rc);
F80 sqrt(const F80& a, int bits, int rc);
int compare(const F80& a, const F80& b);  // -1,0,1, or 2 (unordered)
F80 round_to_integer(const F80& a, int rc);
F80 scale(const F80& a, int64_t n);
F80 remainder(const F80& x, const F80& y, bool nearest, int* quotient_low3);
F80 transcendental(const char* op, const F80* a, const F80* b, int rc);
F80 transcendental_ex(const char* op, const F80* a, const F80* b, int rc, int* err);

}  // namespace reflex_f80
