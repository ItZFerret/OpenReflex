/* Source-evidenced 006b9b20 -> 006248c0 lifecycle contract. No original oracle run.
 * Link current core objects; no game install, asset extraction or executable read. */
#include "rx_mem.h"
#include "rx_fn.h"
#include "rx_heap.h"
#include "rx_image.h"
#include "../game/build_alloc.h"
#include "../game/anim_scenario.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while (0)
static rx_addr expected_entity, expected_reg[2], freed_blocks[4];
static int calls, callback_bad;
static void remove_entity(rx_addr self, rx_addr entity) {
    if (calls >= 2 || self != expected_reg[calls] || entity != expected_entity) callback_bad++;
    if (U32(entity,0) != 0x0095a2e4 || U32(entity,0x90) != 0x0093708c ||
        U32(entity,0x140) != 0x0095f734 || U32(entity,0x200) != 0x0093eb78 ||
        U32(entity,0x148) != 0x00975e6c || U32(entity,0x150) != 0 || U32(entity,0x164) != 0) callback_bad++;
    for (unsigned k=0;k<4;k++) if (rx_heap_size(freed_blocks[k]) != 0) callback_bad++;
    calls++;
}
int main(void) {
    CHECK(rx_mem_init()); CHECK(rx_image_load(NULL)==0); CHECK(rx_image_load_crt(NULL)==0); rx_set_sp(0x03000000);
    /* The image precedes CRT startup. This fixture uses the original's established system-heap mode,
     * as product startup does; otherwise CRT free enters its explicitly unsupported alternate-heap path. */
    SU32(0,0x785bd680,1);
    rx_addr (*construct)(rx_addr) = RX_FN(0x006ba2e0,rx_addr(*)(rx_addr));
    rx_addr (*destroy)(rx_addr,uint32_t) = RX_FN(0x006b9b20,rx_addr(*)(rx_addr,uint32_t));
    CHECK(construct && destroy);
    rx_fn_register(0x0f000031,(void*)remove_entity,"fixture_remove_loop_entity");
    expected_reg[0]=0x02100000; expected_reg[1]=0x02100100;
    const rx_addr vt=0x02100200; SU32(vt,4,0x0f000031);
    SU32(expected_reg[0],0,vt); SU32(expected_reg[1],0,vt);
    const uint32_t flags[]={0,2,1,0x100};
    for(unsigned test=0;test<4;test++) {
        rx_addr entity=rx_reflex_aligned_alloc(0xa70); CHECK(entity);
        rx_addr block=U32(entity,-4); CHECK(rx_heap_size(block)>0);
        CHECK(construct(entity)==entity);
        const rx_addr character=entity+0x148;
        rx_addr slots=U32(character,4), slot=rx_reflex_aligned_alloc(12), bone_map=rx_reflex_aligned_alloc(8), bones=rx_reflex_aligned_alloc(32);
        CHECK(slots && slot && bone_map && bones);
        SU32(slots,0,slot); SU32(character,8,1); SU32(slot,8,bone_map); SU32(character,0x1c,bones);
        freed_blocks[0]=U32(slots,-4); freed_blocks[1]=U32(slot,-4); freed_blocks[2]=U32(bone_map,-4); freed_blocks[3]=U32(bones,-4);
        memset(rx_base+entity+0xa60,0xa5,16); SU32(entity,0x750,0x12345678); // base destructor must not guess emitter cleanup
        rx_addr neighbor=rx_reflex_aligned_alloc(32); CHECK(neighbor); memset(rx_base+neighbor,0x5a,32);
        expected_entity=entity; calls=callback_bad=0;
        SU32(0,0x00a95824,expected_reg[0]); SU32(0,0x00a95828,expected_reg[1]);
        SU8(0,0x00da67d8,test&1); // exercise both allocator-lock flag values (product lock is the existing no-op abstraction)
        const rx_addr stack=rx_sp();
        CHECK(destroy(entity,flags[test])==entity); CHECK(rx_sp()==stack);
        CHECK(calls==2 && callback_bad==0);
        for(unsigned i=0;i<32;i++) CHECK(U8(neighbor,i)==0x5a);
        rx_reflex_aligned_free(neighbor);
        if(flags[test]&1) {
            CHECK(rx_heap_size(block)==0); // inspect heap metadata, never dereference freed entity
        } else {
            CHECK(rx_heap_size(block)>0); CHECK(U32(entity,0x750)==0x12345678);
            for(unsigned i=0;i<16;i++) CHECK(U8(entity,0xa60+i)==0xa5);
            rx_reflex_aligned_free(entity);
        }
    }
    SU32(0,0x00a95824,0); SU32(0,0x00a95828,0); SU8(0,0x00da67d8,0);
    rx_mem_shutdown();
    puts("PASS single-loop scalar destructor: flags 0/2/1/0x100, ordered registries, child frees, canaries, stack restoration (source-evidenced)");
    return 0;
}
