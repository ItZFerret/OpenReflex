/* build_rider_clip.c -- construction unit 7 (riders, core/notes/construction.md section 13): the highlight clip recorder's setup,
 * ClipRecorder_Init 006aa660 (from the game init 0075ed60) and ClipRecorder_AllocBuffer 006abc40: the sample ring [0x00a7f124]
 * (0x5b68 records of 0x50 bytes = 150 samples x 12 riders x ... sections; the race frames' ClipRecorder_Snapshot (anim_scenario_clip.c)
 * writes it) and the recorder's four stunt event handlers. Hand transliterations of the disassembly. */
#include "build_rider.h"
#include "build_rider_util.h"
#include "world_misc.h"
#include "crt.h"

/* 0042e620 (ECX = record): a sample record's eight floats (+0x20..+0x3c) and +0x40 zero */
static void sample_init(rx_addr r) {
    for (uint32_t o = 0x20; o <= 0x3c; o += 4) SF32(r, o, 0.0f);
    SU32(r, 0x40, 0);
}
/* 006abc40 ClipRecorder_AllocBuffer: records per clip section 0x708, ring length 150, 0x5b68 records */
void rx_clip_alloc_buffer(void) {
    SU32(0, 0x00a942d8, 0x708);
    SU32(0, 0x00a942dc, 0x96);
    SU32(0, 0x00a942d4, 0x5b68);
    rx_addr p = rr_alloc(rx_mul_sat(0x5b68, 0x50));
    if (p) rx_rider_vector_ctor(p, 0x50, 0x5b68, sample_init);   /* 004022d0 `vector constructor iterator' */
    SU32(0, 0x00a7f124, p);
}
RX_FUNCTION(0x006abc40, rx_clip_alloc_buffer)

/* 005b9ff0 (EAX = &handler, s0 = list; RET 4): std::list<Handler*>::push_back (list: +0x14 head node, +0x18 size; node 0xc: next,
 * prev, value) */
static void handler_list_push_back(rx_addr list, rx_addr handler) {
    rx_addr head = U32(list, 0x14);
    rx_addr prev = U32(head, 4);
    rx_addr node = rr_alloc(0xc);
    if (node) { SU32(node, 0, head); SU32(node, 4, prev); SU32(node, 8, handler); }
    if (0x3fffffffu - U32(list, 0x18) < 1) rr_fatal("list<T> too long", list);
    SU32(list, 0x18, U32(list, 0x18) + 1);
    SU32(head, 4, node);
    SU32(prev, 0, node);
}
/* 006ac430 (ECX = event name, s0 = callback; RET 4): a static handler of a global event (the event channel [0x00a94304]): a
 * FunctionProxy {vtable 0x0095eb50, +4 0, +8 its signature's type name (type_info 0x009cbf38), +0xc 0x00d32360, +0x10 the callback},
 * appended to the event's handler list when the signature names match (the proxy's vf1 008ad240 returns +8; the event's lock +0x24 is
 * not taken: one thread) */
static void clip_register_handler(rx_addr name, uint32_t cb) {
    rx_addr ev = rx_world_registry_find(U32(0, 0x00a94304), name);
    if (!ev) rr_fatal("clip recorder: event not registered", name);
    rx_addr h = rr_alloc(0x14);
    if (!h) rr_fatal("clip recorder: handler allocation failed (the original faults)", name);
    SU32(h, 4, 0);
    SU32(h, 0, 0x0095eb50u);
    SU32(h, 0xc, 0x00d32360u);
    SU32(h, 0x10, cb);
    SU32(h, 8, crt_type_info_name(0x009cbf38u, 0x009e1a7cu));
    if (crt_strcmp(U32(ev, 4), U32(h, 8)) != 0) return;   /* the original keeps the proxy unlisted (leaked) */
    handler_list_push_back(ev + 8, h);
}
/* 006aa660 ClipRecorder_Init: idle, the ring, no activity, the timers 0, the clip slots' section numbers 1..12, the stunt handlers */
void rx_clip_init(void) {
    SU32(0, 0x00a942b4, 0);
    rx_clip_alloc_buffer();
    SU32(0, 0x00a942b8, 0);
    SF32(0, 0x00a942a4, 0.0f); SF32(0, 0x00a942a8, 0.0f); SF32(0, 0x00a942ac, 0.0f);
    uint32_t n = 0;
    for (rx_addr a = 0x00d3275cu; a < 0x00d5ad1cu; a += 0x35d0) SU32(a, 0, ++n);
    clip_register_handler(0x0095ba28u, 0x006ab290u);   /* "StuntBegin" */
    clip_register_handler(0x00960790u, 0x006ab380u);   /* "StuntHoldBegin" */
    clip_register_handler(0x00960780u, 0x006ab410u);   /* "StuntHoldEnd" */
    clip_register_handler(0x00942784u, 0x006ab4a0u);   /* "StuntCompleted" */
}
RX_FUNCTION(0x006aa660, rx_clip_init)
