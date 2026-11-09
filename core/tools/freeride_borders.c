/* freeride_borders.c -- w-frnowalls: scripted riders ridden off the course and past the borders of a level's Free Ride through the
 * PRODUCT session API, with FREE RIDE WITHOUT BORDERS off (0, the original) or on (1), to show what resets / stops them.
 *   freeride_borders <install root> <level> <location id> <mode 0|1> [event type [type id]] [frames per probe]
 * (event type default "FreeRide" 9; any other event type runs the same probes, e.g. a race to show the option does not touch it).
 * Probes (each starts with the player's own save spot: the spot is written into the player (+0x1140 position, +0x1150 orientation,
 * +0x1130 set) and UseResetPoint is tapped, the original's "back to spot"):
 *   off<k>   from the spawn, k = 0..3: heading spawn + k * 90 deg, throttle held, steered to the heading: off the course across the level;
 *   cb<k>    (w-reyesborders) a level with the Free Ride's cannonball spline (the CANNONBALL line: "<level>-cb.tdf07", RuleCannonball's band):
 *            across it at FRB_CB (default 4) nodes spread along it, from 150 ft on the level's side, riding straight out;
 *   edge<d>  d = +x / -x / +z / -z: on the line whose outer 360 ft of base terrain is the flattest, from FRB_EDGE_IN ft (default 300) inside
 *            the base terrain's edge, riding straight out over the edge.
 * Every frame: a reset (the player's reset timer +0x116c rising; reason +0x1170: 1 manual, 2 crash, 3 off track, 4 wrong way, 6 reset
 * zone, 7 out of bounds / fall net, 9 reset trigger), a JUMP (moved 20+ ft in a frame without the timer: the cannonball's put-back,
 * reason 5, has none; also the frame a timed reset places the rider), a stop (speed under 3 ft/s for 2 s without a wreck: a wall or a hill
 * it cannot climb, or a rider put back every frame), the distance past the terrain's edge. One PROBE line per probe and an END line with the
 * core's gate counters.
 * Also: zone<k> probes ride into the reset zones (TerrainMaterialAt == 7) nearest the base point; env FRB_TARGETS="name:x:z,..." rides into
 * given points (trigger volumes); FRB_DROP="name:x:y:z,..." drops the rider from a height (what he lands on); FRB_LAUNCH="name:x:y:z:vx:vy:vz,..."
 * (w-dfcollide) throws the rider from (x, y, z) at the velocity (ft/s; the simulation set like a remote rider's jump to a record, 004331d0)
 * and follows him 150 frames without input (what he hits in the air: the end point, how far along the throw he got); FRB_TRACE=1 prints the
 * input / reset / water state every 20 frames; FRB_PROBES="cb,edge" runs only those groups (exit, off, zone, cb, edge). The arguments after the mode are positional: [event type [type id]] [frames].
 * Build (static link with the product objects; it reads guest memory and calls core functions directly):
 *   C=core O=<out>; source core/tools/product_link.sh; rx_product_objs; rx_link_driver <out exe> core/tools/freeride_borders.c */
#include "rx_api.h"
#include "rx_mem.h"
#include "../game/free_ride_borders.h"
#include "../game/world_veh.h"
#include "../game/phys_terrain.h"
#include "../game/build_alloc.h"
#include "../game/bike_sim.h"
#include "../game/bike_remote.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rx_session* s;
static rx_addr player;
static int dik_of[256];
static rx_remap_table remap;
#define KB(col) (dik_of[remap.keyboard[(col)] & 255] & 255)

static rx_addr local_player(void) {
    const rx_addr g = U32(0, 0x00a946b8u);
    for (uint32_t i = 0; i < U8(g, 0x1541c) && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4u * i);
        if (slot < 16 && U32(g + 0x7460 + slot * 0x12a0, 0x10) == 0) return g + 0x7460 + slot * 0x12a0;
    }
    return 0;
}
static rx_addr terrain(void) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    return rx_world_find_by_controller(0x009e30d4);
}
static int step(const uint8_t* keys) {
    if (rx_session_set_input(s, keys, NULL, 0) != 0) return 1;
    return rx_session_advance_dt(s, 1.f / 60);
}
static void pose(float pos[3], float fwd[2], float* speed) {
    rx_bike_state b; memset(&b, 0, sizeof b); rx_session_bike(s, &b);
    float m[18 * 16]; const int nn = rx_session_bike_pose(s, m, 18);
    pos[0] = b.position[0][0]; pos[1] = b.position[0][1]; pos[2] = b.position[0][2];
    float hx = nn > 0 ? m[8] : 0.f, hz = nn > 0 ? m[10] : 1.f, l = sqrtf(hx * hx + hz * hz);
    if (l < 1e-3f) { hx = 0; hz = 1; l = 1; }
    fwd[0] = hx / l; fwd[1] = hz / l; *speed = b.speed;
}

/* the save spot at (x, ground + 4, z) facing (dx, dz), then UseResetPoint; the run continues until the reset has placed the vehicle */
static rx_addr scratch;
static float drop_y = -1e9f;   /* FRB_DROP: an explicit height for the next teleport (else the ground + 4 ft) */
static int teleport(float x, float z, float dx, float dz) {
    const rx_addr g = terrain();
    SF32(scratch, 0, x); SF32(scratch, 4, 0); SF32(scratch, 8, z); SF32(scratch, 12, 1);
    uint32_t edx = 0;
    float y = 0;
    if (g && phys_terrain_base_height_at(g, scratch, &edx)) y = F32(scratch, 4);
    else { float p[3], f[2], sp; pose(p, f, &sp); y = p[1]; }
    if (drop_y > -1e8f) { y = drop_y - 4.f; drop_y = -1e9f; }
    const float a = atan2f(dx, dz);   /* yaw quaternion (0, sin a/2, 0, cos a/2): its forward axis (2xz + 2wy, .., 1 - 2xx - 2yy) = (sin a, 0, cos a) */
    SF32(player, 0x1140, x); SF32(player, 0x1144, y + 4.f); SF32(player, 0x1148, z); SF32(player, 0x114c, 1.f);
    SF32(player, 0x1150, 0.f); SF32(player, 0x1154, sinf(a / 2)); SF32(player, 0x1158, 0.f); SF32(player, 0x115c, cosf(a / 2));
    SU32(player, 0x1130, 1);
    uint8_t keys[256] = {0};
    for (int i = 0; i < 3; i++) { keys[KB(13)] = 0x80; if (step(keys)) return 1; }
    memset(keys, 0, sizeof keys);
    for (int i = 0; i < 240; i++) {
        if (step(keys)) return 1;
        float p[3], f[2], sp; pose(p, f, &sp);
        if (!(F32(player, 0x116c) > 0.f) && fabsf(p[0] - x) < 15.f && fabsf(p[2] - z) < 15.f && i > 30) break;
    }
    return 0;
}

/* FRB_PROBES="cb,edge" (a comma list of exit, off, zone, cb, edge): only those probe groups (default all) */
static int want(const char* g) {
    const char* l = getenv("FRB_PROBES");
    if (!l || !*l) return 1;
    const size_t n = strlen(g);
    for (const char* p = l; (p = strstr(p, g)) != NULL; p += n)
        if ((p == l || p[-1] == ',') && (p[n] == 0 || p[n] == ',')) return 1;
    return 0;
}

static int steer_sign = 0;   /* +1: KB(2) turns toward +cross, found on the first probe */
static void probe(const char* name, float x, float z, float dx, float dz, int frames, const float box[4], float margin) {
    if (teleport(x, z, dx, dz)) { printf("PROBE %s teleport failed: %s\n", name, rx_session_last_error(s)); return; }
    float p0[3], f0[2], sp0; pose(p0, f0, &sp0);
    printf("PROBE %s start %.0f %.1f %.0f dir %.2f %.2f facing %.2f %.2f\n", name, p0[0], p0[1], p0[2], dx, dz, f0[0], f0[1]);
    float prior = F32(player, 0x116c), miny = p0[1], maxpast = -1e9f, ride = 0, last[3] = {p0[0], p0[1], p0[2]};
    int resets = 0, jumps = 0, stops = 0, slow = 0, wrecks = 0, wrecked_prev = 0, crossed_zone = -1, crossed_edge = -1, air = 0, maxair = 0;
    char reasons[256] = "";
    for (int i = 0; i < frames; i++) {
        float p[3], f[2], sp; pose(p, f, &sp);
        uint8_t keys[256] = {0};
        keys[KB(0)] = 0x80;
        const float cross = f[0] * dz - f[1] * dx, dot = f[0] * dx + f[1] * dz;
        if (dot < 0.97f || fabsf(cross) > 0.12f) {
            int want = cross > 0 ? 1 : -1;
            if (steer_sign == 0) steer_sign = 1;
            keys[(want * steer_sign) > 0 ? KB(2) : KB(3)] = 0x80;
        }
        if (getenv("FRB_TRACE") && i % 20 == 0) { rx_bike_state b; rx_session_bike(s, &b); printf("    T %d pos %.0f %.1f %.0f speed %.1f in %.2f %.2f %.2f %.2f timer %.2f fade %u keys %d %d %d water %u %u %.2f crash %.2f\n", i, p[0], p[1], p[2], sp, b.inputs[0], b.inputs[1], b.inputs[4], b.inputs[5], F32(player, 0x116c), U32(player, 0x11a0), keys[KB(0)], keys[KB(2)], keys[KB(3)], U32(U32(player, 0x1ac), 0x1310), U32(U32(player, 0x1ac), 0x1314), F32(U32(player, 0x1ac), 0x131c), F32(player, 0x1168)); }
        if (step(keys)) { printf("PROBE %s stopped at frame %d: %s\n", name, i, rx_session_last_error(s)); return; }
        float q[3], g2[2], sq; pose(q, g2, &sq);
        if (i == 40 && steer_sign == 1) {   /* learn the steering keys' sign once: the heading must have turned toward the target */
            const float c2 = g2[0] * dz - g2[1] * dx;
            if (fabsf(cross) > 0.2f && fabsf(c2) > fabsf(cross) + 0.05f) steer_sign = -1;
        }
        const float timer = F32(player, 0x116c);
        if (timer > prior + .1f) {
            resets++;
            const uint32_t why = U32(player, 0x1170);
            char t[48]; snprintf(t, sizeof t, "%s%u@%d", reasons[0] ? "," : "", why, i);
            strncat(reasons, t, sizeof reasons - strlen(reasons) - 1);
            printf("  RESET %s frame %d reason %u at %.0f %.1f %.0f (past edge %.0f ft)\n", name, i, why, q[0], q[1], q[2],
                   fmaxf(fmaxf(box[0] - q[0], q[0] - box[2]), fmaxf(box[1] - q[2], q[2] - box[3])));
        }
        prior = timer;
        rx_onboard_state o; memset(&o, 0, sizeof o); rx_session_onboard_state(s, &o);
        if (o.wrecked && !wrecked_prev) wrecks++;
        wrecked_prev = o.wrecked;
        const int grounded = o.contacts_valid && (o.front_grounded || o.rear_grounded);
        air = grounded ? 0 : air + 1; if (air > maxair) maxair = air;
        if (sq < 3.f && !o.wrecked && !(timer > 0.f)) { if (++slow == 120) { stops++; printf("  STOP %s frame %d at %.0f %.1f %.0f speed %.1f\n", name, i, q[0], q[1], q[2], sq); } }
        else slow = 0;
        const float past = fmaxf(fmaxf(box[0] - q[0], q[0] - box[2]), fmaxf(box[1] - q[2], q[2] - box[3]));
        if (past > maxpast) maxpast = past;
        if (crossed_zone < 0 && past > margin) crossed_zone = i;
        if (crossed_edge < 0 && past > 0.f) crossed_edge = i;
        if (q[1] < miny) miny = q[1];
        const float ddx = q[0] - last[0], ddz = q[2] - last[2];
        if (ddx * ddx + ddz * ddz < 400.f) ride += sqrtf(ddx * ddx + ddz * ddz);
        else if (!(timer > 0.f)) {   /* moved 20+ ft in one frame without the reset timer: a placement no reset reason counts */
            jumps++;
            printf("  JUMP %s frame %d from %.0f %.1f %.0f to %.0f %.1f %.0f reason %u fade %u\n", name, i, last[0], last[1], last[2], q[0], q[1], q[2],
                   U32(player, 0x1170), U32(player, 0x11a0));
        }
        last[0] = q[0]; last[1] = q[1]; last[2] = q[2];
    }
    float p[3], f[2], sp; pose(p, f, &sp);
    printf("PROBE %s end %.0f %.1f %.0f rode %.0f ft resets %d [%s] jumps %d stops %d wrecks %d into_zone %s at %d past_edge %s at %d max_past_edge %.0f ft min_y %.1f max_air %.1f s\n",
           name, p[0], p[1], p[2], ride, resets, reasons, jumps, stops, wrecks, crossed_zone >= 0 ? "yes" : "no", crossed_zone, crossed_edge >= 0 ? "yes" : "no", crossed_edge,
           maxpast, miny, maxair / 60.f);
}

int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: freeride_borders <install> <level> <location id> <mode 0|1> [event type [type id]] [frames]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    const int mode = atoi(argv[4]);
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argc > 5 ? argv[5] : "FreeRide");
    e.event_type_id = argc > 6 ? atoi(argv[6]) : 9;
    snprintf(e.location, sizeof e.location, "%s", argv[2]);
    e.location_id = atoi(argv[3]);
    e.track_id = 0; e.vehicle_class = -1; e.riders = 1; e.ai_difficulty = 0; e.duration = 0; e.duration_type[0] = 0; e.event_activation[0] = 0;
    if (strcmp(e.event_type, "FreeRide") != 0) { e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps"); }
    const int frames = argc > 7 ? atoi(argv[7]) : 900;
    if (rx_session_set_free_ride_borders(s, mode) != 0) { fprintf(stderr, "mode: %s\n", rx_session_last_error(s)); return 1; }
    if (rx_session_event_load(s, &e) != 0) { printf("LOAD failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f++ < 3000) if (rx_session_advance_dt(s, 1.f / 60)) break;
    for (int i = 0; i < 1500 && rx_session_get_state(s) != RX_SESSION_RACING; i++) if (rx_session_advance_dt(s, 1.f / 60)) break;
    if (rx_session_get_state(s) != RX_SESSION_RACING) { printf("not racing (state %d): %s\n", (int)rx_session_get_state(s), rx_session_last_error(s)); return 1; }
    memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d;
    player = local_player();
    scratch = rx_reflex_aligned_alloc(64);
    const rx_addr g = terrain(), act = U32(player, 0x94);
    if (!player || !g || !scratch) { printf("no player / terrain\n"); return 1; }
    const float box[4] = {F32(g, 0x44d8), F32(g, 0x44dc), F32(g, 0x44e0), F32(g, 0x44e4)}, margin = F32(g, 0x44f0);
    float p[3], fw[2], sp; pose(p, fw, &sp);
    printf("LEVEL %s event %s mode %d activity type %u mode %u terrain x %.0f .. %.0f z %.0f .. %.0f margin %.0f spawn %.0f %.1f %.0f facing %.2f %.2f\n",
           argv[2], e.event_type, mode, U32(act, 0x23c), U32(act, 0x238), box[0], box[2], box[1], box[3], margin, p[0], p[1], p[2], fw[0], fw[1]);
    /* the Free Ride boundary of the original: the world's cannonball spline ("<level>-cb.tdf07", world +0x21e10 while +0x22130; RuleCannonball
     * 0051a300 fades to white past its inner line and puts the rider back (00550920, reason 5, no reset timer) at its centre line) */
    rx_addr cbw = 0;
    {
        const rx_addr game = U32(0, 0x00a946b8), ctx = U32(0, 0x00a95844);
        if (U32(U32(ctx, 0x20), 0x34) == 1) cbw = U32(game, 0x5a54); else if (U32(U32(ctx, 0x1c), 0x34) == 1) cbw = U32(game, 0x20c8);
    }
    int cbn = 0; static float cbx[4096], cbz[4096];
    if (cbw && U32(cbw, 0x22130)) {
        const rx_addr blk = cbw + 0x21e10, secs = U32(blk, 0x130);
        const int32_t ns = (int32_t)U32(blk, 0x1d8);
        for (int32_t k = 0; k < ns && secs; k++)
            for (rx_addr n = U32(U32(secs, 4u * (uint32_t)k), 0); n && cbn < 4096; n = U32(n, 0x64)) { cbx[cbn] = F32(n, 0); cbz[cbn] = F32(n, 8); cbn++; }
        printf("CANNONBALL %d sections %d nodes x %.0f .. %.0f z %.0f .. %.0f\n", (int)ns, cbn, F32(blk, 0x300), F32(blk, 0x310), F32(blk, 0x308), F32(blk, 0x318));
    } else printf("CANNONBALL none\n");
    /* save the spawn as the first probe's start */
    float sx = p[0], sz = p[2];
    const float hx = fw[0], hz = fw[1];
    if (want("exit")) probe("exit", sx, sz, hx, hz, 360, box, margin);   /* off the spawn (the Free Ride maps' hovercraft / start pad) along its heading */
    { float q[3], f2[2], sq; pose(q, f2, &sq); if (hypotf(q[0] - sx, q[2] - sz) > 100.f) { sx = q[0]; sz = q[2]; } }
    for (int k = 0; k < 4 && want("off"); k++) {
        const float a = (float)k * 1.5707963f, dx = hx * cosf(a) - hz * sinf(a), dz = hx * sinf(a) + hz * cosf(a);
        char name[16]; snprintf(name, sizeof name, "off%d", k);
        probe(name, sx, sz, dx, dz, frames, box, margin);
    }
    /* cb<k>: across the cannonball spline at 4 nodes spread along it, from 150 ft on the level's side (toward the spline's centre of mass),
     * riding straight out (FRB_CB=n: n crossings) */
    if (cbn > 0 && want("cb")) {
        float mx = 0, mz = 0; for (int i = 0; i < cbn; i++) { mx += cbx[i]; mz += cbz[i]; } mx /= (float)cbn; mz /= (float)cbn;
        const int ncross = getenv("FRB_CB") ? atoi(getenv("FRB_CB")) : 4;
        for (int k = 0; k < ncross; k++) {
            const int i = (int)((long)cbn * (2 * k + 1) / (2 * ncross));
            float dx = cbx[i] - mx, dz = cbz[i] - mz; const float l = hypotf(dx, dz); if (l < 1.f) continue; dx /= l; dz /= l;
            char name[16]; snprintf(name, sizeof name, "cb%d", k);
            printf("CB %s node %d at %.0f %.0f\n", name, i, cbx[i], cbz[i]);
            probe(name, cbx[i] - 150.f * dx, cbz[i] - 150.f * dz, dx, dz, frames, box, margin);
        }
    }
    /* the reset zones (TerrainMaterialAt layer 0 == 7, the original's 0054f3e0 test) on a 20 ft grid: the count, and up to three probes
     * that ride into the zones nearest to the spawn (from 200 ft before the zone cell, on the spawn's side) */
    if (want("zone")) {
        int n7 = 0, picks = 0; float px[3], pz[3];
        static float cx[200000], cz[200000];
        for (float z = box[1] + 10.f; z < box[3]; z += 20.f)
            for (float x = box[0] + 10.f; x < box[2]; x += 20.f) {
                SF32(scratch, 0, x); SF32(scratch, 4, 0); SF32(scratch, 8, z); SF32(scratch, 12, 1);
                if (phys_terrain_material_at(g, scratch, 0, NULL, NULL) == 7 && n7 < 200000) { cx[n7] = x; cz[n7] = z; n7++; }
            }
        printf("ZONES %d reset-zone cells of 20 ft (%.0f %% of the terrain)\n", n7, 100.0 * n7 * 400.0 / ((box[2] - box[0]) * (box[3] - box[1])));
        while (picks < 3) {
            float bd = 1e18f; int bi = -1;
            for (int i = 0; i < n7; i++) {
                int near = 0; for (int j = 0; j < picks; j++) if (hypotf(cx[i] - px[j], cz[i] - pz[j]) < 400.f) near = 1;
                if (near) continue;
                const float d = hypotf(cx[i] - sx, cz[i] - sz);
                if (d > 250.f && d < bd) { bd = d; bi = i; }
            }
            if (bi < 0) break;
            px[picks] = cx[bi]; pz[picks] = cz[bi];
            const float dx = (cx[bi] - sx) / bd, dz = (cz[bi] - sz) / bd;
            char name[16]; snprintf(name, sizeof name, "zone%d", picks);
            printf("ZONE %s cell %.0f %.0f (%.0f ft from the base point)\n", name, cx[bi], cz[bi], bd);
            probe(name, cx[bi] - 200.f * dx, cz[bi] - 200.f * dz, dx, dz, frames, box, margin);
            picks++;
        }
    }
    /* FRB_TARGETS="name:x:z,..." (a trigger volume's centre, e.g.): ride into it from 200 ft away, from the base point's side */
    if (getenv("FRB_TARGETS")) {
        char buf[1024]; snprintf(buf, sizeof buf, "%s", getenv("FRB_TARGETS"));
        for (char* t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
            char nm[32]; float tx, tz;
            if (sscanf(t, "%31[^:]:%f:%f", nm, &tx, &tz) != 3) continue;
            const float d = hypotf(tx - sx, tz - sz), dx = d > 1 ? (tx - sx) / d : 1, dz = d > 1 ? (tz - sz) / d : 0;
            probe(nm, tx - 200.f * dx, tz - 200.f * dz, dx, dz, frames, box, margin);
        }
    }
    /* FRB_DROP="name:x:y:z,...": the save spot in the air at (x, y, z), no throttle: what the rider falls onto (min / end height) */
    if (getenv("FRB_DROP")) {
        char buf[1024]; snprintf(buf, sizeof buf, "%s", getenv("FRB_DROP"));
        for (char* t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
            char nm[32]; float tx, ty, tz;
            if (sscanf(t, "%31[^:]:%f:%f:%f", nm, &tx, &ty, &tz) != 4) continue;
            drop_y = ty;
            if (teleport(tx, tz, 0, 1)) continue;
            float q[3], f2[2], sq, low = 1e9f; pose(q, f2, &sq);
            printf("DROP %s start %.0f %.1f %.0f\n", nm, q[0], q[1], q[2]);
            uint8_t keys[256] = {0};
            for (int i = 0; i < 240; i++) { if (step(keys)) break; pose(q, f2, &sq); if (q[1] < low) low = q[1]; if (i % 30 == 0) printf("  DROP %s frame %d y %.1f speed %.1f\n", nm, i, q[1], sq); }
            printf("DROP %s end %.0f %.1f %.0f lowest %.1f\n", nm, q[0], q[1], q[2], low);
        }
    }
    /* FRB_LAUNCH="name:x:y:z:vx:vy:vz,...": the rider put at (x, y, z) (the save spot, then the simulation's pose and every node's velocity
     * set from a record like a remote rider's jump 004331d0, angular velocity zero), no input: where the throw ends and how far along it
     * (the distance along the velocity's direction) he got before something stopped him */
    if (getenv("FRB_LAUNCH")) {
        char buf[1024]; snprintf(buf, sizeof buf, "%s", getenv("FRB_LAUNCH"));
        const rx_addr rec = rx_reflex_aligned_alloc(0x80);
        for (char* t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
            char nm[32]; float tx, ty, tz, vx, vy, vz;
            if (sscanf(t, "%31[^:]:%f:%f:%f:%f:%f:%f", nm, &tx, &ty, &tz, &vx, &vy, &vz) != 7) continue;
            drop_y = ty;
            if (teleport(tx, tz, vx, vz)) continue;
            const rx_addr sim = U32(U32(player, 0x1ac), 0x16d4);
            float q[3], f2[2], sq; pose(q, f2, &sq);
            bike_get_replay_state(sim, rec);
            SF32(rec, 0, F32(rec, 0) + tx - q[0]); SF32(rec, 4, F32(rec, 4) + ty - q[1]); SF32(rec, 8, F32(rec, 8) + tz - q[2]);
            SF32(rec, 0x20, vx); SF32(rec, 0x24, vy); SF32(rec, 0x28, vz); SF32(rec, 0x2c, 0.f);
            for (uint32_t k = 0x30; k < 0x40; k += 4) SF32(rec, k, 0.f);
            bike_remote_jump_to_record(rec, sim);
            const float vl = sqrtf(vx * vx + vy * vy + vz * vz), ux = vx / vl, uy = vy / vl, uz = vz / vl;
            pose(q, f2, &sq);
            const float s0[3] = {q[0], q[1], q[2]};
            printf("LAUNCH %s start %.1f %.1f %.1f velocity %.0f %.0f %.0f\n", nm, q[0], q[1], q[2], vx, vy, vz);
            uint8_t keys[256] = {0};
            float far = 0.f; int wrecked = 0;
            for (int i = 0; i < 150; i++) {
                if (step(keys)) break;
                pose(q, f2, &sq);
                const float along = (q[0] - s0[0]) * ux + (q[1] - s0[1]) * uy + (q[2] - s0[2]) * uz;
                if (along > far) far = along;
                rx_onboard_state o; memset(&o, 0, sizeof o); rx_session_onboard_state(s, &o); if (o.wrecked) wrecked = 1;
                if (i % 10 == 0) printf("  LAUNCH %s frame %d at %.1f %.1f %.1f speed %.1f along %.1f\n", nm, i, q[0], q[1], q[2], sq, along);
            }
            printf("LAUNCH %s end %.1f %.1f %.1f furthest along the throw %.1f ft wrecked %d\n", nm, q[0], q[1], q[2], far, wrecked);
        }
    }
    /* each edge: the line (every 40 ft along it) whose outer 360 ft of base terrain is the flattest (the steepest 20 ft step the
     * smallest), so the rider can reach the edge on his wheels; start 300 ft inside it */
    static const char* en[4] = {"edge+x", "edge-x", "edge+z", "edge-z"};
    for (int k = 0; k < 4 && want("edge"); k++) {
        const float ox = k == 0 ? 1.f : k == 1 ? -1.f : 0.f, oz = k == 2 ? 1.f : k == 3 ? -1.f : 0.f;
        const float lo = k < 2 ? box[1] : box[0], hi = k < 2 ? box[3] : box[2];
        float best = 1e9f, bt = 0.f;
        for (float t = lo + 200.f; t <= hi - 200.f; t += 40.f) {
            float worst = 0.f, prev = 0.f;
            for (int d = 0; d <= 18; d++) {
                const float in = d < 18 ? 360.f - 20.f * (float)d : 1.f;   /* from 360 ft inside out to the edge (1 ft inside it: the height query fails on the edge itself) */
                const float x = k < 2 ? (k == 0 ? box[2] - in : box[0] + in) : t, z = k < 2 ? t : (k == 2 ? box[3] - in : box[1] + in);
                SF32(scratch, 0, x); SF32(scratch, 4, 0); SF32(scratch, 8, z); SF32(scratch, 12, 1);
                uint32_t edx = 0; const int ok = phys_terrain_base_height_at(g, scratch, &edx);
                const float h = ok ? F32(scratch, 4) : prev;
                if (!ok && d == 0) { worst = 1e9f; break; }
                if (d > 0 && fabsf(h - prev) > worst) worst = fabsf(h - prev);
                prev = h;
            }
            if (worst < best) { best = worst; bt = t; }
        }
        const float in = getenv("FRB_EDGE_IN") ? (float)atof(getenv("FRB_EDGE_IN")) : 300.f;
        const float x = k < 2 ? (k == 0 ? box[2] - in : box[0] + in) : bt, z = k < 2 ? bt : (k == 2 ? box[3] - in : box[1] + in);
        printf("EDGE %s line %.0f: steepest 20 ft step of the outer 360 ft %.1f ft\n", en[k], bt, best);
        probe(en[k], x, z, ox, oz, frames, box, margin);
    }
    printf("END %s mode %d gates: zone frames skipped %u, triggers skipped %u kept %u, edge frames skipped %u, fall-net resets %u, cannonball frames skipped %u, state %d\n", argv[2], mode,
           rx_frb_stats_value.zone_skipped, rx_frb_stats_value.trigger_skipped, rx_frb_stats_value.trigger_kept, rx_frb_stats_value.edge_skipped,
           rx_frb_stats_value.fall_resets, rx_frb_stats_value.cannonball_skipped, (int)rx_session_get_state(s));
    rx_session_destroy(s);
    return 0;
}
