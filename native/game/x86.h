// Floating point exactly as MXReflex.exe's SSE code computes it (MXCSR 0x1f80: round to nearest even, no DAZ / FTZ, masked),
// on any host. IEEE results match the host's; only NaN results differ by ISA: x86 returns the first operand if it is a NaN
// (quieted), else the second, else the default NaN 0xffc00000 (ARM: SNaN priority and 0x7fc00000). So every operation is the
// host's, with that rule applied when the result is a NaN. Build without fast-math and with -ffp-contract=off.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

#if defined(__FAST_MATH__)
#error "native/game must not be built with fast-math"
#endif

namespace x86 {

inline uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
inline float flt(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
inline bool isnan_bits(uint32_t b) { return (b & 0x7fffffffu) > 0x7f800000u; }
inline float nan_rule(float a, float b) {
    uint32_t x = bits(a), y = bits(b);
    return flt(isnan_bits(x) ? (x | 0x400000u) : isnan_bits(y) ? (y | 0x400000u) : 0xffc00000u);
}
// scalar SSE (addss / subss / mulss / divss / sqrtss / minss / maxss): a is the destination operand
inline float add(float a, float b) { float r = a + b; return r != r ? nan_rule(a, b) : r; }
inline float sub(float a, float b) { float r = a - b; return r != r ? nan_rule(a, b) : r; }
inline float mul(float a, float b) { float r = a * b; return r != r ? nan_rule(a, b) : r; }
inline float div(float a, float b) { float r = a / b; return r != r ? nan_rule(a, b) : r; }
inline float sqrt(float a) { float r = std::sqrt(a); return r != r ? (isnan_bits(bits(a)) ? flt(bits(a) | 0x400000u) : flt(0xffc00000u)) : r; }
// minss / maxss: the second operand unless the comparison holds (NaNs and equal zeros give the second)
inline float min(float a, float b) { return a < b ? a : b; }
inline float max(float a, float b) { return a > b ? a : b; }
// cvtps2dq / cvtss2si (nearest even); out of range or NaN: 0x80000000 ("integer indefinite")
inline int32_t cvt(float a) { return std::fabs(a) < 2147483648.0f ? int32_t(std::nearbyint(a)) : INT32_MIN; }
// cvttps2dq / cvttss2si (truncate)
inline int32_t cvtt(float a) { return std::fabs(a) < 2147483648.0f ? int32_t(a) : INT32_MIN; }

// Four lanes (an xmm register as the guest holds it: lane 0 in the low 32 bits).
struct V4 {
    float f[4];
    static V4 splat(float x) { return {{x, x, x, x}}; }
    static V4 from_bits(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return {{flt(a), flt(b), flt(c), flt(d)}}; }
};
inline V4 add(const V4& a, const V4& b) { return {{add(a.f[0], b.f[0]), add(a.f[1], b.f[1]), add(a.f[2], b.f[2]), add(a.f[3], b.f[3])}}; }
inline V4 sub(const V4& a, const V4& b) { return {{sub(a.f[0], b.f[0]), sub(a.f[1], b.f[1]), sub(a.f[2], b.f[2]), sub(a.f[3], b.f[3])}}; }
inline V4 mul(const V4& a, const V4& b) { return {{mul(a.f[0], b.f[0]), mul(a.f[1], b.f[1]), mul(a.f[2], b.f[2]), mul(a.f[3], b.f[3])}}; }
inline V4 div(const V4& a, const V4& b) { return {{div(a.f[0], b.f[0]), div(a.f[1], b.f[1]), div(a.f[2], b.f[2]), div(a.f[3], b.f[3])}}; }

}  // namespace x86
