/* rpm_gauge_trace.c (w-rpmgauge) -- the MODERN HUD's rev gauge against the engine's real speed, headless through the product session.
 *   rpm_gauge_trace <install root> [vehicle class (2 = MX)] [level (NAT_Concord)] [location id (7)] [race frames (1500)]
 * Loads a National with the given VehicleClass and drives the local rider with the game's own default keys (rx_remap_defaults: W
 * throttle, left Shift clutch, A / D steer: the scan code of each column's key id), steering toward the race line (rx_session_race_line).
 * The script, in race frames:
 *     0 ..  59   nothing                      (idle at a standstill)
 *    60 .. 179   clutch + throttle            (revs with the clutch in at a standstill: the limiter)
 *   180 .. 299   clutch only                  (the engine falls back toward idle)
 *   300 ..       throttle (+ the line)        (the launch, the automatic upshifts, the jumps: revs in the air)
 * Every frame it reads the local rider's engine from rx_session_vehicle_audio (the same snapshot the engine sound follows; the rider
 * whose vehicle is rx_session_vehicle_info(-1).vehicle) and computes both gauges:
 *   old   clamp01(mph / 90)                                     (the speed ring before w-rpmgauge, REV GAUGE SPEED: ReflexHudView GaugeMax 90)
 *   idle  clamp01((rpm - idle) / (max - idle))                  (w-rpmgauge's ring, from idle: riding lives in its top third)
 *   gauge clamp01((rpm - floor) / (max - floor)),               (ReflexHudModel.RpmGauge since w-revgauge: the useful band, empty at idle,
 *         floor = max(idle, 0.3 x max)                           full at the limiter; 0 while wrecked / drowned: the rpm is stale then)
 *   needle  the gauge through the HUD's needle (ReflexHudView.UpdateGauge: two first-order stages, the first rising with 100 ms and
 *         falling with 50 ms, the second 50 ms; a step reads 63 % in 150 ms rising, 100 ms falling), and w-rpmgauge's needle (the idle
 *         band through one 50 ms stage) for comparison. NEEDLE (the summary) per vehicle while riding: "jitter" the needle's rms distance
 *         from its own 9-frame centred mean (the flicker a player sees), "travel" how far it moves per second (ring lengths)
 *   light   the shift light: 1 AMBER from a needle of 0.85 (the red stretch) down to 0.81, 2 the limiter (the gauge >= 0.995, held 0.25 s)
 * Prints one line every 10 frames ("T"), and one line at every gear change ("SHIFT", the rpm a frame before and after), take-off
 * ("AIR+", "AIR-" with the peak rpm in the air) and limiter entry ("LIMIT"). Ends with a summary and the checks:
 *   clutch_rev   the gauge reached >= 0.99 during the clutch + throttle phase at < 3 mph
 *   clutch_fall  the gauge fell below 0.5 by the end of the clutch-only phase (from 1.0: the engine's own drag, 2 s)
 *   upshift      at least one upshift, the gauge lower right after each than right before (all of them)
 *   limiter      the rpm reached max_rpm (|rpm - max| < 1) somewhere
 *   air_rev      at least one airborne stretch of >= 10 frames whose peak gauge is >= the take-off gauge + 0.1 or >= 0.99
 * env RPM_CSV=<file>: every race frame as "frame,rpm,idle,max,gauge,needle,gear,air,dead,mph" (to try other needle constants offline).
 * env EQUIP="owner,id": before the event, MXTableHelper:EquipVehicle on the controller-0 Player List row (bike_parity_probe's PROBE_EQUIP
 * test hook), e.g. EQUIP=4,44 an AI rider's ATV (AIVehicle 44): the local rider rides it; without it the default profile's MX bike.
 * Exit 0 all checks pass, 1 a check failed / no race, 2 usage / API failure. Build: tools/rpm_gauge_trace.sh <out>. */
#include "rx_api.h"
#include "rx_vehicle_audio.h"
#include "rx_mem.h"
#include "build_vehicle_select.h"
#include "race_activation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float clamp01(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
/* ReflexHudModel.RpmFloor / RpmGauge (keep the two in step): the ring spans [floor, limiter], floor = max(idle, 0.3 x limiter) (at most
 * 0.9 x limiter); a missing / invalid idle counts as 0 */
#define RPM_FLOOR_FRAC 0.3f
static float rpm_floor(float idle, float max) {
    if (!(idle >= 0.f) || idle >= max) idle = 0.f;
    float lo = idle > RPM_FLOOR_FRAC * max ? idle : RPM_FLOOR_FRAC * max;
    return lo < 0.9f * max ? lo : 0.9f * max;
}
static float rpm_gauge(float rpm, float idle, float max) {
    if (!(max > 0.f)) return 0.f;
    const float lo = rpm_floor(idle, max);
    return clamp01((rpm - lo) / (max - lo));
}
/* w-rpmgauge's fill (from idle), for the comparison */
static float rpm_gauge_idle(float rpm, float idle, float max) {
    if (!(max > 0.f)) return 0.f;
    if (!(idle >= 0.f) || idle >= max) idle = 0.f;
    return clamp01((rpm - idle) / (max - idle));
}
/* ReflexHudView.UpdateGauge's needle and shift light (keep in step): NeedleRise / NeedleFall / NeedleSmooth (s), ShiftFrom / ShiftHyst,
 * Limiter, LimiterHold (s) */
#define NEEDLE_RISE 0.10f
#define NEEDLE_FALL 0.05f
#define NEEDLE_SMOOTH 0.05f
#define SHIFT_FROM 0.85f
#define SHIFT_HYST 0.04f
#define LIMITER 0.995f
#define LIMIT_HOLD 0.25f
static float stage(float prev, float target, float rise, float fall, float dt) {
    if (prev < 0.f) return target;
    return prev + (target - prev) * (1.f - expf(-dt / (target > prev ? rise : fall)));
}
typedef struct { float a, b; } needle2;
static float needle_step(needle2* n, float target, float dt) {
    if (n->a < 0.f) { n->a = n->b = target; return target; }
    n->a = stage(n->a, target, NEEDLE_RISE, NEEDLE_FALL, dt);
    n->b = stage(n->b, n->a, NEEDLE_SMOOTH, NEEDLE_SMOOTH, dt);
    return n->b;
}
/* the jitter: a 9-frame window centred 4 frames back (rms of the centre's distance from the window's mean), and the travel */
typedef struct { float w[9]; int n; double sq, travel; int count; } jitter;
static void jitter_step(jitter* j, float v, int counted) {
    if (j->n > 0 && counted) j->travel += fabsf(v - j->w[(j->n - 1) % 9]);
    j->w[j->n % 9] = v; j->n++;
    if (j->n < 9 || !counted) return;
    float m = 0.f; for (int k = 0; k < 9; k++) m += j->w[k];
    m /= 9.f; const float c = j->w[(j->n - 5) % 9] - m; j->sq += c * c; j->count++;
}
static double jitter_rms(const jitter* j) { return j->count ? sqrt(j->sq / j->count) : 0.0; }

static int local_engine(rx_session* s, rx_vehicle_audio_state* va, rx_vehicle_audio_rider** out) {
    rx_vehicle_info vi; memset(&vi, 0, sizeof vi);
    if (rx_session_vehicle_info(s, -1, &vi)) return 0;
    memset(va, 0, sizeof *va); va->size = sizeof *va; va->version = RX_VEHICLE_AUDIO_VERSION;
    if (rx_session_vehicle_audio(s, va) || !va->valid) return 0;
    for (int i = 0; i < va->rider_count; i++) if (va->riders[i].vehicle == vi.vehicle) { *out = &va->riders[i]; return 1; }
    return 0;
}

static int scan_of(const rx_remap_table* r, int32_t id) { for (int k = 1; k < 256; k++) if (r->scan[k] == id) return k; return -1; }

/* +Z of the frame body's quaternion (x, y, z, w), on the ground plane */
static void forward_xz(const float q[4], float* fx, float* fz) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    *fx = 2.f * (x * z + w * y); *fz = 1.f - 2.f * (x * x + y * y);
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: rpm_gauge_trace <install root> [class] [level] [location id] [race frames]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    const int cls = argc > 2 ? atoi(argv[2]) : 2, frames = argc > 5 ? atoi(argv[5]) : 1500;
    rx_remap_table remap; if (rx_remap_defaults(&remap)) { fprintf(stderr, "remap defaults\n"); return 2; }
    /* the remap columns hold the game's key ids; scan[] maps each DirectInput scan code to its key id */
    const int K_THR = scan_of(&remap, remap.keyboard[0]), K_LEFT = scan_of(&remap, remap.keyboard[2]), K_RIGHT = scan_of(&remap, remap.keyboard[3]),
              K_CLUTCH = scan_of(&remap, remap.keyboard[5]);
    if (K_THR < 0 || K_LEFT < 0 || K_RIGHT < 0 || K_CLUTCH < 0) { fprintf(stderr, "a default key has no scan code\n"); return 2; }
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 2; }
    rx_event_spec e; rx_session_event_default(&e);
    if (argc > 3) snprintf(e.location, sizeof e.location, "%s", argv[3]);
    if (argc > 4) e.location_id = atoi(argv[4]);
    e.vehicle_class = cls; e.riders = 1; e.ai_difficulty = 0; e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    if (getenv("EQUIP")) {
        int owner = -1, id = -1; if (sscanf(getenv("EQUIP"), "%d,%d", &owner, &id) != 2) return 2;
        rx_ensure_local_players(0, 0);
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 8); const uint32_t row = U32(rx_player_table_row(0), 0); rx_set_sp(sp);
        rx_set_sp(sp - 12); rx_table_equip_vehicle((uint32_t)owner, row, (uint32_t)id); rx_set_sp(sp);
        printf("EQUIP row %u vehicle %d owner %d\n", row, id, owner);
    }
    printf("EVENT %s %s class %d keys throttle %02x clutch %02x left %02x right %02x\n", e.event_type, e.location, cls, K_THR, K_CLUTCH, K_LEFT, K_RIGHT);
    if (rx_session_event_load(s, &e)) { fprintf(stderr, "event load: %s\n", rx_session_last_error(s)); rx_session_destroy(s); return 2; }

    static rx_vehicle_audio_state va;
    FILE* csv = getenv("RPM_CSV") ? fopen(getenv("RPM_CSV"), "w") : NULL;
    static float line[3 * 64];
    int racing = -1, gear_prev = -99, air_run = 0, ups = 0, ups_ok = 0, at_limit = 0, air_ok = 0, air_long = 0;
    float prev_g = 0.f, prev_rpm = 0.f, takeoff_g = 0.f, air_peak = 0.f, clutch_peak = 0.f, clutch_end = 1.f, limit_hits = 0;
    int prev_air = 0, cls_kind = -1; float idle0 = 0.f, max0 = 0.f;
    float nd = 0.f, nd_old = -1.f, prev_nd = 0.f, air_peak_nd = 0.f, limit_t = 0.f; int amber = 0, light_prev = 0, light_flips = 0, amber_frames = 0, red_frames = 0, ride_frames = 0;
    needle2 hud = { -1.f, -1.f };
    /* locals, not statics: rx_session_destroy resets the static data of the whole (statically linked) program */
    jitter j_raw, j_old, j_new; memset(&j_raw, 0, sizeof j_raw); memset(&j_old, 0, sizeof j_old); memset(&j_new, 0, sizeof j_new);
    int sh_left = 0, sh_from = 0, sh_to = 0; float sh_before = 0.f, sh_min = 0.f, sh_before_old = 0.f, sh_min_old = 0.f;
    for (int i = 0; i < 20000 && racing < frames; i++) {
        const rx_session_state st = rx_session_get_state(s);
        if (st == RX_SESSION_ERROR) { fprintf(stderr, "error: %s\n", rx_session_last_error(s)); break; }
        if (st == RX_SESSION_FINISHED) { printf("finished at race frame %d\n", racing); break; }
        if (st == RX_SESSION_RACING) racing++;
        uint8_t keys[256] = {0};
        if (racing >= 60 && racing < 180) { keys[K_CLUTCH] = 0x80; keys[K_THR] = 0x80; }
        else if (racing >= 180 && racing < 300) keys[K_CLUTCH] = 0x80;
        else if (racing >= 300) {
            keys[K_THR] = 0x80;
            rx_bike_state b; int n = rx_session_race_line(s, line, 64);
            if (n > 0 && rx_session_bike(s, &b) == 0) {
                const float px = b.position[0][0], pz = b.position[0][2];
                int k = 0; while (k < n - 1 && hypotf(line[3 * k] - px, line[3 * k + 2] - pz) < 40.f) k++;
                float fx, fz; forward_xz(b.orientation[0], &fx, &fz);
                const float tx = line[3 * k] - px, tz = line[3 * k + 2] - pz;
                const float cross = fx * tz - fz * tx, dot = fx * tx + fz * tz, err = atan2f(cross, dot);
                /* a positive error (the target to the left of +Z in x / z) is steer left; LEFT_SIGN=-1 flips it */
                static int left_sign = 0; if (!left_sign) left_sign = getenv("LEFT_SIGN") ? atoi(getenv("LEFT_SIGN")) : 1;
                if (fabsf(err) > 0.08f) keys[(err * left_sign > 0.f) ? K_LEFT : K_RIGHT] = 0x80;
            }
        }
        if (rx_session_set_input(s, keys, NULL, 0) || rx_session_advance_dt(s, 1.f / 60)) { fprintf(stderr, "advance: %s\n", rx_session_last_error(s)); rx_session_destroy(s); return 2; }
        if (racing < 0) continue;
        rx_vehicle_audio_rider* r = NULL;
        if (!local_engine(s, &va, &r)) continue;
        rx_hud_state h; memset(&h, 0, sizeof h);
        const float mph = rx_session_hud(s, &h) == 0 && h.valid ? h.speed * 0.68182f : r->speed * 0.68182f;
        const int dead = r->wrecked || r->drowned;
        const float g_old = clamp01(mph / 90.f), g = dead ? 0.f : rpm_gauge(r->rpm, r->idle_rpm, r->max_rpm),
                    g_idle = dead ? 0.f : rpm_gauge_idle(r->rpm, r->idle_rpm, r->max_rpm);
        if (cls_kind < 0) { cls_kind = r->vehicle_class; idle0 = r->idle_rpm; max0 = r->max_rpm; printf("ENGINE class %d idle %.0f max %.0f floor %.0f\n", cls_kind, idle0, max0, rpm_floor(idle0, max0)); }
        const int air = r->airborne != 0, lim = fabsf(r->rpm - r->max_rpm) < 1.f;
        /* the HUD's needles (one frame of 1/60 s) and the shift light */
        nd = needle_step(&hud, g, 1.f / 60); nd_old = stage(nd_old, g_idle, 0.05f, 0.05f, 1.f / 60);
        limit_t = g >= LIMITER ? LIMIT_HOLD : limit_t > 1.f / 60 ? limit_t - 1.f / 60 : 0.f;
        amber = nd >= SHIFT_FROM || (light_prev >= 1 && nd >= SHIFT_FROM - SHIFT_HYST);
        const int light = limit_t > 0.f ? 2 : amber ? 1 : 0;
        const int riding = racing >= 300 && !dead;
        jitter_step(&j_raw, g, riding); jitter_step(&j_old, nd_old, riding); jitter_step(&j_new, nd, riding);
        if (racing >= 300) {
            ride_frames++;
            if (light != light_prev) light_flips++;
            if (light == 1) amber_frames++; else if (light == 2) red_frames++;
        }
        light_prev = light;
        if (csv) fprintf(csv, "%d,%.1f,%.1f,%.1f,%.4f,%.4f,%d,%d,%d,%.2f\n", racing, r->rpm, r->idle_rpm, r->max_rpm, g, nd, r->gear, air, dead, mph);
        if (sh_left > 0) {
            if (nd < sh_min) sh_min = nd;
            if (nd_old < sh_min_old) sh_min_old = nd_old;
            if (--sh_left == 0) printf("SHIFTN %d -> %d needle %.3f -> %.3f within 200 ms (w-rpmgauge needle %.3f -> %.3f)\n", sh_from, sh_to, sh_before, sh_min, sh_before_old, sh_min_old);
        }
        const char* phase = racing < 60 ? "idle" : racing < 180 ? "clutch+thr" : racing < 300 ? "clutch" : "ride";
        if (racing >= 60 && racing < 180 && mph < 3.f && g > clutch_peak) clutch_peak = g;
        if (racing == 299) clutch_end = g;
        if (lim && !at_limit) printf("LIMIT f %4d %-10s rpm %6.0f gauge %.3f idle %.3f needle %.3f mph %5.1f gear %d air %d clutch %u\n", racing, phase, r->rpm, g, g_idle, nd, mph, r->gear, air, r->clutch);
        if (lim) limit_hits++;
        at_limit = lim;
        if (gear_prev != -99 && r->gear != gear_prev) {
            const int up = r->gear > gear_prev;
            printf("SHIFT f %4d %d -> %d rpm %6.0f -> %6.0f gauge %.3f -> %.3f idle %.3f -> %.3f mph %5.1f old %.3f\n", racing, gear_prev, r->gear, prev_rpm, r->rpm, prev_g, g,
                   rpm_gauge_idle(prev_rpm, r->idle_rpm, r->max_rpm), g_idle, mph, g_old);
            if (up) { ups++; if (g < prev_g) ups_ok++; }
            sh_left = 12; sh_from = gear_prev; sh_to = r->gear; sh_before = prev_nd; sh_min = nd; sh_before_old = sh_min_old = nd_old;
        }
        if (air && !prev_air) { takeoff_g = g; air_peak = g; air_peak_nd = nd; air_run = 0; printf("AIR+ f %4d rpm %6.0f gauge %.3f idle %.3f needle %.3f mph %5.1f gear %d\n", racing, r->rpm, g, g_idle, nd, mph, r->gear); }
        if (air) { air_run++; if (g > air_peak) air_peak = g; if (nd > air_peak_nd) air_peak_nd = nd; }
        if (!air && prev_air) {
            printf("AIR- f %4d frames %3d take-off gauge %.3f peak %.3f (needle peak %.3f) landing rpm %6.0f gauge %.3f idle %.3f\n", racing, air_run, takeoff_g, air_peak, air_peak_nd, r->rpm, g, g_idle);
            if (air_run >= 10) { air_long++; if (air_peak >= takeoff_g + 0.1f || air_peak >= 0.99f) air_ok++; }
        }
        if (racing % 10 == 0)
            printf("T f %4d %-10s mph %5.1f gear %d rpm %6.0f idle %5.0f max %6.0f gauge %.3f needle %.3f light %d | idle %.3f needle %.3f | old %.3f thr %.2f clutch %u air %d load %.2f wreck %u\n",
                   racing, phase, mph, r->gear, r->rpm, r->idle_rpm, r->max_rpm, g, nd, light, g_idle, nd_old, g_old, r->throttle, r->clutch, air, r->load, r->wrecked);
        prev_air = air; prev_g = g; prev_rpm = r->rpm; gear_prev = r->gear; prev_nd = nd;
    }
    rx_session_destroy(s);
    if (csv) fclose(csv);
    if (racing < 300) { printf("FAIL no race (%d race frames)\n", racing); return 1; }
    const int c1 = clutch_peak >= 0.99f, c2 = clutch_end < 0.5f, c3 = ups > 0 && ups_ok == ups, c4 = limit_hits > 0, c5 = air_ok > 0;
    printf("SUMMARY class %d idle %.0f max %.0f | clutch_rev %s (peak %.3f) | clutch_fall %s (%.3f) | upshift %s (%d/%d lower) | limiter %s (%d frames) | air_rev %s (%d of %d long air stretches)\n",
           cls_kind, idle0, max0, c1 ? "PASS" : "FAIL", clutch_peak, c2 ? "PASS" : "FAIL", clutch_end, c3 ? "PASS" : "FAIL", ups_ok, ups,
           c4 ? "PASS" : "FAIL", (int)limit_hits, c5 ? "PASS" : "FAIL", air_ok, air_long);
    if (j_new.count > 0) {
        const double secs = j_new.count / 60.0;
        printf("NEEDLE riding %.1f s | jitter rms: the raw gauge %.4f, w-rpmgauge's needle %.4f, the needle %.4f (%+.0f %%) | travel per s: %.2f, %.2f, %.2f"
               " | light: amber %d frames, limiter %d frames, %d changes (%.2f / s)\n", secs, jitter_rms(&j_raw), jitter_rms(&j_old), jitter_rms(&j_new),
               jitter_rms(&j_old) > 0 ? (jitter_rms(&j_new) / jitter_rms(&j_old) - 1.0) * 100.0 : 0.0, j_raw.travel / secs, j_old.travel / secs, j_new.travel / secs,
               amber_frames, red_frames, light_flips, light_flips / (ride_frames / 60.0));
    }
    return c1 && c2 && c3 && c4 && c5 ? 0 : 1;
}
