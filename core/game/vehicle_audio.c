/* vehicle_audio.c -- the simulation values the original's vehicle sound layer reads (include/rx_vehicle_audio.h) and the sound-layer cues
 * that gameplay code issues into the product's audio boundary (w-audio, 2026-10-27).
 *
 * The original's VehicleAudioManager (00680ec0, 12 vehicle slots registered by 00680b10) builds a telemetry snapshot per vehicle every
 * frame with the vehicle's vf52 and runs the engine emitter (VehicleEngineAudio_Update 00685610) on it. The product has no sound layer
 * (core/plan/QUEUE.md "chain": the audio boundary), so the host runs the emitter (Unity/Reflex/Assets/Scripts/OriginalWorld/
 * OriginalEngineAudio.cs) and this file hands it the same values, read directly where each vf52 reads them:
 *   class  simulation  rpm (rad/s)  max     idle    gear-1  clutch (vf50)          throttle / control2         load    contacts
 *   MX     V+0x16d4    +0x8abc      +0x5014 +0x505c +0x8ac0 [S+0x5c30]+0x84 == 2   [S+0x5c30]+0 / +4           +0x8ad8 +0x8894 +0x89e4
 *   ATV    V+0x1700    +0x96ec      +0x4bfc +0x4c44 +0x96f0 +0x597c == 2           +0x5880 / +0x5884           +0x9708 +0x94e4 +0x9634 +0x9244 +0x9394
 *   UTV    V+0x1540    +0x865c      +0x4d4c +0x4d94 +0x8660 +0x56fc == 2           +0x5600 / +0x5604           +0x8678 +0x8424 +0x8574 +0x8184 +0x82d4
 *   Buggy  V+0x15c0    +0x864c      +0x4d4c +0x4d94 +0x8650 +0x56ec == 2           +0x55f0 / +0x55f4           +0x8668 (contacts - 0x10)
 *   Truck  V+0x15d0    +0x865c      +0x4d4c +0x4d94 +0x8664 +0x56fc == 2           +0x5600 / +0x5604           +0x867c (as UTV)
 * (vf47 rx_moto_engine_rpm / rx_atv_engine_rpm / rx_utv_engine_rpm / rx_buggy_engine_rpm / rx_truck_engine_rpm: rad/s * 9.55f in x87, stored
 * as float by vf52; vf48 / vf42 / vf50 the ports in race_misc_vehicle.c, vehicle_atv_*.c, vehicle_utv_runtime.c, vehicle_fourwheel_leaves.c;
 * the four-wheel and ATV snapshot words in fourwheel_snapshot.c / vehicle_atv_telemetry.c; the idle rpm is the engine step's minimum,
 * bike_force.c / vehicle_atv_physics_engine.c / utv_engine.c / buggy_engine.c / truck_engine.c.)
 *
 * Product only, reads only: nothing here writes guest memory, so the oracle's memory comparisons are unaffected (the oracle build gets
 * empty hooks). Single session / thread. */
#include "rx_game.h"
#include "vehicle_audio.h"
#include "engine_startline.h"
#ifndef RX_ORACLE
#include <string.h>
#include <math.h>
#include "rider_cap.h"

_Static_assert(sizeof(rx_vehicle_audio_rider) == 144, "vehicle audio rider ABI size");
_Static_assert(sizeof(rx_vehicle_audio_cue) == 64, "vehicle audio cue ABI size");
_Static_assert(sizeof(rx_vehicle_audio_state) == 5664 && offsetof(rx_vehicle_audio_state, riders_ext) == RX_VEHICLE_AUDIO_V1_SIZE, "vehicle audio ABI size (version 2; version 1 = the first 3360 bytes)");

#define VT_MX    0x0093cd84u   /* RavenVehicleMotocross */
#define VT_ATV   0x0093d0ecu   /* RavenVehicleATV */
#define VT_UTV   0x0093d4d4u   /* RavenVehicleUTV */
#define VT_BUGGY 0x0093d2e4u   /* RavenVehicleBuggy */
#define VT_TRUCK 0x0093cee4u   /* RavenVehicleTrophyTruck */
#define RPM_PER_RAD 9.55f      /* [0x009774d0] */

static struct { uint32_t serial; rx_vehicle_audio_cue ring[RX_VEHICLE_AUDIO_CUES]; } cues;

/* The manager's 12 vehicle slots (VehicleAudioManager [0xd92f00] + 0x1b10 k, the vehicle at + 0x10), as 00680b10 assigns them and
 * 00680d10 frees them (w-sfx). The slot index is the emitter's +0 (the manager set-up 006809f0: *(manager + 0x520 + k * 0x1b10) = k,
 * after the emitter ctor 006848a0 wrote -1), which the engine emitter posts as AIFX for every rider but the local human (00686370):
 * the events' AIFX envelopes (volume x 0.596, pitch down to x 0.917 at 11, low-pass ~8.7 kHz) make the computer riders' engines quieter
 * and each a little detuned. The product runs no sound layer, so the slots are kept here, outside guest memory. */
static rx_addr va_slots[RX_SL_SLOTS];   /* (w-rc-core) 12 in the original manager; slots 12..31 for riders 13..32 */
static void fill_vehicle(rx_addr p, rx_addr v, rx_vehicle_audio_rider* r);

void rx_vehicle_audio_reset(void) { memset(&cues, 0, sizeof cues); memset(va_slots, 0, sizeof va_slots); rx_engine_start_line_reset(); }

/* 00680b10 (manager, vehicle), the slot choice only: none if the vehicle has a slot; else the first free slot (+0x10 == 0); none when all 12 are taken
 * (w-rc-core: RX_SL_SLOTS = 32 host-side slots; slots 12..31 only fill once 12 are taken, so a race of 12 or fewer is unchanged). */
void rx_vehicle_audio_register(rx_addr vehicle) {
    if (!vehicle) return;
    for (int k = 0; k < RX_SL_SLOTS; k++) if (va_slots[k] == vehicle) return;
    for (int k = 0; k < (rx_rc_more() ? RX_SL_SLOTS : 12); k++) if (va_slots[k] == 0) {   /* (w-rc-core) slots 12.. with 13+ players only */
        va_slots[k] = vehicle;
        rx_engine_start_line_tunables();                                   /* 00684c10's keys (the audio init; w-rng) */
        rx_engine_start_line_bind(k, vehicle, I32(vehicle, 0x170));        /* 00685a40 / 006855c0: the emitter's start-line words (w-rng) */
        return;
    }
}
/* 00680d10: the vehicle's slot freed (+0x10 = 0). */
void rx_vehicle_audio_unregister(rx_addr vehicle) {
    if (!vehicle) return;
    for (int k = 0; k < RX_SL_SLOTS; k++) if (va_slots[k] == vehicle) { va_slots[k] = 0; rx_engine_start_line_unbind(k); break; }
}
static int32_t audio_slot_of(rx_addr vehicle) {
    for (int k = 0; k < RX_SL_SLOTS; k++) if (va_slots[k] == vehicle) return k;
    return -1;
}

static void cue_push(int32_t kind, const char* name, size_t n) {
    rx_vehicle_audio_cue* c = &cues.ring[cues.serial % RX_VEHICLE_AUDIO_CUES];
    memset(c, 0, sizeof *c);
    c->serial = ++cues.serial;
    c->kind = kind;
    if (n >= sizeof c->name) n = sizeof c->name - 1;
    if (name && n) memcpy(c->name, name, n);
}

/* 0055d610's name forms: "{cmd}" / "{cmd|arg}" / "{cmd arg}" -> 0067edc0 splits at the first '|' or ' ' and Scenario_AudioCommand
 * (0067ecf0) compares the command case-insensitively; any other name is a scenario sound effect ("Scenarios/All/<name>" unless the name
 * already names an event path, 0078fcf0). The guest string is only read. */
static int ieq(const char* a, size_t n, const char* lit) {
    size_t i = 0;
    for (; i < n && lit[i]; i++) {
        char x = a[i], y = lit[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return i == n && lit[i] == 0;
}
void rx_vehicle_audio_scenario_sound(rx_addr name) {
    if (!name) return;
    char buf[264]; size_t n = 0;
    while (n < sizeof buf - 1) { const char c = (char)U8(name, (uint32_t)n); if (!c) break; buf[n++] = c; }
    buf[n] = 0;
    if (n == 0) return;
    if (buf[0] == '{' && n >= 2 && buf[n - 1] == '}') {
        const char* cmd = buf + 1; size_t len = n - 2, split = len;
        for (size_t i = 0; i < len; i++) if (cmd[i] == '|' || cmd[i] == ' ') { split = i; break; }
        const char* arg = split < len ? cmd + split + 1 : cmd + len; const size_t arglen = split < len ? len - split - 1 : 0;
        /* w-rng: 00681880 / the disable loop of 0067ecf0 run here on the core's start-line state (game/engine_startline.c): the
         * StartLine enter draws the game's LCG, at this point of the frame as in the original */
        if (ieq(cmd, split, "EnableStartLineState")) { rx_engine_start_line_tunables(); rx_engine_start_line_enable(); cue_push(RX_VA_CUE_START_LINE_ENABLE, 0, 0); }
        else if (ieq(cmd, split, "DisableStartLineState")) { rx_engine_start_line_disable(); cue_push(RX_VA_CUE_START_LINE_DISABLE, 0, 0); }
        else if (ieq(cmd, split, "StartScenarioMusic")) cue_push(RX_VA_CUE_SCENARIO_MUSIC_START, arg, arglen);
        else if (ieq(cmd, split, "StopScenarioMusic")) cue_push(RX_VA_CUE_SCENARIO_MUSIC_STOP, 0, 0);
        else cue_push(RX_VA_CUE_OTHER, cmd, split);
    }
    /* a plain sound name is rx_session_scenario_sounds' (w-frscene): not repeated here */
}
void rx_vehicle_audio_pre_exited(void) { rx_engine_start_line_pre_exited(1); cue_push(RX_VA_CUE_PRE_EXITED, 0, 0); }

/* MXGameTickDuringSync::vf0's audio tick (0067dd10 -> VehicleAudioManager_UpdateVehicles 00680ec0 -> VehicleEngineAudio_Update 00685610
 * per slot, slot order) as far as the engine emitters' start line goes (w-rng): each slot's snapshot words from its vehicle (vf52), then
 * the start-line tick (game/engine_startline.c). */
void rx_vehicle_audio_tick(float dt) {
    for (int k = 0; k < RX_SL_SLOTS; k++) {
        const rx_addr v = va_slots[k];
        if (!v) continue;
        rx_vehicle_audio_rider r;
        memset(&r, 0, sizeof r);
        fill_vehicle(0, v, &r);
        rx_sl_input in;
        memset(&in, 0, sizeof in);
        in.underwater = r.underwater; in.drowned = r.drowned; in.wrecked = r.wrecked; in.rpm = r.rpm; in.max_rpm = r.max_rpm;
        rx_engine_start_line_update(k, &in, dt);
    }
}
/* the host's report: the rev one-shot of the slot's emitter finished (the event wrapper's state 7) */
int rx_vehicle_audio_finished(int32_t slot) {
    if (slot < 0 || slot >= RX_SL_SLOTS || !va_slots[slot]) return 1;
    rx_engine_start_line_finished(slot);
    return 0;
}

static float rpm_of(rx_addr sim, uint32_t off) { return (float)((double)F32(sim, off) * (double)RPM_PER_RAD); }

static void fill_rider(rx_addr g, uint32_t slot, rx_vehicle_audio_rider* r) {
    memset(r, 0, sizeof *r);
    r->slot = (int32_t)slot; r->pad = -1; r->engine_size = -1; r->audio_slot = -1;
    const rx_addr p = rx_seat(g + 0x7460, slot);
    const rx_addr v = U32(p, 0x1ac);
    if (!v) return;
    fill_vehicle(p, v, r);
}
/* the vehicle's part (p: its player slot, 0 when the caller has only the vehicle) */
static void fill_vehicle(rx_addr p, rx_addr v, rx_vehicle_audio_rider* r) {
    r->vehicle = v;
    const uint32_t vt = U32(v, 0);
    rx_addr sim = 0;
    uint32_t delta = 0;
    switch (vt) {
    case VT_MX: r->vehicle_class = RX_VA_CLASS_MX; sim = U32(v, 0x16d4); break;
    case VT_ATV: r->vehicle_class = RX_VA_CLASS_ATV; sim = U32(v, 0x1700); break;
    case VT_UTV: r->vehicle_class = RX_VA_CLASS_UTV; sim = U32(v, 0x1540); break;
    case VT_BUGGY: r->vehicle_class = RX_VA_CLASS_BUGGY; sim = U32(v, 0x15c0); delta = 0x10; break;
    case VT_TRUCK: r->vehicle_class = RX_VA_CLASS_TRUCK; sim = U32(v, 0x15d0); break;
    default: return;
    }
    if (!sim) { r->vehicle_class = RX_VA_CLASS_NONE; return; }
    r->flags = RX_VA_VALID;
    if (p && U32(p, 0x10) == 0) r->flags |= RX_VA_HUMAN_SLOT;
    const rx_addr player = U32(v, 0x190);
    if (player) {
        r->flags |= RX_VA_HAS_PLAYER;
        r->pad = I32(player, 0x1f0);
        if (U32(player, 4) != 0 && U32(player, 0x10) == 0) r->flags |= RX_VA_LOCAL_HUMAN;
    }
    const rx_addr controls = U32(v, 0x234);
    if (controls && U32(controls, 0x2eb0) != 0) r->flags |= RX_VA_MANUAL_GEARBOX;
    r->vehicle_kind = I32(v, 0x194);
    r->engine_size = I32(v, 0x170);
    r->audio_slot = audio_slot_of(v);
    if (r->audio_slot >= 0) {
        r->flags |= RX_VA_AUDIO_SLOT;
        const rx_sl_emitter* e = rx_engine_start_line_slot(r->audio_slot);
        if (e) {   /* w-rng: the core's start line of the slot's emitter */
            r->flags |= RX_VA_START_LINE;
            r->start_line_state = e->state; r->start_line_serial = e->serial; r->start_line_target = e->target;
        }
    }
    for (uint32_t i = 0; i < 3; i++) { r->position[i] = F32(v, 0xc0 + 4 * i); r->velocity[i] = F32(sim, 0x100 + 4 * i); }
    /* vf52: speed = sqrt of the velocity's x, y, z squares (the vec4's w is not summed) */
    r->speed = sqrtf(r->velocity[0] * r->velocity[0] + r->velocity[1] * r->velocity[1] + r->velocity[2] * r->velocity[2]);
    r->water = F32(v, 0x131c); r->underwater = U32(v, 0x1310); r->drowned = U32(v, 0x1314);
    if (r->vehicle_class == RX_VA_CLASS_MX) {
        const rx_addr ctl = U32(sim, 0x5c30);
        r->rpm = rpm_of(sim, 0x8abc); r->max_rpm = F32(sim, 0x5014); r->idle_rpm = F32(sim, 0x505c);
        r->gear = I32(sim, 0x8ac0) + 1;
        if (ctl) { r->throttle = F32(ctl, 0); r->control2 = F32(ctl, 4); r->clutch = U32(ctl, 0x84) == 2; }
        r->load = F32(sim, 0x8ad8);
        r->wheel_air[0] = U32(sim, 0x8894) == 0; r->wheel_air[1] = U32(sim, 0x89e4) == 0;
        r->airborne = r->wheel_air[0] && r->wheel_air[1];
        r->below_half = F32(sim, 0x8adc) < -0.5f;
        const rx_addr rider = U32(v, 0x1a0);
        r->wrecked = rider ? U32(rider, 0xaf00) != 0 : 0;
    } else if (r->vehicle_class == RX_VA_CLASS_ATV) {
        static const uint32_t contact[4] = {0x93e0, 0x9530, 0x9140, 0x9290};
        r->rpm = rpm_of(sim, 0x96ec); r->max_rpm = F32(sim, 0x4bfc); r->idle_rpm = F32(sim, 0x4c44);
        r->gear = I32(sim, 0x96f0) + 1; r->clutch = U32(sim, 0x597c) == 2;
        r->throttle = F32(sim, 0x5880); r->control2 = F32(sim, 0x5884); r->load = F32(sim, 0x9708);
        for (uint32_t i = 0; i < 4; i++) r->wheel_air[i] = U32(sim, contact[i] + 0x104) == 0;
        r->airborne = !U32(sim, 0x9244) && !U32(sim, 0x9394) && !U32(sim, 0x94e4) && !U32(sim, 0x9634);
    } else {
        static const uint32_t contact[4] = {0x8320, 0x8470, 0x8080, 0x81d0};
        const uint32_t rpm_off = r->vehicle_class == RX_VA_CLASS_BUGGY ? 0x864c : 0x865c;
        const uint32_t gear_off = r->vehicle_class == RX_VA_CLASS_UTV ? 0x8660 : r->vehicle_class == RX_VA_CLASS_BUGGY ? 0x8650 : 0x8664;
        const uint32_t load_off = r->vehicle_class == RX_VA_CLASS_UTV ? 0x8678 : r->vehicle_class == RX_VA_CLASS_BUGGY ? 0x8668 : 0x867c;
        r->rpm = rpm_of(sim, rpm_off); r->max_rpm = F32(sim, 0x4d4c); r->idle_rpm = F32(sim, 0x4d94);
        r->gear = I32(sim, gear_off) + 1; r->clutch = U32(sim, 0x56fc - delta) == 2;
        r->throttle = F32(sim, 0x5600 - delta); r->control2 = F32(sim, 0x5604 - delta); r->load = F32(sim, load_off);
        for (uint32_t i = 0; i < 4; i++) r->wheel_air[i] = U32(sim, contact[i] - delta + 0x104) == 0;
        r->airborne = !U32(sim, 0x8184 - delta) && !U32(sim, 0x82d4 - delta) && !U32(sim, 0x8424 - delta) && !U32(sim, 0x8574 - delta);
    }
}

int rx_vehicle_audio_fill(rx_addr g, rx_vehicle_audio_state* out) {
    out->cue_serial = cues.serial;
    for (uint32_t k = 0; k < RX_VEHICLE_AUDIO_CUES; k++) out->cues[k] = cues.ring[k];
    if (!g) return 0;
    const uint32_t count = U8(g, 0x1541c);
    int32_t n = 0;
    for (uint32_t i = 0; i < count && i < RX_RC_MAX && n < 2 * RX_VEHICLE_AUDIO_RIDERS; i++) {   /* (w-rc-core) 32 seats: riders 16.. in riders_ext */
        const uint32_t slot = rx_seat_order(g + 0x7460, i);
        if (slot >= RX_RC_MAX) continue;
        rx_vehicle_audio_rider* r = n < RX_VEHICLE_AUDIO_RIDERS ? &out->riders[n] : &out->riders_ext[n - RX_VEHICLE_AUDIO_RIDERS];
        fill_rider(g, slot, r);
        if (r->flags & RX_VA_VALID) n++;
    }
    out->rider_count = n;
    return 1;
}
#else
void rx_vehicle_audio_reset(void) {}
void rx_vehicle_audio_register(rx_addr vehicle) { (void)vehicle; }
void rx_vehicle_audio_unregister(rx_addr vehicle) { (void)vehicle; }
void rx_vehicle_audio_scenario_sound(rx_addr name) { (void)name; }
void rx_vehicle_audio_pre_exited(void) {}
void rx_vehicle_audio_tick(float dt) { (void)dt; }
int rx_vehicle_audio_finished(int32_t slot) { (void)slot; return 1; }
int rx_vehicle_audio_fill(rx_addr g, rx_vehicle_audio_state* out) { (void)g; (void)out; return 0; }
#endif
