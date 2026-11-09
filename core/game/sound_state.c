/* sound_state.c -- what the original's sound managers read besides the engine emitters (include/rx_sound.h; w-sfx, 2026-10-27):
 * the level volume mix, the manager snapshot's wheel groups, the physics contact events the AudioCollisionManager gets, the crowd
 * reactions, the activity audio cues and the music player. The sound layer itself is on the host's side of the audio boundary
 * (Unity/Reflex/Assets/Scripts/OriginalWorld/OriginalGameSound.cs); this file hands it copies.
 *
 * Product only. Nothing the game reads is changed: the recorders keep their rings outside guest memory, and the few core functions
 * the queries call to ask the objects what the original's managers ask them (the class / sound id / material getters, TerrainMaterialAt
 * for the audio zone, a rider's / object's speed) run inside a guard that restores the dead stack below ESP and the terrain lookup's
 * busy word, so the next game frame sees the same memory whether the host asks or not. The oracle build gets empty hooks. */
#include "rx_game.h"
#include "sound_state.h"
#ifndef RX_ORACLE
#include <string.h>
#include <math.h>
#include "rx_vehicle_audio.h"
#include "phys_terrain.h"
#include "phys_rigbody.h"
#include "world_veh.h"
#include "music_player.h"
#include "rider_cap.h"

_Static_assert(sizeof(rx_sound_wheel) == 72, "sound wheel ABI size");
_Static_assert(sizeof(rx_sound_rider) == 336, "sound rider ABI size");
_Static_assert(sizeof(rx_sound_contact) == 128, "sound contact ABI size");
_Static_assert(sizeof(rx_sound_crowd) == 164, "sound crowd ABI size");
_Static_assert(sizeof(rx_sound_cue) == 96, "sound cue ABI size");
_Static_assert(sizeof(rx_sound_music) == 256, "sound music ABI size");
_Static_assert(sizeof(rx_sound_state) == 29320 && offsetof(rx_sound_state, riders_ext) == RX_SOUND_V1_SIZE, "sound ABI size (version 2; version 1 = the first 23944 bytes)");

#define VT_MX    0x0093cd84u   /* RavenVehicleMotocross */
#define VT_ATV   0x0093d0ecu   /* RavenVehicleATV */
#define VT_UTV   0x0093d4d4u   /* RavenVehicleUTV */
#define VT_BUGGY 0x0093d2e4u   /* RavenVehicleBuggy */
#define VT_TRUCK 0x0093cee4u   /* RavenVehicleTrophyTruck */
#define CHANNEL_RECORDS 0x00d679a0u   /* 0x54 bytes each, the mix at + 0 */
#define CHANNEL_LAST    0x00a957c8u   /* the last channel index (9) */

static struct {
    uint32_t contact_serial; rx_sound_contact contacts[RX_SOUND_CONTACTS];
    uint32_t reaction_serial; struct { uint32_t serial; int32_t kind; } reactions[RX_SOUND_REACTIONS];
    uint32_t cue_serial; rx_sound_cue cues[RX_SOUND_CUES];
    uint32_t rider_serial; struct { uint32_t serial; uint32_t vehicle; int32_t slot; int32_t reserved; } riders[RX_SOUND_RIDER_SOUNDS];
    uint32_t misses;   /* object getters that are not core functions (answered with the manager's defaults) */
} rec;

static uint32_t crowd_running;
void rx_sound_state_reset(void) { memset(&rec, 0, sizeof rec); crowd_running = 0; }
void rx_sound_crowd_set_running(uint32_t on) { crowd_running = on; }
uint32_t rx_sound_crowd_running(void) { return crowd_running; }

/* ---- the guard: the dead stack below ESP and the words a lookup flags, restored after the queries ---------------------------- */
#define GUARD_BYTES 0x1000u
typedef struct { rx_addr sp; uint8_t stack[GUARD_BYTES]; } guard;
static void guard_enter(guard* g) { g->sp = rx_sp(); for (uint32_t i = 0; i < GUARD_BYTES; i++) g->stack[i] = U8(g->sp - GUARD_BYTES, i); }
static void guard_leave(guard* g) { rx_set_sp(g->sp); for (uint32_t i = 0; i < GUARD_BYTES; i++) SU8(g->sp - GUARD_BYTES, i, g->stack[i]); }
/* a 64-byte scratch block on the game stack (inside the guarded bytes); ESP is moved below it so the callee's frames do not reach it */
static rx_addr scratch(const guard* g) { const rx_addr b = ((g->sp - 0x40u) & ~15u); rx_set_sp(b - 0x10u); return b; }

/* a virtual getter (thiscall, integer arguments) when the core has the function; else `dflt` */
static uint32_t vget(rx_addr obj, uint32_t off, int n, const uint32_t* args, uint32_t dflt) {
    if (!obj || !U32(obj, 0)) return dflt;
    const rx_addr target = U32(U32(obj, 0), off);
    if (!rx_fn(target)) { rec.misses++; return dflt; }
    return rx_call_original(target, RX_THISCALL, obj, n, args);
}
static int32_t obj_class(rx_addr o) { return o ? (int32_t)vget(o, 0x58, 0, 0, 0) : 0; }       /* vf22 */
static int32_t obj_sound(rx_addr o) { return o ? (int32_t)vget(o, 0x30, 0, 0, 0) : 0; }       /* vf12 */
static int32_t obj_material(rx_addr o) { return o ? (int32_t)vget(o, 0x34, 0, 0, 5) : 5; }    /* vf13 */
static int is_rider(rx_addr o, int32_t cls) { return o && cls == RX_SC_RIDER_ENTITY && obj_sound(o) == 0x31; }

/* the terrain audio zone at a point (TerrainMaterialAt layer 3 on the world's terrain, 00402dc0 with 0x9e30d4 as the manager and
 * AudioCollision_UpdateRoll look it up); -1 without a terrain or outside its bounds when `bounded` (the manager's wheel test) */
static int32_t zone_at(const guard* g, float x, float y, float z, int bounded) {
    const rx_addr t = rx_world_find_by_controller(0x009e30d4);
    if (!t || !U32(t, 0x44ac)) return -1;
    if (bounded && !(x >= F32(t, 0x44d8) && x <= F32(t, 0x44e0) && z >= F32(t, 0x44dc) && z <= F32(t, 0x44e4))) return -1;
    const rx_addr busy = U32(t, 0x44ac) + 0x4c, pt = scratch(g);
    const uint32_t was = U32(busy, 0);
    SF32(pt, 0, x); SF32(pt, 4, y); SF32(pt, 8, z); SF32(pt, 12, 0.0f);
    const uint32_t m = phys_terrain_material_at(t, pt, 3, NULL, NULL);
    SU32(busy, 0, was);
    rx_set_sp(g->sp);
    return (int32_t)m;
}

/* ---- contacts: World_PostListeners (00769ab0), where the original's manager (0067bac0) gets the world's events ---------------- */
static void side(const guard* g, rx_addr o, uint32_t feature, int32_t* cls, int32_t* snd, int32_t* mat, uint32_t* vocal, uint32_t* flags,
                 uint32_t rider_bit, uint32_t vocal_bit, uint32_t ragdoll_bit) {
    (void)g;
    *cls = obj_class(o);
    *vocal = 0;
    if (!o) { *snd = 0; *mat = 5; return; }
    if (*cls == RX_SC_VEHICLE && (int32_t)feature >= 0) { const uint32_t a[1] = {feature}; *snd = (int32_t)vget(o, 0xd8, 1, a, (uint32_t)obj_sound(o)); }
    else *snd = obj_sound(o);
    *mat = obj_material(o);
    if (*cls == RX_SC_VEHICLE) { *vocal = o; if (U32(o, 0x194) <= 1) *flags |= vocal_bit; }
    else if (is_rider(o, *cls)) {
        *flags |= rider_bit;
        *vocal = U32(o, 0xaf1c);
        if (U32(o, 0xae70) == 0) *flags |= vocal_bit;
        if (U32(o, 0x5bb68) != 0) *flags |= ragdoll_bit;
    }
}

/* A's speed for the roll / slide loop: the switch 0067c350 (terrain) and 0067c670 (a class 5 object's slide) share: a rider's mean velocity
   (00422350), a physics entity's speed (0041cf30), a vehicle's + 0x150; anything else leaves slide_kind 0 (the record's force). */
static void slide_input(guard* g, rx_sound_contact* c) {
    if (c->flags & RX_SCF_RIDER_A) {
        const rx_addr v = scratch(g);
        phys_rigbody_mean_velocity(c->a + 0x950, v);
        rx_set_sp(g->sp);
        const float x = F32(v, 0), y = F32(v, 4), z = F32(v, 8);
        c->slide_speed = sqrtf(x * x + z * z + y * y); c->slide_kind = 1;
    } else if (c->class_a == RX_SC_PHYSICS_ENTITY) {
        const rx_addr v = scratch(g);
        rx_world_physics_entity_speed(c->a, v);
        rx_set_sp(g->sp);
        c->slide_speed = F32(v, 0); c->slide_kind = 2;
    } else if (c->class_a == RX_SC_VEHICLE) { c->slide_speed = F32(c->a, 0x150); c->slide_kind = 3; }
}

void rx_sound_record_contacts(rx_addr events) {
    const uint32_t n = U32(events, 0x1900);
    if (n == 0) return;
    guard g; guard_enter(&g);
    const rx_addr eng = U32(0, 0x00a95844);
    const uint32_t frame = eng && U32(eng, 0x10) ? U32(U32(eng, 0x10), 0x58) : 0;
    for (uint32_t i = 0; i < n && i < 100; i++) {
        const rx_addr e = events + 0x40 * i;
        rx_sound_contact* c = &rec.contacts[(rec.contact_serial + 1) % RX_SOUND_CONTACTS];   /* serial s lives at s % N */
        memset(c, 0, sizeof *c);
        c->frame = frame;
        c->a = U32(e, 0); c->b = U32(e, 4);
        for (uint32_t k = 0; k < 3; k++) c->position[k] = F32(e, 0x10 + 4 * k);
        if (U8(e, 0x20) != 0) c->flags |= RX_SCF_SOFT;
        c->strength_a = F32(e, 0x24); c->strength_b = F32(e, 0x28);
        c->sound_override = I32(e, 0x2c);
        c->feature_a = I32(e, 0x30); c->feature_b = I32(e, 0x34);
        side(&g, c->a, U32(e, 0x30), &c->class_a, &c->sound_a, &c->material_a, &c->vocal_a, &c->flags, RX_SCF_RIDER_A, RX_SCF_VOCAL_A, RX_SCF_RAGDOLL_A);
        side(&g, c->b, U32(e, 0x34), &c->class_b, &c->sound_b, &c->material_b, &c->vocal_b, &c->flags, RX_SCF_RIDER_B, RX_SCF_VOCAL_B, RX_SCF_RAGDOLL_B);
        c->zone = -1;
        if (c->class_b == RX_SC_TERRAIN) {
            /* AudioCollision_UpdateRoll (0067c350): the zone at the contact and A's speed */
            c->zone = zone_at(&g, c->position[0], c->position[1], c->position[2], 0);
            slide_input(&g, c);
        } else if (c->class_b == RX_SC_COLLISION_ENTITY && c->b && U8(c->b, 0x19c) != 0) {
            c->flags |= RX_SCF_SLIDE_B;
            c->object_material = I32(c->b, 0x158);
            slide_input(&g, c);   /* 0067c670 runs the same switch on A as 0067c350 */
        }
        c->serial = ++rec.contact_serial;
    }
    guard_leave(&g);
}

void rx_sound_crowd_reaction(uint32_t kind) {
    if (kind < 1 || kind > 3) return;
    const uint32_t s = ++rec.reaction_serial;
    rec.reactions[s % RX_SOUND_REACTIONS].serial = s;
    rec.reactions[s % RX_SOUND_REACTIONS].kind = (int32_t)kind;
}

void rx_sound_rider_sound(rx_addr vehicle, uint32_t slot) {
    const uint32_t s = ++rec.rider_serial, k = s % RX_SOUND_RIDER_SOUNDS;
    rec.riders[k].serial = s; rec.riders[k].vehicle = vehicle; rec.riders[k].slot = (int32_t)slot; rec.riders[k].reserved = 0;
}
void rx_sound_cue_text(const char* path, int32_t index) {
    rx_sound_cue* c = &rec.cues[(rec.cue_serial + 1) % RX_SOUND_CUES];
    memset(c, 0, sizeof *c);
    for (uint32_t k = 0; path && path[k] && k < RX_SOUND_CUE_PATH - 1; k++) c->path[k] = path[k];
    c->index = index;
    c->serial = ++rec.cue_serial;
}
void rx_sound_cue_played(rx_addr name, int32_t index) {
    rx_sound_cue* c = &rec.cues[(rec.cue_serial + 1) % RX_SOUND_CUES];
    memset(c, 0, sizeof *c);
    for (uint32_t k = 0; name && k < RX_SOUND_CUE_PATH - 1; k++) { const char ch = (char)U8(name, k); if (!ch) break; c->path[k] = ch; }
    c->index = index;
    c->serial = ++rec.cue_serial;
}

/* ---- riders: the manager snapshot's wheel groups (00680ec0 + vf52) --------------------------------------------------------- */
typedef struct { uint32_t contact, body, flag, user, obj; } wheel_at;
static void wheel_fill(const guard* g, rx_addr v, rx_addr s, int cls, uint32_t i, rx_sound_wheel* w) {
    float n2[3], s2[3], r2[3];
    uint32_t air, water, entered;
    rx_addr obj;
    if (cls == RX_VA_CLASS_MX) {
        /* rx_moto_state_report (00467070): rear = group 0, front = group 1 */
        const int r = i == 0;
        for (uint32_t k = 0; k < 3; k++) {
            w->position[k] = F32(s, (r ? 0x8800 : 0x8950) + 4 * k) + F32(s, (r ? 0x360 : 0x2c0) + 4 * k);
            n2[k] = F32(s, (r ? 0x87d0 : 0x8920) + 4 * k);
            s2[k] = F32(s, r ? 0x8810 : 0x8960) * F32(s, (r ? 0x88d0 : 0x8a20) + 4 * k);
            r2[k] = F32(s, r ? 0x8818 : 0x8968) * F32(s, (r ? 0x88b0 : 0x8a00) + 4 * k);
        }
        w->suspension_force = F32(s, r ? 0x8870 : 0x89c0);
        w->angular_velocity = F32(s, r ? 0x8a34 : 0x8a30);
        air = U32(s, r ? 0x8894 : 0x89e4) == 0;
        water = U32(v, r ? 0x1610 : 0x160c); entered = U32(v, r ? 0x1618 : 0x1614); w->depth = F32(v, r ? 0x1620 : 0x161c);
        obj = U32(s, r ? 0x8784 : 0x8780);
    } else {
        /* rx_atv_snapshot_values (00487510) / fourwheel_snapshot.c snapshot_values: rear left / right, front left / right */
        static const uint32_t atv_contact[4] = {0x93e0, 0x9530, 0x9140, 0x9290}, atv_body[4] = {0x400, 0x4a0, 0x2c0, 0x360};
        static const uint32_t atv_flag[4] = {0x9688, 0x968c, 0x9680, 0x9684}, atv_user[4] = {0x1630, 0x1634, 0x1628, 0x162c};
        static const uint32_t atv_obj[4] = {0x90f0, 0x90f4, 0x90e8, 0x90ec};
        static const uint32_t fw_contact[4] = {0x8320, 0x8470, 0x8080, 0x81d0}, fw_body[4] = {0x360, 0x400, 0x220, 0x2c0};
        static const uint32_t fw_flag[4] = {0x85c8, 0x85cc, 0x85c0, 0x85c4}, fw_user[4] = {8, 12, 0, 4}, fw_obj[4] = {0x8030, 0x8034, 0x8028, 0x802c};
        wheel_at a;
        if (cls == RX_VA_CLASS_ATV) a = (wheel_at){atv_contact[i], atv_body[i], atv_flag[i], atv_user[i], atv_obj[i]};
        else {
            const uint32_t d = cls == RX_VA_CLASS_BUGGY ? 0x10 : 0, u = cls == RX_VA_CLASS_UTV ? 0x1468 : cls == RX_VA_CLASS_BUGGY ? 0x14b8 : 0x14f8;
            a = (wheel_at){fw_contact[i] - d, fw_body[i], fw_flag[i] - d, u + fw_user[i], fw_obj[i] - d};
        }
        for (uint32_t k = 0; k < 3; k++) {
            w->position[k] = F32(s, a.contact + 0x70 + 4 * k) + F32(s, a.body + 4 * k);
            n2[k] = F32(s, a.contact + 0x40 + 4 * k);
            s2[k] = F32(s, a.contact + 0x80) * F32(s, a.contact + 0x140 + 4 * k);
            r2[k] = F32(s, a.contact + 0x88) * F32(s, a.contact + 0x120 + 4 * k);
        }
        w->suspension_force = F32(s, a.contact + 0xe0);
        w->angular_velocity = F32(s, a.flag);
        air = U32(s, a.contact + 0x104) == 0;
        water = U32(v, a.user); entered = U32(v, a.user + 0x10); w->depth = F32(v, a.user + 0x20);
        obj = U32(s, a.obj);
    }
    for (uint32_t k = 0; k < 3; k++) w->normal[k] = n2[k];
    w->side_slip2 = (s2[0] * s2[0] + s2[1] * s2[1]) + s2[2] * s2[2];
    w->roll_slip2 = (r2[0] * r2[0] + r2[1] * r2[1]) + r2[2] * r2[2];
    w->flags = (air ? RX_SW_AIR : 0u) | (water ? RX_SW_WATER : 0u) | (entered ? RX_SW_WATER_ENTERED : 0u);
    /* + 0x60 / + 0x64: the contact object only when it is a collision entity (vf22 == 5: the snapshot builders' test) */
    w->contact_id = -1; w->contact = 0; w->contact_class = 0; w->contact_sound = 0; w->contact_material = 5;
    if (obj && obj_class(obj) == RX_SC_COLLISION_ENTITY) {
        w->contact_id = I32(obj, 0x158); w->contact = obj;
        w->contact_class = RX_SC_COLLISION_ENTITY; w->contact_sound = obj_sound(obj); w->contact_material = obj_material(obj);
    }
    /* + 0x5c (the manager): the zone under a wheel in contact and inside the terrain */
    w->zone = air ? -1 : zone_at(g, w->position[0], w->position[1], w->position[2], 1);
}

static void rider_fill(const guard* g, rx_addr game, uint32_t slot, rx_sound_rider* r) {
    memset(r, 0, sizeof *r);
    r->slot = (int32_t)slot;
    const rx_addr v = U32(rx_seat(game + 0x7460, slot), 0x1ac);
    if (!v) return;
    rx_addr s = 0;
    switch (U32(v, 0)) {
    case VT_MX: r->vehicle_class = RX_VA_CLASS_MX; s = U32(v, 0x16d4); break;
    case VT_ATV: r->vehicle_class = RX_VA_CLASS_ATV; s = U32(v, 0x1700); break;
    case VT_UTV: r->vehicle_class = RX_VA_CLASS_UTV; s = U32(v, 0x1540); break;
    case VT_BUGGY: r->vehicle_class = RX_VA_CLASS_BUGGY; s = U32(v, 0x15c0); break;
    case VT_TRUCK: r->vehicle_class = RX_VA_CLASS_TRUCK; s = U32(v, 0x15d0); break;
    default: return;
    }
    if (!s) { r->vehicle_class = RX_VA_CLASS_NONE; return; }
    r->vehicle = v;
    r->wheel_count = r->vehicle_class == RX_VA_CLASS_MX ? 2 : 4;
    r->sound_id = obj_sound(v); r->material = obj_material(v);
    { const uint32_t a[1] = {0}; r->sound_feature0 = (int32_t)vget(v, 0xd8, 1, a, (uint32_t)r->sound_id); }
    for (uint32_t k = 0; k < 3; k++) r->velocity[k] = F32(s, 0x100 + 4 * k);
    for (uint32_t i = 0; i < (uint32_t)r->wheel_count; i++) wheel_fill(g, v, s, r->vehicle_class, i, &r->wheels[i]);
}

int rx_sound_fill(rx_addr game, rx_sound_state* out) {
    out->contact_serial = rec.contact_serial;
    memcpy(out->contacts, rec.contacts, sizeof out->contacts);
    out->cue_serial = rec.cue_serial;
    memcpy(out->cues, rec.cues, sizeof out->cues);
    out->rider_sound_serial = rec.rider_serial;
    memcpy(out->rider_sounds, rec.riders, sizeof out->rider_sounds);
    out->crowd.reaction_serial = rec.reaction_serial;
    memcpy(out->crowd.reactions, rec.reactions, sizeof out->crowd.reactions);
    const int32_t last = I32(0, CHANNEL_LAST);
    out->channel_count = last >= 0 && last < RX_SOUND_CHANNELS ? last + 1 : 0;
    for (int32_t k = 0; k < RX_SOUND_CHANNELS; k++) out->channel_mix[k] = k < out->channel_count ? F32(CHANNEL_RECORDS + 0x54u * (uint32_t)k, 0) : 1.0f;
    out->crowd.type = I32(0, 0x00a941b4); out->crowd.size = I32(0, 0x009c47bc); out->crowd.size_requested = I32(0, 0x009c47c0);
    out->crowd.momentum = F32(0, 0x00c023b8); out->crowd.state = I32(0, 0x00c02400);
    out->crowd.valid = rx_sound_crowd_running();
    rx_music_fill(&out->music);
    if (!game) return 0;
    const rx_addr eng = U32(0, 0x00a95844);
    out->frame = eng && U32(eng, 0x10) ? U32(U32(eng, 0x10), 0x58) : 0;
    guard g; guard_enter(&g);
    const uint32_t count = U8(game, 0x1541c);
    int32_t n = 0;
    for (uint32_t i = 0; i < count && i < RX_RC_MAX && n < 2 * RX_SOUND_RIDERS; i++) {   /* (w-rc-core) 32 seats: riders 16.. in riders_ext */
        const uint32_t slot = rx_seat_order(game + 0x7460, i);
        if (slot >= RX_RC_MAX) continue;
        rx_sound_rider* r = n < RX_SOUND_RIDERS ? &out->riders[n] : &out->riders_ext[n - RX_SOUND_RIDERS];
        rider_fill(&g, game, slot, r);
        if (r->vehicle) n++;
    }
    guard_leave(&g);
    out->rider_count = n;
    return 1;
}
uint32_t rx_sound_misses(void) { return rec.misses; }

/* The first-person camera is the player's view (FUN_0068bfb0's test, the camera sound effect's active flag 0068d2c0): the player's selected camera
   entity (P + 0x974 + 4 * [P + 0x988], one of five) is class 0x1002 (MXFirstPersonCamera) with its "pose valid" word (+ 0x2f4) set. Read only. */
uint32_t rx_sound_first_person_view(rx_addr player) {
    if (!player) return 0;
    const uint32_t sel = U32(player, 0x988);
    if (sel >= 5) return 0;
    const rx_addr cam = U32(player, 0x974 + 4 * sel);
    if (!cam) return 0;
    guard g; guard_enter(&g);
    const int32_t cls = obj_class(cam);
    guard_leave(&g);
    return cls == 0x1002 && U32(cam, 0x2f4) != 0;
}
#else
void rx_sound_state_reset(void) {}
void rx_sound_crowd_set_running(uint32_t on) { (void)on; }
uint32_t rx_sound_crowd_running(void) { return 0; }
void rx_sound_record_contacts(rx_addr events) { (void)events; }
void rx_sound_crowd_reaction(uint32_t kind) { (void)kind; }
void rx_sound_cue_played(rx_addr name, int32_t index) { (void)name; (void)index; }
void rx_sound_cue_text(const char* path, int32_t index) { (void)path; (void)index; }
void rx_sound_rider_sound(rx_addr vehicle, uint32_t slot) { (void)vehicle; (void)slot; }
int rx_sound_fill(rx_addr game, rx_sound_state* out) { (void)game; (void)out; return 0; }
uint32_t rx_sound_misses(void) { return 0; }
uint32_t rx_sound_first_person_view(rx_addr player) { (void)player; return 0; }
#endif
