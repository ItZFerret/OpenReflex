/* race_results_table.c -- the results page's table of the race activities: App.PopulateResultsTable (00742c40, the Lua binding the
 * InGame page PostEventResults:Start calls) -> 00613dd0, which fills the table "Activity Results" (0x00ae18c0: Index, Name, Column0..4
 * with their _Record / _SortValue columns; ActivityResults_CreateSchema 004df240): row 0 the column titles, then one row per driver in
 * the standings order (activity +0x2d0), each column filled by the activity type's filler (00614890). The race types (National 0,
 * Supercross 1, Terracross 0x11, Powercross 0x13) show TOTAL / PENALTY / BEST LAP (00614950); their TOTAL is 00613880, which for a
 * driver still racing behind the leader projects a total time with one draw of the game's LCG (0x009cb860) and keeps it in the
 * driver's gap +0x10b4 (and, in a race of more than one lap, a best lap +0x10c4 for a rider without one).
 *
 * The product has no UI pages (Unity shows the results), so it runs App.PopulateResultsTable itself where the original's page does
 * (rx_results_page_*, product only, below): the same draws, the same driver fields, the same table, whatever Unity's page timing.
 * The offline modes' fillers (w-wpfill): Waypoint 6 (00615450 with the projected total 00613a30, which mirrors a rider still racing
 * above the rider ahead and keeps it as the rider's gap +0x10a4; no draw), Freestyle 5 (00614bf0), Section Race 0xc (006152e0) and
 * the Eliminator's place column 9 (the localized ordinal 005dc7a0, race_results_text.c: a UI boundary in the product). The party
 * modes' fillers (w-partyresults): Tag 9 (00614dc0: WINS, TAG IT TIME), Snake 10 (00614f00: WINS, SNAKE ELIMINATIONS), Infection 0xe
 * (00615070: WINS, OUTBREAK INFECTED = the original's "TODO") and EggHunt 0xf (006151a0: WINS, GREED POINTS); PokerRace 0xd has none
 * (and its page never fills the table: rx_results_page_* below). */
#include "race_results_table.h"
#include "rider_cap.h"
#include "race.h"
#include "race_driver.h"
#include "race_results_text.h"
#include "race_section.h"
#include "waypoint_driver.h"
#include "world_scene_handlers.h"
#include "build_activity_util.h"
#include "fx.h"
#include "crt.h"
#include <float.h>
#include <string.h>

#define RESULTS_TABLE 0x00ae18c0u   /* "Activity Results" */

/* 0078fd60 (EDI = buffer, ESI = size; cdecl format, ...) with the arguments where the original pushes them */
static void results_format(rx_addr buf, uint32_t size, rx_addr fmt, int n, const uint32_t* va) {
    const rx_addr sp = rx_sp(), args = sp - 4u * (uint32_t)(n + 1);
    SU32(args, 0, fmt);
    for (int i = 0; i < n; i++) SU32(args, 4u * (uint32_t)(i + 1), va[i]);
    rx_set_sp(args);
    ACT_CALL(0, rx_format_buffer(buf, size, fmt, args + 4));
    rx_set_sp(sp);
}

/* ---- 00613880 (EAX = driver, ESI inside; cdecl activity; plain RET) -> xmm0: the driver's TOTAL on the results page.
 * A finished driver (+0x70) or an Eliminator (activity mode +0x238 == 5): the elimination time (+0x1124) when its own activity is an
 * Eliminator and it was eliminated (+0x1120 != -1), else its total time (00512870). A network client (session +0x1c4c, not the host
 * +0x1b0 == 1): FLT_MAX. Else, with t0 = the leader's standings time (00512910) and t = +0x10b4 (the gap) + t0: a driver at place
 * k > 0 of the standings gets one LCG draw r = (rand15 / 32768) * 20 s and t is raised to at least the rider ahead's (+0x10b4 + t0)
 * plus r and, in a race of more than one lap (+0xff070 > 1), to the laps times its best lap plus r (a driver without a best lap,
 * +0x10c4 FLT_MAX, gets the rider ahead's plus r first); the gap +0x10b4 = t - t0. The leader keeps t. Frame: push ebx / esi /
 * (main path) edi. */
float rx_results_projected_time_x(rx_addr d, rx_addr act, rx_results_xmm* x) {
    const rx_addr E = rx_sp();
    if (x) { x->from_draw = 0; x->lane1 = 0; }
    rx_set_sp(E - 8);                                                             /* push ebx; push esi */
    if (U32(d, 0x70) != 0 || U32(act, 0x238) == 5) {
        if (U32(U32(U32(d, 0x74), 0x94), 0x238) == 5 && I32(d, 0x1120) != -1) { rx_set_sp(E); return F32(d, 0x1124); }
        float r;
        ACT_CALLR(r, 0, rx_driver_total_time(d));                                 /* 00512870 (ESI = driver) */
        rx_set_sp(E);
        return r;
    }
    race_static_dcf304();                                                         /* 006de1f0 + _atexit(00913e00) under 0xdcf304 */
    const rx_addr g = U32(0, 0x00da6cb0);
    if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1) { rx_set_sp(E); return rx_bf(0x7f7fffffu); }   /* 0x977494 */
    rx_set_sp(E - 0xc);                                                           /* push edi */
    float lead;
    ACT_CALLR(lead, 0, rx_driver_standings_time(U32(act, 0x2d0)));                /* 00512910 (EAX = the leader) */
    float t = rx_adds(F32(d, 0x10b4), lead);
    const int32_t n = I32(act, 0x26c);
    int32_t k = 0;
    for (int32_t i = 0; i < n; i++) if (rx_stand(act, (uint32_t)i) == d) k = i;
    if (n <= 0 || k <= 0) { rx_set_sp(E); return t; }
    const rx_addr ahead = U32(act, 0x2cc + 4u * (uint32_t)k);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    SU32(0, 0x009cb860, seed);
    const double q = (double)(float)(int32_t)((seed >> 16) & 0x7fffu) * 3.0517578125e-05 /* 0x9776a0 */;
    float r = rx_muls((float)q, 20.0f /* 0x935aac */);
    const float t_ahead = rx_adds(F32(ahead, 0x10b4), lead);
    if (I32(act, 0xff070) > 1) {
        if (F32(d, 0x10c4) == rx_bf(0x7f7fffffu)) SF32(d, 0x10c4, rx_adds(F32(ahead, 0x10c4), r));
        const float laps = rx_muls((float)I32(act, 0xff070), F32(d, 0x10c4));
        if (laps > t) t = rx_adds(laps, r);
    }
    if (t_ahead > t) {
        t = rx_adds(r, t_ahead);
        if (x) { uint64_t b; memcpy(&b, &q, 8); x->from_draw = 1; x->lane1 = (uint32_t)(b >> 32); }
    }
    SF32(d, 0x10b4, rx_subs(t, lead));
    rx_set_sp(E);
    return t;
}
float rx_results_projected_time(rx_addr d, rx_addr act) { return rx_results_projected_time_x(d, act, NULL); }
RX_FUNCTION(0x00613880, rx_results_projected_time)

/* ---- 006146f0 (stdcall criteria vector, column, text, sort value; RET 0x10): a cell of a results row as three criteria pushed onto
 * the row's criteria vector (004175e0): "Column<n>" the text (a string criterion, 004f6c60), "Column<n>_Record" 0 (an int,
 * 00419a70), "Column<n>_SortValue" the sort value (a float, 004f73b0, which takes the argument's address); the names are formatted
 * into the static buffers 0x00da9ba8 / 0x00da9aa8 / 0x00da99a8. Each criterion is destroyed after its copy is pushed (the value
 * block, the path proxy, the variant: 00415320's inline). Frame: sub 0xbc; push ebx / ebp / esi / edi; the zero at +0x14, the
 * criterion at +0x18. */
void rx_results_cell(rx_addr vec, uint32_t col, rx_addr text, float sort) {
    const rx_addr E = rx_sp();                      /* the arguments: E + 4 vector, + 8 column, + 0xc text, + 0x10 sort value */
    SF32(E, 0x10, sort);
    ACT_FRAME(L, 0xbc, 4);
    const rx_addr C = L + 0x18;
    const uint32_t va[1] = {col};
    SU32(L, 0x14, 0);
    results_format(0x00da9ba8, 0x40, 0x0093dfe8 /* "Column%i" */, 1, va);
    ACT_CALL(2, rx_tbl_crit_init_string(C, 0x00da9ba8, 0, text));
    ACT_CALL(1, rx_tbl_crit_push(vec, C));
    ACT_CALL(0, rx_fx_crit_destroy(C));
    results_format(0x00da9aa8, 0x40, 0x0093dff4 /* "Column%i_Record" */, 1, va);
    ACT_CALL(1, (void)rx_fx_crit_init_int(C, 0, L + 0x14, 0x00da9aa8));
    ACT_CALL(1, rx_tbl_crit_push(vec, C));
    ACT_CALL(0, rx_fx_crit_destroy(C));
    results_format(0x00da99a8, 0x40, 0x0093e004 /* "Column%i_SortValue" */, 1, va);
    ACT_CALL(1, (void)rx_fx_crit_init_float(C, 0, E + 0x10, 0x00da99a8));
    ACT_CALL(1, rx_tbl_crit_push(vec, C));
    ACT_CALL(0, rx_fx_crit_destroy(C));
    ACT_END(L);
}
RX_FUNCTION(0x006146f0, rx_results_cell)

/* ---- 00614950 (stdcall walker {+4 activity}, criteria vector, column list, driver; RET 0x10): the cells of a race driver's row, one
 * per column type of the list (std::vector<int>: begin +0xc, end +0x10): 0 TOTAL (00613880, "MM:SS.cc" by 00790100, sort value
 * -t), 1 BEST LAP (+0x10c4), 2 PENALTY (+0x10c8: "+MM:SS.cc" when positive, else empty with sort value 0), 9 the Eliminator's place
 * (its elimination order +0x1120 + 1, else the drivers count minus its standings index; the leader of the standings an empty cell:
 * the localized ordinal 005dc7a0, sort value the place); other types nothing. Frame: sub 0xa4; push ebx / ebp / esi / edi; the driver at
 * +0x14, the walker at +0x18, the TOTAL at +0x1c, the column count at +0x20, the penalty at +0x24, the text at +0x28 (0x80). */
void rx_results_race_columns(rx_addr ctx, rx_addr vec, rx_addr cols, rx_addr d) {
    ACT_FRAME(L, 0xa4, 4);
    const rx_addr B = L + 0x28;
    SU32(L, 0x18, ctx);
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    SU32(L, 0x14, d);
    SU32(L, 0x20, (uint32_t)n);
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 0: {
            float t;
            ACT_CALLR(t, 1, rx_results_projected_time(d, U32(ctx, 4)));
            SF32(L, 0x1c, t);
            ACT_CALL(0, rx_lap_time_text(B, t, 1));                               /* 00790100 (EAX = text, xmm0, EDI = 1) */
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, rx_subs(-0.0f /* 0x9774a8 */, F32(L, 0x1c))));
            break;
        }
        case 1:
            ACT_CALL(0, rx_lap_time_text(B, F32(d, 0x10c4), 1));
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, rx_subs(-0.0f, F32(d, 0x10c4))));
            break;
        case 2: {
            const float p = F32(d, 0x10c8);
            SF32(L, 0x24, p);
            if (p > 0.0f /* 0x92e12c */) {
                SU8(B, 0, '+');
                ACT_CALL(0, rx_lap_time_text(B + 1, p, 1));
                ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, rx_subs(-0.0f, F32(L, 0x24))));
            } else {
                ACT_CALL(3, (void)crt_strncpy(B, 0x00935d6c /* "" */, 0x7f));
                SU8(B, 0x7f, 0);
                ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, 0.0f));
            }
            break;
        }
        case 9: {
            int32_t place = I32(d, 0x1120) + 1;
            if (place < 1) {
                const rx_addr act = U32(ctx, 4);
                int32_t i = 0;
                while (i < (int32_t)RX_RC_MAX && rx_stand(act, (uint32_t)i) != d) i++;   /* (w-rc-core) 0xc: standings 12..31 */
                if (i == 0) {
                    ACT_CALL(3, (void)crt_strncpy(B, 0x00935d6c /* "" */, 0x7f));
                    SU8(B, 0x7f, 0);
                    ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, 0.0f));
                    break;
                }
                if (i == (int32_t)RX_RC_MAX) i = -1;
                place = I32(act, 0x26c) - i;
            }
            ACT_CALL(1, (void)rx_ui_ordinal_text(B, 0x80, place));               /* 005dc7a0 (ECX = 0x80, EDI = the text; the place) */
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)place));
            break;
        }
        default:
            break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00614950, rx_results_race_columns)

/* the value of a float the original passes through the x87 stack (fld dword / fstp dword): a signaling NaN comes out quiet */
static float results_x87_pass(float v) {
    uint32_t b; memcpy(&b, &v, 4);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) != 0 && !(b & 0x00400000u)) { b |= 0x00400000u; memcpy(&v, &b, 4); }
    return v;
}
/* "" into the text (strncpy(text, "", 0x7f) through msvcr90's import; the last byte 0) */
static void results_empty(rx_addr B) {
    ACT_CALL(3, (void)crt_strncpy(B, 0x00935d6c /* "" */, 0x7f));
    SU8(B, 0x7f, 0);
}

/* ---- 00613a30 (cdecl activity, driver; plain RET) -> xmm0: a Waypoint driver's TOTAL on the results page. A finished driver (+0x70):
 * its total time (00518430: the sum of its gate times, or +0x10bc in a networked Waypoint). A network client (session +0x1c4c, not the
 * host +0x1b0 == 1): FLT_MAX. Else t0 = the leader's total (00518430 of activity +0x2d0 [0]) and t = t0 + the driver's gap +0x10a4 (the
 * time behind the leader at its last gate); a driver at place k > 0 of the standings whose t is below the rider ahead's (its gap + t0)
 * gets the mirror image above it, t = (ahead - t) + ahead, and keeps it as its gap (+0x10a4 = t - t0). No draw. The result's upper
 * lanes are zero on every path (each value is a scalar load or a sum into one). Frame: push ebx / ebp / edi, esi on the main path. */
float rx_waypoint_projected_time(rx_addr act, rx_addr d) {
    const rx_addr E = rx_sp();
    rx_set_sp(E - 0xc);                                                           /* push ebx / ebp / edi */
    if (U32(d, 0x70) != 0) {
        float r;
        ACT_CALLR(r, 0, rx_waypoint_driver_total_time(d));                        /* 00518430 (EDI = driver) */
        rx_set_sp(E);
        return r;
    }
    race_static_dcf304();                                                         /* 006de1f0 + _atexit(00913e00) under 0xdcf304 */
    const rx_addr g = U32(0, 0x00da6cb0);
    if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1) { rx_set_sp(E); return rx_bf(0x7f7fffffu); }   /* 0x977494 */
    rx_set_sp(E - 0x10);                                                          /* push esi */
    float lead;
    ACT_CALLR(lead, 0, rx_waypoint_driver_total_time(U32(act, 0x2d0)));           /* 00518430 (EDI = the leader) */
    const int32_t n = I32(act, 0x26c);
    int32_t k = 0;
    for (int32_t i = 0; i < n; i++) if (rx_stand(act, (uint32_t)i) == d) k = i;
    if (n > 0 && k > 0) {
        const rx_addr ahead = U32(act, 0x2cc + 4u * (uint32_t)k);
        const float t = rx_adds(lead, F32(d, 0x10a4));
        const float t_ahead = rx_adds(F32(ahead, 0x10a4), lead);
        if (t_ahead > t) {
            const float r = rx_adds(rx_subs(t_ahead, t), t_ahead);
            SF32(d, 0x10a4, rx_subs(r, lead));
            rx_set_sp(E);
            return r;
        }
        rx_set_sp(E);
        return t;
    }
    rx_set_sp(E);
    return rx_adds(F32(d, 0x10a4), lead);
}
RX_FUNCTION(0x00613a30, rx_waypoint_projected_time)

/* ---- 00615450 (stdcall walker {+4 activity}, criteria vector, column list, driver; RET 0x10): a Waypoint driver's cells, per column
 * type of the list: 0 TOTAL (00613a30, "MM:SS.cc" by 00790100, sort value -t), 1 BEST LAP (+0x10b0; the Waypoint's list has no such
 * column); other types nothing. Frame: sub 0x94; push ebx / ebp / esi / edi; the walker at +0x14, the TOTAL at +0x10, the text at
 * +0x18 (0x80). */
void rx_results_waypoint_columns(rx_addr ctx, rx_addr vec, rx_addr cols, rx_addr d) {
    ACT_FRAME(L, 0x94, 4);
    const rx_addr B = L + 0x18;
    SU32(L, 0x14, ctx);
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 0: {
            rx_set_sp(L - 8);                                                     /* push driver; push activity */
            float t;
            ACT_CALLR(t, 0, rx_waypoint_projected_time(U32(ctx, 4), d));          /* 00613a30 (cdecl) */
            SF32(L, 0x10, t);
            ACT_CALL(0, rx_lap_time_text(B, t, 1));                               /* 00790100 (EAX = text, xmm0, EDI = 1) */
            rx_set_sp(L);                                                         /* add esp, 4: the sort value's slot */
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, rx_subs(-0.0f /* 0x9774a8 */, F32(L, 0x10))));
            break;
        }
        case 1:
            ACT_CALL(0, rx_lap_time_text(B, F32(d, 0x10b0), 1));
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, rx_subs(-0.0f, F32(d, 0x10b0))));
            break;
        default:
            break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00615450, rx_results_waypoint_columns)

/* ---- 00614bf0 (ECX = driver; stdcall criteria vector, column list; RET 8): a Freestyle driver's cells (player P = +0x74), per column
 * type: 3 TOTAL SCORE (P+0xcec, "%3.2f" by 0078fd60, sort value the score), 4 BEST TRICK SCORE (with a best trick record P+0x10a0:
 * 0050de10's score in "%3.2f", sort value that score; without one empty, 0), 5 BEST TRICK (the record's LocString key +0x40 as the
 * text, sort value 0; without one empty); other types nothing. The sort values pass through the x87 stack (fld / fstp). Frame: sub
 * 0x9c; push ebx / ebp / esi / edi; the driver at +0x10, the column count at +0x14, the best trick score at +0x18, the score at
 * +0x1c, the text at +0x20 (0x80). */
void rx_results_freestyle_columns(rx_addr d, rx_addr vec, rx_addr cols) {
    ACT_FRAME(L, 0x9c, 4);
    const rx_addr B = L + 0x20;
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    SU32(L, 0x10, d);
    SU32(L, 0x14, (uint32_t)n);
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        const rx_addr P = U32(d, 0x74);
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 3: {
            const float score = F32(P, 0xcec);
            SF32(L, 0x1c, score);
            const double v = (double)score;
            uint32_t w[2]; memcpy(w, &v, 8);
            results_format(B, 0x80, 0x00957f18 /* "%3.2f" */, 2, w);              /* 0078fd60 (EDI = text, ESI = 0x80; the double below the format) */
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, results_x87_pass(F32(L, 0x1c))));
            break;
        }
        case 4:
            if (U32(P, 0x10a0) != 0) {
                float best;
                ACT_CALLR(best, 0, rx_freestyle_driver_best_score(d));            /* 0050de10 (EAX = driver) -> xmm0 */
                SF32(L, 0x18, best);
                const double v = (double)best;
                uint32_t w[2]; memcpy(w, &v, 8);
                results_format(B, 0x80, 0x00957f18 /* "%3.2f" */, 2, w);
                ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, results_x87_pass(F32(L, 0x18))));
            } else {
                results_empty(B);
                ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, 0.0f));          /* fldz; fstp */
            }
            break;
        case 5: {
            const rx_addr rec = U32(P, 0x10a0);
            if (rec != 0) ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, U32(rec, 0x40), 0.0f));
            else {
                results_empty(B);
                ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, 0.0f));
            }
            break;
        }
        default:
            break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00614bf0, rx_results_freestyle_columns)

/* ---- 006152e0 (ECX = driver; stdcall criteria vector, column list; RET 8): a Section Race driver's cells. First its best section
 * (005133f0: the index of its lowest section time and that time; -1 / FLT_MAX with none, the locals start at 0 / 0.0); then per
 * column type: 6 SECTIONS (the sections won +0x1134 by App_FormatCredits 005dc430, "1,250"; sort value the count), 7 BEST SECTION
 * TIME (00790100 "MM:SS.cc", sort value the time through the x87 stack), 8 BEST SECTION (the index + 1 by "%i", "" for -1; sort
 * value the index); other types nothing. Frame: sub 0x9c; push ebx / ebp / esi / edi; the best time at +0x14, the best index at
 * +0x18, the driver at +0x1c, the text at +0x20 (0x80). */
void rx_results_section_columns(rx_addr d, rx_addr vec, rx_addr cols) {
    ACT_FRAME(L, 0x9c, 4);
    const rx_addr B = L + 0x20;
    SU32(L, 0x1c, d);
    SU32(L, 0x18, 0);
    SF32(L, 0x14, 0.0f);
    ACT_CALL(2, rx_section_driver_best(d, L + 0x18, L + 0x14));                   /* 005133f0 (ECX = driver; stdcall &index, &time) */
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 6:
            rx_set_sp(L - 4);                                                     /* push the text (cdecl: stays for the sort value) */
            ACT_CALL(0, (void)rx_app_format_credits(I32(d, 0x1134), 0x80, B));   /* 005dc430 (ECX = value, EDX = 0x80) */
            rx_set_sp(L);
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)I32(d, 0x1134)));
            break;
        case 7:
            ACT_CALL(0, rx_lap_time_text(B, F32(L, 0x14), 1));                    /* 00790100 (EAX = text, xmm0, EDI = 1) */
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, results_x87_pass(F32(L, 0x14))));
            break;
        case 8: {
            const int32_t best = I32(L, 0x18);
            if (best == -1) results_format(B, 0x80, 0x00935d6c /* "" */, 0, NULL);
            else { const uint32_t va[1] = {(uint32_t)(best + 1)}; results_format(B, 0x80, 0x0094be60 /* "%i" */, 1, va); }
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)I32(L, 0x18)));
            break;
        }
        default:
            break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x006152e0, rx_results_section_columns)

/* ---- the party modes' fillers (w-partyresults). Each writes, per column type of its list, the WINS cell (type 0xa: the driver's rounds
 * won as 005dc3c0 writes it, "1,250", into a UTF-16 buffer, the empty string for -1, converted to UTF-8 by WideCharToMultiByte
 * (CP_UTF8, the import 0x009160a8) into the text, whose last byte is then 0; sort value the count) and the mode's own cell. Each frame
 * holds the security cookie ([0x009b45e4] ^ ESP after the sub) at its top, the text (0x80) and the UTF-16 buffer (0x800) below. */
static void results_party_frame_cookie(rx_addr esp0, uint32_t at) { SU32(esp0, at, U32(0, 0x009b45e4) ^ esp0); }
/* the count cell's text: 005dc3c0 (ECX = value, EDI = W) unless -1 (an empty UTF-16 string), then WideCharToMultiByte at the frame's ESP */
static void results_count_text(rx_addr L, rx_addr W, rx_addr B, int32_t v) {
    if (v == -1) SU16(W, 0, 0);
    else ACT_CALL(0, (void)rx_format_thousands_w((uint32_t)v, W));
    rx_set_sp(L);
    (void)rx_results_wcmb(W, B, 0x80);
}

/* ---- 00614dc0 (stdcall criteria vector, column list, driver; RET 0xc): a Tag driver's cells: 0xa WINS (+0xf1c, the rounds won), 0xb
 * TAG IT TIME (the total holding time +0xf18 as "MM:SS.cc" by 00790100 with EDI = 1; sort value the time through the x87 stack); other
 * types nothing. Frame: sub 0x888 (the cookie at +0x884); push ebx / ebp / esi / edi; the text at +0x10, the UTF-16 buffer at +0x90. */
void rx_results_tag_columns(rx_addr vec, rx_addr cols, rx_addr d) {
    ACT_FRAME(L, 0x888, 4);
    results_party_frame_cookie(L + 0x10, 0x884);
    const rx_addr B = L + 0x10, W = L + 0x90;
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 0xa:
            results_count_text(L, W, B, I32(d, 0xf1c));
            SU8(B, 0x7f, 0);
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)I32(d, 0xf1c)));
            break;
        case 0xb:
            ACT_CALL(0, rx_lap_time_text(B, F32(d, 0xf18), 1));                   /* 00790100 (EAX = text, xmm0, EDI = 1) */
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, results_x87_pass(F32(d, 0xf18))));
            break;
        default:
            break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00614dc0, rx_results_tag_columns)

/* ---- 00614f00 (ECX = driver; stdcall criteria vector, column list; RET 8): a Snake driver's cells: 0xa WINS (+0xf24), 0xc SNAKE
 * ELIMINATIONS (the kills +0xf2c, the same count text; sort value the count); other types nothing. Frame: sub 0x894 (the cookie at
 * +0x88c); push ebx / ebp / esi / edi; the driver at +0x14, the text at +0x18, the UTF-16 buffer at +0x98. */
void rx_results_snake_columns(rx_addr d, rx_addr vec, rx_addr cols) {
    ACT_FRAME(L, 0x894, 4);
    results_party_frame_cookie(L + 0x10, 0x88c);
    const rx_addr B = L + 0x18, W = L + 0x98;
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    SU32(L, 0x14, d);
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        uint32_t field;
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 0xa: field = 0xf24; break;
        case 0xc: field = 0xf2c; break;
        default: continue;
        }
        results_count_text(L, W, B, I32(d, field));
        SU8(B, 0x7f, 0);
        ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)I32(d, field)));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00614f00, rx_results_snake_columns)

/* ---- 00615070 (stdcall criteria vector, column list, driver; RET 0xc): an Infection driver's cells: 0xa WINS (+0xf18), 0xd OUTBREAK
 * INFECTED: the original's placeholder text "TODO" (strncpy 0x7f through msvcr90's import; sort value 0 by fldz); other types nothing.
 * Frame: sub 0x890 (the cookie at +0x88c); push ebx / ebp / esi / edi; the driver at +0x14, the text at +0x18, the UTF-16 buffer at
 * +0x98. */
void rx_results_infection_columns(rx_addr vec, rx_addr cols, rx_addr d) {
    ACT_FRAME(L, 0x890, 4);
    results_party_frame_cookie(L + 0x10, 0x88c);
    const rx_addr B = L + 0x18, W = L + 0x98;
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    SU32(L, 0x14, d);
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 0xa:
            results_count_text(L, W, B, I32(d, 0xf18));
            SU8(B, 0x7f, 0);
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)I32(d, 0xf18)));
            break;
        case 0xd:
            ACT_CALL(3, (void)crt_strncpy(B, 0x00957f20 /* "TODO" */, 0x7f));
            SU8(B, 0x7f, 0);
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, 0.0f));
            break;
        default:
            break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00615070, rx_results_infection_columns)

/* ---- 006151a0 (ECX = driver; stdcall criteria vector, column list; RET 8): an EggHunt driver's cells: 0xa WINS (+0xf14), 0xe GREED
 * POINTS (+0xf18 by App_FormatCredits 005dc430, "1,250", -1 empty; sort value the points); other types nothing. Frame: sub 0x894 (the
 * cookie at +0x88c); push ebx / ebp / esi / edi; the driver at +0x14, the text at +0x18, the UTF-16 buffer at +0x98. */
void rx_results_egghunt_columns(rx_addr d, rx_addr vec, rx_addr cols) {
    ACT_FRAME(L, 0x894, 4);
    results_party_frame_cookie(L + 0x10, 0x88c);
    const rx_addr B = L + 0x18, W = L + 0x98;
    const int32_t n = (int32_t)(U32(cols, 0x10) - U32(cols, 0xc)) >> 2;
    SU32(L, 0x14, d);
    for (int32_t k = 0; k < n; k++) {
        if ((uint32_t)k >= (U32(cols, 0x10) - U32(cols, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        switch (U32(U32(cols, 0xc), 4u * (uint32_t)k)) {
        case 0xa:
            results_count_text(L, W, B, I32(d, 0xf14));
            SU8(B, 0x7f, 0);
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)I32(d, 0xf14)));
            break;
        case 0xe: {
            const int32_t v = I32(d, 0xf18);
            rx_set_sp(L - 4);                                                     /* push the text (cdecl: stays for the sort value) */
            ACT_CALL(0, (void)rx_app_format_credits(v, 0x80, B));                /* 005dc430 (ECX = value, EDX = 0x80) */
            rx_set_sp(L);
            ACT_CALL(4, rx_results_cell(vec, (uint32_t)k, B, (float)v));
            break;
        }
        default:
            break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x006151a0, rx_results_egghunt_columns)

/* ---- 00614890 (ESI = walker, EAX = column list, ECX = driver; stdcall criteria vector, activity type; RET 8): the type's filler.
 * The race types 0 / 1 / 0x11 / 0x13 are 00614950 and Waypoint 6 00615450 (called below the dispatcher's push edi); Freestyle 5
 * (00614bf0) and Section Race 0xc (006152e0) are tail jumps with their own arguments (vector, list) over the dispatcher's and ECX =
 * the driver. The party modes' (w-partyresults): Tag 9 (00614dc0) and Infection 0xe (00615070) are called as the race types' (stdcall
 * vector, list, driver), Snake 10 (00614f00) and EggHunt 0xf (006151a0) tail jumps as Freestyle's. PokerRace 0xd has no filler (and
 * no column list in 00613dd0): its rows hold Index and Name only. Other types: no cells. */
void rx_results_columns(rx_addr ctx, rx_addr vec, rx_addr cols, rx_addr d, uint32_t type) {
    switch (type) {
    case 0: case 1: case 0x11: case 0x13: case 6: {
        ACT_FRAME(L, 0, 1);                                        /* push edi (the dispatcher's frame: the filler's stack is below it) */
        if (type == 6) ACT_CALL(4, rx_results_waypoint_columns(ctx, vec, cols, d));
        else ACT_CALL(4, rx_results_race_columns(ctx, vec, cols, d));
        ACT_END(L);
        return;
    }
    case 5: case 0xc: {
        const rx_addr E = rx_sp();                                 /* pop edi; the filler's arguments over the dispatcher's */
        SU32(E, 8, cols);
        SU32(E, 4, vec);
        if (type == 5) rx_results_freestyle_columns(d, vec, cols);
        else rx_results_section_columns(d, vec, cols);
        return;
    }
    case 9: case 0xe: {
        ACT_FRAME(L, 0, 1);                                        /* push edi (the dispatcher's frame) */
        if (type == 9) ACT_CALL(3, rx_results_tag_columns(vec, cols, d));
        else ACT_CALL(3, rx_results_infection_columns(vec, cols, d));
        ACT_END(L);
        return;
    }
    case 10: case 0xf: {
        const rx_addr E = rx_sp();                                 /* pop edi; the filler's arguments over the dispatcher's */
        SU32(E, 8, cols);
        SU32(E, 4, vec);
        if (type == 10) rx_results_snake_columns(d, vec, cols);
        else rx_results_egghunt_columns(d, vec, cols);
        return;
    }
    default:
        return;
    }
}
RX_FUNCTION(0x00614890, rx_results_columns)

/* ---- 00616000 (EDX = name; stdcall criterion; RET 4) -> EAX = the criterion: "Name" = the C string (a string criterion, op 0) */
rx_addr rx_results_name_crit(rx_addr name, rx_addr c) {
    ACT_FRAME(L, 0x20, 4);
    ACT_CALL(2, rx_tbl_crit_init_string(c, 0x0093a3bc /* "Name" */, 0, name));
    ACT_END(L);
    return c;
}
RX_FUNCTION(0x00616000, rx_results_name_crit)

/* ---- 00613830 (EAX = the column type) -> EAX: the column's title key "STR_ActivityResultsColumn_<name>" (names 0x009d732c: TotalTime,
 * BestLapTime, PenaltyTime, ...) formatted into 0x00da9ca8 (0x800) and upper-cased in place by the case table 0x009b76c8. Frame:
 * push esi / edi. */
rx_addr rx_results_column_title(uint32_t type) {
    ACT_FRAME(L, 0, 2);
    const uint32_t va[2] = {0x00957c34 /* "ActivityResultsColumn" */, U32(0, 0x009d732c + 4u * type)};
    results_format(0x00da9ca8, 0x800, 0x00935e2c /* "STR_%s_%s" */, 2, va);
    for (rx_addr p = 0x00da9ca8; U8(p, 0) != 0; p++) SU8(p, 0, U8(0, 0x009b76c8 + U8(p, 0)));
    ACT_END(L);
    return 0x00da9ca8;
}
RX_FUNCTION(0x00613830, rx_results_column_title)

/* ---- 005ba450 (EDI = std::vector<int>; stdcall &value; RET 4) push_back (005c1d30 at the end / 005ba8d0 _Insert_n) */
void rx_vector_int_push(rx_addr vec, rx_addr valp) { rx_tbl_vector_push_ptr(vec, valp); }
RX_FUNCTION(0x005ba450, rx_vector_int_push)

/* ---- 00613dd0 (ECX = the walker {+0 vtable 0x957f84, +4 the activity}) -> EAX = the number of columns: the table emptied
 * (004ecdb0); without an activity that is all (EAX 0). Else row 0 = the titles: "Name" = "STR_PLAYERNAME", the column list of the
 * activity type (std::vector<int>; Eliminator {9, 0, 1}, the other races {0, 2, 1}, Freestyle {3, 4, 5}, Waypoint {0}, Section Race {6, 8, 7},
 * the party modes {0xa, ..}), "Column<k>" = the column's title key (00613830) for k < min(columns, [0x00ae1934] = 5); then per driver of the
 * standings (activity +0x2d0, +0x26c drivers) a row from the criteria {"Index" = k (an inline int criterion), "Name" = the player's
 * name +0x54 (00616000), the filler's cells (00614890)} (004ec950). Frame: sub 0x174; push ebx / ebp / esi / edi; at +0x14 the
 * column value / the row index, +0x18 the column index / the standings offset, +0x1c the column list, +0x34 the title string / the
 * row's criteria, +0x50 the column count, +0x54 the walker, +0x58 the drivers, +0x5c the player, +0x60 the driver, +0x64 the row
 * reference {row, , table}, +0x70 the header's criteria, +0x88 the criterion, +0x138 "Column<k>" (0x40). */
uint32_t rx_results_populate(rx_addr ctx) {
    ACT_FRAME(L, 0x174, 4);
    SU32(L, 0x54, ctx);
    if (U32(ctx, 4) == 0) {
        ACT_CALL(1, rx_tbl_table_clear_rows(RESULTS_TABLE));
        ACT_END(L);
        return 0;
    }
    ACT_CALL(1, rx_tbl_table_clear_rows(RESULTS_TABLE));
    rx_addr px;
    ACT_CALLR(px, 1, rx_reflex_aligned_alloc(4));
    if (px) SU32(px, 0, L + 0x70);
    SU32(L, 0x70, px);
    SU32(L, 0x7c, 0); SU32(L, 0x80, 0); SU32(L, 0x84, 0);
    ACT_CALL(2, (void)rx_tbl_add_row(RESULTS_TABLE, L + 0x64, L + 0x70));   /* row 0: the titles */
    const rx_addr S = L + 0x34;
    SU32(S, 0x18, 0xf); SU32(S, 0x14, 0); SU8(S, 4, 0);
    ACT_CALL(1, rx_tbl_str_assign_n(S, 0x00957f08 /* "STR_PLAYERNAME" */, 0xe));
    const rx_addr t = U32(L, 0x6c);
    uint32_t c;
    ACT_CALLR(c, 1, rx_tbl_column_index(t, 0x0093a3bc /* "Name" */));
    SU32(t, 0x68, U32(t, 0x68) + 1);
    if (c >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) crt_invalid_parameter_noinfo();
    ACT_CALL(2, rx_tbl_set_string(U32(U32(t, 0x10), 4 * c), U32(L, 0x64), S));
    if (U32(S, 0x18) >= 0x10) ACT_CALL(1, rx_tbl_free(U32(S, 4)));
    /* the column list */
    const rx_addr V = L + 0x1c, X = L + 0x14;
    ACT_CALLR(px, 1, rx_reflex_aligned_alloc(4));
    if (px) SU32(px, 0, V);
    SU32(V, 0, px);
    const rx_addr A = U32(ctx, 4);
    SU32(V, 0xc, 0); SU32(V, 0x10, 0); SU32(V, 0x14, 0);
#define COLUMN(v) do { SU32(X, 0, (v)); ACT_CALL(1, rx_vector_int_push(V, X)); } while (0)
    switch (U32(A, 0x23c)) {
    case 0: case 1: case 0x11: case 0x13:
        if (U32(A, 0x238) == 5) { COLUMN(9); COLUMN(0); } else { COLUMN(0); COLUMN(2); }
        COLUMN(1);
        break;
    case 5: COLUMN(3); COLUMN(4); COLUMN(5); break;
    case 6: COLUMN(0); break;
    case 9: COLUMN(0xa); COLUMN(0xb); break;
    case 10: COLUMN(0xa); COLUMN(0xc); break;
    case 0xc: COLUMN(6); COLUMN(8); COLUMN(7); break;
    case 0xe: COLUMN(0xa); COLUMN(0xd); break;
    case 0xf: COLUMN(0xa); COLUMN(0xe); break;
    default: break;
    }
#undef COLUMN
    int32_t ncols = (int32_t)(U32(V, 0x10) - U32(V, 0xc)) >> 2;
    if (I32(0, 0x00ae1934) < ncols) ncols = I32(0, 0x00ae1934);
    SU32(L, 0x50, (uint32_t)ncols);
    SU32(L, 0x18, 0);
    for (int32_t k = 0; k < ncols; k++) {
        const uint32_t va[1] = {(uint32_t)k};
        results_format(L + 0x138, 0x40, 0x0093dfe8 /* "Column%i" */, 1, va);
        if ((uint32_t)k >= (U32(V, 0x10) - U32(V, 0xc)) >> 2) crt_invalid_parameter_noinfo();
        rx_addr name;
        ACT_CALLR(name, 0, rx_results_column_title(U32(U32(V, 0xc), 4u * (uint32_t)k)));
        SU32(S, 0x18, 0xf); SU32(S, 0x14, 0); SU8(S, 4, 0);
        ACT_CALL(1, rx_tbl_str_assign_n(S, name, crt_strlen(name)));
        ACT_CALLR(c, 1, rx_tbl_column_index(t, L + 0x138));
        SU32(t, 0x68, U32(t, 0x68) + 1);
        if (c >= (U32(t, 0x14) - U32(t, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        ACT_CALL(2, rx_tbl_set_string(U32(U32(t, 0x10), 4 * c), U32(L, 0x64), S));
        if (U32(S, 0x18) >= 0x10) ACT_CALL(1, rx_tbl_free(U32(S, 4)));
        SU32(L, 0x18, (uint32_t)(k + 1));
    }
    /* the drivers' rows */
    const int32_t rows = I32(U32(ctx, 4), 0x26c);
    SU32(L, 0x58, (uint32_t)rows);
    SU32(L, 0x14, 0);
    if (rows > 0) {
        SU32(L, 0x18, 0x2d0);
        const rx_addr R = L + 0x34, C = L + 0x88;
        for (int32_t i = 0; i < rows; i++) {
            const rx_addr d = U32(U32(U32(L, 0x54), 4) + U32(L, 0x18), 0);
            SU32(L, 0x60, d);
            SU32(L, 0x5c, U32(d, 0x74));
            ACT_CALLR(px, 1, rx_reflex_aligned_alloc(4));
            if (px) SU32(px, 0, R);
            SU32(R, 0, px);
            SU32(R, 0xc, 0); SU32(R, 0x10, 0); SU32(R, 0x14, 0);
            /* "Index" = i (the inline int criterion: column -1, op 0, variant {1, i}, an empty path) */
            SU32(C, 0x80, 0xffffffffu); SU32(C, 0x84, 0); SU32(C, 0x88, 1); SU32(C, 0x90, (uint32_t)i);
            ACT_CALLR(px, 1, rx_reflex_aligned_alloc(4));
            if (px) SU32(px, 0, C + 0x98);
            SU32(C, 0x98, px);
            SU32(C, 0xa4, 0); SU32(C, 0xa8, 0); SU32(C, 0xac, 0);
            ACT_CALL(3, (void)crt_strncpy(C, 0x009361a8 /* "Index" */, 0x7f));
            SU8(C, 0x7f, 0);
            ACT_CALL(1, rx_tbl_crit_push(R, C));
            ACT_CALL(0, rx_fx_crit_destroy(C));
            rx_addr nc;
            ACT_CALLR(nc, 1, rx_results_name_crit(U32(L, 0x5c) + 0x54, C));
            ACT_CALL(1, rx_tbl_crit_push(R, nc));
            ACT_CALL(0, rx_fx_crit_destroy(C));
            ACT_CALL(2, rx_results_columns(U32(L, 0x54), R, V, U32(L, 0x60), U32(U32(U32(L, 0x54), 4), 0x23c)));
            ACT_CALL(2, (void)rx_tbl_add_row(RESULTS_TABLE, L + 0x64, R));
            if (U32(R, 0xc) != 0) {
                ACT_CALL(3, rx_fx_crit_range_destroy(U32(R, 0xc), U32(R, 0x10)));   /* 0041ac70 (begin, end, the drivers) */
                ACT_CALL(1, rx_reflex_aligned_free(U32(R, 0xc)));
            }
            px = U32(R, 0);
            SU32(R, 0xc, 0); SU32(R, 0x10, 0); SU32(R, 0x14, 0);
            ACT_CALL(1, rx_crt_aligned_free(px));                                  /* _aligned_free (import), under the allocator lock */
            SU32(L, 0x18, U32(L, 0x18) + 4);
            SU32(L, 0x14, (uint32_t)(i + 1));
        }
    }
    if (U32(V, 0xc) != 0) ACT_CALL(1, rx_reflex_aligned_free(U32(V, 0xc)));
    px = U32(V, 0);
    SU32(V, 0xc, 0); SU32(V, 0x10, 0); SU32(V, 0x14, 0);
    ACT_CALL(1, rx_reflex_aligned_free(px));
    ACT_CALL(0, rx_fx_crit_vec_destroy(L + 0x70));                                 /* 00417590 (ESI = the header's criteria) */
    const uint32_t r = U32(L, 0x50);
    ACT_END(L);
    return r;
}
RX_FUNCTION(0x00613dd0, rx_results_populate)

/* ---- App_PopulateResultsTable 00742c40 (the Lua binding's body; cdecl, no arguments): the table emptied (004ecdb0); then, when the
 * running world (the alternate context's [0xa95844]+0x20 world 0x5a54 when its +0x34 == 1, else the main one's +0x1c world 0x20c8)
 * has an activity (+0x21da8), 00613dd0 with a walker {0x957f84, the world's first activity (+0x21da4 [0], 005ee840)} on the stack.
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub 0xc; push esi; the walker at +8. */
void rx_app_populate_results_table(void) {
    ACT_FRAME_EBP(L, 8, 0xc, 1);
    ACT_CALL(1, rx_tbl_table_clear_rows(RESULTS_TABLE));
    const rx_addr ctx = U32(0, 0x00a95844), game = U32(0, 0x00a946b8);
    rx_addr world = 0;
    if (U32(U32(ctx, 0x20), 0x34) == 1) world = U32(game, 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) world = U32(game, 0x20c8);
    if (U32(world, 0x21da8) != 0) {
        SU32(L, 8, 0x00957f84);
        SU32(L, 0xc, U32(U32(world, 0x21da4), 0));                                 /* 005ee840 (ECX = the activity list; 0): &[0] */
        ACT_CALL(0, (void)rx_results_populate(L + 8));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00742c40, rx_app_populate_results_table)

/* ---- the product's results page start ----------------------------------------------------------------------------------------
 * The original (measured in the oracle, w-resultsrng: event boots NAT_Concord 1 / 3 laps and SX_Springfield 2 laps, 12 riders): the
 * race activity's Post sub-state 2 (00569a40) raises "Show Activity Results"; the HUD page's PostEventOverlay (005f42e0) answers in
 * that frame: arcade (not tour, series manager +0x190 != 2): UIRoutingMessage "GotoPostEvent" (activity type 8: the menu page instead)
 * and "GotoHub" (0059b7e0), in a career it queues "MXSeriesManager::AdvancePostEvent" whose vf13 sets the same routing and sends
 * "GotoHub" itself. The hub's edge GotoPostEvent reaches the page PostEventResults; its Start (the Lua) runs App.PopulateResultsTable
 * RESULTS_PAGE_FRAMES frames after the "GotoHub", in the UI manager's update (UIManager::vf19 005db820, the first ticker of the
 * engine context's scene +0x24, which Engine::Update ticks after the event scene +0x20: after MXGame::Update's and the event scene's
 * draws, before the job pool's particle jobs). The product has no UI pages: it starts the same countdown at the same two places
 * (rx_results_page_observe for the arcade, rx_results_page_hub for the career's vf13) and runs App.PopulateResultsTable where the
 * UI scene's tick begins (rx_results_page_frame, loop_engine.c). For the offline modes whose filler is ported (the race types, the
 * Eliminator among them, Freestyle, Waypoint, Section Race) and an offline session. Where the UI page is when the routing comes
 * decides the delay (w-wpfill, measured per mode, core/plan/QUEUE.md): from the HUD page RESULTS_PAGE_FRAMES frames (its exit), but
 * when the activity's finish scenario played (Post sub-state 1: the Waypoint and the Section Race always, a race / Freestyle only
 * with [0x00d60ff0]) the overlay's "Post Scenarios Start" handler (005f4220) had routed the UI to the Event Outro page
 * ("GotoEventOutroPage"), from which the page starts at the very next UI tick, in the same frame as "Show Activity Results"
 * (rx_results_page_outro). The page start is the presentation's in the original, not the activity's:
 * a restart / quit before it cancels it (the activity left Post). The state is a host static (reset with the session's statics,
 * src/rx_reset.c). The countdown and the page start also build in the oracle, where core/oracle/results_probe.cpp drives them from
 * the original's own events to check the product's timing against the original's page (RX_RESULTS_PAGE=replace). */
#define RESULTS_PAGE_FRAMES 14
static struct { rx_addr activity; int32_t wait; rx_addr done; uint32_t count; rx_addr outro; } results_page = {0, -1, 0, 0, 0};

static int results_page_supported(rx_addr act) {
    if (!act) return 0;
    switch (U32(act, 0x23c)) {
    case 0: case 1: case 0x11: case 0x13: case 5: case 6: case 0xc: break;
    case 9: case 10: case 0xe: break;   /* the party modes (w-partyresults): their page's Lua fills the table as a race's does (measured: the
                                           original's App.PopulateResultsTable 15 UI ticks after "Show Activity Results"). Not PokerRace 0xd:
                                           after its "Show Activity Results" (Post sub-state 2, HUD route GotoPokerRaceSinglePlayer)
                                           the post-event overlay's handler 005f42e0 never runs, so no page start (measured); not EggHunt
                                           0xf: no level can run it (STOP_AUDIT R32), its page is unmeasured */
    default: return 0;
    }
    race_static_dcf304();
    const rx_addr g = U32(0, 0x00da6cb0);
    return !(U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1);
}
/* the overlay's "Post Scenarios Start" (005f4220) during the activity's Post: the UI is on the Event Outro page from then on (the
 * mark ends when the activity leaves Post: rx_results_page_frame) */
void rx_results_page_outro(rx_addr act) {
    if (act && U32(act, 0x21c) == 4) results_page.outro = act;
}
/* the hub's routing to the results page: the countdown to PostEventResults:Start (later UI ticks: extra) */
static void results_page_start(rx_addr act, int32_t extra) {
    if (!results_page_supported(act)) return;
    results_page.activity = act;
    results_page.wait = (results_page.outro == act ? 0 : RESULTS_PAGE_FRAMES) + extra;
}
void rx_results_page_show(rx_addr act) { results_page_start(act, 0); }
#ifndef RX_ORACLE
#include "career_properties.h"
/* "Show Activity Results" (the canonical dispatch 004020c0's host observation, world_veh_entity.c): the arcade branch of 005f42e0 */
void rx_results_page_observe(rx_addr channel, rx_addr name, uint32_t payload, uint32_t bytes) {
    if (!name) return;
    if (!crt_strcmp(name, 0x00941c04 /* "Post Scenarios Start" */)) {
        /* 005f4220 (registered by the HUD page's overlay on the activity's channel): the payload is 00587ea0's 8 bytes
         * {activity, the scenario's name} */
        if (bytes != 8 || !payload) return;
        const rx_addr act = U32(payload, 0);
        if (act && channel == U32(act, 0x290) && results_page_supported(act)) rx_results_page_outro(act);
        return;
    }
    if (crt_strcmp(name, 0x00941c30 /* "Show Activity Results" */) || bytes != 4 || !payload) return;
    const rx_addr act = U32(payload, 0);
    if (!act || channel != U32(act, 0x290)) return;
    const rx_addr manager = rx_fx_series_manager();             /* 00413d20 (constructed on first use, as 005f4346 does) */
    if (U32(0, 0x009e2318) == 1 || U32(manager, 0x190) == 2) return;   /* tour: the series event's vf13 routes (rx_results_page_hub) */
    if (U32(act, 0x23c) == 8) return;                           /* "GotoPostEventMenuPage" */
    rx_results_page_show(act);
}
/* the career's "GotoHub" (the series event's vf13 after UIRoutingMessage): the routing "GotoPostEvent" reaches the results page */
void rx_results_page_hub(void) {
    char route[32];
    RX_FRAME_ENTER(F, 32);
    const uint32_t found = rx_owned_property_get_string(0x0094cc74 /* "UIRoutingMessage" */, F, 32);
    for (uint32_t i = 0; i < 32; i++) route[i] = (char)U8(F, i);
    RX_FRAME_LEAVE(F, 32);
    route[31] = 0;
    if (!found || strcmp(route, "GotoPostEvent") != 0) return;
    /* the original queues "MXSeriesManager::AdvancePostEvent" (00596290) and the series manager's channel runs it in that frame's
     * UI tick (UIManager::vf41 005db6d0 drains it, after the page check: measured, the Eliminator series 5), so its routing counts
     * from the next UI tick; the product runs the vf13 at once, in the activity's update (career_owned_results.c): one tick more */
    const rx_addr event = U32(rx_fx_series_manager(), 0x18c);
    if (event) results_page_start(U32(event, 0x14), 1);
}
#endif
/* the UI scene's tick (Engine::Update, loop_engine.c): the page start when its countdown ends */
void rx_results_page_frame(void) {
    if (results_page.outro && U32(results_page.outro, 0x21c) != 4) results_page.outro = 0;
    if (results_page.wait < 0) return;
    const rx_addr act = results_page.activity;
    if (!act || U32(act, 0x21c) != 4) { results_page.activity = 0; results_page.wait = -1; return; }
    if (results_page.wait > 0) { results_page.wait--; return; }
    results_page.wait = -1;
    if (results_page.done != act) results_page.count = 0;
    rx_app_populate_results_table();
    results_page.done = act;
    results_page.count++;
}
uint32_t rx_results_page_populated(rx_addr* activity) {
    if (activity) *activity = results_page.done;
    return results_page.count;
}
