/* world_board_handlers.c -- ThirtySecondBoard (vtable 0095f41c, 0x504 bytes): its "First Human Joined Activity" handler 006b7b90 (subscribed by ThirtySecondBoard::vf9
 * 006b7b60 through 006c3860) and the board's track projection 006b8ab0 that the handler tail-jumps to.
 *
 * Handler: by the activity's mode (+0x238) and kind (+0x23c): modes 1 / 5 / 8 / 6 with the "no board" flag (+0xfe8) set, and mode 3, hide the board (+0x4a0 = 1, +0xa2 = 0);
 * modes 1 / 5 / 8 / 6 without the flag show it for the kinds 0, 1, 0x13, 0xc, 7, 4, 0x23-0x25, 0x27-0x29, 0x2b-0x2d, 0x37-0x39 (any other kind: nothing). Showing: with a fixed
 * position (+0x4f0) the board's B point +0x4d0 is the one at +0x4e0; without, the board's own position (vf17) is projected onto the activity's track (block +0x890): the
 * track point at distance 1.0 from the start gives the direction (node -> next node), the board's offset from that node is projected on it, the point at the projected length
 * along the track is the new B point (+0x4d0), snapped onto the terrain of the board's scene. Without a track (+0xbb0 = 0) B = the board's position. */
#include "world_board_handlers.h"
#include "build_activity_util.h"
#include "phys_terrain.h"

/* fld m32 / fstp m32: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* 00538310 (ESI = out; stack track block, distance; RET 8): the track position at `distance` from the main section's start. out = {section, node, 0.0}: the first section of the
 * block (+0x130 array, count +0x1d8) whose flags (+0x118) have bit 2 set (else the block's +0x38); then the distance is walked: a section whose length (+0x120) is shorter is
 * skipped whole (next section +8 -> first entry), else its nodes are walked (node +0x64 = next node, the step the distance between them in 3-D) until the distance runs out. */
void rx_track_walk(rx_addr out, rx_addr tr, float dist) {
    ACT_FRAME_EBP(F, 16, 8, 2);
    const int32_t n = I32(tr, 0x1d8);
    SF32(out, 8, 0.0f); SU32(out, 4, 0); SU32(out, 0, 0);
    if (n > 0) {
        const rx_addr arr = U32(tr, 0x130);
        for (int32_t i = 0; i < n; i++) {
            if (U8(U32(arr, 4 * (uint32_t)i), 0x118) & 4) {
                const rx_addr s = U32(arr, 4 * (uint32_t)i);
                SU32(out, 0, s);
                SU32(out, 4, U32(s, 0));
                break;
            }
        }
    }
    if (U32(out, 0) == 0) SU32(out, 0, U32(tr, 0x38));
    if (dist > 0.0f) {
        for (;;) {
            const rx_addr sec = U32(out, 0);
            const float len = F32(sec, 0x120);
            if (dist > len) dist = rx_subs(dist, len);
            else {
                SU32(out, 4, U32(sec, 0));
                if (U32(U32(out, 4), 0x64) != 0) {
                    for (;;) {
                        if (!(dist > 0.0f)) goto done;
                        const rx_addr a = U32(out, 4), b = U32(a, 0x64);
                        const float dx = rx_subs(F32(a, 0), F32(b, 0)), dz = rx_subs(F32(a, 8), F32(b, 8)), dy = rx_subs(F32(a, 4), F32(b, 4));
                        const float l2 = rx_adds(rx_adds(rx_muls(dx, dx), rx_muls(dz, dz)), rx_muls(dy, dy));
                        dist = rx_subs(dist, rx_sqrts(l2));
                        SU32(out, 4, b);
                        if (U32(b, 0x64) == 0) break;
                    }
                }
            }
            if (!(dist > 0.0f)) break;
            SU32(out, 0, U32(U32(sec, 8), 0));
        }
    }
done:
    ACT_END(F);
}
RX_FUNCTION(0x00538310, rx_track_walk)

/* 0042d960 (EDX = the terrain list 0x009e30d4 {entry pointers; count at +8 = 0x009e30dc}, EDI = scene; EAX = result): the first non-null entry whose +0x44a8 (its scene) is the
 * scene, else 0. The callers (not this function) make the static list's guard (bit 0 of 0x009e30e0) true first. */
rx_addr rx_terrain_for_scene(rx_addr list, rx_addr scene) {
    const uint32_t n = U32(list, 8);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr e = U32(list, 4 * i);
        if (e != 0 && scene == U32(e, 0x44a8)) return e;
    }
    return 0;
}
RX_FUNCTION(0x0042d960, rx_terrain_for_scene)

/* 006b8ab0 (EAX = the activity, stack [esp + 4] = the board; RET 4; entered only by 006b7b90's tail jump) */
static void board_project_on_track(rx_addr self, rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x54, 3);
    const rx_addr pos = F + 0x40, out = F + 0x24;
    { const uint32_t a[1] = { pos }; act_vcall(self, 0x44, 1, a); }                  /* Entity::vf17: the board's position */
    if (U32(act, 0xbb0) != 0) {
        const rx_addr trk = act + 0x890;
        ACT_CALL(2, rx_track_walk(out, trk, 1.0f));
        const rx_addr node = U32(out, 4), next = U32(node, 0x64);
        const float x = F32(node, 0), y = F32(node, 4), z = F32(node, 8);
        float dy = rx_subs(F32(next, 4), y), dz = rx_subs(F32(next, 8), z), dx = rx_subs(F32(next, 0), x);
        const float l2 = rx_adds(rx_adds(rx_muls(dz, dz), rx_muls(dy, dy)), rx_muls(dx, dx));
        if (l2 > 0.0f) {
            const float inv = rx_divs(1.0f, rx_sqrts(l2));
            dy = rx_muls(dy, inv); dz = rx_muls(dz, inv); dx = rx_muls(dx, inv);
        }
        const float px = F32(pos, 0), py = F32(pos, 4), pz = F32(pos, 8);
        const float rx_ = rx_subs(px, x), ry = rx_subs(py, y), rz = rx_subs(pz, z);
        const float m2 = rx_adds(rx_adds(rx_muls(rz, rz), rx_muls(ry, ry)), rx_muls(rx_, rx_));
        float nx = rx_, ny = ry, nz = rz;
        if (m2 > 0.0f) {
            const float inv = rx_divs(1.0f, rx_sqrts(m2));
            nx = rx_muls(nx, inv); ny = rx_muls(ny, inv); nz = rx_muls(nz, inv);
        }
        const float dot = rx_adds(rx_adds(rx_muls(nz, dz), rx_muls(ny, dy)), rx_muls(nx, dx));
        const float qx = rx_muls(rx_, dot), qy = rx_muls(ry, dot), qz = rx_muls(rz, dot);
        const float len = rx_sqrts(rx_adds(rx_adds(rx_muls(qz, qz), rx_muls(qy, qy)), rx_muls(qx, qx)));
        ACT_CALL(2, rx_track_walk(out, trk, len));
        const rx_addr node2 = U32(out, 4);
        for (uint32_t k = 0; k < 12; k += 4) SU32(self, 0x4d0 + k, x87_copy(U32(node2, k)));
        if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
        const rx_addr scene = U32(self, 0x10);
        rx_addr terrain;
        ACT_CALLR(terrain, 1, rx_terrain_for_scene(0x009e30d4, scene));
        uint32_t edx;
        ACT_CALL(1, phys_terrain_base_height_at(terrain, self + 0x4d0, &edx));
    } else {
        SU32(self, 0x4d0, U32(pos, 0)); SU32(self, 0x4d4, U32(pos, 4)); SU32(self, 0x4d8, U32(pos, 8));
    }
    ACT_END(F);
}

/* 006b7b90 (thiscall ECX = the board, stack the activity; RET 4) */
void rx_thirty_second_board_first_human(rx_addr self, rx_addr act) {
    const uint32_t mode = U32(act, 0x238);
    int hide = 0;
    if (mode == 1 || mode == 5 || mode == 8 || mode == 6) {
        if (U32(act, 0xfe8) != 0) hide = 1;
        else {
            switch (U32(act, 0x23c)) {
            case 0: case 1: case 0x13: case 0xc: case 7: case 4: case 0x23: case 0x24: case 0x25: case 0x27: case 0x28: case 0x29:
            case 0x2b: case 0x2c: case 0x2d: case 0x37: case 0x38: case 0x39:
                if (U32(self, 0x4f0) != 0) {
                    for (uint32_t k = 0; k < 12; k += 4) SU32(self, 0x4d0 + k, x87_copy(U32(self, 0x4e0 + k)));
                } else board_project_on_track(self, act);
                return;
            default: return;
            }
        }
    } else if (mode == 3) hide = 1;
    if (hide) { SU32(self, 0x4a0, 1); SU8(self, 0xa2, 0); }
}
RX_FUNCTION(0x006b7b90, rx_thirty_second_board_first_human)
