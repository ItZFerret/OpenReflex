/* career_aaa_loading.c -- MXUIAAALoadingPage::vf4 005a8030, the entry of the in-game "MXUIAAALoading Page" (InGamePageGraph: the
 * hub's edge "Select Activity Area"; layer AAALoadingLayer.lyr, script AAALoading.lua). A series event routes there when its next
 * race needs other riders (MXSeriesEvent_OnAdvanceNextSubEvent 005aad40); the page's entry re-creates the activity area with them
 * (build_activity_aaa.c: game states 9 / 10).
 *
 * PRODUCT BOUNDARY (UI): the page's own part is UIContainer::vf4 005cf5c0 (UIComponent::vf4 005c6e70: the component's Lua "Present"
 * through 005c3970 / 00744c80 / 00744d50, then vf4 of each child component +0xcc): it writes only the UI component tree and the UI
 * Lua state, which no game code reads. The product's presenter shows the page (Unity OwnAaaLoading); the oracle runs the original.
 * 00805120 (1, 1) is the audio pause boundary (build_boundary_audio.h). Everything else is the game's and is ported here. */
#include "career_single.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_boundary_audio.h"
#include "fx.h"

extern const rx_career_single_dependencies* rx_career_event_dependencies(void);

/* 005a8030 (thiscall page; RET 0). Frame: sub esp, 0x18; push ebx / ebp / esi / edi. The series branch (the manager's mode
 * 0x009e2318 is 1 = Tour or 2 = Arcade): the activity table cleared (004ecdb0), the engine frame mark (0084ea00), the current event's
 * next riders marked active (005a8af0 with reset 1), the activity area set up (0075c430), Game_StartAAACreation(type none 0x3e,
 * mode 1). The other branch (an exhibition event's activity started from the UI properties EventType / TrackId: 005964b0, 0057abe0,
 * MXUIManager::StartEvent) belongs to the front end's event start and is not reached by a career. */
void rx_aaa_loading_page_enter(rx_addr page) {
    ACT_FRAME(F, 0x18, 4);
    act_ui_call(0x005cf5c0, RX_THISCALL, page, 0, 0);                        /* UIContainer::vf4 (UI) */
    rx_audio_pause_flags(1, 1);                                              /* 00805120 (audio) */
    (void)rx_fx_series_manager();                                            /* the manager's lazy construction (005b2370) */
    const uint32_t mode = U32(0, 0x009e2318);
    if (mode != 2 && mode != 1) act_unreached(0x005a8086, "the AAA loading page of an exhibition event (the front end's event start)");
    ACT_CALL(1, rx_tbl_table_clear_rows(ACT_ACTIVITY_TBL));                  /* 004ecdb0 */
    ACT_CALL(0, rx_engine_frame_mark(U32(U32(0, 0x00a95844), 0x10)));        /* 0084ea00 */
    (void)rx_fx_series_manager();
    ACT_CALL(2, (void)rx_career_event_setup_next_players(U32(0, 0x009e2314), 1, rx_career_event_dependencies()));   /* 005a8af0 */
    ACT_CALL(0, rx_game_setup_activity_area(U32(0, 0x00a946b8)));            /* Game_SetupActivityArea 0075c430 (ESI = game) */
    ACT_CALL(0, rx_game_start_aaa_creation(0x3e, 1));                        /* Game_StartAAACreation 0075a380 (EAX = 0x3e, ECX = 1) */
    ACT_END(F);
}
RX_FUNCTION(0x005a8030, rx_aaa_loading_page_enter)
