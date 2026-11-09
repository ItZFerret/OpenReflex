/* build_uiprofile.c -- construction unit 12c: the UI settings block [0x00d92efc] and the profile bootstrap that fills it.
 *
 * The guest profile's settings live in one object (0x170 bytes, MXUISettings vtable 0x94db70) created by MXUIManager::Construct
 * 00598640 while MXGame::Startup runs: four per-device rows at +4 + 0x48*i, a fifth "no device" row at +0x124, the device index /
 * flags at +0x164 .. +0x16c. Each row's defaults are UISettings_Defaults 005b8030: DrivingCamera 0, Brightness 50, Gamma 1,
 * OnlinePlayerName 0, MeasurementSystem 0, ControllerPreset [0xa93f5c], Effects / Music / Engine / Crowd 100, ControllerMotion 0,
 * Handedness 1, CareerDifficulty 80, HUDVisibility 0, MouseSensitivity 50, Vibration 0. The shipping guest route
 * (ProfileReminderPopup.lua) then calls UI_SetPrimaryPlayerDeviceIndex 00743030 with the primary player's controller, which
 * UISettings_Reset 005b80b0 uses to re-default that device's row from the player database's PlayerSettings rows (none in the
 * shipped data: 005b7d70 reads the table in player database `device`, so the defaults above stand), applies the settings
 * (UISettings_Apply 005b9af0: renderer gamma / brightness, the audio volumes, the players' measurement flag, the driving camera,
 * the pads' vibration enable, MouseSensitivity) and - when the ControllerRemapping table is empty, as it is in MXTables_Default -
 * seeds it with the two built-in rows (keyboard: W/S/A/D, Space, LShift, arrows, C/F/R/V/X, J/L/I/K; pad: 0x14/0x13, sticks,
 * buttons, and the keyboard's camera tweaks) and registers the table in player database 0.
 *
 * That block is what gameplay reads: PlayerSlot_FromPlayerList 0054a7b0 takes preset / handedness / motion from the controller's
 * row (rec +0x18 / +0x30 / +0x2c) and hands them to Controller_BuildKeyMaps 00547f30; with handedness 0 (left) the input sampler
 * routes the keyboard steer keys to the rider reflex and ignores the reflex keys (simulation-input.md). The original default is
 * handedness 1 (right), motion 0: keyboard A / D steer, the arrows are the rider reflex.
 *
 * The UI manager itself (pages, widgets, the Lua VM) is not core; this file ports the settings object it creates and the profile
 * route that fills it. The product has no front end, so the route runs once the tables are loaded (rx_globals_run's tail) - the
 * same point of the original's boot (after MXRegistry_Load, before any event), with the single-player session's controller 0 as
 * the primary player device (the event spec's local player; the reference boot's [0xd92efc]+0x16c == 0). */
#include "build_uiprofile.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "loop_input.h"
#include "loop_player.h"
#include "phys_coll_shape.h"
#include "race.h"
#include "rider_cap.h"
#include <string.h>

#define UI_SETTINGS_GLOBAL  0x00d92efcu   /* g_UISettings */
#define UI_MANAGER_GLOBAL   0x00a93fccu   /* g_MXUIManager (0 in the product: no UI) */
#define SAVE_OBJECT_GLOBAL  0x00d92f10u   /* the save-game / records object */
#define CALLBACK_GLOBAL     0x008073c0u   /* the audio mixer's volume-changed callback (FUN_008073c0; ESI = the mixer block) */

/* row field offsets (a row block is 0x48 bytes) */
#define ROW_DRIVING_CAMERA   0x00
#define ROW_BRIGHTNESS       0x04
#define ROW_GAMMA            0x08
#define ROW_ONLINE_NAME      0x0c
#define ROW_MEASUREMENT      0x10
#define ROW_PRESET           0x14
#define ROW_EFFECTS          0x18
#define ROW_MUSIC            0x1c
#define ROW_ENGINE           0x20
#define ROW_CROWD            0x24
#define ROW_MOTION           0x28
#define ROW_HANDEDNESS       0x2c
#define ROW_CAREER_DIFF      0x30
#define ROW_HUD              0x34
#define ROW_MOUSE_SENS       0x38
#define ROW_VIBRATION        0x3c
#define ROW_DEVICE           0x40
#define ROW_LOADED           0x44

/* the "no device" row (+0x124) is the same layout */
#define NODEV_ROW            0x124u

/* 005b8030 UISettings_Defaults (EAX = a settings row block, returned unchanged): the 16 defaults, in the original's store order */
rx_addr rx_uisettings_defaults(rx_addr s) {
    for (uint32_t o = 0; o < 0x40; o += 8) { rx_wr64(s + o, 0); }         /* pxor / movq xmm0 over +0 .. +0x3f */
    SU32(s, ROW_EFFECTS, 100);
    SU32(s, ROW_MUSIC, 100);
    SU32(s, ROW_ENGINE, 100);
    SU32(s, ROW_CROWD, 100);
    const uint32_t preset = U32(0, 0x00a93f5c);                           /* the ControllerPreset default (0) */
    SU32(s, ROW_DRIVING_CAMERA, 0);
    SU32(s, ROW_BRIGHTNESS, 50);
    SU32(s, ROW_GAMMA, 1);
    SU32(s, ROW_ONLINE_NAME, 0);
    SU32(s, ROW_HUD, 0);
    SU32(s, ROW_MEASUREMENT, 0);
    SU32(s, ROW_PRESET, preset);
    SU32(s, ROW_MOTION, 0);
    SU32(s, ROW_HANDEDNESS, 1);                                           /* the original default: right */
    SU32(s, ROW_CAREER_DIFF, 80);
    SU32(s, ROW_VIBRATION, 0);
    SU32(s, ROW_MOUSE_SENS, 50);
    return s;
}
RX_FUNCTION(0x005b8030, rx_uisettings_defaults)

/* 005b7d50 Fn_005b7d50 (ECX = a row block): the row's device index -1 and loaded flag 0 */
rx_addr rx_uisettings_row_construct(rx_addr row) {
    SU32(row, ROW_DEVICE, 0xffffffffu);
    SU32(row, ROW_LOADED, 0);
    return row;
}
RX_FUNCTION(0x005b7d50, rx_uisettings_row_construct)

/* 005b7cd0 FUN_005b7cd0 (EDI = the 0x170-byte block; EAX out = it): vtable 0x94db70; vector_constructor_iterator(block + 4, 0x48,
 * 4, Fn_005b7d50); +0x164 = -1, +0x168 = 0, +0x16c = -1; then each of the four rows (+0x40 = its device index) and the +0x124
 * "no device" row get UISettings_Defaults. */
rx_addr rx_uisettings_construct(rx_addr s) {
    SU32(s, 0, 0x0094db70);
    for (uint32_t i = 0; i < 4; i++) rx_uisettings_row_construct(s + 4 + 0x48u * i);
    SU32(s, 0x164, 0xffffffffu);
    SU32(s, 0x168, 0);
    SU32(s, 0x16c, 0xffffffffu);
    for (uint32_t i = 0; i < 4; i++) {
        const rx_addr row = s + 4 + 0x48u * i;
        SU32(row, ROW_DEVICE, i);
        rx_uisettings_defaults(row);
    }
    rx_uisettings_defaults(s + NODEV_ROW);                                /* EAX = s + 0x124 at the original's last call */
    return s;
}
RX_FUNCTION(0x005b7cd0, rx_uisettings_construct)

/* 005b7d70 FUN_005b7d70 (stack row; RET 4): UISettings_Defaults(row); for a device index in 0..4 the player database's
 * PlayerSettings rows override field `Setting` (a field index < 0x10) with `Value`; +0x44 = 1. */
void rx_uisettings_row_defaults(rx_addr row) {
    rx_uisettings_defaults(row);
    const int32_t dev = I32(row, ROW_DEVICE);
    if (dev >= 0 && dev <= 4) {
        const rx_addr ps = rx_tbl_find(0x00ae10f0u + 0x1f4u * (uint32_t)dev, 0x0094da54u);   /* "PlayerSettings" */
        if (ps) {
            uint32_t n = 0;
            if (((U32(ps, 0x14) - U32(ps, 0x10)) >> 2) != 0) n = rx_tbl_column_rows(U32(U32(ps, 0x10), 0));   /* 004eb830 */
            for (uint32_t r = 0; r < n; r++) {
                const uint32_t setting = act_cell(ps, 0x0094da64u, r);       /* "Setting" */
                if (setting < 0x10) SU32(row, 4 * setting, act_cell(ps, 0x0093e2b8u, r));   /* "Value" */
            }
        }
    }
    SU32(row, ROW_LOADED, 1);
}
RX_FUNCTION(0x005b7d70, rx_uisettings_row_defaults)

/* the value of a row field for the active device (UISettings_Apply's reads: [s + 0x16c] == -1 uses the +0x124 row) */
static uint32_t active_field(rx_addr s, uint32_t no_device_off, uint32_t row_off) {
    const int32_t cur = I32(s, 0x16c);
    return cur == -1 ? U32(s, no_device_off) : U32(s, (uint32_t)cur * 0x48 + row_off);
}

/* the two renderer gamma-ramp calls (Renderer_BuildGammaRamp 007aff40: stack renderer, EAX = the ramp midpoint 0x8f / 0x96) are
 * the render unit's: the oracle runs the original so a Verify of the caller sees its writes; the product has no renderer ramp (the
 * brightness word below is still written). */
static void build_gamma_ramp(rx_addr renderer, uint32_t midpoint) {
#ifdef RX_ORACLE
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[0] = midpoint;                                  /* EAX */
    const uint32_t a[1] = {renderer};
    rx_call_original_ex(0x007aff40, &in, &out, 1, a);
#else
    (void)renderer; (void)midpoint;
#endif
}

/* the audio mixer's volume-changed callback: FUN_008073c0 (ESI = the mixer block; no stack arguments). Audio is not core: the
 * oracle runs the original (its own critical section / category callbacks), the product skips the callback - the volume words it
 * would read are written either way. */
static void audio_volume_changed(rx_addr mixer) {
#ifdef RX_ORACLE
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[6] = mixer;                                     /* ESI */
    rx_call_original_ex(CALLBACK_GLOBAL, &in, &out, 0, NULL);
#else
    (void)mixer;
#endif
}

/* one volume word + its companion "changed" word, then the callback */
static void volume_store(rx_addr volume, rx_addr changed, rx_addr mixer, float v) {
    SF32(0, volume, v);
    SF32(0, changed, 0.0f);
    audio_volume_changed(mixer);
}

/* 005b9af0 UISettings_Apply (stack settings, device; RET 8): the active device's row into the engine's renderer / audio globals,
 * every local player's measurement flag (00552b90 / 0054e110), the device's player's driving camera (0054aef0), the pad's
 * vibration enable (008151b0 through the engine's input manager) and MouseSensitivity (0x009cb7b0 = value * 0.05, clamped
 * [0.1, 5]). The row is chosen by [settings+0x16c] (the active device), not the argument; the argument selects the player and
 * the pad. Writes in the original's order. */
void rx_uisettings_apply(rx_addr s, int32_t device) {
    const rx_addr engine = U32(0, 0x00a95960);                       /* XenonCore */
    const rx_addr renderer = U32(engine, 0x14);                      /* the renderer object (+0x680 / +0x684 gamma) */

    const uint32_t gamma = active_field(s, NODEV_ROW + ROW_GAMMA, ROW_GAMMA + 4);
    if (gamma == 0) build_gamma_ramp(renderer, 0x8f);               /* the CRT ramp midpoint */
    else if (gamma == 1) build_gamma_ramp(renderer, 0x96);           /* the LCD ramp midpoint */
    const uint32_t brightness = active_field(s, NODEV_ROW + ROW_BRIGHTNESS, ROW_BRIGHTNESS + 4);
    SF32(renderer, 0x684, (float)(int32_t)brightness * 0.01f /* 0x9322ac */);
    build_gamma_ramp(renderer, U32(renderer, 0x680));

    /* the volume categories: the original re-clamps the Effects value (row +0x1c) into every music / engine / crowd word, then
     * reads Music (+0x20) and Engine (+0x24) for the last two */
    float v = (float)(int32_t)active_field(s, NODEV_ROW + ROW_EFFECTS, ROW_EFFECTS + 4) * 0.01f;
    if (v < 1.0f) { if (v <= 0.0f) v = 0.0f; } else v = 1.0f;
    volume_store(0x00d679f8, 0x00d67a08, 0x00d679f4, v);
    volume_store(0x00d67a4c, 0x00d67a5c, 0x00d67a48, v);
    volume_store(0x00d67c44, 0x00d67c54, 0x00d67c40, v);
    volume_store(0x00d67c98, 0x00d67ca8, 0x00d67c94, v);
    volume_store(0x00d67b9c, 0x00d67bac, 0x00d67b98, v);
    volume_store(0x00d67bf0, 0x00d67c00, 0x00d67bec, v);
    volume_store(0x00d67b48, 0x00d67b58, 0x00d67b44, v);
    float m = (float)(int32_t)active_field(s, NODEV_ROW + ROW_MUSIC, ROW_MUSIC + 4) * 0.01f;
    if (m < 1.0f) { if (m <= 0.0f) m = 0.0f; } else m = 1.0f;
    volume_store(0x00d67aa0, 0x00d67ab0, 0x00d67a9c, m);
    float e = (float)(int32_t)active_field(s, NODEV_ROW + ROW_ENGINE, ROW_ENGINE + 4) * 0.01f;
    if (e < 1.0f) { if (e <= 0.0f) e = 0.0f; } else e = 1.0f;
    volume_store(0x00d67af4, 0x00d67b04, 0x00d67af0, e);

    /* the per-player flag: the original reads the row's +0x0c word here (the OnlinePlayerName field; the MeasurementSystem
     * default is +0x10 - both 0 in the shipped data): 0 = the player's online pause flag (0054e110), else +0x9ac = 0, for every
     * player slot */
    const uint32_t measurement = active_field(s, NODEV_ROW + ROW_ONLINE_NAME, ROW_ONLINE_NAME + 4);
    const rx_addr game = U32(0, 0x00a946b8);
    const uint32_t players = U8(game, 0x1541c);
    for (uint32_t i = 0; i < players; i++) {
        const rx_addr p = rx_player_active(game + 0x7460, i, 3);     /* 00552b90 */
        if (measurement == 0) rx_player_net_pause_flag(p);           /* 0054e110 */
        else SU32(p, 0x9ac, 0);
    }
    {   /* FUN_00552c10 (ESI = the players, EDI = the controller): the device's player; then its driving camera (0054aef0,
         * the ported camera selector, shared by native and oracle) */
        const rx_addr list = game + 0x7460;
        const uint32_t slots = U8(list, 0xdfbc);
        rx_addr p = 0;
        for (uint32_t i = 0; i < slots; i++) {
            const uint32_t rec = rx_seat_order(list, i);
            if (I32(rx_seat(list, rec), 0x1f0) == device) { p = rx_seat_active(list, i); break; }
        }
        if (p) {
            ACT_CALL(0, (void)rx_player_apply_driving_camera(p));
        }
    }
    {   /* the pad's vibration enable: input manager vf2 of the engine context, then vf53(1, device, setting != 0) */
        const rx_addr im = phys_xenon_core_vf2(engine);
        const uint32_t vib = active_field(s, NODEV_ROW + ROW_VIBRATION, ROW_VIBRATION + 4);
        rx_input_set_vibration_enabled(im, 1, (uint32_t)device, vib != 0);
    }
    {   /* MouseSensitivity * 0.05, clamped [0.1, 5] */
        const uint32_t ms = active_field(s, NODEV_ROW + ROW_MOUSE_SENS, ROW_MOUSE_SENS + 4);
        float v2 = (float)(int32_t)ms * 0.05f /* 0x9d3bfc */;
        if (v2 < 5.0f /* 0x935890 */) { if (v2 <= 0.1f /* 0x931d94 */) v2 = 0.1f; } else v2 = 5.0f;
        SF32(0, 0x009cb7b0, v2);
    }
}
RX_FUNCTION(0x005b9af0, rx_uisettings_apply)

/* ======================================================================================================================== */
/* the ControllerRemapping defaults UISettings_Reset installs (005b80b0's inline copy of 005b9340's rows)                    */

typedef struct { uint32_t name; uint32_t id; } remap_field;

static const remap_field k_remap_keyboard[19] = {
    {0x0094086cu, 0x39}, {0x0094084cu, 0x35}, {0x009411e0u, 0x23}, {0x009411ecu, 0x26}, {0x009411f8u, 0x65},
    {0x0093ed98u, 0x62}, {0x00941208u, 0x86}, {0x00941214u, 0x87}, {0x00941220u, 0x84}, {0x0094122cu, 0x85},
    {0x00941238u, 0x25}, {0x00941240u, 0x28}, {0x0094082cu, 0x34}, {0x00941250u, 0x38}, {0x009408a0u, 0x3a},
    {0x0094da6cu, 0x2c}, {0x0094da7cu, 0x2e}, {0x0094da90u, 0x2b}, {0x0094daa0u, 0x2d},
};
static const remap_field k_remap_pad[19] = {
    {0x0094086cu, 0x14}, {0x0094084cu, 0x13}, {0x009411e0u, 0x1}, {0x009411ecu, 0x1}, {0x009411f8u, 0x0d},
    {0x0093ed98u, 0x0e}, {0x00941208u, 0x3}, {0x00941214u, 0x3}, {0x00941220u, 0x4}, {0x0094122cu, 0x4},
    {0x00941238u, 0x10}, {0x00941240u, 0x0c}, {0x0094082cu, 0x0b}, {0x00941250u, 0x0a}, {0x009408a0u, 0x9},
    {0x0094da6cu, 0x2c}, {0x0094da7cu, 0x2e}, {0x0094da90u, 0x2b}, {0x0094daa0u, 0x2d},
};

/* one row: the 19 (name, id) criteria pushed and the row added (the criterion is destroyed after each push, as the original
 * frees its path vector between entries) */
static void remap_row(rx_addr table, rx_addr vec, rx_addr crit, rx_addr val, rx_addr out,
                      const remap_field* fields) {
    rx_tbl_crit_vec_init(vec);
    for (uint32_t i = 0; i < 19; i++) {
        SU32(val, 0, fields[i].id);
        rx_tbl_crit_init_int(crit, fields[i].name, 0, fields[i].id);   /* 004f6d80 */
        rx_tbl_crit_push(vec, crit);                                   /* 004175e0 */
        rx_tbl_crit_destroy(crit);
    }
    ACT_CALL(2, rx_tbl_add_row(table, out, vec));                      /* 004ec950 */
    rx_tbl_crit_vec_destroy(vec);                                      /* 00417590 + the block / proxy frees */
}

/* 005b80b0 UISettings_Reset (ECX = settings, stack device; RET 4): device -1 clears the four rows' loaded flags and the active
 * device; otherwise the device row (or the one +0x16c names) gets UISettings_Defaults / the PlayerSettings overrides,
 * UISettings_Apply runs, and the ControllerRemapping table gets its two built-in rows when it has no rows (the shipped
 * MXTables_Default has none), registered in player database 0's table list / name map. */
void rx_uisettings_reset(rx_addr s, int32_t device) {
    ACT_FRAME_EBP(L, 8, 0xf4, 3);
    const rx_addr vec0 = L + 0xc0, vec1 = L + 0xe4;      /* the two criteria vectors */
    const rx_addr crit = L + 0x10, val = L + 0x0c;       /* the criterion (0xb0) and the value slot it copies from */
    const rx_addr out = L + 0xd8;                        /* the RowRef the add writes */
    if (device == -1) {
        for (uint32_t i = 0; i < 4; i++) SU32(s, 0x48 + 0x48u * i, 0);
        SU32(s, 0x16c, 0xffffffffu);
    } else {
        if (I32(s, 0x16c) == -1) SU32(s, 0x16c, (uint32_t)device);
        rx_uisettings_row_defaults(s + 4 + 0x48u * (uint32_t)device);
    }
    rx_uisettings_apply(s, I32(s, 0x16c));

    const rx_addr t = rx_tbl_find(0x00ae0910u, 0x009411ccu);   /* "ControllerRemapping" */
    if (!t) { ACT_END(L); return; }
    const uint32_t ncols = (U32(t, 0x14) - U32(t, 0x10)) >> 2;
    if (ncols != 0) {
        const rx_addr col0 = U32(U32(t, 0x10), 0);
        if (rx_tbl_column_rows(col0) >= 1) { ACT_END(L); return; }   /* rows exist: retained */
    }
    ACT_CALL(1, rx_tbl_table_clear_rows(t));                 /* 004ecdb0 */
    remap_row(t, vec0, crit, val, out, k_remap_keyboard);
    remap_row(t, vec1, crit, val, out, k_remap_pad);
    if (!rx_tbl_find(0x00ae10f0u, 0x009411ccu)) {            /* register the table in player database 0 */
        const uint32_t n = U32(0, 0x00ae12d8);               /* its +0x1e8 table count */
        SU32(0, 0x00ae1128 + 4 * n, t);                      /* its +0x38 table array */
        SU32(0, 0x00ae12d8, n + 1);
        SU32(L, 0x10, t + 0x3c);                             /* the key: the table's inline name */
        const rx_addr slot = rx_tbl_name_map_index(0x00ae1108u, L + 0x10);   /* 004f0540 on +0x18 */
        SU32(slot, 0, t);
    }
    ACT_END(L);
}
RX_FUNCTION(0x005b80b0, rx_uisettings_reset)

/* 00743030 UI_SetPrimaryPlayerDeviceIndex (EAX = controller, no stack arguments, plain ret): ui+0x2a0 = it, the save object's
 * per-controller "used" word +0x3c + 4*controller = 1 for 0..3, then UISettings_Reset(settings, controller). The UI manager is not
 * core ([0xa93fcc] = 0): the +0x2a0 write is a UI-only field (no gameplay reader) and is done only when a manager exists. */
void rx_ui_set_primary_player_device_index(int32_t controller) {
    const rx_addr ui = U32(0, UI_MANAGER_GLOBAL);
    if (ui) SU32(ui, 0x2a0, (uint32_t)controller);
    if ((uint32_t)controller < 4) SU32(U32(0, SAVE_OBJECT_GLOBAL), 0x3c + 4 * (uint32_t)controller, 1);
    rx_uisettings_reset(U32(0, UI_SETTINGS_GLOBAL), controller);
}
RX_FUNCTION(0x00743030, rx_ui_set_primary_player_device_index)

/* 00598640's settings part: allocate the 0x170-byte block and construct it when absent (the original's `if (g_UISettings == 0)`
 * inside MXUIManager::Construct). Returns the settings object. */
rx_addr rx_ui_settings_create(void) {
    rx_addr s = U32(0, UI_SETTINGS_GLOBAL);
    if (s) return s;
    s = rx_reflex_aligned_alloc(0x170);
    if (!s) { SU32(0, UI_SETTINGS_GLOBAL, 0); return 0; }
    s = rx_uisettings_construct(s);
    SU32(0, UI_SETTINGS_GLOBAL, s);
    return s;
}

/* The shipping guest route's profile step (ProfileReminderPopup.lua: RestoreDefaultTables -> SetPrimaryPlayerDeviceIndex ->
 * MainMenu), which precedes every event and any local player. The product has no front end: it runs at the end of the startup
 * once MXRegistry_Load has read the tables (the original's point), with controller 0 = the single-player session's primary
 * player device (the event spec's local player; the reference boot's settings +0x16c == 0 and row 0's loaded flag 1). */
void rx_ui_profile_bootstrap(void) {
    if (!rx_ui_settings_create()) return;
    rx_ui_set_primary_player_device_index(0);
}

/* Host-to-Rdb boundary. These are snapshots of real profile rows, not alternate vehicle rules. */
#include "../include/rx_api.h"
#include <stdlib.h>
static const char* const profile_tables[5] = {"PlayerRider", "PlayerRiderGear", "PlayerVehicle", "PlayerVehicleParts", "PlayerVehicleTuning"};
static int profile_error(char* e, size_t n, const char* s) { if (e && n) snprintf(e, n, "%s", s); return -1; }
static int guest_name(rx_addr a, const char* s) { size_t i; for (i=0; s[i]; ++i) if (U8(a,(uint32_t)i)!=(uint8_t)s[i]) return 0; return U8(a,(uint32_t)i)==0; }
static rx_addr profile_table(rx_addr db, const char* name) {
    for(uint32_t i=0;i<U32(db,0x1e8);++i) { rx_addr t=U32(db,0x38+4*i); if(t && guest_name(t+0x3c,name)) return t; } return 0;
}
static rx_addr profile_column(rx_addr t, const char* name) {
    if(!t) return 0;
    for(rx_addr a=U32(t,0x10);a<U32(t,0x14);a+=4) { rx_addr c=U32(a,0); if(guest_name(c,name)) return c; } return 0;
}
static int profile_cell_value(const struct rx_profile_cell* cells,int n,const char* table,int row,const char* column,int32_t* out) {
    for(int i=0;i<n;++i) if(cells[i].row==row && !strcmp(cells[i].table,table) && !strcmp(cells[i].column,column) && cells[i].type==0) { *out=cells[i].value;return 1; } return 0;
}
static int profile_has_key(const struct rx_profile_cell* c,int n,const char* t,int rows,const char* key,int32_t value) {
    for(int r=0;r<rows;++r) { int32_t x; if(profile_cell_value(c,n,t,r,key,&x)&&x==value) return 1; } return 0;
}
static int profile_native_key(const char* table,const char* key,int32_t value) {
    rx_addr t=profile_table(RX_TBL_DEFAULT_DB,table), c=profile_column(t,key);
    if(!c) return 0;
    uint32_t ci=0; while(U32(U32(t,0x10),4*ci)!=c) ++ci;
    for(uint32_t r=0;r<rx_tbl_row_count(t);++r) if(I32(rx_tbl_cell_word(t,ci,r),0)==value) return 1;
    return 0;
}
static int profile_native_pair(const char* table,const char* first,int32_t a,const char* second,int32_t b) {
    rx_addr t=profile_table(RX_TBL_DEFAULT_DB,table), c1=profile_column(t,first),c2=profile_column(t,second);
    if(!c1||!c2) return 0;
    uint32_t i=0,j=0; while(U32(U32(t,0x10),4*i)!=c1) ++i; while(U32(U32(t,0x10),4*j)!=c2) ++j;
    for(uint32_t r=0;r<rx_tbl_row_count(t);++r) if(I32(rx_tbl_cell_word(t,i,r),0)==a&&I32(rx_tbl_cell_word(t,j,r),0)==b) return 1;
    return 0;
}
static int profile_native_int(const char* table,const char* key,int32_t id,const char* field,int32_t* out) {
    rx_addr t=profile_table(RX_TBL_DEFAULT_DB,table), k=profile_column(t,key), f=profile_column(t,field);
    if(!k||!f) return 0;
    uint32_t ki=0,fi=0; while(U32(U32(t,0x10),4*ki)!=k) ++ki; while(U32(U32(t,0x10),4*fi)!=f) ++fi;
    for(uint32_t r=0;r<rx_tbl_row_count(t);++r) if(I32(rx_tbl_cell_word(t,ki,r),0)==id) { *out=I32(rx_tbl_cell_word(t,fi,r),0);return 1; }
    return 0;
}
static int profile_cell_compare(const void* a,const void* b) {
    const struct rx_profile_cell* x=*(const struct rx_profile_cell* const*)a;
    const struct rx_profile_cell* y=*(const struct rx_profile_cell* const*)b;
    int t=strcmp(x->table,y->table); if(t) return t;
    if(x->row!=y->row) return x->row<y->row?-1:1;
    return strcmp(x->column,y->column);
}
static int profile_duplicate_cells(const struct rx_profile_cell* cells,int count) {
    const struct rx_profile_cell** sorted=malloc((size_t)count*sizeof *sorted);
    if(!sorted) return -1;
    for(int i=0;i<count;++i) sorted[i]=cells+i;
    qsort(sorted,(size_t)count,sizeof *sorted,profile_cell_compare);
    int duplicate=0;
    for(int i=1;i<count;++i) if(!profile_cell_compare(sorted+i-1,sorted+i)) {duplicate=1;break;}
    free(sorted); return duplicate;
}
int rx_ui_profile_import(const struct rx_profile_cell* cells,int count,char* error,size_t error_size) {
    rx_addr tables[5]; int rows[5]={0}; uint8_t seen[5][4096]={{0}};
    if(!cells||count<1||count>65536) return profile_error(error,error_size,"Invalid profile snapshot size");
    for(int t=0;t<5;++t) { tables[t]=profile_table(RX_TBL_PLAYER_DB(0),profile_tables[t]); if(!tables[t]) return profile_error(error,error_size,"Profile schema unavailable"); }
    for(int i=0;i<count;++i) {
        const struct rx_profile_cell* c=cells+i; int t;
        if(!memchr(c->table,0,sizeof c->table)||!memchr(c->column,0,sizeof c->column)||!memchr(c->text,0,sizeof c->text)) return profile_error(error,error_size,"Unterminated profile text");
        for(t=0;t<5;++t) if(!strcmp(c->table,profile_tables[t])) break;
        if(t==5||c->row<0||c->row>=4096||c->type<0||c->type>1) return profile_error(error,error_size,"Invalid profile table, row or type");
        rx_addr col=profile_column(tables[t],c->column);
        if(!col||U32(col,0x2c)!=(c->type?4u:1u)) return profile_error(error,error_size,"Profile column type mismatch");
        seen[t][c->row]=1; if(rows[t]<=c->row) rows[t]=c->row+1;
    }
    int duplicate=profile_duplicate_cells(cells,count);
    if(duplicate) return profile_error(error,error_size,duplicate<0?"Profile validation allocation failed":"Duplicate profile cell");
    if(rows[0]!=1||rows[2]<1) return profile_error(error,error_size,"Profile needs one rider and a vehicle");
    for(int t=0;t<5;++t) for(int r=0;r<rows[t];++r) if(!seen[t][r]) return profile_error(error,error_size,"Noncontiguous profile rows");
    int32_t selected,rider,gear;
    if(!profile_cell_value(cells,count,"PlayerRider",0,"LastEquippedVehicle",&selected)||!profile_cell_value(cells,count,"PlayerRider",0,"LastRiderId",&rider)||!profile_cell_value(cells,count,"PlayerRider",0,"LastRiderGearSetId",&gear)) return profile_error(error,error_size,"Missing rider selection");
    if(rider>0) {
        int32_t default_gear;
        if(!profile_native_int("AIRider","AIRiderId",rider,"DefaultRiderGearSetId",&default_gear)||gear!=default_gear||!profile_native_key("RiderGearSet","RiderGearSetId",gear)||!profile_native_key("AIVehicle","AIVehicleId",selected)||!profile_native_pair("AIRiderVehicle","AIRiderId",rider,"AIVehicleId",selected)) return profile_error(error,error_size,"Invalid pro rider selection");
    }
    else if(!profile_has_key(cells,count,"PlayerVehicle",rows[2],"PlayerVehicleId",selected)) return profile_error(error,error_size,"Selected player vehicle missing");
    for(int r=0;r<rows[2];++r) {
        int32_t id,model;
        if(!profile_cell_value(cells,count,"PlayerVehicle",r,"PlayerVehicleId",&id)||id<0||!profile_cell_value(cells,count,"PlayerVehicle",r,"VehicleModelId",&model)||!profile_native_key("VehicleModel","VehicleModelId",model)) return profile_error(error,error_size,"Invalid player vehicle");
        for(int q=0;q<r;++q) { int32_t prior; profile_cell_value(cells,count,"PlayerVehicle",q,"PlayerVehicleId",&prior); if(id==prior) return profile_error(error,error_size,"Duplicate player vehicle id"); }
    }
    int32_t gear_types[4096],part_vehicles[4096],part_types[4096];
    for(int t=1;t<5;++t) for(int r=0;r<rows[t];++r) {
        int32_t id;
        if(t==1) {
            if(!profile_cell_value(cells,count,profile_tables[t],r,"RiderGearId",&id)||!profile_native_int("RiderGear","RiderGearId",id,"RiderGearTypeId",gear_types+r)) return profile_error(error,error_size,"Invalid rider gear");
            for(int q=0;q<r;++q) if(gear_types[q]==gear_types[r]) return profile_error(error,error_size,"Duplicate equipped rider gear type");
        }
        if(t>=3) { if(!profile_cell_value(cells,count,profile_tables[t],r,"PlayerVehicleId",&id)||!profile_has_key(cells,count,"PlayerVehicle",rows[2],"PlayerVehicleId",id)) return profile_error(error,error_size,"Orphan vehicle profile row"); }
        if(t==3) {
            int32_t vehicle_id=id, model=-1;
            if(!profile_cell_value(cells,count,profile_tables[t],r,"VehiclePartId",&id)||!profile_native_key("VehiclePart","VehiclePartId",id)) return profile_error(error,error_size,"Invalid vehicle part");
            for(int q=0;q<rows[2];++q) { int32_t vid; profile_cell_value(cells,count,"PlayerVehicle",q,"PlayerVehicleId",&vid); if(vid==vehicle_id) profile_cell_value(cells,count,"PlayerVehicle",q,"VehicleModelId",&model); }
            if(!profile_native_pair("VehicleModelParts","VehicleModelId",model,"VehiclePartId",id)) return profile_error(error,error_size,"Vehicle part incompatible with model");
            part_vehicles[r]=vehicle_id;
            if(!profile_native_int("VehiclePart","VehiclePartId",id,"VehiclePartTypeId",part_types+r)) return profile_error(error,error_size,"Invalid vehicle part type");
            for(int q=0;q<r;++q) if(part_vehicles[q]==vehicle_id&&part_types[q]==part_types[r]) return profile_error(error,error_size,"Duplicate equipped vehicle part type");
        }
    }
    if(rider<1 && rows[1]==0) return profile_error(error,error_size,"Custom rider gear missing");
    for(int r=0;r<rows[4];++r) {
        static const char* const tune_names[8]={"Engine1","Engine2","Suspension1","Suspension2","Tire1","Tire2","Braking","Steering"};
        int32_t id; profile_cell_value(cells,count,"PlayerVehicleTuning",r,"PlayerVehicleId",&id);
        for(int q=0;q<r;++q) { int32_t prior; profile_cell_value(cells,count,"PlayerVehicleTuning",q,"PlayerVehicleId",&prior); if(id==prior) return profile_error(error,error_size,"Duplicate tuning vehicle id"); }
        int32_t model=-1;
        for(int q=0;q<rows[2];++q) { int32_t vid; profile_cell_value(cells,count,"PlayerVehicle",q,"PlayerVehicleId",&vid); if(vid==id) {profile_cell_value(cells,count,"PlayerVehicle",q,"VehicleModelId",&model);break;} }
        for(int k=0;k<8;++k) {
            int32_t v; if(!profile_cell_value(cells,count,"PlayerVehicleTuning",r,tune_names[k],&v)||v<0||v>100||(k>=6&&v%50)) return profile_error(error,error_size,"Invalid profile tuning");
            if(k<6) { char field[32];int32_t base;snprintf(field,sizeof field,"Default%s",tune_names[k]);
                if(!profile_native_int("VehicleModel","VehicleModelId",model,field,&base)||(int64_t)base+v>100) return profile_error(error,error_size,"Profile tuning exceeds model limits"); }
        }
    }
    /* PlayerRider has one persistent default row, including camera float columns. The original
     * clear-rows path for non-int columns is not ported: update its supplied cells with the
     * existing original setters, retaining omitted camera values. Reject before any mutation. */
    if(rx_tbl_row_count(tables[0])!=1) return profile_error(error,error_size,"Expected one existing player rider row");
    /* Every validation above is read-only. Only complete, checked snapshots reach the original Rdb writers. */
    RX_FRAME_ENTER(F,0x200);
    for(int i=0;i<count;++i) if(!strcmp(cells[i].table,profile_tables[0])) {
        rx_addr col=profile_column(tables[0],cells[i].column);
        if(cells[i].type) {
            const size_t n=strlen(cells[i].text);
            for(size_t k=0;k<=n;++k) SU8(F+0x100,(uint32_t)k,(uint8_t)cells[i].text[k]);
            rx_tbl_str_init(F+0x20);
            rx_tbl_str_assign_n(F+0x20,F+0x100,(uint32_t)n);
            rx_tbl_set_string(col,0,F+0x20);
            rx_tbl_str_tidy(F+0x20);
        } else {
            SU32(F,0,(uint32_t)cells[i].value);
            rx_tbl_set_cell_int(tables[0],col,0,F);
        }
    }
    for(int t=1;t<5;++t) {
        rx_tbl_table_clear_rows(tables[t]);
        for(int r=0;r<rows[t];++r) {
            rx_tbl_crit_vec_init(F);
            for(int i=0;i<count;++i) if(cells[i].row==r&&!strcmp(cells[i].table,profile_tables[t])) {
                rx_addr col=profile_column(tables[t],cells[i].column);
                if(cells[i].type) { for(size_t k=0;k<=strlen(cells[i].text);++k) SU8(F+0x100,(uint32_t)k,(uint8_t)cells[i].text[k]); rx_tbl_crit_init_string(F+0x20,col,0,F+0x100); }
                else rx_tbl_crit_init_int(F+0x20,col,0,(uint32_t)cells[i].value);
                rx_tbl_crit_push(F,F+0x20); rx_tbl_crit_destroy(F+0x20);
            }
            rx_tbl_add_row(tables[t],F+0xe0,F); rx_tbl_crit_vec_destroy(F);
        }
    }
    RX_FRAME_LEAVE(F,0x200);
    if(error&&error_size) *error=0; return 0;
}
static rx_addr profile_local_spec(void) {
    rx_addr g=U32(0,0x00a946b8);
    for(uint32_t i=0;i<U32(g,0x154dc);++i) { rx_addr s=U32(U32(g,0x154d8),4*i); if(s&&rx_vehicle_spec_controller_of(I32(s,4))==0&&U32(s,0xc4)) return s; }
    return 0;
}
int rx_ui_profile_tuning_read(int32_t out[8],char* error,size_t error_size) {
    rx_addr s=profile_local_spec(); if(!out||!s) return profile_error(error,error_size,"Local vehicle unavailable");
    for(int i=0;i<8;++i) out[i]=I32(s,0xa0+4*(uint32_t)i);
    if(error&&error_size) *error=0; return 0;
}
int rx_ui_profile_tune(const int32_t values[8],char* error,size_t error_size) {
    static const char* const names[8]={"Engine1","Engine2","Suspension1","Suspension2","Tire1","Tire2","Braking","Steering"};
    rx_addr s=profile_local_spec(); if(!values||!s||I32(s,0x98)!=0||I32(s,0x9c)<0) return profile_error(error,error_size,"Selected vehicle is not player owned");
    for(int i=0;i<8;++i) if(values[i]<0||values[i]>100||(i>=6&&values[i]%50)|| (i<6&&(int64_t)I32(U32(s,8),0x1c+4*(uint32_t)i)+values[i]>100)) return profile_error(error,error_size,"Invalid tuning values");
    for(int i=0;i<6;i+=2) {
        int64_t before=(int64_t)I32(s,0xa0+4*(uint32_t)i)+I32(s,0xa0+4*(uint32_t)(i+1))
            -I32(U32(s,8),0x1c+4*(uint32_t)i)-I32(U32(s,8),0x1c+4*(uint32_t)(i+1));
        if((int64_t)values[i]+values[i+1]!=before) return profile_error(error,error_size,"Tuning must preserve the equipped bonus points");
    }
    rx_addr t=profile_table(RX_TBL_PLAYER_DB(0),"PlayerVehicleTuning"), key=profile_column(t,"PlayerVehicleId");
    if(!key) return profile_error(error,error_size,"Tuning table unavailable");
    uint32_t ci=0; while(U32(U32(t,0x10),4*ci)!=key) ++ci;
    uint32_t row=rx_tbl_row_count(t); for(uint32_t r=0;r<row;++r) if(I32(rx_tbl_cell_word(t,ci,r),0)==I32(s,0x9c)) {row=r;break;}
    RX_FRAME_ENTER(F,0x100);
    if(row==rx_tbl_row_count(t)) { rx_tbl_crit_vec_init(F); rx_tbl_crit_init_int(F+0x20,key,0,U32(s,0x9c));rx_tbl_crit_push(F,F+0x20);rx_tbl_crit_destroy(F+0x20);rx_tbl_add_row(t,F+0xe0,F);rx_tbl_crit_vec_destroy(F); }
    for(int i=0;i<8;++i) {SU32(F,0,(uint32_t)values[i]);rx_tbl_set_int(profile_column(t,names[i]),row,F);}
    rx_vehicle_spec_apply_tuning(s);
    RX_FRAME_LEAVE(F,0x100);
    if(error&&error_size) *error=0; return 0;
}


/* Product controls boundary: use the real controller table consumed by 00547f30.
 * Save persistence belongs to the client; the native defaults have one owner. */
extern void rx_keyboard_scan_defaults(int32_t out[256]);
int rx_ui_remap_defaults(struct rx_remap_table* out) {
    if (!out) return 1;
    for (int i = 0; i < 19; ++i) {
        out->keyboard[i] = (int32_t)k_remap_keyboard[i].id;
        out->pad[i] = (int32_t)k_remap_pad[i].id;
    }
    rx_keyboard_scan_defaults(out->scan);
    return 0;
}
static int remap_columns(rx_addr columns[19], char* error, size_t error_size) {
    const rx_addr table = profile_table(0x00ae0910u, "ControllerRemapping");
    if (!table || rx_tbl_row_count(table) != 2)
        return profile_error(error, error_size, "Controller table is unavailable");
    const rx_addr begin = U32(table, 0x10), end = U32(table, 0x14);
    if (end < begin || (end - begin) / 4 > 128)
        return profile_error(error, error_size, "Invalid controller schema");
    for (int i = 0; i < 19; ++i) {
        columns[i] = 0;
        for (rx_addr at = begin; at < end; at += 4) {
            const rx_addr col = U32(at, 0);
            int match = col != 0;
            for (uint32_t k = 0; match && k < 32; ++k) {
                const uint8_t a = U8(col, k), b = U8(k_remap_keyboard[i].name, k);
                if (a != b) match = 0;
                if (!a || !b) break;
                if (k == 31) match = 0;
            }
            if (match) { columns[i] = col; break; }
        }
        if (!columns[i] || rx_tbl_column_rows(columns[i]) != 2)
            return profile_error(error, error_size, "Controller column is unavailable");
    }
    return 0;
}
int rx_ui_remap_get(struct rx_remap_table* out, char* error, size_t error_size) {
    if (!out) return profile_error(error, error_size, "Missing controller output");
    rx_addr columns[19];
    if (remap_columns(columns, error, error_size)) return -1;
    struct rx_remap_table next;
    rx_ui_remap_defaults(&next);
    for (int i = 0; i < 19; ++i) {
        const rx_addr cells = U32(U32(columns[i], 0x40), 0x0c);
        next.keyboard[i] = I32(cells, 0); next.pad[i] = I32(cells, 4);
    }
    *out = next;
    return 0;
}
int rx_ui_remap_set(const int32_t values[38], char* error, size_t error_size) {
    if (!values) return profile_error(error, error_size, "Missing controller values");
    int32_t copy[38]; memcpy(copy, values, sizeof copy);
    for (int i = 0; i < 38; ++i) {
        const int32_t v = copy[i];
        const int keyboard = v >= 0x23 && v <= 0x87;
        const int pad = v >= 1 && v <= 24;
        /* Hidden camera-tweak entries in the pad row are keyboard IDs in the original. */
        if ((i < 19 && !keyboard) || (i >= 19 && !(pad || (i >= 34 && keyboard))))
            return profile_error(error, error_size, "Invalid controller input ID");
    }
    rx_addr columns[19];
    if (remap_columns(columns, error, error_size)) return -1;
    RX_FRAME_ENTER(F, 16);
    for (int row = 0; row < 2; ++row) for (int i = 0; i < 19; ++i) {
        SU32(F, 0, (uint32_t)copy[row * 19 + i]);
        rx_tbl_set_int(columns[i], (uint32_t)row, F);
    }
    RX_FRAME_LEAVE(F, 16);
    return 0;
}
