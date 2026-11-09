/* loop_player.c -- the player object (MXPlayer): the per-frame update (BaseDriver::vf7 -> MXPlayer_Update 0054af90) and the
 * Player_* functions it calls (camera selection, reset input / terrain / out-of-bounds checks, wreck recovery, crash timer,
 * respawn fade, input groups, jump tracker), plus the small activity / records / container / thread helpers of the same frame.
 *
 * Layout facts (player P, 0x12a0 bytes per slot at game +0x7460; offsets read from the code):
 *   +0x04 local human, +0x08 view index, +0x0c profile slot, +0x10 AI-controlled flag, +0x94 B the activity / driver object
 *   (+0x21c state 1..4, +0x238 sub state, +0x23c mode, +0x290 event dispatcher, +0xfe8 pause lock), +0x1ac V the vehicle,
 *   +0x1b4 R the rider animation, +0x1e0 / +0x1e4 / +0x1e8 record mirrors, +0x1f0 controller slot (-1 none), +0x21c reset latch,
 *   +0x220 gameplay camera, +0x974..+0x984 camera entities, +0x988 selected camera, +0x98c / +0x990 / +0x994 camera switching,
 *   +0x998 quick camera, +0x99c start-gate camera, +0x9a0 gate request, +0x9a8 current camera entity, +0xb2c player record,
 *   +0xb38..+0xb4c wreck recovery, +0xb58 / +0xc70 trick controls, +0x10b0..+0x10bc idle / air clocks, +0x1104..+0x1120 reset
 *   point candidate, +0x1130..+0x1150 saved spot, +0x1168 / +0x116c crash / reset timers, +0x11a0..+0x11a8 respawn fade,
 *   +0x11c8 / +0x11cc input groups held by the replay, +0x11e0 the jump tracker (0xa4 bytes), +0x1290 / +0x1294 jump clocks.
 *
 * Frames: every function lays out the original's frame from its entry ESP (rx_sp() = the address of the return address) and
 * runs its calls at the original's ESP; direct calls of core functions push the stack arguments and the call site's return
 * address (PL_AT), so callees' frames and the stack addresses they keep match the original's. Calls of other subsystems' core
 * functions go through the registry by address with the owner's prototype (PL_CORE: NULL -> the original runs). x87 code here is
 * only fld / fstp copies (pl_x87). */
#include "highlight_progress.h"
#include "player_reset_completion.h"
#include "loop_player.h"
#include "free_ride_borders.h"
#include "player_reset_gate.h"
#include "player_reset_manual.h"
#include "loop_vehicle.h"
#include "loop_util.h"
#include "loop_platform.h"
#include "world_veh.h"
#include "world_misc.h"
#include "world_node.h"
#include "world_rider.h"
#include "world_wreck.h"
#include "build_vehicle.h"
#include "build_tables.h"
#include "build_activity.h"
#include "build_alloc.h"
#include "race.h"
#include "race_misc.h"
#include "race_activity.h"
#include "race_activity_stub.h"
#include "race_driver.h"
#include "anim_scenario.h"
#include "anim.h"
#include "anim_rider_play.h"
#include "anim_rider_stunt.h"
#include "loop_cam_entity.h"
#include "loop_cam_snap.h"
#include "loop_world.h"
#include "build_uiprofile.h"
#include "phys_body.h"
#include "phys_terrain.h"
#include "fx.h"
#include "fx_util.h"
#include "crt.h"
#include "rider_cap.h"

/* another subsystem's core function by its original address, typed by the owner's prototype; NULL -> not built / not ported */
#ifdef RX_FN_TRACE
#define PL_CORE(addr, fn) RX_FN(addr, __typeof__(&fn))   /* -DRX_FN_TRACE: recorded with the function's name (tools/vtrace_check.py checks it is the port registered at addr) */
#else
#define PL_CORE(addr, fn) ((__typeof__(&fn))rx_fn(addr))
#endif
/* a direct call of a core function as the original's call instruction makes it: n stack arguments and the return address */
#define PL_AT(n, args, ret, stmt) do { const rx_addr s_ = loop_push((n), (args), (ret)); stmt; loop_pop(s_); } while (0)

/* ---- calls of original code with LTCG registers ---- */
typedef struct { uint32_t eax, ecx, edx, ebx, ebp, esi, edi; } pl_in;

static rx_regs pl_run(rx_addr fn, pl_in in, int n, const uint32_t* args) {
    rx_regs r, o;
    memset(&r, 0, sizeof r);
    memset(&o, 0, sizeof o);
    r.r[0] = in.eax; r.r[1] = in.ecx; r.r[2] = in.edx; r.r[3] = in.ebx; r.r[5] = in.ebp; r.r[6] = in.esi; r.r[7] = in.edi;
    rx_call_original_ex(fn, &r, &o, n, args);
    return o;
}
static uint32_t pl_call(rx_addr fn, pl_in in, int n, const uint32_t* args) { return pl_run(fn, in, n, args).r[0]; }
static uint32_t pl_call0(rx_addr fn, pl_in in) { return pl_run(fn, in, 0, 0).r[0]; }
static uint32_t pl_call1(rx_addr fn, pl_in in, uint32_t a) { return pl_run(fn, in, 1, &a).r[0]; }

/* x87 `fld m32; fstp m32`: a NaN keeps its payload, an SNaN comes out quiet (loop_cam.h cam_x87_bits) */
static uint32_t pl_x87(uint32_t b) {
    return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) ? (b | 0x00400000u) : b;
}
static void pl_x87_copy(rx_addr dst, rx_addr src) { SU32(dst, 0, pl_x87(U32(src, 0))); }

/* frame of `push ebp; mov ebp, esp; and esp, -align; sub esp, size; push <n regs>`: the ESP after the prologue */
static rx_addr pl_frame(rx_addr entry, uint32_t align, uint32_t size, uint32_t pushes) {
    return (((entry - 4u) & ~(align - 1u)) - size) - 4u * pushes;
}

#define PL_VEHICLE(p) U32((p), 0x1ac)
#define PL_DRIVER(p)  U32((p), 0x94)
#define PL_RIDER(p)   U32((p), 0x1b4)
#define G_MODE        0x00a942b4u    /* game run mode (1 recording, 2 replay / frozen: the reset checks are skipped) */

/* the per-controller input block of a slot: pointer table at 0x009c52c4 */
static rx_addr pl_input(uint32_t slot) { return U32(0, 0x009c52c4u + 4u * slot); }
/* a KeyMap query through its vtable: byte offset `voff` (4 = vf1 held / latched, 8 = vf2 pressed now), arguments (7, slot) */
static uint32_t pl_keymap_query(rx_addr km, uint32_t voff, uint32_t slot) { return loop_tc2(loop_vslot(km, voff), km, 7, slot); }
/* KeyMap slot 0x24: enable / disable the bindings of a controller slot */
static void pl_keymap_enable(rx_addr km, uint32_t slot, uint32_t on) { loop_tc2(loop_vslot(km, 0x24), km, slot, on); }

static void pl_invalid_parameter(void) { rx_call_original(U32(0, 0x00916224), RX_CDECL, 0, 0, 0); }  /* _invalid_parameter_noinfo */

/* ---- callees of other subsystems (owner's core function, else the original with its registers) ---- */

/* 00402dc0 (EDX = list): the entry of the function-local static list belonging to the current context */
static rx_addr pl_find_by_controller(rx_addr list, rx_addr ret) {
    __typeof__(&rx_world_find_by_controller) f = PL_CORE(0x00402dc0, rx_world_find_by_controller);
    rx_addr r;
    if (f) PL_AT(0, 0, ret, r = f(list));
    else r = pl_call0(0x00402dc0, (pl_in){.edx = list});
    return r;
}
/* The terrain of the current context: the function-local static at 0x009e30d4 (count +8, init bit 0x009e30e0) */
static rx_addr pl_current_terrain(rx_addr ret) {
    if (!(U8(0, 0x009e30e0) & 1)) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
    }
    return pl_find_by_controller(0x009e30d4, ret);
}
/* 00402160 Table_ColumnIndex (EAX = table, s0 = name; RET 4) */
static uint32_t pl_column_index(rx_addr table, rx_addr name, rx_addr ret, pl_in regs) {
    __typeof__(&rx_tbl_column_index) f = PL_CORE(0x00402160, rx_tbl_column_index);
    const uint32_t a[1] = {name};
    uint32_t r;
    if (f) PL_AT(1, a, ret, r = f(table, name));
    else { regs.eax = table; r = pl_call(0x00402160, regs, 1, a); }
    return r;
}
/* 0041aaf0 (EAX = table, EDI = column, s0 = row; RET 4): the cell's address */
static rx_addr pl_cell(rx_addr table, uint32_t col, uint32_t row, rx_addr ret, pl_in regs) {
    __typeof__(&rx_tbl_cell_word) f = PL_CORE(0x0041aaf0, rx_tbl_cell_word);
    const uint32_t a[1] = {row};
    rx_addr r;
    if (f) PL_AT(1, a, ret, r = f(table, col, row));
    else { regs.eax = table; regs.edi = col; r = pl_call(0x0041aaf0, regs, 1, a); }
    return r;
}
/* 004f6e60 Table_SetCellInt (EAX = column name, ESI = table; s0 row, s1 value pointer; RET 8) */
static void pl_set_cell_int(rx_addr table, rx_addr name, uint32_t row, rx_addr valp, rx_addr ret, pl_in regs) {
    __typeof__(&rx_tbl_set_cell_int) f = PL_CORE(0x004f6e60, rx_tbl_set_cell_int);
    const uint32_t a[2] = {row, valp};
    if (f) PL_AT(2, a, ret, f(table, name, row, valp));
    else { regs.eax = name; regs.esi = table; pl_call(0x004f6e60, regs, 2, a); }
}
/* 004f5330 (stdcall column, row, value pointer) */
static void pl_set_int(rx_addr col, uint32_t row, rx_addr valp, rx_addr ret, pl_in regs) {
    __typeof__(&rx_tbl_set_int) f = PL_CORE(0x004f5330, rx_tbl_set_int);
    const uint32_t a[3] = {col, row, valp};
    if (f) PL_AT(3, a, ret, f(col, row, valp));
    else pl_call(0x004f5330, regs, 3, a);
}
/* 0078fbe0 ReflexAlignedAlloc (cdecl size) / 0078fc50 ReflexAlignedFree (cdecl block) */
static rx_addr pl_aligned_alloc(uint32_t size, rx_addr ret, pl_in regs) {
    __typeof__(&rx_reflex_aligned_alloc) f = PL_CORE(0x0078fbe0, rx_reflex_aligned_alloc);
    const uint32_t a[1] = {size};
    rx_addr r;
    if (f) PL_AT(1, a, ret, r = f(size));
    else r = pl_call(0x0078fbe0, regs, 1, a);
    return r;
}
/* 006de1c0: the profile / session singleton (first use builds it and registers its exit handler): 0x00da6ca0 */
static rx_addr pl_profile(rx_addr ret) {
    __typeof__(&rx_fx_session_manager) f = PL_CORE(0x006de1c0, rx_fx_session_manager);
    rx_addr r;
    if (f) PL_AT(0, 0, ret, r = f());
    else r = pl_call0(0x006de1c0, (pl_in){0});
    return r;
}
/* the inline form of the same first-use test (MXPlayer_Update, 0054f0b0): only when the init bit is clear */
static void pl_profile_init(rx_addr ret) { if (!(U8(0, 0x00dcf304) & 1)) pl_profile(ret); }
/* 00884950 (ECX = record): is the record usable (AL); `ret` = the original's return address after the call, `esi` the caller's ESI (the
 * callee's `push ecx; push esi` leave both words below the entry ESP: dead stack that later records copy) */
static uint32_t pl_record_valid(rx_addr rec, uint32_t esi, rx_addr ret) {
    uint32_t r;
    PL_AT(0, 0, ret, SU32(rx_sp(), -4, rec); SU32(rx_sp(), -8, esi); r = rx_net_ref_valid(rec));
    return r & 0xffu;
}
/* 0054c020 Player_ActiveCameraEntity (EDI = p) */
static rx_addr pl_active_camera(rx_addr p, rx_addr ret) {
    __typeof__(&rx_world_player_active_camera_entity) f = PL_CORE(0x0054c020, rx_world_player_active_camera_entity);
    rx_addr r;
    if (f) PL_AT(0, 0, ret, r = f(p));
    else r = pl_call0(0x0054c020, (pl_in){.edi = p});
    return r;
}
/* 00559fd0 (EAX = player, EDI = activity): the player's driver object */
static rx_addr pl_player_driver(rx_addr p, rx_addr act, uint32_t esi, rx_addr ret) {
    __typeof__(&rx_activity_player_driver) f = PL_CORE(0x00559fd0, rx_activity_player_driver);
    rx_addr r;
    if (f) PL_AT(0, 0, ret, r = f(p, act));
    else r = pl_call0(0x00559fd0, (pl_in){.eax = p, .edi = act, .esi = esi});
    return r;
}
/* 00554c90 (stack driver; RET 4): the driver's rule 7 */
static rx_addr pl_rule7(rx_addr drv, pl_in regs, rx_addr ret) {
    __typeof__(&rx_driver_rule_7) f = PL_CORE(0x00554c90, rx_driver_rule_7);
    const uint32_t a[1] = {drv};
    rx_addr r;
    if (f) PL_AT(1, a, ret, r = f(drv));
    else r = pl_call(0x00554c90, regs, 1, a);
    return r;
}
/* 0054e890 (EAX = p): the wreck fields cleared */
static void pl_clear_wreck(rx_addr p, rx_addr ret) {
    __typeof__(&rx_world_rider_clear_wreck) f = PL_CORE(0x0054e890, rx_world_rider_clear_wreck);
    if (f) PL_AT(0, 0, ret, f(p));
    else pl_call0(0x0054e890, (pl_in){.eax = p});
}
/* 00621d70 (stack rider animation; RET 4) */
static void pl_rider_anim_stop(rx_addr r, rx_addr ret) {
    __typeof__(&rx_rider_animation_stop) f = PL_CORE(0x00621d70, rx_rider_animation_stop);
    const uint32_t a[1] = {r};
    if (f) PL_AT(1, a, ret, f(r));
    else pl_call(0x00621d70, (pl_in){0}, 1, a);
}
/* 00622e30 (stack rider controller; RET 4) */
static void pl_rider_end_crash(rx_addr e, rx_addr ret) {
    __typeof__(&rx_world_rider_end_crash) f = PL_CORE(0x00622e30, rx_world_rider_end_crash);
    const uint32_t a[1] = {e};
    if (f) PL_AT(1, a, ret, f(e));
    else pl_call(0x00622e30, (pl_in){0}, 1, a);
}
/* 00554da0 (ECX = sender, EDX = event name, ESI = dispatcher; stack arg; RET 4) */
static void pl_raise_event(rx_addr sender, rx_addr name, rx_addr dispatcher, uint32_t arg, rx_addr ret) {
    __typeof__(&rx_world_raise_event) f = PL_CORE(0x00554da0, rx_world_raise_event);
    const uint32_t a[1] = {arg};
    if (f) PL_AT(1, a, ret, f(sender, name, dispatcher, arg));
    else pl_call(0x00554da0, (pl_in){.ecx = sender, .edx = name, .esi = dispatcher}, 1, a);
}
/* the four pad KeyMaps of input group 0 / 3: 00548f20 / 00548f70 (EAX = slot), 00548fc0 / 00549050 (ESI = slot) */
static rx_addr pl_pad_map(rx_addr fn, uint32_t slot, rx_addr ret) {
    rx_addr (*f)(uint32_t) = 0;
    switch (fn) {
    case 0x00548f20: f = PL_CORE(0x00548f20, rx_world_pad_steer_map); break;
    case 0x00548f70: f = PL_CORE(0x00548f70, rx_world_pad_lean_map); break;
    case 0x00548fc0: f = PL_CORE(0x00548fc0, rx_world_pad_wreck_steer_map); break;
    default:         f = PL_CORE(0x00549050, rx_world_pad_wreck_lean_map); break;
    }
    rx_addr r;
    if (f) PL_AT(0, 0, ret, r = f(slot));
    else r = pl_call0(fn, (fn == 0x00548f20 || fn == 0x00548f70) ? (pl_in){.eax = slot} : (pl_in){.esi = slot});
    return r;
}

/* ---- respawn fade (state +0x11a0) ---- */

/* 0054b720 (ESI = p): fade state 2 (hold): the vehicle's slot 0x80 off, the rider's wreck bits cleared */
void rx_player_set_fade_hold(rx_addr p) {
    rx_addr v = PL_VEHICLE(p);
    if (v) loop_tc1(loop_vslot(v, 0x80), v, 0);
    rx_addr r = PL_RIDER(p);
    if (r) {
        SU8(r, 0x5bb64, 0);
        SU32(r, 0x5b6d0, 0);
        SU32(r, 0x5b568, 0);
        SF32(r, 0x5b570, 0.0f);
    }
    SU32(p, 0x11a0, 2);
}

/* 0054b780 (ESI = p): fade state 1 (fading out): both fade clocks = the .data tunable 0x009c542c (3.0 at run time), as the hold */
void rx_player_set_fade_out(rx_addr p) {
    rx_addr v = PL_VEHICLE(p);
    const uint32_t len = U32(0, 0x009c542c);
    SU32(p, 0x11a4, len);
    SU32(p, 0x11a8, len);
    if (v) loop_tc1(loop_vslot(v, 0x80), v, 0);
    rx_addr r = PL_RIDER(p);
    if (r) {
        SU8(r, 0x5bb64, 0);
        SU32(r, 0x5b6d0, 0);
        SU32(r, 0x5b568, 0);
        SF32(r, 0x5b570, 0.0f);
    }
    SU32(p, 0x11a0, 1);
}

/* 0054b7f0 (ESI = p): fade state 0 (done): vehicle alpha 0xff, slot 0x80 on; rider alpha 0xff, visible */
void rx_player_set_fade_clear(rx_addr p) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                       /* push edi */
    rx_addr v = PL_VEHICLE(p);
    if (v) {
        SU8(U32(v, 0x1328), 0x148, 0xff);
        loop_tc1(loop_vslot(v, 0x80), v, 1);
    }
    rx_addr r = PL_RIDER(p);
    if (r) {
        SU8(r, 0x5b200, 0xff);
        SU8(r, 0x5bb64, 1);
        SU32(r, 0x5b6d0, 0);
        SU32(r, 0xae70, 0);
    }
    SU32(p, 0x11a0, 0);
    rx_set_sp(sp0);
}

/* the fade alpha 255 * (1 - t / length) clamped to [0, 255]: `x >= 255 -> 255; else 0 < x (or unordered) keeps x; else 0` */
static uint8_t pl_fade_alpha(float x) {
    const float k255 = 255.0f /* 0x92e104 */;
    if (x >= k255) x = k255;
    else if (!(0.0f < x || x != x)) x = 0.0f;
    return (uint8_t)rx_cvtt(x);
}

/* 0054b850 Player_UpdateRespawnFade (EAX = p; stack dt; RET 4): state 1 fades the vehicle / rider alpha while the clock +0x11a8
 * runs down; at zero the fade ends (0054b7f0) once the spawn point is clear (00462130) and the player's driver is not busy. */
void rx_player_update_respawn_fade(rx_addr p, float dt) {
    if (U32(p, 0x11a0) != 1) return;
    const rx_addr sp0 = rx_sp(), S = sp0 - 8;  /* push esi, edi */
    rx_set_sp(S);
    const float t = F32(p, 0x11a8);
    rx_addr v = PL_VEHICLE(p);
    if (t == 0.0f) {                          /* ucomiss; lahf; test ah, 0x44; jnp: ordered equal */
        if (v != 0) {
            __typeof__(&rx_spawn_point_clear) f = PL_CORE(0x00462130, rx_spawn_point_clear);
            const uint32_t a[1] = {v};
            uint32_t clear;
            if (f) PL_AT(1, a, 0x0054b91f, clear = f(v, v));
            else clear = pl_call(0x00462130, (pl_in){.eax = v, .esi = p}, 1, a);
            rx_addr b = PL_DRIVER(p);
            if ((clear & 0xffu) != 0 && b != 0) {
                rx_addr sel = pl_player_driver(p, b, p, 0x0054b934);
                if (sel != 0 && U32(sel, 0x70) == 0) PL_AT(0, 0, 0x0054b943, rx_player_set_fade_clear(p));
            }
        }
        rx_set_sp(sp0);
        return;
    }
    const float left = 1.0f /* 0x931f24 */ - t / F32(p, 0x11a4);
    if (v) SU8(U32(v, 0x1328), 0x148, pl_fade_alpha(left * 255.0f));
    rx_addr r = PL_RIDER(p);
    if (r) SU8(r, 0x5b200, pl_fade_alpha(left * 255.0f));
    float n = F32(p, 0x11a8) - dt;
    if (!(n > 0.0f)) n = 0.0f;
    SF32(p, 0x11a8, n);
    rx_set_sp(sp0);
}

/* ---- reset / bounds checks ---- */

/* 0054ee00 Player_UpdateCrashResetTimer (EAX = p; stack dt; RET 4): the crash timer +0x1168 runs down while the rider is crashed
 * (+0x5bb68 or +0xae70) and respawns the player (00550650, ESI = p) at zero. Frame: push ebp; and esp, -8; push ecx; push esi. */
void rx_player_update_crash_reset_timer(rx_addr p, float dt) {
    const rx_addr sp0 = rx_sp();
    if (PL_VEHICLE(p) == 0) {
        SF32(p, 0x1168, 0.0f);
        return;
    }
    float t = F32(p, 0x1168);
    if (!(t > 0.0f)) return;
    rx_addr r = PL_RIDER(p);
    if (U32(r, 0x5bb68) != 0 || U32(r, 0xae70) != 0) SF32(p, 0x1168, t - dt);
    if (!(0.0f >= F32(p, 0x1168))) return;    /* comiss 0, t; jb: 0 < t or unordered */
    if (U32(0, G_MODE) == 2) return;
    rx_set_sp(pl_frame(sp0, 8, 0, 2));
    __typeof__(&rx_player_respawn) f = PL_CORE(0x00550650, rx_player_respawn);
    if (f) PL_AT(0, 0, 0x0054ee5f, f(p));
    else pl_call0(0x00550650, (pl_in){.esi = p, .eax = r});
    rx_set_sp(sp0);
}

/* 0054f330 Player_CheckOutOfBounds (EAX = p): the vehicle (position +0xc0) outside the terrain's box by more than the margin
 * (terrain +0x44d8 / +0x44dc min x / z, +0x44e0 / +0x44e4 max x / z, +0x44f0 margin) -> 00550a20 (ESI = p) unless the mode is 2. */
void rx_player_check_out_of_bounds(rx_addr p) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(pl_frame(sp0, 8, 0, 2));        /* push ebp; and esp, -8; push ecx; push esi */
    const rx_addr g = pl_current_terrain(0x0054f361);
    const rx_addr v = PL_VEHICLE(p);
    const float x = F32(v, 0xc0), m = F32(g, 0x44f0);
    int out = F32(g, 0x44d8) > x + m || x - m > F32(g, 0x44e0);
    if (!out) {
        const float z = F32(v, 0xc8);
        out = F32(g, 0x44dc) > z + m || z - m > F32(g, 0x44e4);
    }
    /* PORT ADDITION (free_ride_borders.h): Free Ride without borders: only the fall net (past the real edge, 4 s in the air; the edge
     * lies inside the original's zone, so `out` already holds there) */
    if (out && rx_frb_for(p) && !rx_frb_fallen(p, g, v)) { rx_frb_stats_value.edge_skipped++; out = 0; }
    if (out && U32(0, G_MODE) != 2) {
        __typeof__(&rx_player_reset_out_of_bounds) reset = PL_CORE(0x00550a20, rx_player_reset_out_of_bounds);
        if (reset) PL_AT(0, 0, 0x0054f3d2, reset(p));
        else pl_call0(0x00550a20, (pl_in){.esi = p, .ecx = v + 0xc0, .eax = g});
    }
    rx_set_sp(sp0);
}

/* 0054f3e0 Player_CheckResetTerrain (EAX = p): the vehicle's position (slot 0x44) inside the terrain box over a "reset" material
 * (TerrainMaterialAt 007c5120 == 7: EDI = terrain, EDX = position, EAX = layer 0) -> 00550b70 (ESI = p) unless the mode is 2.
 * Frame: push ebp; and esp, -16; sub esp, 0x14; push ebx / esi / edi: the position at +0x10. */
void rx_player_check_reset_terrain(rx_addr p) {
    rx_addr v = PL_VEHICLE(p);
    if (v == 0) return;
    /* PORT ADDITION (free_ride_borders.h): no reset zone in Free Ride, except for a drowned vehicle (+0x1314: the zones under the sea) */
    if (rx_frb_for(p) && U32(v, 0x1314) == 0) { rx_frb_stats_value.zone_skipped++; return; }
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x14, 3), L = S + 0x10;
    rx_set_sp(S);
    loop_tc1(loop_vslot(v, 0x44), v, L);
    rx_addr g = pl_current_terrain(0x0054f432);
    const float x = F32(L, 0), z = F32(L, 8);
    if (!(F32(g, 0x44d8) > x) && !(x > F32(g, 0x44e0)) && !(F32(g, 0x44dc) > z) && !(z > F32(g, 0x44e4))) {
        g = pl_current_terrain(0x0054f488);
        rx_regs caller;
        memset(&caller, 0, sizeof caller);
        caller.r[3] = U32(0, 0x009e30e0); caller.r[5] = sp0 - 4; caller.r[6] = p;
        uint32_t mat;
        __typeof__(&phys_terrain_material_at) f = PL_CORE(0x007c5120, phys_terrain_material_at);
        if (f) PL_AT(0, 0, 0x0054f495, mat = f(g, L, 0, &caller, 0));
        else mat = pl_call0(0x007c5120, (pl_in){.eax = 0, .edx = L, .edi = g, .esi = p, .ebx = caller.r[3], .ebp = sp0 - 4});
        if (mat == 7 && U32(0, G_MODE) != 2) {
            __typeof__(&rx_player_reset_terrain) f2 = PL_CORE(0x00550b70, rx_player_reset_terrain);
            if (f2) PL_AT(0, 0, 0x0054f4a8, f2(p));
            else pl_call0(0x00550b70, (pl_in){.esi = p, .edi = g, .ebx = caller.r[3]});
        }
    }
    rx_set_sp(sp0);
}

/* 0054d7c0 (EAX = p; result EAX): may the player reset? Not while the rider slot of the player's driver object (0055a050 index
 * into +0x27c for a remote player (+0x908), else +0x268) has an active rule 7 (00554c90) with +0x10 set. */
uint32_t rx_player_can_reset(rx_addr p) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);                      /* push ebx, esi, edi */
    uint32_t result = 1;
    const rx_addr b = PL_DRIVER(p);
    if (b != 0) {
        int32_t idx;
        __typeof__(&rx_world_group_rider_index) f = PL_CORE(0x0055a050, rx_world_group_rider_index);
        if (f) PL_AT(0, 0, 0x0054d7db, idx = f(b, p));
        else idx = (int32_t)pl_call0(0x0055a050, (pl_in){.ecx = b, .esi = p, .edi = b, .ebx = 1});
        const rx_addr obj = U32(U32(b, U32(p, 0x908) ? 0x27c : 0x268), (uint32_t)idx * 4u);
        if (obj != 0) {
            const rx_addr rule = pl_rule7(obj, (pl_in){.esi = p, .edi = b, .ebx = 1}, 0x0054d802);
            if (rule != 0 && U32(rule, 0x10) != 0) result = 0;
        }
    }
    rx_set_sp(sp0);
    return result;
}

/* ---- input groups of the controller slot (KeyMaps at +0x348 / +0x9ec / +0x127c / +0x167c / +0x748 / +0x147c / +0xf34 of the
 * per-slot input block and the multi-map groups 0 / 2 / 3 / 5) switched on / off; a replay holds them (+0x11c8 set): only the
 * group's bit in +0x11cc changes ---- */

/* group 0: the two pad maps 00548f20 / 00548f70 (EAX = slot), group 3: 00548fc0 / 00549050 (ESI = slot) */
static void pl_group_maps(rx_addr fa, rx_addr fb, uint32_t slot, uint32_t on, rx_addr ret_a, rx_addr ret_b) {
    pl_keymap_enable(pl_pad_map(fa, slot, ret_a), slot, on);
    pl_keymap_enable(pl_pad_map(fb, slot, ret_b), slot, on);
}
/* one KeyMap of the slot's input block */
static void pl_block(uint32_t slot, uint32_t off, uint32_t on) { pl_keymap_enable(pl_input(slot) + off, slot, on); }

/* 00548bf0 (ESI = slot, EDI = on): the blocks +0x148, +0x1134, +0x11d8 */
void rx_player_slot_group2(uint32_t slot, uint32_t on) {
    pl_block(slot, 0x148, on); pl_block(slot, 0x1134, on); pl_block(slot, 0x11d8, on);
}
/* 00548c40 (ESI = slot, EDI = on): ten blocks +0x33a0 .. +0x4630 */
void rx_player_slot_group5(uint32_t slot, uint32_t on) {
    static const uint16_t blk[10] = {0x33a0, 0x358c, 0x378c, 0x398c, 0x3b8c, 0x3d8c, 0x3f8c, 0x418c, 0x438c, 0x4630};
    for (int i = 0; i < 10; i++) pl_block(slot, blk[i], on);
}

/* The "on" sequence of the groups shared by Player_UpdateCamera (all eleven, `all`) and 0054d480 (the first three): each group
 * re-reads the slot and skips when it is negative; under a replay hold only the bit is set. `rets` = the call sites' return
 * addresses of the pad-map calls (group 0 a / b, group 3 a / b) and of the group 2 / 5 calls. */
typedef struct { rx_addr g0a, g0b, g2, g3a, g3b, g5; } pl_group_rets;
static void pl_groups_on(rx_addr p, int all, pl_group_rets rets) {
    int32_t s;
#define PL_GROUP(bit, apply) \
    do { s = I32(p, 0x1f0); \
         if (s >= 0) { if (U32(p, 0x11c8) != 0) SU32(p, 0x11cc, U32(p, 0x11cc) | (bit)); else { apply; } } } while (0)
    PL_GROUP(1, pl_group_maps(0x00548f20, 0x00548f70, (uint32_t)s, 1, rets.g0a, rets.g0b));
    PL_GROUP(2, pl_block((uint32_t)s, 0x348, 1));
    PL_GROUP(4, PL_AT(0, 0, rets.g2, rx_player_slot_group2((uint32_t)s, 1)));
    if (!all) return;
    PL_GROUP(8, pl_group_maps(0x00548fc0, 0x00549050, (uint32_t)s, 1, rets.g3a, rets.g3b));
    PL_GROUP(0x10, pl_block((uint32_t)s, 0x9ec, 1));
    PL_GROUP(0x20, PL_AT(0, 0, rets.g5, rx_player_slot_group5((uint32_t)s, 1)));
    PL_GROUP(0x40, pl_block((uint32_t)s, 0x127c, 1));
    PL_GROUP(0x80, pl_block((uint32_t)s, 0x167c, 1));
    PL_GROUP(0x100, pl_block((uint32_t)s, 0x748, 1));
    PL_GROUP(0x200, pl_block((uint32_t)s, 0x147c, 1));
    PL_GROUP(0x400, pl_block((uint32_t)s, 0xf34, 1));
#undef PL_GROUP
}

/* 0054d480 (EAX = p): the first three groups on. Frame: push esi, edi. */
void rx_player_restore_input_groups(rx_addr p) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    pl_groups_on(p, 0, (pl_group_rets){0x0054d4a7, 0x0054d4ba, 0x0054d524, 0, 0, 0});
    rx_set_sp(sp0);
}

/* 0054fcd0 / 0054fd20 (EAX = p, EDI = on): group 0 / group 3 on or off (the hold bit set / cleared under a replay). push esi. */
static void pl_set_group_maps(rx_addr p, uint32_t on, uint32_t bit, rx_addr fa, rx_addr fb, rx_addr ra, rx_addr rb) {
    const int32_t s = I32(p, 0x1f0);
    if (s < 0) return;
    if (U32(p, 0x11c8) != 0) {
        SU32(p, 0x11cc, on ? (U32(p, 0x11cc) | bit) : (U32(p, 0x11cc) & ~bit));
        return;
    }
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    pl_group_maps(fa, fb, (uint32_t)s, on, ra, rb);
    rx_set_sp(sp0);
}
void rx_player_set_group0(rx_addr p, uint32_t on) { pl_set_group_maps(p, on, 1, 0x00548f20, 0x00548f70, 0x0054fd01, 0x0054fd13); }
void rx_player_set_group3(rx_addr p, uint32_t on) { pl_set_group_maps(p, on, 8, 0x00548fc0, 0x00549050, 0x0054fd4f, 0x0054fd5f); }
/* 0054fd70 (ECX = p, EDX = on) +0x147c; 0054fdc0 +0x167c; 0054fe10 +0x748; 0054fe60 +0xf34. push esi before the call. */
static void pl_set_block(rx_addr p, uint32_t off, uint32_t bit, uint32_t on) {
    const int32_t s = I32(p, 0x1f0);
    if (s < 0) return;
    if (U32(p, 0x11c8) != 0) {
        SU32(p, 0x11cc, on ? (U32(p, 0x11cc) | bit) : (U32(p, 0x11cc) & ~bit));
        return;
    }
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    pl_block((uint32_t)s, off, on);
    rx_set_sp(sp0);
}
void rx_player_set_group_147c(rx_addr p, uint32_t on) { pl_set_block(p, 0x147c, 0x200, on); }
void rx_player_set_group_167c(rx_addr p, uint32_t on) { pl_set_block(p, 0x167c, 0x80, on); }
void rx_player_set_group_748(rx_addr p, uint32_t on)  { pl_set_block(p, 0x748, 0x100, on); }
void rx_player_set_group_f34(rx_addr p, uint32_t on)  { pl_set_block(p, 0xf34, 0x400, on); }

/* 005485c0 (EAX = slot; result st(0)): the analog value of KeyMap +0x348 (slot 0x10, arguments 7, slot) */
float rx_player_analog_348(uint32_t slot) {
    const rx_addr km = pl_input(slot) + 0x348;
    return loop_tc2_x87(loop_vslot(km, 0x10), km, 7, slot);
}
/* 00548880 (EAX = slot; result EAX): KeyMap +0x2fa0 latched (vf1, arguments 7, slot) as 0 / 1 */
uint32_t rx_player_pressed_2fa0(uint32_t slot) {
    const rx_addr km = pl_input(slot) + 0x2fa0;
    return pl_keymap_query(km, 4, slot) != 0;
}

/* 00548850 / 005488b0 (EAX = slot; result EAX): KeyMap +0x2da0 / +0x31a0 latched (vf1, arguments 7, slot) as 0 / 1 */
uint32_t rx_player_pressed_2da0(uint32_t slot) {
    const rx_addr km = pl_input(slot) + 0x2da0;
    return pl_keymap_query(km, 4, slot) != 0;
}
uint32_t rx_player_pressed_31a0(uint32_t slot) {
    const rx_addr km = pl_input(slot) + 0x31a0;
    return pl_keymap_query(km, 4, slot) != 0;
}

/* 00548960 (ESI = player slot; result EAX): the wreck type a scripted control state asks for (the WreckChecker's scripted branch,
 * reached only while [0x00af6c00] is set, which nothing in the exe writes). Three key maps (+0x4f3c, +0x4fe0, +0x5084; vf2 with
 * arguments 7, slot) select a group, the latched maps +0x2ba0 / +0x2fa0 / +0x31a0 / +0x2da0 (vf1) the type inside it:
 *   +0x4f3c set:  +0x2ba0 0x22, +0x2fa0 0x20, +0x31a0 0x21, +0x2da0 0x23
 *   +0x4fe0 set:  +0x2ba0 0x1e, +0x2fa0 0x1c, +0x31a0 0x1d, +0x2da0 0x1f
 *   +0x5084 set:  +0x2ba0 0x1a, +0x2fa0 0x18, +0x31a0 0x19, +0x2da0 0x1b
 * 0 when none applies. */
uint32_t rx_player_scripted_wreck_type(uint32_t slot) {
    static const uint32_t k_group_map[3] = {0x4f3c, 0x4fe0, 0x5084};
    const rx_addr input = pl_input(slot);
    uint32_t group = 3;
    if (pl_keymap_query(input + k_group_map[0], 8, slot) != 0) group = 0;
    else if (pl_keymap_query(input + k_group_map[1], 8, slot) != 0) group = 1;
    else if (pl_keymap_query(input + k_group_map[2], 8, slot) != 0) group = 2;
    if (group == 3) return 0;
    if (pl_keymap_query(pl_input(slot) + 0x2ba0, 4, slot) != 0) return group == 0 ? 0x22 : group == 1 ? 0x1e : 0x1a;
    if (group == 0) {
        if (pl_keymap_query(pl_input(slot) + 0x2fa0, 4, slot) != 0) return 0x20;
        if (pl_keymap_query(pl_input(slot) + 0x31a0, 4, slot) != 0) return 0x21;
        return rx_player_pressed_2da0(slot) ? 0x23 : 0;
    }
    if (group == 1) {
        if (pl_keymap_query(pl_input(slot) + 0x2fa0, 4, slot) != 0) return 0x1c;
        if (rx_player_pressed_31a0(slot)) return 0x1d;
        return rx_player_pressed_2da0(slot) ? 0x1f : 0;
    }
    if (rx_player_pressed_2fa0(slot)) return 0x18;
    if (rx_player_pressed_31a0(slot)) return 0x19;
    return rx_player_pressed_2da0(slot) ? 0x1b : 0;
}

/* 0054ec40 (EAX = p): cancel the wreck: wreck fields cleared (0054e890), the rider animation stopped (00621d70), the event
 * 0x00941260 raised for the vehicle (00554da0: sender vehicle, dispatcher driver +0x290, argument the driver). push esi. */
void rx_player_cancel_wreck(rx_addr p) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    pl_clear_wreck(p, 0x0054ec48);
    const rx_addr r = PL_RIDER(p);
    if (r) pl_rider_anim_stop(r, 0x0054ec58);
    const rx_addr v = PL_VEHICLE(p);
    if (v) {
        const rx_addr b = PL_DRIVER(p);
        pl_raise_event(v, 0x00941260, U32(b, 0x290), b, 0x0054ec79);
    }
    rx_set_sp(sp0);
}

/* 0054d2a0 (ECX = p; stack who, arg; RET 8): for the player itself: a crashed rider controller (V +0x1a0, +0xaf10) ends the
 * crash (00622e30) and the input groups come back; the valid record gets event 5 (006f34d0: EDI = record; stack 5, arg's +0x74
 * or 0, 0). push esi, edi. */
void rx_player_event_a(rx_addr p, rx_addr who, rx_addr arg) {
    if (p != who) return;
    const rx_addr v = PL_VEHICLE(p);
    if (v == 0) return;
    const rx_addr e = U32(v, 0x1a0);
    if (e == 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    if (U32(e, 0xaf10) != 0) {
        pl_rider_end_crash(e, 0x0054d2cd);
        PL_AT(0, 0, 0x0054d2d4, rx_player_restore_input_groups(p));
    }
    const rx_addr rec = U32(p, 0xb2c);
    if (rec != 0 && pl_record_valid(rec, p, 0x0054d2e3) != 0) {
        const uint32_t a[3] = {5, arg ? U32(arg, 0x74) : 0, 0};
        /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
        pl_call(0x006f34d0, (pl_in){.edi = U32(p, 0xb2c), .esi = p}, 3, a);
    }
    rx_set_sp(sp0);
}

/* 0054d530 (ECX = p; stack who, arg, kind; RET 0xc): for the player itself: kinds 3 / 4 go to the rider controller (00623060:
 * EAX = controller, EDI = kind); the valid record gets event 4 (stack 4, arg's +0x74, kind). push esi; push edi. */
void rx_player_event_b(rx_addr p, rx_addr who, rx_addr arg, uint32_t kind) {
    if (p != who) return;
    const rx_addr v = PL_VEHICLE(p);
    if (v == 0) return;
    const rx_addr e = U32(v, 0x1a0);
    if (e == 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    if (kind == 3 || kind == 4) PL_AT(0, 0, 0x0054d561, rx_rider_stunt_end_outputs(e));   /* 00623060 (EAX = the controller) */
    const rx_addr rec = U32(p, 0xb2c);
    if (rec != 0 && pl_record_valid(rec, p, 0x0054d570) != 0) {
        const uint32_t a[3] = {4, U32(arg, 0x74), kind};
        /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
        pl_call(0x006f34d0, (pl_in){.edi = U32(p, 0xb2c), .esi = p}, 3, a);
    }
    rx_set_sp(sp0);
}

/* 0054d1a0 "StuntHoldBegin" listener (ECX = p; stack who, trick; RET 8): for the player itself with a vehicle and a rider
 * controller (V +0x1a0) that is in a trick (+0xaf10 != 0): the hold flags +0xaf30 and +0xaf34 are set to 1. Frame: none. */
void rx_player_event_hold_begin(rx_addr p, rx_addr who, rx_addr trick) {
    (void)trick;
    if (p != who) return;
    const rx_addr v = PL_VEHICLE(p);
    if (v == 0) return;
    const rx_addr e = U32(v, 0x1a0);
    if (e == 0) return;
    if (U32(e, 0xaf10) == 0) return;
    SU32(e, 0xaf30, 1);
    SU32(e, 0xaf34, 1);
}

/* 0054d1e0 "StuntHoldEnd" listener (same shape; RET 8): +0xaf30 = 0 (the hold is released) and +0xaf34 = 1. Frame: none. */
void rx_player_event_hold_end(rx_addr p, rx_addr who, rx_addr trick) {
    (void)trick;
    if (p != who) return;
    const rx_addr v = PL_VEHICLE(p);
    if (v == 0) return;
    const rx_addr e = U32(v, 0x1a0);
    if (e == 0) return;
    if (U32(e, 0xaf10) == 0) return;
    SU32(e, 0xaf30, 0);
    SU32(e, 0xaf34, 1);
}

/* 0054d310 "StuntLanded" listener (ECX = p; stack who, trick; RET 8): for the player itself in a trick (+0xaf10 != 0) and a trick
 * record without the +0x64 flag: the landing clip is requested (00621fe0: ESI = the rider controller, EDI = who, ECX = the trick)
 * and the trick's control suppression (0054d3b0: EAX = p, stack trick) is applied. Frame: push ecx, ebx, ebp, esi, edi. */
void rx_player_event_landed(rx_addr p, rx_addr who, rx_addr trick) {
    if (p != who) return;
    const rx_addr v = PL_VEHICLE(p);
    if (v == 0) return;
    const rx_addr e = U32(v, 0x1a0);
    if (e == 0) return;
    if (U32(e, 0xaf10) == 0) return;
    if (U32(trick, 0x64) != 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 20);
    PL_AT(0, 0, 0x0054d34d, rx_rider_crash_request_begin(e, who, trick));
    {
        const uint32_t a[1] = {trick};
        PL_AT(1, a, 0x0054d355, rx_rider_stunt_controls(p, trick));
    }
    rx_set_sp(sp0);
}

/* 0054d360 "StuntTakeoff" listener (ECX = p; stack who, trick; RET 8): for the player itself in a trick (+0xaf10 != 0) and a trick
 * record WITH the +0x64 flag: the air-clip request (006223e0, same registers as 00621fe0) and the input groups come back
 * (0054d480: EAX = p). Frame: push ebx, esi, edi. */
void rx_player_event_takeoff(rx_addr p, rx_addr who, rx_addr trick) {
    if (p != who) return;
    const rx_addr v = PL_VEHICLE(p);
    if (v == 0) return;
    const rx_addr e = U32(v, 0x1a0);
    if (e == 0) return;
    if (U32(e, 0xaf10) == 0) return;
    if (U32(trick, 0x64) == 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);
    PL_AT(0, 0, 0x0054d399, rx_rider_crash_request_resume(e, who, trick));
    PL_AT(0, 0, 0x0054d39f, rx_player_restore_input_groups(p));
    rx_set_sp(sp0);
}

/* 0054feb0 (EAX = a, ESI = b; stack p, c; RET 8): with a valid record and the mode not 2, post 006f35d0 (EDI = record; nine
 * stack floats c, a, b copied through the x87). push ebx, ebp, edi. */
void rx_player_post_vectors(rx_addr a, rx_addr b, rx_addr p, rx_addr c) {
    const rx_addr rec = U32(p, 0xb2c);
    if (rec == 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);
    if (pl_record_valid(rec, b, 0x0054fecc) != 0 && U32(0, G_MODE) != 2) {
        uint32_t v[9];
        for (uint32_t i = 0; i < 3; i++) {
            v[i] = pl_x87(U32(c, 4u * i));
            v[3 + i] = pl_x87(U32(a, 4u * i));
            v[6 + i] = pl_x87(U32(b, 4u * i));
        }
        /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
        pl_call(0x006f35d0, (pl_in){.edi = U32(p, 0xb2c), .esi = b, .ebx = c, .ebp = p}, 9, v);
    }
    rx_set_sp(sp0);
}

/* 0054f0b0 (ECX = p; stack a, b; RET 8): the "name match" event: for the player's own vehicle `a` whose name (b +0xc0) equals the
 * string at 0x009408d8 (_stricmp), outside the profile's lock-out (+0x1c4c / +0x1b0) or with a valid record: 00550cc0 (ESI = p)
 * unless the mode is 2. Frame: push ebp; and esp, -8; push ecx; push esi. */
void rx_player_event_name(rx_addr p, rx_addr a, rx_addr b) {
    if (a != PL_VEHICLE(p)) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(pl_frame(sp0, 8, 0, 2));
    int same;
    {
        __typeof__(&crt_stricmp) f = PL_CORE(0x78578d50, crt_stricmp);
        const uint32_t args[2] = {0x009408d8, b + 0xc0};
        if (f) same = f(0x009408d8, b + 0xc0) == 0;
        else same = rx_call_original(U32(0, 0x009162bc), RX_CDECL, 0, 2, args) == 0;
    }
    if (same) {
        pl_profile_init(0x0054f0f6);
        const rx_addr g = U32(0, 0x00da6cb0);
        int go = 1;
        if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1) {
            const rx_addr rec = U32(p, 0xb2c);
            go = rec != 0 && pl_record_valid(rec, p, 0x0054f129) != 0;
        }
        if (go && rx_frb_for(p)) {   /* PORT ADDITION (free_ride_borders.h): Free Ride keeps only the deep-water "ResetPlayer" triggers */
            if (rx_frb_keep_trigger(b)) rx_frb_stats_value.trigger_kept++;
            else { rx_frb_stats_value.trigger_skipped++; go = 0; }
        }
        if (go && U32(0, G_MODE) != 2) {
            __typeof__(&rx_player_respawn_trigger) f = PL_CORE(0x00550cc0, rx_player_respawn_trigger);
            if (f) PL_AT(0, 0, 0x0054f13b, f(p));
            else pl_call0(0x00550cc0, (pl_in){.esi = p});
        }
    }
    rx_set_sp(sp0);
}

/* ---- cameras ---- */

/* 00554ce0 (ECX = arg, ESI = the event dispatcher, stack player; RET 4): the "Camera Changed" event. The type signature
 * .P6AXPAVMXPlayer@@PBD@Z (descriptor 0x009ce4e4) is named first (the original's caching call), then the payload {arg, player}
 * (8 bytes, local_c) goes to the activity's event system (004020c0) under the name 0x009416b0 ("Camera Changed").
 * Frame: sub esp, 0xc (/GS cookie at entry - 4); push edi; the calls run at entry - 0x10. */
void rx_player_camera_changed(rx_addr arg, rx_addr dispatcher, rx_addr player) {
    const rx_addr E = rx_sp(), S = E - 0x10u;
    rx_set_sp(S);
    SU32(E, -4u, U32(0, 0x009b45e4) ^ E);          /* the /GS cookie */
    SU32(E, -8u, arg);                             /* local_c[0] */
    SU32(E, -0xcu, player);                        /* local_c[1] */
    {   /* typeid(<signature>).name(): push the type list; ECX = the type descriptor; call [0x916388] */
        const uint32_t a[1] = {0x009e1a7cu};
        const rx_addr s = loop_push(1, a, 0x00554d0bu);
        crt_type_info_name(0x009ce4e4u, 0x009e1a7cu);
        loop_pop(s);
    }
    {   /* push 8; push the payload; EDI = the name, EAX = the dispatcher; call 004020c0 */
        const uint32_t a[2] = {E - 0xcu, 8u};
        const rx_addr s = loop_push(2, a, 0x00554d1eu);
        rx_world_event_dispatch(dispatcher, 0x009416b0u, E - 0xcu, 8u);
        loop_pop(s);
    }
    rx_set_sp(E);
}
RX_FUNCTION(0x00554ce0, rx_player_camera_changed)

/* 0054aef0 Player_ApplyDrivingCamera (thiscall player; RET0).
 * C return exposes original live ECX: player on the negative-controller early
 * return, otherwise the camera entity index passed to Player_SetCamera (which
 * preserves ECX on both success and refusal). UI preset0/1/2/3 maps0/1/4/2;
 * an unrecognized preset retains the selected entity. */
uint32_t rx_player_apply_driving_camera(rx_addr p) {
    const rx_addr entry = rx_sp();
    rx_set_sp(pl_frame(entry, 8, 0, 2));
    const int32_t controller = I32(p, 0x1f0);
    if (controller < 0) { rx_set_sp(entry); return p; }
    const rx_addr settings = U32(0, 0x00d92efc);
    uint32_t camera = U32(p, 0x988);
    /* The original's primary-device special case can only select the no-device
     * row for controller==-1, already excluded by the signed gate above. */
    const uint32_t preset = U32(settings, 4u + 0x48u * (uint32_t)controller);
    switch (preset) {
    case 0: camera = 0; break;
    case 1: camera = 1; break;
    case 2: camera = 4; break;
    case 3: camera = 2; break;
    default: break;
    }
    uint32_t selected;
    PL_AT(0, 0, 0x0054af63u, selected = rx_player_set_camera(p, camera));
    if (!selected) SU32(0, 0x009c5410u + 4u * U32(p, 0x1f0), camera);
    rx_set_sp(entry);
    return camera;
}
RX_FUNCTION(0x0054aef0, rx_player_apply_driving_camera)

/* 0054c9b0 Player_SetCamera (EAX = player @ p, ECX = camera @ the argument; RET 0): the Camera key's (and the UI's) switch.
 * Only for a human local player with a vehicle, in an activity in state 1..3 (Countdown, InProgress, WindDown); else 0.
 * Resets the old view entity's blend offsets (P+0x974 + 4 * old, +0x350..+0x368) when its +0x920 + 0x14 * old flag is clear,
 * writes P+0x988, clears the replay director's controller (world +0x21de0) and updates the replay cameras with 0.033 s, runs the
 * new director's vf29 (slot 0x74) and clears the 0xa9434f flag. Then the per-controller camera index [0x9c5410 + 4 * ctl], the
 * UISettings DrivingCamera row (+0x48 * ctl + 4: 0 / 1 / 3 / 2 for camera 0 / 1 / 2 / 4) and UISettings_Apply when it changed,
 * and finally the "Camera Changed" event {P + 0x910 + 20 * camera, player} on the activity's event system when the camera moved.
 * Frame: push ecx; push esi; push edi (no locals): every call runs at entry - 0xc. */
uint32_t rx_player_set_camera(rx_addr p, uint32_t camera) {
    const rx_addr E = rx_sp(), S = E - 0xcu;
    rx_set_sp(S);
    const rx_addr act = U32(p, 0x94);
    if (U32(p, 4) == 0 || U32(p, 0x10) != 0 || PL_VEHICLE(p) == 0 || U8(p, 0x21c) != 0 || act == 0 ||
        (U32(act, 0x21c) != 1 && U32(act, 0x21c) != 2 && U32(act, 0x21c) != 3)) {
        rx_set_sp(E);
        return 0;
    }
    const uint32_t old = U32(p, 0x988);
    const int moved = old != camera;
    if (U8(p, 0x920u + 0x14u * old) == 0) {        /* the old view entity's blend / follow offsets */
        const rx_addr ent = U32(p, 0x974u + 4u * old);
        static const uint16_t off[7] = {0x350, 0x354, 0x358, 0x35c, 0x360, 0x364, 0x368};
        for (int i = 0; i < 7; i++) SF32(ent, off[i], 0.0f);
    }
    SU32(p, 0x988, camera);
    if (camera != 5) {
        const rx_addr w = U32(act, 0x234);
        if (w) {
            SU32(w, 0x21de0, 0);                    /* the replay camera controller: back to the selected player */
            { const uint32_t a[2] = {w, U32(0, 0x00977818u)};   /* 0.0329999998 s */
              PL_AT(2, a, 0x0054ca9du, rx_replay_cameras_update(w, rx_bf(a[1]))); }
            const rx_addr w2 = U32(U32(p, 0x94), 0x234);
            const rx_addr dir = U32(w2, 0x21dfcu);
            if (dir) loop_tc0(loop_vslot(dir, 0x74), dir);      /* the new replay director's vf29 */
            SU8(0, 0x00a9434fu, 0);
        }
    }
    const uint32_t ctl = U32(p, 0x1f0);
    SU32(0, 0x009c5410u + 4u * ctl, U32(p, 0x988));         /* the per-controller camera index */
    if ((int32_t)ctl >= 0) {
        uint32_t driving;
        switch (U32(0, 0x009c5410u + 4u * ctl)) {
        case 0: driving = 0; break;
        case 1: driving = 1; break;
        case 2: driving = 3; break;               /* first person */
        case 4: driving = 2; break;
        default: driving = 0xffffffffu; break;    /* camera 3 / 5: no DrivingCamera row */
        }
        if (driving != 0xffffffffu) {
            const rx_addr settings = U32(0, 0x00d92efcu);
            const uint32_t idx = ctl;             /* the original's if (+0x16c == ctl) keeps the same value */
            const rx_addr row = settings + 0x48u * idx + 4u;
            if (U32(row, 0) != driving) {
                SU32(row, 0, driving);
                const uint32_t a[2] = {settings, ctl};
                PL_AT(2, a, 0x0054cbb6u, rx_uisettings_apply(settings, (int32_t)ctl));
            }
        }
    }
    if (U32(p, 0x94) != 0 && moved) {
        const rx_addr a2 = U32(p, 0x94);
        const uint32_t arg = U32(p, 0x910u + 20u * U32(p, 0x988));
        const uint32_t a[1] = {p};
        const rx_addr s = loop_push(1, a, 0x0054cbdfu);
        rx_player_camera_changed(arg, U32(a2, 0x290), p);
        loop_pop(s);
    }
    rx_set_sp(E);
    return 1;
}
RX_FUNCTION(0x0054c9b0, rx_player_set_camera)

/* 0063fef0 (stdcall camera, dt): the gameplay camera's update after its target (+0x340) was set to the vehicle position */
static void pl_gameplay_camera_update(rx_addr cam, float dt) {
    const uint32_t a[2] = {cam, pl_x87(rx_fb(dt))};
    /* BOUNDARY (not reached): the gameplay camera's own update. P+0x220 (the gameplay camera pointer these branches compare with) is 0 in the product (read at race frame 1200) and the original's reference world executes 0 of the 5002 bytes in 1700 frames; its only stores are MXPlayer init (0) and the camera teardown 0054bc00. Fatal if reached. */
    pl_call(0x0063fef0, (pl_in){.ecx = cam}, 2, a);
}

/* 0054c150 Player_UpdateCamera (stdcall p, dt; RET 8): picks the camera entity (the active one for the driver's state, the start
 * gate camera before the race), cycles it on the camera key, switches entities (notifying the old one, 0x74 / 0x70), keeps the
 * gameplay camera on the vehicle and updates the chosen one. Frame: sub esp, 0xc; push ebx, ebp, esi, edi (ESP = entry - 0x1c):
 * locals entry - 8 (blend flag), entry - 4 (the camera entity at entry); the blend scale is kept in the p argument slot. */
void rx_player_update_camera(rx_addr p, float dt) {
    const rx_addr E = rx_sp(), S = E - 0x1c;
    rx_set_sp(S);
    rx_addr b = PL_DRIVER(p);
    if (b != 0) {
        if (loop_tc0(loop_vslot(b, 0x20), b) != 0 || U32(U32(PL_DRIVER(p), 0x234), 0x20014) != 0) {
            if (U8(p, 0x21c) == 0) goto done;
        }
    }
    b = PL_DRIVER(p);
    if (b != 0) {
        if (U32(b, 0xfe8) == 0 && U8(p, 0x21c) == 0 && U32(p, 0x230) == 0 && U32(b, 0x21c) == 1 && U32(b, 0x238) == 1) {
            const uint32_t mode = U32(b, 0x23c);
            if (mode == 0 || mode == 5 || mode == 1) {
                const rx_addr gate = U32(p, 0x99c);
                if (gate != 0 && U32(gate, 0x2f4) == 0) SU32(p, 0x9a0, 1);
            }
        }
        if (U32(b, 0x21c) == 4) {                       /* driver state 4: only the gameplay camera runs */
            const rx_addr cam = U32(p, 0x220);
            if (pl_active_camera(p, 0x0054c210) == cam) {
                const rx_addr v = PL_VEHICLE(p);
                if (v != 0) {
                    pl_x87_copy(cam + 0x340, v + 0xc0);
                    pl_x87_copy(cam + 0x344, v + 0xc4);
                    pl_x87_copy(cam + 0x348, v + 0xc8);
                }
                pl_gameplay_camera_update(U32(p, 0x220), rx_bf(U32(E, 8)));
            }
            const rx_addr c980 = U32(p, 0x980);
            if (U32(c980, 0x2f4) == 0) loop_tc1(loop_vslot(c980, 0x70), c980, 0);
            goto done;
        }
    }

    if (U32(p, 4) != 0 && U32(p, 0x10) == 0 && PL_VEHICLE(p) != 0) {
        /* the camera key cycles the selected camera (+0x988: 0 1 2 4) while racing */
        if (U8(p, 0x21c) == 0) {
            const uint32_t slot = U32(p, 0x1f0);
            if (pl_keymap_query(pl_input(slot) + 0xf34, 4, slot) != 0 && U32(U32(U32(0, 0x00a946b8), 0x745c), 0x288) == 0 &&
                U32(PL_VEHICLE(p), 0x280) == 0) {
                const uint32_t st = U32(PL_DRIVER(p), 0x21c);
                if (st == 1 || st == 2 || st == 3) {
                    int32_t idx = I32(p, 0x988) + 1;
                    if (idx >= 5) idx = 0;
                    else if (idx == 3) idx = 4;
                    PL_AT(0, 0, 0x0054c32d, rx_player_set_camera(p, (uint32_t)idx));   /* Player_SetCamera */
                }
            }
        }
        if (PL_VEHICLE(p) != 0) {
            /* the follow camera 2 gives way to 0 while airborne / slow / inside a cannon, and the driving camera comes back */
            const rx_addr drv = PL_DRIVER(p);
            uint32_t inside = 0;
            const rx_addr sel = pl_player_driver(p, drv, 0, 0x0054c348);
            /* PORT ADDITION (free_ride_borders.h): without the Free Ride's boundary its cannonball band is plain ground for the camera too */
            if (pl_rule7(sel, (pl_in){.edi = drv, .ebp = p}, 0x0054c34e) != 0 && !rx_frb_for(p)) {
                __typeof__(&rx_cannonball_inside) f = PL_CORE(0x00519fa0, rx_cannonball_inside);
                const rx_addr pos = PL_VEHICLE(p) + 0xc0;
                uint32_t r;
                if (f) PL_AT(0, 0, 0x0054c364, r = (uint32_t)f(pos, 0));
                else r = pl_call0(0x00519fa0, (pl_in){.eax = pos, .edi = 0, .ebp = p});
                inside = r != 0;
            }
            const rx_addr v = PL_VEHICLE(p);
            if (U32(v, 0x280) != 0 || F32(v, 0x131c) > F32(p, 0x990) || inside != 0) {
                if (U32(p, 0x988) == 2) {
                    SU32(p, 0x988, 0);
                    SU8(p, 0x98c, 1);
                }
            }
            if (U32(v, 0x280) == 0 && F32(p, 0x990) > F32(v, 0x131c) && inside == 0 && U8(p, 0x98c) != 0) {
                PL_AT(0, 0, 0x0054c3d1u, (void)rx_player_apply_driving_camera(p));
                SU8(p, 0x98c, 0);
            }
        }
        SU32(p, 0x994, 0xffffffffu);
        const rx_addr drv = PL_DRIVER(p);
        if (U32(U32(U32(0, 0x00a946b8), 0x745c), 0x288) == 0 && drv != 0 && U32(drv, 0x21c) == 2) {
            /* the look keys (+0x418c / +0x438c / +0x4630, vf2) pick the look direction +0x994 */
            uint32_t slot = U32(p, 0x1f0);
            uint32_t cur;
            if (pl_keymap_query(pl_input(slot) + 0x418c, 8, slot) != 0 && (cur = U32(p, 0x988)) != 2 && cur != 4) {
                SU32(p, 0x994, 0);
            } else {
                slot = U32(p, 0x1f0);
                if (pl_keymap_query(pl_input(slot) + 0x438c, 8, slot) != 0 && (cur = U32(p, 0x988)) != 2 && cur != 4) {
                    SU32(p, 0x994, 1);
                } else {
                    slot = U32(p, 0x1f0);
                    if (pl_keymap_query(pl_input(slot) + 0x4630, 8, slot) != 0 && U32(p, 0x988) != 4) SU32(p, 0x994, 2);
                }
            }
        }
    }

    /* debug camera hot keys (the 0x009b716d lock off): KeyMap::vf2 (7, 0) of four maps pick the camera index 0x00a7a994 */
    {
        static const uint32_t km[4] = {0x00d5c038, 0x00d5cf50, 0x00d5c380, 0x00d5eba0};
        for (uint32_t i = 0; i < 4 && U8(0, 0x009b716d) == 0; i++)
            if (loop_tc2(0x008115f0, km[i], 7, 0) != 0) SU32(0, 0x00a7a994, i);
    }

    /* choose the camera entity */
    {
        rx_addr cam = pl_active_camera(p, 0x0054c55b);
        if (U32(p, 0x9a0) != 0) {
            cam = U32(p, 0x99c);
            SU32(p, 0x9a0, 0);
        }
        const rx_addr old = U32(p, 0x9a8);
        SU32(E, -4, old);
        SU32(E, -8, 0);
        SF32(E, 4, 0.0f);
        if (cam == 0) goto done;
        if (old == 0) {
            SU32(p, 0x9a8, cam);
            if (cam == U32(p, 0x220) && I32(p, 0x1f0) >= 0)
                pl_groups_on(p, 1, (pl_group_rets){0x0054c5d3, 0x0054c5e6, 0x0054c64f, 0x0054c670, 0x0054c681, 0x0054c6ea});
        }
        const rx_addr gate = U32(p, 0x99c);
        if (cam == gate && U32(gate, 0x2f4) != 0 && U32(gate, 0x398) == 0 && U32(PL_DRIVER(p), 0x21c) != 1) {
            loop_tc0(loop_vslot(gate, 0x74), gate);
            cam = pl_active_camera(p, 0x0054c842);
            SU32(p, 0x9a8, U32(p, 0x99c));
        }
        rx_addr cur = U32(p, 0x9a8);
        if (cur != cam) {
            if (loop_tc0(loop_vslot(cur, 0x58), cur) == 0x100d) {          /* leaving a blendable camera */
                SU32(E, -8, 1);
                SF32(E, 4, 1.0f /* 0x931f24 */);
                if (U32(PL_DRIVER(p), 0x23c) == 5) SF32(E, 4, 0.5f /* 0x9322cc */);
            }
            cur = U32(p, 0x9a8);
            if (cur != 0) loop_tc0(loop_vslot(cur, 0x74), cur);
            SU32(p, 0x9a8, cam);
        }
        if (U32(cam, 0x2f4) == 0 && PL_RIDER(p) != 0) {
            loop_tc1(loop_vslot(cam, 0x70), cam, U32(E, -4));
            if (U32(E, -8) != 0) {
                cur = U32(p, 0x9a8);
                if (loop_tc0(loop_vslot(cur, 0x58), cur) == 0x1003) {
                    const uint32_t a[1] = {pl_x87(U32(E, 4))};
                    __typeof__(&rx_cam_snap) f = PL_CORE(0x00649df0, rx_cam_snap);
                    if (f) PL_AT(1, a, 0x0054c900, f(cam, U32(E, -4), rx_bf(a[0])));
                    else pl_call(0x00649df0, (pl_in){.edi = U32(E, -4), .esi = cam, .ebx = cam, .ebp = p}, 1, a);
                }
            }
        }
        const rx_addr gcam = U32(p, 0x220);
        if (cam == gcam) {
            const rx_addr v = PL_VEHICLE(p);
            pl_x87_copy(gcam + 0x340, v + 0xc0);
            pl_x87_copy(gcam + 0x344, v + 0xc4);
            pl_x87_copy(gcam + 0x348, v + 0xc8);
            pl_gameplay_camera_update(U32(p, 0x220), rx_bf(U32(E, 8)));
        } else if (U32(p, 4) != 0 && U32(p, 0x10) == 0 && U32(p, 0x988) == 4) {
            const float t = rx_bf(pl_x87(U32(E, 8)));
            loop_tc_uf(loop_vslot(cam, 0x78), cam, U32(p, 0x1f0), t);
            const rx_addr c984 = U32(p, 0x984);
            loop_tc_uf(loop_vslot(c984, 0x78), c984, U32(p, 0x1f0), t);
        }
    }
done:
    rx_set_sp(E);
    (void)dt;
}

/* 0054cc10 Player_UpdateCameras (EAX = p; stack dt; RET 4): vf19-style update (slot 0x4c, dt) of the gameplay camera, the five
 * entity slots, the quick and the start-gate camera; then the active entity gets the vehicle's view entity (0063dba0, the view
 * index +8 into the level's list) or its own position (slot 0x44). Frame: push ebp; and esp, -16; sub esp, 0x14; 3 pushes. */
void rx_player_update_cameras(rx_addr p, float dt) {
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x14, 3);
    rx_set_sp(S);
    const float t = rx_bf(pl_x87(rx_fb(dt)));
    rx_addr c = U32(p, 0x220);
    if (c) loop_tc_f(loop_vslot(c, 0x4c), c, t);
    for (uint32_t i = 0; i < 5; i++) {
        c = U32(p, 0x974 + 4 * i);
        if (c) loop_tc_f(loop_vslot(c, 0x4c), c, t);
    }
    if ((c = U32(p, 0x998)) != 0) loop_tc_f(loop_vslot(c, 0x4c), c, t);
    if ((c = U32(p, 0x99c)) != 0) loop_tc_f(loop_vslot(c, 0x4c), c, t);
    const rx_addr cam = pl_active_camera(p, 0x0054cca1);
    const rx_addr v = PL_VEHICLE(p);
    if (cam != 0 && v != 0) {
        const int32_t idx = I32(p, 8);
        rx_addr h = U32(v, 0x188);
        if (idx >= 0 && idx < I32(h ? U32(h, 0x20138) : 0, 0x19d0)) {
            h = U32(v, 0x188);
            const rx_addr e = U32(h ? U32(h, 0x20138) : 0, (uint32_t)idx * 4u + 0x19b8);
            if (e != 0) {
                __typeof__(&rx_cam_entity_to_view) f = PL_CORE(0x0063dba0, rx_cam_entity_to_view);
                const uint32_t a[2] = {cam, e};
                if (f) PL_AT(2, a, 0x0054ccf8, f(cam, e));
                else pl_call(0x0063dba0, (pl_in){.edi = (uint32_t)idx, .ebx = 0}, 2, a);
            }
        } else {
            loop_tc1(loop_vslot(cam, 0x44), cam, S + 0x10);
        }
    }
    rx_set_sp(sp0);
}

/* ---- idle-rider fidget (0054cec0) ---- */

/* the SSE normalize of 0054cfb2..0054d016: v / |v| (|v| from (x^2 + y^2) + z^2) masked to 0 where |v| is 0, and the quiet NaN
 * 0x92dc30 in every lane when the squared length is +inf (cmpneqps with 0x92dc20: also true for a NaN length) */
static rx_v4 pl_normalize4(rx_v4 v) {
    const float l = rx_adds(rx_adds(rx_muls(v.v[0], v.v[0]), rx_muls(v.v[1], v.v[1])), rx_muls(v.v[2], v.v[2]));
    const float len = rx_sqrts(l);
    rx_v4 r;
    for (int i = 0; i < 4; i++) {
        uint32_t q = rx_fb(rx_divs(v.v[i], len));
        if (!(len != 0.0f)) q = 0;
        r.v[i] = rx_bf(l != rx_bf(0x7f800000u) ? q : 0x7fc00000u);
    }
    return r;
}

/* 0054cec0 (EDI = p; stack dt; RET 4): the rider fidget: while the vehicle's entry in the 0xa96dc0 table has a stored speed >= 0.6,
 * the rider's clip is the current one and the vehicle is on the ground (slot 0xac), the idle clock +0x10b8 counts; after 30 s and
 * 0.1 s upright (+0x10b0) one try (+0x10b4) of the fidget: the rider's up axis (slot 0x94, normalized) y > 0.3 and the ground
 * normal (00508460) y above the .data limit 0x009cd99c, then the inline game RNG (0x9cb860) picks it (0061ea90, ESI = rider).
 * Frame: push ebp; and esp, -16; sub esp, 0x3c (cookie at +0x38); push esi: +0x10 the mask / normal, +0x20 the axis. */
void rx_player_update_idle_rider(rx_addr p, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, S = A - 0x40;
    SU32(A - 0x3c, 0x38, U32(0, 0x009b45e4) ^ (A - 0x3c));
    rx_set_sp(S);
    const rx_addr v = PL_VEHICLE(p);
    float speed = 0.0f;
    {
        uint32_t idx = 0;
        for (rx_addr e = 0x00a96dc0; (int32_t)e < (int32_t)0x00a970c0; e += 0x40, idx++)
            if (U32(e, 0) == v) { speed = F32(0x00a96d90u + (idx << 6), 0); break; }
        if (rx_mud_side_of(v)) speed = F32(rx_mud_side_of(v), 0);   /* (w-rc-core) records 12..31 */
    }
    const rx_addr r = PL_RIDER(p);
    const rx_addr clip = U32(r, 0x580);
    const uint32_t cur = clip ? U16(clip, 0) : 0;
    if (U16(r, 0x5a0) != (cur & 0xffffu) || !(speed >= 0.6f /* 0x931da8 */)) goto done;
    SF32(p, 0x10b8, dt + F32(p, 0x10b8));
    if (loop_tc0(loop_vslot(v, 0xac), v) == 0 || 30.0f /* 0x931d98 */ > F32(p, 0x10b8)) {
        SF32(p, 0x10b0, 0.0f);
        SU32(p, 0x10b4, 0);
        goto done;
    }
    if (U32(p, 0x10b4) != 0) goto done;
    {
        const float up = F32(p, 0x10b0) + dt;
        SF32(p, 0x10b0, up);
        if (!(up > 0.1f /* 0x931d94 */)) goto done;
    }
    {
        const rx_addr v2 = PL_VEHICLE(p);
        loop_tc1(loop_vslot(v2, 0x94), v2, S + 0x20);
        const rx_v4 n = pl_normalize4(rx_ld4(S + 0x20));
        rx_st4(S + 0x20, n);
        /* dot with (0, 1, 0, 0): (0*x + 1*y) + 0*z, splatted; cmpltps 0.3 < dot in every lane (mask at +0x10) */
        const float dot = rx_adds(rx_adds(rx_muls(0.0f, n.v[0]), rx_muls(1.0f, n.v[1])), rx_muls(0.0f, n.v[2]));
        const uint32_t m = (0.3f /* 0x931da4 */ < dot) ? 0xffffffffu : 0;
        for (uint32_t i = 0; i < 16; i += 4) SU32(S + 0x10, i, m);
        if (m != 0) {
            __typeof__(&rx_world_body_up) up = PL_CORE(0x00508460, rx_world_body_up);
            if (up) PL_AT(0, 0, 0x0054d078, up(U32(p, 0x1ac), S + 0x10));
            else pl_call0(0x00508460, (pl_in){.eax = S + 0x10, .ecx = U32(p, 0x1ac), .edx = 0xfffffffdu, .edi = p});
            float y = rx_muls(F32(S, 0x10), 0.0f);
            y = rx_adds(y, F32(S, 0x14));
            y = rx_adds(y, rx_muls(F32(S, 0x18), 0.0f));
            if (y > F32(0, 0x009cd99c)) {
                SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
                const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
                SU32(0, 0x009cb860, seed);
                if (seed & 0x10000u) {
                    __typeof__(&rx_world_rider_start_idle_gesture) fg = PL_CORE(0x0061ea90, rx_world_rider_start_idle_gesture);
                    if (fg) PL_AT(0, 0, 0x0054d0cf, fg(PL_RIDER(p)));
                    else pl_call0(0x0061ea90, (pl_in){.esi = PL_RIDER(p), .edi = p, .eax = seed});
                }
                SF32(p, 0x10b8, 0.0f);
            }
        }
        SU32(p, 0x10b4, 1);
    }
done:
    rx_set_sp(sp0);
}

/* ---- reset input / reset point / wreck recovery ---- */

/* 0054e4f0 Player_HandleResetInput (ECX = p): the respawn key (KeyMap +0x9ec) or the recover key (+0x167c, which also sets
 * +0x1138) of a controller that is not paused or already resetting (+0x1134 armed) queues the reset (00550400 after 0054e470 agrees,
 * mode not 2); in activity mode 8 the "save spot" key (+0x147c) stores the vehicle's reset point (+0x200 / +0x210 -> +0x1140 /
 * +0x1150) and hands it to the activity's spot marker (+0x1143e0, slot 0x70). Frame: push ebp; and esp, -16; sub esp, 0x14;
 * 3 pushes: +0x0c the pressed flag, +0x10 the spot vector. */
void rx_player_handle_reset_input(rx_addr p) {
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x14, 3);
    rx_set_sp(S);
    SU32(S, 0xc, 0);
    int32_t slot = I32(p, 0x1f0);
    if (slot > -1 && U32(PL_DRIVER(p), 0xfe8) == 0 && U8(p, 0x21c) == 0 && U32(p, 0x1134) != 0) {
        if (pl_keymap_query(pl_input((uint32_t)slot) + 0x9ec, 4, (uint32_t)slot) != 0) SU32(S, 0xc, 1);
        slot = I32(p, 0x1f0);
        if (pl_keymap_query(pl_input((uint32_t)slot) + 0x167c, 4, (uint32_t)slot) != 0) {
            SU32(S, 0xc, 1);
            SU32(p, 0x1138, 1);
        }
        const rx_addr drv = PL_DRIVER(p);
        if (drv != 0 && U32(drv, 0x23c) == 8) {
            slot = I32(p, 0x1f0);
            if (pl_keymap_query(pl_input((uint32_t)slot) + 0x147c, 4, (uint32_t)slot) != 0) {
                uint32_t ghost;
                __typeof__(&rx_player_has_ghost) f = PL_CORE(0x00551070, rx_player_has_ghost);
                if (f) PL_AT(0, 0, 0x0054e5e0, ghost = f(p));
                else ghost = pl_call0(0x00551070, (pl_in){.ecx = p, .esi = p, .ebx = 1});
                if (ghost == 0) {
                    rx_st4(p + 0x1140, rx_ld4(PL_VEHICLE(p) + 0x200));
                    rx_st4(p + 0x1150, rx_ld4(PL_VEHICLE(p) + 0x210));
                    const rx_addr d = PL_DRIVER(p);
                    const rx_v4 pos = rx_ld4(p + 0x1140);
                    SU32(p, 0x1130, 1);
                    rx_st4(S + 0x10, pos);
                    const rx_addr obj = d + 0x1143e0;
                    loop_tc1(loop_vslot(obj, 0x70), obj, S + 0x10);
                    SU8(d, 0x114482, 1);
                }
            }
        }
        if (U32(S, 0xc) != 0) {
            uint32_t allowed;
            __typeof__(&rx_player_reset_input_allowed) gate = PL_CORE(0x0054e470, rx_player_reset_input_allowed);
            if (gate) PL_AT(0, 0, 0x0054e64a, allowed = gate(p));
            else allowed = pl_call0(0x0054e470, (pl_in){.edx = p, .esi = p, .ebx = 1});
            if (allowed != 0 && U32(0, G_MODE) != 2) {
                __typeof__(&rx_player_reset_manual) reset = PL_CORE(0x00550400, rx_player_reset_manual);
                if (reset) PL_AT(0, 0, 0x0054e65c, reset(p));
                else pl_call0(0x00550400, (pl_in){.esi = p, .ebx = 1});
            }
        }
    }
    rx_set_sp(sp0);
}

/* 0054ea30 Player_UpdateWreckRecovery (EAX = p; stack dt; RET 4): while a wreck is pending (+0xb38 > 0) for a local player outside
 * replays: the recovery stick (0054ee80 -> two axes at +0x10) must move past the previous value (+0xb48 / +0xb4c) in the direction
 * the mode +0xb44 asks (byte table 0x54ec28: 1 4 7 10 cancel, 2 3 y rising, 5 6 y falling, 8 9 x falling, 11 12 x rising) for an
 * accumulated 0.25 (+0xb40): then the recovery (0054ec80); else the clock +0xb38 runs down to the fall (0054ed10) or the cancel
 * (0054ec40). Frame: sub esp, 0xc; push ebx / esi / edi: the stick at +0x10. */
void rx_player_update_wreck_recovery(rx_addr p, float dt) {
    if (U32(p, 4) == 0 || PL_VEHICLE(p) == 0 || U32(0, G_MODE) == 2) return;
    if (!(F32(p, 0xb38) > 0.0f /* 0x92e12c */)) return;
    const rx_addr sp0 = rx_sp(), S = sp0 - 0x18;
    rx_set_sp(S);
    {
        __typeof__(&rx_world_rider_wreck_input) f = PL_CORE(0x0054ee80, rx_world_rider_wreck_input);
        const uint32_t a[1] = {S + 0x10};
        if (f) PL_AT(1, a, 0x0054ea81, f(p, S + 0x10));
        else pl_call(0x0054ee80, (pl_in){.eax = p, .edi = p}, 1, a);
    }
    const uint32_t idx = U32(p, 0xb44) - 1u;
    const float x = F32(S, 0x10), y = F32(S, 0x14), zero = 0.0f;
    uint32_t hit = 0, cancel = 0;
    float acc = 0.0f;
    int have = 0;
    if (idx <= 11u) {
        static const uint8_t kind[12] = {0, 1, 1, 0, 2, 2, 0, 3, 3, 0, 4, 4};
        switch (kind[idx]) {
        case 0: cancel = 1; break;
        case 1:          /* modes 2, 3: y rising */
            if (y > zero && y > F32(p, 0xb4c)) { acc = y - F32(p, 0xb4c); have = 1; }
            else if (F32(p, 0xb4c) > y) SF32(p, 0xb40, zero);
            break;
        case 2:          /* modes 5, 6: y falling */
            if (zero > y && F32(p, 0xb4c) > y) { acc = F32(p, 0xb4c) - y; have = 1; }
            else if (y > F32(p, 0xb4c)) SF32(p, 0xb40, zero);
            break;
        case 3:          /* modes 8, 9: x falling */
            if (zero > x && F32(p, 0xb48) > x) { acc = F32(p, 0xb48) - x; have = 1; }
            else if (x > F32(p, 0xb48)) SF32(p, 0xb40, zero);
            break;
        default:         /* modes 11, 12: x rising */
            if (x > zero && x > F32(p, 0xb48)) { acc = x - F32(p, 0xb48); have = 1; }
            else if (F32(p, 0xb48) > x) SF32(p, 0xb40, zero);
            break;
        }
        if (have) {
            acc = acc + F32(p, 0xb40);
            SF32(p, 0xb40, acc);
            if (acc > 0.25f /* 0x92ff84 */) hit = 1;
        }
    }
    SF32(p, 0xb48, x);
    SF32(p, 0xb4c, y);
    if (hit == 0 && U32(p, 0x1ec) == 0) {
        const float t = F32(p, 0xb38) - dt;
        SF32(p, 0xb38, t);
        if (!(zero < t || t != t)) {
            if (cancel == 0) {
                __typeof__(&rx_world_rider_fall) f = PL_CORE(0x0054ed10, rx_world_rider_fall);
                if (f) PL_AT(0, 0, 0x0054ebeb, f(p, 0));
                else pl_call0(0x0054ed10, (pl_in){.edi = p, .esi = 0, .ebx = 0});
            } else {
                PL_AT(0, 0, 0x0054ebfb, rx_player_cancel_wreck(p));
            }
        }
    } else {
        const uint32_t a[1] = {p};
        PL_AT(1, a, 0x0054ec0a, rx_player_wreck_recovery_success(p));   /* 0054ec80 */
    }
    rx_set_sp(sp0);
}

/* 0054ec80 WreckRecoverySuccess (stack p; RET 4): the wreck fields cleared (0054e890), the rider animation stopped (00621d70), the
 * recovery logged in the clip ring (006ab9b0, EAX = p), for an active local player whose pad slot is not -1 the highlight progress
 * of the driver's mode (006d83d0: EAX = mode, ESI = slot), and the event 0x00941260 raised for the vehicle (00554da0: sender
 * vehicle, dispatcher driver +0x290, argument the driver); the player's recovery count (+0x11d0) incremented.
 * Frame: push ecx / ebx / esi / edi (0x10). */
void rx_player_wreck_recovery_success(rx_addr p) {
    const rx_addr sp0 = rx_sp(), S = sp0 - 0x10;
    rx_set_sp(S);
    pl_clear_wreck(p, 0x0054ec8f);
    const rx_addr r = PL_RIDER(p);
    if (r) pl_rider_anim_stop(r, 0x0054ec9f);
    {
        __typeof__(&rx_world_clip_recorder_log_recover) f = PL_CORE(0x006ab9b0, rx_world_clip_recorder_log_recover);
        if (f) PL_AT(0, 0, 0x0054eca6, f(p));
        else pl_call0(0x006ab9b0, (pl_in){.eax = p});
    }
    if (U32(p, 4) != 0 && U32(p, 0x10) == 0) {
        const int32_t slot = I32(p, 0x1f0);
        if (slot >= 0) {
            const uint32_t mode = U32(PL_DRIVER(p), 0x23c);
            __typeof__(&rx_fx_highlight_recover) f = PL_CORE(0x006d83d0, rx_fx_highlight_recover);
            if (f) PL_AT(0, 0, 0x0054ecd3, f(mode, (uint32_t)slot));
            else pl_call0(0x006d83d0, (pl_in){.eax = mode, .esi = (uint32_t)slot, .edi = U32(0, 0x00d92edc)});
        }
    }
    const rx_addr v = PL_VEHICLE(p);
    if (v) {
        const rx_addr b = PL_DRIVER(p);
        pl_raise_event(v, 0x00941260, U32(b, 0x290), b, 0x0054ecf4);
    }
    SU32(p, 0x11d0, U32(p, 0x11d0) + 1);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0054ec80, rx_player_wreck_recovery_success)

/* 0054eee0 Player_UpdateResetPoint (ECX = p; stack dt; RET 4): while the vehicle finds a ground spot (slot 0x88 (out position,
 * out normal, .data 0x009ca20c, 30.0)) where the player may reset (0054d7c0) and the ground is dry (00550f00), the spot becomes the
 * vehicle's reset point (+0x200 / +0x210, flag +0x180). Activity modes 8, 9, 11, 5 hold a candidate (+0x1110 / +0x1120, from the
 * first frame of the run) until it has been valid for the .data time 0x009d78b4 (+0x1104 the clock). Frame: push ebp; and esp, -16;
 * sub esp, 0x3c (security cookie at +0x38); push esi: +0x10 position, +0x20 normal. */
void rx_player_update_reset_point(rx_addr p, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, B = A - 0x3c, S = B - 4;
    SU32(B, 0x38, U32(0, 0x009b45e4) ^ B);
    rx_set_sp(S);
    rx_addr v = PL_VEHICLE(p);
    if (v != 0) {
        const rx_addr L1 = S + 0x10, L2 = S + 0x20, b = PL_DRIVER(p);
        const uint32_t mode = b ? U32(b, 0x23c) : 0;
        const int held = b != 0 && (mode == 8 || mode == 9 || mode == 0xb || mode == 5);
        const uint32_t args[4] = {L1, L2, pl_x87(U32(0, 0x009ca20c)), rx_fb(30.0f /* 0x931d98 */)};
        int ok;
        {   /* the vehicle's ground probe (vtable +0x88: this, out position, out normal, float, float) */
            const rx_addr fn = loop_vslot(v, 0x88);
            /* The ports' C types differ: the bike's 00466340 rx_moto_safe_point is (v, pos, normal, float); the four-wheel classes' (ATV
             * 00486100, buggy 004ac100, truck 00473030, UTV 004c6a60 rx_*_vehicle_safe_pose) take the threshold as uint32_t bits in the
             * 4th place and an unused 5th uint32_t. A float-typed call left the four-wheel threshold in an FP register (garbage as bits). */
            uint32_t (*f)(rx_addr, uint32_t, uint32_t, float, float) = NULL;
            uint32_t (*fw)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t) = NULL;
            if (fn == 0x00466340u) f = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t, float, float));
            else fw = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t));
            if (f) PL_AT(4, args, held ? 0x0054ef5b : 0x0054f057, ok = f(v, L1, L2, rx_bf(args[2]), rx_bf(args[3])) != 0);
            else if (fw) PL_AT(4, args, held ? 0x0054ef5b : 0x0054f057, ok = fw(v, L1, L2, args[2], args[3]) != 0);
            else ok = rx_call_original(fn, RX_THISCALL, v, 4, args) != 0;
        }
        if (ok) PL_AT(0, 0, held ? 0x0054ef66 : 0x0054f062, ok = rx_player_can_reset(p) != 0);
        if (ok) {
            __typeof__(&rx_race_dry_ground) f = PL_CORE(0x00550f00, rx_race_dry_ground);
            if (f) PL_AT(0, 0, held ? 0x0054ef73 : 0x0054f06f, ok = f(L1) != 0);
            else ok = pl_call0(0x00550f00, (pl_in){.eax = L1, .esi = p}) != 0;
        }
        if (held) {
            if (ok) {
                if (F32(p, 0x1104) == 0.0f) {
                    rx_st4(p + 0x1110, rx_ld4(L1));
                    rx_st4(p + 0x1120, rx_ld4(L2));
                }
                SF32(p, 0x1104, F32(p, 0x1104) + dt);
            } else if (F32(p, 0x1104) > 0.0f) {
                SF32(p, 0x1104, 0.0f);
            }
            if (F32(p, 0x1104) >= F32(0, 0x009d78b4)) {
                v = PL_VEHICLE(p);
                rx_st4(v + 0x200, rx_ld4(p + 0x1110));
                rx_st4(v + 0x210, rx_ld4(p + 0x1120));
                SU32(v, 0x180, 1);
                SF32(p, 0x1104, 0.0f);
            }
        } else if (ok) {
            v = PL_VEHICLE(p);
            rx_st4(v + 0x200, rx_ld4(L1));
            rx_st4(v + 0x210, rx_ld4(L2));
            SU32(v, 0x180, 1);
        }
    }
    rx_set_sp(sp0);
}

/* ---- jump / distance tracker (embedded in the player at +0x11e0) ---- */

/* 004db070 (EAX = position vec3, ESI = tracker): the tracker leaves the air: the end point (+0x80) is stored; with an airtime
 * (+0x50) of 0.75 s and a horizontal distance > 20 from the take-off point (+0x70) the jump is recorded (+0x30 airtime, +0x00 start,
 * +0x10 end, +0x20 apex, count +0x44, new-jump flag +0xa0); the clocks +0x4c / +0x50 are cleared. Frame: push ebp; and esp, -16;
 * sub esp, 0x10 (the airtime at +0xc). */
void rx_player_tracker_land(rx_addr s, rx_addr pos) {
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x10, 0);
    rx_set_sp(S);
    pl_x87_copy(s + 0x80, pos);
    pl_x87_copy(s + 0x84, pos + 4);
    pl_x87_copy(s + 0x88, pos + 8);
    const float air = F32(s, 0x50);
    SF32(S, 0xc, air);
    if (air >= 0.75f /* 0x935aa8 */) {
        const float dz = F32(s, 0x88) - F32(s, 0x78);
        const float dx = F32(s, 0x80) - F32(s, 0x70);
        SF32(S, -4, dz * dz + dx * dx);                         /* SqrtF's pushed argument */
        if (rx_sqrts(dz * dz + dx * dx) > 20.0f /* 0x935aac */) {
            SF32(s, 0x30, F32(S, 0xc));
            for (uint32_t i = 0; i < 12; i += 4) pl_x87_copy(s + 0x00 + i, s + 0x70 + i);
            for (uint32_t i = 0; i < 12; i += 4) pl_x87_copy(s + 0x10 + i, s + 0x80 + i);
            for (uint32_t i = 0; i < 12; i += 4) pl_x87_copy(s + 0x20 + i, s + 0x90 + i);
            SU32(s, 0x44, U32(s, 0x44) + 1);
            SU32(s, 0xa0, 1);
        }
    }
    SF32(s, 0x50, 0.0f);
    SF32(s, 0x4c, 0.0f);
    rx_set_sp(sp0);
}

/* 004db160 (EAX = tracker, EDI = vehicle; stack dt; RET 4): per-frame jump tracking from the vehicle's airborne flag (slot 0xac)
 * and position (slot 0x44, at +0x10): +0x40 a jump in progress is cancelled on the ground; +0x48 the previous frame's flag: landing
 * (004db070), or the airtime and the apex (+0x90 .. +0x98) grow; on the ground: a take-off copies the last position (+0x60) to
 * +0x70 and the apex, else the ground clock +0x4c grows. Frame: push ebp; and esp, -16; sub esp, 0x18; push ebx, esi. */
void rx_player_tracker_update(rx_addr s, rx_addr v, float dt) {
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x18, 2), L = S + 0x10;
    rx_set_sp(S);
    SU32(s, 0xa0, 0);
    const uint32_t air = loop_tc0(loop_vslot(v, 0xac), v);
    loop_tc1(loop_vslot(v, 0x44), v, L);
    if (U32(s, 0x40) != 0) {
        if (air == 0) {
            SU32(s, 0x40, 0);
            SF32(s, 0x50, 0.0f);
            SF32(s, 0x4c, 0.0f);
        }
    } else if (U32(s, 0x48) != 0) {
        if (air == 0) {
            PL_AT(0, 0, 0x004db1c4, rx_player_tracker_land(s, L));
        } else {
            SF32(s, 0x50, F32(s, 0x50) + dt);
            const float y = F32(L, 4);
            if (y > F32(s, 0x94)) {
                SF32(s, 0x94, y);
                SU32(s, 0x90, U32(L, 0));
                SU32(s, 0x98, U32(L, 8));
            }
        }
    } else if (loop_tc0(loop_vslot(v, 0xac), v) != 0) {
        for (uint32_t i = 0; i < 12; i += 4) pl_x87_copy(s + 0x70 + i, s + 0x60 + i);
        SF32(s, 0x50, 0.0f);
        for (uint32_t i = 0; i < 12; i += 4) pl_x87_copy(s + 0x90 + i, s + 0x70 + i);
    } else {
        SF32(s, 0x4c, F32(s, 0x4c) + dt);
    }
    SU32(s, 0x60, U32(L, 0));
    SU32(s, 0x64, U32(L, 4));
    SU32(s, 0x68, U32(L, 8));
    SU32(s, 0x48, air);
    rx_set_sp(sp0);
}

/* ---- locks ---- */

/* 00883ac0 / 00883ad0 (ECX = lock object; its first dword is the CRITICAL_SECTION): Enter / LeaveCriticalSection */
void rx_lock_enter_raw(rx_addr lock) { loop_enter_cs(U32(lock, 0)); }
void rx_lock_leave_raw(rx_addr lock) { loop_leave_cs(U32(lock, 0)); }
/* 00883d80 / 00883d90: the same unless the engine runs without locking (byte 0x9df580); tail jumps */
void rx_lock_enter(rx_addr lock) { if (U8(0, 0x009df580) == 0) rx_lock_enter_raw(lock); }
void rx_lock_leave(rx_addr lock) { if (U8(0, 0x009df580) == 0) rx_lock_leave_raw(lock); }

/* ---- the profile / event record of the player (+0xb2c) ---- */

/* The map walk of MXPlayer_Update's two table broadcasts: for each node of the std::map `m` (the rows of the player's key) the
 * cell (row node +0xc, table node +0x14) of `column` gets *value (Table_SetCellInt). The checked-iterator tests of the original
 * (_invalid_parameter_noinfo) are kept. The iterator lives at S + 0xc (proxy) / S + 0x10 (node), the end node at S + 0x1c. */
static void pl_broadcast(rx_addr m, rx_addr S, rx_addr column, rx_addr value, rx_addr ret_set, rx_addr p) {
    SU32(S, 0x10, U32(U32(m, 0x18), 0));
    rx_addr proxy = U32(m, 0);
    SU32(S, 0xc, proxy);
    for (;;) {
        const rx_addr end = U32(m, 0x18);
        SU32(S, 0x1c, end);
        if (proxy == 0 || proxy != U32(m, 0)) pl_invalid_parameter();
        if (U32(S, 0x10) == U32(S, 0x1c)) break;
        rx_addr deref = 0;
        if (proxy == 0) pl_invalid_parameter();
        else deref = U32(proxy, 0);
        if (U32(S, 0x10) == U32(deref, 0x18)) pl_invalid_parameter();
        const rx_addr node = U32(S, 0x10);
        pl_set_cell_int(U32(node, 0x14), column, U32(node, 0xc), value, ret_set, (pl_in){.edi = m, .ebx = p});
        PL_AT(0, 0, ret_set + 9, rx_fx_rowset_next(S + 0xc));                       /* 00417df0 ++iterator (ESI = the iterator) */
        proxy = U32(S, 0xc);
    }
}

/* the record bookkeeping of MXPlayer_Update (0054b200 .. 0054b4f7), run under the record lock; S = the original's ESP */
static void pl_update_record(rx_addr p, rx_addr S) {
    if (U32(p, 0xb2c) == 0) {
        pl_profile_init(0x0054b21d);
        const rx_addr g = U32(0, 0x00da6cb0);
        if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1) {
            const uint32_t a[1] = {U32(pl_profile(0x0054b245), 0x10)};
            /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
            SU32(p, 0xb2c, pl_call(0x006e3030, (pl_in){.eax = U32(p, 0xc), .ebx = p, .esi = 1}, 1, a));
        }
    }
    rx_addr rec = U32(p, 0xb2c);
    if (rec == 0) return;
    {
        const uint32_t a[1] = {U32(rec, 0x14)};
        /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
        const uint32_t v = pl_call(0x006f6930, (pl_in){.ebx = p, .esi = 1}, 1, a);
        if (U32(p, 0x1e0) != v) {
            const uint32_t a2[1] = {U32(U32(p, 0xb2c), 0x14)};
            /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
            SU32(p, 0x1e0, pl_call(0x006f6930, (pl_in){.ebx = p, .esi = 1}, 1, a2));
            SU32(S, 0xc, (uint32_t)(int32_t)(int8_t)U8(U32(p, 0xb2c), 0x139));
            const uint32_t k[1] = {S + 0xc};
            /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
            const rx_addr m = pl_call(0x004199d0, (pl_in){.eax = 0x0093df0c, .ecx = S + 0xc, .esi = 0x00ae0e08, .ebx = p}, 1, k);
            pl_broadcast(m, S, 0x0093de38, p + 0x1e0, 0x0054b329, p);
        }
    }
    {
        const uint32_t a[1] = {U32(U32(p, 0xb2c), 0x14)};
        /* BOUNDARY (online only): the player's session record P+0xb2c (Quazal) is null in the product - act_session_player (build_activity_player.c) answers 0 without a session - so this is never reached offline. */
        SU32(p, 0x1e8, pl_call(0x006f6cd0, (pl_in){.ebx = p}, 1, a));
    }
    rec = U32(p, 0xb2c);
    SU32(p, 0x900, U32(rec, 0xd8));
    SU32(p, 0x904, U32(rec, 0xdc));
    SU32(p, 0x90c, U8(rec, 0x129) & 1);
    const uint32_t flag = (U8(rec, 0x128) >> 4) & 1;
    if (U32(p, 0x1e4) == flag) return;
    SU32(p, 0x1e4, flag);
    if (flag == 0) return;
    /* the player became "hidden": every row of the player's key in table 0x00ae0e08 gets the flag, the vehicle and rider hidden */
    const int32_t key = (int8_t)U8(rec, 0x139);
    const uint32_t col = pl_column_index(0x00ae0e08, 0x0093df0c, 0x0054b3c7, (pl_in){.ebx = p});
    SU32(S, 0xc, (uint32_t)key);
    if (!(col < (uint32_t)((int32_t)(U32(0, 0x00ae0e1c) - U32(0, 0x00ae0e18)) >> 2))) pl_invalid_parameter();
    const rx_addr colptr = U32(U32(0, 0x00ae0e18), col * 4u);
    rx_addr m;
    {
        __typeof__(&rx_tbl_rows) f = PL_CORE(0x00418e50, rx_tbl_rows);
        const uint32_t a[1] = {colptr};
        if (f) PL_AT(1, a, 0x0054b3f4, m = f(colptr, (int32_t)U32(S, 0xc)));
        else m = pl_call(0x00418e50, (pl_in){.eax = S + 0xc, .esi = col, .ebx = p}, 1, a);
    }
    pl_broadcast(m, S, 0x0093dee8, p + 0x1e4, 0x0054b45b, p);
    const rx_addr v = PL_VEHICLE(p);
    if (v != 0) {
        SU8(U32(v, 0x1328), 0x14e, 0);
        loop_tc1(loop_vslot(v, 0xc4), v, 0);
        const rx_addr h = U32(PL_VEHICLE(p), 0x17c);
        if (h != 0) {
            __typeof__(&phys_body_record) f = PL_CORE(0x0042bd90, phys_body_record);
            rx_addr obj;
            if (f) PL_AT(0, 0, 0x0054b4af, obj = f(h));
            else obj = pl_call0(0x0042bd90, (pl_in){.eax = h, .esi = 0, .ebx = p});
            SU32(U32(obj, 0x2c), 0x14, 0xffffffffu);
        }
    }
    const rx_addr r = PL_RIDER(p);
    if (r != 0) {
        SU32(r, 0x5bb5c, 0);
        SU8(r, 0xa2, 0);
        SU8(r, 0x5bb64, 0);
        SU32(r, 0x5b6d0, 0);
    }
    SU32(p, 0x9ac, 0);
}

/* 0054fff0 (EAX = p; stack flag; RET 4): reset the player's run state: the trick controls (006c9100), the respawn fade started
 * (unless flag != 0 with the driver racing and no fade running), the wreck fields, the jump / stunt clocks; recording (mode 1),
 * the reset is logged (ClipRecorder_LogReset with a record of the vehicle and the reset point); the camera flash (00550350); for a
 * local player the crash ends (00622e30), the input groups come back and the vehicle is placed at +0x1180 / +0x1190 (00460020).
 * Frame: push ebp; and esp, -16; sub esp, 0x58; push esi, edi: the log record at +0x10. */
void rx_player_reset(rx_addr p, uint32_t flag) {
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x58, 2);
    rx_set_sp(S);
    SF32(p, 0x116c, 0.0f);
    {
        __typeof__(&rx_controls_reset) f = PL_CORE(0x006c9100, rx_controls_reset);
        if (f) PL_AT(0, 0, 0x00550013, f(p + 0xb58));
        else pl_call0(0x006c9100, (pl_in){.eax = p + 0xb58, .esi = p});
    }
    int fade = flag == 0;
    if (!fade) {
        const rx_addr b = PL_DRIVER(p);
        fade = (b != 0 && (U32(b, 0x21c) == 2 || U32(b, 0x21c) == 3)) || U32(p, 0x11a0) != 0;
    }
    if (fade) PL_AT(0, 0, 0x00550041, rx_player_set_fade_out(p));
    SF32(p, 0x1168, 0.0f);
    pl_clear_wreck(p, 0x00550053);
    SU32(p, 0x1224, 0);
    SU32(p, 0x1280, 0);
    static const uint16_t zero_f[] = {0x122c, 0x1230, 0x1250, 0x1254, 0x1258, 0x1260, 0x1264, 0x1268, 0x1270, 0x1274, 0x1278};
    for (unsigned i = 0; i < sizeof zero_f / sizeof zero_f[0]; i++) SF32(p, zero_f[i], 0.0f);
    SU32(p, 0x1220, 1);
    SF32(p, 0x1290, 0.0f);
    if (U32(0, G_MODE) == 1) {
        const rx_addr R = S + 0x10;
        SU32(R, 0x20, U32(p, 0x1190)); SU32(R, 0x24, U32(p, 0x1194)); SU32(R, 0x28, U32(p, 0x1198));
        SU32(R, 0x10, U32(p, 0x1180)); SU32(R, 0x14, U32(p, 0x1184)); SU32(R, 0x18, U32(p, 0x1188));
        SU32(R, 0x00, PL_VEHICLE(p));
        SU32(R, 0x04, 0);
        SU32(R, 0x40, 0);
        SF32(R, 0x30, 0.0f);
        SF32(R, 0x34, 1.0f /* 0x931f24 */);
        SF32(R, 0x38, 0.0f);
        SU32(R, 0x44, 0);
        SU32(R, 0x48, 1);
        if (U32(p, 0x908) == 0) {
            __typeof__(&rx_clip_recorder_log_reset) f = PL_CORE(0x006ab800, rx_clip_recorder_log_reset);
            const uint32_t a[1] = {R};
            if (f) PL_AT(1, a, 0x00550175, f(p, R));
            else pl_call(0x006ab800, (pl_in){.eax = p, .esi = p, .edi = 0}, 1, a);
        }
    }
    {
        __typeof__(&rx_player_camera_flash) f = PL_CORE(0x00550350, rx_player_camera_flash);
        if (f) PL_AT(0, 0, 0x0055017c, f(p));
        else pl_call0(0x00550350, (pl_in){.ecx = p, .esi = p, .edi = 0});
    }
    const rx_addr v = PL_VEHICLE(p);
    if (v != 0 && U32(p, 4) != 0) {
        const rx_addr e = U32(v, 0x1a0);
        if (e != 0 && U32(e, 0xaf10) != 0) pl_rider_end_crash(e, 0x005501a3);
        PL_AT(0, 0, 0x005501aa, rx_player_restore_input_groups(p));
        __typeof__(&rx_vehicle_effect_query) f = PL_CORE(0x00460020, rx_vehicle_effect_query);
        const uint32_t a[2] = {p + 0x1180, p + 0x1190};
        if (f) PL_AT(2, a, 0x005501c3, f(PL_VEHICLE(p), p + 0x1180, p + 0x1190));
        else pl_call(0x00460020, (pl_in){.esi = PL_VEHICLE(p), .edi = 0}, 2, a);
    }
    rx_set_sp(sp0);
}

/* ---- the per-frame update ---- */

/* 0054af90 MXPlayer_Update (BaseDriver::vf7, stdcall p, dt; RET 8): camera, reset input / timers, wreck recovery, crash timer,
 * respawn fade, airtime clock, reset terrain / bounds, idle fidget, the reset latch keys, the trick controls, the player record
 * (under its lock), the reset point, the stunt timer and the jump tracker / long-jump highlight. Frame: push ebp; and esp, -16;
 * sub esp, 0x14; push ebx, esi, edi: +0x0c / +0x10 / +0x1c map iterator, +0x14 the record lock. */
void rx_player_update(rx_addr p, float dt) {
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x14, 3);
    const uint32_t dtb = pl_x87(rx_fb(dt));     /* fld [ebp+0xc]; fstp [esp] for every float argument */
    const float dtx = rx_bf(dtb);
    rx_set_sp(S);
    {
        const uint32_t a[2] = {p, dtb};
        PL_AT(2, a, 0x0054afac, rx_player_update_camera(p, dtx));
    }
    SU32(p, 0x1138, 0);
    if (PL_VEHICLE(p) != 0) PL_AT(0, 0, 0x0054afc6, rx_player_handle_reset_input(p));
    {
        const float t = F32(p, 0x116c);
        if (t > 0.0f) {
            const float n = t - dt;
            SF32(p, 0x116c, n);
            if (!(0.0f < n || n != n)) {
                const uint32_t a[1] = {0};
                PL_AT(1, a, 0x0054aff1, rx_player_reset(p, 0));
            }
        }
    }
    const uint32_t a_dt[1] = {dtb};
    PL_AT(1, a_dt, 0x0054afff, rx_player_update_wreck_recovery(p, dtx));
    PL_AT(1, a_dt, 0x0054b00d, rx_player_update_crash_reset_timer(p, dtx));
    rx_addr b = PL_DRIVER(p);
    if ((b != 0 && U32(b, 0x21c) == 2) || U32(b, 0x21c) == 3) PL_AT(1, a_dt, 0x0054b03b, rx_player_update_respawn_fade(p, dtx));
    rx_addr v = PL_VEHICLE(p);
    if (v != 0) {
        const uint32_t airborne = loop_tc0(loop_vslot(v, 0xac), v);
        SF32(p, 0x10bc, airborne ? F32(p, 0x10bc) + dt : 0.0f);
        const uint32_t st = U32(PL_DRIVER(p), 0x21c);
        if (st == 2 || st == 3) PL_AT(0, 0, 0x0054b089, rx_player_check_reset_terrain(p));
        PL_AT(0, 0, 0x0054b090, rx_player_check_out_of_bounds(p));
        const uint32_t mode = U32(PL_VEHICLE(p), 0x194);
        if ((mode == 1 || mode == 0) && PL_RIDER(p) != 0) PL_AT(1, a_dt, 0x0054b0bc, rx_player_update_idle_rider(p, dtx));
    }
    if (U8(p, 0x21c) != 0 && U8(0, 0x009b716d) == 0) {
        int go = loop_tc2(0x00811420, 0x00d601f0, 7, 0) != 0;
        if (!go && U8(0, 0x009b716d) == 0) go = loop_tc2(0x00811420, 0x00d5c968, 7, 0) != 0;
        if (go) {
            if (U32(0, G_MODE) != 2) PL_AT(0, 0, 0x0054b108, rx_player_reset_completion(p));
            SU8(p, 0x21c, 0);
        }
    }
    b = PL_DRIVER(p);
    if (b != 0 && (U32(b, 0x21c) == 2 || U32(b, 0x21c) == 3)) {
        __typeof__(&rx_fx_trick_update) ft = PL_CORE(0x006c9770, rx_fx_trick_update);      /* the trick tracker */
        if (ft) PL_AT(1, a_dt, 0x0054b13a, ft(dtx, p + 0xb58));
        else pl_call(0x006c9770, (pl_in){.eax = p + 0xb58, .ebx = p, .esi = 2}, 1, a_dt);
        __typeof__(&rx_fx_stunt_score) fs = PL_CORE(0x006cab80, rx_fx_stunt_score);        /* the stunt scorer */
        if (fs) PL_AT(1, a_dt, 0x0054b14c, fs(dtx, p + 0xc70));
        else pl_call(0x006cab80, (pl_in){.eax = p + 0xc70, .ebx = p, .esi = 2}, 1, a_dt);
    }

    /* the profile / record bookkeeping, under the record lock (the profile's lock object, or the per-thread one) */
    pl_profile_init(0x0054b161);
    if (U32(U32(0, 0x00da6cb0), 0x1c3c) == 0) {
        pl_profile_init(0x0054b19a);
        rx_addr lock;
        if (U32(0, 0x00da6d08) != 0) {
            uint32_t r0;
            __typeof__(&rx_world_shared_vf_ret0) f0 = PL_CORE(0x007a0760, rx_world_shared_vf_ret0);
            if (f0) PL_AT(0, 0, 0x0054b1b5, r0 = f0());
            else r0 = pl_call0(0x007a0760, (pl_in){.ebx = p, .esi = 1});
            const uint32_t a[2] = {3, r0};
            rx_addr e;
            __typeof__(&rx_fx_thread_table_get) fg = PL_CORE(0x00402420, rx_fx_thread_table_get);
            if (fg) PL_AT(2, a, 0x0054b1c2, e = fg(0x009df584, 3, r0));
            else e = pl_call(0x00402420, (pl_in){.ecx = 0x009df584, .ebx = p, .esi = 1}, 2, a);
            const rx_addr q = e ? U32(e, 0xc) : 0;
            lock = q ? U32(q, 8) + 0x3c : 0x3c;
        } else {
            lock = 0x00da6cf0;
        }
        SU32(S, 0x14, lock);
        if (U8(0, 0x009df580) == 0) PL_AT(0, 0, 0x0054b200, rx_lock_enter(U32(S, 0x14)));
        pl_update_record(p, S);
        if (U32(p, 0xb2c) != 0) SF32(p, 0x218, 0.0f);
        if (U8(0, 0x009df580) == 0) PL_AT(0, 0, 0x0054b509, rx_lock_leave(U32(S, 0x14)));
    }

    PL_AT(1, a_dt, 0x0054b517, rx_player_update_reset_point(p, dtx));
    b = PL_DRIVER(p);
    if (b != 0 && U32(b, 0x21c) == 2 && U32(b, 0x238) == 8 && I32(p, 0x11ac) >= 0) {
        const float n = F32(p, 0x11b4) - dt;
        SF32(p, 0x11b4, n > 0.0f ? n : 0.0f);
    }
    v = PL_VEHICLE(p);
    if (v != 0 && b != 0 && U32(b, 0x21c) == 2) {
        PL_AT(1, a_dt, 0x0054b591, rx_player_tracker_update(p + 0x11e0, v, dtx));
        if (U32(p, 0x1280) != 0) {
            const uint32_t a[1] = {p};
            __typeof__(&rx_event_vehicle_landed) veh_landed = PL_CORE(0x00554c40, rx_event_vehicle_landed);
            if (veh_landed) PL_AT(1, a, 0x0054b591, veh_landed(U32(PL_DRIVER(p), 0x290), p, p + 0x11e0));
            else pl_call(0x00554c40, (pl_in){.ecx = p + 0x11e0, .esi = U32(PL_DRIVER(p), 0x290), .edi = v, .ebx = p}, 1, a);
            SF32(p, 0x1290, 1.0f /* 0x931f24 */);
            const uint32_t m = U32(PL_VEHICLE(p), 0x194);
            SU32(p, 0x1294, (m == 1 || m == 0) ? U32(0, 0x00977494) : 0u);
        }
        if (U32(p, 4) != 0 && U32(p, 0x10) == 0) {
            const rx_addr vv = PL_VEHICLE(p);
            if (U32(vv, 0x280) != 0 || F32(p, 0x116c) > 0.0f) SF32(p, 0x1290, 0.0f);
            if (F32(p, 0x1290) > 0.0f) {
                SF32(p, 0x1290, F32(p, 0x1290) - dt);
                if (loop_tc0(loop_vslot(vv, 0xac), vv) == 0) SF32(p, 0x1294, dt + F32(p, 0x1294));
                if (!(0.0f < F32(p, 0x1290) || F32(p, 0x1290) != F32(p, 0x1290)) && F32(p, 0x1294) > 0.75f /* 0x935aa8 */) {
                    /* a long jump: its horizontal length (start +0x11e0, end +0x11f0) into 0x00d92ef8; >= 300 is a highlight */
                    const float dz = F32(p, 0x11f8) - F32(p, 0x11e8);
                    const float dx = F32(p, 0x11f0) - F32(p, 0x11e0);
                    const float d = rx_sqrts(dz * dz + dx * dx);
                    SF32(0, 0x00d92ef8, d);
                    if (d >= 300.0f /* 0x935d28 */) {
                        const uint32_t a[3] = {U32(0, 0x00d92edc), U32(p, 0x1f0), 0x2d};
                        PL_AT(3, a, 0x0054b6f3, (void)rx_highlight_add_progress(a[0], a[1], a[2]));
                    }
                }
            }
            /* 007e3be0 (empty) gets the double of 0x00d92ef8 */
            const double dd = (double)F32(0, 0x00d92ef8);
            const uint32_t a[2] = {(uint32_t)rx_db(dd), (uint32_t)(rx_db(dd) >> 32)};
            __typeof__(&rx_world_shared_vf_empty) fe = PL_CORE(0x007e3be0, rx_world_shared_vf_empty);
            if (fe) PL_AT(2, a, 0x0054b708, fe());
            else pl_call(0x007e3be0, (pl_in){.ebx = p, .edi = v}, 2, a);
        }
    }
    rx_set_sp(sp0);
}

/* ---- activity / race virtual functions and small helpers ---- */

/* 00554fc0 (rtti_shared_vf; ECX = delegate; stack arg; RET 4): invoke the delegate: the member function +0x10 on the object +0xc
 * with the two dwords of **arg (EAX = the delegate, EDX = the function as the original leaves them). The result is the callee's. */
uint32_t rx_delegate_invoke(rx_addr self, rx_addr arg) {
    const rx_addr rec = U32(arg, 0), fn = U32(self, 0x10), obj = U32(self, 0xc);
    const uint32_t a[2] = {U32(rec, 0), U32(rec, 4)};
    uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
    uint32_t r;
    if (f) PL_AT(2, a, 0x00554fd7, r = f(obj, a[0], a[1]));
    else r = pl_call(fn, (pl_in){.eax = self, .ecx = obj, .edx = fn}, 2, a);
    return r;
}

/* 0055dc20 (ECX = entity; stack a, b; RET 8): place the entity's body at a's matrix (+0xd0, copied to the frame): the level's
 * slot +0x20150 gets the matrix (00401830: EAX = slot, ECX = the matrix; cdecl matrix), the body 007d2c80 finds (stdcall level
 * +0x20138, a + 8, entity + 0x118) gets slot 0x40 (the matrix), +0x350 = 0, +0x70 = 1, +0xa2 = 1. Frame: push ebp; and esp, -16;
 * sub esp, 0x44; push ebx / esi / edi: the matrix at +0x10. */
void rx_entity_place_body(rx_addr self, rx_addr a, rx_addr b) {
    (void)b;
    const rx_addr sp0 = rx_sp(), S = pl_frame(sp0, 16, 0x44, 3), M = S + 0x10;
    rx_set_sp(S);
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(M, i, U32(a, 0xd0 + i));
    const rx_addr sim = U32(self, 0x234);
    {   /* 00401830: the level's matrix +0x20150 times the copy, into the copy */
        __typeof__(&rx_fx_matrix4_mul) fm = PL_CORE(0x00401830, rx_fx_matrix4_mul);
        const uint32_t a[1] = {M};
        if (fm) PL_AT(1, a, 0x0055dc58, fm(M, sim + 0x20150, M));
        else pl_call1(0x00401830, (pl_in){.eax = sim + 0x20150, .ecx = M, .esi = sim, .ebx = self, .edi = M + 0x40}, M);
    }
    const uint32_t args[3] = {U32(sim, 0x20138), a + 8, self + 0x118};
    rx_addr body;
    PL_AT(3, args, 0x0055dc75, body = rx_scene_manager_find_in_subscene(args[0], args[1], args[2]));   /* 007d2c80 (stdcall level, entity name, subscene name) */
    if (body != 0) {
        loop_tc1(loop_vslot(body, 0x40), body, M);
        SU32(body, 0x350, 0);
        SU32(body, 0x70, 1);
        SU8(body, 0xa2, 1);
    }
    rx_set_sp(sp0);
}

/* 00413d20: the series manager singleton (0x009e2188) */
static rx_addr pl_series(rx_addr self, rx_addr ret) {
    __typeof__(&rx_fx_series_manager) f = PL_CORE(0x00413d20, rx_fx_series_manager);
    rx_addr r;
    if (f) PL_AT(0, 0, ret, r = f());
    else r = pl_call0(0x00413d20, (pl_in){.ecx = self, .edi = 1});
    return r;
}

/* 0055e820 Activity::vf12 (ECX = activity; result EAX 0 / 1): may the run be restarted from the menu: no pause lock (+0xfe8), not
 * in state 2, no free-ride session (+0x234 +0x20018), the profile allows it, the series manager (00413d20) exists and is not in a
 * single-event flow (+0x190 == 2 or 1 with its event's kind 1). push edi. */
uint32_t rx_activity_can_restart(rx_addr self) {
    if (U32(self, 0xfe8) != 0 || U32(self, 0x238) == 2 || U32(U32(self, 0x234), 0x20018) != 0) return 0;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    uint32_t r = 0;
    const rx_addr g = U32(pl_profile(0x0055e84c), 0x10);
    if ((U32(g, 0x1c4c) == 0 || U32(g, 0x1b0) == 1) && U32(0, 0x00d60ff0) != 0) {
        r = 1;
        if (U32(pl_series(self, 0x0055e86e), 0x190) == 2 || U32(pl_series(self, 0x0055e87c), 0x190) == 1) {
            const rx_addr x = U32(pl_series(self, 0x0055e889), 0x18c);
            uint32_t k = U32(x, 0xc);
            if (k == 2) k = U32(x, 0x10);
            r = k != 1;
        }
    }
    rx_set_sp(sp0);
    return r;
}

/* 00565640 MXActivityRace::vf52 (ECX = activity): per race state (+0x238, jump table 0x5656e0) the music state: 0 / 3: +0xff080 = 0,
 * the track flag 0x009c3edc through 0055e6e0 (EAX = activity, ECX = 0), level 0x009c3ed8 = 0.0, music -1; 2: the same with 0.5
 * and music 1; 1 / 5..8: +0xff080 = 5, the flag 5 (0 with a session flag +0x234 +0x22148), level 1.0, music 2; 4: nothing.
 * The music request 0x009c47c0 is written when it differs from the current one 0x009c47bc. */
void rx_race_audio_state(rx_addr self) {
    const uint32_t state = U32(self, 0x238);
    float level;
    uint32_t music;
    switch (state) {
    case 0: case 3:
        SU32(self, 0xff080, 0);
        PL_AT(0, 0, 0x00565669, rx_activity_set_track_level(self, 0));   /* 0055e6e0 (EAX = the activity, ECX = 0) */
        level = 0.0f; music = 0xffffffffu;
        break;
    case 2:
        SU32(self, 0xff080, 0);
        PL_AT(0, 0, 0x00565682, rx_activity_set_track_level(self, 0));   /* 0055e6e0 (EAX = the activity, ECX = 0) */
        level = 0.5f /* 0x9322cc */; music = 1;
        break;
    case 1: case 5: case 6: case 7: case 8: {
        SU32(self, 0xff080, 5);
        const rx_addr sim = U32(self, 0x234);
        SU32(0, 0x009c3edc, sim ? (U32(sim, 0x22148) != 0 ? 5u : 0u) : 5u);
        level = 1.0f /* 0x931f24 */; music = 2;
        break;
    }
    default:
        return;
    }
    SF32(0, 0x009c3ed8, level);
    if (U32(0, 0x009c47bc) != music) SU32(0, 0x009c47c0, music);
}

/* 00567660 (ECX = race; stack who, n, c, d; RET 0x10): in race state 2 with n > 0, when `who` is the rider at the top of the
 * pending-rider stack (+0xff088 index into +0x2c8, above 1) it is popped: 00512c80 (EAX = the riders left behind it, ECX = the
 * next rider) and, recording (mode 1) a local human, a candidate clip of type 9 (last) / 7 (ClipRecorder_CandidateOfType, EDX =
 * who). push esi; push ebx. */
void rx_race_pop_rider(rx_addr self, rx_addr who, int32_t n, uint32_t c, uint32_t d) {
    (void)c; (void)d;
    if (U32(self, 0x21c) != 2 || n <= 0) return;
    int32_t top = I32(self, 0xff088);
    if (who != (top >= 14 ? rx_stand(self, (uint32_t)top - 2) : U32(self, 0x2c8 + 4u * (uint32_t)top)) || top <= 1) return;   /* (w-rc-core) +0x2c8 + 4 top = standings[top - 2]; 12.. in side storage */
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    const uint32_t remaining = U32(self, 0x26c) - (uint32_t)top;
    top--;
    SU32(self, 0xff088, (uint32_t)top);
    PL_AT(0, 0, 0x005676a7, rx_driver_eliminate(rx_stand(self, (uint32_t)top), remaining));   /* 00512c80 (EAX = remaining, ECX = the driver) */
    if (U32(0, G_MODE) == 1) {
        int human;
        __typeof__(&rx_player_is_local_human) f = PL_CORE(0x00552870, rx_player_is_local_human);
        if (f) PL_AT(0, 0, 0x005676b8, human = f(U32(who, 0x74)));
        else human = (int)pl_call0(0x00552870, (pl_in){.eax = U32(who, 0x74), .esi = self, .ebx = who});
        if (human != 0) {
            const uint32_t type = U32(self, 0xff088) == 1 ? 9 : 7;
            __typeof__(&rx_world_clip_recorder_candidate_of_type) g = PL_CORE(0x006aaca0, rx_world_clip_recorder_candidate_of_type);
            if (g) PL_AT(0, 0, 0x005676d6, g(type, who));
            else pl_call0(0x006aaca0, (pl_in){.ecx = type, .edx = who, .esi = self, .ebx = who});
        }
    }
    rx_set_sp(sp0);
}

/* 00525cd0 (EAX = handicap object): the catch-up tuning by difficulty (+0x1c: 0 / 1 / 2: window 32 / the .data 0x9ddfdc / 8 and
 * floor 1.5 / 0.75 / 0.5625): +0x18 = (1 - min(t, window) / window) * (1 - floor) + floor with t = +0x10 - +0xc, then
 * RavenVehicle_SetTuning (EDI = the vehicle of the player +8 -> +0x74 -> +0x1ac, ESI = stack = this + 0x40). push esi; push edi. */
void rx_activity_apply_tuning(rx_addr self) {
    const uint32_t level = U32(self, 0x1c);
    float window = F32(0, 0x009ddfdc), floor_ = 0.75f /* 0x935aa8 */;
    if (level == 0) { window = 32.0f /* 0x9774c0 */; floor_ = 1.5f /* 0x9322a4 */; }
    else if (level == 2) { window = 8.0f /* 0x9322b8 */; floor_ = 0.5625f /* 0x977998 */; }
    float t = F32(self, 0x10) - F32(self, 0xc);
    const float span = 1.0f /* 0x931f24 */ - floor_;
    if (t > window) t = window;
    SF32(self, 0x18, (1.0f - rx_divs(t, window)) * span + floor_);
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    const rx_addr v = U32(U32(U32(self, 8), 0x74), 0x1ac);
    const uint32_t a[1] = {self + 0x40};
    PL_AT(1, a, 0x00525d5b, rx_vehicle_set_tuning(v, self + 0x40, self + 0x40));
    rx_set_sp(sp0);
}

/* ---- records (career statistics tables) ---- */

/* 00764fc0 MXRecords_AddInt (cdecl rec, delta, column name): the cell (row rec +0, table rec +8) of the column gets
 * min(delta, INT_MAX - cell) added; the sum is written into the rec argument slot, whose address is the value pointer of
 * Table_SetCellInt. push ebx / ebp / esi / edi. */
void rx_records_add_int(rx_addr rec, int32_t delta, uint32_t name) {
    const rx_addr E = rx_sp(), S = E - 0x10;
    rx_set_sp(S);
    const rx_addr table = U32(rec, 8), row = U32(rec, 0);
    const pl_in regs = {.ebx = rec, .ebp = name};
    const uint32_t col = pl_column_index(table, name, 0x00764fd9, (pl_in){.ebx = rec, .ebp = name, .edi = row, .esi = table});
    const rx_addr cell = pl_cell(table, col, row, 0x00764fe3, (pl_in){.ebx = rec, .ebp = name, .esi = table});
    const int32_t cur = I32(cell, 0);
    const int32_t room = (int32_t)(0x7fffffffu - (uint32_t)cur);
    SU32(E, 4, (uint32_t)((delta >= room ? room : delta) + cur));
    pl_set_cell_int(U32(rec, 8), name, U32(rec, 0), E + 4, 0x0076500e, (pl_in){.ebx = regs.ebx, .ebp = regs.ebp, .edi = col});
    rx_set_sp(E);
}

/* the stack record {row, 0, table} of the records' 0x18-byte entries (+0 table, +4 row) handed to MXRecords_AddInt */
static void pl_records_add(rx_addr R, rx_addr e, uint32_t name, rx_addr ret) {
    SU32(R, 0, U32(e, 4));
    SU32(R, 4, 0);
    SU32(R, 8, U32(e, 0));
    const uint32_t a[3] = {R, 1, name};
    PL_AT(3, a, ret, rx_records_add_int(R, 1, name));
}

/* 00764ef0 MXRecords_HoleshotWon (EAX = index, ECX = records base): the holeshot count +1, the entry's flag +0x14 set.
 * Frame: sub esp, 0x10; push esi: the record at +8. */
void rx_records_holeshot_won(uint32_t index, rx_addr base) {
    if (index == 0xffffffffu) return;
    const rx_addr e = base + index * 0x18;
    if (U32(e, 4) == 0xffffffffu) return;
    const rx_addr sp0 = rx_sp(), S = sp0 - 0x14;
    rx_set_sp(S);
    pl_records_add(S + 8, e, 0x0096da08, 0x00764f2a);
    SU32(e, 0x14, 1);
    rx_set_sp(sp0);
}

/* 00764f40 MXRecords_AttemptedJump (EAX = index, ECX = records base): the attempted-jump count +1. Frame: sub esp, 0xc. */
void rx_records_attempted_jump(uint32_t index, rx_addr base) {
    if (index == 0xffffffffu) return;
    const rx_addr e = base + index * 0x18;
    if (U32(e, 4) == 0xffffffffu) return;
    const rx_addr sp0 = rx_sp(), S = sp0 - 0xc;
    rx_set_sp(S);
    pl_records_add(S, e, 0x0096d9b8, 0x00764f79);
    rx_set_sp(sp0);
}

/* 00764f80 MXRecords_FailedJump (EAX = index, ECX = records base): the failed-jump count +1. Frame: sub esp, 0xc. */
void rx_records_failed_jump(uint32_t index, rx_addr base) {
    if (index == 0xffffffffu) return;
    const rx_addr e = base + index * 0x18;
    if (U32(e, 4) == 0xffffffffu) return;
    const rx_addr sp0 = rx_sp(), S = sp0 - 0xc;
    rx_set_sp(S);
    pl_records_add(S, e, 0x0096d9cc, 0x00764fb9);
    rx_set_sp(sp0);
}

/* 00764b90 MXRecords_LongestWheelie (EAX = index, ECX = records base; stack value; RET 4; result EAX): when `value` beats the
 * stored best (column 0x96dacc) the cell is set (004f5330 with the value argument slot as the pointer; the table's change count
 * +0x68 grows when the value differs) and 1 is returned. push ebx / ebp / esi / edi. */
uint32_t rx_records_longest_wheelie(uint32_t index, rx_addr base, int32_t value) {
    if (index == 0xffffffffu) return 0;
    const rx_addr e = base + index * 0x18;
    const uint32_t row = U32(e, 4);
    if (row == 0xffffffffu) return 0;
    const rx_addr E = rx_sp(), S = E - 0x10;
    rx_set_sp(S);
    const rx_addr table = U32(e, 0);
    const pl_in regs = {.ebx = row, .ebp = (uint32_t)value, .esi = table};
    uint32_t col = pl_column_index(table, 0x0096dacc, 0x00764bba, regs);
    if (value <= I32(pl_cell(table, col, row, 0x00764bc4, regs), 0)) { rx_set_sp(E); return 0; }
    col = pl_column_index(table, 0x0096dacc, 0x00764bd4, regs);
    if (value != I32(pl_cell(table, col, row, 0x00764bde, regs), 0)) SU32(table, 0x68, U32(table, 0x68) + 1);
    if (!(col < (uint32_t)((int32_t)(U32(table, 0x14) - U32(table, 0x10)) >> 2))) pl_invalid_parameter();
    const rx_addr colptr = U32(U32(table, 0x10), col * 4u);
    pl_set_int(colptr, row, E + 4, 0x00764c0a, (pl_in){.ebx = row, .ebp = (uint32_t)value, .esi = table, .edi = col});
    rx_set_sp(E);
    return 1;
}

/*00764c20 exact sibling: LongestStoppie uses column96dabc; same signed/dirty/frame semantics.*/
uint32_t rx_records_longest_stoppie(uint32_t index, rx_addr base, int32_t value) {
    if (index == 0xffffffffu) return 0;
    const rx_addr e = base + index * 0x18;
    const uint32_t row = U32(e, 4);
    if (row == 0xffffffffu) return 0;
    const rx_addr E = rx_sp(), S = E - 0x10;
    rx_set_sp(S);
    const rx_addr table = U32(e, 0);
    const pl_in regs = {.ebx = row, .ebp = (uint32_t)value, .esi = table};
    uint32_t col = pl_column_index(table, 0x0096dabc, 0x00764c4a, regs);
    if (value <= I32(pl_cell(table, col, row, 0x00764c54, regs), 0)) { rx_set_sp(E); return 0; }
    col = pl_column_index(table, 0x0096dabc, 0x00764c64, regs);
    if (value != I32(pl_cell(table, col, row, 0x00764c6e, regs), 0)) SU32(table, 0x68, U32(table, 0x68) + 1);
    if (!(col < (uint32_t)((int32_t)(U32(table, 0x14) - U32(table, 0x10)) >> 2))) pl_invalid_parameter();
    const rx_addr colptr = U32(U32(table, 0x10), col * 4u);
    pl_set_int(colptr, row, E + 4, 0x00764c9a, (pl_in){.ebx = row, .ebp = (uint32_t)value, .esi = table, .edi = col});
    rx_set_sp(E);
    return 1;
}

/* ---- allocator / container helpers ---- */

/* 005442c0 (ESI = list header; result EAX = header, ECX = 0): a std::list-like header: the sentinel node (4 bytes,
 * ReflexAlignedAlloc) points to the header; header +0 the node, +0xc / +0x10 / +0x14 cleared. push ecx. */
rx_addr rx_list_init(rx_addr h) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    const rx_addr node = pl_aligned_alloc(4, 0x005442c8, (pl_in){.esi = h});
    if (node != 0) {
        SU32(node, 0, h);
        SU32(h, 0, node);
    } else {
        SU32(h, 0, 0);
    }
    SU32(h, 0xc, 0);
    SU32(h, 0x10, 0);
    SU32(h, 0x14, 0);
    rx_set_sp(sp0);
    return h;
}

/* 00545120 (EAX = first, stack last; cdecl): destroy the 0x1c-byte records [first, last): the block at +0x10 (when set) and the one
 * at +4 are freed (ReflexAlignedFree), the vector fields +0x10..+0x18 cleared between the two. push ebp, edi, ebx, esi. */
void rx_records_free_range(rx_addr first, rx_addr last) {
    /* Frame: push ebp (EBP = last); push edi (EDI = the element); a non-empty range: push ebx (EBX = 0); push esi (ESI = the
     * element + 0x10); each ReflexAlignedFree with its frame (the model destruction of a package unload runs this on the database
     * thread, whose dead stack stays). Entered at the return slot; the caller's registers from the fx register model. */
    const rx_addr E = rx_sp();
    SU32(E, -4, fx_reg(FX_EBP)); SU32(E, -8, fx_reg(FX_EDI));
    if (first == last) return;
    SU32(E, -0xc, fx_reg(FX_EBX)); SU32(E, -0x10, fx_reg(FX_ESI));
    rx_set_sp(E - 0x10);
    for (rx_addr e = first; e != last; e += 0x1c) {
        const crt_regs4 r = {0, last, e + 0x10, e};
        const rx_addr blk = U32(e, 0x10);
        if (blk != 0) { SU32(E, -0x14, blk); SU32(E, -0x18, 0x0054513f); rx_reflex_aligned_free_at(E - 0x18, r, blk); }
        const rx_addr q = U32(e, 4);
        SU32(e, 0x10, 0);
        SU32(e, 0x14, 0);
        SU32(e, 0x18, 0);
        SU32(E, -0x14, q); SU32(E, -0x18, 0x00545153); rx_reflex_aligned_free_at(E - 0x18, r, q);
    }
    rx_set_sp(E);
}

/* 008841d0 (ECX = table of 11 ints; stack index; RET 4; result EAX): table[index], or -1 after recording the error code
 * 0xe0000003 for the thread (00884390) when the index is out of range */
uint32_t rx_table11_get(rx_addr table, uint32_t index) {
    if (index < 0xbu) return U32(table, index * 4u);
    const uint32_t a[4] = {0, 0, 0xe0000003u, 0};
    PL_AT(4, a, 0x008841e9, rx_thread_note(0, 0, 0xe0000003u, 0));
    return 0xffffffffu;
}

/* 00884390 (cdecl a0, a1, a2, a3): record the calling thread's error codes a2 / a3 in the two per-thread std::maps (0x009df64c
 * under lock 0x009df638, 0x009df604 under lock 0x009df5f0; 008c1270 = map[key], key = GetCurrentThreadId). The first key lives
 * in the push-ecx local; the second is stored into the a2 argument slot (the original reuses it), and a3 is read from its slot. */
void rx_thread_note(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    (void)a0; (void)a1; (void)a2; (void)a3;
    const rx_addr E = rx_sp(), K = E - 4;
    rx_set_sp(K);
    if (U8(0, 0x009df580) == 0) PL_AT(0, 0, 0x008843a4, rx_lock_enter(0x009df638));
    SU32(K, 0, loop_get_current_thread_id());
    {
        const uint32_t a[1] = {K};
        /* BOUNDARY (error path): rx_thread_note records an error code (0xe0000003 / 0xa0000007) for the calling thread in two debug std::maps that nothing reads; it is entered only on an out-of-range table index (rx_table11_get) or a network error (race_net.c); 008c1270 is the std::map operator[] (a template shared with other maps) and is not ported for this use. */
        const rx_addr slot = pl_call(0x008c1270, (pl_in){.ecx = 0x009df64c}, 1, a);
        SU32(slot, 0, U32(E, 0xc));
    }
    if (U8(0, 0x009df580) == 0) {
        PL_AT(0, 0, 0x008843d3, rx_lock_leave(0x009df638));
        if (U8(0, 0x009df580) == 0) PL_AT(0, 0, 0x008843e6, rx_lock_enter(0x009df5f0));
    }
    SU32(E, 0xc, loop_get_current_thread_id());
    {
        const uint32_t a[1] = {E + 0xc};
        /* BOUNDARY (error path): rx_thread_note records an error code (0xe0000003 / 0xa0000007) for the calling thread in two debug std::maps that nothing reads; it is entered only on an out-of-range table index (rx_table11_get) or a network error (race_net.c); 008c1270 is the std::map operator[] (a template shared with other maps) and is not ported for this use. */
        const rx_addr slot = pl_call(0x008c1270, (pl_in){.ecx = 0x009df604, .edx = E + 0xc}, 1, a);
        SU32(slot, 0, U32(E, 0x10));
    }
    rx_set_sp(E);
    if (U8(0, 0x009df580) == 0) rx_lock_leave(0x009df5f0);    /* add esp, 4; jmp 00883d90 */
}

/* 00540130 (EAX = parts manager, EDI = manager; stack dt; RET 4): the vehicle part package manager: the package cache is updated
 * (PackageCache_Update, EAX = parts manager + 4), then each of the manager's vehicles (+0x40, count +0x44) whose parts entry (the
 * list +0x28, count +0x2c, entry +4 == vehicle +0x18c; none: 0) has nothing loading (0053ffb0) is finished (005429e0 stdcall
 * manager, index). Frame: push ecx; push ebx / ebp / esi; the loop index is kept in the dt argument slot. */
void rx_package_manager_update(rx_addr eax_obj, rx_addr mgr, float dt) {
    const rx_addr sp0 = rx_sp(), S = sp0 - 0x10;
    rx_set_sp(S);
    {
        __typeof__(&rx_pkgcache_update) f = PL_CORE(0x00543e50, rx_pkgcache_update);
        const uint32_t a[1] = {pl_x87(rx_fb(dt))};
        if (f) PL_AT(1, a, 0x00540144, f(eax_obj + 4, rx_bf(a[0])));
        else pl_call(0x00543e50, (pl_in){.eax = eax_obj + 4, .edi = mgr}, 1, a);
    }
    SU32(sp0, 4, 0);                          /* the index lives in the dt argument slot ([esp + 0x14]) */
    for (int32_t i = 0; i < I32(mgr, 0x44); i = I32(sp0, 4)) {
        const rx_addr veh = U32(U32(mgr, 0x40), (uint32_t)i * 4u);
        const int32_t cnt = I32(mgr, 0x2c);
        rx_addr found = 0;
        for (int32_t k = 0; k < cnt; k++) {
            const rx_addr e = U32(U32(mgr, 0x28), (uint32_t)k * 4u);
            if (e != 0 && U32(e, 4) == U32(veh, 0x18c)) { found = e; break; }
        }
        int busy;
        __typeof__(&rx_race_parts_loading) f = PL_CORE(0x0053ffb0, rx_race_parts_loading);
        const uint32_t a[1] = {found};
        if (f) PL_AT(1, a, 0x00540182, busy = f(found));
        else busy = (int)pl_call(0x0053ffb0, (pl_in){.ecx = found, .edi = mgr, .ebx = veh, .esi = (uint32_t)cnt}, 1, a);
        if (busy != 0) {
            const uint32_t b[2] = {mgr, (uint32_t)i};
            /* BOUNDARY (not reached): finishing a vehicle whose part packages were still loading (rx_package_manager_update, a package set the loader reports busy); the product's part packages are all ready when the vehicle is spawned (the reference world executes 0 of 219 bytes in 1700 frames; stop_audit runs never reach it). */
            pl_call(0x005429e0, (pl_in){.edi = mgr, .ebx = veh, .esi = (uint32_t)i}, 2, b);
        }
        SU32(sp0, 4, (uint32_t)(i + 1));
    }
    rx_set_sp(sp0);
}

/* ---- registry ---- */
RX_FUNCTION(0x0054af90, rx_player_update)
RX_FUNCTION(0x0054c150, rx_player_update_camera)
RX_FUNCTION(0x0054cc10, rx_player_update_cameras)
RX_FUNCTION(0x0054cec0, rx_player_update_idle_rider)
RX_FUNCTION(0x0054eee0, rx_player_update_reset_point)
RX_FUNCTION(0x0054f3e0, rx_player_check_reset_terrain)
RX_FUNCTION(0x0054f330, rx_player_check_out_of_bounds)
RX_FUNCTION(0x0054e4f0, rx_player_handle_reset_input)
RX_FUNCTION(0x0054ea30, rx_player_update_wreck_recovery)
RX_FUNCTION(0x0054ee00, rx_player_update_crash_reset_timer)
RX_FUNCTION(0x0054b850, rx_player_update_respawn_fade)
RX_FUNCTION(0x0054d7c0, rx_player_can_reset)
RX_FUNCTION(0x0054d480, rx_player_restore_input_groups)
RX_FUNCTION(0x0054ec40, rx_player_cancel_wreck)
RX_FUNCTION(0x0054b720, rx_player_set_fade_hold)
RX_FUNCTION(0x0054b780, rx_player_set_fade_out)
RX_FUNCTION(0x0054b7f0, rx_player_set_fade_clear)
RX_FUNCTION(0x0054fff0, rx_player_reset)
RX_FUNCTION(0x0054d2a0, rx_player_event_a)
RX_FUNCTION(0x0054d530, rx_player_event_b)
RX_FUNCTION(0x0054d1a0, rx_player_event_hold_begin)
RX_FUNCTION(0x0054d1e0, rx_player_event_hold_end)
RX_FUNCTION(0x0054d310, rx_player_event_landed)
RX_FUNCTION(0x0054d360, rx_player_event_takeoff)
RX_FUNCTION(0x0054f0b0, rx_player_event_name)
RX_FUNCTION(0x0054feb0, rx_player_post_vectors)
RX_FUNCTION(0x0054fcd0, rx_player_set_group0)
RX_FUNCTION(0x0054fd20, rx_player_set_group3)
RX_FUNCTION(0x0054fd70, rx_player_set_group_147c)
RX_FUNCTION(0x0054fdc0, rx_player_set_group_167c)
RX_FUNCTION(0x0054fe10, rx_player_set_group_748)
RX_FUNCTION(0x0054fe60, rx_player_set_group_f34)
RX_FUNCTION(0x00548bf0, rx_player_slot_group2)
RX_FUNCTION(0x00548c40, rx_player_slot_group5)
RX_FUNCTION(0x005485c0, rx_player_analog_348)
RX_FUNCTION(0x00548880, rx_player_pressed_2fa0)
RX_FUNCTION(0x00548850, rx_player_pressed_2da0)
RX_FUNCTION(0x005488b0, rx_player_pressed_31a0)
RX_FUNCTION(0x00548960, rx_player_scripted_wreck_type)
RX_FUNCTION(0x004db070, rx_player_tracker_land)
RX_FUNCTION(0x004db160, rx_player_tracker_update)
RX_FUNCTION(0x00883ac0, rx_lock_enter_raw)
RX_FUNCTION(0x00883ad0, rx_lock_leave_raw)
RX_FUNCTION(0x00883d80, rx_lock_enter)
RX_FUNCTION(0x00883d90, rx_lock_leave)
RX_FUNCTION(0x00554fc0, rx_delegate_invoke)
RX_FUNCTION(0x0055dc20, rx_entity_place_body)
RX_FUNCTION(0x0055e820, rx_activity_can_restart)
RX_FUNCTION(0x00565640, rx_race_audio_state)
RX_FUNCTION(0x00567660, rx_race_pop_rider)
RX_FUNCTION(0x00525cd0, rx_activity_apply_tuning)
RX_FUNCTION(0x00764fc0, rx_records_add_int)
RX_FUNCTION(0x00764ef0, rx_records_holeshot_won)
RX_FUNCTION(0x00764f40, rx_records_attempted_jump)
RX_FUNCTION(0x00764f80, rx_records_failed_jump)
RX_FUNCTION(0x00764b90, rx_records_longest_wheelie)
RX_FUNCTION(0x00764c20, rx_records_longest_stoppie)
RX_FUNCTION(0x005442c0, rx_list_init)
RX_FUNCTION(0x00545120, rx_records_free_range)
RX_FUNCTION(0x008841d0, rx_table11_get)
RX_FUNCTION(0x00884390, rx_thread_note)
RX_FUNCTION(0x00540130, rx_package_manager_update)
