/* deform_equiv_test.c (w-surfaces) -- the product's CPU deformation pass (core/game/fx_deform.c, product only) against the oracle
 * runtime's (native/drafts/world_runtime/deform.cpp, the model every oracle run uses; validated against a live capture) on the same
 * model and the same random stamp batches: rotated wheel-sized quads, 1..24 per frame, brush uv 0..1, signed depth; each frame's
 * output is the next frame's input; the written rectangles and the whole grid compared bit for bit. The model is a world image's
 * RXDF deformation model (<world>-deform-model.bin: the SoftnessMask and the brush mips as the oracle decoded them), put into
 * fx_deform.c's cache directly (equal-size path; an unequal mask, FR_Cheyenne's 1024 under a 2048 grid, takes fx_deform.c's
 * sampler, which deform.cpp does not have: there the two differ by design). MUTATE=1 perturbs one frame's stamp (sensitivity check).
 * Build (from the repo root, after core/build.sh):
 *   clang++ -std=c++17 -O2 -ffp-contract=off -Inative/drafts/world_runtime -c core/tools/deform_equiv_ref.cpp -o ref.o
 *   clang++ -std=c++17 -O2 -ffp-contract=off -c native/drafts/world_runtime/deform.cpp -o deformcpp.o
 *   clang -std=c11 -O2 -w -ffp-contract=off -fwrapv -fno-strict-aliasing -Icore/include -Icore/src -Icore/game -Icore/api -c core/tools/deform_equiv_test.c -o t.o
 *   clang++ -o deform_equiv_test t.o ref.o deformcpp.o $(ls core/build/obj/*.o | grep -v game_fx_deform_c.o) -lz
 * usage: deform_equiv_test <deform-model.bin> <grid width> <frames> [seed] */
#include "fx_deform.c"
#include <stdlib.h>
int ref_load(const char* path, int* mw, int* mh, int* nb);
const unsigned char* ref_mask(void);
const unsigned char* ref_brush(int l, int* w, int* h);
void ref_apply(float* grid, int W, const float* v5, int nv, int rect[4]);
static uint64_t rng = 88172645463325252ull;
static double rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (double)(rng >> 11) / 9007199254740992.0; }
int main(int argc, char** argv) {
    int mw, mh, nb;
    if (argc < 4 || !ref_load(argv[1], &mw, &mh, &nb)) { fprintf(stderr, "usage / model\n"); return 2; }
    const int W = atoi(argv[2]), frames = atoi(argv[3]);
    if (argc > 4) rng = strtoull(argv[4], 0, 10) * 2654435761ull + 1;
    if (!rx_mem_init()) { fprintf(stderr, "arena\n"); return 2; }
    memset(&g_model, 0, sizeof g_model);
    g_model.mask_w = mw; g_model.mask_h = mh; g_model.mask = (uint8_t*)ref_mask(); g_model.mask_mips = 1;
    g_model.softness[0] = (mip){mw, mh, (uint8_t*)ref_mask()};
    for (int l = 0; l < nb; l++) { int w, h; const unsigned char* p = ref_brush(l, &w, &h); g_model.brush[l] = (mip){w, h, (uint8_t*)p}; }
    g_model.mips = nb;
    const rx_addr SRC = 0x60000000u, DST = SRC + (uint32_t)(W * W * 4) + 0x1000;
    const uint32_t pitch = (uint32_t)W * 4;
    float* grid = malloc(sizeof(float) * (size_t)W * W);
    for (int i = 0; i < W * W; i++) { grid[i] = rx_bf(0x3f008081u) + (float)((rnd() - 0.5) * 0.02); SF32(SRC, 4u * (uint32_t)i, grid[i]); SF32(DST, 4u * (uint32_t)i, grid[i]); }
    long texels = 0, diffs = 0; int first = -1;
    for (int fr = 0; fr < frames; fr++) {
        /* a batch of up to 64 quads like the wheels' stamps: a rotated rectangle around a point, brush uv 0..1, depth z */
        const int quads = 1 + (int)(rnd() * 24);
        stamp_vertex v[64 * 6]; float v5[64 * 6 * 5]; int nv = 0;
        const double cx = rnd() * 1.9 - 0.95, cy = rnd() * 1.9 - 0.95;
        for (int q = 0; q < quads; q++) {
            const double x0 = cx + (rnd() - 0.5) * 0.02, y0 = cy + (rnd() - 0.5) * 0.02, a = rnd() * 6.2831853;
            const double hl = (1 + rnd() * 12) / W, hw = (1 + rnd() * 6) / W, z = rnd() * 2 - 1;
            const double ca = cos(a), sa = sin(a);
            double P[4][2]; const double cu[4] = {0, 1, 1, 0}, cv[4] = {0, 0, 1, 1};
            for (int k = 0; k < 4; k++) { const double l = (k == 1 || k == 2) ? hl : -hl, w = k >= 2 ? hw : -hw; P[k][0] = x0 + l * ca - w * sa; P[k][1] = y0 + l * sa + w * ca; }
            static const int idx[6] = {0, 1, 2, 0, 2, 3};
            const float zz = (float)(z * (0.5 + rnd()));
            for (int k = 0; k < 6; k++) {
                const int c = idx[k];
                v[nv] = (stamp_vertex){(float)P[c][0], (float)P[c][1], zz, (float)cu[c], (float)cv[c]};
                v5[5 * nv] = v[nv].x; v5[5 * nv + 1] = v[nv].y; v5[5 * nv + 2] = v[nv].z; v5[5 * nv + 3] = v[nv].u; v5[5 * nv + 4] = v[nv].v; nv++;
            }
        }
        if (getenv("MUTATE") && fr == 100) { v[0].u += 0.01f; v[1].z *= 1.001f; }
        int rr[4]; ref_apply(grid, W, v5, nv, rr);
        const deform_rect r = apply_stamps(DST, SRC, pitch, W, v, nv);
        for (int z = r.z0; z < r.z1; z++) for (int x = r.x0; x < r.x1; x++) SU32(SRC, (uint32_t)z * pitch + 4u * (uint32_t)x, U32(DST, (uint32_t)z * pitch + 4u * (uint32_t)x));
        if (!(r.x0 == rr[0] && r.z0 == rr[1] && r.x1 == rr[2] && r.z1 == rr[3]) && !(r.x1 < r.x0 && rr[2] < rr[0])) { if (first < 0) first = fr; fprintf(stderr, "frame %d rect %d %d %d %d vs %d %d %d %d\n", fr, r.x0, r.z0, r.x1, r.z1, rr[0], rr[1], rr[2], rr[3]); }
        for (int z = rr[1]; z < rr[3]; z++) for (int x = rr[0]; x < rr[2]; x++) {
            texels++;
            if (U32(SRC, (uint32_t)z * pitch + 4u * (uint32_t)x) != rx_fb(grid[(size_t)z * W + x])) { if (first < 0) first = fr; diffs++; }
        }
    }
    long whole = 0, changed = 0;
    for (int i = 0; i < W * W; i++) if (fabsf(grid[i] - rx_bf(0x3f008081u)) > 0.011f) changed++;
    printf("texels moved beyond the initial noise: %ld\n", changed);
    for (int i = 0; i < W * W; i++) if (U32(SRC, 4u * (uint32_t)i) != rx_fb(grid[i])) whole++;
    printf("W %d frames %d: written texels %ld, differing %ld; whole grid differing %ld; first differing frame %d -> %s\n", W, frames, texels, diffs, whole, first,
           diffs == 0 && whole == 0 && first < 0 ? "IDENTICAL" : "DIFFERENT");
    return 0;
}
