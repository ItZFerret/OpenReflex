/* scene_pages_session.inc -- rx_session_start_gates and rx_session_skip_scenario (include/rx_scene_pages_api.h); included by rx_session.c
 * (w-p2scene, 2026-10-27). rx_session_start_gates is read-only: the game's own lazy node evaluation (007db890, which the renderer would run
 * for these nodes as well) is its only write. */

_Static_assert(sizeof(rx_start_gates) == 880, "rx_start_gates ABI size");

static int gate_name_eq(rx_addr s, const char* name) {
    for (uint32_t i = 0;; i++) {
        char a = (char)U8(s, i), b = name[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
        if (!b) return 1;
    }
}

/* the node record of a name in a hierarchy (Hierarchy_FindNode 007de9e0 without its +0x30 write): 0 when none */
static rx_addr gate_find_node(rx_addr h, const char* name) {
    for (int32_t i = 0; i < I32(h, 0xc); i++) {
        const rx_addr rec = U32(h, 8) + 16u * (uint32_t)i;
        if (U16(rec, 8) == 0xffff) continue;
        if (gate_name_eq(U16(rec, 8) + U32(U32(rec, 0), 0x10), name)) return rec;
    }
    return 0;
}

int rx_session_start_gates(struct rx_session* s, rx_start_gates* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out || out->version != RX_SCENE_PAGES_API_VERSION) return 1;
    rx_start_gates r;
    memset(&r, 0, sizeof r);
    r.size = sizeof r; r.version = RX_SCENE_PAGES_API_VERSION;
    const rx_addr world = s->game ? U32(s->game, 0x5a54) : 0;   /* the event world (0073edc5: the UI world otherwise) */
    const rx_addr scene = world ? U32(world, 0x20138) : 0;
    const rx_addr gate = scene ? rx_scene_manager_find_named(scene, 0x00942060 /* "TO_StartingGate" */) : 0;
    const rx_addr holder = gate ? U32(gate, 0x100) : 0;
    const rx_addr asset = holder ? U32(holder, 0x44) : 0;
    if (asset) {
        const rx_addr hier = asset + 0x10;
        const rx_addr m = gate + 0x30;                           /* Entity::vf14 0088fa20 */
        float M[16];
        copy_floats(M, m, 16);
        copy_floats(r.gate_matrix, m, 16);
        for (int i = 0; i < 16; i++) {
            char name[16];
            snprintf(name, sizeof name, "GateStart_%02d", i + 1);
            const rx_addr node = gate_find_node(hier, name);
            if (!node) continue;
            rx_world_node_evaluate_world(node, 0);
            const rx_addr W = U32(U32(node, 0), 0x1c) + 0x40u * U8(node, 0xb);
            rx_start_gate_node* g = &r.gates[i];
            const float gk = rx_ms_on() ? rx_ms_gate_slots() : 1.0f;   /* MAP SCALE (port): as the grid (build_activity_grid.c) */
            for (int k = 0; k < 3; k++) { g->local_forward[k] = F32(W, 0x20 + 4u * (uint32_t)k); g->local_position[k] = F32(W, 0x30 + 4u * (uint32_t)k) * gk; }
            const float a[4] = {F32(W, 0x20), F32(W, 0x24), F32(W, 0x28), F32(W, 0x2c)}, b[4] = {F32(W, 0x30) * gk, F32(W, 0x34) * gk, F32(W, 0x38) * gk, F32(W, 0x3c)};
            for (int k = 0; k < 3; k++) {
                g->forward[k] = a[0] * M[k] + a[1] * M[4 + k] + a[2] * M[8 + k] + a[3] * M[12 + k];
                g->position[k] = b[0] * M[k] + b[1] * M[4 + k] + b[2] * M[8 + k] + b[3] * M[12 + k];
            }
            r.present_mask |= 1u << i;
            r.gate_count++;
        }
        r.valid = r.gate_count > 0;
    }
    const rx_addr cam_entity = scene ? rx_scene_manager_find_named(scene, 0x0096c020 /* "UI_StartGate_Camera" */) : 0;
    r.has_camera = cam_entity != 0;
    if (cam_entity) {
        rx_camera lens;
        memset(&lens, 0, sizeof lens);
        if (camera_read(cam_entity + 0x90, &lens)) { r.fov = lens.fov; r.near_plane = lens.near_plane; r.far_plane = lens.far_plane; }
    }
    *out = r;
    return 0;
}

/* App.SkipToNextScenario 00736d60 (no arguments): [0xa7f748] = 1 */
int rx_session_skip_scenario(struct rx_session* s) {
    if (!session_ok(s)) return 1;
    SU32(0, 0x00a7f748, 1);
    return 0;
}
