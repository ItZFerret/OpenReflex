// stunt_probe.cpp -- the oracle probe for the stunt input mappings and the gesture matcher (agent "stuntgesture"):
//   core/game/fx_stunt_input.c: 006ca5a0 (the two mapping loads), 006ca3f0 (one "StuntMapping" bxml), 006ca2f0 (a record) and
//   006c9aa0 (the gesture matcher, the port `rx_fx_stunt_mapping_match`).
// No reference world reaches 006c9aa0: the trick input machine only reads a gesture when the right stick leaves the centre
// (006c9510 state 1 -> 2), and the reference world driver holds keys, not the pad's right stick. The probe therefore calls the
// matcher through its original address on the live game state (the mapping table the game itself loaded in
// [0xa94310] / [0xa94314]) with every gesture the classifier can produce (0..3, 1..4 chars, misses, an empty string), and drives
// the real call site: 006c9510 (the trick input machine) on the local player's tracker K = player + 0xb58 with its state fields
// set so the state-2 branch reaches the match. Every call goes through the original address, so with the port installed Verify
// runs the core function and then the original on the same state and compares memory and registers.
//   REFLEX_PORT=rx_fx,rx_StuntProbeHook REFLEX_PORT_VERIFY=1 RX_STUNT_PROBE=<groups> [RX_STUNT_PROBE_FRAME=<n>]
//   groups: m the matcher cases (default), i the 006c9510 call-site path, l the 006ca3f0 loader (a second load of the aerial file)
#include "oracle.h"
extern "C" {
#include "../game/crt.h"
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
// a field the probe sets and restores (the live tracker / the mapping globals are shared game state)
struct Poke { uint32_t a, old; Poke(uint32_t addr, uint32_t v) : a(addr), old(rd(addr)) { wr(addr, v); } ~Poke() { wr(a, old); } };
// a string in the game stack below the current SP (the callee's frame goes below it)
struct StackStr {
    uint32_t sp, at;
    StackStr(const char* s, uint32_t n = 0x40) : sp(rx_sp()), at(sp - n) {
        rx_set_sp(at);
        uint32_t i = 0;
        for (; s[i] && i + 1 < n; i++) rx_wr8(at + i, uint8_t(s[i]));
        rx_wr8(at + i, 0);
    }
    ~StackStr() { rx_set_sp(sp); }
};
/* the game's std::string constructor for the map lookups (msvcr90's own port) */
void crt_probe_str(uint32_t at, uint32_t src) { crt_str_ctor_cstr(at, src); }
uint32_t match(const char* gesture, uint32_t air) {
    StackStr s(gesture);
    rx_regs in = regs(); in.r[7] = s.at;                 // EDI = the gesture (006c9aa0)
    const uint32_t a[1] = {air};
    const rx_regs out = call(0x006c9aa0, in, 1, a);
    return out.r[0];
}
uint32_t table_len(uint32_t table) {
    uint32_t n = 0;
    while (table && mapped(table + 0x22u * n) && rx_rd8(table + 0x22u * n) != 0 && n < 200) n++;
    return n;
}
std::string str_at(uint32_t a, uint32_t max) {
    std::string s;
    for (uint32_t i = 0; i < max && mapped(a + i) && rx_rd8(a + i); i++) s.push_back(char(rx_rd8(a + i)));
    return s;
}
/* the first enabled target's value of a listener vector (0x63b8f0's read) */
float listener_value(uint32_t vec) {
    if (!vec || !mapped(vec)) return 0.0f;
    const uint32_t e = rd(vec + 0xc);
    if (!e || !mapped(e)) return 0.0f;
    const uint32_t t = rd(e);
    return t && mapped(t) ? f32(rd(t)) : 0.0f;
}
void probe_match(void) {
    const uint32_t air = rd(0x00a94310), ground = rd(0x00a94314);
    std::fprintf(stderr, "[stuntprobe] tables aerial %08x (%u) ground %08x (%u)\n", air, table_len(air), ground, table_len(ground));
    /* every gesture the classifier can produce: 1..4 characters of '0'..'3' */
    char g[8];
    int n = 0, hits = 0;
    for (int len = 1; len <= 4; len++) {
        const int total = 1 << (2 * len);
        for (int v = 0; v < total; v++) {
            int x = v;
            for (int k = 0; k < len; k++) { g[k] = char('0' + (x & 3)); x >>= 2; }
            g[len] = 0;
            for (uint32_t a = 0; a <= 1; a++) { const uint32_t r = match(g, a); n++; if (r) hits++; }
        }
    }
    /* misses / odd shapes */
    const char* extra[] = {"", "N", "4", "01234", "32 3", "0x30", "22"};
    for (const char* e : extra) for (uint32_t a = 0; a <= 1; a++) { const uint32_t r = match(e, a); n++; if (r) hits++; }
    std::fprintf(stderr, "[stuntprobe] matcher: %d calls, %d hits\n", n, hits);
    /* the product's old state (no table) cannot be probed on the emulator: address 0 is unmapped there, so 006c9aa0's first
     * read faults; the product's zero page makes it return 0. */
}
void probe_input(void) {
    const uint32_t G = rd(0x00a946b8);
    const uint32_t P = G ? G + 0x7460 : 0;
    const uint32_t K = P ? P + 0xb58 : 0;
    const uint32_t V = P ? rd(P + 0x1ac) : 0;
    std::fprintf(stderr, "[stuntprobe] game %08x player %08x tracker %08x vehicle %08x local %u type %u pad %d state %d\n",
                 G, P, K, V, rd(P + 4), rd(P + 0x10), int32_t(rd(P + 0x1f0)), int32_t(rd(K + 0x5c)));
    if (!K || !V) { std::fprintf(stderr, "[stuntprobe] no live local player\n"); return; }
    /* the live tracker's gesture path is only reached with the right stick off centre; the reference world has no pad, so put the
     * state the classifier leaves after a 3-direction gesture ("323") into K and a matching ground mapping in [0xa94314] (the
     * shipped ground file has no records) */
    const uint32_t asize[1] = {0x22};
    const uint32_t rec = call(0x0078fbe0, regs(), 1, asize).r[0];   /* ReflexAlignedAlloc(0x22) */
    const char* name = "9OClockNac_L_Air";
    for (uint32_t i = 0; i <= std::strlen(name); i++) rx_wr8(rec + i, uint8_t(name[i]));
    const char* gest = "323";
    for (uint32_t i = 0; i <= std::strlen(gest); i++) rx_wr8(rec + 0x18 + i, uint8_t(gest[i]));
    Poke t(0x00a94314, rec);
    Poke s5c(K + 0x5c, 2), s34(K + 0x34, '3'), s35(K + 0x35, '2'), s38(K + 0x38, 3), s3c(K + 0x3c, 0),
         s40(K + 0x40, 0), s44(K + 0x44, bits(1.0f)), s4c(K + 0x4c, 1), s94(K + 0x94, 0);
    Poke b0(K + 0x30, 0x00333233u);                     /* "323\0" at K + 0x30 (little endian) */
    const uint32_t dt = bits(1.0f / 60.0f);
    const uint32_t a[2] = {K, dt};
    const rx_regs in = regs();
    const rx_regs out = call(0x006c9510, in, 2, a);
    (void)out;
    std::fprintf(stderr, "[stuntprobe] trick input: state %d matched %08x history index %u entry \"%s\" flag %u\n",
                 int32_t(rd(K + 0x5c)), rd(K + 0x60), rd(K + 0x94), str_at(K + 0x98, 4).c_str(),
                 rd(K + 0x9c));
}
void probe_load(void) {
    StackStr name("AerialStuntInputMappings.bxml", 0x80);
    rx_regs in = regs();
    const uint32_t a[1] = {name.at};
    const rx_regs out = call(0x006ca3f0, in, 1, a);
    std::fprintf(stderr, "[stuntprobe] loader %08x entries %u\n", out.r[0], table_len(out.r[0]));
}
/* the trick start / hold modes (agent "trickstart"): 006cd050 / 006cd0f0 / 006cd1c0 (the perf asset gate and the hold time),
 * 006c98c0 (the start; posts "StuntBegin"), 006c9b60 / 006c9cc0 / 006c9da0 (hold modes 1..3), 006c9c20 (the chain),
 * 006c9e80 / 006c9eb0 / 006c9df0 (the hold-begin / hold-end / complete events) and the scorer's handlers 006cb1b0 / 006cb3c0 /
 * 006cb560 / 006cb5a0 / 006cb610 / 006cb660, all on the live local player's tracker and the real StuntTable / perfdat handles. */
void probe_trick(void) {
    const uint32_t G = rd(0x00a946b8), P = G ? G + 0x7460 : 0, K = P ? P + 0xb58 : 0;
    const uint32_t V = P ? rd(P + 0x1ac) : 0;
    if (!K || !V) { std::fprintf(stderr, "[stuntprobe] no live local player for the trick start\n"); return; }
    const uint32_t cls = rd(V + 0x194);
    const uint32_t rider = rd(P + 0x1b4);
    const uint32_t S = P + 0xc70;                    /* the stunt scorer (the handlers' ECX) */
    StackStr name("9OClockNac_L_Air");
    uint32_t air_rec = 0, ground_rec = 0;
    for (uint32_t kind = 0; kind <= 1; kind++) {
        const uint32_t a[2] = {name.at, kind};
        const uint32_t rec = call(0x006c8570, regs(), 2, a).r[0];
        std::fprintf(stderr, "[stuntprobe] 006c8570 kind %u -> %08x (anim '%s' +0x64 %u)\n", kind, rec,
                     rec ? str_at(rec + 0x20, 0x18).c_str() : "", rec ? rd(rec + 0x64) : 0);
        if (kind) air_rec = rec; else ground_rec = rec;
    }
    const uint32_t trick = air_rec ? air_rec : ground_rec;
    if (!trick) { std::fprintf(stderr, "[stuntprobe] the trick is not in the table\n"); return; }
    /* the perf asset gate, the handle and the hold time (both tricks of that name) */
    for (uint32_t i = 0; i < 2; i++) {
        const uint32_t t = i ? ground_rec : air_rec;
        if (!t) continue;
        const uint32_t a[1] = {cls};
        rx_regs in = regs(); in.r[1] = t;
        const uint32_t has = call(0x006cd050, in, 1, a).r[0];
        in = regs(); in.r[1] = t;
        const uint32_t h = call(0x006cd0f0, in, 1, a).r[0];
        std::fprintf(stderr, "[stuntprobe] 006cd050 %08x -> %u, 006cd0f0 -> %08x asset %08x\n", t, has, h, h ? rd(h + 0x44) : 0);
        in = regs(); in.r[1] = t; in.r[0] = cls;
        const uint32_t hv = call(0x006cd1c0, in, 0, nullptr).xmm[0].v[0];
        std::fprintf(stderr, "[stuntprobe] 006cd1c0 %08x -> %.4f\n", t, f32(hv));
    }
    /* the start (the real player / vehicle): K + 0x60 = the match, mode 0 -> mode 1 + "StuntBegin" */
    wr(K + 0x1c, 0); wr(K + 8, 0); wr(K + 0x60, trick); wr(K + 0x14, 0); wr(K + 0x18, 0); wr(K + 0x20, 0); wr(K + 0x28, 0);
    { rx_regs in = regs(); in.r[6] = K; in.r[7] = trick;
      call(0x006c98c0, in, 0, nullptr);
      std::fprintf(stderr, "[stuntprobe] 006c98c0 -> mode %u trick %08x holder %08x landed %u\n", rd(K + 8), rd(K + 0x1c), rd(K + 0x20), rd(K + 0x28)); }
    /* the hold-begin / hold-end events (the current trick from the start above) */
    { rx_regs in = regs(); in.r[6] = K; call(0x006c9e80, in, 0, nullptr);
      std::fprintf(stderr, "[stuntprobe] 006c9e80 -> mode %u hold clock %.3f\n", rd(K + 8), f32(rd(K + 0x10)));
      wr(K + 8, 1);
      in = regs(); in.r[6] = K; call(0x006c9eb0, in, 0, nullptr);
      std::fprintf(stderr, "[stuntprobe] 006c9eb0 -> mode %u\n", rd(K + 8)); }
    /* the complete (the hold reset + "StuntCompleted") */
    { wr(K + 0x1c, trick); wr(K + 8, 3);
      rx_regs in = regs(); in.r[0] = K; call(0x006c9df0, in, 0, nullptr);
      std::fprintf(stderr, "[stuntprobe] 006c9df0 -> mode %u current %08x input %u committed %u\n", rd(K + 8), rd(K + 0x1c),
                   rd(K + 0x5c), rd(K + 0xc)); }
    /* mode 1: the flag fresh / set, the air trick, then a forced looping block so the hold-begin path runs */
    for (int v = 0; v < 3; v++) {
        wr(K + 0x1c, trick); wr(K + 8, 1); wr(K + 0x18, v == 0 ? 0 : 1); wr(K + 0x10, 0); wr(K + 0x14, 0);
        {   /* the hold time and the rider's progress the mode reads (a diagnostic) */
            rx_regs vi = regs(); vi.r[1] = trick; vi.r[0] = cls;
            const uint32_t val = call(0x006cd1c0, vi, 0, nullptr).xmm[0].v[0];
            const uint32_t rr = rd(P + 0x1b4);
            const uint32_t lst = rr + (rd(rr + 0xaf10) == 3 ? 0x48c : 0x46c);
            const uint32_t la[1] = {lst};
            const uint32_t got = call(0x0063b8f0, regs(), 1, la).xmm[0].v[0];
            std::fprintf(stderr, "[stuntprobe]   pre %d hold-time %.4f progress %.4f (K %08x P %08x)\n", v, f32(val), f32(got), K, P);
        }
        uint32_t e = 0, o40 = 0, o4c = 0, o6c = 0;
        if (v == 2) {   /* a looping block whose start is below the rider's progress (a stimulus, restored below) */
            rx_regs hi = regs(); hi.r[1] = trick;
            const uint32_t a1[1] = {cls};
            const uint32_t h = call(0x006cd0f0, hi, 1, a1).r[0];
            const uint32_t asset = h ? rd(h + 0x44) : 0;
            if (asset && rd(asset + 0x14) > 0) {
                e = rd(asset + 0x18); o40 = rd(e + 0x40); o4c = rd(e + 0x4c); o6c = rd(e + 0x6c);
                wr(e + 0x40, 0); wr(e + 0x4c, bits(1.0f / 120.0f)); wr(e + 0x6c, 1);
            }
        }
        const uint32_t a[1] = {bits(1.0f / 60.0f)};
        rx_regs in = regs(); in.r[0] = K; in.r[6] = K;
        call(0x006c9b60, in, 1, a);
        std::fprintf(stderr, "[stuntprobe] 006c9b60 case %d -> mode %u flag %u clock10 %.3f clock14 %.3f\n", v, rd(K + 8), rd(K + 0x18),
                     f32(rd(K + 0x10)), f32(rd(K + 0x14)));
        if (e) { wr(e + 0x40, o40); wr(e + 0x4c, o4c); wr(e + 0x6c, o6c); }
    }
    /* mode 2: an airborne trick, the ground clock (the record's +0x64 poked), the used clock and the hold-end flag */
    for (int v = 0; v < 4; v++) {
        wr(K + 0x1c, trick); wr(K + 8, 2); wr(K + 0x18, v == 3 ? 1 : 0); wr(K + 0x10, 0); wr(K + 0x14, 0);
        if (v == 1) wr(trick + 0x64, 0);
        if (v == 2) { wr(K + 0x14, bits(9.0f)); wr(trick + 0x64, 0); }
        rx_regs in = regs(); in.r[0] = K; in.r[6] = K; in.xmm[0].v[0] = bits(1.0f / 60.0f);
        call(0x006c9cc0, in, 0, nullptr);
        std::fprintf(stderr, "[stuntprobe] 006c9cc0 case %d (kind %u) -> mode %u clock10 %.3f clock14 %.3f\n", v, rd(trick + 0x64),
                     rd(K + 8), f32(rd(K + 0x10)), f32(rd(K + 0x14)));
        if (v >= 1) wr(trick + 0x64, 1);
    }
    /* mode 3: above the rider's +0xaf20 threshold (chain) and at it (complete) */
    if (rider) {
        const uint32_t old = rd(rider + 0xaf20);
        for (int v = 0; v < 2; v++) {
            wr(K + 0x1c, trick); wr(K + 8, 3);
            wr(rider + 0xaf20, v ? 0 : bits(1.0e6f));
            rx_regs in = regs(); in.r[0] = K; in.r[6] = K;
            call(0x006c9da0, in, 0, nullptr);
            std::fprintf(stderr, "[stuntprobe] 006c9da0 case %d -> mode %u current %08x\n", v, rd(K + 8), rd(K + 0x1c));
        }
        wr(rider + 0xaf20, old);
    }
    /* the chain: no current trick (early), then the same match (early), then the airborne start path */
    wr(K + 0x1c, 0); wr(K + 0x60, trick);
    { rx_regs in = regs(); in.r[0] = K; call(0x006c9c20, in, 0, nullptr); }
    wr(K + 0x1c, trick);
    { rx_regs in = regs(); in.r[0] = K; call(0x006c9c20, in, 0, nullptr);
      std::fprintf(stderr, "[stuntprobe] 006c9c20 (same trick) -> mode %u current %08x\n", rd(K + 8), rd(K + 0x1c)); }
    if (rider) { wr(rider + 0xaf10, 3); wr(K + 0x60, trick);
        rx_regs in = regs(); in.r[0] = K; call(0x006c9c20, in, 0, nullptr);
        std::fprintf(stderr, "[stuntprobe] 006c9c20 (rider state 3) -> mode %u\n", rd(K + 8)); }
    /* the min-progress check, the switch event and the chain note (their own per-call entries) */
    { rx_regs in = regs(); in.r[0] = K;
      const uint32_t r = call(0x006c9a40, in, 0, nullptr).r[0];
      in = regs(); in.r[1] = trick; in.r[2] = trick;
      const uint32_t b[1] = {P};
      call(0x006cd7b0, in, 1, b);
      in = regs(); in.r[0] = S; in.r[1] = trick;
      call(0x006ccef0, in, 0, nullptr);
      std::fprintf(stderr, "[stuntprobe] 006c9a40 -> %u; 006cd7b0 / 006ccef0 done; scorer %08x %u/%u/%u player %08x/%u/%u\n", r,
                   S, rd(S), rd(S + 4), rd(S + 0x10), rd(S + 0x128), rd(rd(S + 0x128) + 4), rd(rd(S + 0x128) + 0x10)); }
    /* 00581830 ActivityAudioLogic::vf5: the crowd listener on the StuntBegin payload */
    { rx_regs in = regs(); in.r[1] = 0;
      const uint32_t a[2] = {P, 0};
      call(0x00581830, in, 2, a);
      std::fprintf(stderr, "[stuntprobe] 00581830 crowd excite done\n"); }
    /* 00581950 ActivityAudioLogic::vf8: the "StuntCompleted" listener (trickland): the cue then the crowd reaction, on the race
     * activity's audio logic (activity + 0xfd8; the payload is the same {player, trick} the event carries) */
    { const uint32_t act = rd(P + 0x94), logic = act ? rd(act + 0xfd8) : 0;
      std::fprintf(stderr, "[stuntprobe] 00581950 activity %08x logic %08x player %08x +4 %u +0x10 %u trick %08x\n", act, logic,
                   P, rd(P + 4), rd(P + 0x10), trick);
      if (logic) {
          for (int v = 0; v < 3; v++) {
              Poke pl(P + 4, v == 1 ? 0u : 1u), px(P + 0x10, v == 2 ? 1u : 0u);
              rx_regs in = regs(); in.r[1] = logic;
              const uint32_t a[2] = {P, trick};
              call(0x00581950, in, 2, a);
              std::fprintf(stderr, "[stuntprobe] 00581950 case %d done (seed %08x)\n", v, rd(0x009cb860));
          }
      } }
    /* the scorer's handlers with the real player / trick: the reference world's player never joins an activity, so its stunt
     * system is not built; construct it the game's way (006ca680), enable it and link the owner (the race start does both) */
    { rx_regs ci = regs(); ci.r[0] = S; const uint32_t r = call(0x006ca680, ci, 0, nullptr).r[0];
      std::fprintf(stderr, "[stuntprobe] 006ca680 -> %08x maps 324 proxy %08x head %08x | 344 proxy %08x head %08x\n", r,
                   rd(S + 0x324), rd(S + 0x33c), rd(S + 0x344), rd(S + 0x35c)); }
    { Poke en(S, 1), own(S + 0x128, P);   /* the scorer's owner, linked at the race start in the original */
      rx_regs in = regs(); in.r[1] = S;
      const uint32_t a[2] = {P, trick};
      call(0x006cb1b0, in, 2, a);
      std::fprintf(stderr, "[stuntprobe] 006cb1b0 -> combo state %u score %.3f count %u\n", rd(S + 0x124), f32(rd(S + 0x78)), rd(S + 0x418));
      call(0x006cb3c0, in, 2, a);
      std::fprintf(stderr, "[stuntprobe] 006cb3c0 -> count %u score %.3f recent0 '%s'\n", rd(S + 0x418), f32(rd(S + 0x78)),
                   str_at(S + 0x364, 0x20).c_str());
      { const uint32_t b[3] = {P, trick, trick}; in = regs(); in.r[1] = S; call(0x006cb560, in, 3, b); }
      call(0x006cb5a0, in, 2, a);
      call(0x006cb610, in, 2, a);
      call(0x006cb660, in, 2, a);
      std::fprintf(stderr, "[stuntprobe] handlers -> combo %u score %.3f flags 41c %u 420 %u 310 %u\n", rd(S + 0x124),
                   f32(rd(S + 0x78)), rd(S + 0x41c), rd(S + 0x420), rd(S + 0x310)); }
}

/* the rider's stunt-animation start (agent "trickstart"): 0054d120 (the player's StuntBegin listener), 00622bc0 (the air
 * trick: the perf clip drives the float listeners), 006226c0 / 00622790 (the ground trick), 0054d3b0 (the control
 * suppression), 006ccfc0 (the "%sRider_%s" name), 0063b6e0 / 00850cd0 (the clip map lookups) and 0063b2b0 / 0063b1e0 (the
 * listener select). The reference world's player never joins an activity, so the tracker's current trick (player + 0xb74) is
 * poked in (the trick the event carries) and the stunt system is constructed (006ca680) for the scorer handlers. */
void probe_rider(void) {
    const uint32_t G = rd(0x00a946b8), P = G ? G + 0x7460 : 0, K = P ? P + 0xb58 : 0;
    const uint32_t V = P ? rd(P + 0x1ac) : 0;
    if (!K || !V) { std::fprintf(stderr, "[stuntprobe] no live local player for the rider stunt start\n"); return; }
    const uint32_t ra = rd(V + 0x1a0);
    const uint32_t cls = rd(V + 0x194);
    StackStr name("9OClockNac_L_Air");
    const uint32_t a2[2] = {name.at, 1};
    const uint32_t trick = call(0x006c8570, regs(), 2, a2).r[0];
    std::fprintf(stderr, "[stuntprobe] rider %08x player %08x trick %08x class %u af10 %u pose %u\n", ra, P, trick, cls,
                 rd(ra + 0xaf10), rd(ra + 0x5b57c));
    if (!trick || !ra) return;
    /* 006ccfc0: the name into a stack buffer */
    { const uint32_t sp = rx_sp(), dst = sp - 0x120;
      rx_set_sp(dst);
      rx_regs in = regs(); in.r[0] = cls; in.r[6] = 0x104; in.r[7] = dst;
      const uint32_t a[1] = {trick + 0x20};
      call(0x006ccfc0, in, 1, a);
      std::fprintf(stderr, "[stuntprobe] 006ccfc0 cls %u -> '%s'\n", cls, str_at(dst, 0x40).c_str());
      rx_set_sp(sp); }
    /* the tracker's current trick: the listener's name source (player + 0xb74) */
    Poke cur(P + 0xb74, trick);
    /* 0063b6e0 / 00850cd0 on the rider's real maps with a name the animation assets carry (a trick table clip name) */
    { const uint32_t sp = rx_sp(), buf = sp - 0x120, st = sp - 0x160;
      rx_set_sp(st);
      const uint32_t tabname = rd(ra + 0xba24) > 0 ? (ra + 0xaffc) : (rd(ra + 0xc450) > 0 ? (ra + 0xba28) : 0);
      const std::string nm = tabname ? str_at(tabname, 0x28) : std::string("9OClockNac_L_Air");
      for (uint32_t i = 0; i <= nm.size(); i++) rx_wr8(buf + i, uint8_t(i < nm.size() ? nm[i] : 0));
      crt_probe_str(st, buf);
      std::fprintf(stderr, "[stuntprobe] map key '%s'\n", nm.c_str());
      for (int m = 0; m < 4; m++) {
          const uint32_t map = ra + (m == 0 ? 0xaf7c : m == 1 ? 0xaf9c : m == 2 ? 0xafbc : 0xafdc);
          rx_regs in = regs(); in.r[0] = st; in.r[7] = map;
          const uint32_t a[1] = {st};
          const rx_regs out = call(m < 2 ? 0x0063b6e0 : 0x00850cd0, in, 1, a);
          std::fprintf(stderr, "[stuntprobe] %s %08x -> %08x value %08x\n", m < 2 ? "0063b6e0" : "00850cd0", map, out.r[0],
                       out.r[0] ? rd(out.r[0]) : 0);
      }
      rx_set_sp(sp); }
    /* 00622bc0: the air trick start (the trick's +0x64 = 1 picks the air maps) */
    { const uint32_t a[2] = {ra, P};
      rx_regs in = regs(); in.r[1] = trick;
      call(0x00622bc0, in, 2, a);
      std::fprintf(stderr, "[stuntprobe] 00622bc0 -> state %u clip %08x len %.4f id %04x variant %u threshold %.4f\n",
                   rd(ra + 0xaf10), rd(ra + 0xaf14), f32(rd(ra + 0xaf18)), rd(ra + 0xaf3c) & 0xffff, rd(ra + 0xaf28),
                   f32(rd(ra + 0xaf24))); }
    /* 00622790: the ground start (the pushed EDI is its 00621490 argument; a real table entry so its clip exists) */
    { const uint32_t air = rd(trick + 0x64);
      const uint32_t cnt = air ? rd(ra + 0xba24) : rd(ra + 0xc450);
      const uint32_t tab = air ? (ra + 0xb01c) : (ra + 0xba48);
      const uint32_t clip = cnt > 0 ? (rd(tab) & 0xffff) : 0;
      const uint32_t a[2] = {air, clip};
      rx_regs in = regs(); in.r[7] = ra;
      call(0x00622790, in, 2, a);
      std::fprintf(stderr, "[stuntprobe] 00622790 -> state %u variant %u clip %08x len %.4f pose590 %u\n", rd(ra + 0xaf10),
                   rd(ra + 0xaf28), rd(ra + 0xaf14), f32(rd(ra + 0xaf18)), rd(ra + 0x5b590)); }
    /* 006226c0: the clip start (rider state 0, pose 0xc; its map gives the clip id the rider table is scanned for) */
    { const uint32_t old = rd(ra + 0xaf10);
      const uint32_t o64 = rd(trick + 0x64);
      wr(ra + 0xaf10, 0); wr(ra + 0x5b57c, 0xc); wr(trick + 0x64, 1);
      { const uint32_t a[3] = {ra, P, trick};
        rx_regs in = regs(); in.r[7] = ra;
        call(0x006226c0, in, 3, a); }
      std::fprintf(stderr, "[stuntprobe] 006226c0 -> state %u variant %u clip %04x (tables: air n %u id0 %04x id1 %04x, ground n %u id0 %04x)\n",
                   rd(ra + 0xaf10), rd(ra + 0xaf28), rd(ra + 0xaf3c) & 0xffff, rd(ra + 0xba24),
                   rd(ra + 0xba24) > 0 ? rd(ra + 0xb01c) & 0xffff : 0, rd(ra + 0xba24) > 1 ? rd(ra + 0xb044) & 0xffff : 0,
                   rd(ra + 0xc450), rd(ra + 0xc450) > 0 ? rd(ra + 0xba48) & 0xffff : 0);
      /* the two early exits */
      wr(ra + 0xaf00, 1);
      { const uint32_t a[3] = {ra, P, trick}; rx_regs in = regs(); in.r[7] = ra; call(0x006226c0, in, 3, a); }
      wr(ra + 0xaf00, 0); wr(ra + 0x5b57c, 0);
      { const uint32_t a[3] = {ra, P, trick}; rx_regs in = regs(); in.r[7] = ra; call(0x006226c0, in, 3, a); }
      wr(ra + 0x5b57c, 0xc); wr(ra + 0xaf10, old); wr(trick + 0x64, o64); }
    /* 0063b7d0 / 0063c190 with a made-up vector: one record {lo 0, hi 10, tau 2, ? 0, cur 0} and its two listeners */
    { const uint32_t sp = rx_sp();
      const uint32_t vec = sp - 0x40, rec = sp - 0x80, val = sp - 0x90, lst = sp - 0x100;
      rx_set_sp(rec);
      wr(rec + 0, bits(0.0f)); wr(rec + 4, bits(10.0f)); wr(rec + 8, bits(2.0f)); wr(rec + 0xc, 0); wr(rec + 0x10, 0);
      wr(rec + 0x14, 0x30000f50); wr(rec + 0x18, 1); wr(rec + 0x1c, 0); wr(rec + 0x20, 0);
      wr(vec + 0, 0x30000f50); wr(vec + 0xc, sp - 0x20); wr(vec + 0x10, sp - 0x1c);
      wr(sp - 0x20, rec);
      wr(val, bits(5.0f));
      rx_regs in = regs(); in.r[0] = vec;
      const uint32_t a[1] = {val};
      call(0x0063b7d0, in, 1, a);
      std::fprintf(stderr, "[stuntprobe] 0063b7d0 (0->5) cur %.4f\n", f32(rd(rec + 0x10)));
      wr(rec + 0x10, 0); wr(val, bits(-3.0f));
      in = regs(); in.r[0] = vec;
      call(0x0063b7d0, in, 1, a);
      std::fprintf(stderr, "[stuntprobe] 0063b7d0 (0->-3) cur %.4f\n", f32(rd(rec + 0x10)));
      { const uint32_t b[6] = {sp - 0x1c, val, 0, sp - 0x20, 0, sp - 0x1c};
        wr(rec + 0x10, 0); wr(val, bits(5.0f));
        call(0x0063c190, regs(), 6, b);
        std::fprintf(stderr, "[stuntprobe] 0063c190 (0->5) cur %.4f\n", f32(rd(rec + 0x10))); }
      rx_set_sp(sp); }
    /* 0063b2b0 / 0063b1e0: the listener select on the rider's two listener vectors */
    { const uint32_t a[2] = {ra + 0x46c, 0x12};
      call(0x0063b2b0, regs(), 2, a);
      const uint32_t b[2] = {ra + 0x48c, 0x12};
      call(0x0063b1e0, regs(), 2, b);
      std::fprintf(stderr, "[stuntprobe] listeners 46c/48c selected 0x12\n"); }
    /* 0054d3b0: the control suppression (SteeringDisabled / IgnoreBrake / IgnoreThrottle poked on) */
    { const uint32_t o98 = rd(trick + 0x98), o94 = rd(trick + 0x94), o90 = rd(trick + 0x90);
      wr(trick + 0x98, 1); wr(trick + 0x94, 1); wr(trick + 0x90, 1);
      const uint32_t a[1] = {trick};
      rx_regs in = regs(); in.r[0] = P;
      call(0x0054d3b0, in, 1, a);
      std::fprintf(stderr, "[stuntprobe] 0054d3b0 -> 11cc %08x\n", rd(P + 0x11cc));
      wr(trick + 0x98, o98); wr(trick + 0x94, o94); wr(trick + 0x90, o90);
      const uint32_t o8 = rd(P + 0x11c8);
      wr(P + 0x11c8, 1);
      in = regs(); in.r[0] = P;
      call(0x0054d3b0, in, 1, a);
      std::fprintf(stderr, "[stuntprobe] 0054d3b0 (replay) -> 11cc %08x\n", rd(P + 0x11cc));
      wr(P + 0x11c8, o8);
    }
    /* 0054d120: the player's StuntBegin listener (the air branch: state != 0) */
    { const uint32_t a[2] = {P, trick};
      rx_regs in = regs(); in.r[1] = P;
      call(0x0054d120, in, 2, a);
      std::fprintf(stderr, "[stuntprobe] 0054d120 -> rider state %u clip %08x\n", rd(ra + 0xaf10), rd(ra + 0xaf14)); }
    /* 0054d220: the player's StuntCompleted listener (trickland): another player (skip), state 0 (no EndCrash), state 3 (the
     * crash end), and a type-7 activity (the trick's +0x6c added to the player's +0x11b8) */
    { const uint32_t act = rd(P + 0x94);
      std::fprintf(stderr, "[stuntprobe] 0054d220 player %08x act %08x type %u af10 %u +11b8 %u net %08x\n", P, act,
                   act ? rd(act + 0x238) : 0, rd(ra + 0xaf10), rd(P + 0x11b8), rd(P + 0xb2c));
      for (int v = 0; v < 4; v++) {
          Poke a10(ra + 0xaf10, v == 2 ? 3u : 0u);
          Poke ty(act ? act + 0x238 : P + 0x120, v == 3 ? 7u : (act ? rd(act + 0x238) : 0u));
          const uint32_t a[2] = {v == 1 ? 0u : P, trick};
          rx_regs in = regs(); in.r[1] = P;
          call(0x0054d220, in, 2, a);
          std::fprintf(stderr, "[stuntprobe] 0054d220 case %d done (af10 %u +11b8 %u)\n", v, rd(ra + 0xaf10), rd(P + 0x11b8));
      } }
}

/* the gesture read: 00548810 (the stick pair from the pad's steer / lean maps) and 006cd2c0 (the classifier) on the live
 * tracker's input buffer K + 0x30, with the direction / dead zone / NaN / count / restart cases the machine distinguishes */
void probe_gesture(void) {
    const uint32_t G = rd(0x00a946b8), P = G ? G + 0x7460 : 0, K = P ? P + 0xb58 : 0;
    if (!K) { std::fprintf(stderr, "[stuntprobe] no tracker\n"); return; }
    const uint32_t pad = rd(P + 0x1f0);
    {   /* 00548810: the out pair on the game stack */
        const uint32_t sp = rx_sp(), at = sp - 0x20;
        rx_set_sp(at);
        wr(at, 0); wr(at + 4, 0);
        rx_regs in = regs(); in.r[0] = pad;                  /* EAX = pad (it walks g_ControllerKeyMapBlocks[pad]) */
        const uint32_t a[1] = {at};
        call(0x00548810, in, 1, a);
        std::fprintf(stderr, "[stuntprobe] 00548810 pad %d axes %.4f %.4f\n", int32_t(pad), f32(rd(at)), f32(rd(at + 4)));
        rx_set_sp(sp);
    }
    uint8_t saved[0x40];
    for (uint32_t i = 0; i < sizeof saved; i++) saved[i] = mapped(K + 0x30 + i) ? rx_rd8(K + 0x30 + i) : 0;
    struct Case { float dx, dy, dt; int air, count, cflag; uint32_t clock; const char* tag; };
    static const Case cases[] = {
        { 0, 1, 0.016666668f, 0, 0, 0, 0x3f800000u, "up/fresh" },
        { 1, 0, 0.016666668f, 0, 1, 0, 0x3f800000u, "right/count1" },
        { 0, -1, 0.016666668f, 0, 2, 0, 0x3f800000u, "down/count2" },
        { -1, 0, 0.016666668f, 0, 3, 0, 0x3f800000u, "left/count3" },
        { 0.3f, 0.2f, 0.016666668f, 0, 2, 0, 0x3f800000u, "dead zone" },
        { 0.7f, 0.7f, 0.016666668f, 1, 2, 0, 0x3f800000u, "diag/air" },
        { -0.7f, -0.7f, 0.016666668f, 1, 2, 0, 0x3f000000u, "diag-/-air/clock" },
        { 0, 1, 0.016666668f, 0, 2, 1, 0x3f800000u, "restart" },
        { 0, 1, 0.016666668f, 1, 0, 0, 0x80000000u, "-0 clock" },
        { NAN, 1, 0.016666668f, 0, 1, 0, 0x3f800000u, "nan x" },
        { NAN, NAN, 0.016666668f, 1, 2, 0, 0x3f800000u, "nan both" },
    };
    for (const Case& c : cases) {
        wr(K + 0x30, 0x4e4e4e4eu);                          /* bytes 0..3 (the gesture) */
        rx_wr8(K + 0x34, 'N'); rx_wr8(K + 0x35, 'N');
        wr(K + 0x38, c.count); wr(K + 0x3c, c.cflag);
        wr(K + 0x40, 0); wr(K + 0x44, c.clock); wr(K + 0x4c, 0);
        rx_regs in = regs(); in.r[6] = K + 0x30;             /* ESI = the buffer */
        const uint32_t a[4] = {bits(c.dt), bits(c.dx), bits(c.dy), (uint32_t)c.air};
        call(0x006cd2c0, in, 4, a);
        std::fprintf(stderr, "[stuntprobe] 006cd2c0 %-16s dir '%c' last '%c' count %u finished %u clock %.4f window %.4f\n", c.tag,
                     char(rx_rd8(K + 0x34)), char(rx_rd8(K + 0x35)), rd(K + 0x38), rd(K + 0x4c), f32(rd(K + 0x40)), f32(rd(K + 0x44)));
    }
    for (uint32_t i = 0; i < sizeof saved; i++) rx_wr8(K + 0x30 + i, saved[i]);
}
/* the rider's trick playback (agent "trickplayback"): the clip-range predicates 0083e170 / 0083e1d0 / 0083e230 / 0083e0e0, the
 * crash / trick clip update 00620a60, the crash step 00621f40 and its two clip starts 00622160 / 00622460, the first looping
 * end 006cd240 and ClampedFloat_Sub 0063b5f0. The reference world has no pad, so the trick is started through the game's own
 * rider clip start 00622bc0; every call goes through the original address under Verify. */
void probe_play(void) {
    const uint32_t G = rd(0x00a946b8), P = G ? G + 0x7460 : 0, K = P ? P + 0xb58 : 0;
    const uint32_t V = P ? rd(P + 0x1ac) : 0;
    if (!K || !V) { std::fprintf(stderr, "[stuntprobe] no live local player for the rider playback\n"); return; }
    const uint32_t ra = rd(V + 0x1a0), cls = rd(V + 0x194);
    StackStr name("9OClockNac_L_Air");
    const uint32_t a2[2] = {name.at, 1};
    const uint32_t trick = call(0x006c8570, regs(), 2, a2).r[0];
    std::fprintf(stderr, "[stuntprobe] playback rider %08x class %u trick %08x af10 %u\n", ra, cls, trick, rd(ra + 0xaf10));
    if (!trick || !ra) return;
    /* the first looping block's end (006cd240) */
    { rx_regs in = regs(); in.r[0] = cls; in.r[1] = trick;
      const float v = call(0x006cd240, in, 0, nullptr).xmm[0].v[0];
      std::fprintf(stderr, "[stuntprobe] 006cd240 -> %.4f\n", v); }
    /* the air trick's animator (00622bc0; the clip it sets is what 00620a60 drives). Its name source is the tracker's
     * current trick (player + 0xb74), which the reference world never sets (no pad). */
    { Poke cur(P + 0xb74, trick);
      const uint32_t a[2] = {ra, P}; rx_regs in = regs(); in.r[1] = trick;
      call(0x00622bc0, in, 2, a);
      /* the player's StuntBegin listener, as at a real trick start (state 3 -> the air path) */
      const uint32_t b[2] = {P, trick}; rx_regs in2 = regs(); in2.r[1] = P;
      call(0x0054d120, in2, 2, b);
      std::fprintf(stderr, "[stuntprobe] after 0054d120: state %u clip %08x len %.4f\n", rd(ra + 0xaf10), rd(ra + 0xaf14), f32(rd(ra + 0xaf18)));
      in2 = regs(); in2.r[1] = P;
      call(0x0054d120, in2, 2, b);
      std::fprintf(stderr, "[stuntprobe] after 0054d120 (2): state %u clip %08x len %.4f\n", rd(ra + 0xaf10), rd(ra + 0xaf14), f32(rd(ra + 0xaf18))); }
    /* 00622bc0 puts the trick's animation clip in +0xaf18 (the next clip; +0xaf14 is the currently playing one) */
    const uint32_t clip = rd(ra + 0xaf14) ? rd(ra + 0xaf14) : rd(ra + 0xaf18);
    const uint32_t c158c0 = rd(ra + 0x58c), c15980 = rd(ra + 0x598);
    const uint32_t blocks = clip ? rd(rd(clip + 0x70) + 0x18) : 0;
    std::fprintf(stderr, "[stuntprobe] clip %08x (af14 %08x af18 %08x) blocks %d time %.4f range %.4f..%.4f\n", clip,
                 rd(ra + 0xaf14), rd(ra + 0xaf18), clip ? int32_t(rd(clip + 0x8c)) : 0, clip ? f32(rd(clip + 0x98)) : 0.0f,
                 f32(rd(ra + 0x464)), f32(rd(ra + 0x468)));
    if (clip) {
        for (int i = 0; i < int32_t(rd(clip + 0x8c)) && i < 8; i++) {
            const uint32_t b = blocks + 0x70u * uint32_t(i);
            std::fprintf(stderr, "[stuntprobe]   block %d start %.4f dur %.4f crop %.4f/%.4f end %.4f flags %u/%u/%u\n", i,
                         f32(rd(b + 0x40)), f32(rd(b + 0x44)), f32(rd(b + 0x4c)), f32(rd(b + 0x50)), f32(rd(b + 0x54)),
                         rd(b + 0x64), rd(b + 0x68), rd(b + 0x6c));
        }
        /* the predicates over the clip's own range and a few points inside / outside */
        const float ts[] = {-0.5f, 0.0f, 0.001f, 0.25f, 0.5f, 1.0f, 1.0166667f, 1.5f, 2.0f, 2.5f, 3.0f, NAN, INFINITY, -INFINITY};
        for (float t : ts) {
            rx_regs in = regs(); in.r[0] = clip; in.xmm[0].v[0] = t;
            const uint32_t loop = call(0x0083e170, in, 0, nullptr).r[0];
            in = regs(); in.r[0] = clip; in.xmm[1].v[0] = t;
            const float end = call(0x0083e230, in, 0, nullptr).xmm[0].v[0];
            in = regs(); in.r[0] = clip; in.xmm[1].v[0] = t;
            const float start = call(0x0083e1d0, in, 0, nullptr).xmm[0].v[0];
            std::fprintf(stderr, "[stuntprobe] 0083e170/230/1d0 t %.4f -> loop %u end %.4f start %.4f\n", t, loop, end, start);
        }
        {   /* 0083e0e0: the block at the animator's time +0x98 (the fields it writes are restored) */
            const uint32_t o7c = rd(clip + 0x7c), o80 = rd(clip + 0x80), o84 = rd(clip + 0x84), o98 = rd(clip + 0x98);
            const float ts[] = {0.0f, 0.5f, 1.5f, 2.5f, 3.0f, -1.0f};
            for (int v = 0; v < 6; v++) {
                wr(clip + 0x98, bits(ts[v]));
                rx_regs in = regs(); in.r[1] = clip;
                call(0x0083e0e0, in, 0, nullptr);
                std::fprintf(stderr, "[stuntprobe] 0083e0e0 case %d time %.4f -> 7c %.4f 80 %.4f 84 %08x\n", v,
                             f32(rd(clip + 0x98)), f32(rd(clip + 0x7c)), f32(rd(clip + 0x80)), rd(clip + 0x84));
            }
            wr(clip + 0x7c, o7c); wr(clip + 0x80, o80); wr(clip + 0x84, o84); wr(clip + 0x98, o98);
        }
    }
    /* 0063b5f0: ClampedFloat_Sub on a made-up clamped float {min, max, the listener vector embedded at +8} */
    { const uint32_t sp = rx_sp(), base = sp - 0xa0;
      rx_set_sp(base);
      const uint32_t elem = base, cont = base + 0x18, proxy = base + 0x38, cf = base + 0x50,
                     tgt = base + 0x80, delta = base + 0x90;
      const uint32_t vec = cf + 8;
      wr(elem + 0, tgt); wr(elem + 4, 1);
      wr(cont + 0x10, elem + 8);
      wr(proxy + 0, cont);
      wr(vec + 0, proxy); wr(vec + 0xc, elem); wr(vec + 0x10, elem + 8);
      wr(cf + 0, bits(0.0f)); wr(cf + 4, bits(10.0f));
      wr(tgt, bits(7.0f));
      const float ds[] = {3.0f, -5.0f, 20.0f, -30.0f};
      for (float d : ds) {
          wr(delta, bits(d));
          rx_regs in = regs(); in.r[6] = cf;
          const uint32_t a[1] = {delta};
          call(0x0063b5f0, in, 1, a);
          std::fprintf(stderr, "[stuntprobe] 0063b5f0 -> %.4f (delta %.2f) target %.4f\n", f32(rd(tgt)), d, f32(rd(tgt)));
      }
      rx_set_sp(sp); }
    /* 00620a60: advance the trick's clip time a few frames (the chain events / block end go through the ported callees) */
    if (clip) {   /* the state after the next clip switched in: af14 = the trick clip, af18 = 0 */
        const uint32_t s10 = rd(ra + 0xaf10), s14 = rd(ra + 0xaf14), s18 = rd(ra + 0xaf18), s20 = rd(ra + 0xaf20),
                       s24 = rd(ra + 0xaf24), s464 = rd(ra + 0x464), s468 = rd(ra + 0x468), s3c = rd(ra + 0xaf3c),
                       s30 = rd(ra + 0xaf30), s38 = rd(ra + 0xaf38);
        wr(ra + 0xaf10, 3); wr(ra + 0xaf14, clip); wr(ra + 0xaf18, clip);
        wr(ra + 0xaf20, bits(f32(rd(clip + 0x94)))); wr(ra + 0xaf24, bits(f32(rd(clip + 0x94))));
        wr(ra + 0xaf3c, 0xffffffffu); wr(ra + 0xaf30, 0); wr(ra + 0xaf38, 0);
        if (c158c0) wr(c158c0, 0); if (c15980) wr(c15980, 0);
        wr(ra + 0x464, 0); wr(ra + 0x468, bits(f32(rd(clip + 0x94))));
        const uint32_t a[2] = {ra, bits(1.0f / 60.0f)};
        for (int i = 0; i < 200; i++) {
            call(0x00620a60, regs(), 2, a);
            if ((i % 20) == 0 || i == 199 || (i >= 58 && i <= 64))
                { const uint32_t la[1] = {ra + 0x46c}; const uint32_t lb[1] = {ra + 0x48c};
                  const float g1 = call(0x0063b8f0, regs(), 1, la).xmm[0].v[0];
                  const float g2 = call(0x0063b8f0, regs(), 1, lb).xmm[0].v[0];
                  std::fprintf(stderr, "[stuntprobe] 00620a60 %3d -> state %u af14 %08x af18 %08x get46 %.4f get48 %.4f first %.4f/%.4f tgt %.4f\n", i,
                             rd(ra + 0xaf10), rd(ra + 0xaf14), rd(ra + 0xaf18), g1, g2, listener_value(ra + 0x46c),
                             listener_value(ra + 0x48c), mapped(0x15156b68) ? f32(rd(0x15156b68)) : 0.0f); }
        }
        /* the clip-switch body (+0x58c at 1, the next clip switched in: 0063b360 / 0063b3f0 / the event replay) */
        /* the +0xaf30 path (a wind-up clip: 0083e0e0 / 0083e1d0) and the state-4 early return */
        { const uint32_t e = rd(ra + 0x46c + 0xc); if (e && rd(e)) wr(rd(e), bits(1.999f)); }
        for (int v = 0; v < 3; v++) {
            wr(ra + 0xaf10, 1 + v % 2); wr(ra + 0xaf30, 1); wr(ra + 0xaf38, v ? 0 : 1);
            rx_regs in = regs();
            call(0x00620a60, in, 2, a);
            std::fprintf(stderr, "[stuntprobe] 00620a60 af30 case %d -> state %u 38 %u 30 %u\n", v, rd(ra + 0xaf10),
                         rd(ra + 0xaf38), rd(ra + 0xaf30));
        }
        if (c158c0) wr(c158c0, bits(1.0f));
        if (c15980) wr(c15980, bits(1.0f));
        wr(ra + 0xaf10, 4);
        { rx_regs in = regs(); call(0x00620a60, in, 2, a); }
        std::fprintf(stderr, "[stuntprobe] 00620a60 state 4 (targets %.3f/%.3f) -> state %u 28 %u 2c %u\n",
                     f32(c15980 ? rd(c15980) : 0), f32(c158c0 ? rd(c158c0) : 0), rd(ra + 0xaf10), rd(ra + 0xaf28), rd(ra + 0xaf2c));
        /* the switch body hands the pose to the bike simulation, which the reference world's rider is not linked to */
        if (c158c0 && rd(ra + 0xaf1c) != 0 && rd(rd(ra + 0xaf1c) + 0x234) != 0) {
            wr(c158c0, bits(1.0f));
            wr(ra + 0xaf10, 3); wr(ra + 0xaf14, clip); wr(ra + 0xaf18, clip);
            { rx_regs in = regs(); call(0x00620a60, in, 2, a); }
            std::fprintf(stderr, "[stuntprobe] 00620a60 switch -> state %u af14 %08x af18 %08x\n", rd(ra + 0xaf10),
                         rd(ra + 0xaf14), rd(ra + 0xaf18));
            wr(c158c0, 0);
        }

        if (c158c0) wr(c158c0, bits(0.0f));
        if (c15980) wr(c15980, bits(0.0f));
        wr(ra + 0xaf10, s10); wr(ra + 0xaf14, s14); wr(ra + 0xaf18, s18); wr(ra + 0xaf20, s20); wr(ra + 0xaf24, s24);
        wr(ra + 0xaf3c, s3c); wr(ra + 0xaf30, s30); wr(ra + 0xaf38, s38); wr(ra + 0x464, s464); wr(ra + 0x468, s468);
    }
    /* 00621f40 + its two clip starts: save the rider fields they touch, poke a trick snapshot in (+0x5b210) and the player
     * (+0x5b204), then walk the state 0 -> 1 -> 2 paths */
    {
        static const uint32_t off[] = {0xaf10, 0xaf14, 0xaf20, 0xaf24, 0xaf30, 0xaf34, 0x464, 0x468, 0x564, 0x568,
                                       0x57c, 0x580, 0x58c, 0x598, 0x5ac, 0x5b2d0, 0x5b2d4, 0x5b204};
        uint32_t save[sizeof off / sizeof off[0]];
        for (unsigned i = 0; i < sizeof off / sizeof off[0]; i++) save[i] = rd(ra + off[i]);
        uint8_t snap[0xc4];
        for (uint32_t i = 0; i < sizeof snap; i++) snap[i] = rx_rd8(ra + 0x5b210 + i);
        const uint32_t c148c = ra + 0x48c, c158c = rd(ra + 0x58c), c1598 = rd(ra + 0x598);
        /* the trick record copy the crash step reads (the original 00622050 copies 0xb4 bytes there) */
        for (uint32_t i = 0; i < 0xc4 && trick + i; i++) rx_wr8(ra + 0x5b210 + i, i < 0xb4 ? rx_rd8(trick + i) : 0);
        wr(ra + 0x5b204, P);
        /* state 0 */
        wr(ra + 0x5b2d4, 0);
        { rx_regs in = regs(); in.r[6] = ra; call(0x00621f40, in, 0, nullptr); }
        std::fprintf(stderr, "[stuntprobe] 00621f40 state 0 -> %u\n", rd(ra + 0x5b2d4));
        /* state 1 -> 2 (both layer targets at 1) */
        wr(ra + 0x5b2d4, 1);
        if (c148c) { const uint32_t e = rd(c148c + 0xc); const uint32_t t = e ? rd(e) : 0; if (t) wr(t, bits(1.0f)); }
        if (c158c) wr(c158c, bits(1.0f));
        if (c1598) wr(c1598, bits(1.0f));
        { rx_regs in = regs(); in.r[6] = ra; call(0x00621f40, in, 0, nullptr); }
        std::fprintf(stderr, "[stuntprobe] 00621f40 state 1 -> %u\n", rd(ra + 0x5b2d4));
        /* state 2 -> 00622460 (the air layer) then -> 00622160 (the crash layer) */
        for (int which = 0; which < 2; which++) {
            wr(ra + 0x5b2d4, 2); wr(ra + 0x5b2d0, which);
            wr(ra + 0xaf10, 0);
            { rx_regs in = regs(); in.r[6] = ra; call(0x00621f40, in, 0, nullptr); }
            std::fprintf(stderr, "[stuntprobe] 00621f40 state 2 (%s) -> %u, ra state %u clip %08x af20 %.4f af34 %u\n",
                         which ? "00622160" : "00622460", rd(ra + 0x5b2d4), rd(ra + 0xaf10), rd(ra + 0xaf14),
                         f32(rd(ra + 0xaf20)), rd(ra + 0xaf34));
            /* the same start directly, so the adapter has its own per-call entries */
            { const uint32_t a[2] = {ra, ra + 0x5b210};
              rx_regs in = regs(); in.r[1] = P; wr(ra + 0xaf10, 0);
              call(which ? 0x00622160 : 0x00622460, in, 2, a); }
            std::fprintf(stderr, "[stuntprobe] direct start (%s) -> state %u clip %08x\n", which ? "00622160" : "00622460",
                         rd(ra + 0xaf10), rd(ra + 0xaf14));
        }
        for (uint32_t i = 0; i < sizeof snap; i++) rx_wr8(ra + 0x5b210 + i, snap[i]);
        for (unsigned i = 0; i < sizeof off / sizeof off[0]; i++) wr(ra + off[i], save[i]);
        (void)c148c; (void)c158c; (void)c1598;
    }
}

}  // namespace

RX_PORT(0x0084ed20, StuntProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_STUNT_PROBE");
    if (!groups || !*groups) groups = "m";
    static const uint32_t at = std::getenv("RX_STUNT_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_STUNT_PROBE_FRAME"))) : 300;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    if (std::strchr(groups, 'm')) probe_match();
    if (std::strchr(groups, 'i')) probe_input();
    if (std::strchr(groups, 'g')) probe_gesture();
    if (std::strchr(groups, 'l')) probe_load();
    if (std::strchr(groups, 't')) probe_trick();
    if (std::strchr(groups, 'r')) probe_rider();
    if (std::strchr(groups, 'p')) probe_play();
    std::fprintf(stderr, "[stuntprobe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
