/* tests/untamed_convert/foundation/core_stored_test.c -- the C core's package resource reader (core/src/fmt_lzx.c fmt_xmem_resource) on the
 * resources the Untamed converter writes: every 64 KiB chunk stored (packed size == allocation), the whole logical size after the container.
 * Input: stored_blobs.bin as the foundation test writes it (per resource: u32 n, n bytes stored blob, u32 m, m bytes expected).
 * Built by core_stored_test.sh with the platform's C compiler from the core's own sources. Prints "core stored resources: N ok, M failed". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fmt_lzx.h"

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: core_stored_test stored_blobs.bin\n"); return 2; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    int ok = 0, bad = 0;
    unsigned long long bytes = 0;
    for (;;) {
        unsigned char w[4];
        if (fread(w, 1, 4, f) != 4) break;
        size_t n = (size_t)w[0] | (size_t)w[1] << 8 | (size_t)w[2] << 16 | (size_t)w[3] << 24;
        unsigned char* blob = (unsigned char*)malloc(n ? n : 1);
        if (fread(blob, 1, n, f) != n || fread(w, 1, 4, f) != 4) { fprintf(stderr, "truncated input\n"); return 2; }
        size_t m = (size_t)w[0] | (size_t)w[1] << 8 | (size_t)w[2] << 16 | (size_t)w[3] << 24;
        unsigned char* want = (unsigned char*)malloc(m ? m : 1);
        if (fread(want, 1, m, f) != m) { fprintf(stderr, "truncated input\n"); return 2; }
        fmt_buf out;
        int rc = fmt_xmem_resource(blob, n, 0, &out);
        if (rc == 0 && out.size == m && memcmp(out.data, want, m) == 0) ok++;
        else { bad++; if (bad <= 5) fprintf(stderr, "resource %d: rc %d, %llu bytes, want %llu (%s)\n", ok + bad - 1, rc, (unsigned long long)out.size, (unsigned long long)m, rc ? fmt_error() : "content differs"); }
        bytes += m;
        if (rc == 0) free(out.data);
        free(blob); free(want);
    }
    fclose(f);
    printf("core stored resources: %d ok, %d failed (%llu bytes)\n", ok, bad, bytes);
    return bad ? 1 : 0;
}
