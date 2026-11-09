/* alive_gate_test.c (w-alivegates) -- the start gate of a race, headless, through the PRODUCT session API (core/plan/ALIVE_RACING.md "Start
 * gate"): whether the gate holds the riders until the drop, when it drops against the race start, and how the riders leave.
 *   alive_gate_test <install root> <type> <type id> <level> <location id> [frames] [map scale] [base scale]
 *     e.g. alive_gate_test builds/alive/combined/map-a8-gates National 10 NAT_Farm -1
 *          alive_gate_test "game files/Archive" National 10 NAT_Concord 50
 * 12 riders (11 AI at 95, MX class 2, 3 laps); the player holds full throttle from the first frame (pad 0), steering straight.
 * Per frame it follows the race state (Countdown 1, InProgress 2), the gate's bar (an Alive gate: the "DropGate" node of the ScenarioAnimEntity
 * TO_StartingGate, from the ambient export; a Reflex gate: the DropGate entity's matrix) as its angle from the rest pose, the port's gate
 * collision entity (Alive: "TO_StartingGate_Bar", a DropGate) and every rider's distance past his gate slot along the gate's forward axis.
 * Lines: the gate objects found, a line per change of race state or bar angle, the riders' furthest point before GO, and a RESULT line:
 *   RESULT <level> held <0|1> max_before_go <ft> drop_frame <f> go_frame <f> drop_minus_go <frames> bar_final_deg <deg> moved_after <n>/<n>
 *          hidden_before_go <frames the gate was not shown before GO>
 * held = every rider stayed behind the bar line (the slot + the gap to the bar) until GO; moved_after = riders 30 ft past the gate 6 s after GO.
 * The race's own start holds the riders until GO (input), so the collision is shown by holding the gate closed after GO:
 *   RX_GATE_HOLD=<frames>  the bar's collision kept on for that many frames after GO (rx_alive_gate_set_collision): a HOLD line gives every
 *                          rider's furthest point along the gate's axis in that time (the bar plate's back edge is ~4.2 ft from a slot)
 *   RX_GATE_OPEN=1         the bar's collision off from the start (the same HOLD line: what the riders do without it)
 * The activity audio cues (rx_session_sound) are listed as they play (Reflex's ACTIVITY_NATIONALS / SUPERCROSS DROP_GATE at GO).
 * Static link with the product objects:
 *   C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <exe> core/tools/alive_gate_test.c */
#include "rx_api.h"
#include "rx_mem.h"
#include "rx_sound.h"
#ifndef NO_BASE_SCALE
#include "../game/alive_gate.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXS 16
#define MAXE 4096

static rx_ambient_entity g_amb[MAXE];
static float g_mats[16384 * 16];
static rx_scene_object g_obj[8192];

/* the angle (degrees) between two row-vector matrices' y axes */
static float y_angle(const float* a, const float* b) {
    const float d = a[4] * b[4] + a[5] * b[5] + a[6] * b[6];
    const float la = sqrtf(a[4] * a[4] + a[5] * a[5] + a[6] * a[6]), lb = sqrtf(b[4] * b[4] + b[5] * b[5] + b[6] * b[6]);
    float c = d / (la * lb + 1e-12f);
    if (c > 1) c = 1; if (c < -1) c = -1;
    return acosf(c) * 57.29578f;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: alive_gate_test <install root> <type> <type id> <level> <location id> [frames] [map scale] [base scale]\n");
        return 2;
    }
    const int frames = argc > 6 ? atoi(argv[6]) : 60 * 60;
    const float scale = argc > 7 ? (float)atof(argv[7]) : 1.0f, base = argc > 8 ? (float)atof(argv[8]) : 1.0f;
    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("RESULT create failed: %s\n", rx_session_last_error(NULL)); return 1; }
#ifndef NO_BASE_SCALE   /* (-DNO_BASE_SCALE: a core before BASE SCALE, for a before / after comparison) */
    if (base != 1.0f && rx_session_set_map_scale_base(s, base) != 0) { printf("RESULT base: %s\n", rx_session_last_error(s)); return 1; }
#else
    (void)base;
#endif
    if (scale != 1.0f && rx_session_set_map_scale(s, scale) != 0) { printf("RESULT scale: %s\n", rx_session_last_error(s)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]);
    e.track_id = 0; e.vehicle_class = 2; e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = 12; e.ai_difficulty = 95; e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e) != 0) { printf("RESULT event load failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f < 3000) { if (rx_session_advance(s) != 0) break; f++; }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { printf("RESULT load error: %s\n", rx_session_last_error(s)); return 1; }
#ifndef NO_BASE_SCALE
    printf("loaded after %d frames, map scale %.3f (records %s)\n", f, rx_session_map_scale(s), rx_session_map_scale_records(s) ? "kept" : "off");
#else
    printf("loaded after %d frames, map scale %.3f\n", f, rx_session_map_scale(s));
#endif

    /* the gate objects */
    const int no = rx_session_scene_objects(s, g_obj, 8192);
    float gate_m[16] = {0}, bar0[16] = {0};
    int have_gate = 0, dropgate = -1;
    for (int i = 0; i < no; i++) {
        const int named = !strcmp(g_obj[i].name, "TO_StartingGate");
        if (g_obj[i].vtable == 0x0095fc24u || named || !strcmp(g_obj[i].name, "TO_StartingGate_Bar")) {
            printf("object %-24s vtable %08x id %d model %-24s at (%.2f, %.2f, %.2f)\n", g_obj[i].name, g_obj[i].vtable, g_obj[i].entity_id,
                   g_obj[i].model, g_obj[i].matrix[12], g_obj[i].matrix[13], g_obj[i].matrix[14]);
            if (named) { memcpy(gate_m, g_obj[i].matrix, sizeof gate_m); have_gate = 1; }
            if (g_obj[i].vtable == 0x0095fc24u && dropgate < 0) { dropgate = i; memcpy(bar0, g_obj[i].matrix, sizeof bar0); }
        }
    }
    if (!have_gate) { printf("RESULT %s no TO_StartingGate\n", argv[4]); return 1; }

    rx_slot sl[MAXS];
    float start[MAXS][3]; memset(start, 0, sizeof start);
    int ns = 0, placed = 0, prev_state = -1, go_frame = -1, drop_frame = -1, amb_bar = -1, amb_bar_node = -1;
    float amb_rest[16] = {0}, last_angle = 0.0f, max_before = -1e9f, bar_final = 0.0f;
    rx_pad pad; memset(&pad, 0, sizeof pad);
    pad.right_trigger = 255;
    int moved_after = 0, after_checked = 0, hidden_before_go = 0;
    const char* hold_env = getenv("RX_GATE_HOLD"); const int hold = hold_env ? atoi(hold_env) : 0;
    const int open = getenv("RX_GATE_OPEN") != NULL;
    float hold_max[MAXS]; for (int j = 0; j < MAXS; j++) hold_max[j] = -1e9f;
    static rx_sound_state snd; uint32_t cue_seen = 0;
    int prev_coll = -2;
    for (f = 0; f < frames; f++) {
#ifndef NO_BASE_SCALE
        if (open) rx_alive_gate_set_collision(0);
        else if (hold > 0 && go_frame >= 0 && f <= go_frame + hold) rx_alive_gate_set_collision(1);
        if (rx_alive_gate_collision() != prev_coll) { printf("frame %5d bar collision %d (drop frames %d)\n", f, rx_alive_gate_collision(), rx_alive_gate_drop_frames()); prev_coll = rx_alive_gate_collision(); }
#endif
        rx_session_set_input(s, NULL, &pad, 1u);
        if (rx_session_advance(s) != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) { printf("RESULT error at frame %d: %s\n", f, rx_session_last_error(s)); return 1; }
        rx_race_state r; memset(&r, 0, sizeof r);
        rx_session_race(s, &r);
        memset(&snd, 0, sizeof snd); snd.size = sizeof snd; snd.version = RX_SOUND_VERSION;
        if (rx_session_sound(s, &snd) == 0 && snd.cue_serial != cue_seen) {
            for (int i = 0; i < RX_SOUND_CUES; i++)
                if (snd.cues[i].serial > cue_seen) printf("frame %5d audio cue %s\n", f, snd.cues[i].path);
            cue_seen = snd.cue_serial;
        }
        if (r.state != prev_state) {
            printf("frame %5d race state %d start_timer %.2f\n", f, r.state, r.start_timer);
            if (r.state == 2 && go_frame < 0) go_frame = f;
            prev_state = r.state;
        }
        if (!placed) {
            ns = rx_session_slots(s, sl, MAXS);
            int all = ns > 0;
            for (int j = 0; j < ns; j++) all &= sl[j].vehicle != 0 && F32(sl[j].vehicle, 0xc4) != 0.0f;
            if (all) {
                placed = 1;
                for (int j = 0; j < ns; j++) { start[j][0] = F32(sl[j].vehicle, 0xc0); start[j][1] = F32(sl[j].vehicle, 0xc4); start[j][2] = F32(sl[j].vehicle, 0xc8); }
                printf("frame %5d: %d riders placed\n", f, ns);
            }
        }
        /* the bar: an Alive gate's DropGate node (ambient export), else the DropGate entity */
        float angle = 0.0f;
        {
            rx_ambient_info info; memset(&info, 0, sizeof info);
            const int na = rx_session_ambient_entities(s, g_amb, MAXE, g_mats, 16384, &info);
            if (amb_bar < 0) {
                for (int i = 0; i < na; i++)
                    if (g_amb[i].kind == 1 && (!strcmp(g_amb[i].model, "GL_TO_StartGate") || !strcmp(g_amb[i].model, "FR_JSComp_StartGate"))) {
                        amb_bar = i;
                        memcpy(gate_m, g_amb[i].matrix, sizeof gate_m);   /* the gate's world matrix (the scene object's +0x30 has no translation) */
                        printf("ambient gate: model %s bones %d flags %u\n", g_amb[i].model, g_amb[i].bones, g_amb[i].flags);
                        /* the DropGate node: the node whose matrix is not the entity's and is not a GateStart slot: the last bone (13 of 14) */
                        amb_bar_node = g_amb[i].bones >= 14 ? 13 : g_amb[i].bones - 1;
                    }
            }
            static int prev_vis = -1;
            if (amb_bar >= 0 && amb_bar < na && !(g_amb[amb_bar].flags & 1u) && go_frame < 0) hidden_before_go++;
            if (amb_bar >= 0 && amb_bar < na && (int)(g_amb[amb_bar].flags & 1u) != prev_vis) {
                prev_vis = (int)(g_amb[amb_bar].flags & 1u);
                printf("frame %5d the gate %s (race state %d)\n", f, prev_vis ? "shown" : "HIDDEN", r.state);
            }
            if (amb_bar >= 0 && amb_bar < na && g_amb[amb_bar].bones > amb_bar_node && amb_bar_node >= 0) {
                const float* m = g_mats + 16 * (g_amb[amb_bar].first_matrix + amb_bar_node);
                if (amb_rest[15] == 0.0f) memcpy(amb_rest, m, sizeof amb_rest);
                angle = y_angle(m, amb_rest);
                if (f % 600 == 0 || (angle != last_angle && fabsf(angle - last_angle) > 0.5f))
                    printf("frame %5d bar node y (%.3f, %.3f, %.3f) at (%.2f, %.2f, %.2f) angle %.1f visible %u\n", f, m[4], m[5], m[6], m[12], m[13], m[14],
                           angle, g_amb[amb_bar].flags & 1u);
            } else if (dropgate >= 0) {
                rx_session_scene_objects(s, g_obj, 8192);
                angle = y_angle(g_obj[dropgate].matrix, bar0);
                if (fabsf(angle - last_angle) > 0.5f) printf("frame %5d DropGate angle %.1f\n", f, angle);
            }
        }
        if (drop_frame < 0 && angle > 1.0f) drop_frame = f;
        last_angle = angle; bar_final = angle;
        /* the riders' progress past their slot along the gate's forward axis (its z axis: the slots face it) */
        const float fx = gate_m[8], fz = gate_m[10], fl = sqrtf(fx * fx + fz * fz);
        if (placed) {
            for (int j = 0; j < ns; j++) {
                if (!sl[j].vehicle) continue;
                const float dx = F32(sl[j].vehicle, 0xc0) - start[j][0], dz = F32(sl[j].vehicle, 0xc8) - start[j][2];
                const float along = (dx * fx + dz * fz) / (fl + 1e-12f);
                if (go_frame < 0 && along > max_before) max_before = along;
                if (go_frame >= 0 && f <= go_frame + (hold > 0 ? hold : 0) && along > hold_max[j]) hold_max[j] = along;
                if (go_frame >= 0 && !after_checked && f == go_frame + 360 + hold && along > 30.0f) moved_after++;
            }
            if (hold > 0 && go_frame >= 0 && f == go_frame + hold) {
                float worst = -1e9f;
                printf("HOLD %d frames after GO (%s):", hold, open ? "collision off" : "collision on");
                for (int j = 0; j < ns; j++) { printf(" %.2f", hold_max[j]); if (hold_max[j] > worst) worst = hold_max[j]; }
                printf("  furthest %.2f ft\n", worst);
            }
            if (go_frame >= 0 && f == go_frame + 360 + hold) {
                after_checked = 1;
                for (int j = 0; j < ns; j++) {
                    if (!sl[j].vehicle) continue;
                    const float dx = F32(sl[j].vehicle, 0xc0) - start[j][0], dz = F32(sl[j].vehicle, 0xc8) - start[j][2];
                    printf("  slot %2d %s %.1f ft along the gate's axis, %.1f ft from the slot\n", sl[j].slot, sl[j].local ? "player" : "AI    ",
                           (dx * fx + dz * fz) / (fl + 1e-12f), sqrtf(dx * dx + dz * dz));
                }
                printf("frame %5d: %d / %d riders 30 ft past their slot 6 s after GO\n", f, moved_after, ns);
            }
            if (go_frame < 0 && f % 300 == 0) printf("frame %5d before GO: furthest rider %.2f ft past his slot\n", f, max_before);
        }
        if (getenv("RX_GATE_TRACE") && placed && amb_bar >= 0 && (go_frame < 0 ? (f % 120 == 0) : f <= go_frame + 2)) {   /* the bodies in the gate's frame */
            for (int j = 0; j < ns; j++) if (sl[j].local || j == 1) {
                rx_vehicle_info vi; memset(&vi, 0, sizeof vi);
                if (rx_session_vehicle_info(s, sl[j].slot, &vi) != 0) continue;
                printf("bodies f %d slot %d:", f, sl[j].slot);
                for (int k = 0; k < vi.bodies && k < 6; k++) {
                    const float px = vi.render_position[4 * k] - gate_m[12], py = vi.render_position[4 * k + 1] - gate_m[13], pz = vi.render_position[4 * k + 2] - gate_m[14];
                    printf(" [x %.2f y %.2f z %.2f]", px * gate_m[0] + py * gate_m[1] + pz * gate_m[2], px * gate_m[4] + py * gate_m[5] + pz * gate_m[6],
                           px * gate_m[8] + py * gate_m[9] + pz * gate_m[10]);
                }
                printf("\n");
            }
        }
        if (getenv("RX_GATE_TRACE") && go_frame >= 0 && f <= go_frame + 90 && placed) {   /* the player's first 1.5 s after GO */
            for (int j = 0; j < ns; j++) if (sl[j].local && sl[j].vehicle) {
                const float dx = F32(sl[j].vehicle, 0xc0) - start[j][0], dy = F32(sl[j].vehicle, 0xc4) - start[j][1], dz = F32(sl[j].vehicle, 0xc8) - start[j][2];
                rx_bike_state b; memset(&b, 0, sizeof b); rx_session_bike(s, &b);
                printf("trace %d player along %.2f up %.2f speed %.1f wreck %u pos (%.2f %.2f %.2f) bike (%.2f %.2f %.2f)\n", f - go_frame,
                       (dx * fx + dz * fz) / (fl + 1e-12f), dy, b.speed, U32(sl[j].vehicle, 0x280), F32(sl[j].vehicle, 0xc0), F32(sl[j].vehicle, 0xc4),
                       F32(sl[j].vehicle, 0xc8), b.position[0][0], b.position[0][1], b.position[0][2]);
            }
        }
        if (after_checked) break;
    }
    /* the bar line: the slot's node is 7.25 ft behind the Alive gate origin, the bar's back at about -3 ft: a front wheel ~2.4 ft ahead of
     * the bike's centre may move about 1.8 ft before touching it. Held = no rider beyond 3 ft before GO. */
    const int held = max_before < 3.0f;
    printf("RESULT %s held %d max_before_go %.2f drop_frame %d go_frame %d drop_minus_go %d bar_final_deg %.1f moved_after %d/%d hidden_before_go %d\n", argv[4],
           held, max_before, drop_frame, go_frame, drop_frame >= 0 && go_frame >= 0 ? drop_frame - go_frame : 9999, bar_final, moved_after, ns, hidden_before_go);
    rx_session_destroy(s);
    return 0;
}
