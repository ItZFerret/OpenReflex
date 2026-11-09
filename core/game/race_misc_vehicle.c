/* race_misc_vehicle.c -- RavenVehicleMotocross accessors used by the race / AI (vehicle +0x16d4 = its bike / physics object B)
 * and the behaviours' type ids outside 0x0051xxxx. (core/notes/race.md "Vehicle accessors") */
#include "race.h"
#include "race_extern.h"
#include "race_misc.h"
#include "phys_terrain.h"
#include "world_veh.h"
#include "world_rider.h"

/* 004669f0 RavenVehicleMotocross::vf47 GetEngineRpm (ECX = vehicle) -> st0: the engine speed B +0x8abc (rad/s) * 9.55 (rpm).
 * On the main thread's 53-bit x87 the float x float product is exact: returned as the exact double. */
double rx_moto_engine_rpm(rx_addr v) { return (double)F32(U32(v, 0x16d4), 0x8abc) * (double)9.55f /* 0x9774d0 */; }
RX_FUNCTION(0x004669f0, rx_moto_engine_rpm)
/* 00466a10 vf48 (ECX = vehicle) -> st0: B +0x5014. */
float rx_moto_vf48(rx_addr v) { return F32(U32(v, 0x16d4), 0x5014); }
RX_FUNCTION(0x00466a10, rx_moto_vf48)
/* 00466b30 vf49 (ECX = vehicle, arg; RET 4): B +0xb0 = arg. */
void rx_moto_vf49(rx_addr v, uint32_t value) { SU32(U32(v, 0x16d4), 0xb0, value); }
RX_FUNCTION(0x00466b30, rx_moto_vf49)
/* 00466b50 vf50 (ECX = vehicle) -> EAX: B +0x5c30's state +0x84 is 2. */
uint32_t rx_moto_vf50(rx_addr v) { return U32(U32(U32(v, 0x16d4), 0x5c30), 0x84) == 2; }
RX_FUNCTION(0x00466b50, rx_moto_vf50)
/* 00462290 vf46 (ECX = vehicle) -> EAX: either of the vehicle's +0x1610 / +0x160c flags. */
uint32_t rx_moto_vf46(rx_addr v) { return U32(v, 0x1610) != 0 || U32(v, 0x160c) != 0; }
RX_FUNCTION(0x00462290, rx_moto_vf46)
/* 00466b00 vf45 (ECX = vehicle) -> EAX: both B +0x8890 and B +0x89e0 set. */
uint32_t rx_moto_vf45(rx_addr v) { rx_addr b = U32(v, 0x16d4); return U32(b, 0x8890) != 0 && U32(b, 0x89e0) != 0; }
RX_FUNCTION(0x00466b00, rx_moto_vf45)

uint32_t rx_behavior_fix_progress_type(rx_addr self) { (void)self; return 0xc; }
RX_FUNCTION(0x007f1110, rx_behavior_fix_progress_type)
uint32_t rx_behavior_perform_stunt_type(rx_addr self) { (void)self; return 0xf; }
RX_FUNCTION(0x0089e360, rx_behavior_perform_stunt_type)
uint32_t rx_behavior_air_leveling_type(rx_addr self) { (void)self; return 9; }
RX_FUNCTION(0x008a5dc0, rx_behavior_air_leveling_type)
uint32_t rx_behavior_seperate_type(rx_addr self) { (void)self; return 6; }
RX_FUNCTION(0x008ac4a0, rx_behavior_seperate_type)

/* ---- the wheel state reports (B = the bike: +0x88b0 / +0x88c0 / +0x88d0 / +0x8800 / +0x8810 .. the rear wheel, +0x8a00 ..
 * +0x8a20 / +0x8950 / +0x8960 .. the front wheel; +0x360 / +0x2c0 the wheels' hub positions) ---- */
static void copy16(rx_addr dst, rx_addr src) { rx_st4(dst, rx_ld4(src)); }
static void add16(rx_addr dst, rx_addr a, rx_addr b) { rx_st4(dst, rx_v4_add(rx_ld4(a), rx_ld4(b))); }
static void scale16(rx_addr dst, float s, rx_addr v) { rx_st4(dst, rx_v4_mul(rx_v4_splat(s), rx_ld4(v))); }
static void vehicle_vec(rx_addr v, uint32_t slot, rx_addr out) {          /* vf36 / vf37: a 16-byte vector out */
    typedef void (*fn_t)(rx_addr, rx_addr);
    rx_addr fn = race_vfn(v, slot);
    fn_t f = RX_FN(fn, fn_t);
    if (f) f(v, out); else race_thiscall(fn, v, 1, out, 0, 0, 0);
}
static uint32_t object_u32(rx_addr o, uint32_t slot) {
    typedef uint32_t (*fn_t)(rx_addr);
    rx_addr fn = race_vfn(o, slot);
    fn_t f = RX_FN(fn, fn_t);
    return f ? f(o) : race_thiscall(fn, o, 0, 0, 0, 0, 0);
}
/* A wheel's contact: the contact object (B +0x8784 rear / +0x8780 front) is a terrain one (vf22 == 5). */
static int terrain_contact(rx_addr c) { return c != 0 && object_u32(c, 22) == 5; }

/* 00466de0 RavenVehicleMotocross::vf51 (ECX = vehicle, arg = out; RET 4): the wheel report: the velocity (vf37) and angular
 * velocity (vf36) at +0x1f0 / +0x1e0 (+0x200 = 2 wheels), the position +0x1d0, then per wheel (rear at +0x00, front at +0x70):
 * its frame rows, hub + offset, the suspension axis scaled by its compression (x) / travel (z), compression, load, ground contact
 * (+0x68: the wheel's ground flag, or a terrain contact) and the surface (+0x6c). */
void rx_moto_wheel_report(rx_addr v, rx_addr out) {
    RACE_FRAME16_ENTER(F, 0x20);
    vehicle_vec(v, 37, F);
    copy16(out + 0x1f0, F);
    SU32(out, 0x200, 2);
    SF32(out, 0x1d0, F32(v, 0xc0)); SF32(out, 0x1d4, F32(v, 0xc4)); SF32(out, 0x1d8, F32(v, 0xc8));
    vehicle_vec(v, 36, F + 0x10);
    copy16(out + 0x1e0, F + 0x10);
    RACE_FRAME16_LEAVE(F);
    rx_addr b = U32(v, 0x16d4);
    copy16(out, b + 0x88b0);
    copy16(out + 0x10, U32(v, 0x16d4) + 0x88d0);
    copy16(out + 0x20, U32(v, 0x16d4) + 0x88c0);
    b = U32(v, 0x16d4);
    add16(out + 0x30, b + 0x8800, b + 0x360);
    b = U32(v, 0x16d4); scale16(out + 0x40, F32(b, 0x8810), b + 0x88d0);
    b = U32(v, 0x16d4); scale16(out + 0x50, F32(b, 0x8818), b + 0x88b0);
    SF32(out, 0x60, F32(U32(v, 0x16d4), 0x8870));
    SF32(out, 0x64, F32(U32(v, 0x16d4), 0x8a34));
    SU32(out, 0x68, U32(U32(v, 0x16d4), 0x8890) == 0);
    SU32(out, 0x6c, U32(v, 0x1610));
    if (terrain_contact(U32(U32(v, 0x16d4), 0x8784))) SU32(out, 0x68, 1);
    rx_addr o = out + 0x70;
    copy16(o, U32(v, 0x16d4) + 0x8a00);
    copy16(o + 0x10, U32(v, 0x16d4) + 0x8a20);
    copy16(o + 0x20, U32(v, 0x16d4) + 0x8a10);
    b = U32(v, 0x16d4);
    add16(o + 0x30, b + 0x8950, b + 0x2c0);
    b = U32(v, 0x16d4); scale16(o + 0x40, F32(b, 0x8960), b + 0x8a20);
    b = U32(v, 0x16d4); scale16(o + 0x50, F32(b, 0x8968), b + 0x8a00);
    SF32(o, 0x60, F32(U32(v, 0x16d4), 0x89c0));
    SF32(o, 0x64, F32(U32(v, 0x16d4), 0x8a30));
    SU32(o, 0x68, U32(U32(v, 0x16d4), 0x89e0) == 0);
    SU32(o, 0x6c, U32(v, 0x160c));
    if (terrain_contact(U32(U32(v, 0x16d4), 0x8780))) SU32(o, 0x68, 1);
}
RX_FUNCTION(0x00466de0, rx_moto_wheel_report)

/* Vehicle x87 accessors (vf47 engine rpm, vf48) as float (the original stores them with fstp). */
static float vehicle_x87(rx_addr v, uint32_t slot) {
    rx_addr fn = race_vfn(v, slot);
    if (fn == 0x004669f0) return (float)rx_moto_engine_rpm(v);
    if (fn == 0x00466a10) return rx_moto_vf48(v);
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[1] = v;
    rx_call_original_ex(fn, &in, &o, 0, 0);
    return o.st0;
}

/* 00467070 RavenVehicleMotocross::vf52 (ECX = vehicle, arg = out; RET 4): the vehicle's state for the sound / effects / AI: per
 * wheel (rear +0x00, front +0x70): hub + offset, frame row, scaled suspension axes, compression, load, airborne flag, surface,
 * material, slip, the terrain contact (id +0x158 and object, else -1 / 0); then the speed (|vf37|) +0x220, engine rpm (vf47) +0x224,
 * vf48 +0x228, the throttle / brake pair (B +0x5c30) +0x22c, gear (vf42) +0x234, vf50 +0x244, wheelie flags, both wheels
 * down +0x248, the controls (+0x1310 / +0x1314 / +0x131c) and the player's pad state (+0x1a0 +0xaf00). */
void rx_moto_state_report(rx_addr v, rx_addr out) {
    rx_addr b = U32(v, 0x16d4);
    add16(out, b + 0x8800, b + 0x360);
    copy16(out + 0x10, U32(v, 0x16d4) + 0x87d0);
    b = U32(v, 0x16d4); scale16(out + 0x20, F32(b, 0x8810), b + 0x88d0);
    b = U32(v, 0x16d4); scale16(out + 0x30, F32(b, 0x8818), b + 0x88b0);
    SF32(out, 0x40, F32(U32(v, 0x16d4), 0x8870));
    SF32(out, 0x44, F32(U32(v, 0x16d4), 0x8a34));
    SU32(out, 0x48, U32(U32(v, 0x16d4), 0x8894) == 0);
    SU32(out, 0x50, U32(v, 0x1610));
    SU32(out, 0x54, U32(v, 0x1618));
    SF32(out, 0x58, F32(v, 0x1620));
    rx_addr c = U32(U32(v, 0x16d4), 0x8784);
    if (terrain_contact(c)) { c = U32(U32(v, 0x16d4), 0x8784); SU32(out, 0x60, U32(c, 0x158)); SU32(out, 0x64, c); }
    else { SU32(out, 0x60, 0xffffffffu); SU32(out, 0x64, 0); }
    b = U32(v, 0x16d4);
    add16(out + 0x70, b + 0x8950, b + 0x2c0);
    copy16(out + 0x80, U32(v, 0x16d4) + 0x8920);
    b = U32(v, 0x16d4); scale16(out + 0x90, F32(b, 0x8960), b + 0x8a20);
    b = U32(v, 0x16d4); scale16(out + 0xa0, F32(b, 0x8968), b + 0x8a00);
    SF32(out, 0xb0, F32(U32(v, 0x16d4), 0x89c0));
    SF32(out, 0xb4, F32(U32(v, 0x16d4), 0x8a30));
    SU32(out, 0xb8, U32(U32(v, 0x16d4), 0x89e4) == 0);
    SU32(out, 0xc0, U32(v, 0x160c));
    SU32(out, 0xc4, U32(v, 0x1614));
    SF32(out, 0xc8, F32(v, 0x161c));
    c = U32(U32(v, 0x16d4), 0x8780);
    if (terrain_contact(c)) { c = U32(U32(v, 0x16d4), 0x8780); SU32(out, 0xd0, U32(c, 0x158)); SU32(out, 0xd4, c); }
    else { SU32(out, 0xd0, 0xffffffffu); SU32(out, 0xd4, 0); }
    RACE_FRAME16_ENTER(F, 0x10);
    vehicle_vec(v, 37, F);
    const float x = F32(F, 0), y = F32(F, 4), z = F32(F, 8);
    RACE_FRAME16_LEAVE(F);
    SF32(out, 0x220, sqrtf((x * x + z * z) + y * y));   /* the shuffle order: (x^2 + z^2) + y^2 */
    SF32(out, 0x224, vehicle_x87(v, 47));
    SF32(out, 0x228, vehicle_x87(v, 48));
    SF32(out, 0x22c, F32(U32(U32(v, 0x16d4), 0x5c30), 0));
    SF32(out, 0x230, F32(U32(U32(v, 0x16d4), 0x5c30), 4));
    SU32(out, 0x234, object_u32(v, 42));
    SU32(out, 0x244, object_u32(v, 50));
    SU32(out, 0x250, -0.5f /* 0x9774b0 */ > F32(U32(v, 0x16d4), 0x8adc) ? 1 : 0);
    SF32(out, 0x240, F32(U32(v, 0x16d4), 0x8ad8));
    b = U32(v, 0x16d4);
    SU32(out, 0x248, (U32(b, 0x89e4) == 0 && U32(b, 0x8894) == 0) ? 1 : 0);
    SU32(out, 0x258, U32(v, 0x1310));
    SU32(out, 0x25c, U32(v, 0x1314));
    SF32(out, 0x254, F32(v, 0x131c));
    rx_addr pad = U32(v, 0x1a0);
    SU32(out, 0x260, pad != 0 ? (U32(pad, 0xaf00) != 0) : 0);
}
RX_FUNCTION(0x00467070, rx_moto_state_report)

/* 00466340 RavenVehicleMotocross::vf34 (ECX = vehicle, args: position out, velocity out, speed threshold, unused; RET 0x10) ->
 * EAX: a safe point to remember for a reset: only on the ground (vf45 and not vf46), not crashed (+0x280), the rider attached
 * (+0x234 +0xc == 0), no wheel / body contact flags (bike +0x1af0 / +0x1b54, +0x1318 entries), not on terrain material 6
 * (TerrainMaterialAt EAX = 0, EDX = the position +0xc0; the terrain FUN_00402dc0 EDX = 0x9e30d4, its static 0x9e30dc behind
 * 0x9e30e0), inside the terrain's bounds (+0x44d8 .. +0x44e4 by the margin +0x44f0 in x / z), and both wheel speeds (bike
 * +0x8a14, +0x88c4) above the threshold: the bike's position (+0x180) and velocity (+0x7c0), 16 bytes each. Frame: and esp, -16;
 * sub 0x24; 3 pushes. */
uint32_t rx_moto_safe_point(rx_addr v, rx_addr pos, rx_addr vel, float thr) {
    RACE_FRAME_EBP(F, 16, 0x24, 3);
    uint32_t r = 0;
    if (race_thiscall(race_vfn(v, 45), v, 0, 0, 0, 0, 0) == 0) goto done;
    if (race_thiscall(race_vfn(v, 46), v, 0, 0, 0, 0, 0) != 0) goto done;
    if (U32(v, 0x280) != 0 || U32(U32(v, 0x234), 0xc) != 0) goto done;
    {
        const rx_addr b = U32(v, 0x16d4);
        uint32_t any = 0;
        for (int32_t i = 0, n = I32(b, 0x1318); i < n; i++) any |= U32(b, 0x1af0 + 0x64 + 4 * i) | U32(b, 0x1af0 + 4 * i);
        if (I32(b, 0x1318) > 0 && any != 0) goto done;
    }
    SU32(F, 0xc, 0);
    if ((U8(0, 0x9e30e0) & 1) == 0) { SU32(0, 0x9e30e0, U32(0, 0x9e30e0) | 1); SU32(0, 0x9e30dc, 0); }
    {
        rx_addr t;
        uint32_t material;
        RACE_CALL_SP(0, t = rx_world_find_by_controller(0x009e30d4));
        RACE_CALL_SP(0, material = phys_terrain_material_at(t, v + 0xc0, 0, NULL, NULL));   /* TerrainMaterialAt */
        if (material == 6) goto done;
        const float m = F32(t, 0x44f0);
        const float x = F32(v, 0xc0), z = F32(v, 0xc8);
        if (F32(t, 0x44d8) > x + m || x - m > F32(t, 0x44e0)) goto done;
        if (F32(t, 0x44dc) > z + m || z - m > F32(t, 0x44e4)) goto done;
        const rx_addr b = U32(v, 0x16d4);
        for (uint32_t i = 0; i < 16; i += 4) { SU32(F + 0x10, i, U32(b, 0x8a10 + i)); SU32(F + 0x20, i, U32(b, 0x88c0 + i)); }
        if (F32(F, 0x14) > thr && F32(F, 0x24) > thr) {
            for (uint32_t i = 0; i < 16; i += 4) SU32(pos, i, U32(b, 0x180 + i));
            const rx_addr b2 = U32(v, 0x16d4);
            for (uint32_t i = 0; i < 16; i += 4) SU32(vel, i, U32(b2, 0x7c0 + i));
            r = 1;
        }
    }
done:
    RACE_FRAME_END(F);
    return r;
}
RX_FUNCTION(0x00466340, rx_moto_safe_point)

/* 004664f0 RavenVehicleMotocross::vf33 (ECX = vehicle): the reset of the vehicle's ground state: its four tyre-track strips
 * (+0x1578 .. +0x1584: a handle whose bit 0x1000 / 0x2000 / 0x4000 names the track pool 0xa95774 / 0xa95778 / 0xa9577c) are cut
 * (TerrainTracks_CutStrip ESI = the pool, EAX = the handle without the bit), the contact / slide state cleared (+0x1588 .. +0x15a8,
 * +0x160c .. +0x1620, +0x1310 .. +0x131c, +0x1630 .. +0x163c), the bike's +0x8a40 / +0x8a50 set to the .data vector 0xd61670 and
 * +0x1320 set. (The terrain static 0x9e30dc behind 0x9e30e0 first.) */
void rx_moto_reset_ground(rx_addr v) {
    RACE_FRAME_EBP(F, 16, 4, 3);
    if ((U8(0, 0x9e30e0) & 1) == 0) { SU32(0, 0x9e30e0, U32(0, 0x9e30e0) | 1); SU32(0, 0x9e30dc, 0); }
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t h = U32(v, 0x1578 + 4 * i);
        rx_addr pool; uint32_t idx;
        if (h & 0x1000) { pool = U32(0, 0xa95774); idx = h ^ 0x1000; }
        else if (h & 0x2000) { pool = U32(0, 0xa95778); idx = h ^ 0x2000; }
        else { pool = U32(0, 0xa9577c); idx = h ^ 0x4000; }
        RACE_CALL_SP(0, phys_tracks_cut_strip(pool, (int32_t)idx));            /* TerrainTracks_CutStrip */
    }
    for (uint32_t o = 0x1588; o <= 0x15a8; o += 4) SU32(v, o, 0);
    for (uint32_t o = 0x160c; o <= 0x1618; o += 4) SU32(v, o, 0);
    SU32(v, 0x1310, 0); SU32(v, 0x1314, 0);
    SF32(v, 0x161c, 0.0f); SF32(v, 0x1620, 0.0f); SF32(v, 0x1318, 0.0f); SF32(v, 0x131c, 0.0f);
    for (uint32_t i = 0; i < 16; i += 4) SU32(U32(v, 0x16d4), 0x8a50 + i, U32(0, 0xd61670 + i));
    for (uint32_t i = 0; i < 16; i += 4) SU32(U32(v, 0x16d4), 0x8a40 + i, U32(0, 0xd61670 + i));
    for (uint32_t o = 0x1630; o <= 0x163c; o += 4) SU32(v, o, 0);
    SU32(v, 0x1320, 1);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x004664f0, rx_moto_reset_ground)

/* 00462130 (stdcall vehicle; RET 4) -> AL: is the vehicle's spot clear of the other local players' vehicles (the game's player
 * slots: FUN_00552b90 (EDX = the players; slot, 3), not the vehicle's own player +0x190): for each other vehicle with its bounding
 * sphere (the +0x94 part's vf6 (centre, extent, radius)) and position (vf17), clear unless (r * 2 + r') * r' + r * r exceeds the
 * squared distance. Only AL is the result; the upper bytes of EAX are what the last call left (kept for the callers' sake, as
 * the original). Frame: and esp, -16; sub 0x74; 3 pushes (the own radius at +0x18, position at +0x20, the other's radius / position
 * at +0x1c / +0x30, the sphere outputs at +0x40 / +0x50 / +0x60 / +0x70). */
uint32_t rx_spawn_point_clear(rx_addr v, uint32_t eax_in) {
    RACE_FRAME_EBP(F, 16, 0x74, 3);
    uint32_t eax = eax_in, r = 1;
    const rx_addr players = U32(0, 0xa946b8) + 0x7460;
    if (players != 0) {
        const int32_t n = U8(players, 0xdfbc);
        SU32(F, 0x14, (uint32_t)n);
        rx_addr part = v + 0x94;
        race_thiscall(race_vfn(part, 6), part, 3, F + 0x50, F + 0x40, F + 0x18, 0);
        eax = race_thiscall(race_vfn(v, 17), v, 1, F + 0x20, 0, 0, 0);
        for (int32_t i = 0; i < n; i++) {
            rx_addr p;
            RACE_CALL_SP(8, p = rx_world_players_find(players, (uint32_t)i, 3));
            eax = p;
            if (p == 0 || p == U32(v, 0x190)) continue;
            const rx_addr w = U32(p, 0x1ac);
            if (w == 0) continue;
            part = w + 0x94;
            race_thiscall(race_vfn(part, 6), part, 3, F + 0x70, F + 0x60, F + 0x1c, 0);
            eax = race_thiscall(race_vfn(w, 17), w, 1, F + 0x30, 0, 0, 0);
            const float dz = F32(F, 0x28) - F32(F, 0x38), dx = F32(F, 0x20) - F32(F, 0x30), dy = F32(F, 0x24) - F32(F, 0x34);
            const float d2 = dz * dz + dx * dx + dy * dy;
            const float r1 = F32(F, 0x18), r2 = F32(F, 0x1c);
            if ((r1 * 2.0f + r2) * r2 + r1 * r1 > d2) { r = 0; break; }
        }
    }
    RACE_FRAME_END(F);
    return (eax & 0xffffff00u) | r;
}
RX_FUNCTION(0x00462130, rx_spawn_point_clear)
