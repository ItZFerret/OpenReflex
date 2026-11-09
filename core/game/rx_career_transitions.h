#ifndef RX_CAREER_TRANSITIONS_H
#define RX_CAREER_TRANSITIONS_H
#include "rx_career_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Additive API: rx_career_api.h and every existing DTO remain byte-identical.
 * This successor requires Rules capability in configure, even when a particular
 * National phase selects no popup. No native Pre hold is bypassed. AAA entry
 * remains unsupported. */
#define RX_CAREER_PRESENT_RULES 128u
#define RX_CAREER_TRANSITIONS_REQUIRED_CAPABILITIES 243u
#define RX_CAREER_TRANSITIONS_VERSION 1u
enum rx_career_rules_state {
 RX_CAREER_RULES_NONE=0,RX_CAREER_RULES_REQUESTED=1,
 RX_CAREER_RULES_SHOWN=2,RX_CAREER_RULES_WIPING=3,
 RX_CAREER_RULES_HIDE_REQUESTED=4,RX_CAREER_RULES_ENDED=5,
 RX_CAREER_RULES_FINISHED=6,RX_CAREER_RULES_REJECTED=7
};
enum rx_career_loading_state {
 RX_CAREER_LOADING_INACTIVE=0,RX_CAREER_LOADING_ACTIVE=1,
 RX_CAREER_LOADING_WIPING=2,RX_CAREER_LOADING_ROUTE_HUB=3,
 RX_CAREER_LOADING_HUB_ENTERED=4,RX_CAREER_LOADING_FINISHED=5,
 RX_CAREER_LOADING_REJECTED=6
};
typedef struct rx_career_transition_status {
 uint32_t size,version;
 uint64_t generation,rules_request_id,rules_wipe_id,loading_wipe_id;
 uint32_t rules_state,do_not_show_chosen,loading_state,loading_entry_supported;
 char rules_type[64];
 /* At Rules HIDE_REQUESTED: HideRaceRulesPopup. At loading ROUTE_HUB:
  * RoutedGotoHub. This mid-wipe action is copied, not blocked behind Out. */
 char required_ui_event[64];
} rx_career_transition_status;
RX_API int rx_session_career_transitions(struct rx_session*,uint64_t,rx_career_transition_status*);
/* Requires the delivered ShowRaceRulesPopup request already ACKed as shown.
 * Choice1 is only for an explicit Do not show again press; changes PlayerEvent
 * only. The caller still must persist its profile through an authorized save. */
RX_API int rx_session_career_rules_accept(struct rx_session*,uint64_t,uint64_t rules_request_id,uint32_t do_not_show_again);
/* Call only after the presenter actually hides/ends the popup following In.
 * Invokes authentic00742a00 exactly once. No timer or generic ACK does this. */
RX_API int rx_session_career_rules_popup_ended(struct rx_session*,uint64_t,uint64_t rules_wipe_id);
/* Deliberately UNSUPPORTED: full native AAA entry/loading states9/10 unported.
 * Does not activate a page, change game state, clear a table or queue a wipe. */
RX_API int rx_session_career_loading_enter(struct rx_session*,uint64_t);
/* Only usable after a future authentic native page entry activates the internal
 * AAALoading context and a real GotoHub produces its named wipe. At In the
 * presenter performs RoutedGotoHub, then reports actual Hub page entry here. */
RX_API int rx_session_career_loading_hub_entered(struct rx_session*,uint64_t,uint64_t loading_wipe_id);
#ifdef __cplusplus
}
#endif
#endif
