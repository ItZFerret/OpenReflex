/* fx_terrain.c -- the terrain surface's per-frame GPU sync on the main thread (HFTerrainSurface::vf14 007c4fa0: the level
 * reset of the deformation targets, DeformablePatch_ReadbackToSysmem 007cb930, TerrainTracks_FlushPending 007cbe30 for the
 * three track sets, TerrainTracks_Reset 007cc630), and the vehicles' mud (MudRecord_Update 00408c50 with the terrain registry
 * 00402d90). The deformation pass the original renders on the GPU after the sync (007caf70, ps_hft_deform) is fx_deform.c.
 * notes/fx.md; _re/notes/terrain-deformation.md, rt-bump.md, mud.md.
 *
 * PlatformDeformablePatch P (= surface +0x230): +8 / +0xc the two R32F render target textures, +0x10 the system-memory surface
 * the physics reads (= surface +0x240), +0x14 idx (texture read = rt[idx], written = rt[!idx]), +0x18 q (write queue), +0x20 /
 * +0x24 the targets' level-0 surfaces, +0x30 + q 0x208 stamp offsets (count +0x230 + q 0x208), +0x440 + 4 q triangle counts,
 * +0x448 + q 0x1e04 stamp vertices (float count +0x2248 + q 0x1e04).
 * TerrainTracks T (0x38): +4 ring length, +8 capacity, +0xc slot count, +0x10 IB, +0x14 VB, +0x1c vertex stride, +0x24 slots
 * (0xc0 each: +0 chunk table {vb byte offset, ib start index, ib byte offset}, +0xc generation*, +0x44 / +0x6c the two staged
 * vertex sets (4 SHORT4N each) with their chunk / vertex indices +0x64 / +0x68 and +0x8c / +0x90, +0x98 released, +0x9c
 * allocated, +0xa0 just cut, +0xa4 first point), +0x2c rebuild flag. */
#include "fx.h"
#include "fx_util.h"
#include "phys_terrain.h"
#include "world_veh.h"
#include "../src/rx_d3d.h"


/* memset(d, 0, n) through the exe's thunk 008fa06a at the current ESP (cdecl; the call site's return address), its frame with
 * the register model's EBP / EDI */
static void memset_call(rx_addr d, uint32_t n, rx_addr ret) {
    const uint32_t a[3] = {d, 0, n};
    FX_CRT(3, a, ret, crt_memset(d, 0, n));
}
/* a COM method of a D3D object at the current ESP (stdcall, this first): the arguments and the call site's return address */
static uint32_t d3d_call(rx_addr obj, uint32_t slot, int n, const uint32_t* a, rx_addr ret) {
    const rx_addr s = fx_push(n, a, ret);
    rx_set_sp(s);                                             /* rx_d3d_call pushes the arguments itself */
    const uint32_t r = rx_d3d_call(obj, slot, n, a);
    SU32(s - 4u * (uint32_t)(n + 1), 0, ret);
    fx_pop(s);
    return r;
}
static rx_addr device(void) { return U32(U32(0, 0x00a95960), 0x1ec); }   /* the renderer's IDirect3DDevice9 */

/* ---- 007cc630 TerrainTracks_Reset (EAX = tracks): every slot back to empty (generation 1, no staged vertices, last direction
 * from the constant 0x9cb4c0, cut and first-point flags set, lengths and V 0); the rebuild flag set. Frame: push esi (push edi). */
void rx_fx_tracks_reset(rx_addr T) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0 - 4, 0, fx_reg(FX_ESI));
    if (I32(T, 0xc) > 0) {
        SU32(sp0 - 8, 0, fx_reg(FX_EDI));
        for (int32_t i = 0; i < I32(T, 0xc); i++) {
            const uint32_t o = 0xc0u * (uint32_t)i;
            SU32(U32(U32(T, 0x24) + o, 0xc), 0, 1);
            SU32(U32(T, 0x24) + o, 0x68, 0xffffffffu);
            SU32(U32(T, 0x24) + o, 8, 0);
            const rx_addr S = U32(T, 0x24) + o;
            SU32(S, 0x20, U32(0, 0x009cb4c0)); SU32(S, 0x24, U32(0, 0x009cb4c4)); SU32(S, 0x28, U32(0, 0x009cb4c8));
            SF32(U32(T, 0x24) + o, 0x40, 0.0f);
            SU32(U32(T, 0x24) + o, 0xa0, 1);
            SU32(U32(T, 0x24) + o, 0xa4, 1);
            SF32(U32(T, 0x24) + o, 0x14, 0.0f);
            SF32(U32(T, 0x24) + o, 0x10, 0.0f);
            SF32(U32(T, 0x24) + o, 0xb0, 0.0f);
        }
    }
    SU32(T, 0x2c, 1);
}
RX_FUNCTION(0x007cc630, rx_fx_tracks_reset)

/* 4 / 2 SHORT4N vertices (8 bytes each) by movq */
static void copy_q(rx_addr d, rx_addr s, int n) { for (int i = 0; i < n; i++) rx_wr64(d + 8u * (uint32_t)i, rx_rd64(s + 8u * (uint32_t)i)); }
/* the strip's 16-bit indices: a chunk's first quad {0, 1, 2, 3, 3, 3} */
static void first_quad(rx_addr ib) {
    SU16(ib, 0, 0); SU16(ib, 2, 1); SU16(ib, 4, 2); SU16(ib, 8, 3); SU16(ib, 6, 3); SU16(ib, 0xa, 3);
}
/* append the indices k, k + 1 and close the strip's degenerate tail (k + 2 / k + 3 equal: both k + 1) */
static void append_pair(rx_addr ib, int32_t k) {
    SU16(ib, 2u * (uint32_t)k, (uint16_t)k); SU16(ib, 2u * (uint32_t)k + 2, (uint16_t)(k + 1));
    if (k + 3 < 0x100 && U16(ib, 2u * (uint32_t)k + 4) == U16(ib, 2u * (uint32_t)(k + 3))) {
        SU16(ib, 2u * (uint32_t)k + 4, (uint16_t)(k + 1)); SU16(ib, 2u * (uint32_t)(k + 3), (uint16_t)(k + 1));
    }
}

/* ---- 007cbe30 TerrainTracks_FlushPending (EAX = tracks): with the rebuild flag (after a reset): the ring length from the
 * capacity, both buffers locked (DISCARD) and rebuilt empty (each slot's chunks laid out: VB 0x800 bytes / IB 0x200 bytes each,
 * zeroed), the flag cleared. Otherwise the ring length (a quarter in the low-detail mode, timing +0x5c), both buffers locked and
 * every live slot's staged vertices copied in: set A (+0x44, chunk +0x64, vertex +0x68): a chunk's first quad (the chunk's IB
 * zeroed first), a strip's first point (4 vertices) or the next pair; set B (+0x6c, +0x8c / +0x90) likewise; then both unlocked.
 * Released slots are only acknowledged. Frame: sub esp, 0x24; ebx / ebp / esi / edi: E = sp0 - 0x34: +0x10 / +0x1c / +0x20
 * counters and offsets, +0x24 / +0x28 (+0x2c / +0x30 when rebuilding) the VB / IB pointers. */
void rx_fx_tracks_flush(rx_addr T) {
    fx_frame f; fx_frame_flat(&f, 0x24, FX_EBX, FX_EBP, FX_ESI, FX_EDI);
    const rx_addr E = f.E;
    fx_set_reg(FX_ESI, T); fx_set_reg(FX_EDI, 0);
    if (U32(T, 0x2c) != 0) {
        SU32(T, 4, U32(T, 8));
        { const uint32_t a[5] = {U32(T, 0x14), 0, 0, E + 0x2c, 0x2000}; d3d_call(U32(T, 0x14), 11, 5, a, 0x007cbe61); }
        { const uint32_t a[5] = {U32(T, 0x10), 0, 0, E + 0x30, 0x2000}; d3d_call(U32(T, 0x10), 11, 5, a, 0x007cbe78); }
        fx_set_reg(FX_EBP, 0);
        SU32(E, 0x10, 0); SU32(E, 0x14, 0); SU32(E, 0x1c, 0); SU32(E, 0x20, 0);
        uint32_t ebp = 0, ebx = 0;
        for (; I32(E, 0x20) < I32(T, 0xc); ) {
            fx_set_reg(FX_EBX, ebx);
            int32_t j = 0;
            if (I32(T, 4) > 0) {
                SU32(E, 0x18, 0);
                do {
                    fx_set_reg(FX_EDI, (uint32_t)j);
                    const rx_addr tab = U32(U32(T, 0x24) + ebx, 0);
                    SU32(tab + U32(E, 0x18), 0, U32(E, 0x14));
                    memset_call(U32(E, 0x2c) + 8u * U32(E, 0x10), 0x800, 0x007cbecc);
                    SU32(E, 0x10, U32(E, 0x10) + 0x100);
                    SU32(E, 0x18, U32(E, 0x18) + 0xc);
                    SU32(E, 0x14, U32(E, 0x14) + (U32(T, 0x1c) << 8));
                    j++;
                } while (j < I32(T, 4));
            }
            uint32_t edi = 0;
            fx_set_reg(FX_EDI, 0);
            SU32(E, 0x18, 0);
            if (I32(T, 4) > 0) {
                for (;;) {
                    const rx_addr tab = U32(U32(T, 0x24) + ebx, 0);
                    SU32(tab + edi, 4, ebp);
                    SU32(U32(U32(T, 0x24) + ebx, 0) + edi, 8, U32(E, 0x1c));
                    memset_call(U32(E, 0x30) + 2u * ebp, 0x200, 0x007cbf2c);
                    const uint32_t k = U32(E, 0x18) + 1;
                    SU32(E, 0x1c, U32(E, 0x1c) + 0x200);
                    ebp += 0x100; edi += 0xc;
                    fx_set_reg(FX_EBP, ebp); fx_set_reg(FX_EDI, edi);
                    SU32(E, 0x18, k);
                    if (!((int32_t)k < I32(T, 4))) break;
                }
            }
            SU32(E, 0x20, U32(E, 0x20) + 1);
            ebx += 0xc0;
            fx_set_reg(FX_EBX, ebx); fx_set_reg(FX_EDI, 0);
        }
        { const uint32_t a[1] = {U32(T, 0x10)}; d3d_call(U32(T, 0x10), 12, 1, a, 0x007cbf73); }
        { const uint32_t a[1] = {U32(T, 0x14)}; d3d_call(U32(T, 0x14), 12, 1, a, 0x007cbf7e); }
        SU32(T, 0x2c, 0);
        fx_frame_leave(&f);
        return;
    }
    if (U32(fx_timing(), 0x5c) != 0) SU32(T, 4, (uint32_t)(I32(T, 8) / 4));
    else SU32(T, 4, U32(T, 8));
    { const uint32_t a[5] = {U32(T, 0x14), 0, 0, E + 0x24, 0}; d3d_call(U32(T, 0x14), 11, 5, a, 0x007cbfc0); }
    { const uint32_t a[5] = {U32(T, 0x10), 0, 0, E + 0x28, 0}; d3d_call(U32(T, 0x10), 11, 5, a, 0x007cbfd3); }
    SU32(E, 0x1c, 0);
    if (I32(T, 0xc) > 0) {
        SU32(E, 0x10, 0);
        for (;;) {
            const rx_addr S = U32(E, 0x10) + U32(T, 0x24);
            fx_set_reg(FX_EDI, S);
            if (U32(S, 0x98) != 0) {
                SU32(S, 0x9c, 0);
                SU32(U32(E, 0x10) + U32(T, 0x24), 0x98, 0);
            } else if (U32(S, 0xa0) == 0) {
                const uint32_t ci = U32(S, 0x64), tab = U32(S, 0);
                int32_t vi = I32(S, 0x68);
                const rx_addr ib = U32(tab + 12u * ci, 8) + U32(E, 0x28);
                rx_addr vb = U32(tab + 12u * ci, 0) + U32(E, 0x24);
                SU32(E, 0x20, vb);
                fx_set_reg(FX_EBX, (uint32_t)vi); fx_set_reg(FX_EBP, ib);
                if (vi == 0) {
                    memset_call(ib, 0x200, 0x007cc047);
                    copy_q(U32(E, 0x20), S + 0x44, 4);
                    first_quad(ib);
                    vb = U32(E, 0x20);
                }
                if (U32(U32(T, 0x24) + U32(E, 0x10), 0xa4) != 0) {
                    copy_q(vb + 8u * (uint32_t)vi, S + 0x44, 4);
                    SU16(ib, 2u * (uint32_t)vi + 4, (uint16_t)(vi + 2));
                    SU16(ib, 2u * (uint32_t)vi, (uint16_t)vi);
                    SU16(ib, 2u * (uint32_t)vi + 2, (uint16_t)(vi + 1));
                    SU16(ib, 2u * (uint32_t)vi + 6, (uint16_t)(vi + 3));
                    if (vi + 5 < 0x100 && U16(ib, 2u * (uint32_t)vi + 8) == U16(ib, 2u * (uint32_t)(vi + 5))) {
                        SU16(ib, 2u * (uint32_t)vi + 8, (uint16_t)(vi + 3)); SU16(ib, 2u * (uint32_t)(vi + 5), (uint16_t)(vi + 3));
                    }
                } else if (vi > 0) {
                    copy_q(vb + 8u * (uint32_t)vi, S + 0x54, 2);
                    append_pair(ib, vi);
                }
                SU32(U32(T, 0x24) + U32(E, 0x10), 0xa4, 0);
                SU32(U32(T, 0x24) + U32(E, 0x10), 0x68, 0xffffffffu);
                const uint32_t cj = U32(S, 0x8c), tab2 = U32(U32(E, 0x10) + U32(T, 0x24), 0);
                const rx_addr vb2 = U32(tab2 + 12u * cj, 0) + U32(E, 0x24);
                const rx_addr ib2 = U32(tab2 + 12u * cj, 8) + U32(E, 0x28);
                const int32_t vj = I32(S, 0x90);
                fx_set_reg(FX_EBP, vb2); fx_set_reg(FX_EBX, ib2);
                if (vj == 0) {
                    memset_call(ib2, 0x200, 0x007cc1b7);
                    copy_q(vb2, S + 0x6c, 4);
                    first_quad(ib2);
                } else if (vj > 0) {
                    copy_q(vb2 + 8u * (uint32_t)vj, S + 0x7c, 2);
                    append_pair(ib2, vj);
                }
            }
            SU32(E, 0x1c, U32(E, 0x1c) + 1);
            SU32(E, 0x10, U32(E, 0x10) + 0xc0);
            if (!(I32(E, 0x1c) < I32(T, 0xc))) break;
        }
    }
    { const uint32_t a[1] = {U32(T, 0x14)}; d3d_call(U32(T, 0x14), 12, 1, a, 0x007cc285); }
    { const uint32_t a[1] = {U32(T, 0x10)}; d3d_call(U32(T, 0x10), 12, 1, a, 0x007cc290); }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x007cbe30, rx_fx_tracks_flush)

/* ---- 007cb930 DeformablePatch_ReadbackToSysmem (ESI = patch): GetRenderTargetData(rt[!idx] -> the physics surface); idx and
 * the write queue flip; the new write queue's offset, vertex and triangle counts are cleared. */
void rx_fx_patch_readback(rx_addr P) {
    const rx_addr dev = device();
    const uint32_t a[3] = {dev, U32(P, 0x20 + 4u * (U32(P, 0x14) == 0)), U32(P, 0x10)};
    d3d_call(dev, 32, 3, a, 0x007cb957);
    const uint32_t q = U32(P, 0x18) == 0;
    SU32(P, 0x14, U32(P, 0x14) == 0);
    SU32(P, 0x230 + q * 0x208u, 0);
    SU32(P, 0x2248 + q * 0x1e04u, 0);
    SU32(P, 0x440 + 4 * q, 0);
    SU32(P, 0x18, q);
}
RX_FUNCTION(0x007cb930, rx_fx_patch_readback)

/* one FlushPending call of vf14. Oracle: through its address: the world runtime's native there renders the deformation pass
 * (its CPU model) before the flush (this port's adapter) runs. Product: the core's deformation pass, then the flush. */
static void flush_call(rx_addr T, rx_addr P, int first, rx_addr ret) {
#ifdef RX_ORACLE
    (void)P; (void)first; (void)ret;
    rx_regs in = fx_regs(); in.r[FX_EAX] = T; in.r[FX_ESI] = fx_reg(FX_ESI);
    rx_call_original_ex(0x007cbe30, &in, 0, 0, 0);
#else
    if (first) rx_fx_deform_render(P);
    const rx_addr s = fx_push(0, 0, ret);
    rx_fx_tracks_flush(T);
    fx_pop(s);
#endif
}

/* ---- 007c4fa0 HFTerrainSurface::vf14 (fastcall ECX = surface): for the active scene's terrain only: a pending level reset
 * (+0x44b8) clears both deformation targets to 0x80808080 (the depth-stencil surface parked and restored), reads the cleared
 * one back and resets the three track sets; then the deformation readback and the three track flushes (the last one a tail
 * jump: it runs at vf14's own entry ESP). Frame: push ecx (the depth-stencil local); push edi; push esi. */
void rx_fx_terrain_sync(rx_addr surf) {
    const rx_addr sp0 = rx_sp();
    const fx_saved in = fx_save();
    SU32(sp0 - 4, 0, surf);
    SU32(sp0 - 8, 0, in.edi);
    fx_set_reg(FX_EDI, surf);
    if (U32(surf, 0x44a8) != fx_active_scene()) { fx_restore(in); return; }
    SU32(sp0 - 0xc, 0, in.esi);
    rx_set_sp(sp0 - 0xc);
    if (U32(surf, 0x44b8) != 0) {
        const rx_addr dev = device();
        fx_set_reg(FX_ESI, dev);
        const uint32_t idx = U32(surf, 0x244);
        { const uint32_t a[2] = {dev, sp0 - 4}; d3d_call(dev, 40, 2, a, 0x007c4fe5); }        /* GetDepthStencilSurface */
        { const uint32_t a[2] = {dev, 0}; d3d_call(dev, 39, 2, a, 0x007c4ff2); }             /* SetDepthStencilSurface(NULL) */
        { const uint32_t a[3] = {dev, 0, U32(surf, 0x250 + 4 * idx)}; d3d_call(dev, 37, 3, a, 0x007c500d); }
        { const uint32_t a[7] = {dev, 0, 0, 1, 0x80808080u, 0x3f800000u, 0}; d3d_call(dev, 43, 7, a, 0x007c502b); }
        { const uint32_t a[3] = {dev, 0, U32(surf, 0x250 + 4 * (U32(surf, 0x244) == 0))}; d3d_call(dev, 37, 3, a, 0x007c504b); }
        { const uint32_t a[7] = {dev, 0, 0, 1, 0x80808080u, 0x3f800000u, 0}; d3d_call(dev, 43, 7, a, 0x007c5069); }
        { const uint32_t a[2] = {dev, U32(sp0 - 4, 0)}; d3d_call(dev, 39, 2, a, 0x007c5079); }
        const rx_addr ds = U32(sp0 - 4, 0);
        if (ds != 0) {
            const uint32_t a[1] = {ds}; d3d_call(ds, 2, 1, a, 0x007c5089);                      /* Release */
            SU32(sp0 - 4, 0, 0);
        }
        { const rx_addr d2 = device();
          const uint32_t a[3] = {d2, U32(surf, 0x250 + 4 * (U32(surf, 0x244) == 0)), U32(surf, 0x240)}; d3d_call(d2, 32, 3, a, 0x007c50c1); }
        SU32(surf, 0x44b8, 0);
        { const rx_addr s = fx_push(0, 0, 0x007c50d5); rx_fx_tracks_reset(U32(0, 0x00a95774)); fx_pop(s); }
        { const rx_addr s = fx_push(0, 0, 0x007c50df); rx_fx_tracks_reset(U32(0, 0x00a95778)); fx_pop(s); }
        { const rx_addr s = fx_push(0, 0, 0x007c50e9); rx_fx_tracks_reset(U32(0, 0x00a9577c)); fx_pop(s); }
    }
    const rx_addr P = surf + 0x230;
    fx_set_reg(FX_ESI, P);
    { const rx_addr s = fx_push(0, 0, 0x007c50f4); rx_fx_patch_readback(P); fx_pop(s); }
    flush_call(U32(0, 0x00a95774), P, 1, 0x007c50fe);
    flush_call(U32(0, 0x00a95778), P, 0, 0x007c5108);
    /* pop esi; pop edi; add esp, 4; jmp FlushPending: the third flush runs at the entry ESP with vf14's return address */
    fx_restore(in);
    rx_set_sp(sp0);
#ifdef RX_ORACLE
    {   /* the original's call pushes its return address over vf14's own (kept and put back) */
        const uint32_t keep = U32(sp0, 0);
        rx_regs r = fx_regs(); r.r[FX_EAX] = U32(0, 0x00a9577c);
        rx_set_sp(sp0 + 4); rx_call_original_ex(0x007cbe30, &r, 0, 0, 0);
        SU32(sp0, 0, keep);
    }
#else
    rx_fx_tracks_flush(U32(0, 0x00a9577c));
#endif
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007c4fa0, rx_fx_terrain_sync)

/* ---- 00402d90 the terrain registry 0x9e30d4 (constructed on first use: count +8 = 0) */
rx_addr rx_fx_terrain_registry(void) {
    if ((U8(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    return 0x009e30d4;
}
RX_FUNCTION(0x00402d90, rx_fx_terrain_registry)

/* clamp to [0, 1] as the mud code compares (comiss 1; jb / comiss 0, x; jb) */
static inline float clamp01(float x) {
    if (x >= 1.0f) return 1.0f;
    if (0.0f >= x) return 0.0f;
    return x;
}
/* the level's MudManager: (level world) +0x2214c */
static rx_addr mud_manager(void) {
    const rx_addr c = fx_ctx(), g = U32(0, FX_G_GAME);
    rx_addr lw = 0;
    if (U32(U32(c, 0x20), 0x34) == 1) lw = U32(g, 0x5a54);
    else if (U32(U32(c, 0x1c), 0x34) == 1) lw = U32(g, 0x20c8);
    return U32(lw, 0x2214c);
}

/* ---- 00408c50 MudRecord_Update (ESI = record {+0 amount x4, +0x10 wetness x4, +0x20 colour, +0x30 vehicle}; dt = the mud
 * clock [0xa89bc4]): with a level world: the vehicle's position (vf17); debug water (0xa97164): amount -= wash rate * dt,
 * wetness = 1; else when fast enough (+0x150 >= 0x9c6074), not airborne-flagged (vf43) and inside the terrain's bounds: amount
 * = clamp01(amount + rate[material layer 2] * dt) for a non-zero rate; wetness = clamp01(wetness - dt / 30); colour = the
 * manager's, darkened while wet. Frame: push ebp; and esp, -16; sub esp, 0x18; ebx / edi: E = A - 0x20: +0xc dt, +0x10 position. */
void rx_fx_mud_update(rx_addr rec) {
    fx_frame f; fx_frame_enter(&f, 0x18, 16, FX_EBX, FX_EDI);
    const rx_addr E = f.E;
    const float dt = F32(0, 0x00a89bc4);
    fx_set_reg(FX_EBX, 1);
    SF32(E, 0xc, dt);
    const rx_addr c = fx_ctx();
    rx_addr lw = 0;
    if (U32(U32(c, 0x20), 0x34) == 1) lw = U32(U32(0, FX_G_GAME), 0x5a54);
    else if (U32(U32(c, 0x1c), 0x34) == 1) lw = U32(U32(0, FX_G_GAME), 0x20c8);
    if (lw == 0) { fx_frame_leave(&f); return; }
    const rx_addr V = U32(rec, 0x30);
    {   /* V->vf17 (&position) */
        const rx_addr fn = U32(U32(V, 0), 0x44);
        void (*g)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
        const uint32_t a[1] = {E + 0x10};
        if (g) { const rx_addr s = fx_push(1, a, 0x00408cb8); g(V, E + 0x10); fx_pop(s); }
        else rx_call_original(fn, RX_THISCALL, V, 1, a);
    }
    const float one = 1.0f /* 0x931f24 */;
    if (U32(0, 0x00a97164) != 0) {
        const float amount = clamp01(rx_subs(F32(rec, 0), rx_muls(F32(mud_manager(), 0xa0), F32(E, 0xc))));
        for (uint32_t i = 0; i < 0x10; i += 4) SF32(rec, i, amount);
        for (uint32_t i = 0x10; i < 0x20; i += 4) SF32(rec, i, one);
    } else {
        int wet_dry = 1;
        if (F32(V, 0x150) >= F32(0, 0x009c6074)) {
            uint32_t air;
            {   /* V->vf43 */
                const rx_addr fn = U32(U32(V, 0), 0xac);
                uint32_t (*g)(rx_addr) = RX_FN(fn, uint32_t (*)(rx_addr));
                if (g) { const rx_addr s = fx_push(0, 0, 0x00408d85); air = g(V); fx_pop(s); }
                else air = rx_call_original(fn, RX_THISCALL, V, 0, 0);
            }
            if (air == 0) {
                rx_addr reg, terr;
                { const rx_addr s = fx_push(0, 0, 0x00408d92); reg = rx_fx_terrain_registry(); fx_pop(s); }
                { const rx_addr s = fx_push(0, 0, 0x00408d99); terr = rx_world_find_by_controller(reg); fx_pop(s);
                  FX_LEAF_RESIDUE(0x00408d99, FX_EBX, FX_ESI, FX_EDI); }
                const float x = F32(E, 0x10), z = F32(E, 0x18);
                if (!(F32(terr, 0x44d8) > x) && !(x > F32(terr, 0x44e0)) && !(F32(terr, 0x44dc) > z) && !(z > F32(terr, 0x44e4))) {
                    { const rx_addr s = fx_push(0, 0, 0x00408de6); reg = rx_fx_terrain_registry(); fx_pop(s); }
                    { const rx_addr s = fx_push(0, 0, 0x00408ded); terr = rx_world_find_by_controller(reg); fx_pop(s);
                      FX_LEAF_RESIDUE(0x00408ded, FX_EBX, FX_ESI, FX_EDI); }
                    fx_set_reg(FX_EDI, terr);
                    uint32_t m;
                    { const rx_addr s = fx_push(0, 0, 0x00408dfd); rx_regs cr = fx_regs();
                      m = phys_terrain_material_at(terr, E + 0x10, 2, &cr, 0); fx_pop(s); }
                    fx_set_reg(FX_EDI, m);
                    const rx_addr lw2 = fx_level_world(U32(0, FX_G_GAME), 0x00408e0a, fx_reg(FX_ESI));
                    const float rate = F32(U32(lw2, 0x2214c), 0xe8 + 4 * m);
                    if (rate != 0.0f) {
                        const float amount = clamp01(rx_adds(rx_muls(rate, F32(E, 0xc)), F32(rec, 0)));
                        for (uint32_t i = 0; i < 0x10; i += 4) SF32(rec, i, amount);
                    }
                }
            }
        }
        if (wet_dry) {
            const float w = clamp01(rx_subs(F32(rec, 0x10), rx_muls(rx_divs(one, F32(0, 0x009c5da4)), F32(E, 0xc))));
            for (uint32_t i = 0x10; i < 0x20; i += 4) SF32(rec, i, w);
        }
    }
    const rx_addr M = mud_manager();
    const float wet = F32(rec, 0x10);
    if (wet != 0.0f) {
        const float k0 = rx_subs(one, F32(0, 0x009c5a54));
        float k = rx_subs(one, rx_muls(k0, wet));
        if (!(k >= one)) k = (0.0f >= k) ? 0.0f : k;
        else k = one;
        SF32(rec, 0x20, rx_muls(F32(M, 0x90), k));
        SF32(rec, 0x24, rx_muls(F32(M, 0x94), k));
        SF32(rec, 0x28, rx_muls(F32(M, 0x98), k));
    } else {
        fx_x87_copy(rec + 0x20, M + 0x90); fx_x87_copy(rec + 0x24, M + 0x94);
        fx_x87_copy(rec + 0x28, M + 0x98); fx_x87_copy(rec + 0x2c, M + 0x9c);
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x00408c50, rx_fx_mud_update)
