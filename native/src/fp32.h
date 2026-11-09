#ifndef REFLEX_INTERNAL_FP32_H
#define REFLEX_INTERNAL_FP32_H

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
#if defined(_MSC_VER)
#include <float.h>
#endif

#if defined(__FAST_MATH__)
#error "Reflex arithmetic must not be built with fast-math"
#endif
#if defined(__clang__)
#pragma STDC FENV_ACCESS ON
#endif

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "Reflex requires IEEE-754 binary32");

namespace reflex_detail {

inline bool finite(float value) noexcept {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

inline bool supported_fp() noexcept {
    if (std::fegetround() != FE_TONEAREST) return false;
#if defined(__x86_64__) || defined(_M_X64)
    // Ignore six sticky exception-status bits. All other observed controls
    // must match the live profile: masks on, nearest-even, no DAZ or FTZ.
    if ((_mm_getcsr() & 0xffffffc0u) != 0x00001f80u) return false;
    // Also mask x87 exceptions: a platform ceil implementation may use it.
#if defined(_MSC_VER)
    unsigned controls;
    return _controlfp_s(&controls, 0, 0) == 0 && (controls & _MCW_EM) == _MCW_EM;
#else
    uint16_t controls;
    __asm__ volatile("fnstcw %0" : "=m"(controls));
    return (controls & 0x3fu) == 0x3fu;
#endif
#elif defined(__aarch64__) && !defined(_MSC_VER)
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    // RMode[23:22], FZ[24], exception trap enables[12:8,15], and FEAT_AFP
    // FIZ[0]/AH[1]. FIZ can flush inputs even when FZ is clear; AH's alternate
    // handling is outside the verified profile. Older cores read them as zero.
    // DN/AHP and FZ16 do not affect these finite binary32 operations.
    constexpr uint64_t incompatible = (3ull << 22) | (1ull << 24) | 0x9f00ull | 3ull;
    return (fpcr & incompatible) == 0;
#else
    // Another architecture needs its FP control register validated first.
    return false;
#endif
}

// Each original SSE operation rounds separately. Volatile float storage and
// strict compiler flags prevent contraction, reassociation, or excess precision.
inline float add(float a, float b) noexcept { volatile float r = a + b; return r; }
inline float sub(float a, float b) noexcept { volatile float r = a - b; return r; }
inline float mul(float a, float b) noexcept { volatile float r = a * b; return r; }
inline float div(float a, float b) noexcept { volatile float r = a / b; return r; }
inline float sqrt(float a) noexcept { volatile float r = std::sqrt(a); return r; }
inline float from_int(int32_t a) noexcept { volatile float r = static_cast<float>(a); return r; }

} // namespace reflex_detail
#endif
