/* build_globals_test.c -- unit 12b test driver (core/notes/construction.md section 14): the game's startup state built by the core's code
 * from the install alone, written as a snapshot the comparer (build_globals_cmp.py) holds against the original's.
 *   build_globals_test <install root> <out prefix> [phase]
 * phase: "init" (default: the exe's image + rx_globals_static_init = the original's state at ReflexWinMain, s1.bin),
 *        "engine" (+ the Engine::Startup part: state at MXGame::Startup, s2.bin), "game" (+ MXGame::Startup .. : s3.bin), "all".
 * Snapshot files (the same format as the oracle tracer core/oracle/build_globals.cpp writes):
 *   <prefix>.img    the raw bytes of 0x00916000 .. 0x00dd1000 (.rdata, .data, .tls)
 *   <prefix>.heap   {u32 address, 4096 bytes} per page of every live heap block (the core's heap)
 *   <prefix>.blocks one line per live block: "address size" (hex; the block as rx_heap allocated it) */
#include "../src/fmt_db.h"
#include "../src/rx_heap.h"
#include "../src/rx_image.h"
#include "../game/build_globals.h"
#include "../game/crt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RANGE_LO 0x00916000u
#define RANGE_HI 0x00dd1000u

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: build_globals_test <install> <out prefix> [phase]\n"); return 2; }
    const char* phase = argc > 3 ? argv[3] : "init";
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    if (rx_image_load(fmt_install_vfs(in)) != 0) { fprintf(stderr, "image: %s\n", fmt_error()); return 1; }
    if (rx_image_load_crt(fmt_install_vfs(in)) != 0) { fprintf(stderr, "crt image: %s\n", fmt_error()); return 1; }
    int r = rx_globals_run(in, phase);
    if (r != 0) { fprintf(stderr, "rx_globals_run(%s) failed: %d\n", phase, r); }
    if (getenv("RX_CRT_SMOKE")) {           /* the core's CRT ports on the loaded DLL data */
        rx_addr b = rx_heap_alloc(256, 16);
        for (int i = 0; i < 4; i++) SU8(b, i, "AbC"[i]); for (int i = 0; i < 4; i++) SU8(b + 16, i, "aBc"[i]);
        printf("crt smoke: stricmp %d, strnicmp %d", crt_stricmp(b, b + 16), crt_strnicmp(b, b + 16, 3));
        rx_addr s = rx_heap_alloc(0x1c, 16); crt_str_ctor_cstr(s, b); printf(", string size %u", U32(s, 0x14));
        SU32(b + 64, 0, 7); SU32(b + 64, 4, b); rx_addr fm = b + 128; for (int i = 0; i < 5; i++) SU8(fm, i, "%u|%s"[i]); SU8(fm, 5, 0);
        SU32(b + 64, 0, 7); SU32(b + 64, 4, b);
        crt_sprintf(b + 192, fm, b + 64); printf(", sprintf '%s'\n", (char*)(rx_base + b + 192));
    }
    if (getenv("RX_DLC")) {
        static char names[64][64]; int n = rx_dlc_find(fmt_install_vfs(in), names, 64);
        printf("dlc: %d names:", n); for (int i = 0; i < n; i++) printf(" %s", names[i]); printf("\n");
    }
    char path[1024];
    snprintf(path, sizeof path, "%s.img", argv[2]);
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite(rx_base + RANGE_LO, 1, RANGE_HI - RANGE_LO, f);
    fclose(f);
    snprintf(path, sizeof path, "%s.dll", argv[2]);      /* the CRT DLLs' data: msvcp90 .data, msvcr90 .data */
    FILE* dl = fopen(path, "wb");
    for (uint32_t r0 = 0; r0 < 2; r0++) for (uint32_t a = r0 ? 0x785b7000u : 0x78505000u; a < (r0 ? 0x785b7000u : 0x78505000u) + 0x7000u; a += 0x1000) {
        fwrite(&a, 4, 1, dl); fwrite(rx_base + a, 1, 0x1000, dl);
    }
    fclose(dl);
    snprintf(path, sizeof path, "%s.heap", argv[2]);
    FILE* h = fopen(path, "wb");
    snprintf(path, sizeof path, "%s.blocks", argv[2]);
    FILE* b = fopen(path, "w");
    uint32_t lastpage = 0xffffffffu;
    for (int i = 0; i < rx_heap_blocks(); i++) {
        rx_addr a; uint32_t sz; int live;
        rx_heap_block(i, &a, &sz, &live);
        if (!live) continue;
        fprintf(b, "%08x %x\n", a, sz);
        for (uint32_t pg = a & ~0xfffu; pg < a + sz; pg += 0x1000) {
            if (pg == lastpage || (lastpage != 0xffffffffu && pg < lastpage)) continue;
            fwrite(&pg, 4, 1, h); fwrite(rx_base + pg, 1, 0x1000, h); lastpage = pg;
        }
    }
    fclose(h); fclose(b);
    printf("globals: phase %s, heap blocks %d\n", phase, rx_heap_blocks());
    return r == 0 ? 0 : 1;
}
