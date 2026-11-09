/* engine_startline.c -- the engine sound emitters' start line on the product's side of the audio boundary (agent w-rng, 2026-10-27).
 *
 * Why it is in the core. The original's vehicle sound layer (VehicleAudioManager [0xd92f00], 12 EngineSoundEmitter slots at + 0x520 +
 * k * 0x1b10, the 26-state machine of table 0x00c0a138) is on the host's side of the audio boundary: Unity runs it
 * (OriginalEngineAudio.cs, w-audio). Two of its states draw the GAME's random generator (g_GameRandSeed 0x9cb860, g_GameRandCount
 * 0x9cb864) from inside the game frame: the race intro's start line (StartLine 4, enter 00686e20: the time to the next rev) and its revs
 * (Rev 2, enter 00686870: the revs in a row, a car's rev target). Every later draw of the game (the AI, the particles, the crowd ...)
 * depends on them, so the core makes these draws itself, at the original's points and in the original's order, and the host only plays
 * what the core decided (rx_session_vehicle_audio: rx_vehicle_audio_rider start_line_*).
 *
 * What is ported (the start line's part of each function; everything else of the emitter is the host's):
 *   00681880 the "{EnableStartLineState}" scenario command: every slot with a vehicle: +0x2b8 = 1, +0x2e4 / +0x2e8 / +0x2e0 / +0x2dc = 0,
 *            state 4 (00686250)
 *   0067ecf0 "DisableStartLineState": every slot with a vehicle: +0x2b8 = 0
 *   006818e0 ActivityAudioLogic::vf16 "Pre Exited" (argument 1): +0x2b8 = 0 and the size's first state (a7b1dc)
 *   00686250 the state change (exit, the previous state, enter) as far as the start line: only 2 / 4 have start-line enters
 *   00686e20 StartLine enter: a car's rpm (+0x2dc) when it is not positive, ONE DRAW: +0x2e0 = r * (MaxTime - MinTime) + MinTime
 *   00686ec0 StartLine tick: +0x2b8 off -> the first state; while +0x2e4 < StartLineMaxRevs (or no limit) the countdown, at 0 Rev
 *   00686870 Rev enter: +0x2e4 + 1; ONE DRAW unless StartLineMaxSeqRevs is 1: +0x2e8 = 1 - trunc(-r * MaxSeqRevs); a car: ONE DRAW,
 *            +0x2d8 = vf48 * (r * (MaxPerc - MinPerc) + MinPerc) (x87, 53-bit), its ramp's times (00672c30)
 *   006869a0 Rev tick: +0x2b8 off -> the first state; a car: its ramp (0067f1c0 / 0067f210), at each ramp's end +0x2e8 - 1, at 0
 *            00686220; a bike: when the rev event finished (the wrapper's state 7, +0x474) +0x2e8 - 1, at 0 00686220, else the event again
 *   00686220 back to StartLine (+0x2b8 set) or the first state
 *   00685610 VehicleEngineAudio_Update's checks before the tick that leave the start line: under water, drowned -> Choke (00685b10),
 *            wrecked -> Wreck (00685b50) (both "other" states here)
 *   006855c0 / 00685a40 / 00680b10 the bind: +0xc the engine size, +0x2b8 = 0, the ramp's times (first bind), the size's first state
 *   00684c10 the start-line keys of MXRegistry <Audio><EngineSoundEmitter> into their ConVar globals (the product does not run the
 *            audio init 0067da10; without them StartLineMaxRevs stays -1 and the percentages 0.5 / 0.75 instead of 2 / 0.65 / 0.9)
 * The bike's rev event end is the sound engine's (FMOD's EVENT_FINISHED on the audio thread, the wrapper's state 7 seen at the next
 * tick): the host reports it (rx_session_vehicle_audio_finished) and the core takes it at the next tick, as the original does. In the
 * oracle (FMOD stand-ins: an event plays until stopped) it never comes, so there the emitters stay in Rev after their first rev.
 *
 * State: host-side (the product builds no emitter objects in guest memory); guest memory is read (the tunables, the vehicle's vf48) and
 * the generator's two words are written, nothing else. Both builds compile it: the product calls it at the original's points
 * (vehicle_audio.c, race_misc_game.c), the oracle's rng_boundary_probe.cpp runs it against the original emitters. */
#include "rx_game.h"
#include "engine_startline.h"
#include "build_registry.h"
#include "../src/rx_x87.h"
#include <string.h>

#define CFG_SECTION 0x0095cb14u   /* "Audio\\EngineSoundEmitter" */
#define MAX_REVS     0x00c01f7cu  /* StartLineMaxRevs (int, -1 = no limit) */
#define MAX_SEQ_REVS 0x00c0a080u  /* StartLineMaxSeqRevs (int) */
#define MIN_TIME     0x009caa74u  /* StartLineMinTime: the ConVar block 0x009caa50 + 0x24 */
#define MAX_TIME     0x009caaa0u  /* StartLineMaxTime: block 0x009caa7c */
#define MIN_PERC     0x009caa1cu  /* StartLineRPMMinPerc: block 0x009ca9f8 */
#define MAX_PERC     0x009caa48u  /* StartLineRPMMaxPerc: block 0x009caa24 */
#define RAMP_TIME    0x009ca998u  /* StartLineRPMRampTime */
#define HOLD_TIME    0x009ca9c4u  /* StartLineRPMHoldTime */
#define DOWN_TIME    0x009ca9f0u  /* StartLineRPMDownTime */

static struct { rx_sl_emitter e[RX_SL_SLOTS]; uint32_t ramp_set[RX_SL_SLOTS]; int tunables; } S;

void rx_engine_start_line_reset(void) {
    memset(&S, 0, sizeof S);
    for (int k = 0; k < RX_SL_SLOTS; k++) { S.e[k].size = 10; S.e[k].state = RX_SL_OTHER; }   /* 006848a0: +0xc = 10 */
}
rx_sl_emitter* rx_engine_start_line_slot(int k) { return k >= 0 && k < RX_SL_SLOTS ? &S.e[k] : NULL; }

/* 006848a0's table 0x00a7b1dc (the first state of an engine size): 1 Drive for the cars (6 .. 9), else 3 Idle */
static int car(const rx_sl_emitter* e) { return e->size >= 6 && e->size <= 9; }

/* the game's LCG, the inlined form of every caller: (seed >> 16) & 0x7fff of the new seed */
static int32_t sl_draw(void) {
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t s = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, s);
    return (int32_t)((s >> 16) & 0x7fff);
}

/* 00686370 (e, play 1): the state's event played (the host's); the wrapper's state 6 */
static void play(rx_sl_emitter* e) { e->serial++; e->finished = 0; }

static void enter_start_line(rx_sl_emitter* e, const rx_sl_input* in);
static void enter_rev(rx_sl_emitter* e);
/* 00686250 (e, state), the start line's view: no start-line state has an exit function; the enters of 2 / 4 */
static void set_state(rx_sl_emitter* e, int32_t state, const rx_sl_input* in) {
    const int32_t s = (state == RX_SL_REV || state == RX_SL_START) ? state : RX_SL_OTHER;
    if (s == e->state && s != RX_SL_OTHER) return;
    e->state = s;
    if (s == RX_SL_START) enter_start_line(e, in);
    else if (s == RX_SL_REV) enter_rev(e);
}
static void to_initial(rx_sl_emitter* e, const rx_sl_input* in) { set_state(e, RX_SL_OTHER, in); }   /* (a7b1dc)[+0xc]: 1 / 3 */

/* 00686e20 */
static void enter_start_line(rx_sl_emitter* e, const rx_sl_input* in) {
    if (car(e)) {
        /* 00686310 (the rpm smoother: the host's); comiss 0, +0x2dc; jb: kept while it is positive (or unordered) */
        if (!(0.0f < e->rev_rpm) && e->rev_rpm == e->rev_rpm) e->rev_rpm = in ? in->rpm : 0.0f;
    }
    const float r = (float)((double)(float)sl_draw() * 3.0517578125e-05 /* 0x9776a0 */);
    const float lo = F32(0, MIN_TIME);
    e->timer = r * (F32(0, MAX_TIME) - lo) + lo;
    play(e);
}

/* the ValueSmoother_M on the host (0067f1c0 / 0067f210, world_crowd_audio.c's ports on guest memory: the same operations) */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }
static float qf(float f) { return rx_bf(x87_copy(rx_fb(f))); }
static void ramp_start(rx_sl_ramp* s) {
    if (s->up == 0.0f) { s->value = 1.0f; s->prev = 1.0f; s->phase = 2; s->time = 0.0f; return; }
    s->value = 0.0f; s->prev = 0.0f; s->time = s->up * 0.0f; s->phase = 1;
}
static void ramp_step(rx_sl_ramp* s, float dt) {
    switch (s->phase) {
    case 0: s->prev = 0.0f; return;
    case 1: {
        const float t = s->time + dt;
        s->prev = qf(s->value);
        const float up = s->up, one = 1.0f;
        float v = t / up;
        if (!(one > v)) v = one;
        s->time = t; s->value = v;
        if (!(t < up) || t != t || up != up) {
            if (t < up || t != t || up != up) return;
            s->phase = 2; s->time = 0.0f; s->value = one;
        }
        return;
    }
    case 2: {
        const float t = s->time + dt;
        s->prev = qf(s->value);
        const float hold = s->hold;
        s->time = t;
        if (t < hold || t != t || hold != hold) return;
        s->phase = 3; s->time = 0.0f;
        return;
    }
    case 3: {
        const float t = s->time + dt;
        s->prev = qf(s->value);
        const float down = s->down;
        float v = 1.0f - t / down;
        if (!(v > 0.0f)) v = 0.0f;
        s->time = t; s->value = v;
        if (t < down || t != t || down != down) return;
        s->phase = 0; s->time = 0.0f; s->value = 0.0f;
        return;
    }
    default: return;
    }
}

/* 00686870 */
static void enter_rev(rx_sl_emitter* e) {
    e->visits++;
    const int32_t maxseq = I32(0, MAX_SEQ_REVS);
    if (maxseq == 1) e->seq = 1;
    else {
        const float r = (float)((double)(float)sl_draw() * -3.0517578125e-05 /* 0x977708 */);
        e->seq = 1 - rx_cvtt(r * (float)maxseq);
    }
    if (car(e)) {
        /* fild r; fmul [0x9776a0]; fstp m32; fld max; fld min; fsub st(1), st; fxch; fmul m32; faddp; fstp m32: 53-bit, each exact
         * but the sum (one rounding to the double, then to the float); then vf48 * that (fmul m32, fstp m32) */
        const float t = (float)((double)sl_draw() * 3.0517578125e-05);
        const double lo = (double)F32(0, MIN_PERC);
        const float perc = (float)(((double)F32(0, MAX_PERC) - lo) * (double)t + lo);
        float max_rpm = 0.0f;
        if (e->vehicle) {   /* vf48 leaves the limit in ST0. The ports' C types differ: the bike's 00466a10 returns a float, the four-wheel
                             * classes' (ATV 00486640, buggy 004ac640, truck 00473570, UTV 004c6fa0) an rx_f80 of the same float word (a
                             * struct return: calling them as float wrote through a stale x8 and faulted every car's start line) */
            const rx_addr target = U32(U32(e->vehicle, 0), 0xc0);
            if (target == 0x00466a10u) {
                typedef float (*vf48_f32_t)(rx_addr);
                vf48_f32_t f = RX_FN(target, vf48_f32_t);
                if (f) max_rpm = f(e->vehicle);
            } else {
                typedef rx_f80 (*vf48_f80_t)(rx_addr);
                vf48_f80_t f = RX_FN(target, vf48_f80_t);
                if (f) max_rpm = rx_f80_to_f32(f(e->vehicle), 0);   /* exact: the value was loaded from a float */
            }
        }
        e->target = (float)((double)max_rpm * (double)perc);
        /* 00672c30 (EAX = the ramp; xmm0 up, hold, down) */
        e->ramp.up = F32(0, RAMP_TIME); e->ramp.hold = qf(F32(0, HOLD_TIME)); e->ramp.down = qf(F32(0, DOWN_TIME));
    }
    play(e);
}

/* 00686220 */
static void rev_done(rx_sl_emitter* e, const rx_sl_input* in) {
    if (!e->start_line) { to_initial(e, in); return; }
    set_state(e, RX_SL_START, in);
}

void rx_engine_start_line_bind(int k, rx_addr vehicle, int32_t size) {
    rx_sl_emitter* e = rx_engine_start_line_slot(k);
    if (!e) return;
    e->vehicle = vehicle;
    if (!vehicle) return;                                   /* 00685a40 with no vehicle: the slot is free */
    e->size = size;                                         /* +0xc = V + 0x170 */
    e->start_line = 0;                                      /* +0x2b8 */
    if (!S.ramp_set[k]) {                                   /* 006855c0 (once per emitter: +4) */
        S.ramp_set[k] = 1;
        e->ramp.up = F32(0, RAMP_TIME); e->ramp.hold = F32(0, HOLD_TIME); e->ramp.down = F32(0, DOWN_TIME);
    }
    to_initial(e, NULL);                                    /* 00686250(e, 0), then (a7b1dc)[size] */
}
void rx_engine_start_line_unbind(int k) { rx_sl_emitter* e = rx_engine_start_line_slot(k); if (e) e->vehicle = 0; }

void rx_engine_start_line_enable(void) {
    for (int k = 0; k < RX_SL_SLOTS; k++) {
        rx_sl_emitter* e = &S.e[k];
        if (!e->vehicle) continue;
        e->start_line = 1; e->visits = 0; e->seq = 0; e->timer = 0.0f; e->rev_rpm = 0.0f;
        set_state(e, RX_SL_START, NULL);
    }
}
void rx_engine_start_line_disable(void) {
    for (int k = 0; k < RX_SL_SLOTS; k++) if (S.e[k].vehicle) S.e[k].start_line = 0;
}
void rx_engine_start_line_pre_exited(uint32_t to_first) {
    for (int k = 0; k < RX_SL_SLOTS; k++) {
        rx_sl_emitter* e = &S.e[k];
        if (!e->vehicle) continue;
        e->start_line = 0;
        if (to_first) to_initial(e, NULL);                  /* +0x338 = 0, (a7b1dc)[+0xc] */
    }
}
void rx_engine_start_line_finished(int k) { rx_sl_emitter* e = rx_engine_start_line_slot(k); if (e) e->finished = 1; }

void rx_engine_start_line_update(int k, const rx_sl_input* in, float dt) {
    rx_sl_emitter* e = rx_engine_start_line_slot(k);
    if (!e || !e->vehicle) return;                          /* 00685610: +0x10 == 0 */
    /* 00685916: under water in a state other than Off: drowned -> Choke (00685b10: 8, or a car without that event Drive), else wrecked
     * -> Wreck (00685b50) -- both leave the start line (Off -> the first state, 00685af0, does not reach 2 / 4) */
    if (e->state != RX_SL_OTHER && in->underwater) {
        if (in->drowned) to_initial(e, in);
        else if (in->wrecked) to_initial(e, in);
    }
    if (e->state == RX_SL_START) {                          /* 00686ec0 */
        if (!e->start_line) { to_initial(e, in); return; }
        const int32_t maxrevs = I32(0, MAX_REVS);
        if (maxrevs < 0 || e->visits < maxrevs) {
            const float t = e->timer - dt;
            e->timer = t;
            if (!(0.0f < t) && t == t) set_state(e, RX_SL_REV, in);   /* comiss 0, t; jb */
        }
        return;
    }
    if (e->state == RX_SL_REV) {                            /* 006869a0 */
        if (!e->start_line) { to_initial(e, in); return; }
        if (car(e)) {
            if (e->ramp.phase != 0) {
                ramp_step(&e->ramp, qf(dt));
                if (e->ramp.phase == 0) {
                    if (--e->seq <= 0) rev_done(e, in);
                    else ramp_start(&e->ramp);
                }
            } else ramp_start(&e->ramp);                    /* the inlined 0067f1c0 (ucomiss / lahf / jp) */
            const float rpm = in->rpm;
            e->rev_rpm = (e->target - rpm) * e->ramp.value + rpm;
            return;
        }
        if (e->finished) {                                  /* the wrapper +0x474 == 7 */
            if (--e->seq <= 0) { rev_done(e, in); return; }
            play(e);                                        /* AudioEvent_Play(0, 0, 0) */
        }
    }
}

/* ---- 00684c10: the start-line keys of <Audio><EngineSoundEmitter> ------------------------------------------------------------------- */
typedef rx_addr (*cfg_find_t)(rx_addr, rx_addr, rx_addr, uint32_t);
typedef uint32_t (*cfg_read_t)(rx_addr, rx_addr, rx_addr, uint32_t);
static rx_addr cfg_find(rx_addr cfg, rx_addr key) {
    const rx_addr fn = U32(U32(cfg, 0), 4);
    cfg_find_t f = RX_FN(fn, cfg_find_t);
    if (f) return f(cfg, CFG_SECTION, key, 0);
    const uint32_t a[3] = {CFG_SECTION, key, 0};
    return rx_call_original(fn, RX_THISCALL, cfg, 3, a);
}
static uint32_t cfg_read(rx_addr cfg, uint32_t slot, rx_addr entry, uint32_t init) {   /* vf17 floats / vf19 ints (entry, dst, 1) */
    const rx_addr fn = U32(U32(cfg, 0), slot);
    RX_FRAME_ENTER(F, 4);
    SU32(F, 0, init);
    cfg_read_t f = RX_FN(fn, cfg_read_t);
    if (f) f(cfg, entry, F, 1);
    else { const uint32_t a[3] = {entry, F, 1}; rx_call_original(fn, RX_THISCALL, cfg, 3, a); }
    const uint32_t v = U32(F, 0);
    RX_FRAME_LEAVE(F, 4);
    return v;
}
/* 00685590 (EDI = dst ConVar, arg src): the name (0x20 bytes at +4) and the two values (+0x24, +0x28, x87 copies) */
static void convar_copy(rx_addr dst, rx_addr src) {
    for (uint32_t i = 0; i < 0x20; i++) SU8(dst, 4 + i, U8(src, 4 + i));
    SU32(dst, 0x24, x87_copy(U32(src, 0x24)));
    SU32(dst, 0x28, x87_copy(U32(src, 0x28)));
}
void rx_engine_start_line_tunables(void) {
    if (S.tunables) return;
    const rx_addr ctx = U32(0, 0x00a95844);
    const rx_addr cfg = ctx ? U32(ctx, 0xc) : 0;
    if (!cfg) return;
    S.tunables = 1;
    static const struct { rx_addr key, dst; int kind; } k_float[] = {   /* kind 0: > 0; 1: 0 <= v <= 1 */
        {0x0095cc8c, RAMP_TIME, 0}, {0x0095cca4, DOWN_TIME, 0}, {0x0095ccbc, HOLD_TIME, 0}, {0x0095ccd4, MIN_PERC, 1}, {0x0095cce8, MAX_PERC, 1},
    };
    for (unsigned i = 0; i < sizeof k_float / sizeof *k_float; i++) {
        const rx_addr e = cfg_find(cfg, k_float[i].key);
        if (!e) continue;
        const float v = rx_bf(cfg_read(cfg, 0x44, e, 0x00800000u /* [0x97765c] */));
        if (k_float[i].kind == 0 ? v > 0.0f : (v >= 0.0f && 1.0f >= v)) SF32(0, k_float[i].dst, v);
    }
    if (F32(0, MIN_PERC) > F32(0, MAX_PERC)) convar_copy(0x009caa24, 0x009ca9f8);
    rx_addr e = cfg_find(cfg, 0x0095ccfc);                 /* StartLineMaxRevs */
    if (e) SU32(0, MAX_REVS, cfg_read(cfg, 0x4c, e, 0xffffffffu));
    e = cfg_find(cfg, 0x0095cd10);                          /* StartLineMaxSeqRevs */
    if (e) { const int32_t v = (int32_t)cfg_read(cfg, 0x4c, e, 0x80000000u); SU32(0, MAX_SEQ_REVS, (uint32_t)(v < 1 ? 1 : v)); }
    e = cfg_find(cfg, 0x0095cd24);                          /* StartLineMinTime */
    if (e) { const float v = rx_bf(cfg_read(cfg, 0x44, e, 0x00800000u)); if (v > 0.0f) SF32(0, MIN_TIME, v); }
    e = cfg_find(cfg, 0x0095cd38);                          /* StartLineMaxTime */
    if (e) { const float v = rx_bf(cfg_read(cfg, 0x44, e, 0x00800000u)); if (v > 0.0f) SF32(0, MAX_TIME, v); }
    if (F32(0, MIN_TIME) > F32(0, MAX_TIME)) convar_copy(0x009caa7c, 0x009caa50);
}
