/* Actual event/class capacity queries. Native semantic owner; no host roster. */
#include "career_single.h"
#include "rider_cap.h"
#include "build_activity_util.h"
#include "race_misc.h"
#include "build_activity_freeride_run.h"

uint32_t rx_career_local_player_count(void) {
    uint32_t count = 0;
    for (uint32_t row = 0; row < rx_tbl_row_count(ACT_PLAYER_LIST); ++row) {
        if (act_cell(ACT_PLAYER_LIST, 0x936ef4, row) == 0 &&
            act_cell(ACT_PLAYER_LIST, 0x93de90, row) != 0) ++count;
    }
    return count;
}

static uint32_t indexed_limit(rx_addr table_name, rx_addr key_name, int32_t key) {
    const rx_addr table = rx_tbl_find(0xae0910, table_name);
    const rx_addr ref = rx_tbl_row(act_column(table, key_name), key);
    uint32_t limit = 12;
    if (U32(ref, 8) && I32(ref, 0) != -1)
        limit = act_cell(U32(ref, 8), 0x94be6c, U32(ref, 0)); /*MaxRiders*/
    /* PORT ADDITION (w-rc-core): the FIELD SIZE setting (rx_session_set_field_size, opt-in; 0 = the tables' value, the original) */
    if (rx_rc_field_size() > limit) limit = rx_rc_field_size();
    return limit;
}
uint32_t rx_career_class_rider_limit(int32_t vehicle_class) {
    return indexed_limit(0x94cfc4, 0x936cd4, vehicle_class);
}
uint32_t rx_career_event_rider_limit(uint32_t event_type,
                                   const rx_career_single_dependencies* deps) {
    ACT_FRAME(F, 8, 4);                                                   /* sub esp, 8; push ebx / ebp / esi / edi */
    /* 005a1330: the MaxRidersEventType row of the activity type's EventTypeId (004e7780), MaxRiders, else 12 */
    uint32_t event_type_id;
    ACT_CALLR(event_type_id, 1, rx_event_type_id_of(event_type));
    uint32_t limit = indexed_limit(0x94cfdc, 0x936e90, (int32_t)event_type_id);
    if (event_type == 8 && rx_game_player_mode() != 2 && rx_game_player_mode() != 3)
        limit = rx_career_local_player_count();
    if (!deps || !deps->ui_get_int)
        act_unreached(0x005dc0c0, "career event capacity needs actual UI property owner");
    /*Original initializes the property to1, not0, before GetInt.*/
    if (deps->ui_get_int(0x94cc38, 1) == 0 && rx_game_player_mode() == 0) {
        ACT_END(F);
        return rx_career_local_player_count();
    }
    ACT_END(F);
    return limit;
}
