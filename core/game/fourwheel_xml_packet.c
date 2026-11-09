/* Exact small dependency slice of four-wheel pending XML tuning. This is a
 * packet publisher, not a discardable printf/log call. No guest-stream cast,
 * original-code fallback, no-op active branch, or new transport thread. */
#include "fourwheel_xml_packet.h"
#include "build_alloc.h"
#include "build_vehicle_util.h"
#include "crt.h"
#include "../src/rx_os.h"

static uint32_t critical(rx_addr frame,uint32_t ret,uint32_t iat,rx_addr cs){
    veh_call_sp(frame,ret,1,cs,0,0);
    uint32_t value=rx_os_import(iat,1,&cs);
    rx_set_sp(frame);return value;
}

/*00401f90: same CRT allocator as0078fbe0, but its lock is entered once,
 * unlike that function's double-check/double-enter machine-code sequence. */
rx_addr rx_tuning_buffer_alloc(uint32_t bytes){
    const rx_addr entry=rx_sp();
    if(U8(0,0x00da67d8))critical(entry,0x00401fa4,0x0091600c,0x00d61704);
    const rx_addr frame=entry-4; /*saved ESI*/
    veh_call_sp(frame,0x00401fb2,2,bytes,0x10,0);
    const rx_addr result=rx_crt_aligned_malloc(bytes,0x10);
    rx_set_sp(frame);
    if(U8(0,0x00da67d8))critical(frame,0x00401fcb,0x009160f8,0x00d61704);
    rx_set_sp(entry);return result;
}
RX_FUNCTION(0x00401f90,rx_tuning_buffer_alloc)

/*007905d0: doubly-linked FIFO, packet node{data,prev,next}. Allocation failure
 * follows the original's null stores, including any subsequent address0 store;
 * no newly invented failure recovery changes the queue. */
static uint32_t append_observe(rx_addr owner,rx_addr packet,uint32_t* edx_io){
    const rx_addr entry=rx_sp(),frame=entry-4,cs=owner+0x201b0;
    rx_set_sp(frame);
    critical(frame,0x007905de,0x0091600c,cs);
    veh_call_sp(frame,0x007905e5,1,0xc,0,0);
    const rx_addr node=rx_reflex_aligned_alloc(0xc);
    rx_set_sp(frame);
    uint32_t ecx=0;
    if(node){
        if(edx_io)*edx_io=packet;
        SU32(node,0,packet);SU32(node,4,0);SU32(node,8,0);
    }
    if(U32(owner,0x201c8)==0)SU32(owner,0x201cc,node);
    else{
        ecx=U32(owner,0x201d0);
        SU32(ecx,8,node);
        const rx_addr previous=U32(owner,0x201d0);
        if(edx_io)*edx_io=previous;
        SU32(node,4,previous);
    }
    SU32(owner,0x201c8,U32(owner,0x201c8)+1);
    SU32(owner,0x201d0,node);
    critical(frame,0x00790633,0x009160f8,cs);
    rx_set_sp(entry);return ecx;
}
uint32_t rx_tuning_packet_queue_append(rx_addr owner,rx_addr packet){return append_observe(owner,packet,0);}
RX_FUNCTION(0x007905d0,rx_tuning_packet_queue_append)

static uint32_t net32(uint32_t value){
    return(value>>24)|((value>>8)&0x0000ff00u)|((value<<8)&0x00ff0000u)|(value<<24);
}
void rx_tuning_packet_publish_observe(rx_addr sink,uint32_t type,uint32_t size,rx_addr payload,uint32_t* eax_io,uint32_t* edx_io){
    const rx_addr entry=rx_sp();
    /*pushEBP; disabled return preserves EAX/EDX and allocates nothing.*/
    if(U32(sink,0x28)==0)return;
    const rx_addr frame=entry-16,cs=sink+0xc; /*EBP,EBX,ESI,EDI*/
    rx_set_sp(frame);
    critical(frame,0x00790558,0x0091600c,cs);
    veh_call_sp(frame,0x0079055f,1,0x10,0,0);
    const rx_addr packet=rx_reflex_aligned_alloc(0x10);
    /*cdecl arg0x10 stays while second cdecl call pushes0x20000.*/
    rx_set_sp(frame-4);
    veh_call_sp(frame-4,0x00790571,1,0x20000,0,0);
    SU32(packet,0,0);
    const rx_addr buffer=rx_tuning_buffer_alloc(0x20000);
    rx_set_sp(frame);
    /*WS2_32 ordinal8=htonl, exact unsigned32-bit byte swap. The argument
     * spill/return slots are kept even though no OS/socket call is needed.*/
    veh_call_sp(frame,0x00790580,1,type,0,0);
    SU32(buffer,4,net32(type));
    const uint32_t total=size+8u;
    veh_call_sp(frame,0x00790590,1,total,0,0);
    SU32(buffer,0,net32(total));
    veh_call_sp(frame,0x007905a5,3,buffer+8,payload,size);
    crt_memcpy(buffer+8,payload,size);
    rx_set_sp(frame);
    SU32(packet,4,total);
    const rx_addr owner=U32(entry,4); /*reload [esp+14] rather than cached sink*/
    SU32(packet,8,buffer);
    veh_call_sp(frame,0x007905b8,1,packet,0,0);
    uint32_t edx=payload;
    append_observe(owner,packet,&edx);
    rx_set_sp(frame);
    const uint32_t eax=critical(frame,0x007905bf,0x009160f8,cs);
    if(eax_io)*eax_io=eax;
    if(edx_io)*edx_io=edx;
    rx_set_sp(entry);
}
void rx_tuning_packet_publish(rx_addr sink,uint32_t type,uint32_t size,rx_addr payload){
    rx_tuning_packet_publish_observe(sink,type,size,payload,0,0);
}
RX_FUNCTION(0x00790540,rx_tuning_packet_publish)
