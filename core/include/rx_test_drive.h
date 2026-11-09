#ifndef RX_TEST_DRIVE_H
#define RX_TEST_DRIVE_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The garage's TEST DRIVE (w-testride, 2026-10-27; additive, RX_SESSION_API_VERSION unchanged; api/test_drive_session.inc,
 * game/garage_test_drive.c, _re/notes/garage.md section 5).
 * The original rides the test drive in the front end's own world UI_Compound (MXRegistry UISceneName: the MXUIWorld every session
 * builds at create, game + 0x20c8) with its FreeRide activity, game state 2: no level is loaded. GarageHUB:OnStartPressed runs
 * App.EnableTestDrive( TRUE ) and App.SyncDatabaseWithManagers, the Test Drive Loading Page mxGarage:TestDriveStart( ); the world's
 * frames spawn the profile's equipped vehicle and rider (PlayerRider.LastEquippedVehicle / LastRiderId: the profile or player
 * snapshot imported before the start chooses them). The pause request ends it: App.EnableTestDrive( FALSE ), then the Test Drive
 * Exit Page's mxGarage:TestDriveEnd( ).
 *   rx_session_test_drive_start  instead of rx_session_event_load, on a fresh session (after the profile / player import, the
 *                                remap and the UI settings). `spec` gives the player's mode and display name (rx_event_spec_default
 *                                when NULL); the event type / location are the test drive's (FreeRide at the UI level). Then
 *                                rx_session_advance as for an event: the session is LOADING until the local slot has its vehicle
 *                                and rider, then RACING. 0, 1 fault / invalid session (last_error), 2 wrong state.
 *   rx_session_test_drive_end    the pause request's end (EnableTestDrive( FALSE ): the players idle, the activity area without them;
 *                                then TestDriveEnd). The session may then be destroyed (nothing is saved). 0, 1 fault, 2 no test drive.
 *   rx_session_test_drive        the state: active (1 riding, 2 ended), spawned (the local slot's vehicle and rider exist), the local
 *                                slot's vehicle type, the UI words the original keeps (TestDriveEnabled ui+0x290, players idle ui+0x284). */
#define RX_TEST_DRIVE_API_VERSION 1u
typedef struct rx_test_drive_state {
    uint32_t size, version;              /* in: sizeof, RX_TEST_DRIVE_API_VERSION */
    int32_t active;                      /* 0 none, 1 started, 2 ended */
    int32_t spawned;                     /* the local slot's vehicle (+0x1ac) and rider (+0x1b4) exist */
    int32_t vehicle_type;                /* the local vehicle's VehicleTypeId (V+0x194), -1 none */
    int32_t enabled, players_idle;       /* the UI words of the original: ui+0x290, ui+0x284 */
    uint32_t frames;                     /* frames advanced since the start */
} rx_test_drive_state;
RX_API int rx_session_test_drive_start(struct rx_session* s, const rx_event_spec* spec);
RX_API int rx_session_test_drive_end(struct rx_session* s);
RX_API int rx_session_test_drive(struct rx_session* s, rx_test_drive_state* out);
#ifdef __cplusplus
}
#endif
#endif
