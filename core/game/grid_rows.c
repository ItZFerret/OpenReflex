/* grid_rows.c -- PORT ADDITION (not original game code): the start grid's rows behind the gate (grid_rows.h, core/plan/GRID_ROWS.md). */
#include "grid_rows.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_world.h"
#include "loop_vehicle.h"
#include "phys_terrain.h"
#include "map_scale.h"
#include <math.h>
#include <string.h>
#include "rider_cap.h"

uint32_t rx_grid_active_cell(uint32_t row);   /* build_activity_grid.c: the Player List's "Active" cell, as 0075f890 reads it */

#define SLOT(A, z) rx_start_slot((A), (uint32_t)(z))   /* (w-rc-core) slots 28..31 in side storage (rider_cap.h) */
#define ZONE_NAME 0x0093df00u   /* "StartZone" */
#define CLASS_LENGTH 0x0093582cu   /* the vehicle classes' lengths (vehicle +0x194), as race_driver_reset.c spaces a start line */
#define CLASS_WIDTH 0x0093580cu

static uint32_t g_cap;
static rx_grid_info g_info;
static rx_grid_place_info g_place[RX_GRID_SLOTS];
static uint32_t g_places;

void rx_grid_rows_test_cap(uint32_t n) { g_cap = n; }
uint32_t rx_grid_rows_cap(void) { return g_cap; }

/* ---- the places' numbering (grid_rows.h) ---- */
static uint32_t row_places(uint32_t slots, uint32_t row) {
    if (slots <= 1) return 1;
    return (row & 1) ? slots : slots - 1;   /* row 2, 4 ...: behind the gaps; row 3, 5 ...: behind the slots */
}
/* the k-th place of a row of `count` from the centre outwards: its index across the gate */
static uint32_t centre_out(uint32_t count, uint32_t k) {
    /* the positions ranked by their distance from the middle (twice the offset: |2 j - (count - 1)|), ties to the lower index */
    for (uint32_t j = 0; j < count; j++) {
        uint32_t before = 0;
        const int32_t dj = abs((int32_t)(2 * j) - (int32_t)(count - 1));
        for (uint32_t o = 0; o < count; o++) {
            const int32_t d = abs((int32_t)(2 * o) - (int32_t)(count - 1));
            if (d < dj || (d == dj && o < j)) before++;
        }
        if (before == k) return j;
    }
    return 0;
}
int rx_grid_place(uint32_t slots, uint32_t zone, uint32_t* row, uint32_t* across) {
    if (slots == 0 || slots > 16 || zone >= RX_GRID_SLOTS) return -1;
    if (zone < slots) { *row = 1; *across = zone; return 0; }
    uint32_t k = zone - slots, r = 2;
    for (;;) {
        const uint32_t c = row_places(slots, r);
        if (k < c) { *row = r; *across = centre_out(c, k); return 0; }
        k -= c; r++;
    }
}

/* ---- the start zones (generalises w-aliveai's alive_gate_pack) ---- */
/* returns the slots in use: the highest zone + 1, at least n */
static uint32_t pack_zones(uint32_t n, int32_t* riders_out) {
    ACT_FRAME(L, 8, 0);
    const rx_addr t = ACT_PLAYER_LIST;
    uint32_t rows = rx_tbl_row_count(t);
    if (rows > 64) rows = 64;
    int32_t zone[64];
    int32_t riders = 0;
    for (uint32_t r = 0; r < rows; r++) {
        zone[r] = rx_grid_active_cell(r) ? (int32_t)act_cell(t, ZONE_NAME, r) : -1;
        if (rx_grid_active_cell(r)) riders++;
    }
    *riders_out = riders;
    int distinct = 1, in_gate = 1;
    for (uint32_t r = 0; r < rows; r++) {
        if (zone[r] < 0) continue;
        if (zone[r] >= (int32_t)n) in_gate = 0;
        for (uint32_t o = 0; o < r; o++) if (zone[o] == zone[r]) distinct = 0;
    }
    /* kept: zones that already name distinct gate slots */
    const int keep = distinct && in_gate;
    if (!keep) {
        /* ranked in their order (the grid pass's random first zone and step: the riders' order across the gate stays the game's), the
         * lowest zone on the first slot; ranks past the gate take the places behind it */
        for (uint32_t r = 0; r < rows; r++) {
            if (zone[r] < 0) continue;
            uint32_t rank = 0;
            for (uint32_t o = 0; o < rows; o++)
                if (o != r && zone[o] >= 0 && (zone[o] < zone[r] || (zone[o] == zone[r] && o < r))) rank++;
            if (rank > RX_GRID_SLOTS - 1) rank = RX_GRID_SLOTS - 1;
            SU32(L, 4, rank);
            ACT_CALL(2, rx_tbl_set_cell_int(t, ZONE_NAME, r, L + 4));
        }
        for (uint32_t r = 0; r < rows; r++) if (zone[r] >= 0) zone[r] = (int32_t)act_cell(t, ZONE_NAME, r);
    }
    uint32_t used = n;
    for (uint32_t r = 0; r < rows; r++) if (zone[r] >= 0 && (uint32_t)zone[r] + 1 > used) used = (uint32_t)zone[r] + 1;
    ACT_END(L);
    return used;
}

/* ---- the ground ---- */
static rx_addr terrain_of(rx_addr scene) {
    if (!(U8(0, 0x009e30e0) & 1)) return 0;   /* (no terrain registered yet: the list's guard is the track loader's to set) */
    const uint32_t c = U32(0, 0x009e30dc);
    for (uint32_t i = 0; i < c; i++) {
        const rx_addr e = U32(0, 0x009e30d4 + 4 * i);
        if (e && U32(e, 0x44a8) == scene) return e;
    }
    return 0;
}
/* the ground at (x, z) near height y: the terrain's base height, or a static collision surface above it (the first hit of a vertical
 * ray from `up` ft over the higher of the two down to 3 ft under the lower); 0 when neither is there */
static int ground_at(rx_addr terrain, rx_addr world, float x, float y, float z, float up, float* out, int32_t* on_static) {
    ACT_FRAME_EBP(L, 16, 0x80, 0);
    int have = 0;
    float g = 0.0f, top = y, bottom = y;
    *on_static = 0;
    if (terrain) {
        SF32(L, 0, x); SF32(L, 4, y); SF32(L, 8, z); SF32(L, 0xc, 0.0f);
        uint32_t edx = 0, ok;
        ACT_CALLR(ok, 1, phys_terrain_base_height_at(terrain, L, &edx));
        if (ok) { g = F32(L, 4); have = 1; if (g > top) top = g; if (g < bottom) bottom = g; }
    }
    if (world) {
        const rx_addr ray = L + 0x10, hit = L + 0x30;
        SF32(ray, 0, x); SF32(ray, 4, top + up); SF32(ray, 8, z); SF32(ray, 0xc, 0.0f);
        SF32(ray, 0x10, x); SF32(ray, 0x14, bottom - 3.0f); SF32(ray, 0x18, z); SF32(ray, 0x1c, 0.0f);
        ACT_CALL(5, rx_physics_world_raycast_bodies(world, ray, 1, hit, 0));
        if (U8(hit, 0) && (!have || F32(hit, 0x14) > g)) { g = F32(hit, 0x14); have = 1; *on_static = 1; }
    }
    ACT_END(L);
    if (have) *out = g;
    return have;
}

/* the longest / widest vehicle of the event's riders (the class tables: a bike 7.5 x 2.5 ft, an ATV 6 x 4.8), a bike's without one. An
 * online race's riders ride the event's class, so every rider's game works the places out from the same vehicle. */
static void gate_vehicles(float* length, float* width) {
    *length = 0.0f; *width = 0.0f;
    const rx_addr g = U32(0, 0x00a946b8);
    if (g) {
        for (uint32_t i = 0; i < U8(g, 0x1541c) && i < 12; i++) {
            if (U32(g + 0x7460, 0xdf8c + 4 * i) == 0xffffffffu) continue;
            rx_addr P;
            ACT_CALLR(P, 2, rx_player_active(g + 0x7460, i, 3));
            const rx_addr v = U32(P, 0x1ac);
            if (!v || U32(v, 0x194) > 7) continue;
            const float l = F32(0, CLASS_LENGTH + 4 * U32(v, 0x194)), w = F32(0, CLASS_WIDTH + 4 * U32(v, 0x194));
            if (l > *length) *length = l;
            if (w > *width) *width = w;
        }
    }
    if (*length <= 0.0f) *length = F32(0, CLASS_LENGTH);
    if (*width <= 0.0f) *width = F32(0, CLASS_WIDTH);
}

/* ---- the places ----
 * Each place is worked out on its own, from the gate and the level only (never from which places the riders took: every rider's game
 * of an online race holds a different part of the field and must put each place in the same spot). A place stands on a lane, a line
 * along the gate's forward axis through the middle of a gap between two gate slots (even rows) or a slot (odd rows). Its depth (ft behind
 * the gate's slot line) is searched along the lane from the preferred one (the bikes ahead: their rear plus half a bike plus a gap of half
 * a pitch, at most 3 ft) back, then forward (down to the bikes ahead's rear overlapping half a foot: the place is between them), then on
 * lanes to either side (behind a gap, as far as keeps a vehicle's width from both slots: an Alive gate's double middle gap), for the
 * first depth where
 *  - the bike stands on even ground (the ground profile under it and a quarter foot around: no step of more than 0.2 ft between samples
 *    a quarter foot apart: no kerb, rail or platform edge under a wheel; an Alive gate's raised pad ends a few feet behind its slots), and
 *  - the lane ahead to the gate is rideable: no obstacle (a rail, a post: higher by half a foot than the ground 2.5 ft before and after it)
 *    and no rise above 1.5 ft (a platform's edge is climbed from 2 ft of run-up: measured, the AI ride up Alive's 1.3 ft start pads), and
 *  - nothing stands in the way of the rider's body from the place through the gate (flat rays at 1.5, 2.5 and 4.5 ft: a start tower's
 *    legs or a starter's dog house in the middle of an Alive gate, a post, a wall).
 * The ground is sampled every quarter foot on three lines (the lane and 0.9 ft to either side: a bike's width), its height the highest. */
#define PROF_STEP 0.25f
#define PROF_FRONT 1.0f   /* the profile starts 1 ft in front of the slot line */
#define PROF_N 400        /* .. and reaches at most 99 ft behind it */
typedef struct { float h[PROF_N]; } profile_t;
static int idx_of(float t) { const int i = (int)floorf((t + PROF_FRONT) / PROF_STEP + 0.5f); return i < 0 ? 0 : i >= PROF_N ? PROF_N - 1 : i; }
static void lane_profile(rx_addr terrain, rx_addr world, const float base[3], const float f[3], const float a[3], float to, profile_t* p) {
    const int last = idx_of(to);
    for (int i = 0; i < PROF_N; i++) {
        if (i > last) { p->h[i] = p->h[i - 1]; continue; }
        const float t = (float)i * PROF_STEP - PROF_FRONT;
        float best = -1e9f;
        for (int k = -1; k <= 1; k++) {
            const float x = base[0] - f[0] * t + a[0] * 0.9f * (float)k, z = base[2] - f[2] * t + a[2] * 0.9f * (float)k;
            float h; int32_t st;
            if (ground_at(terrain, world, x, base[1], z, 6.0f, &h, &st) && h > best) best = h;
        }
        p->h[i] = best > -1e8f ? best : (i ? p->h[i - 1] : base[1]);
    }
}
/* a static collision that a rider's body meets riding from `from` ft behind the slot line forward through the gate to a vehicle's length in
 * front of it (a post, a tower's leg, a starter's dog house, a wall: what the ground profile cannot see from above): flat rays 1.5, 2.5 and
 * 4.5 ft over the ground at the slot line (the bike, the rider's chest), on the lane and 0.9 ft to either side. Cast from behind (some
 * surfaces collide from one side only: JS7's dog house in the middle of its gate is open to the front). 1 blocked, 0 clear. */
static int lane_blocked(rx_addr world, const float base[3], const float f[3], const float a[3], float ground, float len, float from) {
    if (!world) return 0;
    int blocked = 0;
    ACT_FRAME_EBP(L, 16, 0x60, 0);
    const rx_addr ray = L, hit = L + 0x20;
    for (int k = -1; k <= 1 && !blocked; k++) {
        for (int hgt = 0; hgt < 3 && !blocked; hgt++) {
            const float y = ground + (hgt == 0 ? 1.5f : hgt == 1 ? 2.5f : 4.5f), ox = base[0] + a[0] * 0.9f * (float)k, oz = base[2] + a[2] * 0.9f * (float)k;
            SF32(ray, 0, ox - f[0] * from); SF32(ray, 4, y); SF32(ray, 8, oz - f[2] * from); SF32(ray, 0xc, 0.0f);
            SF32(ray, 0x10, ox + f[0] * len); SF32(ray, 0x14, y); SF32(ray, 0x18, oz + f[2] * len); SF32(ray, 0x1c, 0.0f);
            ACT_CALL(5, rx_physics_world_raycast_bodies(world, ray, 1, hit, 0));
            if (U8(hit, 0)) blocked = 1;
        }
    }
    ACT_END(L);
    return blocked;
}
static float window_min(const profile_t* p, int lo, int hi) {
    if (lo < 0) lo = 0;
    if (hi > PROF_N - 1) hi = PROF_N - 1;
    float m = 1e9f;
    for (int i = lo; i <= hi; i++) if (p->h[i] < m) m = p->h[i];
    return m;
}
/* a bike of length `len` centred `d` ft behind the slot line: 0 fits, 1 uneven ground under it, 2 an obstacle or a too high rise ahead */
static int lane_fits(const profile_t* p, float d, float len) {
    const int front = idx_of(d - 0.5f * len), rear = idx_of(d + 0.5f * len);
    for (int i = front - 1; i < rear + 1; i++) {
        if (i < 0 || i + 1 >= PROF_N) return 1;
        if (fabsf(p->h[i + 1] - p->h[i]) > 0.2f) return 1;
    }
    const int line = idx_of(0.0f), win = (int)(2.5f / PROF_STEP);
    float high = p->h[front];
    for (int i = front - 1; i >= line; i--) {
        const float h = p->h[i];
        if (h - window_min(p, i - win, i) > 0.5f && h - window_min(p, i, i + win) > 0.5f) return 2;   /* a rail, a post */
        if (h - p->h[i + 1] > 0.2f && (float)(front - i) * PROF_STEP < 2.0f) return 2;               /* a step without run-up */
        if (h > high) high = h;
    }
    if (high - p->h[front] > 1.5f) return 2;
    return 0;
}

static void build_places(rx_addr A, uint32_t n, uint32_t used) {
    const rx_addr mxworld = U32(A, 0x234);
    const rx_addr terrain = terrain_of(U32(mxworld, 0x20138)), world = U32(mxworld, 0x20134);
    /* the gate's axes: forward = the slots' mean direction (flat); across = GateStart_01 -> the last slot (flat); the pitch the median
     * spacing of neighbouring slots (an Alive gate's middle gap is a double one) */
    float f[3] = {0, 0, 0}, a[3] = {0, 0, 0};
    for (uint32_t i = 0; i < n; i++) { f[0] += F32(SLOT(A, i), 0x10); f[2] += F32(SLOT(A, i), 0x18); }
    float len, wid;
    gate_vehicles(&len, &wid);
    float pitch = wid;
    if (n >= 2) {
        a[0] = F32(SLOT(A, n - 1), 0) - F32(SLOT(A, 0), 0); a[2] = F32(SLOT(A, n - 1), 8) - F32(SLOT(A, 0), 8);
        const float al = sqrtf(a[0] * a[0] + a[2] * a[2]);
        if (al > 0.0f) { a[0] /= al; a[2] /= al; }
        float gaps[15];
        for (uint32_t i = 0; i + 1 < n; i++) {
            const float dx = F32(SLOT(A, i + 1), 0) - F32(SLOT(A, i), 0), dz = F32(SLOT(A, i + 1), 8) - F32(SLOT(A, i), 8);
            gaps[i] = sqrtf(dx * dx + dz * dz);
        }
        for (uint32_t i = 1; i + 1 < n; i++) for (uint32_t k = i; k > 0 && gaps[k - 1] > gaps[k]; k--) { const float t = gaps[k]; gaps[k] = gaps[k - 1]; gaps[k - 1] = t; }
        if (gaps[(n - 1) / 2] > 0.0f) pitch = gaps[(n - 1) / 2];
    }
    const float fl = sqrtf(f[0] * f[0] + f[2] * f[2]);
    if (fl > 0.0f) { f[0] /= fl; f[2] /= fl; }
    else { f[0] = a[2]; f[2] = -a[0]; }   /* (slots without a direction: square to the gate) */
    if (n < 2 || (a[0] == 0.0f && a[2] == 0.0f)) { a[0] = f[2]; a[2] = -f[0]; }
    float gap = 0.5f * pitch;
    if (gap > 3.0f) gap = 3.0f;
    if (gap < 1.0f) gap = 1.0f;
    /* the gate slots' height over their ground (the floor plates / pad they stand on) */
    float clear = 0.0f; uint32_t grounded = 0;
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr s = SLOT(A, i);
        float gy; int32_t st;
        if (ground_at(terrain, world, F32(s, 0), F32(s, 4), F32(s, 8), 3.0f, &gy, &st)) { clear += F32(s, 4) - gy; grounded++; }
    }
    if (grounded) clear /= (float)grounded;
    g_info.pitch = pitch; g_info.depth = len + gap; g_info.clearance = clear;
    for (int k = 0; k < 3; k++) { g_info.fwd[k] = f[k]; g_info.across[k] = a[k]; }
    g_info.activity = A;
    const float radius = (F32(A, 0xa70) + F32(A, 0xa6c)) * (rx_ms_on() ? rx_ms_scale() : 1.0f);
    /* every lane of the rows up to the last one used, row by row (a lane's depth follows the bikes ahead of it). Within a row the zones
     * take the lanes from the centre out (rx_grid_place), those with a clear spot first: a lane without one (a start tower's legs in the
     * middle of the gate) is the row's last resort, taken only when every other lane of the row is. */
    uint32_t last_row = 1, j0;
    if (rx_grid_place(n, used - 1, &last_row, &j0) != 0) return;
    typedef struct { float c, depth, shift; int32_t how, why; } lane_t;
    static lane_t lanes[2][16];   /* the row before (row 1: the gate's slots) and this row, by position across */
    const rx_addr s0 = SLOT(A, 0);
    uint32_t prev_count = n;
    for (uint32_t j = 0; j < n; j++) {
        lanes[0][j].c = (F32(SLOT(A, j), 0) - F32(s0, 0)) * a[0] + (F32(SLOT(A, j), 8) - F32(s0, 8)) * a[2];
        lanes[0][j].depth = 0.0f;
    }
    static profile_t prof;
    uint32_t z = n;
    for (uint32_t row = 2; row <= last_row && z < RX_GRID_SLOTS; row++) {
        const uint32_t count = row_places(n, row);
        lane_t* cur = lanes[1];
        for (uint32_t j = 0; j < count; j++) {
            /* the lane: behind the gap j | j + 1 (even rows) or slot j (odd rows) */
            const uint32_t jb = (row & 1) || n < 2 ? j : j + 1;
            const rx_addr sa = SLOT(A, j), sb = SLOT(A, jb);
            const float base[3] = {0.5f * (F32(sa, 0) + F32(sb, 0)), 0.5f * (F32(sa, 4) + F32(sb, 4)), 0.5f * (F32(sa, 8) + F32(sb, 8))};
            const float ca = (F32(sa, 0) - F32(s0, 0)) * a[0] + (F32(sa, 8) - F32(s0, 8)) * a[2];
            const float cb = (F32(sb, 0) - F32(s0, 0)) * a[0] + (F32(sb, 8) - F32(s0, 8)) * a[2];
            const float c = 0.5f * (ca + cb);
            /* the bikes ahead: the row before's lanes within 3/4 pitch across (row 2: the two gate slots; row 3: the row-2 lanes beside its
             * slot); their rear */
            float ahead = 0.0f; int any = 0;
            for (uint32_t o = 0; o < prev_count; o++)
                if (fabsf(lanes[0][o].c - c) < 0.75f * pitch) { if (!any || lanes[0][o].depth > ahead) ahead = lanes[0][o].depth; any = 1; }
            if (!any) for (uint32_t o = 0; o < prev_count; o++) if (lanes[0][o].depth > ahead) ahead = lanes[0][o].depth;
            const float pref = ahead + len + gap, lo = ahead + len - 0.5f, hi = pref + 2.0f * len;
            float d = pref, shift = 0.0f; int32_t how = 4, why = 0;
            /* (a lane between two slots may move across, as far as keeps a vehicle's width from either slot: half, then all of that) */
            const float room = jb == j ? 0.0f : 0.5f * fabsf(cb - ca) - wid;
            const float shifts[5] = {0.0f, -0.5f * room, 0.5f * room, -room, room};
            for (int si = 0; si < 5 && how == 4; si++) {
                if (si > 0 && room <= 0.0f) break;
                const float sh = shifts[si], bl[3] = {base[0] + a[0] * sh, base[1], base[2] + a[2] * sh};
                lane_profile(terrain, world, bl, f, a, hi + len, &prof);
                const float gy0 = prof.h[idx_of(0.0f)];
                for (float t = pref; t <= hi && how == 4; t += PROF_STEP) {
                    int r = lane_fits(&prof, t, len);
                    if (r == 0 && lane_blocked(world, bl, f, a, gy0, len, t + 0.5f * len)) r = 3;
                    if (r == 0) { d = t; shift = sh; how = t == pref ? (si ? 3 : 0) : (si ? 3 : 1); }
                    else if (t == pref && si == 0) why = r;
                }
                for (float t = pref - PROF_STEP; t >= lo && how == 4; t -= PROF_STEP)
                    if (lane_fits(&prof, t, len) == 0 && !lane_blocked(world, bl, f, a, gy0, len, t + 0.5f * len)) { d = t; shift = sh; how = si ? 3 : 2; }
            }
            cur[j].c = c + shift; cur[j].depth = d; cur[j].shift = shift; cur[j].how = how; cur[j].why = why;
        }
        /* the zones of this row: the lanes centre-out, clear ones first */
        uint32_t order[16], k = 0;
        for (int pass = 0; pass < 2; pass++)
            for (uint32_t q = 0; q < count; q++) {
                const uint32_t j = centre_out(count, q);
                if ((cur[j].how == 4) == (pass == 1)) order[k++] = j;
            }
        for (uint32_t q = 0; q < count && z < RX_GRID_SLOTS; q++, z++) {
            if (z < n || z >= used) continue;
            const uint32_t j = order[q], jb = (row & 1) || n < 2 ? j : j + 1;
            const rx_addr sa = SLOT(A, j), sb = SLOT(A, jb);
            const float base[3] = {0.5f * (F32(sa, 0) + F32(sb, 0)), 0.5f * (F32(sa, 4) + F32(sb, 4)), 0.5f * (F32(sa, 8) + F32(sb, 8))};
            const float d = cur[j].depth, shift = cur[j].shift;
            float p[3] = {base[0] + a[0] * shift - f[0] * d, base[1], base[2] + a[2] * shift - f[2] * d};
            float dir[3] = {F32(sa, 0x10) + F32(sb, 0x10), F32(sa, 0x14) + F32(sb, 0x14), F32(sa, 0x18) + F32(sb, 0x18)};
            const float dl = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
            if (dl > 0.0f) { dir[0] /= dl; dir[1] /= dl; dir[2] /= dl; } else { dir[0] = f[0]; dir[1] = 0.0f; dir[2] = f[2]; }
            float gy = base[1] - clear; int32_t st = 0;
            if (ground_at(terrain, world, p[0], p[1], p[2], 3.0f, &gy, &st)) p[1] = gy + clear;
            rx_grid_place_info* info = &g_place[g_places++];
            info->zone = (int32_t)z; info->row = (int32_t)row; info->across = (int32_t)j;
            info->depth = d; info->shift = shift; info->how = cur[j].how; info->why = cur[j].why;
            info->ground = gy; info->on_static = st;
            const rx_addr s = SLOT(A, z);
            SF32(s, 0, p[0]); SF32(s, 4, p[1]); SF32(s, 8, p[2]);
            SF32(s, 0x10, dir[0]); SF32(s, 0x14, dir[1]); SF32(s, 0x18, dir[2]);
            /* the track section / node: the gate slot's ahead (the place's own search when it finds that section) */
            uint32_t sec = U32(sa, 0x20), node = U32(sa, 0x24);
            {
                ACT_FRAME(L, 0x10, 0);
                const rx_addr sp = rx_sp();
                rx_set_sp(sp - 0x18);
                rx_track_nearest_in_radius(s, A + 0x890, L, radius, 0, 0);
                rx_set_sp(sp);
                info->found_section = U32(L, 0) == 0 ? 0 : U32(L, 0) == sec ? 1 : 2;
                if (U32(L, 0) == sec && U32(L, 4) != 0) node = U32(L, 4);
                ACT_END(L);
            }
            SU32(s, 0x20, sec); SU32(s, 0x24, node);
            SU32(s, 0x2c, 1);
            for (int q2 = 0; q2 < 3; q2++) { info->pos[q2] = F32(s, 4u * (uint32_t)q2); info->dir[q2] = F32(s, 0x10u + 4u * (uint32_t)q2); }
        }
        memcpy(lanes[0], lanes[1], sizeof lanes[0]);
        prev_count = count;
    }
    g_info.places = (int32_t)g_places;
}

void rx_grid_rows(rx_addr A, rx_addr gate, uint32_t n) {
    (void)gate;
    memset(&g_info, 0, sizeof g_info); memset(g_place, 0, sizeof g_place); g_places = 0;
    g_info.slots = (int32_t)n;
    if (n == 0 || n > 16) return;
    if (n == 16) {
        /* the original's gate: untouched unless more riders than slots could be on it (the Player List's rows) */
        if (rx_tbl_row_count(ACT_PLAYER_LIST) <= n) return;
    }
    int32_t riders = 0;
    const uint32_t used = pack_zones(n, &riders);
    g_info.riders = riders;
    SU32(A, 0x880, used);
    if (used > n) {
        build_places(A, n, used);
        int moved = 0, none = 0;
        for (uint32_t k = 0; k < g_places; k++) { moved += g_place[k].how != 0 && g_place[k].how != 4; none += g_place[k].how == 4; }
        fprintf(stderr, "rxcore: start grid: %d riders, a gate of %u slots: %u place(s) behind it (pitch %.1f ft, row depth %.1f ft; %d moved "
                "for the ground, %d without a clear spot)\n", (int)riders, n, used - n, (double)g_info.pitch, (double)g_info.depth, moved, none);
    }
}

int rx_grid_ground_probe(rx_addr activity, float x, float y, float z, float* h, int32_t* on_static) {
    const rx_addr mxworld = U32(activity, 0x234);
    return ground_at(terrain_of(U32(mxworld, 0x20138)), U32(mxworld, 0x20134), x, y, z, 3.0f, h, on_static);
}

int rx_grid_ray_probe(rx_addr activity, const float p0[3], const float p1[3], float hit_out[3]) {
    const rx_addr world = U32(U32(activity, 0x234), 0x20134);
    if (!world) return 0;
    ACT_FRAME_EBP(L, 16, 0x60, 0);
    const rx_addr ray = L, hit = L + 0x20;
    for (int k = 0; k < 3; k++) { SF32(ray, 4u * (uint32_t)k, p0[k]); SF32(ray, 0x10u + 4u * (uint32_t)k, p1[k]); }
    SF32(ray, 0xc, 0.0f); SF32(ray, 0x1c, 0.0f);
    ACT_CALL(5, rx_physics_world_raycast_bodies(world, ray, 1, hit, 0));
    const int r = U8(hit, 0) != 0;
    if (r) for (int k = 0; k < 3; k++) hit_out[k] = F32(hit, 0x10u + 4u * (uint32_t)k);
    ACT_END(L);
    return r;
}

uint32_t rx_grid_rows_info(rx_grid_info* info, rx_grid_place_info* out, uint32_t max) {
    if (info) *info = g_info;
    uint32_t k = 0;
    for (; k < g_places && k < max; k++) out[k] = g_place[k];
    return g_places;
}
