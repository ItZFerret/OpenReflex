/* build_tables_init.c -- unit 12a: the static initializers of the table objects in .data (the exe's dynamic initializers 00903b20 ..
 * 00904160, run by the CRT's _initterm before WinMain, in this order). Their atexit destructors (00911ff0 ..) are not registered:
 * the core never runs the exit path. */
#include "build_tables.h"
#include "build_alloc.h"
#include "crt.h"

/* the static tables: {initializer, object, name literal, vtable} (each 004ec560 then the subclass vtable) */
typedef struct { uint32_t fn, obj, name, vt; } static_table;
static const static_table g_tables[] = {
    {0x00903c10, 0x00ae1b18, 0x0093e88c, 0x0093ebd4},   /* "Activity List Real World"            ActivityTable */
    {0x00903c40, 0x00ae05c8, 0x0093e8a8, 0x0093ebc8},   /* "Vehicle List"                        VehicleTable */
    {0x00903c70, 0x00ae19b0, 0x0093e8b8, 0x0093ebbc},   /* "Rider List"                          RiderTable */
    {0x00903ca0, 0x00ae0e08, 0x0093e8c4, 0x0093eb88},   /* "Player List"                         PlayerTable */
    {0x00903cd0, 0x00ae18c0, 0x0093e8d0, 0x0093ebe0},   /* "Activity Results"                    ActivityResultsTable (+0x74 = 5) */
    {0x00903d10, 0x00adffe0, 0x0093e8e4, 0x0093ebd4},   /* "Activity List UI World"              ActivityTable */
    {0x00903d40, 0x00ae07a8, 0x0093e400, 0x0093ec34},   /* "Registry Rider List"                 NetworkRider */
    {0x00903d70, 0x00ae0820, 0x0093e8fc, 0x0093ec40},   /* "Registry Rider Gear List"            NetworkRiderGear */
    {0x00903da0, 0x00ae0640, 0x0093e918, 0x0093ec4c},   /* "Registry Vehicle List"               NetworkVehicle */
    {0x00903dd0, 0x00ae06b8, 0x0093e930, 0x0093ec58},   /* "Registry Vehicle Parts List"         NetworkVehicleParts */
    {0x00903e30, 0x00ae0d78, 0x0093e94c, 0x0093ebec},   /* "Network Attribute List"              NetworkTable (+0x74: a msvcp90 std::string) */
    {0x00903e70, 0x00ae0e98, 0x0093e964, 0x0093ebf8},   /* "Network Community Info Table"        NetworkCommunityInfo */
    {0x00903ea0, 0x00ae0460, 0x0093e984, 0x0093ec04},   /* "Network Sessions List"               NetworkSessionsList */
    {0x00903ed0, 0x00ae04d8, 0x0093e414, 0x0093ec10},   /* "Network Stats"                       NetworkLBStats */
    {0x00903f00, 0x00ae1ba8, 0x0093e99c, 0x0093ec1c},   /* "Network Player Experience Table"     NetworkPlayerExpTable */
    {0x00903f30, 0x00ae0f88, 0x0093e9bc, 0x0093ec28},   /* "Network Friends"                     NetworkFriends */
    {0x00903f60, 0x00ae0f10, 0x0093e3ec, 0x0093ec34},   /* "Network Rider List"                  NetworkRider */
    {0x00903f90, 0x00ae1a28, 0x0093e9cc, 0x0093ec40},   /* "Network Rider Gear List"             NetworkRiderGear */
    {0x00903fc0, 0x00ae0898, 0x0093e9e4, 0x0093ec4c},   /* "Network Vehicle List"                NetworkVehicle */
    {0x00903ff0, 0x00ae0550, 0x0093e9fc, 0x0093ec58},   /* "Network Vehicle Parts List"          NetworkVehicleParts */
    {0x00904020, 0x00ae0b08, 0x0093ea18, 0x0093ec64},   /* "Network Motocard Details"            NetworkMotocardDetails */
    {0x00904050, 0x00ae1aa0, 0x0093ea34, 0x0093ec70},   /* "Network Series Table"                NetworkSeriesTable */
    {0x00904080, 0x00ae1000, 0x0093ea4c, 0x0093ec7c},   /* "Network Experience Level Table"      NetworkExperienceLevelTable */
    {0x009040b0, 0x00ae1078, 0x0093ea6c, 0x0093ec88},   /* "Network Experience Bonus Table"      NetworkExperienceBonusTable */
    {0x009040e0, 0x00ae1938, 0x0093ea8c, 0x0093ec94},   /* "Network Experience Coefficient Table" NetworkExperienceCoefficientTable */
    {0x00904110, 0x00ae0730, 0x0093eab4, 0x0093eca0},   /* "Network Game Launch Timer Table"     NetworkGameLaunchTimerTable */
};

static void static_table_init(const static_table* s) {
    rx_tbl_table_construct_named(s->obj, s->name);                    /* 004ec560 */
    SU32(s->obj, 0, s->vt);
    if (s->fn == 0x00903cd0) SU32(0, 0x00ae1934, 5);
    if (s->fn == 0x00903e30) crt_str_ctor(0x00ae0dec);                /* msvcp90 basic_string() (import 0x009161a4) */
}

void rx_tbl_static_init(void) {
    if (U32(0, RX_TBL_DEFAULT_DB) == 0x0093eb64u) return;            /* done */
    rx_tbl_collection_construct(RX_TBL_DEFAULT_DB);                   /* 00903b20 */
    for (int k = 0; k < 4; k++) {                                     /* 00903b40: the MXPlayerDatabases */
        rx_tbl_collection_construct(RX_TBL_PLAYER_DB(k));
        SU32(RX_TBL_PLAYER_DB(k), 0, 0x0093ecac);
    }
    rx_tbl_collection_construct(0x00ae0268); SU32(0, 0x00ae0268, 0x0093ecbc);   /* 00903b80 MX07Leaderboards */
    rx_tbl_collection_construct(0x00adfde8); SU32(0, 0x00adfde8, 0x0093ecc4);   /* 00903bb0 PlaylistDatabase */
    rx_tbl_collection_construct(0x00ae0b80); SU32(0, 0x00ae0b80, 0x0093ecb4);   /* 00903be0 MXMemoryCollection */
    for (int i = 0; i < 10; i++) static_table_init(&g_tables[i]);     /* 00903c10 .. 00903dd0 */
    rx_tbl_collection_construct(0x00ae0070); SU32(0, 0x00ae0070, 0x0093ecb4);   /* 00903e00 MXMemoryCollection */
    for (int i = 10; i < (int)(sizeof g_tables / sizeof g_tables[0]); i++) static_table_init(&g_tables[i]);   /* 00903e30 .. 00904110 */
    /* 00904140: InitializeCriticalSection(0x00ae1c44) as the reference boot leaves it */
    SU32(0, 0x00ae1c44, 0); SU32(0, 0x00ae1c48, 0xffffffffu); SU32(0, 0x00ae1c4c, 0); SU32(0, 0x00ae1c50, 0); SU32(0, 0x00ae1c54, 0); SU32(0, 0x00ae1c58, 0);
    /* 00904160: a std::set<RowRef>-shaped tree at 0x00ae1c1c (proxy, head 0x00ae1c34, size, +0x20 / +0x24 0) */
    rx_tbl_alloc_init();
    rx_addr px = rx_crt_aligned_malloc(4, 0x10);                     /* 004f4a30 */
    if (px) SU32(px, 0, 0x00ae1c1c);
    SU32(0, 0x00ae1c1c, px);
    rx_addr h = rx_crt_aligned_malloc(0x1c, 0x10);                   /* 004f4ea0 */
    if (h) { SU32(h, 0, 0); SU32(h, 4, 0); SU32(h, 8, 0); }
    SU8(h, 0x18, 1); SU8(h, 0x19, 0);
    SU32(0, 0x00ae1c34, h);
    SU8(h, 0x19, 1); SU32(h, 4, h); SU32(h, 0, h); SU32(h, 8, h);
    SU32(0, 0x00ae1c38, 0); SU32(0, 0x00ae1c3c, 0); SU32(0, 0x00ae1c40, 0);
}
