/* Original mask0x0400 versus box contacts and face intersection kernels. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_rodbox_face_x(rx_addr direction,rx_addr center,float radius,float face,float limit_a,float limit_b,rx_addr plus,rx_addr minus,rx_addr valid_plus,rx_addr valid_minus); /*0081da00 ESI,EDI; stack8 RET20*/
void phys_rodbox_face_y(rx_addr direction,rx_addr center,float radius,float face,float limit_a,float limit_b,rx_addr plus,rx_addr minus,rx_addr valid_plus,rx_addr valid_minus); /*0081dc30*/
void phys_rodbox_face_z(rx_addr direction,rx_addr center,float radius,float face,float limit_a,float limit_b,rx_addr plus,rx_addr minus,rx_addr valid_plus,rx_addr valid_minus); /*0081de70*/
void phys_rodbox_face_sum(rx_addr extent,rx_addr center,rx_addr direction,float radius,rx_addr sum,rx_addr count); /*0081e8e0 EAX,ECX; stack4 RET10*/
void phys_pair_0400_box(rx_addr qa,rx_addr pa,rx_addr manifold,rx_addr box,rx_addr qb,rx_addr pb,rx_addr radial,uint32_t flip,uint32_t* edx); /*0086b7f0 ECX,EDX; stack6 RET18*/
#ifdef __cplusplus
}
#endif
