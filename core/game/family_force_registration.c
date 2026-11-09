/* Explicit original thiscall input mapping, unique registrations only. */
#include "fourwheel_force_stage.h"
#include "fx_util.h"
#include "rx_fn.h"
void rx_utv_force_stage_native_entry(rx_addr self,float dt){fx_set_reg(FX_ECX,self);rx_utv_force_stage_exact(self,dt);}
void rx_buggy_force_stage_native_entry(rx_addr self,float dt){fx_set_reg(FX_ECX,self);rx_buggy_force_stage_exact(self,dt);}
void rx_truck_force_stage_native_entry(rx_addr self,float dt){fx_set_reg(FX_ECX,self);rx_truck_force_stage_exact(self,dt);}
RX_FUNCTION(0x004cdaa0,rx_utv_force_stage_native_entry)
RX_FUNCTION(0x004b32a0,rx_buggy_force_stage_native_entry)
RX_FUNCTION(0x00450e20,rx_truck_force_stage_native_entry)
