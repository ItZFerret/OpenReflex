/* fx_worldparticles.c -- the world's vehicle particle effect sets: 00697870 (rebind a set to the world's ParticleEntity: reset its
 * 0x60 0x3f0-byte records' controllers, then copy and bind the matching ParticleAttribAsset records), 006977f0 (one record's three
 * controllers: disable and rate 0.1), 006976a0 (one record from an attrib record and its controller's bind) and 0069b090 (a
 * controller's release through its manager). notes/fx.md "The world particle effect sets".
 *
 * The effect set S (0xc11e90 + k * 0x17c30, 12 sets): +0 the vehicle, +0x220 the world's ParticleEntity, +0x224 the bound record
 * count, +0x230 + i * 0x3f0 the records (0x60). Record R: +0 bound, +0x60 the active controller (+0xf0 / +0x1f0 / +0x2f0 by the
 * descriptor's type), +0x64.. the descriptor copy. Attrib asset data (+0x44 of the partattr handle): +0x10 count, +0x14 records
 * (0xa0 bytes each: +0 name, +4 type, +0x20 vehicle kind, +0x24 the vehicle's second word, +0x28.. the fields). */
#include "scene_fx_entities.h"
#include "rider_cap.h"
#include "fx.h"
#include "fx_util.h"

/* a thiscall virtual with one 32-bit stack argument (the original's callee does RET 4) */
static void vcall1(rx_addr obj, uint32_t off, uint32_t a0, rx_addr ret) {
    const rx_addr fn = U32(U32(obj, 0), off);
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    if (!f) { rx_call_original(fn, RX_THISCALL, obj, 1, &a0); return; }
    (void)ret;
    f(obj, a0);
}
/* a thiscall virtual with one float stack argument (the original pushes it through fld / fstp: a signalling NaN is quieted) */
static void vcallf(rx_addr obj, uint32_t off, float a0) {
    const rx_addr fn = U32(U32(obj, 0), off);
    const float v = rx_bf(fx_x87_bits(rx_fb(a0)));
    void (*f)(rx_addr, float) = RX_FN(fn, void (*)(rx_addr, float));
    if (f) f(obj, v);
    else { const uint32_t a[1] = {rx_fb(v)}; rx_call_original(fn, RX_THISCALL, obj, 1, a); }
}

/* 006977f0 (EDI = the 0x3f0 record): the three controllers at +0xf0 / +0x1f0 / +0x2f0: vf5 (0), vf3 (0.1f) */
void rx_vehicle_effect_reset(rx_addr rec) {
    for (uint32_t off = 0xf0; off <= 0x2f0; off += 0x100) {
        const rx_addr ctl = rec + off;
        vcall1(ctl, 0x14, 0, 0);
        vcallf(ctl, 0xc, 0.1f /* 0x931d94 */);
    }
}
RX_FUNCTION(0x006977f0, rx_vehicle_effect_reset);

/* 006976a0 (stack record, set, descriptor; RET 0xc): copy the descriptor into the record, pick its controller by descriptor type
 * (+4), bind (controller vf1 (set, name)), set the four range flags through the controller's vf4, square three scales */
void rx_vehicle_effect_bind_record(rx_addr rec, rx_addr set, rx_addr d) {
    SU32(rec, 0x64, U32(d, 0x28));
    SU8(rec, 0x68, U8(d, 0x2c));
    SU32(rec, 0x6c, U32(d, 0x30));
    for (uint32_t k = 0; k < 14; k++) SU32(rec, 0x70 + 4 * k, U32(d, 0x34 + 4 * k));
    for (uint32_t k = 0; k < 5; k++) { SU32(rec, 0xa8 + 8 * k, U32(d, 0x6c + 8 * k)); SU32(rec, 0xac + 8 * k, U32(d, 0x70 + 8 * k)); }
    SU32(rec, 0xd0, U32(d, 0x10)); SU32(rec, 0xd4, U32(d, 0x14)); SU32(rec, 0xd8, U32(d, 0x18));
    SU32(rec, 0xe0, U32(d, 8));
    const uint32_t type = U32(d, 4);
    rx_addr ctl;
    if (type == 1) ctl = rec + 0x1f0;
    else if (type == 2) ctl = rec + 0x2f0;
    else ctl = rec + 0xf0;
    SU32(rec, 0x60, ctl);
    {   /* the controller's bind vf1 (set, name) */
        const rx_addr fn = U32(U32(ctl, 0), 4);
        void (*bind)(rx_addr, rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr, rx_addr));
        if (bind) bind(ctl, set, U32(d, 0));
        else { const uint32_t a[2] = {set, U32(d, 0)}; rx_call_original(fn, RX_THISCALL, ctl, 2, a); }
    }
    {   /* vf4: the emitter state; the four range flags from the descriptor's bytes +0xc..+0xf */
        const rx_addr fn = U32(U32(ctl, 0), 0x10);
        rx_addr (*st)(rx_addr) = RX_FN(fn, rx_addr (*)(rx_addr));
        const rx_addr s = st ? st(ctl) : (rx_addr)rx_call_original(fn, RX_THISCALL, ctl, 0, 0);
        for (uint32_t k = 0; k < 4; k++) SU8(s, 0xb4 + k, U8(d, 0xc + k));
    }
    for (uint32_t off = 0x70; off <= 0x80; off += 8) SF32(rec, off, rx_muls(F32(rec, off), F32(rec, off)));
}
RX_FUNCTION(0x006976a0, rx_vehicle_effect_bind_record);

/* 0069ad60 YRingParticleEmitter::vf0 (thiscall S; stack dt, manager, matrix, interpolate, atlas index; RET 0x14): the same
 * emission gate as 00695ab0 but the Y-ring builder 0069a170 (physics splash particles; called only when the emitter is
 * active). notes/fx.md. */
static inline uint32_t lcg(uint32_t s) { return s * 0x41c64e6du + 0x3039u; }
void rx_yring_emitter_vf0(rx_addr S, float dt, rx_addr mgr, rx_addr mat, uint32_t interp, uint32_t index) {
    fx_frame f; fx_frame_enter(&f, 0x74, 128, FX_EBX, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    float d = 0.001f;                                            /* comiss order: NaN takes 0.0333 */
    if (!(dt < rx_bf(0x3d086595u) /* 0x977bb4: 0.0333 */)) d = rx_bf(0x3d086595u);
    else if (!(rx_bf(0x3a83126fu) /* 0x97743c: 0.001 */ >= dt)) d = dt;
    fx_set_reg(FX_EBX, S);
    if (U32(S, 0x20) != 0) {
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(S + 0x30, k, U32(mat, k));
        SU32(S, 0x20, 0);
    }
    if (mgr != 0 && U32(S, 0x14) != 0 && U32(U32(fx_ctx(), 0x10), 0xc) == 0) {
        const rx_addr lw = fx_level_world(U32(0, FX_G_GAME), 0x0069ade1, fx_reg(FX_ESI));
        if (U32(lw, 0x20014) == 0 && U32(S, 0x1c) != 0) {
            const rx_addr holder = U32(S, 0x14);
            SU32(holder, 0x4c, 1);
            const rx_addr T = U32(S, 0x18) * 0x1f0u + U32(U32(holder, 0x44), 0x10);
            const uint32_t s = lcg(U32(0, 0x009cb860));
            SU32(0, 0x009cb860, s);
            SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
            const float r = (float)((double)(float)((s >> 16) & 0x7fffu) * 3.0517578125e-05);
            float x = rx_adds(rx_muls(r, F32(T, 0x108)), F32(T, 0x104));
            x = rx_adds(rx_muls(x, d), F32(S, 0x98));
            const int32_t n = rx_cvtt(x);
            SF32(S, 0x98, rx_subs(x, (float)n));
            SU32(E, 0x38, rx_fb((float)n)); SU32(E, 0x3c, (uint32_t)n);
            if (interp != 0) {
                for (int32_t j = 0; j < n; j++) {
                    const float t = rx_divs(rx_adds((float)j, 1.0f), (float)n);
                    SU32(E, 0x34, (uint32_t)j);
                    rx_fx_matrix_lerp(S + 0x30, t, E + 0x40, mat);
                    if (U32(S, 0x14) != 0) {                 /* the original pushes index / matrix / manager / S (ret 0069aed3) */
                        const uint32_t a[4] = {S, mgr, E + 0x40, index};
                        fx_set_reg(FX_ESI, E + 0x40); fx_set_reg(FX_EDI, mat);
                        const rx_addr sp = fx_push(4, a, 0x0069aed3u);
                        rx_yring_spawn(S, mgr, E + 0x40, index);
                        fx_pop(sp);
                    }
                }
            } else {
                for (int32_t j = n; j > 0; j--) {
                    if (U32(S, 0x14) != 0) {                 /* ESI = index, EDI = the count, ret 0069af02 */
                        const uint32_t a[4] = {S, mgr, mat, index};
                        fx_set_reg(FX_ESI, index); fx_set_reg(FX_EDI, (uint32_t)j);
                        const rx_addr sp = fx_push(4, a, 0x0069af02u);
                        rx_yring_spawn(S, mgr, mat, index);
                        fx_pop(sp);
                    }
                }
            }
            for (uint32_t k = 0; k < 0x40; k += 4) SU32(S + 0x30, k, U32(mat, k));
            SU32(holder, 0x4c, 0);
        }
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x0069ad60, rx_yring_emitter_vf0);

/* 0069b090 (ECX = a particle controller): with a manager (+4): the manager's vf5 (+0x80) */
void rx_particle_controller_release(rx_addr self) {
    const rx_addr mgr = U32(self, 4);
    if (!mgr) return;
    const rx_addr fn = U32(U32(mgr, 0), 0x14);
    uint32_t (*f)(rx_addr, rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr, rx_addr));
    if (f) f(mgr, self + 0x80);
    else { const uint32_t a[1] = {self + 0x80}; rx_call_original(fn, RX_THISCALL, mgr, 1, a); }
}
RX_FUNCTION(0x0069b090, rx_particle_controller_release);

/* PORT ADDITION (w-rc-core): an effect set of the side storage (sets 12..31, rider_cap.h) constructed as the static initializer
 * 0090a170 constructs the original's 12 (build_globals_gen.c: +0 and +0x224 cleared, the 96 records of 0x3f0 from +0x230 by 00696b40:
 * the three controllers 0069af30 / 0069af90 / 0069aff0 on the ParticleEmitter base 00694110), on first use. Never at <= 12 riders. */
static void fx_side_emitter(rx_addr e) {   /* 00694110 */
    SU32(e, 0, 0x0095df44);
    SU32(e, 0x84, 0); SU32(e, 0x74, 0); SU32(e, 0x78, 0); SU32(e, 0x7c, 0); SU32(e, 0x70, 0); SU32(e, 0x80, 0); SU32(e, 0x88, 0);
    SU32(e, 0x10, 0); SU32(e, 0x98, 0);
    SU32(e, 0x20, 1); SU32(e, 0x1c, 0); SU32(e, 0x90, 0); SU32(e, 0x94, 0);
    for (uint32_t i = 0; i < 4; i++) { SU32(e, 0xa4 + 4 * i, U32(0, 0x009776f8)); SU8(e, 0xb4 + i, 1); }
    SU32(e, 0x9c, 0); SU32(e, 0xa0, 0);
}
static void fx_side_controller(rx_addr c, uint32_t kind, rx_addr a0, rx_addr a1) {   /* 0069af30 / 0069af90 / 0069aff0 */
    static const uint32_t vt0[3] = {0x009588ec, 0x0095e0c0, 0x0095e0f8}, vt1[3] = {0x0095e088, 0x0095e0a4, 0x0095e0dc}, sub[3] = {0, 0x0095e074, 0x0095e080};
    SU32(c, 4, 0);
    SU32(c, 0, vt0[kind]);
    fx_side_emitter(c + 0x10);
    if (sub[kind]) SU32(c, 0x10, sub[kind]);
    if (kind == 2) SU32(c, 0xac, 1);
    SU32(c, 0xd0, 0x0095df34);
    SU32(c, 0xe4, U32(0, 0x00931f24));
    SU32(c, 0xf0, a0);
    SU32(c, 0xc, 0);
    SU32(c, 0, vt1[kind]);
    SU32(c, 0xa0, a1);
}
static void fx_side_set_construct(rx_addr set) {
    SU32(set, 0, 0);
    for (uint32_t k = 0; k < 96; k++) {   /* 00696b40 */
        const rx_addr r = set + 0x230 + 0x3f0 * k;
        for (uint32_t o = 0x70; o <= 0xc8; o += 8) { SU32(r, o, 0); SU8(r, o + 4, 0); if (o != 0x98 && o != 0xa0) SU8(r, o + 5, 0); }
        fx_side_controller(r + 0xf0, 0, r + 0x10, r + 0x50);
        fx_side_controller(r + 0x1f0, 1, r + 0x10, r + 0x50);
        fx_side_controller(r + 0x2f0, 2, r + 0x10, r + 0x50);
        const uint32_t one = U32(0, 0x00931f24);
        SU8(r, 0, 0);
        for (uint32_t o = 0x10; o < 0x50; o += 4) SU32(r, o, (o - 0x10) % 0x14 == 0 ? one : 0);
    }
    SU32(set, 0x224, 0);
}
static uint32_t g_fx_side_built;   /* host: the side sets constructed (bit k - 12) */

/* 00691cc0 (EDI = the vehicle; the player's fx slot): the first effect set without a vehicle (0xc11e90, 12 sets of 0x17c30) gets the
 * active scene's ParticleEntity (+0x220, rebound when it changed) and the vehicle (+0); with a zero vehicle the entity is cleared;
 * rebound again after the vehicle is stored (00697870 needs both to copy the vehicle's attrib records). Frame: push esi around the
 * second rebind (no locals). */
void rx_vehicle_effects_slot_bind(rx_addr veh) {
    const rx_addr ent = U32(0x00a94264 + 4u * U32(fx_active_scene(), 0x2cc4), 0);
    for (uint32_t k = 0; k < (rx_rc_more() ? RX_RC_MAX : 12u); k++) {   /* (w-rc-core) the 12 sets, then (13+ players) sets 12..31 (rider_cap.h) once those are taken */
        const rx_addr set = rx_rc_tab(0x00c11e90u, 0x17c30u, k);
        if (k >= RX_RC_ORIG && !(g_fx_side_built & (1u << (k - RX_RC_ORIG)))) { fx_side_set_construct(set); g_fx_side_built |= 1u << (k - RX_RC_ORIG); }
        if (U32(set, 0) != 0) continue;
        rx_rc_tab_mark(0x00c11e90u, k);
        const uint32_t was = U32(set, 0x220);
        SU32(set, 0x220, ent);
        if (was != ent) rx_vehicle_effects_rebind(set);
        SU32(set, 0, veh);
        if (veh == 0) SU32(set, 0x220, 0);
        rx_vehicle_effects_rebind(set);
        return;
    }
}
RX_FUNCTION(0x00691cc0, rx_vehicle_effects_slot_bind);

/* 00697870 (ESI = the effect set): reset every record; when the set has the scene's ParticleEntity, copy the attrib records that
 * match the vehicle (kind at +0x20 / +0x24 against the vehicle's +0x194 / +0x198) and bind them */
void rx_vehicle_effects_rebind(rx_addr set) {
    for (uint32_t i = 0; i < 0x60; i++) {
        const rx_addr rec = set + 0x230 + 0x3f0 * i;
        SU8(rec, 0, 0);
        rx_vehicle_effect_reset(rec);
    }
    const rx_addr ent = U32(set, 0x220);
    const rx_addr veh = U32(set, 0);
    if (!ent || !veh) return;
    const uint32_t v0 = U32(veh, 0x194), v1 = U32(veh, 0x198);
    SU32(set, 0x224, 0);
    const rx_addr handle = U32(ent, 0xb4);
    if (!handle) return;
    const rx_addr data = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    if (data && U32(data, 0x10) != 0) {
        for (uint32_t i = 0; i < U32(data, 0x10); i++) {
            const rx_addr d = U32(data, 0x14) + 0xa0u * i;
            const uint32_t k0 = U32(d, 0x20), k1 = U32(d, 0x24);
            const int match = v0 == 0 ? (k1 == v1) : 1;
            if (k0 == v0 && match && I32(set, 0x224) < 0x60) {
                const uint32_t n = U32(set, 0x224);
                const rx_addr rec = set + 0x230 + 0x3f0u * n;
                rx_vehicle_effect_bind_record(rec, ent, d);
                SU8(rec, 0, 1);
                rx_vehicle_effect_reset(rec);
                SU32(set, 0x224, n + 1);
            }
        }
    }
    SU32(handle, 0x4c, 0);
}
RX_FUNCTION(0x00697870, rx_vehicle_effects_rebind);
