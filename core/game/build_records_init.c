/* build_records_init.c -- the career statistics object g_MXRecords ([0x00a946c0], 0x84 bytes): 007635b0 sets its five per-controller slots (0x18 bytes each: +4 / +8 /
 * +0xc the working rows, -1; +0x14 the holeshot flag, 0; +0 the controller's PlayerRecords table and +0x10 the VehicleTypeId are not set here: the saved game's load
 * fills them) and +0x78 / +0x7c / +0x80 = 0. The original makes it in MXUIManager's constructor (`if (g_MXRecords == 0) { _aligned_malloc(0x84);
 * 007635b0 }`); the product has no UI manager and makes it with the UI world (build_ui_world.c). */
#include "build_activity_util.h"
#include "build_alloc.h"
#include "build_records_init.h"

/* 007635b0 (EAX = the object) */
void rx_records_construct(rx_addr o) {
    static const uint8_t m1[] = {0x04, 0x08, 0x0c, 0x1c, 0x20, 0x24, 0x34, 0x38, 0x3c, 0x4c, 0x50, 0x54, 0x64, 0x68, 0x6c};
    static const uint8_t z[] = {0x14, 0x2c, 0x44, 0x5c, 0x78, 0x7c, 0x80};
    for (unsigned i = 0; i < sizeof m1; i++) SU32(o, m1[i], 0xffffffffu);
    for (unsigned i = 0; i < sizeof z; i++) SU32(o, z[i], 0);
}
RX_FUNCTION(0x007635b0, rx_records_construct)

/* MXUIManager's constructor's part: the records object made once */
void rx_records_create(void) {
    if (U32(0, 0x00a946c0) != 0) return;
    const rx_addr o = rx_reflex_aligned_alloc(0x84);
    if (o) rx_records_construct(o);
    SU32(0, 0x00a946c0, o);
}
