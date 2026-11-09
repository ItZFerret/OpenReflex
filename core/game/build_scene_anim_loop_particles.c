/* Scene0x1058 AnimEntitySingleAnimLoopWithParticles. Original allocation0xa60
 * (004052a7), ctor006ba2e0 and vf5/vf7/vf9/vf19. Source-reviewed decompile and
 * disassembly; no animation/emitter update or missing asset is suppressed. */
#include "build_scene_int.h"
#include "build_rider.h"
#include "anim.h"
#include "loop_entity.h"
#include "fx.h"
#include "fx_bind.h"

uint32_t rx_anim_single_loop_vf5(rx_addr e, uint32_t reader);
rx_addr rx_attachable_ring_construct(rx_addr e, rx_addr node);
rx_addr rx_attachable_sphere_construct(rx_addr e, rx_addr node);

/* 006ba2e0: the006b9ad0 base constructor, then the derived vtables. */
static rx_addr loop_particles_construct(rx_addr e) {
    rx_character_entity_construct(e);
    SU32(e,0,0x0095f674); SU32(e,0x90,0x0095f6f4); SU32(e,0x140,0x0095f734);
    for(uint32_t i=0;i<0x23;i++) SU32(e,0x36c+0x1c*i+0x18,0);
    SU32(e,0x740,1); SU32(e,0x744,0);
    SU32(e,0,0x0095f73c); SU32(e,0x90,0x0095f7bc); SU32(e,0x140,0x0095f7fc);
    SU32(e,0x750,0); return e;
}
RX_FUNCTION(0x006ba2e0,loop_particles_construct)
rx_addr rx_anim_single_loop_particles_create(void) {
    rx_addr e=rx_reflex_aligned_alloc(0xa60);
    return e?loop_particles_construct(e):0;
}
static void string_to(rx_addr dest,const char* text) {
    uint32_t i=0;
    for(;i<0x103&&text&&text[i];i++) SU8(dest,i,(uint8_t)text[i]);
    SU8(dest,i,0);
}
/*006ba310 reads configuration; emitter attachment is delayed until vf7. */
static uint32_t loop_particles_load(rx_addr e,uint32_t reader) {
    rx_anim_single_loop_vf5(e,reader);
    string_to(e+0x754,rs_str("HierarchyNode"));
    string_to(e+0x858,rs_str("EmitterName"));
    string_to(e+0x95c,rs_enum_text("EmitterType"));
    return 0;
}
RX_FUNCTION(0x006ba310,loop_particles_load)
static int type_is(rx_addr text,const char* expected) {
    uint32_t i=0;
    while(expected[i]&&U8(text,i)==(uint8_t)expected[i])i++;
    return U8(text,i)==(uint8_t)expected[i];
}
/*006ba370: look up hierarchy node and the entity scene's particle effect set. */
static void loop_particles_attach(rx_addr e) {
    const uint32_t arg=e+0x754;
    const rx_addr hierarchy=scene_vcall(e,0x70,1,&arg);
    const rx_addr node=rx_hierarchy_find_node(hierarchy,e+0x754);
    const rx_addr set=U32(0,0x00a94264+4*U32(U32(e,0x10),0x2cc4));
    if(!set||!node)return;
    const rx_addr type=e+0x95c;
    rx_addr effect;
    if(type_is(type,"Box")) {
        effect=rx_reflex_aligned_alloc(0x110);
        if(effect)effect=rx_rider_lens_effect_construct(effect,node);
    } else if(type_is(type,"Ring")) {
        effect=rx_reflex_aligned_alloc(0x110);
        if(effect)effect=rx_attachable_ring_construct(effect,node);
    } else if(type_is(type,"Sphere")) {
        effect=rx_reflex_aligned_alloc(0x110);
        if(effect)effect=rx_attachable_sphere_construct(effect,node);
    } else return;
    SU32(e,0x750,effect);
    rx_fx_effect_bind(effect,set,e+0x858);
}
RX_FUNCTION(0x006ba370,loop_particles_attach)
/*006c3a60: release the binding, clearing the two original handle fields. */
static void attachable_unbind(rx_addr e) {
    const rx_addr set=U32(e,4);
    if(set){const uint32_t arg=e+0x80;scene_vcall(set,0x14,1,&arg);}
    SU32(e,4,0);SU32(e,8,0);
}
RX_FUNCTION(0x006c3a60,attachable_unbind)
static rx_addr attachable_delete(rx_addr e,uint32_t flags,int kind) {
    SU32(e,0,kind==1?0x0095e0c0:kind==2?0x0095e0f8:0x009588ec);
    const rx_addr set=U32(e,4);
    if(set){const uint32_t arg=e+0x80;scene_vcall(set,0x14,1,&arg);}
    SU32(e,0xd0,0x0095df34);
    if(kind==1)SU32(e,0x10,0x0095e074);
    SU32(e,0x94,0xdeaddead);SU32(e,0,0x009587fc);
    if(flags&1)rx_reflex_aligned_free(e);
    return e;
}
static rx_addr attachable_box_delete(rx_addr e,uint32_t flags){return attachable_delete(e,flags,0);}
static rx_addr attachable_ring_delete(rx_addr e,uint32_t flags){return attachable_delete(e,flags,1);}
static rx_addr attachable_sphere_delete(rx_addr e,uint32_t flags){return attachable_delete(e,flags,2);}
RX_FUNCTION(0x00696420,attachable_box_delete)
RX_FUNCTION(0x006c4e10,attachable_ring_delete)
RX_FUNCTION(0x006c4e70,attachable_sphere_delete)
/*006ba4d0: animation and model release, then unbind/delete owned emitter. */
static void loop_particles_unload(rx_addr e) {
    rx_character_anim_release(e+0x148);
    rx_fx_model_entity_vf9(e);
    if(U32(e,0x750)){
        scene_vcall(U32(e,0x750),8,0,NULL);
        if(U32(e,0x750)){const uint32_t arg=1;scene_vcall(U32(e,0x750),0,1,&arg);}
        SU32(e,0x750,0);
    }
}
RX_FUNCTION(0x006ba4d0,loop_particles_unload)
/*006ba520: preserve base job submission and emitter tick. */
static void loop_particles_tick(rx_addr e,float dt) {
    rx_anim_entity_single_loop_update(e,dt);
    if(U32(e,0x750))rx_fx_attachable_emitter_vf3(U32(e,0x750),dt);
}
RX_FUNCTION(0x006ba520,loop_particles_tick)
