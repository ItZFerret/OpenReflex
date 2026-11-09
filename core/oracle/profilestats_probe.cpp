// profilestats_probe.cpp -- diagnostics for the race records' TotalScore/BestScore source (agent "profilestats", development only).
// The records' score is (int)MXPlayer+0xcec; this prints who computes it: the stunt scorer's refresh (006cc8b0, P = player + 0xc70,
// P+0x7c is the player's +0xcec), the race Post enter's record write (00568da0), the freestyle finish (005626e0) and the online
// snapshot copy (0050cf00 -> FreestyleDriver::vf7). Only reads; every PORT returns RUN_ORIGINAL.
//   REFLEX_PORT=profstats REFLEX_PROFSTATS=1
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/rx_game.h"
}

namespace {
uint32_t rd(uint32_t a) { return rx_rd32(a); }
float rf(uint32_t a) { uint32_t u = rd(a); float f; std::memcpy(&f, &u, 4); return f; }
bool on() { static const bool b = std::getenv("REFLEX_PROFSTATS") != nullptr; return b; }
}

// 006cc8b0 (ESI = scorer P = player + 0xc70): the total +0x7c is (2 * the sum of the five ratings), clamped [0, 100]; +0x438 an override.
RX_PORT(0x006cc8b0, ProfStuntRefresh, "profstats", 0) {
    if (on()) {
        const uint32_t P = m.r[6];
        const uint32_t pl = rd(P + 0x128);
        std::printf("[profstats] 006cc8b0 scorer %08x player %08x score %.6f (%08x) sum %.6f tricks %u ratings %.4f %.4f %.4f %.4f %.4f ov %.4f pad %d actkind %d\n",
                    P, pl, rf(P + 0x7c), rd(P + 0x7c), rf(P + 0x14) + rf(P + 0x18) + rf(P + 0x1c) + rf(P + 0x20) + rf(P + 0x24),
                    rd(P + 0x418), rf(P + 0x14), rf(P + 0x18), rf(P + 0x1c), rf(P + 0x20), rf(P + 0x24), rf(P + 0x438),
                    pl ? int32_t(rd(pl + 0x1f0)) : -1, pl ? int32_t(rd(rd(pl + 0x94) + 0x23c)) : -1);
        std::fflush(stdout);
    }
    return reflex_vm::RUN_ORIGINAL;
}

// 00568da0 (ECX = activity): the Post enter writes the records rows from the players' +0xcec.
RX_PORT(0x00568da0, ProfRecordsWrite, "profstats", 0) {
    if (on()) {
        const uint32_t A = m.r[1];
        for (uint32_t i = 0; i < uint32_t(rd(A + 0x300)) && i < 16; i++) {
            const uint32_t d = rd(A + 0x2d0 + 4 * i), p = d ? rd(d + 0x74) : 0;
            if (!p) continue;
            const uint32_t P = p + 0xc70;
            std::printf("[profstats] 00568da0 order %u player %08x +cec %.6f (%08x) scorer +7c %.6f tricks %u enabled %u air %.3f ground %.3f ratings %.4f %.4f %.4f %.4f %.4f\n", i, p,
                        rf(p + 0xcec), rd(p + 0xcec), rf(P + 0x7c), rd(P + 0x418), rd(P + 0), rf(P + 0x70), rf(P + 0x6c),
                        rf(P + 0x14), rf(P + 0x18), rf(P + 0x1c), rf(P + 0x20), rf(P + 0x24));
            for (int k = 0; k < 5; k++) {
                char nm[33] = {};
                for (int c = 0; c < 32; c++) nm[c] = char(rd(P + 0x364 + 0x20 * k + (uint32_t)c));
                if (nm[0]) std::printf("[profstats]   recent %d '%s' flag %u\n", k, nm, rd(P + 0x404 + 4 * k));
            }
        }
        std::fflush(stdout);
    }
    return reflex_vm::RUN_ORIGINAL;
}

// 005626e0 (ECX = activity): the freestyle finish (not a National race) writes placement scores 100, 95, ... into +0xcec.
RX_PORT(0x005626e0, ProfFreestyleFinish, "profstats", 0) {
    if (on()) {
        const uint32_t A = m.r[1];
        std::printf("[profstats] 005626e0 freestyle finish activity %08x state %d\n", A, int32_t(rd(A + 0x21c)));
        std::fflush(stdout);
    }
    return reflex_vm::RUN_ORIGINAL;
}

// 0050cf00 (ECX = driver): the online snapshot copy (driver +0xf2c -> player +0xcec).
RX_PORT(0x0050cf00, ProfOnlineCopy, "profstats", 0) {
    if (on()) {
        const uint32_t d = m.r[1], p = d ? rd(d + 0x74) : 0;
        static int n = 0;
        if (n++ < 8)
            std::printf("[profstats] 0050cf00 online copy driver %08x player %08x +cec %.6f -> driver f2c %.6f\n", d, p,
                        p ? rf(p + 0xcec) : 0.0f, rf(d + 0xf2c));
        std::fflush(stdout);
    }
    return reflex_vm::RUN_ORIGINAL;
}

// 0054af90 MXPlayer_Update (stdcall player, dt; RET 8): the trick tracker (player + 0xb58) and scorer (player + 0xc70) over the ride.
RX_PORT(0x0054af90, ProfPlayerTick, "profstats", 0) {
    if (on()) {
        const uint32_t p = m.arg(0);
        static uint32_t n = 0;
        if ((n++ % 300) == 0 && p) {
            const uint32_t K = p + 0xb58, P = p + 0xc70;
            std::printf("[profstats] 0054af90 n %u player %08x gate %u enabled %u refreshed %u tricks %u score %.6f tracker %u/%u/%08x/%u/%u/%u vehclass %u act %08x state %d\n",
                        n, p, rd(0x00d5ab38), rd(P), rd(P + 0x320), rd(P + 0x418), rf(P + 0x7c), rd(K), rd(K + 8),
                        rd(K + 0x1c), rd(K + 0x84), rd(K + 0x88), rd(K + 0x90), rd(p + 0x1ac) ? rd(rd(p + 0x1ac) + 0x194) : 0,
                        rd(p + 0x94), rd(p + 0x94) ? int32_t(rd(rd(p + 0x94) + 0x21c)) : -1);
            std::fflush(stdout);
        }
    }
    return reflex_vm::RUN_ORIGINAL;
}

// 006cbdb0 "StuntEnds" (ECX = scorer, stack player, trick; RET 0xc): the handler that counts a trick (+0x418 + 1).
RX_PORT(0x006cbdb0, ProfStuntEnds, "profstats", 0) {
    if (on()) {
        const uint32_t P = m.r[1], player = m.arg(0), trick = m.arg(1);
        char nm[33] = {};
        if (trick) for (int k = 0; k < 32; k++) nm[k] = char(rd(trick + (uint32_t)k));
        std::printf("[profstats] 006cbdb0 trick '%s' count %u -> %u air %.3f ground %.3f player %08x scorer %08x\n",
                    nm, rd(P + 0x418), rd(P + 0x418) + 1, rf(P + 0x70), rf(P + 0x6c), player, P);
        std::fflush(stdout);
    }
    return reflex_vm::RUN_ORIGINAL;
}
