/* build_vehicle_int.h -- the simulation helpers construction unit 6's vehicle classes share (game/build_vehicle_sim.c; internal to
 * game/build_vehicle*.c). Conventions of the originals in the definitions' comments. */
#pragma once
#include "rx_game.h"

typedef struct { uint16_t off; uint8_t kind; uint32_t name; uint32_t v[4]; } veh_param;   /* a parameter record (004392f0 style) */
void     rx_veh_params_register(rx_addr P, const veh_param* t, uint32_t n);              /* the inlined registrations */
uint32_t rx_veh_params_apply_typed(rx_addr P, rx_addr R, rx_addr type, rx_addr reject);  /* 0043a560 / 0048b680 */
void     rx_veh_read_curve(rx_addr R, rx_addr node, rx_addr C);                          /* 0048bd20 */
void     rx_veh_mechanism_bind(rx_addr M);                                               /* 0085c3f0 */
void     rx_veh_mechanism_set_mass(rx_addr M, float mass, uint32_t body);                /* 0085c470 */
void     rx_veh_main_body_node_construct(rx_addr n);                                     /* 0042f520 (EDI) */
void     rx_veh_fixed_body_node_construct(rx_addr n);                                    /* 0042fae0 (ESI) */
void     rx_veh_sliding_rotor_node_construct(rx_addr n);                                 /* 0042f640 (ESI) */
void     rx_veh_wheel_record_init(rx_addr W, uint32_t id);                               /* 0042d9b0 */
void     rx_veh_quat_rec_init(rx_addr r);                                                /* 0042de50 */
void     rx_veh_rec_init(rx_addr r);                                                     /* 004c0230 (stdcall; RET 4): the 0x100 record (ATV, UTV) */
