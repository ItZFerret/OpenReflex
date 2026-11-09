/* alive_gate.h -- PORT ADDITION (not original game code; w-alivegates, core/plan/ALIVE_RACING.md "Start gate"): the start gate of a converted
 * MX vs ATV Alive race behaves like Reflex's DropGate (world_dropgate.c, anim_ik_ent.c).
 *
 * Reflex's gate is two entities: "TO_StartingGate" (a CollisionEntity whose model holds the 16 GateStart nodes) and a DropGate (the bar: a
 * CollisionEntity that swings down on "Countdown Step" 0 and switches its collision off once it is down). An Alive level has one entity, the
 * ScenarioAnimEntity "TO_StartingGate" (model GL_TO_StartGate, JS7 FR_JSComp_StartGate) with 12 GateStart nodes and the bar on its node
 * "DropGate" (JS7 "FR_JSComp_DropGate"); Alive drops it with the cause-and-effect library GateDrop: on the race's "InProgress" it plays the
 * animation GL_TO_StartGate_Drop on the gate (JS7: the animation of its own bar, FR_JSComp_StartingGate_Drop) and the sound
 * UI_02/UI/GATE_DROP. The gate itself has no collision in Alive (its Gate_Bit entity is the floor plates under the riders).
 *
 * Here, only for such a gate (a ScenarioAnimEntity named TO_StartingGate: no Reflex level has one):
 *  - attach (the grid pass found the 12-slot gate, build_activity_grid.c): the gate is shown (a scenario entity is hidden until a scenario
 *    plays it) at its model's pose (the bar up), and the bar gets a static collision box (the bar plate's box in the gate's frame, at the
 *    session's MAP SCALE) in the level's physics world, owned by the level's Gate_Bit CollisionEntity;
 *  - start states (rx_activity_entities_start, beside the DropGates' 006bdbe0): as Reflex's gate, a gated race starts closed (collision on,
 *    the bar up), a free ride / mode 0 / mode 6 / a spectated session starts open (collision off, the drop played to its end);
 *  - "InProgress" entered (Activity_SetState, the moment Reflex posts "Countdown Step" 0): Alive's drop animation plays (the bar turns down
 *    31 deg in 0.2 s, bounces and settles at 29 deg: Reflex's own drop is 30 deg with a bounce), and the collision switches off when the bar
 *    is down (the 0.2 s key, 12 frames; Reflex's DropGate clears its own at its down angle, 006be1c0 state 1 -> 2), counted in the gate's
 *    own update (it stops while the world is paused).
 * The drop sound: Reflex's own, the activity cue "<ACTIVITY_NATIONALS | ACTIVITY_SUPERCROSS>/ACTIVITY/DROP_GATE" (a Reflex level's
 * ActivityAudioManager plays it at GO; an Alive level has no such entity, so the drop records it for the host, sound_state.c).
 * All state is host statics (restored at the session's destroy). Nothing runs for a Reflex level. */
#pragma once
#include "../src/rx_mem.h"
#include <stdint.h>

void rx_alive_gate_attach(rx_addr activity, rx_addr gate);       /* the grid pass found an Alive gate (12 GateStart nodes) */
void rx_alive_gate_start(rx_addr activity);                      /* the activity entities' start states (00557fe0) */
void rx_alive_gate_state_entered(rx_addr activity, uint32_t state);   /* Activity_SetState (005586c0) entered `state` */
void rx_alive_gate_tick(rx_addr entity);                         /* a ScenarioAnimEntity's update while its world runs (0074ab50) */

void rx_alive_gate_set_collision(int on);                         /* the bar's collision now (a test hook: alive_gate_test) */

/* the session's Alive gate for the exports (0: none): its entity, its collision on (1) / off (0) / none (-1), frames since the drop (-1) */
rx_addr rx_alive_gate_entity(void);
int32_t rx_alive_gate_collision(void);
int32_t rx_alive_gate_drop_frames(void);
