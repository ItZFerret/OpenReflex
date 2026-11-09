/* race_rules.c -- the race rules (Rule* classes): small objects each driver owns (driver +0xa0[i], count +0xee4) and runs once per
 * frame through their vf5 (RaceDriver::vf7). Rule: +0x00 vtable, +0x04 driver, +0x08 enabled. Their vf3 returns the rule's type id
 * (RuleFinishLine 4, ...; the driver finds a rule by id with 0053aaa0). (core/notes/race.md) */
#include "race.h"
#include "../src/fmt_modtrack.h"
#include "race_extern.h"
#include "build_activity_int.h"
#include "world_node.h"
#include "free_ride_borders.h"
#include "practice.h"   /* PORT ADDITION: rx_race_line_fraction (the lap log) */

/* Driver event handler (vf9, slot 0x24): RaceDriver::vf9 0x00512140 for the race drivers (an event raised by a rule). */
typedef void (*race_driver_event_fn)(rx_addr driver, rx_addr source);
static void race_driver_event(rx_addr driver, rx_addr source) {
    rx_addr fn = race_vfn(driver, 9);
    race_driver_event_fn f = RX_FN(fn, race_driver_event_fn);
    if (f) RACE_CORE_CALL(1, f(driver, source)); else race_thiscall(fn, driver, 1, source, 0, 0, 0);   /* push source; call vf9 */
}

/* 0051a810 (EAX = line, EDI = position, arg = previous position; RET 4): did the move prev -> pos cross the line?
 * line: +0x00 a point of the line's plane, +0x10 its normal, +0x2c the gate (two posts: +0x20 left, +0x10 right).
 * The plane normal is made perpendicular to the gate (the gate direction is taken flat, y = 0) and normalized; the move crosses
 * when pos is in front of the plane and prev behind it. Returns 0 = crossed between the posts, 2 = crossed outside them,
 * 1 = not crossed. */
int rx_race_line_crossed(rx_addr line, rx_addr pos, rx_addr prev) {
    rx_addr gate = U32(line, 0x2c);
    float dx = F32(gate, 0x10) - F32(gate, 0x20), dz = F32(gate, 0x18) - F32(gate, 0x28), dy = 0.0f;
    float nx = F32(line, 0x10), ny = F32(line, 0x14), nz = F32(line, 0x18);
    float len2 = dz * dz + dx * dx;
    if (len2 > 0.0f) {
        float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        dy = inv * 0.0f /* 0x92e12c */;
        dx = dx * inv;
        dz = dz * inv;
    }
    /* the normal without its component along the gate */
    float along = (dz * nz + ny * dy) + dx * nx;
    nx = nx - dx * along;
    ny = ny - along * dy;
    nz = nz - dz * along;
    float n2 = (nz * nz + ny * ny) + nx * nx;
    if (n2 > 0.0f) {
        float inv = 1.0f / sqrtf(n2);
        nx = inv * nx;
        ny = ny * inv;
        nz = nz * inv;
    }
    float plane = (nz * F32(line, 8) + ny * F32(line, 4)) + nx * F32(line, 0);
    float px = F32(pos, 0), py = F32(pos, 4), pz = F32(pos, 8);
    if (!(((py * ny + pz * nz) + px * nx) - plane > 0.0f)) return 1;
    if (!(0.0f > ((F32(prev, 8) * nz + F32(prev, 4) * ny) + F32(prev, 0) * nx) - plane)) return 1;
    /* crossed: between the posts? (distance along the gate from the left post) */
    float t = ((pz - F32(gate, 0x28)) * dz + (px - F32(gate, 0x20)) * dx) + (py - F32(gate, 0x24)) * dy;
    if (t > 0.0f) {
        float gx = F32(gate, 0x10) - F32(gate, 0x20), gy = F32(gate, 0x14) - F32(gate, 0x24), gz = F32(gate, 0x18) - F32(gate, 0x28);
        if (sqrtf((gz * gz + gy * gy) + gx * gx) > t) return 0;
    }
    return 2;
}
RX_FUNCTION(0x0051a810, rx_race_line_crossed)

/* PORT ADDITION (not original code; game/practice.h, the lap log's sub-frame time): where on the move prev -> cur (host floats) the line's
 * plane is met, the plane built as rx_race_line_crossed builds it (the normal made perpendicular to the flat gate direction): f in [0, 1]
 * when prev is behind the plane and cur in front of it, else -1. Reads the line only. */
float rx_race_line_fraction(rx_addr line, const float cur[3], const float prev[3]) {
    rx_addr gate = U32(line, 0x2c);
    float dx = F32(gate, 0x10) - F32(gate, 0x20), dz = F32(gate, 0x18) - F32(gate, 0x28), dy = 0.0f;
    float nx = F32(line, 0x10), ny = F32(line, 0x14), nz = F32(line, 0x18);
    const float len2 = dz * dz + dx * dx;
    if (len2 > 0.0f) { const float inv = 1.0f / sqrtf(len2); dx *= inv; dz *= inv; }
    const float along = (dz * nz + ny * dy) + dx * nx;
    nx -= dx * along; ny -= along * dy; nz -= dz * along;
    const float n2 = (nz * nz + ny * ny) + nx * nx;
    if (n2 > 0.0f) { const float inv = 1.0f / sqrtf(n2); nx *= inv; ny *= inv; nz *= inv; }
    const float plane = (nz * F32(line, 8) + ny * F32(line, 4)) + nx * F32(line, 0);
    const double a = (double)((prev[1] * ny + prev[2] * nz) + prev[0] * nx) - plane;
    const double b = (double)((cur[1] * ny + cur[2] * nz) + cur[0] * nx) - plane;
    if (!(a < 0.0) || !(b > 0.0)) return -1.0f;
    const double f = -a / (b - a);
    return f < 0.0 ? 0.0f : f > 1.0 ? 1.0f : (float)f;
}

/* 0051a7a0 RuleFinishLine::vf5 (ECX = rule, arg = dt (unused); RET 4): while enabled, the driver's player has a vehicle that
 * is not wrecking (+0x280) and is not being reset (player +0x116c <= 0), a crossing of the finish line (the track's line, track
 * +0x70) by the move from the driver's last position (+0x80) to the vehicle's (+0xc0) is reported to the driver (vf9 with the rule:
 * RaceDriver::vf9 counts the lap). */
void rx_rule_finish_line_update(rx_addr rule, float dt) {
    (void)dt;
    if (U32(rule, 8) == 0) return;
    rx_addr driver = U32(rule, 4), player = U32(driver, 0x74), vehicle = U32(player, 0x1ac);
    if (vehicle == 0 || U32(vehicle, 0x280) != 0 || F32(player, 0x116c) > 0.0f /* 0x92e12c */) return;
    if(fmt_modtrack_racing()) {
        const rx_addr track=U32(driver,0x10);
        float fraction=1.f-F32(driver,0x10cc)/F32(track,0x1dc);
        if(!fmt_modtrack_lap_gate(driver,I32(driver,0xf1c),fraction))return;
    }
    if (rx_race_line_crossed(U32(driver, 0x10) + 0x70, vehicle + 0xc0, driver + 0x80) == 0) race_driver_event(driver, rule);
}
RX_FUNCTION(0x0051a7a0, rx_rule_finish_line_update)

/* 0051a790 (vf3 of RuleFinishLine and 11 other classes): the type id 4. */
uint32_t rx_rule_finish_line_type(rx_addr self) { (void)self; return 4; }
RX_FUNCTION(0x0051a790, rx_rule_finish_line_type)

/* 0051b0e0 BehaviorBrakeTurn::vf5 (shared by 3 classes): the type id 0xb. */
uint32_t rx_behavior_brake_turn_type(rx_addr self) { (void)self; return 0xb; }
RX_FUNCTION(0x0051b0e0, rx_behavior_brake_turn_type)

/* ---- finding a driver's rule / behaviour by its type id -------------------------------------------------------------------
 * Rules: driver +0xa0[i], i < +0xee4 (empty slots skipped), type = vf3 (0xb RuleOfftrackTimePenalty).
 * Behaviours: driver +0xe8[i], i < +0xee0, type = vf5 (2 NavigateTrack, 9 AirLeveling, 0xc FixForwardProgress, 0x13 LapPacing,
 * 0x17 SuckRacing; the full list in core/notes/race.md). The first match, or 0. */
typedef uint32_t (*race_type_fn)(rx_addr self);
static uint32_t object_type(rx_addr obj, uint32_t slot) {
    rx_addr fn = race_vfn(obj, slot);
    race_type_fn f = RX_FN(fn, race_type_fn);
    return f ? f(obj) : race_thiscall(fn, obj, 0, 0, 0, 0, 0);
}
static rx_addr find_rule(rx_addr driver, uint32_t type) {
    for (uint32_t i = 0; i < U32(driver, 0xee4); i++) {
        rx_addr r = U32(driver, 0xa0 + 4 * i);
        if (r != 0 && object_type(r, 3) == type) return U32(driver, 0xa0 + 4 * i);
    }
    return 0;
}
static rx_addr find_behavior(rx_addr driver, uint32_t type) {
    for (uint32_t i = 0; i < U32(driver, 0xee0); i++)
        if (object_type(U32(driver, 0xe8 + 4 * i), 5) == type) return U32(driver, 0xe8 + 4 * i);
    return 0;
}
/* (all: stack argument = driver; RET 4) */
rx_addr rx_driver_rule_0(rx_addr driver) { return find_rule(driver, 0); }                         /* 0053ae50 */
RX_FUNCTION(0x0053ae50, rx_driver_rule_0)
rx_addr rx_driver_rule_1(rx_addr driver) { return find_rule(driver, 1); }                         /* 0053ae00 */
RX_FUNCTION(0x0053ae00, rx_driver_rule_1)
rx_addr rx_driver_offtrack_penalty_rule(rx_addr driver) { return find_rule(driver, 0xb); }        /* 0053aaa0 */
RX_FUNCTION(0x0053aaa0, rx_driver_offtrack_penalty_rule)
rx_addr rx_driver_behavior_1(rx_addr driver) { return find_behavior(driver, 1); }                 /* 0053baa0 */
RX_FUNCTION(0x0053baa0, rx_driver_behavior_1)
rx_addr rx_driver_navigate_behavior(rx_addr driver) { return find_behavior(driver, 2); }          /* 0053a310 */
RX_FUNCTION(0x0053a310, rx_driver_navigate_behavior)
rx_addr rx_driver_roam_behavior(rx_addr driver) { return find_behavior(driver, 0xe); }           /* 0053a560 */
RX_FUNCTION(0x0053a560, rx_driver_roam_behavior)
rx_addr rx_driver_behavior_3(rx_addr driver) { return find_behavior(driver, 3); }                 /* 0053ac30 */
RX_FUNCTION(0x0053ac30, rx_driver_behavior_3)
rx_addr rx_driver_behavior_5(rx_addr driver) { return find_behavior(driver, 5); }                 /* 0053abe0 */
RX_FUNCTION(0x0053abe0, rx_driver_behavior_5)
rx_addr rx_driver_air_leveling_behavior(rx_addr driver) { return find_behavior(driver, 9); }      /* 0053a510 */
RX_FUNCTION(0x0053a510, rx_driver_air_leveling_behavior)
rx_addr rx_driver_fix_progress_behavior(rx_addr driver) { return find_behavior(driver, 0xc); }    /* 0053ab40 */
RX_FUNCTION(0x0053ab40, rx_driver_fix_progress_behavior)
rx_addr rx_driver_lap_pacing_behavior(rx_addr driver) { return find_behavior(driver, 0x13); }     /* 0053aaf0 */
RX_FUNCTION(0x0053aaf0, rx_driver_lap_pacing_behavior)
rx_addr rx_driver_suck_racing_behavior(rx_addr driver) { return find_behavior(driver, 0x17); }    /* 0053ab90 */
RX_FUNCTION(0x0053ab90, rx_driver_suck_racing_behavior)
rx_addr rx_driver_track_advance_rule(rx_addr driver) { return find_rule(driver, 2); }             /* 0053b670 */
RX_FUNCTION(0x0053b670, rx_driver_track_advance_rule)
rx_addr rx_driver_rule_7(rx_addr driver) { return find_rule(driver, 7); }                         /* 00554c90 */
RX_FUNCTION(0x00554c90, rx_driver_rule_7)


/* ---- RuleTrackAdvance -------------------------------------------------------------------------------------------------------
 * Rule: +0x0c distance travelled since the last section change (FLT_MAX = no re-sync needed), +0x10 the section it was measured
 * for. Driver: +0x14 current section, +0x18 / +0x1c previous / next node (the segment the vehicle is in), +0x90 the node used
 * for lap progress. */

/* 0051d8c0 (ESI = rule) -> xmm0: the distance along the section from its first node to the driver's position: the sum of the
 * lengths (+0x4f0) of the nodes before the driver's current node (+0x18), plus the position's projection on that node's
 * direction (+0x50) clamped to [0, its length]. */
float rx_track_advance_distance(rx_addr rule) {
    rx_addr d = U32(rule, 4);
    rx_addr n = U32(U32(d, 0x14), 0);
    const rx_addr stop = U32(d, 0x18);
    float sum = 0.0f;
    if (n != stop) {
        do {
            sum = rx_adds(F32(n, 0x4f0), sum);
            n = U32(n, 0x64);
        } while (n != stop);
    }
    const rx_addr cur = U32(d, 0x18);
    const rx_addr veh = U32(U32(d, 0x74), 0x1ac);
    const float dy = rx_subs(F32(veh, 0xc4), F32(cur, 4)), dz = rx_subs(F32(veh, 0xc8), F32(cur, 8));
    const float dx = rx_subs(F32(veh, 0xc0), F32(cur, 0));
    float t = rx_adds(rx_adds(rx_muls(F32(cur, 0x58), dz), rx_muls(F32(cur, 0x54), dy)), rx_muls(F32(cur, 0x50), dx));
    if (t > F32(cur, 0x4f0)) t = F32(stop, 0x4f0);
    else t = rx_adds(rx_adds(rx_muls(F32(stop, 0x58), dz), rx_muls(F32(stop, 0x54), dy)), rx_muls(F32(stop, 0x50), dx));
    if (0.0f > t) return rx_adds(0.0f, sum);
    const rx_addr n2 = U32(U32(rule, 4), 0x18);
    const float p = rx_adds(rx_adds(rx_muls(F32(n2, 0x58), dz), rx_muls(F32(n2, 0x54), dy)), rx_muls(F32(n2, 0x50), dx));
    if (p > F32(n2, 0x4f0)) return rx_adds(F32(stop, 0x4f0), sum);
    return rx_adds(rx_adds(rx_adds(rx_muls(F32(stop, 0x58), dz), rx_muls(F32(stop, 0x54), dy)), rx_muls(dx, F32(stop, 0x50))), sum);
}
RX_FUNCTION(0x0051d8c0, rx_track_advance_distance)
static float track_advance_distance(rx_addr rule) {
    float r;
    RACE_CORE_CALL(0, r = rx_track_advance_distance(rule));
    return r;
}
/* 00538bb0 (ECX = node, EAX = next node, EDI = position) -> EAX: is the position within the segment node -> next. */
static uint32_t track_segment_holds(rx_addr node, rx_addr next, rx_addr pos) {
    uint32_t r;
    RACE_CORE_CALL(0, r = rx_track_quad_contains(node, next, pos));
    return r;
}

/* Rule vf5 (slot 5): the per-frame update (dt). */
typedef void (*race_rule_update_fn)(rx_addr rule, float dt);
static void rule_update(rx_addr rule, float dt) {
    rx_addr fn = race_vfn(rule, 5);
    race_rule_update_fn f = RX_FN(fn, race_rule_update_fn);
    if (f) f(rule, dt); else race_thiscall(fn, rule, 1, rx_fb(dt), 0, 0, 0);
}

/* Moves the driver's segment forward while the vehicle is on the track ahead of the next node; AI drivers report each first
 * advance of a frame to the driver (vf9). */
static void advance_segments(rx_addr rule, int report) {
    rx_addr d = U32(rule, 4);
    if (!rx_race_on_track_ahead(d, U32(d, 0x1c))) return;
    for (int n = 0;; n++) {
        if (report && n == 0) race_driver_event(U32(rule, 4), rule);
        d = U32(rule, 4);
        SU32(d, 0x18, U32(d, 0x1c));
        SU32(d, 0x1c, U32(U32(d, 0x1c), 0x64));
        rx_addr next = U32(d, 0x1c);
        if (next != 0 && U32(next, 0x60) == U32(d, 0x90)) SU32(d, 0x90, next);
        if (U32(d, 0x1c) == 0) break;
        if (!rx_race_on_track_ahead(d, U32(d, 0x1c))) break;
    }
}
/* 0051d890 (EAX = rule): after a section change, the re-sync distance starts at 0 when the section's first previous section has
 * forks (more than one next section), otherwise at FLT_MAX (none needed). */
void rx_rule_track_advance_section_changed(rx_addr rule) {
    SF32(rule, 0xc, I32(U32(U32(U32(rule, 0x10), 0xc), 0), 0x10) > 1 ? 0.0f : rx_bf(0x7f7fffff) /* 0x977494 */);
}
RX_FUNCTION(0x0051d890, rx_rule_track_advance_section_changed)

static void enter_section_start(rx_addr d) {
    SU32(d, 0x18, U32(U32(d, 0x14), 0));
    SU32(d, 0x1c, U32(U32(U32(d, 0x14), 0), 0x64));
    SU32(d, 0x90, U32(d, 0x1c));
}

/* 0051d400 RuleTrackAdvance::vf5 (ECX = rule, arg = dt; RET 4): keeps the driver's place on the track (section and segment):
 * advances node by node while the vehicle is in the segment ahead (not while off track by the off-track rule, wrecked
 * (vehicle +0x280), or for the human, being reset); at a section's end goes into the next section (an AI chooses among the
 * forks: 0050a360). For the human driver it also re-syncs: within 450 units of a section change it looks for the segment that
 * holds the vehicle in every section (nearest node within 450 units of each section's start) and jumps there; then the off-track
 * rule is updated at once. */
void rx_rule_track_advance_update(rx_addr rule, float dt) {
    if (U32(rule, 8) == 0) return;
    rx_addr pen = rx_driver_offtrack_penalty_rule(U32(rule, 4));
    rx_addr d = U32(rule, 4), player = U32(d, 0x74), vehicle = U32(player, 0x1ac);
    if (vehicle == 0) return;
    uint32_t on_track = pen != 0 ? U32(pen, 0x10) : 1;
    if (U32(player, 0x10) == 1) {
        uint32_t penalized = pen != 0 ? U32(pen, 0x14) : 0;
        if (on_track != 0 && penalized == 0 && U32(vehicle, 0x280) == 0) advance_segments(rule, 1);
        d = U32(rule, 4);
        if (U32(d, 0x1c) != 0) return;
        rx_addr sec = U32(d, 0x14);
        int32_t forks = I32(sec, 0x10);
        if (forks != 0) {
            if (forks <= 1) SU32(d, 0x14, U32(U32(sec, 8), 0));
            else {
                rx_addr slot;
                rx_addr c = rx_race_choose_section(d, U32(sec, 8), U32(sec, 0x10), 0, &slot);
                if (c != 0) SU32(U32(rule, 4), 0x14, c);
                else { d = U32(rule, 4); SU32(d, 0x14, U32(U32(U32(d, 0x14), 8), 0)); }
            }
        }
        enter_section_start(U32(rule, 4));
        return;
    }
    if (on_track != 0 && U32(vehicle, 0x280) == 0 && !rx_player_is_resetting(player)) advance_segments(rule, 0);
    d = U32(rule, 4);
    if (U32(d, 0x1c) == 0) {
        rx_addr sec = U32(d, 0x14);
        if (U32(sec, 0x10) != 0) SU32(d, 0x14, U32(U32(sec, 8), 0));
        enter_section_start(U32(rule, 4));
    }
    d = U32(rule, 4);
    rx_addr sec = U32(d, 0x14);
    const float none = rx_bf(0x7f7fffff) /* 0x977494 */;
    if (U32(rule, 0x10) != sec) {
        SU32(rule, 0x10, sec);
        SF32(rule, 0xc, (U32(sec, 0x14) != 0 && I32(U32(U32(sec, 0xc), 0), 0x10) > 1) ? 0.0f : none);
    }
    if (!(450.0f /* 0x9358bc */ > F32(rule, 0xc))) return;
    SF32(rule, 0xc, track_advance_distance(rule));
    rx_addr pos = U32(U32(d, 0x74), 0x1ac) + 0xc0;
    rx_addr track = U32(d, 0x10);
    float best = none;
    rx_addr best_node = 0, best_sec = 0;
    for (int32_t i = 0; i < I32(track, 0x1d8); i++) {
        if ((U8(U32(U32(track, 0x130), 4 * (uint32_t)i), 0x118) & 6) != 0) continue;
        rx_addr node = U32(U32(U32(track, 0x130), 4 * (uint32_t)i), 0);
        const float sx = F32(node, 0), sy = F32(node, 4), sz = F32(node, 8);
        float budget = 202500.0f /* 0x977afc: 450^2 */;
        for (rx_addr next = U32(node, 0x64); next != 0; next = U32(node, 0x64)) {
            if (!(budget > 0.0f)) break;
            const float x = F32(node, 0), y = F32(node, 4), z = F32(node, 8);
            const float dy = y - F32(pos, 4), dx = x - F32(pos, 0), dz = z - F32(pos, 8);
            const float d2 = (dz * dz + dy * dy) + dx * dx;
            if (best > d2 && track_segment_holds(node, next, pos) != 0) {
                best_sec = U32(U32(track, 0x130), 4 * (uint32_t)i);
                best_node = node;
                best = d2;
            }
            const float ez = z - sz, ey = y - sy, ex = x - sx;
            budget = budget - ((ez * ez + ey * ey) + ex * ex);
            node = U32(node, 0x64);
        }
    }
    if (best_sec == 0 || U32(U32(rule, 4), 0x14) == best_sec) return;
    d = U32(rule, 4);
    SU32(d, 0x18, best_node);
    SU32(U32(rule, 4), 0x1c, U32(best_node, 0x64));
    SU32(U32(rule, 4), 0x14, best_sec);
    d = U32(rule, 4);
    SU32(d, 0x90, U32(d, 0x1c));
    sec = U32(U32(rule, 4), 0x14);
    SF32(rule, 0xc, (U32(sec, 0x14) != 0 && I32(U32(U32(sec, 0xc), 0), 0x10) < 2) ? none : 0.0f);
    if (pen != 0) rule_update(pen, dt);
}
RX_FUNCTION(0x0051d400, rx_rule_track_advance_update)

/* 005501d0 (EAX = player) -> EAX: is the player being reset to the track (+0x116c > 0)? */
int rx_player_is_resetting(rx_addr player) { return F32(player, 0x116c) > 0.0f /* 0x92e12c */; }
RX_FUNCTION(0x005501d0, rx_player_is_resetting)

/* ---- RuleBackwards ----------------------------------------------------------------------------------------------------------
 * Rule: +0x08 enabled (may flag), +0x0c riding the wrong way. */

/* 00519ce0 (ECX = the segment query of RuleBackwards::vf5, after a hit behind) -> xmm0: how far the vehicle went back along the
 * track: from the found segment's end (the vehicle's position projected on it, in the ground plane) through the nodes up to the
 * driver's node (following each section's first next section). The last term measures the driver's node against itself (0 in
 * practice, as the original computes it). */
float rx_rule_backwards_distance(rx_addr q) {
    rx_addr pos = U32(q, 4), start = U32(q, 0xc), out = U32(q, 0);
    const float px = F32(pos, 0), py = F32(pos, 4), pz = F32(pos, 8);
    const float sx = F32(start, 0), sy = F32(start, 4), sz = F32(start, 8);
    rx_addr n = U32(out, 4);
    if (n == start) {
        const float dz = sz - pz, dy = sy - py, dx = sx - px;
        return sqrtf((dz * dz + dy * dy) + dx * dx);
    }
    rx_addr nn = U32(n, 0x64);
    const float ex = F32(nn, 0) - F32(n, 0), ez = F32(nn, 8) - F32(n, 8);
    const float t = rx_divs((pz - F32(n, 8)) * ez + (px - F32(n, 0)) * ex, ez * ez + ex * ex);
    const float a = rx_subs(F32(nn, 0), rx_adds(rx_muls(ex, t), F32(n, 0)));
    const float b = rx_subs(F32(nn, 8), rx_adds(rx_muls(ez, t), F32(n, 8)));
    float acc = rx_sqrts(rx_adds(rx_muls(b, b), rx_muls(a, a)));
    rx_addr sec = U32(out, 0), node = U32(n, 0x64);
    for (;;) {
        for (; node != 0; node = U32(node, 0x64)) {
            if (node == start) goto done;
            acc = rx_adds(F32(node, 0x4f0), acc);
        }
        if (node == start) break;
        sec = U32(U32(sec, 8), 0);
        node = U32(sec, 0);
        if (node == start) break;
    }
done:;
    rx_addr sn = U32(start, 0x64);
    const float fx = F32(sn, 0) - sx, fz = F32(sn, 8) - sz;
    const float u = rx_divs((sz - sz) * fz + (sx - sx) * fx, fz * fz + fx * fx);
    const float gz = rx_subs(rx_adds(rx_muls(fz, u), sz), sz), gx = rx_subs(rx_adds(rx_muls(fx, u), sx), sx);
    return rx_adds(rx_sqrts(rx_adds(rx_muls(gz, gz), rx_muls(gx, gx))), acc);
}
RX_FUNCTION(0x00519ce0, rx_rule_backwards_distance)

/* 00519cb0 (EAX = rule): reset the driver's player to the track, except in game mode 2 (0xa942b4). */
void rx_rule_backwards_reset(rx_addr rule) {
    rx_addr player = U32(U32(rule, 4), 0x74);
    if (U32(0, 0xa942b4) != 2) rx_player_reset_to_track(player);
}
RX_FUNCTION(0x00519cb0, rx_rule_backwards_reset)

/* Vehicle vf37 (slot 37; RavenVehicleMotocross 00466940 = the body's linear velocity, bike +0x100) into a 16-byte vector. */
static void vehicle_velocity(rx_addr vehicle, rx_addr out) {
    typedef void (*fn_t)(rx_addr, rx_addr);
    rx_addr fn = race_vfn(vehicle, 37);
    fn_t f = RX_FN(fn, fn_t);
    if (f) RACE_CORE_CALL(1, f(vehicle, out)); else race_thiscall(fn, vehicle, 1, out, 0, 0, 0);   /* push out; call vf37 */
}

/* 00519990 RuleBackwards::vf5 (ECX = rule, arg = dt (unused); RET 4): wrong-way detection. When the vehicle (moving at least
 * 1 unit/s) is found in a segment BEHIND the driver's node (005387c0 within the track window, second edges) and its velocity points
 * against that segment (dot < -0.2, 0x9cd95c) the rule flags "wrong way" (if enabled); having gone back more than 60 units, or
 * flagged at the start of the track (no previous node), the player is reset to the track. A change of the flag is reported to the
 * driver (vf9); a wreck clears it (with the "Backwards" event (driver, 0)). */
void rx_rule_backwards_update(rx_addr rule, float dt) {
    (void)dt;
    /* the original's frame (push ebp; and esp, -16; sub esp, 0x64; push ebx / esi / edi): the stack addresses the event
     * handlers keep (list proxies, job records) depend on its depth. F + 0x20 the velocity, F + 0x14 the segment query's result
     * pair, F + 0x40 the query. */
    RACE_ORIG_FRAME_ENTER(F, 0x64, 3);
    rx_addr d = U32(rule, 4), player = U32(d, 0x74);
    if (U32(U32(player, 0x1ac), 0x280) != 0) {
        if (U32(rule, 0xc) == 0) { RACE_ORIG_FRAME_LEAVE(F); return; }
        SU32(rule, 0xc, 0);
        race_driver_event(d, rule);
        d = U32(rule, 4);
        RACE_CORE_CALL(1, rx_event_driver_int(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ef3c /* "Backwards" */, d, 0));
        RACE_ORIG_FRAME_LEAVE(F);
        return;
    }
    vehicle_velocity(U32(player, 0x1ac), F + 0x20);
    float vx = F32(F, 0x20), vy = F32(F, 0x24), vz = F32(F, 0x28);
    const float v2 = race_x87_sumsq3(vx, vz, vy);   /* x87, main thread: 53-bit precision */
    if (1.0f /* 0x931f24 */ > sqrtf(v2)) {
        vx = F32(0, 0xa95d50); vy = F32(0, 0xa95d54); vz = F32(0, 0xa95d58);
    } else if (v2 > 0.0f) {
        const float inv = 1.0f / sqrtf(v2);
        vx = vx * inv; vy = vy * inv; vz = vz * inv;
    }
    const uint32_t was = U32(rule, 0xc);
    d = U32(rule, 4);
    rx_addr vehicle = U32(U32(d, 0x74), 0x1ac);
    const float window = F32(U32(d, 0x10), 0x1b8 + 4 * U32(vehicle, 0x194));
    /* the query lives on the game stack: 00519ce0 reads it */
    const rx_addr Q = F + 0x40, pair = F + 0x14;
    SU32(Q, 0x00, pair); SU32(Q, 0x04, vehicle + 0xc0); SU32(Q, 0x08, U32(d, 0x14)); SU32(Q, 0x0c, U32(d, 0x18));
    SU32(Q, 0x10, 0); SF32(Q, 0x14, window); SF32(Q, 0x18, window); SU32(Q, 0x1c, 0); SU32(Q, 0x20, 1); SU32(Q, 0x24, 0);
    int found;
    RACE_CORE_CALL(1, found = rx_track_find_segment(Q, 1));
    if (found == 2) {
        rx_addr node = U32(pair, 4), nn = U32(node, 0x64);
        float dx = F32(nn, 0) - F32(node, 0), dy = F32(nn, 4) - F32(node, 4), dz = F32(nn, 8) - F32(node, 8);
        const float l2 = (dz * dz + dy * dy) + dx * dx;
        if (l2 > 0.0f) {
            const float inv = 1.0f / sqrtf(l2);
            dx = dx * inv; dy = dy * inv; dz = dz * inv;
        }
        const float dot = (dx * vx + dz * vz) + dy * vy;
        SU32(rule, 0xc, (F32(0, 0x9cd95c) /* -0.2 */ > dot && U32(rule, 8) != 0) ? 1 : 0);
        float gone;
        RACE_CORE_CALL(0, gone = rx_rule_backwards_distance(Q));
        if (gone > 60.0f /* 0x931da0 */ && !(F32(U32(U32(rule, 4), 0x74), 0x116c) > 0.0f)) {
            RACE_CORE_CALL(0, rx_rule_backwards_reset(rule));
            node = U32(pair, 4);
        }
        if (U32(rule, 0xc) != 0 && U32(node, 0x60) == 0 && !(F32(U32(U32(rule, 4), 0x74), 0x116c) > 0.0f))
            RACE_CORE_CALL(0, rx_rule_backwards_reset(rule));
    } else {
        SU32(rule, 0xc, 0);
    }
    if (was != U32(rule, 0xc)) race_driver_event(U32(rule, 4), rule);
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x00519990, rx_rule_backwards_update)

/* ---- RuleOfftrackTimePenalty -------------------------------------------------------------------------------------------------
 * Rule: +0x08 enabled, +0x0c (cleared while on track), +0x10 on track, +0x14 left at the finish segment, +0x18 off-track handled,
 * +0x1c / +0xd0 time penalty this lap / total, +0x20 the running penalty of this excursion (time off track beyond 1.5 s), +0x24 /
 * +0x28 clock when the excursion started / ended, +0x30 / +0x50 / +0x40 the section, node and position where the vehicle left the
 * track (+0x80 / +0x84 / +0x60 copies, +0x70 the driver position at return, +0x88 the section), +0x54 the shortcut penalty,
 * +0xc0 .. +0xcc the last node point ahead, +0xd4 distance ridden off track, +0xd8 no time penalty. Track flags per node and vehicle
 * kind (node +0x68 + 4 * k): 0x1000 = airborne allowed (more than 8 units above the vehicle is not "on" it), 0x10000 / 0x20000 =
 * shortcut markers on either side. */

/* 0051b690 (EAX = node a, ESI = node b, ECX = position near b, args: b's section, position near a; RET 8) -> xmm0: the distance
 * along the track from the position near b (projected on b's segment in the ground plane) forward to the position near a (projected
 * on a's segment): the rest of b's segment, the whole segments between (+0x4f0, following each section's first next section) and
 * the part of a's segment. The straight distance when a == b. */
float rx_track_distance_between(rx_addr a, rx_addr b, rx_addr pos_b, rx_addr section, rx_addr pos_a) {
    if (b == a) {
        const float dz = F32(pos_a, 8) - F32(pos_b, 8), dy = F32(pos_a, 4) - F32(pos_b, 4), dx = F32(pos_a, 0) - F32(pos_b, 0);
        return sqrtf((dz * dz + dy * dy) + dx * dx);
    }
    const float ax = F32(a, 0), az = F32(a, 8);
    rx_addr bn = U32(b, 0x64);
    const float ex = F32(bn, 0) - F32(b, 0), ez = F32(bn, 8) - F32(b, 8);
    const float t = rx_divs((F32(pos_b, 8) - F32(b, 8)) * ez + (F32(pos_b, 0) - F32(b, 0)) * ex, ez * ez + ex * ex);
    const float rx_ = rx_subs(F32(bn, 0), rx_adds(rx_muls(ex, t), F32(b, 0)));
    const float rz = rx_subs(F32(bn, 8), rx_adds(rx_muls(ez, t), F32(b, 8)));
    float acc = rx_sqrts(rx_adds(rx_muls(rz, rz), rx_muls(rx_, rx_)));
    rx_addr node = U32(b, 0x64);
    for (;;) {
        for (; node != 0; node = U32(node, 0x64)) {
            if (node == a) goto done;
            acc = rx_adds(F32(node, 0x4f0), acc);
        }
        if (node == a) break;
        section = U32(U32(section, 8), 0);
        node = U32(section, 0);
        if (node == a) break;
    }
done:;
    rx_addr an = U32(a, 0x64);
    const float fz = F32(an, 8) - az, fx = F32(an, 0) - ax;
    const float u = rx_divs((F32(pos_a, 8) - az) * fz + (F32(pos_a, 0) - ax) * fx, fz * fz + fx * fx);
    const float gz = rx_subs(rx_adds(rx_muls(fz, u), az), az), gx = rx_subs(rx_adds(rx_muls(fx, u), ax), ax);
    return rx_adds(rx_sqrts(rx_adds(rx_muls(gz, gz), rx_muls(gx, gx))), acc);
}
RX_FUNCTION(0x0051b690, rx_track_distance_between)

/* 0051b890 (ESI = node b, EAX = position near a, ECX = b's section, args: rule, position near b, node a; RET 0xc) -> xmm0: the
 * shortcut penalty in seconds for the track distance skipped from b forward to a (0051b690) beyond 77 units (0x9ca0e0): the
 * distance over the speed of the activity kind (+0x23c: 0 -> 88, 1 -> 66, otherwise 51.33 units/s). */
float rx_offtrack_shortcut_penalty(rx_addr rule, rx_addr pos_b, rx_addr a, rx_addr b, rx_addr pos_a, rx_addr section) {
    const float d = rx_track_distance_between(a, b, pos_b, section, pos_a);
    if (F32(0, 0x9ca0e0) /* 77 */ >= d) return 0.0f;
    const uint32_t kind = U32(U32(U32(U32(rule, 4), 0x74), 0x94), 0x23c);
    float speed = 51.3331f /* 0x9358cc */;
    if (kind == 0) speed = 87.9996f /* 0x9779d0 */;
    if (kind == 1) speed = 65.9997f /* 0x97774c */;
    return rx_divs(d, speed);
}
RX_FUNCTION(0x0051b890, rx_offtrack_shortcut_penalty)

/* 0051b9e0 (EAX = from node, ECX = position, args: rule, from's section, to node; RET 0xc) -> EAX: for a local human, the number
 * of shortcut markers between two nodes (the from node's side nearer the position: its +0x40 / +0x48 edge -> 0x10000, else
 * 0x20000), the to node included. */
int rx_offtrack_count_markers(rx_addr rule, rx_addr from, rx_addr pos, rx_addr section, rx_addr to) {
    if (from == to) return 0;
    rx_addr player = U32(U32(rule, 4), 0x74);
    if (U32(player, 4) == 0 || U32(player, 0x10) != 0) return 0;
    const float px = F32(pos, 0), pz = F32(pos, 8);
    const float ax = F32(from, 0x40) - px, az = F32(from, 0x48) - pz, bx = F32(from, 0x30) - px, bz = F32(from, 0x38) - pz;
    const float da = sqrtf(ax * ax + az * az), db = sqrtf(bx * bx + bz * bz);
    const uint32_t mask = da > db ? 0x10000 : 0x20000;
    rx_addr vehicle = U32(U32(U32(rule, 4), 0x74), 0x1ac);
    const uint32_t k = U32(vehicle, 0x198) + 4 * U32(vehicle, 0x194);
    int count = 0;
    rx_addr node = U32(from, 0x64);
    for (;;) {
        for (; node != 0; node = U32(node, 0x64)) {
            if (node == to) goto done;
            if (U32(node, 0x68 + 4 * k) & mask) count++;
        }
        if (node == to) break;
        section = U32(U32(section, 8), 0);
        node = U32(section, 0);
        if (node == to) break;
    }
done:
    if (U32(to, 0x68 + 4 * k) & mask) count++;
    return count;
}
RX_FUNCTION(0x0051b9e0, rx_offtrack_count_markers)

/* 0051b080 (EDI = rule): too far off track: reset the player (reason 3, unless wrecked or game mode 2) and restart the excursion. */
void rx_offtrack_reset(rx_addr rule) {
    rx_addr d = U32(rule, 4);
    SU32(rule, 0x14, 0);
    rx_addr player = U32(d, 0x74);
    if (U32(U32(player, 0x1ac), 0x280) == 0 && U32(0, 0xa942b4) != 2) rx_player_reset_offtrack(player);
    SF32(rule, 0x54, 0.0f);
    SF32(rule, 0xd4, 0.0f);
    SF32(rule, 0x28, F32(U32(rule, 4), 0x130));
}
RX_FUNCTION(0x0051b080, rx_offtrack_reset)

/* 0051b8f0 (EAX = rule): the vehicle left the track: remember where (section, node, the driver's last position, the clock), whether
 * at the finish segment (track +0x9c), and the last node point within the track window ahead of the progress node; report it. */
void rx_offtrack_left_track(rx_addr rule) {
    if (U32(rule, 0x10) == 0 || U32(rule, 0x18) != 0) return;
    rx_addr d = U32(rule, 4);
    rx_addr finish = U32(U32(d, 0x10), 0x9c);
    SU32(rule, 0x14, (U32(d, 0x18) == finish || U32(d, 0x1c) == finish) ? 1 : 0);
    SU32(rule, 0x30, U32(d, 0x14));
    SU32(rule, 0x50, U32(d, 0x18));
    SF32(rule, 0x40, F32(d, 0x80)); SF32(rule, 0x44, F32(d, 0x84)); SF32(rule, 0x48, F32(d, 0x88));
    d = U32(rule, 4);
    SF32(rule, 0x24, F32(d, 0x130));
    float left = F32(U32(d, 0x10), 0x1b8 + 4 * U32(U32(U32(d, 0x74), 0x1ac), 0x194));
    for (rx_addr n = U32(d, 0x90); n != 0 && left > 0.0f; n = U32(n, 0x64)) {
        left = left - F32(n, 0x4f0);
        SF32(rule, 0xc0, F32(n, 0)); SF32(rule, 0xc4, F32(n, 4)); SF32(rule, 0xc8, F32(n, 8));
        SF32(rule, 0xcc, 1.0f /* 0x931f24 */);
    }
    SU32(rule, 0x10, 0);
    race_driver_event(d, rule);
}
RX_FUNCTION(0x0051b8f0, rx_offtrack_left_track)

/* The segment found does not count while the flags node allows air (0x1000) and the found node is more than 8 units above the
 * vehicle (the original tests the driver's own node's flags in the on-track check, the found node's in the return check). */
static int above_airborne_node(rx_addr rule, rx_addr flags_node, rx_addr node) {
    rx_addr vehicle = U32(U32(U32(rule, 4), 0x74), 0x1ac);
    const uint32_t k = U32(vehicle, 0x198) + 4 * U32(vehicle, 0x194);
    return (U32(flags_node, 0x68 + 4 * k) & 0x1000) != 0 && F32(node, 4) - F32(vehicle, 0xc4) > 8.0f /* 0x9322b8 */;
}

/* 0051b130 RuleOfftrackTimePenalty::vf5 (ECX = rule, arg = dt; RET 4): off-track detection and its penalties. On track: the
 * vehicle must stay in a segment within the track window ahead (5x) / behind (1.25x) of the driver's segment, else it left the
 * track (0051b8f0). Off track: the time beyond 1.5 s accrues as penalty (+0x20, unless +0xd8 / the career setting / finished),
 * the distance ridden off track (+0xd4) beyond the window resets the player (0051b080); back in a segment (searching up to the
 * finish node): in the same segment -> fine; behind -> reset if it came back further than the window; ahead -> a shortcut: the
 * skipped distance as time (0051b890), a reset when too long or with more than 5 markers passed (3..5 markers: that many seconds),
 * and the driver's place moves to the new segment. Every change is reported to the driver (vf9). */
void rx_rule_offtrack_update(rx_addr rule, float dt) {
    rx_addr d = U32(rule, 4), player = U32(d, 0x74), vehicle = U32(player, 0x1ac);
    if (vehicle == 0) return;
    if (U32(U32(player, 0x1b4), 0xaf00) != 0) { SF32(rule, 0x20, 0.0f); return; }
    const float window = F32(U32(d, 0x10), 0x1b8 + 4 * U32(vehicle, 0x194));
    race_track_query q = {vehicle + 0xc0, U32(d, 0x14), U32(d, 0x18), 0, window * 5.0f /* 0x935890 */,
                          window * F32(0, 0x9c4068) /* 1.25 */, 0, 1};
    rx_addr sec = 0, node = 0;
    if (U32(rule, 0x10) != 0) {
        SF32(rule, 0xc, 0.0f);
        if (race_track_find(&q, 1, &sec, &node) != 0 && !above_airborne_node(rule, U32(U32(rule, 4), 0x18), node)) {
            SU32(rule, 0x18, 0);
            return;
        }
        if (U32(rule, 8) != 0) rx_offtrack_left_track(rule);
        return;
    }
    if (U8(rule, 0xd8) == 0 && U8(U32(U32(d, 0x74), 0x1b4), 0x5bb64) != 0 && U32(d, 0x70) == 0)
        SF32(rule, 0x20, F32(d, 0x130) - (F32(rule, 0x24) + 1.5f /* 0x9322a4 */));
    else
        SF32(rule, 0x20, 0.0f);
    RACE_FRAME16_ENTER(F, 0x10);
    vehicle_velocity(U32(U32(d, 0x74), 0x1ac), F);
    const float vx = F32(F, 0), vz = F32(F, 8);
    RACE_FRAME16_LEAVE(F);
    const float ridden = sqrtf(vz * vz + vx * vx) * dt + F32(rule, 0xd4);
    SF32(rule, 0xd4, ridden);
    d = U32(rule, 4);
    rx_addr track = U32(d, 0x10);
    if (ridden > F32(track, 0x1b8 + 4 * U32(U32(U32(d, 0x74), 0x1ac), 0x194))) {
        rx_offtrack_reset(rule);
        race_driver_event(U32(rule, 4), rule);
        return;
    }
    q.stop = U32(track, 0x9c);
    q.use_stop = 1;
    SU32(rule, 0x10, 1);
    const int r = race_track_find(&q, 1, &sec, &node);
    if (r == 0) {
        SU32(rule, 0x10, 0);
    } else if (above_airborne_node(rule, node, node)) {
        SU32(rule, 0x10, 0);
    } else if (r == 1) {
        rx_addr v = U32(U32(d, 0x74), 0x1ac);
        SF32(d, 0x80, F32(v, 0xc0)); SF32(d, 0x84, F32(v, 0xc4)); SF32(d, 0x88, F32(v, 0xc8));
    } else if (r == 2) {
        rx_addr v = U32(U32(d, 0x74), 0x1ac);
        if (rx_track_distance_between(U32(rule, 0x50), node, v + 0xc0, sec, rule + 0x40) > window) {
            rx_offtrack_reset(rule);
            race_driver_event(U32(rule, 4), rule);
            return;
        }
    } else if (r == 3) {
        rx_addr v = U32(U32(d, 0x74), 0x1ac);
        float pen = rx_offtrack_shortcut_penalty(rule, rule + 0x40, node, U32(rule, 0x50), v + 0xc0, U32(rule, 0x30));
        SF32(rule, 0x54, pen);
        if (pen * 51.3331f /* 0x9358cc */ > F32(U32(d, 0x10), 0x1b8 + 4 * U32(U32(U32(d, 0x74), 0x1ac), 0x194))) {
            rx_offtrack_reset(rule);
            race_driver_event(U32(rule, 4), rule);
            return;
        }
        SF32(rule, 0x54, 0.0f);
        const int markers = rx_offtrack_count_markers(rule, U32(rule, 0x50), rule + 0x40, U32(rule, 0x30), node);
        if (markers > 5) {
            rx_offtrack_reset(rule);
            race_driver_event(U32(rule, 4), rule);
            return;
        }
        if (markers > 2) SF32(rule, 0x54, (float)markers);
        const float p54 = F32(rule, 0x54);
        SF32(rule, 0x1c, p54 + F32(rule, 0x1c));
        SF32(rule, 0xd0, p54 + F32(rule, 0xd0));
        if (!rx_player_is_resetting(U32(d, 0x74))) {
            SU32(d, 0x18, node);
            SU32(U32(rule, 4), 0x1c, U32(node, 0x64));
            rx_addr dd = U32(rule, 4);
            SU32(dd, 0x90, U32(dd, 0x1c));
            dd = U32(rule, 4);
            rx_addr v2 = U32(U32(dd, 0x74), 0x1ac);
            SF32(dd, 0x80, F32(v2, 0xc0)); SF32(dd, 0x84, F32(v2, 0xc4)); SF32(dd, 0x88, F32(v2, 0xc8));
            if (sec != U32(U32(rule, 4), 0x14)) {
                rx_addr adv = rx_driver_track_advance_rule(U32(rule, 4));
                if (adv != 0) {
                    SU32(adv, 0x10, sec);
                    if (U32(sec, 0x14) != 0) rx_rule_track_advance_section_changed(adv);
                    else SF32(adv, 0xc, rx_bf(0x7f7fffff) /* 0x977494 */);
                }
                SU32(U32(rule, 4), 0x14, sec);
            }
        }
    }
    if (U32(rule, 0x10) == 0) return;
    /* back on track */
    d = U32(rule, 4);
    const float run = F32(rule, 0x20);
    SU32(rule, 0x18, 0);
    SU32(rule, 0x14, 0);
    SF32(rule, 0x28, F32(d, 0x130));
    SF32(rule, 0xd4, 0.0f);
    SF32(rule, 0x1c, run + F32(rule, 0x1c));
    SF32(rule, 0xd0, run + F32(rule, 0xd0));
    if (U8(U32(U32(d, 0x74), 0x1b4), 0x5bb64) == 0) SF32(rule, 0x20, 0.0f);
    SU32(rule, 0x80, U32(rule, 0x50));
    SU32(rule, 0x84, U32(d, 0x18));
    SF32(rule, 0x60, F32(rule, 0x40)); SF32(rule, 0x64, F32(rule, 0x44)); SF32(rule, 0x68, F32(rule, 0x48));
    d = U32(rule, 4);
    SF32(rule, 0x70, F32(d, 0x80)); SF32(rule, 0x74, F32(d, 0x84)); SF32(rule, 0x78, F32(d, 0x88));
    SU32(rule, 0x88, U32(rule, 0x30));
    race_driver_event(U32(rule, 4), rule);
}
RX_FUNCTION(0x0051b130, rx_rule_offtrack_update)

/* 0051b010 (EAX = rule): RuleOfftrackTimePenalty reset (enabled, on track, no penalties, no excursion). */
void rx_rule_offtrack_reset_state(rx_addr rule) {
    SU32(rule, 8, 1); SU32(rule, 0x10, 1); SU32(rule, 0x14, 0);
    SF32(rule, 0x1c, 0.0f); SF32(rule, 0x20, 0.0f); SF32(rule, 0x54, 0.0f); SF32(rule, 0xd0, 0.0f); SF32(rule, 0xc, 0.0f);
    SF32(rule, 0x28, 0.0f);
    SU32(rule, 0x30, 0); SU32(rule, 0x50, 0);
    SF32(rule, 0x40, 0.0f); SF32(rule, 0x44, 0.0f); SF32(rule, 0x48, 0.0f);
    SU32(rule, 0x80, 0); SU32(rule, 0x84, 0); SU32(rule, 0x88, 0);
    SU8(rule, 0xd8, 0);
    SF32(rule, 0xd4, 0.0f);
}
RX_FUNCTION(0x0051b010, rx_rule_offtrack_reset_state)

/* ---- RuleCannonball ---------------------------------------------------------------------------------------------------------
 * A tunnel / cannon section: Rule +0x0c a looping sound, +0x10 state (0 outside, 1 inside, 2 leaving), +0x14 progress 0..1.
 * The cannon's own spline is the track object at the game's +0x5a54 / +0x20c8 (by the active view set) + 0x21e10. */

/* 00552870 (EAX = player) -> EAX: a local human player? */
int rx_player_is_local_human(rx_addr player) { return U32(player, 4) != 0 && U32(player, 0x10) == 0; }
RX_FUNCTION(0x00552870, rx_player_is_local_human)

/* 00519fa0 (EAX = position, EDI = out point (or 0)) -> EAX: is the position on the inner side of the cannon spline's first edge?
 * Nearest node of the spline (00538440), its segment (the previous one when the position projects before it), the side of the
 * edge line (node +0x10 points) the position is on; the point of the edge segment nearest to the position goes to out (00407430).
 * Frame: push ebp; and esp, -16; sub esp, 0x28; push ebx, esi; the nearest-node record at +0x10 (section, node). */
int rx_cannonball_inside(rx_addr pos, rx_addr out_point) {
    rx_addr ctx = U32(0, 0xa95844), track = 0;
    if (U32(U32(ctx, 0x20), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x20c8);
    RACE_FRAME_EBP(S, 16, 0x28, 2);
    RACE_CORE_CALL(3, rx_track_nearest_node_all(track + 0x21e10, 0, 0, pos, S + 0x10));
    rx_addr sec = U32(S, 0x10), node = U32(S, 0x14);
    if (node == 0) { RACE_FRAME_END(S); return 0; }
    rx_addr next = U32(node, 0x64);
    if (next == 0) next = U32(U32(U32(U32(sec, 8), 0), 0), 0x64);
    const float ex = F32(next, 0) - F32(node, 0), ez = F32(next, 8) - F32(node, 8);
    const float t = rx_divs(ez * (F32(pos, 8) - F32(node, 8)) + (F32(pos, 0) - F32(node, 0)) * ex, ez * ez + ex * ex);
    rx_addr a = node, b = next;
    if (0.0f > t) {
        b = node;
        a = U32(node, 0x60);
        if (a == 0) a = U32(U32(U32(U32(sec, 8), 0), 4), 0x60);
    }
    const float qx = F32(pos, 0) - F32(a, 0x10), qz = F32(pos, 8) - F32(a, 0x18);
    const float dx = F32(b, 0x10) - F32(a, 0x10), dz = F32(b, 0x18) - F32(a, 0x18);
    if (out_point != 0) RACE_CORE_CALL(1, (void)rx_closest_point_on_segment(out_point, pos, a + 0x10, b + 0x10));
    RACE_FRAME_END(S);
    return dz * qx - qz * dx > 0.0f;
}
RX_FUNCTION(0x00519fa0, rx_cannonball_inside)

/* The cannon's sound and fade calls are in race_sound.c (0080ade0 stop, 0080b410 volume, 00807170 / 00807130 music channels). */

/* 00519ed0 (ECX = fade state) -> EAX: the fade layer in use: by the state word (0 / default +0x10, 1 +0xb0, 2 +0x70). */
static rx_addr fade_layer_of(rx_addr fade) {
    switch (U32(fade, 0)) {
    case 1: return fade + 0xb0;
    case 2: return fade + 0x70;
    default: return fade + 0x10;
    }
}
rx_addr rx_fade_layer(rx_addr fade) { return fade_layer_of(fade); }
RX_FUNCTION(0x00519ed0, rx_fade_layer)

/* The fade record of a local player's viewport (0 if none). */
static rx_addr player_viewport(rx_addr player) {
    const int32_t idx = I32(player, 8);
    if (idx < 0) return 0;
    rx_addr ctx = U32(0, 0xa95844);
    return U32(U32(ctx, 0x1c + 4 * U32(ctx, 4)), 0x19b8 + 4 * (uint32_t)idx);
}

/* 0051a110 (EAX = position, cdecl out point (or 0); RET 0) -> EAX: has the position reached the exit of the cannon spline?
 * Nearest node of the spline (00538440), the segment it is on (the previous one when the position projects before the node); the
 * projection of the position on that segment (x, 0, z) goes to out; the result: the position is on the inner side of the segment's
 * line (node positions +0 / +8). Frame: push ebp; and esp, -16; sub esp, 0x14; push ebx, esi, edi; the nearest-node record is
 * the callee's EDX out at A - 0xc (section +0, node +4). */
int rx_cannonball_exit(rx_addr pos, rx_addr out_point) {
    rx_addr ctx = U32(0, 0xa95844), track = 0;
    if (U32(U32(ctx, 0x20), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x20c8);
    RACE_FRAME_EBP(S, 16, 0x14, 3);
    const rx_addr R = S + 0x14;
    RACE_CORE_CALL(3, rx_track_nearest_node_all(track + 0x21e10, 0, 0, pos, R));
    const rx_addr sec = U32(R, 0), node = U32(R, 4);
    if (node == 0) { RACE_FRAME_END(S); return 0; }
    rx_addr next = U32(node, 0x64);
    if (next == 0) next = U32(U32(U32(U32(sec, 8), 0), 0), 0x64);
    const float nx = F32(node, 0), nz = F32(node, 8), px = F32(pos, 0), pz = F32(pos, 8);
    const float ex = rx_subs(F32(next, 0), nx), ez = rx_subs(F32(next, 8), nz);
    const float qz = rx_subs(pz, nz), qx = rx_subs(px, nx);
    float t = rx_divs(rx_adds(rx_muls(ez, qz), rx_muls(qx, ex)), rx_adds(rx_muls(ez, ez), rx_muls(ex, ex)));
    float ox = rx_adds(rx_muls(ex, t), nx), oz = rx_adds(rx_muls(ez, t), nz);
    rx_addr a = node, b = next;
    if (0.0f > t) {
        b = node;
        a = U32(node, 0x60);
        if (a == 0) a = U32(U32(U32(U32(sec, 8), 0), 4), 0x60);
        const float ax = F32(a, 0), az = F32(a, 8);
        const float dx = rx_subs(nx, ax), dz = rx_subs(nz, az);
        const float u = rx_muls(rx_subs(px, ax), dx), v = rx_muls(rx_subs(pz, az), dz);
        t = rx_divs(rx_adds(v, u), rx_adds(rx_muls(dz, dz), rx_muls(dx, dx)));
        ox = rx_adds(rx_muls(dx, t), ax); oz = rx_adds(rx_muls(dz, t), az);
    }
    if (out_point != 0) { SF32(out_point, 0, ox); SF32(out_point, 4, 0.0f); SF32(out_point, 8, oz); }
    const float ax = F32(a, 0), az = F32(a, 8);
    const float cx = F32(b, 0) - ax, cz = F32(b, 8) - az;
    const float side = cz * (px - ax) - (pz - az) * cx;
    RACE_FRAME_END(S);
    return side > 0.0f;
}
RX_FUNCTION(0x0051a110, rx_cannonball_exit)

/* 0051a690 (EDI = rule, EBP = the caller's, passed on to the volume call; no result): enters the cannon section: for a local
 * human player the viewport's state-1 .. fade layer is cut (+4 = 0) and the warning sound loop is created (once: rule +0xc) from
 * the world's path name under the tag "CannonballWarning", silent, and started. Frame: sub esp, 0x38 (the name buffer at +4);
 * push esi. */
void rx_cannonball_enter(rx_addr rule, uint32_t ebp) {
    RACE_FRAME_PLAIN(S, 0x38, 1);
    const rx_addr buf = S + 4;
    rx_addr player = U32(U32(rule, 4), 0x74);
    if (U32(player, 4) == 0 || U32(player, 0x10) != 0) { RACE_FRAME_END(S); return; }
    rx_addr vp = player_viewport(player);
    if (vp != 0) {
        rx_addr layer = fade_layer_of(U32(vp, 0x70));
        if (layer != 0) SU32(layer, 4, 0);
    }
    if (U32(rule, 0xc) == 0) {
        RACE_CORE_CALL(1, rx_sound_world_path(buf));
        rx_addr ev = 0;
        if (U32(0, 0xa957d4) != 0) RACE_CORE_CALL(2, ev = rx_world_event_create(U32(0, 0xa957d4), buf, 0x0093ecc8 /* "CannonballWarning" */));
        SU32(rule, 0xc, ev);
        if (ev != 0) {
            RACE_CORE_CALL(0, rx_sound_set_volume(ev, 0.0f, ebp));
            RACE_CORE_CALL(3, rx_world_event_play(U32(rule, 0xc), 0, 0, 0));
        }
    }
    RACE_FRAME_END(S);
}
RX_FUNCTION(0x0051a690, rx_cannonball_enter)

/* 0054f150 (ECX = player, EDI = point; no result): the player is launched out of the cannon toward the point: the vehicle's position
 * (vf17) to the point gives the flat heading (normalised when it has length), its cross with up the side vector; both feed only
 * locals the original never reads afterwards. Then, unless the clip recorder state 0xa942b4 is 2, the player is reset in place
 * (00550920). Frame: push ebp; and esp, -16; sub esp, 0x4c; push esi; the position out is at +0x40. */
void rx_player_launch(rx_addr player, rx_addr point) {
    RACE_FRAME_EBP(S, 16, 0x4c, 1);
    const rx_addr pos = S + 0x40;
    rx_addr v = U32(player, 0x1ac);
    typedef void (*pos_fn)(rx_addr, rx_addr);
    rx_addr fn = race_vfn(v, 17);
    pos_fn f = RX_FN(fn, pos_fn);
    if (f) RACE_CORE_CALL(1, f(v, pos)); else race_thiscall(fn, v, 1, pos, 0, 0, 0);
    const float dx = F32(point, 0) - F32(pos, 0), dz = F32(point, 8) - F32(pos, 8);
    SF32(S, 0x10, dx); SF32(S, 0x18, dz);
    const float len2 = dz * dz + dx * dx;
    if (len2 > 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        SF32(S, 0x20, dx * inv); SF32(S, 0x24, inv * 0.0f); SF32(S, 0x28, dz * inv);
    }
    SF32(S, 0x30, 0.0f); SF32(S, 0x34, 1.0f); SF32(S, 0x38, 0.0f);
    {   /* Vec3_Cross (a = heading at +0x20, b = up at +0x30) -> +0x10 (the local, never read again: the original leaves it) */
        const float ax = F32(S, 0x20), ay = F32(S, 0x24), az = F32(S, 0x28), bx = F32(S, 0x30), by = F32(S, 0x34), bz = F32(S, 0x38);
        SF32(S, 0x10, az * by - ay * bz); SF32(S, 0x14, ax * bz - bx * az); SF32(S, 0x18, bx * ay - ax * by);
    }
    if (U32(0, 0xa942b4) != 2) RACE_CORE_CALL(0, rx_player_reset_in_place(player));
    RACE_FRAME_END(S);
}
RX_FUNCTION(0x0054f150, rx_player_launch)

static void cannon_sounds_off(rx_addr rule, uint32_t ebp) {
    if (U32(rule, 0xc) != 0) { RACE_CORE_CALL(0, rx_sound_stop(U32(rule, 0xc), ebp)); SU32(rule, 0xc, 0); }
    RACE_CORE_CALL(1, rx_channel_fade(0x00d67af0, 0.001f /* 0x97743c */));
    RACE_CORE_CALL(1, rx_channel_fade(0x00d67b98, 0.001f));
}

/* 0051a300 RuleCannonball::vf5 (ECX = rule, arg = dt (unused); RET 4): entering the cannon section starts it (0051a690); inside,
 * the progress (squared distance from the entry point over that of the exit point, at most 1) fades the music channels and the
 * screen to white for a local human; at the exit the player is launched (0054f150) and the driver is told (vf9); leaving the
 * section stops it. Frame: push ebp; and esp, -16; sub esp, 0x34; push ebx esi edi (position at +0x10, exit point +0x20, entry
 * edge point +0x30). */
void rx_rule_cannonball_update(rx_addr rule, float dt) {
    (void)dt;
    rx_addr player = U32(U32(rule, 4), 0x74);
    if (U32(player, 0x1ac) == 0 || U32(rule, 8) == 0) return;
    RACE_FRAME_EBP(S, 16, 0x34, 3);
    const uint32_t ebp = S_sp - 4;
    const rx_addr F = S + 0x10;
    rx_addr v = U32(player, 0x1ac);
    typedef void (*pos_fn)(rx_addr, rx_addr);
    rx_addr fn = race_vfn(v, 17);
    pos_fn f = RX_FN(fn, pos_fn);
    if (f) RACE_CORE_CALL(1, f(v, F)); else race_thiscall(fn, v, 1, F, 0, 0, 0);
    SF32(rule, 0x14, 0.0f);
    rx_addr d = U32(rule, 4);
    if (U32(U32(U32(d, 0x74), 0x1ac), 0x280) != 0) goto out;
    /* PORT ADDITION (free_ride_borders.h): Free Ride without borders: the cannonball spline is the Free Ride's boundary (the white-out and the
     * put-back at its centre line); for this player it is never entered, and a section already entered (the option turned on inside it) is
     * left as the original leaves it (the sounds off, the white layer cut) */
    const uint32_t open = rx_frb_for(player);
    switch (U32(rule, 0x10)) {
    case 0: {
        int in_;
        RACE_CORE_CALL(0, in_ = rx_cannonball_inside(F, 0));
        if (in_ && open) rx_frb_stats_value.cannonball_skipped++;
        else if (in_) { RACE_CORE_CALL(0, rx_cannonball_enter(rule, ebp)); SU32(rule, 0x10, 1); }
        break;
    }
    case 2:
        SU32(rule, 0x10, 0);
        break;
    case 1: {
        int in_ = 0;
        if (!open) RACE_CORE_CALL(0, in_ = rx_cannonball_inside(F, F + 0x20));
        if (!in_) {
            SU32(rule, 0x10, 0);
            if (!rx_player_is_local_human(U32(d, 0x74))) break;
            cannon_sounds_off(rule, ebp);
            rx_addr vp = player_viewport(U32(U32(rule, 4), 0x74));
            if (vp != 0 && U32(vp, 0x70) + 0xb0 != 0) SU32(U32(vp, 0x70) + 0xb0, 4, 0);
            break;
        }
        {
            int reached;
            RACE_CORE_CALL(1, reached = rx_cannonball_exit(F, F + 0x10));
            if (reached != 0) {
                if (!rx_player_is_local_human(U32(d, 0x74))) break;
                cannon_sounds_off(rule, ebp);
                rx_addr vp = player_viewport(U32(U32(rule, 4), 0x74));
                if (vp != 0) {
                    rx_addr layer;
                    RACE_CORE_CALL(0, layer = rx_fade_layer(U32(vp, 0x70)));
                    if (layer != 0) SU32(layer, 4, 0);
                }
                RACE_CORE_CALL(0, rx_player_launch(U32(U32(rule, 4), 0x74), F + 0x10));
                RACE_CORE_CALL(1, race_driver_event(U32(rule, 4), rule));
                SU32(rule, 0x10, 0);
                break;
            }
        }
        {
            const float ax = F32(F, 0x20), az = F32(F, 0x28);
            const float ex = F32(F, 0x10) - ax, px = F32(F, 0) - ax, pz = F32(F, 8) - az, ez = F32(F, 0x18) - az;
            float t = rx_divs(pz * pz + px * px, ez * ez + ex * ex);
            if (t > 1.0f /* 0x931f24 */) t = 1.0f;
            SF32(rule, 0x14, t);
            if (!rx_player_is_local_human(U32(d, 0x74))) break;
            if (U32(rule, 0xc) != 0) RACE_CORE_CALL(0, rx_sound_set_volume(U32(rule, 0xc), t, ebp));
            const float w = 1.0f - F32(rule, 0x14), vol = w * w;
            RACE_CORE_CALL(0, rx_channel_set_volume(0x00d67af0, vol));
            RACE_CORE_CALL(0, rx_channel_set_volume(0x00d67b98, vol));
            rx_addr vp = player_viewport(U32(U32(rule, 4), 0x74));
            if (vp == 0) break;
            SU32(U32(vp, 0x70), 0, 1);
            rx_addr layer = U32(vp, 0x70) + 0xb0;
            if (layer == 0) break;
            if (U32(layer, 4) == 0) {
                SF32(layer, 0x20, F32(0, 0x9ca220)); SF32(layer, 0x24, F32(0, 0x9ca224)); SF32(layer, 0x28, F32(0, 0x9ca228));
                SF32(layer, 0x2c, 1.0f);
                SU32(layer, 0x30, 1); SU32(layer, 0x34, 2); SU32(layer, 4, 1);
                SF32(layer, 0x10, 0.0f); SU32(layer, 8, 0); SF32(layer, 0x18, 0.0f);
            }
            const float u = F32(rule, 0x14) - 1.0f, u2 = u * u;
            SF32(layer, 0x1c, 1.0f - u2 * u2);
        }
        break;
    }
    default:
        break;
    }
out:
    RACE_FRAME_END(S);
}
RX_FUNCTION(0x0051a300, rx_rule_cannonball_update)
