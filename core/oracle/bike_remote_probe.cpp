// bike_remote_probe.cpp -- the oracle's probe for the remote simulation's record application 00432be0 (core/game/bike_remote.c).
// No reference world has a remote (type 1) simulation, so a frame hook (Engine::Update 0084ed20) takes the live MX simulation and
// calls the ORIGINAL 00432be0 / 004331d0 / the track filters on prepared records: with the rx_bike ports installed in Verify mode
// the core runs first, then the original on the same state, per call.
//   REFLEX_PORT=rx_bike,rx_RemoteProbeHook REFLEX_PORT_VERIFY=1 RX_REMOTE_PROBE=1 [RX_REMOTE_FRAME=<n>] [RX_PENDING_PROBE=<bxml>]
// The record is the simulation's own state (00433120 Simulation_GetReplayState) with its time and position set per case; the ring
// of 20 records is filled by 004331d0 and then varied (positions, orientations, velocities, times) so the cubic fits are not flat.
// Every case runs on a snapshot of the guest memory and machine state, restored afterwards (the run continues on the untouched world).
#include "oracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
float rf(uint32_t a) { return rx_oracle::f32(rd(a)); }
void wf(uint32_t a, float f) { wr(a, rx_oracle::bits(f)); }

struct Pages {
    reflex_vm::Memory& memory;
    struct Page { uint32_t address; std::array<uint8_t, 4096> bytes; };
    std::vector<Page> saved;
    std::vector<uint8_t> was;
    explicit Pages(reflex_vm::Memory& m) : memory(m), was(1u << 20, 0) {
        for (uint32_t i = 0; i < (1u << 20); i++) if (m.mapped(i << 12)) {
            was[i] = 1; saved.push_back(Page{}); auto& p = saved.back(); p.address = i << 12;
            std::memcpy(p.bytes.data(), m.page(p.address), 4096);
        }
    }
    void restore() {
        for (auto& p : saved) std::memcpy(memory.page(p.address), p.bytes.data(), 4096);
        for (uint32_t i = 0; i < (1u << 20); i++) if (!was[i] && memory.mapped(i << 12)) memory.unmap(i << 12, 4096);
    }
};

uint32_t find_mx_simulation(reflex_vm::Machine& m) {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (unsigned i = 0; i < 16; i++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * i;
        if (!m.mem.mapped(p + 0x1ac)) continue;
        const uint32_t v = rd(p + 0x1ac);
        if (!v || !m.mem.mapped(v + 0x234)) continue;
        const uint32_t s = rd(v + 0x234);
        if (s && m.mem.mapped(s + 0x8fbc) && rd(s) == 0x0093c8a4) return s;   /* RavenSimulationMotocross */
    }
    return 0;
}
}  // namespace

RX_PORT(0x0084ed20, RemoteProbeHook, "thunk", 0) {
    if (!std::getenv("RX_REMOTE_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_REMOTE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_REMOTE_FRAME"))) : 300;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const uint32_t S = find_mx_simulation(m);
    if (!S) { std::fprintf(stderr, "[remote] ERROR no MX simulation\n"); return reflex_vm::RUN_ORIGINAL; }

    uint32_t regs[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm); std::memcpy(mmx, m.mmx, sizeof mmx);
    auto flags = m.flags; auto fpu = m.fpu; auto cw = m.fpu_cw, sw = m.fpu_sw; auto mxcsr = m.mxcsr; bool mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[remote] ERROR no reversible host state\n"); return reflex_vm::RUN_ORIGINAL; }
    Pages pages(m.mem);
    auto restore_all = [&]() {
        pages.restore(); host_restore();
        std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); std::memcpy(m.mmx, mmx, sizeof mmx);
        m.flags = flags; m.fpu = fpu; m.fpu_cw = cw; m.fpu_sw = sw; m.mxcsr = mxcsr; m.mmx_active = mmx_active;
    };
    auto invoke = [&](uint32_t fn, std::function<void(rx_regs&)> set, std::initializer_list<uint32_t> args) {
        rx_regs in{}, out{};
        for (unsigned i = 0; i < 8; i++) in.r[i] = regs[i];
        for (unsigned i = 0; i < 8; i++) {
            const uint32_t b[4] = {uint32_t(xmm[i].lo), uint32_t(xmm[i].lo >> 32), uint32_t(xmm[i].hi), uint32_t(xmm[i].hi >> 32)};
            std::memcpy(in.xmm[i].v, b, 16);
        }
        set(in);
        std::vector<uint32_t> a(args);
        rx_call_original_ex(fn, &in, &out, int(a.size()), a.empty() ? nullptr : a.data());
        return out;
    };
    auto alloc = [&](uint32_t bytes) { return invoke(0x0078fbe0, [](rx_regs&) {}, {bytes}).r[0]; };
    auto set_xmm_f = [](rx_regs& in, int i, float f) { in.xmm[i].v[0] = f; in.xmm[i].v[1] = 0; in.xmm[i].v[2] = 0; in.xmm[i].v[3] = 0; };

    /* the record: the simulation's own state, then per case its time / position */
    auto make_record = [&]() {
        const uint32_t rec = alloc(0x80);
        invoke(0x00433120, [&](rx_regs& in) { in.r[1] = S; }, {rec});       /* Simulation_GetReplayState */
        return rec;
    };
    /* the ring filled from a record at time t0 (004331d0), then varied */
    auto fill_ring = [&](float t0, int vary) {
        const uint32_t rec = make_record();
        wf(rec + 0x40, t0);
        invoke(0x004331d0, [&](rx_regs& in) { in.r[7] = rec; }, {S});
        if (vary) {
            for (unsigned i = 0; i < 20; i++) {
                const uint32_t k = S + 0x2390 + 0x70 * i;
                const float f = float(i);
                wf(k + 0x00, rf(k + 0x00) + 0.37f * f); wf(k + 0x04, rf(k + 0x04) - 0.21f * f * f * 0.1f); wf(k + 0x08, rf(k + 0x08) + 0.13f * f);
                wf(k + 0x20, rf(k + 0x20) + 0.5f * std::sin(f)); wf(k + 0x24, rf(k + 0x24) - 0.25f * f); wf(k + 0x28, rf(k + 0x28) + 0.1f);
                wf(k + 0x30, rf(k + 0x30) + 0.05f * f); wf(k + 0x34, rf(k + 0x34) - 0.3f); wf(k + 0x38, rf(k + 0x38) + 0.02f * f * f);
                const float a = 0.05f * f;
                wf(k + 0x10, std::sin(a) * 0.6f); wf(k + 0x14, std::sin(a) * 0.8f); wf(k + 0x18, 0.0f); wf(k + 0x1c, std::cos(a));
                if (vary == 2) wf(k + 0x40, rf(k + 0x40) + 0.07f * std::sin(3.0f * f));   /* uneven spacing */
            }
        }
    };
    unsigned cases = 0;
    /* one case: the ring filled from time t0 (and varied), then `tweak`, then S+2c90 = t_sim, the record's time = t_rec, S+237c = 1
     * (0 with first = the first record) */
    auto apply = [&](const char* what, float t0, int vary, float t_sim, float t_rec, bool first, std::function<void(uint32_t)> tweak) {
        fill_ring(t0, vary);
        const uint32_t rec = make_record();
        if (tweak) tweak(rec);
        wf(S + 0x2c90, t_sim);
        wf(rec + 0x40, t_rec);
        wr(S + 0x237c, first ? 0 : 1);
        invoke(0x00432be0, [&](rx_regs& in) { in.r[1] = rec; }, {S});
        cases++;
        std::fprintf(stderr, "[remote] %s (vary %d): t %g rec %g -> newer %u older %u d %g %g %g q %g %g %g %g\n", what, vary, t_sim, t_rec,
                     rd(S + 0x2380), rd(S + 0x2384), rf(S + 0x2da0), rf(S + 0x2da4), rf(S + 0x2da8), rf(S + 0x2dc0), rf(S + 0x2dc4),
                     rf(S + 0x2dc8), rf(S + 0x2dcc));
        restore_all();
    };
    const float T0 = 10.0f;
    for (int v = 0; v < 3; v++) {
        apply("first record (jump)", T0, v, 9.0f, 9.5f, true, nullptr);
        apply("not newer (dropped)", T0, v, 9.5f, 9.4f, false, nullptr);
        apply("newer than the ring", T0, v, 9.95f, 10.2f, false, nullptr);
        apply("older than the ring", T0, v, 4.7f, 4.8f, false, nullptr);
        for (int k = 0; k < 18; k += 4) apply("inside the ring", T0, v, T0 - 0.3f * float(k) - 0.1f, 9.5f, false, nullptr);
        apply("no bracketing ring records (k = -1)", T0, v, 4.5f, 9.5f, false, [&](uint32_t rec) {
            for (unsigned i = 1; i < 17; i++) wf(S + 0x2390 + 0x70 * i + 0x40, 11.0f);
            wr(S + 0x2390 + 0x70 * 17 + 0x40, 0x7fc00000u);
            wr(rec + 0x44, rd(rec + 0x44) | 0x20000u); });
        apply("near zero times (keys skipped)", 0.5f, v, 0.35f, 0.45f, false, nullptr);
        apply("far (jump after)", T0, v, 9.0f, 9.5f, false, [&](uint32_t rec) { wf(rec + 0x00, rf(rec + 0x00) + 100.0f); });
        /* NaNs through the x87 copies (fld / fstp of the record's time and inputs): the emulator's x87 has payload-less NaNs (it
         * stores every NaN as 0x7fffffff / 0xffffffff), real x87 keeps the payload and quiets a signaling NaN (bike_x87_copy32), so
         * these two cases differ exactly in the x87-copied words and what is computed from them: only with RX_REMOTE_X87NAN=1. */
        if (std::getenv("RX_REMOTE_X87NAN")) apply("record time NaN", T0, v, 9.0f, std::nanf(""), false, nullptr);
        apply("ring position NaN", T0, v, 9.0f, 9.5f, false, [&](uint32_t) {
            wr(S + 0x2390 + 0x70 * 3 + 4, 0x7fc01234u); wr(S + 0x2390 + 0x70 * 4, 0xffc00042u); });
        apply("equal ring times", T0, v, 9.0f, 9.5f, false, [&](uint32_t) {
            wf(S + 0x2390 + 0x70 * 4 + 0x40, rf(S + 0x2390 + 0x70 * 5 + 0x40)); });
        if (std::getenv("RX_REMOTE_X87NAN")) apply("input SNaN", T0, v, 9.0f, 9.5f, false, [&](uint32_t rec) {
            wr(rec + 0x48, 0x7f800001u); wr(rec + 0x60, 0xff800123u); wr(rec + 0x44, 0x20000u); });
        apply("zero orientation", T0, v, 9.0f, 9.5f, false, [&](uint32_t) {
            for (unsigned i = 0; i < 20; i++) for (unsigned c = 0; c < 4; c++) wr(S + 0x2390 + 0x70 * i + 0x10 + 4 * c, 0); });
        apply("infinite orientation", T0, v, 9.0f, 9.5f, false, [&](uint32_t) { wr(S + 0x2390 + 0x70 * 2 + 0x10, 0x7f800000u); });
    }
    /* the track filters with a filter time over 5 (the output moves by 5 / it): 0042e840 and the inline scalar evaluation of 0042ec50 */
    for (int which = 0; which < 2; which++) {
        const uint32_t track = alloc(0x100), out = alloc(0x10), key = alloc(0x10);
        invoke(0x0042e8e0, [&](rx_regs& in) { in.r[1] = track; }, {});
        for (int i = 1; i <= 3; i++) {
            wf(key + 0, 0.3f * float(i)); wf(key + 4, -0.2f * float(i * i)); wf(key + 8, 0.1f); wf(key + 12, 0.9f);
            if (which == 0) invoke(0x0042e720, [&](rx_regs& in) { in.r[1] = key; set_xmm_f(in, 2, float(i)); }, {track});
            else invoke(0x0042eb20, [&](rx_regs& in) { in.r[2] = track; }, {key, rx_oracle::bits(float(i))});
        }
        wf(track + 0x3c, 10.0f); wf(track + 0xa8, 12.5f); wf(track + 0xd8, 7.0f);
        if (which == 0) invoke(0x0042e840, [&](rx_regs& in) { in.r[1] = out; set_xmm_f(in, 1, 2.5f); }, {track});
        else invoke(0x0042ec50, [&](rx_regs& in) { in.r[2] = track; in.r[6] = out; set_xmm_f(in, 5, 2.5f); }, {});
        cases++;
        std::fprintf(stderr, "[remote] track filter %d: out %g %g %g %g\n", which, rf(out), rf(out + 4), rf(out + 8), rf(out + 12));
        restore_all();
    }
    /* RX_PENDING_PROBE=<bxml file>: the MX simulation's pending tuning reload 0043b640 (RavenSimulationMotocross::vf8) with the
     * file as the pending stream [0x009e3100] (a MemoryInputStream as the tuning receiver 004391a0 builds it) and the reload flag
     * [0x009cc898] set: the reload's loader 008252c0 (build_xml_stream.c) runs from the core's 0043b640 */
    if (const char* path = std::getenv("RX_PENDING_PROBE")) {
        std::FILE* f = std::fopen(path, "rb");
        std::vector<uint8_t> doc;
        if (f) { int ch; while ((ch = std::fgetc(f)) != EOF) doc.push_back(uint8_t(ch)); std::fclose(f); }
        for (int variant = 0; variant < 2 && !doc.empty(); variant++) {
            const uint32_t size = variant == 0 ? uint32_t(doc.size()) : uint32_t(doc.size() / 2);   /* whole / cut short */
            const uint32_t bytes = alloc(size);
            m.mem.write_bytes(bytes, doc.data(), size);
            const uint32_t stream = alloc(0x1c);
            wr(stream + 0x00, 0x00974d1c); wr(stream + 0x04, bytes); wr(stream + 0x08, size);
            wr(stream + 0x0c, 0); wr(stream + 0x10, 0); wr(stream + 0x14, 0); wr(stream + 0x18, 0);
            wr(0x009e3100, stream);
            wr(0x009cc898, 1);
            invoke(0x0043b640, [&](rx_regs& in) { in.r[1] = S; }, {});
            cases++;
            std::fprintf(stderr, "[remote] pending tuning %s (%u bytes): pending %08x flag %u\n", path, size, rd(0x009e3100), rd(0x009cc898));
            restore_all();
        }
    }
    std::fprintf(stderr, "[remote] frame %u: %u cases on S %08x\n", g_frame, cases, S);
    return reflex_vm::RUN_ORIGINAL;
}
