#ifndef RX_STUNT_HUD_H
#define RX_STUNT_HUD_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The trick names of the stunt feedback overlay (w-polish2, 2026-10-27). A copy of what MXUIStuntFeedbackOverlay (the HUD layers'
 * "StuntFeedbackComponent": Freestyle, every race HUD, Free Ride and the challenges) keeps. Additive: a new query and struct;
 * RX_SESSION_API_VERSION, rx_race_hud_state and every other struct unchanged.
 *
 * The overlay (006032c0 vf11, 00603130 / 00603110 handlers, 006033d0 the text) listens to "StuntBegin" {player, trick} and
 * "PhysicsStuntBegin" {player, stunt, kind} on the stunt channel [0xa94304]. For its own (local) player it builds the text from the
 * player's trick tracker K = player + 0xb58 as it is at that moment: the current trick K + 0x1c and the physics stunt's record K + 0x68
 * (rows of the StuntTable, 0xc0 bytes: +0x40 the LocString key, the name in the MXRaven_Stunts table): "<trick>", "<stunt>" or
 * "<trick> + <stunt>". The text goes into the newest of three slots (the older two move up: StuntSlot1 newest / StuntSlot2 / StuntSlot3),
 * the layer's "RotateStunts" animation plays and the overlay shows; 3 s after the last begin (its own UI clock) it hides and clears the
 * slots; "Restart" hides and clears at once. "StuntFinalScore" / "StuntFailed" do nothing (the handlers are empty).
 * Here: the begin events as a ring, newest first, each with the two LocString keys (the presenter looks them up in MXRaven_Stunts and
 * formats the line) - the keys are copied at the event, as the text is. `events` counts every begin of the local player since
 * "Activity Init" (a drop means a new activity); `restart_events` counts "Restart"; the ring is emptied by a restart (count 0). */
#define RX_STUNT_HUD_VERSION 1u
#define RX_STUNT_HUD_RING 8       /* begin events kept (the presenter reads between frames; a frame never has that many) */
#define RX_STUNT_HUD_KEY 48       /* a LocString key's buffer ("STR_9OCLOCKINDIANAIR") */
typedef struct rx_stunt_hud_entry {
    uint32_t number;              /* the event's running number: events - index */
    uint32_t physics;             /* 1: "PhysicsStuntBegin", 0: "StuntBegin" */
    char trick[RX_STUNT_HUD_KEY]; /* LocString key of the player's current trick (K + 0x1c), "" none */
    char stunt[RX_STUNT_HUD_KEY]; /* LocString key of the physics stunt's record (K + 0x68), "" none */
} rx_stunt_hud_entry;
typedef struct rx_stunt_hud_state {
    uint32_t size, version, valid;
    uint32_t events, restart_events, count;
    rx_stunt_hud_entry entries[RX_STUNT_HUD_RING];   /* [0] the newest */
    uint32_t reserved[4];
} rx_stunt_hud_state;
/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0. valid = the local player has an activity the overlay is
 * bound to. Session owner thread, between advances. */
RX_API int rx_session_stunt_hud(struct rx_session* session, rx_stunt_hud_state* out);
#ifdef __cplusplus
}
#endif
#endif
