/* race_track.c -- the race track's node graph as the drivers and the AI walk it.
 * Track node (0x500 bytes): +0x10 / +0x20 left / right edge point of the track, +0x30 / +0x40 the same for the second edge pair
 * (selected by a flag), +0x60 previous node, +0x64 next node (0 at a section's end), +0x4f0 length of the segment to the next node,
 * +0x4f4 remaining distance to the finish line. Section (a run of nodes between forks): +0x00 first node, +0x04 last node,
 * +0x08 / +0x0c the next / previous section lists (their first entry is followed), +0x10 / +0x14 their counts.
 * (core/notes/race.md "Track") */
#include "race.h"
#include "race_extern.h"

static inline rx_v4 v3(rx_addr p) { rx_v4 r = {{F32(p, 0), F32(p, 4), F32(p, 8), 0.0f}}; return r; }
static inline int same_bits3(rx_v4 a, rx_v4 b) {
    return rx_fb(a.v[0]) == rx_fb(b.v[0]) && rx_fb(a.v[1]) == rx_fb(b.v[1]) && rx_fb(a.v[2]) == rx_fb(b.v[2]);
}

/* One edge of a segment (p at node a, q at node b), cut to `keep` units: from_end != 0 keeps the first `keep` of it (q moves
 * toward p), from_end == 0 the last `keep` (p moves toward q). Edges shorter than keep stay whole. */
static void cut_edge(rx_v4* p, rx_v4* q, float keep, int from_end) {
    float dx = q->v[0] - p->v[0], dy = q->v[1] - p->v[1], dz = q->v[2] - p->v[2];
    float len2 = race_x87_sumsq3(dx, dz, dy);   /* x87, main thread: 53-bit precision */
    float len = sqrtf(len2);
    if (keep > len) return;
    float s = len - keep;
    if (len2 > 0.0f /* 0x92e12c */) {
        float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        dx = inv * dx; dy = dy * inv; dz = dz * inv;
    }
    if (from_end != 0) {
        float k = -0.0f /* 0x9774a8 */ - s;
        q->v[0] = dx * k + q->v[0]; q->v[1] = dy * k + q->v[1]; q->v[2] = dz * k + q->v[2];
    } else {
        p->v[0] = dx * s + p->v[0]; p->v[1] = dy * s + p->v[1]; p->v[2] = dz * s + p->v[2];
    }
}

/* Which side of the edge a -> b the point is on, in the ground plane (>= 0: inside, to the right of the edge); the y term is the
 * original's (a y difference times 0). */
static float edge_side(rx_v4 a, rx_v4 b, rx_v4 p) {
    return ((p.v[2] - a.v[2]) * (a.v[0] - b.v[0]) + (p.v[0] - a.v[0]) * (b.v[2] - a.v[2])) + (p.v[1] - a.v[1]) * 0.0f;
}

/* 00538e00 (ECX = second edge pair, EDX = node a, ESI = node b, args: position, length, from_end; cdecl) -> EAX:
 * is the position inside the track segment a -> b (the quad left a, left b, right b, right a in the ground plane), with both edges
 * cut to `length` (see cut_edge)? An edge whose two points are identical (a fork / join) does not limit the segment. */
int rx_track_segment_contains(int second_edges, rx_addr a, rx_addr b, rx_addr pos, float length, int from_end) {
    rx_v4 la = v3(a + (second_edges ? 0x30 : 0x10)), lb = v3(b + (second_edges ? 0x30 : 0x10));
    rx_v4 ra = v3(a + (second_edges ? 0x40 : 0x20)), rb = v3(b + (second_edges ? 0x40 : 0x20));
    int left_point = same_bits3(la, lb), right_point = same_bits3(ra, rb);
    if (!left_point) cut_edge(&la, &lb, length, from_end);
    if (!right_point) cut_edge(&ra, &rb, length, from_end);
    rx_v4 p = v3(pos);
    if (0.0f > edge_side(la, lb, p) && !left_point) return 0;
    if (0.0f > edge_side(lb, rb, p)) return 0;
    if (0.0f > edge_side(rb, ra, p) && !right_point) return 0;
    if (0.0f > edge_side(ra, la, p)) return 0;
    return 1;
}
RX_FUNCTION(0x00538e00, rx_track_segment_contains)

/* 005387c0 (EDI = query, arg = second edge pair; cdecl) -> EAX: find the track segment that contains a position, near a start node.
 * Query (race_track_query in memory): +0x00 result (section, node), +0x04 position, +0x08 start section, +0x0c start node,
 * +0x10 stop node (+0x1c: use it), +0x14 distance to search ahead, +0x18 behind, +0x20 follow into the next / previous section at a
 * section's end. Tests the start node's own segment first (whole), then walks ahead (segments cut to the distance left) and then
 * behind. Returns 1 = in the start segment, 3 = ahead, 2 = behind (result = the segment's first node and its section), 0 = not
 * found. */
int race_track_find(const race_track_query* q, int second_edges, rx_addr* out_section, rx_addr* out_node) {
    rx_addr node = q->node;
    if (U32(node, 0x64) != 0) {
        if (rx_track_segment_contains(second_edges, node, U32(node, 0x64), q->pos, rx_bf(0x7f7fffff) /* 0x977494 */, 0)) {
            *out_node = q->node; *out_section = q->section;
            return 1;
        }
        node = U32(q->node, 0x64);
    }
    float left = q->ahead;
    if (left > 0.0f) {
        rx_addr sec = q->section;
        do {
            if (U32(node, 0x64) == 0) {
                if (q->follow == 0 || U32(sec, 0x10) == 0) break;
                sec = U32(U32(sec, 8), 0);
                node = U32(sec, 0);
            } else if (q->use_stop != 0 && node == q->stop) break;
            if (rx_track_segment_contains(second_edges, node, U32(node, 0x64), q->pos, left, 1)) {
                *out_node = node; *out_section = sec;
                return 3;
            }
            left = left - F32(node, 0x4f0);
            node = U32(node, 0x64);
        } while (left > 0.0f);
    }
    left = q->behind;
    node = q->node;
    if (left > 0.0f) {
        rx_addr sec = q->section;
        do {
            if (U32(node, 0x60) == 0) {
                if (q->follow == 0 || U32(sec, 0x14) == 0) return 0;
                sec = U32(U32(sec, 0xc), 0);
                node = U32(sec, 4);
            } else if (q->use_stop != 0 && node == q->stop) return 0;
            if (rx_track_segment_contains(second_edges, U32(node, 0x60), node, q->pos, left, 0)) {
                *out_node = U32(node, 0x60); *out_section = sec;
                return 2;
            }
            node = U32(node, 0x60);
            left = left - F32(node, 0x4f0);
        } while (left > 0.0f /* 0x92e12c */);
    }
    return 0;
}
int rx_track_find_segment(rx_addr qa, int second_edges) {
    race_track_query q = {U32(qa, 4), U32(qa, 8), U32(qa, 0xc), U32(qa, 0x10), F32(qa, 0x14), F32(qa, 0x18), U32(qa, 0x1c), U32(qa, 0x20)};
    rx_addr out = U32(qa, 0), sec = 0, node = 0;
    int r = race_track_find(&q, second_edges, &sec, &node);
    if (r != 0) { SU32(out, 4, node); SU32(out, 0, sec); }
    return r;
}
RX_FUNCTION(0x005387c0, rx_track_find_segment)

/* Nearest node of a section and the sections after it (recursive; see rx_track_nearest_node). The distance is measured to the
 * node with its height taken as 0 (the original's y term is (-0 - y)^2). */
typedef struct { rx_addr section, node; float dist2; } race_nearest;
static void nearest_in_sections(rx_addr section, rx_addr node, rx_addr pos, race_nearest* out, float* budget_io) {
    float budget = *budget_io;
    out->dist2 = rx_bf(0x7f7fffff) /* 0x977494 */; out->node = 0; out->section = 0;
    for (; node != 0; node = U32(node, 0x64)) {
        if (!(budget > 0.0f)) { *budget_io = budget; return; }
        const float dz = F32(node, 8) - F32(pos, 8), dx = F32(node, 0) - F32(pos, 0);
        budget = budget - F32(node, 0x4f0);
        const float ny = -0.0f /* 0x9774a8 */ - F32(pos, 4);
        const float d2 = (dx * dx + dz * dz) + ny * ny;
        if (out->dist2 > d2) { out->dist2 = d2; out->node = node; out->section = section; }
        if (U32(node, 0x64) == 0) *budget_io = budget;
    }
    if (!(budget > 0.0f)) return;
    for (int32_t i = 0; i < I32(section, 0x10); i++) {
        rx_addr next = U32(U32(section, 8), 4 * (uint32_t)i);
        race_nearest r;
        float b = budget;
        nearest_in_sections(next, U32(next, 0), pos, &r, &b);
        if (out->dist2 > r.dist2) *out = r;
    }
}

/* 005381f0 (args: start (section, node), position, out (section, node, distance^2), distance budget; cdecl): the node nearest to
 * the position from the start node on, within the budget of track length (node +0x4f0), following every next section.
 * *budget is the original's argument slot: it is left holding the budget remaining after the start section's nodes. */
void rx_track_nearest_node(rx_addr start, rx_addr pos, rx_addr out, float* budget) {
    race_nearest r;
    nearest_in_sections(U32(start, 0), U32(start, 4), pos, &r, budget);
    SF32(out, 8, r.dist2); SU32(out, 4, r.node); SU32(out, 0, r.section);
}
RX_FUNCTION(0x005381f0, rx_track_nearest_node)

/* 00538440 (EDX = out (section, node, distance^2), ESI = position, args: track, direction (or 0), second edge pair; cdecl): the
 * node of the whole track nearest to the position, measured to the node and to its two edge points (the smallest of the three),
 * over every section not flagged +0x118 bit 1 (track +0x130 sections, +0x1d8 count); with a direction only nodes whose
 * direction (+0x50) points the same way (dot > 0). */
void rx_track_nearest_node_all(rx_addr track, rx_addr dir, int second_edges, rx_addr pos, rx_addr out) {
    SF32(out, 8, rx_bf(0x7f7fffff) /* 0x977494 */); SU32(out, 4, 0); SU32(out, 0, 0);
    for (int32_t i = 0; i < I32(track, 0x1d8); i++) {
        rx_addr section = U32(U32(track, 0x130), 4 * (uint32_t)i);
        if ((U8(section, 0x118) & 2) != 0) continue;
        for (rx_addr node = U32(section, 0); node != 0; node = U32(node, 0x64)) {
            const float px = F32(pos, 0), py = F32(pos, 4), pz = F32(pos, 8);
            const float cx = F32(node, 0) - px, cy = F32(node, 4) - py, cz = F32(node, 8) - pz;
            rx_addr l = node + (second_edges ? 0x30 : 0x10), r = node + (second_edges ? 0x40 : 0x20);
            const float lx = F32(l, 0) - px, ly = F32(l, 4) - py, lz = F32(l, 8) - pz;
            const float rx = F32(r, 0) - px, ry = F32(r, 4) - py, rz = F32(r, 8) - pz;
            const float dc = (cz * cz + cx * cx) + cy * cy;
            const float dl = (lz * lz + ly * ly) + lx * lx;
            const float dr = (rz * rz + ry * ry) + rx * rx;
            float d = dc;
            if (d >= dl) d = dl;
            if (d >= dr) d = dr;
            if (!(F32(out, 8) > d)) continue;
            if (dir != 0 && !(((F32(node, 0x58) * F32(dir, 8) + F32(node, 0x54) * F32(dir, 4)) + F32(node, 0x50) * F32(dir, 0)) > 0.0f))
                continue;
            SU32(out, 4, node);
            SU32(out, 0, U32(U32(track, 0x130), 4 * (uint32_t)i));
            SF32(out, 8, d);
        }
    }
}
RX_FUNCTION(0x00538440, rx_track_nearest_node_all)

/* 005389c0 (ECX = a, EAX = b, arg = point, ESI = out; cdecl): the point projected onto the segment a -> b in the ground plane,
 * clamped to the segment, height 0. */
void rx_track_project_clamped_xz(rx_addr a, rx_addr b, rx_addr p, rx_addr out) {
    const float ax = F32(a, 0), az = F32(a, 8);
    const float dx = F32(b, 0) - ax, dz = F32(b, 8) - az;
    float t = ((F32(p, 0) - ax) * dx + (F32(p, 8) - az) * dz) / (dx * dx + dz * dz);
    if (1.0f /* 0x931f24 */ > t && 0.0f > t) t = 0.0f;
    else if (!(1.0f > t)) t = 1.0f;
    SF32(out, 0, dx * t + ax);
    SF32(out, 4, 0.0f);
    SF32(out, 8, dz * t + az);
}
RX_FUNCTION(0x005389c0, rx_track_project_clamped_xz)

/* 00538b30 (EDX = a, ECX = b, ESI = point, arg = out; cdecl) -> EAX = out, xmm0 = t: the point projected onto the line a -> b in
 * the ground plane (not clamped), height 0; t = the line parameter. */
float rx_track_project_xz(rx_addr a, rx_addr b, rx_addr p, rx_addr out) {
    const float ax = F32(a, 0), az = F32(a, 8);
    const float dx = F32(b, 0) - ax, dz = F32(b, 8) - az;
    const float t = rx_divs((F32(p, 8) - az) * dz + (F32(p, 0) - ax) * dx, dz * dz + dx * dx);
    SF32(out, 0, rx_adds(rx_muls(dx, t), ax));
    SF32(out, 4, 0.0f);
    SF32(out, 8, rx_adds(rx_muls(dz, t), az));
    return t;
}
RX_FUNCTION(0x00538b30, rx_track_project_xz)
