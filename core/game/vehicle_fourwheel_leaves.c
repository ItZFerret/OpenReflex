/* Buggy and TrophyTruck virtual state accessors. The vector getters copy all
 * four lanes bitwise; x87 RPM multiplication uses the main thread's 53-bit CW. */
#include "vehicle_fourwheel_leaves.h"
#include "rx_game.h"
static rx_addr copy_vector(rx_addr v,rx_addr out,unsigned simoff,unsigned field){rx_st4(out,rx_ld4(U32(v,simoff)+field));return out;}
static rx_f80 engine_rpm(rx_addr v,unsigned simoff,unsigned field){return rx_f80_mul(rx_f80_from_f32(F32(U32(v,simoff),field)),rx_f80_from_f32(F32(0,0x9774d0)),53,0);}
static uint32_t all_contact(rx_addr s,unsigned field){return U32(s,field)&&U32(s,field+0x150)&&U32(s,field+0x2a0)&&U32(s,field+0x3f0);}
rx_addr rx_buggy_position(rx_addr v,rx_addr out){return copy_vector(v,out,0x15c0,0x180);}
RX_FUNCTION(0x004ac560,rx_buggy_position)
rx_addr rx_buggy_rotation(rx_addr v,rx_addr out){return copy_vector(v,out,0x15c0,0x7c0);}
RX_FUNCTION(0x004ac580,rx_buggy_rotation)
rx_addr rx_buggy_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15c0,0x100);}
RX_FUNCTION(0x004ac5a0,rx_buggy_velocity)
rx_addr rx_buggy_angular_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15c0,0x740);}
RX_FUNCTION(0x004ac5c0,rx_buggy_angular_velocity)
rx_addr rx_buggy_body_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15c0,0x160);}
RX_FUNCTION(0x004ac5e0,rx_buggy_body_velocity)
rx_addr rx_buggy_body_angular_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15c0,0x7a0);}
RX_FUNCTION(0x004ac600,rx_buggy_body_angular_velocity)
rx_f80 rx_buggy_engine_rpm(rx_addr v){return engine_rpm(v,0x15c0,0x864c);}
RX_FUNCTION(0x004ac620,rx_buggy_engine_rpm)
rx_f80 rx_buggy_engine_limit(rx_addr v){return rx_f80_from_f32(F32(U32(v,0x15c0),0x4d4c));}
RX_FUNCTION(0x004ac640,rx_buggy_engine_limit)
uint32_t rx_buggy_gear(rx_addr v){return U32(U32(v,0x15c0),0x8650)+1u;}
RX_FUNCTION(0x004ac650,rx_buggy_gear)
uint32_t rx_buggy_contact_flag(rx_addr v){return U32(U32(v,0x15c0),0x86f0);}
RX_FUNCTION(0x004ac660,rx_buggy_contact_flag)
uint32_t rx_buggy_contact_state(rx_addr v){return U32(U32(v,0x15c0),0x86f8);}
RX_FUNCTION(0x004ac670,rx_buggy_contact_state)
uint32_t rx_buggy_all_wheels_contact(rx_addr v){return all_contact(U32(v,0x15c0),0x8170);}
RX_FUNCTION(0x004ac680,rx_buggy_all_wheels_contact)
uint32_t rx_buggy_mode_two(rx_addr v){return U32(U32(v,0x15c0),0x56ec)==2;}
RX_FUNCTION(0x004ac6e0,rx_buggy_mode_two)
rx_addr rx_truck_position(rx_addr v,rx_addr out){return copy_vector(v,out,0x15d0,0x180);}
RX_FUNCTION(0x00473490,rx_truck_position)
rx_addr rx_truck_rotation(rx_addr v,rx_addr out){return copy_vector(v,out,0x15d0,0x7c0);}
RX_FUNCTION(0x004734b0,rx_truck_rotation)
rx_addr rx_truck_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15d0,0x100);}
RX_FUNCTION(0x004734d0,rx_truck_velocity)
rx_addr rx_truck_angular_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15d0,0x740);}
RX_FUNCTION(0x004734f0,rx_truck_angular_velocity)
rx_addr rx_truck_body_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15d0,0x160);}
RX_FUNCTION(0x00473510,rx_truck_body_velocity)
rx_addr rx_truck_body_angular_velocity(rx_addr v,rx_addr out){return copy_vector(v,out,0x15d0,0x7a0);}
RX_FUNCTION(0x00473530,rx_truck_body_angular_velocity)
rx_f80 rx_truck_engine_rpm(rx_addr v){return engine_rpm(v,0x15d0,0x865c);}
RX_FUNCTION(0x00473550,rx_truck_engine_rpm)
rx_f80 rx_truck_engine_limit(rx_addr v){return rx_f80_from_f32(F32(U32(v,0x15d0),0x4d4c));}
RX_FUNCTION(0x00473570,rx_truck_engine_limit)
uint32_t rx_truck_gear(rx_addr v){return U32(U32(v,0x15d0),0x8664)+1u;}
RX_FUNCTION(0x00473580,rx_truck_gear)
uint32_t rx_truck_contact_flag(rx_addr v){return U32(U32(v,0x15d0),0x8700);}
RX_FUNCTION(0x00473590,rx_truck_contact_flag)
uint32_t rx_truck_contact_state(rx_addr v){return U32(U32(v,0x15d0),0x8708);}
RX_FUNCTION(0x004735a0,rx_truck_contact_state)
uint32_t rx_truck_all_wheels_contact(rx_addr v){return all_contact(U32(v,0x15d0),0x8180);}
RX_FUNCTION(0x004735b0,rx_truck_all_wheels_contact)
uint32_t rx_truck_mode_two(rx_addr v){return U32(U32(v,0x15d0),0x56fc)==2;}
RX_FUNCTION(0x00473610,rx_truck_mode_two)
