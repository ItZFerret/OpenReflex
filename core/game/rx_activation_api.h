#pragma once
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The Free Ride activation areas (game/race_activation.c), additive to RX_SESSION_API_VERSION 2 (no existing struct changed).
 *
 * In the Race & Ride challenges Free Ride the game marks the "StartActivity" area the player rides into in the local MXPlayer
 * (MXUIManager_OnTriggerEnter 005a3b60 / OnTriggerExit 005a3d00): +0xa0 in an area, +0xa4 the area's activity (the
 * EventTypeActivation the pause key starts: "HillClimb", "Trailblazer", "TargetPractice", ...; "FreeRide" after leaving one),
 * +0x1a8 its TrackId. The original's AAA notification shows while the player is in an area, and its pause handler
 * (MXUIManager_OnPauseRequest 00598e40) starts that activity: EventType FreeRide, EventTypeActivation = activity, TrackId = track.
 * The host does both from this snapshot (a new session with event_activation / track_id). */
#define RX_ACTIVATION_API_VERSION 1u
/* Copied snapshot, 48 bytes; no guest addresses. valid = 1 when the local player exists. activity is the MXPlayer's name (NUL
 * terminated, "" before any area was entered); active is +0xa0; track_id +0x1a8. required_class is the UI property
 * ChallengeRequiredVehicleClass the handlers set (the vehicle class an area needs when the player's vehicle is not one of it; -1 none):
 * the AAA notification's help line. The product keeps it beside the handlers (no UI property store in an exhibition session). */
typedef struct rx_activation_state {
    uint32_t size, version, valid, active;
    int32_t track_id, required_class;
    char activity[24];
} rx_activation_state;
/* Caller sets size (48) and version (1). Invalid arguments return 1 and leave the output alone. Session owner thread, between
 * advances. */
RX_API int rx_session_activation(struct rx_session* session, rx_activation_state* out);
/* Before rx_session_event_load: the event runs with the UI property InCareerMode set, as the original's Race & Ride challenges
 * Free Ride does (MainMenu:CareerMode sets it, RaceAndRideChallenges:OnAcceptChallenge then starts the Free Ride). The Free Ride
 * activity's discovery mode (MXActivityFreeRide::vf54) follows it: the activation areas are marked, the "AAA_" area markers stay
 * shown and the profile's found discovery collectables are restored. Also for the challenge started from that Free Ride. 0 ok,
 * 1 bad session, 2 after the event load or in a career session (whose own properties decide). */
RX_API int rx_session_event_in_career_mode(struct rx_session* session, int32_t value);
#ifdef __cplusplus
}
#endif
