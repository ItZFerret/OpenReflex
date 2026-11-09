/* world_node_audio.c -- the FMOD audio event layer (0x00803ed0-0x0080dab8): events, their pools and channels, the command queue to
 * the audio thread, and the aligned allocator they use.
 *
 * Event (0x1a4 bytes, in a pool's array): +0 vtable (vfunc 1 (channel / 0, ...)), +8 owned, +0xc its pool, +0x10, +0x14 the event
 * system, +0x18 its name (0x100 chars), +0x118 the FMOD::Event, +0x11c volume, +0x124 / +0x128 ids (-1: none), +0x12c, +0x134 the
 * owner's tag, +0x138 state (0 idle, 1 / 2 busy, 3 start requested), +0x148 its lock, +0x184..+0x19c callback data, +0x1a0 the
 * channel group. Pool: +0 events, +4 count, +8 the free list (a vector: +0xc begin, +0x10 end, +0x14 capacity end; +0x18 its lock).
 * Calls made off the audio thread ([0xd67d1c]; GetCurrentThreadId through the IAT 0x91607c) are queued instead (0080c280 /
 * Audio_PostCommand); 0x9b7430 = the audio system is shutting down. */
#include "phys_coll_misc.h"
#include "world_node.h"
#include "../src/rx_os.h"
#include "world_util.h"
#include "world_misc.h"
#include "build_alloc.h"
#include "crt.h"
#include "crt_util.h"
#include "loop_player.h"

#define IAT_ENTER_CS   0x0091600cu
#define IAT_LEAVE_CS   0x009160f8u
#define IAT_THREAD_ID  0x0091607cu
static void crt_abort(void) { crt_invalid_parameter_noinfo(); }
static void enter_cs(rx_addr cs) { WORLD_ORIG(U32(0, IAT_ENTER_CS), RX_STDCALL, 0, cs); }
static void leave_cs(rx_addr cs) { WORLD_ORIG(U32(0, IAT_LEAVE_CS), RX_STDCALL, 0, cs); }
static int on_audio_thread(world_ctx c) { return U32(0, 0x00d67d1c) == WORLD_CTX0(U32(0, IAT_THREAD_ID), &c, 0); }
static int event_busy(rx_addr ev) { const uint32_t s = U32(ev, 0x138); return s == 2 || s == 1; }
static uint32_t str_len(rx_addr s) { uint32_t n = 0; while (U8(s, n) != 0) n++; return n; }

/* ---- the aligned allocator ---------------------------------------------------------------------------------------- */
/* 0078fbe0 ReflexAlignedAlloc (cdecl size; EAX): owned by the construction unit (build_alloc.h, rx_reflex_aligned_alloc);
 * this share calls it through that header. */
rx_addr rx_world_aligned_alloc(uint32_t size) { return rx_reflex_aligned_alloc(size); }
/* 0078fc50 ReflexAlignedFree (cdecl p): likewise owned by the construction unit (rx_reflex_aligned_free). */
void rx_world_aligned_free(rx_addr p) { rx_reflex_aligned_free(p); }

/* ---- pools ---------------------------------------------------------------------------------------------------------- */

/* 0080cd90 (EAX = vector {+0 container, +0xc begin, +0x10 end}; RET 0; EAX): the address of its last element (a checked
 * iterator stepped back from end, 00842fa0). */
rx_addr rx_world_vector_last(rx_addr v) {
    world_frame f = world_frame_flat(3, 0x10);
    const rx_addr end = U32(v, 0x10);
    if (U32(v, 0xc) > end) crt_abort();
    const rx_addr it = f.esp + 0x10;
    SU32(it, 0, U32(v, 0));
    SU32(it, 4, end);
    WORLD_CALL_SP(4, phys_iterator_advance(it, -1));   /* --iterator */
    const rx_addr c = U32(it, 0);
    rx_addr bound;
    if (c == 0) { crt_abort(); bound = U32(0, 0x10); }
    else bound = U32(U32(c, 0), 0x10);
    const rx_addr p = U32(it, 4);
    if (!(p < bound)) crt_abort();
    world_frame_leave(&f);
    return p;
}
RX_FUNCTION(0x0080cd90, rx_world_vector_last)

/* 0080cdf0 (EDI = vector; stdcall item*; RET 4): push_back of *item (the reallocating insert 0080d280 when full). */
void rx_world_vector_push_back(rx_addr v, rx_addr item) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(((sp0 - 4) & ~7u) - 8 - 8);   /* push ebp; and esp, -8; sub esp, 8; push ebx; push esi */
    const rx_addr begin = U32(v, 0xc);
    const uint32_t cap = begin == 0 ? 0 : (uint32_t)((int32_t)(U32(v, 0x14) - begin) >> 2);
    const rx_addr end = U32(v, 0x10);
    if ((uint32_t)((int32_t)(end - begin) >> 2) < cap) {
        SU32(end, 0, U32(item, 0));
        SU32(v, 0x10, end + 4);
    } else {
        if (begin > end) crt_abort();
        const rx_addr local = rx_sp() + 8;
        WORLD_CALL_SP(16, phys_vector_insert_n(v, local, item, U32(v, 0), end));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0080cdf0, rx_world_vector_push_back)

/* 0080c440 (ESI = pool free list; RET 0; EAX): takes the last free event (owned flag +8 = 1), 0 when none. */
rx_addr rx_world_event_pool_take(rx_addr list) {
    if (((U32(list, 0x10) - U32(list, 0xc)) & 0xfffffffcu) == 0) return 0;
    world_frame f = world_frame_flat(3, 0);
    enter_cs(list + 0x18);
    rx_addr last;
    WORLD_CALL_SP(0, last = rx_world_vector_last(list));
    const rx_addr ev = U32(last, 0);
    const rx_addr end = U32(list, 0x10);
    if (((end - U32(list, 0xc)) & 0xfffffffcu) != 0) SU32(list, 0x10, end - 4);
    SU32(ev, 8, 1);
    leave_cs(list + 0x18);
    world_frame_leave(&f);
    return ev;
}
RX_FUNCTION(0x0080c440, rx_world_event_pool_take)

/* 0080c400 (EAX = pool free list; stdcall event; RET 4): an owned event goes back to the free list (owned = 0); the list gets
 * the address of the argument slot (holding the event). */
void rx_world_event_pool_give(rx_addr list, rx_addr ev) {
    const rx_addr slot = rx_sp() + 4;
    SU32(slot, 0, ev);
    if (U32(ev, 8) == 0) return;
    world_frame f = world_frame_flat(3, 0);
    enter_cs(list + 0x18);
    WORLD_CALL_SP(4, rx_world_vector_push_back(list, slot));
    SU32(ev, 8, 0);
    leave_cs(list + 0x18);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080c400, rx_world_event_pool_give)

/* ---- events --------------------------------------------------------------------------------------------------------- */

/* 0080c280 (cdecl command, event, a, b; EBP = the caller's, saved by the prologue): queues an event command
 * (Audio_PostCommand 0x13) unless the event is busy or the audio system is shutting down. Its frame (the saved EBP, the five
 * pushed arguments, the return address 0x80c2b6) is left as the original leaves it: a particle emitter later copies this
 * stack uninitialized into its records. */
void rx_world_event_queue(uint32_t cmd, rx_addr ev, uint32_t a, uint32_t b, uint32_t ebp) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, ebp);                 /* push ebp; mov ebp, esp; and esp, -8 */
    rx_set_sp((sp0 - 4) & ~7u);
    if (!event_busy(ev) && U32(0, 0x009b7430) == 0) {
        WORLD_PUSHED(0x0080c2b6, 0x13, ev, a, b, cmd);
        WORLD_CALL_SP(0x14, rx_world_audio_post_command(0x13, ev, a, b, cmd));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0080c280, rx_world_event_queue)

/* 0080a990 (ESI = event; stdcall 6 words; RET 0x18): its callback data under its lock (+0x188 / +0x18c / +0x194 / +0x198 /
 * +0x19c / +0x184; +0x190 = 0). */
void rx_world_event_set_callback(rx_addr ev, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t g) {
    world_frame f = world_frame_flat(1, 0);
    enter_cs(ev + 0x148);
    SU32(ev, 0x188, a); SU32(ev, 0x18c, b); SU32(ev, 0x194, c); SU32(ev, 0x190, 0);
    SU32(ev, 0x198, d); SU32(ev, 0x19c, e); SU32(ev, 0x184, g);
    leave_cs(ev + 0x148);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080a990, rx_world_event_set_callback)

/* 0080a810 (ESI = event, EBP = the caller's, passed on; RET 0): releases the event's FMOD instance: on the audio thread 0080a890, else queued (command 0xc;
 * a state-1 event waits for it: 00805780); its id +0x124 = -1. */
void rx_world_event_release_instance(rx_addr ev, uint32_t ebp) {
    world_frame f = world_frame_flat(1, 0);
    if (on_audio_thread(WCTX(.esi = ev, .edi = U32(0, 0x00d67d1c)))) {
        AUDIO_BOUNDARY_CTX0(0x0080a890, &WCTX(.eax = ev, .esi = ev), 0);
    } else {
        const uint32_t s = U32(ev, 0x138);
        if (s != 2 && s != 1) WORLD_CALL_SP(0x10, rx_world_event_queue(0xc, ev, 0, 0, ebp));
        else if (s == 1) AUDIO_BOUNDARY_CTX(0x00805780, &WCTX(.eax = 0xc, .ecx = 0, .esi = ev), 0, 0x13, ev, 0);   /* Audio_PostCommandWait */
    }
    SU32(ev, 0x124, 0xffffffffu);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080a810, rx_world_event_release_instance)

/* 0080a770 (EDI = event; RET 0): stops the event: an owned one drops its FMOD event (command 0x16 when the system runs) and its
 * state; else it leaves its channel (00807360), stops through its vfunc 1 (0, 0) and releases its instance. Then the name is
 * cleared and the volume reset to 1. */
void rx_world_event_stop(rx_addr ev) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(((sp0 - 4) & ~7u) - 8);   /* push ebp; and esp, -8; push ebx; push esi */
    if (U32(ev, 8) != 0) {
        const rx_addr fe = U32(ev, 0x118);
        if (fe != 0) {
            if (U32(0, 0x009b7430) == 0) WORLD_CALL_SP(0x14, rx_world_audio_post_command(0x16, fe, 0, 0, 0));
            SU32(ev, 0x118, 0);
            SU32(ev, 0x124, 0xffffffffu); SU32(ev, 0x128, 0xffffffffu);
            SU32(ev, 0x12c, 0); SU32(ev, 0x138, 0);
        }
    } else {
        const rx_addr ch = U32(ev, 0x1a0);
        if (ch != 0) { AUDIO_BOUNDARY_CTX(0x00807360, &WCTX(.esi = ch, .edi = ev), 0, ev); SU32(ev, 0x1a0, 0); }
        if (U32(ev, 0x118) != 0) WORLD_CTX(U32(U32(ev, 0), 4), &WCTX(.ecx = ev, .edi = ev), 0, 0, 0);
        WORLD_CALL_SP(0, rx_world_event_release_instance(ev, sp0 - 4));   /* EBP: this frame */
    }
    SU8(ev, 0x18, 0);
    SF32(ev, 0x11c, 1.0f /* 0x931f24 */);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0080a770, rx_world_event_stop)

/* 0080c970 (ESI = event; stdcall pool; RET 4): returns an event to its pool: when it is one of the pool's events, its callback
 * data is cleared, a named event is stopped, and an owned one goes to the free list (+0x10 = 0; a tail call of 0080c400). */
void rx_world_event_pool_return(rx_addr ev, rx_addr pool) {
    const rx_addr entry = rx_sp();
    if (ev == 0 || U32(pool, 0) == 0) return;
    world_frame f = world_frame_flat(4, 0);
    uint32_t i = 0;
    for (; i < U32(pool, 4); i++) if (U32(pool, 0) + i * 0x1a4 == ev) break;
    if (U32(pool, 4) == 0 || i >= U32(pool, 4)) { world_frame_leave(&f); return; }
    enter_cs(ev + 0x148);
    SU32(ev, 0x188, 0); SU32(ev, 0x18c, 0); SU32(ev, 0x190, 0); SU32(ev, 0x194, 0); SU32(ev, 0x198, 0); SU32(ev, 0x19c, 0);
    SU32(ev, 0x184, 0);
    leave_cs(ev + 0x148);
    const rx_addr e = U32(pool, 0) + i * 0x1a4;
    SU32(f.esp, 0xc, e + 0x19);
    if (str_len(e + 0x18) != 0) WORLD_CALL_SP(0, rx_world_event_stop(e));
    if (U32(ev, 8) == 0) { world_frame_leave(&f); return; }
    SU32(ev, 0x10, 0);
    rx_set_sp(entry);   /* a tail call at the entry stack pointer: the argument slot now holds the event */
    rx_world_event_pool_give(pool + 8, U32(pool, 0) + i * 0x1a4);
}
RX_FUNCTION(0x0080c970, rx_world_event_pool_return)

/* 0080c810 (EAX = event; stdcall pool; RET 4): releases an event of the given pool: callback data cleared, then 0080c970. */
void rx_world_event_release(rx_addr ev, rx_addr pool) {
    if (ev == 0 || U32(pool, 0) == 0 || pool != U32(ev, 0xc)) return;
    world_frame f = world_frame_flat(4, 0);
    enter_cs(ev + 0x148);
    SU32(ev, 0x188, 0); SU32(ev, 0x18c, 0); SU32(ev, 0x190, 0); SU32(ev, 0x194, 0); SU32(ev, 0x198, 0); SU32(ev, 0x19c, 0);
    SU32(ev, 0x184, 0);
    leave_cs(ev + 0x148);
    WORLD_CALL_SP(4, rx_world_event_pool_return(ev, pool));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080c810, rx_world_event_release)

/* 0080bce0 AudioEvent_SetChannel (EAX = event; stdcall channel; RET 4): moves the event to a channel group: out of the old one
 * (0080bd40), into the new one's list (00544cc0 on +0x40 under its lock +0x28; the original passes the address of its own argument
 * slot, rewritten with the event), +0x1a0 = the channel, then the event's vfunc 1 (0, channel). */
void rx_world_event_set_channel(rx_addr ev, rx_addr ch) {
    const rx_addr slot = rx_sp() + 4;
    if (ch == 0 || ch == U32(ev, 0x1a0)) return;
    world_frame f = world_frame_flat(2, 0);
    if (U32(ev, 0x1a0) != 0) AUDIO_BOUNDARY_CTX0(0x0080bd40, &WCTX(.eax = U32(ev, 0x1a0), .ebx = ch, .edi = ev), 0);
    rx_set_sp(f.esp - 8);   /* push ebp; push esi */
    enter_cs(ch + 0x28);
    SU32(slot, 0, ev);
    WORLD_CALL_SP(4, (void)rx_world_ptr_vector_push_unique(ch + 0x40, slot));   /* 00544cc0 */
    leave_cs(ch + 0x28);
    SU32(ev, 0x1a0, ch);
    WORLD_CTX(U32(U32(ev, 0), 4), &WCTX(.ecx = ev, .ebx = ch, .ebp = ch + 0x28, .esi = ch + 0x40, .edi = ev), 0, 0, ch);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080bce0, rx_world_event_set_channel)

/* 0080c760 (EAX = event system; stdcall name, tag; RET 8; EAX): a free event of the system's pool prepared for a sound: callback
 * data (0x80ca40 / 0x80c950, owner = the system), its old instance released, the name copied (strncpy 0xff, IAT 0x916260), the
 * tag; joins the default channel group 0xd679f4 when it has none. 0 when the system, the name or the pool is empty. */
rx_addr rx_world_event_create(rx_addr sys, rx_addr name, uint32_t tag) {
    if (U32(sys, 0) == 0 || name == 0 || str_len(name) == 0) return 0;
    if (((U32(sys, 0x18) - U32(sys, 0x14)) & 0xfffffffcu) == 0) return 0;
    world_frame f = world_frame_flat(3, 0);
    rx_addr ev;
    WORLD_CALL_SP(0, ev = rx_world_event_pool_take(sys + 8));
    if (ev != 0) {
        WORLD_CALL_SP(0x18, rx_world_event_set_callback(ev, 0x0080ca40, 0x0080c950, 0, 0, 0, sys));
        WORLD_CALL_SP(0, rx_world_event_release_instance(ev, sys));   /* EBP = sys (mov ebp, [esp+8]) */
        crt_strncpy(ev + 0x18, name, 0xff);   /* strncpy (IAT 0x916260) */
        SU8(ev, 0x117, 0);
        SU32(ev, 0x12c, 0);
        SU32(ev, 0x134, tag);
        if (U32(ev, 0x1a0) == 0) WORLD_CALL_SP(4, rx_world_event_set_channel(ev, 0x00d679f4));
    }
    world_frame_leave(&f);
    return ev;
}
RX_FUNCTION(0x0080c760, rx_world_event_create)

/* 0080a9f0 (ESI = event, EBP = the caller's, passed on; RET 0): starts a named event without an instance: state 3 (unless busy), then 0080aaa0 on the audio
 * thread (a tail call) or the queued command 3. */
void rx_world_event_start(rx_addr ev, uint32_t ebp) {
    const rx_addr entry = rx_sp();
    if (U32(ev, 0x118) != 0 || str_len(ev + 0x18) == 0) return;
    if (!event_busy(ev)) SU32(ev, 0x138, 3);
    world_frame f = world_frame_flat(1, 8);
    if (on_audio_thread(WCTX(.esi = ev, .edi = U32(0, 0x00d67d1c)))) {
        world_frame_leave(&f);
        rx_set_sp(entry);
        AUDIO_BOUNDARY_CTX0(0x0080aaa0, &WCTX(.eax = ev, .esi = ev), 0);
        return;
    }
    WORLD_CALL_SP(0x10, rx_world_event_queue(3, ev, 0, 0, ebp));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080a9f0, rx_world_event_start)

/* 0080b460 (EDI = event, EBP = the caller's, passed on; RET 0): applies the event's volume: on the audio thread a tail call of 0080b490, else queued (2). */
void rx_world_event_apply_volume(rx_addr ev, uint32_t ebp) {
    world_frame f = world_frame_flat(1, 0);
    const int audio = on_audio_thread(WCTX(.esi = U32(0, 0x00d67d1c), .edi = ev));
    world_frame_leave(&f);
    if (audio) { rx_audio_event_apply_volume(ev); return; }   /* AudioEvent_ApplyVolume 0080b490 (race_sound.c): a tail jump */
    WORLD_CALL_SP(0x10, rx_world_event_queue(2, ev, 0, 0, ebp));
}
RX_FUNCTION(0x0080b460, rx_world_event_apply_volume)

/* 0080abd0 AudioEvent_Play (ESI = event; stdcall a, b, c; RET 0xc): on the audio thread 0080aca0 (ECX = event, EAX = a; b, c);
 * else the play command (0x13 with a {a, ?, b, c} block on the stack) unless busy or shutting down. */
void rx_world_event_play(rx_addr ev, uint32_t a, uint32_t b, uint32_t c) {
    const rx_addr sp0 = rx_sp();
    const rx_addr base = ((sp0 - 4) & ~7u) - 0x10 - 8;   /* push ebp; and esp, -8; sub esp, 0x10; push ebx; push edi */
    rx_set_sp(base);
    if (on_audio_thread(WCTX(.ebx = c, .esi = ev, .edi = U32(0, 0x00d67d1c)))) {
        AUDIO_BOUNDARY_CTX(0x0080aca0, &WCTX(.ecx = ev, .eax = a, .ebx = c, .esi = ev), 0, b, c);
    } else {
        SU32(base, 8, a); SU32(base, 0x10, b); SU32(base, 0x14, c);
        if (!event_busy(ev) && U32(0, 0x009b7430) == 0) WORLD_CALL_SP(0x14, rx_world_audio_post_command(0x13, ev, base + 8, 0, 0));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0080abd0, rx_world_event_play)

/* 0080ae80 (EDI = position, EBP = the caller's, passed on; stdcall event; RET 4): the event's 3D position: on the audio thread 0080aee0, else queued (5, with a
 * {position, 0, 0} block). */
void rx_world_event_set_position(rx_addr pos, rx_addr ev, uint32_t ebp) {
    world_frame f = world_frame_flat(2, 0x10);
    const rx_addr base = f.esp;
    if (on_audio_thread(WCTX(.ebx = ev, .esi = U32(0, 0x00d67d1c), .edi = pos))) {
        AUDIO_BOUNDARY_CTX(0x0080aee0, &WCTX(.eax = 0, .ecx = 0, .ebx = ev, .edi = pos), 0, ev, pos);
    } else {
        SU32(base, 0xc, 0); SU32(base, 0x10, 0); SU32(base, 8, pos);
        WORLD_CALL_SP(0x10, rx_world_event_queue(5, ev, base + 8, 0, ebp));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080ae80, rx_world_event_set_position)

/* 0080b500 AudioEvent_IsPlaying (ECX = event; EAX): its FMOD event exists (+0x118) and its flags (+0x13c) have bit 3. */
uint32_t rx_world_event_is_playing(rx_addr ev) {
    return (U32(ev, 0x118) != 0 && (U8(ev, 0x13c) & 8) != 0) ? 1u : 0u;
}
RX_FUNCTION(0x0080b500, rx_world_event_is_playing)

/* 0080b310 (EAX = event, EBP = the caller's, passed on; stdcall min, max distance; RET 8): the event's attenuation range: on the audio
 * thread 0080b360 (ESI = event; stdcall min, max through the x87: the FMOD property leaf, not in the core: audio), else the queued
 * command 6 with the addresses of the two argument slots. Frame: push esi, edi. */
void rx_world_event_set_range(rx_addr ev, uint32_t min_bits, uint32_t max_bits, uint32_t ebp) {
    const rx_addr slots = rx_sp() + 4;
    world_frame f = world_frame_flat(2, 0);
    if (on_audio_thread(WCTX(.esi = ev, .edi = U32(0, 0x00d67d1c)))) {
        AUDIO_BOUNDARY_CTX(0x0080b360, &WCTX(.esi = ev, .edi = U32(0, 0x00d67d1c)), 0, world_x87_pass(min_bits), world_x87_pass(max_bits));
    } else {
        WORLD_CALL_SP(0x10, rx_world_event_queue(6, ev, slots, slots + 4, ebp));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0080b310, rx_world_event_set_range)

/* 0080afc0 (EDI = event; stdcall parameter, value; RET 8): an event parameter: on the audio thread 0080b030 (EAX = event), else
 * the queued command (0x13 with the address of the value's argument slot, kind 4) unless busy or shutting down. */
void rx_world_event_set_parameter(rx_addr ev, uint32_t param, float value) {
    const rx_addr value_slot = rx_sp() + 8;
    SF32(value_slot, 0, value);
    const rx_addr sp0 = rx_sp();
    rx_set_sp(((sp0 - 4) & ~7u) - 8);   /* push ebp; and esp, -8; push ebx; push esi */
    if (on_audio_thread(WCTX(.ebx = param, .esi = U32(0, 0x00d67d1c), .edi = ev)))
        AUDIO_BOUNDARY_CTX(0x0080b030, &WCTX(.eax = ev, .ebx = param, .edi = ev), 0, param, world_x87_pass(rx_fb(value)));
    else if (!event_busy(ev) && U32(0, 0x009b7430) == 0)
        WORLD_CALL_SP(0x14, rx_world_audio_post_command(0x13, ev, param, value_slot, 4));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0080afc0, rx_world_event_set_parameter)

/* 0080bc80 FMODEvent::vf1 (thiscall event; stdcall command 0..4, ?; EBP = the caller's, passed on; RET 8): 0 volume (0080b460), 1 / 2 pause (0080b540 with
 * command == 1), 3 / 4 mute (0080b5e0 with command == 3). */
void rx_world_fmod_event_vf1(rx_addr ev, uint32_t cmd, uint32_t unused, uint32_t ebp) {
    (void)unused;
    if (cmd > 4) return;
    if (cmd == 0) { world_frame f = world_frame_flat(1, 0); WORLD_CALL_SP(0, rx_world_event_apply_volume(ev, ebp)); world_frame_leave(&f); }
    else if (cmd <= 2) AUDIO_BOUNDARY_CTX(0x0080b540, &WCTX(.eax = ev, .edx = cmd), 0, cmd == 1);
    else AUDIO_BOUNDARY_CTX(0x0080b5e0, &WCTX(.eax = ev, .edx = cmd), 0, cmd == 3);
}
RX_FUNCTION(0x0080bc80, rx_world_fmod_event_vf1)

/* 00805570 (cdecl enabled, format, ...): the audio debug log: formats into a stack buffer (vsnprintf, IAT 0x9162ac) and drops it. */
void rx_world_audio_log(uint32_t enabled, rx_addr fmt) {
    const rx_addr args = rx_sp();
    world_frame f = world_frame_flat(1, 0x10c);
    const rx_addr buf = f.esp + 8;
    if (enabled != 0) {
        SU8(buf, 0, 0);
        crt_memset(buf + 1, 0, 0x103);   /* memset (thunk 0x8fa06a) */
        if (fmt != 0 && str_len(fmt) != 0) {
            int32_t n;
            CRT_WITH_REGS(0, 0, fmt, 0, n = crt_vsnprintf(buf, 0x104, fmt, args + 0xc));   /* _vsnprintf (IAT 0x9162ac) */
            if (n > 0 && (uint32_t)n < 0x104) SU8(buf, (uint32_t)n, 0);
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00805570, rx_world_audio_log)

/* 0080da40 (stdcall left, parent, right, value*; RET 0x10; EAX): a new std::map node (0x14 bytes, ReflexAlignedAlloc): links,
 * the value, colour 0, not nil. (The original's SEH frame leaves nothing behind.) */
rx_addr rx_world_map_node_new(uint32_t left, uint32_t parent, uint32_t right, rx_addr value) {
    world_frame f = world_frame_flat(9, 0);
    rx_addr node;
    WORLD_CALL_SP(4, node = rx_world_aligned_alloc(0x14));
    if (node != 0) {
        SU32(node, 0, left); SU32(node, 4, parent); SU32(node, 8, right);
        SU32(node, 0xc, U32(value, 0));
        SU8(node, 0x10, 0); SU8(node, 0x11, 0);
    }
    world_frame_leave(&f);
    return node;
}
RX_FUNCTION(0x0080da40, rx_world_map_node_new)

/* 00803ed0 (RET 0; EAX): the next queued audio command block (the queue [0xa95814]: +0 count, +4 capacity, +8 head, +0xc
 * items; under the lock 0xd67fd0), its kind's pending counter (0xa81ab8) decremented; 0 when the queue is empty. */
rx_addr rx_world_audio_queue_pop(void) {
    world_frame f = world_frame_flat(1, 0);
    rx_addr cmd = 0;
    if (U32(U32(0, 0x00a95814), 0) != 0) {
        enter_cs(0x00d67fd0);
        const rx_addr q = U32(0, 0x00a95814);
        uint32_t n = U32(q, 0);
        if (n != 0) {
            uint32_t head = U32(q, 8);
            cmd = U32(U32(q, 0xc), head * 4);
            head++; n--;
            SU32(q, 0, n);
            SU32(q, 8, head);
            if ((int32_t)head >= I32(q, 4)) SU32(q, 8, 0);
        }
        const uint32_t kind = U32(cmd, 0);
        SU32(0, 0x00a81ab8 + kind * 4, U32(0, 0x00a81ab8 + kind * 4) - 1);
        leave_cs(0x00d67fd0);
    }
    world_frame_leave(&f);
    return cmd;
}
RX_FUNCTION(0x00803ed0, rx_world_audio_queue_pop)

/* ---- small virtuals -------------------------------------------------------------------------------------------------- */
/* 007a0760 rtti_shared_vf_007a0760: returns 0. */
uint32_t rx_world_shared_vf_ret0(void) { return 0; }
RX_FUNCTION(0x007a0760, rx_world_shared_vf_ret0)
/* 007e3be0 rtti_shared_vf_007e3be0: an empty virtual. */
void rx_world_shared_vf_empty(void) {}
RX_FUNCTION(0x007e3be0, rx_world_shared_vf_empty)
/* 00819cc0 CollisionEntity::vf13 (thiscall entity): +0x150. */
uint32_t rx_world_collision_entity_vf13(rx_addr e) { return U32(e, 0x150); }
RX_FUNCTION(0x00819cc0, rx_world_collision_entity_vf13)
/* 008b98c0 (cdecl out): out = {the IAT 0x9164b8 result (a 32-bit counter), 0}; EAX = out. */
rx_addr rx_world_int64_from_counter(rx_addr out) {
    const uint32_t v = RX_OS0(0x009164b8);   /* winmm timeGetTime */
    SU32(out, 0, v);
    SU32(out, 4, 0);
    return out;
}
RX_FUNCTION(0x008b98c0, rx_world_int64_from_counter)
/* 008519a0 (EAX = string map {+0x18 head}; stdcall key; RET 4; EAX): lower_bound by the key compare (IAT 0x9161bc: node key < key
 * goes right); nodes: +0 left, +8 right, +0xc the key string, +0x2d nil. */
rx_addr rx_world_string_map_lower_bound(rx_addr map, rx_addr key) {
    world_frame f = world_frame_flat(3, 0);
    const rx_addr head = U32(map, 0x18);
    rx_addr result = head, x = U32(head, 4);
    if (U8(x, 0x2d) == 0) {
        rx_set_sp(f.esp - 4);   /* push ebx */
        const rx_addr less = U32(0, 0x009161bc);
        while (U8(x, 0x2d) == 0) {
            uint32_t lt;
            CRT_WITH_REGS(less, key, x, result, lt = crt_str_less(x + 0xc, key));   /* 78495979 std::operator< */
            if (lt & 0xff) x = U32(x, 8);
            else { result = x; x = U32(x, 0); }
        }
    }
    world_frame_leave(&f);
    return result;
}
RX_FUNCTION(0x008519a0, rx_world_string_map_lower_bound)
/* 00830130 (thiscall object; stdcall a; RET 4; EAX): the result of 007fa030 on +0x248 (ESI) with a zeroed local. */
uint32_t rx_world_object_query_248(rx_addr obj, uint32_t a) {
    const rx_addr a_slot = rx_sp() + 4;   /* 007fa030 gets the address of the argument (EAX) */
    SU32(a_slot, 0, a);
    world_frame f = world_frame_flat(2, 0);
    const rx_addr local = f.esp + 4;
    SU32(local, 0, 0);
    WORLD_CALL_SP(4, (void)rx_world_string_table_find(obj + 0x248, a_slot, local));   /* 007fa030 */
    const uint32_t r = U32(local, 0);
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x00830130, rx_world_object_query_248)

/* ---- the command queue to the audio thread --------------------------------------------------------------------------
 * Command blocks (0x58 bytes): +0 kind (0 when free), +4.. the arguments. Free blocks: 0xa81b20 (count 0xa819d4, lock 0xd67cec).
 * The queue [0xa95814] (+0 count, +4 capacity, +8 head, +0xc ring; lock 0xd67fd0); pending counters per kind 0xa81ab8; the audio
 * thread is woken through the event [0xd67ce8] (SetEvent IAT 0x916034, GetLastError 0x916078). Audio_ExecuteCommand 00806110
 * (EAX = block) returns nonzero when it ran the command. */
#include "phys_crt.h"

static rx_addr free_block_pop(void) {
    rx_addr b = 0;
    enter_cs(0x00d67cec);
    const uint32_t n = U32(0, 0x00a819d4);
    if (n != 0) { b = U32(U32(0, 0x00a81b20), n * 4 - 4); SU32(0, 0x00a819d4, n - 1); }
    leave_cs(0x00d67cec);
    return b;
}
static void free_block_push(rx_addr b) {
    enter_cs(0x00d67cec);
    const uint32_t n = U32(0, 0x00a819d4);
    SU32(U32(0, 0x00a81b20), n * 4, b);
    SU32(0, 0x00a819d4, n + 1);
    leave_cs(0x00d67cec);
}
/* a block into the ring (overwriting the oldest when full), its kind's counter up, the audio thread woken */
static void queue_push(rx_addr b) {
    enter_cs(0x00d67fd0);
    SU32(0, 0x00a81ab8 + U32(b, 0) * 4, U32(0, 0x00a81ab8 + U32(b, 0) * 4) + 1);
    const rx_addr q = U32(0, 0x00a95814);
    const int32_t count = I32(q, 0), cap = I32(q, 4);
    const int full = count == cap;
    SU32(U32(q, 0xc), 4u * (uint32_t)((I32(q, 8) + count) % cap), b);
    if (full) SU32(q, 8, (uint32_t)((I32(q, 8) + 1) % I32(q, 4)));
    else SU32(q, 0, (uint32_t)(count + 1));
    leave_cs(0x00d67fd0);
    if (RX_OS(0x00916034, U32(0, 0x00d67ce8)) == 0) RX_OS0(0x00916078);
}
static uint32_t execute_command(rx_addr b) { return AUDIO_BOUNDARY_CTX0(0x00806110, &WCTX(.eax = b, .ebx = b, .edi = b), 0); }
static float pass_f(rx_addr p) { return rx_bf(world_x87_pass(U32(p, 0))); }

/* x87 copy of a float (fld / fstp: a signalling NaN comes out quiet) */
static void copy_x87(rx_addr dst, uint32_t off, rx_addr src) { SU32(dst, off, world_x87_pass(U32(src, 0))); }

/* 0080c2c0 (ECX = event command 0..11, ESI = the record, EAX = a, EDI = b; switch 0x80c3d0): the event command's arguments.
 *   0: optional position (a[0] -> +8 vector, flag +0x34), a[2] -> +0x14 (flag +0x2c), a[3] -> +0x18 (flag +0x30)
 *   4: the name a (strncpy 0x3f to +8, terminated) and the float *b at +0x48
 *   5: optional position / velocity / third vector: a[0] -> +8 (flag +0x34), a[1] -> +0x14 (+0x38), a[2] -> +0x20 (+0x3c)
 *   6, 10: the floats *a, *b at +8, +0xc;   7, 8: the dword *a at +8;   9: the dwords *a, *b at +8, +0xc;   11: a at +0x40
 *   1..3: nothing */
void rx_world_audio_command_args(uint32_t kind, rx_addr rec, rx_addr a, rx_addr b) {
    switch (kind) {
    case 0:
        if (a == 0) return;
        if (U32(a, 0) != 0) {
            SU32(rec, 0x34, 1);
            copy_x87(rec, 0x08, U32(a, 0)); copy_x87(rec, 0x0c, U32(a, 0) + 4); copy_x87(rec, 0x10, U32(a, 0) + 8);
        }
        if (U32(a, 8) != 0) { SU32(rec, 0x2c, 1); copy_x87(rec, 0x14, U32(a, 8)); }
        if (U32(a, 0xc) != 0) { SU32(rec, 0x30, 1); copy_x87(rec, 0x18, U32(a, 0xc)); }
        return;
    case 4:
        crt_strncpy(rec + 8, a, 0x3f);   /* strncpy (IAT 0x916260) */
        SU8(rec + 8, 0x3f, 0);
        copy_x87(rec, 0x48, b);
        return;
    case 5:
        if (a == 0) return;
        for (int i = 0; i < 3; i++) {
            const rx_addr v = U32(a, 4 * i);
            if (v == 0) continue;
            SU32(rec, 0x34 + 4 * i, 1);
            copy_x87(rec, 0x08 + 0xc * i, v); copy_x87(rec, 0x0c + 0xc * i, v + 4); copy_x87(rec, 0x10 + 0xc * i, v + 8);
        }
        return;
    case 6: case 10: copy_x87(rec, 8, a); copy_x87(rec, 0xc, b); return;
    case 7: case 8: SU32(rec, 8, U32(a, 0)); return;
    case 9: SU32(rec, 8, U32(a, 0)); SU32(rec, 0xc, U32(b, 0)); return;
    case 11: SU32(rec, 0x40, a); return;
    default: return;
    }
}
RX_FUNCTION(0x0080c2c0, rx_world_audio_command_args)

/* 00805860 Audio_PostCommand (cdecl kind, a1, a2, a3, a4; EAX = the block): a free command block (on the audio thread, when none
 * is free, queued commands are run until one frees a block; elsewhere the caller sleeps 1 ms until one is), cleared and filled
 * per kind (1..24: the arguments, copies through pointers, the event names of kinds 5 / 7 (strncpy 0x3f, then 00806c10)); kinds
 * 2 / 3 also clear / set the shutdown flag 0x9b7430. On the audio thread the command runs at once and the block is freed; else it
 * is queued. */
rx_addr rx_world_audio_post_command(uint32_t kind, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(((sp0 - 4) & ~7u) - 0xc - 0xc);   /* push ebp; and esp, -8; sub esp, 0xc; 3 pushes */
    const rx_addr P = rx_sp();
    rx_addr b = free_block_pop();
    if (b == 0) {
        if (on_audio_thread(WCTX(.ebx = 0, .edi = U32(0, 0x00d67d1c)))) {
            SU32(P, 0x10, 0);
            for (;;) {
                rx_addr cmd = 0;
                if (U32(U32(0, 0x00a95814), 0) != 0) WORLD_CALL_SP(0, cmd = rx_world_audio_queue_pop());
                b = cmd;
                if (execute_command(cmd) != 0) { SU32(cmd, 0, 0); break; }
                queue_push(cmd);
                const uint32_t i = U32(P, 0x10) + 1;
                SU32(P, 0x10, i);
                if ((int32_t)i >= I32(U32(0, 0x00a95814), 0)) break;
            }
        } else {
            while (b == 0) {
                RX_OS(0x00916020, 1);   /* Sleep(1) */
                b = free_block_pop();
            }
        }
    }
    crt_memset(b, 0, 0x58);   /* memset (thunk 0x8fa06a) */
    switch (kind) {
    case 1: SU32(b, 0, kind); SU32(b, 0x14, a1); break;
    case 2: SU32(b, 0, kind); SU32(0, 0x009b7430, 0); break;
    case 3: SU32(b, 0, kind); SU32(b, 4, a1); SU32(0, 0x009b7430, 1); break;
    case 4: case 6: case 8: SU32(b, 0, kind); SU32(b, 4, a1); SU32(b, 0x14, a2); break;
    case 5: case 7: {   /* an event description: its ids and its name */
        SU32(b, 0, kind); SU32(b, 4, a1); SU32(b, 0x54, a2);
        const int five = kind == 5;
        SU32(b, 0x48, U32(a1, five ? 0x24 : 0x60));
        SU32(b, 0x50, U32(a1, five ? 0x2c : 0x5c));
        SU32(b, 0x4c, U32(a1, five ? 0x28 : 0x58));
        crt_strncpy(b + 8, a1 + (five ? 0x30u : 0x10u), 0x3f);   /* strncpy (IAT 0x916260) */
        SU8(b, 8 + 0x3f, 0);
        AUDIO_BOUNDARY_CTX0(0x00806c10, &WCTX(.esi = b + 8, .ebx = b, .edi = a1), 0);
        break;
    }
    case 9: SU32(b, 0, kind); SU32(b, 0xc, U32(a1, 0)); SF32(b, 8, pass_f(a2)); break;
    case 10: SU32(b, 0, kind); SU32(b, 4, a1); SU32(b, 0x14, a2); SF32(b, 8, pass_f(a3)); break;
    case 11: case 15: SU32(b, 0, kind); SU32(b, 0x14, a1); SU32(b, 4, a2); if (a3 != 0) SU32(b, 0xc, U32(a3, 0)); break;
    case 12: SU32(b, 4, a1); SU32(b, 0, kind); SU32(b, 0xc, U32(a2, 0)); break;
    case 13: case 17:
        SU32(b, 0, kind); SU32(b, 4, a1);
        SF32(b, 8, pass_f(a2)); SF32(b, 0xc, pass_f(a2 + 4)); SF32(b, 0x10, pass_f(a2 + 8));
        SF32(b, 0x14, pass_f(a3)); SF32(b, 0x18, pass_f(a4));
        break;
    case 14: case 18: case 20: SU32(b, 0, kind); SU32(b, 4, a1); break;
    case 16: SU32(b, 0, kind); SU32(b, 4, a1); SU32(b, 0xc, U32(a2, 0)); break;
    case 19:
        SU32(b, 0, kind); SU32(b, 4, a1); SU32(b, 8, a4);
        if (a4 <= 0xb) rx_world_audio_command_args(a4, b + 4, a2, a3);
        break;
    case 21: SU32(b, 0, kind); SU32(b, 0x10, a1); SU32(b, 0x14, a2); break;
    case 22: SU32(b, 0, kind); SU32(b, 0x14, a1); break;
    case 23: SU32(b, 0, kind); SF32(b, 8, pass_f(a1)); break;
    case 24: SU32(b, 0, kind); SU32(b, 0xc, U32(a1, 0)); break;
    default: break;
    }
    if (on_audio_thread(WCTX(.ebx = b, .edi = U32(0, 0x00d67d1c)))) {
        execute_command(b);
        SU32(b, 0, 0);
        free_block_push(b);
    } else queue_push(b);
    rx_set_sp(sp0);
    return b;
}
RX_FUNCTION(0x00805860, rx_world_audio_post_command)

/* 00805ff0 Audio_ProcessQueue (RET 0): the audio thread drains the queue: each command runs (00806110) and its block is freed,
 * or, when it cannot run yet, it goes back to the queue. */
void rx_world_audio_process_queue(void) {
    world_frame f = world_frame_flat(5, 0);
    rx_addr cmd;
    WORLD_CALL_SP(0, cmd = rx_world_audio_queue_pop());
    while (cmd != 0) {
        if (execute_command(cmd) != 0) { SU32(cmd, 0, 0); free_block_push(cmd); }
        else queue_push(cmd);
        cmd = 0;
        if (U32(U32(0, 0x00a95814), 0) != 0) {
            enter_cs(0x00d67fd0);
            const rx_addr q = U32(0, 0x00a95814);
            uint32_t n = U32(q, 0);
            if (n != 0) {
                uint32_t head = U32(q, 8);
                cmd = U32(U32(q, 0xc), head * 4);
                head++; n--;
                SU32(q, 0, n);
                SU32(q, 8, head);
                if ((int32_t)head >= I32(q, 4)) SU32(q, 8, 0);
            }
            SU32(0, 0x00a81ab8 + U32(cmd, 0) * 4, U32(0, 0x00a81ab8 + U32(cmd, 0) * 4) - 1);
            leave_cs(0x00d67fd0);
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00805ff0, rx_world_audio_process_queue)

/* 00805420 Audio_NearestListener (cdecl position, float* distance out; EAX): the nearest active listener record of the table
 * 0xd67d20 (4 x 0x80: +0 active, +0x10 position), 0 when listeners are not assigned (0xa957dc) or none is active; *out = its
 * distance (when given). */
rx_addr rx_world_audio_nearest_listener(rx_addr pos, rx_addr out) {
    world_frame f = world_frame_enter(0, 4, 1);
    float best = 3.40282347e+38f /* 0x977494 */;
    SF32(f.esp, 0xc, best);
    rx_addr nearest = 0;
    if (U32(0, 0x00a957dc) != 0) {
        for (rx_addr l = 0x00d67d20; l + 0x18 < 0x00d67f38; l += 0x80) {
            if (U32(l, 0) == 0) continue;
            const float dx = phys_sub(F32(l, 0x10), F32(pos, 0)), dz = phys_sub(F32(l, 0x18), F32(pos, 8));
            const float dy = phys_sub(F32(l, 0x14), F32(pos, 4));
            const float d2 = phys_add(phys_add(phys_mul(dx, dx), phys_mul(dz, dz)), phys_mul(dy, dy));
            float d;
            WORLD_CALL_SP(4, d = phys_sqrtf(d2));
            if (F32(f.esp, 0xc) > d) { best = d; SF32(f.esp, 0xc, d); nearest = l; }
        }
        if (nearest != 0 && out != 0) SF32(out, 0, best);
    }
    world_frame_leave(&f);
    return nearest;
}
RX_FUNCTION(0x00805420, rx_world_audio_nearest_listener)

/* the audio log called with its arguments on the game stack (it reads the varargs from there) */
static void audio_log_args(int n, const uint32_t* a) {
    const rx_addr sp = rx_sp();
    for (int i = 0; i < n; i++) SU32(sp - 4u * (uint32_t)(n - i), 0, a[i]);
    rx_set_sp(sp - 4u * (uint32_t)n - 4u);
    rx_world_audio_log(a[0], a[1]);
    rx_set_sp(sp);
}

/* 0080b160 (ESI = event id; cdecl out min, out max; frame 8; EAX): the range of the event's parameter 0x95c0b0: the event
 * from the event system ([0xa957f4] vf19 (id, 4, &event)), its parameter (FMOD::Event::getParameter), the range
 * (FMOD::EventParameter::getRange); each FMOD result through the log; 1 when getRange succeeded, 0 otherwise. */
uint32_t rx_world_event_parameter_range(uint32_t id, rx_addr out_min, rx_addr out_max) {
    world_frame f = world_frame_flat(3, 8);   /* sub esp, 8; push ebx; push ebp; push edi */
    const rx_addr ev = f.esp + 0xc, param = f.esp + 0x10;
    const rx_addr sys = U32(0, 0x00a957f4);
    SU32(ev, 0, 0);
    uint32_t r = WORLD_ORIG(U32(U32(sys, 0), 0x4c), RX_STDCALL, 0, sys, id, 4, ev);
    { const uint32_t a[3] = {r, 0x009735d4, id}; audio_log_args(3, a); }
    uint32_t result = 0;
    if (U32(ev, 0) != 0 && out_min != 0 && out_max != 0) {
        r = AUDIO_BOUNDARY_ORIG(0x00900360, RX_STDCALL, 0, U32(ev, 0), 0x0095c0b0, param);   /* FMOD::Event::getParameter */
        { const uint32_t a[4] = {r, 0x009735f4, id, 0x0095c0b0}; audio_log_args(4, a); }
        if (U32(param, 0) != 0) {
            r = AUDIO_BOUNDARY_ORIG(0x00900390, RX_STDCALL, 0, U32(param, 0), out_min, out_max);   /* FMOD::EventParameter::getRange */
            { const uint32_t a[4] = {r, 0x00973664, id, 0x0095c0b0}; audio_log_args(4, a); }
            result = r == 0;
        }
    }
    world_frame_leave(&f);
    return result;
}
RX_FUNCTION(0x0080b160, rx_world_event_parameter_range)

/* ---- 00892170 the game clock (cdecl out int64, may-step flag; frame push ebp; and esp, -8; sub 0x34; EAX = out): a
 * monotonic millisecond clock. The raw time comes from the clock source [0x9dfa80] (vf1 (&now)), or without one from the
 * performance counter (008b9940, flag 0x9dfaa4) or the tick counter (008b98c0); the clock is raw + offset (0x9dfa88, int64),
 * the last value returned is kept at 0x9dfaa8 (static, atexit 0x915160). The first call starts the clock at 1. A value that
 * would go backwards (or a negative clock) is replaced by last + 1 with the offset re-based, when the flag allows a step;
 * otherwise the call is redone under the clock lock (00892120). */
static int64_t clock_i64(rx_addr p) { return (int64_t)((uint64_t)U32(p, 0) | ((uint64_t)U32(p, 4) << 32)); }
static void clock_set(rx_addr p, int64_t v) { SU32(p, 0, (uint32_t)v); SU32(p, 4, (uint32_t)((uint64_t)v >> 32)); }

rx_addr rx_world_clock_now(rx_addr out, uint32_t may_step) {
    const rx_addr entry = rx_sp(), A = (entry - 4) & ~7u, sp = A - 0x40;
    rx_set_sp(sp);
    const rx_addr now_p = A - 0x30;
    SU32(now_p, 0, 0);
    if ((U32(0, 0x009dfab0) & 1u) == 0) {
        SU32(0, 0x009dfab0, U32(0, 0x009dfab0) | 1u);
        SU32(0, 0x009dfaa8, 0);
        SU32(0, 0x009dfaac, 0);
#ifdef RX_ORACLE
        WORLD_ORIG(0x008f9fbf, RX_CDECL, 0, 0x00915160);   /* _atexit (oracle only; product RX_ATEXIT convention) */
#endif
    }
    const int64_t last = clock_i64(0x009dfaa8), off = clock_i64(0x009dfa88);
    const rx_addr src = U32(0, 0x009dfa80);
    if (src != 0) {
        const rx_addr fn = U32(U32(src, 0), 4);
        void (*fp)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
        if (fp) WORLD_CALL_SP(4, fp(src, now_p)); else WORLD_ORIG(fn, RX_THISCALL, src, now_p);
    } else if (U8(0, 0x009dfaa4) != 0) {
        /* The performance-counter branch. The flag byte 0x9dfaa4 is written only by the static initializer 00910f70 (rx_init_00910f70,
         * build_globals_gen.c) with the result of 00704480, which is `xor al, al; ret`: always 0 (a data xref scan of the exe finds no
         * other writer), so 008b9940 (QueryPerformanceCounter scaled to ms) is never called. BOUNDARY (unreachable) */
        WORLD_ORIG(0x008b9940, RX_CDECL, 0, now_p);
    } else {
        WORLD_CALL_SP(4, rx_world_int64_from_counter(now_p));
    }
    const int64_t now = clock_i64(now_p);
    int64_t r;
    if (U8(0, 0x009dfa84) == 0) {
        clock_set(0x009dfaa8, 1);
        SU8(0, 0x009dfa84, 1);
        clock_set(0x009dfa88, 1 - now);
        r = 1;
    } else {
        int forward;
        if (off < 0 && (uint64_t)(-off) > (uint64_t)now) forward = 0;   /* the clock would be negative */
        else {
            r = (int64_t)((uint64_t)now + (uint64_t)off);
            forward = (uint64_t)r >= (uint64_t)last;
            if (forward) clock_set(0x009dfaa8, r);
        }
        if (!forward) {
            if ((may_step & 0xff) == 0) {
                WORLD_CALL_SP(4, (void)rx_world_clock_now_locked(out));   /* again, under the clock lock (00892120) */
                rx_set_sp(entry);
                return out;
            }
            r = last + 1;
            clock_set(0x009dfa88, r - now);
            clock_set(0x009dfaa8, r);
        }
    }
    clock_set(out, r);
    rx_set_sp(entry);
    return out;
}
RX_FUNCTION(0x00892170, rx_world_clock_now)

/* 00892120 (cdecl out; EAX = out): the clock read again under the clock lock 0x9dfa90 (not taken while byte 0x9df580 is set) with
 * the step allowed (00892170 with may_step = 1): the retry of a clock that would have gone backwards. Frame: push ecx (a zeroed
 * local), push esi. */
rx_addr rx_world_clock_now_locked(rx_addr out) {
    const rx_addr entry = rx_sp();
    rx_set_sp(entry - 4);                                   /* push ecx */
    SU32(entry - 4, 0, 0);
    if (U8(0, 0x009df580) == 0) WORLD_CALL_SP(0, rx_lock_enter(0x009dfa90));
    rx_set_sp(entry - 8);                                   /* push esi */
    WORLD_CALL_SP(8, (void)rx_world_clock_now(out, 1));
    if (U8(0, 0x009df580) == 0) WORLD_CALL_SP(0, rx_lock_leave(0x009dfa90));
    rx_set_sp(entry);
    return out;
}
RX_FUNCTION(0x00892120, rx_world_clock_now_locked)
