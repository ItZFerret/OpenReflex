/* podium_session.inc -- rx_session_podium (include/rx_podium_api.h) and the post-scenario test the camera getter uses; included by
 * rx_session.c (w-podium, 2026-10-26; the race intro cast: w-cast, 2026-10-27). Read-only: nothing here changes game state except the game's own lazy node evaluation
 * (007db890), which the renderer would run for these characters as well. */

/* The scenarios of an activity class, found by the scenario vtables the constructors write (build_activity_race.c,
 * build_activity_freestyle.c, the Waypoint activity): the Post sub-state field, the intro, the finish and the results scenario
 * (each embedded in the activity; a scenario's started flag is its +0x151b0, its step +0x151b4). */
typedef struct podium_layout { uint32_t post, intro, finish, results; } podium_layout;
static int podium_layout_of(rx_addr act, podium_layout* out) {
    if (!act) return 0;
    if (U32(act, 0x114260) == 0x009433d4u) { out->post = 0x13e640; out->intro = 0xff090; out->finish = 0x114260; out->results = 0x129430; return 1; }  /* RaceFinishScenario */
    if (U32(act, 0x114250) == 0x0094414cu) { out->post = 0x13e5f8; out->intro = 0xff080; out->finish = 0x114250; out->results = 0x129420; return 1; }  /* FMXFinishScenario */
    if (U32(act, 0x114270) == 0x00943b94u) { out->post = 0x13e618; out->intro = 0xff0a0; out->finish = 0x114270; out->results = 0x129440; return 1; }  /* WaypointFinishScenario */
    /* MXActivityFreeRide (0055fdb0): only the intro, FreerideIntroScenario (vtable 0x00944014) at +0xff200, started by the free ride's
     * Pre (005604f0); no Post scenarios (w-frscene) */
    if (U32(act, 0) == 0x0094329cu && U32(act, 0xff200) == 0x00944014u) { out->post = 0; out->intro = 0xff200; out->finish = 0; out->results = 0; return 1; }
    return 0;
}

/* 1 = the finish scenario plays (Post sub-state 1), 2 = the results scenario (sub-state 3), 0 otherwise; *sc = that scenario */
static int podium_scenario_playing(rx_addr act, rx_addr* sc, int32_t* post_state) {
    podium_layout l;
    if (post_state) *post_state = -1;
    if (!podium_layout_of(act, &l) || !l.post || U32(act, 0x21c) != 4) return 0;
    const uint32_t post = U32(act, l.post);
    if (post_state) *post_state = (int32_t)post;
    const rx_addr s = post == 1 ? act + l.finish : post == 3 ? act + l.results : 0;
    if (!s || U32(s, 0x151b0) == 0) return 0;
    if (sc) *sc = s;
    return post == 1 ? 1 : 2;
}

/* 3 = the intro scenario plays (the fly-by, the start-line shots, the crew chief's shots: RaceIntroScenario / FMXIntroScenario /
 * WaypointIntroScenario, started by Scenario_Start 00747810 and running while its step +0x151b4 is neither 0 nor 0xf: BaseScenario::vf11
 * 0055eb40), 0 otherwise; *sc = that scenario. Pre (state 0) and Countdown (1) only: Post has its own scenarios above. */
static int podium_intro_playing(rx_addr act, rx_addr* sc) {
    podium_layout l;
    if (!podium_layout_of(act, &l) || U32(act, 0x21c) > 1) return 0;
    const rx_addr s = act + l.intro;
    const uint32_t step = U32(s, 0x151b4);
    if (U32(s, 0x151b0) == 0 || step == 0 || step == 0xf) return 0;
    if (sc) *sc = s;
    return 3;
}

/* the local activity's scenario camera while a post scenario plays: the world's MXAnimatedCamera (world + 0x218f0) once its update
 * posed it (+0x2b0); the scenario update copies it into the view (007473f0 -> rx_cam_entity_to_view). Not while a replay clip plays
 * (the clip recorder's state 0xa942b4 == 2: the finish replay of step 7): the world update's later ReplayCameras_Update (007678b0)
 * copies the replay director into the view then (the oracle: the view is the director's camera during step 7), and the replay
 * presenter reports that view (rx_session_replay_view). */
static int podium_camera(rx_addr p, rx_camera* out) {
    if (!p || U32(0, 0x00a942b4) == 2) return 0;
    const rx_addr act = U32(p, 0x94);
    if (!podium_scenario_playing(act, NULL, NULL)) return 0;
    const rx_addr w = U32(act, 0x234);
    if (!w) return 0;
    const rx_addr anim = w + 0x218f0u;
    return U8(anim, 0x2b0u) == 1u && camera_read(anim + 0x90u, out);
}

int rx_session_podium(struct rx_session* s, rx_podium_info* info, rx_podium_rider* riders, uint32_t max_riders,
                      float* matrices, uint32_t max_matrices, rx_podium_slot* slots, uint32_t max_slots) {
    if (!session_ok(s)) return 1;
    rx_podium_info local_info;
    if (!info) info = &local_info;
    memset(info, 0, sizeof *info);
    info->post_state = -1;
    const rx_addr p = local_player();
    const rx_addr act = p ? U32(p, 0x94) : 0;
    rx_addr sc = 0;
    info->scenario = act ? podium_scenario_playing(act, &sc, &info->post_state) : 0;
    if (act && !info->scenario) info->scenario = podium_intro_playing(act, &sc);
    info->step = sc ? I32(sc, 0x151b4) : 0;
    /* every player slot's live rider / vehicle drawn flags */
    if (slots) {
        for (uint32_t slot = 0; slot < RX_RC_MAX && (uint32_t)info->slots < max_slots; slot++) {
            const rx_addr pl = rx_seat(s->game + 0x7460, slot);
            const rx_addr v = U32(pl, 0x1ac);
            if (!v) continue;
            rx_podium_slot* r = &slots[info->slots++];
            memset(r, 0, sizeof *r);
            r->slot = (int32_t)slot;
            const rx_addr rider = U32(pl, 0x1b4);
            r->rider_drawn = rider ? U8(rider, 0xa2) != 0 : 0;
            const rx_addr model = U32(v, 0x1328);
            r->vehicle_drawn = model ? U8(model, 0x14e) != 0 : 0;
        }
    }
    /* the podium RiderCharacters of the level cast (0074ba40: records of 0x30 bytes from +0x50, one per player, G+0x1541c) */
    const rx_addr w = act ? U32(act, 0x234) : 0;
    if (!w || !s->game) return 0;
    const rx_addr cast = w + 0x20150;
    const uint32_t count = U8(s->game, 0x1541c);
    uint32_t matrix_count = 0;
    for (uint32_t k = 0; k < count && k < RX_RC_MAX; k++) {
        const rx_addr rec = rx_cast_rec_ro(cast, k);
        const rx_addr chr = rec ? U32(rec, 0) : 0;
        if (!chr || U32(chr, 0) != 0x0095881cu) continue;   /* RiderCharacter */
        if (!riders || (uint32_t)info->riders >= max_riders) break;
        rx_podium_rider* r = &riders[info->riders++];
        memset(r, 0, sizeof *r);
        r->object = chr;
        const rx_addr pl = U32(rec, 4);
        r->slot = rx_seat_of(s->game + 0x7460, pl);
        r->visible = U8(chr, 0xa2) != 0;
        r->placed = U32(chr, 0x70) != 0;
        /* the draw distance RiderCharacter::vf2_o90 (00624b70) hands Model_SubmitSkinned (007f4e50): the instance's own ModelAsset copy
         * (+0x200) +0x120, divided by the renderer's LOD divisor (the unsigned divide of 007f4e72); the model is culled beyond it */
        r->draw_distance = (float)(U32(chr, 0x200 + 0x120) / ambient_lod_divisor());
        copy_vec4(r->mud_amount, chr + 0x3b0);
        copy_vec4(r->mud_color, chr + 0x3c0);
        copy_vec4(r->mud_wetness, chr + 0x3d0);
        /* the skeleton: the instance's ModelAsset copy (+0x200; records +0x218, count +0x21c, world +0x22c) */
        const uint32_t n = U32(chr, 0x21c), records = U32(chr, 0x218), world = U32(chr, 0x22c);
        if (U32(chr, 0x200) != 0x00972844u || n != 69 || !records || !world) continue;
        for (int32_t i = (int32_t)n - 1; i >= 0; i--) rx_world_node_evaluate_world(records + 16u * (uint32_t)i, 0);
        copy_floats(r->root, world + 64u * n, 16);
        if (!matrices || matrix_count + n > max_matrices) continue;   /* no room: bones stays 0 */
        r->first_matrix = (int32_t)matrix_count;
        r->bones = (int32_t)n;
        copy_floats(matrices + 16u * matrix_count, world, 16u * n);
        matrix_count += n;
    }
    info->matrices = (int32_t)matrix_count;
    return 0;
}

/* ---- v3 (w-frscene): the scenery models a scenario script hides / moves, the scenario actors' node flags -------------------------- */

/* the id a ModelEntity had when the session first saw it (ModelPosition 0055d4e0 gives a re-added entity a new +0x14); an address whose
 * name pointer changed is another entity (a subscene unloaded and another one loaded at the same address) */
static int32_t scenario_model_first_id(struct rx_session* s, rx_addr e) {
    const rx_addr name = U32(e, 0x18);
    for (int32_t i = 0; i < s->model_ids_count; i++) {
        struct rx_model_identity* m = &s->model_ids[i];
        if (m->address != e) continue;
        if (m->name != name) { m->name = name; m->id = I32(e, 0x14); }
        return m->id;
    }
    if (s->model_ids_count == s->model_ids_cap) {
        const int32_t cap = s->model_ids_cap ? s->model_ids_cap * 2 : 1024;
        struct rx_model_identity* p = (struct rx_model_identity*)realloc(s->model_ids, (size_t)cap * sizeof *p);
        if (!p) return I32(e, 0x14);
        s->model_ids = p; s->model_ids_cap = cap;
    }
    struct rx_model_identity* m = &s->model_ids[s->model_ids_count++];
    m->address = e; m->name = name; m->id = I32(e, 0x14);
    return m->id;
}

int rx_session_scenario_models(struct rx_session* s, rx_scenario_model* out, uint32_t max) {
    if (!session_ok(s)) return -1;
    const int32_t n = scene_entities(s);
    int32_t total = 0;
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = s->scene_entities[i];
        const int scrolling = U32(e, 0) == 0x00939ba4u && U32(e, 0x90) == 0x00939c1cu;   /* ScrollingTextureEntity (00410b80) */
        if (!scrolling && (U32(e, 0) != 0x009370d4u || U32(e, 0x90) != 0x0093714cu)) continue;   /* ModelEntity (00401db0), no subclass */
        const int32_t first = scenario_model_first_id(s, e);
        if (out && (uint32_t)total < max) {
            rx_scenario_model* m = &out[total];
            memset(m, 0, sizeof *m);
            m->entity_id = first;
            m->current_id = I32(e, 0x14);
            m->drawn = U8(e, 0xa2) != 0;                     /* the ModelInstance's +2: Model_Submit 007f1b00 returns while it is 0 */
            copy_floats(m->matrix, e + 0xc0, 16);           /* the ModelInstance's +0x20: the matrix Model_Submit draws with */
            copy_guest_string(m->name, sizeof m->name, U32(e, 0x18));
            if (scrolling) { m->kind = 1; m->texture_offset[0] = F32(e, 0xaa0); m->texture_offset[1] = F32(e, 0xaa4); }
        }
        total++;
    }
    return total;
}

int rx_session_ambient_node_flags(struct rx_session* s, uint32_t object_id, uint8_t* out, uint32_t max) {
    if (!session_ok(s)) return -1;
    ambient_ensure(s);
    for (int32_t i = 0; i < s->ambient_objects_count; i++) {
        const struct rx_ambient_object* o = &s->ambient_objects[i];
        if (o->address != object_id) continue;
        const rx_addr a = o->address;
        if (U32(a, 0) != k_ambient_kinds[o->kind].vt0 || U32(a, 0x90) != k_ambient_kinds[o->kind].vt90) return 0;   /* gone */
        const uint32_t count = U32(a, 0x21c), records = U32(a, 0x218);
        if (U32(a, 0x200) != RX_MODEL_ASSET_VT || count == 0 || count > 255 || !records) return 0;
        for (uint32_t k = 0; k < count && k < max && out; k++) out[k] = U8(records, 16u * k + 0xc);
        return (int)count;
    }
    return 0;
}

int rx_session_scenario_sounds(struct rx_session* s, rx_scenario_sound* out, uint32_t max) {
    if (!session_ok(s)) return -1;
    if (!out || max == 0) return 0;
    enum { BATCH = 16 };
    char names[BATCH][128]; float pos[BATCH][3]; int32_t has[BATCH];
    uint32_t written = 0;
    while (written < max) {
        const int want = (int)(max - written < BATCH ? max - written : BATCH);
        const int n = rx_scenario_sounds_take(names, pos, has, want);
        for (int i = 0; i < n; i++) {
            rx_scenario_sound* o = &out[written++];
            memset(o, 0, sizeof *o);
            memcpy(o->event, names[i], sizeof o->event); o->event[sizeof o->event - 1] = 0;
            o->position[0] = pos[i][0]; o->position[1] = pos[i][1]; o->position[2] = pos[i][2];
            o->has_position = has[i];
        }
        if (n < want) break;
    }
    return (int)written;
}
