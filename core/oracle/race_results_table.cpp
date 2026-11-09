// race_results_table.cpp -- oracle adapters for core/game/race_results_table.c (group "rx_results": the results page's table and the
// projected times; the five reference worlds never reach a race end: verify them in an event boot that does, w-resultsrng's
// RX_RNG_FINISH_STIM + REFLEX_EVENT_UI, core/plan/QUEUE.md).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/race_results_table.h"
#include "../game/race_results_text.h"
}
using namespace reflex_port;

// 00613880 (EAX = driver; cdecl activity) -> xmm0. The result's upper lanes as the original leaves them: zero, except when the result
// is the draw plus the rider ahead's time (the draw's register: lane 1 = the high dword of its double product, lanes 2-3 the caller's
// xmm1, which cvtsi2ss / cvtss2sd / cvtsd2ss keep).
RX_PORT(0x00613880, Results_ProjectedTime, "rx_results", OUT_XMM0) {
    const uint64_t xmm1_hi = m.xmm[1].hi;
    rx_results_xmm x;
    const float r = rx_results_projected_time_x(m.r[0], m.arg(0), &x);
    m.xmm[0].lo = rx_oracle::bits(r) | (uint64_t(x.from_draw ? x.lane1 : 0u) << 32);
    m.xmm[0].hi = x.from_draw ? xmm1_hi : 0;
    return m.ret(0);
}
// 006146f0 (stdcall criteria vector, column, text, sort value; RET 0x10)
RX_PORT(0x006146f0, Results_Cell, "rx_results", 0) {
    rx_results_cell(m.arg(0), m.arg(1), m.arg(2), rx_oracle::f32(m.arg(3)));
    return m.ret(0x10);
}
// 00614950 (stdcall walker, criteria vector, column list, driver; RET 0x10)
RX_PORT(0x00614950, Results_RaceColumns, "rx_results", 0) {
    rx_results_race_columns(m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
// 00614890 (ESI = walker, EAX = column list, ECX = driver; stdcall criteria vector, activity type; RET 8)
RX_PORT(0x00614890, Results_Columns, "rx_results", 0) {
    rx_results_columns(m.r[6], m.arg(0), m.r[0], m.r[1], m.arg(1));
    return m.ret(8);
}
// 00616000 (EDX = name; stdcall criterion; RET 4) -> EAX = the criterion
RX_PORT(0x00616000, Results_NameCriterion, "rx_results", OUT_EAX) {
    m.r[0] = rx_results_name_crit(m.r[2], m.arg(0));
    return m.ret(4);
}
// 00613830 (EAX = column type) -> EAX = 0x00da9ca8
RX_PORT(0x00613830, Results_ColumnTitle, "rx_results", OUT_EAX) {
    m.r[0] = rx_results_column_title(m.r[0]);
    return m.ret(0);
}
// 005ba450 (EDI = std::vector<int>; stdcall &value; RET 4)
RX_PORT(0x005ba450, VectorInt_Push, "rx_results", 0) {
    rx_vector_int_push(m.r[7], m.arg(0));
    return m.ret(4);
}
// 00613dd0 (ECX = walker) -> EAX = the number of columns
RX_PORT(0x00613dd0, Results_Populate, "rx_results", OUT_EAX) {
    m.r[0] = rx_results_populate(m.r[1]);
    return m.ret(0);
}
// App_PopulateResultsTable 00742c40 (cdecl, no arguments)
RX_PORT(0x00742c40, App_PopulateResultsTable, "rx_results", 0) {
    rx_app_populate_results_table();
    return m.ret(0);
}

// ---- w-wpfill: the offline modes' fillers (Waypoint, Freestyle, Section Race) and the Eliminator's place column's text helpers
// 00613a30 (cdecl activity, driver) -> xmm0 (upper lanes zero on every path)
RX_PORT(0x00613a30, Results_WaypointProjectedTime, "rx_results", OUT_XMM0) {
    const float r = rx_waypoint_projected_time(m.arg(0), m.arg(1));
    m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0;
    return m.ret(0);
}
// 00615450 (stdcall walker, criteria vector, column list, driver; RET 0x10)
RX_PORT(0x00615450, Results_WaypointColumns, "rx_results", 0) {
    rx_results_waypoint_columns(m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
// 00614bf0 (ECX = driver; stdcall criteria vector, column list; RET 8)
RX_PORT(0x00614bf0, Results_FreestyleColumns, "rx_results", 0) {
    rx_results_freestyle_columns(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 006152e0 (ECX = driver; stdcall criteria vector, column list; RET 8)
RX_PORT(0x006152e0, Results_SectionColumns, "rx_results", 0) {
    rx_results_section_columns(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// App_FormatCredits 005dc430 (ECX = value, EDX = size; cdecl buffer) -> EAX
RX_PORT(0x005dc430, App_FormatCredits, "rx_results", OUT_EAX) {
    m.r[0] = rx_app_format_credits(int32_t(m.r[1]), int32_t(m.r[2]), m.arg(0));
    return m.ret(0);
}
// 005dc3c0 (ECX = value, EDI = UTF-16 buffer) -> EAX
RX_PORT(0x005dc3c0, Results_ThousandsW, "rx_results", OUT_EAX) {
    m.r[0] = rx_format_thousands_w(m.r[1], m.r[7]);
    return m.ret(0);
}
// 007900c0 (EAX = UTF-16 string) -> EAX
RX_PORT(0x007900c0, Results_WstrReverse, "rx_results", OUT_EAX) {
    m.r[0] = rx_wstr_reverse(m.r[0]);
    return m.ret(0);
}
// 005dc720 (cdecl UTF-16 buffer, size, place) -> EAX
RX_PORT(0x005dc720, Results_OrdinalW, "rx_results", OUT_EAX) {
    m.r[0] = rx_ui_ordinal_w(m.arg(0), m.arg(1), int32_t(m.arg(2)));
    return m.ret(0);
}
// 005dc7a0 (ECX = size, EDI = buffer; cdecl place) -> EAX
RX_PORT(0x005dc7a0, Results_OrdinalText, "rx_results", OUT_EAX) {
    const uint32_t ret = U32(m.r[4], 0), buf = m.r[7];
    m.r[0] = rx_ui_ordinal_text(buf, int32_t(m.r[1]), int32_t(m.arg(0)));
    // RX_RESULTS_TEXT_LOG=1: the results table's ELIMINATED cells (calls from 00614950) with the language's text (the UI boundary's value)
    if (std::getenv("RX_RESULTS_TEXT_LOG") && ret >= 0x00614950 && ret < 0x00614bf0) {
        char t[64]; uint32_t i = 0; for (; i + 1 < sizeof t && U8(buf, i); i++) t[i] = char(U8(buf, i)); t[i] = 0;
        std::fprintf(stderr, "[ordinal] place %d text \"%s\"\n", int32_t(m.arg(0)), t);
    }
    return m.ret(0);
}

// ---- w-partyresults: the party modes' fillers
// 00614dc0 Tag (stdcall vector, column list, driver; RET 0xc)
RX_PORT(0x00614dc0, Results_TagColumns, "rx_results", 0) {
    rx_results_tag_columns(m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 00614f00 Snake (ECX = driver; stdcall vector, column list; RET 8)
RX_PORT(0x00614f00, Results_SnakeColumns, "rx_results", 0) {
    rx_results_snake_columns(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 00615070 Infection (stdcall vector, column list, driver; RET 0xc)
RX_PORT(0x00615070, Results_InfectionColumns, "rx_results", 0) {
    rx_results_infection_columns(m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 006151a0 EggHunt (ECX = driver; stdcall vector, column list; RET 8)
RX_PORT(0x006151a0, Results_EggHuntColumns, "rx_results", 0) {
    rx_results_egghunt_columns(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
