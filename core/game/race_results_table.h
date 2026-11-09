/* race_results_table.h -- the results page's table (the race types; w-wpfill: Waypoint, Freestyle, Section Race, the Eliminator's
 * place column) (App.PopulateResultsTable 00742c40 -> 00613dd0: table "Activity Results"
 * 0x00ae18c0, one row per driver in the standings order) and the projected times of the riders still racing (00613880: one draw of
 * the game's LCG per unfinished driver behind the leader, which also writes the driver's +0x10b4 / +0x10c4). See race_results_table.c
 * and core/plan/QUEUE.md "w-resultsrng". */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* The x86 result register of 00613880 beyond its low lane (the oracle adapter's ABI glue): when the result is the draw plus the time
 * of the rider ahead it is the draw's register (lane 1 = the high dword of the draw's double product, lanes 2-3 the caller's xmm1). */
typedef struct rx_results_xmm { int from_draw; uint32_t lane1; } rx_results_xmm;

float    rx_results_projected_time(rx_addr d, rx_addr act);                               /* 00613880 (EAX = driver; cdecl activity) -> xmm0 */
float    rx_results_projected_time_x(rx_addr d, rx_addr act, rx_results_xmm* x);
void     rx_results_cell(rx_addr vec, uint32_t col, rx_addr text, float sort);             /* 006146f0 (stdcall vec, column, text, sort; RET 0x10) */
void     rx_results_race_columns(rx_addr ctx, rx_addr vec, rx_addr cols, rx_addr d);       /* 00614950 (stdcall; RET 0x10) */
void     rx_results_columns(rx_addr ctx, rx_addr vec, rx_addr cols, rx_addr d, uint32_t type); /* 00614890 (ESI ctx, EAX cols, ECX d; stdcall vec, type; RET 8) */
float    rx_waypoint_projected_time(rx_addr act, rx_addr d);                               /* 00613a30 (cdecl activity, driver) -> xmm0 */
void     rx_results_waypoint_columns(rx_addr ctx, rx_addr vec, rx_addr cols, rx_addr d);   /* 00615450 (stdcall; RET 0x10) */
void     rx_results_freestyle_columns(rx_addr d, rx_addr vec, rx_addr cols);               /* 00614bf0 (ECX = driver; stdcall vec, cols; RET 8) */
void     rx_results_section_columns(rx_addr d, rx_addr vec, rx_addr cols);                 /* 006152e0 (ECX = driver; stdcall vec, cols; RET 8) */
void     rx_results_tag_columns(rx_addr vec, rx_addr cols, rx_addr d);                     /* 00614dc0 (stdcall vec, cols, driver; RET 0xc) */
void     rx_results_snake_columns(rx_addr d, rx_addr vec, rx_addr cols);                   /* 00614f00 (ECX = driver; stdcall vec, cols; RET 8) */
void     rx_results_infection_columns(rx_addr vec, rx_addr cols, rx_addr d);               /* 00615070 (stdcall vec, cols, driver; RET 0xc) */
void     rx_results_egghunt_columns(rx_addr d, rx_addr vec, rx_addr cols);                 /* 006151a0 (ECX = driver; stdcall vec, cols; RET 8) */
rx_addr  rx_results_name_crit(rx_addr name, rx_addr c);                                    /* 00616000 (EDX = name; stdcall criterion; RET 4) -> EAX */
rx_addr  rx_results_column_title(uint32_t type);                                           /* 00613830 (EAX = the column type) -> EAX = 0x00da9ca8 */
void     rx_vector_int_push(rx_addr vec, rx_addr valp);                                    /* 005ba450 (EDI = std::vector<int>; stdcall &value; RET 4) */
uint32_t rx_results_populate(rx_addr ctx);                                                 /* 00613dd0 (ECX = {vtable 0x957f84, activity}) -> EAX */
void     rx_app_populate_results_table(void);                                              /* App_PopulateResultsTable 00742c40 (the Lua binding's body) */

/* Product only: the results page's start (PostEventResults:Start, the original's Lua: App.PopulateResultsTable) at the original's
 * point in the frame, driven by the hub's routing to it (race_results_table.c): the arcade's "Show Activity Results" (observe), the
 * career's "GotoHub" after UIRoutingMessage "GotoPostEvent" (hub), the UI scene's tick in Engine::Update (frame). */
void     rx_results_page_observe(rx_addr channel, rx_addr name, uint32_t payload, uint32_t bytes);
void     rx_results_page_hub(void);
void     rx_results_page_show(rx_addr activity);
void     rx_results_page_outro(rx_addr activity);   /* the overlay's "Post Scenarios Start": the UI on the Event Outro page */
void     rx_results_page_frame(void);
uint32_t rx_results_page_populated(rx_addr* activity);   /* the page starts run for *activity (the last one filled) */

#ifdef __cplusplus
}
#endif
