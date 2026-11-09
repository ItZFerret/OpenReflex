/* world_rider_4d.c -- vehicle / rider shared helpers at 0x004dxxxx and 0x0054xxxx: key tracks (sorted keyframes, 16 bytes each
 * with the time first), the terrain-track contacts of the rider / bike bodies (track strips of the 0xa9577c set, deformation
 * stamps), the 16-byte and 0x50-byte vector helpers. core/notes/world.md (rider). */
#include "world_rider.h"
#include "world_util.h"
#include "world_rider_extern.h"
#include "phys_crt.h"
#include "../src/rx_x87.h"

/* 005452d0 Vec4_FromScalars (stdcall out; xmm3 = x, xmm2 = y, xmm1 = z, xmm0 = w; RET 4; EAX = out): out = (x, y, z, w). */
void rx_world_vec4_from_scalars(rx_addr out, float x, float y, float z, float w) {
    v4_st(out, v4(x, y, z, w));
}
RX_FUNCTION(0x005452d0, rx_world_vec4_from_scalars)

/* 004dbce0 KeyTrack_FindSegment (EAX = keys, EDX = count, xmm0 = time t; cdecl out prev key, out next key, out fraction; EAX =
 * 1 when t lies inside the track, 0 when it is extrapolated): the keys are 16 bytes with the time first. A binary search finds
 * the first key later than t; the fraction is (t - prev) / (next - prev). Before the first key (or with no keys) the segment is
 * (key 1, key 0) with the fraction (t - last) / (key 0 - key 1), after the last it is (key n-2, key n-1) with
 * (t - last) / (last - key n-2) -- the original's formulas, kept. */
uint32_t rx_world_keytrack_find_segment(rx_addr keys, uint32_t count, float t, rx_addr out_prev, rx_addr out_next,
                                        rx_addr out_frac) {
    const rx_addr last = keys + count * 16u - 16u;
    uint32_t idx = count;   /* index of the first key after t */
    if (count != 0 && !(t >= F32(last, 0))) {
        if (F32(keys, 0) > t) {
            idx = 0;
        } else {
            int32_t lo = 0, hi = (int32_t)count;
            for (;;) {
                const int32_t mid = (hi + lo) >> 1;
                const rx_addr k = keys + (uint32_t)mid * 16u;
                if (t >= F32(k, 0)) {
                    if (F32(k, 0x10) > t) { idx = (uint32_t)mid + 1; break; }
                    lo = mid;
                } else {
                    if (t >= F32(k, -0x10)) { idx = (uint32_t)mid; break; }
                    hi = mid;
                }
            }
        }
    }
    if (count == 0 || idx == 0) {   /* before the first key */
        SU32(out_prev, 0, keys + 0x10);
        SU32(out_next, 0, keys);
        SF32(out_frac, 0, phys_div(phys_sub(t, F32(last, 0)), phys_sub(F32(keys, 0), F32(keys, 0x10))));
        return 0;
    }
    if (idx == count) {             /* at or after the last key */
        const rx_addr before = keys + (count - 2u) * 16u;
        SU32(out_prev, 0, before);
        SU32(out_next, 0, last);
        SF32(out_frac, 0, phys_div(phys_sub(t, F32(last, 0)), phys_sub(F32(last, 0), F32(before, 0))));
        return 0;
    }
    const rx_addr next = keys + idx * 16u, prev = next - 16u;
    SU32(out_prev, 0, prev);
    SU32(out_next, 0, next);
    SF32(out_frac, 0, phys_div(phys_sub(t, F32(prev, 0)), phys_sub(F32(next, 0), F32(prev, 0))));
    return 1;
}
RX_FUNCTION(0x004dbce0, rx_world_keytrack_find_segment)

/* ---- terrain-track contacts of the rider / bike bodies (from the vehicles' update 00468300 and its ATV / UTV / buggy / truck
 * equivalents). A body group is an index array (int[6], count +0x18) into the body arrays (contact positions as vec4s,
 * previous positions, contact flags, weights, radii, track slots). A track slot (handle) is slot | tag: tag 0x1000 = set
 * 0xa95774, 0x2000 = 0xa95778, otherwise 0x4000 = the default set 0xa9577c. */

/* the track set and slot of a handle */
static rx_addr track_set(uint32_t handle, uint32_t* slot) {
    if (handle & 0x1000) { *slot = handle ^ 0x1000; return U32(0, 0x00a95774); }
    if (handle & 0x2000) { *slot = handle ^ 0x2000; return U32(0, 0x00a95778); }
    *slot = handle ^ 0x4000;
    return U32(0, 0x00a9577c);
}

/* 004db710 TerrainTracks_UpdateContactSlots (ECX = last positions, EDX = active flags; stack owner, count, touching, positions,
 * velocities, slots, intensities; RET 0x1c). Per contact i (active[i] cleared first): with the owner's +0x1320 set (replay /
 * no tracks) only last[i] = pos[i]. Otherwise a contact that starts touching without a slot gets one of the default set (tag
 * 0x4000) and intensity 0.5; a contact with a slot gets intensity = clamp(vel[i].y / 0.5 + C, 0, C) (C = the vector at
 * 0xd61650, minps / maxps lane 0); when all of lanes 0..2 of that are <= 0 the strip is cut, the slot released (cut again and
 * marked free in the default set) and set to -1, and last[i] = pos[i]. active[i] = 1 for a contact that keeps or got a slot. */
void rx_world_tracks_update_contact_slots(rx_addr last, rx_addr active, rx_addr owner, int32_t count, rx_addr touching,
                                          rx_addr pos, rx_addr vel, rx_addr slots, rx_addr intensity) {
    world_frame f = world_frame_enter(0x64, 3, 1);   /* push ebp; and esp, -16; sub esp, 0x64; push ebx, esi, edi */
    const rx_addr spill = f.esp + 0x60;                /* the contact position's spill slot */
    for (int32_t i = 0; i < count; i++) {
        const uint32_t k = (uint32_t)i;
        const rx_addr slot = slots + 4 * k, inten = intensity + 4 * k;
        SU32(active, 4 * k, 0);
        const rx_v4 p = v4_ld(pos + 16 * k);
        if (U32(owner, 0x1320) != 0) { v4_st(last + 16 * k, p); continue; }
        const uint32_t touch = U32(touching, 4 * k);
        if (touch == 0 && I32(slot, 0) < 0) continue;
        v4_st(spill, p);
        if (touch != 0 && I32(slot, 0) < 0) {
            SU32(slot, 0, world_tracks_alloc_slot_default(WCTX(.ebx = active - slots, .esi = inten, .edi = slot)) | 0x4000);
            SF32(inten, 0, 0.5f /* 0x9322cc */);
            SU32(active, 4 * k, 1);
            continue;
        }
        const rx_v4 c = v4_ld(0x00d61650), zero = v4_ld(WORLD_ZERO4);
        const rx_v4 w = v4_max(zero, v4_min(c, v4_add(v4_div(v4_lane(v4_ld(vel + 16 * k), 1), v4_splat(0.5f)), c)));
        SF32(inten, 0, w.v[0]);
        const rx_v4 faded = v4_cmple(w, v4_ld(WORLD_ZERO4));
        v4_st(spill, faded);
        if (!v4_all3(faded)) { SU32(active, 4 * k, 1); continue; }
        uint32_t s;
        const rx_addr set = track_set(U32(slot, 0), &s);
        world_tracks_cut_strip(set, s, WCTX(.edi = slot));
        const rx_addr def = U32(0, 0x00a9577c);
        const int32_t free_slot = (int32_t)(U32(slot, 0) ^ 0x4000);
        if (free_slot < I32(def, 0xc) && free_slot >= 0) {   /* TerrainTracks_FreeSlot, inlined */
            world_tracks_cut_strip(def, (uint32_t)free_slot, WCTX(.ebx = (uint32_t)free_slot, .edi = slot));
            SU32(U32(def, 0x24), (uint32_t)free_slot * 0xc0 + 0x98, 1);
            SU32(def, 0x28, U32(def, 0x28) - 1);
        }
        SU32(slot, 0, 0xffffffffu);
        v4_st(last + 16 * k, p);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x004db710, rx_world_tracks_update_contact_slots)

/* 004db8e0 TerrainTracks_AddContactPoints (ECX = terrain surface, EDI = contact positions (vec4s); stack previous positions,
 * slots, scale, touching, group, weights, radii, dt; RET 0x20): for a body group with a touching contact, the centroid c of
 * the group's points, then the weighted centre m = c + sum over the touching points of weight * (p - c) (the falloff starts
 * at 1.33333 and is * 0.75 per touching point). Per touching point p (in order): b = (p + m) * H (H = the vector at 0xd615d0),
 * a = b - (p - prev), half width = max(2 |m - b|, radius); a track point at b (TerrainTracks_AddPoint into the point's strip,
 * intensity = weight) and a deformation stamp from a to b with depth clamp((|p - prev| / dt - [0x9d789c]) / 20, 0, 1) *
 * scale * weight * falloff (DeformablePatch_AddStamp on surface +0x230). b and a live in the frame (passed by address; a's
 * fourth lane is the stale stack the original leaves there). */
void rx_world_tracks_add_contact_points(rx_addr surface, rx_addr pos, rx_addr prev, rx_addr slots, float scale, rx_addr touching,
                                        rx_addr group, rx_addr weights, rx_addr radii, float dt) {
    world_frame f = world_frame_enter(0x108, 2, 1);   /* push ebp; and esp, -16; sub esp, 0x108; push ebx; push esi */
    const rx_addr sp = f.esp, cookie = sp + 0x10c, mid = sp + 0x30, from = sp + 0x50;
    SU32(cookie, 0, U32(0, 0x009b45e4) ^ (sp + 8));
    const uint32_t n = U32(group, 0x18);
    uint32_t any = 0;
    for (uint32_t j = 0; j < n; j++) any += U32(touching, 4 * U32(group, 4 * j));
    if (any != 0) {
        rx_v4 acc = v4_ld(WORLD_ZERO4);
        for (uint32_t j = 0; j < n; j++) acc = v4_add(v4_ld(pos + 16 * U32(group, 4 * j)), acc);
        /* fild n (+2^32 when negative) / fld1 / fdivrp / fstp: x87 at the main thread's 53-bit precision (cw 0x027f) */
        const float inv = rx_f80_to_f32(rx_f80_div(rx_f80_from_f32(1.0f), rx_f80_from_int((int64_t)n), 53, 0), 0);
        const rx_v4 centroid = v4_mul(v4_splat(inv), acc);
        rx_v4 centre = centroid;
        float falloff = 1.33333f /* 0x977b00 */;
        uint32_t list[32], k = 0;   /* the touching points (the original's list holds at most 6 + 6: the group size) */
        for (uint32_t j = 0; j < n; j++) {
            const uint32_t e = U32(group, 4 * j);
            if (U32(touching, 4 * e) == 0) continue;
            const rx_v4 d = v4_mul(v4_splat(F32(weights, 4 * e)), v4_sub(v4_ld(pos + 16 * e), centroid));
            centre = v4_add(d, centre);
            list[k++ & 31] = e;
            falloff = falloff * 0.75f /* 0x935aa8 */;
        }
        const rx_addr patch = surface + 0x230;
        for (uint32_t j = 0; j < k; j++) {
            const uint32_t e = list[j & 31];
            const float radius = F32(radii, 4 * e), weight = F32(weights, 4 * e);
            const rx_addr p = pos + 16 * e, q = prev + 16 * e;
            const rx_v4 b = v4_mul(v4_add(v4_ld(p), centre), v4_ld(0x00d615d0));
            v4_st(mid, b);
            float hw = phys_mul(v4_len_xzy(v4_sub(centre, b)), 2.0f /* 0x92e330 */);
            const float dx = phys_sub(F32(p, 0), F32(q, 0)), dy = phys_sub(F32(p, 4), F32(q, 4)), dz = phys_sub(F32(p, 8), F32(q, 8));
            SF32(from, 0, phys_sub(b.v[0], dx));
            SF32(from, 4, phys_sub(b.v[1], dy));
            SF32(from, 8, phys_sub(b.v[2], dz));
            if (radius > hw) hw = radius;
            uint32_t s;
            const rx_addr set = track_set(U32(slots, 4 * e), &s);
            const uint32_t hw_arg = world_x87_pass(rx_fb(hw));
            world_tracks_add_point(set, s, mid, from, hw_arg, hw_arg, world_x87_pass(rx_fb(weight)), 0,
                                   WCTX(.ecx = from, .edx = mid, .ebx = k, .esi = patch, .edi = pos));
            const float vx = phys_div(dx, dt), vy = phys_div(dy, dt), vz = phys_div(dz, dt);
            float speed;
            WORLD_CALL_SP(4, speed = phys_sqrtf(phys_add(phys_add(phys_mul(vy, vy), phys_mul(vx, vx)), phys_mul(vz, vz))));
            float depth = phys_div(phys_sub(speed, F32(0, 0x009d789c)), 20.0f /* 0x935aac */);
            if (depth >= 1.0f /* 0x931f24 */) depth = 1.0f;
            else if (0.0f >= depth) depth = 0.0f;
            /* fld / fmul / fmul / fmul / fstp at the main thread's 53-bit precision (cw 0x027f, measured) */
            rx_f80 zx = rx_f80_mul(rx_f80_from_f32(depth), rx_f80_from_f32(scale), 53, 0);
            zx = rx_f80_mul(zx, rx_f80_from_f32(weight), 53, 0);
            zx = rx_f80_mul(zx, rx_f80_from_f32(falloff), 53, 0);
            const float z = rx_f80_to_f32(zx, 0);
            world_deform_add_stamp(patch, from, mid, rx_fb(z), rx_fb(z), hw_arg, hw_arg,
                                   WCTX(.ebx = k, .esi = patch, .edi = pos));
        }
    }
    phys_security_check_cookie(U32(cookie, 0) ^ (sp + 8));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004db8e0, rx_world_tracks_add_contact_points)
