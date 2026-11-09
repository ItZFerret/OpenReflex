/* scene_fx_entities.c -- the FX / audio / status-board scene entity classes the scene factory (FUN_004046a0) creates:
 *  0x102a PlaceableParticleEffect   ctor 006964d0, vf5 00696670, vf7 00696810, vf9 00696870, id 00692110 (tick 00696940 = fx unit)
 *  0x102e FireworksParticleEffect  ctor 00692140 (tick 00692340 = fx unit)
 *  0x102f TriggeredPlaceableParticleEffect ctor 006923c0 (tick 00692520 = fx unit)
 *  0x1030 ParticleEntity (ParticleManagerRSData) ctor 00695c60, vf5 00695de0, vf7 00695da0, vf9 00696040, id 00691bb0
 *         (tick 00696100 / vf21 006960c0 = fx unit)
 *  0x105e / 0x1088 AudioVolumeMixEntity ctor 0067d380, vf5 0067d4d0, vf19 0067d7f0 -> 0067d810, vf2 0067d7d0
 *  0x107e StatusBoardEntity ctor 00413d80, vf5 00413ed0, vf19 00416530, vf9 00416430, id 00404690
 *
 * 0x102b ParticleSphere, 0x102c ParticleYRing and 0x102d BermParticle are absent from all 57 level scenes (script scan:
 * builds/fxconstruct/logs/level_fx_scan.txt); their factory entries stay empty as the original's do for a class outside the core.
 *
 * The particle asset itself (ParticleAssetIO::vf3 / ParticleAsset::Parse) is build_particle.c. The UI text billboard of a
 * StatusBoard and the FMOD objects of the audio entities are the established UI / audio boundaries: the oracle runs the original
 * (the StatusBoard port is product-only), the product keeps no UI / audio engine. notes/fx.md. */
#include "scene_fx_entities.h"
#include "build_scene_int.h"
#include "build_scene.h"
#include "build_asset.h"
#include "build_alloc.h"
#include "build_particle.h"
#include "build_activity_freeride_run.h"
#include "anim.h"
#include "fx.h"
#include "fx_bind.h"
#include "loop_platform.h"
#include "../src/fmt_bxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void rx_scene_fx_register(void);
uint32_t rx_placeable_effect_vf5(rx_addr e, uint32_t reader);
uint32_t rx_placeable_effect_vf7(rx_addr e);

/* ---------------------------------------------------------------------------------------------------------------------------
 * small helpers */

static rx_addr own_scene(const rx_addr e) { return U32(e, 0x10); }

/* the child node of a field with a `name` attribute (a nested RSData, e.g. VolumeMix1 -> GlobalVolume) */
static int named_child(const fmt_bxml* d, int parent, const char* name) {
    const int n = fmt_bxml_child_count(d, parent);
    for (int k = 0; k < n; k++) {
        const int c = fmt_bxml_child(d, parent, k);
        const char* nm = fmt_bxml_attr_str(d, c, "name");
        if (nm && strcmp(nm, name) == 0) return c;
    }
    return -1;
}
/* the Float of a nested RSData field: VolumeMixN/<sub>/Float (what the original reads after moving its reader cursor there) */
static int nested_float(const char* field, const char* sub, float* out) {
    const fmt_entity* rs = rx_scene_rs();
    const int f = rs ? fmt_entity_field(rs, field) : -1;
    if (f < 0) return 0;
    const int c = named_child(rs->doc, f, sub);
    if (c < 0) return 0;
    const int v = fmt_bxml_find_child(rs->doc, c, "Float");
    fmt_bxml_value val;
    if (v >= 0 && fmt_bxml_node_value(rs->doc, v, &val) == 0 && val.count >= 1) { *out = fmt_bx_f32(&val, 0, 0); return 1; }
    return 0;
}
/* the name of an Asset field's value (FUN_0080ec00), NULL when absent */
static const char* rs_asset_name(const char* field) {
    const fmt_entity* rs = rx_scene_rs();
    const int c = rs ? fmt_entity_field_tagged(rs, field, "Asset") : -1;
    return c >= 0 ? fmt_bxml_attr_str(rs->doc, c, "name") : NULL;
}

/* a Database handle by a game-memory name (vf27 0084ac70 through the frame so the name is game memory) */
static rx_addr db_handle(rx_addr db, const char* name, uint32_t lo, uint32_t hi) {
    if (!db || !name || !*name) return 0;
    RX_FRAME_ENTER(N, 0x110);
    uint32_t i = 0;
    for (; name[i] && i < 0x103; i++) SU8(N, i, (uint8_t)name[i]);
    SU8(N, i, 0);
    const rx_addr h = rx_database_vf_handle(db, N, lo, hi, 1);
    RX_FRAME_LEAVE(N, 0x110);
    return h;
}
static rx_addr database(void) { const rx_addr c = U32(0, 0x00a95844); return c ? U32(c, 8) : 0; }

/* a UI / audio call: the oracle runs the original, the product has neither (documented boundary) */
static void ui_thunk(rx_addr fn, int conv, rx_addr ecx, int n, const uint32_t* args) {
#ifdef RX_ORACLE
    rx_call_original(fn, conv, ecx, n, args);
#else
    (void)fn; (void)conv; (void)ecx; (void)n; (void)args;
#endif
}
/* the world matrix of an entity (vf14, slot 0x38) */
static rx_addr entity_world_matrix(rx_addr e) {
    const rx_addr fn = U32(U32(e, 0), 0x38);
    rx_addr (*f)(rx_addr) = RX_FN(fn, rx_addr (*)(rx_addr));
    if (f) return f(e);
    return (rx_addr)rx_call_original(fn, RX_THISCALL, e, 0, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * PlaceableParticleEffect (006964d0), with the emitter member 00694110 */

static void emitter_construct(rx_addr em) {
    SU32(em, 0, 0x0095df44);                                     /* ParticleEmitter::vftable */
    SU32(em, 0x84, 0); SU32(em, 0x74, 0); SU32(em, 0x78, 0); SU32(em, 0x7c, 0); SU32(em, 0x70, 0); SU32(em, 0x80, 0); SU32(em, 0x88, 0);
    SF32(em, 0x10, 0.0f); SF32(em, 0x98, 0.0f);
    SU32(em, 0x20, 1);
    SU32(em, 0x1c, 0); SU32(em, 0x90, 0); SU32(em, 0x94, 0);
    for (int i = 0; i < 4; i++) { SU32(em, 0xa4 + 4 * i, 0x4a742400u /* 4e6, 0x9776f8 */); SU8(em, 0xb4 + i, 1); }
    SU32(em, 0x9c, 0); SU32(em, 0xa0, 0);
}

rx_addr rx_particle_emitter_construct(rx_addr em) { emitter_construct(em); return em; }
RX_FUNCTION(0x00694110, rx_particle_emitter_construct);

static rx_addr placeable_construct(rx_addr e) {
    SU32(e, 0, 0x0095a2e4); SU32(e, 0x18, 0); SU32(e, 0x10, 0);
    SF32(e, 0x24, 0.0f); SF32(e, 0x20, 0.0f); SU32(e, 0x70, 0);
    scene_set_matrix_identity(e + 0x30);
    SU32(e, 0x1c, 0); SU32(e, 0x7c, 0); SF32(e, 0x78, 0.0f); SU32(e, 0x74, 0); SU32(e, 0x28, 0); SU32(e, 0x2c, 0);
    SU32(e, 0x94, 0);
    SU32(e, 0x90, 0x009588ec);
    emitter_construct(e + 0xa0);
    SU32(e, 0x160, 0x0095df34);
    SF32(e, 0x174, 1.0f /* 0x931f24 */);
    SU32(e, 0x9c, 0);
    SU32(e, 0x294, 0);
    SU32(e, 0, 0x0095e32c);                                      /* PlaceableParticleEffect::vftable */
    SU32(e, 0x90, 0x0095e3a0);
    SU32(e, 0x70, 1);
    return e;
}
/* 006964d0 (ECX = the effect; plain ret) -> the effect: the PlaceableParticleEffect constructor (MXActivityTag's +0xff0c0 is built by the
 * vector constructor with it, race_tag.c) */
rx_addr rx_placeable_effect_construct(rx_addr e) { return placeable_construct(e); }
RX_FUNCTION(0x006964d0, rx_placeable_effect_construct)
static rx_addr placeable_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x2a0);
    return e ? placeable_construct(e) : 0;
}

/* 00696670 PlaceableParticleEffect::vf5 */
uint32_t rx_placeable_effect_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    const char* pa = rs_str("ParticleAsset");
    { uint32_t k = 0; for (; pa[k] && k < 0x103; k++) SU8(e, 0x190 + k, (uint8_t)pa[k]); SU8(e, 0x190 + k, 0); }
    SU8(e, 0x293, 0);
    rx_entity_vf5(e);
    for (int i = 0; i < 4; i++) {
        char field[32];
        snprintf(field, sizeof field, "DistanceZone%d", i + 1);
        uint32_t v = 1;
        if (rs_has(field, RS_BOOL)) v = rs_bool(field) == 1;
        SU8(e, 0x154 + (uint32_t)i, (uint8_t)v);
    }
    /* FUN_00696760: an AudioEmitter whose SoundAsset is not Editor_Default makes a 0x1e0-byte sound emitter at +0x294.  The
     * audio engine is a product boundary (no FMOD). */
    if (rs_has("AudioEmitter", 0)) {
        const char* snd = rs_asset_name("SoundAsset");
        if (snd && strcasecmp(snd, "Editor_Default") != 0) {
#ifdef RX_ORACLE
            rx_regs in, o; memset(&in, 0, sizeof in); in.r[3] = e;               /* EDI = the effect */
            rx_call_original_ex(0x00696760, &in, &o, 0, 0);
#else
            SU32(e, 0x294, 0);
#endif
        }
    }
    return 0;
}
#ifndef RX_ORACLE
RX_FUNCTION(0x00696670, rx_placeable_effect_vf5);   /* reads the scene RSData through the core fmt reader: product only (the oracle runs the original) */
#endif

/* a thiscall virtual with one stack argument (RET 4) */
static void vcall1(rx_addr obj, uint32_t off, uint32_t a0) {
    const rx_addr fn = U32(U32(obj, 0), off);
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    if (f) f(obj, a0);
    else { const uint32_t a[1] = {a0}; rx_call_original(fn, RX_THISCALL, obj, 1, a); }
}
/* the effect's controller (+0x90) vf5: enable(0) */
static void effect_disable(rx_addr e) { vcall1(e + 0x90, 0x14, 0); }
static rx_addr particle_entity_of(rx_addr e) { const rx_addr sc = own_scene(e); return sc ? U32(0x00a94264 + 4u * U32(sc, 0x2cc4), 0) : 0; }

/* 006921c0 FireworksParticleEffect::vf5: the base loader, then EmitterType (absent: 1; "Box": 0; "Sphere": 1) into the emitter's
 * builder selector (+0x13c) */
uint32_t rx_fireworks_effect_vf5(rx_addr e, uint32_t reader) {
    rx_placeable_effect_vf5(e, reader);
    if (!rs_has("EmitterType", RS_ENUM)) { SU32(e, 0x13c, 1); return 0; }
    const char* t = rs_enum_text("EmitterType");
    if (t && strcmp(t, "Box") == 0) SU32(e, 0x13c, 0);
    else if (t && strcmp(t, "Sphere") == 0) SU32(e, 0x13c, 1);
    return 0;
}
#ifndef RX_ORACLE
RX_FUNCTION(0x006921c0, rx_fireworks_effect_vf5);   /* reads the scene RSData through the core fmt reader: product only (the oracle runs the original) */
#endif
/* 00692440: the Triggered class' vf5 is the base's */
uint32_t rx_triggered_effect_vf5(rx_addr e, uint32_t reader) { return rx_placeable_effect_vf5(e, reader); }
#ifndef RX_ORACLE
RX_FUNCTION(0x00692440, rx_triggered_effect_vf5);   /* reads the scene RSData through the core fmt reader: product only (the oracle runs the original) */
#endif

/* 00692240 FireworksParticleEffect::vf7: bind, disable the controller, clear the timers, set the four distance bounds to 0x4cbebc20 */
uint32_t rx_fireworks_effect_vf7(rx_addr e) {
    rx_fx_effect_bind(e + 0x90, particle_entity_of(e), e + 0x190);
    effect_disable(e);
    SU32(e, 0x180, 0); SU32(e, 0x184, 0); SU32(e, 0x188, 0);
    SU32(e, 0x144, 0x4cbebc20u); SU32(e, 0x148, 0x4cbebc20u); SU32(e, 0x14c, 0x4cbebc20u); SU32(e, 0x150, 0x4cbebc20u);
    return 0;
}
RX_FUNCTION(0x00692240, rx_fireworks_effect_vf7);
/* 00692450 TriggeredPlaceableParticleEffect::vf7: the base vf7, then the same disable / bounds */
uint32_t rx_triggered_effect_vf7(rx_addr e) {
    rx_placeable_effect_vf7(e);
    effect_disable(e);
    SU32(e, 0x144, 0x4cbebc20u); SU32(e, 0x148, 0x4cbebc20u); SU32(e, 0x14c, 0x4cbebc20u); SU32(e, 0x150, 0x4cbebc20u);
    return 0;
}
RX_FUNCTION(0x00692450, rx_triggered_effect_vf7);

/* 00696810 PlaceableParticleEffect::vf7: bind (+0x90) to the scene's particle entity's emitter asset, then enable */
uint32_t rx_placeable_effect_vf7(rx_addr e) {
    const rx_addr sc = own_scene(e);
    const rx_addr ent = sc ? U32(0x00a94264 + 4u * U32(sc, 0x2cc4), 0) : 0;
    rx_fx_effect_bind(e + 0x90, ent, e + 0x190);
    if (U32(e, 0xbc) == 0) SU32(e, 0xc0, 1);
    SU32(e, 0xbc, 1);
    SF32(e, 0x180, 0.0f); SF32(e, 0x184, 0.0f); SF32(e, 0x188, 0.0f);
    return 0;
}
RX_FUNCTION(0x00696810, rx_placeable_effect_vf7);

/* 00696870 PlaceableParticleEffect::vf9 */
uint32_t rx_placeable_effect_vf9(rx_addr e) {
    if (U32(e, 0x94)) {
        const rx_addr mgr = U32(e, 0x94), fn = U32(U32(mgr, 0), 0x14);
        void (*f)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
        const uint32_t a[1] = {e + 0x110};
        if (f) f(mgr, e + 0x110);
        else rx_call_original(fn, RX_THISCALL, mgr, 1, a);
    }
    SU32(e, 0x94, 0); SU32(e, 0x70, 0);
    const rx_addr l1 = U32(0, 0x00a95824);
    if (l1) { const rx_addr fn = U32(U32(l1, 0), 4); const uint32_t a[1] = {e}; ui_thunk(fn, RX_THISCALL, l1, 1, a); }
    const rx_addr l2 = U32(0, 0x00a95828);
    if (l2) { const rx_addr fn = U32(U32(l2, 0), 4); const uint32_t a[1] = {e}; ui_thunk(fn, RX_THISCALL, l2, 1, a); }
    rx_reflex_aligned_free(U32(e, 0x18));
    return 0;
}
RX_FUNCTION(0x00696870, rx_placeable_effect_vf9);

/* the class ids 00692110 / 00691bb0 / 0067d370 are registered by the scene unit (build_scene_vf.c). */

/* 00692140 FireworksParticleEffect (ESI = the block; the base ctor then the class vtable and its fields) */
static rx_addr fireworks_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x2b0);
    if (!e) return 0;
    placeable_construct(e);
    SU32(e, 0, 0x0095e29c); SU32(e, 0x90, 0x0095e310);
    SU32(e, 0x70, 1); SU32(e, 0x2a8, 0); SU32(e, 0x2a0, 0); SU32(e, 0xb0, 0); SU32(e, 0x2a4, 0);
    return e;
}
/* 006923c0 TriggeredPlaceableParticleEffect */
static rx_addr triggered_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x2b0);
    if (!e) return 0;
    placeable_construct(e);
    SU32(e, 0, 0x0095e20c); SU32(e, 0x90, 0x0095e280);
    SU32(e, 0x2a0, 0); SU32(e, 0xb0, 0); SU32(e, 0x2a4, 0); SU32(e, 0x2a8, 0); SU32(e, 0x70, 1);
    return e;
}

/* the world's particle effect objects (rx_mxworld_vf3 007665f0 builds them with these constructors once the world has a
 * ParticleEntity): 00696480 PhysicsObjectSplashParticles (0x100 bytes, its emitter is a YRingParticleEmitter), 006963b0
 * PhysicsObjectDustParticles (0x110 bytes, three of them, +0xf0 / +0xf4 / +0xf8 = 5 / 0.5 / 5). */
rx_addr rx_physics_splash_particles_construct(rx_addr o) {
    SU32(o, 4, 0); SU32(o, 0, 0x0095e0c0);
    emitter_construct(o + 0x10);
    SU32(o + 0x10, 0, 0x0095e074 /* YRingParticleEmitter::vftable */);
    SU32(o, 0xd0, 0x0095df34);
    SF32(o, 0xe4, 1.0f);
    SU32(o, 0xc, 0);
    SU32(o, 0, 0x0095e1a8);
    return o;
}
RX_FUNCTION(0x00696480, rx_physics_splash_particles_construct);

rx_addr rx_physics_dust_particles_construct(rx_addr o) {
    SU32(o, 4, 0); SU32(o, 0, 0x009588ec);
    emitter_construct(o + 0x10);
    SF32(o, 0xe4, 1.0f);
    SU32(o, 0xd0, 0x0095df34);
    SU32(o, 0xc, 0);
    SU32(o, 0, 0x0095e1cc);
    SF32(o, 0xf0, 5.0f /* 0x935890 */); SF32(o, 0xf4, 0.5f /* 0x9322cc */); SF32(o, 0xf8, 5.0f);
    return o;
}
RX_FUNCTION(0x006963b0, rx_physics_dust_particles_construct);

/* ---------------------------------------------------------------------------------------------------------------------------
 * ParticleEntity (0x1030): ctor 00695c60, Load 00695de0, vf7 00695da0, vf9 00696040, id 00691bb0 */

static rx_addr particle_entity_construct(rx_addr e) {
    SU32(e, 0x24, 0); SU32(e, 0x20, 0); SU32(e, 0, 0x0095a2e4);
    SU32(e, 0x70, 0); SU32(e, 0x18, 0); SU32(e, 0x10, 0);
    scene_set_matrix_identity(e + 0x30);
    SU32(e, 0x7c, 0); SU32(e, 0x1c, 0); SU32(e, 0x78, 0); SU32(e, 0x74, 0); SU32(e, 0x28, 0);
    SU32(e, 0x90, 0x0093708c /* IRenderable::vftable */); SU32(e, 0x94, 0xf0);
    SU32(e, 0x90, 0x0095e034 /* ParticleEntity::vftable_o90 */);
    SU32(e, 0xac, e + 0x90);
    SF32(e, 0x98, 100.0f /* 0x42c80000 */);
    const uint32_t ms = U32(0, 0x00a93f10);                       /* SavedGameMinDisplayMs */
    SU32(e, 0, 0x0095dfbc);                                      /* ParticleEntity::vftable */
    SU32(e, 0xb0, 0); SU32(e, 0xb4, 0); SU32(e, 0xc0, 0); SU32(e, 0xc4, 0); SU32(e, 0xb8, 0);
    SU32(e, 0x2c, 1);
    SU32(e, 0x9c, ms); SU32(e, 0xa0, ms); SU32(e, 0xa4, ms); SU32(e, 0xa8, ms);
    SU32(e, 0x70, 1);
    return e;
}
static rx_addr particle_entity_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0xe0);
    return e ? particle_entity_construct(e) : 0;
}

/* 00697ce0 ParticleManager2D_Init: the simulation state.  The D3D vertex buffers / vertex declarations / shaders are render
 * resources; the oracle runs the original (exact), the product keeps none (there is no renderer). */
static void particle_manager_init(rx_addr mgr, int type) {
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = (uint32_t)type; in.r[6] = mgr;
    rx_call_original_ex(0x00697ce0, &in, &o, 0, 0);
#else
    loop_enter_cs(mgr + 0xf4);
    SU32(mgr, 0x24, U32(mgr, 0x24) + 1);
    if (I32(mgr, 0x24) > 1) {
        SU32(mgr, 0xac, 0); SU32(mgr, 0xb0, 0);
        loop_leave_cs(mgr + 0xf4);
        return;
    }
    const uint32_t cap = (type != 2) ? 10000u : 3000u;
    SU32(mgr, 0x1c, cap);
    SU32(mgr, 0xb4, rx_reflex_aligned_alloc(cap * 4));
    SU32(0, 0x00a94280, U32(0, 0x00a94280) + 1);
    SU32(0, 0x00a94284, U32(0, 0x00a94284) + cap - 2);
    SU32(mgr, 0xb8, 0); SU32(mgr, 0xac, 0); SU32(mgr, 0xa8, (uint32_t)type);
    SU32(mgr, 0xd0, 0); SU32(mgr, 0xcc, 0); SU32(mgr, 0xc8, 0);
    SU32(mgr, 0x14, rx_reflex_aligned_alloc(cap << 8));
    SU32(mgr, 0x18, rx_reflex_aligned_alloc(cap << 8));
    SU32(mgr, 0x28, rx_reflex_aligned_alloc(0x9c48));
    SU32(0, 0x00a9427c, U32(0, 0x00a9427c) + cap * 0x104 + 0x9c48);
    SU32(mgr, 0x1c, cap - 2);
    SU32(mgr, 0x64, 0);
    if (U32(mgr, 0x28)) for (uint32_t k = 0; k < 0x9c48; k += 4) SU32(U32(mgr, 0x28), k, 0);
    SU32(mgr, 0x74, 0);
    for (int k = 0; k < 3; k++) SF32(mgr, 0x30 + 4 * k, F32(0, 0x00a95d50 + 4 * k));
    for (int k = 0; k < 3; k++) SF32(mgr, 0x40 + 4 * k, F32(0, 0x00a95d50 + 4 * k));
    SF32(mgr, 0x50, 0.0f);
    SU32(mgr, 0xa0, 0); SU32(mgr, 0xa2, 0); SU32(mgr, 0xa4, 0); SU32(mgr, 0xa6, 0);
    loop_leave_cs(mgr + 0xf4);
#endif
}

/* 00695de0 ParticleEntity::Load */
uint32_t rx_particle_entity_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    const rx_addr sc = own_scene(e);
    SU32(0x00a94264 + 4u * U32(sc, 0x2cc4), 0, e);
    SU32(e, 0xc8, rx_game_current_world());
    SU32(e, 0xb0, rs_asset("ParticleEmitterAsset"));
    if (rs_has("TextureAsset", RS_ASSET)) SU32(e, 0xb8, rs_asset("TextureAsset"));
    else SU32(e, 0xb8, db_handle(database(), rs_asset_name("ParticleEmitterAsset"), 0x74786574u /* "text" */, 0x657275u /* "ure" */));
    if (rs_has("ParticleAttribAsset", RS_ASSET)) SU32(e, 0xb4, rs_asset("ParticleAttribAsset"));
    if (rs_has("MaxNormalParticles", RS_INT)) SU32(0, 0x0095dc68, (uint32_t)rs_int("MaxNormalParticles"));
    if (rs_has("MaxAdditiveParticles", RS_INT)) SU32(0, 0x0095dc7c, (uint32_t)rs_int("MaxAdditiveParticles"));
    if (rs_has("MaxViewDistance", RS_FLOAT)) SF32(e, 0x98, rs_float("MaxViewDistance"));
    if (rs_has("WindAccelleration", RS_VEC3)) { float v[3]; rs_vec3("WindAccelleration", v); for (int k = 0; k < 3; k++) SF32(e, 0xd0 + 4 * k, v[k]); }
    for (int i = 0; i < 4; i++) {
        char f[32];
        snprintf(f, sizeof f, "DrawDistance%d", i + 1);
        if (rs_has(f, RS_FLOAT)) SF32(e, 0x9c + 4 * i, rs_float(f));
    }
    SU32(e, 0xc0, 0x00c11bd0u); SU32(e, 0xc4, 0x00c11d80u);
    particle_manager_init(0x00c11bd0, 2);
    particle_manager_init(0x00c11d80, 1);
    SU32(e, 0xbc, db_handle(database(), "Editor_Default_White", 0x74786574u, 0x657275u));
    rx_entity_vf5(e);
    return 0;
}
#ifndef RX_ORACLE
RX_FUNCTION(0x00695de0, rx_particle_entity_vf5);   /* reads the scene RSData through the core fmt reader: product only (the oracle runs the original) */
#endif

/* 00695da0 ParticleEntity::vf7: collect the scene's 0x1030 entities into an 8-byte local (capacity 2) */
uint32_t rx_particle_entity_vf7(rx_addr e) {
    RX_FRAME_ENTER(F, 0x10);
    SU32(F, 0, 0); SU32(F, 4, 0);
    rx_scene_manager_collect_typed(own_scene(e), 0x1030, F, 2, 0);
    RX_FRAME_LEAVE(F, 0x10);
    return 0;
}
RX_FUNCTION(0x00695da0, rx_particle_entity_vf7);

/* 00696040 ParticleEntity::vf9 */
uint32_t rx_particle_entity_vf9(rx_addr e) {
    const rx_addr sc = own_scene(e);
    SU32(0x00a94264 + 4u * U32(sc, 0x2cc4), 0, 0);
    if (U32(e, 0xc0)) { const uint32_t a[1] = {U32(e, 0xc0)}; ui_thunk(0x00698210, RX_CDECL, 0, 1, a); }
    SU32(e, 0xc0, 0);
    if (U32(e, 0xc4)) { const uint32_t a[1] = {U32(e, 0xc4)}; ui_thunk(0x00698210, RX_CDECL, 0, 1, a); }
    SU32(e, 0xc4, 0); SU32(e, 0x70, 0);
    const rx_addr l1 = U32(0, 0x00a95824);
    if (l1) { const rx_addr fn = U32(U32(l1, 0), 4); const uint32_t a[1] = {e}; ui_thunk(fn, RX_THISCALL, l1, 1, a); }
    const rx_addr l2 = U32(0, 0x00a95828);
    if (l2) { const rx_addr fn = U32(U32(l2, 0), 4); const uint32_t a[1] = {e}; ui_thunk(fn, RX_THISCALL, l2, 1, a); }
    rx_reflex_aligned_free(U32(e, 0x18));
    return 0;
}
RX_FUNCTION(0x00696040, rx_particle_entity_vf9);

/* ---------------------------------------------------------------------------------------------------------------------------
 * AudioVolumeMixEntity (0x105e / 0x1088) */

#define VM_RECORDS 0x00d679a0u
#define VM_NAMES   0x009c49a0u
#define VM_COUNT   0x00a957c8u

static rx_addr audio_mix_construct(rx_addr e) {
    SU32(e, 0, 0x0095a2e4); SU32(e, 0x24, 0); SU32(e, 0x20, 0); SU32(e, 0x18, 0); SU32(e, 0x10, 0); SU32(e, 0x70, 0);
    scene_set_matrix_identity(e + 0x30);
    SU32(e, 0x1c, 0); SU32(e, 0x7c, 0); SU32(e, 0x78, 0); SU32(e, 0x74, 0); SU32(e, 0x28, 0); SU32(e, 0x2c, 0);
    SU32(e, 0, 0x0095d3d4);
    SU32(e, 0x130, 0);
    for (int k = 0; k < 40; k++) SF32(e, 0x90 + 4 * k, 1.0f);
    return e;
}
static rx_addr audio_mix_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x140);
    return e ? audio_mix_construct(e) : 0;
}

/* 0067d4d0 AudioVolumeMixEntity::vf5: the four VolumeMixN groups (channel values clamped to [0, 1]; the channel record's volume
 * when a field is absent) */
uint32_t rx_audio_mix_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    SU32(e, 0x70, 1);
    const int nch = I32(0, VM_COUNT);
    for (int g = 0; g < 4; g++) {
        char mix[32];
        snprintf(mix, sizeof mix, "VolumeMix%d", g + 1);
        if (!rs_has(mix, 0)) continue;
        for (int ch = 0; ch <= nch; ch++) {
            const rx_addr namep = U32(0, VM_NAMES + 4u * (uint32_t)ch);
            char field[64];
            uint32_t k = 0;
            for (; U8(namep, k) && k < 40; k++) field[k] = (char)U8(namep, k);
            const char* vol = "Volume";
            for (int j = 0; vol[j] && k < sizeof field - 1; j++) field[k++] = vol[j];
            field[k] = 0;
            float v;
            if (nested_float(mix, field, &v)) {
                if (v >= 1.0f) v = 1.0f;
                else if (!(0.0f < v) && v == v) v = 0.0f;          /* the original keeps a NaN */
            } else v = F32(0, VM_RECORDS + 0x54u * (uint32_t)ch);
            SF32(e, 0x90 + 0x28 * (uint32_t)g + 4u * (uint32_t)ch, v);
        }
    }
    return 0;
}
#ifndef RX_ORACLE
RX_FUNCTION(0x0067d4d0, rx_audio_mix_vf5);   /* reads the scene RSData through the core fmt reader: product only (the oracle runs the original) */
#endif

uint32_t rx_audio_mix_vf2(rx_addr e) {
    ui_thunk(0x0067d210, RX_CDECL, 0, 0, 0);                     /* Audio_ResetChannelMix (audio boundary) */
    SU32(e, 0x130, 0);
    return 0;
}
#ifndef RX_ORACLE
RX_FUNCTION(0x0067d7d0, rx_audio_mix_vf2);
#endif

/* 0067d810: the group's channel volumes into the channel records and their listeners notified */
static void audio_mix_apply(rx_addr e) {
    rx_addr w = 0;
    const rx_addr ec = U32(0, 0x00a95844);
    if (U32(U32(ec, 0x20), 0x34) == 1) w = U32(0, 0x00a946b8) ? U32(U32(0, 0x00a946b8), 0x5a54) : 0;
    else if (U32(U32(ec, 0x1c), 0x34) == 1) w = U32(0, 0x00a946b8) ? U32(U32(0, 0x00a946b8), 0x20c8) : 0;
    int32_t players = 0;
    if (w && I32(w, 0x21da8) > 0) {
        for (int32_t i = 0; i < I32(w, 0x21da8); i++) {
            const rx_addr A = U32(U32(w, 0x21da4), 4u * (uint32_t)i);
            if (!A) break;
            if (I32(A, 0x21c) != 5) { players = I32(A, 0x26c); break; }
            if (I32(A, 0x26c) > 0) players = I32(A, 0x26c);
        }
    }
    int32_t grp = (players - 1) / 3;
    if (grp < 0) grp = 0; else if (grp > 3) grp = 3;
    const int nch = I32(0, VM_COUNT);
    for (int ch = 0; ch <= nch; ch++) {
        const rx_addr rec = VM_RECORDS + 0x54u * (uint32_t)ch;
        SF32(rec, 0, F32(e, 0x90 + 0x28 * (uint32_t)grp + 4u * (uint32_t)ch));
        loop_enter_cs(rec + 0x28);
        const int n = I32(rec, 0x44);
        for (int k = 0; k < n; k++) {
            const rx_addr obj = U32(rec + 0x40, 4u * (uint32_t)k);
            if (!obj) continue;
            const rx_addr fn = U32(U32(obj, 0), 4);
            const uint32_t a[2] = {0, rec};
            ui_thunk(fn, RX_THISCALL, obj, 2, a);
        }
        loop_leave_cs(rec + 0x28);
    }
    SU32(e, 0x130, 1);
}
uint32_t rx_audio_mix_tick(rx_addr e, float dt) {
    (void)dt;
    if (U32(e, 0x130) == 0) { audio_mix_apply(e); SU32(e, 0x70, 0); }
    return 0;
}
#ifndef RX_ORACLE
RX_FUNCTION(0x0067d7f0, rx_audio_mix_tick);   /* the audio engine's channel records / listeners: product only (the oracle runs the original) */
#endif

/* ---------------------------------------------------------------------------------------------------------------------------
 * StatusBoardEntity (0x107e): a UI billboard.  The entity fields and matrices are ported; its UI text object / render event and
 * the tick's UI page update are UI (product: none; the oracle keeps the original's vf5 / vf19, these ports are product-only). */

static rx_addr status_board_construct(rx_addr e) {
    SU32(e, 0, 0x0095a2e4); SU32(e, 0, 0x0095a2e4);
    SU32(e, 0x70, 0); SU32(e, 0x18, 0); SU32(e, 0x10, 0); SU32(e, 0x24, 0); SU32(e, 0x20, 0);
    scene_set_matrix_identity(e + 0x30);
    SU32(e, 0x1c, 0); SU32(e, 0x7c, 0); SU32(e, 0x78, 0); SU32(e, 0x74, 0); SU32(e, 0x28, 0); SU32(e, 0x2c, 0);
    SU32(e, 0x90, 0x0093708c); SU32(e, 0x94, 0xf0); SU32(e, 0x98, 0x0093706c);
    SU32(e, 0, 0x00939de4); SU32(e, 0x90, 0x00939e5c); SU32(e, 0x98, 0x00939e9c);
    SU32(e, 0x110, 1);
    SU32(e, 0x114, 0); SU32(e, 0x118, 0); SU32(e, 0x10c, 0); SU32(e, 0x108, 0); SU32(e, 0x104, 0);
    SU32(e, 0xf8, 0); SU32(e, 0xfc, 0);
    SU32(e, 0x70, 1); SU32(e, 0x100, 0); SU32(e, 0x9c, 0);
    SU8(e, 0x11c, 0); SU8(e, 0x19c, 0); SU8(e, 0x15c, 0); SU8(e, 0x1dc, 0); SU8(e, 0x25c, 0); SU8(e, 0x21c, 0);
    return e;
}
static rx_addr status_board_create(void) {
    rx_addr e = rx_reflex_aligned_alloc(0x2a0);
    return e ? status_board_construct(e) : 0;
}

#ifndef RX_ORACLE
/* 00413ed0 StatusBoardEntity::vf5: the entity's fields (Material, TextHeight, BoardWidth / Height, PosterWidth) and the
 * billboard's default matrix.  The UI text object (globals 0xa89bd0 / 0xa89bd8) is UI. */
static uint32_t status_board_vf5(rx_addr e, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(e);
    SU32(e, 0x104, rs_asset("Material"));
    if (U32(e, 0x104)) {
        SU32(U32(e, 0x104), 0x4c, 1);
        SU8(U32(e, 0x104), 0x4d, 0); SU8(U32(e, 0x104), 0x4e, 0); SU8(U32(e, 0x104), 0x4f, 0);
        const rx_addr asset = U32(U32(e, 0x104), 0x44);
        if (asset) { SU32(e, 0x108, U32(asset, 0x1c)); SU32(U32(e, 0x104), 0x4c, 0); }
    }
    float h = rs_float("TextHeight");
    SF32(e, 0xa0, h == 0.0f ? 2.0f : h);
    SF32(e, 0xf8, rs_float("BoardWidth"));
    SF32(e, 0xfc, rs_float("BoardHeight"));
    SF32(e, 0xf0, rs_float("PosterWidth"));
    SU32(e, 0xb8, 0xb4fffff9u);                                  /* -4.7683696e-7 (0x977904) */
    SU32(e, 0xc4, 0x3f800000u); SU32(e, 0xec, 0x3f800000u);
    SU32(e, 0xb0, 0xbf7ffff3u); SU32(e, 0xd8, 0xbf7ffff3u);      /* -0.99999923 (0x977908) */
    SU32(e, 0xd0, 0x34fffff9u);                                  /* 4.7683696e-7 (0x977900) */
    SU32(e, 0xb4, 0); SU32(e, 0xbc, 0); SU32(e, 0xc0, 0); SU32(e, 0xc8, 0); SU32(e, 0xcc, 0); SU32(e, 0xd4, 0);
    SU32(e, 0xdc, 0); SU32(e, 0xe0, 0); SU32(e, 0xe4, 0); SU32(e, 0xe8, 0);
    (void)entity_world_matrix(e);
    SU32(e, 0x9c, 0);                                            /* no UI text billboard render event */
    return 0;
}
RX_FUNCTION(0x00413ed0, status_board_vf5);

/* 00416530 StatusBoardEntity::vf19: the game-state / UI-page gate; the UI updates are UI (none in the product) */
static uint32_t status_board_tick(rx_addr e, float dt) {
    (void)dt;
    const rx_addr g = U32(0, 0x00a946b8);
    const int32_t st = g ? I32(g, 0x18) : 0;
    if (I32(e, 0x110) != st) SU32(e, 0x110, (uint32_t)st);
    const rx_addr ui = U32(0, 0x00a93fcc);
    if (ui) SU32(e, 0x118, U32(ui, 0x298) | U32(ui, 0x290));
    return 0;
}
RX_FUNCTION(0x00416530, status_board_tick);
#endif

/* 004167d0 StatusBoardEntity::vf6_o90 (ECX = the entity's IRenderable at +0x90: stack center*, extents*, radius*): the board's
 * bounds: center (0, 0, 0), extents (BoardWidth * 0.5, BoardHeight * 0.5, 0.01), radius max(width, height) * 0.5. */
void rx_status_board_bounds(rx_addr self, rx_addr center, rx_addr ext, rx_addr radius) {
    SF32(center, 0, 0.0f); SF32(center, 4, 0.0f); SF32(center, 8, 0.0f);
    const float w = F32(self, 0x68), h = F32(self, 0x6c);
    SF32(radius, 0, rx_muls(w > h ? w : h, 0.5f));
    SF32(ext, 4, rx_muls(h, 0.5f));
    SF32(ext, 0, rx_muls(w, 0.5f));
    SF32(ext, 8, 0.01f /* 0x3c23d70a */);
}
RX_FUNCTION(0x004167d0, rx_status_board_bounds);

uint32_t rx_status_board_vf9(rx_addr e) {
    SU32(e, 0x70, 0);
    const rx_addr l1 = U32(0, 0x00a95824);
    if (l1) { const rx_addr fn = U32(U32(l1, 0), 4); const uint32_t a[1] = {e}; ui_thunk(fn, RX_THISCALL, l1, 1, a); }
    const rx_addr l2 = U32(0, 0x00a95828);
    if (l2) { const rx_addr fn = U32(U32(l2, 0), 4); const uint32_t a[1] = {e}; ui_thunk(fn, RX_THISCALL, l2, 1, a); }
    rx_reflex_aligned_free(U32(e, 0x18));
    return 0;
}
RX_FUNCTION(0x00416430, rx_status_board_vf9);
/* 0x107e / 00404690 are already registered by the scene unit (build_scene_vf.c). */

/* ---------------------------------------------------------------------------------------------------------------------------
 * the factory registrations (build_scene.c's extension point) */

void rx_scene_fx_register(void) {
    rx_scene_register_class(0x102a, placeable_create);
    rx_scene_register_class(0x102e, fireworks_create);
    rx_scene_register_class(0x102f, triggered_create);
    rx_scene_register_class(0x1030, particle_entity_create);
    rx_scene_register_class(0x105e, audio_mix_create);
    rx_scene_register_class(0x1088, audio_mix_create);
    rx_scene_register_class(0x107e, status_board_create);
}
