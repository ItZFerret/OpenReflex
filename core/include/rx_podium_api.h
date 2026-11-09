/* rx_podium_api.h -- the post-race presentation's cast for the renderer (additive, w-podium 2026-10-26; no existing struct changed).
 *
 * (Version 2, w-cast 2026-10-27: the race intro scenario - RaceIntroScenario / FMXIntroScenario / WaypointIntroScenario, activity Pre / Countdown -
 * uses the same cast: its start-line and crew chief shots play the same RiderCharacters (info.scenario 3) and hide the live riders.)
 * When a race / Freestyle event reaches Post, the game's finish scenario (RaceFinishScenario / FMXFinishScenario, core/game/
 * anim_scenario_finish.c) plays the finish replay and the podium: the level cast (world + 0x20150) gets one RiderCharacter per
 * player (0074ba40 PodiumRiders_Create: the player's own rider gear, his number / name plates bound by Model_OverrideTexture, the mud
 * of his vehicle copied by 0074bcc0 and the level's MudManager colour; G+0x1541c characters, one per player of the event), and the
 * scenario hides / shows the live riders and vehicles
 * (the rider's model instance +0xa2, the vehicle model's +0x14e). The crew chiefs, the scenario actors (the TrophyGirl, the podium
 * crowd) are ambient entities (rx_session_ambient_entities kinds CrewChief / ScenarioAnimEntity) and are drawn from there.
 * The camera of the scenario is rx_session_camera's (the world's MXAnimatedCamera while a post scenario plays).
 * (Version 3, w-frscene 2026-10-27: the Free Ride's FreerideIntroScenario - the arrival by hovercraft / sky crane / box car plane, then the
 * hero shot - is scenario 3 too (MXActivityFreeRide +0xff200, steps 1 the arrival, 4 / 5 the hero bike / vehicle shot, 13 two heroes). The
 * scenario scripts also hide / move static scenery models (DisableEntity 0055ddc0, ModelPosition 0055d4e0: the parked hovercraft HC_Static,
 * Gibson's dropped box FR_CH_DropBox_open) and hide nodes of a scenario actor (SetNodeVisibility 0055dff0: the sky crane box's petals):
 * rx_session_scenario_models and rx_session_ambient_node_flags export what the renderer reads there.) */
#ifndef RX_PODIUM_API_H
#define RX_PODIUM_API_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RX_PODIUM_API_VERSION 3u   /* 2: scenario 3 (the race intro plays: the same cast and slot rows), rx_podium_rider.draw_distance (was reserved);
                                     * 3: the Free Ride intro as scenario 3, rx_session_scenario_models, rx_session_ambient_node_flags,
                                     *    rx_session_scenario_sounds */

typedef struct rx_podium_info {
    int32_t riders;            /* podium RiderCharacters written */
    int32_t matrices;          /* bone matrices written (16 floats each) */
    int32_t scenario;          /* 0 none, 1 the finish scenario plays, 2 the results scenario plays, 3 the intro scenario plays (Pre / Countdown) */
    int32_t step;              /* that scenario's step (+0x151b4: finish 7 replay, 8 winner's mound, 9 / 11 walks, 10 / 12 podium; intro 1 .. 6) */
    int32_t post_state;        /* the activity's Post sub-state (-1 outside Post / an unknown class) */
    int32_t slots;             /* rx_podium_slot rows written */
    int32_t reserved[2];
} rx_podium_info;              /* 32 bytes */

/* One podium RiderCharacter (vtable 0x0095881c): the same 69-bone MXRider_Hierarchy as the riding rider, so it is drawn with that
 * player's rider model; its node world matrices are completed by the game's own evaluator (007db890) first. */
typedef struct rx_podium_rider {
    uint32_t object;           /* the character's guest address (identity) */
    int32_t slot;              /* the player slot it stands for (rx_session_slots), -1 unknown */
    int32_t visible;           /* the model instance's drawn flag (+0xa2) */
    int32_t bones;             /* 69, or 0 when the skeleton is not ready */
    int32_t first_matrix;      /* index of its first bone matrix in the matrix array */
    int32_t placed;            /* +0x70: the scenario placed it and plays its animation (0074be10); cleared by the cast stop (0074b4f0).
                                * The podium creates a character for every player (0074ba40), but a step places only its actors: the
                                * others keep their spawn matrix, the world origin. Draw visible && placed, and what the renderer's
                                * visibility test (draw_distance from the camera) leaves of them: at the origin that is nothing unless
                                * the camera is within draw_distance of it. */
    float draw_distance;       /* the renderer culls the character beyond this distance from the camera (the instance's ModelAsset copy +0x120 /
                                * the renderer's LOD divisor, as Model_SubmitSkinned 007f4e50 reads it from RiderCharacter::vf2_o90 00624b70); 0 = never */
    int32_t reserved;
    float root[16];            /* the entity root node (69) world matrix, row vectors */
    float mud_amount[4];       /* +0x3b0 MUD_Amount (the vehicle record's amount, 0074bcc0) */
    float mud_color[4];        /* +0x3c0 MUD_COLOR (the level's MudManager colour, 0074ba40) */
    float mud_wetness[4];      /* +0x3d0 MUD_Wetness */
} rx_podium_rider;             /* 160 bytes */

/* Each player slot's live models as the scenario left them: the riding rider (P+0x1b4 +0xa2) and the vehicle model
 * (V+0x1328 +0x14e). The original draws neither while it is clear (the podium hides the riders it replaces). */
typedef struct rx_podium_slot {
    int32_t slot, rider_drawn, vehicle_drawn, reserved;
} rx_podium_slot;              /* 16 bytes */

/* Fills what fits (any output may be NULL with max 0). Returns 0, or 1 for a bad session. Call between advances. */
RX_API int rx_session_podium(struct rx_session* s, rx_podium_info* info, rx_podium_rider* riders, uint32_t max_riders,
                             float* matrices, uint32_t max_matrices, rx_podium_slot* slots, uint32_t max_slots);
/* The static scenery models as the game draws them now (v3): every ModelEntity of the event scene (vtable 0x009370d4: ModelRSData /
 * LWModelRSData, 00401db0; kind 0) and every ScrollingTextureEntity (0x00939ba4, ScrollingTextureRSData 00410b80; kind 1: also its
 * gTextureOffset +0xaa0 / +0xaa4, which its vf19 00410dd0 advances every tick), its ModelInstance's drawn flag (entity +0xa2: Model_Submit 007f1b00 draws nothing while it is clear) and the
 * instance matrix it draws with (entity +0xc0, row vectors). A scenario script changes them: DisableEntity 0055ddc0 clears +0xa2;
 * ModelPosition 0055d4e0 places the entity at the scenario origin, sets +0xa2 / +0x70 again and, for a plain scene entity, removes it from
 * the scene and adds it back under a NEW entity id (+0x14 = the scene's next id). entity_id is therefore the id the session first saw for
 * that entity (the scene's RSData id when the caller has been calling since the load), current_id the one it has now. A renderer that builds
 * these models from the level data applies the exported state to its instance of entity_id. Writes at most max rows; returns the number of
 * ModelEntity objects in the scene (may exceed max), -1 on a bad session, 0 before the scene exists. out may be NULL (count only). */
typedef struct rx_scenario_model {
    int32_t entity_id;         /* the scene entity id as first seen (see above) */
    int32_t current_id;        /* +0x14 now */
    int32_t drawn;             /* +0xa2 */
    int32_t kind;              /* 0 ModelEntity, 1 ScrollingTextureEntity */
    float matrix[16];          /* the instance matrix (+0xc0, row vectors) */
    float texture_offset[2];   /* kind 1: the material override gTextureOffset (u, v) = +0xaa0 / +0xaa4; 0 otherwise */
    int32_t reserved[2];
    char name[64];             /* the entity's Name (+0x18) */
} rx_scenario_model;           /* 160 bytes */
RX_API int rx_session_scenario_models(struct rx_session* s, rx_scenario_model* out, uint32_t max);

/* The node flags of an ambient character (an rx_session_ambient_entities row's object_id): the record +0xc byte of each node of the
 * instance's node set (+0x210 hierarchy, records +0x218, count +0x21c), in node order (the same order as the entity's matrices).
 * Bit 0x10 = the node is hidden (SetNodeVisibility 0055dff0 sets / clears it: the bindings of a hidden node are not drawn), bit 0x01 = the
 * world matrix is valid. Writes at most max bytes; returns the node count, 0 when the object is not a current ambient character with a
 * node set, -1 on a bad session. */
RX_API int rx_session_ambient_node_flags(struct rx_session* s, uint32_t object_id, uint8_t* out, uint32_t max);

/* The sound effects the scenario scripts played (v3): SoundEffect 0055d610 hands the sound engine an FMOD event path (0067ead0,
 * "ScenarioSoundEffectEvent"): the Free Ride arrival's "SCENARIOS_FR_AL/FREERIDE/VEHICLE" (the hovercraft / sky crane / box car), the pilot's
 * "[FORMAT]_[LOC]/[FORMAT]/PILOT_AL", "SCENARIOS_FR_CH/FREERIDE/CRATE_OPEN" ... The tokens [FORMAT] / [LOC] / [PLATFORM] are left for the
 * player to resolve as the sound engine does (0067e560: the activity's format, the voice language, "PC"). position / has_position: a 3D
 * sound at that world position, else 2D. Returns the events played since the last call, oldest first (at most max; the rest stay for
 * the next call; at most 64 are kept), -1 on a bad session. The record is outside guest memory (the oracle runs the original). */
typedef struct rx_scenario_sound {
    char event[128];
    float position[3];
    int32_t has_position;
} rx_scenario_sound;           /* 144 bytes */
RX_API int rx_session_scenario_sounds(struct rx_session* s, rx_scenario_sound* out, uint32_t max);
#ifdef __cplusplus
}
#endif
#endif
