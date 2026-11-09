/* Product-only finite composition. No original-call fallback, fake UI object,
 * default-success host callback or independent gameplay algorithm here. */
#include "career_composition.h"
#ifndef RX_ORACLE
#include "career_manager.h"
#include "career_race_start.h"
#include "career_owned_results.h"
#include "career_standings_seed.h"
#include "career_players.h"
#include "career_player_native.h"
#include "career_progress.h"
#include "career_property_bridge.h"
#include "build_activity_util.h"
#include "build_ui_world.h"
#include "fx.h"
#include "race_misc.h"
#include "race_results_table.h"
#include <string.h>
static rx_career_host host;
static uint32_t installed;
static rx_career_single_dependencies single;
static rx_career_player_dependencies players;
static rx_career_start_dependencies start;
static rx_career_route_dependencies routes;
static rx_career_lifecycle_dependencies lifecycle;
static void require(int yes,uint32_t pc,const char*what){if(!yes)act_unreached(pc,what);}
static void ready(void){require(installed,0x5b3b50,"career product composition is not installed");}
/* Native provider edges supply the same original guest argument/return depth;
 * owned UI callbacks stay at the explicit product presentation boundary. */
static rx_addr framed_player_table(rx_addr m,uint32_t selector){rx_addr result;ACT_CALLR(result,0,rx_career_player_table(m,selector));return result;}
static void framed_request_states(rx_addr m,uint32_t from,uint32_t to){ACT_CALL(2,rx_career_request_player_states(m,from,to));}
static void framed_refresh_standings(rx_addr m){ACT_CALL(0,rx_career_update_standings(m));}
static void framed_accumulate_standings(rx_addr m,int32_t dest,int32_t source,int32_t multiplier){ACT_CALL(4,rx_career_accumulate_standings(m,dest,source,multiplier));}

static rx_addr manager(void){ready();return rx_fx_series_manager();}
static void ui_event(rx_addr name){ready();require(host.ui_event(host.context,name)!=0,0x5db500,"owned career UI event request rejected");}
static void ui_route(rx_addr name){ready();require(host.ui_route(host.context,name)!=0,0x5db4c0,"owned career UI route request rejected");}
static void ui_hub(void){ready();require(host.ui_goto_hub(host.context)!=0,0x5db500,"owned career hub request rejected");rx_results_page_hub();/*the hub's GotoPostEvent edge: the results page start (race_results_table.c)*/}
static uint32_t route_accept(rx_addr route){ready();return host.route_accept(host.context,route);}
static void save(uint32_t controller){ready();require(host.request_checkpoint(host.context,controller)!=0,0x5a6910,"career checkpoint request rejected");}
static uint32_t capacity(rx_addr event){return rx_career_single_event_call(event,18,0);}
static void setup_ai(rx_addr m){rx_career_setup_ai_riders(m,&players);}
static void populate(rx_addr m){rx_career_populate_players(m,&players);}
static void refresh(rx_addr m,uint32_t advance){(void)rx_career_refresh(m,advance,rx_career_vehicle_model_id,rx_owned_property_set_int);}
static void victory(rx_addr e){rx_career_event_advance_victory(e,&single);}
static void route_build(rx_addr out,uint32_t size,uint32_t type,uint32_t mode,uint32_t pm,rx_addr prefix){rx_career_build_route(out,size,type,mode,pm,prefix,&routes);}
static uint32_t route_compare(rx_addr route){return rx_career_compare_route(U32(0xa93fcc,0),route,&routes);}
static void require_singleplayer(void){
 ready();RX_FRAME_ENTER(F,32);
 uint32_t found=rx_owned_property_get_string(0x94be84,F,32);
 int mode=found?rx_game_mode_index(F):-1;RX_FRAME_LEAVE(F,32);
 require(found&&mode==1&&rx_game_player_mode()==1&&U32(0xda6cb0,0)==0,0x5b3b50,"career composition requires explicit owned SinglePlayer and offline core");
}
void rx_career_composition_install(const rx_career_host*h){
 require(!installed,0x5b2690,"career composition already installed; explicit teardown required");
 require(h&&h->ui_event&&h->ui_route&&h->ui_goto_hub&&h->route_accept&&h->request_checkpoint,0x5b2690,"incomplete owned presentation/checkpoint host");
 /*One worker/session. Install the same finite owner at every composition seam.*/
 host=*h;memset(&single,0,sizeof single);memset(&players,0,sizeof players);memset(&start,0,sizeof start);memset(&routes,0,sizeof routes);memset(&lifecycle,0,sizeof lifecycle);
 single.manager=manager;single.player_table=framed_player_table;
 single.apply_player_states=rx_career_apply_player_states;single.request_player_states=framed_request_states;
 single.refresh_standings=framed_refresh_standings;single.standings=framed_accumulate_standings;
 single.clear_standings=rx_career_clear_standings;single.seed_standings=rx_career_seed_standings;
 single.ui_get_int=rx_career_property_int;single.ui_string=rx_owned_property_set_string;single.ui_int=rx_owned_property_set_int;
 single.ui_event=ui_event;single.ui_goto_hub=ui_hub;
 players.event_capacity=capacity;players.vehicle_model=rx_career_vehicle_model_id;players.reset_requests=rx_career_reset_player_requests;
 routes.get_int=rx_career_property_int;routes.get_string=rx_career_property_string;routes.compare_owned_route=route_accept;
 routes.session_metadata=rx_career_offline_session_metadata;
 /*No legacy GUI package providers and no stand-in level preparation.
  * Actual session-host entry supplies its asset preparation/request wrappers.*/
 start.event_call=rx_career_single_event_call;start.ui_int=rx_owned_property_set_int;start.ui_string=rx_owned_property_set_string;
 start.ui_get_int=rx_career_property_int;start.clear_nonhuman_players=rx_player_list_prune;
 start.setup_ai_riders=setup_ai;start.populate_players=populate;start.seed_standings=rx_career_seed_standings;
 start.reset_player_requests=rx_career_reset_player_requests;start.manager_refresh=refresh;
 start.build_route=route_build;start.compare_update_route=route_compare;
 lifecycle.start=&start;lifecycle.vehicle_model=rx_career_vehicle_model_id;lifecycle.advance_victory=victory;lifecycle.ui_route=ui_route;lifecycle.request_save=save;
 rx_owned_properties_begin();installed=1;rx_career_single_install_dependencies(&single);
 rx_addr m=rx_fx_series_manager(),channel=rx_ui_event_channel();
 require(channel!=0,0x5b2690,"actual UI event channel unavailable; no fake channel");
 rx_career_bind_manager_channel(m,channel);
 rx_career_owned_results_bind(m);
}
void rx_career_composition_dispose(void){
 /*Caller must first stop all dispatch/jobs and end/destroy live guest bindings.
  * This only releases product provider lifetime, never fakes original cleanup.*/
 rx_career_owned_results_clear();
 rx_career_single_clear_dependencies();rx_owned_properties_dispose();
 memset(&host,0,sizeof host);memset(&single,0,sizeof single);memset(&players,0,sizeof players);memset(&start,0,sizeof start);memset(&routes,0,sizeof routes);memset(&lifecycle,0,sizeof lifecycle);installed=0;
}
/* The series formats this composition runs: the event object 005b6900 selects for the loaded format (manager + 0x10) must be one
 * whose virtual slots rx_career_single_event_call dispatches: MXSeriesEventSingle (every Single-class format: SingleEvent,
 * Overtake, Eliminator, SpeednStyle, Outrun, Faceoff, SectionRace), MXSeriesEventMoto (Moto, + 0x288) or MXSeriesEventHeat (Heat,
 * + 0x194). Freestyle (+ 0x380) is not composed. The authored Series row must agree with the loaded manager (owner Tour, the format's
 * name). */
static int composed_text(rx_addr table, rx_addr column, uint32_t row, const char* expected) {
    rx_addr value;
    rx_addr address;
    ACT_CALLR(value, 1, rx_tbl_cell_string(act_column(table, column), row));
    ACT_CALLR(address, 1, rx_tbl_str_cstr(value));
    for (size_t i = 0; i < 64; i++) {
        const unsigned char c = (unsigned char)expected[i];
        if (U8(address, (uint32_t)i) != c) return 0;
        if (!c) return 1;
    }
    return 0;
}
static rx_addr composed_event(rx_addr m) {
    switch (U32(m, 0x10)) {
    case 0: return m + 0x288;                                          /* Moto */
    case 1: return m + 0x194;                                          /* Heat */
    case 2: case 4: case 5: case 6: case 7: case 8: case 9: return m + 0x46c;   /* the Single event */
    default: return 0;                                                 /* Freestyle, none */
    }
}
int rx_career_composition_format_supported(rx_addr m) {
    if (!U32(m, 0) || U32(m, 8) != 1) return 0;                        /* a loaded Tour series */
    const rx_addr event = composed_event(m);
    if (!event) return 0;
    const uint32_t vtable = U32(event, 0);
    if (vtable != 0x0094e2fc && vtable != 0x0094e36c && vtable != 0x0094e28c) return 0;
    rx_addr series;
    ACT_CALLR(series, 1, rx_tbl_find(RX_TBL_DEFAULT_DB, 0x0093e86c));  /* "Series" */
    uint32_t found = 0;
    uint32_t row = 0;
    for (uint32_t i = 0; i < rx_tbl_row_count(series); i++) {
        if (act_cell(series, 0x0094c990, i) == U32(m, 0x20)) {
            row = i;
            found++;
        }
    }
    if (found != 1) return 0;
    const rx_addr format_name = U32(0x009d2380, 4 * U32(m, 0x10));    /* GetMXSeriesFormat's name table */
    char expected[32];
    size_t n = 0;
    while (n + 1 < sizeof expected && U8(format_name, (uint32_t)n)) {
        expected[n] = (char)U8(format_name, (uint32_t)n);
        n++;
    }
    expected[n] = 0;
    return composed_text(series, 0x0094c9f4, row, "Tour") && composed_text(series, 0x0094d810, row, expected);
}
static int composed_moto(rx_addr m) {
    return rx_career_composition_format_supported(m) && U32(m, 0x10) == 0;
}
rx_addr rx_career_composition_load(uint32_t controller,uint32_t series_id,uint32_t owner){
 require_singleplayer();require(controller<4&&owner==1,0x5b0ad0,"only actual controller0..3 Tour is composed");
 rx_addr m=manager();rx_career_progress_load(m,controller,series_id,owner);
 require(rx_career_composition_format_supported(m),0x5b0ad0,"the loaded series format has no composed series event class");return m;
}
void rx_career_composition_select_vehicle(uint32_t excluded_type){require_singleplayer();rx_career_progress_set_vehicle(manager(),excluded_type);}
void rx_career_composition_initialize(void){
 require_singleplayer();rx_addr m=manager();const int moto=composed_moto(m);
 require(rx_career_composition_format_supported(m),0x5b4160,"load a series of a composed format before initialize");
 /* Canonical lifecycle selects format0->manager+288 and invokes typed Moto slots. */
 rx_career_commit_initialize(m,&lifecycle);
 if(moto)require(composed_moto(m)&&U32(m,0x18c)==m+0x288&&U32(U32(m,0x18c),0)==0x94e2fc&&U32(U32(m,0x18c),0xc)==1,0x5b4160,"Moto canonical initialized event identity differs");
}
int rx_career_composition_start_session(struct rx_session*s){require_singleplayer();return rx_career_session_start_owned(s,manager(),&start,&routes);}
void rx_career_composition_complete_event(void){require_singleplayer();rx_career_complete_event(manager(),&lifecycle);}
void rx_career_composition_complete_series(void){require_singleplayer();rx_career_complete_series(manager(),&lifecycle);}
void rx_career_composition_end(uint32_t save_request){require_singleplayer();rx_career_end_series(manager(),save_request,&lifecycle);}
const rx_career_single_dependencies*rx_career_composition_single_dependencies(void){ready();return &single;}
#endif

#ifndef RX_ORACLE
void rx_career_composition_begin_race(rx_addr m){
 ready();
 require(m==rx_fx_series_manager(),0x5b40e0,"career race-start manager identity mismatch");
 rx_career_begin_race(m,lifecycle.vehicle_model,start.ui_int,start.event_call);
}
#endif
