#include "waypoint_gate.h"
/* fx_entity.c -- the placed world entities of the 0x40xxxx / 0x41xxxx range that the race frames tick: MXTrigger (vf19
 * 0040a650: which players' vehicles / which physics objects are inside the trigger volume, enter / leave events), the
 * TimedRaceSection (00413a80), the volume test 00403c10 / 00403cf0, the trigger's occupant list helpers and its event post
 * 00419820; the overlay colour and scrolling texture ticks; small leaves. notes/fx.md.
 *
 * Trigger T: +0x90 occupant list {entry*: {object, seen}, count +0x94, capacity +0x98, growth +0x9c}, +0xa4 physics object list
 * {object*, count +0xa8, capacity +0xac, growth +0xb0}, +0xb8 shape (0 box, 1 sphere, 2 cylinder: unit volume in local space),
 * +0xbc what it watches (0 players, 1 the physics objects 0xa95d74 / count 0xa95d78), +0x20c tick period (frames), +0x210
 * active, +0x218 hit this frame, +0x240 world-to-local matrix. */
#include "fx.h"
#include "fx_util.h"
#include "world_veh.h"
#include "world_rider.h"
#include "loop_cam_entity.h"
#include "loop_cam_parts_view.h"
#include "fx_overlay.h"
#include "build_rider.h"

/* ---- 00403cf0 (EAX = trigger, ESI = local point*): inside the unit volume of the shape (+0xb8): box |x|, |y|, |z| < 1;
 * sphere x^2 + y^2 + z^2 < 1; cylinder x^2 + z^2 < 1 and |y| < 1. EAX: 1 / 0 (another shape: (shape - 2) with AL = 0). */
uint32_t rx_fx_trigger_inside(rx_addr T, rx_addr p) {
    SU32(rx_sp(), -4, fx_reg(FX_ECX));                         /* push ecx */
    const uint32_t shape = U32(T, 0xb8);
    const float one = 1.0f /* 0x931f24 */;
    const rx_addr sp = rx_sp() - 4;
    if (shape == 0) {
        SU32(sp, -4, 0x00403d7e);
        if (!(one > fabsf(F32(p, 0)))) return 0;
        SU32(sp, -4, 0x00403d95);
        if (!(one > fabsf(F32(p, 4)))) return 0;
        SU32(sp, -4, 0x00403dac);
        return one > fabsf(F32(p, 8)) ? 1 : 0;
    }
    if (shape == 1) {
        const float y = F32(p, 4), z = F32(p, 8), x = F32(p, 0);
        return one > rx_adds(rx_adds(rx_muls(y, y), rx_muls(z, z)), rx_muls(x, x)) ? 1 : 0;
    }
    if (shape == 2) {
        const float x = F32(p, 0), z = F32(p, 8);
        if (!(one > rx_adds(rx_muls(x, x), rx_muls(z, z)))) return 0;
        SU32(sp, -4, 0x00403dac);
        return one > fabsf(F32(p, 4)) ? 1 : 0;
    }
    return (shape - 2) & 0xffffff00u;
}
RX_FUNCTION(0x00403cf0, rx_fx_trigger_inside)

/* ---- 00403c10 (EAX = trigger, ECX = world point*): the point through the trigger's world-to-local matrix (+0x240, rows at
 * +0x240 / +0x250 / +0x260, translation +0x270), then 00403cf0. Frame: push ebp; and esp, -16; sub esp, 0x1c; esi: E = A - 0x20:
 * +0x10 the local point. */
uint32_t rx_fx_trigger_contains(rx_addr T, rx_addr P) {
    fx_frame f; fx_frame_enter(&f, 0x1c, 16, FX_ESI);
    const rx_addr E = f.E;
    const float y = F32(P, 4), z = F32(P, 8), x = F32(P, 0);
    const float r0 = rx_adds(rx_adds(rx_adds(rx_muls(F32(T, 0x250), y), rx_muls(F32(T, 0x260), z)), rx_muls(F32(T, 0x240), x)), F32(T, 0x270));
    const float r1 = rx_adds(rx_adds(rx_adds(rx_muls(F32(T, 0x244), x), rx_muls(F32(T, 0x254), y)), rx_muls(F32(T, 0x264), z)), F32(T, 0x274));
    const float r2 = rx_adds(rx_adds(rx_adds(rx_muls(F32(T, 0x248), x), rx_muls(F32(T, 0x258), y)), rx_muls(F32(T, 0x268), z)), F32(T, 0x278));
    SF32(E, 0x10, r0); SF32(E, 0x14, r1); SF32(E, 0x18, r2);
    fx_set_reg(FX_ESI, E + 0x10);
    fx_set_reg(FX_ECX, P);
    const rx_addr s = fx_push(0, 0, 0x00403cdf);
    const uint32_t r = rx_fx_trigger_inside(T, E + 0x10);
    fx_pop(s);
    fx_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x00403c10, rx_fx_trigger_contains)

/* ---- 0040a7f0 (EDI = trigger; stdcall object; RET 4): the occupant entry of the object, or 0. Frame: push ebx; push esi. */
rx_addr rx_fx_trigger_find(rx_addr obj, rx_addr T) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_EBX)); SU32(sp0 - 8, 0, fx_reg(FX_ESI));
    const int32_t n = I32(T, 0x94);
    rx_addr found = 0;
    for (int32_t i = 0; i < n && found == 0; i++) {
        const rx_addr e = U32(T, 0x90) + 8u * (uint32_t)i;
        if (U32(e, 0) == obj) found = e;
    }
    return found;
}
RX_FUNCTION(0x0040a7f0, rx_fx_trigger_find)

/* ---- 0040a830 (EAX = trigger; stdcall object; RET 4): append {object, seen 1} to the occupants (growing the vector by its
 * growth step when full: 0041a4e0, not in the core). Frame: push esi; push edi. */
void rx_fx_trigger_add(rx_addr obj, rx_addr T) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    const rx_addr v = T + 0x90;
    SU32(sp0 - 8, 0, fx_reg(FX_EDI));
    if (I32(v, 4) >= I32(T, 0x98)) {
        const uint32_t capacity=U32(v,0xc)+U32(T,0x98);
        rx_set_sp(sp0-8);
        const uint32_t args[1]={capacity};rx_addr pushed=fx_push(1,args,0x0040a855);
        rx_waypoint_trigger_grow(v,capacity);fx_pop(pushed);
        rx_set_sp(sp0);
    }
    const uint32_t n = U32(v, 4);
    SU32(U32(v, 0) + 8 * n, 0, obj);
    SU32(U32(v, 0) + 8 * n, 4, 1);
    SU32(v, 4, U32(v, 4) + 1);
}
RX_FUNCTION(0x0040a830, rx_fx_trigger_add)

/* ---- 0040a870 (ECX = trigger): every occupant's seen flag cleared */
void rx_fx_trigger_unsee(rx_addr T) {
    if (I32(T, 0x94) > 0) SU32(rx_sp(), -4, fx_reg(FX_ESI));
    for (int32_t i = 0; i < I32(T, 0x94); i++) SU32(U32(T, 0x90) + 8u * (uint32_t)i, 4, 0);
}
RX_FUNCTION(0x0040a870, rx_fx_trigger_unsee)

/* The trigger event posts 00419820 / 00419880 / 00419a10 are one template: a stdcall (object) with ECX = the trigger and the event name in EDX
 * (00419a10: the name at 0x009c617c): the payload {object, trigger} (8 bytes) goes to an event system global (004020c0: EAX = the system,
 * EDI = the name, stack payload, 8) after the handler signature's type is named (type_info::_name_internal_method of `type`, into 0x9e1a7c). Frame:
 * sub esp, 0x10 (/GS cookie at +0xc); esi; edi: the payload at sp0 - 0x10 + 4. `ret1..3` are the return addresses of its three calls. */
static void trigger_post_common(uint32_t system_global, rx_addr type, rx_addr object, rx_addr T, rx_addr name, rx_addr ret1, rx_addr ret2, rx_addr ret3) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x10;
    SU32(e0, 0xc, U32(0, 0x009b45e4) ^ e0);
    const rx_addr evs = U32(0, system_global);
    SU32(e0, -4, fx_reg(FX_ESI)); SU32(e0, -8, fx_reg(FX_EDI));
    const fx_saved sv = fx_save();
    fx_set_reg(FX_ESI, evs); fx_set_reg(FX_EDI, name);
    SU32(e0, 8, T);
    SU32(e0, 4, object);
    {   /* push 0x9e1a7c; ECX = the type; call [0x916388] (thiscall, RET 4) */
        rx_set_sp(e0 - 8);
        fx_type_name(type, ret1);
    }
    {
        const uint32_t a[2] = {e0 + 4, 8};
        const rx_addr s = fx_push(2, a, ret2);
        rx_world_event_dispatch(evs, name, e0 + 4, 8);
        fx_pop(s);
    }
    fx_restore(sv);
    rx_set_sp(e0);
    SU32(e0, -4, ret3);
    phys_security_check_cookie(U32(e0, 0xc) ^ e0);
    rx_set_sp(sp0);
}

/* ---- 00419820 (ECX = trigger, EDX = event name; stdcall vehicle; RET 4): the event {vehicle, trigger} to the event system [0xa89bc8] (signature
 * void(RavenVehicle*, const MXTrigger*), type 0x9cbb7c). */
void rx_fx_trigger_post(rx_addr veh, rx_addr T, rx_addr name) {
    trigger_post_common(0x00a89bc8, 0x009cbb7c, veh, T, name, 0x00419854, 0x00419862, 0x0041986f);
}
RX_FUNCTION(0x00419820, rx_fx_trigger_post)

/* ---- 00419880 (ECX = trigger, EDX = event name; stdcall physics object; RET 4): the event {object, trigger} to the event system [0xa89bcc]
 * (signature void(PhysicsEntity*, const MXTrigger*), type 0x9cbbac). */
void rx_fx_trigger_post_physics(rx_addr obj, rx_addr T, rx_addr name) {
    trigger_post_common(0x00a89bcc, 0x009cbbac, obj, T, name, 0x004198b4, 0x004198c2, 0x004198cf);
}
RX_FUNCTION(0x00419880, rx_fx_trigger_post_physics)

/* ---- 00419a10 (ECX = trigger; stdcall vehicle; RET 4): the "vehicle announced" event {vehicle, trigger} (signature void(RavenVehicle*,
 * const TimedRaceSection*), type 0x9cbc38) to [0xa89bc8], under the event name held at 0x9c617c. */
void rx_fx_section_post(rx_addr veh, rx_addr T) {
    trigger_post_common(0x00a89bc8, 0x009cbc38, veh, T, U32(0, 0x009c617c), 0x00419a48, 0x00419a56, 0x00419a63);
}
RX_FUNCTION(0x00419a10, rx_fx_section_post)

/* a post from a trigger function: ECX = trigger, EDX = name, the vehicle pushed; the call site's return address */
static void post_call(rx_addr T, rx_addr name, rx_addr veh, rx_addr ret) {
    const uint32_t a[1] = {veh};
    const rx_addr s = fx_push(1, a, ret);
    rx_fx_trigger_post(veh, T, name);
    fx_pop(s);
}

/* ---- 0040a8a0 (EAX = player, ECX = trigger): the player is inside: a known occupant posts "stay" (0x93609c) and is marked
 * seen; a new one is added and posts "enter" (0x935e84). Frame: push ebx / esi / edi. */
void rx_fx_trigger_player_inside(rx_addr pl, rx_addr T) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.ebx); SU32(sp0 - 8, 0, sv.esi); SU32(sp0 - 0xc, 0, sv.edi);
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_ESI, pl); fx_set_reg(FX_EDI, T);
    rx_addr e;
    { const uint32_t a[1] = {pl}; const rx_addr s = fx_push(1, a, 0x0040a8ad); e = rx_fx_trigger_find(pl, T); fx_pop(s); }
    fx_set_reg(FX_EBX, e);
    if (e != 0) {
        const rx_addr veh = U32(U32(e, 0), 0x1ac);
        if (veh != 0) post_call(T, 0x0093609c, veh, 0x0040a8cc);
        SU32(e, 4, 1);
    } else {
        { const uint32_t a[1] = {pl}; const rx_addr s = fx_push(1, a, 0x0040a8df); rx_fx_trigger_add(pl, T); fx_pop(s); }
        const rx_addr veh = U32(pl, 0x1ac);
        fx_set_reg(FX_ESI, veh);
        if (veh != 0) post_call(T, 0x00935e84, veh, 0x0040a8f6);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0040a8a0, rx_fx_trigger_player_inside)

/* ---- 0040a980 (ESI = trigger): occupants not seen this tick leave (swap-removed; "leave" 0x935e94 posted with their
 * vehicle). Frame: push ecx / ebp / edi. */
void rx_fx_trigger_prune(rx_addr T) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX)); SU32(sp0 - 8, 0, sv.ebp); SU32(sp0 - 0xc, 0, sv.edi);
    rx_set_sp(sp0 - 0xc);
    for (int32_t i = 0; i < I32(T, 0x94); i++) {
        fx_set_reg(FX_EDI, (uint32_t)i);
        const rx_addr arr = U32(T, 0x90);
        const rx_addr obj = U32(arr + 8u * (uint32_t)i, 0);
        if (U32(arr + 8u * (uint32_t)i, 4) != 0) continue;
        const uint32_t n = U32(T, 0x94);
        const uint32_t last_obj = U32(arr + 8 * n, -8);
        fx_set_reg(FX_EBP, last_obj);
        SU32(arr + 8u * (uint32_t)i, 0, last_obj);
        SU32(arr + 8u * (uint32_t)i, 4, U32(arr + 8 * n, -4));
        SU32(T, 0x94, U32(T, 0x94) - 1);
        const rx_addr veh = U32(obj, 0x1ac);
        if (veh != 0) post_call(T, 0x00935e94, veh, 0x0040a9d2);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0040a980, rx_fx_trigger_prune)

/* the player list query 00552b90 (EDX = players = game +0x7460; stdcall index, kind 3; RET 8) at the current ESP */
static rx_addr player_at(uint32_t i, rx_addr ret) {
    const uint32_t a[2] = {i, 3};
    const rx_addr s = fx_push(2, a, ret);
    const rx_addr p = rx_world_players_find(U32(0, FX_G_GAME) + 0x7460, i, 3);
    fx_pop(s);
    return p;
}
/* a thiscall virtual with one argument (the position query vf17 +0x44 and the like) */
static uint32_t vcall1(rx_addr obj, uint32_t off, uint32_t a0, rx_addr ret) {
    const rx_addr fn = U32(U32(obj, 0), off);
    uint32_t (*f)(rx_addr, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t));
    const uint32_t a[1] = {a0};
    if (!f) return rx_call_original(fn, RX_THISCALL, obj, 1, a);
    const rx_addr s = fx_push(1, a, ret);
    const uint32_t r = f(obj, a0);
    fx_pop(s);
    return r;
}
static uint32_t vcall0(rx_addr obj, uint32_t off, rx_addr ret) {
    const rx_addr fn = U32(U32(obj, 0), off);
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
    if (!f) return rx_call_original(fn, RX_THISCALL, obj, 0, 0);
    const rx_addr s = fx_push(0, 0, ret);
    const uint32_t r = f(obj);
    fx_pop(s);
    return r;
}
static uint32_t contains_call(rx_addr T, rx_addr P, rx_addr ret) {
    const rx_addr s = fx_push(0, 0, ret);
    const uint32_t r = rx_fx_trigger_contains(T, P);
    fx_pop(s);
    return r & 0xff;
}

/* ---- 0040a600 (ESI = trigger; plain ret): between the trigger's ticks the occupants that have a vehicle (+0x1ac) and are alive (+0x94) re-post
 * "stay" (0x93609c: 00419820, ECX = the trigger, EDX = the name, stack the vehicle). Frame: push edi. */
void rx_fx_trigger_stay(rx_addr T) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.edi);
    fx_set_reg(FX_EDI, 0);
    for (int32_t i = 0; i < I32(T, 0x94); i++) {
        fx_set_reg(FX_EDI, (uint32_t)i);
        const rx_addr obj = U32(U32(T, 0x90) + 8u * (uint32_t)i, 0);
        const rx_addr veh = U32(obj, 0x1ac);
        if (veh != 0 && U32(obj, 0x94) != 0) {
            rx_set_sp(sp0 - 4);
            fx_set_reg(FX_ECX, T);
            post_call(T, 0x0093609c, veh, 0x0040a63b);
            rx_set_sp(sp0);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0040a600, rx_fx_trigger_stay)

/* ---- 0040a900 (EDI = trigger; stdcall physics object; RET 4): a watched physics object that is inside: one already in the trigger's list (+0xa4:
 * {objects, count +0xa8, capacity +0xac, growth +0xb0}) posts "stay" (0x9360b4: 00419880), a new one is appended (the list grown by its growth
 * step when full: 007cff20, ESI = the list, stack the new capacity) and posts "enter" (0x936084). Frame: push ebx; push esi. */
void rx_fx_trigger_watch_object(rx_addr obj, rx_addr T) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx);
    SU32(sp0, -8, sv.esi);
    rx_set_sp(sp0 - 8);
    fx_set_reg(FX_EBX, obj);
    const rx_addr list = T + 0xa4;
    for (int32_t i = 0; i < I32(T, 0xa8); i++) {
        if (U32(U32(T, 0xa4), 4u * (uint32_t)i) == obj) {
            const uint32_t a[1] = {obj};
            const rx_addr s = fx_push(1, a, 0x0040a96d);
            fx_set_reg(FX_ECX, T);
            rx_fx_trigger_post_physics(obj, T, 0x009360b4);
            fx_pop(s);
            fx_restore(sv);
            rx_set_sp(sp0);
            return;
        }
    }
    fx_set_reg(FX_ESI, list);
    if (I32(T, 0xa8) >= I32(T, 0xac)) {
        const uint32_t cap = U32(list, 0xc) + U32(T, 0xac);
        const uint32_t a[1] = {cap};
        const rx_addr s = fx_push(1, a, 0x0040a943);
        rx_iarr_reserve(list, cap);
        fx_pop(s);
    }
    SU32(U32(list, 0), 4u * U32(list, 4), obj);
    SU32(list, 4, U32(list, 4) + 1);
    {
        const uint32_t a[1] = {obj};
        const rx_addr s = fx_push(1, a, 0x0040a95b);
        fx_set_reg(FX_ECX, T);
        rx_fx_trigger_post_physics(obj, T, 0x00936084);
        fx_pop(s);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0040a900, rx_fx_trigger_watch_object)

/* ---- 0040a650 MXTrigger::vf19 (thiscall trigger, dt; RET 4): active triggers tick every +0x20c frames (between: 0040a600
 * re-posts "stay" for the occupants, not in the core); the seen flags cleared; watching players: each player with a vehicle
 * whose position lies inside is marked (+0x218) and handled (0040a8a0); watching physics objects: inside objects are added
 * (0040a900, not in the core), the others dropped from +0xa4; then the unseen occupants leave (0040a980). Frame: push ebp;
 * and esp, -16; sub esp, 0x24; ebx / esi / edi: E = A - 0x30: +0x18 players, +0x1c player count, +0x20 the position. */
void rx_fx_trigger_tick(rx_addr T, float dt) {
    (void)dt;
    fx_frame f; fx_frame_enter(&f, 0x24, 16, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    fx_set_reg(FX_ESI, T);
    if (U8(T, 0x210) == 0) { fx_frame_leave(&f); return; }
    const uint32_t frame = U32(fx_timing(), 0x58);
    if (frame % U32(T, 0x20c) != 0) {
        {   /* call 0040a600 (ESI = the trigger) */
            const rx_addr s = fx_push(0, 0, 0x0040a687);
            rx_fx_trigger_stay(T);
            fx_pop(s);
        }
        fx_frame_leave(&f);
        return;
    }
    fx_set_reg(FX_ECX, T);
    { const rx_addr s = fx_push(0, 0, 0x0040a697); rx_fx_trigger_unsee(T); fx_pop(s); }
    const uint32_t kind = U32(T, 0xbc);
    rx_addr prune_ret = 0x0040a7e2;
    if (kind == 0) {
        const rx_addr players = U32(0, FX_G_GAME) + 0x7460;
        const uint32_t n = U8(U32(0, FX_G_GAME), 0x1541c);
        fx_set_reg(FX_EBX, 0);
        SU32(E, 0x18, players); SU32(E, 0x1c, n);
        for (uint32_t i = 0; (int32_t)i < (int32_t)n; i++) {
            fx_set_reg(FX_EBX, i);
            const rx_addr pl = player_at(i, 0x0040a791);
            fx_set_reg(FX_EDI, pl);
            const rx_addr veh = U32(pl, 0x1ac);
            if (veh != 0 && U32(pl, 0x94) != 0) {
                vcall1(veh, 0x44, E + 0x20, 0x0040a7b7);
                if (contains_call(T, E + 0x20, 0x0040a7c2)) {
                    SU8(T, 0x218, 1);
                    const rx_addr s = fx_push(0, 0, 0x0040a7d6);
                    rx_fx_trigger_player_inside(pl, T);
                    fx_pop(s);
                }
            }
        }
    } else if (kind == 1) {
        fx_set_reg(FX_EBX, 0);
        for (int32_t i = 0; i < I32(0, 0x00a95d78); i++) {
            fx_set_reg(FX_EBX, (uint32_t)i);
            const rx_addr obj = U32(U32(0, 0x00a95d74) + 4u * (uint32_t)i, 0);
            fx_set_reg(FX_EDI, obj);
            const rx_addr m = vcall0(obj, 0x38, 0x0040a6d2);
            SF32(E, 0x20, F32(m, 0x30)); SF32(E, 0x24, F32(m, 0x34)); SF32(E, 0x28, F32(m, 0x38));
            if (contains_call(T, E + 0x20, 0x0040a6fe)) {
                SU8(T, 0x218, 1);
                fx_set_reg(FX_EDI, T);
                const uint32_t a[1] = {obj};
                const rx_addr s = fx_push(1, a, 0x0040a711);
                rx_fx_trigger_watch_object(obj, T);
                fx_pop(s);
            } else {
                for (int32_t k = 0; k < I32(T, 0xa8); k++) {
                    const rx_addr list = U32(T, 0xa4);
                    if (U32(list + 4u * (uint32_t)k, 0) == obj) {
                        SU32(list + 4u * (uint32_t)k, 0, U32(list + 4u * U32(T, 0xa8), -4));
                        SU32(T, 0xa8, U32(T, 0xa8) - 1);
                    }
                }
            }
        }
        prune_ret = 0x0040a759;
    }
    fx_set_reg(FX_ESI, T);
    { const rx_addr s = fx_push(0, 0, prune_ret); rx_fx_trigger_prune(T); fx_pop(s); }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x0040a650, rx_fx_trigger_tick)

/* ---- 00413b40 (EAX = player, ECX = section): inside: a known occupant is marked seen, a new one added with its vehicle
 * announced (00419a10, not in the core). Frame: push ecx / esi / edi. */
void rx_fx_section_player_inside(rx_addr pl, rx_addr T) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, fx_reg(FX_ECX)); SU32(sp0 - 8, 0, sv.esi); SU32(sp0 - 0xc, 0, sv.edi);
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_ESI, pl); fx_set_reg(FX_EDI, T);
    rx_addr e;
    { const uint32_t a[1] = {pl}; const rx_addr s = fx_push(1, a, 0x00413b4d); e = rx_fx_trigger_find(pl, T); fx_pop(s); }
    if (e != 0) SU32(e, 4, 1);
    else {
        { const uint32_t a[1] = {pl}; const rx_addr s = fx_push(1, a, 0x00413b64); rx_fx_trigger_add(pl, T); fx_pop(s); }
        const rx_addr veh = U32(pl, 0x1ac);
        fx_set_reg(FX_ESI, veh);
        if (veh != 0) {
            fx_set_reg(FX_ECX, T);
            const uint32_t a[1] = {veh};
            const rx_addr s = fx_push(1, a, 0x00413b76);
            rx_fx_section_post(veh, T);
            fx_pop(s);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00413b40, rx_fx_section_player_inside)

/* ---- 00413a80 TimedRaceSection::vf19 (thiscall section, dt; RET 4): unless the clip recorder replays ([0xa942b4] = 2): the
 * seen flags cleared; every player whose vehicle (not wrecked: +0x280) is inside is handled (00413b40); the unseen leave.
 * Frame: push ebp; and esp, -16; sub esp, 0x24; ebx / esi / edi: E = A - 0x30 (as 0040a650). */
void rx_fx_timed_section_tick(rx_addr T, float dt) {
    (void)dt;
    fx_frame f; fx_frame_enter(&f, 0x24, 16, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    fx_set_reg(FX_ESI, T);
    if (U32(0, 0x00a942b4) == 2) { fx_frame_leave(&f); return; }
    fx_set_reg(FX_EBX, 0);
    for (int32_t i = 0; i < I32(T, 0x94); i++) SU32(U32(T, 0x90) + 8u * (uint32_t)i, 4, 0);
    const rx_addr players = U32(0, FX_G_GAME) + 0x7460;
    const uint32_t n = U8(U32(0, FX_G_GAME), 0x1541c);
    SU32(E, 0x18, players); SU32(E, 0x1c, n);
    for (uint32_t i = 0; (int32_t)i < (int32_t)n; i++) {
        fx_set_reg(FX_EBX, i);
        const rx_addr pl = player_at(i, 0x00413af0);
        fx_set_reg(FX_EDI, pl);
        const rx_addr veh = U32(pl, 0x1ac);
        if (veh == 0 || U32(veh, 0x280) != 0) continue;
        vcall1(veh, 0x44, E + 0x20, 0x00413b13);
        if (contains_call(T, E + 0x20, 0x00413b1e)) {
            fx_set_reg(FX_ECX, T);
            const rx_addr s = fx_push(0, 0, 0x00413b2b);
            rx_fx_section_player_inside(pl, T);
            fx_pop(s);
        }
    }
    { const rx_addr s = fx_push(0, 0, 0x00413b37); rx_fx_trigger_prune(T); fx_pop(s); }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00413a80, rx_fx_timed_section_tick)

/* ---- 0040b770 OverlayColorEntity::vf19 (thiscall, dt; RET 4): while blending (+0x10c): time left (+0x110) -= dt; past the
 * end the colours (+0x90..+0xc4: top / middle / bottom rgba, the two midpoints) take their targets (+0x120..+0x154, x87 copies)
 * and the blend stops; else each moves by its rate (+0x160..+0x194) * dt; then OverlayColor_SetSceneColors 0040b1d0 (the
 * context's +0x14 scene). Frame: push ebp; and esp, -16; sub esp, 0xc; esi. */
void rx_fx_overlay_colour_tick(rx_addr e, float dt) {
    fx_frame f; fx_frame_enter(&f, 0xc, 16, FX_ESI);
    if (U32(e, 0x10c) == 0) { fx_frame_leave(&f); return; }
    const float left = rx_subs(F32(e, 0x110), dt);
    SF32(e, 0x110, left);
    fx_set_reg(FX_ESI, e + 0x90);
    if (0.0f > left) {
        for (uint32_t o = 0; o < 0x30; o += 4) fx_x87_copy(e + 0x90 + o, e + 0x120 + o);
        SU32(e, 0x10c, 0);
        fx_x87_copy(e + 0xc0, e + 0x150); fx_x87_copy(e + 0xc4, e + 0x154);
    } else {
        for (uint32_t o = 0; o < 0x10; o += 4) SF32(e, 0x90 + o, rx_adds(F32(e, 0x90 + o), rx_muls(F32(e, 0x160 + o), dt)));
        SF32(e, 0xa0, rx_adds(F32(e, 0xa0), rx_muls(dt, F32(e, 0x170))));
        for (uint32_t o = 4; o < 0x10; o += 4) SF32(e, 0xa0 + o, rx_adds(F32(e, 0xa0 + o), rx_muls(F32(e, 0x170 + o), dt)));
        const float b0 = rx_muls(F32(e, 0x180), dt);
        for (uint32_t o = 4; o < 0x10; o += 4) SF32(e, 0xb0 + o, rx_adds(rx_muls(F32(e, 0x180 + o), dt), F32(e, 0xb0 + o)));
        SF32(e, 0xb0, rx_adds(F32(e, 0xb0), b0));
        SF32(e, 0xc0, rx_adds(rx_muls(F32(e, 0x190), dt), F32(e, 0xc0)));
        SF32(e, 0xc4, rx_adds(rx_muls(F32(e, 0x194), dt), F32(e, 0xc4)));
    }
    {
        const rx_addr scene = U32(fx_ctx(), 0x14);
        const uint32_t a[1] = {scene};
        const rx_addr s = fx_push(1, a, 0x0040b9c1);
        rx_fx_overlay_scene_colors(scene, e+0x90, e+0xa0, e+0xb0);
        fx_pop(s);
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x0040b770, rx_fx_overlay_colour_tick)

/* ---- 00410dd0 ScrollingTextureEntity::vf19 (thiscall, dt; RET 4): per axis with a period T (+0xab0 / +0xab4) other than 0:
 * a += dt (+0xab8 / +0xabc), less |T| once a >= |T|; the offset (+0xaa0 / +0xaa4) = a / T. */
void rx_fx_scrolling_texture_tick(rx_addr e, float dt) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_EBP));
    for (int k = 0; k < 2; k++) {
        const uint32_t o = 4u * (uint32_t)k;
        const float T = F32(e, 0xab0 + o);
        if (T == 0.0f) continue;
        float a = k == 0 ? rx_adds(dt, F32(e, 0xab8)) : rx_adds(F32(e, 0xabc), dt);
        const float m = rx_bf(rx_fb(T) & 0x7fffffffu);
        SF32(e, 0xab8 + o, a);
        if (a >= m) { a = rx_subs(a, m); SF32(e, 0xab8 + o, a); }
        SF32(e, 0xaa0 + o, rx_divs(F32(e, 0xab8 + o), T));
    }
}
RX_FUNCTION(0x00410dd0, rx_fx_scrolling_texture_tick)

/* ---- 00404020 UIGateSelectionCameraEntity::vf19 (thiscall, dt; RET 4): when active (+0x4a0): the parts-view camera tick
 * (00656070, loop_cam_parts_view.c) and the scene's first camera view from it (0063dba0). Frame: push ebp; and esp, -8; push ecx;
 * push esi. */
void rx_fx_gate_camera_tick(rx_addr e, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u;
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.ebp); fx_set_reg(FX_EBP, sp0 - 4);
    SU32(A - 4, 0, fx_reg(FX_ECX)); SU32(A - 8, 0, sv.esi);
    rx_set_sp(A - 8);
    fx_set_reg(FX_ESI, e);
    if (U32(e, 0x4a0) != 0) {
        const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
        const uint32_t a[1] = {rx_fb(d)};
        {   /* 00656070 MXUIPartsViewCamera::vf19 (thiscall, dt; RET 4): loop_cam_parts_view.c */
            const rx_addr s = fx_push(1, a, 0x0040403f);
            rx_parts_view_camera_update(e, d);
            fx_pop(s);
        }
        const uint32_t b[2] = {e, U32(U32(e, 0x10), 0x19b8)};
        const rx_addr s = fx_push(2, b, 0x0040404f);
        rx_cam_entity_to_view(e, b[1]);
        fx_pop(s);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00404020, rx_fx_gate_camera_tick)

/* ---- small leaves ---- */
/* 00401060 (thiscall dst, src*; RET 4): *dst = *src (EAX = src, EDX = the value) */
void rx_fx_copy_dword(rx_addr dst, rx_addr src) { SU32(dst, 0, U32(src, 0)); }
RX_FUNCTION(0x00401060, rx_fx_copy_dword)
/* 00401070 (ECX = p*): **p, or 0 when *p is 0 */
uint32_t rx_fx_deref2(rx_addr p) { const rx_addr q = U32(p, 0); return q ? U32(q, 0) : 0; }
RX_FUNCTION(0x00401070, rx_fx_deref2)
/* 004010e0 (cdecl a, b): b */
uint32_t rx_fx_second(uint32_t a, uint32_t b) { (void)a; return b; }
RX_FUNCTION(0x004010e0, rx_fx_second)
/* 00401c80 (EAX = asset holder, EDX = out*): busy flag set, *out = the asset (+0x44); asset != 0 (EAX, ECX) */
uint32_t rx_fx_holder_lock(rx_addr h, rx_addr out) {
    const uint32_t a = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    SU32(out, 0, a);
    return U32(h, 0x44) != 0;
}
RX_FUNCTION(0x00401c80, rx_fx_holder_lock)
/* 00401ca0 (EAX = asset holder): busy flag cleared */
void rx_fx_holder_unlock(rx_addr h) { SU32(h, 0x4c, 0); }
RX_FUNCTION(0x00401ca0, rx_fx_holder_unlock)
/* 00401d40 IRenderable::vf7: the draw distance 15000 (x87, from 0x977564) */
float rx_fx_renderable_vf7(rx_addr self) { (void)self; return F32(0, 0x00977564); }
RX_FUNCTION(0x00401d40, rx_fx_renderable_vf7)
/* 00401f00 ModelEntity::vf1_o90: (word +0x20 >> 3) & 3 */
uint32_t rx_fx_model_entity_vf1(rx_addr self) { return ((uint32_t)U16(self, 0x20) >> 3) & 3; }
RX_FUNCTION(0x00401f00, rx_fx_model_entity_vf1)
/* 004023c0 (EAX = obj): +0x10 */
uint32_t rx_fx_field_10(rx_addr o) { return U32(o, 0x10); }
RX_FUNCTION(0x004023c0, rx_fx_field_10)
/* 00402af0 (EAX = obj): +0x68 */
uint32_t rx_fx_field_68(rx_addr o) { return U32(o, 0x68); }
RX_FUNCTION(0x00402af0, rx_fx_field_68)
/* 00402ad0 (ECX = obj, xmm0 = v): +0x118 = v unless already equal */
void rx_fx_set_118(rx_addr o, float v) { if (!(F32(o, 0x118) == v)) SF32(o, 0x118, v); }
RX_FUNCTION(0x00402ad0, rx_fx_set_118)
/* 00402c20: g_MXUIManager [0xa93fcc] */
uint32_t rx_fx_ui_manager(void) { return U32(0, 0x00a93fcc); }
RX_FUNCTION(0x00402c20, rx_fx_ui_manager)
/* 0041ad70 (EAX = cursor*, EDX = src*): **cursor = *src; *cursor += 4 (ECX / EDX the pointer / value) */
void rx_fx_push_dword(rx_addr cur, rx_addr src) { SU32(U32(cur, 0), 0, U32(src, 0)); SU32(cur, 0, U32(cur, 0) + 4); }
RX_FUNCTION(0x0041ad70, rx_fx_push_dword)
/* 0041c4b0 (cdecl y, x): atan2(y, x) through _CIatan2 (64-bit), rounded into the first argument's slot and loaded (x87) */
float rx_fx_atan2f(float y, float x) {
    const rx_f80 r = crt_ci_atan2(fx_ld(rx_sp() + 4), fx_ld(rx_sp() + 8), FX_CW);
    const float f = rx_f80_to_f32(r, 0);
    SF32(rx_sp(), 4, f);
    (void)y; (void)x;
    return f;
}
RX_FUNCTION(0x0041c4b0, rx_fx_atan2f)
/* 0041c580 Vec4_Int4ToFloatDivScalar (xmm0 = four int32; cdecl s): the lanes as floats (MMX cvtpi2ps pairs) / s; xmm1 = the
 * splat of s. Frame: push ebp; and esp, -16; sub esp, 0x20: the ints at A - 0x20. */
rx_v4 rx_fx_int4_div(const int32_t in[4], float s, rx_v4* splat) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, E = A - 0x20;
    SU32(sp0 - 4, 0, fx_reg(FX_EBP));
    for (int i = 0; i < 4; i++) SU32(E, 4u * (uint32_t)i, (uint32_t)in[i]);
    rx_v4 r;
    for (int i = 0; i < 4; i++) r.v[i] = rx_divs((float)in[i], s);
    if (splat) *splat = (rx_v4){{s, s, s, s}};
    return r;
}
RX_FUNCTION(0x0041c580, rx_fx_int4_div)
/* 0041c5c0 Vec3_DotDoubleSplat (xmm0 = a, xmm1 = b): splat((float)((b.y a.y + b.x a.x) + b.z a.z)) in double precision. Frame:
 * push ebp; and esp, -16; sub esp, 0x20: b at A - 0x20 (then the result), a at A - 0x10. */
rx_v4 rx_fx_dot3_splat(rx_v4 a, rx_v4 b) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, E = A - 0x20;
    SU32(sp0 - 4, 0, fx_reg(FX_EBP));
    rx_st4(E + 0x10, a);
    const double d = ((double)b.v[1] * (double)a.v[1] + (double)b.v[0] * (double)a.v[0]) + (double)b.v[2] * (double)a.v[2];
    const float f = (float)d;
    const rx_v4 r = {{f, f, f, f}};
    rx_st4(E, r);
    return r;
}
RX_FUNCTION(0x0041c5c0, rx_fx_dot3_splat)
