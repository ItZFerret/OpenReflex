/* alive_gate.c -- PORT ADDITION (not original game code): the start gate of a converted MX vs ATV Alive race (alive_gate.h,
 * core/plan/ALIVE_RACING.md "Start gate"). */
#include "alive_gate.h"
#include "anim.h"
#include "anim_extern.h"
#include "build_activity.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "build_collis.h"
#include "build_collision.h"
#include "build_stream.h"
#include "build_world.h"
#include "map_scale.h"
#ifndef RX_ORACLE
#include "sound_state.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Reflex's DropGate (world_dropgate.c / anim_ik_ent.c 006be1c0) swings its bar to its down angle (0.53 rad) in 7 engine ticks, then
 * switches its collision off (state 1 -> 2) and bounces back for 5 more. Alive's GL_TO_StartGate_Drop (and JS7's FR_JSComp_StartingGate_Drop)
 * is the same drop as keys on the bar's node (a turn about the gate's -x through the hinge): 0 deg held for 2 frames, 1.2, 4.9, 11.2, 20.0,
 * then 31.2 deg at 0.2 s (its down angle), a bounce to 22 deg and 28.9 deg at its end (0.4 s, held). It plays as Alive plays it (a cut, from
 * its first key: the bar's model pose) and the collision goes off when the bar is down, the 0.2 s key (12 ticks). */
#define DROP_CLEAR_TICKS 12

/* The bar's collision (the gate model's frame at MAP SCALE 1). The bar the GL_TO_StartGate / FR_JSComp_StartGate "DropGate" node draws
 * (GL_TO_StartMetal: x -32.41 .. 32.28) is a plate leaning back over the riders' front wheels, from (y 0.25, z 0.1) at the hinge up to
 * (y 2.0, z -3.0): at a front hub's height (y 1.3 .. 1.6) it is at z -1.6 .. -2.35. The GateStart slots are at (y 0.36, z -7.25) facing +z;
 * an AI's front tyre reaches about z -4.9, the player's (placed further up) about z -2.7. Reflex's own DropGate collides only through its
 * Editor_Default box (a small box list at the hinge: the race's start holds the riders); here the bar is a wall the width of the gate,
 * z -2.2 .. -1.2, y 0.25 (in the floor plates) .. 2.05: a front tyre stops square against it where it meets the plate. Measured
 * (alive_gate_test): a box along the overhanging plate, or a wall reaching back to z -3.0, overlapped the player's front tyre at the gate,
 * and at GO the rider was thrown 50 ft. */
static const float k_bar_centre[3] = {0.0f, 1.15f, -1.7f};
static const float k_bar_half[3] = {32.45f, 0.9f, 0.5f};
static const float k_bar_rows[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

static rx_addr g_gate;          /* the ScenarioAnimEntity "TO_StartingGate" of the session's Alive race (0: none) */
static rx_addr g_ref;           /* the bar's static object record reference {refcount, world, index} (0: none) */
static int32_t g_collision = -1;
static int32_t g_drop = -1;      /* frames since the drop began (-1: not dropping) */
static int32_t g_cleared;        /* the drop switched the collision off */

rx_addr rx_alive_gate_entity(void) { return g_gate; }
int32_t rx_alive_gate_collision(void) { return g_collision; }
int32_t rx_alive_gate_drop_frames(void) { return g_drop; }

/* a host string on the emulated stack (the caller's frame): its address */
static rx_addr put_str(rx_addr at, const char* s) {
    size_t n = strlen(s);
    for (size_t i = 0; i <= n; i++) SU8(at, (uint32_t)i, (uint8_t)s[i]);
    return at;
}

static void set_collision(int on) {
    if (!g_ref) return;
    const rx_addr world = U32(g_ref, 4);
    const uint32_t idx = U32(g_ref, 8);
    if (idx >= (uint32_t)((int32_t)(U32(world, 0x19c4) - U32(world, 0x19c0)) >> 6)) return;
    SU8(U32(world, 0x19c0), 0x40u * idx + 0x38, on ? 1 : 0);
    g_collision = on ? 1 : 0;
}

/* the bar's static collision box in the gate's physics world (alive_gate.h) */
static void make_collision(rx_addr L, rx_addr gate, rx_addr scene /* the scene manager (world +0x20138) */) {
    const float s = rx_ms_scale();
    /* a kind 11 collis stream (build_collis.c: magic 1234, "col", version 2, kind 11, flag 0, the half extents (x, y, z, 0)) */
    uint8_t col[36];
    const uint32_t head[5] = {0x4d2, 0x006c6f63 /* "col" */, 2, 11, 0};
    memcpy(col, head, sizeof head);
    const float half[4] = {k_bar_half[0] * s, k_bar_half[1] * s, k_bar_half[2] * s, 0.0f};
    memcpy(col + 20, half, sizeof half);
    rx_stream st;
    rx_stream_init(&st, col, sizeof col);
    const rx_addr asset = rx_collision_asset_io_create(&st);   /* {vtable 0x00974f28, +0x10 the geometry}; rx_ms_load() is 1 here */
    if (!asset || !U32(asset, 0x10)) return;
    /* a collis handle for 008446e0: +0x44 the asset, +0x4c its lock word (not the split-triangle handle's vtable) */
    const rx_addr h = rx_reflex_aligned_alloc(0x50);
    for (uint32_t k = 0; k < 0x50; k += 4) SU32(h, k, 0);
    SU32(h, 0x44, asset);
    /* the box's world matrix: its rows through the gate's rotation, its centre through the gate's matrix (row vectors) */
    const rx_addr M = L + 0x40, out = L + 0x80;
    const rx_addr G = act_vcall(gate, 0x38, 0, 0);   /* vf14: the gate's world matrix (as the grid pass, 005663b0; +0x30 keeps no translation) */
    float g[16];
    for (int k = 0; k < 16; k++) g[k] = F32(G, 4u * (uint32_t)k);
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++)
            SF32(M, 16u * (uint32_t)r + 4u * (uint32_t)c, k_bar_rows[r][0] * g[c] + k_bar_rows[r][1] * g[4 + c] + k_bar_rows[r][2] * g[8 + c]);
        SF32(M, 16u * (uint32_t)r + 12, 0.0f);
    }
    for (int c = 0; c < 3; c++)
        SF32(M, 0x30 + 4u * (uint32_t)c, (k_bar_centre[0] * g[c] + k_bar_centre[1] * g[4 + c] + k_bar_centre[2] * g[8 + c]) * s + g[12 + c]);
    SF32(M, 0x3c, 1.0f);
    const rx_addr world = rx_phys_world_of_scene(U32(gate, 0x10));   /* the gate's scene's physics world (as CollisionEntity::vf5) */
    if (!world) return;
    SU32(out, 0, 0); SU32(out, 4, 0);
    ACT_CALL(7, rx_static_add_asset(world, out, M, 0, 0, h, 0));
    g_ref = U32(out, 4);   /* the reference the handle took (never released: the bar keeps it for the session) */
    if (!g_ref) return;
    /* the record's owner (a CollisionEntity: the contacts read its audio material): the level's own Gate_Bit (the gate's floor plates) */
    rx_addr owner;
    ACT_CALLR(owner, 2, rx_scene_manager_find_named(scene, put_str(L + 0xc0, "Gate_Bit")));
    if (owner && act_vcall(owner, 0x58, 0, 0) == 5) SU32(U32(world, 0x19c0) + (U32(g_ref, 8) << 6), 0x30, owner);
    g_collision = 1;
}

/* the drop animation (ScenarioAnimEntity_Play 0074a9f0 with its motion not looping: it ends on its last key; `blend` s of transition from
 * the pose shown, 0 = a cut) */
static int play_drop(rx_addr e, float blend) {
    const rx_addr h = U32(U32(e, 0x100), 0x44) + 0x10;
    rx_addr name_tmp = rx_sp() - 0x40;
    uint32_t own;
    { const rx_addr sp = rx_sp(); rx_set_sp(sp - 0x48);
      own = rx_hierarchy_has_node(h, put_str(name_tmp, "FR_JSComp_DropGate")); rx_set_sp(sp); }
    const char* anim = own ? "FR_JSComp_StartingGate_Drop" : "GL_TO_StartGate_Drop";
    put_str(e + 0x3f8, anim);   /* the entity's animation name (0074aab0's buffer, 127 characters) */
    const rx_addr sp0 = rx_sp(), cs = e + 0x3e0;
    rx_set_sp(sp0 - 0x10);
    anim_enter_cs(cs);
    SF32(e, 0x478, -1.0f);
    const rx_addr mgr = U32(U32(0, 0x00a95844), 8);
    const uint32_t a[4] = {e + 0x3f8, 0x6d696e61 /* 'anim' */, 0, 0};
    const rx_addr res = rx_call_original(U32(U32(mgr, 0), 0x6c), RX_THISCALL, mgr, 4, a);
    if (res) {
        uint32_t b = e + 0x390;
        rx_call_original(U32(U32(e, 0), 0x3c), RX_THISCALL, e, 1, &b);
        const rx_addr c = e + 0x148;
        ANIM_CALL_FRAME(0); rx_addr slot = rx_character_anim_find_slot(res, c); ANIM_CALL_FRAME_END(0);
        if (slot == 0) { ANIM_CALL_FRAME(8); slot = rx_character_anim_set_animation(c, res, 0); ANIM_CALL_FRAME_END(8); }
        ANIM_CALL_FRAME(4); rx_character_anim_play(c, slot, blend); ANIM_CALL_FRAME_END(4);
        SU8(e, 0xa2, 1);
        SU32(e, 0x70, 1);
    } else fprintf(stderr, "rxcore: Alive start gate: no animation %s in the level\n", anim);
    anim_leave_cs(cs);
    rx_set_sp(sp0);
    return res != 0;
}

void rx_alive_gate_attach(rx_addr A, rx_addr gate) {
    if (!gate || act_vcall(gate, 0x58, 0, 0) != 0x104f) return;   /* a ScenarioAnimEntity (Reflex's gates are CollisionEntities) */
    if (gate == g_gate) return;
    ACT_FRAME(L, 0x100, 0);
    g_gate = gate; g_ref = 0; g_collision = -1; g_drop = -1; g_cleared = 0;
    SU8(gate, 0xa2, 1);   /* shown at its model's pose: the bar up */
    SU32(gate, 0x70, 1);
    make_collision(L, gate, U32(U32(A, 0x234), 0x20138));
    fprintf(stderr, "rxcore: Alive start gate: shown, the bar's collision %s (MAP SCALE %.2f)\n", g_ref ? "on" : "not made", (double)rx_ms_scale());
    ACT_END(L);
}

/* 006bdbe0's rule (world_dropgate.c): a gated race starts closed; a free ride, mode 0 / 6 or a session result starts open */
void rx_alive_gate_start(rx_addr A) {
    if (!g_gate) return;
    const int upright = U32(A, 0x23c) != 8 && U32(A, 0x238) != 0 && U32(A, 0x238) != 6 && U32(A, 0xfe8) == 0;
    if (upright) {
        set_collision(1);
        g_drop = -1; g_cleared = 0;
        SU8(g_gate, 0xa2, 1);
        return;
    }
    set_collision(0);
    g_cleared = 1; g_drop = 0x7fffffff;
    play_drop(g_gate, 0.0f);
}

void rx_alive_gate_state_entered(rx_addr A, uint32_t state) {
    (void)A;
    if (!g_gate) return;
    if (state == 1 && g_drop < 0) {
        /* the race intro's scenario takes every ScenarioAnimEntity of the scene as its cast and stops them when it ends, entering the
         * Countdown (anim_scenario_intro.c entity_stop: +0x70 / +0xa2 = 0): the gate is not one of its actors, it stays up and shown */
        SU8(g_gate, 0xa2, 1);
        SU32(g_gate, 0x70, 1);
        return;
    }
    if (state != 2 || g_drop >= 0) return;
    g_drop = 0; g_cleared = 0;
    play_drop(g_gate, 0.0f);
#ifndef RX_ORACLE
    /* the drop's sound: Reflex's gate sound is an activity audio cue, "<ACTIVITY_*>/ACTIVITY/DROP_GATE" on "InProgress Entered" / "Countdown
     * Step" 0, from the ActivityAudioManager entity of a Reflex level (activity_cues.c). An Alive level has none (its RaceActivityAudioManager
     * is Alive's and not converted), so its races had no cue at all: the gate's own drop gives the host Reflex's (Alive's GateDrop plays its
     * own UI_02/UI/GATE_DROP at the same moment). */
    const int sx = strcmp(rx_event_spec_get()->event_type, "Supercross") == 0;
    rx_sound_cue_text(sx ? "ACTIVITY_SUPERCROSS/ACTIVITY/DROP_GATE" : "ACTIVITY_NATIONALS/ACTIVITY/DROP_GATE", -1);
#endif
}

/* the bar's collision on / off now (alive_gate_test: the gate held closed after GO shows that it blocks) */
void rx_alive_gate_set_collision(int on) { set_collision(on); }

void rx_alive_gate_tick(rx_addr e) {
    if (e != g_gate || g_drop < 0 || g_drop == 0x7fffffff) return;
    g_drop++;
    if (!g_cleared && g_drop >= DROP_CLEAR_TICKS) { set_collision(0); g_cleared = 1; }
}
