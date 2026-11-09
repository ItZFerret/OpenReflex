/* ui_settings_session.inc -- rx_session_ui_setting (include/rx_ui_settings.h), included by rx_session.c after race_hud_session.inc (it uses
 * session_ok()). The index is the position in the game's own name table (0x009dd028, read from the guest image), the read is
 * UISettings_Get (rx_ui_settings_value), the write UISettings_Set 005b9fa0 and its UISettings_Apply (rx_uisettings_apply): the same
 * code the game's options pages call (App.SetUISetting). */
#include "rx_ui_settings.h"
#include <strings.h>
#include "../game/race_activity.h"

static const char* const k_ui_setting_names[16] = {
    "DrivingCamera", "Brightness", "Gamma", "OnlinePlayerName", "MeasurementSystem", "ControllerPreset", "EffectsVolume", "MusicVolume",
    "EngineVolume", "CrowdVolume", "ControllerMotion", "ControllerHandedness", "CareerDifficulty", "HUDVisibility", "MouseSensitivity", "Vibration",
};

static int ui_setting_index(const char* name) {
    if (strcasecmp(name, "Handedness") == 0) name = "ControllerHandedness";   /* the short name */
    static const rx_addr table = 0x009dd028u;
    for (int i = 0; i < 16; i++) {
        const rx_addr p = U32(0, table + 4u * (uint32_t)i);
        char guest[48]; guest[0] = 0;
        if (p) {
            uint32_t k = 0;
            for (; k + 1 < sizeof guest; k++) { guest[k] = (char)U8(p, k); if (!guest[k]) break; }
            guest[k] = 0;
        }
        if (guest[0] ? strcasecmp(guest, name) == 0 : strcasecmp(k_ui_setting_names[i], name) == 0) return i;
    }
    return -1;
}

int rx_session_ui_setting(struct rx_session* s, const char* name, int32_t value, int32_t mode, int32_t* before) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR || !name) return 1;
    const rx_addr settings = rx_ui_settings();
    if (!settings) return 2;
    const int index = ui_setting_index(name);
    if (index < 0) return 2;
    int32_t device = I32(settings, 0x16c);
    if (device < 0 || device > 3) return 2;   /* no primary player device yet (the profile bootstrap sets it) */
    const int32_t was = (int32_t)rx_ui_settings_value(settings, (uint32_t)index, (uint32_t)device);
    if (before) *before = was;
    if (mode == 0) return 0;
    /* 005b9fa0 UISettings_Set (EAX settings, EDX value, ESI index, EDI device) */
    const int32_t selected = I32(settings, 0x16c);
    uint32_t row;
    if (device == selected) row = (uint32_t)selected * 9u;
    else if (index == 5 || index == 10 || index == 11 || index == 13 || index == 0) row = (uint32_t)device * 9u;
    else row = (uint32_t)selected * 9u;
    const uint32_t field = (uint32_t)index + row * 2u;
    if (I32(settings, field * 4u + 4u) != value) {
        SU32(settings, field * 4u + 4u, (uint32_t)value);
        rx_uisettings_apply(settings, device);
    }
    return 0;
}
