/* phys_coll_quat.h -- the quaternion product of the collision code (private to phys_coll_*.c).
 * Quaternions are vec4 (x, y, z, w). The exe multiplies them with the SIMD form l * r =
 *     (z + y) + (l.w * r + x)   x = l.x * r.wzyx * (1,-1,1,-1), y = l.y * r.zwxy * (1,1,-1,-1), z = r.yxwz * l.z * (-1,1,1,-1)
 * (operand order kept: x86 returns the first NaN operand). Rotating a vector by q is q * v * conj(q), conj via (-1,-1,-1,1). */
#pragma once
#include "phys_vec.h"

/* 0093a1f0 / 0093a200 / 0093a210 / 0093a220: the quaternion product sign patterns and the conjugate signs */
#define COLL_QX v4(1.0f, -1.0f, 1.0f, -1.0f)
#define COLL_QY v4(1.0f, 1.0f, -1.0f, -1.0f)
#define COLL_QZ v4(-1.0f, 1.0f, 1.0f, -1.0f)
#define COLL_QCONJ v4(-1.0f, -1.0f, -1.0f, 1.0f)

/* l * r, operands in the order of most call sites */
static inline rx_v4 coll_quat_mul(rx_v4 l, rx_v4 r) {
    const rx_v4 s1 = v4_perm(r, 3, 2, 1, 0), s2 = v4_perm(s1, 1, 0, 3, 2), s3 = v4_perm(s2, 3, 2, 1, 0);
    const rx_v4 x = v4_mul(v4_mul(v4_lane(l, 0), s1), COLL_QX);
    const rx_v4 y = v4_mul(v4_mul(v4_lane(l, 1), s2), COLL_QY);
    const rx_v4 z = v4_mul(v4_mul(s3, v4_lane(l, 2)), COLL_QZ);
    return v4_add(v4_add(z, y), v4_add(v4_mul(v4_lane(l, 3), r), x));
}

/* the same product with the sign pattern operands first (x = QX * (l.x * r.wzyx) ...), as 00848650 / 00848910 compute it */
static inline rx_v4 coll_quat_mul_signs_first_observed(rx_v4 l, rx_v4 r, rx_v4*raw_x) {
    const rx_v4 s1 = v4_perm(r, 3, 2, 1, 0), s2 = v4_perm(s1, 1, 0, 3, 2), s3 = v4_perm(s2, 3, 2, 1, 0);
    const rx_v4 x = v4_mul(COLL_QX, v4_mul(v4_lane(l, 0), s1));
    if(raw_x)*raw_x=x; /* actual8486da XMM0: sign-first first product */
    const rx_v4 y = v4_mul(COLL_QY, v4_mul(v4_lane(l, 1), s2));
    const rx_v4 z = v4_mul(v4_mul(s3, v4_lane(l, 2)), COLL_QZ);
    return v4_add(v4_add(z, y), v4_add(v4_mul(v4_lane(l, 3), r), x));
}

static inline rx_v4 coll_quat_mul_signs_first(rx_v4 l,rx_v4 r){return coll_quat_mul_signs_first_observed(l,r,0);}

/* |x| as the exe computes it: max(0 - x, x) */
static inline rx_v4 coll_v4_abs_max(rx_v4 x) { return v4_max(v4_sub(v4_zero(), x), x); }
