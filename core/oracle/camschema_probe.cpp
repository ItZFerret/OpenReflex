// camschema_probe.cpp -- the oracle's probe for the camera tuning schema (development only; not part of the product).
//
// The schema the tuning parser 0065c200 fills is built by the CRT static initializer 00908500 (9 x 006596f0), before
// the tuning files are loaded by 0065bdb0. A port at 0065bdb0 dumps the 9 class records to RX_CAMSCHEMA_DUMP on the
// first call (the schema as built by the initializer, before the first parse), then runs the original.
// A port at Engine::Update 0084ed20 (group rx_CamschemaFrame, the misc_probe device) dumps the records at
// RX_CAMSCHEMA_FRAME, for the post-parse state. No game logic: a byte copy of the guest memory.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr uint32_t kBase = 0x00af6d10u, kSize = 9u * 0x1da80u;

void dump(reflex_vm::Machine& m, const char* path) {
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::fprintf(stderr, "[camschema] cannot write %s\n", path); return; }
    static const uint8_t z[4096] = {};
    const uint32_t end = kBase + kSize;
    for (uint32_t pg = kBase & ~0xfffu; pg < end; pg += 4096) {
        const uint8_t* p = m.mem.peek(pg);
        const uint32_t lo = pg < kBase ? kBase : pg, hi = pg + 4096 < end ? pg + 4096 : end;
        if (p) std::fwrite(p + (lo - pg), 1, hi - lo, f);
        else std::fwrite(z, 1, hi - lo, f);
    }
    std::fclose(f);
    std::fprintf(stderr, "[camschema] dumped %u bytes to %s\n", kSize, path);
}

float rf(reflex_vm::Machine& m, uint32_t a) { float f = 0; if (m.mem.mapped(a)) { uint32_t u = uint32_t(m.mem.read(a, 4)); std::memcpy(&f, &u, 4); } return f; }
uint32_t ru(reflex_vm::Machine& m, uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; }
void cam_line(FILE* f, reflex_vm::Machine& m, const char* tag, uint32_t e) {
    if (!e) { std::fprintf(f, "%s 0\n", tag); return; }
    const uint32_t c = e + 0x90;   /* MXCameraEntity embeds the Camera at +0x90 */
    std::fprintf(f, "%s e %08x vt %08x | cam %08x pos %.9g %.9g %.9g fov %.9g near %.9g far %.9g | e0 %.9g 2f4 %u 4cc %.9g 4e0 %.9g 4e4 %.9g 4e8 %.9g 500 %.9g 510 %.9g 5c4 %.9g 5c8 %.9g\n",
                 tag, e, ru(m, e), c, rf(m, c + 0x10), rf(m, c + 0x14), rf(m, c + 0x18), rf(m, c + 0x50), rf(m, c + 0x58), rf(m, c + 0x5c),
                 rf(m, e + 0xe0), ru(m, e + 0x2f4), rf(m, e + 0x4cc), rf(m, e + 0x4e0), rf(m, e + 0x4e4), rf(m, e + 0x4e8),
                 rf(m, e + 0x500), rf(m, e + 0x510), rf(m, e + 0x5c4), rf(m, e + 0x5c8));
}
void cam_dump(reflex_vm::Machine& m, FILE* f) {
    const uint32_t G = ru(m, 0xa946b8);
    const uint32_t slot = ru(m, G + 0x153ec);
    const uint32_t P = G + 0x7460 + slot * 0x12a0;
    const uint32_t act = ru(m, P + 0x94), w = act ? ru(m, act + 0x234) : 0;
    std::fprintf(f, "G %08x slot %u P %08x act %08x state %d w %08x\n", G, slot, P, act, act ? int(ru(m, act + 0x21c)) : -1, w);
    for (uint32_t i = 0; i < 5; i++) { char t[16]; std::snprintf(t, sizeof t, "slot%u", i); cam_line(f, m, t, ru(m, P + 0x974 + 4 * i)); }
    cam_line(f, m, "quick", ru(m, P + 0x998));
    cam_line(f, m, "gate", ru(m, P + 0x99c));
    cam_line(f, m, "entity", ru(m, P + 0x9a8));
    cam_line(f, m, "worldcam", w ? w + 0x218f0 : 0);
    std::fprintf(f, "world cam 2b0 %u 2f4 %u | path %08x clip %08x len %.6f clock %.6f 430 %.6f 440 %u 444 %u 448 %.6f 44c %.6f\n",
                 w ? ru(m, w + 0x218f0 + 0x2b0) & 0xffu : 0u, w ? ru(m, w + 0x218f0 + 0x2f4) : 0u,
                 w ? ru(m, w + 0x218f0 + 0x310) : 0u, w ? ru(m, w + 0x218f0 + 0x310 + 0x11c) : 0u,
                 w ? rf(m, w + 0x218f0 + 0x310 + 0x120) : 0.f, w ? rf(m, w + 0x218f0 + 0x44c) : 0.f,
                 w ? rf(m, w + 0x218f0 + 0x430) : 0.f, w ? ru(m, w + 0x218f0 + 0x440) : 0u,
                 w ? ru(m, w + 0x218f0 + 0x444) : 0u, w ? rf(m, w + 0x218f0 + 0x448) : 0.f,
                 w ? rf(m, w + 0x218f0 + 0x44c) : 0.f);
    { const uint32_t ctx = m.mem.mapped(0xa95960) ? uint32_t(m.mem.read(0xa95960, 4)) : 0;
      std::fprintf(f, "disp %08x %d x %d\n", ctx ? ctx + 0x3c : 0,
                   ctx ? int(ru(m, ctx + 0x3c + 8)) : 0, ctx ? int(ru(m, ctx + 0x3c + 0xc)) : 0); }
    if (w) { const uint32_t h = ru(m, w + 0x218f0 + 0x310 + 0x11c); char lab[0x40] = {};
             for (uint32_t i = 0; i < 0x3f && h; i++) lab[i] = char(m.mem.mapped(h + 4 + i) ? m.mem.read(h + 4 + i, 1) : 0);
             std::fprintf(f, "clipname '%s' lcg %u %08x base", lab, ru(m, 0x9cb864), ru(m, 0x9cb860));
             for (int k = 0; k < 16; k++) std::fprintf(f, " %.4f", rf(m, w + 0x218f0 + 0x460 + 4 * k));
             std::fprintf(f, "\n"); }
}
}  // namespace

RX_PORT(0x0065bdb0, CamSchemaDump, "rx_camschema_probe", 0) {
    static const char* path = std::getenv("RX_CAMSCHEMA_DUMP");
    static bool done = false;
    if (path && !done) { done = true; dump(m, path); }
    return reflex_vm::RUN_ORIGINAL;
}

RX_PORT(0x00642680, CamFirstPersonProbe, "rx_camschema_probe", 0) {
    static uint32_t n = 0;
    if (++n <= 3) std::fprintf(stderr, "[fp] 00642680 call %u cam %08x dt %08x 2f4 %u\n", n, m.r[1], m.arg(0), ru(m, m.r[1] + 0x2f4));
    return reflex_vm::RUN_ORIGINAL;
}

RX_PORT(0x0084ed20, CamSchemaFrame, "rx_camschema_probe", 0) {
    static const char* path = std::getenv("RX_CAMSCHEMA_FRAME_DUMP");
    static uint32_t at = std::getenv("RX_CAMSCHEMA_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CAMSCHEMA_FRAME"))) : 2;
    static uint32_t frame = 0;
    static bool done = false;
    ++frame;
    if (path && !done && frame == at) { done = true; dump(m, path); }
    if (const char* lt = std::getenv("RX_CAMSCHEMA_LCG_TRACE")) {
        static FILE* lf = std::fopen(lt, "w");
        const uint32_t G2 = ru(m, 0xa946b8);
        const uint32_t P2 = G2 ? G2 + 0x7460 + ru(m, G2 + 0x153ec) * 0x12a0 : 0;
        const uint32_t w2 = P2 ? ru(m, P2 + 0x94) : 0;
        const uint32_t ww = w2 ? ru(m, w2 + 0x234) : 0;
        const uint32_t h = ww ? ru(m, ww + 0x218f0 + 0x310 + 0x11c) : 0;
        char nm[0x40] = {};
        for (uint32_t i = 0; i < 0x3f && h; i++) nm[i] = char(m.mem.mapped(h + 4 + i) ? m.mem.read(h + 4 + i, 1) : 0);
        std::fprintf(lf, "%u %u %08x '%s' %u %.6f\n", frame, ru(m, 0x9cb864), ru(m, 0x9cb860), nm, ww ? ru(m, ww + 0x218f0 + 0x2b0) & 0xffu : 0,
                     ww ? rf(m, ww + 0x218f0 + 0x44c) : 0.f);
        std::fflush(lf);
    }
    if (const char* cp = std::getenv("RX_CAMSCHEMA_CAM_DUMP")) {
        static FILE* cf = std::fopen(cp, "w");
        static uint32_t cf_at = std::getenv("RX_CAMSCHEMA_CAM_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CAMSCHEMA_CAM_FRAME"))) : 300;
        if (frame >= cf_at && (frame - cf_at) % 50 == 0 && frame <= cf_at + 500) { std::fprintf(cf, "frame %u\n", frame); cam_dump(m, cf); std::fflush(cf); }
    }
    return reflex_vm::RUN_ORIGINAL;
}
