// fp_probe.cpp -- the oracle's probe for MXFirstPersonCamera::vf19 00642680 (development only; the same device as
// misc_probe.cpp): at a chosen frame of a Verify run, take the local player's first-person camera (P+0x97c), set its
// "pose valid" +0x2f4 (the boot never selects the camera, so the game's own calls may all early-return) and call 00642680
// through the installed port with three dt values. Verify then runs the core function and the original on the same state
// and compares memory + declared registers per call, so the whole pose math is exercised.
//   REFLEX_PORT=rx_MXFirstPersonCamera_vf19,rx_FPProbeHook REFLEX_PORT_VERIFY=1 RX_FP_PROBE=1
//   [RX_FP_PROBE_FRAME=<n> (200)] [RX_FP_PROBE_SLOT=<0..11> (the first with a vehicle and a rider)]
// No game logic: argument / state construction only; the camera's bytes are saved and restored.
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_firstperson.h"
#include "../game/anim_rider_a.h"
}
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" uint32_t rx_fp_probe_cam = 0;
extern "C" int rx_fp_dbg = 0;   /* the last first-person camera the vf19 adapter saw */

namespace {
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
uint32_t g_frame = 0;

void dump_frame(const char* tag, uint32_t F) {
    std::fprintf(stderr, "[fpab] %s frame %08x", tag, F);
    const uint32_t slots[] = {0x4c,0x12c,0x7c,0x108,0x10,0x14,0x18,0x20,0x24,0x28,0x2c,
                              0x30,0x34,0x38,0x3c,0x9c,0x10c,0x124,
                              0x50,0x54,0x58,0x60,0x64,0x68,0x80,0x84,0x88,
                              0xb0,0xb4,0xb8,0xc0,0xc4,0xc8,0x110,0x114,0x118,
                              0x180,0x184,0x188,0x18c};
    for (uint32_t o : slots) std::fprintf(stderr, " %x:%08x", o, rx_rd32(F + o));
    std::fprintf(stderr, "\n[fpab] %s dbl", tag);
    const uint32_t dslots[] = {0xa0,0xe0,0x130};
    for (uint32_t o : dslots) std::fprintf(stderr, " %x:%016llx", o, (unsigned long long)rx_rd64(F + o));
    std::fprintf(stderr, "\n");
}
void dump_veh(const char* tag, uint32_t c) {
    const uint32_t P = rd(c + 0x344u), V = P ? rd(P + 0x1acu) : 0;
    std::fprintf(stderr, "[fpab] %s veh %08x P %08x rider %08x", tag, V, P, P ? rd(P + 0x1b4u) : 0);
    if (V) for (uint32_t o = 0x130; o <= 0x148; o += 4) std::fprintf(stderr, " %x:%08x", o, rd(V + o));
    std::fprintf(stderr, " cam38c %u cam340 %u\n", rd(c + 0x38c) & 0xffu, rd(c + 0x340) & 0xffu);
}
void dump_cam(const char* tag, uint32_t c) {    std::fprintf(stderr, "[fpab] %s cam %08x", tag, c);
    for (uint32_t o = 0x30; o <= 0x68; o += 4) std::fprintf(stderr, " %x:%08x", o, rx_rd32(c + o));
    for (uint32_t o = 0x430; o <= 0x458; o += 4) std::fprintf(stderr, " %x:%08x", o, rx_rd32(c + o));
    std::fprintf(stderr, "\n");
}
void probe_ab(uint32_t cam, float dt) {
    static uint8_t save[0x490];    for (uint32_t i = 0; i < 0x490; i++) save[i] = rx_rd8(cam + i);
    const uint32_t esp0 = rx_sp();
    const uint32_t F = ((esp0 - 12u) & ~0xfu) - 0x1a0u;
    const bool core_only = std::getenv("RX_FP_AB_CORE") != nullptr;
    rx_wr32(cam + 0x2f4u, 1u);
    if (std::getenv("RX_FP_AB_BRANCH")) rx_wr8(cam + 0x38cu, 1u);
    if (!core_only) {   /* the original */
        rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = cam;
        const uint32_t a[1] = {rx_fb(dt)};
        rx_regs out; std::memset(&out, 0, sizeof out);
        rx_call_original_ex(0x00642680u, &in, &out, 1, a);
    }
    if (!core_only) { dump_frame("orig", F); dump_cam("orig", cam); dump_veh("orig", cam); }
    if (std::getenv("RX_FP_AB_ORIG")) { for (uint32_t i = 0; i < 0x490; i++) rx_wr8(cam + i, save[i]); return; }
    for (uint32_t i = 0; i < 0x490; i++) rx_wr8(cam + i, save[i]);
    rx_wr32(cam + 0x2f4u, 1u);
    if (std::getenv("RX_FP_AB_BRANCH")) rx_wr8(cam + 0x38cu, 1u);
    {   /* the core at the same stack depth */
        const uint32_t sp = rx_sp();
        rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = cam;
        rx_set_sp(sp - 8u);
        rx_first_person_camera_vf19(cam, dt);
        rx_set_sp(sp);
    }
    dump_frame("core", F); dump_cam("core", cam); dump_veh("core", cam);
    for (uint32_t i = 0; i < 0x490; i++) rx_wr8(cam + i, save[i]);
}
void probe_ab28(uint32_t cam) {
    static uint8_t save[0x490];
    for (uint32_t i = 0; i < 0x490; i++) save[i] = rx_rd8(cam + i);
    const uint32_t esp0 = rx_sp();
    const uint32_t a0[1] = {0};
    { rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = cam; rx_regs out; std::memset(&out, 0, sizeof out);
      rx_call_original_ex(0x00641460u, &in, &out, 1, a0); }
    std::fprintf(stderr, "[fpab28] orig 2f4 %u 38c %u 410 %08x %08x %08x %08x 430 %08x %08x %08x 440 %08x %08x %08x 450 %08x %08x %08x\n",
                 rx_rd32(cam + 0x2f4), rx_rd8(cam + 0x38c), rx_rd32(cam + 0x410), rx_rd32(cam + 0x414), rx_rd32(cam + 0x418), rx_rd32(cam + 0x41c),
                 rx_rd32(cam + 0x430), rx_rd32(cam + 0x434), rx_rd32(cam + 0x438), rx_rd32(cam + 0x440), rx_rd32(cam + 0x444), rx_rd32(cam + 0x448),
                 rx_rd32(cam + 0x450), rx_rd32(cam + 0x454), rx_rd32(cam + 0x458));
    for (uint32_t i = 0; i < 0x490; i++) rx_wr8(cam + i, save[i]);
    { const uint32_t sp = rx_sp(); rx_set_sp(sp - 8u); rx_first_person_camera_vf28(cam); rx_set_sp(sp); }
    std::fprintf(stderr, "[fpab28] core 2f4 %u 38c %u 410 %08x %08x %08x %08x 430 %08x %08x %08x 440 %08x %08x %08x 450 %08x %08x %08x\n",
                 rx_rd32(cam + 0x2f4), rx_rd8(cam + 0x38c), rx_rd32(cam + 0x410), rx_rd32(cam + 0x414), rx_rd32(cam + 0x418), rx_rd32(cam + 0x41c),
                 rx_rd32(cam + 0x430), rx_rd32(cam + 0x434), rx_rd32(cam + 0x438), rx_rd32(cam + 0x440), rx_rd32(cam + 0x444), rx_rd32(cam + 0x448),
                 rx_rd32(cam + 0x450), rx_rd32(cam + 0x454), rx_rd32(cam + 0x458));
    (void)esp0;
    for (uint32_t i = 0; i < 0x490; i++) rx_wr8(cam + i, save[i]);
}
void probe_camera(uint32_t P) {
    const uint32_t cam = rd(P + 0x97c);
    if (!cam) return;
    std::fprintf(stderr, "[fpprobe] player %08x cam %08x vt %08x 2f4 %u 340 %u fov %08x\n", P, cam, rd(cam), rd(cam + 0x2f4),
                 rd(cam + 0x340), rd(cam + 0xe0));
    static uint8_t save[0x490];
    for (uint32_t i = 0; i < 0x490; i++) save[i] = rx_rd8(cam + i);
    wr(cam + 0x2f4, 1);
    const float dts[3] = {1.0f / 60.0f, 0.25f, 0.0f};
    for (int k = 0; k < 3; k++) {
        rx_regs in;
        std::memset(&in, 0, sizeof in);
        in.r[1] = cam;                                        // ECX = this
        const uint32_t a[1] = {rx_fb(dts[k])};                // the stack dt
        rx_regs out;
        std::memset(&out, 0, sizeof out);
        rx_call_original_ex(0x00642680u, &in, &out, 1, a);
        std::fprintf(stderr, "[fpprobe] call dt %.6f done: 2f4 %u e0 %08x 3e0 %08x\n", (double)dts[k], rd(cam + 0x2f4),
                     rd(cam + 0xe0), rd(cam + 0x3e0));
        for (uint32_t i = 0; i < 0x490; i++) rx_wr8(cam + i, save[i]);
    }
}
void probe_head(uint32_t cam) {
    const uint32_t P = rd(cam + 0x344u), R = P ? rd(P + 0x1b4u) : 0;
    std::fprintf(stderr, "[fphead] cam %08x P %08x rider %08x\n", cam, P, R);
    if (!R) return;
    const uint32_t esp0 = rx_sp();
    const uint32_t F = ((esp0 - 12u) & ~0xfu) - 0x1a0u;
    const uint32_t p0 = F + 0x80u, q0 = F + 0x90u, p1 = F + 0x40u, q1 = F + 0x50u;
    { rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = R; in.r[6] = p0; in.r[4] = esp0;
      const uint32_t a[1] = {q0}; rx_regs out; std::memset(&out, 0, sizeof out);
      rx_call_original_ex(0x00623bd0u, &in, &out, 1, a); }
    { const uint32_t sp = rx_sp(); rx_set_sp(sp - 8u); rx_rider_anim_head_transform(R, p1, q1); rx_set_sp(sp); }
    std::fprintf(stderr, "[fphead] orig pos %08x %08x %08x quat %08x %08x %08x %08x\n",
                 rx_rd32(p0), rx_rd32(p0 + 4), rx_rd32(p0 + 8), rx_rd32(q0), rx_rd32(q0 + 4), rx_rd32(q0 + 8), rx_rd32(q0 + 12));
    std::fprintf(stderr, "[fphead] core pos %08x %08x %08x quat %08x %08x %08x %08x\n",
                 rx_rd32(p1), rx_rd32(p1 + 4), rx_rd32(p1 + 8), rx_rd32(q1), rx_rd32(q1 + 4), rx_rd32(q1 + 8), rx_rd32(q1 + 12));
    (void)q1; (void)p1;
}
}  // namespace

RX_PORT(0x0084ed20, FPProbeHook, "thunk", 0) {
    rx_fp_dbg = std::getenv("RX_FP_DBG") != nullptr;
    if (!std::getenv("RX_FP_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_FP_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_FP_PROBE_FRAME"))) : 200;
    static const int slot = std::getenv("RX_FP_PROBE_SLOT") ? std::atoi(std::getenv("RX_FP_PROBE_SLOT")) : -1;
    ++g_frame;
    if (std::getenv("RX_FP_PROBE_SELECT")) {   /* every frame: keep the first-person camera's pose enabled */
        if (rx_fp_probe_cam) {
            if (rd(rx_fp_probe_cam + 0x2f4) == 0) {
                std::fprintf(stderr, "[fpprobe] frame %u: pose on camera %08x\n", g_frame, rx_fp_probe_cam);
                if (!std::getenv("RX_FP_NOFORCE")) wr(rx_fp_probe_cam + 0x2f4u, 1u);
            }
            const uint32_t P = rd(rx_fp_probe_cam + 0x344u);   /* its controller (the player) */
            if (P) { wr(P + 0x9a8u, rx_fp_probe_cam); wr(P + 0x988u, 2u); wr(P + 0x9a0u, 0u); }
        }
        if (std::getenv("RX_FP_DUMP_CAM")) {
            static const uint32_t df = std::getenv("RX_FP_DUMP_FRAME") ? uint32_t(std::atoi(std::getenv("RX_FP_DUMP_FRAME"))) : 800u;
            const uint32_t G = rd(0x00a946b8u);
            for (uint32_t i = 0; i < 12 && g_frame == df; i++) {
                const uint32_t Q = G + 0x7460u + 0x12a0u * i;
                const uint32_t cam = rd(Q + 0x97cu);
                if (rd(Q + 0x10u) != 0 || !cam) continue;
                auto f = [](uint32_t a) { float v; uint32_t b = rx_rd32(a); std::memcpy(&v, &b, 4); return v; };
                std::fprintf(stderr, "[fpcam] frame %u player %08x cam %08x 2f4 %u pos %.9g %.9g %.9g fov %.9g near %.9g far %.9g | r %.9g %.9g %.9g u %.9g %.9g %.9g f %.9g %.9g %.9g\n",
                             g_frame, Q, cam, rd(cam + 0x2f4u), (double)f(cam + 0xa0), (double)f(cam + 0xa4), (double)f(cam + 0xa8),
                             (double)f(cam + 0xe0), (double)f(cam + 0xe8), (double)f(cam + 0xecu),
                             (double)f(cam + 0xb0), (double)f(cam + 0xb4), (double)f(cam + 0xb8),
                             (double)f(cam + 0xc0), (double)f(cam + 0xc4), (double)f(cam + 0xc8),
                             (double)f(cam + 0xd0), (double)f(cam + 0xd4), (double)f(cam + 0xd8));
            }
        }
        const uint32_t G = rd(0x00a946b8u);
        for (uint32_t i = 0; i < 12; i++) {
            const uint32_t P = G + 0x7460u + 0x12a0u * i;
            const uint32_t cam = rd(P + 0x97c);
            if (!cam) continue;
            if (rd(cam + 0x2f4) == 0) std::fprintf(stderr, "[fpprobe] frame %u: pose on player %08x cam %08x (rider %08x)\n", g_frame, P, cam, rd(P + 0x1b4u));
            if (!std::getenv("RX_FP_NOFORCE")) wr(cam + 0x2f4u, 1u);
            wr(P + 0x988u, 2u);                 /* select the first-person view */
            wr(P + 0x9a8u, cam);
        }
        return reflex_vm::RUN_ORIGINAL;
    }
    if (std::getenv("RX_FP_PROBE_SETCAM")) {   /* cameraselect: call the game's own Player_SetCamera on a live player, twice */
        static uint32_t P = 0, calls = 0;
        if (!P) {
            const uint32_t G = rd(0x00a946b8u);
            for (uint32_t i = 0; i < 12; i++) {
                const uint32_t Q = G + 0x7460u + 0x12a0u * i;
                const uint32_t act = rd(Q + 0x94u);
                const uint32_t st = act ? rd(act + 0x21cu) : 0;
                if (rd(Q + 0x10u) == 0 && rd(Q + 0x1acu) != 0 && act != 0 && st >= 1 && st <= 3) { P = Q; break; }
            }
        }
        if (P && calls < 2 && g_frame > 300) {
            const uint32_t cam = calls == 0 ? 2u : 1u;
            rx_regs in; std::memset(&in, 0, sizeof in); in.r[0] = P; in.r[1] = cam;
            rx_regs out; std::memset(&out, 0, sizeof out);
            std::fprintf(stderr, "[setcam] frame %u P %08x Player_SetCamera(%u): 988 before %u\n", g_frame, P, cam, rd(P + 0x988u));
            rx_call_original_ex(0x0054c9b0u, &in, &out, 0, 0);
            std::fprintf(stderr, "[setcam]   rc %u 988 after %u [9c5410] %u\n", out.r[0], rd(P + 0x988u), rd(0x009c5410u));
            ++calls;
        }
        if (P && calls == 2 && g_frame > 310) {
            const uint32_t arg = rd(P + 0x910u + 20u * rd(P + 0x988u));
            const uint32_t act = rd(P + 0x94u);
            const uint32_t disp = act ? rd(act + 0x290u) : 0;
            rx_regs in; std::memset(&in, 0, sizeof in); in.r[1] = arg; in.r[6] = disp;
            rx_regs out; std::memset(&out, 0, sizeof out);
            const uint32_t a[1] = {P};
            std::fprintf(stderr, "[setcam] frame %u CameraChanged arg %08x disp %08x player %08x\n", g_frame, arg, disp, P);
            rx_call_original_ex(0x00554ce0u, &in, &out, 1, a);
            ++calls;
        }
    }
    if (std::getenv("RX_FP_PROBE_USERCAM")) {  /* camcycle: select the User view with the game's own Player_SetCamera so that
                                                * Player_UpdateCamera calls MXThirdPersonCamera::vf30 every frame */
        static uint32_t P = 0, selected = 0;
        if (!P) {
            const uint32_t G = rd(0x00a946b8u);
            for (uint32_t i = 0; i < 12; i++) {
                const uint32_t Q = G + 0x7460u + 0x12a0u * i;
                const uint32_t act = rd(Q + 0x94u);
                const uint32_t st = act ? rd(act + 0x21cu) : 0;
                if (rd(Q + 0x10u) == 0 && rd(Q + 0x1acu) != 0 && act != 0 && st >= 1 && st <= 3) { P = Q; break; }
            }
        }
        if (P && !selected && g_frame > 300) {
            rx_regs in; std::memset(&in, 0, sizeof in); in.r[0] = P; in.r[1] = 4u;
            rx_regs out; std::memset(&out, 0, sizeof out);
            std::fprintf(stderr, "[usercam] frame %u P %08x Player_SetCamera(4): 988 before %u\n", g_frame, P, rd(P + 0x988u));
            rx_call_original_ex(0x0054c9b0u, &in, &out, 0, 0);
            std::fprintf(stderr, "[usercam]   rc %u 988 after %u user cam %08x\n", out.r[0], rd(P + 0x988u), rd(P + 0x984u));
            selected = 1;
        }
        if (P && selected) {
            if (rd(P + 0x988u) != 4u) wr(P + 0x988u, 4u);   /* keep the user view selected through the run */
            if (g_frame == 900) {                            /* the vf30 callees at top level (per-call Verify) */
                for (uint32_t at : {0x005486c0u, 0x00548710u, 0x00548760u}) { m.r[6] = 0; m.call_function(at, nullptr, 0); }
            }
        }
    }
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const uint32_t G = rd(0x00a946b8u);
    int done = 0;
    for (uint32_t i = 0; i < 12; i++) {
        if (slot >= 0 && int(i) != slot) continue;
        const uint32_t P = G + 0x7460u + 0x12a0u * i;
        if (!rd(P + 0x94) || !rd(P + 0x1acu) || !rd(P + 0x1b4u) || !rd(P + 0x97c)) continue;
        if (!rd(rd(P + 0x97cu) + 0x344u) || !rd(rd(rd(P + 0x97cu) + 0x344u) + 0x1acu)) continue;
        if (std::getenv("RX_FP_HEAD")) { probe_head(rd(P + 0x97cu)); return reflex_vm::RUN_ORIGINAL; }
        if (std::getenv("RX_FP_AB28")) { probe_ab28(rd(P + 0x97cu)); return reflex_vm::RUN_ORIGINAL; }
        if (std::getenv("RX_FP_AB")) { probe_ab(rd(P + 0x97cu), 1.0f / 60.0f); return reflex_vm::RUN_ORIGINAL; }
        if (std::getenv("RX_FP_PROBE_SELECT")) {
            wr(P + 0x988u, 2u);                 /* the player's selected camera index: the first-person slot */
            wr(P + 0x9a8u, rd(P + 0x97cu));     /* the current camera entity */
            wr(rd(P + 0x97cu) + 0x2f4u, 1u);    /* "pose valid" (the transition would set it too) */
            std::fprintf(stderr, "[fpprobe] selected first person for player %08x cam %08x\n", P, rd(P + 0x97cu));
        } else {
            probe_camera(P);
        }
        done = 1;
        break;
    }
    if (!done)
        for (uint32_t i = 0; i < 12; i++) {
            const uint32_t P = G + 0x7460u + 0x12a0u * i;
            if (!rd(P + 0x97c) && !rd(P + 0x1acu)) continue;
            std::fprintf(stderr, "[fpprobe] frame %u slot %u P %08x act %08x veh %08x rider %08x cam %08x\n", g_frame, i, P,
                         rd(P + 0x94), rd(P + 0x1acu), rd(P + 0x1b4u), rd(P + 0x97c));
        }
    std::fprintf(stderr, "[fpprobe] frame %u: %s\n", g_frame, done ? "called" : "no player with vehicle + rider + first-person camera");
    return reflex_vm::RUN_ORIGINAL;
}
