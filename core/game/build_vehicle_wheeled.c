/* build_vehicle_wheeled.c -- construction unit 6 (vehicles, core/notes/construction.md section 15): the four-wheel classes that
 * share one layout, the UTV (factory 7, "Rhino"), the Buggy (5) and the TrophyTruck (3). Hand transliterations of the disassembly:
 *   the vehicles: 004c03a0 / 004a2550 / 0046a3e0 (constructors), 004c0700 / 004a2960 / 0046a850 (vf28: the simulation, the proxy,
 *   the named nodes, RavenVehicle::vf28, vf49, the terrain tracks slots), 004c7020 / 004ac6c0 / 004735f0 (vf49), the proxies
 *   004c0c00 / 004a2f00 / 0046aef0 (one body, the default name and two offsets differ);
 *   the simulations: 004cba90 RavenSimulationUTV_Construct (0x8850), 004b1260 RavenSimulationBuggy_Construct (0x8840), 0044fe40
 *   RavenSimulationTrophyTruck_Construct (0x8930): RavenSimulation with 25 rays, the 98 parameters (004c9d40 / 004af510), 004c0230,
 *   the main body and four sliding rotors, the circle shape, four wheel contact records, four ranged triples, the parameter file
 *   (004cb360 "utv_vehicle" / 004b0b30 "buggy_vehicle" / 0044f690 "truck2WD_vehicle" or "truck2WDlite_vehicle" by the engine class)
 *   and the masses (004cc030 / 004b1800 / 00450480). The Buggy's simulation is the UTV's with everything after the parameter block
 *   0x10 lower; the TrophyTruck's is the UTV's with a different +0x8640 block, ten more zero vectors and the engine class at +0x55f0.
 * The vehicle V: the RavenVehicle base (0045cec0) with the simulation at +0x1540 / +0x15c0 / +0x15d0 (also +0x234), the proxy
 * shape's four tire children at +0x1590 / +0x1610 / +0x1620, the named nodes from +0x1360, the tracks slots at +0x13c4 / +0x1410 /
 * +0x144c. */
#include "build_vehicle.h"
#include "fourwheel_param_load.h"
#include "phys_studio.h"
#include "build_vehicle_util.h"
#include "build_vehicle_int.h"
#include "build_world.h"
#include "build_load.h"
#include "build_alloc.h"
#include "build_asset.h"
#include "build_rider.h"
#include "anim.h"
#include "phys_vec.h"
#include "phys_misc.h"
#include "phys_body.h"
#include "loop_vehicle.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

#include "build_vehicle_utv_params.inc"   /* k_utv_params: the 98 records of 004c9d40 */
#include "build_vehicle_bt_params.inc"    /* k_bt_params: the 98 records of 004af510 (Buggy, TrophyTruck) */

static void wv_fatal(const char* what, uint32_t a) { fprintf(stderr, "rxcore: four-wheel vehicle: %s (%08x)\n", what, a); abort(); }

#define FLT_MAX_BITS 0x7f7fffffu
#define FLT_MIN_BITS 0xff7fffffu   /* -FLT_MAX */

/* ===========================================================================================================================
 * the vehicles' constructors (ESI = V) */

/* 004c03a0 RavenVehicleUTV_Construct */
rx_addr rx_vehicle_utv_construct(rx_addr v) {
    rx_raven_vehicle_construct(v, 7);
    SU32(v, 0x13fc, U32(0, 0x009cd970));
    SU32(v, 0x1400, U32(0, 0x009cd9a4));
    SU32(v, 0x1404, 0x3f800000u);
    SU32(v, 0x1408, U32(0, 0x009c4068));   /* kTerrainSunScale */
    for (uint32_t o = 0x13c4; o <= 0x13d0; o += 4) SU32(v, o, 0xffffffffu);
    SU32(v, 0, 0x0093d4d4);                 /* RavenVehicleUTV::vftable */
    SU32(v, 0x90, 0x0093d5d4); SU32(v, 0x94, 0x0093d5e4); SU32(v, 0x9c, 0x0093d624);
    for (uint32_t o = 0x1360; o <= 0x13c0; o += 4) SU32(v, o, 0);
    for (uint32_t o = 0x13d4; o <= 0x13f8; o += 4) SU32(v, o, 0);
    veh_fill(v + 0x1410, 0x20, 0);          /* two zero splats */
    SU32(v, 0x1430, 0); SU32(v, 0x1434, 0);
    rx_veh_clamped_init(v + 0x1438, 0, 0x3dcccccdu /* 0.1 (x87 copy) */, FLT_MAX_BITS);   /* 0042d630 */
    rx_veh_clamped_init(v + 0x144c, 0, 0x3dcccccdu, FLT_MAX_BITS);
    SU32(v, 0x1460, 0x3f800000u); SU32(v, 0x1464, 0x3f800000u);
    for (uint32_t o = 0x1468; o <= 0x1494; o += 4) SU32(v, o, 0);
    SU32(v, 0x1540, 0); SU32(v, 0x1544, 0); SU32(v, 0x1550, 0);
    SU32(v, 0x1514, U32(0, 0x009cc380));
    SU32(v, 0x1518, 0x42c80000u);           /* 100.0 */
    SU32(v, 0x151c, U32(0, 0x009d3bfc));    /* k_MouseSensitivityPerPercent */
    SU32(v, 0x1520, 0x41f00000u);           /* 30.0 */
    SU32(v, 0x1524, 0x40a00000u);           /* 5.0 */
    SU32(v, 0x1528, 0x3e4ccccdu);           /* 0.2 */
    SU32(v, 0x1530, 0x40a00000u);           /* 5.0 */
    for (uint32_t o = 0x1590; o <= 0x159c; o += 4) SU32(v, o, 0);
    for (uint32_t o = 0x14a0; o <= 0x14ac; o += 4) SU32(v, o, 0);
    SU32(v, 0x152c, 0x3db851ecu);           /* 0.09 */
    SU32(v, 0x1534, U32(0, 0x009d78dc));    /* ModelDrawDistance_RadiusLimit50 */
    return v;
}

/* 004a2550 RavenVehicleBuggy_Construct */
rx_addr rx_vehicle_buggy_construct(rx_addr v) {
    rx_raven_vehicle_construct(v, 5);
    SU32(v, 0, 0x0093d2e4);                 /* RavenVehicleBuggy::vftable */
    SU32(v, 0x90, 0x0093d3e4); SU32(v, 0x94, 0x0093d3f4); SU32(v, 0x9c, 0x0093d434);
    for (uint32_t o = 0x1360; o <= 0x13d4; o += 4) SU32(v, o, 0);
    /* a RangedFloat {0, 0.5, 1, FLT_MAX, -FLT_MAX} at +0x13d8 */
    SU32(v, 0x13dc, 0x3f000000u); SU32(v, 0x13e4, FLT_MAX_BITS); SU32(v, 0x13e8, FLT_MIN_BITS); SU32(v, 0x13e0, 0x3f800000u);
    SU32(v, 0x13d8, 0); SU32(v, 0x13ec, 0); SU32(v, 0x13f0, 0);
    veh_v4(v + 0x1400, VEH_RT_W1);
    SU32(v, 0x1450, 0x3f800000u); SU32(v, 0x1454, 0x3f800000u);
    SU32(v, 0x1448, 0x3fc00000u);           /* 1.5 */
    for (uint32_t o = 0x1410; o <= 0x141c; o += 4) SU32(v, o, 0xffffffffu);
    for (uint32_t o = 0x1420; o <= 0x1444; o += 4) SU32(v, o, 0);
    SU32(v, 0x144c, 0x40000000u);           /* 2.0 */
    veh_fill(v + 0x1460, 0x20, 0);          /* two zero splats */
    SU32(v, 0x1480, 0); SU32(v, 0x1484, 0);
    rx_veh_clamped_init(v + 0x1488, 0, 0x3dcccccdu, FLT_MAX_BITS);
    rx_veh_clamped_init(v + 0x149c, 0, 0x3dcccccdu, FLT_MAX_BITS);
    SU32(v, 0x14b0, 0x3f800000u); SU32(v, 0x14b4, 0x3f800000u);
    for (uint32_t o = 0x14d8; o <= 0x14e4; o += 4) SU32(v, o, 0);
    SU32(v, 0x1594, 0);
    SU32(v, 0x1590, 0x3d888889u);           /* 1 / 15 */
    for (uint32_t o = 0x14b8; o <= 0x14d4; o += 4) SU32(v, o, 0);
    veh_v4(v + 0x15a0, 0x00d61660); veh_v4(v + 0x15b0, 0x00d61660);   /* the texture parameters' vectors (runtime 0x00d61660) */
    SU32(v, 0x15c0, 0); SU32(v, 0x15c4, 0); SU32(v, 0x15d0, 0);
    SU32(v, 0x1564, 0xc0800000u);           /* -4.0 */
    SU32(v, 0x1568, U32(0, 0x009d7b34));
    SU32(v, 0x156c, 0x3dcccccdu);           /* 0.1 */
    SU32(v, 0x1570, U32(0, 0x009cc37c));
    SU32(v, 0x157c, 0x3dcccccdu);
    SU32(v, 0x1574, 0x40a00000u);           /* 5.0 */
    SU32(v, 0x1580, 0x40c00000u);           /* 6.0 */
    for (uint32_t o = 0x1610; o <= 0x161c; o += 4) SU32(v, o, 0);
    for (uint32_t o = 0x14f0; o <= 0x14fc; o += 4) SU32(v, o, 0);
    SU32(v, 0x1578, 0x3e4ccccdu);           /* 0.2 */
    SU32(v, 0x1584, U32(0, 0x009d78dc));    /* ModelDrawDistance_RadiusLimit50 */
    return v;
}

/* 0046a3e0 RavenVehicleTrophyTruck_Construct */
rx_addr rx_vehicle_truck_construct(rx_addr v) {
    rx_raven_vehicle_construct(v, 3);
    SU32(v, 0, 0x0093cee4);                 /* RavenVehicleTrophyTruck::vftable */
    SU32(v, 0x90, 0x0093cfe4); SU32(v, 0x94, 0x0093cff4); SU32(v, 0x9c, 0x0093d034);
    for (uint32_t o = 0x1360; o <= 0x13dc; o += 4) SU32(v, o, 0);
    /* four RangedFloats {0, rate, 1, FLT_MAX, -FLT_MAX} at +0x13f8 (rates [0x009d789c], 1.5, 2.0, 1.0); +0x13f0 / +0x13f4 zero */
    SU32(v, 0x13f0, 0); SU32(v, 0x13f4, 0);
    const uint32_t rate[4] = {U32(0, 0x009d789c), 0x3fc00000u, 0x40000000u, 0x3f800000u};
    for (uint32_t i = 0; i < 4; i++) {
        rx_addr r = v + 0x13f8 + 0x14 * i;
        SU32(r, 0, 0); SU32(r, 4, rate[i]); SU32(r, 0xc, FLT_MAX_BITS); SU32(r, 0x10, FLT_MIN_BITS); SU32(r, 8, 0x3f800000u);
    }
    for (uint32_t o = 0x144c; o <= 0x1458; o += 4) SU32(v, o, 0xffffffffu);
    for (uint32_t o = 0x145c; o <= 0x146c; o += 4) SU32(v, o, 0);
    SU32(v, 0x148c, 0x3f800000u); SU32(v, 0x1490, 0x3f800000u);
    for (uint32_t o = 0x1470; o <= 0x1480; o += 4) SU32(v, o, 0);
    SU32(v, 0x1484, 0x40000000u); SU32(v, 0x1488, 0x40000000u);   /* 2.0 */
    veh_fill(v + 0x14a0, 0x20, 0);          /* two zero splats */
    SU32(v, 0x14c0, 0); SU32(v, 0x14c4, 0);
    rx_veh_clamped_init(v + 0x14c8, 0, 0x3dcccccdu, FLT_MAX_BITS);
    rx_veh_clamped_init(v + 0x14dc, 0, 0x3dcccccdu, FLT_MAX_BITS);
    SU32(v, 0x14f0, 0x3f800000u); SU32(v, 0x14f4, 0x3f800000u);
    for (uint32_t o = 0x1518; o <= 0x1524; o += 4) SU32(v, o, 0);
    for (uint32_t o = 0x14f8; o <= 0x1514; o += 4) SU32(v, o, 0);
    SU32(v, 0x15d0, 0); SU32(v, 0x15d4, 0); SU32(v, 0x15e0, 0);
    for (uint32_t o = 0x1620; o <= 0x162c; o += 4) SU32(v, o, 0);
    for (uint32_t o = 0x13e0; o <= 0x13ec; o += 4) SU32(v, o, 0);
    SU32(v, 0x15a4, 0xc0800000u);           /* -4.0 */
    SU32(v, 0x15a8, 0x437a0000u);           /* 250.0 */
    SU32(v, 0x15ac, 0x3dcccccdu);           /* 0.1 */
    SU32(v, 0x15b0, U32(0, 0x009cc37c));
    SU32(v, 0x15bc, 0x3dcccccdu);
    SU32(v, 0x15b4, 0x40a00000u);           /* 5.0 */
    SU32(v, 0x15c0, 0x40c00000u);           /* 6.0 */
    for (uint32_t o = 0x1530; o <= 0x153c; o += 4) SU32(v, o, 0);
    SU32(v, 0x15b8, 0x3e4ccccdu);           /* 0.2 */
    SU32(v, 0x15c4, U32(0, 0x009d78dc));    /* ModelDrawDistance_RadiusLimit50 */
    return v;
}

/* vf49 (thiscall V, value; RET 4): the simulation's +0xb0 -- 004c7020 UTV, 004ac6c0 Buggy, 004735f0 TrophyTruck */
void rx_vehicle_utv_vf49(rx_addr V, uint32_t value) { SU32(U32(V, 0x1540), 0xb0, value); }
void rx_vehicle_buggy_vf49(rx_addr V, uint32_t value) { SU32(U32(V, 0x15c0), 0xb0, value); }
void rx_vehicle_truck_vf49(rx_addr V, uint32_t value) { SU32(U32(V, 0x15d0), 0xb0, value); }

/* ===========================================================================================================================
 * the proxy: 004c0c00 UTV, 004a2f00 Buggy, 0046aef0 TrophyTruck (ECX = the creation block, s0 = V; RET 4) */

/* an IntrusiveHandle<ObjectMonitor<ProxyObject>> {vtable, object} (the object's +0 its reference count) */
static void wv_handle_assign(rx_addr dst, rx_addr src) {   /* 004298d0 (ESI = dst, s0 = src; RET 4) */
    rx_addr t = U32(src, 4);
    if (U32(dst, 4) == t) return;
    rx_addr old = U32(dst, 4);
    if (old) { SU32(old, 0, U32(old, 0) - 1); if (!U32(old, 0)) wv_fatal("004298d0: proxy released (0042b0f0)", old); }
    SU32(dst, 4, t);
    if (t) SU32(t, 0, U32(t, 0) + 1);
}
static void wv_handle_destroy(rx_addr h) {                 /* 0041c770 (ECX = h) */
    SU32(h, 0, 0x0093a650);
    rx_addr t = U32(h, 4);
    if (t) { SU32(t, 0, U32(t, 0) - 1); if (!U32(t, 0)) wv_fatal("0041c770: proxy freed (0042af70)", t); }
}

typedef struct wv_proxy {
    rx_addr fn;          /* the function's address: its return addresses are the UTV's (004c0c00) moved by fn - 004c0c00 */
    rx_addr def_name;    /* "UTVProxyCollision" / "BuggyProxyCollision" / "TrophyTruckProxyCollision" */
    uint32_t tires;      /* V + this: the four tire children */
    uint32_t sim;        /* V + this: the simulation */
} wv_proxy;
static const wv_proxy k_proxy_utv = {0x004c0c00, 0x0093c6cc, 0x1590, 0x1540};
static const wv_proxy k_proxy_buggy = {0x004a2f00, 0x0093c5a8, 0x1610, 0x15c0};
static const wv_proxy k_proxy_truck = {0x0046aef0, 0x0093bfa4, 0x1620, 0x15d0};

/* the proxy collision "<hierarchy name with Hierarchy -> Proxy>" when the databases list it, else the class's default; none: no
 * proxy. The ProxyObject with the "Vehicle" physics material, the proxy shape's four tire children (FLTire / FRTire / RLTire /
 * RRTire) flagged 4, the simulation's handle (+0x2ebc), its vf18. The three std::strings live in the original's frame. */
static void wv_create_proxy(const wv_proxy* d, rx_addr blk, rx_addr V) {
#define RET(a) (d->fn + ((a) - 0x004c0c00u))
    /* push ebp; mov ebp, esp; and esp, -8; sub esp, 0x64; cookie [esp + 0x60]; push ebx, esi, edi */
    const rx_addr sp0 = rx_sp(), E = ((sp0 - 4) & ~7u) - 0x64, B = E - 0xc;
    rx_set_sp(B);
    veh_cookie(E, 0x60);
    const rx_addr s_def = B + 0x50, s_name = B + 0x34, s_mat = B + 0x18, out = B + 0x10;
    veh_call_sp(B, RET(0x004c0c2b), 1, d->def_name, 0, 0); crt_str_ctor_cstr(s_def, d->def_name);
    veh_call_sp(B, RET(0x004c0c3a), 1, s_def, 0, 0); crt_str_ctor_copy(s_name, s_def);
    rx_addr db = U32(U32(0, 0x00a95844), 8);
    if (U32(blk, 0)) {   /* the instance's hierarchy name (vf2 of its +0x13c), "Hierarchy" -> "Proxy" */
        rx_addr o = U32(U32(blk, 0), 0x13c);
        veh_call_sp(B, RET(0x004c0c58), 0, 0, 0, 0);
        rx_addr nm = veh_vcall(o, 8, 0, 0, 0, 0);
        veh_call_sp(B, RET(0x004c0c63), 1, nm, 0, 0); crt_str_operator_assign(s_name, nm);
        SU32(B, -4, 0x0093bfc0); SU32(B, -8, 9);   /* replace's last two arguments, pushed first */
        veh_call_sp(B - 8, RET(0x004c0c7b), 2, 0x0093bfc8, 0, 0);
        uint32_t pos = crt_str_find_cstr(s_name, 0x0093bfc8 /* "Hierarchy" */, 0);
        veh_call_sp(B, RET(0x004c0c86), 3, pos, 9, 0x0093bfc0); crt_str_replace_cstr(s_name, pos, 9, 0x0093bfc0 /* "Proxy" */);
    }
#define STR_PTR(s) (U32((s), 0x18) >= 0x10 ? U32((s), 4) : (s) + 4)
    rx_addr name = STR_PTR(s_name);
    veh_call_sp(B, RET(0x004c0cad), 3, name, 0x6c6c6f63, 0x7369);
    if (!rx_database_vf_has_handle(db, name, 0x6c6c6f63u, 0x7369u)) {   /* Database vf26 (name, 'collis') */
        name = STR_PTR(s_def);
        veh_call_sp(B, RET(0x004c0ce3), 3, name, 0x6c6c6f63, 0x7369);
        if (!rx_database_vf_has_handle(db, name, 0x6c6c6f63u, 0x7369u)) name = 0;
    }
#undef STR_PTR
    rx_addr h = 0;
    if (name) {
        SU32(B, -4, 1);                             /* the fourth argument */
        veh_call_sp(B - 4, RET(0x004c0d0f), 3, name, 0x6c6c6f63, 0x7369);
        h = rx_database_vf_handle(db, name, 0x6c6c6f63u, 0x7369u, 1);   /* Database vf27 (name, 'collis', 1) */
    }
    if (h) {
        veh_call_sp(B, RET(0x004c0d28), 1, 0x00935e68, 0, 0); crt_str_ctor_cstr(s_mat, 0x00935e68);   /* "Vehicle" */
        veh_call_sp(B, RET(0x004c0d3d), 0, 0, 0, 0);
        rx_addr reg = rx_phys_props_get();                                  /* the registry's one-time build (00850010) */
        veh_call_sp(B, RET(0x004c0d5e), 0, 0, 0, 0);
        uint32_t mat = rx_phys_props_find(reg, "Vehicle");                  /* 00850bf0 */
        rx_addr world = U32(U32(V, 0x188), 0x20134);
        veh_call_sp(B, RET(0x004c0d7a), 3, world, out, mat);
        rx_addr r = rx_proxy_object_create(world, out, mat, h, h);         /* 00769160 (ECX = EDX = the handle) */
        veh_call_sp(B, RET(0x004c0d80), 1, r, 0, 0); wv_handle_assign(V + 0x178, r);
        veh_call_sp(B, RET(0x004c0d89), 0, 0, 0, 0); wv_handle_destroy(out);
        veh_call_sp(B, RET(0x004c0d93), 0, 0, 0, 0); crt_str_dtor(s_mat);
        rx_addr rec = phys_body_record(U32(V, 0x17c));
        SU32(rec, 0x34, V);
        rec = phys_body_record(U32(V, 0x17c));
        rx_addr shape = U32(rec, 0x2c);
        static const rx_addr names[4] = {0x0093bfd4, 0x0093bfdc, 0x0093bfe4, 0x0093bfec};   /* FLTire FRTire RLTire RRTire */
        for (uint32_t i = 0; i < 4; i++) SU32(B, 0x18 + 4 * i, names[i]);
        SU32(B, 0xc, V + d->tires);
        for (uint32_t i = 0; i < 4; i++) {
            veh_call_sp(B, RET(0x004c0dee), 1, names[i], 0, 0);
            uint32_t idx = veh_vcall(shape, 0x20, 1, names[i], 0, 0);   /* vf8: the child by name */
            rx_set_sp(B);
            SU32(V, d->tires + 4 * i, idx);
            SU32(B, 0x10, idx);
            if (idx >= (U32(shape, 0x30) - U32(shape, 0x2c)) >> 2) wv_fatal("proxy: child index", idx);
            rx_addr child = U32(U32(shape, 0x2c), 4 * idx);
            SU32(child, 0x14, U32(child, 0x14) | 4);
            SU32(B, 0xc, V + d->tires + 4 + 4 * i);
        }
        rx_addr S = U32(V, d->sim);
        SU32(S, 0x2ebc, V + 0x178);
        veh_call_sp(B, RET(0x004c0e40), 0, 0, 0, 0);
        veh_vcall(S, 0x48, 0, 0, 0, 0);   /* S vf18 (004da440) */
        SU8(V, 0x174, 1);
    }
    veh_call_sp(B, RET(0x004c0e51), 0, 0, 0, 0); crt_str_dtor(s_name);
    veh_call_sp(B, RET(0x004c0e5b), 0, 0, 0, 0); crt_str_dtor(s_def);
    rx_set_sp(sp0);
#undef RET
}
void rx_vehicle_utv_create_proxy(rx_addr blk, rx_addr V) { wv_create_proxy(&k_proxy_utv, blk, V); }
void rx_vehicle_buggy_create_proxy(rx_addr blk, rx_addr V) { wv_create_proxy(&k_proxy_buggy, blk, V); }
void rx_vehicle_truck_create_proxy(rx_addr blk, rx_addr V) { wv_create_proxy(&k_proxy_truck, blk, V); }

/* ===========================================================================================================================
 * the simulations */

/* Existing public native signatures and RX_FUNCTION registrations remain the
 * single entry points. Exact bodies preserve the original guest caller frames;
 * Truck's native explicit P corresponds to original EDI, cls to stack arg0. */
/* PHYSICS STUDIO (port, phys_studio.h): a config's values over the file's after each load; nothing without one */
void rx_sim_utv_params_load(rx_addr P) { rx_param_load_utv_exact(P); rx_ps_params_loaded(P, RX_PS_UTV); }
void rx_sim_buggy_params_load(rx_addr P) { rx_param_load_buggy_exact(P); rx_ps_params_loaded(P, RX_PS_BUGGY); }
void rx_sim_truck_params_load(rx_addr P, uint32_t cls) {
    rx_param_load_truck_exact(P, cls);
    rx_ps_params_loaded(P, cls == 7 ? RX_PS_TRUCK2WD : cls == 8 ? RX_PS_TRUCK2WDLITE : -1);
}

/* the masses (EDX = S): 004cc030 UTV, 004b1800 Buggy, 00450480 TrophyTruck. The gravity, the five bodies' masses and inertias, the
 * rotors' frames, the filters and limits, the wheels' splats and offset vectors, the derived values; vf35. */
typedef struct wv_mass {
    int32_t d;           /* the offsets from the rotors on: 0 (UTV, TrophyTruck), -0x10 (Buggy) */
    uint32_t front;      /* the front pair (+0x80a0 / +0x81f0 + d): 0 zero vectors, else (-x / 2, 0, 0, 0) / (x / 2, 0, 0, 0) of S + this */
    uint32_t rear;       /* the rear pair (+0x8340 / +0x8490 + d), the same */
    uint32_t prod;       /* the five derived products' first offset */
} wv_mass;
static const wv_mass k_mass_utv = {0, 0, 0, 0x8664};
static const wv_mass k_mass_buggy = {-0x10, 0, 0x4774, 0x8654};
static const wv_mass k_mass_truck = {0, 0x42d4, 0x4774, 0x8668};

static void wv_offset_pair(rx_addr S, uint32_t lo, uint32_t hi, uint32_t src) {
    if (!src) { veh_v4(S + lo, VEH_RT_ZERO); veh_v4(S + hi, VEH_RT_ZERO); return; }
    const float x = F32(S, src);
    SF32(S + lo, 0, phys_mul(x, -0.5f)); SU32(S + lo, 4, 0); SU32(S + lo, 8, 0); SU32(S + lo, 0xc, 0);
    SF32(S + hi, 0, phys_mul(x, 0.5f)); SU32(S + hi, 4, 0); SU32(S + hi, 8, 0); SU32(S + hi, 0xc, 0);
}
static void wv_setup_masses(const wv_mass* m, rx_addr S) {
    const int32_t d = m->d;
#define O(x) ((uint32_t)((int32_t)(x) + d))
    rx_addr M = S + 0xd0;
    veh_v4(M, S + 0x3370);                                   /* VehicleSim_Gravity */
    static const uint16_t mass[5] = {0x3734, 0x37c4, 0x37c4, 0x3854, 0x3854};
    static const uint16_t inertia[5] = {0x38b0, 0x3970, 0x3970, 0x3a30, 0x3a30};
    for (uint32_t i = 0; i < 5; i++) rx_veh_mechanism_set_mass(M, F32(S, mass[i]), i);
    for (uint32_t i = 0; i < 5; i++) {   /* the inertia vectors and their reciprocals (divps from the 1-splat) */
        veh_v4(M + 0x680 + 0xa0 * i, S + inertia[i]);
        rx_st4(M + 0x690 + 0xa0 * i, v4_div(rx_ld4(0x00d61650), rx_ld4(S + inertia[i])));
    }
    /* the four rotors' frames: -(0, 1, 0, 0), -(1, 0, 0, 0), -(0, 0, 1, 1) (subps from zero), identity, and their radius */
    static const struct { uint16_t base, radius; } rot[4] = {{0x5b20, 0x3dcc}, {0x6480, 0x3dcc}, {0x6de0, 0x402c}, {0x7740, 0x402c}};
    const rx_v4 z = v4(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; i++) {
        rx_addr b = S + O(rot[i].base);
        rx_st4(b, v4_sub(z, rx_ld4(0x00d616a0)));
        rx_st4(b + 0x10, v4_sub(z, rx_ld4(0x00d615c0)));
        rx_st4(b + 0x20, v4_sub(z, rx_ld4(0x00d61680)));
        veh_v4(b + 0x30, VEH_RT_W1);
        SU32(b, 0x40, veh_x87_copy(U32(S, rot[i].radius)));   /* fld / fstp */
    }
    SU32(S, O(0x5754), U32(0, 0x009d3bfc)); SU32(S, O(0x5750), 0); SU32(S, O(0x5758), 0x3f800000u);
    phys_lag_filter_set(S + 0x94, 0.0f, F32(S, 0x33c4));    /* 0042d670 inline */
    SU32(S, O(0x5764), 0); SU32(S, O(0x5768), 0x3e19999au); SU32(S, O(0x576c), 0x3f800000u);   /* 0.15 */
    for (uint32_t k = 0; k < 4; k++) rx_vehicle_set_limits(VEH_ZERO3, S + O(0x85d0) + 0xc * k, 0.1f /* 0x931d94 */);   /* 0042d8c0 inline */
    rx_v4 a = v4_splat(F32(S, 0x428c));
    rx_st4(S + O(0x8090), a); rx_st4(S + O(0x81e0), a);
    rx_v4 b = v4_splat(F32(S, 0x472c));
    rx_st4(S + O(0x8330), b); rx_st4(S + O(0x8480), b);
    wv_offset_pair(S, O(0x80a0), O(0x81f0), m->front);
    wv_offset_pair(S, O(0x8340), O(0x8490), m->rear);
    SU32(S, O(0x8658), veh_x87_copy(U32(S, 0x4f9c)));
    SU32(S, 0x2eb0, F32(S, 0x502c) > 0.0f ? 1 : 0);
    float e = phys_mul(F32(S, 0x5224), F32(S, 0x5074));
    static const uint16_t src[5] = {0x50bc, 0x5104, 0x514c, 0x5194, 0x51dc};
    for (uint32_t i = 0; i < 5; i++) SF32(S, m->prod + 4 * i, phys_mul(F32(S, src[i]), e));
    veh_vcall(S, 0x8c, 0, 0, 0, 0);   /* vf35 */
#undef O
}
void rx_sim_utv_setup_masses(rx_addr S) { wv_setup_masses(&k_mass_utv, S); }
void rx_sim_buggy_setup_masses(rx_addr S) { wv_setup_masses(&k_mass_buggy, S); }
void rx_sim_truck_setup_masses(rx_addr S) { wv_setup_masses(&k_mass_truck, S); }

/* the constructors: 004cba90 UTV, 004b1260 Buggy (stdcall S; RET 4), 0044fe40 TrophyTruck (stdcall S, engine class; RET 8) */
typedef struct wv_sim {
    uint32_t vtable;          /* RavenSimulation<class>::vftable */
    const veh_param* params;  /* the parameter records (004c9d40 / 004af510) */
    int32_t d;                /* the offsets after the parameter block: 0, Buggy -0x10 */
    uint32_t utv;             /* UTV: +0x55f0 = -1 before the record */
    uint32_t rate6;           /* the sixth RangedFloat's rate */
    uint32_t truck;           /* TrophyTruck: its +0x8640 block, the ten zero vectors at +0x8810, the class at +0x55f0 */
    void (*load)(rx_addr P, uint32_t cls);
    const wv_mass* mass;
    rx_addr rets[20];         /* the return address of each call, in order */
} wv_sim;
static void wv_load_utv(rx_addr P, uint32_t cls) { (void)cls; rx_sim_utv_params_load(P); }
static void wv_load_buggy(rx_addr P, uint32_t cls) { (void)cls; rx_sim_buggy_params_load(P); }
static const wv_sim k_sim_utv = {0x0093d43c, k_utv_params, 0, 1, 0x3e800000u /* 0.25 */, 0, wv_load_utv, &k_mass_utv,
    {0x004cbaa0, 0x004cbab1, 0x004cbac7, 0x004cbbfd, 0x004cbc08, 0x004cbc13, 0x004cbc1e, 0x004cbc29, 0x004cbc5f, 0x004cbc7f,
     0x004cbc8e, 0x004cbc9d, 0x004cbcac, 0x004cbce9, 0x004cbcfe, 0x004cbd13, 0x004cbd28, 0x004cbe4b, 0x004cbeec, 0x004cbef6}};
static const wv_sim k_sim_buggy = {0x0093d24c, k_bt_params, -0x10, 0, 0x3e19999au /* 0.15 */, 0, wv_load_buggy, &k_mass_buggy,
    {0x004b1270, 0x004b1281, 0x004b128d, 0x004b13c3, 0x004b13ce, 0x004b13d9, 0x004b13e4, 0x004b13ef, 0x004b1425, 0x004b1445,
     0x004b1454, 0x004b1463, 0x004b1472, 0x004b14af, 0x004b14c4, 0x004b14d9, 0x004b14ee, 0x004b1611, 0x004b16b2, 0x004b16bc}};
static const wv_sim k_sim_truck = {0x0093c93c, k_bt_params, 0, 0, 0x3e19999au /* 0.15 */, 1, rx_sim_truck_params_load, &k_mass_truck,
    {0x0044fe50, 0x0044fe61, 0x0044fe6d, 0x0044ffa3, 0x0044ffae, 0x0044ffb9, 0x0044ffc4, 0x0044ffcf, 0x00450005, 0x00450025,
     0x00450034, 0x00450043, 0x00450052, 0x0045008f, 0x004500a4, 0x004500b9, 0x004500ce, 0x00450291, 0x00450338, 0x00450342}};

static rx_addr wv_sim_construct(const wv_sim* c, rx_addr S, uint32_t cls) {
    const int32_t d = c->d;
#define O(x) ((uint32_t)((int32_t)(x) + d))
    const rx_addr sp0 = rx_sp(), B = sp0 - 0x10;   /* push ebx, ebp, esi, edi */
    const rx_addr* R = c->rets;
    veh_call_sp(B, R[0], 2, S, 0x19, 0);
    rx_raven_simulation_construct(S, 0x19);
    SU32(S, 0, c->vtable);
    veh_call_sp(B, R[1], 0, 0, 0, 0);
    rx_veh_params_register(S + 0x2f10, c->params, 98);   /* 004c9d40 / 004af510 (ESI = P) */
    if (c->utv) SU32(S, 0x55f0, 0xffffffffu);
    veh_call_sp(B, R[2], 1, S + O(0x5600), 0, 0);
    rx_veh_rec_init(S + O(0x5600));
    rx_set_sp(B);
    /* six RangedFloats {value, rate, 1, FLT_MAX, -FLT_MAX} (rates 1, 1, 1, 1, k_MouseSensitivityPerPercent, rate6) */
    const uint32_t rates[6] = {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u, U32(0, 0x009d3bfc), c->rate6};
    for (uint32_t i = 0; i < 6; i++) {
        rx_addr r = S + O(0x5700) + 0x14 * i;
        SU32(r, 0, 0); SU32(r, 4, rates[i]); SU32(r, 0xc, FLT_MAX_BITS); SU32(r, 0x10, FLT_MIN_BITS); SU32(r, 8, 0x3f800000u);
    }
    SU32(S, O(0x5778), 0); SU32(S, O(0x577c), 0);
    const uint32_t node[5] = {O(0x5780), O(0x5aa0), O(0x6400), O(0x6d60), O(0x76c0)};
    veh_call_sp(B, R[3], 0, 0, 0, 0); rx_veh_main_body_node_construct(S + node[0]);
    for (uint32_t i = 0; i < 4; i++) { veh_call_sp(B, R[4 + i], 0, 0, 0, 0); rx_veh_sliding_rotor_node_construct(S + node[1 + i]); }
    for (uint32_t o = O(0x8020); o <= O(0x8034); o += 4) SU32(S, o, 0);
    veh_call_sp(B, R[8], 0, 0, 0, 0);
    rx_collision_shape_construct(S + O(0x8040), 0x400);   /* 00844110 */
    SU32(S, O(0x8040), 0x00976750);                       /* RavenPhysics::CollisionShapeCircle::vftable */
    veh_v4(S + O(0x8060), VEH_RT_ZERO);
    for (uint32_t k = 0; k < 4; k++) {   /* 0042d9b0 ids 1 .. 4 */
        veh_call_sp(B, R[9 + k], 1, S + O(0x8080) + 0x150 * k, 0, 0);
        rx_veh_wheel_record_init(S + O(0x8080) + 0x150 * k, 1 + k);
    }
    rx_set_sp(B);
    for (uint32_t o = O(0x85c0); o <= O(0x85cc); o += 4) SU32(S, o, 0);
    for (uint32_t k = 0; k < 4; k++) {   /* 0042d700 */
        veh_call_sp(B, R[13 + k], 1, 0x3dcccccdu, 0, 0);
        rx_veh_ranged3_alloc(S + O(0x85d0) + 0xc * k, VEH_ZERO3, 0x3dcccccdu /* 0.1 (x87 copy) */);
    }
    rx_set_sp(B);
    for (uint32_t o = O(0x8600); o <= O(0x8630); o += 0x10) veh_v4(S + o, VEH_RT_ZERO);
    for (uint32_t o = O(0x8640); o <= O(0x8654); o += 4) SU32(S, o, 0);
    SU32(S, O(0x865c), 0); SU32(S, O(0x8660), 0);
    if (!c->truck) {   /* 0.5, 0.3, 0.15 */
        SU32(S, O(0x8678), 0x3f000000u); SU32(S, O(0x867c), 0x3e99999au); SU32(S, O(0x8680), 0x3e19999au);
        SU32(S, O(0x8684), 0);
    } else {
        SU32(S, 0x867c, 0x3f000000u); SU32(S, 0x8684, 0x3e99999au); SU32(S, 0x8688, 0x3e19999au);
        SU32(S, 0x8664, 0); SU32(S, 0x8680, 0); SU32(S, 0x868c, 0);
    }
    veh_v4(S + O(0x8690), VEH_RT_ZERO); veh_v4(S + O(0x86a0), VEH_RT_ZERO);
    for (uint32_t o = O(0x86b0); o <= O(0x86e0); o += 0x10) veh_v4(S + o, VEH_RT_W1);
    SU32(S, O(0x8700), 0); SU32(S, O(0x8704), 0); SU32(S, O(0x8708), 0);
    if (c->truck) for (uint32_t o = 0x8810; o <= 0x88a0; o += 0x10) veh_v4(S + o, VEH_RT_ZERO);
    veh_call_sp(B, R[17], 1, c->truck ? cls : S + 0x2f10, 0, 0);
    c->load(S + 0x2f10, cls);                          /* 004cb360 / 004b0b30 / 0044f690 */
    rx_set_sp(B);
    if (c->truck) SU32(S, 0x55f0, cls);
    /* the mechanism's five nodes: index / parent */
    rx_addr M = S + 0xd0;
    for (uint32_t i = 0; i < 5; i++) {
        SU32(M, 0x1c, U32(M, 0x1c) + 1);
        SU32(M, 0xca0 + 4 * i, S + node[i]);
        SU32(S + node[i], 8, i);
        SU32(S + node[i], 0xc, i ? 0 : 0xffffffffu);
    }
    for (uint32_t i = 1; i < 5; i++) SU32(S + node[i], 4, 0);
    veh_call_sp(B, R[18], 0, 0, 0, 0);
    rx_veh_mechanism_bind(M);
    SU32(S, 0x58, 1);
    veh_call_sp(B, R[19], 0, 0, 0, 0);
    wv_setup_masses(c->mass, S);                       /* 004cc030 / 004b1800 / 00450480 */
    rx_set_sp(sp0);
    return S;
#undef O
}
rx_addr rx_sim_utv_construct(rx_addr S) { return wv_sim_construct(&k_sim_utv, S, 0); }
rx_addr rx_sim_buggy_construct(rx_addr S) { return wv_sim_construct(&k_sim_buggy, S, 0); }
rx_addr rx_sim_truck_construct(rx_addr S, uint32_t cls) { return wv_sim_construct(&k_sim_truck, S, cls); }

/* ===========================================================================================================================
 * vf28 (ECX = V, s0 = the creation block; RET 4): push ebx, ebp, esi, edi around everything */

typedef struct wv_find { rx_addr ret; uint16_t off; rx_addr name; } wv_find;

/* the physics world's simulations (+0x1940: an Rdb pointer array; 007cff20 when full) */
static void wv_push_simulation(rx_addr V, rx_addr B, rx_addr ret, rx_addr S) {
    rx_addr a = U32(U32(V, 0x188), 0x20134) + 0x1940;
    if ((int32_t)U32(a, 4) >= (int32_t)U32(a, 8)) { veh_call_sp(B, ret, 1, U32(a, 0xc) + U32(a, 8), 0, 0); rx_iarr_reserve(a, U32(a, 0xc) + U32(a, 8)); rx_set_sp(B); }
    SU32(U32(a, 0), 4 * U32(a, 4), S);
    SU32(a, 4, U32(a, 4) + 1);
}
/* the named nodes (007de9e0 on the instance's set) */
static void wv_find_nodes(rx_addr V, rx_addr B, const wv_find* f, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        veh_call_sp(B, f[i].ret, 1, f[i].name, 0, 0);
        SU32(V, f[i].off, rx_hierarchy_find_node(U32(V, 0x1328) + 0x10, f[i].name));
    }
    rx_set_sp(B);
}
/* RavenVehicle::vf28, vf49 (0), the tracks slots (kind; two of [0x00a95774] | 0x1000, two of [0x00a95778] | 0x2000) */
static void wv_finish(rx_addr V, rx_addr B, rx_addr blk, rx_addr ret_vf28, rx_addr ret_vf49, const rx_addr rets[4], uint32_t kind, uint32_t slots) {
    veh_call_sp(B, ret_vf28, 1, blk, 0, 0);
    rx_raven_vehicle_vf28(V, blk);
    veh_call_sp(B, ret_vf49, 1, 0, 0, 0);
    veh_vcall(V, 0xc4, 1, 0, 0, 0);   /* vf49 */
    if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    for (uint32_t k = 0; k < 4; k++) {
        veh_call_sp(B, rets[k], 1, kind, 0, 0);
        uint32_t slot = (uint32_t)rx_tracks_alloc_slot(U32(0, k < 2 ? 0x00a95774 : 0x00a95778), kind);
        SU32(V, slots + 4 * k, slot | (k < 2 ? 0x1000u : 0x2000u));
    }
    rx_set_sp(B);
}

/* RavenVehicleUTV::vf28 004c0700 */
void rx_vehicle_utv_vf28(rx_addr V, rx_addr blk) {
    const rx_addr sp0 = rx_sp(), B = sp0 - 0x10;
    rx_set_sp(B);
    SU32(V, 0x188, U32(blk, 0x14));
    veh_call_sp(B, 0x004c071d, 1, 0x8850, 0, 0);
    rx_addr S = rx_reflex_aligned_alloc(0x8850);
    if (S) { veh_call_sp(B, 0x004c072a, 1, S, 0, 0); S = rx_sim_utv_construct(S); }
    rx_set_sp(B);
    SU32(V, 0x1540, S); SU32(V, 0x234, S);
    {   /* the simulation knows whether the model is the UTV01 hierarchy (the inlined strcmp of the instance's +0x98) */
        rx_addr x = U32(V, 0x1328) + 0x98, y = 0x0093c5d4;   /* "UTV01_Hierarchy" */
        while (U8(x, 0) == U8(y, 0) && U8(x, 0)) { x++; y++; }
        SU32(S, 0x55f0, U8(x, 0) != U8(y, 0));
    }
    wv_push_simulation(V, B, 0x004c07b0, U32(V, 0x1540));
    veh_call_sp(B, 0x004c07cc, 1, V, 0, 0);
    rx_vehicle_utv_create_proxy(blk, V);
    static const rx_addr names[25] = {
        0x93bbf0, 0x93c5e4, 0x93c5fc, 0x93bd5c, 0x93bcd4, 0x93bd84, 0x93bd9c, 0x93c614, 0x93c62c, 0x93bd70, 0x93bce8, 0x93bde4, 0x93bdfc,
        0x93c644, 0x93c65c, 0x93c674, 0x93bcac, 0x93be44, 0x93be5c, 0x93c688, 0x93c6a0, 0x93c6b8, 0x93bcc0, 0x93bea4, 0x93bebc};
    wv_find f[25];
    for (uint32_t i = 0; i < 25; i++) { f[i].ret = 0x004c07df + 0x10 * i; f[i].off = (uint16_t)(0x1360 + 4 * i); f[i].name = names[i]; }
    wv_find_nodes(V, B, f, 25);
    static const rx_addr rets[4] = {0x004c09a1, 0x004c09b3, 0x004c09cb, 0x004c09dd};
    wv_finish(V, B, blk, 0x004c096d, 0x004c097c, rets, 3, 0x13c4);
    SU32(U32(V, 0x234), 0x44, 1);
    rx_set_sp(sp0);
}

/* a texture parameter of the vehicle's material block (V + 0x6c0: names 64 apart, +0x400 types, +0x440 values, +0x940 the
 * count): strncpy of the name (cdecl, the caller pops later) */
static void wv_texture_param(rx_addr blk, rx_addr sp_before, rx_addr ret, rx_addr name, rx_addr value) {
    rx_addr dst = blk + 64 * U8(blk, 0x940);
    veh_call_sp(sp_before, ret, 3, dst, name, 0x3f);
    crt_strncpy(dst, name, 0x3f);
    SU8(dst, 0x3f, 0);
    SU32(blk + 4 * U8(blk, 0x940), 0x440, value);
    SU32(blk + 4 * U8(blk, 0x940), 0x400, 4);
    SU8(blk, 0x940, (uint8_t)(U8(blk, 0x940) + 1));
}

/* RavenVehicleBuggy::vf28 004a2960 */
void rx_vehicle_buggy_vf28(rx_addr V, rx_addr blk) {
    const rx_addr sp0 = rx_sp(), B = sp0 - 0x10;
    rx_set_sp(B);
    SU32(V, 0x188, U32(blk, 0x14));
    veh_call_sp(B, 0x004a297d, 1, 0x8840, 0, 0);
    rx_addr S = rx_reflex_aligned_alloc(0x8840);
    if (S) { veh_call_sp(B, 0x004a298a, 1, S, 0, 0); S = rx_sim_buggy_construct(S); }
    rx_set_sp(B);
    SU32(V, 0x15c0, S); SU32(V, 0x234, S);
    wv_push_simulation(V, B, 0x004a29c4, S);
    veh_call_sp(B, 0x004a29d7, 1, V, 0, 0);
    rx_vehicle_buggy_create_proxy(blk, V);
    static const wv_find f[30] = {
        {0x004a29ea, 0x1360, 0x93bbf0}, {0x004a29fa, 0x1364, 0x93c564}, {0x004a2a0a, 0x1368, 0x93be44}, {0x004a2a1a, 0x136c, 0x93be5c},
        {0x004a2a2a, 0x1370, 0x93be74}, {0x004a2a3a, 0x1374, 0x93be8c}, {0x004a2a4a, 0x1378, 0x93bcac}, {0x004a2a5a, 0x137c, 0x93c578},
        {0x004a2a6a, 0x1380, 0x93bea4}, {0x004a2a7a, 0x1384, 0x93bebc}, {0x004a2a8a, 0x1388, 0x93bed4}, {0x004a2a9a, 0x138c, 0x93beec},
        {0x004a2aaa, 0x1390, 0x93bcc0}, {0x004a2aba, 0x1394, 0x93bd14}, {0x004a2aca, 0x1398, 0x93bcfc}, {0x004a2ada, 0x139c, 0x93bd5c},
        {0x004a2aea, 0x13a0, 0x93bcd4}, {0x004a2afa, 0x13a4, 0x93bd84}, {0x004a2b0a, 0x13a8, 0x93bd9c}, {0x004a2b1a, 0x13ac, 0x93bdb4},
        {0x004a2b2a, 0x13b0, 0x93bdcc}, {0x004a2b3a, 0x13b4, 0x93bd44}, {0x004a2b4a, 0x13b8, 0x93bd2c}, {0x004a2b5a, 0x13bc, 0x93bd70},
        {0x004a2b6a, 0x13c0, 0x93bce8}, {0x004a2b7a, 0x13c4, 0x93bde4}, {0x004a2b8a, 0x13c8, 0x93bdfc}, {0x004a2b9a, 0x13cc, 0x93be14},
        {0x004a2baa, 0x13d0, 0x93be2c}, {0x004a2bba, 0x13d4, 0x93c58c}};   /* the last: OBJ_Engine */
    wv_find_nodes(V, B, f, 30);
    veh_call_sp(B, 0x004a2bcb, 0, 0, 0, 0);
    rx_node_local_rotation(U32(V, 0x13d4), V + 0x1400);   /* 007dd380: the engine node's local rotation */
    rx_set_sp(B);
    static const rx_addr rets[4] = {0x004a2c09, 0x004a2c1b, 0x004a2c33, 0x004a2c45};
    wv_finish(V, B, blk, 0x004a2bd3, 0x004a2be2, rets, 5, 0x1410);
    SU32(U32(V, 0x234), 0x44, 1);
    /* the material's texture parameters "gTextureOffset" (V + 0x15a0) and "usr_fade_value" (V + 0x15b0); add esp, 0x18 after both */
    wv_texture_param(V + 0x6c0, B, 0x004a2c7f, 0x009363f4, V + 0x15a0);
    wv_texture_param(V + 0x6c0, B - 0xc, 0x004a2cc5, 0x0093c598, V + 0x15b0);
    rx_set_sp(sp0);
}

/* RavenVehicleTrophyTruck::vf28 0046a850 (the simulation takes the creation block's engine class) */
void rx_vehicle_truck_vf28(rx_addr V, rx_addr blk) {
    const rx_addr sp0 = rx_sp(), B = sp0 - 0x10;
    rx_set_sp(B);
    SU32(V, 0x188, U32(blk, 0x14));
    veh_call_sp(B, 0x0046a86d, 1, 0x8930, 0, 0);
    rx_addr S = rx_reflex_aligned_alloc(0x8930);
    if (S) { veh_call_sp(B, 0x0046a87e, 2, S, U32(blk, 0xc), 0); S = rx_sim_truck_construct(S, U32(blk, 0xc)); }
    rx_set_sp(B);
    SU32(V, 0x15d0, S); SU32(V, 0x234, S);
    wv_push_simulation(V, B, 0x0046a8b8, S);
    veh_call_sp(B, 0x0046a8cb, 1, V, 0, 0);
    rx_vehicle_truck_create_proxy(blk, V);
    static const wv_find f[36] = {
        {0x0046a8de, 0x1360, 0x93bbf0}, {0x0046a8f7, 0x1364, 0x93bc9c}, {0x0046a910, 0x1368, 0x93bcac}, {0x0046a929, 0x136c, 0x93bcc0},
        {0x0046a942, 0x1370, 0x93bcd4}, {0x0046a95b, 0x1374, 0x93bce8}, {0x0046a974, 0x1378, 0x93bcfc}, {0x0046a98d, 0x137c, 0x93bd14},
        {0x0046a9a6, 0x1380, 0x93bd2c}, {0x0046a9bf, 0x1384, 0x93bd44}, {0x0046a9d8, 0x13d8, 0x93bd5c}, {0x0046a9f1, 0x13dc, 0x93bd70},
        {0x0046aa0a, 0x1388, 0x93bd84}, {0x0046aa23, 0x138c, 0x93bd9c}, {0x0046aa3c, 0x1390, 0x93bdb4}, {0x0046aa55, 0x1394, 0x93bdcc},
        {0x0046aa6e, 0x1398, 0x93bde4}, {0x0046aa87, 0x139c, 0x93bdfc}, {0x0046aaa0, 0x13a0, 0x93be14}, {0x0046aab9, 0x13a4, 0x93be2c},
        {0x0046aad2, 0x13a8, 0x93be44}, {0x0046aaeb, 0x13ac, 0x93be5c}, {0x0046ab04, 0x13b0, 0x93be74}, {0x0046ab1d, 0x13b4, 0x93be8c},
        {0x0046ab36, 0x13b8, 0x93bea4}, {0x0046ab4f, 0x13bc, 0x93bebc}, {0x0046ab68, 0x13c0, 0x93bed4}, {0x0046ab81, 0x13c4, 0x93beec},
        {0x0046ab9a, 0x13c8, 0x93bf04}, {0x0046abb3, 0x13cc, 0x93bf1c}, {0x0046abcc, 0x13d0, 0x93bf34}, {0x0046abe5, 0x13d4, 0x93bf4c},
        {0x0046abfe, 0x13e0, 0x93bf64}, {0x0046ac17, 0x13e4, 0x93bf74}, {0x0046ac30, 0x13e8, 0x93bf84}, {0x0046ac49, 0x13ec, 0x93bf94}};
    wv_find_nodes(V, B, f, 36);
    static const rx_addr rets[4] = {0x0046ac8d, 0x0046ac9f, 0x0046acb7, 0x0046acc9};
    wv_finish(V, B, blk, 0x0046ac57, 0x0046ac66, rets, 4, 0x144c);
    SU32(U32(V, 0x234), 0x44, 1);
    rx_set_sp(sp0);
}

RX_FUNCTION(0x004c03a0, rx_vehicle_utv_construct);
RX_FUNCTION(0x004a2550, rx_vehicle_buggy_construct);
RX_FUNCTION(0x0046a3e0, rx_vehicle_truck_construct);
RX_FUNCTION(0x004c7020, rx_vehicle_utv_vf49);
RX_FUNCTION(0x004ac6c0, rx_vehicle_buggy_vf49);
RX_FUNCTION(0x004735f0, rx_vehicle_truck_vf49);
RX_FUNCTION(0x004c0c00, rx_vehicle_utv_create_proxy);
RX_FUNCTION(0x004a2f00, rx_vehicle_buggy_create_proxy);
RX_FUNCTION(0x0046aef0, rx_vehicle_truck_create_proxy);
RX_FUNCTION(0x004cb360, rx_sim_utv_params_load);
RX_FUNCTION(0x004b0b30, rx_sim_buggy_params_load);
RX_FUNCTION(0x0044f690, rx_sim_truck_params_load);
RX_FUNCTION(0x004cc030, rx_sim_utv_setup_masses);
RX_FUNCTION(0x004b1800, rx_sim_buggy_setup_masses);
RX_FUNCTION(0x00450480, rx_sim_truck_setup_masses);
RX_FUNCTION(0x004cba90, rx_sim_utv_construct);
RX_FUNCTION(0x004b1260, rx_sim_buggy_construct);
RX_FUNCTION(0x0044fe40, rx_sim_truck_construct);
RX_FUNCTION(0x004c0700, rx_vehicle_utv_vf28);
RX_FUNCTION(0x004a2960, rx_vehicle_buggy_vf28);
RX_FUNCTION(0x0046a850, rx_vehicle_truck_vf28);
