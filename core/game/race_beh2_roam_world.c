/* race_beh2_roam_world.c -- BehaviorRoamWorld (the Free Ride AI's roaming): the update vf4 0052fcf0 and its helpers (2026-10-26,
 * agent w-stunts). Its lifecycle (init / reset / pick an area) is race_beh2_perform_init.c.
 *
 * The AI rider roams a Free Ride world: it picks an activity area (B +0x10, whose vf15 gives the area's matrix; the area's type
 * name is at area + 0x108), rides to its centre (+0x20 / +0x24 / +0x28), follows the track nodes of the areas that suit its
 * vehicle (the target area +0x30 and node +0x34), and at the area asks the Player List to start the activity (004dde70).
 * Layout: +0x04 enabled, +0x08 the driver D (D +0x10 the world's track: areas +0x130, count +0x1d8; D +0x60 the throttle
 * demand, D +0x130 the clock), +0x0c the state (0 approach, 1 follow, 2 approach (second), 3 follow (second), 4 look for a
 * target, 5 / 6 / 7 hold off to the side by probing terrain), +0x30 / +0x34 the target area / node, +0x38 + 4 * k the candidate
 * nodes, +0x60 the candidate count, +0x64 the next re-pick time, +0x68 / +0x6c the settle interval / time, +0x70 / +0xb0 /
 * +0xf0 three terrain-rise probes (0x40 bytes each: +0x08 steps, +0x14 / +0x18 flags, +0x30 position) with their ranged
 * parameter blocks, +0x84 / +0x88 / +0xc4 / +0xc8 / +0x104 / +0x108 their switches.
 *
 * Every function is a hand transliteration at the original's stack frame (address in the comment); ESI / EAX / EDI register
 * arguments are explicit parameters. The vehicle V is B -> driver -> player (+0x74) -> vehicle (+0x1ac): position +0xc0, velocity
 * +0x130. x87: the sqrt arguments pass through fld / fstp (a signalling NaN comes out quiet). */
#include "race_beh2_extern.h"
#include "race_beh1_extern.h"
#include "phys_crt.h"
#include "race_misc.h"
#include "build_activity_util.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "build_activity_freeride_run.h"
#include "build_activity.h"

void rx_waypoint_probe_reset(rx_addr probe, rx_addr origin);        /* 005166e0 (EAX = probe, ECX = origin) */
void rx_waypoint_probe_advance(rx_addr probe, rx_addr direction);  /* 00516590 (ESI = probe; stack direction) */

#define RW_DRIVER(b)   U32((b), 8)
#define RW_PLAYER(b)   U32(RW_DRIVER(b), 0x74)
#define RW_VEHICLE(b)  U32(RW_PLAYER(b), 0x1ac)
#define RW_TRACK(b)    U32(RW_DRIVER(b), 0x10)
#define RW_CLOCK(b)    F32(RW_DRIVER(b), 0x130)

/* a float through the x87 (fld m32 / fstp m32) */
static float rw_x87(float v) {
    uint32_t b; memcpy(&b, &v, 4);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) b |= 0x00400000u;
    memcpy(&v, &b, 4);
    return v;
}
/* SqrtF 0042c230 (cdecl float through the x87 argument slot) at the original's depth */
static float rw_sqrtf(float x) {
    float r;
    RACE_CORE_CALL(1, r = phys_sqrtf(rw_x87(x)));
    return r;
}
/* a thiscall through a vtable byte offset with one pointer argument (vf15 = 0x3c: the object's 4x4 matrix into the buffer) */
static void rw_vcall_out(rx_addr obj, uint32_t byte_off, rx_addr out) {
    const rx_addr fn = race_vfn(obj, byte_off / 4);
    void (*f)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
    if (f) { RACE_CORE_CALL(1, f(obj, out)); } else race_thiscall(fn, obj, 1, out, 0, 0, 0);
}
/* the pair (a, b) steering terms: 1 / sqrt scale of a 2D vector when its squared length is not zero (ucomiss: NaN counts as not zero) */
static int rw_nonzero(float len2) { return !(len2 == 0.0f); }

/* 0052efa0 (EAX = the behaviour, ESI = an area) -> EAX: does the area carry the flag bit that goes with the behaviour's target
 * activity? The activity type of the area name (B +0x10, name at +0x108; 00508920) selects the bit of the area's +0x118:
 * types 0 .. 7 -> 0x800, 0x200, 0x400, 0x20, 0x40, 0x80, 0x100, 0x1000; 17 .. 19 -> 0x10000, 0x20000, 0x8000; others none. */
uint32_t rx_roam_area_matches(rx_addr beh, rx_addr area) {
    static const uint32_t mask[20] = {0x800, 0x200, 0x400, 0x20, 0x40, 0x80, 0x100, 0x1000, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      0x10000, 0x20000, 0x8000};
    uint32_t type;
    RACE_CORE_CALL(1, type = rx_roam_activity_type(U32(beh, 0x10) + 0x108));
    if (type > 0x13) return 0;
    return (U32(area, 0x118) & mask[type]) != 0;
}
RX_FUNCTION(0x0052efa0, rx_roam_area_matches)

/* 00530dc0 (ECX = the activity type, EAX = the vehicle class (+0x194); stack kind (+0x198); RET 4) -> EAX: may this vehicle class do
 * the activity? Per type a rule: 0: classes 0 / 1; 1: any; 2: classes 0 / 1 / 3; 3: class 0 with kind 0, or class 7; 4 / 6: none; 5: any.
 * Types above 0x14 are never asked. */
uint32_t rx_roam_class_allowed(uint32_t type, uint32_t klass, uint32_t kind) {
    static const uint8_t rule[21] = {0, 0, 1, 2, 3, 0, 4, 0, 5, 6, 6, 6, 6, 6, 6, 6, 6, 1, 1, 1, 0};
    switch (rule[type]) {
    case 0: return klass == 0 || klass == 1;
    case 1: return 1;
    case 2: return klass == 0 || klass == 1 || klass == 3;
    case 3: return klass == 0 ? kind == 0 : klass == 7;
    case 5: return 1;
    default: return 0;
    }
}
RX_FUNCTION(0x00530dc0, rx_roam_class_allowed)

/* 00530590 (EDI = out (area, node, distance^2); stack the behaviour; RET 4) -> EAX = the area: the nearest node, over the
 * track's areas that suit the vehicle (flag 1 of area +0x18 + 4 * (kind + 4 * class)), match the target activity (0052efa0) and carry
 * the 0x2000 flag, walking each area's nodes (+0x64 next) while the area's length budget (+0x120, less node +0x4f0 per node) lasts:
 * a node nearer than the best so far whose segment to the next node passes within 1 unit (00538a60 parameter below 1.0) wins.
 * Frame: push ebp; and esp, -16; sub esp, 0x28; push ebx / esi. */
rx_addr rx_roam_nearest_area(rx_addr beh, rx_addr out) {
    RACE_FRAME_EBP(F, 16, 0x28, 2);
    SF32(out, 8, rx_bf(0x7f7fffffu) /* 0x977494 FLT_MAX */);
    SU32(out, 4, 0);
    SU32(out, 0, 0);
    int32_t index = 0;
    if (I32(RW_TRACK(beh), 0x1d8) <= 0) { RACE_FRAME_END(F); return 0; }
    do {
        const rx_addr vehicle0 = RW_VEHICLE(beh);
        const rx_addr area = U32(U32(RW_TRACK(beh), 0x130), 4u * (uint32_t)index);
        const uint32_t k = U32(vehicle0, 0x198) + 4u * U32(vehicle0, 0x194);
        if ((U8(area, 0x18 + 4 * k) & 1) != 0 && rx_roam_area_matches(beh, area) && (U32(area, 0x118) & 0x2000) != 0) {
            rx_addr node = U32(area, 0);
            float budget = F32(area, 0x120);
            while (node != 0) {
                const rx_addr next = U32(node, 0x64);
                if (next == 0 || !(budget > 0.0f /* 0x92e12c */)) break;
                const rx_addr vehicle = RW_VEHICLE(beh);
                const float dx = F32(node, 0) - F32(vehicle, 0xc0);
                const float dy = F32(node, 4) - F32(vehicle, 0xc4);
                const float dz = F32(node, 8) - F32(vehicle, 0xc8);
                const float dist2 = (dy * dy + dz * dz) + dx * dx;
                if (F32(out, 8) > dist2) {
                    float t;
                    SU32(F, -4, F + 0x20);
                    RACE_CORE_CALL(1, t = rx_waypoint_line_project(next, node, vehicle + 0xc0, F + 0x20));
                    if (1.0f /* 0x931f24 */ > t) {
                        SU32(out, 0, area);
                        SU32(out, 4, node);
                        SF32(out, 8, dist2);
                    }
                }
                budget = budget - F32(node, 0x4f0);
                node = next;
            }
        }
        index++;
    } while (index < I32(RW_TRACK(beh), 0x1d8));
    RACE_FRAME_END(F);
    return U32(out, 0);
}
RX_FUNCTION(0x00530590, rx_roam_nearest_area)

/* 0052f0e0 (stack behaviour, out (area, node, distance^2); RET 8): choose the area to roam to. Over the track's areas that suit
 * the vehicle (area +0x18 + 4 * (kind + 4 * class) bit 0, the target activity's flag 0052efa0), walking each area's nodes (+0x64) while
 * the area's length budget (+0x120, less node +0x4f0 each) lasts: the nearest node within 200 units (40000 = 0x9cd9e0 squared) that the
 * vehicle is heading toward (node direction +0x50 against the velocity > 0), whose segment projects ahead of the vehicle (00538a60
 * below 1.0) and is level with it (|(z + x) * 0 + y| of the unit offset below 0.3), is kept as the area's candidate (the node into
 * B +0x38 + 4 * n, the area, its distance); up to ten areas. The candidates (count into B +0x60, the next re-pick time B +0x64 = the
 * clock + 25) are offered to the section chooser (0050a360); the chosen area, its node and distance go to out. Frame: push ebp; and
 * esp, -16; sub esp, 0xa4; push ebx / esi / edi (the candidate distances at +0x48, areas at +0x88). */
void rx_roam_choose_area(rx_addr beh, rx_addr out) {
    RACE_FRAME_EBP(F, 16, 0xa4, 3);
    const rx_addr cand_dist = F + 0x48, cand_area = F + 0x88, tmp = F + 0x70;
    const float flt_max = rx_bf(0x7f7fffffu);                        /* 0x977494 */
    for (uint32_t o = 0; o < 0x28; o += 4) SU32(F, 0x88 + o, 0);
    SU32(out, 8, rx_fb(flt_max));
    SU32(out, 4, 0);
    SU32(out, 0, 0);
    for (uint32_t o = 0; o < 0x28; o += 4) SF32(cand_dist, o, flt_max);
    uint32_t found = 0;                                               /* [esp+0x14] */
    int32_t index = 0;                                                /* [esp+0x24] */
    if (I32(RW_TRACK(beh), 0x1d8) > 0) {
        uint32_t slot = 0;                                            /* ebx: byte offset of the candidate slot */
        do {
            const rx_addr area = U32(U32(RW_TRACK(beh), 0x130), 4u * (uint32_t)index);
            const rx_addr vehicle0 = RW_VEHICLE(beh);
            const uint32_t k = U32(vehicle0, 0x198) + 4u * U32(vehicle0, 0x194);
            if ((U8(area, 0x18 + 4 * k) & 1) != 0 && rx_roam_area_matches(beh, area)) {
                rx_addr node = U32(area, 0);
                float budget = F32(area, 0x120);
                while (node != 0) {
                    const rx_addr next = U32(node, 0x64);
                    if (next == 0 || !(budget > 0.0f)) break;
                    const rx_addr vehicle = RW_VEHICLE(beh);
                    const float dx = F32(node, 0) - F32(vehicle, 0xc0);
                    const float dy = F32(node, 4) - F32(vehicle, 0xc4);
                    const float dz = F32(node, 8) - F32(vehicle, 0xc8);
                    const float dist2 = (dy * dy + dz * dz) + dx * dx;
                    if (F32(cand_dist, slot) > dist2 && 40000.0f /* 0x9cd9e0 */ > dist2) {
                        const float heading = (F32(node, 0x58) * F32(vehicle, 0x138) + F32(node, 0x54) * F32(vehicle, 0x134)) +
                                              F32(vehicle, 0x130) * F32(node, 0x50);
                        if (heading > 0.0f) {
                            float t;
                            SU32(F, -4, tmp);
                            RACE_CORE_CALL(1, t = rx_waypoint_line_project(next, node, vehicle + 0xc0, tmp));
                            if (1.0f > t) {
                                float ox = F32(tmp, 0) - F32(vehicle, 0xc0);
                                float oy = F32(tmp, 4) - F32(vehicle, 0xc4);
                                float oz = F32(tmp, 8) - F32(vehicle, 0xc8);
                                float len2 = (oz * oz + oy * oy) + ox * ox;
                                if (len2 > 0.0f) {
                                    const float inv = 1.0f / rw_sqrtf(len2);
                                    ox = inv * ox;
                                    oy = oy * inv;
                                    oz = oz * inv;
                                }
                                const float level = fabsf(((oz + ox) * 0.0f) + oy);
                                if (0.3f /* 0x931da4 */ > level) {
                                    SU32(cand_area, slot, area);
                                    SU32(beh, 0x38 + slot, node);
                                    SF32(cand_dist, slot, dist2);
                                }
                            }
                        }
                    }
                    budget = budget - F32(node, 0x4f0);
                    node = next;
                }
            }
            /* a candidate was kept for this slot: the next slot */
            if (flt_max > F32(cand_dist, slot)) {
                found++;
                slot += 4;
            }
            if (slot == 0x28) break;
            index++;
        } while (index < I32(RW_TRACK(beh), 0x1d8));
    }
    if (found != 0) {
        const rx_addr driver = RW_DRIVER(beh);
        SU32(beh, 0x60, found);
        rx_addr chosen;
        rx_addr slot_out = 0;
        (void)slot_out;
        RACE_CORE_CALL(4, chosen = rx_race_choose_section(driver, cand_area, found, 0, &slot_out));
        SF32(beh, 0x64, F32(driver, 0x130) + 25.0f /* 0x92e134 */);
        if (chosen != 0) {
            uint32_t i = 0;
            while (i < found && U32(cand_area, 4 * i) != chosen) i++;
            SF32(out, 8, rw_x87(F32(cand_dist, 4 * i)));
            SU32(out, 4, U32(beh, 0x38 + 4 * i));
            SU32(out, 0, chosen);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0052f0e0, rx_roam_choose_area)

/* 0052fca0 (ESI = the behaviour) -> EAX: pick the roaming target: choose an area (0052f0e0 into a frame triple); when one was found
 * it becomes the target area (+0x30) and node (+0x34), the settle time (+0x6c) = the clock + the interval (+0x68), the state 0. */
uint32_t rx_roam_pick_target(rx_addr beh) {
    RACE_FRAME_PLAIN(F, 0xc, 0);
    SU32(F, -8, beh);
    SU32(F, -4, F);
    RACE_CORE_CALL(2, rx_roam_choose_area(beh, F));
    if (U32(F, 0) == 0) { RACE_FRAME_END(F); return 0; }
    SU32(beh, 0x30, U32(F, 0));
    SU32(beh, 0x34, U32(F, 4));
    SF32(beh, 0x6c, RW_CLOCK(beh) + F32(beh, 0x68));
    SU32(beh, 0xc, 0);
    RACE_FRAME_END(F);
    return 1;
}
RX_FUNCTION(0x0052fca0, rx_roam_pick_target)

/* 0052f4a0 (ECX = the behaviour) -> EAX: is the target still good? The target node is the area's first node (area +0x00) -> yes;
 * else the vehicle's position projected onto the segment from the node's previous (+0x60) to the node (00538a60): the projection
 * must not be more than 15 units (0x9322d0) above the vehicle. Frame: push ebp; and esp, -16; sub esp, 0x18; push esi / edi. */
uint32_t rx_roam_target_ok(rx_addr beh) {
    RACE_FRAME_EBP(F, 16, 0x18, 2);
    const rx_addr area = U32(beh, 0x30), node = U32(beh, 0x34);
    if (node != U32(area, 0)) {
        const rx_addr vehicle = RW_VEHICLE(beh);
        SU32(F, -4, F + 0x10);
        (void)rx_waypoint_line_project(node, U32(node, 0x60), vehicle + 0xc0, F + 0x10);
        if (F32(F, 0x18 - 4) - F32(vehicle, 0xc4) > 15.0f) {
            RACE_FRAME_END(F);
            return 0;
        }
    }
    RACE_FRAME_END(F);
    return 1;
}
RX_FUNCTION(0x0052f4a0, rx_roam_target_ok)

/* The unit direction of the vehicle's horizontal velocity (+0x130, +0x138): the vector divided by its length (1 / SqrtF, then
 * multiplied) unless its squared length (vz^2 + vx^2) is exactly zero. */
static void rw_velocity_direction(rx_addr vehicle, float* dir_x, float* dir_z) {
    const float vx = F32(vehicle, 0x130), vz = F32(vehicle, 0x138);
    const float len2 = vz * vz + vx * vx;
    *dir_x = vx;
    *dir_z = vz;
    if (rw_nonzero(len2)) {
        const float inv = 1.0f / rw_sqrtf(len2);
        *dir_x = inv * vx;
        *dir_z = vz * inv;
    }
}

/* The steering shared by 0052f990, 0052f510 and 00530740 toward the ground point (tx, tz): the offset from the target to the
 * vehicle (unit length) scaled by k = 0 / pow(1, 0) - 4.5 / pow(1, -1) (-4.5: the compiled constant powers) goes through the vehicle
 * frame (005084f0, in place on the vector at `vec`: x, 0, z); the unit offset to the target dotted with the velocity direction,
 * clamped to [0, 0.5], decides how much of the demand the driver's throttle (D +0x60) takes: D +0x60 += (1 - dot) * vec.x, and
 * while the settle time (B +0x6c) is ahead D +0x60 is scaled by 1 - (time left / interval B +0x68). */
static void rw_steer_toward(rx_addr beh, float tx, float tz, float dir_x, float dir_z, rx_addr vec) {
    const rx_addr vehicle = RW_VEHICLE(beh);
    const float px = F32(vehicle, 0xc0), pz = F32(vehicle, 0xc8);
    const float d0 = px - tx, d1 = pz - tz;
    const float first = 0.0f / (float)race_pow(1.0 /* 0x977638 */, 0.0);
    const float second = 4.5f /* 0x9cd9e4 */ / (float)race_pow(1.0, -1.0 /* 0x977ae8 */);
    const float k = first - second;
    float n0 = d0, n1 = d1;
    const float len2 = d0 * d0 + d1 * d1;
    if (rw_nonzero(len2)) {
        const float inv = 1.0f / rw_sqrtf(len2);
        n0 = inv * d0;
        n1 = inv * d1;
    }
    SF32(vec, 0, n0 * k);
    SF32(vec, 4, 0.0f);
    SF32(vec, 8, n1 * k);
    RACE_CORE_CALL(0, rx_race_to_vehicle_frame(RW_VEHICLE(beh), vec));
    const float e0 = tx - px, e1 = tz - pz;
    float u0 = e0, u1 = e1;
    const float elen2 = e1 * e1 + e0 * e0;
    if (rw_nonzero(elen2)) {
        const float inv = 1.0f / rw_sqrtf(elen2);
        u0 = e0 * inv;
        u1 = e1 * inv;
    }
    float dot = u1 * dir_z + u0 * dir_x;
    if (dot > 0.5f /* 0x9322cc */) dot = 0.5f;
    else if (0.0f > dot) dot = 0.0f;
    const rx_addr driver = RW_DRIVER(beh);
    SF32(driver, 0x60, (1.0f - dot) * F32(vec, 0) + F32(driver, 0x60));
    if (F32(beh, 0x6c) > F32(driver, 0x130)) {
        const float left = (F32(beh, 0x6c) - F32(driver, 0x130)) / F32(beh, 0x68);
        SF32(driver, 0x60, (1.0f - left) * F32(driver, 0x60));
    }
}

/* 0052f990 (ESI = the behaviour) -> EAX: ride to the target node (+0x34): steer toward its position (x, z); true when the steering
 * vector is short (squared length below 100 = 0x93005c). Frame: push ebp; and esp, -16; sub esp, 0x40 (the vector at +0x30). */
uint32_t rx_roam_arrive(rx_addr beh) {
    RACE_FRAME_EBP(F, 16, 0x40, 0);
    float dir_x, dir_z;
    rw_velocity_direction(RW_VEHICLE(beh), &dir_x, &dir_z);
    const rx_addr node = U32(beh, 0x34);
    rw_steer_toward(beh, F32(node, 0), F32(node, 8), dir_x, dir_z, F + 0x30);
    const float vx = F32(F, 0x30), vz = F32(F, 0x38);
    const uint32_t arrived = 100.0f /* 0x93005c */ > vx * vx + vz * vz;
    RACE_FRAME_END(F);
    return arrived;
}
RX_FUNCTION(0x0052f990, rx_roam_arrive)

/* 0052f510 (stack the behaviour; RET 4) -> EAX = the target node (+0x34): follow the track toward the target node. Steer toward
 * the track point `reach` ahead of the node (0050a650, reach = the lesser of 30 and the area's length +0x120); once the vehicle is
 * past the node (the node's direction +0x50 / +0x54 / +0x58 against the vehicle's offset > 0) the target moves on to the next
 * node (+0x64) and the driver's steering state D +0x2f0 .. +0x324 is cleared; at the end of the area (no next node) the
 * linked areas (area +0x08, count +0x10) that suit the vehicle (flag 1, 0052efa0; at most ten) are offered to the section
 * chooser (0050a360) and the chosen area and its first node become the target. Frame: push ebp; and esp, -16; sub esp, 0x74; push
 * ebx / esi / edi (the look-ahead point at +0x30, the steering vector at +0x40, the candidates at +0x58). */
rx_addr rx_roam_follow(rx_addr beh) {
    RACE_FRAME_EBP(F, 16, 0x74, 3);
    float dir_x, dir_z;
    rw_velocity_direction(RW_VEHICLE(beh), &dir_x, &dir_z);
    const rx_addr area0 = U32(beh, 0x30);
    const float length = F32(area0, 0x120);
    const float reach = 30.0f /* 0x931d98 */ > length ? length : 30.0f;
    SU32(F, -8, F + 0x40);
    SF32(F, -4, rw_x87(reach));
    RACE_CORE_CALL(2, rx_track_look_ahead(U32(beh, 0x34), F + 0x30, F + 0x40, reach));
    rw_steer_toward(beh, F32(F, 0x30), F32(F, 0x38), dir_x, dir_z, F + 0x40);
    const rx_addr driver = RW_DRIVER(beh);
    const rx_addr vehicle = RW_VEHICLE(beh);
    const rx_addr node = U32(beh, 0x34);
    const float dz = F32(vehicle, 0xc8) - F32(node, 8);
    const float dx = F32(vehicle, 0xc0) - F32(node, 0);
    const float progress = (F32(node, 0x58) * dz + F32(node, 0x54) * 0.0f) + F32(node, 0x50) * dx;
    if (!(progress > 0.0f)) { RACE_FRAME_END(F); return node; }
    SU32(beh, 0x34, U32(node, 0x64));
    SU32(driver, 0x2f0, 0);
    SU32(driver, 0x2f4, 0);
    SU32(driver, 0x30c, 0);
    SU32(driver, 0x31c, 0);
    SU32(driver, 0x324, 0);
    SF32(driver, 0x318, 0.0f);
    SF32(driver, 0x320, 0.0f);
    SU32(driver, 0x2f8, 0);
    if (U32(beh, 0x34) != 0 || U32(U32(beh, 0x30), 0x10) == 0) { RACE_FRAME_END(F); return U32(beh, 0x34); }
    /* the end of the area: the linked areas that suit the vehicle */
    const rx_addr cand = F + 0x58;
    for (uint32_t o = 0; o < 0x28; o += 4) SU32(cand, o, 0);
    uint32_t found = 0;
    if (I32(U32(beh, 0x30), 0x10) > 0) {
        for (int32_t i = 0;;) {
            const rx_addr linked = U32(U32(U32(beh, 0x30), 8), 4u * (uint32_t)i);
            const rx_addr veh = RW_VEHICLE(beh);
            const uint32_t k = U32(veh, 0x198) + 4u * U32(veh, 0x194);
            if ((U8(linked, 0x18 + 4 * k) & 1) != 0 && rx_roam_area_matches(beh, linked)) SU32(cand, 4 * found++, linked);
            if (found == 10) break;
            i++;
            if (!(i < I32(U32(beh, 0x30), 0x10))) break;
        }
    }
    if (found != 0) {
        rx_addr chosen, slot_out = 0;
        RACE_CORE_CALL(4, chosen = rx_race_choose_section(RW_DRIVER(beh), cand, found, 0, &slot_out));
        if (chosen != 0) {
            SU32(beh, 0x30, chosen);
            SU32(beh, 0x34, U32(chosen, 0));
            RACE_FRAME_END(F);
            return U32(beh, 0x34);
        }
    }
    RACE_FRAME_END(F);
    return U32(beh, 0x34);
}
RX_FUNCTION(0x0052f510, rx_roam_follow)

/* the Player List row whose PlayerName is the driver's player's name (player +0x14): the row index, or the row count when none */
static uint32_t rw_player_row(rx_addr beh) {
    const rx_addr table = 0x00ae0e08;                                /* "Player List" */
    uint32_t row = 0;
    uint32_t rows;
    RACE_CORE_CALL(0, rows = rx_tbl_row_count(table));
    for (; row < rows; row++) {
        const rx_addr name = rx_tbl_str_cstr(rx_tbl_cell_string(act_column(table, 0x00936f00 /* "PlayerName" */), row));
        rx_addr a = RW_PLAYER(beh) + 0x14;
        rx_addr b = name;
        while (U8(a, 0) == U8(b, 0) && U8(a, 0) != 0) { a++; b++; }
        if (U8(a, 0) == U8(b, 0)) break;
        RACE_CORE_CALL(0, rows = rx_tbl_row_count(table));
    }
    return row;
}

/* 00530740 (stack the behaviour; RET 4): the AI at its journey's end: steer toward the area's centre (B +0x20 / +0x28: the
 * matrix translation of the area, vf15), and by the activity type (00508920 of the area name) either (case 0: types 0 .. 4, 7, 12,
 * 17 .. 19) look up the area's activity (0055f630 over the game's activities of the view) and, when the centre or the track node
 * nearest to the vehicle is within 100 units (10000 = 0x931e98 squared) and the vehicle class may do it (00530dc0), ask the Player
 * List to start it for this player (004dde70 with the area's type and area byte +0x104) -- or, when it cannot but the centre is far,
 * go to the nearest suitable area (00530590: target area / node, the settle time, state 2) or choose another area (0052ed50); or
 * (case 1: types 5, 6, 20) within 100 units of the centre ask for activity 5 in area 0; other types do nothing. Frame: push ebp; and
 * esp, -16; sub esp, 0xc4; push ebx / esi / edi. */
void rx_roam_arrived(rx_addr beh) {
    RACE_FRAME_EBP(F, 16, 0xc4, 3);
    static const uint8_t type_case[21] = {0, 0, 0, 0, 0, 1, 1, 0, 2, 2, 2, 2, 0, 2, 2, 2, 2, 0, 0, 0, 1};
    rw_vcall_out(U32(beh, 0x10), 0x3c, F + 0x90);
    float dir_x, dir_z;
    rw_velocity_direction(RW_VEHICLE(beh), &dir_x, &dir_z);
    const float tx = F32(beh, 0x20), tz = F32(beh, 0x28);
    rw_steer_toward(beh, tx, tz, dir_x, dir_z, F + 0x40);
    const rx_addr vehicle = RW_VEHICLE(beh);
    const float e0 = tx - F32(vehicle, 0xc0), e1 = tz - F32(vehicle, 0xc8);
    const float target_dist2 = e0 * e0 + e1 * e1;
    uint32_t type;
    RACE_CORE_CALL(1, type = rx_roam_activity_type(U32(beh, 0x10) + 0x108));
    if (type > 0x14) { RACE_FRAME_END(F); return; }
    const float near = 10000.0f;                                      /* 0x931e98 */
    if (type_case[type] == 0) {
        /* the activity of this area among the view's activities */
        const rx_addr frame = U32(0, 0x00a95844);
        rx_addr holder;
        if (U32(U32(frame, 0x20), 0x34) == 1) holder = U32(U32(0, 0x00a946b8), 0x5a54);
        else if (U32(U32(frame, 0x1c), 0x34) == 1) holder = U32(U32(0, 0x00a946b8), 0x20c8);
        else holder = 0;
        rx_addr activity;
        RACE_CORE_CALL(1, activity = rx_activity_find_by_kind_area(holder + 0x21da0, type, U32(U32(beh, 0x10), 0x104)));
        if (activity == 0) { RACE_FRAME_END(F); return; }
        const rx_addr veh = RW_VEHICLE(beh);
        const rx_addr nearest = F + 0x20;
        RACE_CORE_CALL(3, rx_track_nearest_node_all(activity + 0x890, veh + 0x130, 0, veh + 0xc0, nearest));
        if (!(near > F32(nearest, 8)) && !(near > target_dist2)) { RACE_FRAME_END(F); return; }
        const rx_addr v2 = RW_VEHICLE(beh);
        if (rx_roam_class_allowed(type, U32(v2, 0x194), U32(v2, 0x198))) {
            const uint32_t row = rw_player_row(beh);
            uint32_t rows;
            RACE_CORE_CALL(0, rows = rx_tbl_row_count(0x00ae0e08));
            if (row < rows) RACE_CORE_CALL(2, rx_player_list_request_activity(row, type, U8(U32(beh, 0x10), 0x104)));
            RACE_FRAME_END(F);
            return;
        }
        if (target_dist2 > near) {
            rx_addr area;
            RACE_CORE_CALL(1, area = rx_roam_nearest_area(beh, F + 0x20));
            if (area != 0) {
                SU32(beh, 0x30, U32(F, 0x20));
                SU32(beh, 0x34, U32(F, 0x24));
                SF32(beh, 0x6c, RW_CLOCK(beh) + F32(beh, 0x68));
                SU32(beh, 0xc, 2);
                RACE_FRAME_END(F);
                return;
            }
        }
        RACE_CORE_CALL(1, (void)rx_behavior_roam_world_pick_area(beh));
        RACE_FRAME_END(F);
        return;
    }
    if (type_case[type] == 1) {
        rw_vcall_out(U32(beh, 0x10), 0x3c, F + 0x50);
        const rx_addr veh = RW_VEHICLE(beh);
        const float d0 = F32(F, 0x80) - F32(veh, 0xc0);
        const float d2 = F32(F, 0x88) - F32(veh, 0xc8);
        const float d1 = F32(F, 0x84) - F32(veh, 0xc4);
        const float dist2 = (d0 * d0 + d2 * d2) + d1 * d1;
        if (!(near > dist2)) { RACE_FRAME_END(F); return; }
        const rx_addr v2 = RW_VEHICLE(beh);
        if (!rx_roam_class_allowed(type, U32(v2, 0x194), U32(v2, 0x198))) {
            RACE_CORE_CALL(1, (void)rx_behavior_roam_world_pick_area(beh));
            RACE_FRAME_END(F);
            return;
        }
        const uint32_t row = rw_player_row(beh);
        uint32_t rows;
        RACE_CORE_CALL(0, rows = rx_tbl_row_count(0x00ae0e08));
        if (row < rows) RACE_CORE_CALL(2, rx_player_list_request_activity(row, 5, 0));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00530740, rx_roam_arrived)

/* The terrain-rise probes (B +0x70, +0xb0, +0xf0) restart at the vehicle's position (005166e0: EAX = probe, ECX = position). */
static void rw_probe_reset(rx_addr beh, rx_addr probe) {
    RACE_CORE_CALL(0, rx_waypoint_probe_reset(probe, RW_VEHICLE(beh) + 0xc0));
}
/* ... and advance along a direction (00516590: ESI = probe, stack the direction) */
static void rw_probe_advance(rx_addr probe, rx_addr direction) {
    SU32(rx_sp(), -4, direction);
    RACE_CORE_CALL(1, rx_waypoint_probe_advance(probe, direction));
}

/* the vehicle's matrix row 0 (vf15 into the frame buffer at `m`) as the unit vectors states 5 / 6 / 7 ride along: p = row, q = (-x, y, -z)
 * of the row; `unit` stores the normalized form (1 / SqrtF times the components) when the squared length is above zero */
static void rw_unit3(rx_addr dst, float x, float y, float z, float len2, int normalize_gate) {
    if (normalize_gate ? len2 > 0.0f : rw_nonzero(len2)) {
        const float inv = 1.0f / rw_sqrtf(len2);
        SF32(dst, 0, inv * x);
        SF32(dst, 4, inv * y);
        SF32(dst, 8, inv * z);
    } else {
        SF32(dst, 0, x);
        SF32(dst, 4, y);
        SF32(dst, 8, z);
    }
}

/* 0052fcf0 BehaviorRoamWorld::vf4 (ECX = the behaviour; RET 4, the argument is unused) -> EAX 0. See the file comment. The probes' direction
 * for the rises follows the heading while the "ahead" probe (B +0x88 clear) is advanced first. States:
 *  0 / 2: ride to the target node (0052f990), when there -> 1 / 3; 1: follow the track (0052f510): the target gone (no node) -> pick the
 *  next after the settle time (4); a target that is no longer good (0052f4a0) -> 4; 3: follow, a node: stay; none: choose another area
 *  (0052ed50), -> 4; 4: look for a target (0052fca0), else probes restarted at the vehicle (states 7 with the side probes) and the journey's
 *  end (00530740); 5 / 6: hold to one side (the left / right probe and the throttle +-1) until the probe is clear; 7: both sides.
 * Frame: push ebp; and esp, -16; sub esp, 0xc4; push ebx / esi / edi (matrices at +0x50 / +0x90, directions at +0x10 / +0x30). */
uint32_t rx_behavior_roam_world_update(rx_addr beh, float dt) {
    (void)dt;
    RACE_FRAME_EBP(F, 16, 0xc4, 3);
    if (U32(beh, 4) == 0) goto done;
    const rx_addr probe_main = beh + 0x70, probe_left = beh + 0xb0, probe_right = beh + 0xf0;
    if (U32(beh, 0x88) == 0) rw_probe_advance(probe_main, RW_VEHICLE(beh) + 0x130);
    const rx_addr driver = RW_DRIVER(beh);
    switch (U32(beh, 0xc)) {
    case 0:
        if (rx_roam_arrive(beh)) SU32(beh, 0xc, 1);
        goto done;
    case 1: {
        rx_addr node;
        RACE_CORE_CALL(1, node = rx_roam_follow(beh));
        if (node == 0) {
            SF32(beh, 0x6c, F32(driver, 0x130) + F32(beh, 0x68));
            SU32(beh, 0x30, 0);
            SU32(beh, 0x34, 0);
            SU32(beh, 0xc, 4);
            goto done;
        }
        uint32_t ok;
        RACE_CORE_CALL(0, ok = rx_roam_target_ok(beh));
        if (ok) goto done;
        SU32(beh, 0x30, 0);
        SU32(beh, 0x34, 0);
        SU32(beh, 0xc, 4);
        goto done; }
    case 2:
        if (rx_roam_arrive(beh)) SU32(beh, 0xc, 3);
        goto done;
    case 3: {
        rx_addr node;
        RACE_CORE_CALL(1, node = rx_roam_follow(beh));
        if (node != 0) goto done;
        SF32(beh, 0x6c, F32(RW_DRIVER(beh), 0x130) + F32(beh, 0x68));
        RACE_CORE_CALL(1, (void)rx_behavior_roam_world_pick_area(beh));
        SU32(beh, 0xc, 4);
        goto done; }
    case 4: {
        uint32_t picked;
        RACE_CORE_CALL(0, picked = rx_roam_pick_target(beh));
        if (picked != 0) goto done;
        if (U32(beh, 0x88) != 0) {
            if (U32(beh, 0x84) != 0) {
                SU32(beh, 0xc, 7);
                rw_probe_reset(beh, probe_left);
                rw_probe_reset(beh, probe_right);
            }
            rw_probe_reset(beh, probe_main);
        }
        RACE_CORE_CALL(1, rx_roam_arrived(beh));
        goto done; }
    case 5: {
        uint32_t picked;
        RACE_CORE_CALL(0, picked = rx_roam_pick_target(beh));
        if (picked != 0) goto done;
        if (U32(beh, 0x84) != 0) SF32(driver, 0x60, F32(driver, 0x60) - 1.0f /* 0x931f24 */);
        rw_vcall_out(RW_VEHICLE(beh), 0x3c, F + 0x50);
        {
            const float m0 = F32(F, 0x50), m1 = F32(F, 0x54), m2 = F32(F, 0x58);
            const float len2 = (m0 * m0 + m1 * m1) + m2 * m2;
            rw_unit3(F + 0x10, m0, m1, m2, len2, 1);
        }
        if (U32(beh, 0x88) != 0) rw_probe_reset(beh, probe_main);
        if (U32(beh, 0x108) == 0) rw_probe_advance(probe_right, F + 0x10);
        if (U32(beh, 0x108) != 0) {
            if (U32(beh, 0x104) == 0 && U32(beh, 0x84) == 0) SU32(beh, 0xc, 4);
            rw_probe_reset(beh, probe_right);
        }
        goto done; }
    case 6: {
        uint32_t picked;
        RACE_CORE_CALL(0, picked = rx_roam_pick_target(beh));
        if (picked != 0) goto done;
        if (U32(beh, 0x84) != 0) SF32(driver, 0x60, F32(driver, 0x60) + 1.0f);
        rw_vcall_out(RW_VEHICLE(beh), 0x3c, F + 0x50);
        {
            const float m0 = F32(F, 0x50), m1 = F32(F, 0x54), m2 = F32(F, 0x58);
            const float q0 = -0.0f /* 0x9774a8 */ - m0, q2 = -0.0f - m2;
            const float len2 = (m1 * m1 + q0 * q0) + q2 * q2;
            rw_unit3(F + 0x10, q0, m1, q2, len2, 1);
        }
        if (U32(beh, 0x88) != 0) rw_probe_reset(beh, probe_main);
        if (U32(beh, 0xc8) == 0) rw_probe_advance(probe_left, F + 0x10);
        if (U32(beh, 0xc8) != 0) {
            if (U32(beh, 0xc4) == 0 && U32(beh, 0x84) == 0) SU32(beh, 0xc, 4);
            rw_probe_reset(beh, probe_left);
        }
        goto done; }
    case 7: {
        uint32_t picked;
        RACE_CORE_CALL(0, picked = rx_roam_pick_target(beh));
        if (picked != 0) goto done;
        rw_vcall_out(RW_VEHICLE(beh), 0x3c, F + 0x50);
        {
            /* the side vector q = (-x, y, -z) at +0x30 (normalized when its length is not zero... above zero) and the row p at +0x10 */
            const float m0 = F32(F, 0x50), m1 = F32(F, 0x54), m2 = F32(F, 0x58);
            const float q2 = -0.0f - m2, q0 = -0.0f - m0;
            const float qlen2 = (q2 * q2 + q0 * q0) + m1 * m1;
            rw_unit3(F + 0x30, q0, m1, q2, qlen2, 1);
            const float plen2 = (m0 * m0 + m2 * m2) + m1 * m1;
            rw_unit3(F + 0x10, m0, m1, m2, plen2, 1);
        }
        if (U32(beh, 0xc8) == 0) rw_probe_advance(probe_left, F + 0x30);
        if (U32(beh, 0x108) == 0) rw_probe_advance(probe_right, F + 0x10);
        rw_vcall_out(U32(beh, 0x10), 0x3c, F + 0x90);
        {
            const rx_addr vehicle = RW_VEHICLE(beh);
            const float px = F32(vehicle, 0xc0), pz = F32(vehicle, 0xc8);
            float dir_x, dir_z;
            rw_velocity_direction(vehicle, &dir_x, &dir_z);
            const float a = F32(beh, 0x20) - px, b = F32(beh, 0x28) - pz;
            const float along = b * dir_z + a * dir_x;
            if (U32(beh, 0x108) == 0 && U32(beh, 0xc8) == 0) goto arrived;
            if (-0.5f /* 0x9774b0 */ > along) goto arrived;
            const rx_addr v2 = RW_VEHICLE(beh);
            float far_left = rx_bf(0x7f7fffffu), far_right = rx_bf(0x7f7fffffu);
            if (U32(beh, 0xc4) != 0) {
                const float dx = F32(probe_left, 0x20) - F32(v2, 0xc0);
                const float dz = F32(probe_left, 0x28) - F32(v2, 0xc8);
                const float dy = F32(probe_left, 0x24) - F32(v2, 0xc4);
                far_left = (dz * dz + dy * dy) + dx * dx;
            }
            if (U32(beh, 0x104) != 0) {
                const float dx = F32(probe_right, 0x20) - F32(v2, 0xc0);
                const float dy = F32(probe_right, 0x24) - F32(v2, 0xc4);
                const float dz = F32(probe_right, 0x28) - F32(v2, 0xc8);
                far_right = (dy * dy + dx * dx) + dz * dz;
            }
            const float side = b * F32(F, 0x18) + a * F32(F, 0x10);
            const float limit = 40.0f;                               /* 0x9cc37c */
            if (0.0f > side) {
                if (far_right > limit) goto state6;
                if (far_left > limit) goto state5;
            } else {
                if (far_left > limit) goto state5;
                if (far_right > limit) goto state6;
            }
            rw_probe_reset(beh, probe_left);
            rw_probe_reset(beh, probe_right);
            goto done;
        state5:
            SU32(beh, 0xc, 5);
            rw_probe_reset(beh, probe_main);
            rw_probe_reset(beh, probe_right);
            goto done;
        state6:
            SU32(beh, 0xc, 6);
            rw_probe_reset(beh, probe_main);
            rw_probe_reset(beh, probe_left);
            goto done;
        arrived:
            RACE_CORE_CALL(1, rx_roam_arrived(beh));
            goto done;
        } }
    default:
        break;
    }
done:
    RACE_FRAME_END(F);
    return 0;
}
RX_FUNCTION(0x0052fcf0, rx_behavior_roam_world_update)
