/* build_vehicle_atv.c -- construction unit 6 (vehicles, core/notes/construction.md section 15): the ATV (factory 1). Hand
 * transliterations of the disassembly:
 *   0053f790 RavenVehicleATV_Alloc, 0047c4a0 RavenVehicleATV_Construct, 0047c7f0 RavenVehicleATV::vf28 (the simulation, the proxy
 *   0047cdf0, the 27 named nodes, RavenVehicle::vf28, vf49, the terrain tracks slots), 00486760 RavenVehicleATV::vf49,
 *   0048bde0 RavenSimulationATV_Construct (0x9bd0: RavenSimulation with 13 rays, the 103 parameters of 00489cb0, 004c0230, six
 *   mechanism nodes: main body, fixed body, four sliding rotors (the wheels), the circle shape, four wheel contact records, the
 *   parameter file 0048b5d0 "atv_vehicle" through 0048b680, the masses 0048c750, the stunt poses), RavenSimulationATV::vf18 004da440
 *   (0084f440).
 * The vehicle V (6000 bytes): the RavenVehicle base (0045cec0) and +0x1700 the simulation (also +0x234), +0x1750 the proxy shape's
 * four tire children, +0x1528 .. +0x1580 the model's named nodes, +0x1584 .. +0x1590 the terrain tracks slots. The simulation S:
 * +0x2f10 the parameter block, +0x5880 a 0x100 record (004c0230), +0x5a30 / +0x5d50 / +0x6b60 / +0x74c0 / +0x7e20 / +0x8780 the six
 * mechanism nodes (the bodies: frame, rider, the four wheels), +0x9100 the circle shape, +0x9140 + 0x150 k the wheel contact records,
 * +0x9880 / +0x98f0 the stunt poses. */
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "build_vehicle_int.h"
#include "build_world.h"
#include "build_load.h"
#include "build_alloc.h"
#include "build_rider.h"
#include "anim.h"
#include "phys_vec.h"
#include "phys_misc.h"
#include "phys_body.h"
#include "loop_vehicle.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

#include "phys_studio.h"
#include "build_vehicle_atv_params.inc"   /* k_atv_params: the 103 records of 00489cb0 */

static void atv_fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore: ATV: %s (%08x)\n", what, a); abort(); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * the vehicle */

/* 0053f790: _aligned_malloc(6000, 16) under the allocator's lock */
rx_addr rx_vehicle_atv_alloc(void) { return rx_crt_aligned_malloc(6000, 16); }

/* 0047c4a0 (ESI = V) */
rx_addr rx_vehicle_atv_construct(rx_addr v) {
    rx_raven_vehicle_construct(v, 1);
    SU32(v, 0x15bc, U32(0, 0x009cd970));
    SU32(v, 0x15c0, U32(0, 0x009cd9a4));
    SU32(v, 0x15c4, 0x3f800000u);
    SU32(v, 0x15c8, U32(0, 0x009c4068));   /* kTerrainSunScale */
    SU32(v, 0, 0x0093d0ec);                 /* RavenVehicleATV::vftable */
    SU32(v, 0x90, 0x0093d1f0); SU32(v, 0x94, 0x0093d204); SU32(v, 0x9c, 0x0093d244);
    for (uint32_t o = 0x1584; o <= 0x1590; o += 4) SU32(v, o, 0xffffffffu);
    for (uint32_t o = 0x1594; o <= 0x15b8; o += 4) SU32(v, o, 0);
    veh_fill(v + 0x15d0, 0x20, 0);          /* two zero splats */
    SU32(v, 0x15f0, 0); SU32(v, 0x15f4, 0);
    rx_veh_clamped_init(v + 0x15f8, 0, 0x3dcccccdu /* 0.1 (x87 copy) */, 0x7f7fffffu);   /* 0042d630 */
    rx_veh_clamped_init(v + 0x160c, 0, 0x3dcccccdu, 0x7f7fffffu);
    SU32(v, 0x1620, 0x3f800000u); SU32(v, 0x1624, 0x3f800000u);
    for (uint32_t o = 0x1628; o <= 0x1654; o += 4) SU32(v, o, 0);
    SU32(v, 0x1700, 0); SU32(v, 0x1704, 0); SU32(v, 0x1710, 0); SU32(v, 0x1760, 0);
    for (uint32_t o = 0x1750; o <= 0x175c; o += 4) SU32(v, o, 0);
    for (uint32_t k = 0; k < 0x19; k++) {   /* 25 records: ids -1, zero vectors */
        SU32(v, 0x14c0 + 4 * k, 0xffffffffu);
        veh_zero3(v + 0x1330 + 0x10 * k);
    }
    SU32(v, 0x16d4, 0xbfc00000u);           /* -1.5 */
    SU32(v, 0x16d8, U32(0, 0x009cc37c));
    SU32(v, 0x16dc, U32(0, 0x009d3bfc));    /* k_MouseSensitivityPerPercent */
    SU32(v, 0x16e0, 0x41f00000u);           /* 30.0 */
    SU32(v, 0x16e4, 0x40a00000u);           /* 5.0 */
    SU32(v, 0x16e8, 0x3e4ccccdu);           /* 0.2 */
    SU32(v, 0x16ec, 0x3da3d70au);           /* 0.08 */
    for (uint32_t o = 0x1660; o <= 0x166c; o += 4) SU32(v, o, 0);
    SU32(v, 0x16f0, 0x40800000u);           /* 4.0 */
    SU32(v, 0x16f4, U32(0, 0x009d78dc));    /* ModelDrawDistance_RadiusLimit50 */
    return v;
}

/* RavenVehicleATV::vf49 00486760 (thiscall V, value; RET 4): the simulation's +0xb0 */
void rx_vehicle_atv_vf49(rx_addr V, uint32_t value) { SU32(U32(V, 0x1700), 0xb0, value); }

/* 0047cdf0 (stdcall V; RET 4): the ProxyObject of "RonR_ATVProxyCollision" with the "ATV" physics material; the proxy shape's four
 * tire children (FLTire / FRTire / RLTire / RRTire) at V + 0x1750, flagged 4 */
static void atv_create_proxy(rx_addr V) {
    rx_addr h = rx_load_find_handle_c("collis", "RonR_ATVProxyCollision");   /* Database vf27 (name, 'collis', 1) */
    uint32_t mat = rx_phys_props_find(rx_phys_props_get(), "ATV");          /* 00850bf0 on the registry 0x00da6ad0 */
    RX_FRAME_ENTER(F, 0x40);
    rx_addr out = F + 0xc;
    rx_proxy_object_create(U32(U32(V, 0x188), 0x20134), out, mat, h, h);
    rx_addr t = U32(out, 4);
    if (U32(V, 0x17c) != t) {
        rx_addr old = U32(V, 0x17c);
        if (old) { SU32(old, 0, U32(old, 0) - 1); if (!U32(old, 0)) atv_fatal("0047cdf0: proxy released (0042b0f0)", old); }
        SU32(V, 0x17c, t);
        if (t) SU32(t, 0, U32(t, 0) + 1);
    }
    SU32(out, 0, 0x0093a650);
    if (t) { SU32(t, 0, U32(t, 0) - 1); if (!U32(t, 0)) atv_fatal("0047cdf0: proxy freed (0042af70)", t); }
    rx_addr rec = phys_body_record(U32(V, 0x17c));
    SU32(rec, 0x34, V);
    rec = phys_body_record(U32(V, 0x17c));
    rx_addr shape = U32(rec, 0x2c);
    static const rx_addr names[4] = {0x0093bfd4, 0x0093bfdc, 0x0093bfe4, 0x0093bfec};   /* FLTire FRTire RLTire RRTire */
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t idx = veh_vcall(shape, 0x20, 1, names[i], 0, 0);   /* vf8: the child by name */
        SU32(V, 0x1750 + 4 * i, idx);
        if (idx >= (U32(shape, 0x30) - U32(shape, 0x2c)) >> 2) atv_fatal("0047cdf0: child index", idx);
        rx_addr child = U32(U32(shape, 0x2c), 4 * idx);
        SU32(child, 0x14, U32(child, 0x14) | 4);
    }
    SU8(V, 0x174, 1);
    RX_FRAME_LEAVE(F, 0x40);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the simulation */

/* 004c0230 (s0 = the record; RET 4): a 0x100 record: zeros, two identity quaternions, eleven zero vectors */
void rx_veh_rec_init(rx_addr r) {
    for (uint32_t o = 0; o <= 0x18; o += 4) SU32(r, o, 0);
    veh_v4(r + 0x20, VEH_RT_W1); veh_v4(r + 0x30, VEH_RT_W1);
    veh_v4(r + 0x40, VEH_RT_ZERO);
    SU32(r, 0x50, 0); SU32(r, 0x54, 0);
    for (uint32_t o = 0x60; o <= 0xd0; o += 0x10) veh_v4(r + o, VEH_RT_ZERO);
    for (uint32_t o = 0xe0; o <= 0xfc; o += 4) SU32(r, o, 0);
}

/* the parameter file: 0048b5d0 (EDI = P): "atv_vehicle" (bxml, Database vf27) applied with 0048b680 (type "atv") while the handle's
 * loading byte is set; the tuning registry's one-time init (00790400, guard bit 1 of [0x00d72ad4]) and the callback [0x00d92cd4] */
void rx_veh_tuning_registry_init(void);
static void atv_params_load(rx_addr P) {
    rx_addr h = rx_load_find_handle_c("bxml", "atv_vehicle");
    SU32(h, 0x4c, 1);
    rx_addr a = U32(h, 0x44);
    rx_veh_params_apply_typed(P, a ? a - 0x460 : 0, 0x0093c560u /* "atv" */, 0);
    rx_ps_params_loaded(P, RX_PS_ATV);   /* PHYSICS STUDIO (port, phys_studio.h) */
    SU32(h, 0x4c, 0);
    if (!(U32(0, 0x00d72ad4) & 1)) { SU32(0, 0x00d72ad4, U32(0, 0x00d72ad4) | 1); rx_veh_tuning_registry_init(); }
    SU32(0, 0x00d92cd4, 0x004391a0);
}

/* 0048c750 (ESI = S): the gravity, the six bodies' masses and inertias, the nodes' frames and limits, the derived values; vf35 */
static void atv_setup_masses(rx_addr S) {
    rx_addr M = S + 0xd0;
    veh_v4(M, S + 0x3370);                                   /* VehicleSim_Gravity */
    rx_veh_mechanism_set_mass(M, F32(S, 0x3694), 0);
    rx_veh_mechanism_set_mass(M, F32(S, 0x36dc), 1);
    rx_veh_mechanism_set_mass(M, F32(S, 0x3724), 2);
    rx_veh_mechanism_set_mass(M, F32(S, 0x3724), 3);
    static const uint32_t in03[4] = {0x37c0, 0x3820, 0x3880, 0x3880};
    for (uint32_t i = 0; i < 4; i++) {   /* the inertia vectors and their reciprocals (divps from the 1-splat) */
        veh_v4(M + 0x680 + 0xa0 * i, S + in03[i]);
        rx_st4(M + 0x690 + 0xa0 * i, v4_div(rx_ld4(0x00d61650), rx_ld4(S + in03[i])));
    }
    rx_veh_mechanism_set_mass(M, F32(S, 0x376c), 4);
    rx_veh_mechanism_set_mass(M, F32(S, 0x376c), 5);
    for (uint32_t i = 4; i < 6; i++) {
        veh_v4(M + 0x680 + 0xa0 * i, S + 0x38e0);
        rx_st4(M + 0x690 + 0xa0 * i, v4_div(rx_ld4(0x00d61650), rx_ld4(S + 0x38e0)));
    }
    veh_v4(S + 0x5dc0, S + 0x57e0);
    veh_v4(S + 0x5dd0, VEH_RT_W1);
    /* the four rotors' frames: -(0, 1, 0, 0), -(1, 0, 0, 0), -(0, 0, 1, 1) (subps from zero), identity, and their radius */
    static const struct { uint32_t base, radius; } rot[4] = {{0x6be0, 0x3c7c}, {0x7540, 0x3c7c}, {0x8800, 0x3edc}, {0x7ea0, 0x3edc}};
    const rx_v4 z = v4(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++) {
        rx_addr b = S + rot[i].base;
        rx_st4(b, v4_sub(z, rx_ld4(0x00d616a0)));
        rx_st4(b + 0x10, v4_sub(z, rx_ld4(0x00d615c0)));
        rx_st4(b + 0x20, v4_sub(z, rx_ld4(0x00d61680)));
        veh_v4(b + 0x30, VEH_RT_W1);
        SU32(b, 0x40, veh_x87_copy(U32(S, rot[i].radius)));   /* fld / fstp */
    }
    SU32(S, 0x59d4, U32(0, 0x009d3bfc)); SU32(S, 0x59d0, 0); SU32(S, 0x59d8, 0x3f800000u);   /* the filter +0x59d0 (inline 0042d670) */
    phys_lag_filter_set(S + 0x94, 0.0f, F32(S, 0x33c4));
    SU32(S, 0x5a0c, 0); SU32(S, 0x5a10, 0x3e800000u); SU32(S, 0x5a14, 0x3f800000u);
    for (uint32_t k = 0; k < 4; k++) rx_vehicle_set_limits(VEH_ZERO3, S + 0x9a14 + 0xc * k, 0.1f /* 0x931d94 */);   /* 0042d8c0 inline */
    rx_v4 a = v4_splat(F32(S, 0x4184));
    rx_st4(S + 0x9150, a); rx_st4(S + 0x92a0, a);
    rx_v4 b = v4_splat(F32(S, 0x4624));
    rx_st4(S + 0x93f0, b); rx_st4(S + 0x9540, b);
    for (uint32_t o = 0x9160; o <= 0x9550; o += 0x150) veh_v4(S + o, VEH_RT_ZERO);   /* (+0x9160, +0x92b0, +0x9400, +0x9550: zero splats) */
    phys_lag_filter_set(S + 0x97ac, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x5730))));   /* 0042d670 with the x87-copied rate */
    phys_lag_filter_set(S + 0x97c0, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x5734))));
    phys_lag_filter_set(S + 0x97d4, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x5738))));
    phys_lag_filter_set(S + 0x97e8, 0.0f, rx_bf(veh_x87_copy(U32(S, 0x5784))));
    SU32(S, 0x96e8, veh_x87_copy(U32(S, 0x4e4c)));
    SU32(S, 0x2eb0, F32(S, 0x4e94) > 0.0f ? 1 : 0);
    float e = phys_mul(F32(S, 0x508c), F32(S, 0x4edc));
    SF32(S, 0x96f4, phys_mul(F32(S, 0x4f24), e)); SF32(S, 0x96f8, phys_mul(F32(S, 0x4f6c), e)); SF32(S, 0x96fc, phys_mul(F32(S, 0x4fb4), e));
    SF32(S, 0x9700, phys_mul(F32(S, 0x4ffc), e)); SF32(S, 0x9704, phys_mul(F32(S, 0x5044), e));
    veh_vcall(S, 0x8c, 0, 0, 0, 0);   /* vf35 */
}

/* 0048bde0 RavenSimulationATV_Construct (stdcall S; RET 4) */
rx_addr rx_sim_atv_construct(rx_addr S) {
    rx_raven_simulation_construct(S, 0xd);
    SU32(S, 0, 0x0093d054);   /* RavenSimulationATV::vftable */
    rx_veh_params_register(S + 0x2f10, k_atv_params, sizeof k_atv_params / sizeof k_atv_params[0]);   /* 00489cb0 */
    rx_veh_rec_init(S + 0x5880);
    /* eight RangedFloats {value, rate, 1, FLT_MAX, -FLT_MAX} at +0x5980 (rates 1, 1, 1, 1, k_MouseSensitivityPerPercent, 0.1, 0.1, 0.25) */
    const uint32_t rates[8] = {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, U32(0, 0x009d3bfc), 0x3dcccccdu, 0x3dcccccdu, 0x3e800000u};
    for (uint32_t i = 0; i < 8; i++) {
        rx_addr r = S + 0x5980 + 0x14 * i;
        SU32(r, 0, 0); SU32(r, 4, rates[i]); SU32(r, 0xc, 0x7f7fffffu); SU32(r, 0x10, 0xff7fffffu); SU32(r, 8, 0x3f800000u);
    }
    SU32(S, 0x5a20, 0); SU32(S, 0x5a24, 0);
    rx_veh_main_body_node_construct(S + 0x5a30);
    rx_veh_fixed_body_node_construct(S + 0x5d50);
    rx_veh_sliding_rotor_node_construct(S + 0x6b60);
    rx_veh_sliding_rotor_node_construct(S + 0x74c0);
    rx_veh_sliding_rotor_node_construct(S + 0x7e20);
    rx_veh_sliding_rotor_node_construct(S + 0x8780);
    SU32(S, 0x90e0, 0); SU32(S, 0x90e8, 0); SU32(S, 0x90ec, 0); SU32(S, 0x90f0, 0); SU32(S, 0x90f4, 0);
    rx_collision_shape_construct(S + 0x9100, 0x400);   /* 00844110 */
    SU32(S, 0x9100, 0x00976750);                       /* RavenPhysics::CollisionShapeCircle::vftable */
    veh_v4(S + 0x9120, VEH_RT_ZERO);
    for (uint32_t k = 0; k < 4; k++) rx_veh_wheel_record_init(S + 0x9140 + 0x150 * k, 2 + k);   /* 0042d9b0 ids 2 .. 5 */
    for (uint32_t o = 0x9680; o <= 0x968c; o += 4) SU32(S, o, 0);
    for (uint32_t o = 0x9690; o <= 0x96c0; o += 0x10) veh_v4(S + o, VEH_RT_ZERO);
    SU32(S, 0x9708, 0x3f000000u); SU32(S, 0x970c, 0x3e99999au); SU32(S, 0x9710, 0x3e19999au);
    for (uint32_t o = 0x96d0; o <= 0x96e4; o += 4) SU32(S, o, 0);
    SU32(S, 0x96ec, 0); SU32(S, 0x96f0, 0); SU32(S, 0x9714, 0);
    veh_v4(S + 0x9730, VEH_RT_ZERO);
    for (uint32_t o = 0x9740; o <= 0x9770; o += 0x10) veh_v4(S + o, VEH_RT_W1);
    for (uint32_t o = 0x9780; o <= 0x979c; o += 4) SU32(S, o, 0);
    SU32(S, 0x97a4, 0); SU32(S, 0x97a8, 1);
    for (uint32_t i = 0; i < 4; i++) {   /* RangedFloats {0, 1, 1, FLT_MAX, -FLT_MAX} at +0x97ac .. +0x97e8 */
        rx_addr r = S + 0x97ac + 0x14 * i;
        SU32(r, 0, 0); SU32(r, 4, 0x3f800000u); SU32(r, 0xc, 0x7f7fffffu); SU32(r, 0x10, 0xff7fffffu); SU32(r, 8, 0x3f800000u);
    }
    veh_v4(S + 0x9800, VEH_RT_W1); veh_v4(S + 0x9810, VEH_RT_W1);
    veh_v4(S + 0x9820, VEH_RT_ZERO);
    SU32(S, 0x983c, 1); SU32(S, 0x9830, 0); SU32(S, 0x9834, 0); SU32(S, 0x9838, 0);
    for (uint32_t i = 0; i < 3; i++) rx_veh_quat_rec_init(S + 0x9880 + 0x20 * i);   /* `vector constructor iterator' (004022d0) */
    for (uint32_t i = 0; i < 3; i++) rx_veh_quat_rec_init(S + 0x98f0 + 0x20 * i);
    static const uint16_t z1[] = {0x9950, 0x9954, 0x9958, 0x9960, 0x9968, 0x996c, 0x9970, 0x997c, 0x995c, 0x9964, 0x9974, 0x9978, 0x9980, 0x9984};
    for (uint32_t i = 0; i < sizeof z1 / sizeof z1[0]; i++) SU32(S, z1[i], 0);
    SU32(S, 0x9988, 0x3f800000u);
    veh_v4(S + 0x9990, VEH_RT_ZERO);
    SU32(S, 0x99a8, 0); SU32(S, 0x99ac, 0); SU32(S, 0x99b0, 0);
    SU32(S, 0x99b4, 0); SU32(S, 0x99b8, 0); SU32(S, 0x99bc, 0); SU32(S, 0x99c0, 0); SU32(S, 0x9a10, 0);
    for (uint32_t k = 0; k < 4; k++) rx_veh_ranged3_alloc(S + 0x9a14 + 0xc * k, VEH_ZERO3, 0x3dcccccdu /* 0.1 (x87 copy) */);   /* 0042d700 */
    SU32(S, 0x9a44, 0); SU32(S, 0x9a48, 0); SU32(S, 0x9a4c, 0);
    veh_zero3(S + 0x9a50); veh_zero3(S + 0x9a60); veh_zero3(S + 0x9a80);
    atv_params_load(S + 0x2f10);                       /* 0048b5d0 */
    /* the mechanism's six nodes: index / parent */
    rx_addr M = S + 0xd0;
    static const uint32_t node[6] = {0x5a30, 0x5d50, 0x6b60, 0x74c0, 0x7e20, 0x8780};
    for (uint32_t i = 0; i < 6; i++) {
        SU32(M, 0x1c, U32(M, 0x1c) + 1);
        SU32(M, 0xca0 + 4 * i, S + node[i]);
        SU32(S + node[i], 8, i);
        SU32(S + node[i], 0xc, i ? 0 : 0xffffffffu);
    }
    rx_veh_mechanism_bind(M);
    atv_setup_masses(S);                               /* 0048c750 */
    /* the stunt poses: six fixed quaternions / angle triples (0048c557..) */
    static const uint32_t q98f0[3][4] = {{0x3c92556d, 0xbeae0ded, 0x3eba49a1, 0x3f5df55a}, {0xbeab9000, 0xbedee5ac, 0x3ecec8b4, 0x3f3b42ff},
                                         {0x3ed55f9a, 0xbe93b0c4, 0x3e80c261, 0x3f531540}};
    static const uint32_t q9880[3][4] = {{0x3c92556d, 0x3eae0ded, 0xbeba49a1, 0x3f5df55a}, {0xbeab9000, 0x3edee5ac, 0xbecec8b4, 0x3f3b42ff},
                                         {0x3ed55f9a, 0x3e93b0c4, 0xbe80c261, 0x3f531540}};
    static const uint32_t a3[3][3] = {{0x3edf66f1, 0x3e860a91, 0x3eb2b8c1}, {0x3f060a91, 0x3f060a91, 0x3f060a91}, {0x3eb2b8c1, 0x3e860a91, 0x3eb2b8c1}};
    for (uint32_t i = 0; i < 3; i++) {
        for (uint32_t k = 0; k < 4; k++) { SU32(S, 0x98f0 + 0x20 * i + 4 * k, q98f0[i][k]); SU32(S, 0x9880 + 0x20 * i + 4 * k, q9880[i][k]); }
        for (uint32_t k = 0; k < 3; k++) { SU32(S, 0x9900 + 0x20 * i + 4 * k, a3[i][k]); SU32(S, 0x9890 + 0x20 * i + 4 * k, a3[i][k]); }
    }
    return S;
}

/* RavenSimulationATV::vf18 004da440 (ECX = S): the proxy's dynamic record gets the bodies' summed +0x110 x (0084f440: times the
 * unit splat 0x00dad4b0, built once) and S + 0x750 at +0x10 */
void rx_sim_atv_vf18(rx_addr S) {
    const rx_addr sp0 = rx_sp(), E = ((sp0 - 4) & ~15u) - 0x38, B = E - 8;   /* push ebp; and esp, -16; sub esp, 0x38; cookie; push esi, edi */
    rx_set_sp(B);
    veh_cookie(E, 0x34);
    rx_addr hnd = U32(S, 0x2ebc);
    if (hnd) {
        rx_v4 sum = rx_ld4(VEH_RT_ZERO);
        for (int32_t i = 0; i < I32(S, 0xec); i++) sum = v4_add(rx_ld4(S + 0x110 + 0xa0 * (uint32_t)i), sum);
        rx_st4(B + 0x10, sum);
        veh_v4(B + 0x20, S + 0x750);
        if (U32(hnd, 4)) {
            rx_addr rec = phys_body_record(U32(hnd, 4));
            if (!(U32(0, 0x00dad4c0) & 1)) { SU32(0, 0x00dad4c0, U32(0, 0x00dad4c0) | 1); rx_st4(0x00dad4b0, v4_splat(1.0f)); }
            rx_v4 one = rx_ld4(0x00dad4b0);
            SF32(rec, 0, v4_mul(rx_ld4(B + 0x10), one).v[0]);
            rx_st4(rec + 0x10, v4_mul(rx_ld4(B + 0x20), one));
        }
    }
    rx_set_sp(sp0);
}

/* RavenVehicleATV::vf28 0047c7f0 (ECX = V, s0 = the creation block; RET 4) */
void rx_vehicle_atv_vf28(rx_addr V, rx_addr blk) {
    const rx_addr sp0 = rx_sp(), B = sp0 - 0xc, B2 = B - 4;   /* push ebx, ebp, edi; then push esi */
    rx_set_sp(B);
    SU32(V, 0x188, U32(blk, 0x14));
    veh_call_sp(B, 0x0047c80c, 1, 0x9bd0, 0, 0);
    rx_addr S = rx_reflex_aligned_alloc(0x9bd0);
    if (S) { veh_call_sp(B, 0x0047c819, 1, S, 0, 0); S = rx_sim_atv_construct(S); }
    rx_set_sp(B2);
    SU32(V, 0x1700, S); SU32(V, 0x234, S);
    {   /* the physics world's simulations (+0x1940: an Rdb pointer array; 007cff20 when full) */
        rx_addr a = U32(U32(V, 0x188), 0x20134) + 0x1940;
        if ((int32_t)U32(a, 4) >= (int32_t)U32(a, 8)) { veh_call_sp(B2, 0x0047c854, 1, U32(a, 0xc) + U32(a, 8), 0, 0); rx_iarr_reserve(a, U32(a, 0xc) + U32(a, 8)); rx_set_sp(B2); }
        SU32(U32(a, 0), 4 * U32(a, 4), S);
        SU32(a, 4, U32(a, 4) + 1);
    }
    veh_call_sp(B2, 0x0047c865, 1, V, 0, 0);
    atv_create_proxy(V);
    SU32(U32(V, 0x1700), 0x2ebc, V + 0x178);
    veh_call_sp(B2, 0x0047c87e, 0, 0, 0, 0);
    veh_vcall(U32(V, 0x1700), 0x48, 0, 0, 0, 0);   /* S vf18 */
    static const struct { uint16_t off; uint32_t name; } nodes[27] = {
        {0x1a4, 0x93bb88}, {0x1a8, 0x93bb98}, {0x1ac, 0x93bba8}, {0x1b0, 0x93bbb8}, {0x1528, 0x93bbf0}, {0x152c, 0x93bd14}, {0x1530, 0x93bcfc},
        {0x1534, 0x93bd5c}, {0x1538, 0x93bcd4}, {0x153c, 0x93bd44}, {0x1540, 0x93bd2c}, {0x1544, 0x93bd70}, {0x1548, 0x93bce8}, {0x154c, 0x93bc2c},
        {0x1550, 0x93c484}, {0x1554, 0x93c498}, {0x1558, 0x93bde4}, {0x155c, 0x93c4a4}, {0x1560, 0x93bdfc}, {0x1564, 0x93bd84}, {0x1568, 0x93c4bc},
        {0x156c, 0x93bd9c}, {0x1570, 0x93c4d4}, {0x1574, 0x93c4ec}, {0x1578, 0x93c504}, {0x157c, 0x93bcac}, {0x1580, 0x93bcc0}};
    for (uint32_t i = 0; i < 27; i++) {   /* NDE_RHT_Hand .. OBJ_BCK_RHT_Wheel (007de9e0 on the instance's set) */
        veh_call_sp(B2, 0x0047c891 + 0x19 * i, 1, nodes[i].name, 0, 0);
        SU32(V, nodes[i].off, rx_hierarchy_find_node(U32(V, 0x1328) + 0x10, nodes[i].name));
    }
    veh_call_sp(B2, 0x0047cb29, 1, blk, 0, 0);
    rx_raven_vehicle_vf28(V, blk);
    veh_call_sp(B2, 0x0047cb37, 1, 0, 0, 0);
    veh_vcall(V, 0xc4, 1, 0, 0, 0);   /* vf49 */
    if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    static const uint32_t rets[4] = {0x0047cb5f, 0x0047cb71, 0x0047cb89, 0x0047cb9b};
    for (uint32_t k = 0; k < 4; k++) {
        veh_call_sp(B, rets[k], 1, 2, 0, 0);
        uint32_t slot = (uint32_t)rx_tracks_alloc_slot(U32(0, k < 2 ? 0x00a95774 : 0x00a95778), 2);
        SU32(V, 0x1584 + 4 * k, slot | (k < 2 ? 0x1000u : 0x2000u));
    }
    rx_set_sp(sp0);
}

RX_FUNCTION(0x0053f790, rx_vehicle_atv_alloc);
RX_FUNCTION(0x0047c4a0, rx_vehicle_atv_construct);
RX_FUNCTION(0x00486760, rx_vehicle_atv_vf49);
RX_FUNCTION(0x0047cdf0, atv_create_proxy);
RX_FUNCTION(0x004c0230, rx_veh_rec_init);
RX_FUNCTION(0x0048b5d0, atv_params_load);
RX_FUNCTION(0x0048c750, atv_setup_masses);
/* PHYSICS STUDIO (port, phys_studio.c): the masses again after a live change of the parameter block, as 0048d320's reload */
void rx_sim_atv_setup_masses(rx_addr S);
void rx_sim_atv_setup_masses(rx_addr S) { atv_setup_masses(S); }
RX_FUNCTION(0x0048bde0, rx_sim_atv_construct);
RX_FUNCTION(0x004da440, rx_sim_atv_vf18);
RX_FUNCTION(0x0047c7f0, rx_vehicle_atv_vf28);
