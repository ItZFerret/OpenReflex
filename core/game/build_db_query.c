/* build_db_query.c -- Database::vf9 0084a840 (thiscall db, database name, package name; RET 8): the package is known (vf8, vtable +0x20, found it). */
#include "build_asset.h"
#include "build_rider_util.h"
#include "loop_platform.h"

uint32_t rx_database_vf_package_known(rx_addr db, rx_addr dbname, rx_addr pkg) {
    const uint32_t a[2] = {dbname, pkg};
    return rr_vcall(db, 0x20, 2, a) != 0;
}
RX_FUNCTION(0x0084a840, rx_database_vf_package_known)

/* Database::vf11 0084c090 (thiscall -> EAX): 1 when no job of the loader's table (1000 slots of 0x194 bytes from +0x104: the first word is the job) is pending; under the
 * table's lock (+0x62b24); the loader's wake event (+0xf8) is set in any case (SetEvent; GetLastError when it fails) */
uint32_t rx_database_vf_idle(rx_addr db) {
    rr_os_cs_enter(db + 0x62b24);
    uint32_t r = 1;
    for (uint32_t i = 0; i < 1000; i++) if (U32(db, 0x104 + 0x194 * i) != 0) { r = 0; break; }
    rr_os_cs_leave(db + 0x62b24);
    (void)loop_set_event(U32(db, 0xf8));   /* SetEvent (the oracle's emulated worker runs inside it; the product has none: 1) */
    return r;
}
RX_FUNCTION(0x0084c090, rx_database_vf_idle)
