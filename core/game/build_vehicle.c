/* build_vehicle.c -- see build_vehicle.h. Construction unit 6: the vehicle objects. Hand transliteration from the disassembly of
 * RavenVehicleMotocross_Alloc 0053f750, RavenVehicleMotocross_Construct 004622b0, the RavenVehicle base 0045cec0 and its helpers
 * (0042d700, 00706df0, 0042e560, 0042e5c0, 0042e620, 0042d630, 00439240). */
#include "phys_studio.h"
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "build_alloc.h"
#include "build_load.h"
#include "build_world.h"
#include "build_rider.h"
#include "anim.h"
#include "phys_body.h"
#include "world_misc.h"
#include "loop_vehicle.h"
#include "phys_vec.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------------------------------------------------------
 * small constructors the vehicle and simulation objects share */

/* 0042d700 (ESI = out, EDI = three source floats, s0 = rate; RET 4): three RangedFloat-like records (0x14 each, ReflexAlignedAlloc):
 * {value (x87 copy of src[i]), rate, 1.0f, FLT_MAX, -FLT_MAX}; out[i] = the record (0 when the allocation fails) */
rx_addr rx_veh_ranged3_alloc(rx_addr out, rx_addr src, uint32_t rate_bits) {
    for (uint32_t i = 0; i < 3; i++) {
        rx_addr r = rx_reflex_aligned_alloc(0x14);
        if (r) {
            SU32(r, 4, rate_bits);
            SU32(r, 0, veh_x87_copy(U32(src, 4 * i)));
            SU32(r, 0xc, 0x7f7fffffu);   /* [0x977494] FLT_MAX */
            SU32(r, 0x10, 0xff7fffffu);  /* [0x977550] -FLT_MAX */
            SU32(r, 8, 0x3f800000u);     /* [0x931f24] 1.0f */
        }
        SU32(out, 4 * i, r);
    }
    return out;
}

/* 00706df0: the head node of an empty std::map (ReflexAlignedAlloc(0x18): left / parent / right 0, color 1 (black), isnil 0) */
rx_addr rx_veh_map_head_alloc(void) {
    rx_addr n = rx_reflex_aligned_alloc(0x18);
    if (n) SU32(n, 0, 0);
    SU32(n, 4, 0);
    SU32(n, 8, 0);
    SU8(n, 0x14, 1);
    SU8(n, 0x15, 0);
    return n;
}

/* 0042e560 (ECX = r): runtime zero / (0,0,0,1) / zero / zero vectors, then nine zero words */
void rx_veh_body_rec_init(rx_addr r) {
    veh_v4(r, VEH_RT_ZERO); veh_v4(r + 0x10, VEH_RT_W1); veh_v4(r + 0x20, VEH_RT_ZERO); veh_v4(r + 0x30, VEH_RT_ZERO);
    for (uint32_t o = 0x40; o <= 0x60; o += 4) SU32(r, o, 0);
}
RX_FUNCTION(0x0042e560, rx_veh_body_rec_init);
/* 0042e5c0 (EAX = r) */
void rx_veh_rec9_init(rx_addr r) { SU32(r, 0, 0); SU32(r, 4, 0); SU32(r, 8, 0); for (uint32_t o = 0x10; o <= 0x20; o += 4) SU32(r, o, 0); }
/* 0042e620 (ECX = r) */
void rx_veh_rec_20_init(rx_addr r) { for (uint32_t o = 0x20; o <= 0x40; o += 4) SU32(r, o, 0); }

/* 0042d630 (EAX = r, xmm0 = value, s0 = rate, s1 = limit; RET 8): {value, rate, 1.0f, limit, -0.0f - limit} */
void rx_veh_clamped_init(rx_addr r, uint32_t value_bits, uint32_t rate_bits, uint32_t limit_bits) {
    SU32(r, 0, value_bits);
    SU32(r, 4, rate_bits);
    SF32(r, 0xc, rx_bf(limit_bits));
    SF32(r, 0x10, rx_subs(-0.0f /* [0x9774a8] */, rx_bf(limit_bits)));
    SU32(r, 8, 0x3f800000u);
}

/* 00439240 (s0 = r; RET 4) */
void rx_veh_rec_00439240(rx_addr r) {
    for (uint32_t o = 0; o <= 0x10; o += 4) SU32(r, o, 0);
    veh_v4(r + 0x20, VEH_RT_ZERO); veh_v4(r + 0x30, VEH_RT_ZERO);
    SU32(r, 0x40, 0);
    veh_v4(r + 0x50, VEH_RT_W1);
    for (uint32_t o = 0x60; o <= 0x84; o += 4) SU32(r, o, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the vehicle object */

/* 0053f750 RavenVehicleMotocross_Alloc: _aligned_malloc(0x17d0, 16) (under the allocator lock) */
rx_addr rx_vehicle_motocross_alloc(void) { return rx_crt_aligned_malloc(0x17d0, 16); }

/* 0045cec0 (stdcall this, p2; RET 8): the Entity base, the RavenPhysicsListener / IRenderable / IRenderEvent parts, the RavenVehicle
 * vtables and fields */
rx_addr rx_raven_vehicle_construct(rx_addr v, uint32_t p2) {
    SU32(v, 0, 0x0095a2e4);   /* Entity::vftable */
    SU32(v, 0x18, 0); SU32(v, 0x10, 0); SU32(v, 0x24, 0); SU32(v, 0x20, 0); SU32(v, 0x70, 0);
    veh_copy(v + 0x30, VEH_IDENTITY, 0x40);
    SU32(v, 0x1c, 0); SU32(v, 0x7c, 0); SU32(v, 0x78, 0); SU32(v, 0x74, 0); SU32(v, 0x28, 0); SU32(v, 0x2c, 0);
    SU32(v, 0x90, 0x0093cbf4);   /* RavenPhysics::RavenPhysicsListener::vftable */
    SU32(v, 0x94, 0x0093708c);   /* IRenderable::vftable */
    SU32(v, 0x98, 0xf0);
    SU32(v, 0x9c, 0x009370cc);   /* IRenderEvent::vftable */
    SU32(v, 0, 0x0093c74c);      /* RavenVehicle::vftable */
    SU32(v, 0x90, 0x0093c84c); SU32(v, 0x94, 0x0093c85c); SU32(v, 0x9c, 0x0093c89c);
    rx_veh_ranged3_alloc(v + 0xa0, 0x00c01b90, 0x3dcccccdu /* [0x931d94] 0.1f, x87 copy */);
    SU32(v, 0xb8, 0x7f7fffffu);   /* [0x977494] */
    SU32(v, 0xac, 0);
    SU32(v, 0xb0, 0x3f800000u);
    SU32(v, 0xbc, 0xff7fffffu);   /* [0x977550] */
    SU32(v, 0xb4, 0x3f800000u);
    veh_zero3(v + 0xc0);
    veh_v4(v + 0xe0, VEH_QIDENTITY);
    veh_copy(v + 0xf0, VEH_IDENTITY, 0x40);
    veh_zero3(v + 0x130); veh_zero3(v + 0x140);
    SU32(v, 0x150, 0); SU32(v, 0x154, 0);
    veh_zero3(v + 0x160);
    SU8(v, 0x174, 0);
    SU32(v, 0x17c, 0);
    SU32(v, 0x178, 0x0093a644);   /* RavenPhysics::ProxyObjectHandle::vftable */
    for (uint32_t o = 0x180; o <= 0x190; o += 4) SU32(v, o, 0);
    SU32(v, 0x194, p2);
    SU32(v, 0x198, 0);
    for (uint32_t o = 0x1a0; o <= 0x1b0; o += 4) SU32(v, o, 0);
    veh_zero3(v + 0x1c0); veh_zero3(v + 0x1d0); veh_zero3(v + 0x1e0); veh_zero3(v + 0x1f0);
    veh_v4(v + 0x200, VEH_RT_ZERO); veh_v4(v + 0x210, VEH_RT_W1);
    for (uint32_t o = 0x220; o <= 0x230; o += 4) SU32(v, o, 0);
    SU32(v, 0x290, U32(0, 0x009d789c));
    SU32(v, 0x234, 0); SU32(v, 0x238, 1); SU32(v, 0x23c, 1); SU32(v, 0x280, 0); SU32(v, 0x28c, 0);
    SU32(v, 0x294, 0x3fc00000u);   /* [0x9322a4] 1.5f */
    /* +0x6a0: a std::map (proxy ReflexAlignedAlloc(4), head 00706df0, size 0) */
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, v + 0x6a0);
    SU32(v, 0x6a0, px);
    rx_addr h = rx_veh_map_head_alloc();
    SU32(v, 0x6b8, h);
    SU8(h, 0x15, 1);
    SU32(U32(v, 0x6b8), 4, U32(v, 0x6b8)); SU32(U32(v, 0x6b8), 0, U32(v, 0x6b8)); SU32(U32(v, 0x6b8), 8, U32(v, 0x6b8));
    SU32(v, 0x6bc, 0);
    SU8(v, 0x1000, 0); SU8(v, 0x1001, 0);
    SU32(v, 0x1010, 0); SU32(v, 0x1214, 0);
    rx_veh_body_rec_init(v + 0x1220);
    rx_veh_rec9_init(v + 0x1290);
    rx_veh_rec_20_init(v + 0x12c0);
    uint32_t serial = U32(0, 0x00a89bf4);   /* the vehicles' creation counter */
    SU32(v, 0x1324, serial);
    SU32(0, 0x00a89bf4, serial + 1);
    SU32(v, 0x1310, 0); SU32(v, 0x1314, 0); SU32(v, 0x1318, 0); SU32(v, 0x131c, 0); SU32(v, 0x1320, 1); SU32(v, 0x1328, 0);
    SU32(v, 0x298, 0); SU32(v, 0x29c, 0); SU32(v, 0x70, 0);
    for (uint32_t i = 0; i < 0x19; i++) {   /* 25 slots: +0x11b0 index -1, +0x1020 a zero vector (x, y, z) */
        SU32(v, 0x11b0 + 4 * i, 0xffffffffu);
        veh_zero3(v + 0x1020 + 0x10 * i);
    }
    return v;
}
RX_FUNCTION(0x0045cec0, rx_raven_vehicle_construct);

/* 004622b0 RavenVehicleMotocross_Construct (ESI = this): the base with p2 0, the RavenVehicleMotocross vtables and fields */
rx_addr rx_vehicle_motocross_construct(rx_addr v) {
    rx_raven_vehicle_construct(v, 0);
    SU32(v, 0x15ac, 0x3f8ccccdu);   /* [0x9774c4] 1.1f */
    SU32(v, 0x15b0, U32(0, 0x009d789c));
    SU32(v, 0x15b8, U32(0, 0x009d789c));
    SU32(v, 0x15b4, U32(0, 0x009cd970));
    SU32(v, 0x15bc, 0x3f400000u);   /* [0x935aa8] 0.75f */
    SU32(v, 0, 0x0093cd84);         /* RavenVehicleMotocross::vftable */
    SU32(v, 0x90, 0x0093ce88); SU32(v, 0x94, 0x0093ce9c); SU32(v, 0x9c, 0x0093cedc);
    for (uint32_t o = 0x1560; o <= 0x1574; o += 4) SU32(v, o, 0);
    for (uint32_t o = 0x1578; o <= 0x1584; o += 4) SU32(v, o, 0xffffffffu);
    for (uint32_t o = 0x1588; o <= 0x15a8; o += 4) SU32(v, o, 0);
    veh_fill(v + 0x15c0, 0x30, 0);   /* three zero vectors (xorps splat) */
    SU32(v, 0x15f0, 0);
    rx_veh_clamped_init(v + 0x15f4, 0, 0x3dcccccdu /* 0.1f (x87 copy) */, 0x7f7fffffu /* FLT_MAX (x87 copy) */);
    SU32(v, 0x1608, 0x3f800000u);
    for (uint32_t o = 0x160c; o <= 0x1618; o += 4) SU32(v, o, 0);
    SU32(v, 0x161c, 0); SU32(v, 0x1620, 0); SU32(v, 0x16d4, 0);
    rx_veh_rec_00439240(v + 0x16e0);
    SU32(v, 0x1770, 0); SU32(v, 0x1780, 0);
    for (uint32_t o = 0x17c0; o <= 0x17cc; o += 4) SU32(v, o, 0xfffffffeu);
    for (uint32_t i = 0; i < 0x19; i++) {   /* 25 slots: +0x14c0 index -1, +0x1330 a zero vector */
        SU32(v, 0x14c0 + 4 * i, 0xffffffffu);
        veh_zero3(v + 0x1330 + 0x10 * i);
    }
    SU32(v, 0x16a4, U32(0, 0x009d7b30));
    SU32(v, 0x16a8, 0x41a00000u); SU32(v, 0x16b0, 0x41a00000u);   /* [0x935aac] 20.0f */
    SU32(v, 0x16b4, 0x40a00000u);   /* 5.0f */
    SU32(v, 0x16b8, 0x3e4ccccdu);   /* 0.2f */
    SU32(v, 0x16bc, 0x3da3d70au);   /* 0.08f */
    for (uint32_t o = 0x1630; o <= 0x163c; o += 4) SU32(v, o, 0);
    SU32(v, 0x16c0, 0x40400000u);   /* 3.0f */
    SU32(v, 0x16ac, U32(0, 0x009d3bfc));   /* k_MouseSensitivityPerPercent */
    SU32(v, 0x16c4, U32(0, 0x009d78dc));   /* ModelDrawDistance_RadiusLimit50 */
    return v;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * 00790400: the one-time init of the tuning-reload registry (0x00d72ad8..: its lock 0x00d72ae4, the pending tuning XML slots,
 * a std::vector proxy at 0x00d92d2c, the 16 slot pairs 0x00d92cac / 0x00d92cec); called under the guard of 0043a3f0 */
static void cs_init(rx_addr cs) {   /* InitializeCriticalSection as the reference boot leaves it: {0, -1, 0, 0, 0, 0} */
    SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0);
}
void rx_veh_tuning_registry_init(void) {
    SU32(0, 0x00d72adc, 0);
    cs_init(0x00d72ae4);
    for (uint32_t o = 0; o < 0x18; o += 4) { SU32(0, 0x00d92c10 + o, 0); SU32(0, 0x00d92c40 + o, 0); SU32(0, 0x00d92c64 + o, 0); SU32(0, 0x00d92c88 + o, 0); }
    static const uint32_t z[] = {0x00d92c28, 0x00d92c30, 0x00d92c34, 0x00d92c2c, 0x00d92c38, 0x00d92c5c, 0x00d92c60, 0x00d92c58,
                                 0x00d92c80, 0x00d92c84, 0x00d92c7c, 0x00d92ca4, 0x00d92ca8, 0x00d92ca0};
    for (uint32_t i = 0; i < sizeof z / sizeof z[0]; i++) SU32(0, z[i], 0);
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, 0x00d92d2c);
    SU32(0, 0x00d92d2c, px);
    SU32(0, 0x00d92d38, 0); SU32(0, 0x00d92d3c, 0); SU32(0, 0x00d92d40, 0);
    SU32(0, 0x00d72afc, 0); SU32(0, 0x00d72b00, 0);
    SU8(0, 0x00d72ad8, 0);
    for (rx_addr a = 0x00d92cec; a < 0x00d92d2c; a += 4) { SU32(0, a - 0x40, 0); SU32(0, a, 0); }
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the vehicle's model instance (V + 0x1328) */

/* 00542e00 (ECX = the vehicle list (its +0x3c: the VehicleHierarchies table), s0 = the hierarchy id; RET 4): the hierarchy model's
 * handle (Database::vf27 of the row's AssetName, type "model") */
rx_addr rx_vehicle_hierarchy_handle(rx_addr list, int32_t hierarchy_id) {
    rx_addr t = U32(list, 0x3c);
    uint32_t c = rx_tbl_column_index(t, 0x00940664 /* "VehicleHierarchyId" */);
    rx_addr ref = rx_tbl_row(U32(U32(t, 0x10), 4 * c), hierarchy_id);
    rx_addr store = U32(ref, 8);
    uint32_t row = U32(ref, 0);
    uint32_t c2 = rx_tbl_column_index(store, 0x00936fa0 /* "AssetName" */);
    rx_addr str = rx_tbl_cell_string(U32(U32(store, 0x10), 4 * c2), row);
    rx_addr name = U32(str, 0x18) >= 0x10 ? U32(str, 4) : str + 4;
    return rx_load_find_handle("model", name);   /* Database::vf27(name, 'model', 1) */
}

/* 004300e0 (EDX = the instance, s0 = the model handle; RET 4): the handle's loading byte set around the copy 007d4d60; a model with
 * nodes keeps its root's local matrix (ModelNodeRef_SetMatrix 004300a0 after the copy) */
static void model_instance_copy(rx_addr inst, rx_addr h) {
    SU32(h, 0x4c, 1);
    if (U32(inst, 0x1c)) { fprintf(stderr, "rxcore: 004300e0 over a used instance: not ported\n"); abort(); }
    rx_model_asset_copy(inst, U32(h, 0x44));
    SU32(h, 0x4c, 0);
}

/* 00460190 (EDI = V, s0 = the hierarchy model handle; RET 4): V + 0x1328 = a new ModelAsset (0x150: 007d3f30, +0x148 / +0x149 0xff,
 * +0x140 0 -> 2, +0x14e 1, +0x14f 1, +0x144 -1) copied from the handle's model; V + 0x2a0 0x400 bytes 0xff */
void rx_vehicle_model_create(rx_addr V, rx_addr h) {
    rx_addr m = rx_reflex_aligned_alloc(0x150);
    if (m) {
        rx_rider_model_asset_construct(m);
        SU8(m, 0x148, 0xff); SU8(m, 0x149, 0xff);
        SU32(m, 0x140, 0);
        SU8(m, 0x14e, 1);
        SU32(m, 0x144, 0xffffffffu);
    }
    SU32(V, 0x1328, m);
    model_instance_copy(m, h);
    SU32(U32(V, 0x1328), 0x140, 2);
    SU8(U32(V, 0x1328), 0x14f, 1);
    for (uint32_t k = 0; k < 0x400; k++) SU8(V, 0x2a0 + k, 0xff);
}

/* 007df210 with the original's frames (e = its return slot; the caller's EBX EBP ESI EDI): push ebx, ebp, esi, edi; [the allocator
 * lock 0xd61704 around each heap call when 0xda67d8 is set]; push spare; call [_aligned_free] (return 007df238); push 0x10, n * 0x40;
 * call [_aligned_malloc] (return 007df27f); the copy loop keeps its counter in the argument slot. In the oracle the heap calls run the
 * originals at the original's ESP (their frames are the later event records' dead w lanes, construction.md 15.2 (a)). */
static void nodeset_spare_fr(rx_addr e, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi, rx_addr ns) {
    const int lock = U8(0, 0x00da67d8) != 0;
    SU32(e, -4, ebx); SU32(e, -8, ebp); SU32(e, -0xc, esi); SU32(e, -0x10, edi);
    const rx_addr sp = rx_sp(), spare = U32(ns, 0x2c);
    if (lock) { SU32(e, -0x14, 0x00d61704u); SU32(e, -0x18, 0x007df231u); }
    SU32(e, -0x14, spare);
    rx_set_sp(e - 0x10);
    rx_crt_aligned_free(spare);
    SU32(e, -0x18, 0x007df238u);
    if (lock) { SU32(e, -0x14, 0x00d61704u); SU32(e, -0x18, 0x007df251u); SU32(e, -0x14, 0x00d61704u); SU32(e, -0x18, 0x007df276u); }
    const uint32_t n = U32(ns, 0xc), size = rx_mul_sat(n, 0x40);
    SU32(e, -0x14, 0x10); SU32(e, -0x18, size);
    rx_set_sp(e - 0x10);
    const rx_addr blk = rx_crt_aligned_malloc(size, 16);
    SU32(e, -0x1c, 0x007df27fu);
    if (lock) { SU32(e, -0x14, 0x00d61704u); SU32(e, -0x18, 0x007df294u); }
    rx_set_sp(sp);
    SU32(ns, 0x2c, blk);
    SU32(e, 4, 0);
    for (uint32_t i = 0; i < n; i++) {
        rx_addr rec = U32(ns, 8) + 16 * i;
        rx_addr src = U32(U32(rec, 0), 0x18) + ((uint32_t)U8(rec, 0xb) << 6);
        veh_copy(U32(ns, 0x2c) + 0x40 * i, src, 0x40);
        SU32(e, 4, i + 1);
    }
}
/* 007df210 (s0 = node set; RET 4): the set's spare matrices (+0x2c) re-made as a copy of every node's local matrix */
static void nodeset_spare_from_locals(rx_addr ns) {
    rx_crt_aligned_free(U32(ns, 0x2c));
    uint32_t n = U32(ns, 0xc);
    rx_addr blk = rx_crt_aligned_malloc(rx_mul_sat(n, 0x40), 16);
    SU32(ns, 0x2c, blk);
    for (uint32_t i = 0; i < n; i++) {
        rx_addr rec = U32(ns, 8) + 16 * i;
        rx_addr src = U32(U32(rec, 0), 0x18) + ((uint32_t)U8(rec, 0xb) << 6);
        veh_copy(U32(ns, 0x2c) + 0x40 * i, src, 0x40);
    }
}

/* 007cff20 (ESI = a growable pointer array {data, count, capacity, growth}, s0 = the new capacity; RET 4) */
static void ptr_array_push(rx_addr a, uint32_t value) {
    if (I32(a, 4) >= I32(a, 8)) rx_iarr_reserve(a, U32(a, 0xc) + U32(a, 8));   /* 007cff20 (build_rider.h) */
    SU32(U32(a, 0), 4 * U32(a, 4), value);
    SU32(a, 4, U32(a, 4) + 1);
}

/* 007c33a0 TerrainTracks_AllocSlot (EDX = the tracks, s0 = kind; RET 4): the first free slot (+0x94 != 0) of the ring: +0x94 0,
 * +0x9c 1, +0xb4 the kind, used count +0x28 + 1; -1 when full */
int32_t rx_tracks_alloc_slot(rx_addr tr, uint32_t kind) {
    for (int32_t i = 0; i < I32(tr, 0xc); i++) {
        rx_addr r = U32(tr, 0x24) + 0xc0 * (uint32_t)i;
        if (U32(r, 0x94) == 0) continue;
        SU32(r, 0x94, 0); SU32(U32(tr, 0x24) + 0xc0 * (uint32_t)i, 0x9c, 1); SU32(U32(tr, 0x24) + 0xc0 * (uint32_t)i, 0xb4, kind);
        SU32(tr, 0x28, U32(tr, 0x28) + 1);
        return i;
    }
    return -1;
}
RX_FUNCTION(0x007c33a0, rx_tracks_alloc_slot);

/* ---------------------------------------------------------------------------------------------------------------------------
 * the vehicle's physics */

/* 00462b90 (s0 = V; RET 4): the bike's collision proxy object (collis "Ron_BikeCollisionExperiment", material "MXBike"; 00769160)
 * into V + 0x178 (ProxyObjectHandle), its record's +0x34 = V, the shape's three named children (FrontTire / RearTire / FrontFender,
 * flags |= 4) into V + 0x17c0 / 0x17c4 / 0x17c8, V + 0x174 = 1 */
void rx_vehicle_moto_create_proxy(rx_addr V) {
    rx_addr h = rx_load_find_handle_c("collis", "Ron_BikeCollisionExperiment");   /* vf27(name, 'collision', 1) */
    uint32_t mat = rx_phys_props_find(rx_phys_props_get(), "MXBike");               /* 00850bf0 on the registry 0x00da6ad0 */
    RX_FRAME_ENTER(F, 0x40);
    rx_addr out = F + 0x14;
    rx_proxy_object_create(U32(U32(V, 0x188), 0x20134), out, mat, h, h);
    rx_addr t = U32(out, 4);
    if (U32(V, 0x17c) != t) {
        rx_addr old = U32(V, 0x17c);
        if (old) { SU32(old, 0, U32(old, 0) - 1); if (!U32(old, 0)) { fprintf(stderr, "rxcore: 00462b90: proxy released\n"); abort(); } }
        SU32(V, 0x17c, t);
        if (t) SU32(t, 0, U32(t, 0) + 1);
    }
    SU32(out, 0, 0x0093a650);
    if (t) { SU32(t, 0, U32(t, 0) - 1); if (!U32(t, 0)) { fprintf(stderr, "rxcore: 00462b90: proxy freed\n"); abort(); } }
    rx_addr rec = phys_body_record(U32(V, 0x17c));
    SU32(rec, 0x34, V);
    rec = phys_body_record(U32(V, 0x17c));
    rx_addr shape = U32(rec, 0x2c);
    static const char* names[3] = {"FrontTire", "RearTire", "FrontFender"};
    static const rx_addr name_at[3] = {0x0093bc70, 0x0093bc7c, 0x0093bc88};
    for (int i = 0; i < 3; i++) {
        (void)names[i];
        uint32_t idx = veh_vcall(shape, 0x20, 1, name_at[i], 0, 0);   /* vf8: child by name */
        SU32(V, 0x17c0 + 4 * (uint32_t)i, idx);
        if (i < 2) { rx_addr child = U32(U32(shape, 0x2c), 4 * idx); SU32(child, 0x14, U32(child, 0x14) | 4); }
    }
    SU8(V, 0x174, 1);
    RX_FRAME_LEAVE(F, 0x40);
}

/* 0044eee0 RavenSimulationMotocross::vf18 (ECX = S): the proxy record's mass (the four bodies' summed mass lanes, x) and inertia
 * (S + 0x750), each times the static 1-splat (guard bit 1 of [0x00dad4c0], 0x00dad4b0) */
void rx_sim_moto_vf18(rx_addr S) {
    rx_addr hnd = U32(S, 0x2ebc);
    if (!hnd) return;
    rx_v4 sum = v4_add(v4_add(v4_add(rx_ld4(S + 0x1b0), rx_ld4(S + 0x110)), rx_ld4(S + 0x250)), rx_ld4(S + 0x2f0));
    rx_v4 in = rx_ld4(S + 0x750);
    if (!U32(hnd, 4)) return;
    rx_addr rec = phys_body_record(U32(hnd, 4));
    if (!(U32(0, 0x00dad4c0) & 1)) { SU32(0, 0x00dad4c0, U32(0, 0x00dad4c0) | 1); rx_st4(0x00dad4b0, v4_splat(1.0f)); }
    rx_v4 one = rx_ld4(0x00dad4b0);
    SF32(rec, 0, v4_mul(sum, one).v[0]);
    rx_st4(rec + 0x10, v4_mul(in, one));
}
RX_FUNCTION(0x0044eee0, rx_sim_moto_vf18);

/* RavenVehicle::vf28 004602c0 (ECX = V, s0 = the creation block; RET 4): V + 0x198 / 0x18c / 0x188 from the block, V + 0x90 into the
 * physics world's listeners (+0x1914), the model's +0x14f by the factory index, V + 0x170, the root's rotation (V + 0x210) and
 * translation (V + 0x200), the spare matrices, the placement at the origin (0045f600), vf49(0), V + 0x28c 2 */
void rx_raven_vehicle_vf28(rx_addr V, rx_addr blk) {
    const rx_addr sp0 = rx_sp(), B = veh_frame16(sp0, 4);   /* the original's frame (push ecx, ebx, esi, edi) */
    rx_set_sp(B);
    SU32(V, 0x198, U32(blk, 8));
    SU32(V, 0x18c, U32(blk, 0x10));
    SU32(V, 0x188, U32(blk, 0x14));
    rx_addr lis = U32(U32(blk, 0x14), 0x20134) + 0x1914;
    SU32(B, 0xc, V + 0x90);   /* the local pushed by address */
    veh_call_sp(B, 0x0046030a, 1, B + 0xc, 0, 0);
    rx_world_ptr_vector_push_unique(lis, B + 0xc);
    SU8(U32(V, 0x1328), 0x14f, U32(V, 0x194) <= 7 ? 1 : 0);
    SU32(V, 0x170, U32(blk, 0xc));
    rx_addr root = U32(U32(V, 0x1328), 0x10);
    veh_call_sp(B, 0x00460370, 0, 0, 0, 0);
    rx_node_local_rotation(root, V + 0x210);
    root = U32(U32(V, 0x1328), 0x10);
    veh_copy(V + 0x200, U32(U32(root, 0), 0x18) + ((uint32_t)U8(root, 0xb) << 6) + 0x30, 0x10);
    veh_call_sp(B, 0x004603a0, 1, U32(V, 0x1328) + 0x10, 0, 0);
    nodeset_spare_fr(B - 8, blk, sp0 - 4, V + 0x210, V, U32(V, 0x1328) + 0x10);   /* EBX = the block, EBP the frame, ESI = V + 0x210, EDI = V */
    veh_call_sp(B, 0x004603b5, 3, V, VEH_ZERO3, 0x009cb4a0);
    rx_vehicle_place(V, VEH_ZERO3, 0x009cb4a0, 0x009cb4b0);
    veh_call_sp(B, 0x004603c3, 1, 0, 0, 0);
    veh_vcall(V, 0xc4, 1, 0, 0, 0);   /* vf49 */
    rx_set_sp(sp0);
    SU32(V, 0x28c, 2); SU32(V, 0x298, 0); SU32(V, 0x29c, 0);
}
RX_FUNCTION(0x004602c0, rx_raven_vehicle_vf28);

/* the game's random generator 0x009cb860 (count 0x009cb864): (seed >> 16 & 0x7fff) / 32768 * 100 as the code computes it */
static float game_rand100(void) {
    uint32_t s = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, s);
    float f = (float)(int32_t)((s >> 16) & 0x7fff);
    return rx_muls((float)((double)f * 3.0517578125e-05 /* [0x9776a0] */), 100.0f /* [0x93005c] */);
}

/* RavenVehicleMotocross::vf28 004625d0 (ECX = V, s0 = the creation block; RET 4) */
void rx_vehicle_moto_vf28(rx_addr V, rx_addr blk) {
    const rx_addr sp0 = rx_sp(), B = veh_frame16(sp0, 4);   /* the original's frame (push ecx, ebx, esi, edi; [esp + 0xc] = blk) */
    rx_set_sp(B);
    SU32(B, 0xc, blk);
    SU32(V, 0x188, U32(blk, 0x14));
    veh_call_sp(B - 8, 0x0046260d, 0, 0, 0, 0);   /* _aligned_malloc(0x8fc0, 16): two cdecl arguments */
    SU32(B - 8, 0, 0x8fc0); SU32(B - 4, 0, 0x10);
    rx_addr S = rx_crt_aligned_malloc(0x8fc0, 16);
    if (S) { veh_call_sp(B, 0x0046263b, 3, S, V + 0x16e0, U32(blk, 0xc)); S = rx_sim_moto_construct(S, V + 0x16e0, U32(blk, 0xc)); }
    SU32(V, 0x16d4, S); SU32(V, 0x234, S);
    rx_set_sp(B);
    ptr_array_push(U32(U32(V, 0x188), 0x20134) + 0x1940, S);   /* the physics world's simulations (007cff20 when full) */
    veh_call_sp(B, 0x00462686, 1, V, 0, 0);
    rx_vehicle_moto_create_proxy(V);
    SU32(U32(V, 0x16d4), 0x2ebc, V + 0x178);
    veh_call_sp(B, 0x0046269f, 0, 0, 0, 0);
    rx_sim_moto_vf18(U32(V, 0x16d4));   /* vf18 */
    static const struct { uint32_t off, name; } nodes[] = {
        {0x1a4, 0x0093bb88}, {0x1a8, 0x0093bb98}, {0x1ac, 0x0093bba8}, {0x1b0, 0x0093bbb8}, {0x1550, 0x0093bbc8}, {0x1554, 0x0093bbdc},
        {0x1560, 0x0093bbf0}, {0x1564, 0x0093bbfc}, {0x1568, 0x0093bc0c}, {0x156c, 0x0093bc1c}, {0x1570, 0x0093bc2c}, {0x1574, 0x0093bc3c}};
    for (uint32_t i = 0; i < sizeof nodes / sizeof nodes[0]; i++) {   /* NDE_RHT_Hand .. OBJ_BCK_Tire (007de9e0 on the instance's set) */
        veh_call_sp(B, 0x004626b2 + 0x19 * i, 1, nodes[i].name, 0, 0);
        SU32(V, nodes[i].off, rx_hierarchy_find_node(U32(V, 0x1328) + 0x10, nodes[i].name));
    }
    rx_set_sp(B);
    rx_addr fv = U32(V, 0x1550), bv = U32(V, 0x1554);
    if (fv && bv) {   /* the fender vibration nodes' rest translations and random phases */
        veh_copy(V + 0x1530, U32(U32(fv, 0), 0x18) + ((uint32_t)U8(fv, 0xb) << 6) + 0x30, 0x10);
        veh_copy(V + 0x1540, U32(U32(bv, 0), 0x18) + ((uint32_t)U8(bv, 0xb) << 6) + 0x30, 0x10);
        float r = game_rand100();
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
        SF32(V, 0x1558, r);
        SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
        SF32(V, 0x155c, game_rand100());
    }
    veh_call_sp(B, 0x004628c2, 1, blk, 0, 0);
    rx_raven_vehicle_vf28(V, blk);
    veh_call_sp(B, 0x004628d0, 1, 0, 0, 0);
    veh_vcall(V, 0xc4, 1, 0, 0, 0);   /* vf49 */
    if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    veh_call_sp(B, 0x004628f5, 1, 1, 0, 0);
    SU32(V, 0x1578, (uint32_t)rx_tracks_alloc_slot(U32(0, 0x00a95774), 1) | 0x1000);
    veh_call_sp(B, 0x00462906, 1, 1, 0, 0);
    SU32(V, 0x157c, (uint32_t)rx_tracks_alloc_slot(U32(0, 0x00a95774), 1) | 0x1000);
    veh_call_sp(B, 0x0046291e, 1, 0, 0, 0);
    SU32(V, 0x1580, (uint32_t)rx_tracks_alloc_slot(U32(0, 0x00a95778), 0) | 0x2000);
    veh_call_sp(B, 0x00462930, 1, 0, 0, 0);
    SU32(V, 0x1584, (uint32_t)rx_tracks_alloc_slot(U32(0, 0x00a95778), 0) | 0x2000);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004625d0, rx_vehicle_moto_vf28);

/* ---------------------------------------------------------------------------------------------------------------------------
 * the factory and the tuning */

/* 00543020 VehicleSpec_ControllerOf (s0 = VehicleList VehicleId; RET 4): the Controller of the first Player List row (static table
 * 0x00ae0e08) whose RequestedVehicleId, else whose VehicleId, is the id; -1 when none */
int32_t rx_vehicle_spec_controller_of(int32_t vehicle_id) {
    static const rx_addr cols[2] = {0x0093df5c /* "RequestedVehicleId" */, 0x00936cb4 /* "VehicleId" */};
    for (int k = 0; k < 2; k++) {
        uint32_t c = rx_tbl_column_index(0x00ae0e08, cols[k]);
        rx_addr set = rx_tbl_rows(U32(U32(0, 0x00ae0e18), 4 * c), vehicle_id);
        if (!U32(set, 0x1c)) continue;
        rx_addr first = U32(U32(set, 0x18), 0);   /* the set's leftmost node */
        uint32_t row = U32(first, 0xc);
        rx_addr store = U32(first, 0x14);
        uint32_t cc = rx_tbl_column_index(store, 0x0093ded0 /* "Controller" */);
        return I32(rx_tbl_cell_word(store, cc, row), 0);
    }
    return -1;
}
RX_FUNCTION(0x00543020, rx_vehicle_spec_controller_of);

/* 00543130 VehicleSpec_ApplyTuning (s0 = spec; RET 4) */
void rx_vehicle_spec_apply_tuning(rx_addr spec) {
    rx_addr m = U32(spec, 8);
    for (uint32_t k = 0; k < 8; k++) SU32(spec, 0xa0 + 4 * k, U32(m, 0x1c + 4 * k));
    int32_t ctl = rx_vehicle_spec_controller_of(I32(spec, 4));
    int done = 0;
    if (ctl >= 0 && I32(spec, 0x9c) != -1) {
        rx_addr t = rx_tbl_find(0x00ae10f0 + 0x1f4 * (uint32_t)ctl, 0x0093e6bc /* "PlayerVehicleTuning" */);
        uint32_t c = rx_tbl_column_index(t, 0x0093e578 /* "PlayerVehicleId" */);
        rx_addr ref = rx_tbl_row(U32(U32(t, 0x10), 4 * c), I32(spec, 0x9c));
        rx_addr store = U32(ref, 8);
        if (store && I32(ref, 0) != -1) {
            static const struct { rx_addr name; uint32_t off; int add; } f[8] = {
                {0x0093dcfc, 0xa0, 1}, {0x0093dcf4, 0xa4, 1}, {0x0093dce8, 0xa8, 1}, {0x0093dcdc, 0xac, 1},
                {0x0093e734, 0xb0, 1}, {0x0093e73c, 0xb4, 1}, {0x00940498, 0xb8, 0}, {0x0093a72c, 0xbc, 0}};   /* Engine1 .. Steering */
            for (int i = 0; i < 8; i++) {
                uint32_t v = U32(rx_tbl_cell_word(U32(ref, 8), rx_tbl_column_index(U32(ref, 8), f[i].name), U32(ref, 0)), 0);
                SU32(spec, f[i].off, f[i].add ? U32(spec, f[i].off) + v : v);
            }
            done = 1;
        }
    }
    if (!done)
        for (uint32_t i = 0; i < 0x11; i++) {   /* the parts' bonuses by their type (+0x3c): tires 4 / 16, engine 6 / 14, suspension 7 / 15 */
            rx_addr p = U32(spec, 0xc + 4 * i);
            if (!p) continue;
            uint32_t t = U32(p, 0x3c), o = 0;
            if (t == 4 || t == 0x10) o = 0xb0; else if (t == 6 || t == 0xe) o = 0xa0; else if (t == 7 || t == 0xf) o = 0xa8; else continue;
            SU32(spec, o, U32(spec, o) + U32(p, 0x48));
            SU32(spec, o + 4, U32(spec, o + 4) + U32(p, 0x4c));
        }
    if (U32(spec, 0xc4)) rx_vehicle_set_tuning(U32(spec, 0xc4), spec + 0xa0, U32(spec, 8) + 0x1c);
}
RX_FUNCTION(0x00543130, rx_vehicle_spec_apply_tuning);

/* VehicleFactory_CreateVehicle 00542b90 (EDX = the vehicle list, s0 = the MXWorld, s1 = the spec id; RET 8) -> the vehicle */
rx_addr rx_vehicle_factory_create(rx_addr mxworld, rx_addr list, uint32_t spec_id) {
    /* the original's frame: sub esp, 0x134; push ebx, ebp, esi, edi -> B = entry ESP - 0x144 (esp between the calls) */
    const rx_addr sp0 = rx_sp(), B = sp0 - 0x144;
    rx_set_sp(B);
    veh_cookie(sp0 - 0x134, 0x12c);
    rx_addr spec = 0;
    for (int32_t i = 0; i < I32(list, 0x2c); i++) {
        rx_addr s = U32(U32(list, 0x28), 4 * (uint32_t)i);
        if (s && U32(s, 4) == spec_id) { spec = s; break; }
    }
    if (g_rx_ps_armed || g_rx_vp_on) rx_ps_vehicle_begin(spec);   /* PHYSICS STUDIO / ALIVE CLASSES (port, phys_studio.h): is it a local player's vehicle */
    uint32_t fac = U32(U32(spec, 8), 0xc);
    if (fac == 0) {
        veh_call_sp(B, 0x00542c0c, 0, 0, 0, 0);
        rx_addr v = rx_vehicle_motocross_alloc();
        veh_call_sp(B, 0x00542c17, 0, 0, 0, 0);
        SU32(spec, 0xc4, v ? rx_vehicle_motocross_construct(v) : 0);
    } else if (fac == 1) {
        veh_call_sp(B, 0x00542bfa, 0, 0, 0, 0);
        rx_addr v = rx_vehicle_atv_alloc();
        veh_call_sp(B, 0x00542c05, 0, 0, 0, 0);
        SU32(spec, 0xc4, v ? rx_vehicle_atv_construct(v) : 0);
    } else if (fac == 7) {
        veh_call_sp(B, 0x00542c57, 1, 0x15a0, 0, 0);
        rx_addr v = rx_reflex_aligned_alloc(0x15a0);
        veh_call_sp(B, 0x00542c65, 0, 0, 0, 0);
        SU32(spec, 0xc4, v ? rx_vehicle_utv_construct(v) : 0);
    } else if (fac == 3) {
        veh_call_sp(B, 0x00542c23, 1, 0x1630, 0, 0);
        rx_addr v = rx_reflex_aligned_alloc(0x1630);
        veh_call_sp(B, 0x00542c31, 0, 0, 0, 0);
        SU32(spec, 0xc4, v ? rx_vehicle_truck_construct(v) : 0);
    } else if (fac == 5) {
        veh_call_sp(B, 0x00542c3d, 1, 0x1620, 0, 0);
        rx_addr v = rx_reflex_aligned_alloc(0x1620);
        veh_call_sp(B, 0x00542c4b, 0, 0, 0, 0);
        SU32(spec, 0xc4, v ? rx_vehicle_buggy_construct(v) : 0);
    }
    rx_addr V = U32(spec, 0xc4);
    SU32(V, 0x194, U32(U32(spec, 8), 0xc));
    /* the factory's name into a local (strncpy 0x103 of g_VehicleFactoryNames[i] 0x009ce220, NUL at +0x103): unused */
    crt_strncpy(B + 0x30, U32(0, 0x009ce220 + 4 * U32(U32(spec, 8), 0xc)), 0x103);
    SU8(B + 0x30, 0x103, 0);
    veh_call_sp(B, 0x00542cc3, 1, (uint32_t)I32(U32(spec, 8), 8), 0, 0);
    rx_addr h = rx_vehicle_hierarchy_handle(list, I32(U32(spec, 8), 8));
    veh_call_sp(B, 0x00542cc9, 1, h, 0, 0);
    rx_vehicle_model_create(V, h);
    rx_set_sp(B);
    const rx_addr inst = U32(V, 0x1328);
    veh_call_sp(B, 0x00542ce5, 2, inst, spec, 0);   /* the original's push edx (instance) / push ebx (spec) / call 00541310 */
    rx_vehicle_parts_assemble(V + 0x6a0, list, inst, spec);
    rx_set_sp(B);
    crt_memset(V + 0x2a0, 0xff, 0x400);
    const rx_addr blk = B + 0x18;   /* the creation block */
    SU32(blk, 0, U32(V, 0x1328)); SU32(blk, 4, 0);
    SU32(blk, 8, U32(U32(spec, 8), 0x10)); SU32(blk, 0xc, U32(U32(spec, 8), 0x14));
    SU32(blk, 0x10, spec_id); SU32(blk, 0x14, mxworld);
    veh_call_sp(B, 0x00542d49, 1, blk, 0, 0);
    veh_vcall(V, 0x70, 1, blk, 0, 0);   /* vf28 */
    veh_call_sp(B, 0x00542d52, 1, spec + 0xc4, 0, 0);
    rx_world_ptr_vector_push_unique(list + 0x40, spec + 0xc4);
    rx_set_sp(B);
    SU32(U32(V, 0x1328), 0x140, 1);
    SU8(U32(V, 0x1328), 0x14e, 0);
    rx_addr scene = U32(mxworld, 0x20138);
    SU32(V, 0x14, U32(scene, 0x19f4));   /* the scene's entity counter, under its lock (+4: Enter / LeaveCriticalSection) */
    SU32(scene, 0x19f4, U32(scene, 0x19f4) + 1);
    veh_call_sp(B, 0x00542dae, 1, scene, 0, 0);
    rx_scene_manager_add_entity(V, scene);
    veh_call_sp(B, 0x00542db4, 1, spec, 0, 0);
    rx_vehicle_spec_apply_tuning(spec);
    if (g_rx_ps_armed || g_rx_vp_on) rx_ps_vehicle_end(spec);
    rx_set_sp(sp0);
    return V;
}
RX_FUNCTION(0x00542b90, rx_vehicle_factory_create);

/* the registry: the transliterations above that are whole original functions (their conventions in the comments) */
RX_FUNCTION(0x00706df0, rx_veh_map_head_alloc);
RX_FUNCTION(0x0042e5c0, rx_veh_rec9_init);
RX_FUNCTION(0x0042e620, rx_veh_rec_20_init);
RX_FUNCTION(0x0042d630, rx_veh_clamped_init);
RX_FUNCTION(0x00439240, rx_veh_rec_00439240);
RX_FUNCTION(0x0053f750, rx_vehicle_motocross_alloc);
RX_FUNCTION(0x004622b0, rx_vehicle_motocross_construct);
RX_FUNCTION(0x007df210, nodeset_spare_from_locals);
RX_FUNCTION(0x0042d700, rx_veh_ranged3_alloc);
RX_FUNCTION(0x00790400, rx_veh_tuning_registry_init);
RX_FUNCTION(0x00542e00, rx_vehicle_hierarchy_handle);
RX_FUNCTION(0x004300e0, model_instance_copy);
RX_FUNCTION(0x00460190, rx_vehicle_model_create);
RX_FUNCTION(0x00462b90, rx_vehicle_moto_create_proxy);
