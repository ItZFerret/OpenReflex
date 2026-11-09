#ifndef RX_REFLEX_HUD_H
#define RX_REFLEX_HUD_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The wreck-recovery ("reflex") arrows of the HUD (w-reflexarrows, 2026-10-27): what MXUIReflexComponent - the HUD layers' "Arrows"
 * component - shows while a landing / a crash can still be saved by flicking the stick. Additive: a new query and struct;
 * RX_SESSION_API_VERSION and every other struct unchanged.
 *
 * The original (vf2 005f79c0, vf11 005f7b80, the "WreckRecoveryFailed" handler 005f7b00): the component finds its four arrow nodes
 * (ArrowTop / ArrowRight / ArrowBottom / ArrowLeft), starts hidden, and every frame reads the local driver's player P and its vehicle V:
 *   - V + 0x194 must be 0 (bike) or 1 (ATV), else the component is hidden;
 *   - P + 0xb44 (the wreck's kind, set by the rider's start-wreck 0054e8f0 from the wreck class' animation table 00637ff0, cleared by
 *     0054e890) picks the arrow: 0 hides all four arrows and the component; 2, 3 = ArrowTop (flick UP), 5, 6 = ArrowBottom (DOWN),
 *     8, 9 = ArrowLeft (LEFT), 11, 12 = ArrowRight (RIGHT); 1, 4, 7, 10 (the scripted "no input" kinds) change nothing. A lit arrow makes
 *     the component visible and plays the layer's "ReflexIn" animation (slides the arrow in over 0.2 s, alpha 0 -> 1; it is started
 *     again whenever it has finished, so the arrow pulses) - arrows are only hidden by kind 0, so two stay lit if the kind changes
 *     without passing 0 (the lit_mask keeps that);
 *   - the frame the component turns visible it starts the rider's sound slots 0 ("WreckRecoveryBegin": FX/WRECK_AVOIDANCE_WARN) and
 *     5 (WreckAvoidanceLoop, an empty event in the install) (006817c0), the frame it hides it stops slot 5 (00681840);
 *   - "WreckRecoveryFailed" (raised by the rider's fall 0054ed10 for ANY rider, sender = his vehicle, payload {activity, vehicle}): its
 *     handler sets the colour of ArrowTop / ArrowRight / ArrowBottom to (1, 0, 0, 0) - invisible; ArrowLeft is not in its loop.
 * The input side (Player_UpdateWreckRecovery 0054ea30): while P + 0xb38 > 0 (the time left; P + 0xb3c is the time the wreck started
 * with = the class' weight 1.0 x the severity) the wreck input (0054ee80: the two axes at P + 0xb48 / P + 0xb4c) must move past its
 * previous value in the direction of the kind; the moves add up in P + 0xb40 and above 0.25 it is WreckRecoverySuccess (0054ec80,
 * event "WreckRecoverySuccess" {driver, vehicle}); the clock running out is the fall (0054ed10, "WreckRecoveryFailed"). The kind -> arrow
 * table below is the same as the install's WreckAvoidanceParms.xml (GoLimp / HitHead / Cannonball / OffBack / VehicleDirection = Up,
 * OffRight = Left, OffLeft = Right, OverBars = Down) with MaxTime 1.0.
 * Which controls feed the wreck input: ControllerHandedness (pad block + 0x512c, 1 right) and ControllerMotion (+ 0x5130, 0 off, 1 left,
 * 2 right) pick the wreck maps (0054ee80 -> 00548fc0 / 00549050): input_source 0 = the compound reflex input (keys - the table's
 * ReflexLeft / Right / Up / Down columns -, mouse and the right stick), 1 = the right stick, 2 = the left stick, 3 = the motion sensor.
 *
 * Here: the component's own state (shown, lit arrows, the sound edge) is kept in game/reflex_hud.c, one tick per frame next to the
 * race HUD copies; the rest is read from P and V at the moment of the query; the four wreck events are counted by the event observer.
 * Nothing is written to guest memory (the one side effect is the host-side rider-sound ring, as the original's sound slots). */
#define RX_REFLEX_HUD_VERSION 1u
#define RX_REFLEX_DIR_NONE  0
#define RX_REFLEX_DIR_UP    1
#define RX_REFLEX_DIR_RIGHT 2
#define RX_REFLEX_DIR_DOWN  3
#define RX_REFLEX_DIR_LEFT  4
typedef struct rx_reflex_hud_state {
    uint32_t size, version, valid;      /* valid: the local player has an activity the component is bound to */
    uint32_t shown;                     /* the component is visible (vf11's SetVisible) */
    uint32_t lit_mask;                  /* arrows made visible and not yet hidden: bit 0 ArrowTop, 1 ArrowRight, 2 ArrowBottom, 3 ArrowLeft */
    int32_t direction;                  /* the direction the current wreck asks (RX_REFLEX_DIR_*; 0 none / no input kind) */
    uint32_t mode;                      /* P + 0xb44: the wreck kind (0 none, 1..12) */
    uint32_t wreck_class, wreck_type;   /* P + 0xb30, P + 0xb34 */
    uint32_t vehicle_kind;              /* V + 0x194 (0 bike, 1 ATV, else the component stays hidden) */
    uint32_t input_source;              /* 0 keys + mouse + right stick, 1 right stick, 2 left stick, 3 motion sensor */
    uint32_t handedness, motion;        /* the pad block's ControllerHandedness (1 right) and ControllerMotion (0 off, 1 left, 2 right) */
    float time_left, time_max;          /* P + 0xb38 (runs down; > 0 = a recovery is pending), P + 0xb3c (the start value) */
    float progress, threshold;          /* P + 0xb40 (the flick so far), 0.25 (above it the recovery succeeds) */
    float stick_x, stick_y;             /* P + 0xb48 / P + 0xb4c: the wreck input as last read (x rising = right, y rising = up) */
    uint32_t begin_events;              /* "WreckRecoveryBegin" of the local player's vehicle since "Activity Init" (a drop = a new activity) */
    uint32_t success_events;            /* "WreckRecoverySuccess" of the local player's vehicle */
    uint32_t fail_events;               /* "WreckRecoveryFailed" of the local player's vehicle */
    uint32_t fail_any_events;           /* "WreckRecoveryFailed" of any rider (what the component's handler reacts to) */
    uint32_t reset_events;              /* "WreckRecoveryReset" (the wreck state cleared by a fall / reset) of the local player's vehicle */
    uint32_t last_result;               /* the newest of success / fail of the local player: 0 none, 1 success, 2 fail */
    uint32_t recovered, falls;          /* P + 0x11d0, P + 0x11d4: the player's totals */
    uint32_t reserved[4];
} rx_reflex_hud_state;
/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0. Session owner thread, between advances. */
RX_API int rx_session_reflex_hud(struct rx_session* session, rx_reflex_hud_state* out);
#ifdef __cplusplus
}
#endif
#endif
