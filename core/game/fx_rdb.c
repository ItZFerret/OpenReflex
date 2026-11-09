/* fx_rdb.c -- the small Rdb (game database) helpers of the 0x402xxx / 0x415xxx .. 0x419xxx range, register-exact: the string
 * keyed map search (00417da0 lower bound, 00416e40 find: _stricmp, IAT 0x9162bc), the row-set iterator (00417df0), the
 * criterion vector's iterator advance (00418e00, 0xb0 records), the int-keyed set map search (00419ea0), the by-name cell
 * helpers (004199a0 / 00419ae0), a criterion's int initialiser (00419a70) and destructor (00415320), the criterion vector's
 * destructor (00417590), the row count (004021d0), ModelEntity::vf9 (00401ea0) and the bool ConVar's toggle (004172e0).
 * The tables themselves are the construction code's (build_vehicle_tables.c / build_tables_rdb.c: rx_tbl_*). The checked
 * iterators' _invalid_parameter_noinfo (IAT 0x916224) never fires on valid tables; it is kept where the original calls it.
 * notes/fx.md. */
#include "fx.h"
#include "build_rider.h"
#include "fx_util.h"
#include "build_vehicle.h"
#include "build_tables.h"
#include "build_alloc.h"
#include "build_scene_entity.h"

static void invalid_parameter(void) { rx_call_original(U32(0, 0x00916224), RX_CDECL, 0, 0, 0); }
/* ReflexAlignedFree 0078fc50 (cdecl) at the current ESP */
static void aligned_free_call(rx_addr p, rx_addr ret) {
    const uint32_t a[1] = {p}; const rx_addr s = fx_push(1, a, ret); rx_reflex_aligned_free(p); fx_pop(s);
}

/* ---- 00417da0 (EAX = map object; stdcall &key char*; RET 4): the lower bound of the key by _stricmp (head [map + 0x18]; node
 * +0 left, +8 right, +0xc the key, +0x15 isnil). Frame: push ebp; push esi; push edi; (a root) push ebx. */
rx_addr rx_fx_strmap_lower_bound(rx_addr map, rx_addr keyp) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    const rx_addr head = U32(map, 0x18);
    SU32(sp0, -4, sv.ebp); fx_set_reg(FX_EBP, keyp);
    SU32(sp0, -8, sv.esi);
    rx_addr n = U32(head, 4);
    fx_set_reg(FX_ESI, n);
    SU32(sp0, -0xc, sv.edi);
    rx_addr best = head;
    fx_set_reg(FX_EDI, head);
    if (U8(n, 0x15) == 0) {
        SU32(sp0, -0x10, sv.ebx);
        rx_set_sp(sp0 - 0x10);
        fx_set_reg(FX_EBX, U32(0, 0x009162bc));
        do {
            int32_t c;
            { const uint32_t a[2] = {U32(n, 0xc), U32(keyp, 0)}; const rx_addr s = fx_push(2, a, 0x00417dca); c = crt_stricmp(a[0], a[1]); fx_pop(s); }
            if (c < 0) n = U32(n, 8);
            else { best = n; fx_set_reg(FX_EDI, best); n = U32(n, 0); }
            fx_set_reg(FX_ESI, n);
        } while (U8(n, 0x15) == 0);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
    return best;
}
RX_FUNCTION(0x00417da0, rx_fx_strmap_lower_bound)

/* ---- 00416e40 (ESI = map object; stdcall out iterator*, &key; RET 8): find: the lower bound unless it is the end or the key
 * sorts before its key -> out = {proxy, node or the head}. Frame: sub esp, 0x18; ebx / ebp / edi: E = sp0 - 0x24: +0x10 the
 * found iterator, +0x18 the end iterator. */
rx_addr rx_fx_strmap_find(rx_addr map, rx_addr out, rx_addr keyp) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x18, E = e0 - 0xc;
    const fx_saved sv = fx_save();
    fx_push_regs(e0, 2, (int[]){FX_EBX, FX_EBP});
    fx_set_reg(FX_EBP, keyp);
    fx_push_regs(e0 - 8, 1, (int[]){FX_EDI});
    rx_set_sp(E);
    rx_addr lb;
    { const uint32_t a[1] = {keyp}; const rx_addr s = fx_push(1, a, 0x00416e52); lb = rx_fx_strmap_lower_bound(map, keyp); fx_pop(s); }
    fx_set_reg(FX_EBX, lb);
    SU32(E, 0x14, lb);
    if (map == 0) invalid_parameter();
    const rx_addr proxy = U32(map, 0), head = U32(map, 0x18);
    fx_set_reg(FX_EDI, head);
    SU32(E, 0x10, proxy);
    if (proxy == 0) invalid_parameter();
    rx_addr it = E + 0x18;
    if (lb != head) {
        int32_t c;
        { const uint32_t a[2] = {U32(keyp, 0), U32(lb, 0xc)}; const rx_addr s = fx_push(2, a, 0x00416e8b); c = crt_stricmp(a[0], a[1]); fx_pop(s); }
        if (c >= 0) it = E + 0x10;
    }
    if (it == E + 0x18) { SU32(E, 0x18, U32(map, 0)); SU32(E, 0x1c, U32(map, 0x18)); }
    const uint32_t w0 = U32(it, 0), w1 = U32(it, 4);
    SU32(out, 4, w1); SU32(out, 0, w0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x00416e40, rx_fx_strmap_find)

/* ---- 00417df0 (ESI = iterator {proxy, node}): ++it over a row set (node +0 left, +4 parent, +8 right, +0x19 isnil). Frame:
 * push edi. */
void rx_fx_rowset_next(rx_addr it) {
    SU32(rx_sp(), -4, fx_reg(FX_EDI));
    if (U32(it, 0) == 0) invalid_parameter();
    const rx_addr n = U32(it, 4);
    if (U8(n, 0x19) != 0) { invalid_parameter(); return; }
    rx_addr r = U32(n, 8);
    if (U8(r, 0x19) == 0) {
        for (rx_addr l = U32(r, 0); U8(l, 0x19) == 0; l = U32(r, 0)) r = l;
        SU32(it, 4, r);
        return;
    }
    rx_addr p = U32(n, 4);
    while (U8(p, 0x19) == 0 && U32(it, 4) == U32(p, 8)) { SU32(it, 4, p); p = U32(p, 4); }
    SU32(it, 4, p);
}
RX_FUNCTION(0x00417df0, rx_fx_rowset_next)

/* ---- 00418e00 (ESI = iterator {proxy, record}; stdcall n; RET 4): it += n criteria (0xb0 bytes), checked against the vector
 * ([proxy] +0xc .. +0x10). Frame: push ebx; push edi. */
rx_addr rx_fx_crit_iter_add(rx_addr it, int32_t n) {
    SU32(rx_sp(), -4, fx_reg(FX_EBX)); SU32(rx_sp(), -8, fx_reg(FX_EDI));
    rx_addr px = U32(it, 0);
    rx_addr vec;
    if (px == 0) { invalid_parameter(); px = U32(it, 0); vec = px ? U32(px, 0) : 0; }
    else vec = U32(px, 0);
    const uint32_t d = (uint32_t)n * 0xb0u, p = U32(it, 4) + d;
    if (p > U32(vec, 0x10) || p < U32(px ? U32(px, 0) : 0, 0xc)) invalid_parameter();
    SU32(it, 4, U32(it, 4) + d);
    return it;
}
RX_FUNCTION(0x00418e00, rx_fx_crit_iter_add)

/* ---- 00419ea0 (EDI = map object; stdcall out, &key; RET 8): find in a std::map<int, std::set> (node +0xc key, +0x39 isnil).
 * Frame: sub esp, 0x18; ebx / ebp / esi: E = sp0 - 0x24: +0x10 the found iterator, +0x18 the end iterator. */
rx_addr rx_fx_intsetmap_find(rx_addr map, rx_addr out, rx_addr keyp) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x18, E = e0 - 0xc;
    const fx_saved sv = fx_save();
    const rx_addr head = U32(map, 0x18);
    fx_push_regs(e0, 2, (int[]){FX_EBX, FX_EBP});
    fx_push_regs(e0 - 8, 1, (int[]){FX_ESI});
    rx_set_sp(E);
    rx_addr best = head;
    for (rx_addr n = U32(head, 4); U8(n, 0x39) == 0;) {
        if (I32(n, 0xc) < I32(keyp, 0)) n = U32(n, 8);
        else { best = n; n = U32(n, 0); }
    }
    const rx_addr proxy = U32(map, 0);
    SU32(E, 0x14, best); SU32(E, 0x10, proxy);
    if (proxy == 0) invalid_parameter();
    rx_addr it = E + 0x18;
    if (best != U32(map, 0x18) && !(I32(keyp, 0) < I32(best, 0xc))) it = E + 0x10;
    if (it == E + 0x18) { SU32(E, 0x1c, U32(map, 0x18)); SU32(E, 0x18, U32(map, 0)); }
    const uint32_t w0 = U32(it, 0), w1 = U32(it, 4);
    SU32(out, 4, w1); SU32(out, 0, w0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x00419ea0, rx_fx_intsetmap_find)

/* ---- 00419ae0 (ESI = table, EAX = column name; stdcall &key; RET 4): the row record of the key in the named column's index
 * (Table_ColumnIndex, 00418ec0). Frame: push ecx; push edi. */
rx_addr rx_fx_table_row_by(rx_addr table, rx_addr name, rx_addr keyp) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, fx_reg(FX_ECX)); SU32(sp0, -8, sv.edi);
    rx_set_sp(sp0 - 8);
    uint32_t col;
    { const uint32_t a[1] = {name}; const rx_addr s = fx_push(1, a, 0x00419aea); col = rx_tbl_column_index(table, name); fx_pop(s); }
    fx_set_reg(FX_EDI, col);
    if (!(col < (U32(table, 0x14) - U32(table, 0x10)) >> 2)) invalid_parameter();
    const rx_addr column = U32(U32(table, 0x10), 4 * col);
    rx_addr r;
    { const uint32_t a[1] = {column}; const rx_addr s = fx_push(1, a, 0x00419b11); r = rx_tbl_row(column, I32(keyp, 0)); fx_pop(s); }
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x00419ae0, rx_fx_table_row_by)

/* ---- 004199a0 (EAX = row reference {row, , table}; stdcall column name; RET 4): &the int cell of the row in the named column
 * (Table_ColumnIndex, 0041aaf0). Frame: push ecx; push ebx; push esi; push edi. */
rx_addr rx_fx_row_cell(rx_addr ref, rx_addr name) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, fx_reg(FX_ECX)); SU32(sp0, -8, sv.ebx);
    const uint32_t row = U32(ref, 0);
    fx_set_reg(FX_EBX, row);
    SU32(sp0, -0xc, sv.esi);
    const rx_addr table = U32(ref, 8);
    fx_set_reg(FX_ESI, table);
    SU32(sp0, -0x10, sv.edi);
    rx_set_sp(sp0 - 0x10);
    uint32_t col;
    { const uint32_t a[1] = {name}; const rx_addr s = fx_push(1, a, 0x004199b5); col = rx_tbl_column_index(table, name); fx_pop(s); }
    fx_set_reg(FX_EDI, col);
    rx_addr r;
    { const uint32_t a[1] = {row}; const rx_addr s = fx_push(1, a, 0x004199bf); r = rx_tbl_cell_word(table, col, row); fx_pop(s); }
    fx_restore(sv);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x004199a0, rx_fx_row_cell)

/* ---- 00419a70 (ESI = criterion, EAX = the operator, ECX = &value; stdcall column name; RET 4): an int criterion: +0x80 -1,
 * +0x84 the operator, +0x88 1 (int), +0x90 the value, the path vector (+0x98: its proxy allocated, empty), the name (strncpy
 * 0x7f, terminated). Frame: push ebx; push edi. */
rx_addr rx_fx_crit_init_int(rx_addr c, uint32_t op, rx_addr valp, rx_addr name) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx);
    SU32(c, 0x80, 0xffffffffu); SU32(c, 0x84, op);
    SU32(sp0, -8, sv.edi);
    SU32(c, 0x88, 1); SU32(c, 0x90, U32(valp, 0));
    const rx_addr v = c + 0x98;
    fx_set_reg(FX_EDI, v);
    rx_set_sp(sp0 - 8);
    rx_addr p;
    { const uint32_t a[1] = {4}; const rx_addr s = fx_push(1, a, 0x00419aa1); p = rx_reflex_aligned_alloc(4); fx_pop(s); }
    fx_set_reg(FX_EBX, 0);
    if (p != 0) SU32(p, 0, v);
    SU32(v, 0, p); SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    { const uint32_t a[3] = {c, name, 0x7f}; const rx_addr s = fx_push(3, a, 0x00419ac9); crt_strncpy(c, name, 0x7f); fx_pop(s); }
    SU8(c, 0x7f, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return c;
}
RX_FUNCTION(0x00419a70, rx_fx_crit_init_int)

/* ---- 004f73b0 (ESI = criterion, EAX = the operator, ECX = &value; stdcall column name; RET 4): the same for a float criterion: the
 * value's type is 2 and the 32-bit value is copied through XMM0 (movss, no arithmetic: the bits as they are). Frame: push ebx; push edi. */
rx_addr rx_fx_crit_init_float(rx_addr c, uint32_t op, rx_addr valp, rx_addr name) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx);
    SU32(c, 0x80, 0xffffffffu); SU32(c, 0x84, op);
    SU32(sp0, -8, sv.edi);
    SU32(c, 0x88, 2); SU32(c, 0x90, U32(valp, 0));
    const rx_addr v = c + 0x98;
    fx_set_reg(FX_EDI, v);
    rx_set_sp(sp0 - 8);
    rx_addr p;
    { const uint32_t a[1] = {4}; const rx_addr s = fx_push(1, a, 0x004f73e5); p = rx_reflex_aligned_alloc(4); fx_pop(s); }
    fx_set_reg(FX_EBX, 0);
    if (p != 0) SU32(p, 0, v);
    SU32(v, 0, p); SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    { const uint32_t a[3] = {c, name, 0x7f}; const rx_addr s = fx_push(3, a, 0x004f740d); crt_strncpy(c, name, 0x7f); fx_pop(s); }
    SU8(c, 0x7f, 0);
    fx_restore(sv);
    rx_set_sp(sp0);
    return c;
}
RX_FUNCTION(0x004f73b0, rx_fx_crit_init_float)

/* ---- 004f3320 (stdcall p; RET 4): the game's _aligned_free site: the allocator table made first (004dc0f0), the CRT's _aligned_free under the
 * allocator lock (0x00d61704, taken when [0x00da67d8] is set: the core runs on one thread). */
void rx_tbl_free(rx_addr p) {
    rx_tbl_alloc_init();
    rx_crt_aligned_free(p);
}
RX_FUNCTION(0x004f3320, rx_tbl_free)

/* ---- 004eee20 (ESI = a std::vector<char>-like {+0 the container proxy, +8 the allocator, +0xc begin, +0x10 end, +0x14 capacity end}; plain ret) ->
 * EAX = ESI: its element block deallocated (allocator<char>::deallocate = operator delete, via msvcp90 0x00916198), the three pointers cleared,
 * the proxy block freed (ReflexAlignedFree). */
rx_addr rx_fx_char_vector_free(rx_addr v) {
    const uint32_t begin = U32(v, 0xc);
    if (begin != 0) {
        const uint32_t size = U32(v, 0x14) - begin;
        const uint32_t a[2] = {begin, size};
        const rx_addr x = fx_push(2, a, 0x004eee37);
        crt_operator_delete(begin);                             /* allocator<char>::deallocate(begin, size) */
        fx_pop(x);
    }
    const uint32_t proxy = U32(v, 0);
    SU32(v, 0xc, 0);
    SU32(v, 0x10, 0);
    SU32(v, 0x14, 0);
    aligned_free_call(proxy, 0x004eee54);
    return v;
}
RX_FUNCTION(0x004eee20, rx_fx_char_vector_free)

/* ---- 004eedc0 (EDI = a value variant {+0 kind, +8 payload}; plain ret): a criterion's value destroyed: kind 4 (a std::string): its heap block
 * freed (004f3320) when it has one and the string reset to empty; kind 5 (a vector<char>): 004eee20; then the allocator table made
 * (004dc0f0) and the payload block freed (StdMemoryAllocator::vf2 00775540). Other kinds: nothing. Frame: push esi. */
void rx_fx_variant_destroy(rx_addr v) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.esi);
    rx_set_sp(sp0 - 4);
    const uint32_t kind = U32(v, 0);
    if (kind == 4) {
        const rx_addr str = U32(v, 8);
        fx_set_reg(FX_ESI, str);
        if (U32(str, 0x18) >= 0x10) {
            const uint32_t a[1] = {U32(str, 4)};
            const rx_addr x = fx_push(1, a, 0x004eedf7);
            rx_tbl_free(U32(str, 4));
            fx_pop(x);
        }
        SU32(str, 0x18, 0xf);
        SU32(str, 0x14, 0);
        SU8(str, 4, 0);
        { const rx_addr x = fx_push(0, 0, 0x004eee0b); rx_tbl_alloc_init(); fx_pop(x); }
        { const uint32_t a[1] = {U32(v, 8)}; const rx_addr x = fx_push(1, a, 0x004eee14); (void)rx_std_allocator_free(0, a[0]); fx_pop(x); }
    } else if (kind == 5) {
        fx_set_reg(FX_ESI, U32(v, 8));
        { const rx_addr x = fx_push(0, 0, 0x004eedd5); (void)rx_fx_char_vector_free(U32(v, 8)); fx_pop(x); }
        { const rx_addr x = fx_push(0, 0, 0x004eedda); rx_tbl_alloc_init(); fx_pop(x); }
        { const uint32_t a[1] = {U32(v, 8)}; const rx_addr x = fx_push(1, a, 0x004eede3); (void)rx_std_allocator_free(0, a[0]); fx_pop(x); }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004eedc0, rx_fx_variant_destroy)

/* ---- 0041ac70 (cdecl begin, end, ecx; plain ret): the criteria of [begin, end) (0xb0 bytes each) destroyed: the value block (+0xa4) freed and
 * its three words cleared, the path proxy (+0x98) freed, and the value variant (+0x88, payload +0x90) as 004eedc0 does inline - a string's heap
 * block (004f3320) and reset, or a char vector (004eee20) - then the allocator table made (004dc0f0) and the payload block freed with the CRT's
 * _aligned_free (import 0x00916350, under the allocator lock). Frame: push edi; (a non-empty range) push ebx, ebp, esi. */
void rx_fx_crit_range_destroy(rx_addr begin, rx_addr end) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.edi);
    if (begin == end) { rx_set_sp(sp0); return; }
    SU32(sp0, -8, sv.ebx);
    SU32(sp0, -0xc, sv.ebp);
    SU32(sp0, -0x10, sv.esi);
    rx_set_sp(sp0 - 0x10);
    fx_set_reg(FX_EBP, U32(0, 0x00916350));
    fx_set_reg(FX_EBX, 0);
    for (rx_addr rec = begin; rec != end; ) {
        const rx_addr p = rec + 0xa4;
        fx_set_reg(FX_EDI, p);
        if (U32(p, 0) != 0) aligned_free_call(U32(p, 0), 0x0041ac9c);
        const rx_addr proxy = U32(rec, 0x98);
        SU32(p, 0, 0); SU32(p, 4, 0); SU32(p, 8, 0);
        aligned_free_call(proxy, 0x0041acb0);
        const uint32_t kind = U32(rec, 0x88);
        if (kind == 4) {
            const rx_addr str = U32(rec, 0x90);
            fx_set_reg(FX_ESI, str);
            if (U32(str, 0x18) >= 0x10) {
                const uint32_t a[1] = {U32(str, 4)};
                const rx_addr x = fx_push(1, a, 0x0041ad06);
                rx_tbl_free(U32(str, 4));
                fx_pop(x);
            }
            SU32(str, 0x18, 0xf);
            SU32(str, 0x14, 0);
            SU8(str, 4, 0);
        } else if (kind == 5) {
            const rx_addr vec = U32(rec, 0x90);
            fx_set_reg(FX_ESI, vec);
            if (U32(vec, 0xc) != 0) {
                const uint32_t a[2] = {U32(vec, 0xc), U32(vec, 0x14) - U32(vec, 0xc)};
                const rx_addr x = fx_push(2, a, 0x0041acde);
                crt_operator_delete(a[0]);                      /* allocator<char>::deallocate */
                fx_pop(x);
            }
            const uint32_t block = U32(vec, 0);
            SU32(vec, 0xc, 0); SU32(vec, 0x10, 0); SU32(vec, 0x14, 0);
            aligned_free_call(block, 0x0041acef);
        }
        if (kind == 4 || kind == 5) {
            const rx_addr payload = U32(rec, 0x90);
            fx_set_reg(FX_ESI, payload);
            { const rx_addr x = fx_push(0, 0, 0x0041ad1b); rx_tbl_alloc_init(); fx_pop(x); }
            { const uint32_t a[1] = {payload}; const rx_addr x = fx_push(1, a, 0x0041ad31); rx_crt_aligned_free(payload); fx_pop(x); }
        }
        rec += 0xb0;
        SU32(sp0, 4, rec);                                      /* the loop advances the `begin` argument slot itself ([esp + 0x14]) */
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0041ac70, rx_fx_crit_range_destroy)

/* ---- 00415320 (ESI = criterion): its value block (+0xa4) and path proxy (+0x98) freed, then 004eedc0 (EDI = +0x88; not in the
 * core). Frame: push edi. */
void rx_fx_crit_destroy(rx_addr c) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.edi);
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_EDI, 0);
    if (U32(c, 0xa4) != 0) aligned_free_call(U32(c, 0xa4), 0x00415333);
    const rx_addr q = U32(c, 0x98);
    SU32(c, 0xa4, 0); SU32(c, 0xa8, 0); SU32(c, 0xac, 0);
    aligned_free_call(q, 0x00415354);
    fx_set_reg(FX_EDI, c + 0x88);
    { const rx_addr x = fx_push(0, 0, 0x00415362); rx_fx_variant_destroy(c + 0x88); fx_pop(x); }   /* 004eedc0 (EDI = the value variant) */
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00415320, rx_fx_crit_destroy)

/* ---- 00417590 (ESI = criterion vector {proxy, , , begin, end, cap}): the criteria destroyed (0041ac70, not in the core), the
 * block and the proxy freed. Frame: push ecx (its value: 0041ac70's third argument). */
void rx_fx_crit_vec_destroy(rx_addr v) {
    const rx_addr sp0 = rx_sp();
    const uint32_t ecx = fx_reg(FX_ECX);
    SU32(sp0, -4, ecx);
    const rx_addr b = U32(v, 0xc);
    if (b != 0) {
        const uint32_t a[3] = {b, U32(v, 0x10), ecx};
        {   /* 0041ac70 (cdecl begin, end, ecx): the three words pushed stay (the caller's add esp) */
            rx_set_sp(sp0 - 4);
            const rx_addr x = fx_push(3, a, 0x004175a6);
            rx_fx_crit_range_destroy(a[0], a[1]);
            fx_pop(x);
        }
        for (int i = 0; i < 3; i++) SU32(sp0 - 0x10 + 4 * (uint32_t)i, 0, a[i]);   /* the cdecl arguments stay pushed */
        SU32(sp0, -0x14, 0x004175a6);
        { rx_set_sp(sp0 - 0x10); aligned_free_call(U32(v, 0xc), 0x004175af); }
    }
    const rx_addr px = U32(v, 0);
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    rx_set_sp(sp0 - 4);
    aligned_free_call(px, 0x004175cf);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00417590, rx_fx_crit_vec_destroy)

/* ---- 004021d0 (EAX = table): its row count, the rows of column 0 (tail jump to 004eb830), 0 without columns. Frame: push ebx;
 * push esi; push edi. */
uint32_t rx_fx_table_rows(rx_addr t) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_EBX)); SU32(sp0, -8, fx_reg(FX_ESI)); SU32(sp0, -0xc, fx_reg(FX_EDI));
    if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) return 0;
    const rx_addr first = U32(t, 0x10);
    if (first > U32(t, 0x14)) invalid_parameter();
    rx_addr px = U32(t, 4);
    if (px == 0) invalid_parameter(); else px = U32(px, 0);
    if (!(first < U32(px, 0x10))) invalid_parameter();
    return rx_tbl_column_rows(U32(first, 0));
}
RX_FUNCTION(0x004021d0, rx_fx_table_rows)

/* ---- 00401ea0 ModelEntity::vf9 (thiscall): unregistered from the two renderer lists ([0xa95824] / [0xa95828] vf1), its block
 * (+0x18) freed and the model instance (+0xa0, 007f1aa0: not in the core) released. Frame: push esi. */
void rx_fx_model_entity_vf9(rx_addr self) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.esi);
    fx_set_reg(FX_ESI, self);
    rx_set_sp(sp0 - 4);
    SU32(self, 0x70, 0);
    for (int k = 0; k < 2; k++) {
        const rx_addr L = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (L == 0) continue;
        const rx_addr fn = U32(U32(L, 0), 4);
        void (*f)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
        const uint32_t a[1] = {self};
        if (f) { const rx_addr s = fx_push(1, a, k ? 0x00401ece : 0x00401ebc); f(L, self); fx_pop(s); }
        else rx_call_original(fn, RX_THISCALL, L, 1, a);
    }
    aligned_free_call(U32(self, 0x18), 0x00401ed7);
    fx_set_reg(FX_ESI, self + 0xa0);
    { const rx_addr s = rx_sp(); rx_set_sp(s - 4u); rx_model_instance_release(self + 0xa0); rx_set_sp(s); }   /* 007f1aa0 */
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00401ea0, rx_fx_model_entity_vf9)

/* ---- 004172e0 ConVar<bool>::vf2 (thiscall; stdcall out*; RET 4): a toggle request of the variable's own type (the two
 * type_info names (0x9cbb50) compare equal under _stricmp): +0x24 flipped, *out = the new value -> 1 (else 0). Frame: push esi;
 * push edi. */
uint32_t rx_fx_convar_bool_toggle(rx_addr self, rx_addr out) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_ESI)); SU32(sp0, -8, fx_reg(FX_EDI));
    rx_set_sp(sp0 - 8);
    rx_addr n1, n2;
    n1 = fx_type_name(0x009cbb50, 0x004172f6);
    rx_set_sp(sp0 - 0xc); SU32(sp0, -0xc, n1);
    n2 = fx_type_name(0x009cbb50, 0x00417303);
    int32_t c;
    rx_set_sp(sp0 - 0x10); SU32(sp0, -0x10, n2);
    { const rx_addr s = fx_push(0, 0, 0x0041730a); c = crt_stricmp(n2, n1); fx_pop(s); }
    rx_set_sp(sp0);
    if (c != 0) return 0;
    const uint32_t v = U32(self, 0x24) == 0;
    SU32(self, 0x24, v);
    SU32(out, 0, v != 0);
    return 1;
}
RX_FUNCTION(0x004172e0, rx_fx_convar_bool_toggle)
