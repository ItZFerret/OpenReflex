/* fx_vehicle.c -- the vehicle particle effects (dust / roost / spray per wheel): the effect sets at 0x00c11e90 (stride 0x17c30,
 * to 0x00d2f0d0) updated by 00691bd0 -> 00697a00 (wheel surface materials, then the job 006979a0 on the engine job pool) ->
 * 00696d80 per effect (gate 00697460, the emitter matrix from the vehicle's orientation quaternion and the wheel / body
 * position, the rate scale 00693560 into the particle type, the emitter's vf5 / vf3). notes/fx.md.
 *
 * Set (0x17c30): +0 vehicle, +0x10 the vehicle block V the vehicle fills (vf51 0xcc; offsets below are V's: four wheel
 * records of 0x70 from +0x30 (+0 position, +0x10 acceleration, +0x20 velocity, +0x30 / +0x34 slip values, +0x38 / +0x3c ids),
 * +0x1c0 wheel surface ids, +0x1d0 position, +0x1e0 orientation quaternion (x y z w), +0x1f0 velocity, +0x200 wheel count),
 * +0x220 the particle entity, +0x224 effect count, +0x230 the effects (0x3f0 each). Effect F: +0x10 matrix (4x4), +0x50 velocity, +0x60 emitter,
 * +0x64 wheel index, +0x68 body effect (no wheel), +0x6c, +0x70..+0x9c the gate's thresholds (value, flag "below" +4, enabled
 * +5), +0xa0 / +0xa4 surface id match, +0xa8 the rate scale record (00693560), +0xd0 offset, +0xe0 surface id. */
#include "fx.h"
#include "rider_cap.h"
#include "scene_fx_entities.h"
#include "fx_util.h"
#include "phys_terrain.h"
#include "world_veh.h"
#include "loop_engine.h"
#include "loop_entity.h"

/* a >= b (comiss; jae) as 1.0f / 0.0f, or 1.0f when the test is disabled */
static inline float gate(uint8_t enabled, uint8_t below, float v, float limit) {
    if (!enabled) return 1.0f;
    return (below ? limit >= v : v >= limit) ? 1.0f : 0.0f;
}

/* ---- 00697460 (ECX = effect F, EAX = set V): 1 when the effect may emit: a body effect (+0x68) while +0x6c is 0; a wheel
 * effect (records the wheel's surface id into the effect class table 0x9c4740 [wheel] +0x28, +0x24 = 1) whose surface matches
 * (+0xe0, or +0xa4 with the wheel's +0x6c = +0xa0) and whose gates all pass: wheel +0x60 vs +0x88, +0x64 vs +0x90, |wheel
 * velocity|^2 vs +0x70, |wheel acceleration|^2 vs +0x78, |set velocity|^2 vs +0x80 (product truncated to an int), and the
 * wheel +0x68 = +0x98 when +0x9c. AL (EAX's upper bytes are the set's). Frame: push ecx; push esi (push edi around the id). */
uint32_t rx_fx_vehicle_effect_gate(rx_addr F, rx_addr V) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, F);
    const uint32_t hi = V & 0xffffff00u;
    if (U8(F, 0x68) != 0) { SU32(sp0 - 8, 0, fx_reg(FX_ESI)); return hi | (U32(F, 0x6c) == 0 ? 1u : 0u); }
    SU32(sp0 - 8, 0, fx_reg(FX_ESI));                          /* push esi */
    SU32(sp0 - 0xc, 0, fx_reg(FX_EDI));                        /* push edi (around the id) */
    const uint32_t w = U32(F, 0x64);
    const uint32_t id = U32(V, 0x1c0 + 4 * w);
    const rx_addr cls = U32(0x009c4740 + 4 * w, 0);
    SU32(cls, 0x28, id);
    SU8(cls, 0x24, 1);
    if (id != U32(F, 0xe0)) {
        if (U8(F, 0xa4) == 0) return hi;
        if (U32(V + w * 0x70u, 0x6c) != U32(F, 0xa0)) return hi;
    }
    const rx_addr B = V + w * 0x70u;
    const float z2 = rx_muls(F32(V, 0x1f8), F32(V, 0x1f8));
    SF32(sp0 - 4, 0, z2);
    const float sp2 = rx_adds(rx_adds(rx_muls(F32(B, 0x50), F32(B, 0x50)), rx_muls(F32(B, 0x54), F32(B, 0x54))), rx_muls(F32(B, 0x58), F32(B, 0x58)));
    const float ac2 = rx_adds(rx_adds(rx_muls(F32(B, 0x40), F32(B, 0x40)), rx_muls(F32(B, 0x44), F32(B, 0x44))), rx_muls(F32(B, 0x48), F32(B, 0x48)));
    const float v2 = rx_adds(rx_adds(rx_muls(F32(V, 0x1f4), F32(V, 0x1f4)), rx_muls(F32(V, 0x1f0), F32(V, 0x1f0))), F32(sp0 - 4, 0));
    const float g60 = gate(U8(F, 0x8d), U8(F, 0x8c), F32(B, 0x60), F32(F, 0x88));
    const float g64 = gate(U8(F, 0x95), U8(F, 0x94), F32(B, 0x64), F32(F, 0x90));
    float p = rx_muls(g64, g60);
    p = rx_muls(p, gate(U8(F, 0x75), U8(F, 0x74), sp2, F32(F, 0x70)));
    p = rx_muls(p, gate(U8(F, 0x7d), U8(F, 0x7c), ac2, F32(F, 0x78)));
    p = rx_muls(gate(U8(F, 0x85), U8(F, 0x84), v2, F32(F, 0x80)), p);
    const int32_t n = rx_cvtt(p);
    uint32_t al = 1;
    if (U8(F, 0x9c) != 0 && U32(B, 0x68) != U32(F, 0x98)) al = 0;
    if (n == 0) al = 0;
    return hi | al;
}
RX_FUNCTION(0x00697460, rx_fx_vehicle_effect_gate)

/* a rate factor of 00693560: v * k, then + 1 (additive) or 1 / max(v k, 1) */
static inline float rate_factor(uint8_t enabled, uint8_t inverse, float k, float v) {
    if (!enabled) return 1.0f;
    const float x = rx_muls(k, v);
    if (!inverse) return rx_adds(x, 1.0f);
    return x > 1.0f ? rx_divs(1.0f, x) : rx_divs(1.0f, 1.0f);
}
static float sqrt_pushed(float x, rx_addr ret) {               /* push ecx; movss [esp], x; call SqrtF; add esp, 4 */
    const rx_addr sp = rx_sp(); SF32(sp, -4, x); SU32(sp, -8, ret);
    return phys_sqrtf(x);
}

/* ---- 00693560 (EAX = wheel, ESI = the scale record R; stdcall set V, body flag, base, range, out*, out2*; RET 0x18): the
 * effect's rate scale P = (wheel effects) |wheel velocity| (R +0 / +4 / +5), |wheel acceleration| (R +8 / +0xc / +0xd), wheel
 * +0x60 (R +0x18 / +0x1c / +0x1d), wheel +0x64 (R +0x20 / +0x24 / +0x25) factors, times the set speed factor (R +0x10 / +0x14 /
 * +0x15); *out = P * base, *out2 = P * range - *out. The product is kept in the first argument's slot (as the original). */
void rx_fx_vehicle_effect_rate(rx_addr V, uint32_t body, float base, float range, rx_addr out, rx_addr out2, uint32_t wheel, rx_addr R) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_EBX));                          /* push ebx */
    SF32(sp0, 4, 1.0f);
    rx_set_sp(sp0 - 4);
    if ((uint8_t)body == 0) {
        const rx_addr B = wheel * 0x70u + V;
        SU32(sp0 - 8, 0, fx_reg(FX_EDI));                      /* push edi */
        rx_set_sp(sp0 - 8);
        float f1 = 1.0f;
        if (U8(R, 5) != 0) {
            const float l2 = rx_adds(rx_adds(rx_muls(F32(B, 0x50), F32(B, 0x50)), rx_muls(F32(B, 0x54), F32(B, 0x54))), rx_muls(F32(B, 0x58), F32(B, 0x58)));
            const float s = sqrt_pushed(l2, 0x006935c2);
            const float x = rx_muls(F32(R, 0), s);
            f1 = U8(R, 4) == 0 ? rx_adds(x, 1.0f) : (x > 1.0f ? rx_divs(1.0f, x) : rx_divs(1.0f, 1.0f));
        }
        SF32(sp0, 4, f1);
        float f2 = 1.0f;
        if (U8(R, 0xd) != 0) {
            const float l2 = rx_adds(rx_adds(rx_muls(F32(B, 0x40), F32(B, 0x40)), rx_muls(F32(B, 0x44), F32(B, 0x44))), rx_muls(F32(B, 0x48), F32(B, 0x48)));
            const float s = sqrt_pushed(l2, 0x00693644);
            const float x = rx_muls(F32(R, 8), s);
            f2 = U8(R, 0xc) == 0 ? rx_adds(x, 1.0f) : (x > 1.0f ? rx_divs(1.0f, x) : rx_divs(1.0f, 1.0f));
        }
        const float p2 = rx_muls(f2, F32(sp0, 4));
        const float p3 = rx_muls(rate_factor(U8(R, 0x1d), U8(R, 0x1c), F32(R, 0x18), F32(B, 0x60)), p2);
        rx_set_sp(sp0 - 4);                                   /* pop edi */
        const float p4 = rx_muls(rate_factor(U8(R, 0x25), U8(R, 0x24), F32(R, 0x20), F32(B, 0x64)), p3);
        SF32(sp0, 4, p4);
    }
    float c = 1.0f;
    if (U8(R, 0x15) != 0) {
        const float l2 = rx_adds(rx_adds(rx_muls(F32(V, 0x1f0), F32(V, 0x1f0)), rx_muls(F32(V, 0x1f4), F32(V, 0x1f4))), rx_muls(F32(V, 0x1f8), F32(V, 0x1f8)));
        c = rx_muls(sqrt_pushed(l2, 0x00693762), F32(R, 0x10));
        if (U8(R, 0x14) == 0) c = rx_adds(c, 1.0f);
        else { if (!(c > 1.0f)) c = 1.0f; c = rx_divs(1.0f, c); }
    }
    const float P = rx_muls(c, F32(sp0, 4));
    const float a = rx_muls(P, base);
    SF32(out, 0, a);
    SF32(out2, 0, rx_subs(rx_muls(P, range), a));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00693560, rx_fx_vehicle_effect_rate)

/* a thiscall virtual of the effect's emitter (+0x60) with one 32-bit argument (or none: n = 0) */
static uint32_t emitter_call(rx_addr em, uint32_t off, int n, uint32_t a0, rx_addr ret) {
    const rx_addr fn = U32(U32(em, 0), off);
    const uint32_t a[1] = {a0};
    if (n == 0) {
        uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
        if (!f) return rx_call_original(fn, RX_THISCALL, em, 0, 0);
        const rx_addr s = fx_push(0, 0, ret); const uint32_t r = f(em); fx_pop(s); return r;
    }
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    if (!f) return rx_call_original(fn, RX_THISCALL, em, 1, a);
    const rx_addr s = fx_push(1, a, ret); f(em, a0); fx_pop(s); return 0;
}

/* ---- 00696d80 (EAX = set V, EDI = effect F; stdcall dt; RET 4): when the gate passes: F's matrix from V's quaternion
 * (+0x1e0, the classic 1 - 2(yy + zz) form), its translation = the offset (+0xd0) through the matrix + the wheel position
 * (+0x30 of the wheel record; body effects: V +0x1d0), F +0x50 = V's velocity (x87 copies); the rate scale (00693560 on the
 * emitter's particle type +0x104 / +0x108, with +0x168 / +0x16c) under the asset's busy flag; then the emitter's vf5 (on /
 * off) and vf3 (dt). Frame: push ebp; and esp, -16; sub esp, 0x18; ebx / esi: E = A - 0x20: +0xc..+0x1c the products. */
void rx_fx_vehicle_effect_update(float dt, rx_addr V, rx_addr F) {
    fx_frame f; fx_frame_enter(&f, 0x18, 16, FX_EBX, FX_ESI);
    const rx_addr E = f.E;
    fx_set_reg(FX_ESI, V);
    uint32_t on = 0;
    uint32_t pass;
    {
        const rx_addr s = fx_push(0, 0, 0x00696d94);
        pass = rx_fx_vehicle_effect_gate(F, V) & 0xff;
        fx_pop(s);
    }
    if (pass) {
        const float two = 2.0f /* 0x92e330 */;
        const float x = F32(V, 0x1e0), y = F32(V, 0x1e4), z = F32(V, 0x1e8);
        const float x2 = rx_muls(x, two), xx = rx_muls(x, x2);
        const float y2 = rx_muls(y, two), z2 = rx_muls(z, two);
        SF32(E, 0x14, xx);
        const float zz = rx_muls(z, z2), yy = rx_muls(y, y2), xy = rx_muls(y, x2);
        const int body = U8(F, 0x68) != 0;
        const float w = F32(V, 0x1ec);
        const float wx = rx_muls(w, x2), wz = rx_muls(w, z2), wy = rx_muls(w, y2);
        const float yz = rx_muls(z, y2), xz = rx_muls(z, x2);
        if (body) { SF32(E, 0xc, z2); SF32(E, 0x18, wx); SF32(E, 0x10, wy); SF32(E, 0xc, wz); }
        else { SF32(E, 0x18, z2); SF32(E, 0x1c, wx); SF32(E, 0xc, wy); SF32(E, 0x10, wz); }
        SF32(F, 0x10, rx_subs(1.0f, rx_adds(zz, yy)));
        SF32(F, 0x20, rx_subs(xy, wz));
        SF32(F, 0x14, rx_adds(xy, wz));
        SF32(F, 0x4c, 1.0f);
        SF32(F, 0x24, rx_subs(1.0f, rx_adds(zz, xx)));
        SF32(F, 0x34, rx_subs(yz, wx));
        SF32(F, 0x18, rx_subs(xz, wy));
        SF32(F, 0x1c, 0.0f); SF32(F, 0x2c, 0.0f); SF32(F, 0x3c, 0.0f); SF32(F, 0x40, 0.0f); SF32(F, 0x44, 0.0f); SF32(F, 0x48, 0.0f);
        SF32(F, 0x30, rx_adds(xz, wy));
        SF32(F, 0x38, rx_subs(1.0f, rx_adds(yy, xx)));
        const float m21 = rx_adds(yz, wx);
        SF32(F, 0x28, m21);
        const float d0 = F32(F, 0xd0), d1 = F32(F, 0xd4), d2 = F32(F, 0xd8);
        const float px = rx_adds(rx_adds(rx_adds(rx_muls(F32(F, 0x20), d1), rx_muls(F32(F, 0x30), d2)), rx_muls(d0, F32(F, 0x10))), F32(F, 0x40));
        const float py = rx_adds(rx_adds(rx_adds(rx_muls(F32(F, 0x14), d0), rx_muls(F32(F, 0x24), d1)), rx_muls(F32(F, 0x34), d2)), F32(F, 0x44));
        const float pz = rx_adds(rx_adds(rx_adds(rx_muls(F32(F, 0x18), d0), rx_muls(m21, d1)), rx_muls(F32(F, 0x38), d2)), F32(F, 0x48));
        rx_addr base = V + 0x1d0;
        if (!body) base = U32(F, 0x64) * 0x70u + V + 0x30;
        const float tx = rx_adds(F32(base, 0), px), ty = rx_adds(F32(base, 4), py);
        SF32(F, 0x44, ty);
        SF32(F, 0x40, tx);
        SF32(F, 0x48, rx_adds(F32(base, 8), pz));
        fx_x87_copy(F + 0x50, V + 0x1f0); fx_x87_copy(F + 0x54, V + 0x1f4); fx_x87_copy(F + 0x58, V + 0x1f8);
        const rx_addr S = emitter_call(U32(F, 0x60), 0x10, 0, 0, 0x0069716f);
        const rx_addr holder = U32(S, 0x14);
        fx_set_reg(FX_EBX, holder);
        const rx_addr T = U32(S, 0x18) * 0x1f0u + U32(U32(holder, 0x44), 0x10);
        SU32(holder, 0x4c, 1);
        {
            const uint32_t a[6] = {V, U8(F, 0x68), fx_x87_bits(U32(T, 0x168)), fx_x87_bits(U32(T, 0x16c)), T + 0x104, T + 0x108};
            fx_set_reg(FX_ESI, F + 0xa8);
            const rx_addr s = fx_push(6, a, 0x006971c0);
            rx_fx_vehicle_effect_rate(V, a[1], rx_bf(a[2]), rx_bf(a[3]), T + 0x104, T + 0x108, U32(F, 0x64), F + 0xa8);
            fx_pop(s);
        }
        SU32(holder, 0x4c, 0);
        on = 1;
    }
    emitter_call(U32(F, 0x60), 0x14, 1, on, 0x006971d7);
    {
        const rx_addr em = U32(F, 0x60), fn = U32(U32(em, 0), 0xc);
        const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
        void (*g)(rx_addr, float) = RX_FN(fn, void (*)(rx_addr, float));
        const uint32_t a[1] = {rx_fb(d)};
        if (g) { const rx_addr s = fx_push(1, a, 0x006971e8); g(em, d); fx_pop(s); }
        else rx_call_original(fn, RX_THISCALL, em, 1, a);
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00696d80, rx_fx_vehicle_effect_update)

/* ---- 006979a0 (job {+0 dt, +4 set + 0x10 ... , +8 set}; cdecl): 00696d80 for each of the set's effects (+0x224 of them at
 * +0x230, 0x3f0 apart) with EAX = the record's +4 (the set's vehicle block). Frame: push ebp; and esp, -8; sub esp, 0xc; ebx /
 * esi / edi: E = A - 0x18: +0x10 dt, +0x14 the block. */
void rx_fx_vehicle_effects_job(rx_addr job) {
    fx_frame f; fx_frame_enter(&f, 0xc, 8, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    const uint32_t blk = U32(job, 4);
    const float dt = F32(job, 0);
    const rx_addr set = U32(job, 8);
    fx_set_reg(FX_EBX, set);
    SF32(E, 0x10, dt);
    SU32(E, 0x14, blk);
    rx_addr fx = set + 0x230;
    for (int32_t i = 0; i < I32(set, 0x224); i++, fx += 0x3f0) {
        fx_set_reg(FX_ESI, (uint32_t)i); fx_set_reg(FX_EDI, fx);
        const float d = rx_bf(fx_x87_bits(U32(E, 0x10)));
        const uint32_t a[1] = {rx_fb(d)};
        const rx_addr s = fx_push(1, a, 0x006979e4);
        rx_fx_vehicle_effect_update(d, U32(E, 0x14), fx);
        fx_pop(s);
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006979a0, rx_fx_vehicle_effects_job)

/* ---- 00697a00 (ESI = set; stdcall dt; RET 4): with a vehicle whose (+0x1328) +0x14e flag is set: the vehicle fills the wheel
 * block (vf51 +0xcc, 4 wheels), the surface material id (TerrainMaterialAt layer 2) under each wheel into +0x1d0; when the set's
 * quaternion is a number, the job 006979a0 {dt, set + 0x10, set} goes to the engine job pool. Frame: sub esp, 0x44; ebx / ebp /
 * edi: E = sp0 - 0x50: +0xc the wheel position cursor, +0x10 the job record. */
void rx_fx_vehicle_effects_update(rx_addr set, float dt) {
    fx_frame f; fx_frame_flat(&f, 0x44, FX_EBX, FX_EBP, FX_EDI);
    const rx_addr E = f.E;
    const rx_addr veh = U32(set, 0);
    if (veh != 0 && U8(U32(veh, 0x1328), 0x14e) != 0) {
        SU32(set, 0x210, 4);
        {
            const rx_addr fn = U32(U32(veh, 0), 0xcc);
            void (*g)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
            const uint32_t a[1] = {set + 0x10};
            if (g) { const rx_addr s = fx_push(1, a, 0x00697a3b); g(veh, set + 0x10); fx_pop(s); }
            else rx_call_original(fn, RX_THISCALL, veh, 1, a);
        }
        fx_set_reg(FX_EBP, 0);
        for (int32_t i = 0; i < I32(set, 0x210); i++) {
            if (i == 0) { SU32(E, 0xc, set + 0x40); fx_set_reg(FX_EBX, set + 0x1d0); }
            if ((U8(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
            rx_addr terrain;
            { const rx_addr s = fx_push(0, 0, 0x00697a76); terrain = rx_world_find_by_controller(0x009e30d4); fx_pop(s);
              FX_LEAF_RESIDUE(0x00697a76, FX_EBX, FX_ESI, FX_EDI); }
            fx_set_reg(FX_EDI, terrain);
            rx_regs caller = fx_regs();
            uint32_t m;
            { const rx_addr s = fx_push(0, 0, 0x00697a86); m = phys_terrain_material_at(terrain, U32(E, 0xc), 2, &caller, 0); fx_pop(s); }
            SU32(E, 0xc, U32(E, 0xc) + 0x70);
            SU32(set, 0x1d0 + 4u * (uint32_t)i, m);
            fx_set_reg(FX_EBP, (uint32_t)i + 1); fx_set_reg(FX_EBX, set + 0x1d0 + 4u * ((uint32_t)i + 1));
        }
        const float q = F32(set, 0x1e0);
        if (q == q) {
            const rx_addr rec = E + 0x10;
            SF32(rec, 0, dt); SU32(rec, 4, set + 0x10); SU32(rec, 8, set);
            const rx_addr pool = rx_jobpool_instance();           /* inlined in the original (the same static init) */
            fx_jobpool_submit(pool, rec, 0x006979a0, 0x00697afc);
        }
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00697a00, rx_fx_vehicle_effects_update)

/* ---- 00691bd0 (cdecl dt): with the active scene's particle entity (0xa94264 [scene +0x2cc4]): every vehicle effect set
 * (0x00c11e90.., 0x17c30 apart): rebinds it (00697870) when the entity changed, then 00697a00 (dt through fld / fstp).
 * Frame: push esi; push edi. */
void rx_fx_vehicle_effects_update_all(float dt) {
    fx_frame f; fx_frame_flat(&f, 0, FX_ESI, FX_EDI);
    const rx_addr ent = U32(0x00a94264 + 4u * U32(fx_active_scene(), 0x2cc4), 0);
    fx_set_reg(FX_EDI, ent);
    if (ent != 0) {
        for (uint32_t k = 0; k < rx_rc_tab_n(0x00c11e90u); k++) {   /* (w-rc-core) + the side sets taken (rider_cap.h) */
            const rx_addr set = rx_rc_tab(0x00c11e90u, 0x17c30u, k);
            fx_set_reg(FX_ESI, set);
            const uint32_t was = U32(set, 0x220);
            SU32(set, 0x220, ent);
            if (was != ent) rx_vehicle_effects_rebind(set);   /* 00697870 (fx_worldparticles.c) */
            const float d = rx_bf(fx_x87_bits(rx_fb(dt)));
            const uint32_t a[1] = {rx_fb(d)};
            const rx_addr s = fx_push(1, a, 0x00691c19);
            rx_fx_vehicle_effects_update(set, d);
            fx_pop(s);
        }
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00691bd0, rx_fx_vehicle_effects_update_all)
