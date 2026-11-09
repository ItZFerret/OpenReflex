/* build_globals_dlc.c -- unit 12b: the downloadable-content finder (core/notes/construction.md section 14).
 *
 * XenonOnlineContentPackage::vf5 (006d3f40) lists the install's "Database\*DLC*.dx9.database" (FindFirstFileA: the name up to its first '.')
 * into a std::map<string, int>, i.e. in strcmp order; FUN_0053c410 then walks that list: a name containing "Tables" has its table database and
 * package loaded (Database vf4 / vf15: the asset system's request, served at once in the product), a name containing "MXUI" its UI package
 * (the UI is not core: the database / package loads are still issued, so the asset serials stay exact), "Leaderboards" a BXML table file.
 * The game's own DLC lists (the reference install: MXTables_DLC003 .. 005 and MXUI_DLC001 .. 005) come out of the install's folder, not from a
 * constant. */
#include "build_globals_int.h"
#include "build_tables.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char (*names)[64]; int n, max; } dlc_acc;
static int has_ci(const char* s, const char* sub) {
    size_t m = strlen(sub);
    for (; *s; s++) {
        size_t k = 0;
        while (k < m && s[k] && ((s[k] | 0x20) == (sub[k] | 0x20))) k++;
        if (k == m) return 1;
    }
    return 0;
}
static int dlc_cb(const char* name, uint64_t size, void* user) {
    (void)size;
    dlc_acc* a = (dlc_acc*)user;
    size_t len = strlen(name), ext = strlen(".dx9.database");
    if (len <= ext || fmt_stricmp(name + len - ext, ".dx9.database") != 0 || !has_ci(name, "DLC")) return 0;
    if (a->n >= a->max) return 0;
    char* dot = strchr(name, '.');
    size_t k = (size_t)(dot - name);
    if (k > 63) k = 63;
    memcpy(a->names[a->n], name, k); a->names[a->n][k] = 0;
    a->n++;
    return 0;
}
static int cmp(const void* x, const void* y) { return strcmp((const char*)x, (const char*)y); }

int rx_dlc_find(const fmt_vfs* v, char (*names)[64], int max) {
    dlc_acc a = {names, 0, max};
    fmt_vfs_list(v, "Database", dlc_cb, &a);
    qsort(names, (size_t)a.n, 64, cmp);                     /* the std::map's order */
    return a.n;
}

void rx_dlc_load(fmt_install* in, rx_addr db) {
    char names[64][64], chosen[64][64];
    int n = rx_dlc_find(fmt_install_vfs(in), names, 64), m = 0;
    for (int i = 0; i < n; i++)
        if (strstr(names[i], "Tables") || strstr(names[i], "MXUI")) { memcpy(chosen[m], names[i], 64); m++; }
    const char* list[64];
    for (int i = 0; i < m; i++) list[i] = chosen[i];
    rx_tbl_load_dlc_tables(in, db, list, m);
}
