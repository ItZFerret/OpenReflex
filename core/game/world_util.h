/* world_util.h -- small helpers shared by the physics world code (core/game/world_*.c): SSE lane operations as the original
 * computes them, the original's stack frames at the same game addresses, calls into code that is not ported yet, and the
 * recurring patterns of the generic dynamic objects (contact sets, contact events, broad-phase cursors). core/notes/world.md. */
#pragma once
#include "rx_game.h"
#include "phys_vec.h"
#include "crt.h"
#include "rx_vcall.h"

/* ---- 4-lane values: the physics core's vector helpers (phys_vec.h: x86's exact results, NaN rule included), plus: ---- */
static inline rx_v4 v4_splat_bits(uint32_t b) { return v4_bits(b, b, b, b); }
static inline uint32_t v4_lane_bits(rx_v4 x, int i) { return rx_fb(x.v[i]); }
static inline rx_v4 v4_cmpneq(rx_v4 a, rx_v4 b) { return v4_mask(!(a.v[0] == b.v[0]), !(a.v[1] == b.v[1]), !(a.v[2] == b.v[2]), !(a.v[3] == b.v[3])); }
/* "all of lanes 0..2 set" as the original tests a compare mask: (m1 + m2 + m0) == -3 in 32-bit arithmetic. */
static inline int v4_all3(rx_v4 mask) { return (uint32_t)(v4_lb(mask, 1) + v4_lb(mask, 2) + v4_lb(mask, 0)) == 0xfffffffdu; }
/* |v|^2 as the original sums mulps v, v: (x + z) + y (shufps 0x66 pattern) or (x + y) + z (shufps 0x99). */
static inline float v4_len2_xzy(rx_v4 v) { rx_v4 p = v4_mul(v, v); return phys_add(phys_add(p.v[0], p.v[2]), p.v[1]); }
static inline float v4_len2_xyz(rx_v4 v) { return phys_hsum3(v4_mul(v, v)); }
/* |v| (xzy sum) as one sqrtss */
static inline float v4_len_xzy(rx_v4 v) { return phys_sqrt(v4_len2_xzy(v)); }
/* The constant vectors the physics code reads at run time (writable data: never folded into literals). */
#define WORLD_ZERO4      0x00d61670u   /* (0, 0, 0, 0) */
#define WORLD_IDENTITYQ  0x00d61640u   /* (0, 0, 0, 1) */
#define WORLD_INF4       0x0092dc20u   /* +inf lanes */
#define WORLD_NAN4       0x0092dc30u   /* the NaN pattern of a zero-length normalize */
/* v / |v| (|v|^2 summed (x + y) + z), with the 0x92dc30 pattern where |v|^2 is inf and 0 where |v| is 0. */
static inline rx_v4 v4_normalize_or_nan(rx_v4 v) {
    float len2 = v4_len2_xyz(v);
    rx_v4 l2 = v4_splat(len2), len = v4_sqrt(l2);
    rx_v4 r = v4_div(v, len);
    rx_v4 finite = v4_cmpneq(l2, v4_ld(WORLD_INF4));
    r = v4_and(v4_and(r, v4_cmpneq(len, v4_zero())), finite);
    return v4_or(r, v4_andn(finite, v4_ld(WORLD_NAN4)));
}

/* ---- the generic dynamic objects (world +0x1960, 0xe0-byte records), keyed (+0x19a0) and static (+0x19c0, 64 bytes) objects */
#define WORLD_OBJ_SIZE 0xe0u
static inline uint32_t world_obj_count(rx_addr world) { return (uint32_t)((I32(world, 0x1964) - I32(world, 0x1960)) / (int32_t)WORLD_OBJ_SIZE); }
static inline uint32_t world_keyed_count(rx_addr world) { return (uint32_t)((I32(world, 0x19a4) - I32(world, 0x19a0)) / (int32_t)WORLD_OBJ_SIZE); }
static inline uint32_t world_static_count(rx_addr world) { return (uint32_t)((I32(world, 0x19c4) - I32(world, 0x19c0)) >> 6); }

/* The broad-phase result cursor ([c] position, [c+0x30] -> index array with its count at +0x400): the next index, or -1. */
static inline uint32_t world_next_pair(rx_addr cursor) {
    rx_addr arr = U32(cursor, 0x30);
    int32_t pos = I32(cursor, 0);
    if (pos < I32(arr, 0x400)) { pos++; SU32(cursor, 0, (uint32_t)pos); return U32(arr, 4u * (uint32_t)pos - 4); }
    return 0xffffffffu;
}

/* The kinematic axis lock: lanes replaced by the zero vector's (bit 0: x, bit 1: y, bit 2: lanes 2 and 3 from its lane 2, as
 * shufps v, z, 0xa4 leaves them). */
static inline rx_v4 world_apply_lock(rx_v4 v, uint32_t lock, rx_v4 z) {
    if (lock == 0) return v;
    if (lock & 1) v.v[0] = z.v[0];
    if (lock & 2) v.v[1] = z.v[1];
    if (lock & 4) { v.v[2] = z.v[2]; v.v[3] = z.v[2]; }
    return v;
}

/* A float passed on through the x87 stack (fld dword / fstp dword): bit-exact except that a signalling NaN comes out quiet. */
static inline uint32_t world_x87_pass(uint32_t b) { return ((b & 0x7fffffffu) > 0x7f800000u) ? (b | 0x400000u) : b; }

/* ---- the original's stack frame at the same game addresses ----------------------------------------------------------------
 * Buffers the original passes to callees by address live there, and each call sees the ESP it sees in the original, so the
 * callee's frame (and any read of stale stack) lands on the same bytes:
 *     world_frame f = world_frame_enter(0x7a4, 3, 1);   push ebp; mov ebp, esp; and esp, -16; sub esp, 0x7a4; 3 pushes
 *     f.esp + X is [esp + X] after the prologue; world_frame_leave(&f) at the end. */
typedef struct { rx_addr entry, ebp, esp; } world_frame;
static inline world_frame world_frame_enter(uint32_t size, int pushes, int align16) {
    world_frame f; f.entry = rx_sp(); f.ebp = f.entry - 4;
    f.esp = (align16 ? (f.ebp & ~15u) : f.ebp) - size - 4u * (uint32_t)pushes;
    rx_set_sp(f.esp); return f;
}
/* No "push ebp": pushes of callee-saved registers, then sub esp, size. */
static inline world_frame world_frame_flat(int pushes, uint32_t size) {
    world_frame f; f.entry = rx_sp(); f.ebp = 0; f.esp = f.entry - 4u * (uint32_t)pushes - size; rx_set_sp(f.esp); return f;
}
static inline void world_frame_leave(const world_frame* f) { rx_set_sp(f->entry); }
/* A direct call of a core function as if the original had pushed `arg_bytes` of arguments and its return address. */
#define WORLD_CALL_SP(arg_bytes, call) do { rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - (arg_bytes) - 4); call; rx_set_sp(sp_); } while (0)

/* The stack a `push args...; call` leaves below the current ESP: the arguments ([esp] first) and the return address `ret`.
 * Dead after the call, but some later code copies uninitialized stack into heap records (particle records: notes/world.md
 * traps), so a port whose dead stack matters writes it before the direct call (WORLD_CALL_SP passes the C arguments). */
static inline void world_pushed(uint32_t ret, int n, const uint32_t* args) {
    const rx_addr sp = rx_sp() - 4u * (uint32_t)n;
    for (int i = 0; i < n; i++) SU32(sp, 4 * i, args[i]);
    SU32(sp, -4, ret);
}
#define WORLD_PUSHED(ret, ...) world_pushed((ret), (int)(sizeof((uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t)), (uint32_t[]){__VA_ARGS__})

/* ---- calls into code that is not in the core yet -------------------------------------------------------------------------- */
#define WORLD_NARGS(...) ((int)(sizeof((uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t)))
#define WORLD_ORIG(fn, conv, ecx, ...) rx_call_original((fn), (conv), (ecx), WORLD_NARGS(__VA_ARGS__), (uint32_t[]){__VA_ARGS__})
#define WORLD_ORIG0(fn, conv, ecx) rx_call_original((fn), (conv), (ecx), 0, (const uint32_t*)0)
/* A virtual thiscall of integer arguments through vtable byte offset `off` of `obj` (sweep A): the core function of the slot's
 * target when registered, else the original (rx_vcall.h). A float argument needs a typed call at the site (WORLD_VCALL_F). */
#define WORLD_VCALL(obj, off, ...) rx_vcall((obj), (off), WORLD_NARGS(__VA_ARGS__), (uint32_t[]){__VA_ARGS__})
#define WORLD_VCALL0(obj, off) rx_vcall((obj), (off), 0, (const uint32_t*)0)
/* A virtual thiscall with one float argument (the core function takes (rx_addr this, float)); the original gets the bits. */
static inline uint32_t world_vcall_f(rx_addr obj, uint32_t off, float x) {
    const rx_addr target = U32(U32(obj, 0), off);
    uint32_t (*f)(rx_addr, float) = RX_FN(target, uint32_t (*)(rx_addr, float));
    if (f) { const rx_addr sp = rx_sp(); rx_set_sp(sp - 8u); const uint32_t r = f(obj, x); rx_set_sp(sp); return r; }
    uint32_t a = rx_fb(x);
    return rx_call_original(target, RX_THISCALL, obj, 1, &a);
}
/* register conventions: rx_regs in = world_regs(); in.r[WORLD_ESI] = ...; rx_call_original_ex(fn, &in, &out, n, args) */
enum { WORLD_EAX, WORLD_ECX, WORLD_EDX, WORLD_EBX, WORLD_ESP, WORLD_EBP, WORLD_ESI, WORLD_EDI };
static inline rx_regs world_regs(void) { rx_regs r; memset(&r, 0, sizeof r); return r; }
#define WORLD_ORIG_EX(fn, in, out, ...) rx_call_original_ex((fn), (in), (out), WORLD_NARGS(__VA_ARGS__), (uint32_t[]){__VA_ARGS__})
#define WORLD_ORIG_EX0(fn, in, out) rx_call_original_ex((fn), (in), (out), 0, (const uint32_t*)0)
/* The CRT's invalid-parameter abort (vector bounds checks), through the IAT slot 0x916224. */
static inline void world_bounds_check(uint32_t i, uint32_t n) { if (i >= n) crt_invalid_parameter_noinfo(); }

/* The registers the original holds at a call into code that is not ported (its LTCG register arguments, and the incidental
 * values a callee may spill and later read back as stale stack): world_ctx with designated fields, e.g.
 *     &WCTX(.ebx = world, .esi = i, .edi = A)  (a macro, so that it can stand inside other macro calls)                                                                */
typedef struct { uint32_t eax, ecx, edx, ebx, ebp, esi, edi; rx_v4 xmm0, xmm1, xmm2; } world_ctx;
#define WCTX(...) ((world_ctx){__VA_ARGS__})
static inline rx_regs world_ctx_regs(const world_ctx* c) {
    rx_regs r = world_regs();
    if (c) {
        r.r[WORLD_EAX] = c->eax; r.r[WORLD_ECX] = c->ecx; r.r[WORLD_EDX] = c->edx; r.r[WORLD_EBX] = c->ebx;
        r.r[WORLD_EBP] = c->ebp; r.r[WORLD_ESI] = c->esi; r.r[WORLD_EDI] = c->edi;
        r.xmm[0] = c->xmm0; r.xmm[1] = c->xmm1; r.xmm[2] = c->xmm2;
    }
    return r;
}
/* rx_call_original_ex with a world_ctx; returns EAX, *out (may be NULL) gets every register after the call */
static inline uint32_t world_call_ctx(rx_addr fn, const world_ctx* c, rx_regs* out, int n, const uint32_t* args) {
#ifndef RX_ORACLE   /* the product: a virtual thiscall (ECX = the object) of a registered function with an integer-only signature runs as a core call (src/rx_mem.c) */
    if (c && c->ecx != 0 && rx_fn(fn)) {
        const uint32_t r = rx_call_original(fn, RX_THISCALL, c->ecx, n, args);
        if (out) { *out = world_ctx_regs(c); out->r[WORLD_EAX] = r; }
        return r;
    }
#endif
    rx_regs in = world_ctx_regs(c), o;
    rx_call_original_ex(fn, &in, &o, n, args);
    if (out) *out = o;
    return o.r[WORLD_EAX];
}
#define WORLD_CTX(fn, ctx, out, ...) world_call_ctx((fn), (ctx), (out), WORLD_NARGS(__VA_ARGS__), (uint32_t[]){__VA_ARGS__})
#define WORLD_CTX0(fn, ctx, out) world_call_ctx((fn), (ctx), (out), 0, (const uint32_t*)0)

/* ---- AUDIO BOUNDARY: calls of the audio layer into functions that only an audio engine can reach ---------------------------------
 * AUDIO_BOUNDARY_CTX / _CTX0 / _ORIG are WORLD_CTX / WORLD_CTX0 / WORLD_ORIG for the originals listed below: the oracle runs them, the
 * product is fatal if one is reached anyway (naming the address; there is no silent skip). The census (tools/census_original_calls.py)
 * lists the targets of these macros as documented boundaries, class "audio".
 *
 * Why the product never reaches them (disassembly + data xrefs of MXReflex.exe, product runs):
 *  - The audio system is created only by FUN_00804070 (the command queue [0xa95814], the free command blocks [0xa81b20], the event
 *    system [0xa957d4], the wake-up event [0xd67ce8] and the audio thread, whose id CreateThread stores at [0xd67d1c]). Its only caller is
 *    Engine::Startup; the core's startup port does not build it (build_globals_engine.c: "NOT built: renderer / audio / platform").
 *    So in the product [0xd67d1c] is 0 (never the id of the running thread), the queue / free list / event system are null, and the
 *    shutdown flag [0x9b7430] keeps its initial 1 (only a posted command kind 2 clears it). Every poster guards on that flag
 *    (Audio_PostCommand's callers: the engine frame, 0080c280 / 0080abd0 / 0080afc0 / ..., fx_animal.c), so nothing is ever queued.
 *  - The audio-thread functions below run only (a) as the audio thread itself (Audio_ExecuteCommand 00806110 pops commands from that
 *    queue) or (b) inline when GetCurrentThreadId() == [0xd67d1c] (the `on_audio_thread` tests of world_node_audio.c: never true).
 *    They are the event layer's FMOD side: 00805780 post-and-wait, 00806110 command executor, 00806c10 event-name parse,
 *    00807360 / 0080bd40 channel-group leave, 0080a890 instance release, 0080aaa0 start, 0080aca0 play, 0080aee0 3D position,
 *    0080b030 parameter, 0080b360 attenuation range, 0080b540 pause, 0080b5e0 mute, and the two FMOD import thunks 00900360 /
 *    00900390 (getParameter / getRange). What they write is the FMOD instance (the device), the event / command records of the layer
 *    (event state +0x138, instance +0x118, ids +0x124 / +0x128, flags +0x13c) and the layer's counters; none of it is read by
 *    simulation code (physics, race rules, AI, the scene): the readers are the same layer plus fx_effect.c / race_sound.c, which only
 *    decide whether to restart a sound.
 *  - The collision-sound manager (AudioCollisionManager singleton, created by FUN_0067da10 from the audio setup, fed through the
 *    physics world's collision listener list by vf0 0067bac0) is never created, so 0067c670 (a type 5 object's sound) and 0067c990
 *    (AudioCollision_StealLoudest, when the manager's event pool is empty) are unreachable too.
 *  - Dynamic check (stop_audit_run, development librxcore, National NAT_Concord, fuzzed rides with wrecks, 3000 frames): breakpoints on
 *    rx_world_audio_post_command, rx_world_event_create, rx_world_audio_collision_add and rx_world_audio_collision_manager_vf0: 0 hits.
 * The oracle worlds run the original engine start-up with the audio layer present, which is why the ports stay verified there. */
#define AUDIO_BOUNDARY_CTX(fn, ctx, out, ...) WORLD_CTX((fn), (ctx), (out), __VA_ARGS__)
#define AUDIO_BOUNDARY_CTX0(fn, ctx, out) WORLD_CTX0((fn), (ctx), (out))
#define AUDIO_BOUNDARY_ORIG(fn, conv, ecx, ...) WORLD_ORIG((fn), (conv), (ecx), __VA_ARGS__)
/* The registered core functions whose C signature takes one float stack argument and that the world code dispatches by vtable
 * slot (the collision shapes' vf4, the scale: composite 00848af0, sphere 00843fc0). rx_call_original's integer-signature table
 * (src/rx_mem.c) cannot call them -- their argument is a float, not an integer -- so they need the typed path below; their
 * integer-argument siblings (00679000 shared no-op) keep WORLD_CTX. */
static inline int world_fn_float1(rx_addr fn) { return fn == 0x00848af0u || fn == 0x00843fc0u || fn == 0x00844050u; }
/* world_call_ctx with one float argument: the product calls the registered port with its own prototype (the argument in the
 * original's pushed slot at [esp], the return address slot below it: the original's `fld [item]; push ecx; fstp [esp]` at
 * 0066850c / 00668578); the oracle runs the original with the caller's registers (the adapter verifies the port). */
static inline uint32_t world_call_ctx_f(rx_addr fn, const world_ctx* c, rx_regs* out, float x) {
#ifndef RX_ORACLE
    if (c && c->ecx != 0 && world_fn_float1(fn)) {
        uint32_t (*f)(rx_addr, float) = RX_FN(fn, uint32_t (*)(rx_addr, float));
        if (f) {
            const rx_addr sp = rx_sp();
            SU32(sp, -4, rx_fb(x));       /* the argument ([esp] after the push, [esp + 4] at the callee's entry) */
            rx_set_sp(sp - 8u);           /* pushed argument + the call's return address */
            const uint32_t r = f(c->ecx, x);
            rx_set_sp(sp);
            if (out) { *out = world_ctx_regs(c); out->r[WORLD_EAX] = r; }
            return r;
        }
    }
#endif
    uint32_t a = rx_fb(x);
    return world_call_ctx(fn, c, out, 1, &a);
}
#define WORLD_CTX_F(fn, ctx, out, x) world_call_ctx_f((fn), (ctx), (out), (x))
