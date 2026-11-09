/* loop_cam_build.h -- the player camera entities' construction and tuning apply (0054bc00 and the camera class
 * constructors / vf32 of loop_cam_build.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
struct fmt_vfs;

/* 0063f240 (once, at the original's init-thread place): the six camera tuning bxml files into the class records at
 * 0x00af6d10 / 0x00b14790 / 0x00b4fc90 / 0x00b8b190 / 0x00bc6690 / 0x00be4110 through the parser 0065c200. The original
 * also re-applies vf32 to the cameras alive at that moment; the product has none yet (its cameras are made at the spawn,
 * after this). Returns 1 when the MX file loaded. */
int  rx_camera_tuning_load_all(const struct fmt_vfs* vfs);
/* 0054bc00 (EAX = the player, after the spawn): the player's five view cameras P + 0x974 .. + 0x984 (0063f160), the
 * start-gate camera P + 0x99c (00647cf0) and the quick camera P + 0x998 (00646d70), each inserted into the vehicle's
 * scene (0063dae0) and registered with the camera sound-effects manager (0068cd90, an audio boundary: product skips).
 * The first-person camera (slot 2, P+0x97c) is made here too (type 0 of the config table); its per-frame update is vf19
 * 00642680 (loop_cam_firstperson.c). The MX camera path selects the GamePlay third-person camera by default. */
void rx_player_cameras_create(rx_addr p);

/* the camera class constructors / methods (addresses in the function comments) */
rx_addr rx_camera_factory(rx_addr desc, rx_addr p);                 /* 0063f160 */
rx_addr rx_player_camera_construct(rx_addr cam, rx_addr p);         /* 00646730 */
rx_addr rx_first_person_camera_construct(rx_addr cam, rx_addr p, rx_addr desc);  /* 00641280 (MXFirstPersonCamera) */
rx_addr rx_third_person_camera_construct(rx_addr cam, rx_addr p, rx_addr desc);   /* 00649610 */
rx_addr rx_start_gate_camera_construct(rx_addr cam, rx_addr p);     /* 00647cf0 */
rx_addr rx_quick_camera_construct(rx_addr cam, rx_addr p);          /* 00646d70 */
void    rx_player_camera_vf32(rx_addr cam);                         /* 00646b70 */
void    rx_third_person_camera_vf32(rx_addr cam);                   /* 0064a330 */
void    rx_quick_camera_vf32(rx_addr cam);                          /* 00647700 */
void    rx_camera_entity_vf28(rx_addr cam);                         /* 0063de60 */
void    rx_third_person_camera_vf28(rx_addr cam);                   /* 00649bd0 */
void    rx_start_gate_camera_vf28(rx_addr cam);                     /* 00647e00 */
void    rx_start_gate_camera_place(rx_addr cam);                    /* 00647e40 */
void    rx_third_person_camera_vf35(rx_addr cam);                   /* 006534e0 */
void    rx_player_camera_vf36(rx_addr cam);                         /* 0063f110 */
void    rx_cam_pose_row(uint32_t index, rx_addr src, rx_addr rows); /* 00407410 (a 4x4's row: x87 copy of vec3) */
#ifdef __cplusplus
}
#endif
