/* Included once by the isolated rx_session successor. Host API policy only;
 * actual game algorithms remain their canonical career/registry owners. */
#include "career_shop.h"
#include "rx_career_moto.h"
#include "career_moto_observation.h"
#include "career_players.h"
#include "career_manager.h"
#include "rx_career_supercross.h"
#include "rx_career_powercross.h"
#include "career_progress.h"
#include "race_results_continue.h"
#include "rx_career_series.h"
#include "rx_career_heat.h"
#include "career_heat.h"
enum career_public_operation { CP_CONFIGURE,CP_SERIES,CP_LOAD,CP_SELECT,CP_INITIALIZE,
 CP_START,CP_STATUS,CP_NEXT,CP_ACK,CP_WIPE,CP_CONTINUE,
 CP_CHECKPOINT_COMMITTED,CP_CHECKPOINT_FAILED,CP_TRANSITIONS,CP_RULES_ACCEPT,CP_RULES_ENDED,CP_LOADING_ENTER,CP_LOADING_HUB_ENTERED,CP_COMPLETION,CP_COMPLETE_EVENT,CP_FRONTEND_READY,CP_COMPLETE_SERIES,CP_VEHICLE_QUERY,CP_MOTO_STATUS,CP_MOTO_ACTION,CP_MOTO_PRACTICE,
 CP_SERIES_PRESENTATION,CP_HEAT_STATUS,CP_HEAT_ACTION };
struct career_public_call {
 struct rx_session*s;int operation,rc;uint32_t value,kind,accepted;
 uint64_t generation,id,ticket;uint32_t moto_phase,moto_sub,moto_cursor;int32_t moto_gate;rx_career_moto_status moto;char name[256];
 rx_career_config config;rx_career_series_info series;rx_career_vehicle_identity vehicle;
 rx_career_status status;rx_career_request request;rx_career_transition_status transitions;rx_career_completion_status completion;
 rx_career_series_presentation presentation;
 rx_career_heat_status heat;
};
static void cp_require(int yes,const char*what){if(!yes)act_unreached(0x5b0ad0,what);}
static void cp_text(rx_addr output,const char*text){
 for(uint32_t n=0;;n++){SU8(output,n,(uint8_t)text[n]);if(!text[n])break;}
}
static int cp_copy_guest(rx_addr text,char*out,size_t capacity){
 if(!text||!out||!capacity||text>UINT32_MAX-(uint32_t)(capacity-1))return 0;
 for(size_t n=0;n<capacity;n++){out[n]=(char)U8(text,(uint32_t)n);if(!out[n])return 1;}
 return 0;
}
static uint32_t cp_copy_name(void*context,uint32_t address,char*out,size_t capacity){
 struct rx_session*s=context;
 if(s!=g_career_live_session||!s->career_phase||s->state==RX_SESSION_ERROR)return 0;
 return (uint32_t)cp_copy_guest(address,out,capacity);
}
/* The presenter capability that owns a route the original requests (a HUD page of the race, or a front-end page of the
 * completion). 0 = no presenter can own it. The HUD routes are "Hud" + the activity name + "SinglePlayer" (005ce910). */
static uint32_t cp_route_capability(const char* name) {
    static const struct { const char* route; uint32_t capability; } routes[] = {
        { "HudNationalSinglePlayer", RX_CAREER_PRESENT_NATIONAL_HUD },
        { "HudWaypointRaceSinglePlayer", RX_CAREER_PRESENT_WAYPOINT_HUD },
        { "HudSupercrossSinglePlayer", RX_CAREER_PRESENT_SUPERCROSS_HUD },
        { "HudPowercrossSinglePlayer", RX_CAREER_PRESENT_POWERCROSS_HUD },
        { "HudTerracrossSinglePlayer", RX_CAREER_PRESENT_TERRACROSS_HUD },
        { "HudFreestyleSinglePlayer", RX_CAREER_PRESENT_FREESTYLE_HUD },
        { "HudSectionRaceSinglePlayer", RX_CAREER_PRESENT_SECTION_RACE_HUD },
        { "HudOutrunSinglePlayer", RX_CAREER_PRESENT_OUTRUN_HUD },
        { "GotoRewardsPage", RX_CAREER_PRESENT_REWARDS },
        { "GotoCareerRaceAndRidePage", RX_CAREER_PRESENT_CAREER },
    };
    for (size_t i = 0; i < sizeof routes / sizeof routes[0]; i++) {
        if (strcmp(name, routes[i].route) == 0) return routes[i].capability;
    }
    return 0;
}
static uint32_t cp_route_available(void* context, const char* name) {
    struct rx_session* s = context;
    const uint32_t bit = cp_route_capability(name);
    return bit != 0 && (s->career_capabilities & bit) != 0;
}
static uint32_t cp_ui_event(void*context,rx_addr name){
 /* No generic name-to-action bridge. The currently reached extra popup is not
  * implemented by this minimal presenter; rejection remains visible/fatal. */
 (void)context;(void)name;return 0;
}
static uint32_t cp_ui_route(void*context,rx_addr name){
 struct rx_session*s=context;
 uint32_t accepted=rx_career_presentation_ui_route(&s->career_presentation,name);
 if(accepted)rx_powercross_countdown_prepare(0);
 return accepted;
}
static uint32_t cp_loading_goto_hub(struct rx_session*);
static uint32_t cp_hub(void*context){
 struct rx_session*s=context;
 if(s->career_transitions.loading_state!=RX_CAREER_LOADING_INACTIVE&&
    s->career_transitions.loading_state!=RX_CAREER_LOADING_FINISHED&&
    s->career_transitions.loading_state!=RX_CAREER_LOADING_HUB_ENTERED)return cp_loading_goto_hub(s);
 return (s->career_capabilities&RX_CAREER_PRESENT_HUB)?rx_career_presentation_hub(&s->career_presentation):0;
}
static uint32_t cp_route_accept(void*context,rx_addr name){
 struct rx_session*s=context;char route[256];
 uint32_t accepted=rx_career_presentation_route_accept(&s->career_presentation,name);
 if(accepted&&cp_copy_guest(name,route,sizeof route))rx_powercross_countdown_prepare(!strcmp(route,"HudPowercrossSinglePlayer"));
 return accepted;
}
static uint32_t cp_checkpoint(void*context,uint32_t controller){
 struct rx_session*s=context;
 return controller==s->career_controller?rx_career_presentation_checkpoint(&s->career_presentation,controller):0;
}
/* The host attests durable publication; native validates exact delivered request.
 * This is not native filesystem verification or a generic accepted-ack bypass. */
static int cp_checkpoint_receipt(struct rx_session*s,uint64_t id,uint32_t committed){
 rx_career_host_queue*q=&g_career_public_queue;
 for(uint32_t i=0;i<q->count;i++){
  rx_career_host_request*r=&q->records[(q->first+i)%RX_CAREER_HOST_QUEUE_CAPACITY];
  if(r->id!=id||r->generation!=s->career_presentation.generation)continue;
  if(r->kind!=RX_CAREER_HOST_CHECKPOINT)return RX_CAREER_WRONG_KIND;
  if(r->controller!=s->career_controller)return RX_CAREER_INVALID;
  if(!r->delivered)return RX_CAREER_NOT_DELIVERED;
  if(r->state==RX_CAREER_HOST_ACCEPTED)return committed?RX_CAREER_REPEAT:RX_CAREER_CONFLICT;
  if(r->state!=RX_CAREER_HOST_PENDING)return RX_CAREER_CONFLICT;
  if(!committed){s->career_checkpoint_error=1;return RX_CAREER_OK;}
  int rc=rx_career_presentation_ack(&s->career_presentation,r->generation,id,RX_CAREER_HOST_CHECKPOINT,1);
  if(rc==RX_CAREER_OK)s->career_checkpoint_error=0;
  return rc;
 }
 return RX_CAREER_NOT_FOUND;
}
static void career_public_detach(struct rx_session*s){
 if(!s||!s->career_phase)return;
 if(s->state==RX_SESSION_ERROR)rx_powercross_countdown_reset();else rx_powercross_countdown_unbind();
 rx_replay_host_end(&s->replay_host);
 /* After a native fault, the synchronous callback stack has unwound. Clearing
  * this host guard permits explicit cancellation, never a replay/success. */
 rx_career_completion_unbind(s);
 rx_owned_rules_unbind(s);rx_owned_loading_unbind(s);
 memset(&s->career_transitions,0,sizeof s->career_transitions);s->career_rules_activity=0;
 s->career_presentation.dispatching=0;
 rx_career_presentation_unbind(&s->career_presentation);
 if(s->career_presentation.active)(void)rx_career_presentation_end(&s->career_presentation,1);
 rx_career_composition_dispose();
 s->career_phase=RX_CAREER_PHASE_ABORTED;
}
static int cp_entry(struct rx_session*s,uint64_t generation,int configuring){
 /* Identity is compared BEFORE dereferencing a caller-supplied stale handle.
  * Generation also rejects a recycled host address after destroy/recreate. */
 if(!s||s!=g_career_live_session||!session_ok(s))return RX_CAREER_INVALID;
 if(s->career_busy||s->career_presentation.dispatching||t_guard_env)return RX_CAREER_BUSY;
 if(s->state==RX_SESSION_ERROR)return RX_CAREER_FAULT;
 if(configuring)return s->career_phase?RX_CAREER_BAD_STATE:RX_CAREER_OK;
 if(!s->career_presentation.active)return RX_CAREER_INACTIVE;
 if(generation!=s->career_presentation.generation)return RX_CAREER_STALE;
 return RX_CAREER_OK;
}
static rx_addr cp_table(rx_addr database,rx_addr key){
 rx_addr t;ACT_CALLR(t,1,rx_tbl_find(database,key));cp_require(t!=0,"career API required actual table missing");return t;
}
static uint32_t cp_word(rx_addr t,rx_addr key,uint32_t row){
 cp_require(row<rx_tbl_row_count(t),"career API row outside actual table");
 uint32_t column;ACT_CALLR(column,1,rx_tbl_column_index(t,key));
 cp_require(column<(U32(t,0x14)-U32(t,0x10))/4,"career API required actual column missing");
 rx_addr cell;ACT_CALLR(cell,1,rx_tbl_cell_word(t,column,row));return U32(cell,0);
}
static rx_addr cp_string(rx_addr t,rx_addr key,uint32_t row){
 cp_require(row<rx_tbl_row_count(t),"career API text row outside actual table");
 uint32_t index;ACT_CALLR(index,1,rx_tbl_column_index(t,key));
 cp_require(index<(U32(t,0x14)-U32(t,0x10))/4,"career API required actual text column missing");
 rx_addr cell;ACT_CALLR(cell,1,rx_tbl_cell_string(U32(U32(t,0x10),4*index),row));return rx_tbl_str_cstr(cell);
}
static int32_t cp_find(rx_addr table,rx_addr key,int32_t value){
 int32_t found=-1;uint32_t count=rx_tbl_row_count(table);
 for(uint32_t row=0;row<count;row++)if((int32_t)cp_word(table,key,row)==value){
  cp_require(found<0,"career API expected unique authored identity");found=(int32_t)row;
 }
 return found;
}
#include "career_vehicle_session.inc"

/* ==== The series rules (w-career, 2026-10-26) ===================================================================================
 * The career API admits the series the original plays from the Race & Ride events page, by the original's own rules and the
 * authored tables, not by a list of series ids (rx_career_series.h has the rules). Everything here only READS the tables and the
 * series manager; the series is loaded by the canonical LoadSeries (career_progress.c) as before. */

#define CP_SERIES_MANAGER 0x009e2188u

/* MXSeriesFormat: the index of the format name in the table at 0x009d2380 (GetMXSeriesFormat). */
enum cp_series_format {
    CP_FORMAT_MOTO = 0,
    CP_FORMAT_HEAT = 1,
    CP_FORMAT_SINGLE_EVENT = 2,
    CP_FORMAT_FREESTYLE = 3,
    CP_FORMAT_OVERTAKE = 4,
    CP_FORMAT_ELIMINATOR = 5,
    CP_FORMAT_SPEED_AND_STYLE = 6,
    CP_FORMAT_OUTRUN = 7,
    CP_FORMAT_FACEOFF = 8,
    CP_FORMAT_SECTION_RACE = 9,
    CP_FORMAT_UNKNOWN = 10
};

/* The vtables of the series event classes (RTTI). */
#define CP_EVENT_HEAT_VTABLE      0x0094e28cu
#define CP_EVENT_MOTO_VTABLE      0x0094e2fcu
#define CP_EVENT_SINGLE_VTABLE    0x0094e36cu
#define CP_EVENT_FREESTYLE_VTABLE 0x0094e3dcu
#define CP_EVENT_VIRTUAL_SLOTS    27u        /* slots 0 .. 26 (slot 27 is the next class's complete object locator) */

/* The activity enum value for "none" (the end of the name table at 0x009d3c00). */
#define CP_ACTIVITY_NONE 0x3eu

/* The product of the Race & Ride tier that opens the challenges page instead of the events page (RaceAndRide.lua line 445:
 * "if( storeProductId == 2904 ) then -- Freeride: GotoRaceAndRideChallengesPage"). RaceAndRideChallenges:OnAcceptChallenge then
 * starts the series' location as a plain Free Ride event (EventType "FreeRide", UI SeriesId) without LoadSeries. */
#define CP_CHALLENGES_PAGE_PRODUCT 2904

struct cp_series_rules {
    int32_t row;                  /* the Series row, -1 when the series is not authored */
    int32_t product_id;
    int32_t parent_product_id;
    int32_t vehicle_class;
    int32_t event_type_id;
    int32_t point_id;
    int32_t duration;             /* SeriesEventRules Duration (-1: no rules row) */
    int32_t riders;               /* SeriesEventRules NumRiders */
    int32_t rules_rows;
    int32_t location_rows;
    uint32_t format;              /* enum cp_series_format */
    rx_addr event;                /* the series event object the manager selects for the format (005b6900) */
    uint32_t event_vtable;
    uint32_t activity_type;       /* the activity enum of EventType.ActivityName (005964b0) */
    uint32_t activity_mode;       /* the mode the event gives the first race's activity */
    uint32_t hud_capability;      /* the presenter bit of the race's HUD route */
    uint32_t format_capability;   /* the presenter bit of the format's HUD elements (0: none) */
    char owner[32];
    char format_name[32];
    char ai_format[16];
    char activity[32];
    char route[64];
    char reason[128];             /* empty when the series is admitted */
};

/* GetMXSeriesFormat: the index of `name` in the format name table at 0x009d2380 (10 names). */
static uint32_t cp_format_index(const char* name) {
    for (uint32_t index = 0; index < CP_FORMAT_UNKNOWN; index++) {
        const rx_addr text = U32(0x009d2380, 4 * index);
        uint32_t n = 0;
        while (n < 32 && U8(text, n) == (uint8_t)name[n] && name[n] != 0) n++;
        if (n < 32 && U8(text, n) == (uint8_t)name[n]) return index;
    }
    return CP_FORMAT_UNKNOWN;
}

/* 005b6900 (ECX = format, EAX = manager): the series event object of a format: Moto + 0x288, Heat + 0x194, Freestyle + 0x380,
 * every other format the Single event + 0x46c; formats above 9 have none. */
static rx_addr cp_format_event(uint32_t format) {
    switch (format) {
    case CP_FORMAT_MOTO: return CP_SERIES_MANAGER + 0x288;
    case CP_FORMAT_HEAT: return CP_SERIES_MANAGER + 0x194;
    case CP_FORMAT_FREESTYLE: return CP_SERIES_MANAGER + 0x380;
    case CP_FORMAT_SINGLE_EVENT:
    case CP_FORMAT_OVERTAKE:
    case CP_FORMAT_ELIMINATOR:
    case CP_FORMAT_SPEED_AND_STYLE:
    case CP_FORMAT_OUTRUN:
    case CP_FORMAT_FACEOFF:
    case CP_FORMAT_SECTION_RACE:
        return CP_SERIES_MANAGER + 0x46c;
    default:
        return 0;
    }
}

/* A series event class runs natively when every one of its virtual slots is a registered core function. Returns the first slot
 * that is not (or CP_EVENT_VIRTUAL_SLOTS when all are). */
static uint32_t cp_event_class_unported_slot(uint32_t vtable) {
    for (uint32_t slot = 0; slot < CP_EVENT_VIRTUAL_SLOTS; slot++) {
        if (rx_fn(U32(vtable, 4 * slot)) == NULL) return slot;
    }
    return CP_EVENT_VIRTUAL_SLOTS;
}

/* MXSeriesEventSingle::vf16 005b04e0: the activity mode a format gives the race (+0x20): Overtake 6, Eliminator 5,
 * SpeednStyle 7, Outrun 8, everything else 1. The Moto, Heat and Freestyle events keep their constructor's 1. */
static uint32_t cp_format_activity_mode(uint32_t format) {
    switch (format) {
    case CP_FORMAT_OVERTAKE: return 6;
    case CP_FORMAT_ELIMINATOR: return 5;
    case CP_FORMAT_SPEED_AND_STYLE: return 7;
    case CP_FORMAT_OUTRUN: return 8;
    default: return 1;
    }
}

/* The presenter bit of the HUD elements a format's activity mode adds to its HUD layer. */
static uint32_t cp_format_capability(uint32_t activity_mode) {
    if (activity_mode == 5) return RX_CAREER_PRESENT_ELIMINATOR;
    if (activity_mode == 6) return RX_CAREER_PRESENT_OVERTAKE;
    return 0;
}

/* 005ce910 + the route builder (career_routes.c rx_career_build_route): "Hud" + the activity's name (the table at 0x009d3c00;
 * "Outrun" for activity mode 8, "FreeRide" for mode 4) + "SinglePlayer" (the career is single player). */
static void cp_hud_route(uint32_t activity_type, uint32_t activity_mode, char* out, size_t capacity) {
    char name[32];
    rx_addr text = U32(0x009d3c00, 4 * activity_type);
    if (activity_mode == 4) text = U32(0x009d3c00, 4 * 8);
    if (activity_mode == 8) text = 0x0092d690;   /* "Outrun" */
    if (!cp_copy_guest(text, name, sizeof name)) name[0] = 0;
    snprintf(out, capacity, "Hud%sSinglePlayer", name);
}

static void cp_series_reject(struct cp_series_rules* rules, const char* reason) {
    if (rules->reason[0] == 0) snprintf(rules->reason, sizeof rules->reason, "%s", reason);
}

/* Read the rules of series `id` from the authored tables. rules->row < 0: no such series. rules->reason empty: admitted. */
static void cp_series_rules_read(uint32_t id, struct cp_series_rules* rules) {
    memset(rules, 0, sizeof *rules);
    rules->row = -1;
    rules->duration = -1;
    rules->riders = -1;
    rules->format = CP_FORMAT_UNKNOWN;
    rules->activity_type = CP_ACTIVITY_NONE;
    rules->product_id = -1;
    rules->parent_product_id = -1;

    const rx_addr series = cp_table(RX_TBL_DEFAULT_DB, 0x0093e86c);   /* "Series" */
    rules->row = cp_find(series, 0x0094c990, (int32_t)id);            /* "SeriesId" */
    if (rules->row < 0) {
        cp_series_reject(rules, "no Series row");
        return;
    }
    const uint32_t row = (uint32_t)rules->row;
    rules->product_id = (int32_t)cp_word(series, 0x00936ed4, row);     /* "StoreProductId" */
    rules->vehicle_class = (int32_t)cp_word(series, 0x00936cd4, row);  /* "VehicleClassId" */
    rules->event_type_id = (int32_t)cp_word(series, 0x00936e90, row);  /* "EventTypeId" */
    rules->point_id = (int32_t)cp_word(series, 0x0094d830, row);       /* "SeriesPointId" */
    cp_require(cp_copy_guest(cp_string(series, 0x0094c9f4, row), rules->owner, sizeof rules->owner),
               "career API authored owner name exceeds DTO");
    cp_require(cp_copy_guest(cp_string(series, 0x0094d810, row), rules->format_name, sizeof rules->format_name),
               "career API authored format exceeds DTO");
    cp_require(cp_copy_guest(cp_string(series, 0x0094d820, row), rules->ai_format, sizeof rules->ai_format),
               "career API authored AI format exceeds DTO");

    /* the EventType's activity */
    const rx_addr types = cp_table(RX_TBL_DEFAULT_DB, 0x00935e08);       /* "EventType" */
    const int32_t type_row = cp_find(types, 0x00936e90, rules->event_type_id);
    cp_require(type_row >= 0, "career API authored EventType not found");
    const rx_addr activity_name = cp_string(types, 0x0093e52c, (uint32_t)type_row);   /* "ActivityName" */
    cp_require(cp_copy_guest(activity_name, rules->activity, sizeof rules->activity), "career API activity name exceeds DTO");
    rules->activity_type = rx_event_type_of(activity_name);

    /* the store product's parent (the Race & Ride tier) */
    const rx_addr products = cp_table(RX_TBL_DEFAULT_DB, 0x0093e790);    /* "StoreProducts" */
    const int32_t product_row = cp_find(products, 0x00936ed4, rules->product_id);
    if (product_row >= 0) {
        RX_FRAME_ENTER(F, 32);
        cp_text(F, "ParentStoreProductId");
        rules->parent_product_id = (int32_t)cp_word(products, F, (uint32_t)product_row);
        RX_FRAME_LEAVE(F, 32);
    }

    /* SeriesEventRules (one row: Duration, NumRiders) and SeriesEventLocations (one row per event of the series) */
    RX_FRAME_ENTER(F, 64);
    cp_text(F, "SeriesEventRules");
    const rx_addr event_rules = cp_table(RX_TBL_DEFAULT_DB, F);
    for (uint32_t r = 0; r < rx_tbl_row_count(event_rules); r++) {
        if (cp_word(event_rules, 0x0094c990, r) != id) continue;
        rules->rules_rows++;
        cp_text(F, "Duration");
        rules->duration = (int32_t)cp_word(event_rules, F, r);
        cp_text(F, "NumRiders");
        rules->riders = (int32_t)cp_word(event_rules, F, r);
    }
    cp_text(F, "SeriesEventLocations");
    const rx_addr locations = cp_table(RX_TBL_DEFAULT_DB, F);
    for (uint32_t r = 0; r < rx_tbl_row_count(locations); r++) {
        if (cp_word(locations, 0x0094c990, r) == id) rules->location_rows++;
    }
    RX_FRAME_LEAVE(F, 64);

    /* the format's series event class and the race it sets up */
    rules->format = cp_format_index(rules->format_name);
    rules->event = cp_format_event(rules->format);
    rules->event_vtable = rules->event ? U32(rules->event, 0) : 0;
    rules->activity_mode = cp_format_activity_mode(rules->format);
    if (rules->activity_type < CP_ACTIVITY_NONE) {
        cp_hud_route(rules->activity_type, rules->activity_mode, rules->route, sizeof rules->route);
    }
    rules->hud_capability = cp_route_capability(rules->route);
    rules->format_capability = cp_format_capability(rules->activity_mode);
    if (rules->format == CP_FORMAT_HEAT) rules->format_capability |= RX_CAREER_PRESENT_HEAT;

    /* the admission, in the order the original's pages would refuse */
    if (strcmp(rules->owner, "Tour") != 0) cp_series_reject(rules, "SeriesOwner is not Tour (the events page lists Tour series)");
    if (rules->parent_product_id == CP_CHALLENGES_PAGE_PRODUCT) {
        cp_series_reject(rules, "a Race & Ride challenges Free Ride (product 2904: started as a Free Ride event, no series manager)");
    }
    if (rules->rules_rows != 1) cp_series_reject(rules, "no single SeriesEventRules row");
    if (rules->location_rows < 1) cp_series_reject(rules, "no SeriesEventLocations row");
    if (!rules->event) cp_series_reject(rules, "SeriesFormat has no series event class");
    if (rules->activity_type >= CP_ACTIVITY_NONE) cp_series_reject(rules, "EventType has no activity of the original's enum");
    if (rules->event) {
        const uint32_t slot = cp_event_class_unported_slot(rules->event_vtable);
        if (slot < CP_EVENT_VIRTUAL_SLOTS) {
            char text[128];
            snprintf(text, sizeof text, "the series event class (vtable 0x%08x) is not ported: slot %u (0x%08x)", rules->event_vtable,
                     slot, U32(rules->event_vtable, 4 * slot));
            cp_series_reject(rules, text);
        }
    }
    if (!rules->hud_capability) cp_series_reject(rules, "no presenter capability owns the race's HUD route");
}

/* The presenter capabilities a series needs. */
static uint32_t cp_series_required_capabilities(const struct cp_series_rules* rules) {
    return RX_CAREER_BASE_REQUIRED_CAPABILITIES | rules->hud_capability | rules->format_capability;
}

/* The series event the manager currently runs, when it is one of the classes the product composes (career_composition.c). */
static int cp_manager_event_is(uint32_t vtable) {
    const rx_addr event = U32(CP_SERIES_MANAGER, 0x18c);
    return event != 0 && U32(event, 0) == vtable && event == cp_format_event(U32(CP_SERIES_MANAGER, 0x10));
}

/* The activity the series event set up for its current race (005a9290): the activity type the event recorded (+0x18) and the
 * activity mode it requested (+0x20). The class's Post sub-state must be known (cp_activity_post_offset). */
static uint32_t cp_activity_post_offset(rx_addr activity);
static int cp_activity_layout(rx_addr activity, rx_addr event) {
    if (!activity || !event) return 0;
    if (U32(activity, 0x23c) != U32(event, 0x18)) return 0;
    if (U32(activity, 0x238) != U32(event, 0x20)) return 0;
    return cp_activity_post_offset(activity) != 0;
}

/* The Post sub-state field of the activity classes a career series builds (2 = the results are shown):
 * MXActivityRace and MXActivitySectionRace +0x13e640 (00569a90), MXActivityWaypointRace +0x13e618 (0057a500),
 * MXActivityFreestyle +0x13e5f8 (00563aa0). 0 = another class. */
static uint32_t cp_activity_post_offset(rx_addr activity) {
    switch (U32(activity, 0)) {
    case 0x00943554: return 0x13e640;   /* MXActivityRace */
    case 0x00944a74: return 0x13e640;   /* MXActivitySectionRace */
    case 0x00943c14: return 0x13e618;   /* MXActivityWaypointRace */
    case 0x009442cc: return 0x13e5f8;   /* MXActivityFreestyle */
    default: return 0;
    }
}
static uint32_t cp_post_substate(rx_addr activity) {
    const uint32_t offset = cp_activity_post_offset(activity);
    return offset ? U32(activity, offset) : 0;
}

/* The DNF flag of a result row, as the class's own results read it:
 * - WaypointDriver (vtable 0x0093ffe4): 0057eb80 vf61's signed lap-completion predicate (laps done < the activity's laps); the
 *   waypoint driver has no RaceDriver + 0x1128;
 * - RaceDriver (0x0093f8dc) and SectionRaceDriver (0x0093fd14, a RaceDriver): + 0x1128 (Single vf13 005affb0 reads it);
 * - FreestyleDriver (0x0093fafc, not a RaceDriver: the Single vf13's __RTDynamicCast to RaceDriver fails) has no DNF: 0. */
static uint32_t cp_result_dnf(rx_addr activity, rx_addr driver) {
    switch (U32(driver, 0)) {
    case 0x0093ffe4:
        return I32(driver, 0xf10) < I32(activity, 0xff090);
    case 0x0093f8dc:
    case 0x0093fd14:
        return U32(driver, 0x1128);
    case 0x0093fafc:
        return 0;
    default:
        cp_require(0, "career API result driver of an unknown class");
        return 0;
    }
}

/* rx_career_series_info for series `id`: the authored identity, the admission (supported) and the profile's eligibility. */
static void cp_series(struct rx_session* s, uint32_t id, rx_career_series_info* out) {
    memset(out, 0, sizeof *out);
    out->size = sizeof *out;
    out->version = RX_CAREER_API_VERSION;
    out->series_id = id;
    out->product_id = -1;
    out->blocking_product_id = -1;
    struct cp_series_rules rules;
    cp_series_rules_read(id, &rules);
    if (rules.row < 0) return;
    out->authored = 1;
    out->product_id = rules.product_id;
    out->vehicle_class = rules.vehicle_class;
    out->event_type_id = rules.event_type_id;
    snprintf(out->owner, sizeof out->owner, "%s", rules.owner);
    snprintf(out->format, sizeof out->format, "%s", rules.format_name);
    snprintf(out->activity, sizeof out->activity, "%s", rules.activity);
    out->supported = rules.reason[0] == 0;

    /* IsProductLocked (004e8700 through rx_career_shop_is_locked) for the product and every authored parent. A missing player
     * unlock row stays locked even if the authored requirement happens to be 0. */
    const rx_addr products = cp_table(RX_TBL_DEFAULT_DB, 0x0093e790);
    RX_FRAME_ENTER(F, 32);
    cp_text(F, "ParentStoreProductId");
    int32_t product = out->product_id;
    int32_t seen[64];
    unsigned visited = 0;
    while (product != -1) {
        cp_require(visited < 64, "career API product parent chain exceeded bound");
        for (unsigned n = 0; n < visited; n++) cp_require(seen[n] != product, "career API cyclic product parent chain");
        seen[visited++] = product;
        const int32_t product_row = cp_find(products, 0x00936ed4, product);
        cp_require(product_row >= 0, "career API parent product missing");
        uint32_t locked;
        ACT_CALLR(locked, 2, rx_career_shop_is_locked(U32(0, 0xd72a34), (int32_t)s->career_controller, product));
        if (locked) {
            out->locked = 1;
            if (out->blocking_product_id == -1) out->blocking_product_id = product;
        }
        product = (int32_t)cp_word(products, F, (uint32_t)product_row);
    }
    RX_FRAME_LEAVE(F, 32);

    /* A reward vehicle not yet owned is shown by the original's RewardVehiclePresentation (the garage view), which the open front
     * end does not have: such a series stays ineligible (no PC series has one: SeriesRewardsPurchase names no VehicleModel). */
    const rx_addr rewards = cp_table(RX_TBL_DEFAULT_DB, 0x0094d8bc);     /* "SeriesRewardsPurchase" */
    const rx_addr models = cp_table(RX_TBL_DEFAULT_DB, 0x0093e180);      /* "VehicleModel" */
    for (uint32_t r = 0; r < rx_tbl_row_count(rewards); r++) {
        if (cp_word(rewards, 0x0094c990, r) != id) continue;
        const int32_t asset = (int32_t)cp_word(rewards, 0x0093e63c, r);
        uint32_t purchased;
        ACT_CALLR(purchased, 1, rx_career_shop_is_purchased((int32_t)s->career_controller, asset));
        if (purchased) continue;
        for (uint32_t v = 0; v < rx_tbl_row_count(models); v++) {
            if ((int32_t)cp_word(models, 0x0093e63c, v) == asset) {
                out->unowned_reward_vehicle_count++;
                break;
            }
        }
    }

    /* RaceAndRideEvents:OnAcceptSeries: the equipped vehicle's type must be one of the class's types */
    rx_career_vehicle_identity selected = {0};
    const int vehicle_rc = cp_vehicle_identity(s, &selected);
    selected.authored_class = out->vehicle_class;
    const uint32_t required = cp_series_required_capabilities(&rules);
    out->eligible = out->supported && !out->locked && !out->unowned_reward_vehicle_count &&
                    (s->career_capabilities & required) == required &&
                    vehicle_rc == RX_CAREER_OK && cp_vehicle_supported(&selected);
}

/* rx_career_series_presentation for series `id`. */
static void cp_series_presentation(uint32_t id, rx_career_series_presentation* out) {
    memset(out, 0, sizeof *out);
    out->size = sizeof *out;
    out->version = RX_CAREER_SERIES_PRESENTATION_VERSION;
    out->series_id = id;
    struct cp_series_rules rules;
    cp_series_rules_read(id, &rules);
    out->authored = rules.row >= 0;
    out->admitted = rules.row >= 0 && rules.reason[0] == 0;
    out->format = rules.format;
    out->event_vtable = rules.event_vtable;
    out->activity_type = rules.activity_type;
    out->activity_mode = rules.activity_mode;
    out->required_capabilities = cp_series_required_capabilities(&rules);
    out->duration = rules.duration;
    out->riders = rules.riders;
    out->locations = rules.location_rows;
    out->point_id = rules.point_id;
    snprintf(out->hud_route, sizeof out->hud_route, "%s", rules.route);
    snprintf(out->ai_format, sizeof out->ai_format, "%s", rules.ai_format);
    snprintf(out->reason, sizeof out->reason, "%s", rules.reason);
}

/* ---- the Moto format (MXSeriesEventMoto, vtable 0x0094e2fc) ---------------------------------------------------------------- */

/* The manager runs a loaded Tour series of the Moto format (manager + 0x10 = 0) with its Moto event current. */
static int cp_manager_moto(void) {
    const rx_addr m = CP_SERIES_MANAGER;
    return U32(m, 0) != 0 && U32(m, 0x190) == 1 && U32(m, 0x10) == CP_FORMAT_MOTO && cp_manager_event_is(CP_EVENT_MOTO_VTABLE);
}

/* The authored class is not the dynamically resolved vehicle class. Keep the immutable canonical CP_SELECT witness and the actual
 * imported identity separate: the selection's class is the series' (manager + 0x68), its resolved class the manager's (+ 0x64). */
static int cp_moto_selected_identity(struct rx_session* s) {
    const rx_career_vehicle_identity* selected = &s->career_vehicle_selection;
    rx_career_vehicle_identity current;
    return selected->authored_class == I32(CP_SERIES_MANAGER, 0x68) &&
           selected->resolved_class >= 0 &&
           I32(CP_SERIES_MANAGER, 0x64) == selected->resolved_class &&
           cp_vehicle_identity(s, &current) == RX_CAREER_OK &&
           cp_vehicle_same(selected, &current);
}

/* The points MXSeriesEventMoto::vf13 005af2c0 gives place `place` (0-based) of a moto: 25, 22, 20, 18, then 20 - place (the
 * constants are in the code: lea edi, [esi + 0x19] ...). */
static int cp_moto_points(unsigned place) {
    if (place == 0) return 25;
    if (place == 1) return 22;
    if (place == 2) return 20;
    if (place == 3) return 18;
    return 20 - (int)place;
}

/* Both motos were scored by the original: each moto gave every one of the `drivers` riders a distinct place's points. */
static int cp_moto_scored(const rx_moto_observation* score, unsigned drivers, int both) {
    unsigned one[26] = {0};
    unsigned two[26] = {0};
    if (drivers == 0 || drivers > score->row_count) return 0;
    for (unsigned i = 0; i < drivers; i++) {
        const int a = score->rows[i].score_one;
        const int b = score->rows[i].score_two;
        if (a < 0 || a > 25 || b < 0 || b > 25) return 0;
        if (both && score->rows[i].sort_value != (float)(50 - a - b)) return 0;
        one[a]++;
        two[b]++;
    }
    for (unsigned place = 0; place < drivers; place++) {
        const int points = cp_moto_points(place);
        if (points < 0 || points > 25) return 0;
        if (one[points] != 1) return 0;
        if (both && two[points] != 1) return 0;
    }
    return 1;
}

/* Separate actual Moto discriminator. Never pretend phase 7 is Single phase 5.
 * MXSeriesEventMoto + 4 is 0 for the whole event in the original (static 0x9e2410 in .bss; no store to + 4 in 005b2370 / 005b6950 /
 * vf4 / vf5 / 005a87b0 or any MXSeriesEvent / series-manager function). Moto two is phase 7. */
static int cp_moto_final_identity(struct rx_session* s, rx_addr e, rx_addr a) {
    const rx_addr m = CP_SERIES_MANAGER;
    if (!e || !a || U32(m, 0x18c) != e || !cp_manager_moto()) return 0;
    if (U32(m, 0x1c) != s->career_controller || !cp_moto_selected_identity(s)) return 0;
    if (U32(e, 8) != 0 || U32(e, 4) != 0 || U32(e, 0xc) != 7 || !U32(e, 0x30) || U32(e, 0x14) != a) return 0;
    if (!cp_activity_layout(a, e) || U32(a, 0x21c) != 4) return 0;
    rx_career_vehicle_identity vehicle;
    if (cp_vehicle_identity(s, &vehicle) != RX_CAREER_OK || !cp_vehicle_supported(&vehicle)) return 0;
    rx_moto_observation score = {0};
    score.size = sizeof score;
    score.version = 1;
    if (rx_career_moto_observe(e, &score)) return 0;
    return cp_moto_scored(&score, U32(a, 0x26c), 1);
}

/* A Moto race of the current series is the local player's activity. */
static int cp_moto_live_layout(struct rx_session* s, rx_addr a) {
    const rx_addr e = U32(CP_SERIES_MANAGER, 0x18c);
    if (!a || !cp_manager_moto()) return 0;
    if (U32(e, 0xc) != 6 && U32(e, 0xc) != 7) return 0;
    return cp_activity_layout(a, e) && a == U32(s->game + 0x7460, 0x94);
}
static int cp_moto_final_layout(struct rx_session* s, rx_addr a) {
    return cp_moto_final_identity(s, U32(CP_SERIES_MANAGER, 0x18c), a);
}

/* ---- the Heat format (MXSeriesEventHeat, vtable 0x0094e28c; rx_career_heat.h) ---------------------------------------------- */

/* The manager runs a loaded Tour series of the Heat format (manager + 0x10 = 1) with its Heat event (+ 0x194) current. */
static int cp_manager_heat(void) {
    const rx_addr m = CP_SERIES_MANAGER;
    return U32(m, 0) != 0 && U32(m, 0x190) == 1 && U32(m, 0x10) == CP_FORMAT_HEAT && cp_manager_event_is(CP_EVENT_HEAT_VTABLE);
}
/* the phases a Heat event races: 1 the practice qualifier, 3 the heat, 4 the LCQ, 5 the main event */
static int cp_heat_race_phase(uint32_t phase) {
    return phase == 1 || phase == 3 || phase == 4 || phase == 5;
}
/* The owned UI property `key` (a string) equals `text`. */
static int cp_property_is(rx_addr key, const char* text) {
    char value[64];
    RX_FRAME_ENTER(F, 64);
    const uint32_t have = rx_owned_property_get_string(key, F, 64);
    const int copied = have && cp_copy_guest(F, value, sizeof value);
    RX_FRAME_LEAVE(F, 64);
    return copied && strcmp(value, text) == 0;
}
/* A race of the current Heat event is the local player's activity. */
static int cp_heat_live_layout(struct rx_session* s, rx_addr a) {
    const rx_addr e = U32(CP_SERIES_MANAGER, 0x18c);
    if (!a || !cp_manager_heat() || !cp_heat_race_phase(U32(e, 0xc))) return 0;
    return cp_activity_layout(a, e) && a == U32(s->game + 0x7460, 0x94);
}
/* The race whose results end the Heat event: the main event once vf13 005adde0 finished it (phase 5, + 0x30, SeriesEventStatus
 * "MainEvent"), or the heat / LCQ the local rider did not qualify from ("Failed": the original sets no + 0x30 there; its
 * PostEventOverlay:OnPostStateComplete goes back to the events page without CompleteSeriesEvent). */
static int cp_heat_final_identity(struct rx_session* s, rx_addr e, rx_addr a) {
    const rx_addr m = CP_SERIES_MANAGER;
    if (!e || !a || U32(m, 0x18c) != e || !cp_manager_heat()) return 0;
    if (U32(m, 0x1c) != s->career_controller || !cp_moto_selected_identity(s)) return 0;
    if (U32(e, 4) != 0 || U32(e, 0x14) != a || !cp_activity_layout(a, e) || U32(a, 0x21c) != 4) return 0;
    const uint32_t phase = U32(e, 0xc);
    if (phase == 5) return U32(e, 0x30) != 0 && cp_property_is(0x0094c99c, "MainEvent");
    return (phase == 3 || phase == 4) && cp_property_is(0x0094c99c, "Failed");
}

/* ---- the results of the current race ---------------------------------------------------------------------------------------- */

/* The activity whose results the current series event shows: the Single event's race (phase 5) or the Moto event's second moto,
 * set up by the event, Post-complete, owned by a local player slot. */
static rx_addr cp_results_activity(struct rx_session* s) {
    if (s->career_phase != RX_CAREER_PHASE_STARTED) return 0;
    const rx_addr manager = CP_SERIES_MANAGER;
    const rx_addr event = U32(manager, 0x18c);
    if (!U32(manager, 0) || U32(manager, 0x190) != 1 || !event) return 0;
    const rx_addr activity = U32(event, 0x14);
    const int moto = cp_moto_final_identity(s, event, activity);
    const int heat = cp_heat_final_identity(s, event, activity);
    const int single = cp_manager_event_is(CP_EVENT_SINGLE_VTABLE) && cp_activity_layout(activity, event);
    if (!moto && !heat && !single) return 0;
    if ((!heat && !U32(event, 0x30)) || U32(activity, 0x21c) != 4) return 0;
    uint32_t matched = 0;
    for (uint32_t slot = 0; slot < 4; slot++) matched |= U32(s->game + 0x7460 + slot * 0x12a0, 0x94) == activity;
    return matched ? activity : 0;
}
static int cp_continue_allowed(struct rx_session*s,rx_addr activity){
 if(!activity||s->records_finalized||s->career_continue_submitted||cp_post_substate(activity)!=2||
    rx_career_host_queue_pending(&g_career_public_queue)!=0)return 0;
 /* Same actual app context/world selection as00742aa0, checked before the
  * canonical action. Do not substitute the player's pointer for its world. */
 rx_addr context=U32(0,0xa95844);if(!context)return 0;
 rx_addr alternate=U32(context,0x20),normal=U32(context,0x1c),world=0;
 if(alternate&&U32(alternate,0x34)==1)world=U32(s->game,0x5a54);
 else if(normal&&U32(normal,0x34)==1)world=U32(s->game,0x20c8);
 return world&&I32(world,0x21da8)>0&&U32(world,0x21da4)&&U32(U32(world,0x21da4),0)==activity;
}
static void cp_status(struct rx_session*s,rx_career_status*out){
 memset(out,0,sizeof *out);out->size=sizeof *out;out->version=RX_CAREER_API_VERSION;
 out->generation=s->career_presentation.generation;out->phase=s->career_phase;
 out->controller=s->career_controller;out->session_state=(uint32_t)s->state;
 out->pending_requests=(uint32_t)rx_career_host_queue_pending(&g_career_public_queue);
 out->continue_submitted=s->career_continue_submitted;out->checkpoint_error=s->career_checkpoint_error;
 out->records_finalized=(uint32_t)s->records_finalized;out->local_rank=-1;out->selected_vehicle_class=-1;
 if(s->career_phase<RX_CAREER_PHASE_LOADED)return;
 const rx_addr m=0x9e2188,e=U32(m,0x18c),r=m+0x78;
 out->series_id=U32(m,0x20);out->manager_active=U32(m,0);out->selected_vehicle_class=I32(m,0x64);
 out->event_present=e!=0;
 if(e){out->event_phase=U32(e,0xc);out->event_next_phase=U32(e,0x10);out->post_event_complete=U32(e,0x30);}
 RX_FRAME_ENTER(F,256);
 if(rx_owned_property_get_string(0x94c99c,F,256))cp_require(cp_copy_guest(F,out->event_status,sizeof out->event_status),"career API status name exceeds DTO");
 RX_FRAME_LEAVE(F,256);
 cp_require(strlen(s->spec.location)<sizeof out->location,"career API location exceeds DTO");strcpy(out->location,s->spec.location);
 out->reward_base=U32(r,0xf4);out->reward_bonus=U32(r,0xf8);
 out->reward_credits_flag=U32(r,0xfc);out->reward_purchase_flag=U32(r,0x100);out->reward_unlock_flag=U32(r,0x104);
 out->reward_valid=out->reward_credits_flag||out->reward_purchase_flag||out->reward_unlock_flag;
 rx_addr a=cp_results_activity(s);
 rx_addr local=s->career_phase==RX_CAREER_PHASE_STARTED?U32(s->game+0x7460,0x94):0;
 if(cp_activity_layout(local,e)||cp_moto_final_layout(s,local)||cp_moto_live_layout(s,local)||cp_heat_live_layout(s,local)){
  out->activity_present=1;out->activity_state=U32(local,0x21c);
  if(out->activity_state==4)out->activity_substate=cp_post_substate(local);
 }
 if(!a)return;
 out->continue_allowed=(uint32_t)cp_continue_allowed(s,a);
 uint32_t count=U32(a,0x26c);cp_require(count<=RX_CAREER_RESULT_CAPACITY,"career API result DTO capacity exceeded");
 out->results_count=count;rx_addr standings=m+0x560;
 for(uint32_t rank=0;rank<count;rank++){
  rx_addr driver=rx_stand(a,rank);cp_require(driver!=0,"career API actual result driver missing");
  rx_addr player=U32(driver,0x74);cp_require(player!=0,"career API actual result player missing");
  rx_career_result_row*row=&out->results[rank];row->rank=(int32_t)rank;row->player_type=I32(player,0x10);row->dnf=cp_result_dnf(a,driver);
  cp_require(cp_copy_guest(player+0x14,row->name,sizeof row->name),"career API actual player name exceeds DTO");
  int32_t found=-1;
  for(uint32_t n=0;n<rx_tbl_row_count(standings);n++)if((int32_t)cp_word(standings,0x94c990,n)==-3&&
    (int32_t)cp_word(standings,0x936ef4,n)==row->player_type&&!crt_strcmp(cp_string(standings,0x936f00,n),player+0x14)){
   cp_require(found<0,"career API duplicate working standing identity");found=(int32_t)n;
  }
  cp_require(found>=0,"career API actual result standing missing");
  row->model_id=(int32_t)cp_word(standings,0x93e170,(uint32_t)found);
  row->points=cp_word(standings,0x94d6b8,(uint32_t)found);row->gold=cp_word(standings,0x94c150,(uint32_t)found);
  row->silver=cp_word(standings,0x94c144,(uint32_t)found);row->bronze=cp_word(standings,0x94c138,(uint32_t)found);
  row->holeshot=cp_word(standings,0x94d910,(uint32_t)found);
  if(row->player_type==0)out->local_rank=(int32_t)rank;
 }
}
#include "career_transition_session.inc"
#include "career_completion_session.inc"
static int cp_moto_status(struct rx_session*,rx_career_moto_status*);
static int cp_moto_action(struct career_public_call*);
static int cp_heat_status(struct rx_session*,rx_career_heat_status*);
static int cp_heat_action(struct career_public_call*);
static int cp_loading_enter(struct rx_session*);
static void cp_run(void*opaque){
 struct career_public_call*c=opaque;struct rx_session*s=c->s;c->rc=RX_CAREER_OK;
 switch(c->operation){
 case CP_CONFIGURE:{
  if(s->spec_loaded||s->records_finalized||s->level||!s->install||!s->game||U32(s->game,0x18)!=2||U32(0,0xda6cb0)) {c->rc=RX_CAREER_BAD_STATE;break;}
  if(!g_career_public_queue_initialized){rx_career_host_queue_init(&g_career_public_queue);g_career_public_queue_initialized=1;}
  s->career_controller=c->config.controller;s->career_capabilities=c->config.presenter_capabilities;
  rx_career_presentation_services services={s,cp_copy_name,cp_route_available,
   rx_career_presentation_dispatch_fade_complete,rx_career_presentation_dispatch_page_wipe_transition,
   rx_career_presentation_dispatch_page_wipe_complete};
  c->rc=rx_career_presentation_begin(&s->career_presentation,&g_career_public_queue,&services);if(c->rc)break;
  s->career_phase=RX_CAREER_PHASE_CONFIGURED;rx_career_host host;
  rx_ms_set(1.0f);   /* MAP SCALE (port): a career event is always at 100 % */
  cp_require(rx_career_presentation_bind(&s->career_presentation,&host),"career API presentation binding rejected");
  host=(rx_career_host){s,cp_ui_event,cp_ui_route,cp_hub,cp_route_accept,cp_checkpoint};
  rx_career_composition_install(&host);
  cp_transition_reset(s);cp_completion_reset(s);
  const rx_owned_rules_host rules={s,s->career_controller,cp_rules_show};rx_owned_rules_bind(&rules);
  const rx_owned_loading_host loading={s,cp_loading_entered};rx_owned_loading_bind(&loading);
  RX_FRAME_ENTER(F,192);cp_text(F,"SinglePlayer");rx_owned_property_set_string(0x94be84,F);
  cp_text(F+64,"Tour");cp_text(F+128,"SeriesOwner");rx_owned_property_set_string(F+128,F+64);
  cp_text(F+128,"InCareerMode");rx_owned_property_set_int(F+128,1);RX_FRAME_LEAVE(F,192);
  rx_ui_set_primary_player_device_index(0);rx_ensure_local_players(0,6);
  c->generation=s->career_presentation.generation;break;
 }
 case CP_SERIES:cp_series(s,c->value,&c->series);break;
 case CP_SERIES_PRESENTATION:
  cp_series_presentation(c->value, &c->presentation);
  break;
 case CP_LOAD:
  if(s->career_phase!=RX_CAREER_PHASE_CONFIGURED){c->rc=RX_CAREER_BAD_STATE;break;}
  cp_series(s,c->value,&c->series);
  if(!c->series.supported||c->series.unowned_reward_vehicle_count){c->rc=RX_CAREER_UNSUPPORTED;break;}
  if(c->series.locked){c->rc=RX_CAREER_LOCKED;break;}
  {
   /* The presenter must own every page the series' races request (its HUD route and the format's HUD elements). */
   struct cp_series_rules rules;
   cp_series_rules_read(c->value, &rules);
   const uint32_t required = cp_series_required_capabilities(&rules);
   if ((s->career_capabilities & required) != required) {
    c->rc = RX_CAREER_UNSUPPORTED;
    break;
   }
  }
  /* Catalog and direct native callers share the same imported-identity gate.
   * Propagate lookup/conflict failures; incompatible real types return UNSUPPORTED.
   * This all precedes the original load and its manager/progress mutations. */
  c->rc=cp_vehicle_identity(s,&c->vehicle);if(c->rc)break;
  c->vehicle.authored_class=c->series.vehicle_class;
  if(!cp_vehicle_supported(&c->vehicle)){c->rc=RX_CAREER_UNSUPPORTED;break;}
  (void)rx_career_composition_load(s->career_controller,c->value,1);s->career_phase=RX_CAREER_PHASE_LOADED;break;
 case CP_VEHICLE_QUERY:
  if(s->career_phase>RX_CAREER_PHASE_VEHICLE_SELECTED){c->rc=RX_CAREER_BAD_STATE;break;}
  c->rc=cp_vehicle_identity(s,&c->vehicle);break;
 case CP_SELECT:
  if(s->career_phase!=RX_CAREER_PHASE_LOADED){c->rc=RX_CAREER_BAD_STATE;break;}
  c->rc=cp_vehicle_identity(s,&c->vehicle);if(c->rc)break;
  if(c->value!=(uint32_t)c->vehicle.vehicle_type){c->rc=RX_CAREER_CONFLICT;break;}
  if(!cp_vehicle_supported(&c->vehicle)){c->rc=RX_CAREER_UNSUPPORTED;break;}
  rx_career_composition_select_vehicle(c->value);
  if(I32(0x9e2188,0x64)<0){c->rc=RX_CAREER_UNSUPPORTED;break;}
  c->vehicle.resolved_class=I32(0x9e2188,0x64);s->career_vehicle_selection=c->vehicle;
  s->career_phase=RX_CAREER_PHASE_VEHICLE_SELECTED;break;
 case CP_INITIALIZE:
  if(s->career_phase!=RX_CAREER_PHASE_VEHICLE_SELECTED){c->rc=RX_CAREER_BAD_STATE;break;}
  c->rc=cp_vehicle_identity(s,&c->vehicle);if(c->rc)break;
  if(!cp_vehicle_same(&s->career_vehicle_selection,&c->vehicle)){c->rc=RX_CAREER_CONFLICT;break;}
  if(!cp_vehicle_supported(&c->vehicle)){c->rc=RX_CAREER_UNSUPPORTED;break;}
  rx_career_composition_initialize();s->career_phase=RX_CAREER_PHASE_INITIALIZED;break;
 case CP_START:
  if(s->career_phase!=RX_CAREER_PHASE_INITIALIZED){c->rc=RX_CAREER_BAD_STATE;break;}
  c->rc=rx_career_composition_start_session(s);
  if(c->rc==0)s->career_phase=RX_CAREER_PHASE_STARTED;
  else c->rc=s->state==RX_SESSION_ERROR?RX_CAREER_FAULT:RX_CAREER_BAD_STATE;
  break;
 case CP_MOTO_PRACTICE: {
  /* CareerPracticePopup: only a Moto (or Heat) series asks, after CommitAndInitializeSeries put the event in its first phase
   * (MXSeriesEventMoto vf4/vf5: phase 1, the practice or the first gate selection), before Start. */
  const rx_addr event = U32(CP_SERIES_MANAGER, 0x18c);
  const int initialized_moto = (cp_manager_moto() || cp_manager_heat()) && U32(event, 0xc) == 1 && U32(event, 4) == 0;
  if (s->career_phase != RX_CAREER_PHASE_INITIALIZED || c->value > 1 || !initialized_moto || !cp_moto_selected_identity(s)) {
   c->rc = RX_CAREER_BAD_STATE;
   break;
  }
  }
  /* Exact semantic body of original SetSkipPractice734fe0, before real Start.
   * This is a user setting, not phase/finish/score manipulation. */
  SU32(0x9e2188,0x74,c->value);break;
 case CP_STATUS:cp_status(s,&c->status);break;
 case CP_MOTO_STATUS:c->rc=cp_moto_status(s,&c->moto);break;
 case CP_MOTO_ACTION:c->rc=cp_moto_action(c);break;
 case CP_HEAT_STATUS:c->rc=cp_heat_status(s,&c->heat);break;
 case CP_HEAT_ACTION:c->rc=cp_heat_action(c);break;
 case CP_NEXT:{
  rx_career_host_request request;c->rc=rx_career_presentation_next(&s->career_presentation,c->generation,&request);
  if(c->rc)break;
  rx_career_request*out=&c->request;memset(out,0,sizeof *out);out->size=sizeof *out;out->version=RX_CAREER_API_VERSION;
  out->generation=request.generation;out->id=request.id;out->kind=request.kind;out->state=request.state;
  out->delivered=request.delivered;out->wipe_milestone=request.wipe_milestone;out->seconds=request.seconds;
  out->color=request.color;out->controller=request.controller;memcpy(out->name,request.name,sizeof out->name);break;
 }
 case CP_ACK:
  if(c->kind==RX_CAREER_REQUEST_CHECKPOINT&&c->accepted){c->rc=RX_CAREER_UNSUPPORTED;break;}
  c->rc=rx_career_presentation_ack(&s->career_presentation,c->generation,c->id,c->kind,c->accepted);
  if(c->rc==RX_CAREER_OK)cp_transition_ack(s,c->id,c->accepted);
  if(c->rc==RX_CAREER_OK&&c->kind==RX_CAREER_REQUEST_CHECKPOINT&&!c->accepted)s->career_checkpoint_error=1;
  break;
 case CP_CHECKPOINT_COMMITTED:c->rc=cp_checkpoint_receipt(s,c->id,1);cp_completion_receipt(s,c->id,c->rc);break;
 case CP_CHECKPOINT_FAILED:c->rc=cp_checkpoint_receipt(s,c->id,0);break;
 case CP_WIPE:c->rc=cp_transition_wipe(c);break;
 case CP_TRANSITIONS:c->transitions=s->career_transitions;break;
 case CP_RULES_ACCEPT:c->rc=cp_rules_accept(s,c->id,c->value);break;
 case CP_RULES_ENDED:c->rc=cp_rules_ended(s,c->id);break;
 case CP_LOADING_ENTER:c->rc=cp_loading_enter(s);break;
 case CP_LOADING_HUB_ENTERED:c->rc=cp_loading_hub_entered(s,c->id);break;
 case CP_COMPLETION:cp_completion_status(s,&c->completion);break;
 case CP_COMPLETE_EVENT:c->rc=cp_complete_event(s,c->ticket);break;
 case CP_FRONTEND_READY:c->rc=cp_frontend_ready(s,c->ticket,c->id);break;
 case CP_COMPLETE_SERIES:c->rc=cp_complete_series(s,c->ticket);break;
 case CP_CONTINUE:
  if(s->career_continue_submitted){c->rc=RX_CAREER_REPEAT;break;}
  if(!cp_continue_allowed(s,cp_results_activity(s))){c->rc=RX_CAREER_BAD_STATE;break;}
  c->rc=cp_completion_arm(s,cp_results_activity(s));if(c->rc)break;
  /* Consume action before dispatch; a fault cannot replay a half-executed Continue. */
  s->career_continue_submitted=1;ACT_CALL(0,rx_app_post_activity_results_page_done());break;
 default:c->rc=RX_CAREER_INVALID;break;
 }
}
/* Owner-thread only. The snapshot is assembled before publishing; rejection
 * leaves the caller's buffer untouched. Missing bounded history fails closed. */
extern const rx_career_single_dependencies* rx_career_event_dependencies(void);
static int cp_moto_status(struct rx_session*s,rx_career_moto_status*out){
 rx_addr e=U32(0x9e2188,0x18c);rx_moto_observation score={0};score.size=sizeof score;score.version=1;
 if(s->career_phase!=RX_CAREER_PHASE_STARTED||rx_career_moto_observe(e,&score))return RX_CAREER_UNSUPPORTED;
 rx_career_moto_status copy={0};copy.size=sizeof copy;copy.version=2;copy.generation=s->career_presentation.generation;
 copy.series_id=score.series_id;copy.sub_event=score.sub_event;copy.phase=score.phase;copy.next_phase=score.next_phase;copy.post_complete=score.post_complete;copy.row_count=score.row_count;
 copy.first_continue_submitted=s->moto_first_submitted;copy.first_post_observed=s->moto_first_post_observed;copy.gate_active=U32(e,0x24);copy.gate_cursor=U32(e,0x34);copy.gate_count=U32(e,0x3c);copy.device=-1;
 if(copy.gate_cursor>12||copy.gate_count>16||copy.gate_active>1)return RX_CAREER_BAD_STATE;
 if(copy.gate_active&&copy.gate_cursor<U32(e,0x38)){
  uint32_t row=U32(e,0x40+4*copy.gate_cursor);if(row>=12)return RX_CAREER_BAD_STATE;
  rx_addr table=rx_career_player_table(0x9e2188,2);
  if(act_cell(table,0x936ef4,row)==0)copy.device=(int32_t)act_cell(table,0x93ded0,row);
 }
 for(unsigned i=0;i<16;i++)copy.gates[i]=I32(e,0x70+4*i);
 memcpy(copy.rows,score.rows,sizeof copy.rows);*out=copy;return RX_CAREER_OK;
}
static void cp_moto_first_post_witness(void*context,uint64_t generation,rx_addr activity){
 struct rx_session*s=context;const rx_addr e=s->moto_first_event;
 if(s!=g_career_live_session||s->state==RX_SESSION_ERROR||generation!=s->career_presentation.generation||!s->moto_first_submitted||activity!=s->moto_first_activity||U32(0x9e2188,0x18c)!=e||!cp_moto_live_layout(s,activity)||U32(e,4)!=0||U32(e,0xc)!=6||U32(e,0x10)!=7||U32(e,0x30))return;
 /* Only the existing typed real Post-state dispatcher calls this witness. */
 s->moto_first_post_observed=1;
}
static int cp_moto_action(struct career_public_call*c){
 struct rx_session*s=c->s;rx_career_moto_status v;int rc=cp_moto_status(s,&v);if(rc)return rc;
 if(v.phase!=c->moto_phase||v.sub_event!=c->moto_sub||v.gate_cursor!=c->moto_cursor)return RX_CAREER_STALE;
 const rx_career_host_request*receipt=NULL;rx_career_host_queue*q=&g_career_public_queue;
 for(uint32_t i=0;i<q->count;i++){const rx_career_host_request*r=&q->records[(q->first+i)%RX_CAREER_HOST_QUEUE_CAPACITY];if(r->id==c->id&&r->generation==c->generation)receipt=r;}
 if(!receipt)return RX_CAREER_NOT_FOUND;
 if(receipt->kind!=RX_CAREER_HOST_HUB)return RX_CAREER_WRONG_KIND;
 if(!receipt->delivered)return RX_CAREER_NOT_DELIVERED;
 if(receipt->state!=RX_CAREER_HOST_ACCEPTED)return RX_CAREER_BAD_STATE;
 const rx_addr e=U32(0x9e2188,0x18c);const rx_career_single_dependencies*d=rx_career_event_dependencies();
 if(c->value==RX_MOTO_GATE_BEGIN){
  /* Gate owner prepared the real twelve-row order and real available slots. */
  if(v.post_complete||!U32(e,0x38)||v.gate_active||v.next_phase<6||v.next_phase>7)return RX_CAREER_BAD_STATE;
  rx_career_event_process_gate(e,d);return RX_CAREER_OK;
 }
 if(c->value==RX_MOTO_GATE_SELECT){
  if(!v.gate_active||v.device!=(int32_t)s->career_controller||c->moto_gate<0||c->moto_gate==0x7fffffff)return RX_CAREER_BAD_STATE;
  int available=0;for(uint32_t i=0;i<v.gate_count;i++)if(v.gates[i]==c->moto_gate)available=1;
  if(!available)return RX_CAREER_CONFLICT;
  rx_career_event_assign_gate(e,s->career_controller,c->moto_gate);rx_career_event_advance_gate(e,d);return RX_CAREER_OK;
 }
 if(c->value==RX_MOTO_NEXT||c->value==RX_MOTO_FIRST_CONTINUE){
  if(v.phase!=6||v.next_phase!=7||v.sub_event!=0||v.post_complete||v.gate_active)return RX_CAREER_BAD_STATE;
  /* Require the genuine scored boundary before an inter-moto action. */
  {
   rx_moto_observation score = {0};
   score.size = sizeof score;
   score.version = 1;
   const rx_addr first = U32(e, 0x14);
   if (!first || rx_career_moto_observe(e, &score) || !cp_moto_scored(&score, U32(first, 0x26c), 0)) return RX_CAREER_BAD_STATE;
  }
  if(rx_career_host_queue_pending(&g_career_public_queue))return RX_CAREER_BUSY;
  if(c->value==RX_MOTO_FIRST_CONTINUE){
   if(s->moto_first_submitted)return RX_CAREER_REPEAT;
   const rx_addr a=U32(e,0x14);if(!cp_moto_live_layout(s,a)||U32(a,0x21c)!=4||!cp_continue_allowed(s,a)||s->career_completion.ticket)return RX_CAREER_BAD_STATE;
   if(!rx_career_completion_bind(s,s->career_presentation.generation,U32(a,0x290),a,cp_moto_first_post_witness))return RX_CAREER_UNSUPPORTED;
   s->moto_first_event=e;s->moto_first_activity=a;s->moto_first_submitted=1;
   ACT_CALL(0,rx_app_post_activity_results_page_done());return RX_CAREER_OK;
  }
  if(!s->moto_first_submitted||!s->moto_first_post_observed||s->moto_first_event!=e)return RX_CAREER_BAD_STATE;
  /* Dispose the first Post witness before the final-Moto ticket may bind. */
  rx_career_completion_unbind(s);
  /* Canonical next-sub-event owns roster/state changes and the next gate page. */
  rx_career_event_next_sub_event(e,d);return RX_CAREER_OK;
 }
 return RX_CAREER_INVALID;
}

#include "heat_session.inc"

static int cp_dispatch(struct career_public_call*c){
 int rc=cp_entry(c->s,c->generation,c->operation==CP_CONFIGURE);if(rc)return rc;
 /* Transport accepted is not renderer-completed. Retain copied reads and
  * durable receipts; no further canonical lifecycle action before paint. */
 if(c->s->replay_host.count&&c->operation!=CP_STATUS&&c->operation!=CP_MOTO_STATUS&&c->operation!=CP_HEAT_STATUS&&c->operation!=CP_SERIES&&c->operation!=CP_SERIES_PRESENTATION&&
    c->operation!=CP_NEXT&&c->operation!=CP_ACK&&c->operation!=CP_TRANSITIONS&&
    c->operation!=CP_COMPLETION&&c->operation!=CP_CHECKPOINT_COMMITTED&&
    c->operation!=CP_CHECKPOINT_FAILED)return RX_CAREER_BUSY;
 /* Terminal records forbid native frames and new gameplay actions. Only
  * copied reads, exact checkpoint receipts and guarded settlement/repeats remain. */
 if(c->s->records_finalized&&c->operation!=CP_STATUS&&c->operation!=CP_MOTO_STATUS&&c->operation!=CP_HEAT_STATUS&&c->operation!=CP_TRANSITIONS&&
    c->operation!=CP_SERIES&&c->operation!=CP_SERIES_PRESENTATION&&c->operation!=CP_NEXT&&c->operation!=CP_COMPLETION&&
    c->operation!=CP_COMPLETE_EVENT&&c->operation!=CP_FRONTEND_READY&&c->operation!=CP_COMPLETE_SERIES&&
    c->operation!=CP_CHECKPOINT_COMMITTED&&c->operation!=CP_CHECKPOINT_FAILED)return RX_CAREER_BAD_STATE;
 struct rx_session*s=c->s;s->career_busy=1;char captured[4096];
 int sig=run_guarded(cp_run,c,captured,sizeof captured);
 career_load_active=NULL;s->career_busy=0;
 if(sig){char message[512];describe_fault(message,sizeof message,"rx_session_career",sig,captured);session_error(s,"%s",message);return RX_CAREER_FAULT;}
 return c->rc;
}
int rx_session_career_configure(struct rx_session*s,const rx_career_config*config,uint64_t*generation){
 if(!config||!generation||config->size!=sizeof *config||config->version!=RX_CAREER_API_VERSION)return RX_CAREER_INVALID;
 /* Capabilities, version 2 (rx_career_series.h): only known bits; the hub / fade / wipe / results / rules pages every series needs;
  * at least one race HUD. Each series then checks its own HUD and format bits at load. */
 const uint32_t capabilities = config->presenter_capabilities;
 if (config->controller != 0) return RX_CAREER_UNSUPPORTED;
 if (capabilities & ~RX_CAREER_ALL_PRESENTER_CAPABILITIES) return RX_CAREER_UNSUPPORTED;
 if ((capabilities & RX_CAREER_BASE_REQUIRED_CAPABILITIES) != RX_CAREER_BASE_REQUIRED_CAPABILITIES) return RX_CAREER_UNSUPPORTED;
 if ((capabilities & RX_CAREER_ALL_HUD_CAPABILITIES) == 0) return RX_CAREER_UNSUPPORTED;
 /* An exhibition replay presenter (rx_session_replay_begin_event) owns this session's host. */
 if(s&&s==g_career_live_session&&session_ok(s)&&s->replay_event)return RX_CAREER_BAD_STATE;
 struct career_public_call c={0};c.s=s;c.operation=CP_CONFIGURE;c.config=*config;
 int rc=cp_dispatch(&c);if(!rc)*generation=c.generation;return rc;
}
int rx_session_career_series(struct rx_session*s,uint64_t generation,uint32_t id,rx_career_series_info*out){
 if(!out||out->size!=sizeof *out||out->version!=RX_CAREER_API_VERSION)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=generation;c.operation=CP_SERIES;c.value=id;
 int rc=cp_dispatch(&c);if(!rc)*out=c.series;return rc;
}
/* rx_career_series.h: what series `id` needs from the presenter, and why it is not admitted when it is not. A read, like
 * rx_session_career_series: allowed whenever the series catalog is. */
int rx_session_career_series_presentation(struct rx_session* s, uint64_t generation, uint32_t id, rx_career_series_presentation* out) {
    if (!out || out->size != sizeof *out || out->version != RX_CAREER_SERIES_PRESENTATION_VERSION) return RX_CAREER_INVALID;
    struct career_public_call c = {0};
    c.s = s;
    c.generation = generation;
    c.operation = CP_SERIES_PRESENTATION;
    c.value = id;
    const int rc = cp_dispatch(&c);
    if (rc == RX_CAREER_OK) *out = c.presentation;
    return rc;
}
static int cp_action(struct rx_session*s,uint64_t generation,int operation,uint32_t value){
 struct career_public_call c={0};c.s=s;c.generation=generation;c.operation=operation;c.value=value;return cp_dispatch(&c);
}
int rx_session_career_load(struct rx_session*s,uint64_t g,uint32_t id){return cp_action(s,g,CP_LOAD,id);}
int rx_session_career_select_vehicle(struct rx_session*s,uint64_t g,uint32_t type){return cp_action(s,g,CP_SELECT,type);}
int rx_session_career_initialize(struct rx_session*s,uint64_t g){return cp_action(s,g,CP_INITIALIZE,0);}
int rx_session_career_start(struct rx_session*s,uint64_t g){return cp_action(s,g,CP_START,0);}
int rx_session_career_continue(struct rx_session*s,uint64_t g){return cp_action(s,g,CP_CONTINUE,0);}
int rx_session_career_status(struct rx_session*s,uint64_t g,rx_career_status*out){
 if(!out||out->size!=sizeof *out||out->version!=RX_CAREER_API_VERSION)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.operation=CP_STATUS;
 int rc=cp_dispatch(&c);if(!rc)*out=c.status;return rc;
}
int rx_session_career_next(struct rx_session*s,uint64_t g,rx_career_request*out){
 if(!out||out->size!=sizeof *out||out->version!=RX_CAREER_API_VERSION)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.operation=CP_NEXT;
 int rc=cp_dispatch(&c);if(!rc)*out=c.request;return rc;
}
int rx_session_career_ack(struct rx_session*s,uint64_t g,uint64_t id,uint32_t kind,uint32_t accepted){
 if(accepted>1)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.id=id;c.kind=kind;c.accepted=accepted;c.operation=CP_ACK;return cp_dispatch(&c);
}
int rx_session_career_wipe(struct rx_session*s,uint64_t g,uint64_t id,uint32_t kind,const char*name,uint32_t milestone){
 if(!name)return RX_CAREER_INVALID;
 struct career_public_call c={0};size_t n=0;while(n<sizeof c.name&&name[n])n++;
 if(!n||n==sizeof c.name)return RX_CAREER_INVALID;
 memcpy(c.name,name,n+1);c.s=s;c.generation=g;c.id=id;c.kind=kind;c.value=milestone;c.operation=CP_WIPE;return cp_dispatch(&c);
}
int rx_session_career_abort(struct rx_session*s,uint64_t g){
 int rc=cp_entry(s,g,0);if(rc)return rc;
 career_public_detach(s);session_error(s,"Career explicitly aborted; destroy this session before continuing");return RX_CAREER_OK;
}

int rx_session_career_transitions(struct rx_session*s,uint64_t g,rx_career_transition_status*out){
 if(!out||out->size!=sizeof *out||out->version!=RX_CAREER_TRANSITIONS_VERSION)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.operation=CP_TRANSITIONS;
 int rc=cp_dispatch(&c);if(!rc)*out=c.transitions;return rc;
}
static int cp_transition_action(struct rx_session*s,uint64_t g,uint64_t id,int op,uint32_t value){
 struct career_public_call c={0};c.s=s;c.generation=g;c.id=id;c.operation=op;c.value=value;return cp_dispatch(&c);
}
int rx_session_career_rules_accept(struct rx_session*s,uint64_t g,uint64_t id,uint32_t skip){
 if(skip>1)return RX_CAREER_INVALID;return cp_transition_action(s,g,id,CP_RULES_ACCEPT,skip);
}
int rx_session_career_rules_popup_ended(struct rx_session*s,uint64_t g,uint64_t id){return cp_transition_action(s,g,id,CP_RULES_ENDED,0);}
int rx_session_career_loading_enter(struct rx_session*s,uint64_t g){return cp_transition_action(s,g,0,CP_LOADING_ENTER,0);}
int rx_session_career_loading_hub_entered(struct rx_session*s,uint64_t g,uint64_t id){return cp_transition_action(s,g,id,CP_LOADING_HUB_ENTERED,0);}

int rx_session_career_checkpoint_committed(struct rx_session*s,uint64_t g,uint64_t id){
 struct career_public_call c={0};c.s=s;c.generation=g;c.id=id;c.operation=CP_CHECKPOINT_COMMITTED;return cp_dispatch(&c);
}
int rx_session_career_checkpoint_failed(struct rx_session*s,uint64_t g,uint64_t id){
 struct career_public_call c={0};c.s=s;c.generation=g;c.id=id;c.operation=CP_CHECKPOINT_FAILED;return cp_dispatch(&c);
}

int rx_session_career_completion(struct rx_session*s,uint64_t g,rx_career_completion_status*out){
 if(!out||out->size!=sizeof *out||out->version!=RX_CAREER_COMPLETION_VERSION)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.operation=CP_COMPLETION;
 int rc=cp_dispatch(&c);if(!rc)*out=c.completion;return rc;
}
static int cp_completion_action(struct rx_session*s,uint64_t g,uint64_t ticket,uint64_t id,int operation){
 struct career_public_call c={0};c.s=s;c.generation=g;c.ticket=ticket;c.id=id;c.operation=operation;return cp_dispatch(&c);
}
int rx_session_career_complete_event(struct rx_session*s,uint64_t g,uint64_t ticket){return cp_completion_action(s,g,ticket,0,CP_COMPLETE_EVENT);}
int rx_session_career_frontend_ready(struct rx_session*s,uint64_t g,uint64_t ticket,uint64_t route){return cp_completion_action(s,g,ticket,route,CP_FRONTEND_READY);}
int rx_session_career_complete_series(struct rx_session*s,uint64_t g,uint64_t ticket){return cp_completion_action(s,g,ticket,0,CP_COMPLETE_SERIES);}

int rx_session_career_vehicle(struct rx_session*s,uint64_t g,rx_career_vehicle_identity*out){
 if(!out||out->size!=sizeof *out||out->version!=RX_CAREER_VEHICLE_VERSION)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.operation=CP_VEHICLE_QUERY;
 int rc=cp_dispatch(&c);if(!rc)*out=c.vehicle;return rc;
}

int rx_session_career_moto_status(struct rx_session*s,uint64_t g,rx_career_moto_status*out){
 if(!out||out->size!=sizeof *out||out->version!=2)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.operation=CP_MOTO_STATUS;int rc=cp_dispatch(&c);if(!rc)*out=c.moto;return rc;
}
int rx_session_career_moto_action(struct rx_session*s,uint64_t g,uint64_t hub,uint32_t action,uint32_t phase,uint32_t sub,uint32_t cursor,int32_t gate){
 if(!hub||action<1||action>4)return RX_CAREER_INVALID;
 struct career_public_call c={0};c.s=s;c.generation=g;c.id=hub;c.value=action;c.moto_phase=phase;c.moto_sub=sub;c.moto_cursor=cursor;c.moto_gate=gate;c.operation=CP_MOTO_ACTION;return cp_dispatch(&c);
}

int rx_session_career_moto_practice_choice(struct rx_session*s,uint64_t generation,uint32_t skip){return cp_action(s,generation,CP_MOTO_PRACTICE,skip);}
int rx_session_career_heat_status(struct rx_session* s, uint64_t g, rx_career_heat_status* out) {
    if (!out || out->size != sizeof *out || out->version != RX_CAREER_HEAT_VERSION) return RX_CAREER_INVALID;
    struct career_public_call c = {0};
    c.s = s;
    c.generation = g;
    c.operation = CP_HEAT_STATUS;
    const int rc = cp_dispatch(&c);
    if (!rc) *out = c.heat;
    return rc;
}
int rx_session_career_heat_action(struct rx_session* s, uint64_t g, uint64_t hub, uint32_t action, uint32_t phase, uint32_t next_phase,
                                  uint32_t cursor, int32_t gate) {
    if (!hub || action < RX_HEAT_GATE_BEGIN || action > RX_HEAT_ROUND_CONTINUE) return RX_CAREER_INVALID;
    struct career_public_call c = {0};
    c.s = s;
    c.generation = g;
    c.id = hub;
    c.value = action;
    c.moto_phase = phase;
    c.moto_sub = next_phase;
    c.moto_cursor = cursor;
    c.moto_gate = gate;
    c.operation = CP_HEAT_ACTION;
    return cp_dispatch(&c);
}
