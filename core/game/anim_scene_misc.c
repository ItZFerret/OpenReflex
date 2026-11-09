/* anim_scene_misc.c -- scene light / event registry removal, found by the sweep census (miscport):
 * 007e0190 (SceneLights removal of an entity: the lists the scene keeps per entity class, under two critical sections, and the
 * event-grid events the entity registered) with its helpers 007da9c0 / 007dab70 (event cell removal in lists C / D),
 * 007da290 / 007da350 (grid removal), 007f9db0 (pointer array find), 007f9fd0 (0x40-byte record removal).
 * Conventions as anim_scene.c (frames on the original's addresses, anim_vcall for virtual calls, x87 float copies). */
#include "anim.h"
#include "anim_extern.h"
#include "anim_scene_misc.h"
#include "race_misc.h"

/* A cell list's 32-byte event copy (id, x87 radius / value, word +0xc, position vec4): as anim_scene.c's. */
static void event_copy(rx_addr dst, rx_addr src) {
    SU32(dst, 0, U32(src, 0));
    anim_x87_copy_f32(dst + 4, src + 4);
    anim_x87_copy_f32(dst + 8, src + 8);
    SU32(dst, 0xc, U32(src, 0xc));
    rx_st4(dst + 0x10, rx_ld4(src + 0x10));
}

/* The removal loop of 007da810 / 007dad20 / 007da9c0 / 007dab70 (the original unrolls it four times: the same visit order): the
 * events of id *key leave the list (count +cnt, tail +tail, storage +data), each match taking the oldest event's place. */
static uint32_t cell_list_remove(rx_addr cell, uint32_t cnt, uint32_t tail, uint32_t data, rx_addr key) {
    const int32_t n0 = I32(cell, cnt);
    for (int32_t i = 0; i < n0; i++) {
        const rx_addr ev = U32(cell, data) + 32u * (uint32_t)(I32(cell, tail) - i);
        if (U32(key, 0) != U32(ev, 0)) continue;
        event_copy(ev, U32(cell, data) + 32u * (uint32_t)(I32(cell, tail) - I32(cell, cnt) + 1));
        SU32(cell, cnt, U32(cell, cnt) - 1);
    }
    return n0 != I32(cell, cnt);
}

/* 007da9c0 EventCell_RemoveC / 007dab70 EventCell_RemoveD (stdcall: cell, event (id first); RET 8): lists 0x44 / 0x20. */
uint32_t rx_event_cell_remove_c(rx_addr cell, rx_addr ev) { return cell_list_remove(cell, 0x44, 0x4c, 0x5c, ev); }
RX_FUNCTION(0x007da9c0, rx_event_cell_remove_c)
uint32_t rx_event_cell_remove_d(rx_addr cell, rx_addr ev) { return cell_list_remove(cell, 0x20, 0x28, 0x60, ev); }
RX_FUNCTION(0x007dab70, rx_event_cell_remove_d)

static uint32_t cells_in_radius(rx_addr g, rx_addr pos, float radius, rx_addr out) {
    ANIM_CALL_FRAME(4); uint32_t k = rx_event_grid_cells_in_radius(g, pos, radius, out); ANIM_CALL_FRAME_END(4);
    return k;
}

/* 007da290 / 007da350 (EDI = grid; stack id, radius, position; RET 0xc): as 007da1d0 / 007da410 (anim_scene.c grid_remove) on the
 * lists C / D: under the grid's critical section (+0x20) the event (id, radius, 0, ?, position) is removed from every cell the
 * radius reaches. The 32-byte event is a local of the original's frame. */
static uint32_t grid_remove(rx_addr g, uint32_t id, float radius, rx_addr pos, int list_c) {
    ANIM_FRAME_ENTER(A, 0x70);
    const rx_addr cells = A - 0x54, ev = A - 0x30;
    anim_enter_cs(g + 0x20);
    const uint32_t k = cells_in_radius(g, pos, radius, cells);
    uint32_t removed = 0;
    for (int32_t i = (int32_t)k - 1; i >= 0; i--) {
        const rx_addr cell = U32(g, 0xf78) + 0x70u * U32(cells, 4u * (uint32_t)i);
        SU32(ev, 0, id); SF32(ev, 4, radius); rx_st4(ev + 0x10, rx_ld4(pos)); SF32(ev, 8, 0.0f);
        ANIM_CALL_FRAME(8);
        removed |= list_c ? rx_event_cell_remove_c(cell, ev) : rx_event_cell_remove_d(cell, ev);
        ANIM_CALL_FRAME_END(8);
    }
    anim_leave_cs(g + 0x20);
    ANIM_FRAME_LEAVE(A);
    return removed;
}
uint32_t rx_event_grid_remove_c(rx_addr g, uint32_t id, float radius, rx_addr pos) { return grid_remove(g, id, radius, pos, 1); }
RX_FUNCTION(0x007da290, rx_event_grid_remove_c)
uint32_t rx_event_grid_remove_d(rx_addr g, uint32_t id, float radius, rx_addr pos) { return grid_remove(g, id, radius, pos, 0); }
RX_FUNCTION(0x007da350, rx_event_grid_remove_d)

/* 007f9db0 (ECX = array {data, count}; stack &key; RET 4) -> EAX: the index of the first pointer equal to *key, or -1. */
uint32_t rx_array_ptr_find(rx_addr arr, rx_addr keyp) {
    const int32_t n = I32(arr, 4);
    if (n <= 0) return 0xffffffffu;
    const uint32_t key = U32(keyp, 0);
    rx_addr p = U32(arr, 0);
    for (int32_t i = 0; i < n; i++, p += 4) if (U32(p, 0) == key) return (uint32_t)i;
    return 0xffffffffu;
}
RX_FUNCTION(0x007f9db0, rx_array_ptr_find)

/* 007f9fd0 (ECX = index, EDX = array {data, count}): the last 0x40-byte record moves into the slot (floats through the x87), the
 * count drops by one. */
void rx_array40_remove(uint32_t index, rx_addr arr) {
    const rx_addr data = U32(arr, 0);
    const rx_addr last = data + (U32(arr, 4) << 6) - 0x40, dst = data + (index << 6);
    anim_x87_copy_f32(dst, last); anim_x87_copy_f32(dst + 4, last + 4); anim_x87_copy_f32(dst + 8, last + 8);
    SU32(dst, 0x10, U32(last, 0x10));
    anim_x87_copy_f32(dst + 0x14, last + 0x14); anim_x87_copy_f32(dst + 0x18, last + 0x18);
    SU32(dst, 0x20, U32(last, 0x20)); SU32(dst, 0x24, U32(last, 0x24)); SU32(dst, 0x28, U32(last, 0x28));
    SU32(dst, 0x2c, U32(last, 0x2c)); SU32(dst, 0x30, U32(last, 0x30));
    SU32(arr, 4, U32(arr, 4) - 1);
}
RX_FUNCTION(0x007f9fd0, rx_array40_remove)

/* ---------------------------------------------------------------------------------------------------------------------- */
/* 007e0190 (EAX = the scene's light registry (scene + 0x1a18), stack entity; RET 4; the callers read no register): the entity
 * leaves the registry. Under two critical sections (+0x1114, +0x112c; left in the reverse order) the entity's volume (virtual
 * 0x64) decides by its type (vf1):
 *   0: the entity's record in the 8-byte-item list (+0x10fc, count +0x1108) is replaced by the last one (the octree +0x1100
 *      redirects the moved item: 007df2e0); the volume's centre / extent / radius (its vf6) are brought to world space by the
 *      entity's matrix (vf14) and the event (id vf23, else the entity) leaves the grid +0x10f0 (007da410 with an id, else 007da350);
 *   3: swap-remove from the pointer list +0x1014 (count +0x1018), 2: from +0x1000 (count +0x1004, 007f9db0);
 *   1: the event records of the entity: 0x20-byte list +0x10b4 (007f9e40: 007da290 with an id, else 007da1d0, then 007f9ee0) and
 *      0x40-byte list +0x10dc (007f9f80 on the id (vf23 != 0) or the entity: 007da290 / 007da1d0, then 007f9fd0).
 * Then the entity leaves the pointer list +0x103c (count +0x1040; an absent entity writes the slot before the array, as the
 * original) and, with the list empty, +0x10b8 / +0x10e0 are cleared.
 * Frame: push ebp; and esp, -16; sub esp, 0x64; push ebx / esi / edi: S = A - 0x70; locals above S: +0x14 the volume's third
 * output (the radius) / the found index / the saved array, +0x18 / +0x1c the two critical sections, +0x20 the volume's world
 * point (vec3), +0x30 the volume's second output and, for the event record search, the 0x20 / 0x40-byte key (+0x10 = the entity,
 * +0x14 = 15000.0f; the 0x40 key's +0x30 = the id). */
static rx_addr volume_of(rx_addr e) { return anim_vcall0(e, 0x64); }
static uint32_t volume_type(rx_addr v) { return anim_vcall0(v, 4); }
static uint32_t entity_event_id(rx_addr e) { return anim_vcall0(e, 0x5c); }
/* the float the original passes through fld / fstp (a signalling NaN comes back quiet) */
static float x87_f32(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) b |= 0x00400000u;
    float f; memcpy(&f, &b, 4); return f;
}
/* the volume's vf6 (thiscall, three output pointers; RET 0xc) */
static void volume_bounds(rx_addr v, rx_addr o0, rx_addr o1, rx_addr o2) {
    const rx_addr target = U32(U32(v, 0), 0x18);
    void (*f)(rx_addr, rx_addr, rx_addr, rx_addr) = RX_FN(target, void (*)(rx_addr, rx_addr, rx_addr, rx_addr));
    if (f) { ANIM_CALL_FRAME(0xc); f(v, o0, o1, o2); ANIM_CALL_FRAME_END(0xc); return; }
    uint32_t a[3] = {o0, o1, o2};
    rx_call_original(target, RX_THISCALL, v, 3, a);
}
/* the grid removals (007da410 A / 007da350 D / 007da290 C / 007da1d0 B) with EDI = the grid, the arguments pushed, the registers
 * as the callers of the original set them */
static void grid_call(rx_addr fn, rx_addr grid, uint32_t id, float radius, rx_addr pos) {
    switch (fn) {
    case 0x007da410: { ANIM_CALL_FRAME(0xc); rx_event_grid_remove_a(grid, id, radius, pos); ANIM_CALL_FRAME_END(0xc); break; }
    case 0x007da350: { ANIM_CALL_FRAME(0xc); rx_event_grid_remove_d(grid, id, radius, pos); ANIM_CALL_FRAME_END(0xc); break; }
    case 0x007da290: { ANIM_CALL_FRAME(0xc); rx_event_grid_remove_c(grid, id, radius, pos); ANIM_CALL_FRAME_END(0xc); break; }
    default:         { ANIM_CALL_FRAME(0xc); rx_event_grid_remove_b(grid, id, radius, pos); ANIM_CALL_FRAME_END(0xc); break; }
    }
}

void rx_lights_remove_entity(rx_addr lights, rx_addr e) {
    const rx_addr sp0 = rx_sp();
    const uint32_t ebx0 = anim_reg_get(ANIM_EBX), esi0 = anim_reg_get(ANIM_ESI), edi0 = anim_reg_get(ANIM_EDI);
    ANIM_FRAME_ENTER(A, 0x70);
    const rx_addr S = A - 0x70;
    SU32(S, 0xc, ebx0); SU32(S, 8, esi0); SU32(S, 4, edi0);   /* push ebx / esi / edi (below the sub 0x64 area) */
    SU32(sp0, 4, e);                                         /* the argument slot: 007f9db0 reads the entity through [ebp + 8] */
    anim_reg_set(ANIM_EBX, e); anim_reg_set(ANIM_ESI, lights);
    const rx_addr cs1 = lights + 0x1114, cs2 = lights + 0x112c;
    SU32(S, 0x1c, cs1);
    anim_enter_cs(cs1);
    SU32(S, 0x18, cs2);
    anim_enter_cs(cs2);
    rx_addr vol = volume_of(e);
    if (volume_type(vol) == 0) {
        const uint32_t n = U32(lights, 0x1108);
        uint32_t idx = 0;
        if (n != 0) {
            rx_addr p = U32(lights, 0x10fc);
            for (;;) {
                if (U32(p, 0) == e) break;
                idx++; p += 8;
                if (!(idx < n)) break;
            }
        }
        if (idx < n) {
            const uint32_t last = n - 1;
            const rx_addr items = U32(lights, 0x10fc);
            SU32(lights, 0x1108, last);
            ANIM_CALL_FRAME(8); rx_octree_replace_item(U32(lights, 0x1100), items + idx * 8, items + last * 8); ANIM_CALL_FRAME_END(8);
            const rx_addr items2 = U32(lights, 0x10fc);
            const uint32_t cnt2 = U32(lights, 0x1108);
            SU32(items2, idx * 8, U32(items2, cnt2 * 8));
            SU32(items2, idx * 8 + 4, U32(items2, cnt2 * 8 + 4));
            vol = volume_of(e);
            volume_bounds(vol, S + 0x20, S + 0x30, S + 0x14);
            const rx_addr m = anim_vcall0(e, 0x38);
            const float v0 = F32(S, 0x20), v1 = F32(S, 0x24), v2 = F32(S, 0x28);
            const float x = ((F32(m, 0x10) * v1 + F32(m, 0x20) * v2) + F32(m, 0) * v0) + F32(m, 0x30);
            const float y = ((F32(m, 0x14) * v1 + F32(m, 0x24) * v2) + F32(m, 4) * v0) + F32(m, 0x34);
            const float z = ((F32(m, 0x18) * v1 + F32(m, 0x28) * v2) + F32(m, 8) * v0) + F32(m, 0x38);
            SF32(S, 0x20, x); SF32(S, 0x24, y); SF32(S, 0x28, z);
            const uint32_t id = entity_event_id(e);
            const float radius = x87_f32(U32(S, 0x14));
            if (id != 0) {
                const uint32_t id2 = entity_event_id(e);
                grid_call(0x007da410, U32(lights, 0x10f0), id2, radius, S + 0x20);
            } else {
                grid_call(0x007da350, U32(lights, 0x10f0), e, radius, S + 0x20);
            }
        }
    } else {
        vol = volume_of(e);
        const uint32_t type = volume_type(vol);
        if (type == 3) {
            const int32_t n = I32(lights, 0x1018);
            int32_t i = 0;
            if (n > 0) {
                rx_addr p = U32(lights, 0x1014);
                for (; i < n; i++, p += 4) if (U32(p, 0) == e) break;
                if (i >= n) i = -1;
            } else i = -1;
            const rx_addr d = U32(lights, 0x1014);
            SU32(d, 4u * (uint32_t)i, U32(d, 4u * (uint32_t)n - 4u));
            SU32(lights, 0x1018, U32(lights, 0x1018) - 1);
        } else {
            vol = volume_of(e);
            if (volume_type(vol) == 2) {
                const rx_addr d = U32(lights, 0x1000);
                const uint32_t cnt = U32(lights, 0x1004);
                SU32(S, 0x14, d);
                uint32_t idx;
                ANIM_CALL_FRAME(4); idx = rx_array_ptr_find(lights + 0x1000, sp0 + 4); ANIM_CALL_FRAME_END(4);
                const rx_addr d2 = U32(S, 0x14);
                SU32(d2, idx * 4, U32(d2, cnt * 4 - 4));
                SU32(lights, 0x1004, U32(lights, 0x1004) - 1);
            } else {
                vol = volume_of(e);
                if (volume_type(vol) == 1) {
                    const rx_addr key = S + 0x30, arr20 = lights + 0x10b4, arr40 = lights + 0x10dc;
                    SF32(S, 0x44, 0.0f);
                    SU32(key, 0, U32(0, 0x00a95d50)); SU32(key, 4, U32(0, 0x00a95d54)); SU32(key, 8, U32(0, 0x00a95d58));
                    SU32(key, 0x10, e); SU32(key, 0x18, U32(0, 0x00977564));
                    int32_t idx;
                    ANIM_CALL_FRAME(4); idx = rx_array20_find(arr20, key); ANIM_CALL_FRAME_END(4);
                    SU32(S, 0x14, (uint32_t)idx);
                    if (idx >= 0) {
                        const rx_addr rec = U32(arr20, 0) + ((uint32_t)idx << 5);
                        if (entity_event_id(e) != 0) {
                            const float radius = x87_f32(U32(rec, 0x14));
                            grid_call(0x007da290, U32(lights, 0x10f0), entity_event_id(e), radius, rec);
                        } else {
                            const float radius = x87_f32(U32(rec, 0x14));
                            grid_call(0x007da1d0, U32(lights, 0x10f0), e, radius, rec);
                        }
                        rx_array20_remove((uint32_t)U32(S, 0x14), arr20);
                    }
                    if (entity_event_id(e) != 0) {
                        const uint32_t id = entity_event_id(e);
                        SF32(S, 0x44, 0.0f);
                        SU32(key, 0, U32(0, 0x00a95d50)); SU32(key, 4, U32(0, 0x00a95d54)); SU32(key, 8, U32(0, 0x00a95d58));
                        SU32(S, 0x60, id);
                        SU32(key, 0x10, 0); SU32(key, 0x18, U32(0, 0x00977564));
                        ANIM_CALL_FRAME(4); idx = rx_array40_find(arr40, key); ANIM_CALL_FRAME_END(4);
                        SU32(S, 0x14, (uint32_t)idx);
                        if (idx >= 0) {
                            const rx_addr rec = U32(arr40, 0) + ((uint32_t)idx << 6);
                            const float radius = x87_f32(U32(rec, 0x14));
                            grid_call(0x007da290, U32(lights, 0x10f0), entity_event_id(e), radius, rec);
                            rx_array40_remove((uint32_t)U32(S, 0x14), arr40);
                        }
                    } else {
                        SF32(S, 0x44, 0.0f);
                        SU32(key, 0, U32(0, 0x00a95d50)); SU32(key, 4, U32(0, 0x00a95d54)); SU32(key, 8, U32(0, 0x00a95d58));
                        SU32(key, 0x10, e); SU32(key, 0x18, U32(0, 0x00977564));
                        ANIM_CALL_FRAME(4); idx = rx_array40_find(arr40, key); ANIM_CALL_FRAME_END(4);
                        SU32(S, 0x14, (uint32_t)idx);
                        if (idx >= 0) {
                            const rx_addr rec = U32(arr40, 0) + ((uint32_t)idx << 6);
                            const float radius = x87_f32(U32(rec, 0x14));
                            grid_call(0x007da1d0, U32(lights, 0x10f0), e, radius, rec);
                            rx_array40_remove((uint32_t)U32(S, 0x14), arr40);
                        }
                    }
                }
            }
        }
    }
    {   /* 007e060b: the entity leaves the pointer list +0x103c */
        const int32_t n = I32(lights, 0x1040);
        int32_t i = 0;
        if (n > 0) {
            rx_addr p = U32(lights, 0x103c);
            for (; i < n; i++, p += 4) if (U32(p, 0) == e) break;
            if (i >= n) i = -1;
        } else i = -1;
        const rx_addr d = U32(lights, 0x103c);
        SU32(d, 4u * (uint32_t)i, U32(d, 4u * (uint32_t)n - 4u));
        SU32(lights, 0x1040, U32(lights, 0x1040) - 1);
        if (U32(lights, 0x1040) == 0) { SU32(lights, 0x10b8, 0); SU32(lights, 0x10e0, 0); }
    }
    anim_leave_cs(U32(S, 0x18));
    anim_leave_cs(U32(S, 0x1c));
    anim_reg_set(ANIM_EBX, ebx0); anim_reg_set(ANIM_ESI, esi0); anim_reg_set(ANIM_EDI, edi0);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x007e0190, rx_lights_remove_entity)
