// deform_equiv_ref.cpp (w-surfaces): the oracle runtime's deformation model (native/drafts/world_runtime/deform.cpp) behind C entries for
// core/tools/deform_equiv_test.c (see there for the build).
#include "deform.h"
#include <cstring>
using namespace reflex_vm;
static DeformModel g;
extern "C" int ref_load(const char* path, int* mw, int* mh, int* nb) {
    if (!g.load(path)) return 0;
    *mw = g.mask_w; *mh = g.mask_h; *nb = int(g.brush.size()); return 1;
}
extern "C" const unsigned char* ref_mask(void) { return g.mask.data(); }
extern "C" const unsigned char* ref_brush(int l, int* w, int* h) { *w = g.brush[l].w; *h = g.brush[l].h; return g.brush[l].rgb.data(); }
extern "C" void ref_apply(float* grid, int W, const float* v5, int nv, int rect[4]) {
    std::vector<float> G(grid, grid + size_t(W) * W);
    std::vector<StampVertex> verts; verts.resize(size_t(nv));
    for (int i = 0; i < nv; i++) verts[i] = {v5[5 * i], v5[5 * i + 1], v5[5 * i + 2], v5[5 * i + 3], v5[5 * i + 4]};
    DeformRect r = apply_stamps(G, W, verts, g);
    std::memcpy(grid, G.data(), sizeof(float) * G.size());
    rect[0] = r.x0; rect[1] = r.z0; rect[2] = r.x1; rect[3] = r.z1;
}
