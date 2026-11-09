/* loop_cam_entity.h -- the Camera base and the MXCameraEntity shared code (loop_cam_entity.c). Register arguments first (named in
 * each comment), then the stack arguments in order. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void    rx_cam_set_basis(rx_addr c, float fx, float fy, float fz, float ux, float uy, float uz);          /* 00790ce0 (ESI, xmm0) */
void    rx_cam_set_render_basis(rx_addr c, float fx, float fy, float fz, float ux, float uy, float uz);   /* 00790f40 (ESI, xmm0) */
void    rx_cam_set_render_basis_from(rx_addr c, rx_addr up, rx_addr fwd);          /* 007911b0 (ESI, EAX, ECX) */
void    rx_cam_set_fov(rx_addr c, float fov);                                      /* 007911e0 (EAX, xmm0) */
void    rx_cam_get_position(rx_addr c, rx_addr out);                               /* 00792330 (ECX, EAX) */
void    rx_cam_get_right(rx_addr c, rx_addr out);                                  /* 007923a0 (ECX, EAX) */
void    rx_cam_get_forward(rx_addr c, rx_addr out);                                /* 00792380 (ECX, EAX) */
void    rx_camera_vf0(rx_addr c, rx_addr out);                                     /* 00792350 Camera::vf0 */
void    rx_cam_set_render_position(rx_addr c, rx_addr v);                          /* 00790cb0 (ECX, EAX) */
void    rx_camera_copy_block(rx_addr src, rx_addr dst);                            /* 0063ddd0 (ECX, EAX) */
rx_addr rx_camera_copy(rx_addr out, rx_addr src);                                  /* 0063dc30 (stdcall) */
void    rx_camera_assign(rx_addr src, rx_addr dst);                                /* 00790b30 (EDI, ESI) */
void    rx_cam_entity_to_view(rx_addr e, rx_addr view);                            /* 0063dba0 (stdcall) */
void    rx_cam_entity_invalidate(rx_addr e);                                       /* 0063de70 MXCameraEntity::vf29 */
void    rx_cam_entity_record(rx_addr e);                                           /* 0063de80 MXCameraEntity::vf31 */
void    rx_cam_entity_set_pose(rx_addr e, rx_addr pose);                           /* 0063dee0 (stdcall) */
void    rx_cam_entity_look_at(rx_addr from, rx_addr to, rx_addr e);                /* 0063df90 (ECX, EAX; stack e) */
void    rx_cam_entity_vf19(rx_addr e, float dt);                                   /* 0063e310 MXCameraEntity::vf19 */
#ifdef __cplusplus
}
#endif
