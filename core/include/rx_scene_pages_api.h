/* rx_scene_pages_api.h -- what the in-world pages need from the scene (additive, w-p2scene 2026-10-27; no existing struct changed; product
 * only): the start gate's frames for the career Gate Selection page's 3D camera (rx_session_start_gates, read-only) and the Event Intro page's
 * SkipToNextScenario (rx_session_skip_scenario).
 *
 * The original's Gate Selection page (GateSelection.lua) runs App.ShowGates( 25, 7 ) and App.SetCameraToGate( gate, 25, 7, device, 0.5 )
 * (GateSelection_ShowGates 0073f8a0 / GateSelection_SetCameraToGate 0073ed70): from the level's start gate model "TO_StartingGate"
 * (a CollisionEntity, model GL_StartGate / NAT_CO_StartGate) it reads the node "GateStart_%02d" (gate + 1) of every gate, puts the
 * gate cards "UI_StartCard_NN" (and "UI_StartCard_X_NN" for a taken gate) above them and the arrow "UI_StartCard_Arrow" over the focused
 * one, and aims the level's UIGateSelectionCameraEntity "UI_StartGate_Camera" (an MXUIPartsViewCamera in its spring mode, placement
 * = the gate entity's matrix) at it: look-at point = the node's position, eye = that position - forward * distance + (0, height, 0),
 * both in the gate model's space, blended over the call's 0.5 s. This export hands the renderer the inputs of that code:
 * the gate entity's matrix (entity + 0x30, Entity::vf14 0088fa20) and every node's world matrix (Node_EvaluateWorld 007db890, the
 * game's own lazy evaluator) rows 2 (forward) and 3 (position) in the model's space, and the same two world-space (x the entity
 * matrix, as 005663b0 does for the start grid). has_camera: the level has the UI_StartGate_Camera entity (the camera's points are in
 * the model's space and its placement is the gate matrix; without it the original computes the world-space points and aims the
 * view camera directly); fov / near_plane / far_plane are that entity's camera's lens when it has one (else 0). */
#ifndef RX_SCENE_PAGES_API_H
#define RX_SCENE_PAGES_API_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RX_SCENE_PAGES_API_VERSION 1u

typedef struct rx_start_gate_node {
    float local_position[3], local_forward[3];   /* node world matrix row 3 / row 2, in the gate model's space */
    float position[3], forward[3];                /* the same x the gate entity's matrix (world space; forward is not normalised) */
} rx_start_gate_node;                             /* 48 bytes */

typedef struct rx_start_gates {
    uint32_t size, version;
    int32_t valid;                                /* TO_StartingGate found, with its model's nodes */
    int32_t gate_count;                           /* GateStart_01 .. found (16 in the shipped levels) */
    int32_t has_camera;                           /* the UI_StartGate_Camera entity exists */
    float fov, near_plane, far_plane;             /* its lens (0 when unknown) */
    float gate_matrix[16];                        /* the gate entity's matrix, row vectors */
    uint32_t present_mask;                        /* bit i: GateStart_(i+1) exists */
    uint32_t reserved[3];
    rx_start_gate_node gates[16];
} rx_start_gates;                                 /* 1008 bytes */

/* Fills out (size / version set by the caller). Returns 0 (valid says whether there is a start gate), 1 for a bad session or struct. */
RX_API int rx_session_start_gates(struct rx_session* s, rx_start_gates* out);

/* The Event Intro page's accept (EventIntro.lua: UIKeypressTrigger MapKey_UI_Accept -> EventIntro:SkipToNextScenario -> App.SkipToNextScenario,
 * 00736d60): sets the skip request word [0xa7f748] = 1, which the intro scenario's update (IntroScenario_Update 007473f0, core/game/
 * anim_scenario_intro.c) consumes on its next call when the activity's rules hold is clear (it clears the word either way), ending the intro
 * and starting the race's countdown. The word is the whole of the call: the same bytes the Lua function writes, and what the pad's
 * ScenarioNextButton (input 0xe, the X button) does through the scenario's own button test. Returns 0, or 1 for a bad session. Call between
 * advances. Nothing else reads the word: the post scenarios (the finish and results scenarios) have no skip on the PC. */
RX_API int rx_session_skip_scenario(struct rx_session* s);
#ifdef __cplusplus
}
#endif
#endif
