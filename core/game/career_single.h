#pragma once
#include "rx_game.h"

/* Explicit composition seams, never an original-code fallback. Implementations
 * must be the audited native owners named below. UI callbacks publish genuine
 * requested presentation; they must not invent standings, success or rewards. */
typedef struct rx_career_single_dependencies {
    rx_addr (*manager)(void);                                  /* 00413d20 + lazy005b2370 */
    rx_addr (*player_table)(rx_addr manager, uint32_t selector); /* 005b6840 */
    void (*apply_player_states)(rx_addr manager);               /*005b39a0*/
    void (*request_player_states)(rx_addr manager, uint32_t from, uint32_t to); /*005b3890*/
    void (*refresh_standings)(rx_addr manager);                 /* 005b43c0 */
    void (*standings)(rx_addr manager, int32_t destination, int32_t source, int32_t multiplier); /*005b55d0*/
    void (*clear_standings)(rx_addr manager, int32_t selector); /*005b6370*/
    void (*seed_standings)(rx_addr manager, int32_t selector); /*005b4d30*/
    uint32_t (*ui_get_int)(rx_addr key, uint32_t initial);       /*005dc0c0 actual TrackId*/
    void (*ui_string)(rx_addr key, rx_addr value);               /*005dbf50*/
    void (*ui_int)(rx_addr key, uint32_t value);                 /*005dbfb0*/
    void (*ui_event)(rx_addr name);                           /*005db500 + real registry dispatch*/
    void (*ui_goto_hub)(void);                                 /*005db500/004020c0, presentation only*/
} rx_career_single_dependencies;

uint32_t rx_career_single_player_state(uint32_t phase);          /*005afbf0 RET4*/
int32_t rx_career_event_driver_row(rx_addr driver, rx_addr rows, rx_addr ordinal_out); /*005ab030*/
void rx_career_single_reset(rx_addr event, const rx_career_single_dependencies* deps); /*005afbd0*/
void rx_career_single_advance(rx_addr event, const rx_career_single_dependencies* deps); /*005aff00*/
void rx_career_single_post_event(rx_addr event, const rx_career_single_dependencies* deps); /*005affb0*/

void rx_career_event_reset_fields(rx_addr event); /*005a87b0*/
void rx_career_single_gate_result(rx_addr event, uint32_t value, const rx_career_single_dependencies* deps); /*005afd00*/

void rx_career_single_bind(rx_addr event, rx_addr channel, uint32_t attach, const rx_career_single_dependencies* deps); /*005afc20/90*/
void rx_career_single_holeshot(rx_addr event, uint32_t enable, const rx_career_single_dependencies* deps); /*005b03e0*/

uint32_t rx_career_find_player_row(rx_addr name, uint32_t type); /*005aac00; lookup only, not insertion*/
void rx_career_event_setup_activity(rx_addr event, uint32_t requested, uint32_t mode, uint32_t length, uint32_t limit_kind, const rx_career_single_dependencies* deps); /*005a9290*/

void rx_career_event_gate(rx_addr event, const rx_career_single_dependencies* deps); /*005a8a10*/
void rx_career_gate_reset_actors(void); /*005ac5d0*/

void rx_career_event_process_gate(rx_addr event, const rx_career_single_dependencies* deps); /*005a9a30*/

void rx_career_event_prepare_gate(rx_addr event, const rx_career_single_dependencies* deps); /*005a9640*/
uint32_t rx_career_gate_row_less(uint32_t first, uint32_t second, const rx_career_single_dependencies* deps); /*005b0590*/

void rx_career_event_base_reset(rx_addr event, const rx_career_single_dependencies* deps); /*005a88a0*/
void rx_career_event_initialize(rx_addr event); /*005a8850*/
void rx_career_sort_gate_rows(rx_addr first, rx_addr last, int32_t ideal, const rx_career_single_dependencies* deps); /*005c1980*/
void rx_career_sort_gate_values(rx_addr first, rx_addr last, int32_t ideal); /*005c1a80*/
uint32_t rx_career_event_current_activity_type(rx_addr event); /*005ad830*/
void rx_career_single_set_holeshot_player(rx_addr event, rx_addr player); /*005b0490*/
void rx_career_single_set_type(rx_addr event, uint32_t type); /*005b04a0*/
void rx_career_single_set_format(rx_addr event, uint32_t format); /*005b04e0*/
void rx_career_event_uninitialize(rx_addr event, const rx_career_single_dependencies* deps); /*005a8900*/
uint32_t rx_career_local_player_count(void); /*004de4d0*/
uint32_t rx_career_class_rider_limit(int32_t vehicle_class); /*005a1290*/
uint32_t rx_career_event_rider_limit(uint32_t event_type,const rx_career_single_dependencies* deps); /*005a1330*/
void rx_career_event_prepare(rx_addr event,const rx_career_single_dependencies* deps); /*005ab3e0 slot23*/
void rx_career_event_restart(rx_addr event,const rx_career_single_dependencies* deps); /*005aa150*/
uint32_t rx_career_event_setup_next_players(rx_addr event,uint32_t reset,const rx_career_single_dependencies* deps); /*005a8af0*/
void rx_career_event_next_sub_event(rx_addr event,const rx_career_single_dependencies* deps); /*005aad40*/
int32_t rx_career_event_player_by_name(rx_addr name,rx_addr table); /*005aab30*/
void rx_career_event_result_sort_columns(rx_addr vector); /*005aafc0 vf22*/
void rx_career_event_select_standings(rx_addr sorted,int32_t series,const rx_career_single_dependencies* deps); /*005ab290*/
void rx_career_event_complete_results(rx_addr event,const rx_career_single_dependencies* deps); /*005aa440*/
void rx_career_event_advance_victory(rx_addr event,const rx_career_single_dependencies* deps); /*005aa1c0*/
void rx_career_event_advance_gate(rx_addr event,const rx_career_single_dependencies* deps); /*005a9ce0*/
void rx_career_event_assign_gate(rx_addr event,uint32_t device,int32_t gate); /*005a9e00*/

void rx_career_event_post_result(rx_addr event,const rx_career_single_dependencies* deps); /*005a9f40 vf11*/
void rx_career_single_install_dependencies(const rx_career_single_dependencies* deps);
void rx_career_single_clear_dependencies(void);
void rx_career_single_construct_at(rx_addr event); /*inline005b2370; unregistered*/
rx_addr rx_career_event_scalar_delete(rx_addr event,uint32_t flags); /*005a8790*/
uint32_t rx_career_single_event_call(rx_addr event,uint32_t slot,uint32_t argument);
