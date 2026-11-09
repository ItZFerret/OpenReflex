/* stop_audit_trace_hooks.c -- DEVELOPMENT-ONLY exports for the audit drivers (stop_audit_run.c); linked by stop_audit_trace.sh into a
 * copy of librxcore whose rx_call_original(_ex) log "RXTRACE-UNPORTED fn=... caller_off=..." and return instead of aborting.
 * rxtrace_census: counts the vtable words in the arena; rxtrace_try_trick: starts an aerial trick on the airborne local rider through
 * the game's own 006c98c0; rxtrace_set_state: the local activity's SetState; rxtrace_poke / rxtrace_peek: write / read a game-memory word; rxtrace_activity: the local player's activity. Never in the product. */
#include "rx_mem.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* class census: counts the 4-byte aligned words of the arena that equal a vtable address (vtable list: one hex address per line) */
__attribute__((visibility("default"))) void rxtrace_census(const char* vtfile, const char* out) {
    FILE* f = fopen(vtfile, "r"); if (!f) return;
    static uint32_t vt[4096]; int n = 0; unsigned x;
    while (n < 4096 && fscanf(f, "%x", &x) == 1) vt[n++] = x;
    fclose(f);
    static uint32_t cnt[4096];
    for (uint64_t a = 0x00400000u; a < 0x7ff00000u; a += 4) {
        uint32_t w = *(volatile uint32_t*)(rx_base + a);
        if (w < 0x00900000u || w > 0x00a00000u) continue;
        int lo = 0, hi = n - 1;
        while (lo <= hi) { int m = (lo + hi) / 2; if (vt[m] == w) { cnt[m]++; break; } if (vt[m] < w) lo = m + 1; else hi = m - 1; }
    }
    FILE* o = fopen(out, "w"); if (!o) return;
    for (int i = 0; i < n; i++) if (cnt[i]) fprintf(o, "%08x %u\n", vt[i], cnt[i]);
    fclose(o);
}

/* development: start a trick on the local player's tracker while airborne (the game's own 006c98c0 with the aerial mapping's record) */
extern uint32_t rx_fx_trick_find(uint32_t name, uint32_t kind);
extern void rx_fx_trick_start(uint32_t K, uint32_t trick);
static uint32_t rxt_local_player(void) {
    const uint32_t g = rx_rd32(0xa946b8); if (!g) return 0;
    const uint32_t n = rx_rd8(g + 0x1541c);
    for (uint32_t k = 0; k < n && k < 8; k++) {
        const uint32_t slot = rx_rd32(g + 0x153ec + 4u * k); if (slot >= 16) continue;
        const uint32_t p = g + 0x7460 + slot * 0x12a0;
        if (rx_rd32(p + 0x10) == 0) return p;
    }
    return 0;
}
__attribute__((visibility("default"))) int rxtrace_try_trick(const char* name) {
    const uint32_t p = rxt_local_player(); if (!p || !rx_rd32(p + 0x1ac)) return 0;
    const uint32_t K = p + 0xb58;
    if (!rx_rd32(K + 0x88) || rx_rd32(K + 8) != 0 || rx_rd32(rx_rd32(p + 0x1ac) + 0x280) != 0) return 0;
    const uint32_t sp = rx_sp(), nm = sp - 0x40; rx_set_sp(nm);
    uint32_t i = 0; for (; name[i]; i++) rx_wr8(nm + i, (uint8_t)name[i]); rx_wr8(nm + i, 0);
    const uint32_t rec = rx_fx_trick_find(nm, 1); rx_set_sp(sp);
    if (!rec) return -1;
    rx_fx_trick_start(K, rec);
    return 1;
}

extern void rx_activity_set_state(uint32_t act, uint32_t state);
__attribute__((visibility("default"))) int rxtrace_set_state(int state) {
    const uint32_t p = rxt_local_player(); if (!p) return 0;
    const uint32_t A = rx_rd32(p + 0x94); if (!A) return 0;
    const uint32_t sp = rx_sp(); rx_set_sp(sp - 8);
    rx_activity_set_state(A, (uint32_t)state);
    rx_set_sp(sp);
    return 1;
}

__attribute__((visibility("default"))) void rxtrace_poke(unsigned addr, unsigned val) { rx_wr32(addr, val); }

__attribute__((visibility("default"))) uint32_t rxtrace_peek(uint32_t addr) { return rx_rd32(addr); }
__attribute__((visibility("default"))) uint32_t rxtrace_activity(void) { const uint32_t p = rxt_local_player(); return p ? rx_rd32(p + 0x94) : 0; }

/* development: the live activity's teardown as the next event / career "Continue" runs it: vf3, vf6(0) (the area's entities), vf0(1)
 * (the scalar deleting destructor); the same sequence as core/oracle/dtor_probe.cpp case 'a'. Returns 1 when it ran. */
#include "rx_vcall.h"
__attribute__((visibility("default"))) int rxtrace_teardown(void) {
    const uint32_t p = rxt_local_player(); if (!p) return 0;
    const uint32_t A = rx_rd32(p + 0x94); if (!A) return 0;
    const uint32_t zero = 0, one = 1;
    const uint32_t sp = rx_sp(); rx_set_sp(sp - 16);
    rx_vcall(A, 0xc, 0, NULL);
    rx_vcall(A, 0x18, 1, &zero);
    rx_vcall(A, 0, 1, &one);
    rx_set_sp(sp);
    return 1;
}

/* development: every player of the game's players block (game + 0x7460, 12 slots of 0x12a0, the in-use flags +0xdf80) leaves its slot as the
 * original's player sync does for a Player List row that went inactive (0075ba20 -> 00552c60 -> MXPlayer vf0 -> 00549830: the handlers, the
 * vehicle (0054a500: its damage manager entry 0047bbe0 -> 00479dd0, the part entities deleted), the stunt system). Run after rxtrace_teardown it
 * is the next event's / the activity area re-creation's player removal. Returns the number of players freed. */
extern void rx_player_slot_free(uint32_t players, uint32_t player);
__attribute__((visibility("default"))) int rxtrace_players_free(void) {
    const uint32_t g = rx_rd32(0xa946b8); if (!g) return 0;
    const uint32_t players = g + 0x7460;
    int n = 0;
    for (uint32_t k = 0; k < 12; k++) {
        if (rx_rd8(players + 0xdf80 + k) == 0) continue;
        const uint32_t sp = rx_sp(); rx_set_sp(sp - 16);
        rx_player_slot_free(players, players + k * 0x12a0);
        rx_set_sp(sp);
        n++;
    }
    return n;
}

/* development: every other part of the damage manager's entries activated as a hit does it (0047a630: the part entity into the scene, its body an
 * active kinematic one), so that the teardown takes the "active part" path of 00479dd0 (0047a830). Returns the number of parts activated. */
extern void rx_replay_damage_activate(uint32_t entry, uint32_t index);
__attribute__((visibility("default"))) int rxtrace_dmg_activate(void) {
    int n = 0;
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t e = 0x00db1410u + 0x10 + 0x410 * k;
        if (!rx_rd32(e + 0x3fc)) continue;
        for (uint32_t i = 0; i < rx_rd32(e) && i < 8; i++) {
            if (((i + k) & 1) == 0 || !rx_rd32(e + 0x2c + 4 * i)) continue;
            const uint32_t sp = rx_sp(); rx_set_sp(sp - 16);
            rx_replay_damage_activate(e, i);
            rx_set_sp(sp);
            n++;
        }
    }
    return n;
}

/* development (w-frametail): the tail of the original's frame -- Engine::Frame 0084f260 after Engine::Update: the update counter + 1, the
 * Database's end-of-frame removal (vf33 0084cc80 with the render counter: the pending list the update queued into), the render counter + 1 --
 * run once outside a frame, after rxtrace_players_free (which the original runs inside a frame's update, so its frame ends this way).
 * Returns the handles left in the two pending lists (+0x64 / +0x7c). */
extern void rx_database_drain_removals(uint32_t db, uint32_t frame);
__attribute__((visibility("default"))) int rxtrace_frame_tail(void) {
    const uint32_t ctx = rx_rd32(0x00a95844), eng = rx_rd32(ctx + 0x10), db = rx_rd32(ctx + 8);
    rx_wr32(eng + 0x58, rx_rd32(eng + 0x58) + 1);
    const uint32_t sp = rx_sp(); rx_set_sp(sp - 16);
    rx_database_drain_removals(db, rx_rd32(eng + 0x54));
    rx_set_sp(sp);
    rx_wr32(eng + 0x54, rx_rd32(eng + 0x54) + 1);
    return (int)((rx_rd32(db + 0x74) - rx_rd32(db + 0x70)) / 4 + (rx_rd32(db + 0x8c) - rx_rd32(db + 0x88)) / 4);
}
/* development (w-frametail): the number of handles in a type's handle table (the Database's tag table +0x44 -> the type entry, its name
 * table +0x18: +8 the count), e.g. "model"; -1 when the type is not registered */
extern int rx_tagtab_find(uint32_t t, uint32_t lo, uint32_t hi, uint32_t* value);
extern uint32_t rx_asset_tag_lo(const char* type);
extern uint32_t rx_asset_tag_hi(const char* type);
__attribute__((visibility("default"))) int rxtrace_type_handles(const char* type) {
    const uint32_t db = rx_rd32(rx_rd32(0x00a95844) + 8);
    uint32_t e = 0;
    if (!db || !rx_tagtab_find(db + 0x44, rx_asset_tag_lo(type), rx_asset_tag_hi(type), &e)) return -1;
    return (int)rx_rd32(e + 0x18 + 8);
}
