/* fx_effect.c -- the placed particle effects of the level scene: PlaceableParticleEffect (vf19 00696940: binds itself to the
 * scene's particle entity's global or additive manager, then emits through its emitter at +0xa0), the timed Fireworks /
 * Triggered variants (00692340 / 00692520, started by 006922c0) and the enable virtual 006968e0 (with the effect's sound).
 * notes/fx.md.
 *
 * Effect (PlaceableParticleEffect, entity): +0x10 scene, +0x2c enabled, +0x30 restart, +0x90 the controller (vtable; +0x14
 * enable(on)), +0x94 the bound manager, +0x9c particle type index, +0xa0 the emitter (vf0 (dt, manager, matrix)), +0xb0
 * delay, +0xb4 asset holder, +0xb8 type, +0xbc running, +0x204 sound, +0x294 sound emitter, +0x2a0 delay left, +0x2a4 /
 * +0x2a8 time left (triggered / fireworks). */
#include "fx.h"
#include "fx_util.h"
#include "world_node.h"
#include "race.h"

/* a thiscall virtual with one 32-bit argument at the current ESP */
static void vcall1(rx_addr obj, rx_addr vt_obj, uint32_t off, uint32_t a0, rx_addr ret) {
    const rx_addr fn = U32(U32(vt_obj, 0), off);
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    const uint32_t a[1] = {a0};
    if (!f) { rx_call_original(fn, RX_THISCALL, obj, 1, a); return; }
    const rx_addr s = fx_push(1, a, ret);
    f(obj, a0);
    fx_pop(s);
}
/* the effect's own vf19 (dt through fld / fstp) at the current ESP */
static void placeable_tick_call(rx_addr e, float dt, rx_addr ret) {
    const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
    const uint32_t a[1] = {rx_fb(d)};
    const rx_addr s = fx_push(1, a, ret);
    rx_fx_placeable_effect_tick(e, d);
    fx_pop(s);
}

/* ---- 00678980 (EAX = the sound emitter, an audio event: +0x118 its FMOD event, +0x13c / +0x144 flags, +0x1b0 the position, +0x1c0
 * "2D" (no position updates), +0x1c4 the previous "bit 1 of +0x144", +0x1c8 audible range set, +0x1cc / +0x1d0 the attenuation
 * min / max, +0x1d8 a pending start): the placed effect's looping sound, started / stopped by the listener's distance. Not the
 * 2D kind: the position is updated; a pending start (+0x1d8): the event started (0080a9f0), its range set (0080b310), played;
 * then the nearest listener's distance d (Audio_NearestListener 00805420, FLT_MAX when none; nothing more without a listener):
 * a 3D emitter (flag bit 3 of +0x144 clear) with a range: inside it (max > d) and not playing (0080b500) -- or flagged by the
 * previous bit and no longer so -- it is (re)started: stopped first when the previous bit was set and is now clear, its range
 * set when it has no FMOD event yet, then played at the position; the other kind: leaves the range (max > d and the previous bit
 * set, now clear) with a stop and a play, or stops (d > max, playing flag +0x13c bit 3), or plays when entering (max > d, not
 * playing). The audio calls go through the world unit's event layer. Frame: push ebp; and esp, -8; push ecx (the distance
 * local), ebx, esi, edi: E = A - 0x10, A = EBP & ~7. ---- */
void rx_fx_sound_emitter_update(rx_addr em) {
    fx_frame f; fx_frame_enter(&f, 4, 8, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr A = f.A;
    SU32(A, -4u, fx_reg(FX_ECX));
    const uint32_t ebp = f.ebp;
    fx_set_reg(FX_ESI, em);
    const uint32_t prev = U32(em, 0x1c4);
    fx_set_reg(FX_EBX, prev);
    SU32(em, 0x1c4, (U32(em, 0x144) & 2) >> 1);
    const rx_addr pos = em + 0x1b0;
    if (U32(em, 0x1c0) == 0) {
        fx_set_reg(FX_EDI, pos);
        const uint32_t a[1] = {em}; const rx_addr s = fx_push(1, a, 0x006789b8);
        rx_world_event_set_position(pos, em, ebp);
        fx_pop(s);
    }
    if (U32(em, 0x1d8) != 0) {
        { const rx_addr s = fx_push(0, 0, 0x006789c6); rx_world_event_start(em, ebp); fx_pop(s); }
        { const uint32_t a[2] = {fx_x87_bits(U32(em, 0x1cc)), fx_x87_bits(U32(em, 0x1d0))}; const rx_addr s = fx_push(2, a, 0x006789e3);
          rx_world_event_set_range(em, a[0], a[1], ebp); fx_pop(s); }
        { const uint32_t a[3] = {0, 0, 0}; const rx_addr s = fx_push(3, a, 0x006789ee); rx_world_event_play(em, 0, 0, 0); fx_pop(s); }
        SU32(em, 0x1d8, 0);
    }
    const rx_addr dist = A - 4u;
    SU32(dist, 0, U32(0, 0x00977494));                /* FLT_MAX */
    fx_set_reg(FX_EDI, pos);
    rx_addr listener;
    { const uint32_t a[2] = {pos, dist}; const rx_addr s = fx_push(2, a, 0x00678a17); listener = rx_world_audio_nearest_listener(pos, dist); fx_pop(s); }
    if (listener == 0) { fx_frame_leave(&f); return; }
    if (U32(em, 0x1c0) == 0 && (U32(em, 0x144) & 8) == 0) {
        if (U32(em, 0x1c8) == 0 || !(F32(em, 0x1d0) > F32(dist, 0))) { fx_frame_leave(&f); return; }
        { fx_set_reg(FX_ECX, em); const rx_addr s = fx_push(0, 0, 0x00678a64); const uint32_t pl = rx_world_event_is_playing(em); fx_pop(s);
          if (pl != 0 && (prev == 0 || U32(em, 0x1c4) != 0)) { fx_frame_leave(&f); return; } }
        if (prev != 0 && U32(em, 0x1c4) == 0) { const rx_addr s = fx_push(0, 0, 0x00678a91); rx_sound_stop(em, ebp); fx_pop(s); }
        if (U32(em, 0x118) == 0) {
            { const rx_addr s = fx_push(0, 0, 0x00678a9f); rx_world_event_start(em, ebp); fx_pop(s); }
            const uint32_t a[2] = {fx_x87_bits(U32(em, 0x1cc)), fx_x87_bits(U32(em, 0x1d0))}; const rx_addr s = fx_push(2, a, 0x00678abc);
            rx_world_event_set_range(em, a[0], a[1], ebp); fx_pop(s);
        }
        { const uint32_t a[3] = {pos, 0, 0}; const rx_addr s = fx_push(3, a, 0x00678ac6); rx_world_event_play(em, pos, 0, 0); fx_pop(s); }
        fx_frame_leave(&f);
        return;
    }
    if (U32(em, 0x1c8) == 0) { fx_frame_leave(&f); return; }
    const float range = F32(em, 0x1d0), d = F32(dist, 0);
    if (range > d && prev != 0 && U32(em, 0x1c4) == 0) {
        { const rx_addr s = fx_push(0, 0, 0x00678b01); rx_sound_stop(em, ebp); fx_pop(s); }
        const uint32_t a[3] = {0, 0, 0}; const rx_addr s = fx_push(3, a, 0x00678b0c); rx_world_event_play(em, 0, 0, 0); fx_pop(s);
        fx_frame_leave(&f);
        return;
    }
    if (d > range && U32(em, 0x118) != 0 && (U8(em, 0x13c) & 8) != 0) {
        const rx_addr s = fx_push(0, 0, 0x00678b30); rx_sound_stop(em, ebp); fx_pop(s);
        fx_frame_leave(&f);
        return;
    }
    if (range > d) {
        fx_set_reg(FX_ECX, em);
        uint32_t pl;
        { const rx_addr s = fx_push(0, 0, 0x00678b43); pl = rx_world_event_is_playing(em); fx_pop(s); }
        if (pl == 0) {
            if (U32(em, 0x118) == 0) { const rx_addr s = fx_push(0, 0, 0x00678b54); rx_world_event_start(em, ebp); fx_pop(s); }
            const uint32_t a[3] = {0, 0, 0}; const rx_addr s = fx_push(3, a, 0x00678b5f); rx_world_event_play(em, 0, 0, 0); fx_pop(s);
        }
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00678980, rx_fx_sound_emitter_update)

/* ---- 00696940 PlaceableParticleEffect::vf19 (thiscall effect, dt; RET 4): its sound emitter's update (00678980, EAX = the
 * emitter) when it has one; unbound: take the scene particle entity's (0xa94264 [scene +0x2cc4]) additive manager (+0xc4)
 * when the type is additive (+0x160), else the global one (+0xc0) after disabling the controller (+0x90 vf5 (0)); then the
 * emitter (+0xa0) vf0 (dt, manager, the effect's world matrix (vf14), 1, -1). Frame: push ebx / ebp / esi / edi. */
void rx_fx_placeable_effect_tick(rx_addr e, float dt) {
    fx_frame f; fx_frame_flat(&f, 0, FX_EBX, FX_EBP, FX_ESI, FX_EDI);
    const rx_addr sp0 = f.sp0;
    fx_set_reg(FX_ESI, e); fx_set_reg(FX_EBX, 0);
    if (U32(e, 0x294) != 0) {
        fx_set_reg(FX_ECX, e);
        const rx_addr s = fx_push(0, 0, 0x00696957);
        rx_fx_sound_emitter_update(U32(e, 0x294));           /* the sound emitter (the world unit's audio event layer) */
        fx_pop(s);
    }
    const rx_addr ent = U32(0x00a94264 + 4u * U32(U32(e, 0x10), 0x2cc4), 0);
    fx_set_reg(FX_EBP, ent);
    if (U32(e, 0x94) == 0) {
        if (ent != 0) {
            const rx_addr holder = U32(ent, 0xb0);
            fx_set_reg(FX_EDI, holder);
            const rx_addr data = U32(holder, 0x44);
            SU32(holder, 0x4c, 1);
            rx_addr mgr;
            if (U8(U32(e, 0x9c) * 0x1f0u + U32(data, 0x10), 0x160) != 0) mgr = U32(ent, 0xc4);
            else {
                vcall1(e + 0x90, e + 0x90, 0x14, 0, 0x006969ad);
                mgr = U32(ent, 0xc0);
            }
            SU32(e, 0x94, mgr);
            SU32(holder, 0x4c, 0);
        }
        if (U32(e, 0x94) == 0) { fx_frame_leave(&f); return; }
    }
    SU32(U32(ent, 0xb0), 0x4c, 0);
    fx_set_reg(FX_EBP, U32(ent, 0xb0)); fx_set_reg(FX_EBX, U32(e, 0xa0)); fx_set_reg(FX_EDI, e + 0xa0);
    /* push -1; push 1 (the emitter call's last two arguments, pushed first); the world matrix: vf14 (no arguments) */
    const rx_addr spc = rx_sp();
    SU32(spc, -4, 0xffffffffu); SU32(spc, -8, 1);
    rx_set_sp(spc - 8);
    rx_addr mat;
    {
        const rx_addr fn = U32(U32(e, 0), 0x38);
        uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
        if (f) { const rx_addr s = fx_push(0, 0, 0x006969e6); mat = f(e); fx_pop(s); }
        else mat = rx_call_original(fn, RX_THISCALL, e, 0, 0);
    }
    {   /* the emitter (+0xa0) vf0 (dt, manager, matrix, 1, -1) */
        const rx_addr em = e + 0xa0, fn = U32(U32(em, 0), 0);
        const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
        const uint32_t mgr = U32(e, 0x94);
        void (*f)(rx_addr, float, rx_addr, rx_addr, uint32_t, uint32_t) =
            RX_FN(fn, void (*)(rx_addr, float, rx_addr, rx_addr, uint32_t, uint32_t));
        const uint32_t a[3] = {rx_fb(d), mgr, mat};
        if (f) { const rx_addr s = fx_push(3, a, 0x006969fc); f(em, d, mgr, mat, 1, 0xffffffffu); fx_pop(s); }
        else { rx_set_sp(spc); const uint32_t a5[5] = {rx_fb(d), mgr, mat, 1, 0xffffffffu}; rx_call_original(fn, RX_THISCALL, em, 5, a5); }
    }
    (void)sp0;
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00696940, rx_fx_placeable_effect_tick)

/* the timed variants: while running (+0xbc): the start delay (+0x2a0) runs down with an update each frame; the time left
 * (+0x2a8 fireworks, +0x2a4 triggered) runs down, at 0 the controller is disabled and a last update runs. Frame: push esi. */
static void timed_effect_tick(rx_addr e, float dt, uint32_t left, rx_addr r1, rx_addr r2, rx_addr r3) {
    fx_frame f; fx_frame_flat(&f, 0, FX_ESI);
    fx_set_reg(FX_ESI, e);
    if (U32(e, 0xbc) != 0) {
        float x = F32(e, 0x2a0);
        if (x > 0.0f) {
            x = rx_subs(x, dt);
            SF32(e, 0x2a0, x);
            placeable_tick_call(e, dt, r1);
        }
        const float y = rx_subs(F32(e, left), dt);
        SF32(e, left, y);
        if (!(0.0f < y) && y == y) {
            vcall1(e + 0x90, e + 0x90, 0x14, 0, r2);
            placeable_tick_call(e, dt, r3);
        }
    }
    fx_frame_leave(&f);
}
/* ---- 00692340 FireworksParticleEffect::vf19 (thiscall, dt; RET 4) */
void rx_fx_fireworks_effect_tick(rx_addr e, float dt) { timed_effect_tick(e, dt, 0x2a8, 0x00692378, 0x006923a9, 0x006923b8); }
RX_FUNCTION(0x00692340, rx_fx_fireworks_effect_tick)
/* ---- 00692520 TriggeredPlaceableParticleEffect::vf19 (thiscall, dt; RET 4) */
void rx_fx_triggered_effect_tick(rx_addr e, float dt) { timed_effect_tick(e, dt, 0x2a4, 0x00692558, 0x00692589, 0x00692598); }
RX_FUNCTION(0x00692520, rx_fx_triggered_effect_tick)

/* ---- 006922c0 (ESI = fireworks effect; stdcall delay; RET 4): start: +0xb0 = +0x2a0 = delay (a positive one enables the
 * controller: +0x90 vf5 (1)); the time left +0x2a8 = (life range + life base of the type) + delay. */
void rx_fx_fireworks_start(float delay, rx_addr e) {
    SF32(e, 0xb0, delay);
    SF32(e, 0x2a0, delay);
    if (delay > 0.0f) vcall1(e + 0x90, e + 0x90, 0x14, 1, 0x006922f2);
    const rx_addr holder = U32(e, 0xb4);
    const rx_addr T = U32(e, 0xb8) * 0x1f0u + U32(U32(holder, 0x44), 0x10);
    SU32(holder, 0x4c, 1);
    SF32(e, 0x2a8, rx_adds(rx_adds(F32(T, 0xc8), F32(T, 0xc4)), delay));
    SU32(holder, 0x4c, 0);
}
RX_FUNCTION(0x006922c0, rx_fx_fireworks_start)

/* GetCurrentThreadId (IAT 0x91607c): oracle: the import; product: the main thread (the core runs every thread's work on it) */
static uint32_t current_thread_id(void) {
#ifdef RX_ORACLE
    return rx_call_original(U32(0, 0x0091607c), RX_STDCALL, 0, 0, 0);
#else
    return U32(0, 0x00d67d1c);
#endif
}

/* ---- 00678bd0 (EAX = the effect's sound event; plain ret): starts the sound: the event started (0080a9f0: ESI = the event, EBP = the caller's), its
 * attenuation range set from its own min / max distance (+0x1cc / +0x1d0, through the x87: 0080b310, EAX = the event, stdcall min, max) and
 * played (AudioEvent_Play 0080abd0: ESI = the event, stdcall 0, 0, 0). Frame: push esi. */
void rx_fx_sound_start(rx_addr snd) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.esi);
    fx_set_reg(FX_ESI, snd);
    rx_set_sp(sp0 - 4);
    { const rx_addr s = fx_push(0, 0, 0x00678bd8); rx_world_event_start(snd, fx_reg(FX_EBP)); fx_pop(s); }
    {
        const uint32_t a[2] = {fx_x87_bits(U32(snd, 0x1cc)), fx_x87_bits(U32(snd, 0x1d0))};
        fx_set_reg(FX_EAX, snd);
        const rx_addr s = fx_push(2, a, 0x00678bf5);
        rx_world_event_set_range(snd, a[0], a[1], fx_reg(FX_EBP));
        fx_pop(s);
    }
    {
        const uint32_t a[3] = {0, 0, 0};
        const rx_addr s = fx_push(3, a, 0x00678c00);
        rx_world_event_play(snd, 0, 0, 0);
        fx_pop(s);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00678bd0, rx_fx_sound_start)

/* ---- 006968e0 PlaceableParticleEffect::vf5_o90 (thiscall effect, on; RET 4): enable: switching on from off sets +0x30;
 * +0x2c = on; the effect's sound (+0x204): on: start it (00678bd0, EAX = sound); off: stop it now on the audio thread
 * ([0xd67d1c] = this thread: 0080ae10) or queue the stop (0080c280 (1, sound, 0, 0)). */
void rx_fx_placeable_effect_enable(rx_addr e, uint32_t on) {
    if (on != 0 && U32(e, 0x2c) == 0) SU32(e, 0x30, 1);
    SU32(e, 0x2c, on);
    const rx_addr snd = U32(e, 0x204);
    const fx_saved sv = fx_save();
    SU32(rx_sp(), -4, sv.esi);                                 /* push esi */
    fx_set_reg(FX_ESI, snd);
    if (snd == 0) { fx_restore(sv); return; }
    if (on != 0) {
        fx_set_reg(FX_EAX, snd); fx_set_reg(FX_ECX, e);
        const rx_addr s = fx_push(0, 0, 0x0069690e);
        rx_fx_sound_start(snd);                                 /* 00678bd0 (EAX = the sound) */
        fx_pop(s);
        fx_restore(sv);
        return;
    }
    if (U32(0, 0x00d67d1c) == current_thread_id()) {
        const rx_addr s = fx_push(0, 0, 0x00696929);
        rx_sound_stop_now(snd);                                 /* 0080ae10 (ESI = the sound event) */
        fx_pop(s);
        fx_restore(sv);
        return;
    }
    const uint32_t a[4] = {1, snd, 0, 0};
    const rx_addr s = fx_push(4, a, 0x00696939);
    rx_world_event_queue(1, snd, 0, 0, fx_reg(FX_EBP));
    fx_pop(s);
    fx_restore(sv);
}
RX_FUNCTION(0x006968e0, rx_fx_placeable_effect_enable)
