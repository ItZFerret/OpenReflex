/* build_render.c -- see build_render.h. Transliterated from the disassembly; the critical section 0x00d649f8 around the list
 * (InitializeCriticalSection in 007b0920, Enter / Leave around each insertion) is not needed: the core builds on one thread. */
#include "build_render.h"
#include "build_alloc.h"
#include "../src/rx_d3d.h"
#include <ctype.h>

/* 007ce320: a std::list head node (0xc bytes, next = previous = itself) */
static rx_addr list_head_node(void) {
    rx_addr n = rx_reflex_aligned_alloc(0xc);
    if (n) { SU32(n, 0, n); SU32(n, 4, n); }
    return n;
}

/* 007b0920 (+ 007cdb30) */
rx_addr rx_gpu_registry_init(void) {
    if (U32(0, RX_GPU_REGISTRY)) return U32(0, RX_GPU_REGISTRY);
    rx_addr r = rx_reflex_aligned_alloc(0x1c);
    if (r) {
        rx_addr p = rx_reflex_aligned_alloc(4);
        if (p) { SU32(p, 0, r); SU32(r, 0, p); } else SU32(r, 0, 0);
        SU32(r, 0x14, list_head_node());
        SU32(r, 0x18, 0);
    }
    SU32(0, RX_GPU_REGISTRY, r);
    return r;
}

/* 007b0780 (ESI = record): switch on the kind (jump table 0x7b089c), the device call with the record's fields; +0x30 = 1 */
uint32_t rx_gpu_create(rx_addr r) {
    rx_addr dev = rx_renderer_device();
    uint32_t hr = 0;
    switch (U32(r, 0)) {
    case 0: { uint32_t a[9] = {dev, U32(r, 8), U32(r, 0xc), U32(r, 0x20), U32(r, 0x24), U32(r, 0x28), U32(r, 0x2c), U32(r, 4), 0};
              hr = rx_d3d_call(dev, 23, 9, a); break; }                      /* CreateTexture */
    case 1: { uint32_t a[9] = {dev, U32(r, 8), U32(r, 0xc), U32(r, 0x28), U32(r, 0x14), U32(r, 0x10), U32(r, 0x1c), U32(r, 4), 0};
              hr = rx_d3d_call(dev, 29, 9, a); break; }                      /* CreateDepthStencilSurface */
    case 2: { uint32_t a[9] = {dev, U32(r, 8), U32(r, 0xc), U32(r, 0x28), U32(r, 0x14), U32(r, 0x10), U32(r, 0x18), U32(r, 4), 0};
              hr = rx_d3d_call(dev, 28, 9, a); break; }                      /* CreateRenderTarget */
    case 3: { uint32_t a[7] = {dev, U32(r, 8), U32(r, 0xc), U32(r, 0x28), U32(r, 0x2c), U32(r, 4), 0};
              hr = rx_d3d_call(dev, 36, 7, a); break; }                      /* CreateOffscreenPlainSurface */
    case 4: { uint32_t a[7] = {dev, U32(r, 8), U32(r, 0x24), U32(r, 0x28), U32(r, 0x2c), U32(r, 4), 0};
              hr = rx_d3d_call(dev, 27, 7, a); break; }                      /* CreateIndexBuffer */
    case 5: { uint32_t a[7] = {dev, U32(r, 8), U32(r, 0x24), U32(r, 0xc), U32(r, 0x2c), U32(r, 4), 0};
              hr = rx_d3d_call(dev, 26, 7, a); break; }                      /* CreateVertexBuffer */
    default: break;
    }
    SU8(r, 0x30, 1);
    return hr;
}

/* the record linked at the end of the registry's list (005ba990 the node {head, old last, record}, 007cf6d0 size + 1) */
static void registry_link(rx_addr rec) {
    rx_addr reg = U32(0, RX_GPU_REGISTRY), head = U32(reg, 0x14);
    rx_addr n = rx_reflex_aligned_alloc(0xc);
    if (n) { SU32(n, 0, head); SU32(n, 4, U32(head, 4)); SU32(n, 8, rec); }
    SU32(reg, 0x18, U32(reg, 0x18) + 1);
    SU32(head, 4, n);
    SU32(U32(n, 4), 0, n);
}

static rx_addr new_record(void) {
    rx_gpu_registry_init();
    rx_addr r = rx_reflex_aligned_alloc(0x48);
    if (r) for (uint32_t k = 0; k < 0x48; k += 4) SU32(r, k, 0);
    return r;
}
static uint32_t finish(rx_addr r) {
    uint32_t hr = rx_gpu_create(r);
    if ((int32_t)hr < 0) { rx_reflex_aligned_free(r); return hr; }
    registry_link(r);
    return hr;
}

/* 007b0970 */
uint32_t rx_gpu_create_texture(uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool, rx_addr out,
                               rx_addr callback, rx_addr context, uint32_t a9, rx_addr file, uint32_t line) {
    rx_addr r = new_record();
    SU32(r, 0, 0); SU32(r, 4, out); SU32(r, 8, w); SU32(r, 0xc, h); SU32(r, 0x20, levels); SU32(r, 0x24, usage); SU32(r, 0x28, format);
    SU32(r, 0x2c, pool); SU32(r, 0x34, callback); SU32(r, 0x38, context); SU32(r, 0x3c, a9); SU32(r, 0x40, file); SU32(r, 0x44, line);
    return finish(r);
}
/* 007b0cd0 */
uint32_t rx_gpu_create_vb(uint32_t length, uint32_t usage, rx_addr out, rx_addr callback, rx_addr context, rx_addr file, uint32_t line) {
    rx_addr r = new_record();
    SU32(r, 0, 5); SU32(r, 4, out); SU32(r, 8, length); SU32(r, 0x24, usage); SU32(r, 0xc, 0); SU32(r, 0x2c, 0);
    SU32(r, 0x34, callback); SU32(r, 0x38, context); SU32(r, 0x3c, 0); SU32(r, 0x40, file); SU32(r, 0x44, line);
    return finish(r);
}
/* 007b0c10 */
uint32_t rx_gpu_create_ib16(uint32_t length, rx_addr out) {
    rx_addr r = new_record();
    SU32(r, 0, 4); SU32(r, 4, out); SU32(r, 8, length); SU32(r, 0x24, 0x208); SU32(r, 0x28, 0x65); SU32(r, 0x2c, 0);
    return finish(r);
}

/* 007afd60: the cached declaration whose elements match (GetDeclaration of each; per element the offset, type, method, usage and
 * usage index are compared, not the stream), else CreateVertexDeclaration + AddRef + push_back; the result AddRef'ed */
rx_addr rx_vertex_decl_get(rx_addr elements, rx_addr cache) {
    rx_addr dev = rx_renderer_device();
    rx_addr v = cache + 0x18;
    uint32_t n = (U32(cache, 0x28) - U32(cache, 0x24)) >> 2;
    RX_FRAME_ENTER(F, 0x240);
    rx_addr local = F + 0x38, countp = F + 0x10;
    for (uint32_t i = 0; i < n; i++) {
        SU32(countp, 0, 0x41);
        rx_addr d = U32(U32(v, 0xc), 4 * i);
        uint32_t a[3] = {d, local, countp};
        rx_d3d_call(d, 4, 3, a);                                             /* GetDeclaration */
        uint32_t k = 0, cnt = U32(countp, 0);
        for (; k < cnt; k++) {
            rx_addr e = elements + 8 * k, l = local + 8 * k;
            if (U8(e, 6) != U8(l, 6) || U16(e, 2) != U16(l, 2) || U8(e, 4) != U8(l, 4) || U8(e, 7) != U8(l, 7) || U8(e, 5) != U8(l, 5)) break;
        }
        if (k >= cnt) {
            uint32_t ar[1] = {d}; rx_d3d_call(d, 1, 1, ar);                 /* AddRef */
            RX_FRAME_LEAVE(F, 0x240);
            return d;
        }
    }
    rx_addr out = F + 0xc;
    SU32(out, 0, 0);
    uint32_t a[3] = {dev, elements, out};
    rx_d3d_call(dev, 86, 3, a);                                              /* CreateVertexDeclaration */
    rx_addr d = U32(out, 0);
    uint32_t ar[1] = {d}; rx_d3d_call(d, 1, 1, ar);
    RX_FRAME_LEAVE(F, 0x240);
    /* push_back on the cache's vector (FUN_004178e0 when full) */
    rx_addr first = U32(v, 0xc), last = U32(v, 0x10);
    uint32_t cap = first ? (U32(v, 0x14) - first) >> 2 : 0;
    if ((last - first) >> 2 < cap) { SU32(last, 0, d); SU32(v, 0x10, last + 4); }
    else {
        uint32_t size = (last - first) >> 2, ncap = 0x3fffffffu - (cap >> 1) < cap ? 0 : cap + (cap >> 1);
        if (ncap < size + 1) ncap = size + 1;
        rx_addr nb = rx_reflex_aligned_alloc(ncap * 4);
        for (uint32_t k = 0; k < size; k++) SU32(nb, 4 * k, U32(first, 4 * k));
        SU32(nb, 4 * size, d);
        if (first) rx_reflex_aligned_free(first);
        SU32(v, 0x14, nb + ncap * 4); SU32(v, 0x10, nb + (size + 1) * 4); SU32(v, 0xc, nb);
    }
    return d;
}

/* msvcr90 _stricmp in the C locale: bytes compared after ASCII tolower */
static int stricmp_game(const char* a, rx_addr b) {
    for (uint32_t k = 0;; k++) {
        int x = tolower((unsigned char)a[k]), y = tolower(U8(b, k));
        if (x != y) return x - y;
        if (!x) return 0;
    }
}
/* 007b12a0 / 007b1330: the last entry of that name (searched from the end); the shader AddRef'ed into *out, the second object (if
 * any) AddRef'ed into *out2. An asset the core has not built (0) finds nothing (the original always has the shader asset). */
static void shader_find(rx_addr asset, const char* name, rx_addr out, rx_addr out2, uint32_t count_off, uint32_t names_off) {
    SU32(out, 0, 0);
    if (!asset) return;
    for (int32_t i = I32(asset, count_off) - 1; i >= 0; i--) {
        rx_addr e = asset + 0x48u * (uint32_t)i + names_off;
        if (stricmp_game(name, e)) continue;
        rx_addr s = U32(e, 0x40);
        uint32_t a[1] = {s}; rx_d3d_call(s, 1, 1, a);
        SU32(out, 0, U32(e, 0x40));
        if (out2) {
            if (U32(e, 0x44)) { uint32_t b[1] = {U32(e, 0x44)}; rx_d3d_call(U32(e, 0x44), 1, 1, b); }
            SU32(out2, 0, U32(e, 0x44));
        }
        return;
    }
}
void rx_shader_find_vs(rx_addr asset, const char* name, rx_addr out, rx_addr out2) { shader_find(asset, name, out, out2, 0x454, 0x1c); }
void rx_shader_find_ps(rx_addr asset, const char* name, rx_addr out, rx_addr out2) { shader_find(asset, name, out, out2, 0x890, 0x458); }
