/* compass_session.inc -- the HUD compass' markers (game/rx_compass_api.h), included by rx_session.c after challenge_session.inc (it uses
 * local_player(), session_ok(), scene_manager(), scene_collect_entities() and copy_guest_string()). Read-only copies of positions the
 * running activity holds; nothing here runs guest code, so it is safe between advances. */
#include "rx_compass_api.h"
_Static_assert(sizeof(rx_compass_state) == 992, "compass ABI size");

/* the challenge classes' vtables (race_challenge_state.c's list): MXActivityChallenge and its six subclasses */
static int compass_challenge_vtable(uint32_t vtable) {
    switch (vtable) {
    case 0x009450ccu: case 0x009451dcu: case 0x009452ecu: case 0x009453fcu: case 0x00945514u: case 0x00945624u: case 0x00945734u: return 1;
    default: return 0;
    }
}

static void compass_add(rx_compass_state* v, int32_t type, int32_t index, int32_t aux, rx_addr entity) {
    if (v->count >= RX_COMPASS_MARKS) return;
    rx_compass_mark* m = &v->marks[v->count++];
    m->type = type; m->index = index; m->aux = aux;
    m->pos[0] = F32(entity, 0x60); m->pos[1] = F32(entity, 0x64); m->pos[2] = F32(entity, 0x68);
}

/* the scene's "Trailblazer_Finish_Trigger" (vf1_oe0 compares the entity name +0x18): found once per scene and activity, then kept */
static rx_addr g_compass_finish, g_compass_finish_scene, g_compass_finish_activity;
static rx_addr compass_finish(rx_addr scene, rx_addr activity) {
    if (!scene) return 0;
    if (g_compass_finish_scene == scene && g_compass_finish_activity == activity) return g_compass_finish;
    g_compass_finish_scene = scene; g_compass_finish_activity = activity; g_compass_finish = 0;
    rx_addr* list = (rx_addr*)malloc(8192 * sizeof *list);
    if (!list) return 0;
    const int32_t n = scene_collect_entities(scene, list, 8192);
    for (int32_t i = 0; i < n && !g_compass_finish; i++) {
        char name[40];
        copy_guest_string(name, sizeof name, U32(list[i], 0x18));
        if (strcmp(name, "Trailblazer_Finish_Trigger") == 0) g_compass_finish = list[i];
    }
    free(list);
    return g_compass_finish;
}

/* A Waypoint race's gates as MXUICompassComponent::vf1_oe0 (UI kind 6) files them: 007d2820 (scene, 0x1043, buffer, 0xff, A + 0x118)
 * walks the scene's 24 group buckets (scene + 0x38 + b * 0x110: the group name at +0, live flag +0x10c, entity array +0x104, count
 * +0x108), takes the first live bucket named as the activity's group and keeps its entities of RSData type 0x1043 (WaypointRaceGate,
 * vtable 0x00938cd4) in the bucket's order; vf1_oe0 stores each one's position (vf 0x44: matrix row 3) in the slot of its place, the
 * layer has 16 (WaypointGate1..16). Read-only: no lock, no guest call. */
static void compass_waypoint_gates(rx_compass_state* v, rx_addr scene, rx_addr A) {
    if (!scene) return;
    char group[0x104];
    copy_guest_string(group, sizeof group, A + 0x118);
    for (uint32_t b = 0; b < 24; b++) {
        const rx_addr bucket = scene + 0x38 + b * 0x110;
        if (!U32(bucket, 0x10c)) continue;
        char name[0x104];
        copy_guest_string(name, sizeof name, bucket);
        if (strcmp(name, group) != 0) continue;
        const rx_addr list = U32(bucket, 0x104);
        const int32_t n = I32(bucket, 0x108);
        int32_t slot = 0;
        for (int32_t i = 0; list && i < n && i < 0xff && slot < 16; i++) {
            const rx_addr e = U32(list, 4u * (uint32_t)i);
            if (!e || U32(e, 0) != 0x00938cd4u) continue;
            compass_add(v, RX_COMPASS_GATE, slot++, I32(e, 0x280), e);
        }
        return;   /* 007d2820 stops at the first bucket of the group */
    }
}

int rx_session_compass(struct rx_session* s, rx_compass_state* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_COMPASS_API_VERSION) return 1;
    rx_compass_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_COMPASS_API_VERSION;
    v.challenge_kind = -1;
    v.waypoint_gate = -1;
    const rx_addr p = local_player();
    const rx_addr A = p ? U32(p, 0x94) : 0;
    rx_addr wa = 0;
    const rx_addr wd = p ? race_driver_for(p, &wa) : 0;
    if (wa && waypoint_driver_layout(wd)) {   /* a Waypoint race (rx_session_waypoint's test): UI kind 6 */
        v.valid = 1;
        v.waypoint_gate = I32(wd, 0x10d4);
        v.waypoint_finished = U32(wd, 0x70) ? 1u : 0u;
        compass_waypoint_gates(&v, scene_manager(), wa);
    } else if (A) {
        const uint32_t vt = U32(A, 0);
        if (vt == 0x0094329cu) {   /* MXActivityFreeRide: the StartActivity triggers MXActivityFreeRide::vf5 collected (A + 0xff190, 24 slots) */
            v.valid = 1;
            v.discovery_mode = U32(A, 0xff180);
            for (uint32_t k = 0; k < 0x18; k++) {
                const rx_addr t = U32(A, 0xff190 + 4 * k);
                if (!t) continue;
                const uint32_t index = rx_challenge_activity_index(t + 0x108);
                if (index < 10) compass_add(&v, RX_COMPASS_AREA, (int32_t)index, I32(t, 0x104), t);
            }
        } else if (compass_challenge_vtable(vt)) {
            v.valid = 1;
            v.challenge_kind = I32(A, 0xff070);
            if (v.challenge_kind == 8) {   /* TargetPractice: the targets (MXTargetPracticeCollectable) and whether each was hit */
                const int32_t n = I32(A, 0xff3b0);
                for (int32_t i = 0; i < n && i < 0x20; i++) {
                    const rx_addr t = U32(A, 0xff3b4 + 4 * (uint32_t)i);
                    if (t) compass_add(&v, RX_COMPASS_TARGET, i, U32(t, 0x140) ? 1 : 0, t);
                }
            } else if (v.challenge_kind == 6) {   /* Trailblazer: the finish trigger */
                const rx_addr f = compass_finish(scene_manager(), A);
                if (f) compass_add(&v, RX_COMPASS_FINISH, 0, 0, f);
            }
        }
    }
    *out = v;
    return 0;
}
