/* build_stl_cue_map.c -- the MSVC 9 (checked) std::multimap<std::string, Cue*> operations ActivityAudioLogic's cue map (+0x8c) uses: equal_range 00705f70, the checked
 * iterator distance 00709390 and the iterator increment 00593dc0. Node: +0 left, +4 parent, +8 right, +0xc the key (std::string), +0x28 the value, colour +0x2c,
 * isnil +0x2d; the map: +0 proxy, +0x18 head, +0x1c size. An iterator {+0 the container proxy, +4 the node}. */
#include "build_activity_util.h"
#include "build_stl_cue_map.h"
#include "crt.h"
#include "fx.h"

#define NIL(n) (U8((n), 0x2d) != 0)

/* 00705f70 (stdcall map, out range, key string; RET 0xc) -> out: {+0 proxy, +4 lower bound, +8 proxy, +0xc upper bound} */
rx_addr rx_cue_map_equal_range(rx_addr map, rx_addr out, rx_addr key) {
    ACT_FRAME(L, 0, 4);
    const rx_addr head = U32(map, 0x18);
    rx_addr lower = head, upper = head, n = U32(head, 4);
    while (!NIL(n)) {
        uint32_t lt;
        ACT_CALLR(lt, 2, crt_str_less(n + 0xc, key) & 0xff);
        if (lt) { n = U32(n, 8); continue; }
        if (NIL(upper)) { uint32_t gt; ACT_CALLR(gt, 2, crt_str_less(key, n + 0xc) & 0xff); if (gt) upper = n; }
        lower = n;
        n = U32(n, 0);
    }
    n = NIL(upper) ? U32(U32(map, 0x18), 4) : U32(upper, 0);
    while (!NIL(n)) {
        uint32_t gt;
        ACT_CALLR(gt, 2, crt_str_less(key, n + 0xc) & 0xff);
        if (gt) { upper = n; n = U32(n, 0); } else n = U32(n, 8);
    }
    SU32(out, 0, U32(map, 0)); SU32(out, 8, U32(map, 0)); SU32(out, 0xc, upper); SU32(out, 4, lower);
    ACT_END(L);
    return out;
}
RX_FUNCTION(0x00705f70, rx_cue_map_equal_range)

/* 00709390 (EDI = &count; first {proxy, node}, last {proxy, node} on the stack, popped by the caller (a plain RET)): *count += the number of increments from first to last (checked: each step
 * needs the iterator's proxy to be the last's) */
void rx_cue_range_count(rx_addr count, uint32_t fproxy, uint32_t fnode, uint32_t lproxy, uint32_t lnode) {
    ACT_FRAME(L, 0x10, 3);
    SU32(L, 0x10, fproxy); SU32(L, 0x14, fnode);
    for (;;) {
        const uint32_t px = U32(L, 0x10);
        if (px == 0 || px != lproxy) crt_invalid_parameter_noinfo();
        if (U32(L, 0x14) == lnode) break;
        SU32(count, 0, U32(count, 0) + 1);
        ACT_CALL(0, rx_fx_tree_next(L + 0x10));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00709390, rx_cue_range_count)
