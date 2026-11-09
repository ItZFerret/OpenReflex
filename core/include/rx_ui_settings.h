#ifndef RX_UI_SETTINGS_H
#define RX_UI_SETTINGS_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The game's UI settings in the running session (w-menus, 2026-10-27; additive, RX_SESSION_API_VERSION unchanged).
 * The settings object [0x00d92efc] (MXUISettings: four per-device rows and a default row of 16 values) is what the game's own code
 * reads: MouseSensitivity (the Raw Input sensitivity), Vibration (the pad's motors), CareerDifficulty (the AI's pace), Handedness /
 * ControllerMotion / ControllerPreset (the KeyMaps built when a player is created), MeasurementSystem, HUDVisibility, DrivingCamera,
 * the volumes. Before this call the product session kept the guest profile's defaults whatever the player chose in the options.
 *   name   one of the 16 names of the game's table (0x009dd028; case-insensitive): DrivingCamera, Brightness, Gamma, OnlinePlayerName,
 *          MeasurementSystem, ControllerPreset, EffectsVolume, MusicVolume, EngineVolume, CrowdVolume, ControllerMotion, ControllerHandedness ("Handedness" also),
 *          CareerDifficulty, HUDVisibility, MouseSensitivity, Vibration
 *   mode   0 read, 1 write: UISettings_Set 005b9fa0 (the device's row, UISettings_Apply 005b9af0 when the value changed)
 *   before receives the value before the call (may be NULL)
 * The device is the settings' selected one (the primary player's controller). Returns 0, 1 bad arguments / no session, 2 unknown name or
 * no settings object. Session owner thread, between advances. */
RX_API int rx_session_ui_setting(struct rx_session* s, const char* name, int32_t value, int32_t mode, int32_t* before);
#ifdef __cplusplus
}
#endif
#endif
