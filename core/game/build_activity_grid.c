/* build_activity_grid.c -- construction unit 11: the race's start grid and start (core/notes/construction.md section 16).
 * The activity's start slots (A + 0x340 + 0x30 i, 28 of them: +0 position, +0x10 direction, +0x20 the track section, +0x24 its node,
 * +0x2c a flag) from the starting gate's "GateStart_NN" nodes (005666d0 -> 005663b0) projected on the track (005385f0 / 00538bb0);
 * MXGame's grid pass 0075f890 and the race start 0075e020; the load checks of MXGame::Update's states 3 .. 6. */
#include "alive_gate.h"
#include "grid_rows.h"
#include "rider_cap.h"
#include "career_activity.h"
#include "build_series_init.h"
#include "crt_atexit.h"
#include "build_activity_int.h"
#include "anim.h"
#include "build_ui_world.h"
#include "world_node.h"
#include "race_misc.h"
#include "race_activity_stub.h"
#include "loop_platform.h"
#include "map_scale.h"
#include "../src/fmt_modtrack.h"
#include <math.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

/* ======================================================================================================================== */
/* the track (the activity's track block A + 0x890: +0x130 the sections, count +0x1d8; a section: +0 its first node, +0x118 flags;
 * a node: +0 the centre, +0x10 / +0x20 the left / right edge points, +0x64 the next node)                                     */

/* 00538bb0 (ECX = node, EAX = next node, EDI = position) -> EAX: the position lies inside the quad node -> next (the four edge
 * half-planes in the xz plane; a degenerate edge, bit-equal end points, accepts its side) */
uint32_t rx_track_quad_contains(rx_addr n, rx_addr e, rx_addr p) {
    const float n10 = F32(n, 0x10), n14 = F32(n, 0x14), n18 = F32(n, 0x18), n20 = F32(n, 0x20), n24 = F32(n, 0x24), n28 = F32(n, 0x28);
    const float e10 = F32(e, 0x10), e14 = F32(e, 0x14), e18 = F32(e, 0x18), e20 = F32(e, 0x20), e24 = F32(e, 0x24), e28 = F32(e, 0x28);
    const int same0 = U32(n, 0x10) == U32(e, 0x10) && U32(n, 0x14) == U32(e, 0x14) && U32(n, 0x18) == U32(e, 0x18);
    const int same1 = U32(n, 0x20) == U32(e, 0x20) && U32(n, 0x24) == U32(e, 0x24) && U32(n, 0x28) == U32(e, 0x28);
    const float px = F32(p, 0), py = F32(p, 4), pz = F32(p, 8), zero = 0.0f;
    float t = (pz - n18) * (n10 - e10) + (px - n10) * (e18 - n18);
    t = t + (py - n14) * zero;
    if (zero > t && !same0) return 0;
    t = (pz - e18) * (e10 - e20) + (px - e10) * (e28 - e18);
    t = t + (py - e14) * zero;
    if (zero > t) return 0;
    t = (pz - e28) * (e20 - n20) + (px - e20) * (n28 - e28);
    t = t + (py - e24) * zero;
    if (zero > t && !same1) return 0;
    t = (pz - n28) * (n20 - n10) + (px - n20) * (n18 - n28);
    t = t + (py - n24) * zero;
    if (zero > t) return 0;
    return 1;
}
RX_FUNCTION(0x00538bb0, rx_track_quad_contains)

/* 005385f0 (EAX = position; stdcall track, out {section, node, squared distance}, radius, skip flag-4 sections, skip flag-2
 * sections; RET 0x14): the nearest node (the segment containing the position, 00538bb0) of every section within the radius
 * walked along the section from its first node */
void rx_track_nearest_in_radius(rx_addr p, rx_addr t, rx_addr out, float radius, uint32_t skip4, uint32_t skip2) {
    ACT_FRAME_EBP(L, 16, 0x34, 3);
    SU32(out, 8, 0x7f7fffffu);   /* FLT_MAX (0x977494) */
    SU32(out, 4, 0);
    SU32(out, 0, 0);
    for (int32_t s = 0; s < I32(t, 0x1d8); s++) {
        const uint32_t fl = U32(U32(U32(t, 0x130), 4u * (uint32_t)s), 0x118);
        if ((fl & 4) && skip4) continue;
        if ((fl & 2) && skip2) continue;
        rx_addr n = U32(U32(U32(t, 0x130), 4u * (uint32_t)s), 0);
        const float x0 = F32(n, 0), y0 = F32(n, 4), z0 = F32(n, 8);
        float r2 = radius * radius;
        if (U32(n, 0x64) == 0) continue;
        while (r2 > F32(0, 0x0092e12c)) {
            const float nx = F32(n, 0), ny = F32(n, 4), nz = F32(n, 8);
            const float dy = ny - F32(p, 4), dx = nx - F32(p, 0), dz = nz - F32(p, 8);
            const float d = dz * dz + dy * dy + dx * dx;
            if (F32(out, 8) > d) {
                uint32_t in;
                ACT_CALLR(in, 0, rx_track_quad_contains(n, U32(n, 0x64), p));
                if (in) {
                    SU32(out, 4, n);
                    SU32(out, 0, U32(U32(t, 0x130), 4u * (uint32_t)s));
                    SF32(out, 8, d);
                }
            }
            const float ez = nz - z0, ey = ny - y0, ex = nx - x0;
            n = U32(n, 0x64);
            const uint32_t next = U32(n, 0x64);
            r2 = r2 - (ez * ez + ey * ey + ex * ex);
            if (next == 0) break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x005385f0, rx_track_nearest_in_radius)

/* ======================================================================================================================== */
/* the start grid                                                                                                            */

/* PORT ADDITION (w-aliveai, w-secondrow; core/plan/ALIVE_RACING.md, GRID_ROWS.md): a gate of fewer than 16 "GateStart_NN" nodes (a converted
 * MX vs ATV Alive level's 12, a custom gate's) is a grid of that many slots (the original stops at a missing node: the start-line path,
 * act_unreached below; no Reflex level comes there), and the riders past a gate's slots line up behind it: grid_rows.h (the start zones
 * packed onto the gate, the places behind it). */
static uint32_t player_active_cell(uint32_t row);
uint32_t rx_grid_active_cell(uint32_t row) { return player_active_cell(row); }

/* 005663b0 (ECX = activity, EDX = the "TO_StartingGate" entity) -> EAX: the 16 slots from the gate's "GateStart_01" .. "_16" nodes:
 * each node's world matrix (Node_EvaluateWorld) times the gate's matrix (vf14): row 3 the slot's position, row 2 its direction,
 * the nearest track node within the track's start radius (+0xa6c + +0xa70; 005385f0); 1 when all 16 exist, else 0 */
static uint32_t race_grid_gate_nodes(rx_addr A, rx_addr gate) {
    ACT_FRAME_EBP(L, 16, 0x184, 3);
    SU32(L, 0x18c, U32(0, 0x009b45e4) ^ (L + 0xc));                    /* the /GS cookie */
    SU32(U32(gate, 0x100), 0x4c, 1);
    SU32(L, 0x30, U32(U32(gate, 0x100), 0x44) + 0x10);
    SU32(L, 0x20, 1); SU32(L, 0x28, A); SU32(L, 0x24, gate);
    rx_addr M = act_vcall(gate, 0x38, 0, 0);                           /* vf14: the gate's matrix */
    SU32(U32(gate, 0x100), 0x4c, 0);
    rx_addr slot = A + 0x348;
    SU32(L, 0x18, slot);
    uint32_t result = 0;
    for (uint32_t i = 1;; i++) {
        SU32(L, 0x1c, i);
        const rx_addr name = L + 0x80;
        {
            const rx_addr sp = rx_sp();
            SU32(sp, -8, 0x00942050); SU32(sp, -4, i);
            rx_set_sp(sp - 12);
            rx_format_buffer(name, 0x104, 0x00942050 /* "GateStart_%02d" */, sp - 4);
            rx_set_sp(sp);
        }
        const rx_addr h = U32(L, 0x30);
        uint32_t has;
        ACT_CALLR(has, 1, rx_hierarchy_has_node(h, name));
        if (has && rx_grid_rows_cap() != 0 && i > rx_grid_rows_cap()) has = 0;   /* (a tools-only test hook, grid_rows.h; 0 in the game) */
        if (!has) {
            result = 0;
            /* PORT ADDITION: a gate of i - 1 < 16 slots (an Alive gate's 12; its bar: alive_gate.c), and the riders past them behind it */
            if (i >= 2) { rx_grid_rows(A, gate, i - 1); rx_alive_gate_attach(A, gate); result = 1; }
            break;
        }
        rx_addr node;
        ACT_CALLR(node, 1, rx_hierarchy_find_node(h, name));
        ACT_CALL(1, rx_world_node_evaluate_world(node, 0));
        const rx_addr W = U32(U32(node, 0), 0x1c) + 0x40u * U8(node, 0xb);
        float m[16];
        for (int k = 0; k < 16; k++) m[k] = F32(M, 4 * k);
        const float a0 = F32(W, 0x20), a1 = F32(W, 0x24), a2 = F32(W, 0x28), a3 = F32(W, 0x2c);
        const float dx = a0 * m[0] + a1 * m[4] + a2 * m[8] + a3 * m[12];
        const float dy = a0 * m[1] + a1 * m[5] + a2 * m[9] + a3 * m[13];
        const float dz = a0 * m[2] + a1 * m[6] + a2 * m[10] + a3 * m[14];
        float b0 = F32(W, 0x30), b1 = F32(W, 0x34), b2 = F32(W, 0x38);
        const float b3 = F32(W, 0x3c);
        if (rx_ms_on()) { const float k = rx_ms_gate_slots(); b0 *= k; b1 *= k; b2 *= k; }   /* MAP SCALE (port): the slots follow the scaled gate */
        const float px = b0 * m[0] + b1 * m[4] + b2 * m[8] + b3 * m[12];
        const float py = b0 * m[1] + b1 * m[5] + b2 * m[9] + b3 * m[13];
        const float pz = b0 * m[2] + b1 * m[6] + b2 * m[10] + b3 * m[14];
        SF32(L, 0x68, dz); SF32(L, 0x14, b2); SF32(L, 0x2c, b1);
        slot = U32(L, 0x18);
        SF32(slot, -8, px); SF32(slot, -4, py); SF32(slot, 0, pz);
        SF32(slot, 8, dx); SF32(slot, 0xc, dy); SF32(slot, 0x10, dz);
        float r = F32(A, 0xa70) + F32(A, 0xa6c);
        if (rx_ms_on()) r *= rx_ms_scale();   /* MAP SCALE (port): the search radius along the scaled track */
        {
            const rx_addr sp = rx_sp();
            rx_set_sp(sp - 0x18);
            rx_track_nearest_in_radius(slot - 8, A + 0x890, L + 0x34, r, 0, 0);
            rx_set_sp(sp);
        }
        SU32(slot, 0x1c, U32(L, 0x38));
        SU32(slot, 0x18, U32(L, 0x34));
        SU32(slot, 0x24, 1);
        SU32(L, 0x18, slot + 0x30);
        if (i >= 0x10) {
            result = U32(L, 0x20);
            rx_grid_rows(A, gate, 0x10);   /* PORT ADDITION (w-secondrow): riders past the 16 slots behind the gate (never with <= 16 riders) */
            break;
        }
    }
    if (act_vcall(U32(L, 0x24), 0x58, 0, 0) == 3) SU32(U32(gate, 0x100), 0x4c, 0);   /* vf22: the entity's type */
    ACT_END(L);
    return result;
}

/* 005666d0 (stdcall activity; RET 4): the start grid of the gate modes: 16 slots (+0x880), no free start (+0x884 0); the types of
 * 0x2 / 0x3 / 0x11 .. 0x13 keep the slots at the track start (+0x8c8 the section of each of the 28); with "TO_StartingGate" in the
 * level's base subscene ("<level>Base", 007d2da0) the gate's slots (005663b0); else slots spaced along the track's start line (the
 * flagged-4 sections) or the start node (not in the reference events) */
void rx_race_grid_from_gate(rx_addr A) {
    ACT_FRAME_EBP(L, 16, 0x174, 3);
    SU32(L, 0x17c, U32(0, 0x009b45e4) ^ (L + 0xc));                    /* the /GS cookie */
    SU32(L, 0x1c, U32(U32(A, 0x234), 0x20138));
    uint32_t t = U32(A, 0x23c);
    SU32(A, 0x880, 0x10);
    SU32(A, 0x884, 0);
    if (t == 0xc) ACT_CALLR(t, 1, rx_race_track_kind(U32(A, 0x234) + 0x20030));   /* the section race takes the grid of its level's track kind (0056a120) */
    switch (t) {
    case 2: case 3: case 0x11: case 0x12: case 0x13:
        for (uint32_t i = 0; i < 0x1c; i++) SU32(A, 0x360 + 0x30 * i, U32(A, 0x8c8));
        ACT_END(L);
        return;
    default: break;
    }
    if(fmt_modtrack_racing()) {
        const float* g=fmt_modtrack_gate();const float scale=rx_ms_scale();
        const float width=g[9]*scale;
        int columns=(int)(width/6.f);if(columns<2)columns=2;if(columns>16)columns=16;
        const rx_addr sec=U32(U32(A+0x890,0x130),4), node=U32(sec,0);
        for(uint32_t i=0;i<RX_RC_MAX;i++) {
            const rx_addr slot=rx_start_slot(A,i);
            /* The source gate is static scenery. Stage beyond its collision bars;
             * Reflex's countdown still holds every rider until the start. */
            const float across=((i%columns)-(columns-1)*.5f)*6.f, back=-32.f+12.f*(i/columns);
            const float x=g[0]*scale+g[6]*across-g[3]*back,z=g[2]*scale+g[8]*across-g[5]*back;
            SF32(slot,0,x);SF32(slot,4,g[1]);SF32(slot,8,z);
            /* Sample the same uniformly scaled terrain used by native physics. */
            float gx=fminf(2047,fmaxf(0,(x/scale+2047)*.5f)),gz=fminf(2047,fmaxf(0,(2047-z/scale)*.5f));
            int ix=(int)gx,iz=(int)gz;SF32(slot,4,fmt_modtrack_height((uint32_t)iz*2048+ix)*scale);
            for(int k=0;k<3;k++)SF32(slot,0x10+4*k,g[3+k]);
            SU32(slot,0x20,sec);SU32(slot,0x24,node);SU32(slot,0x2c,1);
        }
        SU32(A,0x880,RX_RC_MAX);ACT_END(L);return;
    }
    if (U32(0, 0x00af645c) == 0) {
        const rx_addr buf = L + 0x70;
        SU8(buf, 0, 0);
        for (uint32_t i = 1; i < 0x104; i++) SU8(buf, i, 0);
        {
            const rx_addr sp = rx_sp();
            SU32(sp, -8, 0x0093f350); SU32(sp, -4, U32(A, 0x234) + 0x20030);
            rx_set_sp(sp - 12);
            rx_format_buffer(buf, 0x104, 0x0093f350 /* "%sBase" */, sp - 4);
            rx_set_sp(sp);
        }
        rx_addr gate;
        ACT_CALLR(gate, 2, rx_scene_manager_find_named(U32(L, 0x1c), 0x00942060 /* "TO_StartingGate" */));
        if (gate) {
            uint32_t ok;
            ACT_CALLR(ok, 0, race_grid_gate_nodes(A, gate));
            if (ok) { ACT_END(L); return; }
        }
    }
    act_unreached(0x005667e0, "a start grid without a starting gate (the start line / start node paths)");
    ACT_END(L);
}
RX_FUNCTION(0x005666d0, rx_race_grid_from_gate)

/* 00564c90 (EDI = activity): the start of the free / mode-0 races (two slots at the start node, 12 slots on random track nodes:
 * 00538310 and the game's LCG) -- not in the reference events */
void rx_race_grid_from_track(rx_addr A) {
    (void)A;
    act_unreached(0x00564c90, "the mode 0 / 2 start grid (00564c90)");
}
RX_FUNCTION(0x00564c90, rx_race_grid_from_track)

/* ======================================================================================================================== */
/* MXGame::Update's load checks                                                                                              */

/* FUN_004df6a0 (EAX = key) -> EAX: the int of the startup table's first column's row of the key (+1), or -1 (the reads only) */
uint32_t rx_startup_table_value(uint32_t key) {
    ACT_FRAME(L, 8, 1);
    SU32(L, 8, key);
    if (((U32(0, 0x00ae0d8c) - U32(0, 0x00ae0d88)) >> 2) == 0) crt_invalid_parameter_noinfo();
    rx_addr r;
    ACT_CALLR(r, 1, rx_tbl_row(U32(U32(0, 0x00ae0d88), 0), I32(L, 8)));
    uint32_t v = 0xffffffffu;
    if (U32(r, 8) != 0 && U32(r, 0) != 0xffffffffu) {
        rx_addr p;
        ACT_CALLR(p, 1, rx_tbl_cell_word(U32(r, 8), 1, U32(r, 0)));
        v = U32(p, 0);
    }
    ACT_END(L);
    return v;
}
RX_FUNCTION(0x004df6a0, rx_startup_table_value)

/* MXGame_UpdateLoadWait 0075d760 (stdcall game, dt; RET 8) -> the wait flag: the online modes' load timers (not core: the
 * network session); offline nothing waits */
uint32_t rx_game_update_load_wait(rx_addr g, float dt) {
    ACT_FRAME(L, 4, 4);
    (void)g; (void)dt;
    ACT_CALL(0, rx_startup_table_value(0xd));
    ACT_CALL(0, rx_startup_table_value(0x13));
    uint32_t pm;
    ACT_CALLR(pm, 0, rx_game_player_mode());
    if (pm == 2 || pm == 3) act_unreached(0x0075d792, "the online load wait");
    ACT_END(L);
    return 0;
}
RX_FUNCTION(0x0075d760, rx_game_update_load_wait)

/* FUN_0054e2e0 (EAX = player, ECX = the front end's world): the player's vehicle and rider are loaded (+0x1c4 / +0x1c0 negative
 * request handles, +0x1ac / +0x1b4 built; a UI player only its handles) */
static uint32_t player_loaded(rx_addr P, rx_addr uiw) {
    if (U32(P, 0x10) == 0 || U32(uiw, 0x20018) == 0) {
        if (U32(P, 0x1c4) > 0x7fffffffu && U32(P, 0x1c0) > 0x7fffffffu && U32(P, 0x1ac) != 0 && U32(P, 0x1b4) != 0) return 1;
    } else if (U32(P, 0x1c4) > 0x7fffffffu && I32(P, 0x1c0) < 0) return 1;
    return 0;
}
/* MXGame_CanBeginWorldLoad 0075f500 (stdcall game; RET 4): no pending destruction (+0x15b50), the Player List's revision seen
 * (0x00ae0e70 = +0x15b30), every local human's vehicle and rider loaded, and the session's gate (006df120 ESI = 0x00da6ca0:
 * network; the product's offline answer is "ready") */
uint32_t rx_game_can_begin_world_load(rx_addr g) {
    ACT_FRAME(L, 0, 3);
    uint32_t r = 0;
    if (U32(g, 0x15b50) == 0 && U32(0, 0x00ae0e70) == U32(g, 0x15b30)) {
        r = 1;
        for (uint32_t i = 0; i < U8(g, 0x1541c); i++) {
            rx_addr P;
            ACT_CALLR(P, 2, rx_player_active(g + 0x7460, i, 3));
            if (U32(P, 0x10) == 0 && !player_loaded(P, U32(g, 0x20c8))) { r = 0; break; }
        }
        if (r) {
            if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
#ifdef RX_ORACLE
            rx_regs in, out; memset(&in, 0, sizeof in); in.r[6] = 0x00da6ca0;
            rx_call_original_ex(0x006df120, &in, &out, 0, 0);
            r = (out.r[0] & 0xff) != 0;
#endif
        }
    }
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x0075f500, rx_game_can_begin_world_load)

/* FUN_0075f2b0 (stdcall game; RET 4) -> every player is ready (state 6): offline, the debug override [0x00d611f8] answers with the
 * UI's +0x280, a quitting UI (+0x288) with 0, else the UI's loading page waits for a controller's accept key (00599110: UI input;
 * the product's front end has accepted: ready) */
uint32_t rx_game_players_ready(rx_addr g) {
    ACT_FRAME(L, 0x28, 4);
    (void)g;
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
    const rx_addr s = U32(0, 0x00da6cb0);
    if (U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1) act_unreached(0x0075f2fb, "the online players' readiness");
    uint32_t r;
    const rx_addr ui = U32(0, 0x00a93fcc);
    if (U32(0, 0x00d611f8) != 0) r = U32(ui, 0x280);
    else if (U32(ui, 0x288) != 0) r = 0;
    else r = act_ui_call(0x00599110, RX_CDECL, 0, 0, 0) < 0x80000000u;
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x0075f2b0, rx_game_players_ready)

/* ======================================================================================================================== */
/* the grid pass 0075f890 (the players' start zones)                                                                         */

/* the network session's start-zone bookkeeping (Quazal: 006f70c0 (cdecl variant), 006f6f70, 006f71e0 (ESI = unique id, EDI =
 * the UniqueId column; cdecl zone)): not core -- the oracle runs the original, the product nothing (no race code reads it) */
static void net_zone_variant(uint32_t v) { const uint32_t a[1] = {v}; act_ui_call(0x006f70c0, RX_CDECL, 0, 1, a); }
static void net_zones_reset(void) { act_ui_call(0x006f6f70, RX_CDECL, 0, 0, 0); }
static void net_zone_set(uint32_t uid, uint32_t col, uint32_t zone) {
    rx_regs in, out; memset(&in, 0, sizeof in); in.r[6] = uid; in.r[7] = col;
    const uint32_t a[1] = {zone};
    act_ui_call_ex(0x006f71e0, &in, &out, 1, a);
}
/* the Player List's "Active" cell of a row, read as 0075f890 inlines it (the column's int data under the table lock 0x00ae1c44) */
static uint32_t player_active_cell(uint32_t row) {
    const rx_addr t = ACT_PLAYER_LIST;
    const uint32_t c = rx_tbl_column_index(t, 0x0093def8 /* "Active" */);
    if (c >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) crt_invalid_parameter_noinfo();
    const rx_addr col = U32(U32(t, 0x10), 4 * c);
    loop_enter_cs(0x00ae1c44);
    const rx_addr d = U32(col, 0x40);
    if (row >= (U32(d, 0x10) - U32(d, 0xc)) >> 2) crt_invalid_parameter_noinfo();
    const rx_addr cell = U32(d, 0xc) + 4 * row;
    loop_leave_cs(0x00ae1c44);
    return U32(cell, 0);
}
/* One existing generator kernel; split the seed operation so the original
 * DIV-before-count order of75fb49..75fb6e is preserved, including its trap. */
static uint32_t game_rand15_seed(void) {
    const uint32_t s = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, s);
    return (s >> 16) & 0x7fff;
}
static uint32_t game_rand15(void) {
    const uint32_t value = game_rand15_seed();
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    return value;
}

/* FUN_004defa0 (cdecl type, zone): the activity table's row of the type gets MiniGameStartZone = zone (the table's revision
 * +0x68 counted when it changes) -> 1, or 0 without such a row */
static uint32_t activity_start_zone_set(uint32_t type, uint32_t zone) {
    ACT_FRAME(L, 0, 5);                                                /* push ecx / ebx / ebp / esi / edi; the zone argument slot at L + 0x1c */
    const rx_addr t = ACT_ACTIVITY_TBL;
    SU32(L, 0x1c, zone);
    uint32_t r = 0;
    for (uint32_t i = 0;; i++) {
        if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) break;
        if (i >= rx_tbl_row_count(t)) break;
        if (act_cell(t, 0x0093dd90 /* "ActivityType" */, i) != type) continue;
        const uint32_t c = rx_tbl_column_index(t, 0x0093dfd4 /* "MiniGameStartZone" */);
        if (zone != U32(rx_tbl_cell_word(t, c, i), 0)) SU32(t, 0x68, U32(t, 0x68) + 1);
        if (c >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        ACT_CALL(3, rx_tbl_set_int(U32(U32(t, 0x10), 4 * c), i, L + 0x1c));
        r = 1;
        break;
    }
    ACT_END(L);
    return r;
}

/* FUN_0075f890 (stdcall game, activity type, mode, assign; RET 0x10): the start zones of the event's players (the Player List's
 * "StartZone"): the mini-game variant (game rand; 004defa0, the session), then per type: the free-ride types (5 / 8 / 0xb / 0x14)
 * the row index; the races (the default case) a random first zone (rand & 0xf) stepped by round(16 / rows) (mode 0 / 2: the
 * humans and the AI counted apart; mode 3: zone 0) */
void rx_game_grid_pass(rx_addr g, uint32_t type, uint32_t mode, uint32_t assign) {
    ACT_FRAME_EBP(L, 16, 0x74, 3);
    if (type > 0x21 && (int32_t)type <= 0x3d) { ACT_END(L); return; }
    if (!(U8(0, 0x009e275c) & 1)) {   /* 0075f8b2: the series manager's static guard (w-rng: reached by the front end's first grid pass,
                                         * MXGame_SwitchToUIWorld, before anything else made the manager) */
        SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
        ACT_CALL(0, (void)rx_series_manager_construct());              /* MXSeriesManager_Construct 005b2370 */
        RX_ATEXIT(0x00911da0);                                          /* _atexit */
    }
    if (U32(0, 0x009e2318) == 2 || U32(0, 0x009e2318) == 1) ACT_CALLR(type, 1, rx_career_current_event_type(0x009e2188));
    const uint32_t rows = rx_tbl_row_count(ACT_PLAYER_LIST);
    SU32(L, 0x14, rows);
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
    for (uint32_t o = 0x40; o < 0x80; o += 4) SU32(L, o, 0);              /* the zones taken */
    rx_addr vec;
    ACT_CALLR(vec, 1, rx_reflex_aligned_alloc(0x10));
    SU32(L, 0x2c, vec); SU32(L, 0x30, 0); SU32(L, 0x34, 4); SU32(L, 0x38, 4); SU32(L, 0x3c, 0x7fffffff);
    uint32_t variants = 1;
    if (type == 9 || type == 10) {
        /* 0075f99a .. 0075fa09: the mini-game variants: the entities of type 0x1017 of the current world's scene (the event world
         * +0x5a54 when the engine's scene slot +0x20 is active, else the front end's +0x20c8 with slot +0x1c) collected into the
         * vector (007d25c0, EAX = scene, ESI = the vector); their number when there are any. The Free Ride challenges' Countdown exit
         * (0057c2f0) passes type 9. */
        const rx_addr core = U32(0, 0x00a95844);
        rx_addr world = 0;
        if (U32(U32(core, 0x20), 0x34) == 1) world = U32(g, 0x5a54);
        else if (U32(U32(core, 0x1c), 0x34) == 1) world = U32(g, 0x20c8);
        ACT_CALL(1, rx_scene_query_typed(U32(world, 0x20138), L + 0x2c, 0x1017));
        if (U32(L, 0x30) != 0) variants = U32(L, 0x30);
    }
    uint32_t v = game_rand15() % variants;
    if ((int32_t)variants > 1) {   /* 0075fa3f: redrawn (at most 10000 more times) while it repeats the last variant [0x00da6738] */
        int32_t tries = 0;
        while (U32(0, 0x00da6738) == v) {
            v = game_rand15() % variants;
            tries++;
            if (tries > 0x2710) break;
        }
    }
    SU32(0, 0x00da6738, v);
    ACT_CALL(2, activity_start_zone_set(type, v));
    net_zone_variant(v);
    if (assign) {
        net_zones_reset();
        net_zone_variant(v);
        if (mode == 6) act_unreached(0x0075fab0, "the mode 6 start zones (0075f640)");
        const rx_addr t = ACT_PLAYER_LIST;
        switch (type) {
        case 5: case 8: case 0xb: case 0x14:   /* the free rides: the row index (offline) */
            for (uint32_t r = 0; r < rows; r++) {
                if (act_cell(t, 0x0093def8 /* "Active" */, r) == 0) continue;
                if (mode == 3) act_unreached(0x0075fcbb, "the free ride's mode 3 start zones");
                const uint32_t uid = act_cell(t, 0x0093df0c /* "UniqueId" */, r);
                if ((int32_t)uid <= -1) continue;
                const rx_addr s = U32(0, 0x00da6cb0);
                const int online = U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1;
                SU32(L, 0x1c, online ? uid : r);
                ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00 /* "StartZone" */, r, L + 0x1c));
                net_zone_set(uid, rx_tbl_column_index(t, 0x0093df0c), online ? uid : r);
            }
            break;
        case 2: case 3: case 6: case 0x11: case 0x13: {
            /* Jump table7602f8[0] ->75fad4, shared by Powercross type19.
             * This is distinct from the default16-zone coprime-step branch. */
            SU32(L, 0x18, 0x10); SU32(L, 0x1c, 0);
            if (mode == 0 || mode == 2) {
                /* Only this mode pair tests Active. Humans (PlayerType==1)
                 * consume16+, AI consume0+, each counter advanced after set. */
                for (uint32_t r = 0; r < rows; ++r) {
                    if (act_cell(t, 0x0093def8, r) == 0) continue;
                    const uint32_t at = act_cell(t, 0x00936ef4, r) == 1 ? 0x18 : 0x1c;
                    const uint32_t zone = U32(L, at);
                    SU32(L, 0x24, zone);
                    ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00, r, L + 0x24));
                    SU32(L, at, zone + 1u);
                }
            } else if (mode == 3) {
                /*75faf8..75fb42 intentionally includes inactive/negative-UID
                 * rows and publishes zero to session bookkeeping as well. */
                SU32(L, 0x24, 0);
                for (uint32_t r = 0; r < rows; ++r) {
                    ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00, r, L + 0x24));
                    const uint32_t col = rx_tbl_column_index(t, 0x0093df0c);
                    const uint32_t uid = U32(rx_tbl_cell_word(t, col, r), 0);
                    net_zone_set(uid, col, 0);
                }
            } else {
                const uint32_t draw = game_rand15_seed();
                /*75fb6c performs unsigned DIV before its zero-row branch.
                 * Preserve the original fault, not an invented empty success. */
                if (rows == 0) { raise(SIGFPE); abort(); }
                SU32(L, 0x18, draw % rows);
                SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
                for (uint32_t r = 0; r < rows; ++r) {
                    const uint32_t zone = U32(L, 0x18);
                    SU32(L, 0x24, zone);
                    ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00, r, L + 0x24));
                    const uint32_t col = rx_tbl_column_index(t, 0x0093df0c);
                    const uint32_t uid = U32(rx_tbl_cell_word(t, col, r), 0);
                    if ((int32_t)uid > -1) net_zone_set(uid, col, U32(L, 0x18));
                    SU32(L, 0x18, (U32(L, 0x18) + 1u) % rows);
                }
            }
            break;
        }
        case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1e:
            /* 0076002d: the Free Ride challenges (BigAir .. Trailblazer, Wheelie): the Player List's row 0 starts in zone 0 */
            SU32(L, 0x24, 0);
            ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00 /* "StartZone" */, 0, L + 0x24));
            break;
        case 9: case 10: case 0xe: case 0xf:
            /* 0075ff6e (jump table 0x7602f8[2]): the mini games (Tag, Snake, Infection, EggHunt; w-partymodes): every row (Active is not
             * read) draws a zone (game rand % rows, a signed idiv), stepped on (+1 mod rows, at most rows times) past the zones taken
             * (L + 0x40); the row's StartZone, the zone taken, the session told for a row with a unique id */
            for (uint32_t r = 0; r < rows; r++) {
                SU32(L, 0x18, r);
                uint32_t z = (uint32_t)((int32_t)game_rand15_seed() % (int32_t)rows);
                SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
                for (uint32_t tries = 0; U32(L, 0x40 + 4 * z) != 0 && tries < rows; tries++) z = (z + 1) % rows;
                SU32(L, 0x24, z);
                ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00 /* "StartZone" */, r, L + 0x24));
                SU32(L, 0x40 + 4 * z, 1);
                const uint32_t col = rx_tbl_column_index(t, 0x0093df0c /* "UniqueId" */);
                const uint32_t uid = U32(rx_tbl_cell_word(t, col, r), 0);
                if ((int32_t)uid > -1) net_zone_set(uid, col, z);
            }
            SU32(L, 0x18, rows);
            break;
        case 0x10:
            act_unreached(0x0075fd87, "the Capture start zones (jump table 0x7602f8[3])");
            break;
        default: {   /* the races */
            const double q = (double)F32(0, 0x009ddfdc) / (double)rows;   /* fild / fdivr (53-bit) */
            const float qf = (float)q;
            int32_t step0 = (int32_t)0x80000000u;                          /* (_ftol2_sse of an infinity: no rows) */
            if (rows != 0) step0 = (int32_t)((double)qf + 0.5);            /* copysign(0.5, q) + qf, _ftol2_sse */
            uint32_t step = (uint32_t)rx_cvtt((float)step0);
            if (step == 0) step = 1;   /* PORT ADDITION (w-rc-core): 33+ rows round the step to 0 and the original divides by zero (16 % 0); the
                                        * cap is 32 riders, so only a Player List with more rows than riders reaches it (never at <= 32 rows) */
            while (0x10u % step == 0) step++;                              /* (the step must not divide 16) */
            SU32(L, 0x1c, step);
            uint32_t zone = game_rand15() & 0xf;
            SU32(L, 0x18, zone);
            SU32(L, 0x20, 0x10); SU32(L, 0x24, 0);
            for (uint32_t r = 0; r < rows; r++) {
                if (player_active_cell(r) == 0) continue;
                if (mode == 0 || mode == 2) {   /* humans from zone 16 up, the AI from 0 */
                    const uint32_t k = act_cell(t, 0x00936ef4 /* "PlayerType" */, r) == 1 ? 0x20 : 0x24;
                    SU32(L, 0x28, U32(L, k));
                    ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00 /* "StartZone" */, r, L + 0x28));
                    SU32(L, k, U32(L, k) + 1);
                } else if (mode == 3) {
                    SU32(L, 0x28, 0);
                    ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00, r, L + 0x28));
                    net_zone_set(act_cell(t, 0x0093df0c, r), rx_tbl_column_index(t, 0x0093df0c), 0);
                } else {
                    zone = U32(L, 0x18);
                    SU32(L, 0x28, zone);
                    ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093df00 /* "StartZone" */, r, L + 0x28));
                    SU32(L, 0x40 + 4 * zone, 1);
                    const uint32_t uid = act_cell(t, 0x0093df0c /* "UniqueId" */, r);
                    if ((int32_t)uid > -1) net_zone_set(uid, rx_tbl_column_index(t, 0x0093df0c), U32(L, 0x18));
                    SU32(L, 0x18, (U32(L, 0x18) + U32(L, 0x1c)) & 0xf);
                }
            }
            break;
        }
        }
    }
    if (U8(0, 0x00da67d8) != 0) { loop_enter_cs(0x00d61704); if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704); }
    ACT_CALL(1, rx_crt_aligned_free(U32(L, 0x2c)));
    if (U8(0, 0x00da67d8) != 0) { loop_leave_cs(0x00d61704); if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704); }
    ACT_END(L);
}
RX_FUNCTION(0x0075f890, rx_game_grid_pass)

/* ======================================================================================================================== */
/* the race's start 0075e020                                                                                                 */

/* the session's presence of a controller (PCSession::vf12 006e9e00: thiscall [0x00da6cb0]; controller, key, value): online
 * sessions only (an offline session writes nothing) -- not core: the oracle runs the original, the product nothing */
static void session_presence(uint32_t ctrl, uint32_t key, uint32_t value) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
    const rx_addr s = U32(0, 0x00da6cb0);
    const uint32_t a[3] = {ctrl, key, value};
    act_ui_call(U32(U32(s, 0), 0x30), RX_THISCALL, s, 3, a);
}
/* FUN_0075e020 (cdecl): for every local human row of the Player List the session's presence (the activity type 0xc, the player
 * mode 8, the activity's presence kind 0: 1 / 2 / 5 / 6 / 8 by type and online) */
void rx_game_race_start(void) {
    ACT_FRAME(L, 0xc, 4);
    uint32_t pm;
    ACT_CALLR(pm, 0, rx_game_player_mode());
    SU32(L, 0x14, pm);
    const uint32_t multi = pm != 1;
    SU32(L, 0x18, multi);
    const rx_addr t = ACT_PLAYER_LIST;
    for (uint32_t r = 0;; r++) {
        if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) break;
        if (r >= rx_tbl_row_count(t)) break;
        if (act_cell(t, 0x00936ef4 /* "PlayerType" */, r) != 0 || act_cell(t, 0x0093de90 /* "PlayerLocal" */, r) == 0) continue;
        const uint32_t type = act_cell(t, 0x0093dd90 /* "ActivityType" */, r);
        const uint32_t ctrl = act_cell(t, 0x0093ded0 /* "Controller" */, r);
        session_presence(ctrl, 0xc, type);
        session_presence(ctrl, 8, pm);
        uint32_t kind;
        int32_t net = -1;
#ifdef RX_ORACLE
        if (multi) net = (int32_t)rx_call_original(0x006f8760, RX_CDECL, 0, 0, 0);   /* (the network session's players) */
#endif
        if (multi && net >= 0) kind = 4;
        else {
            if (!(U8(0, 0x009e275c) & 1)) act_unreached(0x005b2370, "the series manager's static initialiser (MXSeriesManager_Construct)");
            if (U32(0, 0x009e2318) == 2 || U32(0, 0x009e2318) == 1) {
                uint32_t presence; ACT_CALLR(presence, 0, rx_career_series_presence(0x009e2188));
                session_presence(ctrl, 0xa, presence); kind = 3;
            } else {
            if (pm == 2) session_presence(ctrl, 8, 4);   /* (through 006de1c0 + 0x10: the same session) */
            switch (type) {
            case 0: case 1: case 2: case 3: case 4: case 6: case 7: case 9: case 10: case 0xb: case 0xc: case 0x11: case 0x12: case 0x13:
                kind = multi * 4 + 1; break;
            case 5: case 0x14: kind = multi ? 5 : 8; break;
            default: kind = multi * 4 + 2; break;
            }
            }
        }
        session_presence(ctrl, 0, kind);
    }
    ACT_END(L);
}
RX_FUNCTION(0x0075e020, rx_game_race_start)
