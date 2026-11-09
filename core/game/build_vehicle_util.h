/* build_vehicle_util.h -- small shared helpers of construction unit 6 (vehicles): word copies of the runtime constant vectors
 * the constructors copy, x87 float copies, the MSVC 9 container pieces. Internal to game/build_vehicle*.c. */
#pragma once
#include "rx_game.h"

/* the constants the constructors copy (all in .data: written by the exe's static initializers, unit 12) */
#define VEH_ZERO3      0x00a95d50u   /* the zero vector (x, y, z) the constructors copy word by word */
#define VEH_IDENTITY   0x009cb510u   /* the identity matrix (16 words) */
#define VEH_QIDENTITY  0x009cb550u   /* the identity quaternion (0, 0, 0, 1) */
#define VEH_RT_ZERO    0x00d61670u   /* runtime vector (0, 0, 0, 0) (movaps) */
#define VEH_RT_W1      0x00d61640u   /* runtime vector (0, 0, 0, 1) (movaps) */

static inline void veh_copy(rx_addr dst, rx_addr src, uint32_t bytes) { for (uint32_t k = 0; k < bytes; k += 4) SU32(dst, k, U32(src, k)); }
static inline void veh_v4(rx_addr dst, rx_addr src) { veh_copy(dst, src, 16); }
static inline void veh_zero3(rx_addr dst) { veh_copy(dst, VEH_ZERO3, 12); }   /* movss x3 from the zero vector */
static inline void veh_fill(rx_addr dst, uint32_t bytes, uint32_t v) { for (uint32_t k = 0; k < bytes; k += 4) SU32(dst, k, v); }
/* fld m32; fstp m32: the bits, a signalling NaN quieted */
static inline uint32_t veh_x87_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}

/* a virtual call (thiscall, `this` + n stack words): the core's function when the vtable entry is registered, else the original
 * (oracle build only) */
static inline uint32_t veh_vcall(rx_addr obj, uint32_t off, int n, uint32_t a0, uint32_t a1, uint32_t a2) {
    rx_addr t = U32(U32(obj, 0), off);
    void* f = RX_FN_INTS(t, (n < 3 ? n : 3) + 1);
    if (f) switch (n) {
        case 0: return ((uint32_t (*)(uint32_t))f)(obj);
        case 1: return ((uint32_t (*)(uint32_t, uint32_t))f)(obj, a0);
        case 2: return ((uint32_t (*)(uint32_t, uint32_t, uint32_t))f)(obj, a0, a1);
        default: return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))f)(obj, a0, a1, a2);
    }
    uint32_t a[3] = {a0, a1, a2};
    return rx_call_original(t, RX_THISCALL, obj, n, a);
}

/* a call mirrored on the game stack: ESP at the callee's entry = B - 4 - 4 * n (B = the caller's ESP between its calls), the return
 * address and the stack arguments written where the original pushes them, so the callees' frames (and the dead data they leave)
 * lie where the original's do. Returns the entry ESP (rx_sp is set to it; the caller restores B or its entry ESP). */
static inline rx_addr veh_call_sp(rx_addr B, uint32_t ret, int n, uint32_t a0, uint32_t a1, uint32_t a2) {
    rx_addr sp = B - 4 - 4 * (uint32_t)n;
    SU32(sp, 0, ret);
    if (n > 0) SU32(sp, 4, a0);
    if (n > 1) SU32(sp, 8, a1);
    if (n > 2) SU32(sp, 0xc, a2);
    rx_set_sp(sp);
    return sp;
}
/* push ebp; mov ebp, esp; and esp, -16; then `pushes` registers: the frame's ESP between its calls */
static inline rx_addr veh_frame16(rx_addr sp0, uint32_t pushes) { return ((sp0 - 4) & ~0xfu) - 4 * pushes; }
/* the /GS frame cookie: [esp + off] = __security_cookie (0x009b45e4) ^ esp, esp the frame's ESP when the prologue stores it */
static inline void veh_cookie(rx_addr esp, uint32_t off) { SU32(esp, off, U32(0, 0x009b45e4) ^ esp); }
/* a virtual call with one float argument (thiscall this, float) */
static inline void veh_vcall_f(rx_addr obj, uint32_t off, float a0) {
    rx_addr t = U32(U32(obj, 0), off);
    void* f = rx_fn(t);
    if (f) { ((void (*)(uint32_t, float))f)(obj, a0); return; }
    uint32_t a[1] = {rx_fb(a0)};
    rx_call_original(t, RX_THISCALL, obj, 1, a);
}
