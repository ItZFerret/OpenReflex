/* build_activity_controller.c -- construction unit 8: the controller's key maps (build_activity.h).
 * Controller_BuildKeyMaps 00547f30 (cdecl controller, preset, handedness, motion; plain RET) reads the remapping row of the static
 * database 0x00ae0910 (the "ControllerRemapping" table: the same table object as controller 0's) and writes the controller's key
 * maps [0x009c52c4 + 4 * controller] + 0x200 .. +0x21d0 (KeyMap_SetSlot 008112e0: device 2 = the keyboard key, device 1 = the pad
 * input) plus the preset / handedness / motion at +0x5128 / +0x512c / +0x5130. PlayerSlot_FromPlayerList 0054a7b0 calls it for a
 * local controller after attaching a save (remap.cpp apply). The original's stack frame is not needed here: the cells are read and
 * the slots written through the memory accessors, exactly as the original's writes.
 *
 * The calls, in the original's order (row 0 = the keyboard key, row 1 = the pad input; the keyboard left/right and up/down axes carry
 * the opposite key in ESI, and the keyboard Clutch carries EDI = 2): */
#include "build_activity_util.h"
#include "build_globals_int.h"     /* rx_keymap_set_slot (008112e0, the KeyMap owner) */

#define CTRL_REMAP_DB  0x00ae0910u   /* the static database */
#define CTRL_REMAP_TBL 0x009411ccu   /* "ControllerRemapping" */
#define CTRL_BLOCKS    0x009c52c4u   /* the four controllers' KeyMap blocks */

/* K(off, id, esi, edi, device): KeyMap_SetSlot(the controller, [CTRL_BLOCKS + 4 * controller] + off, ...); the block is re-read
 * for every call as the original does */
#define K(controller, off, id, esi, edi, device) \
    rx_keymap_set_slot(U32(CTRL_BLOCKS, 4u * (controller)) + (off), (int32_t)(controller), (id), (esi), (edi), (device))

uint32_t rx_controller_build_key_maps(uint32_t controller, uint32_t preset, uint32_t handedness, uint32_t motion) {
    const rx_addr t = rx_tbl_find(CTRL_REMAP_DB, CTRL_REMAP_TBL);
    /* row 0: every column, in the original's order */
    const uint32_t throttle0    = act_cell(t, 0x0094086cu, 0);   /* "Throttle" */
    const uint32_t brake0       = act_cell(t, 0x0094084cu, 0);   /* "Brake" */
    const uint32_t steerleft0   = act_cell(t, 0x009411e0u, 0);   /* "SteerLeft" */
    const uint32_t steerright0  = act_cell(t, 0x009411ecu, 0);   /* "SteerRight" */
    const uint32_t trick0       = act_cell(t, 0x009411f8u, 0);   /* "TrickModifier" */
    const uint32_t clutch0      = act_cell(t, 0x0093ed98u, 0);   /* "Clutch" */
    const uint32_t reflexleft0  = act_cell(t, 0x00941208u, 0);   /* "ReflexLeft" */
    const uint32_t reflexright0 = act_cell(t, 0x00941214u, 0);   /* "ReflexRight" */
    const uint32_t reflexup0    = act_cell(t, 0x00941220u, 0);   /* "ReflexUp" */
    const uint32_t reflexdown0  = act_cell(t, 0x0094122cu, 0);   /* "ReflexDown" */
    const uint32_t camera0      = act_cell(t, 0x00941238u, 0);   /* "Camera" */
    const uint32_t setreset0    = act_cell(t, 0x00941240u, 0);   /* "SetResetPoint" */
    const uint32_t reset0       = act_cell(t, 0x0094082cu, 0);   /* "Reset" */
    const uint32_t usereset0    = act_cell(t, 0x00941250u, 0);   /* "UseResetPoint" */
    const uint32_t reverse0     = act_cell(t, 0x009408a0u, 0);   /* "Reverse" */
    /* row 1: the pad inputs (the SteerRight cell is read but no slot uses it, as in the original) */
    const uint32_t throttle1    = act_cell(t, 0x0094086cu, 1);
    SU32(rx_sp(), 4, throttle1);   /* the original reuses its first argument slot for this cell (a scratch local at [esp + 0x7c]
                                    * while the next column's name is pushed); the write lands above the final ESP, so it is
                                    * part of the call's observable memory */
    const uint32_t brake1       = act_cell(t, 0x0094084cu, 1);
    const uint32_t steerleft1   = act_cell(t, 0x009411e0u, 1);
    (void)act_cell(t, 0x009411ecu, 1);                           /* "SteerRight" (row 1): read, unused */
    const uint32_t trick1       = act_cell(t, 0x009411f8u, 1);
    const uint32_t clutch1      = act_cell(t, 0x0093ed98u, 1);
    const uint32_t reflexleft1  = act_cell(t, 0x00941208u, 1);
    const uint32_t reflexup1    = act_cell(t, 0x00941220u, 1);
    const uint32_t camera1      = act_cell(t, 0x00941238u, 1);
    const uint32_t setreset1    = act_cell(t, 0x00941240u, 1);
    const uint32_t reset1       = act_cell(t, 0x0094082cu, 1);
    const uint32_t usereset1    = act_cell(t, 0x00941250u, 1);
    const uint32_t reverse1     = act_cell(t, 0x009408a0u, 1);
    K(controller, 0x0200, throttle0,    0,           0, 2);
    K(controller, 0x02a4, throttle1,    0,           0, 1);
    K(controller, 0x0000, brake0,       0,           0, 2);
    K(controller, 0x00a4, brake1,       0,           0, 1);
    K(controller, 0x187c, steerright0,  steerleft0,  0, 2);
    K(controller, 0x1920, steerleft1,   0,           0, 1);
    K(controller, 0x0aa4, trick0,       0,           0, 2);
    K(controller, 0x0b48, trick1,       0,           0, 1);
    K(controller, 0x0400, clutch0,      0,           2, 2);
    K(controller, 0x04a4, clutch1,      0,           0, 1);
    K(controller, 0x1e7c, reflexright0, reflexleft0, 0, 2);
    K(controller, 0x1f20, reflexleft1,  0,           0, 1);
    K(controller, 0x212c, reflexup0,    reflexdown0, 0, 2);
    K(controller, 0x21d0, reflexup1,    0,           0, 1);
    K(controller, 0x0d48, camera0,      0,           0, 2);
    K(controller, 0x0e90, camera1,      0,           0, 1);
    K(controller, 0x1334, setreset0,    0,           0, 2);
    K(controller, 0x13d8, setreset1,    0,           0, 1);
    K(controller, 0x08a4, reset0,       0,           0, 2);
    K(controller, 0x0948, reset1,       0,           0, 1);
    K(controller, 0x1534, usereset0,    0,           0, 2);
    K(controller, 0x15d8, usereset1,    0,           0, 1);
    K(controller, 0x0600, reverse0,     0,           0, 2);
    K(controller, 0x06a4, reverse1,     0,           0, 1);
    const rx_addr blk = U32(CTRL_BLOCKS, 4u * controller);
    SU32(blk, 0x5128, preset);
    SU32(blk, 0x512c, handedness);
    SU32(blk, 0x5130, motion);
    return blk;   /* EAX at the original's ret (ECX = motion, EDX = handedness there too) */
}
RX_FUNCTION(0x00547f30, rx_controller_build_key_maps)
