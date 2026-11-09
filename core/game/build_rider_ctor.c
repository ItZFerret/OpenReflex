/* build_rider_ctor.c -- construction unit 7 (riders): the RiderAnimation constructor FUN_00618180 and the member constructors it
 * calls (the AnimationEntity / Character base 0082d040 / 0082df30, the IK / limb data 00424410 / 0041f9f0 and their element
 * constructors, the limb controllers 00624c80 / 00629840 / 0062a1d0, the rider body shape 00845440, the std::map heads, the event
 * slots 006b7040 / 006b6c20). Hand transliterations of the disassembly; layouts in build_rider.h. */
#include "build_rider.h"
#include "build_rider_util.h"
#include "build_scene.h"
#include "build_vehicle.h"
#include "build_world.h"

/* ---------------------------------------------------------------------------------------------------------------------------
 * small element constructors (thiscall, ECX = element; the vector constructor iterator 004022d0 calls them) */

/* 004022d0 `vector constructor iterator'(array, size, count, ctor) (stdcall, RET 0x10): ctor(element) for each element in order */
void rx_rider_vector_ctor(rx_addr a, uint32_t size, int32_t count, void (*ctor)(rx_addr)) {
    for (int32_t k = count - 1; k >= 0; k--, a += size) ctor(a);
}
/* 0041c760 (ECX): {vtable 0x0093a644, 0} */
static void ctor_0041c760(rx_addr e) { SU32(e, 4, 0); SU32(e, 0, 0x0093a644); }
/* 0041db00 (ECX): seven zero vectors, +0x70 0, byte +0x74 0 */
void rx_rider_ik_rec70_init(rx_addr e) {
    for (uint32_t o = 0; o < 0x70; o += 0x10) rr_set4(e + o, 0, 0, 0, 0);
    SU32(e, 0x70, 0); SU8(e, 0x74, 0);
}
RX_FUNCTION(0x0041db00, rx_rider_ik_rec70_init)
/* 0041db50 (ECX): the identity quaternion, six zero vectors */
void rx_rider_ik_rec_q_init(rx_addr e) {
    rr_set4(e, 0, 0, 0, 1.0f);
    for (uint32_t o = 0x10; o < 0x70; o += 0x10) rr_set4(e + o, 0, 0, 0, 0);
}
RX_FUNCTION(0x0041db50, rx_rider_ik_rec_q_init)
/* 00859ae0 (ECX): two zero vectors, the identity quaternion, a zero vector */
void rx_rider_rec40_init(rx_addr e) {
    rr_set4(e, 0, 0, 0, 0); rr_set4(e + 0x10, 0, 0, 0, 0); rr_set4(e + 0x20, 0, 0, 0, 1.0f); rr_set4(e + 0x30, 0, 0, 0, 0);
}
RX_FUNCTION(0x00859ae0, rx_rider_rec40_init)
/* 0041f9c0 (ECX): {0, -1, -1, -1, 0, -1, -1, -1} */
static void ctor_0041f9c0(rx_addr e) {
    SU32(e, 0, 0); SU32(e, 4, 0xffffffffu); SU32(e, 8, 0xffffffffu); SU32(e, 0xc, 0xffffffffu);
    SU32(e, 0x10, 0); SU32(e, 0x14, 0xffffffffu); SU32(e, 0x18, 0xffffffffu); SU32(e, 0x1c, 0xffffffffu);
}
/* 00430430 (ECX): {0, 0}, the zero vector's x y z at +0x10, the identity quaternion at +0x20 */
void rx_rider_xform30_init(rx_addr e) {
    SU32(e, 0, 0); SU32(e, 4, 0);
    rr_copy(e + 0x10, RR_ZERO3, 12);
    rr_copy(e + 0x20, RR_QIDENTITY, 16);
}
RX_FUNCTION(0x00430430, rx_rider_xform30_init)

/* 0041dca0 (EAX = joint): the joint's limit planes from its angle limits in degrees (+0xc0..+0xdc: four lower, four upper),
 * computed in double precision with the constants of .rdata 0x9774f8..0x977548 */
void rx_rider_joint_limits(rx_addr e) {
    const double k = 0x1.1df46ap-6;           /* [0x977540] degrees -> radians (float precision of pi / 180) */
    const double three = 3.0, two = 2.0;      /* [0x977500], [0x9774f8] */
    const double three_half_pi = 0x1.2d97c88p+2;   /* [0x977538] */
    const double pi_d = 0x1.921fb6p+1;        /* [0x977528] */
    const double half_pi = 0x1.921fb6p+0;     /* [0x977510] */
    const double mhalf_pi = -0x1.921fb6p+0;   /* [0x977518] */
    float a0 = F32(e, 0xc0), a1 = F32(e, 0xc4), a2 = F32(e, 0xc8), a3 = F32(e, 0xcc), a4 = F32(e, 0xd0);
    float s0 = (float)((double)F32(e, 0xd4) * k);
    float s2 = (float)((double)F32(e, 0xd8) * k);
    float s3 = (float)((double)F32(e, 0xdc) * k);
    float x2 = (float)((double)a0 * k);
    double x1d = (double)a1 * k, x3d = (double)a2 * k, x4d = (double)a3 * k, x5d = (double)a4 * k;
    float s1 = (float)(k * 0.0 /* [0x977548] */);
    SF32(e, 0xe8, 0.0f);
    float x1 = (float)x1d;
    SF32(e, 0xec, (float)(((double)x1 - (double)x2) * three));
    SF32(e, 0xf0, (float)(((double)x2 - (double)x1) * two));
    SU32(e, 0x124, 0xc0490fdbu);              /* [0x977530] -pi */
    float x5 = (float)x5d;
    SF32(e, 0x12c, (float)((three_half_pi - (double)x5 * two) - (double)s0));
    SF32(e, 0x130, (float)(((double)x5 - pi_d) + (double)s0));
    SF32(e, 0xf8, 0.0f);
    float x3 = (float)x3d;
    double t7 = ((double)x3 - (double)x1) * three;
    float x4 = (float)x4d;
    SF32(e, 0xe4, x2); SF32(e, 0x128, x5); SF32(e, 0xf4, x1);
    SF32(e, 0xfc, (float)t7);
    SF32(e, 0x100, (float)(((double)x1 - (double)x3) * two));
    SU32(e, 0x134, 0xbfc90fdbu);              /* [0x977520] -pi / 2 */
    SF32(e, 0x138, s0);
    SF32(e, 0x13c, (float)((((double)s1 - mhalf_pi) * three - (double)s0 * two) - (double)s2));
    SF32(e, 0x104, x3);
    SF32(e, 0x140, (float)(((mhalf_pi - (double)s1) * two + (double)s0) + (double)s2));
    SF32(e, 0x108, 0.0f);
    SF32(e, 0x10c, (float)(((double)x4 - (double)x3) * three));
    SF32(e, 0x110, (float)(((double)x3 - (double)x4) * two));
    SF32(e, 0x144, s1);
    SF32(e, 0x14c, (float)(((half_pi - (double)s1) * three - (double)s2 * two) - (double)s3));
    SF32(e, 0x148, s2);
    SF32(e, 0x150, (float)((((double)s1 - half_pi) * two + (double)s2) + (double)s3));
    SF32(e, 0x118, 0.0f);
    SF32(e, 0x11c, (float)(((double)x2 - (double)x4) * three));
    SF32(e, 0x114, x4);
    SF32(e, 0x120, (float)(((double)x4 - (double)x2) * two));
    SU32(e, 0x154, 0x3fc90fdbu);              /* [0x977508] pi / 2 */
    SF32(e, 0x15c, (float)((three_half_pi - (double)s3 * two) - (double)x5));
    SF32(e, 0x158, s3);
    SF32(e, 0x160, (float)(((double)s3 - pi_d) + (double)x5));
}
/* 0041dbb0 (ECX): a joint record (0x170): -1 at +0 / +0x20, zero vectors, the default limits (20 lower, 90 upper degrees) and their
 * planes (0041dca0), the runtime axes */
void rx_rider_joint_init(rx_addr e) {
    SU32(e, 0, 0xffffffffu);
    rr_set4(e + 0x10, 0, 0, 0, 0);
    SU32(e, 0x20, 0xffffffffu);
    rr_set4(e + 0x30, 0, 0, 0, 0);
    for (uint32_t o = 0xc0; o < 0xd0; o += 4) SF32(e, o, 20.0f);   /* [0x935aac] */
    for (uint32_t o = 0xd0; o < 0xe0; o += 4) SF32(e, o, 90.0f);   /* [0x935850] */
    rx_rider_joint_limits(e);
    rr_v4(e + 0xb0, RR_RT_W1);
    rr_v4(e + 0x40, RR_RT_X1);
    rr_v4(e + 0x50, RR_RT_Y1);
    rr_v4(e + 0x60, RR_RT_Z1);
    rr_v4(e + 0x70, e + 0x40);
    rr_v4(e + 0x80, e + 0x50);
    rr_v4(e + 0x90, e + 0x60);
    rr_v4(e + 0xa0, RR_RT_ZERO);
}
RX_FUNCTION(0x0041dbb0, rx_rider_joint_init)

/* ---------------------------------------------------------------------------------------------------------------------------
 * the IK solver data (InverseKinematicsAnimator's +0x54: 0x0a8d0 bytes inside RiderAnimation + 0x5d0) */

/* 0041da90 (EAX = n, s0 = &block): _aligned_malloc((n rounded up to 128 + 128) * 0x30 + 0x80, 0x80); returns the rounded count */
uint32_t rx_rider_pose_block_alloc(int32_t n, rx_addr out) {
    int32_t r = n % 128;   /* and ecx, 0x8000007f with the sign fix-up: the C remainder */
    uint32_t cnt = (uint32_t)(n - r + 0x80);
    SU32(out, 0, rx_crt_aligned_malloc(cnt * 0x30 + 0x80, 0x80));
    return cnt;
}
/* 0041f9f0 (ESI = s): the solver's chains: a zero vector, +0x10 1, floats, 21 records of 0x80 (0041db00), 21 of 0x70 (0041db50), 21 of
 * 0x40 (00859ae0), 20 joints (0041dbb0), 41 links of 0x230 (0041f9c0) */
void rx_rider_ik_chains_init(rx_addr s) {
    rr_set4(s, 0, 0, 0, 0);
    SU32(s, 0x10, 1);
    SF32(s, 0x14, 0); SF32(s, 0x6c, 0); SF32(s, 0x70, 0); SF32(s, 0x74, 0); SF32(s, 0x78, 0);
    SU32(s, 0x7c, 0);
    rx_rider_vector_ctor(s + 0x80, 0x80, 0x15, rx_rider_ik_rec70_init);
    rx_rider_vector_ctor(s + 0xb00, 0x70, 0x15, rx_rider_ik_rec_q_init);
    rx_rider_vector_ctor(s + 0x1430, 0x40, 0x15, rx_rider_rec40_init);
    SU32(s, 0x1b20, 0);
    rx_rider_vector_ctor(s + 0x1b30, 0x170, 0x14, rx_rider_joint_init);
    SU32(s, 0x39d0, 0);
    rx_rider_vector_ctor(s + 0x39e0, 0x230, 0x29, ctor_0041f9c0);
    SU32(s, 0x9438, 0); SU32(s, 0x943c, 0);
}
/* 00424410 (EDI = d): the IK data object */
rx_addr rx_rider_ik_data_init(rx_addr d) {
    SU32(d, 0, 0); SU32(d, 4, 0);
    rx_rider_vector_ctor(d + 8, 8, 0x10, ctor_0041c760);
    SU32(d, 0x8c, 0); SU32(d, 0x90, 0); SU32(d, 0x98, 0);
    SF32(d, 0x2f8, 1.0f);
    SU32(d, 0x2f0, 0); SU32(d, 0x2f4, 0xffffffffu);
    SF32(d, 0x300, 0); SF32(d, 0x304, 0);
    SU8(d, 0x2fc, 0);
    rr_v4(d + 0x310, RR_RT_ZERO);
    rr_splat(d + 0x320, 2.0f);    /* [0x92e330] */
    rr_splat(d + 0x330, 1.0f);
    rr_splat(d + 0x340, 0.25f);   /* [0x92ff84] */
    SU32(d, 0x370, 0); SU32(d, 0x374, 0); SU32(d, 0x378, 0); SU32(d, 0x37c, 0);
    rx_rider_ik_chains_init(d + 0x380);
    rr_v4(d + 0xa5a0, RR_RT_ZERO);
    SU32(d, 0xa760, 0); SU32(d, 0xa830, 0); SU32(d, 0xa834, 0);
    rr_v4(d + 0xa840, RR_RT_Y1);
    SF32(d, 0xa868, 0); SF32(d, 0xa86c, 0); SF32(d, 0xa870, 0); SF32(d, 0xa874, 0);
    rr_splat(d + 0x320, 1.0f);
    rr_splat(d + 0x330, 1.0f);
    rr_splat(d + 0x340, 0.25f);
    rr_splat(d + 0x350, 16900.0f);   /* [0x977e1c] */
    SU32(d, 0xa860, 0); SU32(d, 0xa864, 1);
    SU32(d, 0xa878, 0); SU32(d, 0xa87c, 0); SU32(d, 0xa8a0, 0); SU32(d, 0xa8c0, 0);
    rr_splat(d + 0x360, 8100.0f);    /* [0x977e18] */
    rx_rider_pose_block_alloc(0x320, d + 0xa80c);
    SU32(d, 0xa0, 0x10);
    rr_zero8(d + 0xa4, 0x40);
    return d;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * containers */

/* 00690910: a std::map / std::set head node (ReflexAlignedAlloc(0x30): left / parent / right 0, color byte +0x2c 1, isnil +0x2d 0) */
rx_addr rx_rider_map30_head_alloc(void) {
    rx_addr n = rr_alloc(0x30);
    if (n) SU32(n, 0, 0);
    SU32(n, 4, 0);
    SU32(n, 8, 0);
    SU8(n, 0x2c, 1);
    SU8(n, 0x2d, 0);
    return n;
}
RX_FUNCTION(0x00690910, rx_rider_map30_head_alloc)
/* the map the constructors inline: proxy (ReflexAlignedAlloc(4)), head (00690910, isnil, self links), size 0 */
static void map30_init(rx_addr m) {
    rx_addr p = rr_alloc(4);
    if (p) SU32(p, 0, m);
    SU32(m, 0, p);
    rx_addr h = rx_rider_map30_head_alloc();
    SU32(m, 0x18, h);
    SU8(h, 0x2d, 1);
    SU32(U32(m, 0x18), 4, U32(m, 0x18)); SU32(U32(m, 0x18), 0, U32(m, 0x18)); SU32(U32(m, 0x18), 8, U32(m, 0x18));
    SU32(m, 0x1c, 0);
}
/* 0063b790 (ESI = m): the same as a function (a FloatListener map) */
rx_addr rx_rider_map30_init(rx_addr m) { map30_init(m); return m; }
/* 00430060 (ESI = m): a std::map with an 0x18 head node (00706df0) */
rx_addr rx_rider_map18_init(rx_addr m) {
    rx_addr p = rr_alloc(4);
    if (p) SU32(p, 0, m);
    SU32(m, 0, p);
    rx_addr h = rx_veh_map_head_alloc();
    SU32(m, 0x18, h);
    SU8(h, 0x15, 1);
    SU32(U32(m, 0x18), 4, U32(m, 0x18)); SU32(U32(m, 0x18), 0, U32(m, 0x18)); SU32(U32(m, 0x18), 8, U32(m, 0x18));
    SU32(m, 0x1c, 0);
    return m;
}

/* 00843480 (ESI = t): a string hash table (0x20): 50 buckets, load 0.75f, its own StdMemoryAllocator (0x0097_53b0), the bucket array
 * from the allocator's vf1, zeroed */
rx_addr rx_rider_strtab_init(rx_addr t) {
    rx_addr a = rr_alloc(4);
    if (a) SU32(a, 0, 0x009753b0);
    SU32(t, 0x18, a);
    SU8(t, 0x1c, 1);
    SU32(t, 0, 0x32);
    SF32(t, 4, 0.75f);   /* [0x935aa8] */
    SU32(t, 0x10, 0); SU32(t, 0x14, 0xffffffffu); SU32(t, 8, 0);
    uint32_t sz = 0xc8;
    rx_addr b = rr_vcall(a, 4, 1, &sz);
    SU32(t, 0xc, b);
    rr_zero8(b, U32(t, 0) * 4);
    return t;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the rider body shape, the camera / effect members */

/* 00845440 (ECX = shape; stack radius; RET 4): CollisionShapeCircle (type 2, vtable 0x00976674), +0x20 the radius splat */
rx_addr rx_rider_shape_circle_construct(rx_addr s, float radius) {
    rx_collision_shape_construct(s, 2);
    SU32(s, 0, 0x00976674);
    rr_splat(s + 0x20, radius);
    return s;
}
/* 00694110 (EAX = e): a particle-effect slot */
void rx_rider_effect_slot_init(rx_addr e) {
    SU32(e, 0, 0x0095df44);
    SU32(e, 0x84, 0); SU32(e, 0x74, 0); SU32(e, 0x78, 0); SU32(e, 0x7c, 0); SU32(e, 0x70, 0); SU32(e, 0x80, 0); SU32(e, 0x88, 0);
    SF32(e, 0x10, 0); SF32(e, 0x98, 0);
    SU32(e, 0x20, 1); SU32(e, 0x1c, 0); SU32(e, 0x90, 0); SU32(e, 0x94, 0);
    for (uint32_t k = 0; k < 4; k++) { SF32(e, 0xa4 + 4 * k, 4000000.0f /* [0x9776f8] */); SU8(e, 0xb4 + k, 1); }
    SU32(e, 0x9c, 0); SU32(e, 0xa0, 0);
}
/* 0063bff0 (ESI = e, stack p; RET 4): the lens-swipe effect holder (0x110) */
rx_addr rx_rider_lens_effect_construct(rx_addr e, uint32_t p) {
    SU32(e, 4, 0);
    SU32(e, 0, 0x009588ec);
    rx_rider_effect_slot_init(e + 0x10);
    SU32(e, 0xd0, 0x0095df34);
    SF32(e, 0xe4, 1.0f);
    SU32(e, 0x100, p);
    SU32(e, 0xc, 0);
    SU32(e, 0, 0x00958908);
    return e;
}

/* 00617e80 (ECX = r): the rider's wreck-impact record */
void rx_rider_impact_init(rx_addr r) {
    SF32(r, 0x80, 0); SF32(r, 0x84, 0); SF32(r, 0x88, 0);
    SF32(r, 0xa0, 0); SF32(r, 0xa4, 0); SF32(r, 0xa8, 0); SF32(r, 0xac, 0);
    SU32(r, 0x90, 0); SU32(r, 0x94, 0); SU32(r, 0x98, 0); SU32(r, 0x9c, 0); SU32(r, 0xb0, 0);
    SU8(r, 0, 0); SU8(r, 0x20, 0);
    SU32(r, 0x64, 0); SU32(r, 0x68, 0); SU32(r, 0x6c, 0); SU32(r, 0x70, 0);
    SF32(r, 0x78, 5.0f);   /* [0x935890] */
    SU32(r, 0x40, 0);
}

/* a ranged float {value 0, rate, 1.0, FLT_MAX, -FLT_MAX} (the controllers' inline form) */
static void ranged(rx_addr r, float rate) {
    SF32(r, 0, 0); SF32(r, 4, rate); SF32(r, 0xc, 0x1.fffffep+127f); SF32(r, 0x10, -0x1.fffffep+127f); SF32(r, 8, 1.0f);
}
/* 00624c80 (stack c; RET 4): the head / neck look controller (RiderAnimation + 0x5b590) */
rx_addr rx_rider_look_controller_init(rx_addr c) {
    ranged(c + 0x1c, 0.1f);
    for (uint32_t o = 0xb4; o <= 0x118; o += 0x14) ranged(c + o, 0.1f);
    RX_FRAME_ENTER(F, 0x20);   /* the three zero floats 0042d700 copies (the original's frame: esp + 0x14 after its pushes) */
    SF32(F, 0x10, 0); SF32(F, 0x14, 0); SF32(F, 0x18, 0);
    rx_veh_ranged3_alloc(c + 0x12c, F + 0x10, 0x3dcccccdu /* [0x931d94] 0.1f, fld / fstp */);
    RX_FRAME_LEAVE(F, 0x20);
    SU32(c, 0, 4); SU32(c, 4, 4);
    SF32(c, 0x60, 0); SF32(c, 0x64, 0); SF32(c, 0x68, 0);
    SF32(c, 0x78, 1.0f);
    SF32(c, 0x70, 0); SF32(c, 0x74, 0);
    SU32(c, 0x10, 0xffffffffu); SU32(c, 0x14, 0xffffffffu);
    SF32(c, 0x30, 1.0f); SF32(c, 0x34, 0.5f);
    SF32(c, 0x3c, 1.0f);
    SF32(c, 0x38, 10.0f); SF32(c, 0x40, 10.0f);      /* [0x92fd88] */
    SF32(c, 0x18, 0.01f);                             /* [0x9322ac] */
    SU32(c, 0x50, 0);
    SF32(c, 0xc, 0); SF32(c, 0x48, 0); SF32(c, 0x54, 0);
    SU32(c, 0x5c, 0); SU32(c, 0x58, 0); SU32(c, 0x44, 0); SU32(c, 0x84, 0); SU32(c, 0x4c, 0); SU32(c, 0xac, 0);
    SF32(c, 0x80, 0); SF32(c, 0xa0, 0); SF32(c, 0xa4, 0); SF32(c, 0xa8, 0); SF32(c, 0x88, 0);
    SF32(c, 0xb0, 1.5f);                              /* [0x9322a4] */
    SF32(c, 0x90, 0); SF32(c, 0x94, 0); SF32(c, 0x98, 0);
    return c;
}
/* 00629840 (EAX = c): the lean controller (RiderAnimation + 0x5b6e4) */
void rx_rider_lean_controller_init(rx_addr c) {
    for (uint32_t o = 0x20; o <= 0x5c; o += 0x14) ranged(c + o, 0.2f);   /* [0x92e388] */
    SU32(c, 0x10, 0xffffffffu);
    SF32(c, 0x14, 0); SF32(c, 0x18, 0); SF32(c, 0x1c, 0); SF32(c, 0, 0);
    SU32(c, 8, 0); SU32(c, 0xc, 1);
}
/* 0062a1d0 (EAX = L): a limb state (RiderAnimation + 0x5b760 / + 0x5b850; core/notes/anim.md "Rider limb state") */
void rx_rider_limb_init(rx_addr L) {
    ranged(L + 0x98, 0.1f);
    SU32(L, 4, 0); SU32(L, 0, 0);
    SF32(L, 0x30, 0); SF32(L, 0x34, 0); SF32(L, 0x38, 0);
    SF32(L, 0x44, 4.0f);                 /* [0x9322c0] */
    SU32(L, 0x2c, 2);
    SF32(L, 0x40, 0);
    SU32(L, 0x48, 0); SU32(L, 0x4c, 0); SU32(L, 0x50, 0); SU32(L, 0x54, 0); SU32(L, 0xbc, 0);
    SU32(L, 0x20, 0xffffffffu); SU32(L, 0x24, 0xffffffffu); SU32(L, 0x28, 0xffffffffu);
    for (uint32_t o = 0x60; o <= 0x88; o += 4) if (o != 0x6c && o != 0x7c) SF32(L, o, 0);
    SF32(L, 0x10, 0); SF32(L, 0x14, 0); SF32(L, 0x18, 0);
    SU32(L, 0xc0, U32(0, 0x009d78c4));
    SF32(L, 0xc8, 0.15f);                /* [0x9322c8] */
    SF32(L, 0xcc, 0.25f);                /* [0x92ff84] */
    SF32(L, 0xac, 0); SF32(L, 0xb4, 0);
    SU32(L, 0xd0, U32(0, 0x009cd9e8));
    SU8(L, 0xd4, 0); SU8(L, 0xd5, 0);
    SU32(L, 0xe0, U32(0, 0x009cb4b0)); SU32(L, 0xe4, U32(0, 0x009cb4b4)); SU32(L, 0xe8, U32(0, 0x009cb4b8));
    SU32(L, 0x90, 0); SU32(L, 0x94, 0);
    SF32(L, 0xb0, 0); SF32(L, 0xb8, 0);
}

/* 006b6c20 (ECX = s): an event slot (0xe10): five 0xd8 records (006b5dd0), fifty transforms (00430430), counters */
static void ctor_006b5dd0(rx_addr e);
static void event_slot_init(rx_addr s) {
    rx_rider_vector_ctor(s + 4, 0xd8, 5, ctor_006b5dd0);
    rx_rider_vector_ctor(s + 0x440, 0x30, 0x32, rx_rider_xform30_init);
    SU32(s, 0xda8, 0); SU32(s, 0xe00, 0);
    SF32(s, 0xdb4, 0); SF32(s, 0xdb0, 0);
}
/* 006b7040 (ESI = s): the rider's animation event slots (RiderAnimation + 0xc940) */
rx_addr rx_rider_event_slots_init(rx_addr s) {
    rx_rider_vector_ctor(s, 0x30, 0x32, rx_rider_xform30_init);
    rx_rider_vector_ctor(s + 0x960, 0xe10, 0x41, event_slot_init);
    rx_rider_vector_ctor(s + 0x39b70, 0xe10, 4, event_slot_init);
    rx_rider_vector_ctor(s + 0x3d3b0, 0xe10, 0x10, event_slot_init);
    SU32(s, 0x4b4b0, 0); SU32(s, 0x4b4b4, 0); SU32(s, 0x4b4b8, 0);
    return s;
}
static void ctor_006b5dd0(rx_addr e) { SF32(e, 0xd4, 0); }   /* 006b5dd0 (ECX) */

/* ---------------------------------------------------------------------------------------------------------------------------
 * the AnimationEntity base */

/* 0082df30 (EDI = c): the Character (pose owner, 0x2a0): the pose pool's critical section +0x208, pool fields, the bone name table
 * (00843480 at +0x248), the root delta (zero vector, identity quaternion), the 0x8e0 pose-cache block at +0x294 */
rx_addr rx_rider_character_construct(rx_addr c) {
    rr_os_cs_init(c + 0x208);
    for (uint32_t o = 0x220; o <= 0x240; o += 4) SU32(c, o, 0);
    rx_rider_strtab_init(c + 0x248);
    rr_copy(c + 0x270, RR_ZERO3, 12);
    rr_copy(c + 0x280, RR_QIDENTITY, 16);
    SU32(c, 0x294, 0);
    rx_addr b = rr_alloc(0x8e0);
    if (b) { SU32(b, 0x8c0, 0); SU32(b, 0x8c4, 0); SU32(b, 0x8c8, 0); }
    SU32(c, 0x294, b);
    SU32(c, 0x1f8, 0);
    SU8(c, 0x204, 0);
    SU8(c, 0x1f4, 1);
    return c;
}
/* 0082d040 (stack e; RET 4): AnimationEntity: the ModelEntity base (scene unit 00401db0), vtables 0x0097605c / +0x90 0x009760dc,
 * the animator name table +0x148 (00843480), the embedded ModelAsset +0x170 (007d3f30), the Character (0x2a0) at +0x144, +0x70 1 */
rx_addr rx_rider_animation_entity_construct(rx_addr e) {
    rx_model_entity_construct(e);
    SU32(e, 0, 0x0097605c);
    SU32(e, 0x90, 0x009760dc);
    rx_rider_strtab_init(e + 0x148);
    rx_rider_model_asset_construct(e + 0x170);
    rx_addr c = rr_alloc(0x2a0);
    SU32(e, 0x144, c ? rx_rider_character_construct(c) : 0);
    SU32(e, 0x70, 1);
    return e;
}
RX_FUNCTION(0x0082d040, rx_rider_animation_entity_construct)

/* 007d3f30 (ESI = m): the ModelAsset constructor (the model loader's, build_model.c, is static: this is the same code) */
rx_addr rx_rider_model_asset_construct(rx_addr m) {
    SU32(m, 0, 0x00972844);
    for (uint32_t o = 8; o <= 0x44; o += 4) SU32(m, o, 0);
    SU32(m, 0x4c, 0);
    SU16(m, 0x48, 0);
    for (uint32_t o = 0x50; o <= 0x84; o += 4) SU32(m, o, 0);
    SU16(m, 0x88, 0);
    SU32(m, 0x8c, 0);
    rr_vec_init(m + 0x124);
    SU32(m, 0x94, 0);
    SU8(m, 0x98, 0);
    SU32(m, 0x118, 0); SU32(m, 0x120, 0); SU32(m, 0x11c, 0);
    SU32(m, 0x90, 0xffffffffu);
    return m;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * RiderAnimation 00618180 */

/* a listener-carrying float (FloatListeners: the value block then a std::vector of listeners at +0xc) */
static void flt3_vec(rx_addr ra, uint32_t f, uint32_t vec) {
    SF32(ra, f, 1.0f); SF32(ra, f + 4, 0); SF32(ra, f + 8, 0);
    rr_vec_init(ra + vec);
}

/* 00618180 (stack ra; RET 4) */
rx_addr rx_rider_animation_construct(rx_addr ra) {
    rx_rider_animation_entity_construct(ra);
    SU32(ra, 0x2b0, 0x009370cc);                 /* IRenderEvent::vftable */
    SU32(ra, 0, 0x00958724);                     /* RiderAnimation::vftable */
    SU32(ra, 0x90, 0x009587a4);
    SU32(ra, 0x2b0, 0x009587e4);
    flt3_vec(ra, 0x2d0, 0x2dc);
    flt3_vec(ra, 0x2fc, 0x308);
    flt3_vec(ra, 0x328, 0x334);
    flt3_vec(ra, 0x354, 0x360);
    SF32(ra, 0x380, 1.0f); SF32(ra, 0x384, 0); SF32(ra, 0x388, 0);
    rr_vec_init(ra + 0x38c);
    SF32(ra, 0x3ac, 1.0f); SF32(ra, 0x3b0, 0); SF32(ra, 0x3b4, 0);
    SF32(ra, 0x40c, 1.0f); SF32(ra, 0x410, 0); SF32(ra, 0x414, 0);
    SF32(ra, 0x424, 1.0f); SF32(ra, 0x428, 0); SF32(ra, 0x42c, 0);
    SF32(ra, 0x43c, 1.0f); SF32(ra, 0x440, 0); SF32(ra, 0x444, 0);
    SF32(ra, 0x454, 1.0f); SF32(ra, 0x458, 0); SF32(ra, 0x45c, 0);
    rr_vec_init(ra + 0x46c);
    rr_vec_init(ra + 0x48c);
    rr_vec_init(ra + 0x4a8);
    rr_vec_init(ra + 0x4d0);
    rr_vec_init(ra + 0x4f8);
    rr_vec_init(ra + 0x520);
    rr_vec_init(ra + 0x548);
    rx_rider_ik_data_init(ra + 0x5d0);
    rx_rider_shape_circle_construct(ra + 0xaea0, 1.0f);   /* fld1; fstp */
    SU32(ra, 0xaf60, 0x41afffcc);                /* [0x977e38] */
    SU32(ra, 0xaf64, 0x413bbb84);                /* [0x977e34] */
    SU32(ra, 0xaf68, 0x3ebbbb84);                /* [0x977e30] */
    SU32(ra, 0xaf6c, 0x403bbb84);                /* [0x977e2c] */
    SU32(ra, 0xaf70, U32(0, 0x009d3bfc));
    SF32(ra, 0xaf58, 0.4f);                      /* [0x935860] */
    SF32(ra, 0xaf5c, 0.6f);                      /* [0x931da8] */
    SF32(ra, 0xaf74, -0.01f);                    /* [0x977d14] */
    SF32(ra, 0xaf78, 0.6f);
    map30_init(ra + 0xaf7c);
    map30_init(ra + 0xaf9c);
    map30_init(ra + 0xafbc);                     /* 0063b790 */
    map30_init(ra + 0xafdc);
    rx_rider_event_slots_init(ra + 0xc940);
    rx_rider_vector_ctor(ra + 0x57e00, 0x30, 0x32, rx_rider_xform30_init);
    rx_rider_vector_ctor(ra + 0x58760, 0x30, 0x32, rx_rider_xform30_init);
    rx_rider_vector_ctor(ra + 0x590c0, 0x30, 0x32, rx_rider_xform30_init);
    rx_rider_vector_ctor(ra + 0x59a20, 0x30, 0x32, rx_rider_xform30_init);
    rx_rider_vector_ctor(ra + 0x5a380, 0x30, 0x32, rx_rider_xform30_init);
    SF32(ra, 0x5b1f4, 0);
    rx_rider_impact_init(ra + 0x5b210);
    /* the WreckChecker at +0x5b54c */
    SU32(ra, 0x5b544, 0);
    SU32(ra, 0x5b54c, 0x009587ec);               /* WreckChecker::vftable */
    SU32(ra, 0x5b550, 0); SU32(ra, 0x5b554, 0); SU32(ra, 0x5b558, 0); SU32(ra, 0x5b55c, 0); SU32(ra, 0x5b568, 0);
    SU32(ra, 0x5b56c, 0xffffffffu);
    SF32(ra, 0x5b570, 0); SF32(ra, 0x5b574, 0); SF32(ra, 0x5b560, 0);
    SU32(ra, 0x5b564, 0);
    rx_rider_look_controller_init(ra + 0x5b590);
    rx_rider_lean_controller_init(ra + 0x5b6e4);
    rx_rider_limb_init(ra + 0x5b760);
    rx_rider_limb_init(ra + 0x5b850);
    SU32(ra, 0x5b940, 0); SU32(ra, 0x5b944, 0);
    SU32(ra, 0x5b948, 0xffffffffu); SU32(ra, 0x5b94c, 0xffffffffu);
    SU32(ra, 0x5b950, 0); SU32(ra, 0x5b954, 0);
    SF32(ra, 0x5b958, 0); SF32(ra, 0x5b95c, 0); SF32(ra, 0x5b960, 0);
    SU32(ra, 0x5b968, 0);
    {   /* ranged floats {0, rate, 1, FLT_MAX, -FLT_MAX} */
        static const uint32_t at[] = {0x5b984, 0x5b998, 0x5b9ac, 0x5b9c0, 0x5b9d4, 0x5ba10, 0x5ba24, 0x5ba38, 0x5ba4c, 0x5ba64, 0x5ba7c,
                                      0x5ba94};
        static const float rate[] = {0.1f, 0.1f, 0.2f, 0.5f, 0.1f, 0.1f, 1.0f, 0.5f, 0.5f, 0.1f, 0.1f, 0.1f};
        SF32(ra, 0x5b964, 0.5f);
        for (int k = 0; k < 12; k++) ranged(ra + at[k], rate[k]);
    }
    rx_rider_map18_init(ra + 0x5bb6c);
    SF32(ra, 0x78, 100.0f);                      /* [0x93005c] */
    SU8(ra, 0x5c4d0, 0); SU8(ra, 0x5c4d1, 0);
    SU32(ra, 0x5b51c, 0xffffffffu);
    SU32(ra, 0x2b8, 0xffffffffu);
    SU32(ra, 0x5b1fc, 0); SU32(ra, 0x5bab8, 0); SU32(ra, 0x5babc, 0); SU32(ra, 0x5bac0, 0);
    for (uint32_t o = 0x5bad0; o <= 0x5bb28; o += 0x10) { SF32(ra, o, 0); SF32(ra, o + 4, 0); SF32(ra, o + 8, 0); }
    SF32(ra, 0x5bb34, 0);
    SU32(ra, 0x5b53c, 0); SU32(ra, 0x5bb40, 0); SU32(ra, 0x5b8, 0); SU32(ra, 0x5bc, 0); SU32(ra, 0xaf00, 0);
    SU32(ra, 0x5b2f4, 0); SU32(ra, 0x5b2d0, 0); SU32(ra, 0x5b2ec, 0);
    SF32(ra, 0x5b97c, 0); SF32(ra, 0x5bb30, 0); SF32(ra, 0x5bb48, 0);
    SU32(ra, 0x5bb4c, 0); SU32(ra, 0x5b548, 0);
    SF32(ra, 0x5bac4, 0);
    SU32(ra, 0x5bac8, 0); SU32(ra, 0x5bb38, 0); SU32(ra, 0x5bb3c, 0); SU32(ra, 0x5bb44, 0); SU32(ra, 0x5b6d0, 0);
    SU32(ra, 0x5bb68, 0); SU32(ra, 0x5baa8, 0);
    SU32(ra, 0x5bb5c, 1); SU32(ra, 0xaf04, 1); SU8(ra, 0x5bb64, 1);
    SF32(ra, 0x5b528, 0); SF32(ra, 0x5b52c, 0); SF32(ra, 0x5b530, 0); SF32(ra, 0x5b534, 0);
    SF32(ra, 0x5ba60, 0); SF32(ra, 0x5ba78, 0); SF32(ra, 0x5ba90, 0);
    SU32(ra, 0xba24, 0); SU32(ra, 0xc450, 0); SU32(ra, 0xaf14, 0); SU32(ra, 0xaf18, 0);
    SF32(ra, 0x5b980, 0);
    SU32(ra, 0xaefc, 0);
    SU16(ra, 0xaf3c, 0xffff);
    SU32(ra, 0xaee4, 0xffffffffu); SU32(ra, 0xaeec, 0xffffffffu); SU32(ra, 0xaef0, 0xffffffffu);
    SU32(ra, 0xaef4, 0xffffffffu); SU32(ra, 0xaef8, 0xffffffffu);
    SU32(ra, 0x5b2e4, 0);
    SF32(ra, 0x5b538, 0); SF32(ra, 0x5ba00, 0); SF32(ra, 0x5ba04, 0); SF32(ra, 0x5ba08, 0);
    SF32(ra, 0x5b974, 1.0f);
    SU32(ra, 0x5b57c, 0xc); SU32(ra, 0x5b580, 3);
    SF32(ra, 0x5b96c, 0); SF32(ra, 0x5b970, 0);
    SF32(ra, 0x5b978, 1.3f);                     /* [0x9773f0] */
    SU8(ra, 0x5b200, 0xff);
    SF32(ra, 0x5b9f0, 0); SF32(ra, 0x5b9f4, 0); SF32(ra, 0x5b9f8, 0);
    SU32(ra, 0x59c, 0); SU32(ra, 0xaee0, 0);
    SU32(ra, 0x2bc, 0xffffffffu);
    SU8(ra, 0xaf0c, 0);
    SU32(ra, 0x5c4e0, 0);
    SU32(ra, 0x5bb60, 1);
    rx_addr fx = rr_alloc(0x110);
    SU32(ra, 0x5b6dc, fx ? rx_rider_lens_effect_construct(fx, 0) : 0);
    SU32(ra, 0xaeb0, 8);
    return ra;
}
RX_FUNCTION(0x00618180, rx_rider_animation_construct)

/* StdMemoryAllocator (vtable 0x009753b0, the hash tables' allocator): vf1 004eef20 _aligned_malloc(size, 16), vf2 00775540
 * _aligned_free (under the allocator lock of [0x00da67d8]: the core builds on one thread) */
uint32_t rx_std_allocator_alloc(rx_addr self, uint32_t size) { (void)self; return rx_crt_aligned_malloc(size, 0x10); }
RX_FUNCTION(0x004eef20, rx_std_allocator_alloc)
uint32_t rx_std_allocator_free(rx_addr self, rx_addr p) { (void)self; rx_crt_aligned_free(p); return 0; }
RX_FUNCTION(0x00775540, rx_std_allocator_free)

/* ---------------------------------------------------------------------------------------------------------------------------
 * CharacterEntity (the scene's animated characters and the RiderCharacter base) */

/* 00834cb0 (EDI = c): the character animation state (CharacterEntity + 0x148, vtable 0x00975e6c): a growable array of 2 pointers
 * (+4 data, +8 count, +0xc capacity 2, +0x10 growth 2, +0x14 max 0x7fffffff), the motion name table +0x8c (00843480), the fields */
rx_addr rx_character_anim_construct(rx_addr c) {
    SU32(c, 0, 0x00975e6c);
    rx_addr d = rr_alloc(rx_mul_sat(2, 4));
    SU32(c, 0xc, 2); SU32(c, 0x10, 2);
    SU32(c, 4, d); SU32(c, 8, 0); SU32(c, 0x14, 0x7fffffff);
    rx_rider_strtab_init(c + 0x8c);
    SU32(c, 0x18, 0); SU32(c, 0x1c, 0); SU32(c, 0x20, 0); SU32(c, 0x24, 1);
    SU32(c, 0x28, 0); SU32(c, 0x2c, 0); SU32(c, 0x30, 0); SU32(c, 0x34, 0);
    SU32(c, 0x54, 0); SU8(c, 0x58, 0); SU32(c, 0x5c, 0); SU32(c, 0x60, 0); SU32(c, 0x64, 1); SU32(c, 0x68, 1);
    SU32(c, 0x78, 0); SU32(c, 0x7c, 0); SU32(c, 0x80, 1); SU32(c, 0x70, 0); SU32(c, 0x74, 0); SU32(c, 0x6c, 0);
    SU32(c, 0x88, 0); SU32(c, 0x84, 0); SU32(c, 0x38, 0);
    for (uint32_t o = 0x3c; o <= 0x50; o += 4) SF32(c, o, 0);
    return c;
}
/* 00839a40 (stack e; RET 4): CharacterEntity: ModelEntity (00401db0), vtables 0x00976134 / +0x90 0x009761b4 / +0x140 0x009761f4
 * (IRenderEvent 0x009370cc first), the character animation state +0x148, the ModelAsset +0x200, +0x340 / +0x144 / +0x1f4 0, +0x70 1 */
rx_addr rx_character_entity_construct(rx_addr e) {
    rx_model_entity_construct(e);
    SU32(e, 0x140, 0x009370cc);
    SU32(e, 0, 0x00976134);
    SU32(e, 0x90, 0x009761b4);
    SU32(e, 0x140, 0x009761f4);
    rx_character_anim_construct(e + 0x148);
    rx_rider_model_asset_construct(e + 0x200);
    SU32(e, 0x340, 0); SU32(e, 0x144, 0); SU32(e, 0x1f4, 0);
    SU32(e, 0x70, 1);
    return e;
}
RX_FUNCTION(0x00839a40, rx_character_entity_construct)
