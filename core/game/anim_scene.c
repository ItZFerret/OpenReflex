/* anim_scene.c -- the scene manager's entity bookkeeping and tickers, and its event grid (0x007d1xxx..0x007dfxxx).
 *
 * Scene manager S (as these functions use it):
 *   +0x0004 CRITICAL_SECTION (entities), +0x001c CRITICAL_SECTION (the second sorted list), +0x0034 active,
 *   +0x013c + b * 0x110 (b < 0x18) entity buckets: +0x00 array, +0x04 count, +0x08 enabled (the bucket of an entity type),
 *   +0x19b8 / +0x19d0 render-sync objects (array, count), +0x19f8 name -> entity hash, +0x1a18 lights,
 *   +0x2bfc list {data, count, capacity, growth} of the entities whose vf11 says "tick list", +0x2c10 / +0x2c1c..+0x2c20 tickers
 *   (vf19), +0x2c28 list {data, count} of entities without a sort key, sorted checked vectors (proxy +0, first +0xc, last +0x10):
 *   +0x2c3c (kind 0, by key +0x78), +0x2c84 (kind 1), +0x2c54 (+0x28 flag), +0x2c6c (+0x2c flag); +0x2c60..+0x2c64 vf20
 *   entities, +0x2c78..+0x2c7c vf21 tickers, +0x2cc0 the type-14 entity, +0x2d14 time scale.
 * Engine context [0xa95844] +0x10: +0x40 time now, +0x48 largest step.
 * Event grid G: +0x10 / +0x14 origin x / z (int), +0x18 / +0x1c cell size x / z, +0x20 CRITICAL_SECTION, +0xf78 cells (0x70
 *   each), +0xf8c / +0xf90 cells in x / z. Cell: two lists of 32-byte events (id, radius, value, ?, position vec4): A +0x2c count,
 *   +0x30 head, +0x34 tail, +0x50 / +0x54 grow down / up, +0x64 storage; B +0x38 count, +0x40 tail, +0x58 storage.
 * (core/notes/anim.md) */
#include "anim.h"
#include "anim_extern.h"
#include "anim_scene_misc.h"
#include "anim_scene_vec.h"
#include "crt.h"
#include "race_misc.h"
#include "world_misc.h"
#include "build_rider.h"
#include "build_asset.h"
#include "loop_player.h"
#include "rx_vcall.h"
#include "build_scene_int.h"

static rx_addr engine_time_source(void) { return U32(U32(0, 0x00a95844), 0x10); }

/* ---- callees outside the list ---- */
static void vsnprintf_fmt(rx_addr buf, uint32_t size, rx_addr fmt, rx_addr arg) {     /* 0078fd60: ESI size, EDI buffer */
    const rx_addr sp = rx_sp();                     /* push arg; push fmt; call: the argument words on the game stack */
    SU32(sp - 4, 0, arg); SU32(sp - 8, 0, fmt); rx_set_sp(sp - 12);
    rx_format_buffer(buf, size, fmt, sp - 4);       /* crt_format.c */
    rx_set_sp(sp);
}
static void tickers_refresh(rx_addr list, rx_addr src) {                              /* 007f8fa0: EAX list, stack src; RET 4 */
    uint32_t ecx_out;
    ANIM_CALL_FRAME(4); (void)rx_vector_assign(list, src, 0, &ecx_out); ANIM_CALL_FRAME_END(4);
}
static void render_sync_object_update(rx_addr o) {                                    /* 00794d10: EDI object */
#ifdef RX_ORACLE
    rx_regs r = anim_regs(); r.r[ANIM_EDI] = o; rx_call_original_ex(0x00794d10, &r, 0, 0, 0);
#else
    (void)o;   /* PRODUCT BOUNDARY: the renderer's camera sync of a viewport (00794d10); nothing gameplay reads its fields */
#endif
}
static void checked_vector_erase(rx_addr vec, rx_addr out, rx_addr proxy, rx_addr ptr) {  /* 007f9140: ESI vec, EDI out; RET 8 */
    (void)proxy;
    ANIM_CALL_FRAME(8); (void)rx_vector_erase(vec, out, ptr); ANIM_CALL_FRAME_END(8);
}
/* 00544cc0 (ESI list, stack &value; RET 4) at an original call site: push &value; ESI = list; call (return ret), with the caller's EBX EBP EDI (the frames it leaves) */
static void list_push_unique_at(rx_addr list, rx_addr value_ptr, uint32_t ret, uint32_t ebx, uint32_t ebp, uint32_t edi) {
    const rx_addr e = rx_sp() - 8;
    SU32(e, 0, ret); SU32(e, 4, value_ptr);
    const crt_regs4 r = {ebx, ebp, list, edi};
    ANIM_CALL_FRAME(4); (void)rx_world_ptr_vector_push_unique_fr(e, r, list, value_ptr); ANIM_CALL_FRAME_END(4);
}
static void name_hash_erase(rx_addr map, rx_addr key_ptr) {                           /* 0084dd30: EDI map, stack &key; RET 4 */
    ANIM_CALL_FRAME(4); (void)rx_strtab_remove(map, key_ptr); ANIM_CALL_FRAME_END(4);
}
/* 007d2500 Scene_RegisterEntityName (EAX = the scene, stack entity; RET 4): under the scene's lock (+4) the entity's name (the pointer at
 * entity +0x18) is looked up in the scene's name table (+0x19f8, 007fa250: EAX = the table, EDI = the address of the key slot, stack the
 * value out) and, when it is not there, inserted with the entity as its value (007cfaf0: EDI = the table, stack the addresses of the key
 * slot and of the argument slot). Frame: sub esp, 8; push ebx / ebp / esi / edi: the key slot at sp0 - 4, the lookup result at sp0 - 8. */
void rx_scene_register_entity_name(rx_addr scene, rx_addr entity) {
    /* entered at the return slot R (the entity at R + 4); the calls' frames are the original's (the lookup 007fa250 and the insert
     * 007cfaf0 with theirs: build_asset.h), as SceneManager_AddEntity's Lights_Add reads this depth back (dead stack) */
    const rx_addr R = rx_sp();
    const rx_addr key = R - 4, found = R - 8, lock = scene + 4, table = scene + 0x19f8;
    SU32(R, 4, entity);
    SU32(R, -0xc, anim_reg_get(ANIM_EBX));
    SU32(R, -0x10, anim_reg_get(ANIM_EBP));
    SU32(R, -0x14, anim_reg_get(ANIM_ESI));
    SU32(R, -0x18, anim_reg_get(ANIM_EDI));
    rx_set_sp(R - 0x18);
    { const rx_addr sp = rx_sp(); anim_enter_cs(lock); rx_set_sp(sp); SU32(R, -0x1c, lock); SU32(R, -0x20, 0x007d2517); }
    SU32(key, 0, U32(entity, 0x18));
    SU32(R, -0x1c, found); SU32(found, 0, 0); SU32(R, -0x20, 0x007d253c);
    { const crt_regs4 r = {table, entity, lock, key}; (void)rx_name_hash_find_at(R - 0x20, r, table, key, found); }
    if (U32(found, 0) == 0) {
        SU32(R, -0x1c, R + 4); SU32(R, -0x20, key); SU32(key, 0, U32(entity, 0x18)); SU32(R, -0x24, 0x007d255b);
        const crt_regs4 r = {table, entity, lock, table};
        rx_strtab_insert_at(R - 0x24, r, key, table, key, R + 4);
    }
    { const rx_addr sp = rx_sp(); anim_leave_cs(lock); rx_set_sp(sp); SU32(R, -0x1c, lock); SU32(R, -0x20, 0x007d2562); }
    rx_set_sp(R);
}
RX_FUNCTION(0x007d2500, rx_scene_register_entity_name)

static void lights_remove(rx_addr lights, rx_addr e) {                                /* 007e0190: EAX lights, stack entity; RET 4 */
    ANIM_CALL_FRAME(4); rx_lights_remove_entity(lights, e); ANIM_CALL_FRAME_END(4);
}

/* ---- profiling ---- */

/* 007d1cd0 Scene_ProfileScope (stdcall: name or 0; RET 4): formats the scope's label ("Scene:%s", or "Scene") into a local
 * buffer for the profiler (no other effect). */
void rx_scene_profile_scope(rx_addr name) {
    /* sub esp, 0x108; the stack cookie at [esp + 0x104]; what its calls leave in the dead stack is written as the original's
     * (their return addresses, the saved ESI / EDI, the pushed arguments: a heap buffer later copies this stack) */
    const rx_addr sp0 = rx_sp();
    const rx_addr buf = sp0 - 0x108;
    SU32(sp0 - 4, 0, U32(0, 0x009b45e4) ^ (sp0 - 0x108));
    rx_set_sp(sp0 - 0x108);
    if (name) {
        SU32(sp0 - 0x10c, 0, anim_reg_get(ANIM_ESI)); SU32(sp0 - 0x110, 0, anim_reg_get(ANIM_EDI));
        rx_set_sp(sp0 - 0x110);                     /* + push esi, edi */
        vsnprintf_fmt(buf, 0x104, 0x00971fa8 /* "Scene:%s" */, name);
        anim_ret_slot(8, 0x007d1d05);
        rx_set_sp(sp0 - 0x108);
        SU32(sp0 - 0x10c, 0, 0x007d1d18);           /* __security_check_cookie's return address */
    } else {
        SU32(sp0 - 0x10c, 0, 0x103); SU32(sp0 - 0x110, 0, 0x00971fb4); SU32(sp0 - 0x114, 0, buf); SU32(sp0 - 0x118, 0, 0x007d1d36);
        crt_strncpy(buf, 0x00971fb4 /* "Scene" */, 0x103);
        SU32(sp0 - 0x10c, 0, 0x007d1d47);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007d1cd0, rx_scene_profile_scope)

static void profile_scope(rx_addr name) {
    ANIM_CALL_FRAME(4); rx_scene_profile_scope(name); ANIM_CALL_FRAME_END(4);
}

/* A ticker's virtual tick (thiscall, one float: the elapsed time) through vtable byte offset `off`. */
typedef void (*anim_tick_fn)(rx_addr self, float dt);
static void entity_tick(rx_addr e, uint32_t off, float dt) {
    rx_addr target = U32(U32(e, 0), off);
    anim_tick_fn f = RX_FN(target, anim_tick_fn);
    if (f) { ANIM_CALL_FRAME(4); f(e, dt); ANIM_CALL_FRAME_END(4); return; }
    uint32_t a = rx_fb(dt);
    rx_call_original(target, RX_THISCALL, e, 1, &a);
}

/* The elements of a checked std::vector<T*> (first +0xc, last +0x10 of the vector at v); `i` checked against the size. */
static rx_addr checked_at(rx_addr v, uint32_t i) {
    if (!(i < (uint32_t)((int32_t)(U32(v, 0x10) - U32(v, 0xc)) >> 2))) anim_invalid_parameter();
    return U32(U32(v, 0xc), 4 * i);
}

/* ---- tickers ---- */

/* 007d23a0 SceneManager_Tick (stdcall: scene; RET 4): every ticker (+0x2c1c) whose interval (+0x20) has passed since its last tick
 * (+0x24) gets vf19 with the elapsed time (capped at the engine's largest step) times the time scale +0x2d14. */
void rx_scene_manager_tick(rx_addr s) {
    if (U32(s, 0x34) == 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x14);                    /* push ecx, ebp, ebx, esi, edi: the tickers may keep stack addresses */
    profile_scope(0x00971fbc /* "SceneManager::Tick" */);
    float now = F32(engine_time_source(), 0x40), cap = F32(engine_time_source(), 0x48);
    anim_enter_cs(s + 4);
    tickers_refresh(s + 0x2c10, s + 0x2c3c);
    anim_leave_cs(s + 4);
    int32_t n = (int32_t)(U32(s, 0x2c20) - U32(s, 0x2c1c)) >> 2;
    for (int32_t i = 0; i < n; i++) {
        rx_addr t = checked_at(s + 0x2c10, (uint32_t)i);
        float dt = now - F32(t, 0x24);
        if (!(dt >= F32(t, 0x20))) continue;
        if (dt > cap) dt = cap;
        anim_x87_copy_f32(t + 0x24, engine_time_source() + 0x40);
        entity_tick(t, 0x4c, F32(s, 0x2d14) * dt);
    }
    rx_set_sp(sp0);                           /* the closing scope is a tail jump (its RET 4 is ours) */
    rx_scene_profile_scope(0x00971fbc);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007d23a0, rx_scene_manager_tick)

/* 007d21d0 SceneManager_TickLate (EDI = scene): the same for the +0x2c78 tickers (vf21), without updating their last tick. */
void rx_scene_manager_tick_late(rx_addr s) {
    if (U32(s, 0x34) == 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x10);                    /* sub esp, 8; push ebx, esi */
    profile_scope(0x00971fbc);
    int32_t n = (int32_t)(U32(s, 0x2c7c) - U32(s, 0x2c78)) >> 2;
    float now = F32(engine_time_source(), 0x40), cap = F32(engine_time_source(), 0x48);
    for (int32_t i = 0; i < n; i++) {
        rx_addr t = checked_at(s + 0x2c6c, (uint32_t)i);
        float dt = now - F32(t, 0x24);
        if (!(dt >= F32(t, 0x20))) continue;
        if (dt > cap) dt = cap;
        rx_set_sp(sp0 - 0x14);                /* + push ebp */
        entity_tick(t, 0x54, F32(s, 0x2d14) * dt);
        rx_set_sp(sp0 - 0x10);
    }
    profile_scope(0x00971fbc);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007d21d0, rx_scene_manager_tick_late)

/* 007d2190 SceneManager_UpdateRenderSync (stdcall: scene; RET 4): 00794d10 on every render-sync object (+0x19b8, count +0x19d0). */
void rx_scene_manager_update_render_sync(rx_addr s) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(((sp0 - 4u) & ~7u) - 0x10);     /* push ebp; and esp, -8; push ecx, ebx, esi, edi */
    for (uint32_t i = 0; i < U32(s, 0x19d0); i++) render_sync_object_update(U32(s, 0x19b8 + 4 * i));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007d2190, rx_scene_manager_update_render_sync)

/* 007d2490 SceneManager_PostUpdate (stdcall: scene; RET 4): vf20 of every +0x2c60 entity, then the render-sync objects. */
void rx_scene_manager_post_update(rx_addr s) {
    const rx_addr sp0 = rx_sp();
    int32_t n = (int32_t)(U32(s, 0x2c64) - U32(s, 0x2c60)) >> 2;
    rx_set_sp(sp0 - 0x10);                    /* push ebx, esi, edi, ebp */
    for (int32_t i = 0; i < n; i++) anim_vcall0(checked_at(s + 0x2c54, (uint32_t)i), 0x50);
    rx_set_sp(sp0 - 0xc);
    ANIM_CALL_FRAME(4); rx_scene_manager_update_render_sync(s); ANIM_CALL_FRAME_END(4);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007d2490, rx_scene_manager_post_update)

/* ---- entity lookups ---- */

/* 007d2e70 SceneManager_FindTyped (stdcall: scene, type, name; RET 0xc): the first entity of the buckets whose vf22 type is
 * `type` and whose name (+0x18) equals `name` (strcmp), or 0. */
rx_addr rx_scene_manager_find_typed(rx_addr s, uint32_t type, rx_addr name) {
    anim_enter_cs(s + 4);
    for (uint32_t b = 0; b < 0x18; b++) {
        rx_addr bucket = s + 0x13c + 0x110 * b;
        for (int32_t i = 0; i < I32(bucket, 4); i++) {
            if (anim_vcall0(U32(U32(bucket, 0), 4u * (uint32_t)i), 0x58) != type) continue;
            rx_addr n = U32(U32(U32(bucket, 0), 4u * (uint32_t)i), 0x18);
            if (n && anim_strcmp(n, name) == 0) {
                rx_addr e = U32(U32(s, 0x13c + 0x110 * b), 4u * (uint32_t)i);
                anim_leave_cs(s + 4);
                return e;
            }
        }
    }
    anim_leave_cs(s + 4);
    return 0;
}
RX_FUNCTION(0x007d2e70, rx_scene_manager_find_typed)

/* 007d2c80 SceneManager_FindInSubscene (stdcall: scene, entity name, subscene name or 0; RET 0xc): under the scene's lock, the 24
 * buckets (+0x38 + 0x110 * b: +0 the subscene name, +0x104 the entity array, +0x108 its count, +0x10c in use) in order: an in-use
 * bucket whose name equals the subscene name (every in-use bucket when that is 0) yields the first of its entities whose name
 * (+0x18; a null name never matches) equals the entity name, or 0. */
rx_addr rx_scene_manager_find_in_subscene(rx_addr s, rx_addr name, rx_addr subscene) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x18);                    /* sub esp, 8; push ebx, ebp, esi, edi */
    anim_enter_cs(s + 4);
    rx_addr found = 0;
    for (uint32_t b = 0; b < 0x18 && found == 0; b++) {
        const rx_addr bucket = s + 0x38 + 0x110 * b;
        if (U32(bucket, 0x10c) == 0) continue;
        if (subscene != 0 && anim_strcmp(subscene, bucket) != 0) continue;
        for (int32_t i = 0; i < I32(bucket, 0x108); i++) {
            const rx_addr entity = U32(U32(bucket, 0x104), 4u * (uint32_t)i);
            const rx_addr n = U32(entity, 0x18);
            if (n != 0 && anim_strcmp(n, name) == 0) { found = entity; break; }
        }
    }
    anim_leave_cs(s + 4);
    rx_set_sp(sp0);
    return found;
}
RX_FUNCTION(0x007d2c80, rx_scene_manager_find_in_subscene)

/* 007d2920 SceneManager_CollectTyped (EAX = scene; stack: type, out array, capacity, overflow flag ptr or 0; RET 0x10): the
 * entities of enabled buckets whose vf22 type is `type`, up to the capacity (then *overflow = 1); returns the count. */
uint32_t rx_scene_manager_collect_typed(rx_addr s, uint32_t type, rx_addr out, int32_t cap, rx_addr overflow) {
    anim_enter_cs(s + 4);
    if (overflow) SU32(overflow, 0, 0);
    int32_t n = 0;
    for (uint32_t b = 0; b < 0x18; b++) {
        rx_addr bucket = s + 0x140 + 0x110 * b;          /* count; +4 enabled; -4 array */
        if (U32(bucket, 4) == 0) continue;
        for (int32_t i = 0; i < I32(bucket, 0); i++) {
            if (n >= cap) goto full;
            if (anim_vcall0(U32(U32(bucket, -4), 4u * (uint32_t)i), 0x58) == type)
                SU32(out, 4u * (uint32_t)n++, U32(U32(bucket, -4), 4u * (uint32_t)i));
        }
        if (n >= cap) goto full;
    }
    anim_leave_cs(s + 4);
    return (uint32_t)n;
full:
    if (overflow) SU32(overflow, 0, 1);
    anim_leave_cs(s + 4);
    return (uint32_t)n;
}
RX_FUNCTION(0x007d2920, rx_scene_manager_collect_typed)

/* 007d2da0 SceneManager_FindNamed (stdcall: scene, name; RET 8): the first entity of any bucket named `name`, or 0. */
rx_addr rx_scene_manager_find_named(rx_addr s, rx_addr name) {
    anim_enter_cs(s + 4);
    for (uint32_t b = 0; b < 0x18; b++) {
        rx_addr bucket = s + 0x13c + 0x110 * b;
        int32_t count = I32(bucket, 4);
        for (int32_t i = 0; i < count; i++) {
            rx_addr n = U32(U32(U32(bucket, 0), 4u * (uint32_t)i), 0x18);
            if (n && anim_strcmp(n, name) == 0) {
                rx_addr e = U32(U32(s, 0x13c + 0x110 * b), 4u * (uint32_t)i);
                anim_leave_cs(s + 4);
                return e;
            }
        }
    }
    anim_leave_cs(s + 4);
    return 0;
}
RX_FUNCTION(0x007d2da0, rx_scene_manager_find_named)

/* std::find over a checked vector<T*> (proxy +0, first +0xc, last +0x10), with the debug iterator checks: the iterator
 * (proxy, element) of the value, or (proxy, last). */
static void checked_vector_find(rx_addr v, uint32_t value, uint32_t* it_proxy, rx_addr* it_ptr) {
    rx_addr p = U32(v, 0xc);
    if (p > U32(v, 0x10)) anim_invalid_parameter();
    rx_addr last = U32(v, 0x10), proxy = U32(v, 0);
    if (U32(v, 0xc) > last) anim_invalid_parameter();
    rx_addr proxy_end = U32(v, 0);
    for (;;) {
        if (proxy == 0 || proxy != proxy_end) anim_invalid_parameter();
        if (p == last) {
            rx_addr l = U32(v, 0x10);
            if (U32(v, 0xc) > l) anim_invalid_parameter();
            *it_proxy = U32(v, 0); *it_ptr = l;
            return;
        }
        rx_addr cont = 0;
        if (proxy == 0) anim_invalid_parameter(); else cont = U32(proxy, 0);
        if (!(p < U32(cont, 0x10))) anim_invalid_parameter();
        if (U32(p, 0) == value) { *it_proxy = proxy; *it_ptr = p; return; }
        if (proxy == 0) { anim_invalid_parameter(); cont = 0; } else cont = U32(proxy, 0);
        if (!(p < U32(cont, 0x10))) anim_invalid_parameter();
        p += 4;
    }
}

/* 007d3a40 SceneManager_FindSorted0 (stdcall: scene, iterator out, entity; RET 0xc): the entity's iterator in the +0x2c3c list
 * (under the entity lock); returns the out pointer. */
rx_addr rx_scene_manager_find_sorted0(rx_addr s, rx_addr out, uint32_t e) {
    anim_enter_cs(s + 4);
    uint32_t proxy; rx_addr p;
    checked_vector_find(s + 0x2c3c, e, &proxy, &p);
    anim_leave_cs(s + 4);
    SU32(out, 0, proxy); SU32(out, 4, p);
    return out;
}
RX_FUNCTION(0x007d3a40, rx_scene_manager_find_sorted0)

static rx_addr find_in(rx_addr v, rx_addr out, uint32_t e) {
    uint32_t proxy; rx_addr p;
    checked_vector_find(v, e, &proxy, &p);
    SU32(out, 0, proxy); SU32(out, 4, p);
    return out;
}
/* 007d3b50 / 007d3c40 / 007d3d30 SceneManager_FindSorted1 / Flagged28 / Flagged2c (stdcall: scene, iterator out, entity;
 * RET 0xc): the same in the +0x2c84 / +0x2c54 / +0x2c6c lists, without locking. */
rx_addr rx_scene_manager_find_sorted1(rx_addr s, rx_addr out, uint32_t e) { return find_in(s + 0x2c84, out, e); }
RX_FUNCTION(0x007d3b50, rx_scene_manager_find_sorted1)
rx_addr rx_scene_manager_find_flagged28(rx_addr s, rx_addr out, uint32_t e) { return find_in(s + 0x2c54, out, e); }
RX_FUNCTION(0x007d3c40, rx_scene_manager_find_flagged28)
rx_addr rx_scene_manager_find_flagged2c(rx_addr s, rx_addr out, uint32_t e) { return find_in(s + 0x2c6c, out, e); }
RX_FUNCTION(0x007d3d30, rx_scene_manager_find_flagged2c)

/* Erase the found iterator from the checked vector v (debug checks as the original's). it = (proxy, element). */
static void erase_found(rx_addr v, uint32_t proxy, rx_addr p, rx_addr ret_iter) {
    rx_addr last = U32(v, 0x10);
    if (U32(v, 0xc) > last) anim_invalid_parameter();
    if (proxy == 0 || proxy != U32(v, 0)) anim_invalid_parameter();
    if (p != last) checked_vector_erase(v, ret_iter, proxy, p);
}

/* 007d3850 SceneManager_Unlink (stdcall: scene, entity; RET 8): removes the entity from the four sorted lists (the kind-1 list
 * under its own lock) and from the unsorted +0x2c28 list (swap with the last). */
void rx_scene_manager_unlink(rx_addr s, rx_addr e) {
    const rx_addr sp0 = rx_sp();
    const rx_addr it = sp0 - 0x14, ret_iter = sp0 - 0xc, it2 = sp0 - 0xc;
    rx_set_sp(sp0 - 0x2c);
    anim_enter_cs(s + 4);
    ANIM_CALL_FRAME(0xc); rx_scene_manager_find_sorted0(s, it, e); ANIM_CALL_FRAME_END(0xc);
    erase_found(s + 0x2c3c, U32(it, 0), U32(it, 4), ret_iter);
    anim_enter_cs(s + 0x1c);
    ANIM_CALL_FRAME(0xc); rx_addr r = rx_scene_manager_find_sorted1(s, it2, e); ANIM_CALL_FRAME_END(0xc);
    erase_found(s + 0x2c84, U32(r, 0), U32(r, 4), ret_iter);
    anim_leave_cs(s + 0x1c);
    ANIM_CALL_FRAME(0xc); r = rx_scene_manager_find_flagged28(s, it2, e); ANIM_CALL_FRAME_END(0xc);
    erase_found(s + 0x2c54, U32(r, 0), U32(r, 4), ret_iter);
    ANIM_CALL_FRAME(0xc); r = rx_scene_manager_find_flagged2c(s, it2, e); ANIM_CALL_FRAME_END(0xc);
    erase_found(s + 0x2c6c, U32(r, 0), U32(r, 4), ret_iter);
    int32_t n = I32(s, 0x2c2c), i;
    for (i = 0; i < n; i++) if (U32(U32(s, 0x2c28), 4u * (uint32_t)i) == e) break;
    if (i < n) {
        SU32(U32(s, 0x2c28), 4u * (uint32_t)i, U32(U32(s, 0x2c28), 4u * (uint32_t)n - 4));
        SU32(s, 0x2c2c, U32(s, 0x2c2c) - 1);
    }
    anim_leave_cs(s + 4);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007d3850, rx_scene_manager_unlink)


/* ---- SceneManager_Link / AddEntity with the original's frames ----
 * Lights_Add 007dfdd0, which AddEntity calls at the depth where it called Link, copies never-written locals of its frame (the
 * light position's w lane) into the scene's light event records, so Link's and AddEntity's frames (pushes, arguments, return
 * addresses, locals, the sorted inserts' frames: anim_scene_vec.c) are written as the original's, with its registers in the
 * register model for the originals and the core callees they reach. */

/* a call through the register model's registers: `ret` at the return slot e (the stack pointer is e + 4 + the pushed bytes) */
static void sc_regs(uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi) {
    anim_reg_set(ANIM_EBX, ebx); anim_reg_set(ANIM_EBP, ebp); anim_reg_set(ANIM_ESI, esi); anim_reg_set(ANIM_EDI, edi);
}
/* `push cs; call [Enter / LeaveCriticalSection]`: the argument at e + 4, the return address at e */
static void sc_cs(rx_addr e, int enter, rx_addr cs, rx_addr ret) {
    const rx_addr sp = rx_sp();
    rx_set_sp(e + 8);
    if (enter) anim_enter_cs(cs); else anim_leave_cs(cs);
    SU32(e, 4, cs); SU32(e, 0, ret);
    rx_set_sp(sp);
}
/* a no-argument virtual call (ECX = obj) with the return slot e */
static uint32_t sc_vcall0(rx_addr e, rx_addr obj, uint32_t off, rx_addr ret) {
    const rx_addr sp = rx_sp();
    rx_set_sp(e + 4);
    SU32(e, 0, ret);
    const uint32_t r = anim_vcall0(obj, off);
    SU32(e, 0, ret);
    rx_set_sp(sp);
    return r;
}
/* one sorted insert of SceneManager_Link (by key +0x78, upper bound) into the checked vector v: the binary search, then
 * 007f90a0 at the return slot L - 0x38 (EBX / EBP = proxy / vector, or vector / proxy for the kind-1 list: `swap`) */
static void link_insert_sorted(rx_addr L, rx_addr v, rx_addr e, int swap, rx_addr ret) {
    const rx_addr first = U32(v, 0xc);
    if (first > U32(v, 0x10)) anim_invalid_parameter();
    const rx_addr last = U32(v, 0x10), proxy = U32(v, 0);
    if (U32(v, 0xc) > last) anim_invalid_parameter();
    const int32_t n0 = (int32_t)(last - first) >> 2;
    SU32(L, -8, first);
    const float key = F32(e, 0x78);
    rx_addr lo = first;
    int32_t n = n0;
    while (n > 0) {
        const int32_t half = n / 2;
        if (key > F32(U32(lo, 4u * (uint32_t)half), 0x78)) { lo = lo + 4u * (uint32_t)half + 4; n += -1 - half; }
        else n = half;
    }
    SU32(L, -0x38, ret);
    const crt_regs4 r = {swap ? v : proxy, swap ? proxy : v, (uint32_t)n0, lo};
    (void)rx_scene_vector_insert_at(L - 0x38, r, v, L - 0xc, L + 8, proxy, lo);
}

/* 007d3580 SceneManager_Link (stdcall: scene, entity; RET 8): an entity without a sort list (+0x70 == 0) goes to the unsorted
 * list; otherwise it gets a sort key (+0x78; 0 = the next of the global counter 0xa95838, step 0.1) and is inserted by key into
 * the kind-0 or kind-1 list (vf18), the +0x28 list and the +0x2c list as its flags say. Frame: sub esp, 0x14; push ebx / ebp /
 * esi / edi (EBP = the scene, ESI = [EnterCriticalSection], EDI = the entity); [L - 0x10] the lock, [L - 0xc] the result
 * iterator, [L - 8] the list's first. Entered at the return slot L (the arguments at L + 4 / L + 8). */
void rx_scene_manager_link(rx_addr s, rx_addr e) {
    const rx_addr L = rx_sp();
    const uint32_t ebx0 = anim_reg_get(ANIM_EBX), ebp0 = anim_reg_get(ANIM_EBP), esi0 = anim_reg_get(ANIM_ESI), edi0 = anim_reg_get(ANIM_EDI);
    const uint32_t enter = U32(0, 0x0091600c);
    SU32(L, 4, s); SU32(L, 8, e);
    SU32(L, -0x18, ebx0); SU32(L, -0x1c, ebp0); SU32(L, -0x20, esi0); SU32(L, -0x24, edi0);
    rx_set_sp(L - 0x24);
    sc_regs(ebx0, s, enter, edi0);
    SU32(L, -0x10, s + 4);
    sc_cs(L - 0x2c, 1, s + 4, 0x007d359b);
    sc_regs(ebx0, s, enter, e);
    rx_addr leave_ret = 0x007d3846;
    if (U32(e, 0x70) == 0) {
        /* lea edx, [esp + 0x2c] (the entity's argument slot); push edx; ESI = the list; call 00544cc0 (return 007d383b) */
        SU32(L, -0x28, L + 8); SU32(L, -0x2c, 0x007d383b);
        const crt_regs4 r = {ebx0, s, s + 0x2c28, e};
        rx_set_sp(L - 0x2c);
        (void)rx_world_ptr_vector_push_unique_fr(L - 0x2c, r, s + 0x2c28, L + 8);
        rx_set_sp(L - 0x24);
    } else {
        if (F32(e, 0x78) == 0.0f) {
            float k = F32(0, 0x00a95838) + 0.1f /* 0x931d94 */;
            SF32(0, 0x00a95838, k);
            SF32(e, 0x78, k);
        }
        const uint32_t kind = sc_vcall0(L - 0x28, e, 0x48, 0x007d35e1);
        if (kind == 0) link_insert_sorted(L, s + 0x2c3c, e, 0, 0x007d365b);
        else if (sc_vcall0(L - 0x28, e, 0x48, 0x007d3669) == 1) {
            sc_cs(L - 0x2c, 1, s + 0x1c, 0x007d3678);
            link_insert_sorted(L, s + 0x2c84, e, 1, 0x007d36f6);
            sc_cs(L - 0x2c, 0, s + 0x1c, 0x007d3704);
        }
        if (U32(e, 0x28) != 0) link_insert_sorted(L, s + 0x2c54, e, 0, 0x007d3788);
        if (U32(e, 0x2c) != 0) { link_insert_sorted(L, s + 0x2c6c, e, 0, 0x007d3816); leave_ret = 0x007d3821; }
    }
    sc_cs(L - 0x2c, 0, s + 4, leave_ret);
    sc_regs(ebx0, ebp0, esi0, edi0);
    rx_set_sp(L);
}
RX_FUNCTION(0x007d3580, rx_scene_manager_link)

/* 007d2f70 SceneManager_AddEntity (EDI = entity, stack scene; RET 4): links it, registers its name (+0x1c), its light (vf25),
 * the tick list (vf11), the type-14 entity, then its vf1 (added). Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 8; push
 * ebx / esi / edi (EBX = the scene, ESI = the lock after the link and the name; [A8 - 4] the lock). Entered at the return slot. */
void rx_scene_manager_add_entity(rx_addr e, rx_addr s) {
    const rx_addr E = rx_sp(), fp = E - 4, A8 = fp & ~7u;
    const uint32_t ebx0 = anim_reg_get(ANIM_EBX), ebp0 = anim_reg_get(ANIM_EBP), esi0 = anim_reg_get(ANIM_ESI), edi0 = anim_reg_get(ANIM_EDI);
    SU32(E, 4, s);
    SU32(E, -4, ebp0);
    SU32(A8, -0xc, ebx0); SU32(A8, -0x10, esi0); SU32(A8, -0x14, edi0);
    /* push ebx (the scene; the entity is the saved EDI below it); call 007d3580 (return 007d2f85; RET 8) */
    SU32(A8, -0x18, s); SU32(A8, -0x1c, 0x007d2f85);
    sc_regs(s, fp, esi0, e);
    rx_set_sp(A8 - 0x1c);
    rx_scene_manager_link(s, e);
    rx_set_sp(A8 - 0x10);
    if (U32(e, 0x1c) != 0) {
        /* push edi; EAX = the scene; call 007d2500 (return 007d2f93; RET 4) */
        SU32(A8, -0x14, e); SU32(A8, -0x18, 0x007d2f93);
        rx_set_sp(A8 - 0x18);
        rx_scene_register_entity_name(s, e);
        rx_set_sp(A8 - 0x10);
    }
    SU32(A8, -4, s + 4);
    sc_cs(A8 - 0x18, 1, s + 4, 0x007d2fa1);
    sc_regs(s, fp, s + 4, e);
    if (sc_vcall0(A8 - 0x14, e, 0x64, 0x007d2faa) != 0) {
        SU32(A8, -0x14, 0x00971fa0); SU32(A8, -0x18, 0x007d2fb8);
        rx_set_sp(A8 - 0x18); rx_scene_profile_scope(0x00971fa0 /* "Octree" */); rx_set_sp(A8 - 0x10);
        SU32(A8, -0x14, e); SU32(A8, -0x18, s + 0x1a18); SU32(A8, -0x1c, 0x007d2fc5);
        rx_set_sp(A8 - 0x1c); rx_lights_add(s + 0x1a18, e); rx_set_sp(A8 - 0x10);
        SU32(A8, -0x14, 0x00971fa0); SU32(A8, -0x18, 0x007d2fca);
        rx_set_sp(A8 - 0x18); rx_scene_profile_scope(0x00971fa0); rx_set_sp(A8 - 0x10);
    }
    if (sc_vcall0(A8 - 0x14, e, 0x2c, 0x007d2fd8) != 0) {
        rx_addr list = s + 0x2bfc;
        if (!(I32(list, 4) < I32(list, 8))) {
            /* push capacity + growth; ESI = the list; call 007cff20 (return 007d2ffb; RET 4) */
            const uint32_t cap = U32(list, 0xc) + U32(list, 8);
            SU32(A8, -0x14, cap); SU32(A8, -0x18, 0x007d2ffb);
            sc_regs(s, fp, list, e);
            rx_set_sp(A8 - 0x18); rx_iarr_reserve(list, cap); rx_set_sp(A8 - 0x10);
            sc_regs(s, fp, s + 4, e);
        }
        SU32(U32(list, 0), 4u * U32(list, 4), e);
        SU32(list, 4, U32(list, 4) + 1);
    }
    if (sc_vcall0(A8 - 0x14, e, 0x58, 0x007d3013) == 0xe) SU32(s, 0x2cc0, e);
    (void)sc_vcall0(A8 - 0x14, e, 4, 0x007d3027);
    sc_cs(A8 - 0x18, 0, s + 4, 0x007d302e);
    sc_regs(ebx0, ebp0, esi0, edi0);
    rx_set_sp(E);
}
RX_FUNCTION(0x007d2f70, rx_scene_manager_add_entity)

/* 007d3040 SceneManager_RemoveEntity (ESI = entity, stack scene; RET 4): its vf2 (removed) under the lock, unlinked, its name
 * (+0x18) dropped from the name hash, its light removed (vf25), dropped from the tick list (vf11; swap with the last). */
void rx_scene_manager_remove_entity(rx_addr e, rx_addr s) {
    const rx_addr sp0 = rx_sp();
    const rx_addr A8 = (sp0 - 4u) & ~7u;
    const rx_addr key = A8 - 4;
    rx_set_sp(A8 - 0x10);
    anim_enter_cs(s + 4);
    anim_vcall0(e, 8);
    anim_leave_cs(s + 4);
    ANIM_CALL_FRAME(8); rx_scene_manager_unlink(s, e); ANIM_CALL_FRAME_END(8);
    if (U32(e, 0x1c) != 0) {
        anim_enter_cs(s + 4);
        SU32(key, 0, U32(e, 0x18));
        name_hash_erase(s + 0x19f8, key);
        anim_leave_cs(s + 4);
    }
    anim_enter_cs(s + 4);
    if (anim_vcall0(e, 0x64) != 0) {
        profile_scope(0x00971fa0);
        lights_remove(s + 0x1a18, e);
        profile_scope(0x00971fa0);
    }
    if (anim_vcall0(e, 0x2c) != 0) {
        int32_t n = I32(s, 0x2c00), i;
        for (i = 0; i < n; i++) if (U32(U32(s, 0x2bfc), 4u * (uint32_t)i) == e) break;
        if (i >= n) i = -1;
        SU32(U32(s, 0x2bfc), 4u * (uint32_t)i, U32(U32(s, 0x2bfc), 4u * U32(s, 0x2c00) - 4));
        SU32(s, 0x2c00, U32(s, 0x2c00) - 1);
    }
    anim_leave_cs(s + 4);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007d3040, rx_scene_manager_remove_entity)

/* ---- the event grid ---- */

/* 007d7230 EventGrid_CellsInRadius (ECX = grid, EAX = position, xmm0 = radius, stack = cell index array; RET 4): the cell of the
 * position and those of its 8 neighbours that the radius (ceil) reaches; a position outside the grid or a radius beyond two
 * cells gives the single index nx * nz (the "everywhere" cell). Returns the count. */
uint32_t rx_event_grid_cells_in_radius(rx_addr g, rx_addr pos, float radius, rx_addr out) {
    double c = crt_ceil((double)radius);                 /* MSVCR90 ceil, then _ftol2_sse (truncation, low 32 bits) */
    int32_t r = (c != c || c >= 9223372036854775808.0 || c < -9223372036854775808.0) ? 0 : (int32_t)(int64_t)c;
    int32_t csx = I32(g, 0x18);
    if (r > 2 * csx) { SU32(out, 0, (uint32_t)(I32(g, 0xf90) * I32(g, 0xf8c))); return 1; }
    int32_t x = rx_cvtt(F32(pos, 0) - (float)I32(g, 0x10)), gx = x / csx;
    int32_t z = rx_cvtt(F32(pos, 8) - (float)I32(g, 0x14)), csz = I32(g, 0x1c), gz = z / csz;
    int32_t nx = I32(g, 0xf8c), nz = I32(g, 0xf90);
    if (gx >= nx || gz >= nz || gx < 0 || gz < 0) { SU32(out, 0, (uint32_t)(nz * nx)); return 1; }
    uint32_t k = 0;
#define PUT(v) SU32(out, 4u * k++, (uint32_t)(v))
    int32_t base = nz * gx, cell = base + gz;
    PUT(cell);
    if (gz != 0 && z - r < csz * gz) PUT(cell - 1);
    if (gz < nz - 1 && z + r > (gz + 1) * csz) PUT(cell + 1);
    if (gx != 0 && x - r < csx * gx) {
        int32_t c2 = base - nz + gz;
        PUT(c2);
        if (gz != 0 && z - r < csz * gz) PUT(c2 - 1);
        if (gz < nz - 1 && z + r > (gz + 1) * csz) PUT(c2 + 1);
    }
    if (gx < nx - 1 && x + r > (gx + 1) * csx) {
        int32_t c3 = (gx + 1) * nz + gz;
        PUT(c3);
        if (gz != 0 && z - r < csz * gz) PUT(c3 - 1);
        if (gz < nz - 1 && z + r > (gz + 1) * csz) PUT(c3 + 1);
    }
#undef PUT
    return k;
}
RX_FUNCTION(0x007d7230, rx_event_grid_cells_in_radius)

static uint32_t cells_in_radius(rx_addr g, rx_addr pos, float radius, rx_addr out) {
    ANIM_CALL_FRAME(4); uint32_t k = rx_event_grid_cells_in_radius(g, pos, radius, out); ANIM_CALL_FRAME_END(4);
    return k;
}

/* A cell list's 32-byte event copy (id, x87 radius / value, word +0xc, position vec4). */
static void event_copy(rx_addr dst, rx_addr src) {
    SU32(dst, 0, U32(src, 0));
    anim_x87_copy_f32(dst + 4, src + 4);
    anim_x87_copy_f32(dst + 8, src + 8);
    SU32(dst, 0xc, U32(src, 0xc));
    rx_st4(dst + 0x10, rx_ld4(src + 0x10));
}

/* Removes the events of id *key from a cell list (count +cnt, tail +tail, storage +data): each match takes the oldest event's
 * place. Returns 1 when something was removed. */
static uint32_t cell_list_remove(rx_addr cell, uint32_t cnt, uint32_t tail, uint32_t data, rx_addr key) {
    int32_t n0 = I32(cell, cnt);
    for (int32_t i = 0; i < n0; i++) {
        rx_addr ev = U32(cell, data) + 32u * (uint32_t)(I32(cell, tail) - i);
        if (U32(key, 0) != U32(ev, 0)) continue;
        event_copy(ev, U32(cell, data) + 32u * (uint32_t)(I32(cell, tail) - I32(cell, cnt) + 1));
        SU32(cell, cnt, U32(cell, cnt) - 1);
    }
    return n0 != I32(cell, cnt);
}

/* 007da810 EventCell_RemoveB / 007dad20 EventCell_RemoveA (stdcall: cell, event (id first); RET 8). */
uint32_t rx_event_cell_remove_b(rx_addr cell, rx_addr ev) { return cell_list_remove(cell, 0x38, 0x40, 0x58, ev); }
RX_FUNCTION(0x007da810, rx_event_cell_remove_b)
uint32_t rx_event_cell_remove_a(rx_addr cell, rx_addr ev) { return cell_list_remove(cell, 0x2c, 0x34, 0x64, ev); }
RX_FUNCTION(0x007dad20, rx_event_cell_remove_a)

/* 007d5e70 / 007d60f0 (not in the list): grow the cell list's window up / down (ECX = grower); nonzero when it did. */
static uint32_t cell_grower(rx_addr fn, rx_addr grower) {
    uint32_t (*f)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));   /* anim_event_cells.c */
    if (f) { ANIM_CALL_FRAME(0); const uint32_t r = f(grower); ANIM_CALL_FRAME_END(0); return r; }
    rx_regs r = anim_regs(), o; r.r[ANIM_ECX] = grower; rx_call_original_ex(fn, &r, &o, 0, 0); return o.r[ANIM_EAX];
}

/* 007d6320 EventCell_PushA (ESI = cell, EDI = event): stores the event in list A: in the free part of the window (tail - count)
 * or, when full, one past the tail (grown up) / before the head (grown down); counts it in any case. */
/* The four lists of a cell (count / tail / head / storage offsets, the growers up / down): 0x38 (007d61a0), 0x44 (007d6220),
 * 0x20 (007d62a0), 0x2c = A (007d6320); the pushes are the same code on other fields. */
static void cell_push(rx_addr cell, rx_addr ev, uint32_t cnt, uint32_t tail, uint32_t head, uint32_t data, rx_addr up, rx_addr down) {
    int32_t idx;
    int store = 1;
    if ((uint32_t)(I32(cell, tail) - I32(cell, head) + 1) > U32(cell, cnt)) idx = I32(cell, tail) - I32(cell, cnt);
    else if (U32(cell, 0x54) && cell_grower(up, U32(cell, 0x54))) { SU32(cell, tail, U32(cell, tail) + 1); idx = I32(cell, tail); }
    else if (U32(cell, 0x50) && cell_grower(down, U32(cell, 0x50))) { SU32(cell, head, U32(cell, head) - 1); idx = I32(cell, head); }
    else { store = 0; idx = 0; }
    if (store) event_copy(U32(cell, data) + 32u * (uint32_t)idx, ev);
    SU32(cell, cnt, U32(cell, cnt) + 1);
}
void rx_event_cell_push_l38(rx_addr cell, rx_addr ev) { cell_push(cell, ev, 0x38, 0x40, 0x3c, 0x58, 0x007d5d20, 0x007d5ee0); }
RX_FUNCTION(0x007d61a0, rx_event_cell_push_l38)
void rx_event_cell_push_l44(rx_addr cell, rx_addr ev) { cell_push(cell, ev, 0x44, 0x4c, 0x48, 0x5c, 0x007d5d90, 0x007d5f90); }
RX_FUNCTION(0x007d6220, rx_event_cell_push_l44)
void rx_event_cell_push_l20(rx_addr cell, rx_addr ev) { cell_push(cell, ev, 0x20, 0x28, 0x24, 0x60, 0x007d5e00, 0x007d6040); }
RX_FUNCTION(0x007d62a0, rx_event_cell_push_l20)
void rx_event_cell_push_a(rx_addr cell, rx_addr ev) { cell_push(cell, ev, 0x2c, 0x34, 0x30, 0x64, 0x007d5e70, 0x007d60f0); }
RX_FUNCTION(0x007d6320, rx_event_cell_push_a)

/* Removes the events of `id` from every cell the radius reaches (list B: 007da810, list A: 007dad20). (EDI = grid; stack id,
 * radius, position; RET 0xc.) The 32-byte event (id, radius, 0, ?, position) is a local of the original's frame. */
static uint32_t grid_remove(rx_addr g, uint32_t id, float radius, rx_addr pos, int list_b) {
    ANIM_FRAME_ENTER(A, 0x70);
    const rx_addr cells = A - 0x54, ev = A - 0x30;
    anim_enter_cs(g + 0x20);
    uint32_t k = cells_in_radius(g, pos, radius, cells), removed = 0;
    for (int32_t i = (int32_t)k - 1; i >= 0; i--) {
        rx_addr cell = U32(g, 0xf78) + 0x70u * U32(cells, 4u * (uint32_t)i);
        SU32(ev, 0, id); SF32(ev, 4, radius); rx_st4(ev + 0x10, rx_ld4(pos)); SF32(ev, 8, 0.0f);
        ANIM_CALL_FRAME(8);
        removed |= list_b ? rx_event_cell_remove_b(cell, ev) : rx_event_cell_remove_a(cell, ev);
        ANIM_CALL_FRAME_END(8);
    }
    anim_leave_cs(g + 0x20);
    ANIM_FRAME_LEAVE(A);
    return removed;
}
/* 007da1d0 EventGrid_RemoveB / 007da410 EventGrid_RemoveA. */
uint32_t rx_event_grid_remove_b(rx_addr g, uint32_t id, float radius, rx_addr pos) { return grid_remove(g, id, radius, pos, 1); }
RX_FUNCTION(0x007da1d0, rx_event_grid_remove_b)
uint32_t rx_event_grid_remove_a(rx_addr g, uint32_t id, float radius, rx_addr pos) { return grid_remove(g, id, radius, pos, 0); }
RX_FUNCTION(0x007da410, rx_event_grid_remove_a)

/* 007da740 EventGrid_AddA (ECX = grid, EDX = position; stack id, radius, value; RET 0xc): the event (id, radius, value, ?,
 * position) pushed to list A of every cell the radius reaches. Word +0xc of the local event is never written: it carries the
 * original's dead stack into the cells. */
static void grid_add(rx_addr g, rx_addr pos, uint32_t id, float radius, float value, void (*push)(rx_addr, rx_addr)) {
    ANIM_FRAME_ENTER(A, 0x80);
    const rx_addr cells = A - 0x54, ev = A - 0x30;
    anim_enter_cs(g + 0x20);
    uint32_t k = cells_in_radius(g, pos, radius, cells);
    for (uint32_t i = 0; i < k; i++) {
        SF32(ev, 4, radius);
        rx_v4 p = rx_ld4(pos);
        rx_addr cell = U32(g, 0xf78) + 0x70u * U32(cells, 4 * i);
        rx_st4(ev + 0x10, p);
        SU32(ev, 0, id);
        SF32(ev, 8, value);
        ANIM_CALL_FRAME(0); push(cell, ev); ANIM_CALL_FRAME_END(0);
    }
    anim_leave_cs(g + 0x20);
    ANIM_FRAME_LEAVE(A);
}
/* 007da4d0 / 007da5a0 / 007da670 / 007da740 (A): the same code over the four lists. */
void rx_event_grid_add_l38(rx_addr g, rx_addr pos, uint32_t id, float radius, float value) { grid_add(g, pos, id, radius, value, rx_event_cell_push_l38); }
RX_FUNCTION(0x007da4d0, rx_event_grid_add_l38)
void rx_event_grid_add_l44(rx_addr g, rx_addr pos, uint32_t id, float radius, float value) { grid_add(g, pos, id, radius, value, rx_event_cell_push_l44); }
RX_FUNCTION(0x007da5a0, rx_event_grid_add_l44)
void rx_event_grid_add_l20(rx_addr g, rx_addr pos, uint32_t id, float radius, float value) { grid_add(g, pos, id, radius, value, rx_event_cell_push_l20); }
RX_FUNCTION(0x007da670, rx_event_grid_add_l20)
void rx_event_grid_add_a(rx_addr g, rx_addr pos, uint32_t id, float radius, float value) { grid_add(g, pos, id, radius, value, rx_event_cell_push_a); }
RX_FUNCTION(0x007da740, rx_event_grid_add_a)

/* ---- lights ---- */
/* 007f9e70: EAX tree, ESI item; Lights_Add's EBX = the lights, EBP its frame, EDI the entity (their saves are dead stack the next
 * event record copies): called at the original's ESP with its return address */
static uint32_t light_octree_insert(rx_addr tree, rx_addr item, rx_addr lights, uint32_t ebp, rx_addr entity) {
    const rx_addr e = rx_sp() - 4;
    SU32(e, 0, 0x007e00f7u);
    return scene_light_list_add_fr(e, lights, ebp, entity, tree, item);
}
/* 007da670 / 007da5a0 / 007da4d0 (not in the list): event grid adds of the lights (ECX = grid, EDX = position; stack 3; RET 0xc). */
static void light_grid_add(rx_addr fn, rx_addr grid, rx_addr pos, uint32_t a0, float a1, float a2) {
    void (*f)(rx_addr, rx_addr, uint32_t, float, float) = RX_FN(fn, void (*)(rx_addr, rx_addr, uint32_t, float, float));
    if (f) { ANIM_CALL_FRAME(12); f(grid, pos, a0, a1, a2); ANIM_CALL_FRAME_END(12); return; }
    rx_regs r = anim_regs(); r.r[ANIM_ECX] = grid; r.r[ANIM_EDX] = pos; uint32_t a[3] = {a0, rx_fb(a1), rx_fb(a2)};
    rx_call_original_ex(fn, &r, 0, 3, a);
}
/* The light's world position: M (vf14 of the entity) applied to the light's local position. */
static void light_world(rx_addr m, float vx, float vy, float vz, int order_zyx, float* out) {
    if (!order_zyx) {
        out[0] = ((F32(m, 0x10) * vy + F32(m, 0x20) * vz) + F32(m, 0) * vx) + F32(m, 0x30);
        out[1] = ((F32(m, 0x14) * vy + F32(m, 4) * vx) + F32(m, 0x24) * vz) + F32(m, 0x34);
        out[2] = ((F32(m, 0x18) * vy + F32(m, 8) * vx) + F32(m, 0x28) * vz) + F32(m, 0x38);
    } else {
        out[0] = ((F32(m, 0x20) * vz + F32(m, 0x10) * vy) + F32(m, 0) * vx) + F32(m, 0x30);
        out[1] = ((F32(m, 0x24) * vz + F32(m, 0x14) * vy) + F32(m, 4) * vx) + F32(m, 0x34);
        out[2] = ((F32(m, 0x28) * vz + F32(m, 0x18) * vy) + F32(m, 8) * vx) + F32(m, 0x38);
    }
}

/* 007dfdd0 Lights_Add (stdcall: lights L, entity; RET 8): the entity's light (vf25 -> light: vf1 type (asked again for each test), vf6 shape (position,
 * direction, range), vf7 intensity): type 0 (point) into the light list +0x10fc and the event grid +0x10f0 (by the entity's
 * vf23 id when it has one), types 3 / 2 into the lists +0x1014 / +0x1000, type 1 (spot) into the octree +0x10b4 and the grid;
 * then every light into +0x103c and the light's vf14. Under L's two locks (+0x1114, +0x112c). */
void rx_lights_add(rx_addr L, rx_addr e) {
    ANIM_FRAME_ENTER(A, 0x60);
    const rx_addr body = A - 0x60, e_slot = A_sp0 + 8;
    /* push ebx, esi, edi (the caller's); then EBX = L, ESI = EnterCriticalSection, EDI = the entity, as its callees save them */
    const uint32_t ebx0 = anim_reg_get(ANIM_EBX), esi0 = anim_reg_get(ANIM_ESI), edi0 = anim_reg_get(ANIM_EDI);
    SU32(A - 0x58, 0, ebx0); SU32(A - 0x5c, 0, esi0); SU32(A - 0x60, 0, edi0);
    anim_reg_set(ANIM_EBX, L); anim_reg_set(ANIM_ESI, U32(0, 0x0091600c)); anim_reg_set(ANIM_EDI, e);
    SU32(body, 0x1c, L + 0x1114); SU32(body, 0x18, L + 0x112c);   /* the two locks, kept for the leave */
    anim_enter_cs(L + 0x1114); anim_ret_slot(4, 0x007dfdf5);
    anim_enter_cs(L + 0x112c); anim_ret_slot(4, 0x007dfe02);
    uint32_t type = anim_vcall0(anim_vcall0(e, 0x64), 4);
    if (type == 0) {
        rx_addr d = anim_vcall0(e, 0x64);
        anim_vcall0_f(d, 0x1c);
        uint32_t a[3] = {body + 0x30, body + 0x50, body + 0x14};
        rx_vcall(d, 0x18, 3, a);
        rx_addr m = anim_vcall0(e, 0x38);
        float w[3];
        light_world(m, F32(body, 0x30), F32(body, 0x34), F32(body, 0x38), 0, w);
        SU32(U32(L, 0x10fc), 8u * U32(L, 0x1108), e);
        SU32(L, 0x1108, U32(L, 0x1108) + 1);
        SF32(body, 0x20, w[0]); SF32(body, 0x24, w[1]); SF32(body, 0x28, w[2]);
        if (anim_vcall0(e, 0x5c) != 0) {
            float range = anim_vcall0_f(anim_vcall0(e, 0x64), 0x1c);
            uint32_t id = anim_vcall0(e, 0x5c);
            SU32(body - 4, 0, rx_fb(range)); anim_x87_copy_f32(body - 8, body + 0x14);
            light_grid_add(0x007da740, U32(L, 0x10f0), body + 0x20, id, F32(body - 8, 0), range);
        } else {
            float range = anim_vcall0_f(anim_vcall0(e, 0x64), 0x1c);
            SU32(body - 4, 0, rx_fb(range)); anim_x87_copy_f32(body - 8, body + 0x14);
            light_grid_add(0x007da670, U32(L, 0x10f0), body + 0x20, e, F32(body - 8, 0), range);
        }
    } else if (anim_vcall0(anim_vcall0(e, 0x64), 4) == 3) {
        list_push_unique_at(L + 0x1014, e_slot, 0x007dff9bu, L, A_sp0 - 4, e);
    } else if (anim_vcall0(anim_vcall0(e, 0x64), 4) == 2) {
        list_push_unique_at(L + 0x1000, e_slot, 0x007dffc6u, L, A_sp0 - 4, e);
    } else if (anim_vcall0(anim_vcall0(e, 0x64), 4) == 1) {
        rx_addr d = anim_vcall0(e, 0x64);
        SF32(body, 0x10, anim_vcall0_f(d, 0x1c));
        uint32_t a[3] = {body + 0x20, body + 0x50, body + 0x14};
        rx_vcall(d, 0x18, 3, a);
        rx_addr m = anim_vcall0(e, 0x38);
        float w[3];
        light_world(m, F32(body, 0x20), F32(body, 0x24), F32(body, 0x28), 1, w);
        SF32(body, 0x20, w[0]); SF32(body, 0x30, w[0]);
        SF32(body, 0x24, w[1]); SF32(body, 0x28, w[2]);
        SU32(body, 0x40, e);
        SF32(body, 0x44, F32(body, 0x14));
        SF32(body, 0x34, w[1]); SF32(body, 0x38, w[2]);
        SF32(body, 0x48, F32(body, 0x10));
        if (light_octree_insert(L + 0x10b4, body + 0x30, L, A_sp0 - 4, e)) {
            uint32_t has_id = anim_vcall0(e, 0x5c);
            anim_x87_copy_f32(body - 4, body + 0x10); anim_x87_copy_f32(body - 8, body + 0x14);
            const float a1 = F32(body - 8, 0), a2 = F32(body - 4, 0);
            if (has_id) {
                /* the id is asked again with the two float arguments already pushed (sub esp, 8): below them */
                rx_set_sp(body - 8);
                uint32_t id = anim_vcall0(e, 0x5c);
                rx_set_sp(body);
                light_grid_add(0x007da5a0, U32(L, 0x10f0), body + 0x20, id, a1, a2);
            } else light_grid_add(0x007da4d0, U32(L, 0x10f0), body + 0x20, e, a1, a2);
        }
    }
    list_push_unique_at(L + 0x103c, e_slot, 0x007e0154u, L, A_sp0 - 4, e);
    rx_addr light = anim_vcall0(e, 0x64); anim_ret_slot(0, 0x007e015d);
    anim_vcall0(light, 0x38); anim_ret_slot(0, 0x007e0166);
    anim_leave_cs(L + 0x112c); anim_ret_slot(4, 0x007e0173);
    anim_leave_cs(L + 0x1114); anim_ret_slot(4, 0x007e017a);
    anim_reg_set(ANIM_EBX, ebx0); anim_reg_set(ANIM_ESI, esi0); anim_reg_set(ANIM_EDI, edi0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x007dfdd0, rx_lights_add)

/* ---- teardown ---- */
/* 007d4030 ModelInstance_Destroy (ESI = instance, ECX = passed on): base vtable 0x972844, frees its +0x130 .. +0x134 element range
 * (00545120) and +0x124, releases the node sets +0x50 / +0x10 (007dd4d0, EDI = the set), then the base vtable 0x93eb78.
 * Frame: push ecx; push edi; EDI = 0; the calls' arguments and return addresses (007d4055 / 007d4061 / 007d4082 / 007d408d /
 * 007d4095) and the callees' frames as the original leaves them (the package unload's model destruction runs on the database
 * thread, whose dead stack stays); entered at the return slot, EBX / EBP from the register model. */
void rx_model_instance_destroy(rx_addr o, uint32_t ecx_in) {
    const rx_addr E = rx_sp();
    const uint32_t ebx = anim_reg_get(ANIM_EBX), ebp = anim_reg_get(ANIM_EBP), esi0 = anim_reg_get(ANIM_ESI), edi0 = anim_reg_get(ANIM_EDI);
    SU32(E, -4, ecx_in); SU32(E, -8, edi0);
    anim_reg_set(ANIM_ESI, o); anim_reg_set(ANIM_EDI, 0);
    const crt_regs4 r = {ebx, ebp, o, 0};
    SU32(o, 0, 0x00972844);
    if (U32(o, 0x130) != 0) {
        /* 00545120 (EAX = the first 0x1c-byte element; cdecl end, the pushed ECX): frees their +0x10 and +0x04 */
        SU32(E, -0xc, ecx_in); SU32(E, -0x10, U32(o, 0x134)); SU32(E, -0x14, 0x007d4055);
        rx_set_sp(E - 0x14);
        rx_records_free_range(U32(o, 0x130), U32(o, 0x134));
        SU32(E, -0x14, U32(o, 0x130)); SU32(E, -0x18, 0x007d4061);
        rx_reflex_aligned_free_at(E - 0x18, r, U32(o, 0x130));
    }
    SU32(o, 0x130, 0); SU32(o, 0x134, 0); SU32(o, 0x138, 0);
    SU32(E, -0xc, U32(o, 0x124)); SU32(E, -0x10, 0x007d4082);
    rx_reflex_aligned_free_at(E - 0x10, r, U32(o, 0x124));
    for (int k = 0; k < 2; k++) {
        const rx_addr set = o + (k ? 0x10u : 0x50u);
        anim_reg_set(ANIM_EDI, set);
        SU32(E, -0xc, k ? 0x007d4095u : 0x007d408du);
        rx_set_sp(E - 0xc);
        rx_nodeset_release(set);
    }
    SU32(o, 0, 0x0093eb78);
    anim_reg_set(ANIM_ESI, esi0); anim_reg_set(ANIM_EDI, edi0);
    rx_set_sp(E);
}
RX_FUNCTION(0x007d4030, rx_model_instance_destroy)
