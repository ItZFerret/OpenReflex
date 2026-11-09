/* heat_session.inc -- the Heat format's pages (rx_career_heat.h), included once by career_public_session.inc. Host API policy only:
 * every game step is the original's own code (career_heat.c, career_single*.c, career_aaa_loading.c, build_activity_aaa.c). */

#define CP_KEY_ROUTING_MESSAGE 0x0094cc74u   /* "UIRoutingMessage" */

/* The owned UI property `key` copied into `out` ("" when it is not set). */
static void cp_property_text(rx_addr key, char* out, size_t capacity) {
    out[0] = 0;
    RX_FRAME_ENTER(F, 64);
    if (rx_owned_property_get_string(key, F, 64) && !cp_copy_guest(F, out, capacity)) out[0] = 0;
    RX_FRAME_LEAVE(F, 64);
}

/* The current round's activity: the race the event set up (+ 0x14 once its results were shown, else the local player's). */
static rx_addr cp_heat_round_activity(struct rx_session* s, rx_addr e) {
    const rx_addr a = U32(e, 0x14);
    if (a && cp_activity_layout(a, e)) return a;
    const rx_addr local = U32(s->game + 0x7460, 0x94);
    return cp_heat_live_layout(s, local) ? local : 0;
}

static int cp_heat_status(struct rx_session* s, rx_career_heat_status* out) {
    const rx_addr m = CP_SERIES_MANAGER;
    const rx_addr e = U32(m, 0x18c);
    if (s->career_phase != RX_CAREER_PHASE_STARTED || !cp_manager_heat() || U32(e, 0) != CP_EVENT_HEAT_VTABLE) return RX_CAREER_UNSUPPORTED;
    rx_career_heat_status copy;
    memset(&copy, 0, sizeof copy);
    copy.size = sizeof copy;
    copy.version = RX_CAREER_HEAT_VERSION;
    copy.generation = s->career_presentation.generation;
    copy.series_id = U32(m, 0x20);
    copy.sub_event = U32(e, 4);
    copy.phase = U32(e, 0xc);
    copy.next_phase = U32(e, 0x10);
    copy.post_complete = U32(e, 0x30);
    copy.advance_pending = U32(e, 0x28);
    copy.round_continue_submitted = s->heat_round_submitted;
    copy.round_post_observed = s->heat_round_post_observed;
    copy.gate_active = U32(e, 0x24);
    copy.gate_cursor = U32(e, 0x34);
    copy.gate_count = U32(e, 0x3c);
    copy.device = -1;
    cp_property_text(0x0094c99c, copy.event_status, sizeof copy.event_status);
    cp_property_text(CP_KEY_ROUTING_MESSAGE, copy.routing_message, sizeof copy.routing_message);
    copy.activity_area_required = strcmp(copy.routing_message, "Select Activity Area") == 0;
    if (copy.gate_cursor > 24 || copy.gate_count > RX_CAREER_HEAT_GATES || copy.gate_active > 1) return RX_CAREER_BAD_STATE;
    const rx_addr table = rx_career_player_table(m, 2);
    const uint32_t rows = rx_tbl_row_count(table);
    if (rows > RX_CAREER_HEAT_ROWS) return RX_CAREER_BAD_STATE;
    /* the gate pick due: the event's ordered player rows (+ 0x40, cursor + 0x34, count + 0x38); a local human's controller */
    if (copy.gate_active && copy.gate_cursor < U32(e, 0x38)) {
        const uint32_t row = U32(e, 0x40 + 4 * copy.gate_cursor);
        if (row >= rows) return RX_CAREER_BAD_STATE;
        if (act_cell(table, 0x00936ef4, row) == 0) copy.device = (int32_t)act_cell(table, 0x0093ded0, row);
    }
    for (unsigned i = 0; i < RX_CAREER_HEAT_GATES; i++) copy.gates[i] = I32(e, 0x70 + 4 * i);
    copy.row_count = rows;
    for (uint32_t row = 0; row < rows; row++) {
        rx_career_heat_row* r = &copy.rows[row];
        r->row = (int32_t)row;
        r->player_type = (int32_t)act_cell(table, 0x00936ef4, row);          /* PlayerType */
        r->player_local = (int32_t)act_cell(ACT_PLAYER_LIST, 0x0093de90, row); /* the Player List's PlayerLocal, as vf13 reads it */
        r->player_state = (int32_t)act_cell(table, 0x0094d650, row);         /* PlayerState */
        r->requested_state = (int32_t)act_cell(table, 0x0094d670, row);      /* RequestedPlayerState */
        const uint32_t bits = act_cell(table, 0x0094d6cc, row);              /* SortValue */
        memcpy(&r->sort_value, &bits, sizeof bits);
        cp_require(cp_copy_guest(rx_tbl_str_cstr(rx_tbl_cell_string(act_column(table, 0x00936f00), row)), r->name, sizeof r->name),
                   "career API Heat player name exceeds DTO");
    }
    /* the round's results: its finishing order once the activity is in its Post state */
    const rx_addr a = cp_heat_round_activity(s, e);
    if (a && U32(a, 0x21c) == 4) {
        const uint32_t count = U32(a, 0x26c);
        if (count > RX_CAREER_HEAT_GATES) return RX_CAREER_BAD_STATE;
        copy.result_count = count;
        for (uint32_t rank = 0; rank < count; rank++) {
            const rx_addr driver = rx_stand(a, rank);
            cp_require(driver != 0, "career API Heat result driver missing");
            const rx_addr player = U32(driver, 0x74);
            cp_require(player != 0, "career API Heat result player missing");
            rx_career_heat_result* r = &copy.results[rank];
            r->rank = (int32_t)rank;
            r->player_type = I32(player, 0x10);
            r->dnf = (int32_t)cp_result_dnf(a, driver);
            r->requested_state = -1;
            cp_require(cp_copy_guest(player + 0x14, r->name, sizeof r->name), "career API Heat result name exceeds DTO");
            for (uint32_t row = 0; row < rows; row++) {
                if (copy.rows[row].player_type == r->player_type && strcmp(copy.rows[row].name, r->name) == 0) {
                    r->requested_state = copy.rows[row].requested_state;
                }
            }
        }
    }
    *out = copy;
    return RX_CAREER_OK;
}

/* the witness of the round's "Post State Complete" (the typed dispatch after PostActivityResultsPageDone): OnPostStateComplete */
static void cp_heat_round_post_witness(void* context, uint64_t generation, rx_addr activity) {
    struct rx_session* s = context;
    const rx_addr e = s->heat_round_event;
    if (s != g_career_live_session || s->state == RX_SESSION_ERROR || generation != s->career_presentation.generation) return;
    if (!s->heat_round_submitted || activity != s->heat_round_activity || U32(CP_SERIES_MANAGER, 0x18c) != e) return;
    if (!cp_manager_heat() || U32(e, 4) != 0 || U32(e, 0x14) != activity) return;
    s->heat_round_post_observed = 1;
}

static int cp_heat_action(struct career_public_call* c) {
    struct rx_session* s = c->s;
    rx_career_heat_status v;
    int rc = cp_heat_status(s, &v);
    if (rc) return rc;
    if (v.phase != c->moto_phase || v.next_phase != c->moto_sub || v.gate_cursor != c->moto_cursor) return RX_CAREER_STALE;
    /* the hub request the page was painted for */
    const rx_career_host_request* receipt = NULL;
    const rx_career_host_queue* q = &g_career_public_queue;
    for (uint32_t i = 0; i < q->count; i++) {
        const rx_career_host_request* r = &q->records[(q->first + i) % RX_CAREER_HOST_QUEUE_CAPACITY];
        if (r->id == c->id && r->generation == c->generation) receipt = r;
    }
    if (!receipt) return RX_CAREER_NOT_FOUND;
    if (receipt->kind != RX_CAREER_HOST_HUB) return RX_CAREER_WRONG_KIND;
    if (!receipt->delivered) return RX_CAREER_NOT_DELIVERED;
    if (receipt->state != RX_CAREER_HOST_ACCEPTED) return RX_CAREER_BAD_STATE;
    const rx_addr e = U32(CP_SERIES_MANAGER, 0x18c);
    const rx_career_single_dependencies* d = rx_career_event_dependencies();
    const int next_race = v.next_phase == 3 || v.next_phase == 4 || v.next_phase == 5;
    switch (c->value) {
    case RX_HEAT_GATE_BEGIN:
        /* the gate page (vf9 prepared the order: + 0x38 rows) for the next race: MXSeriesManager::ProcessGateSelection */
        if (v.post_complete || !U32(e, 0x38) || v.gate_active || !next_race || strcmp(v.routing_message, "Goto GateSelection") != 0)
            return RX_CAREER_BAD_STATE;
        rx_career_event_process_gate(e, d);
        return RX_CAREER_OK;
    case RX_HEAT_GATE_SELECT: {
        if (!v.gate_active || v.device != (int32_t)s->career_controller || c->moto_gate < 0 || c->moto_gate == 0x7fffffff)
            return RX_CAREER_BAD_STATE;
        int available = 0;
        for (uint32_t i = 0; i < v.gate_count; i++) available |= v.gates[i] == c->moto_gate;
        if (!available) return RX_CAREER_CONFLICT;
        rx_career_event_assign_gate(e, s->career_controller, c->moto_gate);   /* AssignPlayerGate 005a9e00 */
        rx_career_event_advance_gate(e, d);                                    /* AdvanceGateSelection 005a9ce0 */
        return RX_CAREER_OK;
    }
    case RX_HEAT_ROUND_CONTINUE: {
        /* PostEventMenu:SeriesContinue after a heat / LCQ ("HeatOrLCQ"): App.PostActivityResultsPageDone (00742aa0) */
        if (s->heat_round_submitted) return RX_CAREER_REPEAT;
        if ((v.phase != 3 && v.phase != 4) || (v.next_phase != 4 && v.next_phase != 5) || v.gate_active ||
            strcmp(v.event_status, "HeatOrLCQ") != 0)
            return RX_CAREER_BAD_STATE;
        if (rx_career_host_queue_pending(&g_career_public_queue)) return RX_CAREER_BUSY;
        const rx_addr a = U32(e, 0x14);
        if (!cp_heat_live_layout(s, a) || U32(a, 0x21c) != 4 || !cp_continue_allowed(s, a) || s->career_completion.ticket)
            return RX_CAREER_BAD_STATE;
        if (!rx_career_completion_bind(s, s->career_presentation.generation, U32(a, 0x290), a, cp_heat_round_post_witness))
            return RX_CAREER_UNSUPPORTED;
        s->heat_round_event = e;
        s->heat_round_activity = a;
        s->heat_round_submitted = 1;
        s->heat_round_post_observed = 0;
        ACT_CALL(0, rx_app_post_activity_results_page_done());
        return RX_CAREER_OK;
    }
    case RX_HEAT_NEXT:
        /* PostEventOverlay:OnPostStateComplete, "HeatOrLCQ": MXSeriesManager::HandleAdvanceNextSubEvent (005aad40) */
        if (!s->heat_round_submitted || !s->heat_round_post_observed || s->heat_round_event != e) return RX_CAREER_BAD_STATE;
        if (strcmp(v.event_status, "HeatOrLCQ") != 0 || v.gate_active) return RX_CAREER_BAD_STATE;
        if (rx_career_host_queue_pending(&g_career_public_queue)) return RX_CAREER_BUSY;
        rx_career_completion_unbind(s);
        s->heat_round_submitted = 0;
        s->heat_round_post_observed = 0;
        s->heat_round_event = 0;
        s->heat_round_activity = 0;
        rx_career_event_next_sub_event(e, d);
        return RX_CAREER_OK;
    default:
        return RX_CAREER_INVALID;
    }
}

/* rx_session_career_loading_enter: the presenter's AAA loading page is up for the hub whose routing is "Select Activity Area"; the
 * page's entry runs (MXUIAAALoadingPage::vf4 005a8030: the activity area re-created, game state 9). Heat only (the only series
 * format that changes riders between its races). */
static int cp_loading_enter(struct rx_session* s) {
    rx_career_transition_status* t = &s->career_transitions;
    if (s->career_phase != RX_CAREER_PHASE_STARTED) return RX_CAREER_BAD_STATE;
    if (!cp_manager_heat()) return RX_CAREER_UNSUPPORTED;
    if (!(s->career_capabilities & RX_CAREER_PRESENT_HEAT)) return RX_CAREER_UNSUPPORTED;
    if (t->loading_state != RX_CAREER_LOADING_INACTIVE && t->loading_state != RX_CAREER_LOADING_FINISHED) return RX_CAREER_BAD_STATE;
    if (t->rules_state != RX_CAREER_RULES_NONE && t->rules_state != RX_CAREER_RULES_FINISHED) return RX_CAREER_BAD_STATE;
    if (!cp_property_is(CP_KEY_ROUTING_MESSAGE, "Select Activity Area") || U32(s->game, 0x18) != 2) return RX_CAREER_BAD_STATE;
    if (rx_career_host_queue_pending(&g_career_public_queue)) return RX_CAREER_BUSY;
    ACT_CALL(0, rx_aaa_loading_page_enter(0));
    rx_owned_loading_native_entry_completed();   /* -> cp_loading_entered: the loading page is the hub's owner now */
    return RX_CAREER_OK;
}
