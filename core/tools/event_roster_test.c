/* event_roster_test.c (w-opencross) -- the vehicle classes of an event's field, headless through the product session.
 *   event_roster_test <install root> <event type> <event type id> <level> <location id> <vehicle class> [riders (12)] [ai difficulty (95)]
 * Loads the event with the given VehicleClass (the one the front end hands the world: LocationSelect's class), runs it to the race and
 * prints each slot's vehicle (rx_session_vehicle_info type: 0 MX, 1 ATV, 3 Trophy Truck / Pro, 5 Buggy, 7 UTV) and its VehicleModel
 * (rx_session_appearance), then a count per vehicle type. Exit 0 when the race started and every slot has a vehicle.
 * env ER_FRAMES (frames to run once racing, 5). Links against the product objects (tools/event_roster_test.sh). */
#include "rx_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kind_name(int t) {
    switch (t) { case 0: return "MX"; case 1: return "ATV"; case 3: return "TrophyTruck"; case 5: return "Buggy"; case 7: return "UTV"; default: return "other"; }
}

int main(int argc, char** argv) {
    if (argc < 7) { fprintf(stderr, "usage: event_roster_test <install root> <event type> <type id> <level> <location id> <vehicle class> [riders] [ai]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 2; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]); e.track_id = 0;
    e.vehicle_class = atoi(argv[6]); e.riders = argc > 7 ? atoi(argv[7]) : 12; e.ai_difficulty = argc > 8 ? atoi(argv[8]) : 95;
    e.duration = 2; strcpy(e.duration_type, "Laps");
    printf("EVENT %s/%s type=%d location=%d class=%d riders=%d ai=%d\n", e.event_type, e.location, e.event_type_id, e.location_id, e.vehicle_class, e.riders, e.ai_difficulty);
    if (rx_session_event_load(s, &e)) { fprintf(stderr, "event load: %s\n", rx_session_last_error(s)); rx_session_destroy(s); return 1; }
    const int after = getenv("ER_FRAMES") ? atoi(getenv("ER_FRAMES")) : 5;
    int racing = 0, result = 1;
    for (int i = 0; i < 7200 && racing < after; i++) {
        uint8_t keys[256] = { 0 };
        if (rx_session_set_input(s, keys, NULL, 0) || rx_session_advance_dt(s, 1.f / 60)) { fprintf(stderr, "advance: %s\n", rx_session_last_error(s)); break; }
        if (rx_session_get_state(s) == RX_SESSION_ERROR) { fprintf(stderr, "error: %s\n", rx_session_last_error(s)); break; }
        if (rx_session_get_state(s) == RX_SESSION_RACING) racing++;
    }
    if (racing >= after) {
        rx_slot slots[32]; int n = rx_session_slots(s, slots, 32), count[16] = { 0 }, missing = 0;
        for (int k = 0; k < n; k++) {
            rx_vehicle_info v; rx_appearance a; memset(&v, 0, sizeof v); memset(&a, 0, sizeof a);
            int vi = rx_session_vehicle_info(s, slots[k].slot, &v), ap = rx_session_appearance(s, slots[k].slot, &a);
            if (vi) { missing++; continue; }
            printf("SLOT %2d %s %-11s type=%d model=%d%s\n", slots[k].slot, slots[k].local ? "human" : "AI   ", kind_name(v.type), v.type, ap ? -1 : a.vehicle_model_id, "");
            if (v.type >= 0 && v.type < 16) count[v.type]++;
        }
        printf("FIELD");
        for (int t = 0; t < 16; t++) if (count[t]) printf(" %s=%d", kind_name(t), count[t]);
        printf("  (%d slots)\n", n);
        result = missing || n != e.riders;
        if (result) printf("FAIL slots=%d expected=%d missing vehicles=%d\n", n, e.riders, missing);
    } else printf("FAIL no racing state\n");
    rx_session_destroy(s);
    return result;
}
