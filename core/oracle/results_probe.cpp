// results_probe.cpp -- the product's results page start (core/game/race_results_table.c rx_results_page_*) in the ORIGINAL's run
// (agent w-resultsrng; development only, group "thunk": select the ports by name).
//
//   REFLEX_PORT=rx_RngProbeFrame,rx_ResultsPageShow,rx_ResultsPageTick,rx_ResultsPageSkip RX_RESULTS_PAGE=replace
//
// The product has no UI pages, so it starts the race results page itself: the countdown from the arcade's "Show Activity Results"
// (the HUD page's PostEventOverlay 005f42e0, whose arcade branch routes the hub to PostEventResults) and App.PopulateResultsTable
// (00742c40) when it ends, at the start of the UI scene's tick (the engine context's scene +0x24, whose first ticker is the UI
// manager). Here the same core code runs from the original's own events: 005f42e0's entry starts the countdown (the product's arcade
// checks), SceneManager_Tick 007d23a0's entry for the UI scene runs rx_results_page_frame, and the original's own call of
// 00742c40 from the page's Lua returns at once (the table is already filled). With core/tools/rng_cmp.py --abs --by-frame against a
// run of the originals (the same boot without these ports) every frame's random streams must be identical: the product's timing,
// place in the frame and draw count are the original page's. (No skip control: the page's Lua reads row 0
// of the table right after the call, so an original run without the table stops there.)
#include "oracle.h"
#include <cstdlib>
#include <cstring>
#include <string>
extern "C" {
#include "../game/race_results_table.h"
#include "../game/fx.h"
}
using namespace reflex_port;

static bool replace_mode() { const char* v = std::getenv("RX_RESULTS_PAGE"); return v && std::strcmp(v, "replace") == 0; }
// RX_RESULTS_PAGE=trace (w-wpfill) runs the originals and logs on stderr, by the UI scene's tick count: "Show Activity Results" at the
// HUD page's overlay (with the product's arcade checks' verdict and the activity type / mode), the page Lua's App.PopulateResultsTable
// call, and (replace) the core's fill; the product's timing is right when the Lua's call comes RESULTS_PAGE_FRAMES ticks after the show.
static bool trace_mode() { const char* v = std::getenv("RX_RESULTS_PAGE"); return v && (std::strcmp(v, "trace") == 0 || std::strcmp(v, "replace") == 0); }
static uint32_t g_ui_ticks = 0;

// 005f42e0 MXUIPostEventOverlay_OnShowActivityResults (ECX = the overlay; stdcall activity): the arcade branch's routing to the
// results page (not tour: [0x9e2318] != 1 and the series manager +0x190 != 2; activity type 8 routes to the menu page)
RX_PORT(0x005f42e0, ResultsPageShow, "thunk", 0) {
    if (!trace_mode()) return reflex_vm::RUN_ORIGINAL;
    const uint32_t act = m.arg(0);
    const bool arcade = act && U32(m.r[1], 0xe0) != 0 && U32(m.r[1], 0xf4) == 10 && U32(0, 0x009e2318) != 1 && U32(rx_fx_series_manager(), 0x190) != 2 &&
        U32(act, 0x23c) != 8;
    std::fprintf(stderr, "[resultspage] show tick %u activity %08x type %u mode %u arcade %d\n", g_ui_ticks, act, act ? U32(act, 0x23c) : 0u, act ? U32(act, 0x238) : 0u, arcade ? 1 : 0);
    if (arcade && replace_mode()) rx_results_page_show(act);
    return reflex_vm::RUN_ORIGINAL;
}
// 007d23a0 SceneManager_Tick (stdcall scene): the UI scene's tick begins with the page start's countdown
RX_PORT(0x007d23a0, ResultsPageTick, "thunk", 0) {
    if (!trace_mode()) return reflex_vm::RUN_ORIGINAL;
    const uint32_t scene = m.arg(0);
    if (scene == U32(U32(0, 0x00a95844), 0x24) && U32(scene, 0x34) != 0) {
        g_ui_ticks++;
        if (replace_mode()) {
            const uint32_t before = rx_results_page_populated(nullptr);
            rx_results_page_frame();
            if (rx_results_page_populated(nullptr) != before) std::fprintf(stderr, "[resultspage] core fill tick %u\n", g_ui_ticks);
        }
    }
    return reflex_vm::RUN_ORIGINAL;
}
// App_PopulateResultsTable 00742c40 (the page's Lua): the core filled the table already
RX_PORT(0x00742c40, ResultsPageSkip, "thunk", 0) {
    if (!trace_mode()) return reflex_vm::RUN_ORIGINAL;
    std::fprintf(stderr, "[resultspage] lua call tick %u\n", g_ui_ticks);
    if (!replace_mode()) return reflex_vm::RUN_ORIGINAL;
    return m.ret(0);
}
// the series events' vf13 (MXSeriesEventSingle 005affb0, Moto 005af2c0, Freestyle 005acb80, Heat 005adde0: the career's routing to the
// results page after "MXSeriesManager::AdvancePostEvent"): logged by the UI tick count in trace mode (w-wpfill: the career's timing)
static uint32_t vf13_log(reflex_vm::Machine& m, const char* which) {
    if (!trace_mode()) return reflex_vm::RUN_ORIGINAL;
    std::string rets; char b[16]; int n = 0;   // the caller chain (code return addresses on the stack): inside the UI scene's tick or not
    for (uint32_t a = m.r[4], i = 0; i < 2048 && n < 40; a += 4, i++) {
        const uint32_t v = m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u;
        if (v > 0x401000 && v < 0x916000) { std::snprintf(b, sizeof b, " %08x", v); rets += b; n++; }
    }
    std::fprintf(stderr, "[resultspage] series vf13 %s tick %u rets%s\n", which, g_ui_ticks, rets.c_str());
    // replace: the career's routing (the vf13 sets UIRoutingMessage "GotoPostEvent" and sends GotoHub; the product's
    // rx_results_page_hub) from where the original runs it, inside the UI tick after the page check: the current series event's activity
    if (replace_mode()) {
        const uint32_t ev = U32(rx_fx_series_manager(), 0x18c);
        if (ev) rx_results_page_show(U32(ev, 0x14));
    }
    return reflex_vm::RUN_ORIGINAL;
}
RX_PORT(0x005affb0, ResultsPageVf13Single, "thunk", 0) { return vf13_log(m, "single"); }
RX_PORT(0x005af2c0, ResultsPageVf13Moto, "thunk", 0) { return vf13_log(m, "moto"); }
RX_PORT(0x005acb80, ResultsPageVf13Freestyle, "thunk", 0) { return vf13_log(m, "freestyle"); }
RX_PORT(0x005adde0, ResultsPageVf13Heat, "thunk", 0) { return vf13_log(m, "heat"); }
// 005f4220 MXUIPostEventOverlay_OnPostScenariosStart (ECX = the overlay): the UI goes to the Event Outro page (w-wpfill); replace: the
// product's mark (rx_results_page_outro), from which the results page starts at the next UI tick
RX_PORT(0x005f4220, ResultsPageOutro, "thunk", 0) {
    if (!trace_mode()) return reflex_vm::RUN_ORIGINAL;
    const uint32_t act = U32(m.r[1], 0xe0);
    std::fprintf(stderr, "[resultspage] outro tick %u activity %08x\n", g_ui_ticks, act);
    if (act && replace_mode()) rx_results_page_outro(act);
    return reflex_vm::RUN_ORIGINAL;
}
