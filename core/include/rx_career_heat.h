#pragma once
#include <stdint.h>
#include "rx_career_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The Heat format (Series rows 41-46, MXSeriesEventHeat 0x0094e28c; 2026-10-26, w-heat). Additive: no existing struct changes.
 *
 * A Heat event is up to four races in the same world: the practice qualifier (CareerPracticePopup PRACTICE; its best laps order the
 * gate picks), the heat (the local rider's 12 of the 24 riders; the other heat is decided by the table order), the LCQ (when the
 * local rider placed 6th-11th), the main event (the heats' 1-5, the LCQ's 1-2). Every race starts with the gate selection. Between
 * the rounds the riders change, so the original re-creates the activity area (UIRoutingMessage "Select Activity Area": the in-game
 * MXUIAAALoading page, game states 9 / 10).
 *
 * The presenter's pages, in the original's order (InGame PostEventMenu.lua / PostEventOverlay.lua / AAALoading.lua):
 *   - the practice choice before Start: rx_session_career_moto_practice_choice (the same CareerPracticePopup as the Moto format);
 *   - a hub with routing "Goto GateSelection": the gate page, RX_HEAT_GATE_BEGIN then the local rider's RX_HEAT_GATE_SELECT;
 *   - a hub after a heat / LCQ with SeriesEventStatus "HeatOrLCQ" (routing "GotoPostEvent"): the round's results (rx_session_career_status
 *     rows) and CONTINUE = RX_HEAT_ROUND_CONTINUE (PostEventMenu:SeriesContinue -> App.PostActivityResultsPageDone); once the round's
 *     Post state completed (round_post_observed) RX_HEAT_NEXT (OnPostStateComplete -> HandleAdvanceNextSubEvent);
 *   - a hub with routing "Select Activity Area" (activity_area_required): the AAA loading page; the presenter calls
 *     rx_session_career_loading_enter once it is up, keeps it up while the world re-creates the area (the session is LOADING), and
 *     answers the "AAALoading" page wipe (rx_career_transitions.h); RoutedGotoHub then delivers the next round's gate hub;
 *   - after the main event ("MainEvent") or a lost round ("Failed"): the results page and rx_session_career_continue, then the
 *     completion (rx_career_completion.h), as for the other formats.
 * Each action names the hub request it answers and the phase / next phase / gate cursor it was painted for. */
#define RX_CAREER_HEAT_VERSION 1u
#define RX_CAREER_HEAT_ROWS 24u
#define RX_CAREER_HEAT_GATES 16u
typedef struct {
    int32_t row, player_type, player_local, player_state, requested_state;
    float sort_value;
    char name[128];
} rx_career_heat_row;
/* A row of the current round's results: the activity's finishing order (MXActivityRace + 0x2d0) and where vf13 sent each rider
 * (RequestedPlayerState: 8 the main event, 6 / 7 the LCQ, 9 / 10 out; 0 before the round's results). */
typedef struct {
    int32_t rank, player_type, dnf, requested_state;
    char name[64];
} rx_career_heat_result;
typedef struct {
    uint32_t size, version;
    uint64_t generation;
    uint32_t series_id, sub_event, phase, next_phase, post_complete, advance_pending;
    uint32_t round_continue_submitted;   /* RX_HEAT_ROUND_CONTINUE was accepted for the current round */
    uint32_t round_post_observed;        /* that round's Post state completed ("Post State Complete") */
    uint32_t gate_active, gate_cursor, gate_count;
    int32_t device;                      /* the controller whose gate pick is due, -1 none */
    uint32_t activity_area_required;     /* the routing message is "Select Activity Area" */
    uint32_t row_count;
    char event_status[32];               /* SeriesEventStatus */
    char routing_message[32];            /* UIRoutingMessage */
    int32_t gates[RX_CAREER_HEAT_GATES];
    rx_career_heat_row rows[RX_CAREER_HEAT_ROWS];
    uint32_t result_count;               /* the current round's activity drivers once its results are shown (Post), else 0 */
    rx_career_heat_result results[RX_CAREER_HEAT_GATES];
} rx_career_heat_status;
#ifdef __cplusplus
static_assert(sizeof(rx_career_heat_row) == 152, "Heat row ABI");
static_assert(sizeof(rx_career_heat_result) == 80, "Heat result ABI");
static_assert(sizeof(rx_career_heat_status) == 5136, "Heat status ABI");
#else
_Static_assert(sizeof(rx_career_heat_row) == 152, "Heat row ABI");
_Static_assert(sizeof(rx_career_heat_result) == 80, "Heat result ABI");
_Static_assert(sizeof(rx_career_heat_status) == 5136, "Heat status ABI");
#endif
enum { RX_HEAT_GATE_BEGIN = 1, RX_HEAT_GATE_SELECT = 2, RX_HEAT_NEXT = 3, RX_HEAT_ROUND_CONTINUE = 4 };
/* RX_CAREER_UNSUPPORTED when the current series is not a started Heat series. */
RX_API int rx_session_career_heat_status(struct rx_session*, uint64_t generation, rx_career_heat_status* out);
RX_API int rx_session_career_heat_action(struct rx_session*, uint64_t generation, uint64_t hub_request, uint32_t action,
                                         uint32_t phase, uint32_t next_phase, uint32_t gate_cursor, int32_t gate);
#ifdef __cplusplus
}
#endif
