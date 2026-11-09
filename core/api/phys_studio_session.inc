/* phys_studio_session.inc -- the PHYSICS STUDIO calls of rx_api.h (port addition; game/phys_studio.h, core/plan/PHYSICS_STUDIO.md),
 * included by rx_session.c (it uses session_ok, run_guarded and session_error). */
#include "../game/phys_studio.h"

int rx_phys_catalog(const char* install_root, char* out, uint32_t capacity, uint32_t* required) {
    if (!rx_ps_param_set_hash()) {   /* not read yet: the install opened for this call (the format library only, no game state) */
        if (!install_root) return -1;
        fmt_install* in = fmt_install_open(install_root);
        if (!in) return -1;
        const int ok = rx_ps_catalog_build(in);
        fmt_install_close(in);
        if (ok != 0) return -1;
    }
    return rx_ps_catalog_text(out, capacity, required);
}

uint32_t rx_phys_param_set_hash(void) { return rx_ps_param_set_hash(); }

int rx_session_phys_set(struct rx_session* s, const char* text, char* report, uint32_t capacity) {
    if (report && capacity) report[0] = 0;
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return -1;
    if (rx_ps_catalog_build(s->install) != 0) { session_error(s, "the vehicle parameter files could not be read (EngineDependencies)"); return -2; }
    const int n = rx_ps_set_text(text, report, capacity);
    if (n > 0) fprintf(stderr, "rxcore: PHYSICS STUDIO: %d value(s) set for the local players' vehicles%s\n", n,
                       s->spec_loaded ? " (the running event: rx_session_phys_apply_live)" : "; this event keeps no offline records");
    return n;
}

/* ALIVE CLASSES (game/phys_studio.h): the parameter file the local players' vehicles of a VehicleModelId take over their class file */
int rx_session_vehicle_physics(struct rx_session* s, int32_t vehicle_model, int32_t cls, const uint8_t* data, uint32_t size, char* report, uint32_t capacity) {
    if (report && capacity) report[0] = 0;
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return -1;
    if (!data || !size) { rx_vp_clear(); return 0; }
    const int n = rx_vp_set(vehicle_model, cls, data, size, report, capacity);
    if (n < 0) { session_error(s, "vehicle physics for model %d class %d refused (%d)", (int)vehicle_model, (int)cls, n); return n; }
    fprintf(stderr, "rxcore: ALIVE CLASS %d: %d parameter record(s) for the local players' VehicleModelId %d\n", (int)cls, n, (int)vehicle_model);
    return n;
}

int rx_session_vehicle_physics_nodes(struct rx_session* s, int32_t vehicle_model, const char* text) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return -1;
    const int n = rx_vp_set_nodes(vehicle_model, text);
    if (n < 0) { session_error(s, "vehicle physics nodes for model %d: set its physics first", (int)vehicle_model); return n; }
    fprintf(stderr, "rxcore: ALIVE CLASS: %d hierarchy node(s) of its own for VehicleModelId %d\n", n, (int)vehicle_model);
    return n;
}

int rx_session_phys_get(struct rx_session* s, char* out, uint32_t capacity, uint32_t* required) {
    if (!session_ok(s)) return -1;
    return rx_ps_get_text(out, capacity, required);
}

int rx_session_phys_live_values(struct rx_session* s, char* out, uint32_t capacity, uint32_t* required) {
    if (!session_ok(s)) return -1;
    if (rx_ps_catalog_build(s->install) != 0) return -2;
    return rx_ps_live_text(out, capacity, required);
}

int rx_session_phys_arm_live(struct rx_session* s, int32_t on) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return -1;
    if (s->spec_loaded && on) { session_error(s, "live physics tuning is armed before the event loads"); return -2; }
    rx_ps_arm(on != 0);
    return 0;
}

int rx_session_phys_active(const struct rx_session* s) { return s && s->magic == RX_SESSION_MAGIC ? rx_ps_active() : 0; }

struct phys_live_call { int n; };
static void phys_live_run(void* p) { ((struct phys_live_call*)p)->n = rx_ps_apply_live(); }
int rx_session_phys_apply_live(struct rx_session* s) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return -1;
    struct phys_live_call call = {0};
    char captured[4096];
    const int sig = run_guarded(phys_live_run, &call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, "rx_session_phys_apply_live", sig, captured);
        session_error(s, "%s", msg);
        s->state = RX_SESSION_ERROR;
        return -1;
    }
    return call.n;
}
