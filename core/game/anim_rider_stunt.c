/* anim_rider_stunt.c -- the rider's stunt (trick) animation start: the player's "StuntBegin" listener and the rider-animation
 * entries it calls (2026-10-21, agent "trickstart").
 *
 * The trick tracker (fx_stunt.c, 006c98c0) posts "StuntBegin" {player, trick} on the stunt channel [0xa94304]. The player's
 * handler (0054d120) starts the trick's animation on the rider: a *ground* trick (the rider's +0xaf10 = 0) goes through
 * 006226c0 -> 00622790 (a scripted trick clip of the rider's trick table), an *air* trick through 00622bc0 (the perf asset's
 * "MXRider_<Anim>" clip drives the float listeners +0x46c / +0x48c that the tracker's hold modes read). Then 0054d3b0
 * suppresses the controls the trick record asks for (SteeringDisabled / IgnoreBrake / IgnoreThrottle) and, online, the
 * "UpdateStuntEvent" packet goes out.
 *
 * RiderAnimation (ra = the rider object, the vehicle's +0x1a0; the tracker's K + 4 + 0x1b4 is the same object):
 *   +0x144 the clip table (+0x248, keyed by clip name: 007fa030 gives the clip length), +0xaf10 the trick state (0 idle,
 *   1 / 2 starting, 3 holding), +0xaf14 the clip, +0xaf18 its length, +0xaf1c the vehicle, +0xaf20 / +0xaf24 the thresholds
 *   the tracker reads, +0xaf28 the variant (1 air / 2 ground), +0xaf2c, +0xaf30 / +0xaf38 / +0xaf3c, +0xaf50, the rider's
 *   string maps +0xaf7c u16 / +0xafbc u32 (air) and +0xaf9c u16 / +0xafdc u32 (ground), the trick tables +0xb01c (0x28
 *   records: u16 id at +0, name at +0x1c; count +0xba24) and +0xba48 (+0xba28, count +0xc450), the clip data +0xd2a0 (0xe10
 *   each, +0xe00 loaded), the float listeners +0x46c / +0x48c ({target, enabled}) with the targets +0x484 / +0x488 / +0x590 /
 *   +0x594 / +0x598, and the pose / trick state +0x5b520 / +0x5b524 / +0x5b590 / +0x5b594 / +0x5b640 / +0x5b6f0 / +0x5b700..
 *   / +0x5b740.
 *
 * The "%sRider_%s" asset name is 006ccfc0 (the vehicle class picks the MX / ATV prefix); it formats into the caller's EDI with
 * ESI as the size (the original keeps those in registers, so the port takes them as arguments).
 *
 * The two string->value map lookups (0063b6e0 u16, 00850cd0 u32) are the same std::map<string, T> get-or-insert as the trick
 * maps (006cd3f0, fx_stunt.c: head +0x18, checked-iterator bounds, lower_bound 008519a0), so their missing-key insert reuses
 * the trick map's instantiation 006cd4a0 (rx_fx_map_insert_hint): the node is the same 0x30 bytes {left, parent, right,
 * string, value at +0x28, two colour bytes} and the inserted pair's value is 0 in both places. The only difference would be the
 * two pad bytes +0x2a / +0x2b of a newly inserted node (the u16 ctor leaves the allocation's residue, the int ctor zeroes
 * them); the animation assets contain these clip names, so the insert path is not taken (notes/fx.md).
 *
 * 006f34d0 ("UpdateStuntEvent", the online stunt event packet) is a network boundary: the oracle runs the original, the product
 * sends nothing (nothing in the race reads it; the caller's branch needs the player's network record +0xb2c).
 */
#include "rx_game.h"
#include "fx.h"
#include "fx_util.h"
#include "crt.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "world_rider.h"
#include "world_rider_extern.h"
#include "race.h"
#include "loop_player.h"
#include "build_rider_misc.h"
#include "build_asset.h"
#include "anim_rider_stunt.h"

/* 006f34d0 (EDI = the player's network record; stdcall a, b, c; RET 0xc): the "UpdateStuntEvent" packet. PRODUCT BOUNDARY. */
static void stunt_event_send(rx_addr rec, uint32_t a, uint32_t b, uint32_t c) {
    const uint32_t args[3] = {a, b, c};
    rx_regs in; memset(&in, 0, sizeof in); in.r[7] = rec;
#ifdef RX_ORACLE
    rx_call_original_ex(0x006f34d0, &in, 0, 3, args);
#else
    (void)args;
#endif
}

/* ---- 006ccfc0 (EAX = the vehicle class; stack the source name; the caller's EDI the destination, ESI the size): the
 * "%sRider_%s" asset name (MX / ATV prefix; no prefix for another class). Frame: none. ---- */
void rx_rider_stunt_name(rx_addr dst, uint32_t size, uint32_t klass, rx_addr src) {
    rx_addr prefix = 0;
    if (klass == 0) prefix = 0x0092fc24;              /* "MX" */
    else if (klass == 1) prefix = 0x0092fc28;         /* "ATV" */
    const rx_addr sp = rx_sp();
    rx_set_sp(sp - 0x10);
    SU32(sp, -8, prefix);
    SU32(sp, -4, src);
    rx_format_buffer(dst, size, 0x00960ac0 /* "%sRider_%s" */, sp - 8);
    rx_set_sp(sp);
}
RX_FUNCTION(0x006ccfc0, rx_rider_stunt_name)

/* ---- the string -> value map lookups (0063b6e0 / 00850cd0): the std::map<string, T> get-or-insert of 006cd3f0, returning
 * node + 0x28. Frame: sub esp, 0x2c; ebx / ebp / esi; E = sp0 - 0x38. ---- */
#define MAP_GET_RET_LB  0x0063b6e4u
static rx_addr rider_map_get(rx_addr map, rx_addr key) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x2c, E = e0 - 0xc;
    const fx_saved sv = fx_save();
    fx_push_regs(e0, 2, (int[]){FX_EBX, FX_EBP});
    fx_set_reg(FX_EBP, key);
    fx_push_regs(e0 - 8, 1, (int[]){FX_ESI});
    rx_set_sp(E);
    rx_addr node;
    { const uint32_t a[1] = {key}; const rx_addr s = fx_push(1, a, MAP_GET_RET_LB); node = rx_world_string_map_lower_bound(map, key); fx_pop(s); }
    fx_set_reg(FX_EBX, node);
    if (map == 0) crt_invalid_parameter_noinfo();
    rx_addr it = U32(map, 0);
    fx_set_reg(FX_ESI, it);
    SU32(E, 0x14, U32(map, 0x18));
    if (it == 0) crt_invalid_parameter_noinfo();
    int insert = node == U32(E, 0x14);
    if (!insert) {
        const uint32_t a[2] = {key, node + 0xc};
        const rx_addr s = fx_push(2, a, 0x0063b6f4);
        insert = (crt_str_less(key, node + 0xc) & 0xff) != 0;
        fx_pop(s);
    }
    if (insert) {
        { const uint32_t a[1] = {key}; const rx_addr s = fx_push(1, a, 0x0063b706); crt_str_ctor_copy(E + 0x18, key); fx_pop(s); }
        SU32(E, 0x34, 0);
        { const uint32_t a[5] = {map, E + 0x10, E + 0x18, it, node};
          const rx_addr s = fx_push(5, a, 0x0063b720);
          const rx_addr r = rx_fx_map_insert_hint(map, E + 0x10, E + 0x18, it, node);
          it = U32(r, 0); node = U32(r, 4);
          fx_set_reg(FX_ESI, it); fx_set_reg(FX_EBX, node);
          fx_pop(s); }
        { const rx_addr s = fx_push(0, 0, 0x0063b72f); crt_str_dtor(E + 0x18); fx_pop(s); }
    }
    if (it == 0) crt_invalid_parameter_noinfo();
    else if (node == U32(U32(it, 0), 0x18)) crt_invalid_parameter_noinfo();
    fx_restore(sv);
    rx_set_sp(sp0);
    return node + 0x28;
}

/* 0063b6e0 (EAX = the key string, EDI = the map; RET 4): the rider clip map's u16 value address */
rx_addr rx_rider_stunt_map_u16(rx_addr map, rx_addr key) { return rider_map_get(map, key); }
RX_FUNCTION(0x0063b6e0, rx_rider_stunt_map_u16)
/* 00850cd0 (EAX = the key string, EDI = the map; RET 4): the animation map's dword value address */
rx_addr rx_rider_stunt_map_u32(rx_addr map, rx_addr key) { return rider_map_get(map, key); }
RX_FUNCTION(0x00850cd0, rx_rider_stunt_map_u32)

/* a KeyMap's vf9 (the virtual at +0x24: SetValue(slot, value)) at the call site's depth */
static void keymap_vf9(rx_addr km, uint32_t a, uint32_t b, rx_addr ret) {
    const rx_addr fn = U32(U32(km, 0), 0x24);
    void (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t, uint32_t));
    const uint32_t args[2] = {a, b};
    if (f) { const rx_addr s = fx_push(2, args, ret); f(km, a, b); fx_pop(s); }
    else rx_call_original(fn, RX_THISCALL, km, 2, args);
}

/* ---- 0063b2b0 (stdcall vector, id; RET 8): every 8-byte {id, enabled} entry with this id gets enabled = 0 ---- */
void rx_rider_stunt_listener_disable(rx_addr vec, uint32_t id) {
    const rx_addr proxy = U32(vec, 0);
    rx_addr e = U32(vec, 0xc);
    const rx_addr end = U32(vec, 0x10);
    if (end < e) crt_invalid_parameter_noinfo();
    while (e != end) {
        if (proxy == 0 || proxy != U32(vec, 0)) crt_invalid_parameter_noinfo();
        if (e == U32(vec, 0x10)) return;
        if (proxy == 0) crt_invalid_parameter_noinfo();
        else if (U32(U32(proxy, 0), 0x10) <= e) crt_invalid_parameter_noinfo();
        if (U32(e, 0) == id) { SU32(e, 4, 0); break; }
        if (proxy == 0) crt_invalid_parameter_noinfo();
        else if (U32(U32(proxy, 0), 0x10) <= e) crt_invalid_parameter_noinfo();
        e += 8;
    }
}
RX_FUNCTION(0x0063b2b0, rx_rider_stunt_listener_disable)

/* ---- 0063b1e0 (stdcall vector, id; RET 8): every 8-byte {id, enabled} entry gets enabled = (its id == this id) ---- */
void rx_rider_stunt_listener_select(rx_addr vec, uint32_t id) {
    const rx_addr proxy = U32(vec, 0);
    rx_addr e = U32(vec, 0xc);
    const rx_addr end = U32(vec, 0x10);
    if (end < e) crt_invalid_parameter_noinfo();
    while (e != end) {
        if (proxy == 0 || proxy != U32(vec, 0)) crt_invalid_parameter_noinfo();
        if (e == U32(vec, 0x10)) return;
        if (proxy == 0) crt_invalid_parameter_noinfo();
        else if (U32(U32(proxy, 0), 0x10) <= e) crt_invalid_parameter_noinfo();
        SU32(e, 4, U32(e, 0) == id);
        if (proxy == 0) crt_invalid_parameter_noinfo();
        else if (U32(U32(proxy, 0), 0x10) <= e) crt_invalid_parameter_noinfo();
        e += 8;
    }
}
RX_FUNCTION(0x0063b1e0, rx_rider_stunt_listener_select)

/* the rider animation's ranged clamp as its comiss sequences write it: t = [ra + hi] limited to <= 0 (a NaN becomes 0), then
 * raised to [ra + lo] (a NaN t stays): the trick start's +0x588 / +0x584 and the listener's +0x488 / +0x484 pairs. */
static float rider_range_clamp(rx_addr ra, uint32_t hi, uint32_t lo_off) {
    float t = F32(ra, hi);
    if (!(0.0f >= t)) t = 0.0f;
    const float lo = F32(ra, lo_off);
    if (lo >= t) t = lo;
    return t;
}

/* ---- 0054d3b0 (EAX = player; stdcall trick; RET 4): the trick's control suppression: SteeringDisabled (+0x98) clears the
 * steer / lean key maps, IgnoreBrake (+0x94) the brake map, IgnoreThrottle (+0x90) the throttle maps (the KeyMap vf9 with 0);
 * a replay (+0x11c8) clears the +0x11cc flag bits 0 / 1 / 2 instead. Frame: push ebx / esi / edi. ---- */
void rx_rider_stunt_controls(rx_addr P, rx_addr trick) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 3, (int[]){FX_EBX, FX_ESI, FX_EDI});
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_EDI, P);
    if (U32(trick, 0x98) != 0) {
        const uint32_t pad = U32(P, 0x1f0);
        fx_set_reg(FX_ESI, pad);
        if ((int32_t)pad >= 0) {
            if (U32(P, 0x11c8) != 0) SU32(P, 0x11cc, U32(P, 0x11cc) & ~1u);
            else {
                const rx_addr steer = rx_world_pad_steer_map(pad);
                fx_set_reg(FX_EAX, pad);
                keymap_vf9(steer, pad, 0, 0x0054d3ef);
                const rx_addr lean = rx_world_pad_lean_map(pad);
                fx_set_reg(FX_EAX, pad);
                keymap_vf9(lean, pad, 0, 0x0054d402);
            }
        }
    }
    if (U32(trick, 0x94) != 0) {
        const uint32_t pad = U32(P, 0x1f0);
        if ((int32_t)pad >= 0) {
            if (U32(P, 0x11c8) != 0) SU32(P, 0x11cc, U32(P, 0x11cc) & ~2u);
            else {
                const rx_addr km = U32(0, 0x009c52c4u + 4u * pad) + 0x348;
                fx_set_reg(FX_EAX, pad);
                keymap_vf9(km, pad, 0, 0x0054d442);
            }
        }
    }
    if (U32(trick, 0x90) != 0) {
        const uint32_t pad = U32(P, 0x1f0);
        if ((int32_t)pad >= 0) {
            if (U32(P, 0x11c8) != 0) SU32(P, 0x11cc, U32(P, 0x11cc) & ~4u);
            else {
                fx_set_reg(FX_ESI, pad);
                rx_player_slot_group2(pad, 0);
            }
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0054d3b0, rx_rider_stunt_controls)

/* ---- 00622790 (EDI = the rider animation; stdcall airborne, clip byte; RET 8): the ground trick's scripted clip. The pose
 * override ends (00621490), the bike simulation mode 2 -> 1, then the air (state 1) or ground (state 2) variant: the clip id
 * ([+0x580] / [+0x568] set to the map byte or [+0x5a0] / [+0x5ac]), the range clamp into the listener +0x598, the trick
 * table scan (0x28 records) for the clip id -> +0xc928 = its clip data (+0xd2a0, 0xe10 each; cleared when +0xe00 = 0) and the
 * record's name (+0x1c) copied to the frame, +0xaf28 = 1 / 2 and, for a clip with data not in mode 2, the bike's vf21 (+0x84);
 * the range into +0x58c; the clip length (007fa030 of the name) into +0xaf18: without a clip the listeners switch off and
 * +0xaf3c = -1; else state 3, the clamp into the +0x48c listener, the perf asset's end (+0x24) into +0xaf24 (and +0x484 =
 * 0 / +0x488 = it when positive), +0xaf30 / +0xaf38 = 0; then +0x5b524 = 1, +0x5b520 = 0, +0xaf3c = -1, the pose state
 * +0x5b594 / +0x5b590 = 3 with +0x5b640 = 1.5, three NaN-guarded copies and +0x5b740 = +0x5b6f0 = 0. Frame: push ebp; and
 * esp, -16; sub esp, 0x118 (/GS at +0x114); ebx / esi / edi (the pushed EDI is 00621490's argument). ---- */
void rx_rider_stunt_ground(rx_addr ra, uint32_t airborne, uint32_t clip) {
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4, A0 = ebp & ~15u, A = A0 - 0x118, E = A - 0xc;
    const fx_saved sv = fx_save();
    SU32(ebp, 0, sv.ebp);
    fx_set_reg(FX_EBP, ebp);
    SU32(A, 0x114, U32(0, 0x009b45e4) ^ A);
    fx_push_regs(A, 2, (int[]){FX_EBX, FX_ESI});
    fx_set_reg(FX_EBX, 0);
    fx_push_regs(A - 8, 1, (int[]){FX_EDI});
    rx_set_sp(E);
    fx_set_reg(FX_EDI, ra);
    rx_world_rider_end_pose_override(ra);                       /* 00621490 (its argument is the pushed EDI = ra) */
    {
        const rx_addr veh = U32(ra, 0xaf1c);
        const rx_addr sim = U32(veh, 0x234);
        if (U32(sim, 0x2de4) == 2) SU32(sim, 0x2de4, 1);
    }
    const uint32_t bl = clip & 0xff;
    uint32_t ecx_state = 2;
    if (airborne != 0) {
        const uint32_t cur = U32(ra, 0x580);
        const uint32_t w = cur ? (uint32_t)U16(cur, 0) : 0;
        if (w != U16(ra, 0x5a0)) {
            const rx_addr en = U32(ra, 0x57c);
            if (en) SU8(en, 0, 1);
            const rx_addr dst = U32(ra, 0x580);
            if (dst) SU16(dst, 0, U16(ra, 0x5a0));
            if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, rider_range_clamp(ra, 0x588, 0x584));   /* a float* target */
        }
        SU32(ra, 0xaf10, 1);
        if (U32(ra, 0x564)) SU8(U32(ra, 0x564), 0, 1);
        if (U32(ra, 0x568)) SU16(U32(ra, 0x568), 0, (uint16_t)bl);
        if ((int32_t)U32(ra, 0xba24) > 0) {
            uint32_t i = 0;
            rx_addr p = ra + 0xb01c;
            int found = 0;
            for (; i < U32(ra, 0xba24); i++, p += 0x28) {
                if ((uint32_t)U16(p, 0) == bl) { found = 1; break; }
            }
            if (found) {
                const rx_addr cd = i * 0xe10u + ra + 0xd2a0;
                SU32(ra, 0xc928, (uint32_t)cd);
                if (U32(cd, 0xe00) == 0) SU32(ra, 0xc928, 0);
                const rx_addr nm = ra + 0xaffc + i * 0x28;
                rx_addr d = E + 0x10;
                for (uint32_t k = 0;; k++) { const uint8_t c = U8(nm, k); SU8(d, k, c); if (c == 0) break; }
            }
        }
        SU32(ra, 0xaf28, 1);
        if (U32(ra, 0xc928) != 0 && U32(0, 0x00a942b4) != 2) {
            const rx_addr sim = U32(U32(ra, 0xaf1c), 0x234);
            const rx_addr fn = U32(U32(sim, 0), 0x84);
            void (*f)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
            if (f) f(sim); else rx_call_original(fn, RX_THISCALL, sim, 0, 0);
        }
        if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, rider_range_clamp(ra, 0x588, 0x584));   /* +0x58c is a float* */
    } else {
        const rx_addr cur = U32(ra, 0x568);
        const uint32_t w = cur ? (uint32_t)U16(cur, 0) : 0;
        if (w != U16(ra, 0x5ac)) {
            if (U32(ra, 0x564)) SU8(U32(ra, 0x564), 0, 1);
            if (U32(ra, 0x568)) SU16(U32(ra, 0x568), 0, U16(ra, 0x5ac));
            if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, rider_range_clamp(ra, 0x588, 0x584));   /* a float* target */
        }
        SU32(ra, 0xaf10, ecx_state);
        if (U32(ra, 0x57c)) SU8(U32(ra, 0x57c), 0, 1);
        if (U32(ra, 0x580)) SU16(U32(ra, 0x580), 0, (uint16_t)bl);
        if ((int32_t)U32(ra, 0xc450) > 0) {
            uint32_t i = 0;
            rx_addr p = ra + 0xba48;
            int found = 0;
            for (; i < U32(ra, 0xc450); i++, p += 0x28) {
                if ((uint32_t)U16(p, 0) == bl) { found = 1; break; }
            }
            if (found) {
                const rx_addr nm = ra + 0xba28 + i * 0x28;
                rx_addr d = E + 0x10;
                for (uint32_t k = 0;; k++) { const uint8_t c = U8(nm, k); SU8(d, k, c); if (c == 0) break; }
            }
        }
        SU32(ra, 0xaf28, 2);
        SU32(ra, 0xc928, 0);
        if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, rider_range_clamp(ra, 0x588, 0x584));
    }
    {
        rx_addr clip;
        {   const rx_addr keyp = E + 0xc;
            SU32(keyp, 0, (uint32_t)(E + 0x10));
            SU32(E, 8, 0);
            const uint32_t a[1] = {E + 8};
            const rx_addr s = fx_push(1, a, 0x00622a6e);
            rx_world_string_table_find(U32(ra, 0x144) + 0x248, keyp, E + 8);
            fx_pop(s);
            clip = U32(E, 8);
            (void)clip;
        }
        SU32(ra, 0xaf14, clip);                                  /* the clip record */
        const float clen = rx_bf(fx_x87_bits(U32(clip, 0x94)));   /* fld / fstp of the clip's length */
        SF32(ra, 0xaf20, clen);
        if (!(0.0f >= clen)) { SU32(ra, 0x464, 0); SF32(ra, 0x468, clen); }
        float x = F32(ra, 0x468);
        if (!(0.0f >= x)) x = 0.0f;
        const float lo = F32(ra, 0x464);
        x = (lo >= x) ? lo : x;
        SF32(E, 8, x);
        rx_world_float_listeners_set(ra + 0x46c, E + 8);
    }
    SU32(ra, 0xaf38, 0);
    SU32(ra, 0x5b524, 1);
    SU32(ra, 0x5b520, 0);
    SU16(ra, 0xaf3c, 0xffff);
    {
        const uint32_t old = U32(ra, 0x5b594);
        if (old - 1 == 0) { SU32(ra, 0x5b5e8, 0); SU32(ra, 0x5b5ec, 0); }
        SU32(ra, 0x5b594, U32(ra, 0x5b590));
        SF32(ra, 0x5b640, 1.5f /* 0x9322a4 */);
        SU32(ra, 0x5b590, 3);
    }
    {
        const float inf = 3.4028234663852886e+38f /* 0x977494 */;
        if (!(F32(ra, 0x5b700) == inf)) fx_x87_copy(ra + 0x5b704, ra + 0x5b700);
        if (!(F32(ra, 0x5b6fc) == inf)) fx_x87_copy(ra + 0x5b718, ra + 0x5b6fc);
        if (!(F32(ra, 0x5b6f8) == inf)) fx_x87_copy(ra + 0x5b72c, ra + 0x5b6f8);
    }
    SF32(ra, 0x5b740, 0.0f);
    SU32(ra, 0x5b6f0, 0);
    SU32(A, -4, 0x00622bb3);
    phys_security_check_cookie(U32(A, 0x114) ^ A);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00622790, rx_rider_stunt_ground)

/* ---- 006226c0 (EDI = the rider animation; stdcall ra, player, trick; RET 0xc): the ground trick's start. Only a rider in pose
 * 0xc that is not already tricking: the trick record (the tracker's current trick, player + 0xb74) +0x20 names the perf asset
 * ("%sRider_%s"), the record's +0x64 (airborne) picks the rider's clip map (+0xaf7c air / +0xaf9c ground), whose u16 value is
 * the clip slot 00622790 starts. Frame: push ebp; and esp, -8; sub esp, 0x134 (/GS at +0x130); ebx / esi / edi; the name
 * buffer at A + 0x24 (0x104), the std::string at A + 8. ---- */
void rx_rider_stunt_clip(rx_addr ra, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4, A0 = ebp & ~7u, A = A0 - 0x134, E = A - 0xc;
    const fx_saved sv = fx_save();
    SU32(ebp, 0, sv.ebp);
    fx_set_reg(FX_EBP, ebp);
    SU32(A, 0x130, U32(0, 0x009b45e4) ^ A);
    fx_push_regs(A, 2, (int[]){FX_EBX, FX_ESI});
    fx_set_reg(FX_EBX, ra);
    fx_push_regs(A - 8, 1, (int[]){FX_EDI});
    rx_set_sp(E);
    const rx_addr buf = A + 0x24;
    if (U32(ra, 0xaf00) != 0 || U32(ra, 0x5b57c) != 0xc) goto end;
    {
        const rx_addr ktrick = U32(player, 0xb74);
        rx_rider_stunt_name(buf, 0x104, U32(ra, 0x5bab8), ktrick + 0x20);
    }
    { const uint32_t a[1] = {buf}; const rx_addr s = fx_push(1, a, 0x00622721); crt_str_ctor_cstr(A + 8, buf); fx_pop(s); }
    {
        const rx_addr map = ra + (U32(trick, 0x64) != 0 ? 0xaf7c : 0xaf9c);
        const rx_addr v = rx_rider_stunt_map_u16(map, A + 8);
        const uint32_t byte = (uint32_t)U8(v, 0);
        rx_rider_stunt_ground(ra, U32(trick, 0x64), byte);
    }
    { const rx_addr s = fx_push(0, 0, 0x00622772); crt_str_dtor(A + 8); fx_pop(s); }
end:
    SU32(A, -4, 0x00622784);
    phys_security_check_cookie(U32(A, 0x130) ^ A);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006226c0, rx_rider_stunt_clip)

/* ---- 00622bc0 (ECX = the trick; stdcall ra, player; RET 8): the air trick's start. The perf asset's clip name
 * ("%sRider_%s" of the tracker's current trick) looks up the animation map (+0xafbc air / +0xafdc ground) for the animation id
 * (u32) and the rider's clip map (+0xaf7c / +0xaf9c) for the clip slot (u16, +0xaf3c): the +0x46c listeners keep that slot, the
 * +0x48c ones only it; the clip's data (+0xd2a0, the 0x28 record tables +0xb01c / +0xba48) is looked up for the air trick
 * (+0xc92c; +0xaf2c = 1) or +0xaf2c = 2 for the ground one; then the clip length (007fa030) into +0xaf18: without it the
 * listeners switch off and +0xaf3c = -1; else state 3, the range clamp (0x488 / 0x484) into the +0x48c listener, the perf
 * asset's end (+0x24) into +0xaf24 (+0x484 = 0 / +0x488 = it when positive) and +0xaf30 / +0xaf38 = 0. Frame: push ebp; and
 * esp, -8; sub esp, 0x13c (/GS at +0x134); ebx / esi / edi; the trick at A + 8, the std::string at A + 0x10, the name buffer at
 * A + 0x2c (0x104), the duration out / float scratch at A + 4. ---- */
void rx_rider_stunt_air(rx_addr ra, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4, A0 = ebp & ~7u, A = A0 - 0x13c, E = A - 0xc;
    const fx_saved sv = fx_save();
    SU32(ebp, 0, sv.ebp);
    fx_set_reg(FX_EBP, ebp);
    SU32(A, 0x134, U32(0, 0x009b45e4) ^ A);
    fx_push_regs(A, 2, (int[]){FX_EBX, FX_ESI});
    fx_set_reg(FX_EBX, ra);
    SU32(A, 8, trick);
    fx_push_regs(A - 8, 1, (int[]){FX_EDI});
    rx_set_sp(E);
    const rx_addr buf = A + 0x2c;
    {
        const rx_addr ktrick = U32(player, 0xb74);
        rx_rider_stunt_name(buf, 0x104, U32(ra, 0x5bab8), ktrick + 0x20);
    }
    { const uint32_t a[1] = {buf}; const rx_addr s = fx_push(1, a, 0x00622c2c); crt_str_ctor_cstr(A + 0x10, buf); fx_pop(s); }
    rx_addr esi;
    rx_addr map2;
    {
        const rx_addr t = U32(A, 8);
        if (U32(t, 0x64) != 0) {
            const rx_addr v = rx_rider_stunt_map_u32(ra + 0xafbc, A + 0x10);
            esi = U32(v, 0);
            map2 = ra + 0xaf7c;
        } else {
            const rx_addr v = rx_rider_stunt_map_u32(ra + 0xafdc, A + 0x10);
            esi = U32(v, 0);
            map2 = ra + 0xaf9c;
        }
    }
    {
        const rx_addr v = rx_rider_stunt_map_u16(map2, A + 0x10);
        SU16(ra, 0xaf3c, U16(v, 0));
    }
    rx_rider_stunt_listener_disable(ra + 0x46c, esi);
    rx_rider_stunt_listener_select(ra + 0x48c, esi);
    {
        const rx_addr t = U32(A, 8);
        if (U32(t, 0x64) != 0) {
            const uint32_t clipid = (uint32_t)(int16_t)U16(ra, 0xaf3c);
            if ((int32_t)U32(ra, 0xba24) > 0) {
                uint32_t i = 0;
                rx_addr p = ra + 0xb01c;
                int found = 0;
                for (; i < U32(ra, 0xba24); i++, p += 0x28) if ((uint32_t)U16(p, 0) == clipid) { found = 1; break; }
                if (found) {
                    const rx_addr cd = i * 0xe10u + ra + 0xd2a0;
                    SU32(ra, 0xc92c, (uint32_t)cd);
                    if (U32(cd, 0xe00) == 0) SU32(ra, 0xc92c, 0);
                }
            }
            SU32(ra, 0xaf2c, 1);
        } else {
            SU32(ra, 0xaf2c, 2);
        }
    }
    {
        SU32(A, 0xc, (uint32_t)buf);
        SU32(A, 4, 0);
        const uint32_t a[1] = {A + 4};
        const rx_addr s = fx_push(1, a, 0x00622d36);
        const uint32_t r = rx_world_string_table_find(U32(ra, 0x144) + 0x248, A + 0xc, A + 4);
        fx_pop(s);
        (void)r;
    }
    const uint32_t len = U32(A, 4);
    SU32(ra, 0xaf18, len);
    if (len == 0) {
        rx_rider_vec8_mark_all(ra + 0x46c);
        rx_rider_vec8_clear_all(ra + 0x48c);
        SU16(ra, 0xaf3c, 0xffff);
    } else {
        SU32(ra, 0xaf10, 3);
        { const float t = rider_range_clamp(ra, 0x488, 0x484); SF32(A, 4, t); }
        rx_world_float_listeners_set(ra + 0x48c, A + 4);
        const rx_addr h = rx_fx_trick_asset_handle(U32(A, 8), U32(ra, 0x5bab8));
        const rx_addr asset = U32(h, 0x44);
        SU32(h, 0x4c, 1);
        const float end = F32(asset, 0x24);
        SU32(h, 0x4c, 0);
        SF32(ra, 0xaf24, end);
        if (0.0f < F32(ra, 0xaf24)) { SF32(ra, 0x484, 0.0f); fx_x87_copy(ra + 0x488, ra + 0xaf24); }
        SU32(ra, 0xaf30, 0);
        SU32(ra, 0xaf38, 0);
    }
    { const rx_addr s = fx_push(0, 0, 0x00622e0e); crt_str_dtor(A + 0x10); fx_pop(s); }
    SU32(A, -4, 0x00622e24);
    phys_security_check_cookie(U32(A, 0x134) ^ A);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00622bc0, rx_rider_stunt_air)

/* ---- 0054d120 (ECX = the player; stdcall player, trick; RET 8): the player's "StuntBegin" listener: only its own player with a
 * vehicle and a rider animation; the rider's state +0xaf10 picks the ground (0: 006226c0) or air (else: 00622bc0) start; the
 * control suppression (0054d3b0); and, with the player's network record +0xb2c, the online "UpdateStuntEvent" packet (006f34d0
 * with 1, the trick's +0x74, 0). Frame: push esi; push edi. ---- */
void rx_rider_stunt_begin_evt(rx_addr P, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    fx_set_reg(FX_ESI, P);
    SU32(sp0 - 8, 0, fx_reg(FX_EDI));
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_EDI, trick);
    if (P != player) goto end;
    {
        const rx_addr veh = U32(P, 0x1ac);
        if (veh == 0) goto end;
        const rx_addr ra = U32(veh, 0x1a0);
        if (ra == 0) goto end;
        if (U32(ra, 0xaf10) == 0) {
            const uint32_t a[3] = {ra, player, trick};
            const rx_addr s = fx_push(3, a, 0x0054d155);
            rx_rider_stunt_clip(ra, player, trick);
            fx_pop(s);
        } else {
            const uint32_t a[2] = {ra, player};
            const rx_addr s = fx_push(2, a, 0x0054d160);
            rx_rider_stunt_air(ra, player, trick);
            fx_pop(s);
        }
        { const uint32_t a[1] = {trick}; const rx_addr s = fx_push(1, a, 0x0054d168);
          rx_rider_stunt_controls(P, trick); fx_pop(s); }
        if (U32(P, 0xb2c) != 0) {
            if (rx_net_ref_valid(P + 0xb2c)) stunt_event_send(P + 0xb2c, 1, U32(trick, 0x74), 0);
        }
    }
end:
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0054d120, rx_rider_stunt_begin_evt)

/* ---- 0054d220 (ECX = the player; stdcall player, trick; RET 8): the player's "StuntCompleted" listener: only its own player
 * with a vehicle and a rider animation; the rider's trick state (+0xaf10) ends (00622e30 EndCrash); the input groups come back
 * (0054d480, the first three); a type-7 activity adds the trick's +0x6c to the player's +0x11b8; and, with the player's network
 * record +0xb2c, the online "UpdateStuntEvent" packet (006f34d0 with 2, the trick's +0x74, 0) when the record is usable
 * (00884950). Frame: push esi; push edi. ---- */
void rx_rider_stunt_completed_evt(rx_addr P, rx_addr player, rx_addr trick) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    fx_set_reg(FX_ESI, P);
    SU32(sp0 - 8, 0, fx_reg(FX_EDI));
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_EDI, trick);
    if (P != player) goto end;
    {
        const rx_addr veh = U32(P, 0x1ac);
        if (veh == 0) goto end;
        const rx_addr ra = U32(veh, 0x1a0);
        if (ra == 0) goto end;
        if (U32(ra, 0xaf10) != 0) {
            const uint32_t a[1] = {ra};
            const rx_addr s = fx_push(1, a, 0x0054d248);
            rx_world_rider_end_crash(ra);
            fx_pop(s);
        }
        { const rx_addr s = fx_push(0, 0, 0x0054d254);
          rx_player_restore_input_groups(P);
          fx_pop(s); }
        {
            const rx_addr act = U32(P, 0x94);
            if (act != 0 && U32(act, 0x238) == 7) SU32(P, 0x11b8, U32(P, 0x11b8) + U32(trick, 0x6c));
        }
        if (U32(P, 0xb2c) != 0) {
            uint32_t ok;
            { const rx_addr s = fx_push(0, 0, 0x0054d283);
              ok = rx_net_ref_valid(U32(P, 0xb2c)) & 0xff;
              fx_pop(s); }
            if (ok) stunt_event_send(U32(P, 0xb2c), 2, U32(trick, 0x74), 0);
        }
    }
end:
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0054d220, rx_rider_stunt_completed_evt)

/* ---- 0063c190 (stack: out*, value*, proxy, begin, proxy2, end; RET 0x18): every target record of the listener vector
 * (0x14 bytes: {lo, hi, tau, ?, current} + the listeners at +0x14) steps its current toward *value: m = min(tau, hi) (the
 * first compare's t > hi), cur = (m / tau) * (*value - cur) + cur, x = clamp(cur, lo, hi), and the normalized (x - lo) /
 * (hi - lo) goes to the record's listeners (0063b830); *out = value. With begin == end only *out = value. ---- */
void rx_rider_listeners_broadcast(rx_addr out, rx_addr value, rx_addr proxy, rx_addr begin, rx_addr proxy2, rx_addr end) {
    (void)proxy; (void)proxy2;
    if (begin > end) crt_invalid_parameter_noinfo();
    if (begin == end) { SU32(out, 0, value); return; }
    const float v = F32(value, 0);
    for (rx_addr p = begin; p != end; p += 4) {
        const rx_addr f = U32(p, 0);
        const float tau = F32(f, 8), rh = F32(f, 0xc);
        const float m = (tau > rh) ? rh : tau;
        const float cur = rx_adds(rx_muls(rx_divs(m, tau), rx_subs(v, F32(f, 0x10))), F32(f, 0x10));
        SF32(f, 0x10, cur);
        const float hi = F32(f, 4), lo = F32(f, 0);
        float x = cur;
        if (x >= hi) x = hi;                              /* comiss x, hi: jb keeps x */
        if (lo >= x) x = lo;
        const rx_addr E = rx_sp();                        /* the local float reuses the argument slot ([esp + 4]) */
        SF32(E, 4, rx_divs(rx_subs(x, lo), rx_subs(hi, lo)));
        { const uint32_t a[2] = {f + 0x14, E + 4};
          const rx_addr s = fx_push(2, a, 0x0063c222);
          rx_world_float_listeners_set(f + 0x14, E + 4);
          fx_pop(s); }
    }
    SU32(out, 0, value);
}
RX_FUNCTION(0x0063c190, rx_rider_listeners_broadcast)

/* ---- 0063b7d0 (EAX = the listener vector; stdcall value*; RET 4): the vector's target records broadcast (0063c190); the out
 * slot is the value argument slot. Frame: push ebx / ebp / esi / edi. ---- */
void rx_rider_listeners_broadcast_vec(rx_addr vec, rx_addr value) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    fx_push_regs(sp0, 3, (int[]){FX_EBX, FX_EBP, FX_ESI});
    fx_push_regs(sp0 - 0xc, 1, (int[]){FX_EDI});
    rx_set_sp(sp0 - 0x10);
    fx_set_reg(FX_ESI, vec);
    const rx_addr begin = U32(vec, 0xc), end = U32(vec, 0x10);
    if (((end - begin) & 0xfffffffc) != 0) {
        if (begin > end) crt_invalid_parameter_noinfo();
        const rx_addr proxy = U32(vec, 0);
        if (begin > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
        fx_set_reg(FX_EBX, begin);
        fx_set_reg(FX_EBP, proxy);
        fx_set_reg(FX_EDI, end);
        { const uint32_t a[6] = {sp0 + 4, value, proxy, begin, proxy, end};
          const rx_addr s = fx_push(6, a, 0x0063b817);
          rx_rider_listeners_broadcast(sp0 + 4, value, proxy, begin, proxy, end);
          fx_pop(s); }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0063b7d0, rx_rider_listeners_broadcast_vec)

/* ---- 0063b4b0 (EAX = the clamped float {min, max, float* target}; stdcall value*; RET 4): *target = the value plus the
 * target's current (0 without a target), clamped to [min, max] (the max compare first, then the min; NaN keeps the running
 * value) ---- */
void rx_rider_clamped_float_add(rx_addr obj, rx_addr value) {
    const rx_addr tgt = U32(obj, 8);
    const float cur = tgt ? F32(tgt, 0) : 0.0f;
    const float hi = F32(obj, 4), lo = F32(obj, 0);
    float x = rx_adds(F32(value, 0), cur);
    if (x >= hi) x = hi;                                  /* comiss sum, max: jb keeps sum */
    if (lo >= x) x = lo;
    if (tgt) SF32(tgt, 0, x);
}
RX_FUNCTION(0x0063b4b0, rx_rider_clamped_float_add)
