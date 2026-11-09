/* loop_cam_replay.h -- the replay cameras (the TV director's cameras): MXReplayCamera (vtable 0x95a504, the base) and its four classes
 * MXReplayTowerCamera (0x1046, 0x95ab04, 0x430 bytes), MXZipLineCamera (0x1047, 0x95abac, 0x450), MXSplineCamera (0x1048, 0x95ac54, 0x4b0)
 * and MXAnimatedReplayCamera (0x1049, 0x95acfc, 0x450). loop_cam_replay.c.
 *
 * The scene factory (build_scene.c) makes them from the level's scene (the *_create functions: allocation + constructor); the world links the
 * scene's entities of these four types into its director list (rx_world_link_lights 00767ba0: world +0x21dd8 / +0x21ddc, links +0x310 /
 * +0x314), and ReplayCameras_Update (loop_world.c) picks the director with the smallest vf37 value ("how far the player is outside this
 * camera's activation area": 0 inside, FLT_MAX outside every area) and copies its Camera into the scene's first view.
 *
 * Core signatures follow loop_util.h: `this` first, the stack arguments in order; vf37 returns its x87 float, the RSData readers
 * (vf5 and the property functions) read the entity being loaded (rx_scene_rs()). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ---- the base MXReplayCamera ---- */
rx_addr  rx_replay_camera_construct(rx_addr cam);                               /* 006485e0 (ESI = cam) -> EAX = cam */
rx_addr  rx_replay_camera_sdtor(rx_addr cam, uint32_t flags);                   /* 006486c0 MXReplayCamera::sdtor (vf0) */
uint32_t rx_replay_camera_vf5(rx_addr cam, uint32_t reader);                    /* 006486f0 MXReplayCamera::vf5 (ret 4) */
float    rx_replay_camera_vf37(rx_addr cam);                                    /* 00648a40 -> st(0): FLT_MAX */
void     rx_replay_camera_vf28(rx_addr cam, uint32_t arg);                      /* 00648a50 (ret 4) */
void     rx_replay_camera_vf38(rx_addr cam);                                    /* 00655880: two updates of one second */

/* ---- MXReplayTowerCamera (0x1046) ---- */
rx_addr  rx_replay_tower_camera_construct(rx_addr cam);                         /* 00648cc0 (EAX = cam) -> EAX = cam */
rx_addr  rx_replay_tower_camera_create(void);                                   /* 004046a0 case 0x1046: ReflexAlignedAlloc(0x430) + 00648cc0 */
uint32_t rx_replay_tower_camera_vf5(rx_addr cam, uint32_t reader);              /* 00648e40 */
void     rx_replay_tower_camera_props(rx_addr cam);                             /* 00648f50 (EDI = cam) */
void     rx_replay_tower_camera_vf28(rx_addr cam, uint32_t arg);                /* 00648e60 (ret 4) */
void     rx_replay_activation_vf39(rx_addr cam);                                /* 00649470 (Tower and ZipLine) */
float    rx_replay_tower_camera_vf37(rx_addr cam);                              /* 00654720 (Tower and Spline) */

/* ---- MXZipLineCamera (0x1047) ---- */
rx_addr  rx_zipline_camera_construct(rx_addr cam);                              /* 006534f0 (EAX = cam) -> EAX = cam */
rx_addr  rx_zipline_camera_create(void);                                        /* case 0x1047: 0x450 bytes */
void     rx_zipline_camera_destroy(rx_addr cam);                                /* 00653670 (EAX = cam) */
rx_addr  rx_zipline_camera_sdtor(rx_addr cam, uint32_t flags);                  /* 00653650 */
uint32_t rx_zipline_camera_vf5(rx_addr cam, uint32_t reader);                   /* 006536c0 */
void     rx_zipline_camera_props(rx_addr cam);                                  /* 006537d0 (EDI = cam) */
void     rx_zipline_camera_vf28(rx_addr cam, uint32_t arg);                     /* 006536e0 (ret 4) */
float    rx_zipline_camera_vf37(rx_addr cam);                                   /* 00653aa0 */
void     rx_zipline_camera_vf19(rx_addr cam, float dt);                         /* 00653c30 (ret 4) */

/* ---- MXSplineCamera (0x1048) ---- */
rx_addr  rx_spline_camera_construct(rx_addr cam);                               /* 00653f60 (EAX = cam) -> EAX = cam */
rx_addr  rx_spline_camera_create(void);                                         /* case 0x1048: 0x4b0 bytes */
void     rx_spline_camera_destroy(rx_addr cam);                                 /* 006541e0 (EAX = cam) */
rx_addr  rx_spline_camera_sdtor(rx_addr cam, uint32_t flags);                   /* 006541c0 */
uint32_t rx_spline_camera_vf5(rx_addr cam, uint32_t reader);                    /* 00654270 */
void     rx_spline_camera_props(rx_addr cam);                                   /* 00654380 (EDI = cam) */
void     rx_spline_camera_vf28(rx_addr cam, uint32_t arg);                      /* 00654290 (ret 4) */
void     rx_spline_camera_vf19(rx_addr cam, float dt);                          /* 00654870 (ret 4) */
void     rx_spline_camera_vf39(rx_addr cam);                                    /* 00654c90 */
void     rx_spline_curve_at(rx_addr cam, rx_addr out, double u);                /* 00655050 (ECX = cam, EAX = out vec3, xmm0 = u) */

/* ---- MXAnimatedReplayCamera (0x1049) ---- */
rx_addr  rx_animated_replay_camera_construct(rx_addr cam);                      /* 00655200 (EAX = cam) -> EAX = cam */
rx_addr  rx_animated_replay_camera_create(void);                                /* case 0x1049: 0x450 bytes */
void     rx_animated_replay_camera_destroy(rx_addr cam);                        /* 00655360 (EAX = cam) */
rx_addr  rx_animated_replay_camera_sdtor(rx_addr cam, uint32_t flags);          /* 00655340 */
uint32_t rx_animated_replay_camera_vf5(rx_addr cam, uint32_t reader);           /* 006553d0 */
void     rx_animated_replay_camera_props(rx_addr cam);                          /* 006554e0 (EDI = cam) */
void     rx_animated_replay_camera_vf28(rx_addr cam, uint32_t arg);             /* 006553f0 (ret 4) */
float    rx_animated_replay_camera_vf37(rx_addr cam);                           /* 00655730 */
void     rx_animated_replay_camera_vf19(rx_addr cam, float dt);                 /* 006558b0 (ret 4) */
void     rx_animated_replay_camera_set_time(rx_addr cam, float t);              /* 00655b40 (ESI = cam; stack t) */
void     rx_animated_replay_camera_vf39(rx_addr cam);                           /* 00655c30 */

/* ---- shared ---- */
uint32_t rx_point_in_triangle_prism(rx_addr a, rx_addr b, rx_addr c, rx_addr n, rx_addr p);   /* 0063d060 (stack a; EAX = b, EDX = c, ESI = n, EDI = p) -> EAX */
#ifdef __cplusplus
}
#endif
