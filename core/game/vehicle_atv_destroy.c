/* Original ATV simulation destruction: exact owned-buffer and sparse vtable
 * cleanup from 0048cfc0, with its scalar deleting wrapper and direct leaves. */
#include "vehicle_atv_destroy.h"
#include "world_veh_util.h"
#include "build_alloc.h"
#include "build_rider.h"
#include "build_vehicle.h"
#include "loop_vehicle.h"
#include "../src/rx_os.h"
/* Exact ATV simulation teardown closure. Fixed-offset lists are the original
 * destructor MOVs, not inferred ownership or array-wide cleanup. */
void rx_vehicle_wheel_block_destruct(rx_addr a){
 static const uint16_t off[]={0x910,0x8d0,0x890,0x850,0x810,0x800,0x770,0x730,0x720,0x6d0,0x680,0x670,0x620,0x5d0,0x580,0x530,0x520,0x4e0,0x4a0,0x490,0x450,0x410,0x3d0,0x390,0x380,0x340,0x300,0x2c0,0x280,0x270,0x1e0,0x190,0x180,0x130,0xe0,0xd0};
 for(unsigned i=0;i<sizeof off/sizeof off[0];i++)SU32(a,off[i],0x93c9e0);SU32(a,0,0x93cb44);
}
void rx_vehicle_chassis_block_destruct(rx_addr a){
 static const uint16_t off[]={0xdd0,0xd90,0xd50,0xd10,0xd00,0xcc0,0xc80,0xc40,0xc00,0xbf0,0xb50,0xb10,0xad0,0xa90,0xa80,0xa40,0xa00,0x9c0,0x980,0x970,0x930,0x8f0,0x8b0,0x870,0x860,0x820,0x7e0,0x7a0,0x760,0x750,0x710,0x6d0,0x690,0x650,0x640,0x600,0x5c0,0x580,0x540,0x530,0x4f0,0x4b0,0x470,0x430,0x420,0x3e0,0x3a0,0x360,0x320,0x310,0x270,0x230,0x1f0,0x1b0,0x1a0,0x160,0x120,0xe0,0xa0,0x90};
 for(unsigned i=0;i<sizeof off/sizeof off[0];i++)SU32(a,off[i],0x93c9e0);SU32(a,0,0x93cb44);
}
void rx_vehicle_mechanism_destruct(rx_addr a){
 static const uint16_t off[]={0x11f0,0x11b0,0x1170,0x1130,0x1120,0x10e0,0x10a0,0x1090,0x1040,0xff0,0xfa0,0xf90,0xf50};
 for(unsigned i=0;i<sizeof off/sizeof off[0];i++)SU32(a,off[i],0x93c9e0);
}
void rx_vehicle_sim_vector_destruct(rx_addr a){
 world_frame frame=world_frame_flat(1,0);
 if(U32(a,0xc))WORLD_CALL_SP(4,(void)rx_std_allocator_free(0,U32(a,0xc)));
 SU32(a,0xc,0);SU32(a,0x10,0);SU32(a,0x14,0);const rx_addr proxy=U32(a,0);
 if(U8(0,0xda67d8))RX_OS(0x91600c,0xd61704);WORLD_CALL_SP(4,rx_crt_aligned_free(proxy));
 const uint32_t locked=U8(0,0xda67d8);world_frame_leave(&frame);if(locked)RX_OS(0x9160f8,0xd61704);
}
void rx_atv_sim_destruct(rx_addr s){
 world_frame frame=world_frame_flat(1,0);SU32(s,0,0x93d054);
 static const uint16_t arrays[]={0x9a38,0x9a3c,0x9a40,0x9a2c,0x9a30,0x9a34,0x9a20,0x9a24,0x9a28,0x9a14,0x9a18,0x9a1c};
 for(unsigned i=0;i<12;i++){rx_set_sp(rx_sp()-4);SU32(rx_sp(),0,U32(s,arrays[i]));WORLD_CALL_SP(0,rx_reflex_aligned_free(U32(s,arrays[i])));}
 rx_set_sp(rx_sp()+0x30);SU32(s,0x9100,0x976750);WORLD_CALL_SP(0,rx_registry_remove(s+0x9100));
 static const uint16_t wheel[]={0x8780,0x7e20,0x74c0,0x6b60};for(unsigned i=0;i<4;i++)WORLD_CALL_SP(0,rx_vehicle_wheel_block_destruct(s+wheel[i]));
 WORLD_CALL_SP(0,rx_vehicle_chassis_block_destruct(s+0x5d50));
 static const uint16_t curve[]={0x5cc0,0x5c80,0x5c40,0x5c00,0x5bc0,0x5bb0,0x5b70,0x5b30,0x5af0,0x5ab0,0x5aa0};
 for(unsigned i=0;i<11;i++)SU32(s,curve[i],0x93c9e0);SU32(s,0x5a30,0x93cb44);
 if(!(U32(0,0xd72ad4)&1)){SU32(0,0xd72ad4,U32(0,0xd72ad4)|1);WORLD_CALL_SP(0,rx_veh_tuning_registry_init());WORLD_ORIG(0x8f9fbf,RX_CDECL,0,0x911ed0);}
 SU32(0,0xd92cd4,0);SU32(s,0,0x93cc14);WORLD_CALL_SP(0,rx_registry_remove(s+0x20d0));
 WORLD_CALL_SP(0,rx_vehicle_sim_vector_destruct(s+0x1300));WORLD_CALL_SP(0,rx_vehicle_mechanism_destruct(s+0xd0));SU32(s,0,0x93c9d0);world_frame_leave(&frame);
}
rx_addr rx_atv_sim_delete(rx_addr s,uint32_t flags){
 world_frame frame=world_frame_flat(1,0);WORLD_CALL_SP(0,rx_atv_sim_destruct(s));if(flags&1)WORLD_CALL_SP(4,rx_reflex_aligned_free(s));world_frame_leave(&frame);return s;
}

RX_FUNCTION(0x0042f9f0,rx_vehicle_wheel_block_destruct)
RX_FUNCTION(0x0042fd50,rx_vehicle_chassis_block_destruct)
RX_FUNCTION(0x0085c390,rx_vehicle_mechanism_destruct)
RX_FUNCTION(0x0081f6e0,rx_vehicle_sim_vector_destruct)
RX_FUNCTION(0x0048cfc0,rx_atv_sim_destruct)
RX_FUNCTION(0x0048c730,rx_atv_sim_delete)
