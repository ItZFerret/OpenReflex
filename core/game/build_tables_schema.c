/* build_tables_schema.c -- unit 12a: the columns of the static tables (build_tables.h). The schema functions FUN_00756760 runs
 * (00756887 .. 00756936) are column lists: FUN_004f0340 reserve, then FUN_004f69d0 / 004f6f90 / 004f6ae0 / 004f7180 per column with
 * a default on the frame (a std::string built by FUN_004f2ce0 for string columns, freed after) and a few globals set at the end.
 * Each is the table of build_tables_schemas.inc run by one interpreter here; ActivityResults_CreateSchema (a loop of formatted
 * names) is written out. */
#include "build_tables.h"
#include "build_alloc.h"
#include "build_vehicle.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>

enum { OP_END, OP_RESERVE, OP_INT, OP_FLOAT, OP_STRING, OP_INT64, OP_GLOBAL, OP_CLEAR, OP_CRIT, OP_ADDROW };
typedef struct { uint32_t op, table, name, def, flags, parent, pcol, a7; } schema_op;
#define PARENT_FIND(lit) (0x80000000u | (lit))   /* Database_FindTable(0x00ae0910, lit) at the call */
#include "build_tables_schemas.inc"

static uint32_t gstrlen(rx_addr p) { uint32_t n = 0; while (U8(p, n)) n++; return n; }

static void run_schema_on(const schema_op* s, rx_addr from, rx_addr to);
static void run_schema(const schema_op* s) { run_schema_on(s, 0, 0); }
/* FUN_004e1820 / FUN_004e0a40 on another table (EDI = the table): the NetworkRider / NetworkLBStats column lists */
void rx_tbl_rider_schema(rx_addr t) { run_schema_on(S_004e1820_ae07a8, 0x00ae07a8, t); }
void rx_tbl_stats_schema(rx_addr t) { run_schema_on(S_004e0a40_ae04d8, 0x00ae04d8, t); }
static void run_schema_on(const schema_op* s, rx_addr from, rx_addr to) {
    RX_FRAME_ENTER(F, 0x140);
    const rx_addr def = F + 0x10, str = F + 0x18, vec = F + 0x38, crit = F + 0x50, out = F + 0x100;
    int have_vec = 0;
    for (; s->op != OP_END; s++) {
        rx_addr parent = s->parent;
        rx_addr table = s->table;
        if (from && table == from) table = to;
        else if (from && table == from + 4) table = to + 4;
        if (parent & 0x80000000u) parent = rx_tbl_find(RX_TBL_DEFAULT_DB, parent & 0x7fffffffu);   /* 00402220 */
        switch (s->op) {
        case OP_RESERVE: rx_tbl_vector_reserve_ptr(table, s->def); break;
        case OP_INT: SU32(def, 0, s->def); rx_tbl_add_int_column(table, s->name, def, s->flags, parent, s->pcol, s->a7); break;
        case OP_FLOAT: SU32(def, 0, s->def); rx_tbl_add_float_column(table, s->name, def, s->flags, parent, s->pcol); break;
        case OP_STRING:
            rx_tbl_str_init(str);
            rx_tbl_str_assign_n(str, s->def, gstrlen(s->def));          /* 004f2ce0 */
            rx_tbl_add_string_column(table, s->name, str, s->flags, parent, s->pcol);
            rx_tbl_str_tidy(str);                                       /* 004f3320 when on the heap */
            break;
        case OP_INT64: SU32(def, 0, s->def); SU32(def, 4, 0); rx_tbl_add_int64_column(table, s->name, def); break;
        case OP_GLOBAL: SU32(0, s->table, s->def); break;
        /* rows (FUN_004e3c50 / 004e5f70): one criteria vector, cleared before each row (004f2c40 / 004f1790), an int criterion per
         * value (00419a70 + 004175e0 + the criterion destroyed (00415320)), the row added (004ec950); the vector destroyed at the end
         * (00417590) */
        case OP_CLEAR:
            if (!have_vec) { rx_tbl_crit_vec_init(vec); have_vec = 1; }
            else { rx_tbl_crit_vec_clear(vec); }
            break;
        case OP_CRIT:
            rx_tbl_crit_init_int(crit, s->name, 0, s->def);
            rx_tbl_crit_push(vec, crit);
            rx_tbl_crit_destroy(crit);
            break;
        case OP_ADDROW: rx_tbl_add_row(table, out, vec); break;
        default: abort();
        }
    }
    if (have_vec) rx_tbl_crit_vec_destroy(vec);
    RX_FRAME_LEAVE(F, 0x140);
}

/* ActivityResults_CreateSchema 004df240: "Index", "Name" ("Unnamed Player"), then for k < [0x00ae1934] (5): "Column%i" (string),
 * "Column%i_Record" (int 0), "Column%i_SortValue" (float [0x00977494]); names formatted by FUN_0078fd60 (_vsnprintf into 0x40) */
void rx_tbl_activity_results_schema(void) {
    const rx_addr t = 0x00ae18c0;
    RX_FRAME_ENTER(F, 0x80);
    const rx_addr ival = F + 0xc, fval = F + 0x10, str = F + 0x18, name = F + 0x34, arg = F + 0x78;
    rx_tbl_vector_reserve_ptr(t + 4, 6);
    SU32(ival, 0, 0xffffffffu);
    rx_tbl_add_int_column(t, 0x009361a8, ival, 1, 0, 0x00935d6c, 0);                 /* "Index" */
    rx_tbl_str_init(str); rx_tbl_str_assign_n(str, 0x0093de9c, 0xe);                  /* "Unnamed Player" */
    rx_tbl_add_string_column(t, 0x0093a3bc, str, 0, 0, 0x00935d6c);                    /* "Name" */
    rx_tbl_str_tidy(str);
    if (I32(0, 0x00ae1934) > 0) {
        SU32(ival, 0, 0); SU32(fval, 0, U32(0, 0x00977494));
        for (int32_t k = 0; k < I32(0, 0x00ae1934); k++) {
            static const uint32_t fmt[3] = {0x0093dfe8, 0x0093dff4, 0x0093e004};      /* "Column%i", "Column%i_Record", "Column%i_SortValue" */
            for (int j = 0; j < 3; j++) {
                /* FUN_0078fd60: _vsnprintf(name, 0x40, fmt, k), terminated; the formats are plain "%i" (msvcr90's printf needs the
                 * thread's locale data: the core formats the decimal itself, same characters) */
                char f[32], o[0x40]; uint32_t L = 0;
                for (; L < sizeof f - 1 && U8(fmt[j], L); L++) f[L] = (char)U8(fmt[j], L);
                f[L] = 0;
                int n = snprintf(o, sizeof o, f, (int)k);
                for (int b = 0; b < n && b < 0x3f; b++) SU8(name, (uint32_t)b, (uint8_t)o[b]);
                SU8(name, (uint32_t)(n < 0x3f ? n : 0x3f), 0);
                (void)arg;
                if (j == 0) {
                    rx_tbl_str_init(str); rx_tbl_str_assign_n(str, 0x00935d6c, 0);
                    rx_tbl_add_string_column(t, name, str, 0, 0, 0x00935d6c);
                    rx_tbl_str_tidy(str);
                } else if (j == 1) rx_tbl_add_int_column(t, name, ival, 0, 0, 0x00935d6c, 0);
                else rx_tbl_add_float_column(t, name, fval, 0, 0, 0x00935d6c);
            }
        }
    }
    RX_FRAME_LEAVE(F, 0x80);
}

void rx_tbl_create_schemas(void) {
    run_schema(S_004decd0_ae1b18);                                     /* "Activity List Real World" */
    run_schema(S_004decd0_adffe0);                                     /* "Activity List UI World" */
    rx_tbl_activity_results_schema();
    for (int i = 2; i < (int)(sizeof g_schemas / sizeof g_schemas[0]) - 2; i++) {
        run_schema(g_schemas[i]);
        if (g_schemas[i] == S_004e2590) run_schema(S_004e3c50);     /* the experience levels (50 rows) */
        if (g_schemas[i] == S_004e5eb0) run_schema(S_004e5f70);     /* the experience coefficients (one row of defaults) */
    }
}
