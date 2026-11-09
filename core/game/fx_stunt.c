/* fx_stunt.c -- the players' stunt (trick) tracking of the 0x6c8xxx..0x6cdxxx range, ticked by MXPlayer_Update (0054af90):
 * the trick tracker K = player +0xb58 (006c9770: the trick input state machine 006c9510, the airborne / landing passes
 * 006c9190 / 006c9380 / 006ca0c0, the per-mode handlers) and the stunt scorer P = player +0xc70 (006cab80). Tricks are looked
 * up by name in the trick table [0xd5aad4] (count 0xd5aad8, 0xc0 records: name, +0x64 kind) and announced through the event
 * system [0xa94304] ("PhysicsStuntBegin" / "PhysicsStuntEnd", trick start / land / fail events). notes/fx.md.
 *
 * Tracker K: +0 enabled, +4 the player (+0x1ac vehicle: +0x194 class, +0x234 controls (+0x2de4 mode, +0x2eb4 / +0x2eb8 trick
 * buttons), +0x280 wrecked; +0x1b4 rider: +0xaf10 state, +0x5b568 animation request, +0x5b57c pose; +0x1f0 pad), +8 mode,
 * +0x1c the current trick, +0x20 its asset holder, +0x28 landed, +0x2c input clock, +0x30 the input buffer (+0x34 / +0x35
 * the last directions), +0x38 / +0x3c / +0x40 / +0x44 / +0x48 / +0x4c input state, +0x5c input state, +0x60 the matched trick,
 * +0x64 physics stunt kind, +0x68 its record, +0x6c / +0x80 stunt clocks, +0x84 jump state, +0x88 airborne, +0x8c air clock,
 * +0x90 blocked, +0x94 the input history ring (15 x 8 at +0x98). */
#include "highlight_progress.h"
#include "fx.h"
#include "fx_util.h"
#include "fx_stunt_input.h"
#include "world_veh.h"
#include "world_wreck.h"
#include "world_misc.h"
#include "world_node.h"
#include "race.h"
#include "loop_player.h"
#include "race_activity.h"
#include "anim.h"
#include "build_asset.h"

/* ---- helpers ---- */
/* the vehicle's vf43 (+0xac: airborne) */
static uint32_t veh_air(rx_addr veh, rx_addr ret) {
    const rx_addr fn = U32(U32(veh, 0), 0xac);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (!f) return rx_call_original(fn, RX_THISCALL, veh, 0, 0);
    const rx_addr s = fx_push(0, 0, ret);
    const uint32_t r = f(veh);
    fx_pop(s);
    return r;
}
static rx_addr K_veh(rx_addr K) { return U32(U32(K, 4), 0x1ac); }
/* an event post as the inline sequences make it: push 0x9e1a7c; ECX = the handler signature's type_info; call
 * type_info::_name_internal_method; push size; push payload; EAX = the event system, EDI = the name; call 004020c0 */
static void post_event(rx_addr evs, rx_addr ti, rx_addr name, rx_addr payload, uint32_t size, rx_addr ret_ti, rx_addr ret_post) {
    fx_type_name(ti, ret_ti);
    const fx_saved sv = fx_save();
    fx_set_reg(FX_EDI, name);
    { const uint32_t a[2] = {payload, size}; const rx_addr s = fx_push(2, a, ret_post); rx_world_event_dispatch(evs, name, payload, size); fx_pop(s); }
    fx_restore(sv);
}

/* ---- 006cd000 (ECX = physics stunt kind 1..5): its name (0x92fbf4.. ; else 0) */
rx_addr rx_fx_stunt_kind_name(uint32_t kind) {
    static const rx_addr names[5] = {0x0092fbf4, 0x0092fbfc, 0x0092fc04, 0x0092fc10, 0x0092fc1c};
    return kind - 1 <= 4 ? names[kind - 1] : 0;
}
RX_FUNCTION(0x006cd000, rx_fx_stunt_kind_name)

/* ---- 006ca070 (EAX = kind): the time a physics stunt must last before it counts (xmm0): 1 / 2 [0x9c3e1c] / [0x9c3e20],
 * 3 / 4 0.5, 5 0, else 0.25 */
float rx_fx_stunt_kind_time(uint32_t kind) {
    switch (kind) {
        case 1: return F32(0, 0x009c3e1c);
        case 2: return F32(0, 0x009c3e20);
        case 3: case 4: return 0.5f;    /* 0x9322cc */
        case 5: return 0.0f;
        default: return 0.25f;          /* 0x92ff84 */
    }
}
RX_FUNCTION(0x006ca070, rx_fx_stunt_kind_time)

/* ---- 006c8570 (cdecl name*, kind): the trick record of that kind whose (lower-cased, 0x9b77c8) name equals the name, or 0.
 * Frame: sub esp, 0x68 (/GS cookie at +0x64); ebx / ebp / edi (esi in the search): E = sp0 - 0x74: +0xc the name (99 chars). */
rx_addr rx_fx_trick_find(rx_addr name, uint32_t kind) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x68, E = e0 - 12;
    const fx_saved sv = fx_save();
    SU32(e0, 0x64, U32(0, 0x009b45e4) ^ e0);
    fx_push_regs(e0, 3, (int[]){FX_EBX, FX_EBP, FX_EDI});
    rx_set_sp(E);
    { const uint32_t a[3] = {E + 0xc, name, 0x63}; const rx_addr s = fx_push(3, a, 0x006c8595); crt_strncpy(E + 0xc, name, 0x63); fx_pop(s); }
    SU8(E, 0x6f, 0);
    for (rx_addr p = E + 0xc; U8(p, 0) != 0; p++) SU8(p, 0, U8(0x009b77c8 + U8(p, 0), 0));
    const int32_t n = I32(0, 0x00d5aad8);
    rx_addr found = 0;
    if (n > 0) {
        SU32(E, -4, sv.esi);
        rx_addr r = U32(0, 0x00d5aad4);
        for (int32_t i = 0; i < n && found == 0; i++, r += 0xc0) {
            if (U32(r, 0x64) != kind) continue;
            rx_addr a = E + 0xc, b = r;
            int diff = 0;
            for (;;) {
                const uint8_t x = U8(a, 0);
                if (x != U8(b, 0)) { diff = 1; break; }
                if (x == 0) break;
                const uint8_t y = U8(a, 1);
                if (y != U8(b, 1)) { diff = 1; break; }
                a += 2; b += 2;
                if (y == 0) break;
            }
            if (!diff) found = r;
        }
    }
    rx_set_sp(e0);
    SU32(e0, -4, 0x006c8637);
    phys_security_check_cookie(U32(e0, 0x64) ^ e0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return found;
}
RX_FUNCTION(0x006c8570, rx_fx_trick_find)
static rx_addr trick_find_call(rx_addr name, uint32_t kind, rx_addr ret) {
    const uint32_t a[2] = {name, kind};
    const rx_addr s = fx_push(2, a, ret);
    const rx_addr r = rx_fx_trick_find(name, kind);
    fx_pop(s);
    return r;
}

/* ---- 006cd750 (ECX = trick, EDX = event name; stdcall player; RET 4): posts {player, trick} (8 bytes) as a trick event (type
 * 0x9cbf38). Frame: sub esp, 0x10 (/GS cookie at +0xc); esi / edi: the payload at sp0 - 0x10 + 4. */
void rx_fx_trick_event(rx_addr player, rx_addr trick, rx_addr name) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x10;
    const fx_saved sv = fx_save();
    SU32(e0, 0xc, U32(0, 0x009b45e4) ^ e0);
    fx_push_regs(e0, 2, (int[]){FX_ESI, FX_EDI});
    const rx_addr evs = U32(0, 0x00a94304);
    fx_set_reg(FX_ESI, evs); fx_set_reg(FX_EDI, name);
    SU32(e0, 8, trick);
    SU32(e0, 4, player);
    rx_set_sp(e0 - 8);
    post_event(evs, 0x009cbf38, name, e0 + 4, 8, 0x006cd784, 0x006cd792);
    rx_set_sp(e0);
    SU32(e0, -4, 0x006cd79f);
    phys_security_check_cookie(U32(e0, 0xc) ^ e0);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cd750, rx_fx_trick_event)
static void trick_event_call(rx_addr player, rx_addr trick, rx_addr name, rx_addr ret) {
    const uint32_t a[1] = {player};
    const rx_addr s = fx_push(1, a, ret);
    rx_fx_trick_event(player, trick, name);
    fx_pop(s);
}

/* ---- 006cd810 (ECX = record, EDX = kind; stdcall name, player; RET 8): posts {player, record, kind} (12 bytes) under the
 * name (type 0x9ce768). Frame: sub esp, 0x10 (/GS cookie at +0xc); esi / edi: the payload at sp0 - 0x10. */
void rx_fx_stunt_event(rx_addr name, rx_addr player, rx_addr rec, uint32_t kind) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x10;
    const fx_saved sv = fx_save();
    SU32(e0, 0xc, U32(0, 0x009b45e4) ^ e0);
    fx_push_regs(e0, 2, (int[]){FX_ESI, FX_EDI});
    const rx_addr evs = U32(0, 0x00a94304);
    fx_set_reg(FX_ESI, evs); fx_set_reg(FX_EDI, name);
    SU32(e0, 4, rec);
    SU32(e0, 0, player);
    SU32(e0, 8, kind);
    rx_set_sp(e0 - 8);
    post_event(evs, 0x009ce768, name, e0, 0xc, 0x006cd84a, 0x006cd858);
    rx_set_sp(e0);
    SU32(e0, -4, 0x006cd865);
    phys_security_check_cookie(U32(e0, 0xc) ^ e0);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cd810, rx_fx_stunt_event)

/* ---- 006ca030 (EAX = tracker): a running physics stunt ends: kind / record / clocks cleared, "PhysicsStuntEnd" (0x942794)
 * posted with the old record and kind. Frame: push ecx. */
void rx_fx_stunt_end(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    const rx_addr rec = U32(K, 0x68);
    const uint32_t kind = U32(K, 0x64);
    if (rec == 0) return;
    SU32(K, 0x64, 0); SU32(K, 0x68, 0);
    SF32(K, 0x6c, 0.0f); SF32(K, 0x80, 0.0f);
    rx_set_sp(sp0 - 4);
    const uint32_t a[2] = {0x00942794, U32(K, 4)};
    const rx_addr s = fx_push(2, a, 0x006ca067);
    rx_fx_stunt_event(0x00942794, a[1], rec, kind);
    fx_pop(s);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006ca030, rx_fx_stunt_end)

/* ---- 006c9fc0 (ESI = tracker, EDX = kind): a physics stunt begins: kind set, clocks 0, its record (006c8570 of the kind's
 * name, list 1 for kinds 3..5) and "PhysicsStuntBegin" (0x9607a0). Frame: push edi. */
void rx_fx_stunt_begin(rx_addr K, uint32_t kind) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.edi);
    rx_set_sp(sp0 - 4);
    SU32(K, 0x64, kind);
    SF32(K, 0x80, 0.0f); SF32(K, 0x6c, 0.0f);
    const rx_addr nm = rx_fx_stunt_kind_name(kind);
    SU32(sp0 - 8, 0, 0x006c9fdb);
    const uint32_t list = (kind >= 3 && kind <= 5) ? 1 : 0;
    fx_set_reg(FX_EDI, list);
    const rx_addr rec = trick_find_call(nm, list, 0x006c9ffc);
    SU32(K, 0x68, rec);
    { const uint32_t a[2] = {0x009607a0, U32(K, 4)}; const rx_addr s = fx_push(2, a, 0x006ca015);
      rx_fx_stunt_event(0x009607a0, a[1], rec, U32(K, 0x64)); fx_pop(s); }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9fc0, rx_fx_stunt_begin)

/* ---- 006c9ed0 (stdcall tracker; RET 4): the physics stunt the controls ask for: 1 / 2 the trick buttons (+0x2eb8 / +0x2eb4)
 * on the ground; airborne the vehicle's stunt state (vf58 +0xe8): 1..6 -> 5 without a current trick or landing, 7 -> 3, 8 -> 4;
 * else 0. Frame: push ebx / esi / edi. */
uint32_t rx_fx_stunt_wanted(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 1, (int[]){FX_EBX});
    fx_set_reg(FX_EBX, K);
    const rx_addr v = K_veh(K);
    fx_push_regs(sp0 - 4, 2, (int[]){FX_ESI, FX_EDI});
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_ESI, v); fx_set_reg(FX_EDI, 0);
    uint32_t r = 0;
    if (U32(U32(v, 0x234), 0x2eb8) != 0 && veh_air(v, 0x006c9efc) == 0) r = 1;
    else if (U32(U32(v, 0x234), 0x2eb4) != 0 && veh_air(v, 0x006c9f27) == 0) r = 2;
    else if (veh_air(v, 0x006c9f44) != 0) {
        const rx_addr fn = U32(U32(v, 0), 0xe8);
        uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
        uint32_t st;
        if (f) { const rx_addr s = fx_push(0, 0, 0x006c9f54); st = f(v); fx_pop(s); }
        else st = rx_call_original(fn, RX_THISCALL, v, 0, 0);
        if (st >= 1 && st <= 6) { if (U32(K, 0x1c) == 0 && U32(K, 0x28) == 0) r = 5; }
        else if (st == 7) r = 3;
        else if (st == 8) r = 4;
    }
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x006c9ed0, rx_fx_stunt_wanted)
static uint32_t wanted_call(rx_addr K, rx_addr ret) {
    const uint32_t a[1] = {K};
    const rx_addr s = fx_push(1, a, ret);
    const uint32_t r = rx_fx_stunt_wanted(K);
    fx_pop(s);
    return r;
}

/* ---- 006ca0c0 (EAX = tracker; stdcall dt; RET 4): the physics stunt state (+0x64): none: a wanted stunt that lasts its time
 * (006ca070) begins; else the clock resets. Ground stunts (1): airborne the air clock (+0x80) runs and ends it past
 * [0x9c3e2c] (also once more after the landing check); landed without the button (or the wanted check) ends it. 2: ends when
 * no longer wanted. 3 / 4 / 5: no longer wanted: past 0.5 / 0.5 / [0xa9430c] it ends, else the trick fails (006cd750
 * 0x960764). Frame: push ecx; push esi. */
void rx_fx_stunt_update(float dt, rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX)); SU32(sp0 - 8, 0, sv.esi);
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_ESI, K);
    const uint32_t st = U32(K, 0x64);
    const rx_addr v = K_veh(K);
#define END(ret) do { const rx_addr s_ = fx_push(0, 0, (ret)); rx_fx_stunt_end(K); fx_pop(s_); } while (0)
    switch (st) {
        case 0: {
            uint32_t kind = 0;
            if (U32(v, 0x280) == 0) kind = wanted_call(K, 0x006ca0ef);
            if (kind == 0) { SF32(K, 0x6c, 0.0f); break; }
            const float c = rx_adds(F32(K, 0x6c), dt);
            SF32(K, 0x6c, c);
            SU32(rx_sp(), -4, 0x006ca10a);
            if (c >= rx_fx_stunt_kind_time(kind)) {
                const rx_addr s = fx_push(0, 0, 0x006ca118);
                rx_fx_stunt_begin(K, kind);
                fx_pop(s);
            }
            break;
        }
        case 1: {
            SF32(K, 0x6c, rx_adds(F32(K, 0x6c), dt));
            int end = 0;
            if (veh_air(v, 0x006ca14d) != 0) {
                const float a = rx_adds(F32(K, 0x80), dt);
                SF32(K, 0x80, a);
                if (a > F32(0, 0x009c3e2c)) end = 1;
            } else {
                uint32_t w;
                if (!(F32(K, 0x80) == 0.0f)) w = U32(U32(K_veh(K), 0x234), 0x2eb8);
                else w = wanted_call(K, 0x006ca19d);
                if (w == 0) end = 1;
            }
            if (end) END(0x006ca1a8);
            if (veh_air(K_veh(K), 0x006ca1bb) != 0) {
                const float a = rx_adds(F32(K, 0x80), dt);
                SF32(K, 0x80, a);
                if (a > F32(0, 0x009c3e2c)) END(0x006ca1ed);
            }
            break;
        }
        case 2:
            SF32(K, 0x6c, rx_adds(F32(K, 0x6c), dt));
            if (wanted_call(K, 0x006ca208) == 0) END(0x006ca217);
            break;
        case 3: case 4: case 5: {
            SF32(K, 0x6c, rx_adds(F32(K, 0x6c), dt));
            const rx_addr r = st == 3 ? 0x006ca232 : st == 4 ? 0x006ca292 : 0x006ca264;
            if (wanted_call(K, r) != 0) break;
            const float lim = st == 5 ? F32(0, 0x00a9430c) : 0.5f;
            if (F32(K, 0x6c) > lim) { END(0x006ca217); break; }
            trick_event_call(U32(K, 4), U32(K, 0x68), 0x00960764, 0x006ca2b9);
            SU32(K, 0x68, 0); SU32(K, 0x64, 0);
            break;
        }
        default: break;
    }
#undef END
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006ca0c0, rx_fx_stunt_update)

/* ---- 006c9480 (ESI = tracker): the trick's follow-up: none: the "no trick" event (0x960738, type 0x9cbeb4, payload the
 * player) and the air clock 0; a trick without a follow-up (+0x64): its list-1 variant (006c8570) becomes the trick and is
 * announced (006cd750 0x960748), or the rider is asked for animation 2. Frame: push ecx / ebx / edi. */
void rx_fx_trick_followup(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    fx_push_regs(sp0 - 4, 2, (int[]){FX_EBX, FX_EDI});
    rx_set_sp(sp0 - 0xc);
    const rx_addr t = U32(K, 0x1c);
    if (t == 0) {
        const rx_addr evs = U32(0, 0x00a94304);
        fx_set_reg(FX_EBX, evs);
        SU32(sp0 - 4, 0, U32(K, 4));
        post_event(evs, 0x009cbeb4, 0x00960738, sp0 - 4, 4, 0x006c94ed, 0x006c9500);
        SF32(K, 0x80, 0.0f);
    } else if (U32(t, 0x64) == 0) {
        const rx_addr nt = trick_find_call(t + 0x44, 1, 0x006c949b);
        if (nt != 0) {
            SU32(K, 0x1c, nt);
            trick_event_call(U32(K, 4), nt, 0x00960748, 0x006c94b5);
        } else SU32(U32(U32(K, 4), 0x1b4), 0x5b568, 2);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9480, rx_fx_trick_followup)

/* ---- 006c9250 (ESI = tracker): landing a trick: none: the "no trick" event (0x9407c4); a running trick (mode +8): when the
 * trick's minimum (holder +0x20 asset +0x30) is reached by the rider's progress (FloatListeners_Get of +0x48c in state 3,
 * else +0x46c): its list-0 variant becomes the trick (+0x14 = 0, announced 0x960758) or the "no trick" event; not reached and
 * no demo ([0xa94308]): the rider is asked for animation 0x17. Frame: push ecx / ebx / edi. */
void rx_fx_trick_land(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    fx_push_regs(sp0 - 4, 2, (int[]){FX_EBX, FX_EDI});
    rx_set_sp(sp0 - 0xc);
    const rx_addr evs = U32(0, 0x00a94304);
    if (U32(K, 0x1c) == 0) {
        fx_set_reg(FX_EBX, evs);
        SU32(sp0 - 4, 0, U32(K, 4));
        post_event(evs, 0x009cbeb4, 0x009407c4, sp0 - 4, 4, 0x006c935d, 0x006c9370);
    } else if (U32(K, 8) != 0) {
        const rx_addr h = U32(K, 0x20);
        SU32(h, 0x4c, 1);
        const float need = F32(U32(h, 0x44), 0x30);
        SU32(U32(K, 0x20), 0x4c, 0);
        const rx_addr rider = U32(U32(K, 4), 0x1b4);
        const rx_addr lst = rider + (U32(rider, 0xaf10) == 3 ? 0x48c : 0x46c);
        SF32(sp0 - 4, 0, need);
        float got;
        { const uint32_t a[1] = {lst}; const rx_addr s = fx_push(1, a, 0x006c92ad); got = rx_world_float_listeners_get(lst); fx_pop(s); }
        if (!(F32(sp0 - 4, 0) > got)) {
            const rx_addr nt = trick_find_call(U32(K, 0x1c) + 0x44, 0, 0x006c92c6);
            if (nt != 0) {
                SU32(K, 0x1c, nt);
                SF32(K, 0x14, 0.0f);
                trick_event_call(U32(K, 4), nt, 0x00960758, 0x006c92e8);
            } else {
                fx_set_reg(FX_EBX, evs);
                SU32(sp0 - 4, 0, U32(K, 4));
                post_event(evs, 0x009cbeb4, 0x009407c4, sp0 - 4, 4, 0x006c9309, 0x006c931c);
            }
        } else if (U32(0, 0x00a94308) == 0) {
            SU32(U32(U32(K, 4), 0x1b4), 0x5b568, 0x17);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9250, rx_fx_trick_land)

/* ---- 006c9380 (EAX = tracker; stdcall dt; RET 4): the jump state (+0x84): 0: taking off (vf43, not already airborne +0x88)
 * counts an attempted jump (MXRecords_AttemptedJump) -> 1; 1: the air clock (+0x8c) runs; a trick with a follow-up or past 0.25
 * s -> the follow-up (006c9480), 2; back on the ground -> 0; 2: landing -> 006c9250, 0. Frame: push ecx; push esi. */
void rx_fx_trick_jump(float dt, rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX)); SU32(sp0 - 8, 0, sv.esi);
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_ESI, K);
    const uint32_t st = U32(K, 0x84);
    if (st == 0) {
        if (veh_air(K_veh(K), 0x006c9445) != 0 && U32(K, 0x88) == 0) {
            SF32(K, 0x8c, 0.0f);
            { const rx_addr s = fx_push(0, 0, 0x006c9471);        /* MXRecords_AttemptedJump (rx_loop) */
              rx_records_attempted_jump(U32(U32(K, 4), 0x1f0), U32(0, 0x00a946c0)); fx_pop(s); }
            SU32(K, 0x84, 1);
        }
    } else if (st == 1) {
        const float c = rx_adds(F32(K, 0x8c), dt);
        SF32(K, 0x8c, c);
        const rx_addr t = U32(K, 0x1c);
        if ((t != 0 && U32(t, 0x64) != 0) || c > 0.25f /* 0x92ff84 */) {
            const rx_addr s = fx_push(0, 0, 0x006c9423);
            rx_fx_trick_followup(K);
            fx_pop(s);
            SU32(K, 0x84, 2);
        } else if (veh_air(K_veh(K), 0x006c940f) == 0) SU32(K, 0x84, 0);
    } else if (st == 2) {
        if (veh_air(K_veh(K), 0x006c93b4) == 0) {
            const rx_addr s = fx_push(0, 0, 0x006c93c1);
            rx_fx_trick_land(K);
            fx_pop(s);
            SU32(K, 0x84, 0);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9380, rx_fx_trick_jump)

/* ---- 006c9190 (ESI = tracker): a wreck (vehicle +0x280) or a rider not in pose 0xc blocks tricks (+0x90 = 1); a trick or a
 * physics stunt in progress is dropped: the fail event (006cd750 0x960764), a failed jump counted when it was a scored trick
 * (+0x64) of a local active player (MXRecords_FailedJump), and the input / trick state reset. Frame: push edi. */
void rx_fx_trick_abort(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.edi);
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_EDI, 0);
    const rx_addr pl = U32(K, 4);
    if (U32(U32(pl, 0x1ac), 0x280) == 0 && U32(U32(pl, 0x1b4), 0x5b57c) == 0xc) { fx_restore(sv); rx_set_sp(sp0); return; }
    SU32(K, 0x90, 1);
    if (U32(K, 0x68) == 0 && U32(K, 0x1c) == 0) { fx_restore(sv); rx_set_sp(sp0); return; }
    trick_event_call(pl, U32(K, 0x1c), 0x00960764, 0x006c91d9);
    const rx_addr t = U32(K, 0x1c);
    if (t != 0 && U32(U32(K, 4), 4) != 0 && U32(U32(K, 4), 0x10) == 0 && U32(t, 0x64) != 0)
        rx_records_failed_jump(U32(U32(K, 4), 0x1f0), U32(0, 0x00a946c0));   /* 00764f80 */
    SU32(K, 0x1c, 0); SU32(K, 8, 0); SU32(K, 0x5c, 1); SU32(K, 0x38, 0xffffffffu); SU32(K, 0x30, 0);
    fx_x87_copy(K + 0x44, K + 0x48);
    SU8(K, 0x34, 0x4e); SU8(K, 0x35, 0x4e);
    SU32(K, 0x3c, 0); SF32(K, 0x40, 0.0f); SU32(K, 0x4c, 0); SU32(K, 0xc, 0); SU32(K, 0x68, 0); SU32(K, 0x64, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9190, rx_fx_trick_abort)

/* ---- 006c9940 (EAX = tracker): whether a ground trick may start now: not during the race intro (game +0x745c +0x288), the
 * vehicle in mode 3 / 4, or fast (>= 10) and airborne with both trick axes (+0x58c / +0x598 of the rider) exactly 1; and no
 * current trick, the rider idle (+0xaf10 = 0), not wrecked and in pose 0xc. Frame: push ebx / esi (edi). */
uint32_t rx_fx_trick_ground_ok(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 2, (int[]){FX_EBX, FX_ESI});
    fx_set_reg(FX_ESI, K); fx_set_reg(FX_EBX, 0);
    uint32_t r = 0;
    if (U32(U32(U32(0, FX_G_GAME), 0x745c), 0x288) == 0) {
        SU32(sp0 - 0xc, 0, sv.edi);
        fx_set_reg(FX_EDI, 0);
        rx_set_sp(sp0 - 0xc);
        const rx_addr pl = U32(K, 4), v = U32(pl, 0x1ac);
        const uint32_t mode = U32(U32(v, 0x234), 0x2de4);
        int ok = 0;
        if (mode == 4 || mode == 3) ok = 1;
        else if (F32(v, 0x150) >= 10.0f /* 0x92fd88 */ && veh_air(v, 0x006c99a2) != 0) {
            const rx_addr rider = U32(U32(K, 4), 0x1b4);
            const rx_addr a = U32(rider, 0x58c);
            if (a != 0 && F32(a, 0) == 1.0f) {
                const rx_addr b = U32(rider, 0x598);
                if (b != 0 && F32(b, 0) == 1.0f) ok = 1;
            }
        }
        if (U32(K, 0x1c) == 0) {
            const rx_addr pl2 = U32(K, 4), rider = U32(pl2, 0x1b4);
            if (U32(rider, 0xaf10) == 0)
                r = (U32(U32(pl2, 0x1ac), 0x280) == 0 && U32(rider, 0x5b57c) == 0xc && ok) ? 1 : 0;
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x006c9940, rx_fx_trick_ground_ok)

/* ---- 006c9b00 (EAX = tracker): ground mode 0: the input state at least 1; when a ground trick may start and the matched trick
 * (+0x60) is of the airborne state now (+0x64 = vf43), it starts (006c98c0) and +0x28 = 1; else the match
 * is dropped. Frame: push ebx / esi / edi. */
void rx_fx_trick_ground(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 3, (int[]){FX_EBX, FX_ESI, FX_EDI});
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_ESI, K);
    if (U32(K, 0x5c) == 0) SU32(K, 0x5c, 1);
    const uint32_t air = veh_air(K_veh(K), 0x006c9b25);
    fx_set_reg(FX_EBX, air);
    uint32_t ok;
    { const rx_addr s = fx_push(0, 0, 0x006c9b2e); ok = rx_fx_trick_ground_ok(K); fx_pop(s); }
    if (ok == 0) SU32(K, 0x60, 0);
    else {
        const rx_addr t = U32(K, 0x60);
        fx_set_reg(FX_EDI, t);
        if (t != 0 && U32(t, 0x64) == air) {
            rx_fx_trick_start(K, t);
            SU32(K, 0x28, 1);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9b00, rx_fx_trick_ground)

/* the gesture read of 006c9510 (states 2 / 3): the stick direction (00548810, EAX = pad, cdecl &dir) and the classifier
 * 006cd2c0 (ESI = the input buffer K + 0x30; stdcall dt, dir.x, dir.y, airborne) */
static void gesture_read(rx_addr K, rx_addr E, rx_addr r1, rx_addr r2, rx_addr r3) {
    {
        rx_fx_stunt_gesture_dir(U32(U32(K, 4), 0x1f0), E + 0x10);
        SU32(E - 4, 0, E + 0x10); SU32(E - 8, 0, r1);
    }
    const uint32_t air = veh_air(K_veh(K), r2);
    fx_set_reg(FX_ESI, K + 0x30);
    rx_fx_stunt_gesture_classify(K + 0x30, rx_bf(U32(E, 0x24)), rx_bf(U32(E, 0x10)), rx_bf(U32(E, 0x14)), air);
    (void)r3;
}

/* ---- 006c9510 (stdcall tracker, dt; RET 8): the trick input state machine (+0x5c): only for a local active player (else
 * +0x60 = +0x24); the stick (00548660 of the pad +0x1f0, xmm0) starts a gesture (1 -> 2) away from centre; in 2 / 3 the
 * direction is read and classified (gesture_read), in 2 a finished gesture (+0x44 > 0, +0x4c) is matched (006c9aa0),
 * recorded in the history ring (15 entries of 8 at +0x98: its name and whether it matched) and looked up (006c8570) into +0x60,
 * -> 3; the gesture clock +0x2c runs; back to 1 when the stick returns (below 0.1) or the direction changes. Frame: sub esp,
 * 0x10; ebp / esi / edi: E = sp0 - 0x1c: +0x10 the direction, +0x20 the stick, +0x24 dt (the argument slots above). */
void rx_fx_trick_input(rx_addr K, float dt) {
    (void)dt;
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x10;
    const fx_saved sv = fx_save();
    fx_push_regs(e0, 1, (int[]){FX_EBP});
    fx_set_reg(FX_EBP, K);
    const rx_addr pl = U32(K, 4);
    fx_push_regs(e0 - 4, 2, (int[]){FX_ESI, FX_EDI});
    const rx_addr E = e0 - 0xc;
    rx_set_sp(E);
    if (U32(pl, 4) == 0 || U32(pl, 0x10) != 0) {
        SU32(K, 0x60, U32(K, 0x24));
        fx_restore(sv); rx_set_sp(sp0); return;
    }
    const uint32_t pad = U32(pl, 0x1f0);
    fx_set_reg(FX_ESI, pad);
    float stick;
    { const rx_addr s = fx_push(0, 0, 0x006c9543); stick = rx_world_pad_look_back(pad); fx_pop(s); }
    const uint32_t st = U32(K, 0x5c);
    fx_set_reg(FX_EDI, 1);
    SF32(E, 0x20, stick);
    int back_check = 0;
    if (st == 1) {
        if (!(stick == 0.0f)) {
            SU32(K, 0x5c, 2); SU32(K, 0x38, 0); SU32(K, 0x30, 0);
            fx_x87_copy(K + 0x44, K + 0x48);
            SF32(K, 0x40, 0.0f); SU32(K, 0x4c, 0); SU32(K, 0x3c, 1); SF32(K, 0x2c, 0.0f);
        }
    } else if (st == 3) {
        gesture_read(K, E, 0x006c9578, 0x006c958e, 0x006c95b1);
        const uint8_t c = U8(K, 0x34);
        if (c == 0x4e || c != U8(K, 0x35)) SU32(K, 0x5c, 1);
        else back_check = 1;
    } else if (st == 2) {
        gesture_read(K, E, 0x006c95ef, 0x006c9608, 0x006c9628);
        if (U32(K, 0x3c) != 0) back_check = 1;
        else if (F32(K, 0x44) > 0.0f && U32(K, 0x4c) != 0) {
            const uint32_t air = veh_air(K_veh(K), 0x006c965c);
            fx_set_reg(FX_EDI, K + 0x30);
            const rx_addr match = rx_fx_stunt_mapping_match(air, K + 0x30);
            fx_set_reg(FX_EDI, (uint32_t)match);
            uint32_t len = 0;
            while (U8(K + 0x30 + len, 0) != 0) len++;
            const uint32_t i = U32(K, 0x94);
            { const uint32_t a[3] = {K + 0x98 + 8 * i, K + 0x30, len}; FX_CRT(3, a, 0x006c968e, crt_memcpy(K + 0x98 + 8 * i, K + 0x30, len)); }
            SU32(K, 0x9c + 8 * U32(K, 0x94), match != 0);
            SU32(K, 0x94, (uint32_t)((I32(K, 0x94) + 1) % 0xf));
            if (match != 0) {
                const uint32_t air2 = veh_air(K_veh(K), 0x006c96d1);
                SU32(K, 0x60, trick_find_call(match, air2, 0x006c96d8));
            }
            SU8(K, 0x58, U8(K, 0x34));
            SU32(K, 0x5c, 3);
        } else {
            SF32(K, 0x2c, rx_adds(F32(K, 0x2c), F32(E, 0x24)));
            if (0.1f /* 0x931d94 */ > F32(E, 0x20) || !(0.0f < F32(K, 0x44)))
                if (U8(K, 0x34) != U8(K, 0x35)) SU32(K, 0x5c, 1);
        }
    }
    if (back_check && 0.1f /* 0x931d94 */ > F32(E, 0x20)) SU32(K, 0x5c, 1);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9510, rx_fx_trick_input)

/* ---- 006c9770 (EAX = tracker; stdcall dt; RET 4): with a vehicle, a rider and the trick table loaded ([0xd5ab38]): landed
 * flag cleared on the ground; the input machine (006c9510); a block (+0x90) lifts once not wrecked and on the ground; enabled
 * trackers of bikes (class <= 1): abort checks (006c9190), the jump states (006c9380), +0x88 = airborne, the physics stunts
 * (006ca0c0), then the mode handler (+8: 0 006c9b00, 1 006c9b60, 2 006c9cc0, 3 006c9da0; 1..3 not reached). Frame: push ecx;
 * push esi. */
void rx_fx_trick_update(float dt, rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX)); SU32(sp0 - 8, 0, sv.esi);
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_ESI, K);
    const rx_addr pl = U32(K, 4);
    if (U32(pl, 0x1ac) == 0 || U32(pl, 0x1b4) == 0 || U32(0, 0x00d5ab38) == 0) { fx_restore(sv); rx_set_sp(sp0); return; }
    if (veh_air(U32(pl, 0x1ac), 0x006c97b0) == 0) SU32(K, 0x28, 0);
    const uint32_t d = fx_x87_bits(rx_fb(dt));
    { const uint32_t a[2] = {K, d}; const rx_addr s = fx_push(2, a, 0x006c97c5); rx_fx_trick_input(K, rx_bf(d)); fx_pop(s); }
    if (U32(K, 0x90) != 0) {
        const rx_addr v = K_veh(K);
        if (U32(v, 0x280) == 0 && veh_air(v, 0x006c97ec) == 0) SU32(K, 0x90, 0);
        if (U32(K, 0x90) != 0) { fx_restore(sv); rx_set_sp(sp0); return; }
    }
    if (U32(K, 0) == 0 || U32(K_veh(K), 0x194) > 1) { fx_restore(sv); rx_set_sp(sp0); return; }
    { const rx_addr s = fx_push(0, 0, 0x006c9827); rx_fx_trick_abort(K); fx_pop(s); }
    { const uint32_t a[1] = {d}; const rx_addr s = fx_push(1, a, 0x006c9836); rx_fx_trick_jump(rx_bf(d), K); fx_pop(s); }
    SU32(K, 0x88, veh_air(K_veh(K), 0x006c9849));
    { const uint32_t a[1] = {d}; const rx_addr s = fx_push(1, a, 0x006c985e); rx_fx_stunt_update(rx_bf(d), K); fx_pop(s); }
    const uint32_t mode = U32(K, 8);
    if (mode == 0) { const rx_addr s = fx_push(0, 0, 0x006c9874); rx_fx_trick_ground(K); fx_pop(s); }
    else if (mode == 1) { const uint32_t a[1] = {d}; const rx_addr s = fx_push(1, a, 0x006c9883); rx_fx_trick_mode1(K, rx_bf(d)); fx_pop(s); }
    else if (mode == 2) rx_fx_trick_mode2(K, rx_bf(d));      /* dt in xmm0, no stack argument (006c9895) */
    else if (mode == 3) rx_fx_trick_mode3(K);                /* no argument (006c989f) */
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9770, rx_fx_trick_update)

/* ======== the trick start and the hold modes 1..3 (006c98c0 / 006c9b60 / 006c9cc0 / 006c9da0) ========
 * A matched mapping (K + 0x60, the trick record) starts a trick in 006c98c0: the trick's performance asset is looked up
 * ("<MX|ATV>Rider_<trick + 0x20>", the perfdat handle; 006cd050 / 006cd0f0), kept in K + 0x20, and the tracker switches to mode
 * 1 (K + 8) with K + 0x1c = the trick, K + 0x14 = K + 0x18 = 0 and the "StuntBegin" event (0x95ba28) posted with {player,
 * trick}. Mode 1 holds the trick in the air: the anim block that loops in the perf asset (006cd1c0: the first looping block's
 * start) is compared with the rider's animation progress (+0x48c / +0x46c FloatListeners): past it the "StuntHoldBegin" event
 * (0x960790) starts mode 2. Mode 2 (on the ground) runs the hold clock K + 0x14 (limit 4.0) and ends the hold ("StuntHoldEnd"
 * 0x960780) back into mode 3 or chains the next matched trick; mode 3 completes the trick (006c9df0: "StuntCompleted" 0x942784)
 * once the rider's progress reaches +0xaf20. The scorer's handlers of these events are the 006cb1b0 / 006cb3c0 group below.
 *
 * The perf asset (PerformanceAsset, build_perf.c): +0x14 the AnimBlock count, +0x18 the records (0x70: +0x40 start, +0x4c
 * cropFront, +0x6c looping), +0x24 the end time, +0x2c / +0x28 / +0x30 the last type-10 / 11 / 12 hash mark times. The AssetHandle
 * (build_asset.h): +0x44 the asset, +0x4c the "in use" flag the originals raise around every read. */

/* 006cd050 (ECX = trick; stack the vehicle class; RET 0; 1 when the perf handle exists): "<MX|ATV>Rider_<trick + 0x20>" through the
 * "%sRider_%s" format into a 0x80-byte frame string, then Database::vf26 (0084ac20, "perfdat"). Frame: sub esp, 0x88 (/GS cookie
 * at +0x84); esi / edi; the string at +0, the original's memset at -7 (lea [esp + 0xd]). */
uint32_t rx_fx_trick_perf_exists(rx_addr trick, uint32_t klass) {
    const rx_addr sp0 = rx_sp(), S = sp0 - 0x88;
    const fx_saved sv = fx_save();
    SU32(S, 0x84, U32(0, 0x009b45e4) ^ S);
    fx_push_regs(S, 2, (int[]){FX_ESI, FX_EDI});
    rx_set_sp(S - 8);
    fx_set_reg(FX_ESI, trick + 0x20);
    SU8(S, 0, 0);
    crt_memset(S - 7, 0, 0x7f);
    rx_addr prefix = 0;
    if (klass == 0) prefix = 0x0092fc24;              /* "MX" */
    else if (klass == 1) prefix = 0x0092fc28;         /* "ATV" */
    SU32(S, -8, prefix); SU32(S, -4, trick + 0x20);
    rx_format_buffer(S, 0x80, 0x00960ac0, S - 8);     /* "%sRider_%s" */
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    const uint32_t r = rx_database_vf_has_handle(db, S, 0x66726570u, 0x746164u);
    SU32(S, -4, 0x006cd0e4);
    phys_security_check_cookie(U32(S, 0x84) ^ S);
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x006cd050, rx_fx_trick_perf_exists)

/* 006cd0f0 (ECX = trick; stack the vehicle class; RET 0): the same name through Database::vf27 (0084ac70, create) -> the perfdat
 * AssetHandle. Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x88 (/GS cookie at +0x84); esi / edi. */
rx_addr rx_fx_trick_asset_handle(rx_addr trick, uint32_t klass) {
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4, A = ebp & ~7u, S = A - 0x88;
    const fx_saved sv = fx_save();
    SU32(ebp, 0, sv.ebp);
    fx_set_reg(FX_EBP, ebp);
    SU32(S, 0x84, U32(0, 0x009b45e4) ^ S);
    fx_push_regs(S, 2, (int[]){FX_ESI, FX_EDI});
    rx_set_sp(S - 8);
    fx_set_reg(FX_ESI, trick + 0x20);
    SU8(S, 0, 0);
    crt_memset(S - 7, 0, 0x7f);
    rx_addr prefix = 0;
    if (klass == 0) prefix = 0x0092fc24;
    else if (klass == 1) prefix = 0x0092fc28;
    SU32(S, -8, prefix); SU32(S, -4, trick + 0x20);
    rx_format_buffer(S, 0x80, 0x00960ac0, S - 8);
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    const rx_addr h = rx_database_vf_handle(db, S, 0x66726570u, 0x746164u, 1);
    SU32(S, -4, 0x006cd181);
    phys_security_check_cookie(U32(S, 0x84) ^ S);
    fx_restore(sv);
    rx_set_sp(sp0);
    return h;
}
RX_FUNCTION(0x006cd0f0, rx_fx_trick_asset_handle)

/* 006cd1c0 (ECX = trick, EAX = the vehicle class; RET 0) -> xmm0: the "hold begins" time of the trick's perf asset: its first
 * looping AnimBlock's cropFront + start, or 0 when it has none. Frame: sub esp, 8; esi; the class pushed for 006cd0f0. */
float rx_fx_trick_hold_time(rx_addr trick, uint32_t klass) {
    const rx_addr sp0 = rx_sp(), S = sp0 - 8;
    const fx_saved sv = fx_save();
    SF32(S, 4, 0.0f);
    SU32(S - 4, 0, sv.esi);
    SU32(S - 8, 0, klass);
    rx_set_sp(S - 8);
    const rx_addr h = rx_fx_trick_asset_handle(trick, klass);
    const rx_addr asset = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    rx_set_sp(S - 4);
    fx_set_reg(FX_ESI, asset);
    float r = F32(S, 4);
    const int32_t n = I32(asset, 0x14);
    if (n >= 1) {
        rx_addr e = U32(asset, 0x18);
        for (int32_t i = 0; i < n; i++, e += 0x70) {
            if (U32(e, 0x6c) != 0) { r = rx_adds(F32(e, 0x4c), F32(e, 0x40)); break; }
        }
    }
    SU32(h, 0x4c, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x006cd1c0, rx_fx_trick_hold_time)

/* 006c9a40 (EAX = tracker; RET 0; 1 when the rider's animation progress passed the trick's perf end +0x2c): the holder K + 0x20
 * raised around the read. Frame: sub esp, 8; esi; the value at +4. */
uint32_t rx_fx_trick_min_reached(rx_addr K) {
    const rx_addr sp0 = rx_sp(), S = sp0 - 8;
    const fx_saved sv = fx_save();
    const rx_addr h = U32(K, 0x20);
    SU32(h, 0x4c, 1);
    const float need = F32(U32(h, 0x44), 0x2c);
    SU32(U32(K, 0x20), 0x4c, 0);
    const rx_addr rider = U32(U32(K, 4), 0x1b4);
    const rx_addr lst = rider + (U32(rider, 0xaf10) == 3 ? 0x48c : 0x46c);
    SU32(S - 4, 0, sv.esi);
    rx_set_sp(S - 4);
    fx_set_reg(FX_ESI, 0);
    SF32(S, 4, need);
    const float got = rx_world_float_listeners_get(lst);
    fx_restore(sv);
    rx_set_sp(sp0);
    return got > need;
}
RX_FUNCTION(0x006c9a40, rx_fx_trick_min_reached)

/* 006c9e80 (ESI = tracker): "StuntHoldBegin" (0x960790) with {player, trick} and mode 2, the hold clock K + 0x10 = 0. Frame:
 * push ecx. */
void rx_fx_trick_hold_begin(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    rx_set_sp(sp0 - 4);
    const rx_addr trick = U32(K, 0x1c);
    if (trick != 0) {
        const uint32_t a[1] = {U32(K, 4)};
        SU32(K, 8, 2);
        const rx_addr s = fx_push(1, a, 0x006c9e98);
        rx_fx_trick_event(a[0], trick, 0x00960790);
        fx_pop(s);
        SF32(K, 0x10, 0.0f);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9e80, rx_fx_trick_hold_begin)

/* 006c9eb0 (ESI = tracker): "StuntHoldEnd" (0x960780) with {player, trick} and mode 3. Frame: push ecx. */
void rx_fx_trick_hold_end(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    rx_set_sp(sp0 - 4);
    const rx_addr trick = U32(K, 0x1c);
    if (trick != 0) {
        const uint32_t a[1] = {U32(K, 4)};
        const rx_addr s = fx_push(1, a, 0x006c9ec1);
        rx_fx_trick_event(a[0], trick, 0x00960780);
        fx_pop(s);
        SU32(K, 8, 3);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9eb0, rx_fx_trick_hold_end)

/* 006c9df0 (EAX = tracker): a held trick completes: the current trick / mode / input state cleared (+0x1c, +8, +0x5c = 1,
 * +0x38 = -1, +0x30 = 0, +0x44 = +0x48, +0x34 / +0x35 = 'N', +0x3c = 0, +0x40 = 0, +0x4c = 0), +0xc = whether the input is not
 * mid-gesture, and "StuntCompleted" (0x942784) with {player, trick}. Frame: push ebp / esi / edi. */
void rx_fx_trick_complete(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    const rx_addr trick = U32(K, 0x1c);
    SU32(sp0 - 4, 0, sv.ebp);
    fx_set_reg(FX_EBP, trick);
    SU32(sp0 - 8, 0, sv.esi); SU32(sp0 - 0xc, 0, sv.edi);
    rx_set_sp(sp0 - 0xc);
    if (trick == 0) { fx_restore(sv); rx_set_sp(sp0); return; }
    SU32(K, 0x1c, 0); SU32(K, 8, 0); SU32(K, 0x5c, 1); SU32(K, 0x38, 0xffffffffu); SU32(K, 0x30, 0);
    fx_x87_copy(K + 0x44, K + 0x48);
    SU8(K, 0x34, 0x4e); SU8(K, 0x35, 0x4e);
    SU32(K, 0x3c, 0); SF32(K, 0x40, 0.0f); SU32(K, 0x4c, 0);
    const rx_addr pl = U32(K, 4);
    uint32_t v;
    if (U32(pl, 0x10) == 1 || U32(pl, 4) == 0 || U32(pl, 0x10) != 0) v = (U32(K, 0x24) == 0);
    else v = (U32(K, 0x5c) != 3);
    SU32(K, 0xc, v == 0);
    const uint32_t a[1] = {pl};
    const rx_addr s = fx_push(1, a, 0x006c9e69);
    rx_fx_trick_event(pl, trick, 0x00942784);
    fx_pop(s);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9df0, rx_fx_trick_complete)

/* 006cd7b0 (ECX = the old trick, EDX = the new one; stack player; RET 4): "ComboFromStunt" (0x9607b4) with {player, old, new}
 * (type 0x9d9a04) on the stunt channel. Frame: sub esp, 0x10 (/GS cookie at +0xc); esi / edi; the payload at +0. */
void rx_fx_trick_switch_event(rx_addr player, rx_addr oldt, rx_addr newt) {
    const rx_addr sp0 = rx_sp(), S = sp0 - 0x10;
    const fx_saved sv = fx_save();
    SU32(S, 0xc, U32(0, 0x009b45e4) ^ S);
    fx_push_regs(S, 2, (int[]){FX_ESI, FX_EDI});
    const rx_addr evs = U32(0, 0x00a94304);
    fx_set_reg(FX_ESI, evs); fx_set_reg(FX_EDI, 0x009607b4);
    SU32(S, 0, player); SU32(S, 4, oldt); SU32(S, 8, newt);
    rx_set_sp(S - 8);
    post_event(evs, 0x009d9a04, 0x009607b4, S, 0xc, 0x006cd7e6, 0x006cd7f4);
    rx_set_sp(S);
    SU32(S, -4, 0x006cd806);
    phys_security_check_cookie(U32(S, 0xc) ^ S);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cd7b0, rx_fx_trick_switch_event)

/* ---- 006c98c0 (ESI = tracker, EDI = the trick; RET 0): the trick starts. The perf handle must exist (006cd050), else the match
 * K + 0x60 is dropped; then the holder K + 0x20, mode 1, K + 0x1c = the trick, K + 0x60 = 0, the "StuntBegin" event (0x95ba28)
 * and K + 0x14 = K + 0x18 = 0. Frame: push ecx. ---- */
void rx_fx_trick_start(rx_addr K, rx_addr trick) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_EDI, trick);
    const uint32_t klass = U32(U32(U32(K, 4), 0x1ac), 0x194);
    if (rx_fx_trick_perf_exists(trick, klass) == 0) {
        SU32(K, 0x60, 0);
        fx_restore(sv); rx_set_sp(sp0);
        return;
    }
    const rx_addr holder = rx_fx_trick_asset_handle(trick, U32(U32(U32(K, 4), 0x1ac), 0x194));
    SU32(K, 0x20, holder);
    SU32(K, 8, 1);
    SU32(K, 0x1c, trick);
    SU32(K, 0x60, 0);
    { const uint32_t a[1] = {U32(K, 4)}; const rx_addr s = fx_push(1, a, 0x006c991d);
      rx_fx_trick_event(a[0], trick, 0x0095ba28); fx_pop(s); }
    SF32(K, 0x14, 0.0f);
    SU32(K, 0x18, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c98c0, rx_fx_trick_start)

/* the input-state flag of the hold modes (+0x18): an active local player's gesture not finished (3), else the match K + 0x24 */
static uint32_t hold_input_flag(rx_addr K) {
    const rx_addr pl = U32(K, 4);
    if (U32(pl, 0x10) == 1 || U32(pl, 4) == 0 || U32(pl, 0x10) != 0) return U32(K, 0x24) == 0;
    return U32(K, 0x5c) != 3;
}

/* ---- 006c9b60 (ESI = tracker; stdcall dt; RET 4): hold mode 1 (airborne). The flag +0x18; then the perf asset's loop-start
 * (006cd1c0): > 0 and the rider's progress past loop-start - dt -> "StuntHoldBegin" (006c9e80, mode 2) when the input state is
 * clean and the player is active; every other way mode 3. Frame: sub esp, 8; esi / edi; the value at +4 (E - 4). ---- */
void rx_fx_trick_mode1(rx_addr K, float dt) {
    const rx_addr sp0 = rx_sp(), S = sp0 - 8;
    const fx_saved sv = fx_save();
    SU32(S - 4, 0, fx_reg(FX_ESI)); SU32(S - 8, 0, fx_reg(FX_EDI));
    rx_set_sp(S - 8);
    fx_set_reg(FX_ESI, K);
    if (U32(K, 0x18) == 0) SU32(K, 0x18, hold_input_flag(K));
    const float val = rx_fx_trick_hold_time(U32(K, 0x1c), U32(U32(U32(K, 4), 0x1ac), 0x194));
    SF32(S, 4, val);
    if (0.0f >= val) {
        SU32(K, 8, 3);
        fx_restore(sv); rx_set_sp(sp0);
        return;
    }
    {
        const rx_addr rider = U32(U32(K, 4), 0x1b4);
        const rx_addr lst = rider + (U32(rider, 0xaf10) == 3 ? 0x48c : 0x46c);
        const float got = rx_world_float_listeners_get(lst);
        const float lim = rx_subs(val, dt);
        if (got <= lim) {                     /* the original's jbe: nothing is written */
            fx_restore(sv); rx_set_sp(sp0);
            return;
        }
        if (U32(K, 0x18) != 0 || U32(U32(K, 4), 4) == 0) {
            SU32(K, 8, 3);
            fx_restore(sv); rx_set_sp(sp0);
            return;
        }
        rx_fx_trick_hold_begin(K);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9b60, rx_fx_trick_mode1)

/* ---- 006c9cc0 (ESI = tracker; stdcall dt; RET 4): hold mode 2 (on the ground). The flag +0x18; the clocks K + 0x10 += dt and,
 * for a ground trick (its +0x64 = 0), K + 0x14 += dt: clean input and K + 0x14 < 4.0 -> the airborne / slow (speed < 5) checks
 * chain the next trick (006c9c20); else "StuntHoldEnd" (006c9eb0) and the chain; a used clock / bad input / trick is a
 * "StuntHoldEnd" and mode 3. Frame: push esi; the frame slot +4 is the caller's dt. ---- */
void rx_fx_trick_mode2(rx_addr K, float dt) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_ESI, K);
    if (U32(K, 0x18) == 0) SU32(K, 0x18, hold_input_flag(K));
    const rx_addr trick = U32(K, 0x1c);
    SF32(K, 0x10, rx_adds(F32(K, 0x10), dt));
    int ground = 0;
    if (trick != 0 && U32(trick, 0x64) == 0) {
        SF32(K, 0x14, rx_adds(F32(K, 0x14), dt));
        ground = 1;
    }
    if (U32(K, 0x18) != 0) goto hold_end;
    if (ground) {
        if (F32(K, 0x14) >= 4.0f /* 0x9322c0 */) goto hold_end;
    }
    {
        const rx_addr v = U32(U32(K, 4), 0x1ac);
        if (veh_air(v, 0x006c9d4a) != 0) goto chain;
        if (!(5.0f /* 0x9c3e24 */ <= F32(U32(U32(K, 4), 0x1ac), 0x150))) {
            rx_fx_trick_hold_end(K);
        }
        goto chain;
    }
hold_end:
    if (trick != 0) {
        const uint32_t a[1] = {U32(K, 4)};
        const rx_addr s = fx_push(1, a, 0x006c9d88);
        rx_fx_trick_event(a[0], trick, 0x00960780);
        fx_pop(s);
    }
    SU32(K, 8, 3);
chain:
    fx_restore(sv);
    rx_set_sp(sp0);
    rx_fx_trick_chain(K);
}
RX_FUNCTION(0x006c9cc0, rx_fx_trick_mode2)

/* ---- 006c9da0 (ESI = tracker): hold mode 3: the rider's progress reached +0xaf20 -> the trick completes (006c9df0); else the
 * next trick may chain (006c9c20). Frame: push ecx. ---- */
void rx_fx_trick_mode3(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    rx_set_sp(sp0 - 4);
    const rx_addr rider = U32(U32(K, 4), 0x1b4);
    const rx_addr lst = rider + (U32(rider, 0xaf10) == 3 ? 0x48c : 0x46c);
    const float got = rx_world_float_listeners_get(lst);
    fx_restore(sv);
    rx_set_sp(sp0);
    if (got >= F32(U32(U32(K, 4), 0x1b4), 0xaf20)) rx_fx_trick_complete(K);
    else rx_fx_trick_chain(K);
}
RX_FUNCTION(0x006c9da0, rx_fx_trick_mode3)

/* ---- 006c9c20 (EAX = tracker): the next matched trick (K + 0x60) chains when the current trick is airborne (or the player is a
 * replay) and the rider is not in state 3, the vehicle is airborne the same way, not in mode 3, and the current trick's perf end
 * passed (006c9a40): "ComboFromStunt" for the old one (006cd7b0) and the start of the new (006c98c0). Frame: push ebx / esi /
 * edi. ---- */
void rx_fx_trick_chain(rx_addr K) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 3, (int[]){FX_EBX, FX_ESI, FX_EDI});
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_ESI, K);
    const rx_addr cur = U32(K, 0x1c);
    if (cur == 0) goto end;
    if (U32(cur, 0x64) == 0 && U32(U32(K, 4), 0x10) != 1) goto end;
    if (U32(U32(U32(K, 4), 0x1b4), 0xaf10) == 3) goto end;
    {
        const rx_addr v = U32(U32(K, 4), 0x1ac);
        const uint32_t air = veh_air(v, 0x006c9c67);
        fx_set_reg(FX_EBX, air);
        const uint32_t ok = rx_fx_trick_min_reached(K);
        if (ok == 0 || air == 0) goto end;
    }
    if (U32(U32(U32(U32(K, 4), 0x1ac), 0x234), 0x2de4) == 3) goto end;
    {
        const rx_addr match = U32(K, 0x60);
        if (match == 0) goto end;
        if (U32(match, 0x64) != fx_reg(FX_EBX)) goto end;
        const rx_addr c = U32(K, 0x1c);
        if (match == c) goto end;
        if (c != 0) {
            const uint32_t a[1] = {U32(K, 4)};
            const rx_addr s = fx_push(1, a, 0x006c9ca9);
            rx_fx_trick_switch_event(a[0], c, match);
            fx_pop(s);
        }
        rx_fx_trick_start(K, match);
    }
end:
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c9c20, rx_fx_trick_chain)

/* ======== the stunt scorer P = player +0xc70 (006cab80) ========
 * P: +0 enabled, +4 / +8 / +0xc / +0x10 the four ratings, +0x28..+0x38 stat counters, +0x54, +0x58 / +0x5c combo clock /
 * state, +0x68 / +0x6c / +0x70 / +0x74 airtime / ground / air clocks, +0x78 score, +0x84..+0xac 40 trick flags, +0x124 combo
 * state (0 none, 1 running, 2 finishing), +0x128 the player (+0x94 its activity: +0x21c kind 2 = freestyle; +0xb38, +0xb44,
 * +0xb60, +0x116c, +0x1230 stats; +0x1f0 pad), +0x130 the position, +0x2d0 / +0x2d4 combo timer, +0x2d8, +0x2dc wheelie /
 * stoppie, +0x30c wrecked, +0x310, +0x314 ground time, +0x324 / +0x344 the two trick maps, +0x418 trick count, +0x41c..+0x42c
 * highlight flags. */

/* std::map iterator increment 00593dc0 (ESI = iterator {container, node}; fx_tree.c) at the call site (return address ret) */
static void tree_next(rx_addr it, rx_addr ret) {
    const rx_addr s = fx_push(0, 0, ret);
    rx_fx_tree_next(it);
    fx_pop(s);
}
static void invalid_parameter(void) { rx_call_original(U32(0, 0x00916224), RX_CDECL, 0, 0, 0); }

/* ---- 006cbad0 (EDI = scorer): the tricks of both trick maps (iterator {+0x324, [+0x33c]->left} and {+0x344, [+0x35c]->left};
 * checked: the container proxy, its head [proxy]+0x18) landed at least once (+0x28 > 0). Frame: sub esp, 0x10; ebx / ebp /
 * esi: E = sp0 - 0x1c: +0xc the count, +0x10 the iterator {proxy, node}. */
uint32_t rx_fx_tricks_landed(rx_addr P) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x10, E = e0 - 0xc;
    const fx_saved sv = fx_save();
    fx_push_regs(e0, 3, (int[]){FX_EBX, FX_EBP, FX_ESI});
    rx_set_sp(E);
    SU32(E, 0xc, 0);
    for (int m = 0; m < 2; m++) {
        const uint32_t head = m ? 0x35c : 0x33c, cont = m ? 0x344 : 0x324;
        rx_addr c = U32(P, cont), n = U32(U32(P, head), 0);
        SU32(E, 0x10, c); SU32(E, 0x14, n);
        if (m == 0) fx_set_reg(FX_EBP, n);
        for (;;) {
            const rx_addr end = U32(P, head);
            fx_set_reg(FX_EBX, end);
            if (c == 0 || c != U32(P, cont)) invalid_parameter();
            if (m) fx_set_reg(FX_EBP, U32(0, 0x00916224));
            if (n == end) break;
            if (c == 0) invalid_parameter();
            else c = U32(c, 0);
            if (n == U32(c, 0x18)) invalid_parameter();
            if (I32(n, 0x28) > 0) SU32(E, 0xc, U32(E, 0xc) + 1);
            fx_set_reg(FX_ESI, E + 0x10);
            tree_next(E + 0x10, m ? 0x006cbbc2 : 0x006cbb46);
            n = U32(E, 0x14); c = U32(E, 0x10);
            if (m == 0) fx_set_reg(FX_EBP, n);
            fx_set_reg(FX_ESI, c);
        }
    }
    const uint32_t r = U32(E, 0xc);
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x006cbad0, rx_fx_tricks_landed)

/* a rating from x in [0, 1]: <= 0 -> lo, >= 1 -> hi, else x * 5.5 + lo (lo = [0x9cd9e4]) */
static inline float rating(float x) {
    const float lo = F32(0, 0x009cd9e4);
    if (0.0f >= x) return lo;
    if (x >= 1.0f) return 10.0f /* 0x92fd88 */;
    return rx_adds(rx_muls(x, 5.5f /* 0x977650 */), lo);
}

/* ---- 006cc670 (ESI = scorer): the variety rating +0x10 from the trick flags +0x84.. (the leading run of set flags, up to 40)
 * against the trick table size [0xa9431c] * [0x9d78a0]. Frame: push edi. */
void rx_fx_stunt_variety(rx_addr P) {
    SU32(rx_sp(), -4, fx_reg(FX_EDI));
    const int32_t total = I32(0, 0x00a9431c);
    if (total == 0) { SF32(P, 0x10, 0.0f); return; }
    int32_t n = 0;
    for (uint32_t k = 0; k < 40; k++) { if (U32(P, 0x84 + 4 * k) == 0) break; n++; }
    if (n <= 0) { SF32(P, 0x10, 0.0f); return; }
    const float v = rx_muls(rx_divs((float)n, rx_muls((float)total, F32(0, 0x009d78a0))), 10.0f /* 0x92fd88 */);
    SF32(P, 0x10, v);
    const float x = rx_muls(v, 0.1f /* 0x931d94 */);
    SF32(P, 0x10, rating(x));
}
RX_FUNCTION(0x006cc670, rx_fx_stunt_variety)

/* ---- 006cc4b0 (EAX = scorer): the ratings: with tricks (+0x418): style +4 = rating((score / n - clamp(2 - 2 n / (0.4 T)))
 * * 0.1), flow +8 = rating((sqrt(n) * [0x9d789c] - +0x2c) * 0.1), mastery +0xc = rating(landed (006cbad0) / max(1, 1.25 T)),
 * variety +0x10 (006cc670); without: the variety, and the others 1 or 0 by it. Frame: push ebp; and esp, -8; sub esp, 8; esi /
 * edi: +0xc the divisor. */
void rx_fx_stunt_ratings(rx_addr P) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u, E = A - 0x10;
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.ebp); fx_set_reg(FX_EBP, sp0 - 4);
    fx_push_regs(A - 8, 2, (int[]){FX_ESI, FX_EDI});
    rx_set_sp(E);
    fx_set_reg(FX_ESI, P);
    const int32_t n = I32(P, 0x418);
    if (n == 0) {
        { const rx_addr s = fx_push(0, 0, 0x006cc645); rx_fx_stunt_variety(P); fx_pop(s); }
        const float v = 0.0f < F32(P, 0x10) ? 1.0f : 0.0f;
        SF32(P, 0xc, v); SF32(P, 8, v); SF32(P, 4, v);
        fx_restore(sv); rx_set_sp(sp0); return;
    }
    const float T = (float)I32(0, 0x00a9431c), fn = (float)n, one = 1.0f;
    float k = rx_divs(fn, rx_muls(T, 0.4f /* 0x935860 */));
    float bonus;
    if (0.0f >= k) bonus = 2.0f;
    else if (k >= one) bonus = 0.0f;
    else bonus = rx_subs(2.0f, rx_muls(k, 2.0f));
    const float s1 = rx_muls(rx_subs(rx_divs(F32(P, 0x78), fn), bonus), 0.1f);
    SF32(P, 4, rating(s1));
    const float s2 = rx_muls(rx_subs(rx_muls(sqrtf(fn), F32(0, 0x009d789c)), F32(P, 0x2c)), 0.1f);
    SF32(P, 8, rating(s2));
    const float d = rx_muls(T, F32(0, 0x009c4068));
    SF32(E, 0xc, d > one ? d : one);
    fx_set_reg(FX_EDI, P);
    uint32_t landed;
    { const rx_addr s = fx_push(0, 0, 0x006cc5cc); landed = rx_fx_tricks_landed(P); fx_pop(s); }
    const float r = rx_divs((float)(int32_t)landed, F32(E, 0xc));
    float m;
    if (!(r >= one) && 0.0f >= r) m = F32(0, 0x009cd9e4);
    else if (r >= one) m = 10.0f;
    else m = rx_adds(rx_muls(r, 5.5f), F32(0, 0x009cd9e4));
    SF32(P, 0xc, m);
    { const rx_addr s = fx_push(0, 0, !(r >= one) && 0.0f >= r ? 0x006cc5fd : r >= one ? 0x006cc61a : 0x006cc63a); rx_fx_stunt_variety(P); fx_pop(s); }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cc4b0, rx_fx_stunt_ratings)

/* ---- 006cb9b0 (EDI = scorer; EAX = 0 / 1): no trick of either map was started more than once (+0x28 <= 1): the walk of
 * 006cbad0 stops at the first repeated one. Frame: sub esp, 0x10; ebx / ebp / esi: E = sp0 - 0x1c: +0xc the flag, +0x10 the
 * iterator {proxy, node}. ---- */
uint32_t rx_fx_tricks_unique(rx_addr P) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x10, E = e0 - 0xc;
    const fx_saved sv = fx_save();
    fx_push_regs(e0, 3, (int[]){FX_EBX, FX_EBP, FX_ESI});
    rx_set_sp(E);
    SU32(E, 0xc, 1);
    for (int m = 0; m < 2; m++) {
        const uint32_t head = m ? 0x35c : 0x33c, cont = m ? 0x344 : 0x324;
        rx_addr c = U32(P, cont), n = U32(U32(P, head), 0);
        fx_set_reg(FX_ESI, c);
        SU32(E, 0x10, c); SU32(E, 0x14, n);
        fx_set_reg(FX_EBP, n);
        for (;;) {
            const rx_addr end = U32(P, head);
            fx_set_reg(FX_EBX, end);
            if (c == 0 || c != U32(P, cont)) invalid_parameter();
            if (m) fx_set_reg(FX_EBP, U32(0, 0x00916224));
            if (n == end || U32(E, 0xc) == 0) break;
            if (c == 0) invalid_parameter();
            else c = U32(c, 0);
            if (n == U32(c, 0x18)) invalid_parameter();
            if (I32(n, 0x28) > 1) SU32(E, 0xc, 0);
            fx_set_reg(FX_ESI, E + 0x10);
            tree_next(E + 0x10, m ? 0x006cbabe : 0x006cba31);
            n = U32(E, 0x14); c = U32(E, 0x10);
            if (m == 0) fx_set_reg(FX_EBP, n);
            fx_set_reg(FX_ESI, c);
        }
    }
    const uint32_t r = U32(E, 0xc);
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x006cb9b0, rx_fx_tricks_unique)

/* clamp to [lo, 10] as the scorer's ratings compile it: x >= 10 -> 10, else lo when not above it (a NaN stays) */
static inline float clamp_rating(float x) {
    const float lo = F32(0, 0x009cd9e4);
    if (x >= 10.0f /* 0x92fd88 */) return 10.0f;
    if (!(lo >= x)) return x;
    return lo;
}

/* ---- 006cc830 (EAX = scorer): rating +0x20 (the fifth): with tricks (+0x418): the mastery +0xc (x87 copy), +[0x9c3e74] and the
 * flag +0x48 when no trick repeated (006cb9b0), minus +0x30, clamped to [lo, 10]; else 0. Frame: push edi. ---- */
void rx_fx_stunt_rating_e(rx_addr P) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_EDI));
    const fx_saved sv = fx_save();
    fx_set_reg(FX_EDI, P);
    const int tricks = U32(P, 0x418) != 0;
    fx_x87_copy(P + 0x20, P + 0xc);
    if (!tricks) { SF32(P, 0x20, 0.0f); fx_restore(sv); return; }
    uint32_t u;
    { const rx_addr s = fx_push(0, 0, 0x006cc847); u = rx_fx_tricks_unique(P); fx_pop(s); }
    if (u != 0) { const float v = rx_adds(F32(P, 0x20), F32(0, 0x009c3e74)); SU32(P, 0x48, 1); SF32(P, 0x20, v); }
    if (U32(P, 0x418) == 0) { SF32(P, 0x20, 0.0f); fx_restore(sv); return; }
    const float x = rx_subs(F32(P, 0x20), F32(P, 0x30));
    SF32(P, 0x20, x);
    SF32(P, 0x20, clamp_rating(x));
    fx_restore(sv);
}
RX_FUNCTION(0x006cc830, rx_fx_stunt_rating_e)

/* ---- 006cc740 (EAX = scorer): rating +0x14 (the first): with tricks: the mean of the middle ratings +0x18 / +0x1c / +0x24 / +0x20
 * (of +0x18 / +0x1c / +0x24 the largest, the middle one, the smallest raised to +0x20: their sum / 3), plus [0x9c3e5c] (+0x3c)
 * when tricks landed (006cbad0) and +0x54, plus [0x9c3e6c] (+0x40) when +0x70 > +0x6c, minus +0x28, clamped to [lo, 10]; else 0.
 * Frame: push edi. ---- */
void rx_fx_stunt_rating_a(rx_addr P) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_EDI));
    const fx_saved sv = fx_save();
    fx_set_reg(FX_EDI, P);
    if (U32(P, 0x418) == 0) { SF32(P, 0x14, 0.0f); fx_restore(sv); return; }
    float hi = F32(P, 0x18), mid = F32(P, 0x1c);
    const float c = F32(P, 0x24), d = F32(P, 0x20);
    if (mid > hi) { const float t = hi; hi = mid; mid = t; }          /* hi = the larger, mid = the smaller of +0x18 / +0x1c */
    float lo = c;
    if (c > mid) { lo = mid; mid = c; }                                /* mid = the larger of (smaller, +0x24), lo = the smaller */
    if (d > lo) lo = d;
    SF32(P, 0x14, rx_muls(rx_adds(rx_adds(lo, mid), hi), 0.33333334f /* 0x92e214 */));
    uint32_t landed;
    { const rx_addr s = fx_push(0, 0, 0x006cc7a2); landed = rx_fx_tricks_landed(P); fx_pop(s); }
    if (landed != 0) {
        if (U32(P, 0x54) != 0) { const float v = rx_adds(F32(P, 0x14), F32(0, 0x009c3e5c)); SU32(P, 0x3c, 1); SF32(P, 0x14, v); }
        if (F32(P, 0x70) > F32(P, 0x6c)) { const float v = rx_adds(F32(P, 0x14), F32(0, 0x009c3e6c)); SU32(P, 0x40, 1); SF32(P, 0x14, v); }
    }
    const float x = rx_subs(F32(P, 0x14), F32(P, 0x28));
    SF32(P, 0x14, x);
    SF32(P, 0x14, clamp_rating(x));
    fx_restore(sv);
}
RX_FUNCTION(0x006cc740, rx_fx_stunt_rating_a)

/* Local profile progress is maintained by the native core; the comparison oracle
 * retains the original child until this parent's full incidental outputs are proven. */
static uint32_t highlight_progress(rx_addr pad, uint32_t index, rx_addr ret) {
    const uint32_t a[3] = {U32(0, 0x00d92edc), pad, index};
#ifdef RX_ORACLE
    return rx_call_original(0x006d7980, RX_STDCALL, 0, 3, a);
#else
    const rx_addr before = fx_push(3, a, ret);
    const uint32_t result = rx_highlight_add_progress(a[0], a[1], a[2]);
    fx_pop(before);
    return result;
#endif
}

/* ---- 006cc8b0 (ESI = scorer): the pending refresh of an enabled scorer that has not been refreshed (+0x320 = 0): the ratings
 * (006cc4b0), then the five stunt ratings recomputed: +0x18 (style: +4 * 0.1 * [0x9c4068] + +4, +0x4c the bonus), +0x1c (flow:
 * +8 + +0x64 * [0x9c3e68], +0x50), +0x20 (006cc830), +0x24 (10 when no tricks counted ([0xa9431c] = 0), else +0x10, minus +0x34),
 * +0x14 (006cc740), each clamped to [lo, 10]; with the debug switch [0xd5aacc] and an active player all five are 10; the total
 * +0x7c = 2 * the sum clamped to [0, 100]. An active player of a local pad (+0x1f0 >= 0) in a freestyle activity state
 * (+0x94 +0x23c = 5) at its limit (+0x294 >= +0x2bc) with +0x1068 = 0 adds highlight progress 0x22 for each rating at 10 (+0x18,
 * +0x14, +0x20, +0x24, +0x1c); a total override +0x438 >= 0 replaces +0x7c; +0x320 = 1. Frame: push ebp; and esp, -8. ---- */
void rx_fx_stunt_refresh(rx_addr P) {
    fx_frame f; fx_frame_enter0(&f, 0, 8);
    fx_set_reg(FX_ESI, P);
    if (U32(P, 0) == 0 || U32(P, 0x320) != 0) { fx_frame_leave(&f); return; }
    { const rx_addr s = fx_push(0, 0, 0x006cc8d3); rx_fx_stunt_ratings(P); fx_pop(s); }
    const float ten = 10.0f /* 0x92fd88 */;
    const uint32_t tricks = U32(P, 0x418);
    if (tricks != 0) {
        const float a = F32(P, 4);
        const float mid = rx_muls(rx_muls(a, 0.1f /* 0x931d94 */), F32(0, 0x009c4068));
        SF32(P, 0x4c, mid);
        const float x = rx_adds(mid, a);
        SF32(P, 0x18, rx_subs(F32(P, 0x18), F32(P, 0x38)));
        SF32(P, 0x18, clamp_rating(x));
    } else { SF32(P, 0x18, 0.0f); SF32(P, 0x4c, 0.0f); }
    if (tricks != 0) {
        const float b = rx_muls(F32(P, 0x64), F32(0, 0x009c3e68));
        const float x = rx_adds(F32(P, 8), b);
        SF32(P, 0x50, b);
        SF32(P, 0x1c, x);
        SF32(P, 0x1c, clamp_rating(x));
    } else SF32(P, 0x1c, 0.0f);
    { const rx_addr s = fx_push(0, 0, 0x006cc98f); rx_fx_stunt_rating_e(P); fx_pop(s); }
    float r24;
    if (U32(P, 0x418) != 0) {
        float v = I32(0, 0x00a9431c) == 0 ? ten : F32(P, 0x10);
        v = rx_subs(v, F32(P, 0x34));
        SF32(P, 0x24, v);
        r24 = clamp_rating(v);
    } else r24 = 0.0f;
    SF32(P, 0x24, r24);
    { const rx_addr s = fx_push(0, 0, 0x006cc9e8); rx_fx_stunt_rating_a(P); fx_pop(s); }
    if (U32(0, 0x00d5aacc) != 0) {
        const rx_addr pl = U32(P, 0x128);
        if (U32(pl, 4) != 0 && U32(pl, 0x10) == 0) for (uint32_t o = 0x14; o <= 0x24; o += 4) SF32(P, o, ten);
    }
    {
        const float sum = rx_adds(rx_adds(rx_adds(rx_adds(F32(P, 0x20), F32(P, 0x1c)), F32(P, 0x18)), F32(P, 0x14)), F32(P, 0x24));
        float t = rx_muls(sum, 2.0f /* 0x92e330 */);
        SF32(P, 0x7c, t);
        if (t >= 100.0f /* 0x93005c */) t = 100.0f;
        else if (!(0.0f < t)) t = 0.0f;
        SF32(P, 0x7c, t);
    }
    const rx_addr pl = U32(P, 0x128);
    if (U32(pl, 4) != 0 && U32(pl, 0x10) == 0 && I32(pl, 0x1f0) >= 0) {
        const rx_addr act = U32(pl, 0x94);
        if (U32(act, 0x23c) == 5 && !(F32(act, 0x294) < F32(act, 0x2bc)) && U32(act, 0x1068) == 0) {
            static const uint32_t order[5] = {0x18, 0x14, 0x20, 0x24, 0x1c};
            static const uint32_t rets[5] = {0x006ccadd, 0x006ccb09, 0x006ccb35, 0x006ccb61, 0x006ccb8d};
            for (int i = 0; i < 5; i++)
                if (F32(P, order[i]) >= ten) highlight_progress(I32(U32(P, 0x128), 0x1f0), 0x22, rets[i]);
        }
    }
    const float ov = F32(P, 0x438);
    if (!(ov < 0.0f /* 0x92e12c */)) SF32(P, 0x7c, ov);
    SU32(P, 0x320, 1);
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006cc8b0, rx_fx_stunt_refresh)

/* 006d83d0 (EAX = the driver mode, ESI = the player's pad slot): the wreck recovery's highlight progress through the byte switch
 * 0x6d8400 (modes 0, 1, 2, 6, 0xc, 0x11, 0x13 add 0x2a; a mode above 0x13 does not) and always 0x2b (MXHighlightTracking_AddProgress
 * 006d7980 = the product boundary above). Frame: push ecx; both calls are stdcall with 3 arguments (RET 0xc). */
void rx_fx_highlight_recover(uint32_t mode, uint32_t pad) {
    static const uint8_t add_2a[20] = {1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1};
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    if (mode <= 0x13u && add_2a[mode]) highlight_progress(pad, 0x2a, 0);
    highlight_progress(pad, 0x2b, 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006d83d0, rx_fx_highlight_recover)

/* ---- 006cae70 (EAX = scorer): a combo clock (+0x58) past [0x9c3e48] scores the freestyle counters (+0x34 / +0x30 / +0x38 /
 * +0x28 += [0x9c3e4c] when the activity's kind is 2) and resets; then the ratings (tail jump to 006cc4b0). Frame: push esi. */
void rx_fx_stunt_combo(rx_addr P) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    if (F32(P, 0x58) > F32(0, 0x009c3e48)) {
        const rx_addr pl = U32(P, 0x128);
        const float b = F32(0, 0x009c3e4c);
        static const uint16_t o[4] = {0x34, 0x30, 0x38, 0x28};
        for (int i = 0; i < 4; i++) if (U32(U32(pl, 0x94), 0x21c) == 2) SF32(P, o[i], rx_adds(F32(P, o[i]), b));
        SF32(P, 0x58, 0.0f);
    }
    rx_fx_stunt_ratings(P);
}
RX_FUNCTION(0x006cae70, rx_fx_stunt_combo)

/* ---- 00588b50 (EAX = the player; stdcall manager; RET 4): the combo score drop: the manager's event "0x92fce0" (type
 * 0x9cbeb4) with the player as its 4-byte payload. Frame: push ecx; the payload at sp0 - 4. ---- */
void rx_fx_stunt_score_drop(rx_addr mgr, rx_addr player) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 4;
    SU32(E, 0, player);
    fx_type_name(0x009cbeb4, 0x00588b62);
    fx_set_reg(FX_EDI, 0x0092fce0);
    { const uint32_t a[2] = {E, 4}; const rx_addr s = fx_push(2, a, 0x00588b75);
      rx_world_event_dispatch(mgr, 0x0092fce0, E, 4); fx_pop(s); }
}
RX_FUNCTION(0x00588b50, rx_fx_stunt_score_drop)

/* ---- 006cb720 (ESI = scorer): a fresh wreck (vehicle +0x280, not yet noted in +0x30c) ends the combo: a running one posts
 * the "combo lost" event (0x92fd20, type 0x9cbeb4, payload the player) to [0xa94318], a finishing one with time left the
 * score drop (00588b50, EDI = 0x92fce0, not in the core); the combo state reset, the freestyle counters +0x28 += 0.25
 * [0x9c3e44], +0x2c += [0x9c3e44]. Frame: sub esp, 8; ebx / ebp / edi. */
void rx_fx_stunt_wreck(rx_addr P) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 8, E = e0 - 0xc;
    const fx_saved sv = fx_save();
    const rx_addr pl = U32(P, 0x128);
    fx_push_regs(e0, 1, (int[]){FX_EBX});
    const uint32_t wrecked = U32(U32(pl, 0x1ac), 0x280) != 0;
    fx_set_reg(FX_EBX, 0);
    fx_push_regs(e0 - 4, 2, (int[]){FX_EBP, FX_EDI});
    rx_set_sp(E);
    fx_set_reg(FX_EBP, wrecked);
    if (wrecked == 0 || U32(P, 0x30c) != 0) { SU32(P, 0x30c, wrecked); fx_restore(sv); rx_set_sp(sp0); return; }
    SU32(P, 0x54, 0);
    if (U32(P, 0x124) != 0) {
        SU32(E, 0x10, U32(0, 0x00a94318));
        SU32(E, 0xc, pl);                                     /* the payload: the player */
        post_event(U32(E, 0x10), 0x009cbeb4, 0x0092fd20, E + 0xc, 4, 0x006cb781, 0x006cb796);
        fx_set_reg(FX_EDI, 0x0092fd20);
    } else if (F32(P, 0x2d0) > 0.0f) {
        rx_fx_stunt_score_drop(U32(0, 0x00a94318), pl);   /* 00588b50, EAX = the player */
    }
    const rx_addr p2 = U32(P, 0x128);
    SF32(P, 0x2d0, 0.0f); SF32(P, 0x2d4, 0.0f); SF32(P, 0x58, 0.0f);
    const float b = F32(0, 0x009c3e44);
    SU32(P, 0x124, 0); SU32(P, 0x2dc, 0); SU32(P, 0x310, 0); SU32(P, 0x2d8, 0); SU32(P, 0x5c, 0); SU32(P, 0x41c, 0); SU32(P, 0x420, 0);
    if (U32(U32(p2, 0x94), 0x21c) == 2) SF32(P, 0x28, rx_adds(rx_muls(b, 0.25f /* 0x92ff84 */), F32(P, 0x28)));
    const uint32_t k2 = U32(U32(p2, 0x94), 0x21c);
    SU32(P, 0x30c, wrecked);
    if (k2 == 2) SF32(P, 0x2c, rx_adds(F32(P, 0x2c), b));
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cb720, rx_fx_stunt_wreck)

/* ---- 006cab80 (EAX = scorer; stdcall dt; RET 4): for an active local player with a vehicle: the wreck check (006cb720); the
 * air (+0x70 / +0x74) or ground (+0x6c; +0x58 combo clock unless wrecked or in a wheelie) clocks; the combo state (+0x124:
 * finishing: the timer +0x2d0 runs out -> 006caaf0, not in the core; running: +0x68 airtime when the stunt mode +0xb60 = 2 or
 * a wheelie); the position (vf17 into +0x130); the ground time +0x314 (airborne high above the ground +0x1230 > 1 resets it);
 * the highlight goals (MXHighlightTracking_AddProgress 0x20 / 0x21) and their flags; then the combo / ratings (006cae70); a
 * pending refresh [0xd5ab64] runs 006cc8b0. Frame: push esi; push edi. */
void rx_fx_stunt_score(float dt, rx_addr P) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 2, (int[]){FX_ESI, FX_EDI});
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_ESI, P); fx_set_reg(FX_EDI, 0);
    const float one = 1.0f;
    if (U32(P, 0) != 0) {
        const rx_addr pl = U32(P, 0x128);
        int go = pl != 0 && U32(pl, 0x1ac) != 0 && U32(pl, 4) != 0;
        if (go && U32(pl, 0x10) != 0) go = U32(pl, 4) != 0 && U32(pl, 0x10) == 1;
        if (go) {
            { const rx_addr s = fx_push(0, 0, 0x006cabce); rx_fx_stunt_wreck(P); fx_pop(s); }
            if (veh_air(U32(U32(P, 0x128), 0x1ac), 0x006cabe4) != 0) {
                SF32(P, 0x70, rx_adds(F32(P, 0x70), dt));
                SF32(P, 0x74, rx_adds(F32(P, 0x74), dt));
            } else {
                SF32(P, 0x6c, rx_adds(F32(P, 0x6c), dt));
                SF32(P, 0x74, 0.0f);
                if (U32(P, 0x30c) == 0 && U32(P, 0x2dc) == 0) SF32(P, 0x58, rx_adds(F32(P, 0x58), dt));
            }
            const uint32_t cs = U32(P, 0x124);
            if (cs == 2) {
                const float t = rx_subs(F32(P, 0x2d0), dt);
                SF32(P, 0x2d0, t);
                if (0.0f >= t) rx_fx_stunt_combo_end(P);
            } else if (cs == 1) {
                if (U32(U32(P, 0x128), 0xb60) == 2 || U32(P, 0x2dc) != 0) SF32(P, 0x68, rx_adds(F32(P, 0x68), dt));
            }
            {   /* vf17 (&position) */
                const rx_addr v = U32(U32(P, 0x128), 0x1ac), fn = U32(U32(v, 0), 0x44);
                void (*g)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
                const uint32_t a[1] = {P + 0x130};
                if (g) { const rx_addr s = fx_push(1, a, 0x006cacb5); g(v, P + 0x130); fx_pop(s); }
                else rx_call_original(fn, RX_THISCALL, v, 1, a);
            }
            const uint32_t air = veh_air(U32(U32(P, 0x128), 0x1ac), 0x006caccb);
            if (air != 0 && F32(U32(P, 0x128), 0x1230) > one) SF32(P, 0x314, 0.0f);
            else SF32(P, 0x314, rx_adds(dt, F32(P, 0x314)));
            const rx_addr p2 = U32(P, 0x128);
            if (F32(p2, 0x116c) > 0.0f) SU32(P, 0x41c, 0);
            if (U32(P, 0x41c) != 0) {
                int clear = 0;
                if (F32(P, 0x314) > one) {
                    highlight_progress(U32(p2, 0x1f0), 0x20, 0);   /* MXHighlightTracking_AddProgress (product boundary) */
                    clear = 1;
                } else if (U32(U32(p2, 0x1ac), 0x280) != 0) clear = 1;
                else {
                    const float b = F32(p2, 0xb38);
                    if (b > 0.0f) {
                        const uint32_t k = U32(p2, 0xb44);
                        if (k == 1 || k == 4 || k == 7 || k == 0xa) clear = 1;
                    }
                }
                if (clear) SU32(P, 0x41c, 0);
            }
            int reset = 0;
            if (U32(P, 0x420) != 0) {
                if (F32(P, 0x314) > one && !(F32(U32(P, 0x128), 0xb38) > 0.0f)) {
                    if (U32(P, 0x424) != 0 && U32(P, 0x428) != 0 && U32(P, 0x42c) != 0) {
                        highlight_progress(U32(U32(P, 0x128), 0x1f0), 0x21, 0);
                    }
                    reset = 1;
                } else if (U32(U32(U32(P, 0x128), 0x1ac), 0x280) != 0) reset = 1;
            } else if (U32(U32(U32(P, 0x128), 0x1ac), 0x280) != 0 || F32(P, 0x314) > one) reset = 1;
            if (reset) { SU32(P, 0x420, 0); SU32(P, 0x424, 0); SU32(P, 0x428, 0); SU32(P, 0x42c, 0); }
        }
        { const rx_addr s = fx_push(0, 0, 0x006cae4b); rx_fx_stunt_combo(P); fx_pop(s); }
    }
    if (U32(0, 0x00d5ab64) != 0) {
        const rx_addr s = fx_push(0, 0, 0x006cae58);
        rx_fx_stunt_refresh(P);
        fx_pop(s);
        SU32(0, 0x00d5ab64, 0);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cab80, rx_fx_stunt_score)

/* ---- 006cd870 (EAX = player, xmm0 = score): the "StuntFinalScore" event on the stunt channel [0x00a94318], payload
 * {player, float} (8 bytes; type .P8MXPlayer@@M 0x009cbda8). Frame: sub 0x10 (the payload). ---- */
void rx_fx_stunt_final_score(rx_addr player, float score) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x10);
    SU32(sp0, 0, player); SF32(sp0, 4, score);
    post_event(U32(0, 0x00a94318), 0x009cbda8, 0x0092fc60, sp0, 8, 0x006cd8ab, 0x006cd8b7);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cd870, rx_fx_stunt_final_score)

/* ---- 006caaf0 (ECX = scorer): the combo ends. +0x124 = 0; with a player whose activity is InProgress (state 2) the score is
 * ((tricks * 10 - style - combo clocks) / tricks) and the "StuntFinalScore" event is raised (006cd870); otherwise "StuntFailed"
 * (0x92fd20, type 0x9cbeb4, payload the player) on the stunt channel. Frame: push ecx / esi / edi. ---- */
void rx_fx_stunt_combo_end(rx_addr P) {
    const rx_addr pl = U32(P, 0x128);
    SU32(P, 0x124, 0);
    const rx_addr act = pl ? U32(pl, 0x94) : 0;
    if (pl && act && U32(act, 0x21c) == 2) {
        const float n = (float)I32(P, 0x418);
        float f = rx_muls(n, 10.0f /* 0x92fd88 */);
        f = rx_subs(f, F32(P, 0xc));
        f = rx_subs(f, F32(P, 0x28));
        f = rx_subs(f, F32(P, 0x2c));
        f = rx_divs(f, n);
        rx_fx_stunt_final_score(pl, f);
    } else {
        const rx_addr sp0 = rx_sp();
        rx_set_sp(sp0 - 0x10);
        SU32(sp0, 0, pl);
        post_event(U32(0, 0x00a94318), 0x009cbeb4, 0x0092fd20, sp0, 4, 0x006cab64, 0x006cab72);
        rx_set_sp(sp0);
    }
}
RX_FUNCTION(0x006caaf0, rx_fx_stunt_combo_end)

/* ---- 006cc040 (ESI = scorer): a running combo finishes: its timer +0x2d0 = [0x9c3e3c] for a fresh combo (+0x2d8 0) or
 * +0x2d4 * [0x9c3e40]; shorter than [0x977470]: it ends at once (tail jump to 006caaf0, ECX = scorer, not in the core); else
 * +0x2d4 = the timer, the "combo finishing" event (0x92fcc0, type 0x9cbeb4, payload the player) and the state 2. Frame: push
 * ecx (the payload); push ebx; push edi. */
void rx_fx_stunt_combo_finish(rx_addr P) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0xc;
    const fx_saved sv = fx_save();
    SU32(sp0, -4, fx_reg(FX_ECX));
    fx_push_regs(sp0 - 4, 2, (int[]){FX_EBX, FX_EDI});
    rx_set_sp(E);
    const float t = U32(P, 0x2d8) == 0 ? F32(0, 0x009c3e3c) : rx_muls(F32(P, 0x2d4), F32(0, 0x009c3e40));
    SF32(P, 0x2d0, t);
    if (F32(0, 0x00977470) > t) {
        fx_restore(sv);
        rx_set_sp(sp0);
        rx_fx_stunt_combo_end(P);
        return;
    }
    const rx_addr evs = U32(0, 0x00a94318);
    fx_set_reg(FX_EBX, evs);
    SF32(P, 0x2d4, t);
    SU32(E, 8, U32(P, 0x128));
    post_event(evs, 0x009cbeb4, 0x0092fcc0, E + 8, 4, 0x006cc0af, 0x006cc0c2);
    SU32(P, 0x124, 2);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cc040, rx_fx_stunt_combo_finish)

/* ---- 006cc0d0 (ECX = scorer; stdcall player; RET 4): the scorer's player landed (an active local one): no trick input
 * (+0x5c 0): a long airtime (+0x74 > 1) scores the freestyle counter +0x30 += [0x9c3e58]; with input, a local player with a
 * pad (+0x1f0 >= 0) not in a replay (00551070, race unit) arms the highlight flags +0x41c / +0x420; a running combo (1)
 * with the rider down (+0xbc0 / +0xb74 0) finishes (006cc040). Frame: push esi; push edi. */
void rx_fx_stunt_landed(rx_addr P, rx_addr player) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 2, (int[]){FX_ESI, FX_EDI});
    rx_set_sp(sp0 - 8);
    const rx_addr pl = U32(P, 0x128);
    fx_set_reg(FX_ESI, P); fx_set_reg(FX_EDI, pl);
    int go = player == pl && U32(pl, 4) != 0;
    if (go && U32(pl, 0x10) != 0) go = U32(pl, 4) != 0 && U32(pl, 0x10) == 1;
    if (go) {
        if (U32(P, 0x5c) == 0) {
            if (F32(P, 0x74) > 1.0f && U32(U32(pl, 0x94), 0x21c) == 2) SF32(P, 0x30, rx_adds(F32(P, 0x30), F32(0, 0x009c3e58)));
        } else if (U32(pl, 4) != 0 && U32(pl, 0x10) == 0 && I32(pl, 0x1f0) >= 0) {
            const rx_addr s = fx_push(0, 0, 0x006cc160);
            const uint32_t r = rx_player_has_ghost(pl);
            fx_pop(s);
            if (r == 0) { SU32(P, 0x41c, 1); SU32(P, 0x420, 1); }
        }
        if (U32(P, 0x124) == 1 && U32(pl, 0xbc0) == 0 && U32(pl, 0xb74) == 0) {
            const rx_addr s = fx_push(0, 0, 0x006cc198); rx_fx_stunt_combo_finish(P); fx_pop(s);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cc0d0, rx_fx_stunt_landed)

/* ---- 006cc1f0 (EAX = scorer, EDI = trick): whether the trick is new in this combo: the 100 combo slots (+0x140) are
 * scanned to the trick (not new) or the first empty slot (taken by the trick). Frame: push ebx; push esi. */
uint32_t rx_fx_stunt_combo_new(rx_addr P, rx_addr trick) {
    SU32(rx_sp(), -4, fx_reg(FX_EBX)); SU32(rx_sp(), -8, fx_reg(FX_ESI));
    for (uint32_t i = 0; i < 100; i++) {
        const rx_addr v = U32(P, 0x140 + 4 * i);
        if (v == trick) return 0;
        if (v == 0) { SU32(P, 0x140 + 4 * i, trick); return 1; }
    }
    return 1;
}
RX_FUNCTION(0x006cc1f0, rx_fx_stunt_combo_new)

/* ---- 006cc230 (ECX = the scorer P + 0xc70; stack vehicle, trigger; RET 8): the scorer's "VehicleEntered" handler on the vehicle
 * trigger channel [0x00a89bc8] (registered by 006cdd50, build_activity_driver.c): only for the scorer's own player (+0x128) and
 * that player's vehicle (+0x1ac), only a "FlowJudging" trigger (+0xc0): when it is the trigger the scorer saw last (+0x80) the flow
 * judging time (FMXJudgingParameters 0x9c3e54) is added to +0x34 while the player's activity is InProgress (006ccf90); the trigger
 * takes the first empty slot of the scorer's 0x28-entry flow-trigger list (+0x84; an already listed one leaves the list) and +0x80
 * is the trigger either way. Frame: push edi / ebx. */
void rx_fx_stunt_vehicle_entered(rx_addr P, rx_addr vehicle, rx_addr trigger) {
    if (U32(P, 0) == 0) return;
    const rx_addr pl = U32(P, 0x128);
    if (vehicle != U32(pl, 0x1ac)) return;
    if (crt_strcmp(trigger + 0xc0, 0x00960710 /* "FlowJudging" */) != 0) return;
    if (trigger == U32(P, 0x80)) {                       /* 006ccf90 inline */
        const rx_addr act = U32(pl, 0x94);
        if (U32(act, 0x21c) == 2) SF32(P, 0x34, rx_adds(F32(P, 0x34), F32(0, 0x009c3e54)));
    }
    for (uint32_t i = 0; i < 0x28; i++) {
        const rx_addr e = U32(P, 0x84 + 4 * i);
        if (trigger == e) break;
        if (e == 0) { SU32(P, 0x84 + 4 * i, (uint32_t)trigger); break; }
    }
    SU32(P, 0x80, (uint32_t)trigger);
}
RX_FUNCTION(0x006cc230, rx_fx_stunt_vehicle_entered)

/* ---- 006cb500 (ECX = best, EDX = candidate trick; stdcall v; RET 4): the best trick so far (+0x430, its value +0x434): none
 * or a better score (+0x78) takes it; an equal score takes it with a larger v. */
void rx_fx_stunt_best(rx_addr B, rx_addr cand, float v) {
    const rx_addr cur = U32(B, 0x430);
    const float a = cur ? F32(cand, 0x78) : 0.0f, b = cur ? F32(cur, 0x78) : 0.0f;
    if (cur == 0 || a > b || (a == b && v > F32(B, 0x434))) { SU32(B, 0x430, cand); SF32(B, 0x434, v); }
}
RX_FUNCTION(0x006cb500, rx_fx_stunt_best)

/* ---- the trick maps (std::map<std::string, int>, +0x324 airborne / +0x344 ground: how often each trick was started) ---- */
/* 006cd4a0 (stdcall map, &result, &pair, hint proxy, hint node; RET 0x14): insert with a hint (fx_tree.c) at the call site */
static rx_addr map_insert_call(rx_addr map, rx_addr result, rx_addr pair, rx_addr hp, rx_addr hn, rx_addr ret) {
    const uint32_t a[5] = {map, result, pair, hp, hn};
    const rx_addr s = fx_push(5, a, ret);
    const rx_addr r = rx_fx_map_insert_hint(map, result, pair, hp, hn);
    fx_pop(s);
    return r;
}

/* ---- 006cd3f0 (EDI = map; stdcall key string*; RET 4): map[key] -> &value: lower_bound (008519a0, rx_world); a missing key
 * is inserted as {key, 0} at the hint (006cd4a0). Checked iterators: _invalid_parameter_noinfo (never in practice). Frame:
 * sub esp, 0x2c; ebx / ebp / esi: E = sp0 - 0x38: +0x10 the result iterator, +0x14 the head, +0x18 the pair {string, +0x34
 * int}. */
rx_addr rx_fx_trick_map_get(rx_addr map, rx_addr key) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x2c, E = e0 - 0xc;
    const fx_saved sv = fx_save();
    fx_push_regs(e0, 2, (int[]){FX_EBX, FX_EBP});
    fx_set_reg(FX_EBP, key);
    fx_push_regs(e0 - 8, 1, (int[]){FX_ESI});
    rx_set_sp(E);
    rx_addr node;
    { const uint32_t a[1] = {key}; const rx_addr s = fx_push(1, a, 0x006cd402); node = rx_world_string_map_lower_bound(map, key); fx_pop(s); }
    fx_set_reg(FX_EBX, node);
    if (map == 0) invalid_parameter();
    rx_addr it = U32(map, 0);
    fx_set_reg(FX_ESI, it);
    SU32(E, 0x14, U32(map, 0x18));
    if (it == 0) invalid_parameter();
    int insert = node == U32(E, 0x14);
    if (!insert) {
        const uint32_t a[2] = {key, node + 0xc};
        const rx_addr s = fx_push(2, a, 0x006cd436);
        insert = (crt_str_less(key, node + 0xc) & 0xff) != 0;
        fx_pop(s);
    }
    if (insert) {
        { const uint32_t a[1] = {key}; const rx_addr s = fx_push(1, a, 0x006cd448); crt_str_ctor_copy(E + 0x18, key); fx_pop(s); }
        SU32(E, 0x34, 0);
        const rx_addr r = map_insert_call(map, E + 0x10, E + 0x18, it, node, 0x006cd462);
        it = U32(r, 0); node = U32(r, 4);
        fx_set_reg(FX_ESI, it); fx_set_reg(FX_EBX, node);
        { const rx_addr s = fx_push(0, 0, 0x006cd471); crt_str_dtor(E + 0x18); fx_pop(s); }
    }
    if (it == 0) invalid_parameter();
    else if (node == U32(U32(it, 0), 0x18)) invalid_parameter();
    fx_restore(sv);
    rx_set_sp(sp0);
    return node + 0x28;
}
RX_FUNCTION(0x006cd3f0, rx_fx_trick_map_get)

/* the trick's map: airborne tricks (+0x64) +0x324, ground ones +0x344 */
static rx_addr trick_map(rx_addr P, rx_addr trick) { return P + (U32(trick, 0x64) != 0 ? 0x324 : 0x344); }

/* ---- 006cb960 (ECX = scorer, EAX = trick): how often the trick was started (its map's entry for its name). Frame: sub esp,
 * 0x1c; esi / edi: E = sp0 - 0x24: +8 the name string. */
uint32_t rx_fx_trick_count(rx_addr P, rx_addr trick) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0x24;
    const fx_saved sv = fx_save();
    fx_push_regs(sp0 - 0x1c, 2, (int[]){FX_ESI, FX_EDI});
    fx_set_reg(FX_ESI, P);
    rx_set_sp(E);
    const int air = U32(trick, 0x64) != 0;
    { const uint32_t a[1] = {trick}; const rx_addr s = fx_push(1, a, air ? 0x006cb978 : 0x006cb98b); crt_str_ctor_cstr(E + 8, trick); fx_pop(s); }
    fx_set_reg(FX_EDI, trick_map(P, trick));
    rx_addr v;
    { const uint32_t a[1] = {E + 8}; const rx_addr s = fx_push(1, a, 0x006cb99b); v = rx_fx_trick_map_get(trick_map(P, trick), E + 8); fx_pop(s); }
    const uint32_t n = U32(v, 0);
    fx_set_reg(FX_ESI, n);
    { const rx_addr s = fx_push(0, 0, 0x006cb9a7); crt_str_dtor(E + 8); fx_pop(s); }
    fx_restore(sv);
    rx_set_sp(sp0);
    return n;
}
RX_FUNCTION(0x006cb960, rx_fx_trick_count)

/* 00587ca0 (ECX = count, EDX = name; stdcall player; RET 4): the event void(MXPlayer*, int) (type 0x9cf610) to [0xa94318].
 * Frame: sub esp, 0x10 (/GS cookie at +0xc); esi / edi: E = sp0 - 0x18: the payload {player, count} at +0xc. (Not in a plan;
 * the combo's continuation below is its only caller here.) */
static void event_player_int(rx_addr name, rx_addr player, uint32_t count) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x10, E = e0 - 8;
    const fx_saved sv = fx_save();
    SU32(e0, 0xc, U32(0, 0x009b45e4) ^ e0);
    fx_push_regs(e0, 2, (int[]){FX_ESI, FX_EDI});
    const rx_addr evs = U32(0, 0x00a94318);
    fx_set_reg(FX_ESI, evs);
    rx_set_sp(E);
    SU32(E, 0x10, count); SU32(E, 0xc, player);
    post_event(evs, 0x009cf610, name, E + 0xc, 8, 0x00587cd4, 0x00587ce2);
    SU32(e0, -4, 0x00587cef);   /* __security_check_cookie */
    fx_restore(sv);
    rx_set_sp(sp0);
}
/* 006cb390 (EAX = scorer): a finishing combo continues: +0x2d8 (its continuations) + 1, running again, the event 0x92fc80
 * (player, continuations). Frame: push ecx. */
static void stunt_combo_continue(rx_addr P) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_ECX));
    SU32(P, 0x2d8, U32(P, 0x2d8) + 1);
    const uint32_t n = U32(P, 0x2d8);
    SU32(P, 0x124, 1);
    rx_set_sp(sp0 - 4);
    { const uint32_t a[1] = {U32(P, 0x128)}; const rx_addr s = fx_push(1, a, 0x006cb3b9); event_player_int(0x0092fc80, a[0], n); fx_pop(s); }
    rx_set_sp(sp0);
}

/* ---- 006cbbe0 "StuntBegins" (ECX = scorer; stdcall player, trick, kind; RET 0xc): for an active local player: the combo clock
 * +0x58 0; the same trick again (+0x60) outside the physics stunts 1..4 scores +0x30 += [0x9c3e70] (006ccf60), else it becomes
 * +0x60; the combo starts (006cb180) or continues (006cb390); the trick's start count + 1 (006cd3f0); the combo's new-trick
 * check (006cc1f0); a wheelie (1) / stoppie (2) notes its start position (vf17 into +0x2f0), 3 / 4 clear +0x300 / +0x304; the
 * event "StuntBegins" (0x92fd00: player, continuing, count; 0058d7d0, rx_race). Frame: push ebp; and esp, -8; sub esp, 0x24;
 * ebx / esi / edi: E: +0x10 continuing, +0x14 the name string. */
void rx_fx_stunt_begins(rx_addr P, rx_addr player, rx_addr trick, uint32_t kind) {
    fx_frame f;
    fx_frame_enter(&f, 0x24, 8, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    fx_set_reg(FX_ESI, P);
    const rx_addr pl = U32(P, 0x128);
    int go = player == pl && trick != 0 && U32(pl, 4) != 0;
    if (go && U32(pl, 0x10) != 0) go = U32(pl, 4) != 0 && U32(pl, 0x10) == 1;
    if (player == pl) fx_set_reg(FX_EBX, trick);
    if (!go) { fx_frame_leave(&f); return; }
    fx_set_reg(FX_EDI, 1);
    SF32(P, 0x58, 0.0f);
    if (U32(P, 0x60) == trick && !(kind >= 1 && kind <= 4)) {
        const uint32_t a[1] = {rx_fb(F32(0, 0x009c3e70))};
        const rx_addr s = fx_push(1, a, 0x006cbc6b); rx_fx_stunt_add_30(P, F32(0, 0x009c3e70)); fx_pop(s);
    } else SU32(P, 0x60, trick);
    const uint32_t st = U32(P, 0x124);
    SU32(E, 0x10, 0);
    if (st == 0) { const rx_addr s = fx_push(0, 0, 0x006cbc9f); rx_fx_stunt_combo_start(P); fx_pop(s); }
    else if (st == 1) SU32(E, 0x10, 1);
    else if (st == 2) { { const rx_addr s = fx_push(0, 0, 0x006cbc92); stunt_combo_continue(P); fx_pop(s); } SU32(E, 0x10, 1); }
    const int air = U32(trick, 0x64) != 0;
    { const uint32_t a[1] = {trick}; const rx_addr s = fx_push(1, a, air ? 0x006cbcb0 : 0x006cbcc3); crt_str_ctor_cstr(E + 0x14, trick); fx_pop(s); }
    fx_set_reg(FX_EDI, trick_map(P, trick));
    {   rx_addr v;
        const uint32_t a[1] = {E + 0x14}; const rx_addr s = fx_push(1, a, 0x006cbcd3); v = rx_fx_trick_map_get(trick_map(P, trick), E + 0x14); fx_pop(s);
        SU32(v, 0, U32(v, 0) + 1); }
    { const rx_addr s = fx_push(0, 0, 0x006cbcdf); crt_str_dtor(E + 0x14); fx_pop(s); }
    fx_set_reg(FX_EDI, trick);
    { const rx_addr s = fx_push(0, 0, 0x006cbce8); rx_fx_stunt_combo_new(P, trick); fx_pop(s); }
    if (kind == 1 || kind == 2) {
        SU32(P, 0x2e0, kind == 1); SU32(P, 0x2dc, 1);
        const rx_addr v = U32(U32(P, 0x128), 0x1ac), fn = U32(U32(v, 0), 0x44);
        void (*g)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
        const uint32_t a[1] = {P + 0x2f0};
        if (g) { const rx_addr s = fx_push(1, a, kind == 1 ? 0x006cbd23 : 0x006cbd53); g(v, P + 0x2f0); fx_pop(s); }
        else rx_call_original(fn, RX_THISCALL, v, 1, a);
    } else if (kind == 3) SU32(P, 0x300, 0);
    else if (kind == 4) SU32(P, 0x304, 0);
    uint32_t n;
    { const rx_addr s = fx_push(0, 0, 0x006cbd74); n = rx_fx_trick_count(P, trick); fx_pop(s); }
    const rx_addr evs = U32(0, 0x00a94318);
    fx_set_reg(FX_ESI, evs);
    {   const uint32_t a[2] = {0x0092fd00, U32(P, 0x128)};
        const rx_addr s = fx_push(2, a, 0x006cbd91);
        rx_event_player_2int(evs, 0x0092fd00, a[1], U32(E, 0x10), n);
        fx_pop(s); }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006cbbe0, rx_fx_stunt_begins)

/* ---- 006ccef0 (EAX = scorer, ECX = trick): the highlight's three-trick chain (+0x424 / +0x428 / +0x42c): a trick not in it
 * takes the first free place; the second one again restarts the chain with it. Frame: push esi; push edi. ---- */
void rx_fx_stunt_chain_note(rx_addr P, rx_addr trick) {
    SU32(rx_sp(), -4, fx_reg(FX_ESI)); SU32(rx_sp(), -8, fx_reg(FX_EDI));
    const rx_addr a = U32(P, 0x424), b = U32(P, 0x428), c = U32(P, 0x42c);
    if (trick == a || trick == b || trick == c) {
        if (trick == U32(P, 0x428)) { SU32(P, 0x42c, 0); SU32(P, 0x428, 0); SU32(P, 0x424, trick); }
    } else if (a == 0) SU32(P, 0x424, trick);
    else if (b == 0) SU32(P, 0x428, trick);
    else if (c == 0) SU32(P, 0x42c, trick);
}
RX_FUNCTION(0x006ccef0, rx_fx_stunt_chain_note)

/* ---- 006cbdb0 "StuntEnds" (ECX = scorer; stdcall player, trick, unused; RET 0xc): for an active local player: the trick count
 * +0x418 + 1, the score +0x78 += the trick's (+0x78), the best trick (006cb500, value 0); a wheelie / stoppie (+0x2dc, the
 * kind +0x2e0) measures its length from the start position (vf17, SqrtF) into +0x308, the records (MXRecords_LongestWheelie /
 * _LongestStoppie) and the highlights 0x2e (>= [0x9d7b34]) / 0x2f (>= 75); the combo finishes once the rider is down
 * (006cc040); an airborne trick of a local pad arms +0x41c and the chain (006ccef0); the name joins the five recent ones
 * (+0x364, 32 bytes each, flags +0x404) unless already there. Frame: push ebp; and esp, -16; sub esp, 0x14; ebx / esi / edi:
 * E: +0xc the length / the flag cursor, +0x10 the position. */
void rx_fx_stunt_ends(rx_addr P, rx_addr player, rx_addr trick) {
    fx_frame f;
    fx_frame_enter(&f, 0x14, 16, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    fx_set_reg(FX_ESI, P); fx_set_reg(FX_EDI, player);
    const rx_addr pl = U32(P, 0x128);
    if (player == pl) fx_set_reg(FX_EBX, trick);
    int go = player == pl && trick != 0 && U32(pl, 4) != 0;
    if (go && U32(pl, 0x10) != 0) go = U32(pl, 4) != 0 && U32(pl, 0x10) == 1;
    if (!go) { fx_frame_leave(&f); return; }
    SU32(P, 0x418, U32(P, 0x418) + 1);
    SF32(P, 0x78, rx_adds(F32(trick, 0x78), F32(P, 0x78)));
    { const uint32_t a[1] = {0}; const rx_addr s = fx_push(1, a, 0x006cbe20); rx_fx_stunt_best(P, trick, 0.0f); fx_pop(s); }
    const rx_addr p2 = U32(P, 0x128);
    if (U32(p2, 4) != 0 && U32(p2, 0x10) == 0 && U32(P, 0x2dc) != 0) {
        {   const rx_addr v = U32(player, 0x1ac), fn = U32(U32(v, 0), 0x44);
            void (*g)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
            const uint32_t a[1] = {E + 0x10};
            if (g) { const rx_addr s = fx_push(1, a, 0x006cbe59); g(v, E + 0x10); fx_pop(s); }
            else rx_call_original(fn, RX_THISCALL, v, 1, a); }
        const float dz = rx_subs(F32(E, 0x18), F32(P, 0x2f8)), dy = rx_subs(F32(E, 0x14), F32(P, 0x2f4)), dx = rx_subs(F32(E, 0x10), F32(P, 0x2f0));
        const float q = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
        SF32(E, 0x18, dz); SF32(E, 0x14, dy); SF32(E, 0x10, dx);
        float r;
        { const uint32_t a[1] = {rx_fb(q)}; const rx_addr s = fx_push(1, a, 0x006cbebd); r = phys_sqrtf(q); fx_pop(s); }
        SF32(E, 0xc, r); SF32(P, 0x308, r);
        const int wheelie = U32(P, 0x2e0) != 0;
        const uint32_t pad = U32(U32(P, 0x128), 0x1f0);
        const int32_t len = rx_cvtt((double)r);
        if (wheelie) {
            const uint32_t a[1] = {(uint32_t)len}; const rx_addr s = fx_push(1, a, 0x006cbef3);
            rx_records_longest_wheelie(pad, U32(0, 0x00a946c0), len); fx_pop(s);
        } else {
            const uint32_t a[1] = {(uint32_t)len}; const rx_addr s = fx_push(1, a, 0x006cbf36);
            rx_records_longest_stoppie(pad, U32(0, 0x00a946c0), len); fx_pop(s);
        }
        if (F32(E, 0xc) >= (wheelie ? F32(0, 0x009d7b34) : 75.0f /* 0x977b0c */)) {
            highlight_progress(U32(U32(P, 0x128), 0x1f0), wheelie ? 0x2e : 0x2f, 0);   /* MXHighlightTracking_AddProgress (product boundary) */
        }
    }
    const rx_addr p3 = U32(P, 0x128);
    SU32(P, 0x2dc, 0); SU32(P, 0x300, 0); SU32(P, 0x304, 0);
    if (U32(p3, 0xb74) == 0) { const rx_addr s = fx_push(0, 0, 0x006cbf87); rx_fx_stunt_combo_finish(P); fx_pop(s); }
    if (U32(trick, 0x64) != 0 && I32(U32(P, 0x128), 0x1f0) > -1) {
        SU32(P, 0x41c, 1);
        const rx_addr s = fx_push(0, 0, 0x006cbfaf); rx_fx_stunt_chain_note(P, trick); fx_pop(s);
    }
    for (uint32_t i = 0; i < 5; i++) {
        SU32(E, 0xc, P + 0x404 + 4 * i);
        fx_set_reg(FX_EDI, i); fx_set_reg(FX_EBX, P + 0x364 + 0x20 * i);
        if (U32(P, 0x404 + 4 * i) == 0) {
            const uint32_t a[3] = {P + 0x364 + 0x20 * i, trick, 0x20};
            const rx_addr s = fx_push(3, a, 0x006cc026); crt_strncpy(P + 0x364 + 0x20 * i, trick, 0x20); fx_pop(s);
            SU32(P, 0x404 + 4 * i, 1);
            break;
        }
        if (crt_strcmp(trick, P + 0x364 + 0x20 * i) == 0) break;
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006cbdb0, rx_fx_stunt_ends)

/* ---- 006cb6a0 "combo lost" (ECX = scorer; stdcall player, unused; RET 8): an enabled scorer's active local player: the
 * combo-lost event (0x92fd20, type 0x9cbeb4, payload the player in its argument slot) and the combo state, wheelie flag and
 * airtime (+0x124 / +0x2dc / +0x68) cleared. Frame: push ebx; push esi; push edi. */
void rx_fx_stunt_combo_lost(rx_addr P, rx_addr player) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0xc;
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 2, (int[]){FX_EBX, FX_ESI});
    fx_set_reg(FX_ESI, P);
    fx_push_regs(sp0 - 8, 1, (int[]){FX_EDI});
    rx_set_sp(E);
    const rx_addr pl = U32(P, 0x128);
    int go = U32(P, 0) != 0 && player == pl && U32(pl, 4) != 0;
    if (go && U32(pl, 0x10) != 0) go = U32(pl, 4) != 0 && U32(pl, 0x10) == 1;
    if (go) {
        const rx_addr evs = U32(0, 0x00a94318);
        fx_set_reg(FX_EBX, evs);
        SU32(E, 0x10, pl);
        post_event(evs, 0x009cbeb4, 0x0092fd20, E + 0x10, 4, 0x006cb6e8, 0x006cb6fb);
        fx_set_reg(FX_EDI, 0x0092fd20);
        SU32(P, 0x2dc, 0); SU32(P, 0x124, 0); SF32(P, 0x68, 0.0f);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cb6a0, rx_fx_stunt_combo_lost)

/* ---- 006ccbc0 (EAX = the activity): the trick table size [0xa9431c]: the level's MXTrigger entities (class 0x1028, at most 255:
 * SceneManager_CollectTyped of the scene +0x234 +0x20138) named 0x960710 ("FlowJudging": the flow-judging zones, the triggers
 * 006cc230 tracks). Frame: sub esp, 0x400; esi / edi: E = sp0 - 0x408: +8 the overflow flag, +0xc the entities. */
void rx_fx_trick_table_count(rx_addr act) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x400, E = e0 - 8;
    const fx_saved sv = fx_save();
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    fx_push_regs(e0, 1, (int[]){FX_ESI});
    fx_set_reg(FX_ESI, scene);
    fx_push_regs(e0 - 4, 1, (int[]){FX_EDI});
    rx_set_sp(E);
    SU32(E, 0xc, 0);
    { const uint32_t a[3] = {E + 0x10, 0, 0x3f8}; FX_CRT(3, a, 0x006ccbed, crt_memset(E + 0x10, 0, 0x3f8)); }
    uint32_t n;
    { const uint32_t a[4] = {0x1028, E + 0xc, 0xff, E + 8}; const rx_addr s = fx_push(4, a, 0x006ccc0b);
      n = rx_scene_manager_collect_typed(scene, 0x1028, E + 0xc, 0xff, E + 8); fx_pop(s); }
    SU32(0, 0x00a9431c, 0);
    if (n != 0) {
        SU32(E, -4, sv.ebx);
        uint32_t k = 0;
        for (uint32_t i = 0; i < n; i++) if (crt_strcmp(U32(E, 0xc + 4 * i) + 0xc0, 0x00960710) == 0) k++;
        SU32(0, 0x00a9431c, k);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006ccbc0, rx_fx_trick_table_count)

/* ======== the scorer's handlers on the stunt channel [0x00a94304] (006caf20 registered them; the trick-start unit posts the
 * events: "StuntBegin" when a mapped trick starts, "StuntHoldBegin" / "StuntHoldEnd" from the air hold, "StuntCompleted" when it
 * completes, "StuntFailed" / "StuntKickOut" / "ComboFromStunt" / "StuntLanded" / "StuntTakeoff"). Each one is an
 * EventHandlerProxy callback: ECX = the scorer P, then the event's two words (player, trick). ======== */

/* the "active local player" test the handlers share: the event's player is the scorer's (+0x128) and it is local (or a replay) */
static int started_local(rx_addr P, rx_addr player) {
    if (U32(P, 0) == 0) return 0;
    const rx_addr pl = U32(P, 0x128);
    if (player != pl || U32(pl, 4) == 0) return 0;
    if (U32(pl, 0x10) != 0 && (U32(pl, 4) == 0 || U32(pl, 0x10) != 1)) return 0;
    return 1;
}

/* ---- 006cb1b0 "StuntBegin" (ECX = scorer; stdcall player, trick; RET 8): the combo clock +0x58 = 0; airborne -> +0x5c = 1; the
 * combo starts (006cb180) / continues (006cb390, "continuing" = 1); the same trick again outside a physics stunt adds
 * [0x9c3e70] to the freestyle counter +0x30 (006ccf60) when the activity is freestyle; +0x60 = the trick; the trick's map
 * (+0x324 airborne / +0x344 ground) counts it and a second one in the same combo (not a continuation) adds [0x9c3e60]; +0x68 = 0;
 * the combo's new-trick check (006cc1f0), the trick count (006cb960) and "StuntBegins" (0x92fd00: player, continuing, count;
 * 0058d7d0). Frame: push ebp; and esp, -8; sub esp, 0x24; ebx / esi / edi: the std::string temporary at E + 0x14. ---- */
void rx_fx_stunt_begin_evt(rx_addr P, rx_addr player, rx_addr trick) {
    fx_frame f; fx_frame_enter(&f, 0x24, 8, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr A = f.E + 0x14;                 /* the std::string temporary ([esp + 0x18] after `push ebx`) */
    fx_set_reg(FX_ESI, P);
    fx_set_reg(FX_EBX, 1);
    if (!started_local(P, player)) { fx_frame_leave(&f); return; }
    uint32_t cont = 0;
    SF32(P, 0x58, 0.0f);
    if (veh_air(U32(U32(P, 0x128), 0x1ac), 0x006cb21f) != 0) SU32(P, 0x5c, 1);
    const uint32_t cs = U32(P, 0x124);
    if (cs == 0) rx_fx_stunt_combo_start(P);
    else if (cs == 1) cont = 1;
    else if (cs == 2) { stunt_combo_continue(P); cont = 1; }
    if (U32(P, 0x60) == trick) {
        const rx_addr pl = U32(P, 0x128);
        if (U32(U32(pl, 0x94), 0x21c) == 2) SF32(P, 0x30, rx_adds(F32(P, 0x30), F32(0, 0x009c3e70)));
    } else SU32(P, 0x60, trick);
    const rx_addr map = P + (U32(trick, 0x64) != 0 ? 0x324 : 0x344);
    fx_set_reg(FX_EDI, map);
    { const rx_addr s = fx_push(1, (uint32_t[]){trick}, 0x006cb290); crt_str_ctor_cstr(A, trick); fx_pop(s); }
    { const rx_addr s = fx_push(1, (uint32_t[]){A}, 0x006cb2a1);
      const rx_addr v = rx_fx_trick_map_get(map, A);
      SU32(v, 0, U32(v, 0) + 1);
      fx_pop(s); }
    { const rx_addr s = fx_push(0, 0, 0x006cb2ac); crt_str_dtor(A); fx_pop(s); }
    { const rx_addr s = fx_push(1, (uint32_t[]){trick}, 0x006cb2b7); crt_str_ctor_cstr(A, trick); fx_pop(s); }
    int twice;
    { const rx_addr s = fx_push(1, (uint32_t[]){A}, 0x006cb2c2);
      twice = I32(rx_fx_trick_map_get(map, A), 0) >= 2;
      fx_pop(s); }
    { const rx_addr s = fx_push(0, 0, 0x006cb2dd); crt_str_dtor(A); fx_pop(s); }
    if (twice && cont == 0) {
        const uint32_t a[1] = {rx_fb(F32(0, 0x009c3e60))};
        const rx_addr s = fx_push(1, a, 0x006cb2fa); rx_fx_stunt_add_30(P, F32(0, 0x009c3e60)); fx_pop(s);
    }
    SU32(P, 0x68, 0);
    fx_set_reg(FX_EDI, trick);
    { const rx_addr s = fx_push(0, 0, 0x006cb34e); rx_fx_stunt_combo_new(P, trick); fx_pop(s); }
    const uint32_t n = rx_fx_trick_count(P, trick);
    { const uint32_t a[2] = {U32(P, 0x128), 0x0092fd00};
      const rx_addr s = fx_push(2, a, 0x006cb374);
      rx_event_player_2int(U32(0, 0x00a94318), 0x0092fd00, a[0], cont, n);
      fx_pop(s); }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006cb1b0, rx_fx_stunt_begin_evt)

/* ---- 006cb3c0 "StuntCompleted" (ECX = scorer; stdcall player, trick; RET 8): the trick count +0x418 + 1; the score +0x78 +=
 * the trick's; the combo's airtime +0x64 += +0x68 and the best trick (006cb500, the old +0x68); no current trick / physics
 * stunt in the tracker -> the combo finishes (006cc040); an airborne trick notes the chain (006ccef0) and joins the five recent
 * names (+0x364, 0x20 each, flags +0x404) unless already there. Frame: push ecx / ebx / ebp / esi / edi. ---- */
void rx_fx_stunt_completed(rx_addr P, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0x14;
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX));
    fx_push_regs(sp0 - 4, 3, (int[]){FX_EBX, FX_EBP, FX_ESI});
    fx_set_reg(FX_ESI, P);
    SU32(sp0 - 0x14, 0, sv.edi);
    fx_set_reg(FX_EDI, player);
    rx_set_sp(E);
    if (!started_local(P, player)) { fx_restore(sv); rx_set_sp(sp0); return; }
    fx_set_reg(FX_EBP, trick);
    SU32(P, 0x418, U32(P, 0x418) + 1);
    SF32(P, 0x78, rx_adds(F32(trick, 0x78), F32(P, 0x78)));
    const float old = rx_bf(fx_x87_bits(U32(P, 0x68)));
    SF32(P, 0x64, rx_adds(F32(P, 0x68), F32(P, 0x64)));
    { const uint32_t a[1] = {rx_fb(old)}; const rx_addr s = fx_push(1, a, 0x006cb43c); rx_fx_stunt_best(P, trick, old); fx_pop(s); }
    const rx_addr K = U32(P, 0x128) + 0xb58;
    if (U32(K, 0x68) == 0 && U32(K, 0x1c) == 0) {
        const rx_addr s = fx_push(0, 0, 0x006cb458); rx_fx_stunt_combo_finish(P); fx_pop(s);
    }
    if (U32(trick, 0x64) == 0) { fx_restore(sv); rx_set_sp(sp0); return; }
    { const rx_addr s = fx_push(0, 0, 0x006cb46b); rx_fx_stunt_chain_note(P, trick); fx_pop(s); }
    for (uint32_t i = 0; i < 5; i++) {
        if (U32(P, 0x404 + 4 * i) == 0) {
            const uint32_t a[3] = {P + 0x364 + 0x20 * i, trick, 0x20};
            FX_CRT(3, a, 0x006cb4e5, crt_strncpy(P + 0x364 + 0x20 * i, trick, 0x20));
            SU32(P, 0x404 + 4 * i, 1);
            break;
        }
        if (crt_strcmp(trick, P + 0x364 + 0x20 * i) == 0) break;
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cb3c0, rx_fx_stunt_completed)

/* ---- 006cb560 "ComboFromStunt" (ECX = scorer; stdcall player, trick, unused; RET 0xc): the scorer's player only: the combo's
 * new-trick check (006cc1f0) and, for an airborne trick, the chain (006ccef0). Frame: push esi / edi. ---- */
void rx_fx_stunt_combo_from(rx_addr P, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI)); SU32(sp0 - 8, 0, fx_reg(FX_EDI));
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_ESI, P); fx_set_reg(FX_EDI, trick);
    if (player == U32(P, 0x128)) {
        rx_fx_stunt_combo_new(P, trick);
        if (U32(trick, 0x64) != 0) rx_fx_stunt_chain_note(P, trick);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cb560, rx_fx_stunt_combo_from)

/* ---- 006cb5a0 "StuntLanded" (ECX = scorer; stdcall player, trick; RET 8): an active local player's +0x310 = 1; a local pad
 * (not a replay) notes the chain (006ccef0) and arms the highlight flags +0x420 / +0x41c. Frame: push esi. ---- */
void rx_fx_stunt_landed_evt(rx_addr P, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_ESI, 1);
    if (started_local(P, player)) SU32(P, 0x310, 1);
    const rx_addr pl = U32(P, 0x128);
    if (pl == player && U32(pl, 4) != 0 && U32(pl, 0x10) == 0 && I32(pl, 0x1f0) >= 0) {
        rx_fx_stunt_chain_note(P, trick);
        SU32(P, 0x420, 1);
        SU32(P, 0x41c, 1);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cb5a0, rx_fx_stunt_landed_evt)

/* ---- 006cb610 "StuntTakeoff" (ECX = scorer; stdcall player, trick; RET 8): an active local player's +0x310 = 1 and the
 * combo's new-trick check (006cc1f0). Frame: push edi. ---- */
void rx_fx_stunt_takeoff_evt(rx_addr P, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_EDI));
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_EDI, trick);
    if (started_local(P, player)) {
        SU32(P, 0x310, 1);
        rx_fx_stunt_combo_new(P, trick);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cb610, rx_fx_stunt_takeoff_evt)

/* ---- 006cb660 "StuntKickOut" (ECX = scorer; stdcall player, unused; RET 8): an active local player's combo finishes
 * (006cc040). Frame: push esi. ---- */
void rx_fx_stunt_kickout(rx_addr P, rx_addr player, rx_addr trick) {
    (void)trick;
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_ESI, P);
    if (started_local(P, player)) rx_fx_stunt_combo_finish(P);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006cb660, rx_fx_stunt_kickout)

/* ---- 00581830 ActivityAudioLogic::vf5 (ECX = the activity's audio logic; stdcall player, unused; RET 8): the stunt channel's
 * crowd listener: an active local player excites the crowd by [0x9c47f0] (00674440) and, when the game LCG's draw
 * ([0x9cb860] / count [0x9cb864], (seed >> 16 & 0x7fff) / 32768) is below [0x9c4818], plays reaction 2 (1 for a negative
 * excitement) on every crowd emitter (006745d0). ---- */
void rx_fx_stunt_audio_begin(rx_addr logic, rx_addr player, rx_addr trick) {
    (void)logic; (void)trick;
    if (player == 0 || U32(player, 4) == 0 || U32(player, 0x10) != 0) return;
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, 0);                                    /* push ecx; fld [0x9c47f0]; fstp [esp] */
    SF32(sp0 - 4, 0, rx_bf(fx_x87_bits(U32(0, 0x009c47f0))));
    rx_set_sp(sp0 - 4);
    rx_world_crowd_excite(F32(sp0 - 4, 0));
    rx_set_sp(sp0);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, seed);
    const float f = (float)((double)(float)((seed >> 0x10) & 0x7fffu) * 3.0517578125e-05 /* 0x9776a0, a double */);
    if (F32(0, 0x009c4818) <= f) return;                    /* comiss [0x9c4818], f; jbe */
    const uint32_t kind = (0.0f <= F32(0, 0x009c47f0)) ? 2u : 1u;
    rx_world_crowd_react(kind);
}
RX_FUNCTION(0x00581830, rx_fx_stunt_audio_begin)

/* ---- 00581950 ActivityAudioLogic::vf8 (ECX = the activity's audio logic; stdcall player, trick; RET 8): the stunt channel's
 * "StuntCompleted" listener. The cue 005811a0 with the player under "StuntCompleted" (0x942784), then, for an active local player
 * (player +4 != 0, +0x10 == 0), the crowd excite by [0x9c47f4] (00674440) and, when the game LCG's draw
 * ((seed >> 16 & 0x7fff) / 32768) is below [0x9c481c], reaction 2 (1 for a negative excitement) on every crowd emitter
 * (006745d0). The trick argument is passed on by the event only (never read). Frame: push esi. ---- */
void rx_fx_stunt_audio_complete(rx_addr logic, rx_addr player, rx_addr trick) {
    (void)trick;
    rx_audio_cue(logic, 0x00942784, player, 0);
    if (player == 0 || U32(player, 4) == 0 || U32(player, 0x10) != 0) return;
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, 0);                                    /* push ecx; fld [0x9c47f4]; fstp [esp] */
    SF32(sp0 - 4, 0, rx_bf(fx_x87_bits(U32(0, 0x009c47f4))));
    rx_set_sp(sp0 - 4);
    rx_world_crowd_excite(F32(sp0 - 4, 0));
    rx_set_sp(sp0);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, seed);
    const float f = (float)((double)(float)((seed >> 0x10) & 0x7fffu) * 3.0517578125e-05 /* 0x9776a0, a double */);
    if (F32(0, 0x009c481c) <= f) return;                    /* comiss [0x9c481c], f; jbe */
    const uint32_t kind = (0.0f <= F32(0, 0x009c47f4)) ? 2u : 1u;
    rx_world_crowd_react(kind);
}
RX_FUNCTION(0x00581950, rx_fx_stunt_audio_complete)
