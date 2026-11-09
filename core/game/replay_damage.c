#include "replay_damage.h"
#include "build_activity_util.h"
#include "anim.h"
#include "world_node.h"
#include "world_veh.h"
#include "world.h"
#include "loop_platform.h"

void rx_replay_physics_entity_deactivate(rx_addr entity) {
    ACT_FRAME(f,0,1);
    rx_addr handle=U32(entity,0x158);
    if(handle) {
        rx_addr world=U32(handle,4),body;
        ACT_CALLR(body,0,rx_world_physics_handle_body(handle));
        ACT_CALL(1,rx_world_kinematic_deactivate(body,world));
        ACT_CALLR(body,0,rx_world_physics_handle_body(U32(entity,0x158)));
        SU8(body,0xb0,0);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0041ce60,rx_replay_physics_entity_deactivate)

void rx_replay_scene_add_entity(rx_addr entity,rx_addr scene) {
    ACT_FRAME(f,0,2);
    loop_enter_cs(scene+4); /* platform bridge owns the real stdcall depth */
    uint32_t sequence=U32(scene,0x19f4);
    SU32(scene,0x19f4,sequence+1);SU32(entity,0x14,sequence);
    loop_leave_cs(scene+4);
    ACT_CALL(1,rx_scene_manager_add_entity(entity,scene));
    ACT_END(f);
}
RX_FUNCTION(0x007d2f40,rx_replay_scene_add_entity)

void rx_replay_damage_deactivate(rx_addr entry,uint32_t index) {
    ACT_FRAME(f,0,2);
    if(U32(entry,0x370+4*index)&&U8(entry,0x3d0+index)) {
        rx_addr entity=U32(entry,0x2c+4*index);
        if(entity && U32(entity,0x10)) {
            ACT_CALL(1,rx_scene_manager_remove_entity(entity,U32(entity,0x10)));
            SU32(entity,0x10,0);
            ACT_CALL(0,rx_replay_physics_entity_deactivate(entity));
        }
        SU8(entry,0x3d0+index,0);
    }
    ACT_END(f);
}
RX_FUNCTION(0x0047a830,rx_replay_damage_deactivate)

void rx_replay_damage_reset_rotation(rx_addr entry,uint32_t index) {
    ACT_FRAME(f,0,1);
    if(U32(entry,0x370+4*index)) {
        rx_addr source=entry+(index+0x13)*16,dest=entry+(index+0x1b)*16;
        rx_wr64(dest,rx_rd64(source));rx_wr64(dest+8,rx_rd64(source+8));
        ACT_CALL(1,rx_node_set_local_rotation(U32(entry,0x370+4*index),dest));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0047afb0,rx_replay_damage_reset_rotation)

void rx_replay_damage_reset_motion(rx_addr entry,uint32_t index) {
    ACT_FRAME(f,4,4);
    rx_addr node=U32(entry,0x370+index*4);
    if(node) {
        SU32(entry,0x230+index*4,0);SU32(entry,0x250+index*4,0);
        ACT_CALL(0,rx_world_node_get_world_position(node,entry+0x270+index*32));
        ACT_CALL(0,rx_world_node_get_world_position(U32(entry,0x370+index*4),entry+(index+0x14)*32));
        rx_addr source=entry+(index+0x13)*16,dest=entry+(index+0x1b)*16;
        rx_wr64(dest,rx_rd64(source));rx_wr64(dest+8,rx_rd64(source+8));
    }
    ACT_END(f);
}
RX_FUNCTION(0x0047af30,rx_replay_damage_reset_motion)

static void replay_damage_visibility(rx_addr entry,rx_addr name,int hidden) {
    rx_addr hierarchy=U32(U32(entry,0x3fc),0x1328)+0x10;
    uint32_t exists;
    ACT_CALLR(exists,1,rx_hierarchy_has_node(hierarchy,name));
    if(exists) {
        rx_addr node;
        ACT_CALLR(node,1,rx_hierarchy_find_node(hierarchy,name));
        SU8(node,0xc,hidden ? U8(node,0xc)|0x10 : U8(node,0xc)&0xef);
    }
}
void rx_replay_damage_apply(rx_addr entry,uint32_t index,uint32_t state) {
    ACT_FRAME(f,4,4);
    if(U32(entry,0x3fc)) {
        SU32(entry,0xc+4*index,state);
        rx_addr names=U32(entry,4)+index*0x350;
        switch(state) {
            case 0:
                replay_damage_visibility(entry,names,0);
                replay_damage_visibility(entry,names+0x104,1);
                ACT_CALL(1,rx_replay_damage_deactivate(entry,index));
                ACT_CALL(0,rx_replay_damage_reset_rotation(entry,index));
                break;
            case 1:
                replay_damage_visibility(entry,names+0x104,0);
                replay_damage_visibility(entry,names,1);
                ACT_CALL(1,rx_replay_damage_deactivate(entry,index));
                ACT_CALL(0,rx_replay_damage_reset_rotation(entry,index));
                break;
            case 2:
                replay_damage_visibility(entry,names+0x104,0);
                replay_damage_visibility(entry,names,1);
                ACT_CALL(1,rx_replay_damage_deactivate(entry,index));
                ACT_CALL(2,rx_replay_damage_reset_motion(entry,index));
                break;
            case 3:
                replay_damage_visibility(entry,names+0x104,1);
                replay_damage_visibility(entry,names,1);
                ACT_CALL(1,rx_replay_damage_activate(entry,index));
                break;
            case 4:
                replay_damage_visibility(entry,names+0x104,1);
                replay_damage_visibility(entry,names,1);
                ACT_CALL(0,rx_replay_damage_reset_rotation(entry,index));
                ACT_CALL(1,rx_replay_damage_deactivate(entry,index));
                break;
            default:break;
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x00479ab0,rx_replay_damage_apply)

void rx_replay_damage_restore(rx_addr vehicle,rx_addr manager,rx_addr values) {
    ACT_FRAME(f,4,3);
    for(uint32_t slot=0;slot!=12;++slot) {
        if(vehicle!=U32(manager,0x40c+slot*0x410))continue;
        rx_addr entry=manager+0x10+slot*0x410;
        for(uint32_t index=0;index<U32(entry,0);++index)
            ACT_CALL(2,rx_replay_damage_apply(entry,index,U32(values,index*4)));
        break;
    }
    ACT_END(f);
}
RX_FUNCTION(0x0047bdd0,rx_replay_damage_restore)

#include "phys_vec.h"
#include "phys_crt.h"
static void replay_damage_vcall_out(rx_addr object,uint32_t offset,rx_addr out) {
    rx_addr target=U32(U32(object,0),offset);
    if(!rx_fn(target))act_unreached(target,"replay damage typed output owner");
    ACT_CALL(1,((void(*)(rx_addr,rx_addr))rx_fn(target))(object,out));
}
void rx_replay_damage_activate(rx_addr entry,uint32_t index) {
    ACT_FRAME_EBP(f,16,0x64,3);
    SU32(f,0x6c,U32(0,0x009b45e4)^(f+12));
    SU32(f,0x1c,index);
    if(U32(entry,0x370+index*4) && !U8(entry,0x3d0+index)) {
        rx_addr entity=U32(entry,0x2c+index*4);
        SU32(f,0x14,entity);
        if(entity) {
            rx_addr hierarchy=U32(U32(entry,0x3fc),0x1328)+0x10;
            rx_addr root_record=U32(hierarchy,0);
            rx_addr matrix=U32(U32(root_record,0),0x18)+(uint32_t)U8(root_record,0xb)*64;
            rx_addr node;
            ACT_CALLR(node,1,rx_hierarchy_find_node(hierarchy,U32(entry,4)+index*0x350+0x104));
            SU32(f,0x18,node);
            if(node) {
                ACT_CALL(1,rx_world_node_evaluate_world(node,0));
                matrix=U32(U32(node,0),0x1c)+(uint32_t)U8(node,0xb)*64;
            }
            replay_damage_vcall_out(U32(f,0x14),0x40,matrix);
            entity=U32(f,0x14);
            rx_addr embedded=entity+0x90,target=U32(U32(embedded,0),0x38);
            if(!rx_fn(target))act_unreached(target,"replay damage physics matrix getter");
            ACT_CALL(0,((rx_addr(*)(rx_addr))rx_fn(target))(embedded));
            rx_addr vehicle=U32(entry,0x3fc),world=U32(vehicle,0x188);
            rx_addr scene=world?U32(world,0x20138):0;
            SU32(entity,0x10,scene);
            ACT_CALL(0,rx_replay_scene_add_entity(entity,scene));
            root_record=U32(hierarchy,0);
            rx_addr root_matrix=U32(U32(root_record,0),0x18)+(uint32_t)U8(root_record,0xb)*64;
            rx_st4(f+0x30,rx_ld4(root_matrix+0x30));
            ACT_CALL(0,rx_world_node_get_world_position(U32(f,0x18),f+0x20));
            rx_v4 offset=v4_sub(rx_ld4(f+0x20),rx_ld4(f+0x30));
            rx_st4(f+0x30,v4_scale(phys_normalize3(offset),F32(0,0x00935aac)));
            replay_damage_vcall_out(U32(entry,0x3fc),0x94,f+0x40);
            replay_damage_vcall_out(U32(entry,0x3fc),0x98,f+0x50);
            entity=U32(f,0x14);
            rx_addr handle=U32(entity,0x158);
            if(handle) {
                world=U32(handle,4);
                rx_addr body;
                /* Original pushes velocity before resolving the handle; retain
                 * that argument while the register-only leaf runs. */
                rx_set_sp(rx_sp()-4);
                ACT_CALLR(body,0,rx_world_physics_handle_body(handle));
                rx_set_sp(rx_sp()+4);
                ACT_CALL(2,rx_world_kinematic_activate(body,world,f+0x30));
            }
            index=U32(f,0x1c);
            SU32(entry,0x3d8+index*4,U32(0,0x009cd95c));
            SU8(entry,0x3d0+index,1);
        }
    }
    rx_set_sp(f+12);
    ACT_CALL(0,phys_security_check_cookie(U32(f,0x6c)^(f+12)));
    ACT_END(f);
}
RX_FUNCTION(0x0047a630,rx_replay_damage_activate)
