/* fx_types.c -- the leaves of the effects range: the static-instance getters behind their init guards, the MSVC vector
 * constructor / destructor iterators and the small fills of the stunt scorer. (The class ids (vf22), behaviour kinds (vf5) and
 * the Entity / ModelEntity transform accessors of this range are build_scene_vf.c's, build_rider_tree.c's, build_scene.c's and
 * build_scene_model.c's; JobPool_Instance 006925a0 is loop_entity.c's.) notes/fx.md. */
#include "crt_atexit.h"
#include "fx.h"
#include "build_series_init.h"
#include "fx_util.h"
#include "build_alloc.h"
#include "build_vehicle.h"
#include "phys_crt.h"

/* a thiscall with no arguments (a constructor / destructor of the iterators) */
static void call_this0(rx_addr fn, rx_addr obj, rx_addr ret) {
    void (*f)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
    if (!f) { rx_regs in = fx_regs(); in.r[FX_ECX] = obj; rx_call_original_ex(fn, &in, 0, 0, 0); return; }
    const rx_addr s = fx_push(0, 0, ret);
    f(obj);
    fx_pop(s);
}

/* 00402be0 (cdecl; plain ret) -> EAX = 0x009e30e4: constructs the static list 0x9e30e4: its container proxy (a 4-byte block, ReflexAlignedAlloc,
 * holding the address of the list; a failed allocation leaves 0) and the three counters +0xc / +0x10 / +0x14 (0x9e30f0 .. 0x9e30f8) cleared.
 * Frame: push ecx. */
rx_addr rx_fx_static_9e30e4_construct(void) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_ECX));
    rx_set_sp(sp0 - 4);
    rx_addr proxy;
    { const uint32_t a[1] = {4}; const rx_addr s = fx_push(1, a, 0x00402be8); proxy = rx_reflex_aligned_alloc(4); fx_pop(s); }
    if (proxy != 0) SU32(proxy, 0, 0x009e30e4);
    SU32(0, 0x009e30e4, proxy);
    SU32(0, 0x009e30f0, 0);
    SU32(0, 0x009e30f4, 0);
    SU32(0, 0x009e30f8, 0);
    rx_set_sp(sp0);
    return 0x009e30e4;
}
RX_FUNCTION(0x00402be0, rx_fx_static_9e30e4_construct)

/* ---- static instances behind their init guards ---- */
/* 00402b10: the static at 0x9e30e4 (guard 0x9e30fc bit 0: 00402be0, destroyed by 00911d40). Frame: push ecx. */
rx_addr rx_fx_static_9e30e4(void) {
    SU32(rx_sp(), -4, fx_reg(FX_ECX));
    if ((U32(0, 0x009e30fc) & 1) == 0) {
        SU32(0, 0x009e30fc, U32(0, 0x009e30fc) | 1);
        { const rx_addr s = fx_push(0, 0, 0x00402b29); (void)rx_fx_static_9e30e4_construct(); fx_pop(s); }   /* 00402be0 */
        RX_ATEXIT(0x00911d40);
    }
    return 0x009e30e4;
}
RX_FUNCTION(0x00402b10, rx_fx_static_9e30e4)
/* 00413d20: the series manager 0x9e2188 (guard 0x9e275c bit 0: MXSeriesManager_Construct 005b2370, destroyed by 00911da0).
 * Frame: push ecx. */
rx_addr rx_fx_series_manager(void) {
    SU32(rx_sp(), -4, fx_reg(FX_ECX));
    if ((U32(0, 0x009e275c) & 1) == 0) {
        SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
        /* MXSeriesManager_Construct 005b2370 (build_series_init.c), entered at the original's ESP after the call's return address */
        { const rx_addr sp = rx_sp(); rx_set_sp(sp - 4u); (void)rx_series_manager_construct(); rx_set_sp(sp); }
        RX_ATEXIT(0x00911da0);
    }
    return 0x009e2188;
}
RX_FUNCTION(0x00413d20, rx_fx_series_manager)

/* ---- the MSVC array iterators (stdcall ptr, size, count, fn; RET 0x10) ---- */
/* 004022d0 `vector constructor iterator': fn (thiscall) on each element, first to last. Frame: push edi; (count > 0) push ebx;
 * push ebp; push esi -- EBX = fn, EBP = size, ESI = the element, EDI = the remaining count during the calls. */
void rx_fx_vector_ctor_iterator(rx_addr ptr, uint32_t size, int32_t count, rx_addr fn) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.edi);
    int32_t n = count - 1;
    if (n >= 0) {
        SU32(sp0, -8, sv.ebx); SU32(sp0, -0xc, sv.ebp); SU32(sp0, -0x10, sv.esi);
        rx_set_sp(sp0 - 0x10);
        fx_set_reg(FX_EBX, fn); fx_set_reg(FX_EBP, size);
        for (rx_addr p = ptr; n >= 0; n--, p += size) {
            fx_set_reg(FX_ESI, p); fx_set_reg(FX_EDI, (uint32_t)n);
            call_this0(fn, p, 0x004022f4);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004022d0, rx_fx_vector_ctor_iterator)
/* 00402500 `vector destructor iterator': fn on each element, last to first. Frame: push ebx; push esi; push edi; (count > 0)
 * push ebp -- EBX = size, ESI = the element, EDI = the remaining count, EBP = fn. */
void rx_fx_vector_dtor_iterator(rx_addr ptr, uint32_t size, int32_t count, rx_addr fn) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx); SU32(sp0, -8, sv.esi); SU32(sp0, -0xc, sv.edi);
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_EBX, size);
    rx_addr p = ptr + size * (uint32_t)count;
    int32_t n = count - 1;
    if (n >= 0) {
        SU32(sp0, -0x10, sv.ebp);
        rx_set_sp(sp0 - 0x10);
        fx_set_reg(FX_EBP, fn);
        for (; n >= 0; n--) {
            p -= size;
            fx_set_reg(FX_ESI, p); fx_set_reg(FX_EDI, (uint32_t)n);
            call_this0(fn, p, 0x00402526);
        }
    }
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00402500, rx_fx_vector_dtor_iterator)

/* ---- small fills ---- */
/* 006c8230 (ESI = n, EDI = dst, EDX = src*): n copies of *src at dst (std::uninitialized_fill_n of dwords) -> dst + n.
 * Frame (n > 0): push ebx. */
rx_addr rx_fx_fill_dwords(uint32_t n, rx_addr dst, rx_addr src) {
    if (n != 0) {
        SU32(rx_sp(), -4, fx_reg(FX_EBX));
        for (uint32_t i = 0; i < n; i++) SU32(dst, 4 * i, U32(src, 0));
    }
    return dst + 4 * n;
}
RX_FUNCTION(0x006c8230, rx_fx_fill_dwords)
/* 006cb180 (EDX = scorer): a combo starts: the 100 combo slots (+0x140) and +0x2d8 cleared, the state +0x124 = 1 (running).
 * Frame: push edi. */
void rx_fx_stunt_combo_start(rx_addr P) {
    SU32(rx_sp(), -4, fx_reg(FX_EDI));
    SU32(P, 0x2d8, 0);
    for (uint32_t i = 0; i < 100; i++) SU32(P, 0x140 + 4 * i, 0);
    SU32(P, 0x124, 1);
}
RX_FUNCTION(0x006cb180, rx_fx_stunt_combo_start)
/* 006ccf60 (EAX = scorer; stdcall v; RET 4): a freestyle activity (kind 2) adds v to the counter +0x30 -> whether it did */
uint32_t rx_fx_stunt_add_30(rx_addr P, float v) {
    if (U32(U32(U32(P, 0x128), 0x94), 0x21c) != 2) return 0;
    SF32(P, 0x30, rx_adds(F32(P, 0x30), v));
    return 1;
}
RX_FUNCTION(0x006ccf60, rx_fx_stunt_add_30)

/* ---- the package request queue (the Database loader thread's requests, drained by the main thread: 0041b410) ----
 * Queue Q (the database manager 0x00d72a30): +4 the package cache C, +0x3c the lock, +0x54 the pending list {count, head, tail}; a node (0x40
 * bytes): +0 the package name, +0x1c the database name (msvcp90 std::strings), +0x38 previous, +0x3c next. The std::string members called through
 * the msvcp90 import slots: 0x009161a4 string(), 0x009161a8 operator=(const string&), 0x009161ac string(const char*), 0x009161b0
 * string(const string&), 0x009161b4 ~string() (crt_cpp.c). Each call here is made at the original's ESP with its return address. */
static void str_ctor(rx_addr s, rx_addr ret) { const rx_addr x = fx_push(0, 0, ret); crt_str_ctor(s); fx_pop(x); }
static void str_ctor_cstr(rx_addr s, rx_addr p, rx_addr ret) { const uint32_t a[1] = {p}; const rx_addr x = fx_push(1, a, ret); crt_str_ctor_cstr(s, p); fx_pop(x); }
static void str_ctor_copy(rx_addr s, rx_addr r, rx_addr ret) { const uint32_t a[1] = {r}; const rx_addr x = fx_push(1, a, ret); crt_str_ctor_copy(s, r); fx_pop(x); }
static void str_assign(rx_addr s, rx_addr r, rx_addr ret) { const uint32_t a[1] = {r}; const rx_addr x = fx_push(1, a, ret); crt_str_assign_sub(s, r, 0, 0xffffffffu); fx_pop(x); }
static void str_dtor(rx_addr s, rx_addr ret) { const rx_addr x = fx_push(0, 0, ret); crt_str_dtor(s); fx_pop(x); }

/* ---- 0041bdd0 (ESI = the node; stdcall two strings by value (0x38 bytes); RET 0x38) -> EAX = the node: the node's two strings made and assigned from
 * the arguments, the links cleared, the argument strings destroyed (the callee owns its by-value arguments). Frame: push edi. */
rx_addr rx_fx_package_node_construct(rx_addr node, rx_addr args) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, fx_reg(FX_EDI));
    rx_set_sp(sp0 - 4);
    fx_set_reg(FX_EDI, node + 0x1c);
    str_ctor(node, 0x0041bdd9);
    str_ctor(node + 0x1c, 0x0041bde4);
    str_assign(node, args, 0x0041bdf1);
    str_assign(node + 0x1c, args + 0x1c, 0x0041bdfe);
    SU32(node, 0x38, 0);
    SU32(node, 0x3c, 0);
    str_dtor(args + 0x1c, 0x0041be10);
    str_dtor(args, 0x0041be1a);
    rx_set_sp(sp0);
    return node;
}
RX_FUNCTION(0x0041bdd0, rx_fx_package_node_construct)

/* ---- 0041bca0 (EDI = the list; stdcall two strings by value (0x38 bytes); RET 0x38): push_back: a 0x40-byte node (ReflexAlignedAlloc) built from
 * copies of the arguments (0041bdd0) and linked behind the tail (the first node also becomes the head), the count +1; a failed allocation links a
 * null node. The argument strings are destroyed. Frame: push ebx; push esi. */
void rx_fx_package_queue_push(rx_addr list, rx_addr args) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx);
    SU32(sp0, -8, sv.esi);
    rx_set_sp(sp0 - 8);
    rx_addr node;
    { const uint32_t a[1] = {0x40}; const rx_addr x = fx_push(1, a, 0x0041bca9); node = rx_reflex_aligned_alloc(0x40); fx_pop(x); }
    fx_set_reg(FX_ESI, node);
    if (node != 0) {
        const rx_addr blk = sp0 - 8 - 0x38;
        rx_set_sp(blk);
        fx_set_reg(FX_EBX, blk);
        str_ctor_copy(blk, args, 0x0041bcc4);
        str_ctor_copy(blk + 0x1c, args + 0x1c, 0x0041bcd2);
        {   /* call 0041bdd0 (ESI = the node; the block is its by-value arguments) */
            const rx_addr x = fx_push(0, 0, 0x0041bcd7);
            node = rx_fx_package_node_construct(node, blk);
            fx_pop(x);
        }
        rx_set_sp(sp0 - 8);
    }
    if (U32(list, 0) == 0) {
        SU32(list, 4, node);
    } else {
        SU32(U32(list, 8), 0x3c, node);
        SU32(node, 0x38, U32(list, 8));
    }
    SU32(list, 0, U32(list, 0) + 1);
    SU32(list, 8, node);
    str_dtor(args + 0x1c, 0x0041bd00);
    str_dtor(args, 0x0041bd0a);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0041bca0, rx_fx_package_queue_push)

/* ---- 0041bd10 (ESI = the pending list; stdcall out; RET 4) -> EAX = out: pop_front: the first node's two strings copied into `out` and
 * `out + 0x1c`, the head moved to its next (+0x3c; unchanged when there is none), the new head's previous cleared, the node destroyed and
 * freed, the count -1. Frame: push ebx / ebp / edi. */
rx_addr rx_fx_package_queue_pop(rx_addr out, rx_addr list) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx);
    SU32(sp0, -8, sv.ebp);
    SU32(sp0, -0xc, sv.edi);
    rx_set_sp(sp0 - 0xc);
    fx_set_reg(FX_EBX, out);
    str_ctor(out, 0x0041bd1f);
    fx_set_reg(FX_EBP, out + 0x1c);
    str_ctor(out + 0x1c, 0x0041bd2a);
    const rx_addr node = U32(list, 4);
    fx_set_reg(FX_EDI, node);
    str_assign(out, node, 0x0041bd36);
    str_assign(out + 0x1c, node + 0x1c, 0x0041bd42);
    if (U32(U32(list, 4), 0x3c) != 0) SU32(list, 4, U32(U32(list, 4), 0x3c));
    SU32(U32(list, 4), 0x38, 0);
    if (node != 0) {
        str_dtor(node + 0x1c, 0x0041bd66);
        str_dtor(node, 0x0041bd6e);
        { const uint32_t a[1] = {node}; const rx_addr x = fx_push(1, a, 0x0041bd74); rx_reflex_aligned_free(node); fx_pop(x); }
    }
    SU32(list, 0, U32(list, 0) - 1);
    fx_restore(sv);
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x0041bd10, rx_fx_package_queue_pop)

/* ---- 005dffc0 (ESI = the language index; plain ret) -> EAX = the name's characters: the language names are std::strings (0x1c bytes) in the vector
 * [0x00af6950, 0x00af6954); an index outside it reports through _invalid_parameter_noinfo. */
rx_addr rx_fx_language_name(uint32_t index) {
    const int32_t bytes = (int32_t)(U32(0, 0x00af6954) - U32(0, 0x00af6950));
    if (index >= (uint32_t)(bytes / 28)) crt_invalid_parameter_noinfo();
    const rx_addr str = U32(0, 0x00af6950) + 28u * index;
    return U32(str, 0x18) >= 0x10 ? U32(str, 4) : str + 4;
}
RX_FUNCTION(0x005dffc0, rx_fx_language_name)

/* ---- 005dbf00 (EAX = the destination (0x40 bytes); stdcall name; RET 4) -> EAX = the destination: the name with the UI language's suffix
 * ("%s.%s": 0078fd60 = _vsnprintf of size 0x40) when a UI language is selected ([0x00a94074] >= 0), else the name copied (strncpy 0x3f and a
 * terminator). Frame: push ebx / esi / edi. */
rx_addr rx_fx_localized_name(rx_addr dst, rx_addr name) {
    const rx_addr sp0 = rx_sp();
    const fx_saved sv = fx_save();
    SU32(sp0, -4, sv.ebx);
    SU32(sp0, -8, sv.esi);
    SU32(sp0, -0xc, sv.edi);
    rx_set_sp(sp0 - 0xc);
    const uint32_t language = U32(0, 0x00a94074);
    fx_set_reg(FX_EDI, dst);
    if ((int32_t)language >= 0) {
        rx_addr lang;
        fx_set_reg(FX_ESI, language);
        { const rx_addr x = fx_push(0, 0, 0x005dbf18); lang = rx_fx_language_name(language); fx_pop(x); }
        const uint32_t a[3] = {0x0093a024, name, lang};
        const rx_addr x = fx_push(3, a, 0x005dbf29);
        fx_set_reg(FX_ESI, 0x40);
        rx_format_buffer(dst, 0x40, 0x0093a024, rx_sp() + 8);       /* 0078fd60: _vsnprintf(dst, 0x40, "%s.%s", name, language) */
        fx_pop(x);
    } else {
        const uint32_t a[3] = {dst, name, 0x3f};
        FX_CRT(3, a, 0x005dbf3e, crt_strncpy(dst, name, 0x3f));
        SU8(dst, 0x3f, 0);
    }
    fx_restore(sv);
    rx_set_sp(sp0);
    return dst;
}
RX_FUNCTION(0x005dbf00, rx_fx_localized_name)

/* a thiscall virtual of the database object (engine context +8) with n stack arguments, at the current ESP */
static uint32_t db_vcall(rx_addr db, uint32_t off, int n, const uint32_t* a, rx_addr ret) {
    const rx_addr fn = U32(U32(db, 0), off);
    uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
    const rx_addr x = fx_push(n, a, ret);
    uint32_t r;
    if (f) r = f(db, n > 0 ? a[0] : 0, n > 1 ? a[1] : 0);
    else r = rx_call_original(fn, RX_THISCALL, db, n, a);
    fx_pop(x);
    return r;
}

/* ---- 0041b4b0 (LTCG: ECX = the package name's characters, EDX = the database name's characters; stdcall Q; RET 4): a package request. On the
 * loader thread (Database::vf1 0084a7a0: the current thread is the loader's, db +0x100) it is queued: the two names are made into
 * strings (0x9161ac), copied (0x9161b0) into the by-value argument block of 0041bca0, which appends them to Q +0x54 under Q +0x3c's lock.
 * On any other thread it is made now: the database requested (00543750, async 0), the 0x40-byte request name built from the database
 * name (005dbf00: with the UI language suffix), checked with the database's vf9 (+0x24: name, buffer) - taken as it is when that accepts the
 * suffixed one, else the plain database name if vf9 accepts it, else nothing - and added to (00543830) and requested from (00543b30, async 1)
 * the package cache Q +4. Frame: sub esp, 0x80 (the /GS cookie at +0x7c); ebx / ebp / esi / edi; the strings at E0 + 0x10 / + 0x2c, the request
 * buffer at E0 + 0x48. */
void rx_fx_package_request(rx_addr name, rx_addr db_name, rx_addr Q) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0x80, E0 = E - 0x10;
    const fx_saved sv = fx_save();
    SU32(E, 0x7c, U32(0, 0x009b45e4) ^ E);
    SU32(E, -4, sv.ebx);
    SU32(E, -8, sv.ebp);
    SU32(E, -0xc, sv.esi);
    SU32(E, -0x10, sv.edi);
    rx_set_sp(E0);
    fx_set_reg(FX_EBP, Q);
    fx_set_reg(FX_ESI, name);
    fx_set_reg(FX_EDI, db_name);
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    if (db_vcall(db, 4, 0, 0, 0x0041b4df) != 0) {
        const rx_addr lock = Q + 0x3c, A = E0 + 0x10, B = E0 + 0x2c, K = E0 - 0x38;
        fx_set_reg(FX_EBX, lock);
        fx_enter_cs(lock, 0x0041b4ed);
        str_ctor_cstr(A, db_name, 0x0041b4f8);
        str_ctor_cstr(B, name, 0x0041b503);
        rx_set_sp(K);
        fx_set_reg(FX_ESI, K);
        str_ctor_copy(K, B, 0x0041b515);
        str_ctor_copy(K + 0x1c, A, 0x0041b523);
        fx_set_reg(FX_EDI, Q + 0x54);
        {   /* call 0041bca0 (EDI = the list; RET 0x38 pops the argument block) */
            const rx_addr x = fx_push(0, 0, 0x0041b52b);
            rx_fx_package_queue_push(Q + 0x54, K);
            fx_pop(x);
        }
        rx_set_sp(E0);
        str_dtor(B, 0x0041b535);
        str_dtor(A, 0x0041b53f);
        fx_leave_cs(lock, 0x0041b546);
    } else {
        const rx_addr buf = E0 + 0x48;
        fx_set_reg(FX_EBX, 0);
        { const uint32_t a[1] = {0}; const rx_addr x = fx_push(1, a, 0x0041b553); rx_pkgcache_request_db(name, 0); fx_pop(x); }   /* ESI = the name */
        SU8(buf, 0, 0);
        { const uint32_t a[3] = {buf + 1, 0, 0x3f}; FX_CRT(3, a, 0x0041b564, crt_memset(buf + 1, 0, 0x3f)); }
        { const uint32_t a[1] = {db_name}; const rx_addr x = fx_push(1, a, 0x0041b571); rx_fx_localized_name(buf, db_name); fx_pop(x); }
        int accepted;
        {   /* database vf9 (+0x24): (name, buffer) */
            const uint32_t a[2] = {name, buf};
            accepted = db_vcall(db, 0x24, 2, a, 0x0041b587) != 0;
        }
        if (accepted) {
            const uint32_t a[3] = {buf, buf, 0x3f};
            FX_CRT(3, a, 0x0041b5bb, crt_strncpy(buf, buf, 0x3f));
        } else {
            const uint32_t a[2] = {name, db_name};
            if (db_vcall(db, 0x24, 2, a, 0x0041b5a9) == 0) goto done;
            const uint32_t b[3] = {buf, db_name, 0x3f};
            FX_CRT(3, b, 0x0041b5bb, crt_strncpy(buf, db_name, 0x3f));
        }
        SU8(buf, 0x3f, 0);
        fx_set_reg(FX_EDI, Q + 4);
        { const uint32_t a[2] = {buf, name}; const rx_addr x = fx_push(2, a, 0x0041b5d3); rx_pkgcache_add(Q + 4, buf, name); fx_pop(x); }
        { const uint32_t a[2] = {buf, 1}; const rx_addr x = fx_push(2, a, 0x0041b5e1); rx_pkgcache_request(Q + 4, buf, 1); fx_pop(x); }
    }
done:
    rx_set_sp(E0);
    SU32(E, -0x14, 0x0041b5f3);
    phys_security_check_cookie(U32(E, 0x7c) ^ E);
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0041b4b0, rx_fx_package_request)

/* ---- 0041b410 (stdcall queue; RET 4): the localized-package request queue (0xd72a30): while requests are pending (+0x54),
 * one is taken under the lock (+0x3c; 0041bd10 into a local pair of std::strings) and handled (0041b4b0: ECX / EDX the two
 * strings' characters), the strings destroyed (IAT 0x9161b4). Frame: sub esp, 0x40 (/GS cookie at +0x3c); ebx / ebp / esi /
 * edi: E = sp0 - 0x50: +0x14 / +0x30 the strings. Callees not in the core (the queue is empty in the frames measured). */
void rx_fx_package_queue_drain(rx_addr Q) {
    const rx_addr sp0 = rx_sp(), e0 = sp0 - 0x40, E = e0 - 0x10;
    const fx_saved sv = fx_save();
    SU32(e0, 0x3c, U32(0, 0x009b45e4) ^ e0);
    fx_push_regs(e0, 1, (int[]){FX_EBX});
    fx_set_reg(FX_EBX, Q);
    fx_push_regs(e0 - 4, 2, (int[]){FX_EBP, FX_ESI});
    fx_set_reg(FX_ESI, Q + 0x54);
    fx_push_regs(e0 - 0xc, 1, (int[]){FX_EDI});
    rx_set_sp(E);
    if (U32(Q, 0x54) != 0) {
        fx_set_reg(FX_EBP, U32(0, 0x0091600c)); fx_set_reg(FX_EDI, Q + 0x3c);
        do {
            fx_enter_cs(Q + 0x3c, 0x0041b43b);
            { const uint32_t a[1] = {E + 0x14}; const rx_addr x = fx_push(1, a, 0x0041b445); rx_fx_package_queue_pop(E + 0x14, Q + 0x54); fx_pop(x); }   /* 0041bd10 (ESI = the list) */
            fx_leave_cs(Q + 0x3c, 0x0041b44c);
            const rx_addr s2 = U32(E, 0x48) >= 0x10 ? U32(E, 0x34) : E + 0x34;
            const rx_addr s1 = U32(E, 0x2c) >= 0x10 ? U32(E, 0x18) : E + 0x18;
            fx_set_reg(FX_ECX, s1); fx_set_reg(FX_EDX, s2);
            { const uint32_t a[1] = {Q}; const rx_addr x = fx_push(1, a, 0x0041b473); rx_fx_package_request(s1, s2, Q); fx_pop(x); }   /* 0041b4b0 */
            str_dtor(E + 0x30, 0x0041b47d);                                                                                     /* ~string (0x9161b4) */
            str_dtor(E + 0x14, 0x0041b487);
        } while (U32(Q, 0x54) != 0);
    }
    SU32(e0, -4, 0x0041b49b);   /* __security_check_cookie's return */
    fx_restore(sv);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0041b410, rx_fx_package_queue_drain)
