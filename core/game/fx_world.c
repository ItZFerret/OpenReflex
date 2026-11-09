/* fx_world.c -- placed world entities of the 0x6cxxxx / 0x6dxxxx range the race frames tick: the pop-up / pop-down hay bales
 * (kinematic props moving to their end position), the track-side flag holder, the land animals' (LandAnimalProximity)
 * animation phases (vf29) and their flee steering 006d2eb0, key-map slots, the online-content / storage pollers and the
 * session tick 006de690. notes/fx.md. */
#include <stdio.h>
#include <stdlib.h>
#include "fx.h"
#include "fx_util.h"
#include "world_veh.h"
#include "world_rider.h"
#include "world.h"
#include "anim.h"
#include "build_alloc.h"
#include "loop_world.h"
#include "crt.h"
#include "phys_coll_misc.h"
#include "race_misc.h"
#include "race_driver.h"
#include "race.h"
#include "net_boundary.h"

/* the entity's level world: game +0x5a54 when its scene (+0x10) is the context's +0x20, +0x20c8 for +0x1c, else 0 */
static rx_addr entity_world(rx_addr e) {
    const rx_addr c = fx_ctx(), sc = U32(e, 0x10);
    if (sc == U32(c, 0x20)) return U32(U32(0, FX_G_GAME), 0x5a54);
    if (sc == U32(c, 0x1c)) return U32(U32(0, FX_G_GAME), 0x20c8);
    return 0;
}
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
static float sqrt_x87(float x, rx_addr ret) {              /* fld x; push ecx; fstp [esp]; call SqrtF; add esp, 4 */
    const rx_addr sp = rx_sp();
    SU32(sp, -4, fx_x87_bits(rx_fb(x))); SU32(sp, -8, ret);
    return phys_sqrtf(rx_bf(fx_x87_bits(rx_fb(x))));
}
static float sqrt_sse(float x, rx_addr ret) {              /* push ecx; movss [esp], x; call SqrtF; add esp, 4 */
    const rx_addr sp = rx_sp();
    SF32(sp, -4, x); SU32(sp, -8, ret);
    return phys_sqrtf(x);
}
static rx_addr body_of(rx_addr h, rx_addr ret) {           /* PhysicsHandle_Body 00418f30 (EAX = handle) */
    const rx_addr s = fx_push(0, 0, ret);
    const rx_addr b = rx_world_physics_handle_body(h);
    fx_pop(s);
    return b;
}

/* ---- 006c1250 PopUpBale::vf19 / 006c08f0 PopDownBale::vf19 (thiscall bale, dt; RET 4): not while paused; once triggered
 * (up: +0x170 = 1, +0x174 = 0; down: +0x174 = 1, +0x178 = 0) the bale's position (vf17) moves toward the end position +0x1d0
 * by the step +0x1e0 (written to the local position +0x60..+0x68); within one step it snaps: the body's position and its
 * previous position (+0x40 / +0x60 of the physics body +0x158) set, the body enabled (up) or deactivated and disabled
 * (down), done (+0x174 / +0x178 = 1), +0xa2 = 1 (up) / 0 (down). Frame: push ebp; and esp, -16; sub esp, 0x38; ebx (up) /
 * edi (down); esi: E = A - 0x40: +0x8 |d|^2, +0xc step, +0x10..+0x18 d, +0x20 the end, +0x30 the position. */
static void bale_tick(rx_addr b, int down) {
    fx_frame f;
    if (down) fx_frame_enter(&f, 0x38, 16, FX_ESI, FX_EDI); else fx_frame_enter(&f, 0x38, 16, FX_EBX, FX_ESI);
    const rx_addr E = f.E, base = down ? 0x006c08f0 : 0x006c1250;
#define RA(up, dn) (down ? (rx_addr)(dn) : (rx_addr)(up))
    fx_set_reg(FX_ESI, b);
    (void)base;
    if (U32(entity_world(b), 0x20014) != 0 || U32(fx_timing(), 0xc) != 0) { fx_frame_leave(&f); return; }
    vcall1(b, 0x44, E + 0x30, RA(0x006c12b5, 0x006c0955));
    if (U32(b, down ? 0x178 : 0x174) != 0 || U32(b, down ? 0x174 : 0x170) != 1) { fx_frame_leave(&f); return; }
    if (!down) fx_set_reg(FX_EBX, 1);
    rx_st4(E + 0x20, rx_ld4(b + 0x1d0));
    const float dy = rx_subs(F32(E, 0x24), F32(E, 0x34));
    const float dz = rx_subs(F32(E, 0x28), F32(E, 0x38));
    const float dx = rx_subs(F32(E, 0x20), F32(E, 0x30));
    SF32(E, 0x18, dz);
    SF32(E, 0x14, dy);
    const float l2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
    SF32(E, 8, l2);
    SF32(E, 0x10, dx);
    rx_set_sp(E - 4);
    SF32(E - 4, 0x10, F32(b, 0x1e0));                         /* [esp + 0x10] after the push: E + 0xc */
    rx_set_sp(E);
    const float len = sqrt_x87(l2, RA(0x006c1350, 0x006c09ec));
    const float step = F32(E, 0xc);
    if (step > len) {
        const rx_addr h = U32(b, 0x158);
        if (h) rx_st4(body_of(h, RA(0x006c1371, 0x006c0a0d)) + 0x40, rx_ld4(b + 0x1d0));
        if (U32(b, 0x158)) rx_st4(body_of(U32(b, 0x158), RA(0x006c138d, 0x006c0a29)) + 0x60, rx_ld4(b + 0x1d0));
        if (!down) {
            if (U32(b, 0x158)) SU8(body_of(U32(b, 0x158), 0x006c13a9), 0xb0, 1);
        } else if (U32(b, 0x158)) {
            const rx_addr w = U32(U32(b, 0x158), 4);
            fx_set_reg(FX_EDI, w);
            const rx_addr body = body_of(U32(b, 0x158), 0x006c0a4d);
            {   /* push edi (the world); EDX = body; call Kinematic_Deactivate (RET 4) */
                const uint32_t a[1] = {w};
                const rx_addr s = fx_push(1, a, 0x006c0a55);
                rx_world_kinematic_deactivate(body, w);
                fx_pop(s);
            }
            SU8(body_of(U32(b, 0x158), 0x006c0a60), 0xb0, 0);
        }
        rx_st4(E + 0x20, rx_ld4(b + 0x1d0));
        SF32(b, 0x60, F32(E, 0x20));
        SF32(b, 0x64, F32(E, 0x24));
        SU32(b, down ? 0x178 : 0x174, 1);
        SU8(b, 0xa2, down ? 0 : 1);
        SF32(b, 0x68, F32(E, 0x28));
        fx_frame_leave(&f);
        return;
    }
    float nx, ny, nz;
    const float l2b = F32(E, 8);
    SF32(E, 8, l2b);
    if (l2b > 0.0f) {
        const float inv = 1.0f / sqrt_x87(l2b, RA(0x006c1412, 0x006c0acf));
        nx = rx_muls(inv, F32(E, 0x10));
        ny = rx_muls(F32(E, 0x14), inv);
        nz = rx_muls(F32(E, 0x18), inv);
    } else { nz = F32(E, 0x18); ny = F32(E, 0x14); nx = F32(E, 0x10); }
    const float s = F32(E, 0xc);
    SF32(b, 0x60, rx_adds(rx_muls(s, nx), F32(E, 0x30)));
    SF32(b, 0x64, rx_adds(rx_muls(ny, s), F32(E, 0x34)));
    SF32(b, 0x68, rx_adds(rx_muls(nz, s), F32(E, 0x38)));
#undef RA
    fx_frame_leave(&f);
}
void rx_fx_popup_bale_tick(rx_addr b, float dt) { (void)dt; bale_tick(b, 0); }
RX_FUNCTION(0x006c1250, rx_fx_popup_bale_tick)
void rx_fx_popdown_bale_tick(rx_addr b, float dt) { (void)dt; bale_tick(b, 1); }
RX_FUNCTION(0x006c08f0, rx_fx_popdown_bale_tick)

/* ---- 006c2eb0 TrackSideFlagHolder::vf19 (thiscall, dt; RET 4): not while paused or frozen (+0x360): the character's keyframes
 * sampled (00835430 on +0x148, xmm1 = dt); a finished one-shot (+0x370 in 1..2, playing +0x16c) goes back to the idle loop
 * (+0x174 = +0x358, +0x184 = 0). Frame: push edi; push esi. */
void rx_fx_trackside_flag_tick(rx_addr e, float dt) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.edi);
    fx_set_reg(FX_EDI, e);
    if (U32(entity_world(e), 0x20014) != 0 || U32(fx_timing(), 0xc) != 0 || U32(e, 0x360) != 0) { fx_restore(sv); return; }
    SU32(sp0 - 8, 0, sv.esi);
    rx_set_sp(sp0 - 8);
    const rx_addr c = e + 0x148;
    fx_set_reg(FX_ESI, c);
    { const uint32_t a[1] = {c}; const rx_addr s = fx_push(1, a, 0x006c2f11); rx_character_sample_keyframes(c, dt); fx_pop(s); }
    if (U32(e, 0x370) - 1u <= 1u && U32(e, 0x16c) != 0) {
        const uint32_t idle = U32(e, 0x358);
        SU32(c, 0x24, 0); SU32(c, 0x28, 0); SU32(c, 0x2c, idle); SF32(c, 0x3c, 0.0f);
        SU32(e, 0x370, 0);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006c2eb0, rx_fx_trackside_flag_tick)

/* ---- 006d07a0 LandAnimalProximity::vf29 (thiscall animal, dt, -, -, -; RET 0x10): the run blend (+0x35c): calm (+0x770 = 0)
 * it rises by 2 dt, held at 0.5 while the speed (+0x830) is below the run speed (+0x850), at 1 above it, and the herd's
 * reaction (+0x86c) is triggered as it crosses [0x9d78a0] (position +0x1b0, flag +0x1d8); fleeing it falls by 0.75 dt to 0.
 * The three gait phases (+0x838 / +0x840 / +0x848, periods +0x83c / +0x844 / +0x84c) wrap in [0, 1); then
 * Character_SampleBlend3 (008363b0) of the three gait slots (+0x354 / +0x774 / +0x358) with the blend. Frame: push ebp;
 * and esp, -16; sub esp, 0x1c; esi: E = A - 0x20: +0x10 the position. */
void rx_fx_land_animal_vf29(rx_addr e, float dt, uint32_t a1, uint32_t a2, rx_addr a3) {
    (void)a1; (void)a2; (void)a3;
    fx_frame f; fx_frame_enter(&f, 0x1c, 16, FX_ESI);
    const rx_addr E = f.E;
    fx_set_reg(FX_ESI, e);
    const float one = 1.0f;
    if (U32(e, 0x770) == 0) {
        const float old = F32(e, 0x35c);
        const float run = F32(e, 0x850), speed = F32(e, 0x830);
        const float v = rx_adds(rx_muls(dt, 2.0f /* 0x92e330 */), old);
        SF32(e, 0x35c, v);
        if (run > speed && v >= 0.5f /* 0x9322cc */) SF32(e, 0x35c, 0.5f);
        else if (speed >= run && v >= one) SF32(e, 0x35c, one);
        if (U32(e, 0x86c) != 0) {
            const float k = F32(0, 0x009d78a0);
            if (k > old && F32(e, 0x35c) > k) {
                vcall1(e, 0x44, E + 0x10, 0x006d0863);
                const rx_addr h = U32(e, 0x86c) + 0x1b0;
                SF32(h, 0, F32(E, 0x10)); SF32(h, 4, F32(E, 0x14)); SF32(h, 8, F32(E, 0x18));
                SU32(U32(e, 0x86c), 0x1d8, 1);
            }
        }
    } else {
        float v = rx_subs(F32(e, 0x35c), rx_muls(dt, 0.75f /* 0x935aa8 */));
        SF32(e, 0x35c, v);
        if (0.0f >= v) SF32(e, 0x35c, 0.0f);
    }
    static const uint16_t ph[3][2] = {{0x838, 0x83c}, {0x840, 0x844}, {0x848, 0x84c}};
    for (int k = 0; k < 3; k++) {
        float p = rx_adds(rx_divs(dt, F32(e, ph[k][1])), F32(e, ph[k][0]));
        SF32(e, ph[k][0], p);
        while (p >= one) { p = rx_subs(F32(e, ph[k][0]), one); SF32(e, ph[k][0], p); }
    }
    {
        const uint32_t a[7] = {e + 0x148, U32(e, 0x354), U32(e, 0x774), fx_x87_bits(U32(e, 0x838)), fx_x87_bits(U32(e, 0x840)),
                               fx_x87_bits(U32(e, 0x848)), fx_x87_bits(U32(e, 0x35c))};
        const rx_addr s = fx_push(7, a, 0x006d09e3);
        rx_character_sample_blend3(U32(e, 0x358), a[0], a[1], a[2], rx_bf(a[3]), rx_bf(a[4]), rx_bf(a[5]), rx_bf(a[6]));
        fx_pop(s);
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006d07a0, rx_fx_land_animal_vf29)

/* ---- 006d2eb0 (ESI = animal): the flee steering: for every player's vehicle within its radius (0x92fafc [type +0x194] + the
 * animal's +0x368) of the animal's position (vf17), the push-away (+0x7c0..+0x7c8) grows by d (r - |d|) / |d| (xz); the
 * pull back to the current waypoint (+0x7b0 -> the path +0x780 + 0x10 i) is added (cleared first when no vehicle pushes and
 * the waypoint is within 5); the sum damped by [0x9cc8a8], less half its direction, its length into +0x830 and limited to
 * +0x834. Frame: push ebp; and esp, -16; sub esp, 0x48; ebx / edi: E = A - 0x50: +0x14 pushed, +0x18 radius, +0x1c count /
 * |v|^2, +0x20 the position, +0x30..+0x38 d, +0x40 the vehicle's position. */
void rx_fx_land_animal_steer(rx_addr e) {
    fx_frame f; fx_frame_enter(&f, 0x48, 16, FX_EBX, FX_EDI);
    const rx_addr E = f.E;
    vcall1(e, 0x44, E + 0x20, 0x006d2ec9);
    const uint32_t n = U8(U32(0, FX_G_GAME), 0x1541c);
    fx_set_reg(FX_EBX, 0);
    SU32(E, 0x1c, n); SU32(E, 0x14, 0);
    for (uint32_t i = 0; (int32_t)i < (int32_t)n; i++) {
        fx_set_reg(FX_EBX, i);
        rx_addr pl;
        { const uint32_t a[2] = {i, 3}; const rx_addr s = fx_push(2, a, 0x006d2efe);
          pl = rx_world_players_find(U32(0, FX_G_GAME) + 0x7460, i, 3); fx_pop(s); }
        if (pl == 0) continue;
        const rx_addr veh = U32(pl, 0x1ac);
        if (veh == 0) continue;
        fx_set_reg(FX_EDI, veh);
        vcall1(veh, 0x44, E + 0x40, 0x006d2f22);
        const float r = rx_adds(F32(0x0092fafc + 4u * U32(veh, 0x194), 0), F32(e, 0x368));
        const float dz = rx_subs(F32(E, 0x28), F32(E, 0x48));
        SF32(E, 0x18, r);
        const float dx = rx_subs(F32(E, 0x20), F32(E, 0x40));
        SF32(E, 0x38, dz);
        const float l2 = rx_adds(rx_muls(dz, dz), rx_muls(dx, dx));
        rx_set_sp(E - 4);
        SF32(E - 4, 0x34, dx);
        rx_set_sp(E);
        const float len = sqrt_sse(l2, 0x006d2f80);
        const float rr = F32(E, 0x18);
        if (!(rr > len)) continue;
        const float k = rx_divs(rx_subs(rr, len), len);
        SF32(e, 0x7c0, rx_adds(rx_muls(F32(E, 0x30), k), F32(e, 0x7c0)));
        SF32(e, 0x7c4, rx_adds(F32(e, 0x7c4), rx_muls(k, 0.0f)));
        SF32(e, 0x7c8, rx_adds(F32(e, 0x7c8), rx_muls(F32(E, 0x38), k)));
        SU32(E, 0x14, 1);
    }
    const rx_addr wp = e + ((U32(e, 0x7b0) + 0x78) << 4);
    float wx = rx_subs(F32(wp, 0), F32(E, 0x20)), wz = rx_subs(F32(wp, 8), F32(E, 0x28));
    if (U32(E, 0x14) == 0 && 25.0f /* 0x92e134 */ > rx_adds(rx_muls(wz, wz), rx_muls(wx, wx))) {
        SF32(e, 0x7c0, 0.0f); SF32(e, 0x7c4, 0.0f); SF32(e, 0x7c8, 0.0f);
        wx = 0.0f; wz = 0.0f;
    }
    const float sx = rx_adds(F32(e, 0x7c0), wx);
    fx_x87_copy(e + 0x7c4, e + 0x7c4);
    SF32(e, 0x7c8, rx_adds(F32(e, 0x7c8), wz));
    const float damp = F32(0, 0x009cc8a8);
    SF32(e, 0x7c0, sx);
    SF32(e, 0x7c0, rx_muls(sx, damp));
    SF32(e, 0x7c4, rx_muls(F32(e, 0x7c4), damp));
    SF32(e, 0x7c8, rx_muls(F32(e, 0x7c8), damp));
    float y = F32(e, 0x7c4), z = F32(e, 0x7c8), x = F32(e, 0x7c0);
    const float l2 = rx_adds(rx_adds(rx_muls(z, z), rx_muls(y, y)), rx_muls(x, x));
    SF32(E, 0x20, x); SF32(E, 0x24, y); SF32(E, 0x28, z); SF32(E, 0x1c, l2);
    if (l2 > 0.0f) {
        const float inv = 1.0f / sqrt_x87(l2, 0x006d313a);
        x = rx_muls(inv, F32(E, 0x20)); y = rx_muls(F32(E, 0x24), inv); z = rx_muls(F32(E, 0x28), inv);
    }
    const float h = 0.5f /* 0x9322cc */;
    x = rx_muls(x, h); y = rx_muls(y, h); z = rx_muls(z, h);
    const float vx = rx_subs(F32(e, 0x7c0), x);
    SF32(e, 0x7c4, rx_subs(F32(e, 0x7c4), y));
    const float vz = rx_subs(F32(e, 0x7c8), z);
    SF32(e, 0x7c0, vx);
    SF32(e, 0x7c8, vz);
    const float vy = F32(e, 0x7c4);
    const float len = sqrt_sse(rx_adds(rx_adds(rx_muls(vx, vx), rx_muls(vy, vy)), rx_muls(vz, vz)), 0x006d31e9);
    const float lim = F32(e, 0x834);
    SF32(e, 0x830, len);
    if (len > lim) {
        const float k = rx_divs(lim, len);
        SF32(e, 0x7c0, rx_muls(F32(e, 0x7c0), k));
        SF32(e, 0x7c4, rx_muls(F32(e, 0x7c4), k));
        SF32(e, 0x7c8, rx_muls(F32(e, 0x7c8), k));
        fx_x87_copy(e + 0x830, e + 0x834);
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006d2eb0, rx_fx_land_animal_steer)

/* ---- 006d7840 KeyMap::vf9 (thiscall, slot, value; RET 8): slot +0x34 + 0x24 slot = value */
void rx_fx_keymap_vf9(rx_addr k, uint32_t slot, uint32_t v) { SU32(k, 0x34 + 0x24 * slot, v); }
RX_FUNCTION(0x006d7840, rx_fx_keymap_vf9)
/* ---- 006d7860 KeyMap::vf10 (thiscall, slot; RET 4): that value */
uint32_t rx_fx_keymap_vf10(rx_addr k, uint32_t slot) { return U32(k, 0x34 + 0x24 * slot); }
RX_FUNCTION(0x006d7860, rx_fx_keymap_vf10)

/* ---- 006c5960 (shared vf2 of the EventHandlerProxy family; thiscall proxy, event*; RET 4): the member callback +0x10 on
 * the object at +0xc + the adjustment +0x14, with the event's two words */
void rx_fx_event_proxy_call(rx_addr p, rx_addr ev) {
    const rx_addr d = U32(ev, 0);
    const uint32_t b = U32(d, 4), a = U32(d, 0);
    const rx_addr fn = U32(p, 0x10), obj = U32(p, 0x14) + U32(p, 0xc);
    void (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t, uint32_t));
    const uint32_t args[2] = {a, b};
    if (f) { const rx_addr s = fx_push(2, args, 0x006c597a); f(obj, a, b); fx_pop(s); }
    else rx_call_original(fn, RX_THISCALL, obj, 2, args);
}
RX_FUNCTION(0x006c5960, rx_fx_event_proxy_call)

/* 005933a0 (stdcall handler list: the pending handlers run; rx_loop's, not in the core) */
static void handlers_run(rx_addr list, rx_addr ret) {
    const uint32_t a[1] = {list};
    const rx_addr s = fx_push(1, a, ret); rx_pending_handlers_run(list); fx_pop(s);
}

/* ---- 006d3d30 (stdcall online content; RET 4): on the first call the PC DLC content database is set up: its directory
 * "<video path prefix>Database\\" (0078fd60 = _vsnprintf into a 0x104 buffer), the name L"PCDLCContent" (swprintf_s), a
 * DLC content object (0xf44 bytes, 006d3e30) started (vf5) and registered (008423d0, EDI = content + 0xc); +0x24 = 1. Frame:
 * push ebp; and esp, -8; sub esp, 0x71c (/GS cookie at +0x718); ebx / esi / edi: E = A - 0x728: +0xc the object, +0x10 the
 * record {+0x10 0, +0x14 0, +0x18 the wide name}, +0x418 the path copy, +0x618 the path. */
void rx_fx_online_content_poll(rx_addr oc) {
    fx_frame f; fx_frame_enter(&f, 0x71c, 8, FX_EBX);
    const rx_addr esp0 = f.A - 0x71c;
    SU32(esp0, 0x718, U32(0, 0x009b45e4) ^ esp0);
    fx_set_reg(FX_EBX, oc);
    rx_addr E = esp0 - 4;
    if (U8(oc, 0x24) == 0) {
        fx_push_regs(E, 2, (int[]){FX_ESI, FX_EDI});
        E = esp0 - 12;
        rx_set_sp(E);
        SU8(E, 0x618, 0);
        { const uint32_t a[3] = {E + 0x619, 0, 0x103}; FX_CRT(3, a, 0x006d3d76, crt_memset(E + 0x619, 0, 0x103)); }
        fx_set_reg(FX_ESI, 0x104); fx_set_reg(FX_EDI, E + 0x618);
        {   /* 0078fd60 (ESI = size, EDI = buffer; cdecl format, ...: _vsnprintf) */
            const uint32_t a[2] = {0x00961070, 0x00a899a0};
            const rx_addr s = fx_push(2, a, 0);
            rx_format_buffer(E + 0x618, 0x104, a[0], s + 8);
            fx_pop(s);
            SU32(E - 8, 0, a[0]); SU32(E - 4, 0, a[1]);
        }
        {   /* swprintf_s(record + 0x18, 0x104, L"PCDLCContent") (IAT 0x916268) */
            SU32(E, 0x10, 0);
            const uint32_t a[3] = {E + 0x18, 0x104, 0x0096107c};
            rx_set_sp(E - 8);
            rx_call_original(U32(0, 0x00916268), RX_CDECL, 0, 3, a);
            rx_set_sp(E);
        }
        fx_set_reg(FX_ESI, 0);
        SU32(E, 0x14, 0);
        { const uint32_t a[3] = {E + 0x418, E + 0x618, 0x103}; const rx_addr s = fx_push(3, a, 0x006d3dcc); crt_strncpy(E + 0x418, E + 0x618, 0x103); fx_pop(s); }
        SU8(E, 0x51b, 0);
        rx_addr obj;
        { const uint32_t a[1] = {0xf44}; const rx_addr s = fx_push(1, a, 0x006d3de1); obj = rx_reflex_aligned_alloc(0xf44); fx_pop(s); }
        if (obj != 0) {
            const uint32_t a[3] = {obj, E + 0x10, oc};
            /* ONLINE BOUNDARY: XenonOnlineContentPackage's constructor (006d3e30); this poll runs only for the XenonOnlineContent object, which the
             * product never creates (net_boundary.h) */
            obj = rx_net_call(0x006d3e30, RX_STDCALL, 0, 3, a, "XenonOnlineContentPackage constructor");
        }
        SU32(E, 0xc, obj);
        rx_call_original(U32(U32(obj, 0), 0x14), RX_THISCALL, obj, 0, 0);
        fx_set_reg(FX_EDI, oc + 0xc);
        { const uint32_t a[1] = {E + 0xc}; const rx_addr s = fx_push(1, a, 0); phys_vector_push_back(oc + 0xc, E + 0xc); fx_pop(s); }
        SU8(oc, 0x24, 1);
    } else {
        fx_push_regs(E, 2, (int[]){FX_ESI, FX_EDI});
    }
    rx_set_sp(esp0);
    SU32(esp0, -4, 0x006d3e27);
    phys_security_check_cookie(U32(esp0, 0x718) ^ esp0);
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006d3d30, rx_fx_online_content_poll)

/* ---- 006d3c40 XenonOnlineContent::vf2 (thiscall; RET 4): 006d3d30 (this), then the handler list [0xa94328] (005933a0).
 * Frame: push ebp; and esp, -8; push ecx (the argument slot). */
void rx_fx_online_content_vf2(rx_addr oc, uint32_t arg) {
    (void)arg;
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u;
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.ebp); fx_set_reg(FX_EBP, sp0 - 4);
    rx_set_sp(A);
    { const uint32_t a[1] = {oc}; const rx_addr s = fx_push(1, a, 0x006d3c4c); rx_fx_online_content_poll(oc); fx_pop(s); }
    handlers_run(U32(0, 0x00a94328), 0x006d3c57);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006d3c40, rx_fx_online_content_vf2)

/* ---- 006d4ac0 (EDI = a 0x1c block; plain ret) -> EAX = the device: PCStorageDevice constructed: the vtable 0x00961244, its container proxy (+4:
 * a 4-byte block holding the address of +4; a failed allocation leaves 0) and the three counters +0x10 / +0x14 / +0x18 cleared. Frame: push esi. */
rx_addr rx_fx_pc_storage_device_construct(rx_addr dev) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.esi);
    SU32(dev, 0, 0x00961244);
    fx_set_reg(FX_ESI, dev + 4);
    rx_set_sp(sp0 - 4);
    rx_addr proxy;
    { const uint32_t a[1] = {4}; const rx_addr s = fx_push(1, a, 0x006d4ad1); proxy = rx_reflex_aligned_alloc(4); fx_pop(s); }
    if (proxy != 0) SU32(proxy, 0, dev + 4);
    SU32(dev, 4, proxy);
    SU32(dev, 0x10, 0);
    SU32(dev, 0x14, 0);
    SU32(dev, 0x18, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return dev;
}
RX_FUNCTION(0x006d4ac0, rx_fx_pc_storage_device_construct)

/* ---- 006d5600 (ECX = the storage, ESI = the event system [0xa94330]; stdcall v; RET 4): the storage event {v, storage} (8 bytes) to the event
 * system (004020c0: EAX = the system, EDI = the name 0x958018, stack payload, 8) after the handler signature's type (void(IStorageMX*), type
 * 0x9d74fc) is named. Frame: sub esp, 0xc (the /GS cookie at +8); edi. */
void rx_fx_pc_storage_enumerate(rx_addr storage, uint32_t v, rx_addr evs) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0xc;
    const fx_saved sv = fx_save();
    SU32(E, 8, U32(0, 0x009b45e4) ^ E);
    SU32(sp0, -0x10, sv.edi);
    SU32(E, 4, storage);
    SU32(E, 0, v);
    {   /* push 0x9e1a7c; ECX = the type; call [0x916388] (thiscall, RET 4) */
        rx_set_sp(sp0 - 0x10);
        fx_type_name(0x009d74fc, 0x006d562b);
    }
    fx_set_reg(FX_EDI, 0x00958018);
    {
        const uint32_t a[2] = {E, 8};
        rx_set_sp(sp0 - 0x10);
        const rx_addr s = fx_push(2, a, 0x006d563e);
        rx_world_event_dispatch(evs, 0x00958018, E, 8);
        fx_pop(s);
    }
    fx_restore(sv);
    rx_set_sp(E);
    SU32(E, -4, 0x006d564a);
    phys_security_check_cookie(U32(E, 8) ^ E);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006d5600, rx_fx_pc_storage_enumerate)

/* ---- 006d4a70 PCStorage::vf7 (thiscall storage, arg; RET 4): when a refresh is pending (+0xc): the device (+4) made on first
 * use (006d4ac0, EDI = a 0x1c block), the flag cleared, the enumeration 006d5600 (thiscall storage, ESI = [0xa94330]; +8).
 * Frame: push esi. */
void rx_fx_pc_storage_vf7(rx_addr st, uint32_t arg) {
    (void)arg;
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, sv.esi);
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_ESI, st);
    if (U8(st, 0xc) != 0) {
        if (U32(st, 4) == 0) {
            rx_addr blk;
            { const uint32_t a[1] = {0x1c}; const rx_addr s = fx_push(1, a, 0x006d4a86); blk = rx_reflex_aligned_alloc(0x1c); fx_pop(s); }
            rx_addr dev = 0;
            if (blk != 0) {
                SU32(rx_sp(), -4, sv.edi);
                rx_set_sp(sp0 - 8);
                fx_set_reg(FX_EDI, blk);
                const rx_addr s = fx_push(0, 0, 0x006d4a95);
                dev = rx_fx_pc_storage_device_construct(blk);          /* 006d4ac0 (EDI = the block) */
                fx_pop(s);
                rx_set_sp(sp0 - 4);
            }
            SU32(st, 4, dev);
        }
        const uint32_t v = U32(st, 8);
        SU8(st, 0xc, 0);
        fx_set_reg(FX_ESI, U32(0, 0x00a94330));
        fx_set_reg(FX_ECX, st);
        const uint32_t a[1] = {v};
        const rx_addr s = fx_push(1, a, 0x006d4ab2);
        rx_fx_pc_storage_enumerate(st, v, U32(0, 0x00a94330));     /* 006d5600 (ECX = the storage, ESI = the event system) */
        fx_pop(s);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006d4a70, rx_fx_pc_storage_vf7)

/* ---- 0054de80 (stdcall player, flag; RET 8): a player's return from the online spectator state. Without the flag (s1) nothing happens unless the
 * player is marked (+0x908); the session statics are made on first use (race_static_dcf304: 006de1f0, _atexit 00913e00); the rest is for a network
 * game only: the stats block (0xda6cb0) must say a network session is running (+0x1c4c != 0, +0x1b0 != 1) and the player have a network
 * record (+0xb2c) with +0x1d8 set. Then the player's vehicle and rider camera are re-enabled and its spectator state cleared through the UI
 * property "SpectatorMode", the player table row and the activity's state (FUN_0045d4d0 / 005084d0 / 0061c6b0 / 0054e110 / 0055c800 /
 * 004df0f0 / 0055f630 / 006fe110 / 006f3400 and the UIProperty and table accessors).
 * ONLINE BOUNDARY: that second half is the online session's, reached only with +0x1c4c != 0, which the product (no network session) never
 * has and no reference world has either; it is fatal here (rx_net_unreached). Frame: push ebp; and esp, -8; sub esp, 0x34 (the /GS cookie at
 * +0x30); push ebx / esi / edi. */
void rx_player_leave_spectate(rx_addr player, uint32_t flag) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~7u, E = A - 0x34u;
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebp);
    SU32(E, 0x30, U32(0, 0x009b45e4) ^ E);
    SU32(E, -4, sv.ebx);
    SU32(E, -8, sv.esi);
    SU32(E, -0xc, sv.edi);
    rx_set_sp(E - 0xc);
    fx_set_reg(FX_EBX, player);
    fx_set_reg(FX_EDI, 0);
    if (flag != 0 || U32(player, 0x908) != 0) {
        if ((U32(0, 0x00dcf304) & 1) == 0) {
            const rx_addr s = fx_push(0, 0, 0x0054dec2);
            race_static_dcf304();                                        /* 006de1f0 + _atexit(00913e00) */
            fx_pop(s);
        }
        const rx_addr stats = U32(0, 0x00da6cb0);
        if (U32(stats, 0x1c4c) != 0 && U32(stats, 0x1b0) != 1 && U32(player, 0xb2c) != 0 && U32(U32(player, 0xb2c), 0x1d8) != 0)
            rx_net_unreached(0x0054de80, "the online spectator return");
    }
    SU32(E, -0x10, 0x0054e101);
    phys_security_check_cookie(U32(E, 0x30) ^ E);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0054de80, rx_player_leave_spectate)

/* ---- 006de1c0: the session manager instance 0xda6ca0 (constructed on first use: 006de1f0 through race_static_dcf304) */
rx_addr rx_fx_session_manager(void) {
    if ((U8(0, 0x00dcf304) & 1) == 0) {
        race_static_dcf304();   /* 006de1f0 + the atexit registration of 00913e00 */
    }
    return 0x00da6ca0;
}
RX_FUNCTION(0x006de1c0, rx_fx_session_manager)

/* ---- 006de690 (EDI = session; stdcall dt; RET 4): the session's pending handlers (005933a0 on [+0]); the countdown +0x64
 * runs down while not negative; the two sub-systems +0xc / +0x10 ticked (vf2 dt), the lobby +0x44 (006ed7e0) and the voice
 * +0x48 (006ee820) ticks; +0x68 = whether +0x10 is busy (+0x7c), then its vf34; a pending start (+0x74) outside the
 * front end (0075c5f0) kicks the first local player in a race (0054de80 (player, 1)). Frame: push ecx (/GS cookie); ebx / ebp
 * / esi. (Network play is not ported; these callees run the original.) */
void rx_fx_session_tick(float dt, rx_addr ses) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0 - 4, 0, U32(0, 0x009b45e4) ^ (sp0 - 4));
    fx_push_regs(sp0 - 4, 3, (int[]){FX_EBX, FX_EBP, FX_ESI});
    rx_set_sp(sp0 - 0x10);
    handlers_run(U32(ses, 0), 0x006de6a6);
    const float c = F32(ses, 0x64);
    if (c >= 0.0f) SF32(ses, 0x64, rx_subs(c, dt));
    const uint32_t d = fx_x87_bits(rx_fb(dt));
    for (int k = 0; k < 2; k++) {
        const rx_addr o = U32(ses, k ? 0x10 : 0xc);
        /* PRODUCT BOUNDARY: the offline session has no network / lobby subsystems (the original's Quazal initializer
         * 006de1f0 is not core); the original object always has them. Skip the null slots, nothing gameplay reads. */
        if (o == 0) continue;
        const rx_addr fn = U32(U32(o, 0), 8);
        void (*g)(rx_addr, float) = RX_FN(fn, void (*)(rx_addr, float));
        const uint32_t a[1] = {d};
        if (g) { const rx_addr s = fx_push(1, a, k ? 0x006de6e3 : 0x006de6d1); g(o, rx_bf(d)); fx_pop(s); }
        else rx_call_original(fn, RX_THISCALL, o, 1, a);
    }
    {
        const uint32_t a[1] = {d};
        if (U32(ses, 0x44) != 0) {   /* PRODUCT BOUNDARY: the lobby subsystem (offline: null) */
            fx_set_reg(FX_ESI, U32(ses, 0x44));
            { rx_regs in = fx_regs(); rx_net_call_ex(0x006ed7e0, &in, 0, 1, a, "the session's lobby tick"); }
        }
        if (U32(ses, 0x48) != 0) {   /* PRODUCT BOUNDARY: the voice subsystem (offline: null) */
            fx_set_reg(FX_ESI, U32(ses, 0x48));
            { rx_regs in = fx_regs(); rx_net_call_ex(0x006ee820, &in, 0, 1, a, "the session's voice tick"); }
        }
    }
    {
        const rx_addr o = U32(ses, 0x10);
        if (o != 0) {   /* PRODUCT BOUNDARY: the network session object (offline: null) */
            SU32(ses, 0x68, U32(o, 0x7c) != 0);
            rx_call_original(U32(U32(o, 0), 0x88), RX_THISCALL, o, 0, 0);
        }
    }
    if (U32(ses, 0x74) != 0) {
        const rx_addr game = U32(0, FX_G_GAME);
        fx_set_reg(FX_EBX, game);
        const rx_addr sv_ = fx_push(0, 0, 0); const uint32_t view = rx_game_is_view_track(game); fx_pop(sv_);
        if (view == 0) {
            const uint32_t n = U8(game, 0x1541c);
            fx_set_reg(FX_EBP, n);
            for (uint32_t i = 0; i < n; i++) {
                fx_set_reg(FX_ESI, i);
                rx_addr p;
                { const uint32_t a[2] = {i, 3}; const rx_addr s = fx_push(2, a, 0x006de752); p = rx_world_players_find(game + 0x7460, i, 3); fx_pop(s); }
                if (U32(p, 4) != 0 && U32(p, 0x10) == 0 && U32(p, 0xb2c) != 0) {
                    const uint32_t a[2] = {p, 1};
                    const rx_addr s = fx_push(2, a, 0x006de772);
                    rx_player_leave_spectate(p, 1);                        /* 0054de80 */
                    fx_pop(s);
                    break;
                }
            }
            SU32(ses, 0x74, 0);
        }
    }
    rx_set_sp(sp0 - 4);
    SU32(sp0 - 8, 0, 0x006de787);
    phys_security_check_cookie(U32(sp0 - 4, 0) ^ (sp0 - 4));
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006de690, rx_fx_session_tick)

/* ---- 006decd0 (EAX = the stats root; stdcall t; RET 4): the online session's clock: a handle to the session object
 * [0xd60d54] (0088bcd0 into a local), when it is valid (00703070) and ready (00884950), 006fbfe0 (EDI = the object; t); the
 * handle released (0088bac0). None of the callees is in the core. Frame: sub esp, 0xc; push edi; push ecx (the handle's
 * argument): E = sp0 - 0x10: +4 the handle.
 * ONLINE SESSION BOUNDARY: [0xd60d54] is the Quazal session object (set only by the online session code, never by the product).
 * With it 0 the original still builds the handle around null (0088bcd0 -> 0088bb50 returns at once), the validity test
 * 00703070 logs a Quazal error and returns 0, and 0088bac0 does nothing: no game memory is written. The product has no Quazal
 * (the offline stand-in is [0xd60d54] = 0): product nothing, oracle the original. Nothing gameplay reads the clock. */
void rx_fx_session_clock(rx_addr root, float t) {
    (void)root;
#ifdef RX_ORACLE
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0xc, E = e0 - 4;
    const fx_saved sv = fx_save();
    SU32(e0, -4, sv.edi);
    SU32(e0, -8, U32(0, 0x00d60d54));
    rx_set_sp(e0 - 8);
    rx_regs in = fx_regs(), o;
    in.r[FX_ECX] = e0; in.r[FX_EAX] = e0 - 8;
    { const uint32_t a[1] = {U32(0, 0x00d60d54)}; rx_set_sp(e0 - 4); rx_call_original_ex(0x0088bcd0, &in, &o, 1, a); }
    rx_set_sp(E);
    in = fx_regs(); in.r[FX_EAX] = e0;
    rx_net_call_ex(0x00703070, &in, &o, 0, 0, "the Quazal reference validity test");
    if ((o.r[FX_EAX] & 0xff) != 0) {
        in = fx_regs(); in.r[FX_EAX] = e0;
        rx_net_call_ex(0x00703070, &in, &o, 0, 0, "the Quazal reference validity test");
        uint32_t ready;
        { const rx_addr s = fx_push(0, 0, 0x006ded0e); ready = rx_net_ref_valid((o.r[FX_EAX] & 0xff) ? U32(e0, 0) : 0); fx_pop(s); }   /* 00884950 (ECX = the object) */
        if ((ready & 0xff) != 0) {
            in = fx_regs(); in.r[FX_EAX] = e0;
            rx_net_call_ex(0x00703070, &in, &o, 0, 0, "the Quazal reference validity test");
            fx_set_reg(FX_EDI, (o.r[FX_EAX] & 0xff) ? U32(e0, 0) : 0);
            in = fx_regs();
            const uint32_t a[1] = {fx_x87_bits(rx_fb(t))};
            rx_net_call_ex(0x006fbfe0, &in, 0, 1, a, "the online elapsed-time message");
        }
    }
    in = fx_regs(); in.r[FX_ECX] = e0;
    rx_call_original_ex(0x0088bac0, &in, 0, 0, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
#else
    if (U32(0, 0x00d60d54) == 0) { (void)t; return; }   /* the offline stand-in: no online session */
    fprintf(stderr, "rxcore: fx: 006decd0 the online session's clock: a Quazal session object exists (the product has no online session)\n");
    abort();
#endif
}
RX_FUNCTION(0x006decd0, rx_fx_session_clock)
