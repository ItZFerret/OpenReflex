/* world_crowd_audio.c -- the crowd unit's momentum ("Crowd_Momentum", the parameter of every crowd emitter's CROWD_AMB event): the
 * block at 0x00c023a8 .. 0x00c02400 (set up by 0067f3c0 / 00674d90, world_cam_ramp.c), its per-frame state machine 0067f450 with the
 * wander smoother 0067f210 / 0067f1c0 on +0x3c (0x00c023e4) and the wander draw 0067f6d0 (the game's LCG 0x9cb860 / count 0x9cb864);
 * and the crowd's frame update 00674e90 / 00674f20 in the product (w-sfx, 2026-10-27).
 *
 * Block: +0x00 min 0, +0x04 max 10, +0x08 base (AmbientExcitementLevel), +0x0c W = |AmbientExcitementLevelWander|, +0x10 the momentum,
 * +0x14 the previous value, +0x18 the target, +0x1c the start value, +0x20 the wander offset, +0x24 the timer, +0x28 / +0x2c / +0x30
 * attack / sustain / decay, +0x3c the wander smoother (value, prev, up, hold, down, time, phase), +0x58 the state (0 off, 1 attack,
 * 2 sustain, 3 decay, 4 ambient, 5 / 6 wander up / down).
 * The excitement sources are ported elsewhere (00674440 rx_world_crowd_excite / 00674540 rx_world_crowd_event, race_activity_audio.c,
 * fx_stunt.c); the emitters (CrowdLineSoundEmitterEntity, their vf19 and the FMOD events) are the host's (the audio boundary).
 * Exact SSE single precision (the arithmetic is movss / addss / mulss / divss / comiss; the x87 fld / fstp pairs only copy floats:
 * a signalling NaN comes out quiet). */
#include "rx_game.h"
#include "world_crowd_audio.h"
#include "world_cam_ramp.h"
#include "sound_state.h"

#define B(o) (0x00c023a8u + (o))
#define WANDER 0x00c023e4u
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }
static float f32(rx_addr a) { return F32(0, a); }
static void sf32(rx_addr a, float v) { SF32(0, a, v); }

/* 0067f1c0 (ECX = smoother): start: with no ramp-up time (+8 == 0) value = prev = 1, phase 2 (hold), time 0; else (also NaN) value =
 * prev = 0, time = up * 0, phase 1 (ucomiss / lahf / test ah, 0x44 / jp: only an ordered equal takes the first branch). */
void rx_crowd_smoother_start(rx_addr s) {
    const float up = F32(s, 8);
    if (up == 0.0f) {
        SF32(s, 0, 1.0f /* 0x931f24 */); SF32(s, 4, 1.0f); SU32(s, 0x18, 2); SF32(s, 0x14, 0.0f);
        return;
    }
    SF32(s, 0, 0.0f); SF32(s, 4, 0.0f); SF32(s, 0x14, up * 0.0f); SU32(s, 0x18, 1);
}
RX_FUNCTION(0x0067f1c0, rx_crowd_smoother_start)

/* 0067f210 (EAX = smoother, float dt; RET 4): ValueSmoother_M step: phase 1 ramps the value up over +8, 2 holds +0xc, 3 ramps down
 * over +0x10, then 0; +4 = the value before the step (an x87 copy). Phases above 3 do nothing. */
void rx_crowd_smoother_step(rx_addr s, float dt) {
    switch (U32(s, 0x18)) {
    case 0:
        SF32(s, 4, 0.0f);
        return;
    case 1: {
        const float t = F32(s, 0x14) + dt;
        SU32(s, 4, x87_copy(U32(s, 0)));
        const float up = F32(s, 8), one = 1.0f /* 0x931f24 */;
        float v = t / up;
        if (!(one > v)) v = one;                          /* comiss 1, v; ja keeps v */
        SF32(s, 0x14, t);
        SF32(s, 0, v);
        if (!(t < up) || t != t || up != up) {            /* comiss t, up; jb skips (an unordered compare jumps too) */
            if (t < up || t != t || up != up) return;
            SU32(s, 0x18, 2); SF32(s, 0x14, 0.0f); SF32(s, 0, one);
        }
        return;
    }
    case 2: {
        const float t = F32(s, 0x14) + dt;
        SU32(s, 4, x87_copy(U32(s, 0)));
        const float hold = F32(s, 0xc);
        SF32(s, 0x14, t);
        if (t < hold || t != t || hold != hold) return;
        SU32(s, 0x18, 3); SF32(s, 0x14, 0.0f);
        return;
    }
    case 3: {
        const float t = F32(s, 0x14) + dt;
        SU32(s, 4, x87_copy(U32(s, 0)));
        const float down = F32(s, 0x10);
        float v = 1.0f /* 0x931f24 */ - t / down;
        if (!(v > 0.0f)) v = 0.0f;                        /* comiss v, 0; ja keeps v */
        SF32(s, 0x14, t);
        SF32(s, 0, v);
        if (t < down || t != t || down != down) return;
        SU32(s, 0x18, 0); SF32(s, 0x14, 0.0f); SF32(s, 0, 0.0f);
        return;
    }
    default: return;
    }
}
RX_FUNCTION(0x0067f210, rx_crowd_smoother_step)

/* 0067f6d0: a new wander offset: one draw of the game's LCG, r = (float)((double)(float)((seed >> 16) & 0x7fff) * 2^-15), offset =
 * W * (r * W); in state 5 the negated offset ([0x9774a8] - offset) and state 6, else the offset and state 5; the smoother started. */
void rx_crowd_wander(void) {
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    const float r = (float)((double)(float)(int32_t)((seed >> 16) & 0x7fff) * 3.0517578125e-05 /* 0x9776a0 */);
    const float w = f32(B(0xc));
    const float off = w * (r * w);
    if (I32(0, B(0x58)) == 5) {
        sf32(B(0x20), F32(0, 0x009774a8) - off);
        rx_crowd_smoother_start(WANDER);
        SU32(0, B(0x58), 6);
        return;
    }
    sf32(B(0x20), off);
    rx_crowd_smoother_start(WANDER);
    SU32(0, B(0x58), 5);
}
RX_FUNCTION(0x0067f6d0, rx_crowd_wander)

/* 0067f450 (float dt; RET 4): the momentum's state machine (see the file comment); the wander smoother steps first. */
void rx_crowd_momentum_tick(float dt) {
    const float d = rx_bf(x87_copy(rx_fb(dt)));
    rx_crowd_smoother_step(WANDER, d);
    switch (U32(0, B(0x58))) {
    case 0:
        sf32(B(0x24), 0.0f);
        return;
    case 1: {
        const float t = f32(B(0x24)) + d;
        sf32(B(0x14), f32(B(0x10)));
        const float attack = f32(B(0x28));
        sf32(B(0x24), t);
        if (!(t < attack) && t == t && attack == attack) {
            sf32(B(0x24), 0.0f); SU32(0, B(0x58), 2); sf32(B(0x10), f32(B(0x18)));
            return;
        }
        float f = t / attack;
        const float one = 1.0f /* 0x931f24 */;
        if (!(one > f)) f = one;
        const float start = f32(B(0x1c));
        sf32(B(0x10), (f32(B(0x18)) - start) * f + start);
        return;
    }
    case 2: {
        const float t = f32(B(0x24)) + d;
        const float sustain = f32(B(0x2c));
        sf32(B(0x24), t);
        sf32(B(0x14), f32(B(0x10)));
        if (t < sustain || t != t || sustain != sustain) return;
        SU32(0, B(0x58), 3); sf32(B(0x24), 0.0f);
        return;
    }
    case 3: {
        const float t = f32(B(0x24)) + d;
        sf32(B(0x14), f32(B(0x10)));
        const float decay = f32(B(0x30));
        sf32(B(0x24), t);
        if (!(t < decay) && t == t && decay == decay) {
            sf32(B(0x24), 0.0f); sf32(B(0x10), f32(B(0x08))); SU32(0, B(0x58), 4);
            return;
        }
        float f = 1.0f /* 0x931f24 */ - t / decay;
        if (!(f > 0.0f)) f = 0.0f;
        const float base = f32(B(0x08));
        sf32(B(0x10), (f32(B(0x18)) - base) * f + base);
        return;
    }
    case 4:
        sf32(B(0x14), f32(B(0x10)));
        sf32(B(0x24), 0.0f);
        if (f32(B(0xc)) > 0.0f) rx_crowd_wander();
        return;
    case 5:
    case 6: {
        const int moving = U32(0, WANDER + 0x18) != 0;
        sf32(B(0x24), f32(B(0x24)) + d);
        sf32(B(0x14), f32(B(0x10)));
        if (moving) { sf32(B(0x10), f32(B(0x20)) * f32(WANDER) + f32(B(0x08))); return; }
        sf32(B(0x10), f32(B(0x08)));
        sf32(B(0x24), 0.0f);
        if (f32(B(0xc)) > 0.0f) { rx_crowd_wander(); return; }
        SU32(0, B(0x58), 4);
        return;
    }
    default: return;
    }
}
RX_FUNCTION(0x0067f450, rx_crowd_momentum_tick)

/* 00674680: the crowd type of the current world (g_MXGame + 0x5a54 when the engine's event world mode is active, + 0x20c8 for the
 * UI world): 0 stadium (world + 0x22144 == 1), 1 outdoor; -1 without a game / world. */
int32_t rx_crowd_type(void) {
    const rx_addr g = U32(0, 0x00a946b8);
    if (!g) return -1;
    const rx_addr ec = U32(0, 0x00a95844);
    rx_addr w;
    if (U32(U32(ec, 0x20), 0x34) == 1) w = U32(g, 0x5a54);
    else if (U32(U32(ec, 0x1c), 0x34) == 1) w = U32(g, 0x20c8);
    else return -1;
    if (!w) return -1;
    if (I32(w, 0x22144) != 0 && I32(w, 0x22144) == 1) return 0;
    return 1;
}
RX_FUNCTION(0x00674680, rx_crowd_type)

#ifndef RX_ORACLE
#include <stdlib.h>
#include <string.h>
#include "build_world.h"
#include "../src/fmt_db.h"
#include "../src/fmt_bxml.h"

/* ---- product: the crowd init 00674cd0 (the init thread, which the product does not run) ------------------------------------------
 * The select counters 0xa7f088 .. 0xa7f090 cleared; the CrowdAudioParameters document (CrowdSoundProjects / Crowd_Config, the loader
 * 006746d0 registered through 00803f30) into the crowd tunables: AmbientExcitementLevel [0x9c47b8], AmbientExcitementLevelWander
 * [0x9c47c4], EventExcitementAttack / Sustain / DecayTime [0x9c47c8 / cc / d0], AmbientWanderTransition / SustainTime [0x9c47d4 / d8],
 * ExcitementLevels [0x9c47dc] x 10, ReactionOneShotPercentages [0x9c4804] x 10 (the shipped .data values stay where a field is absent;
 * the loader's own acceptance rules were not traced: values are taken as written). Then [0x9c47c0] = -1 when a size is set, and the
 * block set up again (00674d90). The emitter list (0xc02470) stays empty: the emitters are the host's. Once per process. */
static float cfg_float(const char* s) { if (!s) return 0.0f; return strtof(s, NULL); }
static int cfg_value(const fmt_bxml* d, int node, float* out, int max) {
    fmt_bxml_value v;
    if (node < 0 || fmt_bxml_node_value(d, node, &v) != 0) return 0;
    if (v.kind == FMT_BX_FLOAT && v.count >= 1) { int n = 0; for (; n < max && n < (int)v.count; n++) memcpy(&out[n], v.data + 4 * n, 4); return n; }
    if (v.kind != FMT_BX_STRING || !v.str) return 0;
    int n = 0; const char* p = v.str;
    while (*p && n < max) {
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        char* e; const float f = strtof(p, &e);
        if (e == p) break;
        out[n++] = f; p = e;
        while (*p && *p != ',') p++;
    }
    (void)cfg_float;
    return n;
}
void rx_crowd_config_init(void) {
    static int done;
    if (done) return;
    struct fmt_install* in = rx_phys_props_install();
    if (!in) return;
    done = 1;
    SU32(0, 0x00a7f088, 0); SU32(0, 0x00a7f08c, 0); SU32(0, 0x00a7f090, 0);
    fmt_buf b = {0};
    if (fmt_install_read_named(in, "CrowdSoundProjects", "Crowd_Config", "CrowdAudioParameters", "bxml", &b) == 0) {
        fmt_bxml* d = fmt_bxml_parse(b.data, b.size);
        if (d) {
            static const struct { const char* tag; rx_addr at; int n; } k[] = {
                {"AmbientExcitementLevel", 0x009c47b8, 1}, {"AmbientExcitementLevelWander", 0x009c47c4, 1},
                {"EventExcitementAttackTime", 0x009c47c8, 1}, {"EventExcitementSustainTime", 0x009c47cc, 1}, {"EventExcitementDecayTime", 0x009c47d0, 1},
                {"AmbientWanderTransitionTime", 0x009c47d4, 1}, {"AmbientWanderSustainTime", 0x009c47d8, 1},
                {"ExcitementLevels", 0x009c47dc, 10}, {"ReactionOneShotPercentages", 0x009c4804, 10}};
            for (size_t i = 0; i < sizeof k / sizeof k[0]; i++) {
                float v[10];
                const int n = cfg_value(d, fmt_bxml_find_child(d, 0, k[i].tag), v, k[i].n);
                for (int j = 0; j < n; j++) SF32(0, k[i].at + 4u * (uint32_t)j, v[j]);
            }
            fmt_bxml_free(d);
        }
        fmt_buf_free(&b);
    }
    if (I32(0, 0x009c47bc) != -1) SU32(0, 0x009c47c0, 0xffffffffu);
    rx_cam_ramp_setup();                                  /* 00674d90 */
}

/* ---- product: the crowd's frame update 00674e90 (cdecl float dt) ----------------------------------------------------------------
 * 00674f20 adopts the requested crowd size ([0x9c47c0] -> [0x9c47bc]) and the world's crowd type ([0xa941b4], 00674680), and runs the
 * crowd sound package state machine of the (type, size) record (0xc01c38 + (type + 2 size) * 0x80, state + 0x34: load "Database\<sound
 * package>" and its sound / bank assets 1 .. 3, ready 4, the emitters' events named 6 (00673dc0), the request changed 8, unloaded 10 ->
 * 0 with the size released). The packages are sound assets the host loads (its crowd projects): the product treats a package as ready at
 * once and an unload as immediate, keeping the record's state in the game's word so the sequence of adoptions is the original's. Then
 * 00674e90: no type or size -> the block reset (00674d90) when it runs; a type and size with the block off -> the momentum at its base
 * and state 4; the state machine (0067f450, which draws the game's LCG on every wander cycle); the emitters' vf19 (none in the
 * product: the host's emitters). The crowd init (00674cd0, the init thread) is the product's crowd_init: its counters / list and the
 * CrowdAudioParameters values (006746d0 loader, crowd_config.c). */
static void crowd_packages(void) {
    int32_t size = I32(0, 0x009c47bc);
    if (size == -1) {
        if (I32(0, 0x009c47c0) == -1) return;
        size = I32(0, 0x009c47c0);
        SU32(0, 0x009c47bc, (uint32_t)size);
        SU32(0, 0x00a941b4, (uint32_t)rx_crowd_type());
    } else if (I32(0, 0x00a941b4) == -1) SU32(0, 0x00a941b4, (uint32_t)rx_crowd_type());
    const int32_t type = I32(0, 0x00a941b4);
    if (size == -1 || type == -1) return;
    const rx_addr rec = 0x00c01c38u + (uint32_t)(type + size * 2) * 0x80u;
    uint32_t st = U32(rec, 0x34);
    if (st < 6) st = 6;                                   /* loaded, ready, the emitters' events named: at once */
    if (st == 6 && I32(0, 0x009c47c0) != I32(0, 0x009c47bc)) st = 8;
    if (st == 8) st = 10;                                 /* unloaded at once */
    if (st == 10) { st = 0; SU32(0, 0x009c47bc, 0xffffffffu); }
    SU32(rec, 0x34, st);
}

void rx_crowd_frame(float dt) {
    rx_crowd_config_init();
    crowd_packages();
    if (I32(0, 0x00a941b4) == -1 || I32(0, 0x009c47bc) == -1) {
        if (U32(0, B(0x58)) != 0) rx_cam_ramp_setup();   /* 00674d90 */
    } else if (U32(0, B(0x58)) == 0) {
        SU32(0, B(0x10), U32(0, B(0x08)));
        SU32(0, B(0x58), 4);
    }
    rx_crowd_momentum_tick(dt);
    rx_sound_crowd_set_running(1);
}
#endif
