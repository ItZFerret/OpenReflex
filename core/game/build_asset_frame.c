/* build_asset_frame.c -- the Database's end-of-frame removal of the asset handles queued for removal (group rx_build_asset).
 *
 * Database::vf22 0084ce00 (build_asset.c) queues a handle in one of two pending lists (+0x64 / +0x7c, std::vector<AssetHandle*> of 0x18
 * bytes: +0 proxy, +0xc first, +0x10 last, +0x14 end) chosen by the parity of the frame its caller passes (the engine's update counter,
 * [[0xa95844] + 0x10] + 0x58). Engine::Frame 0084f260 (loop_engine.c rx_engine_frame) calls vf33 after the render with the engine's
 * render counter (+0x54): that frame's list is drained -- each handle's asset loses the reference vf22's caller gave up (vf31 0084af30
 * with package -1) and the handle leaves its type's table and is deleted when nothing else references it (vf32) -- and cleared.
 * The handle virtuals (AssetHandle, 0x50 bytes; every one of the 37 handle vtables shares slots 2..8): vf2 004dce30 the name (+4), vf3
 * 0082a1f0 the reference count (+0x48), vf4 00884610 the asset (+0x44), vf5 004dce40 the asset set, vf7 004dce50 / vf8 004dce60 the
 * count + 1 / - 1; vf1 the type tag (build_asset_types.c); vf0 the scalar deleting destructor: IAssetHandle 00668b20 (33 classes),
 * IconGeometryAssetHandle 005e15c0 (uicmpnt / texatlas / icongeom), SplitTriangleCollisionAssetHandle 007726b0 (collis 0x0096dd90). */
#include "build_asset.h"
#include "build_alloc.h"
#include "crt.h"
#include "phys_coll_misc.h"
#include "rx_vcall.h"
#include "vehicle_atv_destroy.h"
#include "../src/rx_os.h"

#define IASSETHANDLE_VT 0x00976934u   /* IAssetHandle::vftable (the destructors' base vtable) */
#define OS_ENTER_CS     0x0091600cu   /* EnterCriticalSection */
#define OS_LEAVE_CS     0x009160f8u   /* LeaveCriticalSection */

/* ---- AssetHandle virtuals ---- */

/* AssetHandle::vf3 0082a1f0 (thiscall): the reference count +0x48 */
uint32_t rx_asset_handle_refs(rx_addr h) { return U32(h, 0x48); }
RX_FUNCTION(0x0082a1f0, rx_asset_handle_refs)

/* AssetHandle::vf8 004dce60 (thiscall): one reference less (dec dword [ecx + 0x48]; EAX is left as the caller had it) */
void rx_asset_handle_unref(rx_addr h) { SU32(h, 0x48, U32(h, 0x48) - 1u); }
RX_FUNCTION(0x004dce60, rx_asset_handle_unref)

/* 0084a2c0 (thiscall): the IAssetHandle base destructor body: the base vtable written back */
void rx_asset_handle_base_dtor(rx_addr h) { SU32(h, 0, IASSETHANDLE_VT); }
RX_FUNCTION(0x0084a2c0, rx_asset_handle_base_dtor)

/* IAssetHandle::vf0 00668b20 (thiscall handle, flags; RET 4): the base vtable, freed (ReflexAlignedFree) with flag 1; returns the
 * handle. Frame: push esi. */
rx_addr rx_asset_handle_sdtor(rx_addr h, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                   /* push esi */
    SU32(h, 0, IASSETHANDLE_VT);
    if (flags & 1) {
        const rx_addr s = rx_sp();
        SU32(s, -4, h); rx_set_sp(s - 8);                 /* push esi; call 0078fc50 */
        rx_reflex_aligned_free(h);
        rx_set_sp(s);
    }
    rx_set_sp(sp0);
    return h;
}
RX_FUNCTION(0x00668b20, rx_asset_handle_sdtor)

/* IconGeometryAssetHandle::sdtor 005e15c0 (thiscall handle, flags; RET 4; an unoptimised frame: push ebp; mov ebp, esp; push ecx =
 * the handle): 0084a2c0, freed with flag 1; returns the handle. */
rx_addr rx_icon_geometry_handle_sdtor(rx_addr h, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4;                          /* push ebp; mov ebp, esp */
    SU32(ebp, -4, h);                                     /* push ecx; mov [ebp - 4], ecx */
    rx_set_sp(ebp - 4);
    { const rx_addr s = rx_sp(); rx_set_sp(s - 4); rx_asset_handle_base_dtor(h); rx_set_sp(s); }
    if (flags & 1) {
        const rx_addr s = rx_sp();
        SU32(s, -4, h); rx_set_sp(s - 8);
        rx_reflex_aligned_free(h);
        rx_set_sp(s);
    }
    rx_set_sp(sp0);
    return h;
}
RX_FUNCTION(0x005e15c0, rx_icon_geometry_handle_sdtor)

/* RavenPhysics::SplitTriangleCollisionAssetHandle::sdtor 007726b0 (thiscall handle, flags; RET 4): the vector at +0x50 destroyed
 * (0081f6e0, EAX = handle + 0x50), the base vtable, freed with flag 1; returns the handle. Frame: push esi. */
rx_addr rx_split_triangle_handle_sdtor(rx_addr h, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                   /* push esi */
    { const rx_addr s = rx_sp(); rx_set_sp(s - 4); rx_vehicle_sim_vector_destruct(h + 0x50); rx_set_sp(s); }
    SU32(h, 0, IASSETHANDLE_VT);
    if (flags & 1) {
        const rx_addr s = rx_sp();
        SU32(s, -4, h); rx_set_sp(s - 8);
        rx_reflex_aligned_free(h);
        rx_set_sp(s);
    }
    rx_set_sp(sp0);
    return h;
}
RX_FUNCTION(0x007726b0, rx_split_triangle_handle_sdtor)

/* ---- the Database ---- */

/* Database::vf32 0084af90 (thiscall db, handle; RET 4): the handle gives up one reference (vf8); with none left (vf3 == 0) it leaves its
 * type's handle table -- the type entry (a CRITICAL_SECTION at +0, the name table at +0x18) found by the handle's type tag (vf1) in the
 * table +0x44 (0084dea0), the name (vf2) erased from the entry's table (0084dd30) under that lock -- and is deleted (vf0(1)).
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0xc; push ebx / esi / edi. B = the aligned EBP; locals: B - 0xc the found entry
 * (then the name for 0084dd30), B - 8 / B - 4 the type tag (0084dea0's key). */
void rx_database_remove_handle(rx_addr db, rx_addr h) {
    const rx_addr sp0 = rx_sp();
    const rx_addr B = (sp0 - 4) & ~7u;
    const rx_addr S = B - 0xc - 12;
    rx_set_sp(S);
    rx_vcall(h, 0x20, 0, 0);                              /* vf8: one reference less */
    if (rx_vcall(h, 0xc, 0, 0) == 0) {                    /* vf3: the reference count */
        const uint64_t tag = rx_asset_handle_type(h);    /* vf1: EDX:EAX */
        SU32(B, -8, (uint32_t)tag); SU32(B, -4, (uint32_t)(tag >> 32));
        SU32(S, -4, B - 0xc);                             /* push the result's address */
        rx_set_sp(S - 8);
        uint32_t entry = 0;
        if (rx_tagtab_find(db + 0x44, (uint32_t)tag, (uint32_t)(tag >> 32), &entry)) SU32(B, -0xc, entry);   /* 0084dea0 (ESI db + 0x44) */
        rx_set_sp(S);
        const rx_addr cs = U32(B, -0xc);
        (void)RX_OS(OS_ENTER_CS, cs);
        const rx_addr name = rx_vcall(h, 8, 0, 0);       /* vf2: the name */
        SU32(S, -4, B - 0xc);                             /* push the key slot's address */
        SU32(B, -0xc, name);
        rx_set_sp(S - 8);
        (void)rx_strtab_remove(cs + 0x18, B - 0xc);      /* 0084dd30 (EDI = the entry's name table) */
        rx_set_sp(S);
        (void)RX_OS(OS_LEAVE_CS, cs);
        const uint32_t one = 1;
        rx_vcall(h, 0, 1, &one);                          /* vf0(1): deleted */
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0084af90, rx_database_remove_handle)

/* 0084d9c0 (EDI = a std::vector<T*>): cleared -- erase(begin(), end()) with the checked iterators (the _invalid_parameter_noinfo tests
 * of a reversed range), 00842fa0 (the begin iterator advanced by 0) and the erase 007cf450 (ESI = the vector) into a result iterator.
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0x10; push ebx / esi. B = the aligned EBP; locals: B - 0x10 / B - 0xc the begin
 * iterator {proxy, position}, B - 8 the proxy (then the erase's result {proxy, position} at B - 8 / B - 4). */
void rx_ptr_vector_clear(rx_addr v) {
    const rx_addr sp0 = rx_sp();
    const rx_addr B = (sp0 - 4) & ~7u;
    const rx_addr S = B - 0x10 - 8;
    rx_set_sp(S);
    const uint32_t first = U32(v, 0xc), last = U32(v, 0x10);
    if ((uint32_t)((int32_t)(last - first) >> 2) != 0) {  /* sar ecx, 2; test ecx, ecx; jbe */
        if (first > last) crt_invalid_parameter_noinfo();
        const uint32_t begin = U32(v, 0xc);
        SU32(B, -8, U32(v, 0));
        if (begin > U32(v, 0x10)) crt_invalid_parameter_noinfo();
        SU32(B, -0xc, begin);
        SU32(S, -4, 0);                                   /* push 0 */
        SU32(B, -0x10, U32(v, 0));
        rx_set_sp(S - 8);
        phys_iterator_advance(B - 0x10, 0);               /* 00842fa0 (ESI = the iterator; RET 4) */
        rx_set_sp(S);
        const uint32_t proxy = U32(B, -8), pos = U32(B, -0xc), it_proxy = U32(B, -0x10);
        SU32(S, -4, last); SU32(S, -8, proxy); SU32(S, -0xc, pos); SU32(S, -0x10, it_proxy); SU32(S, -0x14, B - 8);
        rx_set_sp(S - 0x18);
        (void)phys_vector_erase(v, B - 8, pos, last);     /* 007cf450 (ESI = the vector; RET 0x14) */
        rx_set_sp(S);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0084d9c0, rx_ptr_vector_clear)

/* Database::vf33 0084cc80 (thiscall db, frame; RET 4): under the CRITICAL_SECTION +0x62b3c, every handle of the pending list of the
 * frame's parity (+0x64 + 0x18 * (frame & 1); the count taken once, each element read with the checked vector's bound test): vf31
 * (handle, -1) then vf32 (handle); the list cleared (0084d9c0) and the lock left by a tail jump to LeaveCriticalSection (the lock's
 * address is written over the frame argument's slot). Frame: push ecx (the lock's address is stored there); push ebx / ebp / esi / edi. */
void rx_database_drain_removals(rx_addr db, uint32_t frame) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = sp0 - 0x14;
    const rx_addr cs = db + 0x62b3c;
    SU32(sp0, -4, cs);
    rx_set_sp(S);
    (void)RX_OS(OS_ENTER_CS, cs);
    const rx_addr list = db + 0x64 + 0x18u * (frame & 1u);
    const int32_t n = (I32(list, 0x10) - I32(list, 0xc)) >> 2;
    for (int32_t i = 0; i < n; i++) {
        if ((uint32_t)i >= (uint32_t)((I32(list, 0x10) - I32(list, 0xc)) >> 2)) crt_invalid_parameter_noinfo();
        const uint32_t a[2] = {U32(U32(list, 0xc), 4u * (uint32_t)i), 0xffffffffu};
        rx_vcall(db, 0x7c, 2, a);                         /* vf31 0084af30 (handle, -1) */
        if ((uint32_t)i >= (uint32_t)((I32(list, 0x10) - I32(list, 0xc)) >> 2)) crt_invalid_parameter_noinfo();
        const uint32_t h = U32(U32(list, 0xc), 4u * (uint32_t)i);
        rx_vcall(db, 0x80, 1, &h);                        /* vf32 0084af90 (handle) */
    }
    rx_set_sp(S - 4);                                     /* call 0084d9c0 (EDI = the list) */
    rx_ptr_vector_clear(list);
    rx_set_sp(sp0);
    SU32(sp0, 4, cs);                                     /* mov [esp + 4], eax; jmp [LeaveCriticalSection] */
    (void)RX_OS(OS_LEAVE_CS, cs);
}
RX_FUNCTION(0x0084cc80, rx_database_drain_removals)
