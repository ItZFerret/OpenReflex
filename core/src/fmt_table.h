/* fmt_table.h -- the game's tables (tbldata assets: MXTables_Default and the DLC table databases) and its string tables.
 *
 * tbldata is BXML:  <Asset typeCode="tbldata"><Tables><Table><Name/><Columns><Column name type default unique searchable/>..
 * </Columns><Rows><Row Col="value" .../>..</Rows></Table>..</Tables></Asset>. Column types: int, float, string; a Row has one
 * attribute per column (int attributes are typed ints, floats typed floats, strings plain). The game merges the default
 * tables with the rows of every DLC database's tbldata assets (fmt_tables_load_merged does the same).
 *
 * localiz is a string table (UI text, Database "MXUI" packages <Area>.<LANG>):
 *     u32 2, u32 key count, u32 key pool bytes, the keys (NUL-terminated ASCII), u32 capacity[key count], u32 total,
 *     then one NUL-terminated UTF-16LE string per key in key order. "EMB_" keys whose string is only '?' characters carry a
 *     second, embedded-format string right after it; "DYN_" keys hold '?' placeholders of their capacity.
 *     Strings are returned as UTF-8. */
#ifndef FMT_TABLE_H
#define FMT_TABLE_H

#include "fmt_db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmt_tables fmt_tables;
typedef struct fmt_table fmt_table;

/* A cell. kind is FMT_BX_INT / FMT_BX_FLOAT / FMT_BX_STRING (FMT_BX_EMPTY when the row has no such column). */
typedef struct fmt_cell {
    int         kind;
    int32_t     i;
    float       f;
    const char* s;
} fmt_cell;

fmt_tables* fmt_tables_parse(const uint8_t* data, size_t size, const char* origin);   /* origin names the source (kept, may be NULL) */
void        fmt_tables_free(fmt_tables* t);
/* Appends the tables of another tbldata document: rows of a table of the same name are added after the existing rows (a new
 * name adds a table). The document stays alive with the set. */
int         fmt_tables_append(fmt_tables* t, const uint8_t* data, size_t size, const char* origin);
/* MXTables_Default (Database MXTables_Default, package and asset of that name) + the tbldata assets of every database whose
 * file name contains "DLC", databases in ordinal name order, packages by name, assets in package order. */
fmt_tables* fmt_tables_load_merged(fmt_install* in);

int         fmt_tables_count(const fmt_tables* t);
const fmt_table* fmt_tables_at(const fmt_tables* t, int index);
const fmt_table* fmt_tables_find(const fmt_tables* t, const char* name);              /* exact name */

const char* fmt_table_name(const fmt_table* t);
int         fmt_table_col_count(const fmt_table* t);
const char* fmt_table_col_name(const fmt_table* t, int col);
const char* fmt_table_col_type(const fmt_table* t, int col);                           /* "int" "float" "string" */
const char* fmt_table_col_default(const fmt_table* t, int col);                        /* the default as text */
int         fmt_table_col_find(const fmt_table* t, const char* name);                  /* -1 if absent */
int         fmt_table_row_count(const fmt_table* t);
const char* fmt_table_row_origin(const fmt_table* t, int row);                         /* the origin given when its document was added */
int         fmt_table_cell(const fmt_table* t, int row, int col, fmt_cell* out);       /* 0, or -1 for a bad index */
/* by column name (the game's Int / Str helpers: def when the row, column or conversion is missing) */
int32_t     fmt_table_int(const fmt_table* t, int row, const char* col, int32_t def);
float       fmt_table_float(const fmt_table* t, int row, const char* col, float def);
const char* fmt_table_str(const fmt_table* t, int row, const char* col);               /* NULL if absent; numbers are not converted */
int         fmt_table_find_row(const fmt_table* t, const char* col, int32_t value, int from);  /* first row >= from with that int, or -1 */

/* ---- localiz ---- */
typedef struct fmt_strings fmt_strings;

fmt_strings* fmt_strings_parse(const uint8_t* data, size_t size);
void         fmt_strings_free(fmt_strings* s);
int          fmt_strings_count(const fmt_strings* s);
const char*  fmt_strings_key(const fmt_strings* s, int i);
const char*  fmt_strings_value(const fmt_strings* s, int i);                           /* UTF-8 */
const char*  fmt_strings_embedded(const fmt_strings* s, int i);                        /* the EMB_ extra string, or NULL */
uint32_t     fmt_strings_capacity(const fmt_strings* s, int i);
const char*  fmt_strings_get(const fmt_strings* s, const char* key);                   /* NULL if absent */

#ifdef __cplusplus
}
#endif
#endif
