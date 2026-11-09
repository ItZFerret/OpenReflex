/* build_vehicle_sim.c -- construction unit 6: the motocross simulation object S (RavenSimulationMotocross, 0x8fc0 bytes; layout in
 * core/notes/bike.md / native/game/notes/bike.md). Hand transliteration from the disassembly of the RavenSimulation base 00430b50
 * (mechanism 0085c200, the DynamicRays shape, vectors), RavenSimulationMotocross 0043ac40 with the parameter registration 004392f0,
 * the body nodes (0042f460 / 0042f520 / 0042fae0 / 0042f640), the wheel contact records 0042d9b0, the parameter file 0043a3f0 /
 * 0043a560 / 0048bd20, the mechanism wiring 0085c3f0 / 0085c470, the mass setup 0043b750 (+ 0042d670 / 0042d8c0) and vf35 0043be80.
 * The simulation is built on the main thread (x87 control word 0x027f): the x87 code here is fld / fstp copies only. */
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "build_vehicle_int.h"
#include "build_alloc.h"
#include "build_world.h"
#include "build_load.h"
#include "build_xml.h"
#include "bike_math.h"
#include "crt.h"
#include "phys_misc.h"
#include "loop_vehicle.h"
#include "build_rider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* a virtual call with only `this` (the RavenPhysics matrix / vector vfuncs): the core function by the vtable slot */
static uint32_t vcall0(rx_addr self, uint32_t slot) {
    rx_addr fn = U32(U32(self, 0), 4 * slot);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (!f) { fprintf(stderr, "rxcore: virtual %08x (slot %u of %08x) not in the core\n", fn, slot, U32(self, 0)); abort(); }
    return f(self);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * RavenPhysics matrix / vector clears the phys unit has not registered (vf1 of matrices, vf0 of vectors) */
static uint32_t rows(rx_addr self, int n, rx_addr src) { for (int r = 0; r < n; r++) veh_v4(self + 0x10u + 0x10u * (uint32_t)r, src); return self; }
uint32_t rx_matrix4x3_clear(rx_addr m) { return rows(m, 4, VEH_RT_ZERO); }                 /* 00858380 */
RX_FUNCTION(0x00858380, rx_matrix4x3_clear);
uint32_t rx_matrix3x4_clear(rx_addr m) { return rows(m, 3, 0x00d61660u); }                 /* 00858130 */
RX_FUNCTION(0x00858130, rx_matrix3x4_clear);
uint32_t rx_vec4_clear(rx_addr v) { veh_v4(v + 0x10, 0x00d61660u); return v; }             /* 0042f420 (returns ECX) */
RX_FUNCTION(0x0042f420, rx_vec4_clear);
uint32_t rx_matrix4x6_clear(rx_addr m) { vcall0(m + 0x10, 1); return vcall0(m + 0x60, 1); } /* 00858960 */
RX_FUNCTION(0x00858960, rx_matrix4x6_clear);
uint32_t rx_matrix3x6_clear(rx_addr m) { vcall0(m + 0x10, 1); return vcall0(m + 0x50, 1); } /* 00858360 */
RX_FUNCTION(0x00858360, rx_matrix3x6_clear);

/* matrix headers: {vtable, rows, columns} */
#define VT_M3X3 0x0093ca04u
#define VT_M3X4 0x0093ca28u
#define VT_M4X3 0x0093ca4cu
#define VT_M4X4 0x0093ca70u
#define VT_M6X6 0x0093ca94u
#define VT_M4X6 0x0093cab8u
#define VT_M6X4 0x0093cadcu
#define VT_VEC6 0x0093cb00u
#define VT_VEC3 0x0093cb14u
#define VT_VEC4 0x0093cb2cu
static void mhdr(rx_addr m, uint32_t vt, uint32_t r, uint32_t c) { SU32(m, 0, vt); SU32(m, 4, r); SU32(m, 8, c); }
static void vhdr(rx_addr v, uint32_t vt, uint32_t n) { SU32(v, 0, vt); SU32(v, 4, n); }

/* 0042f1d0 (EAX = m): a Matrix6x6 of four Matrix3x3 blocks */
/* Matrix4x6 = {4x3, 4x3} (+0x10, +0x60) and Matrix6x4 = {3x4, 3x4} (+0x10, +0x50) headers */
static void m4x6_init(rx_addr m) { mhdr(m, VT_M4X6, 4, 6); mhdr(m + 0x10, VT_M4X3, 4, 3); mhdr(m + 0x60, VT_M4X3, 4, 3); }
static void m6x4_init(rx_addr m) { mhdr(m, VT_M6X4, 6, 4); mhdr(m + 0x10, VT_M3X4, 3, 4); mhdr(m + 0x50, VT_M3X4, 3, 4); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * the mechanism M (S + 0xd0) */

/* 0085c200 (ESI = M) */
static void mechanism_init(rx_addr M) {
    veh_fill(M, 0x10, 0);
    SU32(M, 0x10, 1); SU32(M, 0x14, 0); SU32(M, 0x18, 0); SU32(M, 0x1c, 0);
    for (uint32_t i = 0; i < 10; i++) veh_fill(M + 0x20 + 0xa0 * i, 0xa0, 0);              /* 00859a00: the body states */
    for (uint32_t i = 0; i < 10; i++) {                                                     /* 00859a60 */
        rx_addr b = M + 0x660 + 0xa0 * i;
        veh_v4(b, VEH_RT_W1);
        for (uint32_t k = 0x10; k < 0x80; k += 0x10) veh_v4(b + k, VEH_RT_ZERO);
        veh_v4(b + 0x80, VEH_RT_W1); veh_v4(b + 0x90, VEH_RT_W1);
    }
    for (uint32_t i = 0; i < 10; i++) rx_rider_rec40_init(M + 0xcd0 + 0x40 * i);           /* 00859ae0 (build_rider_ctor.c) */
    mhdr(M + 0xf50, VT_M3X3, 3, 3);
    m4x6_init(M + 0xf90);
    mhdr(M + 0xff0, VT_M4X3, 4, 3);
    mhdr(M + 0x1040, VT_M4X4, 4, 4);
    m6x4_init(M + 0x1090);
    mhdr(M + 0x10e0, VT_M3X4, 3, 4);
    phys_matrix6x6_construct(M + 0x1120);
    veh_fill(M + 0xca0, 0x28, 0);
}

/* 0085c3f0 (EDX = M): the nodes' state pointers (+0x28 / +0x2c / +0x30 / +0x34 of each node) and the parents' child lists */
void rx_veh_mechanism_bind(rx_addr M) {
    rx_addr n0 = U32(M, 0xca0);
    SU32(n0, 0x30, M + 0x20);
    SU32(n0, 0x34, M + 0x660);
    for (uint32_t i = 1; (int32_t)i < I32(M, 0x1c); i++) {
        rx_addr n = U32(M, 0xca0 + 4 * i);
        rx_addr parent = U32(M, 0xca0 + 4 * U32(n, 0xc));
        SU32(parent, 0x14 + 4 * U32(parent, 0x10), i);
        SU32(parent, 0x10, U32(parent, 0x10) + 1);
        uint32_t p = U32(n, 0xc);
        SU32(n, 0x2c, M + 0x660 + 0xa0 * p);
        SU32(n, 0x34, M + 0x700 + 0xa0 * (i - 1));
        SU32(n, 0x28, M + 0x20 + 0xa0 * p);
        SU32(n, 0x30, M + 0x700 + 0xa0 * (i - 1) - 0x640);
    }
}

/* 0085c470 (ECX = M, xmm1 = mass, s0 = body; RET 4): mass splat at body +0x40, its reciprocal at +0x50 */
void rx_veh_mechanism_set_mass(rx_addr M, float mass, uint32_t body) {
    rx_addr b = M + 0xa0 * body;
    rx_st4(b + 0x40, v4_splat(mass));
    rx_st4(b + 0x50, v4_splat(phys_div(1.0f /* [0x931f24] */, mass)));
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the mechanism's nodes */

/* 0042f460 (EAX = node, 13 stack arguments; RET 0x34): the Node base: +0x38.. the state matrices / vectors the node uses */
static void node_init(rx_addr n, const rx_addr a[13]) {
    SU32(n, 0x10, 0); SU32(n, 0x28, 0); SU32(n, 0x2c, 0); SU32(n, 0x30, 0); SU32(n, 0x34, 0);
    SU32(n, 0x38, a[0]); SU32(n, 0x3c, a[1]); SU32(n, 0x40, a[2]); SU32(n, 0x44, a[3]); SU32(n, 0x48, a[4]);
    SU32(n, 0x4c, a[5]); SU32(n, 0x50, a[6]); SU32(n, 0x54, a[8]); SU32(n, 0x58, a[9]); SU32(n, 0x5c, a[10]);
    SU32(n, 0x60, a[11]); SU32(n, 0x64, a[7]);
    SU32(n, 0, 0x0093cb44);   /* RavenPhysics::Node::vftable */
    SU32(n, 4, 1); SU32(n, 8, 0xffffffffu); SU32(n, 0xc, 0xffffffffu);
    SU32(n, 0x68, a[12]);
    for (uint32_t o = 0x14; o <= 0x24; o += 4) SU32(n, o, 0xffffffffu);
}

/* 0042f520 (EDI = node): MainBodyNode */
void rx_veh_main_body_node_construct(rx_addr n) {
    rx_addr e = n + 0x290, p = n + 0x2d0, b = n + 0x2f0;
    const rx_addr a[13] = {e, e, p, n + 0x70, n + 0x180, e, e, b, e, e, e, e, p};
    node_init(n, a);
    SU32(n, 0, 0x0093cb70);   /* RavenPhysics::MainBodyNode::vftable */
    phys_matrix6x6_construct(n + 0x70); phys_matrix6x6_construct(n + 0x180);
    mhdr(e, VT_M3X3, 3, 3);
    vhdr(p, VT_VEC3, 3);
    vhdr(b, VT_VEC6, 6);
    vcall0(n + 0x70, 1); vcall0(n + 0x180, 1); vcall0(b, 0);
}

/* 0042fae0 (ESI = node): ConstraintFixedBody */
void rx_veh_fixed_body_node_construct(rx_addr n) {
    const rx_addr a[13] = {n + 0x90, n + 0x1a0, n + 0x2e0, n + 0x310, n + 0x420, n + 0x530, n + 0x640, n + 0xbc0, n + 0x750,
                           n + 0x860, n + 0x970, n + 0xa80, n + 0xb90};
    node_init(n, a);
    SU32(n, 0, 0x0093cbc8);   /* RavenPhysics::ConstraintFixedBody::vftable */
    phys_matrix6x6_construct(n + 0x90); phys_matrix6x6_construct(n + 0x1a0);
    SU32(n, 0x2b4, 6); SU32(n, 0x2b0, VT_VEC6);
    vhdr(n + 0x2e0, VT_VEC6, 6);
    for (uint32_t o = 0x310; o <= 0xa80; o += 0x110) phys_matrix6x6_construct(n + o);
    vhdr(n + 0xb90, VT_VEC6, 6);
    vhdr(n + 0xbc0, VT_VEC6, 6);
    phys_matrix6x6_construct(n + 0xbf0); phys_matrix6x6_construct(n + 0xd00);
    vcall0(n + 0x90, 1); vcall0(n + 0x1a0, 1); vcall0(n + 0x2e0, 0);
    vcall0(n + 0x310, 1); vcall0(n + 0x420, 1); vcall0(n + 0x530, 1); vcall0(n + 0x640, 1);
    vcall0(n + 0xbc0, 0);
    vcall0(n + 0x750, 1); vcall0(n + 0x860, 1); vcall0(n + 0x970, 1); vcall0(n + 0xa80, 1);
    vcall0(n + 0xb90, 0);
}

/* 0042f640 (ESI = node): ConstraintSlidingRotor (the wheels) */
void rx_veh_sliding_rotor_node_construct(rx_addr n) {
    const rx_addr a[13] = {n + 0xd0, n + 0x180, n + 0x250, n + 0x270, n + 0x380, n + 0x490, n + 0x520, n + 0x7b0, n + 0x5d0,
                           n + 0x620, n + 0x670, n + 0x720, n + 0x7e0};
    node_init(n, a);
    SU32(n, 0, 0x0093cb9c);   /* RavenPhysics::ConstraintSlidingRotor::vftable */
    m4x6_init(n + 0xd0);
    m4x6_init(n + 0x180);
    vhdr(n + 0x230, VT_VEC4, 4);
    vhdr(n + 0x250, VT_VEC4, 4);
    phys_matrix6x6_construct(n + 0x270); phys_matrix6x6_construct(n + 0x380);
    m6x4_init(n + 0x490);
    m4x6_init(n + 0x520);
    mhdr(n + 0x5d0, VT_M4X4, 4, 4);
    mhdr(n + 0x620, VT_M4X4, 4, 4);
    m4x6_init(n + 0x670);
    m6x4_init(n + 0x720);
    vhdr(n + 0x7b0, VT_VEC6, 6);
    vhdr(n + 0x7e0, VT_VEC4, 4);
    phys_matrix6x6_construct(n + 0x800);
    mhdr(n + 0x910, VT_M4X4, 4, 4);
    vcall0(n + 0xd0, 1); vcall0(n + 0x180, 1); vcall0(n + 0x250, 0); vcall0(n + 0x270, 1); vcall0(n + 0x380, 1);
    vcall0(n + 0x490, 1); vcall0(n + 0x520, 1); vcall0(n + 0x7b0, 0); vcall0(n + 0x5d0, 1); vcall0(n + 0x620, 1);
    vcall0(n + 0x670, 1); vcall0(n + 0x720, 1); vcall0(n + 0x7e0, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the RavenSimulation base */

/* 004db290 (ECX = vector): reserve(100) of an empty std::vector of 0x50-byte elements (_aligned_malloc(0x1f40, 16)) */
static void vec_reserve_100x50(rx_addr v) {
    if (U32(v, 0xc) && (int32_t)(U32(v, 0x14) - U32(v, 0xc)) / 0x50 >= 100) return;
    rx_addr blk = rx_crt_aligned_malloc(100 * 0x50, 16);
    rx_addr old = U32(v, 0xc);
    uint32_t n = (U32(v, 0x10) - old) / 0x50;
    if (n) { fprintf(stderr, "rxcore: 004db290 with elements: not ported\n"); abort(); }
    if (old) rx_reflex_aligned_free(old);
    SU32(v, 0x14, blk + 0x1f40);
    SU32(v, 0x10, blk + 0x50 * n);
    SU32(v, 0xc, blk);
}

/* 00430b50 (stdcall S, rays; RET 8) */
rx_addr rx_raven_simulation_construct(rx_addr S, uint32_t rays) {
    uint32_t serial = U32(0, 0x00a89be0);
    SU32(S, 8, serial);
    SU32(S, 0, 0x0093cc14);   /* RavenSimulation::vftable */
    for (uint32_t o = 4; o <= 0x18; o += 4) if (o != 8) SU32(S, o, 0);
    for (uint32_t o = 0x1c; o <= 0x38; o += 4) SU32(S, o, 0);
    for (uint32_t o = 0x3c; o <= 0x50; o += 4) SU32(S, o, 0);
    SU32(S, 0x54, 1); SU32(S, 0x58, 0); SU32(S, 0x68, 0); SU32(S, 0x5c, 0); SU32(S, 0x60, 0); SU32(S, 0x64, 0);
    veh_v4(S + 0x70, VEH_RT_W1); veh_v4(S + 0x80, VEH_RT_ZERO);
    SU32(S, 0x90, 0); SU32(S, 0x94, 0);
    SU32(S, 0xa0, 0x7f7fffffu); SU32(S, 0x98, 0x3f800000u); SU32(S, 0xa4, 0xff7fffffu); SU32(S, 0x9c, 0x3f800000u);
    SU32(0, 0x00a89be0, serial + 1);
    SU32(S, 0xa8, 0); SU32(S, 0xac, 0); SU32(S, 0xb0, 0); SU32(S, 0xb8, 6);
    mechanism_init(S + 0xd0);
    rx_vec_proxy_init(S + 0x1300);           /* 007ce6a0: the contact vector's proxy (_aligned_malloc(4, 4)) */
    SU32(S, 0x130c, 0); SU32(S, 0x1310, 0); SU32(S, 0x1314, 0);
    SU32(S, 0x1318, rays);
    rx_collision_shape_construct(S + 0x20d0, 0x20);   /* 00844110: the DynamicRays shape */
    SU32(S, 0x20f0, rays);
    SU32(S, 0x20d0, 0x00976650);             /* RavenPhysics::CollisionShapeDynamicRays::vftable */
    SU32(S, 0x20dc, 0xa1);
    SU32(S, 0x20f4, S + 0x1c20);
    SU32(S, 0x20f8, 0);
    SU32(S, 0x2370, 0); SU32(S, 0x2374, 0); SU32(S, 0x237c, 0); SU32(S, 0x2380, 0); SU32(S, 0x2384, 0); SU32(S, 0x2388, 0);
    for (uint32_t i = 0; i < 0x14; i++) rx_veh_body_rec_init(S + 0x2390 + 0x70 * i);
    for (uint32_t i = 0; i < 3; i++) rx_veh_body_rec_init(S + 0x2c50 + 0x70 * i);
    veh_v4(S + 0x2da0, VEH_RT_ZERO); veh_v4(S + 0x2db0, VEH_RT_ZERO); veh_v4(S + 0x2dc0, VEH_RT_W1); veh_v4(S + 0x2dd0, VEH_RT_ZERO);
    SU32(S, 0x2de0, 0); SU32(S, 0x2de4, 1);
    for (uint32_t i = 0; i < 2; i++) phys_pose_clear(S + 0x2df0 + 0x30 * i);
    veh_v4(S + 0x2e50, VEH_RT_W1); veh_v4(S + 0x2e60, VEH_RT_ZERO);
    SU32(S, 0x2ea4, 0x40a00000u); SU32(S, 0x2ea8, 0x40a00000u);   /* [0x935890] 5.0f */
    SU32(S, 0x2e70, 0);
    for (uint32_t o = 0x2e74; o <= 0x2e80; o += 4) SU32(S, o, 0);
    for (uint32_t o = 0x2e94; o <= 0x2ea0; o += 4) SU32(S, o, 0);
    SU32(S, 0x2eac, 0); SU32(S, 0x2eb0, 1); SU32(S, 0x2eb4, 0); SU32(S, 0x2eb8, 0); SU32(S, 0x2ebc, 0);
    veh_v4(S + 0x2ec0, VEH_RT_ZERO); veh_v4(S + 0x2ed0, VEH_RT_ZERO); veh_v4(S + 0x2ee0, VEH_RT_ZERO);
    for (uint32_t o = 0x2ef0; o <= 0x2efc; o += 4) SU32(S, o, 0x3f800000u);
    SU32(S, 0x2f00, 0);
    vec_reserve_100x50(S + 0x1300);
    return S;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the parameter block P (S + 0x2f10): 004392f0 registers the motocross simulation's tunable parameters, the parameter file
 * (0043a3f0 / 0043a560) overwrites their values. A record: +0 the name (0x40, strncpy 0x3f), +0x40 the kind, then by kind:
 * 0 float (+0x44), 1 vector (+0x50, 16 bytes), 2 curve (+0x44: count, +0x48.. x[10], +0x70.. y[10], +0x98 / +0x9c the end slopes),
 * 3 three floats (+0x44 .. +0x4c); +0x400 the count, +0 .. the record pointers in registration order. */
#include "phys_studio.h"
#include "build_vehicle_params.inc"   /* k_moto_params: the 108 records of 004392f0 in its order (offset, kind, .rdata name, default) */

void rx_veh_params_register(rx_addr P, const veh_param* t, uint32_t n) {
    SU32(P, 0x400, 0);
    for (uint32_t i = 0; i < n; i++) {
        rx_addr r = P + t[i].off;
        switch (t[i].kind) {
        case 0: SU32(r, 0x40, 0); crt_strncpy(r, t[i].name, 0x3f); SU8(r, 0x3f, 0); SU32(r, 0x44, t[i].v[0]); break;   /* 004db3a0 */
        case 1: SU32(r, 0x40, 1); crt_strncpy(r, t[i].name, 0x3f); SU8(r, 0x3f, 0);
                for (int k = 0; k < 4; k++) SU32(r, 0x50 + 4 * (uint32_t)k, t[i].v[k]);
                break;
        case 2: SU32(r, 0x44, 0); SU32(r, 0x40, 2); crt_strncpy(r, t[i].name, 0x3f); SU8(r, 0x3f, 0); break;
        case 3: for (uint32_t k = 0x44; k < 0x50; k++) SU8(r, k, 0);                                                    /* 004db3e0 */
                SU32(r, 0x40, 3); crt_strncpy(r, t[i].name, 0x3f); SU8(r, 0x3f, 0); break;
        }
        SU32(P, 4 * U32(P, 0x400), r);
        SU32(P, 0x400, U32(P, 0x400) + 1);
    }
}

/* the parameter file's XMLReader virtuals as 0043a560 / 0048bd20 call them (through the reader's vtable: build_xml's exact
 * BXMLAsset reader, including its cursor pool and current node +0x3c) */
#define XR_ROOT(R)              veh_vcall((R), 0x04, 0, 0, 0, 0)                      /* vf1 */
#define XR_FIRST(R, n, nm, out) veh_vcall((R), 0x20, 3, (n), (nm), (out))             /* vf8 (node, name, out cursor*) */
#define XR_RELEASE(R, c)        veh_vcall((R), 0x24, 1, (c), 0, 0)                    /* vf9 */
#define XR_NEXT(R, c)           veh_vcall((R), 0x28, 1, (c), 0, 0)                    /* vf10 */
#define XR_NAME(R, n, buf, sz)  veh_vcall((R), 0xa0, 3, (n), (buf), (sz))             /* vf40 */
#define XR_VEC3(R, n, dst, k)   veh_vcall((R), 0xc0, 3, (n), (dst), (k))              /* vf48 */
#define XR_FLOATS(R, n, dst, k) veh_vcall((R), 0xcc, 3, (n), (dst), (k))              /* vf51 */
#define XR_ATTR(R, n, nm)       veh_vcall((R), 0xe8, 2, (n), (nm), 0)                 /* vf58 */
#define XR_AFLOATS(R, e, dst, k) veh_vcall((R), 0x130, 3, (e), (dst), (k))            /* vf76 */
#define XR_AINTS(R, e, dst, k)  veh_vcall((R), 0x134, 3, (e), (dst), (k))             /* vf77 */
#define XR_ASTRING(R, e, b, sz) veh_vcall((R), 0x13c, 3, (e), (b), (sz))              /* vf79 */

/* 0048bd20 (ESI = reader, EDI = curve, s0 = node; RET 4): count "num_knots", "mleft" / "mright", then count pairs (x, y) */
void rx_veh_read_curve(rx_addr R, rx_addr node, rx_addr C) {
    XR_AINTS(R, XR_ATTR(R, node, 0x0093b484u), C, 1);            /* "num_knots" */
    XR_AFLOATS(R, XR_ATTR(R, node, 0x0093b490u), C + 0x54, 1);   /* "mleft" */
    XR_AFLOATS(R, XR_ATTR(R, node, 0x0093b498u), C + 0x58, 1);   /* "mright" */
    RX_FRAME_ENTER(F, 0x50);   /* the original's 20-float buffer on its frame */
    XR_FLOATS(R, node, F, 2 * U32(C, 0));
    for (int32_t i = 0; i < I32(C, 0); i++) {
        SU32(C, 4 + 4 * (uint32_t)i, veh_x87_copy(U32(F, 8 * (uint32_t)i)));
        SU32(C, 0x2c + 4 * (uint32_t)i, veh_x87_copy(U32(F, 8 * (uint32_t)i + 4)));
    }
    RX_FRAME_LEAVE(F, 0x50);
}

/* the inlined strcmp of two game strings: 0 when equal */
static int gstrcmp(rx_addr a, rx_addr b) {
    for (uint32_t k = 0;; k++) {
        uint8_t x = U8(a, k), y = U8(b, k);
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}

/* 0043a560 (stdcall P, reader; RET 8): the root must have type="mx" (not "mx_bike"); a cursor walks the root's children. For the
 * current child (its name in NAME), the parameters are tried in registration order (_stricmp); a match reads the value(s) and
 * advances the cursor past the following siblings of the same name (kinds 3 / 4 / 5 read up to three of them, the missing ones get
 * the first value); NAME takes NEXT, the last name read (which keeps its old contents when the cursor ran out), and the remaining
 * parameters are tried with the new child. After the pass the walk restarts the parameters on the current child, or advances the
 * cursor when nothing matched; the cursor goes back to the pool at the end. The frame mirrors the original's (cursor +0x1c, NAME
 * +0x48, NEXT +0x848, both 0x800; NEXT starts with what the game stack held). */
/* the vehicle classes' copies (0043a560 Motocross "mx" but not "mx_bike", 0048b680 ATV "atv") differ only in the root's type */
uint32_t rx_veh_params_apply_typed(rx_addr P, rx_addr R, rx_addr type, rx_addr reject) {
    if (!XR_ROOT(R)) return 0;
    RX_FRAME_ENTER(F, 0x1050);
    const rx_addr CUR = F + 0x1c, NAME = F + 0x48, NEXT = F + 0x848;
    uint32_t ret = 0;
    XR_NAME(R, XR_ROOT(R), NAME, 0x800);
    XR_ASTRING(R, XR_ATTR(R, XR_ROOT(R), 0x0093b470u), NAME, 0x40);      /* "type" */
    if (gstrcmp(NAME, type) != 0 || (reject && gstrcmp(NAME, reject) == 0)) goto out;
    ret = 1;
    rx_addr node = XR_FIRST(R, XR_ROOT(R), 0, CUR);
    while (node) {
        XR_NAME(R, node, NAME, 0x800);
        uint32_t matched = 0;
        for (uint32_t i = 0; i < U32(P, 0x400); i++) {
            rx_addr rec = U32(P, 4 * i);
            if (crt_stricmp(NAME, rec) != 0) continue;
            uint32_t kind = U32(rec, 0x40), k = 0;
            if (kind > 5) continue;
            if (kind == 0 || kind == 1 || kind == 2) {   /* one value; the same-named siblings are skipped */
                if (kind == 0) XR_FLOATS(R, node, rec + 0x44, 1);
                else if (kind == 1) XR_VEC3(R, node, rec + 0x50, 1);
                else rx_veh_read_curve(R, node, rec + 0x44);
                do {
                    node = XR_NEXT(R, U32(CUR, 0));
                    if (!node) break;
                    XR_NAME(R, node, NEXT, 0x800);
                } while (gstrcmp(NAME, NEXT) == 0);
            } else {                                       /* up to three values (3 floats, 4 vectors, 5 curves) */
                const rx_addr first = kind == 4 ? rec + 0x50 : rec + 0x44;
                const uint32_t stride = kind == 3 ? 4 : kind == 4 ? 0x10 : 0x5c;
                do {
                    if (k < 3) {
                        rx_addr dst = first + stride * k;
                        if (kind == 3) XR_FLOATS(R, node, dst, 1);
                        else if (kind == 4) XR_VEC3(R, node, dst, 1);
                        else rx_veh_read_curve(R, node, dst);
                    }
                    k++;
                    node = XR_NEXT(R, U32(CUR, 0));
                    if (!node) break;
                    XR_NAME(R, node, NEXT, 0x800);
                } while (gstrcmp(NAME, NEXT) == 0);
                for (uint32_t j = k; j < 3; j++) {   /* the missing values: copies of the first */
                    if (kind == 3) SU32(first, 4 * j, veh_x87_copy(U32(first, 0)));   /* fld / fstp */
                    else if (kind == 4) veh_v4(first + 0x10 * j, first);              /* movaps */
                    else veh_copy(first + 0x5c * j, first, 0x5c);                     /* rep movsd */
                }
            }
            crt_strncpy(NAME, NEXT, 0x7ff);
            SU8(NAME, 0x7ff, 0);
            matched = 1;
        }
        if (!matched && node) node = XR_NEXT(R, U32(CUR, 0));
    }
    XR_RELEASE(R, U32(CUR, 0));
out:
    RX_FRAME_LEAVE(F, 0x1050);
    return ret;
}
static uint32_t params_apply(rx_addr P, rx_addr R) { return rx_veh_params_apply_typed(P, R, 0x0093b478u /* "mx" */, 0x0093b47cu /* "mx_bike" */); }

/* 0043a3f0 (EDI = P, s0 = the engine class 1..4; RET 4): the simulation's parameter file "mx125_vehicle" / "mx250_vehicle" /
 * "mx250f_vehicle" / "mx450f_vehicle" (a bxml asset), applied with 0043a560 while the handle's loading byte is set; then the tuning
 * registry's one-time init (00790400, guard bit 1 of [0x00d72ad4]) and its callback [0x00d92cd4] = 0x004391a0 */
void rx_veh_tuning_registry_init(void);
static void params_load_file(rx_addr P, uint32_t cls) {
    static const char* names[4] = {"mx125_vehicle", "mx250_vehicle", "mx250f_vehicle", "mx450f_vehicle"};
    const char* nm = cls >= 1 && cls <= 4 ? names[cls - 1] : NULL;
    if (!nm) { fprintf(stderr, "rxcore: 0043a3f0 engine class %u\n", cls); abort(); }
    rx_addr h = rx_load_find_handle_c("bxml", nm);
    SU32(h, 0x4c, 1);
    rx_addr a = U32(h, 0x44);
    params_apply(P, a ? a - 0x460 : 0);
    rx_ps_params_loaded(P, (int)cls - 1);   /* PHYSICS STUDIO (port, phys_studio.h): a config's values over the file's; nothing without one */
    SU32(h, 0x4c, 0);
    if (!(U32(0, 0x00d72ad4) & 1)) { SU32(0, 0x00d72ad4, U32(0, 0x00d72ad4) | 1); rx_veh_tuning_registry_init(); }
    SU32(0, 0x00d92cd4, 0x004391a0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * masses and derived values */

/* Float_Abs 00401000 + the inlined [0, 2 pi) wrap of the cosine polynomials: 0043b750's pole angles */
static float wrap_abs(float a) {
    float x = bike_fabs(a);
    return phys_sub(x, phys_mul((float)rx_cvtt(phys_mul(x, 0.15915493667125702f /* 009774a0 */)), 6.2831854820251465f /* 0097749c */));
}

/* 0043b750 (ESI = S): the mechanism's masses / inertias from the parameters, the fork's rest orientation and the derived
 * constants, then vf35 */
void rx_sim_moto_setup_masses(rx_addr S) {
    rx_addr M = S + 0xd0;
    veh_v4(M, S + 0x3370);                      /* VehicleSim_Gravity */
    rx_veh_mechanism_set_mass(M, F32(S, 0x37c4), 0);
    rx_veh_mechanism_set_mass(M, F32(S, 0x380c), 1);
    rx_veh_mechanism_set_mass(M, F32(S, 0x3854), 2);
    rx_veh_mechanism_set_mass(M, F32(S, 0x389c), 3);
    static const uint32_t inertia[4] = {0x38f0, 0x3950, 0x39b0, 0x3a10};
    for (uint32_t i = 0; i < 4; i++) {          /* the inertia vectors and their reciprocals (divps from the 1-splat) */
        veh_v4(M + 0x680 + 0xa0 * i, S + inertia[i]);
        rx_st4(M + 0x690 + 0xa0 * i, v4_div(rx_ld4(0x00d61650), rx_ld4(S + inertia[i])));
    }
    veh_v4(S + 0x6060, S + 0x5540);
    veh_v4(S + 0x6070, VEH_RT_W1);
    /* the fork quaternion from the rake angle a = [S + 0x3e14]: (0, sin(a / 2)?, 0, cos) through the two cosine polynomials */
    float a = F32(S, 0x3e14);
    float x = wrap_abs(phys_sub(1.5707963705062866f /* 00977508 */, phys_mul(a, 0.5f /* 009322cc */)));
    float u = phys_sub(3.1415927410125732f, x), u2s = phys_mul(u, u), u4s = phys_mul(u2s, u2s);
    float y = wrap_abs(phys_mul(a, -0.5f /* 009774b0 */));
    float w = phys_sub(3.1415927410125732f, y), w2 = phys_mul(w, w), w4 = phys_mul(w2, w2);
    const float k6 = rx_bf(0xb46beed0u), k4 = rx_bf(0x37cb076bu), k3 = rx_bf(0x3d2aa372u), k2 = rx_bf(0xbab5a662u);
    const float k1 = rx_bf(0xbeffff3du), k0 = rx_bf(0x3f7ffff3u) /* k_CosPolynomial 00935d3c */;
    /* xmm2 = ((w2 k6 + k4) w4 + (w2 k2 + k3)) w4 + (w2 k1 + k0); lane 1 = -0.0 - that */
    float cw = phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(w2, k6), k4), w4), phys_add(phys_mul(w2, k2), k3)), w4),
                        phys_add(phys_mul(w2, k1), k0));
    float sy = phys_sub(-0.0f, cw);
    float cu = phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2s, k6), k4), u4s), phys_add(phys_mul(u2s, k2), k3)), u4s),
                        phys_add(phys_mul(u2s, k1), k0));
    /* unpcklps (0043b9bf..c5): (0, sy, 0, 0), (cu, 0, 0, 0) -> the quaternion (cu, 0, 0, sy) */
    rx_st4(S + 0x8a70, v4(cu, 0.0f, 0.0f, sy)); rx_st4(S + 0x8a80, v4(cu, 0.0f, 0.0f, sy));
    veh_v4(S + 0x8a90, VEH_RT_W1);
    rx_st4(S + 0x6e70, v4_sub(rx_ld4(S + 0x3dc0), rx_ld4(S + 0x3770)));
    rx_v4 qa = rx_ld4(S + 0x8a70);
    rx_st4(S + 0x6e80, bike_quat_mul(qa, bike_quat_mul(v4_xyz(rx_ld4(0x00d616a0)), bike_quat_conj(qa))));
    veh_v4(S + 0x6e90, 0x00d615c0);
    rx_st4(S + 0x6ea0, bike_quat_mul(qa, bike_quat_mul(v4_xyz(rx_ld4(0x00d61680)), bike_quat_conj(qa))));
    veh_v4(S + 0x6eb0, S + 0x8a80);
    SF32(S, 0x6ec0, phys_sub(-0.0f, F32(S, 0x3e5c)));
    {
        rx_v4 s2 = v4_mul(v4_splat(F32(S, 0x4224)), rx_ld4(0x00d61680));
        rx_v4 s0 = v4_splat(F32(S, 0x4054));
        rx_v4 d = v4_sub(v4_sub(rx_ld4(S + 0x41d0), rx_ld4(S + 0x3770)), s2);
        rx_st4(S + 0x77d0, d);
        rx_v4 e = v4_shuf(s0, d, 0xe5);
        e.v[0] = d.v[0];
        rx_st4(S + 0x77d0, e);
    }
    veh_v4(S + 0x77e0, 0x00d616a0); veh_v4(S + 0x77f0, 0x00d615c0); veh_v4(S + 0x7800, 0x00d61680); veh_v4(S + 0x7810, VEH_RT_W1);
    uint32_t c3634 = U32(S, 0x3634);
    rx_st4(S + 0x88f0, v4_splat(F32(S, 0x42b4)));
    SF32(S, 0x7820, phys_sub(-0.0f, F32(S, 0x400c)));
    rx_st4(S + 0x87a0, v4_splat(F32(S, 0x48ec)));
    phys_lag_filter_set(S + 0x5c70, 0.0f /* xmm1 left 0 by the xorps at 0043bc27 */, rx_bf(veh_x87_copy(c3634)));   /* 0042d670 */
    rx_vehicle_set_limits(0x009cb4b0, S + 0x8b40, rx_bf(veh_x87_copy(U32(S, 0x371c))));   /* 0042d8c0 */
    phys_lag_filter_set(S + 0x94, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x33c4))));
    phys_lag_filter_set(S + 0x5c84, 0.0f, rx_bf(veh_x87_copy(U32(0, 0x009d3bfc))));
    phys_lag_filter_set(S + 0x5c98, 0.0f, rx_bf(0x3e4ccccdu));
    phys_lag_filter_set(S + 0x5cac, 0.0f, rx_bf(0x3e800000u));
    rx_vehicle_set_limits(VEH_ZERO3, S + 0x8e40, rx_bf(0x3dcccccdu));
    rx_vehicle_set_limits(VEH_ZERO3, S + 0x8e4c, rx_bf(0x3dcccccdu));
    phys_lag_filter_set(S + 0x8b78, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x5950))));
    phys_lag_filter_set(S + 0x8b8c, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x5954))));
    phys_lag_filter_set(S + 0x8ba0, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x5958))));
    phys_lag_filter_set(S + 0x8bdc, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x59a4))));
    SU32(S, 0x8ab8, veh_x87_copy(U32(S, 0x52ac)));
    SU32(S, 0x2eb0, F32(S, 0x52f4) > 0.0f ? 1 : 0);
    float e = phys_mul(F32(S, 0x54ec), F32(S, 0x533c));
    SF32(S, 0x8ac4, phys_mul(F32(S, 0x5384), e)); SF32(S, 0x8ac8, phys_mul(F32(S, 0x53cc), e)); SF32(S, 0x8acc, phys_mul(F32(S, 0x5414), e));
    SF32(S, 0x8ad0, phys_mul(F32(S, 0x545c), e)); SF32(S, 0x8ad4, phys_mul(F32(S, 0x54a4), e));
    veh_vcall(S, 0x8c, 0, 0, 0, 0);   /* vf35 */
}

/* 0042d9b0 (ECX = id, s0 = the wheel contact record W (0x150); RET 4) */
void rx_veh_wheel_record_init(rx_addr W, uint32_t id) {
    SU32(W, 0, id);
    veh_v4(W + 0x10, VEH_RT_ZERO); veh_v4(W + 0x30, 0x00d615c0); veh_v4(W + 0x40, 0x00d616a0); veh_v4(W + 0x50, VEH_RT_ZERO);
    veh_v4(W + 0x60, VEH_RT_ZERO);
    rx_st4(W + 0x70, v4_sub(v4_zero(), rx_ld4(0x00d616a0)));
    veh_v4(W + 0x80, VEH_RT_ZERO);
    rx_st4(W + 0x90, v4_sub(v4_zero(), rx_ld4(0x00d616a0)));
    veh_v4(W + 0xa0, 0x00d61650); veh_v4(W + 0xb0, 0x00d61650);
    SU32(W, 0xc0, 1); SU32(W, 0xc4, 1);
    veh_v4(W + 0xd0, VEH_RT_ZERO);
    SU32(W, 0xe0, 0); SU32(W, 0xe4, 0x3dcccccdu); SU32(W, 0xec, 0x7f7fffffu); SU32(W, 0xf0, 0xff7fffffu); SU32(W, 0xe8, 0x3f800000u);
    SU32(W, 0xf4, 0); SU32(W, 0xf8, 1); SU32(W, 0xfc, 0); SU32(W, 0x100, 0); SU32(W, 0x104, 0);
    SU32(W, 0x110, 0x3f000000u); SU32(W, 0x114, 0x3f000000u);
    veh_v4(W + 0x120, 0x00d61680); veh_v4(W + 0x130, 0x00d616a0); veh_v4(W + 0x140, 0x00d615c0);
}

/* 0042de50 (ECX = r): (0,0,0,1) and three zero floats */
void rx_veh_quat_rec_init(rx_addr r) { veh_v4(r, VEH_RT_W1); SU32(r, 0x10, 0); SU32(r, 0x14, 0); SU32(r, 0x18, 0); }

/* 0043ac40 (stdcall S, vehicle block (V + 0x16e0), engine class; RET 0xc): RavenSimulationMotocross */
rx_addr rx_sim_moto_construct(rx_addr S, rx_addr vblock, uint32_t cls) {
    rx_raven_simulation_construct(S, 0xc);
    SU32(S, 0, 0x0093c8a4);   /* RavenSimulationMotocross::vftable */
    rx_veh_params_register(S + 0x2f10, k_moto_params, sizeof k_moto_params / sizeof k_moto_params[0]);
    SU32(S, 0x5c30, vblock);
    /* seven RangedFloats {0, rate, 1, FLT_MAX, -FLT_MAX} at +0x5c34 (rates 1, 1, 1, 1, k_MouseSensitivityPerPercent, 0.2f, 0.25f) */
    const uint32_t rates[7] = {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, U32(0, 0x009d3bfc), 0x3e4ccccdu, 0x3e800000u};
    for (uint32_t i = 0; i < 7; i++) {
        rx_addr r = S + 0x5c34 + 0x14 * i;
        SU32(r, 0, 0); SU32(r, 4, rates[i]); SU32(r, 0xc, 0x7f7fffffu); SU32(r, 0x10, 0xff7fffffu); SU32(r, 8, 0x3f800000u);
    }
    SU32(S, 0x5cc0, 0); SU32(S, 0x5cc4, 0);
    rx_veh_main_body_node_construct(S + 0x5cd0);
    rx_veh_fixed_body_node_construct(S + 0x5ff0);
    rx_veh_sliding_rotor_node_construct(S + 0x6e00);
    rx_veh_sliding_rotor_node_construct(S + 0x7760);
    SU32(S, 0x80c0, 0); SU32(S, 0x80c4, 0);
    for (uint32_t i = 0; i < 0x1e; i++) { SU32(S, 0x8680 + 4 * i, 0xffffffffu); SU32(S, 0x86f8 + 4 * i, 0xffffffffu); }
    SU32(S, 0x8780, 0); SU32(S, 0x8784, 0);
    rx_veh_wheel_record_init(S + 0x8790, 3);
    rx_veh_wheel_record_init(S + 0x88e0, 2);
    veh_v4(S + 0x8a40, VEH_RT_ZERO); veh_v4(S + 0x8a50, VEH_RT_ZERO);
    SU32(S, 0x8ad8, 0x3f000000u); SU32(S, 0x8ae4, 0x3e99999au); SU32(S, 0x8ac0, 0);
    for (uint32_t o = 0x8aa0; o <= 0x8ab4; o += 4) SU32(S, o, 0);
    SU32(S, 0x8abc, 0); SU32(S, 0x8adc, 0); SU32(S, 0x8ae0, 0); SU32(S, 0x8ae8, 0x3e19999au); SU32(S, 0x8aec, 0);
    veh_v4(S + 0x8af0, VEH_RT_ZERO); veh_v4(S + 0x8b00, VEH_RT_ZERO); veh_v4(S + 0x8b10, VEH_RT_ZERO);
    veh_v4(S + 0x8b20, VEH_RT_W1); veh_v4(S + 0x8b30, VEH_RT_W1);
    rx_veh_ranged3_alloc(S + 0x8b40, 0x009cb4b0, 0x3f800000u /* fld1 */);
    SU32(S, 0x8b70, 0); SU32(S, 0x8b74, 0);
    for (uint32_t i = 0; i < 3; i++) {   /* RangedFloats {0, 1, 1, FLT_MAX, -FLT_MAX} at +0x8b78 / +0x8b8c / +0x8ba0 */
        rx_addr r = S + 0x8b78 + 0x14 * i;
        SU32(r, 0, 0); SU32(r, 4, 0x3f800000u); SU32(r, 0xc, 0x7f7fffffu); SU32(r, 0x10, 0xff7fffffu); SU32(r, 8, 0x3f800000u);
    }
    for (uint32_t o = 0x8bb4; o <= 0x8bcc; o += 4) SU32(S, o, 0);
    SU32(S, 0x8bd4, 0); SU32(S, 0x8bd8, 0);
    { rx_addr r = S + 0x8bdc; SU32(r, 0, 0); SU32(r, 4, 0x3f800000u); SU32(r, 0xc, 0x7f7fffffu); SU32(r, 0x10, 0xff7fffffu); SU32(r, 8, 0x3f800000u); }
    veh_v4(S + 0x8bf0, VEH_RT_ZERO); veh_v4(S + 0x8c00, VEH_RT_W1); veh_v4(S + 0x8c10, VEH_RT_W1);
    SU32(S, 0x8c20, 0); SU32(S, 0x8c24, 0); SU32(S, 0x8c28, 0); SU32(S, 0x8c2c, 1);
    for (uint32_t i = 0; i < 3; i++) rx_veh_quat_rec_init(S + 0x8c70 + 0x20 * i);
    for (uint32_t i = 0; i < 3; i++) rx_veh_quat_rec_init(S + 0x8ce0 + 0x20 * i);
    SU32(S, 0x8d40, 0); SU32(S, 0x8d44, 0); SU32(S, 0x8d48, 0); SU32(S, 0x8d50, 0); SU32(S, 0x8d58, 0); SU32(S, 0x8d5c, 0);
    SU32(S, 0x8d60, 0); SU32(S, 0x8d6c, 0); SU32(S, 0x8d4c, 0); SU32(S, 0x8d54, 0); SU32(S, 0x8d64, 0); SU32(S, 0x8d68, 0);
    SU32(S, 0x8d70, 0); SU32(S, 0x8d74, 0); SU32(S, 0x8d78, 0x3f800000u);
    veh_v4(S + 0x8d80, VEH_RT_ZERO);
    SU32(S, 0x8d98, 0); SU32(S, 0x8d9c, 0); SU32(S, 0x8da0, 0); SU32(S, 0x8db8, 0);
    SU32(S, 0x8da4, 1);
    for (uint32_t o = 0x8da8; o <= 0x8db4; o += 4) SU32(S, o, 0);
    SU32(S, 0x8e00, 0);
    rx_collision_shape_construct(S + 0x8e10, 0x400);   /* 00844110 */
    SU32(S, 0x8e10, 0x00976750);                       /* RavenPhysics::CollisionShapeCircle::vftable */
    veh_v4(S + 0x8e30, VEH_RT_ZERO);
    rx_veh_ranged3_alloc(S + 0x8e40, VEH_ZERO3, 0x3f800000u);
    rx_veh_ranged3_alloc(S + 0x8e4c, VEH_ZERO3, 0x3f800000u);
    params_load_file(S + 0x2f10, cls);
    /* the mechanism's four nodes: index / parent */
    rx_addr M = S + 0xd0;
    static const uint32_t node[4] = {0x5cd0, 0x5ff0, 0x6e00, 0x7760};
    for (uint32_t i = 0; i < 4; i++) {
        SU32(M, 0x1c, U32(M, 0x1c) + 1);
        SU32(M, 0xca0 + 4 * i, S + node[i]);
        SU32(S + node[i], 8, i);
        SU32(S + node[i], 0xc, i ? 0 : 0xffffffffu);
    }
    rx_veh_mechanism_bind(M);
    rx_sim_moto_setup_masses(S);
    /* the stunt poses: six fixed quaternions / angle triples (0043b271..) */
    static const uint32_t q8c70[3][4] = {{0x3c92556d, 0x3eae0ded, 0xbeba49a1, 0x3f5df55a}, {0xbeab9000, 0x3edee5ac, 0xbecec8b4, 0x3f3b42ff},
                                         {0x3ed55f9a, 0x3e93b0c4, 0xbe80c261, 0x3f531540}};
    static const uint32_t q8ce0[3][4] = {{0x3c92556d, 0xbeae0ded, 0x3eba49a1, 0x3f5df55a}, {0xbeab9000, 0xbedee5ac, 0x3ecec8b4, 0x3f3b42ff},
                                         {0x3ed55f9a, 0xbe93b0c4, 0x3e80c261, 0x3f531540}};
    static const uint32_t a3[3][3] = {{0x3edf66f1, 0x3e860a91, 0x3eb2b8c1}, {0x3f060a91, 0x3f060a91, 0x3f060a91}, {0x3eb2b8c1, 0x3e860a91, 0x3eb2b8c1}};
    for (uint32_t i = 0; i < 3; i++) {
        for (uint32_t k = 0; k < 4; k++) { SU32(S, 0x8c70 + 0x20 * i + 4 * k, q8c70[i][k]); SU32(S, 0x8ce0 + 0x20 * i + 4 * k, q8ce0[i][k]); }
        for (uint32_t k = 0; k < 3; k++) { SU32(S, 0x8c80 + 0x20 * i + 4 * k, a3[i][k]); SU32(S, 0x8cf0 + 0x20 * i + 4 * k, a3[i][k]); }
    }
    return S;
}

/* the registry: the transliterations above that are whole original functions (their conventions in the comments) */
RX_FUNCTION(0x0085c200, mechanism_init);
RX_FUNCTION(0x0085c3f0, rx_veh_mechanism_bind);
RX_FUNCTION(0x0085c470, rx_veh_mechanism_set_mass);
RX_FUNCTION(0x0042f460, node_init);
RX_FUNCTION(0x0042f520, rx_veh_main_body_node_construct);
RX_FUNCTION(0x0042fae0, rx_veh_fixed_body_node_construct);
RX_FUNCTION(0x0042f640, rx_veh_sliding_rotor_node_construct);
RX_FUNCTION(0x004db290, vec_reserve_100x50);
RX_FUNCTION(0x00430b50, rx_raven_simulation_construct);
RX_FUNCTION(0x0048bd20, rx_veh_read_curve);
RX_FUNCTION(0x0042d9b0, rx_veh_wheel_record_init);
RX_FUNCTION(0x0042de50, rx_veh_quat_rec_init);
RX_FUNCTION(0x0043ac40, rx_sim_moto_construct);
RX_FUNCTION(0x0043a560, params_apply);
RX_FUNCTION(0x0043a3f0, params_load_file);
RX_FUNCTION(0x0043b750, rx_sim_moto_setup_masses);
