/* practice_session.inc -- PRACTICE, the LAP LOG and the local vehicle's physics class (port additions, rx_api.h; game/practice.h,
 * core/plan/LEADERBOARDS.md 3.3), included by rx_session.c (it uses session_ok, session_error, local_player and race_driver_for). */
#include "../game/practice.h"

int rx_session_set_practice(struct rx_session* s, int32_t on) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (s->spec_loaded || s->career_phase || s->level) {   /* refused, the session stays usable (the running event keeps its own rule) */
        snprintf(s->error, sizeof s->error, "practice is set before the event loads (exhibition lap races only)");
        return 2;
    }
    if ((on != 0) != rx_practice_on())
        fprintf(stderr, "rxcore: PRACTICE %s\n", on ? "on: an exhibition lap race of this session never finishes (NumLaps 0x7fffffff, the original's Practice rule)" : "off");
    rx_practice_set(on != 0);
    return 0;
}
int32_t rx_session_practice(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? (int32_t)rx_practice_on() : 0; }

int32_t rx_session_lap_log(struct rx_session* s, uint32_t after_serial, rx_lap_record* out, int32_t max) {
    if (!session_ok(s) || max < 0 || (max > 0 && !out)) return -1;
    rx_lap_note n[RX_LAP_LOG_RING];
    const int count = rx_lap_log_read(after_serial, n, max < RX_LAP_LOG_RING ? max : RX_LAP_LOG_RING);
    for (int i = 0; i < count; i++) {
        rx_lap_record* r = &out[i];
        r->serial = n[i].serial; r->flying = n[i].flying; r->time = n[i].time; r->exact = n[i].exact; r->penalty = n[i].penalty;
        r->off_track = n[i].off_track; r->race_time = n[i].race_time; r->resets = n[i].resets; r->wrong_way = n[i].wrong_way; r->mods = n[i].mods;
        r->markers = n[i].markers;
    }
    return count;
}

int32_t rx_session_local_phys_class(const struct rx_session* s) {
    if (!s || s->magic != RX_SESSION_MAGIC) return -1;
    const rx_addr p = local_player();
    const rx_addr v = p ? U32(p, 0x1ac) : 0;
    return v ? rx_ps_class_of_sim(U32(v, 0x234)) : -1;
}

/* the local human's driver in a lap race (rx_practice_lap_type: National, Supercross, Terracross, UTV Moto, Powercross; their drivers'
 * +0xa8 / +0xac are the race rules rx_session_hud reads), else 0 */
static rx_addr lap_log_driver(void) {
    const rx_addr p = local_player();
    rx_addr a = 0;
    const rx_addr d = p ? race_driver_for(p, &a) : 0;
    return d && a && rx_practice_lap_type(U32(a, 0x23c)) && !waypoint_driver_layout(d) ? d : 0;
}
/* before the frame: whose crossings the log takes (the local human's race driver) */
static void lap_log_before_frame(void) { rx_lap_log_track(lap_log_driver()); }
/* after the frame: the open lap's accumulators from the frame's state, read as rx_session_hud reads it */
static void lap_log_after_frame(void) {
    const rx_addr d = rx_lap_log_driver;
    if (!d || d != lap_log_driver()) return;
    const rx_addr p = U32(d, 0x74), a = U32(p, 0x94), v = U32(p, 0x1ac);
    const int racing = a && I32(a, 0x21c) == 2;
    const rx_addr offtrack = U32(d, 0xac), backwards = U32(d, 0xa8);
    const int on_track = offtrack ? U32(offtrack, 0x10) != 0 : 1;
    const int wrong_way = backwards ? U32(backwards, 0xc) != 0 : 0;
    const int resetting = F32(p, 0x116c) > 0.0f;
    uint32_t mods = (uint32_t)rx_ps_active() & (RX_LAP_MOD_PHYS_CONFIG | RX_LAP_MOD_PHYS_RESTORED | RX_LAP_MOD_PHYS_TAINT);
    if (rx_fa_mask_value || rx_fa_level_value != 1.0f || rx_fa_snap_value != 1.0f || rx_fa_looseness_value != 0.0f) mods |= RX_LAP_MOD_AIR_HELPERS;
    if (rx_ms_records_off()) mods |= RX_LAP_MOD_MAP_SCALE;   /* (BASE SCALE: not at the track's default scale) */
    float pos[3] = {0, 0, 0};
    if (v) { pos[0] = F32(v, 0xc0); pos[1] = F32(v, 0xc4); pos[2] = F32(v, 0xc8); }
    rx_lap_log_frame(racing, on_track, wrong_way, resetting, mods, v ? pos : NULL);
    if (racing) rx_practice_marker_settle(d);   /* PRACTICE's SPAWN MARKER (w-lb-markers): the driver placed at the marker once the respawn ran */
}
