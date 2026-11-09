// Validation CLI: run original frames from a full-world dump and write every page
// changed relative to the dump, in the replay harness's delta format
// (<u32 address, 4096 bytes> per page), for byte comparison.
// usage: world_validate exe crt full-entry.bin deform.bin frames.txt delta.bin
#include "runtime.h"
#include "vm.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <map>
namespace reflex_vm { const char* mnemonic_name(int i); }

int main(int argc, char** argv) {
    using namespace reflex_vm;
    if (argc != 7) { std::fprintf(stderr, "usage: world_validate exe crt full-entry.bin deform.bin frames.txt delta.bin\n"); return 2; }
    try {
        World w;
        w.load(argv[1], argv[2], argv[3], argv[4]);
        Machine& m = w.machine();
        if (const char* every = std::getenv("REFLEX_TRACE_EVERY")) { m.trace_every = std::strtoull(every, nullptr, 10); m.trace_out = std::fopen(std::getenv("REFLEX_TRACE_FILE"), "w"); }
        if (const char* from = std::getenv("REFLEX_TRACE_FROM")) { m.trace_from = std::strtoull(from, nullptr, 10); m.trace_to = std::strtoull(std::getenv("REFLEX_TRACE_TO"), nullptr, 10); if (const char* mod = std::getenv("REFLEX_TRACE_MOD")) m.trace_mod = std::strtoull(mod, nullptr, 10); m.trace_out = std::fopen(std::getenv("REFLEX_TRACE_FILE"), "w"); }
        if (const char* watch = std::getenv("REFLEX_WATCH")) { m.watch_address = uint32_t(std::strtoul(watch, nullptr, 16)); m.trace_out = std::fopen(std::getenv("REFLEX_TRACE_FILE"), "w"); }
        std::vector<uint64_t> hist(M_COUNT, 0);
        if (std::getenv("REFLEX_HISTOGRAM")) m.histogram = hist.data();
        if (const char* a = std::getenv("REFLEX_AMBIENT")) w.set_ambient_animation(std::atoi(a) != 0);
        // REFLEX_COUNT_PCS=addr,addr: count executions of these addresses (debugging).
        static std::map<uint32_t, uint64_t> pc_counts;
        if (const char* at = std::getenv("REFLEX_STACK_AT")) {   // print likely return addresses the first time `at` runs
            uint32_t pc = uint32_t(std::strtoul(at, nullptr, 16));
            m.natives[pc] = [pc](Machine& m) -> uint32_t {
                static bool done = false;
                if (!done) { done = true; std::fprintf(stderr, "stack at %08x:", pc);
                    for (uint32_t a = m.r[4], n = 0; a < m.r[4] + 0x1000 && n < 24; a += 4) { uint32_t v = uint32_t(m.mem.read(a, 4)); if (v > 0x401000 && v < 0x916000) { std::fprintf(stderr, " %08x", v); n++; } }
                    std::fprintf(stderr, "\n"); }
                return RUN_ORIGINAL; };
        }
        if (const char* list = std::getenv("REFLEX_COUNT_PCS")) {
            std::string s = list; size_t at = 0;
            while (at < s.size()) {
                size_t comma = s.find(',', at); uint32_t pc = uint32_t(std::stoul(s.substr(at, comma - at), nullptr, 16));
                pc_counts[pc] = 0;
                m.natives[pc] = [pc](Machine&) -> uint32_t { pc_counts[pc]++; return RUN_ORIGINAL; };
                if (comma == std::string::npos) break; at = comma + 1;
            }
        }
        m.mem.keep_originals(true);
        m.mem.clear_dirty();
        std::ifstream frames(argv[5]); std::string line; unsigned count = 0;
        auto t0 = std::chrono::steady_clock::now();
        while (std::getline(frames, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line); std::string dt, keys; ss >> dt >> keys;
            uint8_t state[256] = {};
            if (keys != "-") { std::istringstream ks(keys); std::string k; while (std::getline(ks, k, ',')) state[std::stoul(k, nullptr, 16) & 255] = 0x80; }
            if (dt[0] == 't') { w.engine_frame(std::stoull(dt.substr(1), nullptr, 16), state, nullptr, 0); count++; continue; }   // raw 100 ns ticks
            uint32_t bits = uint32_t(std::stoul(dt, nullptr, 16));
            if (std::getenv("REFLEX_ENGINE")) { float sec; std::memcpy(&sec, &bits, 4); w.engine_frame(uint64_t(double(sec) * 1e7 + 0.5), state, nullptr, 0); }
            else w.frame(bits, state, nullptr, 0);
            count++;
        }
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::ofstream out(argv[6], std::ios::binary); unsigned pages = 0;
        for (uint32_t page : m.mem.dirty_pages()) {
            if (page < 0x400000 || page >= 0x20000000) continue;
            const uint8_t* before = m.mem.original(page); uint8_t* now = m.mem.page(page);
            if (!before || std::memcmp(before, now, 4096) == 0) continue;
            out.write(reinterpret_cast<const char*>(&page), 4); out.write(reinterpret_cast<const char*>(now), 4096); pages++;
        }
        std::printf("{\"frames\":%u,\"steps\":%llu,\"seconds\":%.3f,\"changed_pages\":%u,\"stand_in_words\":[%u", count, (unsigned long long)m.steps, seconds, pages, w.resource());
        for (uint32_t k : w.keyboards()) std::printf(",%u", k);
        std::printf("]}\n");
        for (auto& [pc, n] : pc_counts) std::fprintf(stderr, "pc %08x executed %llu times\n", pc, (unsigned long long)n);
        if (m.histogram) {
            std::vector<std::pair<uint64_t, int>> top; for (int i = 0; i < M_COUNT; i++) if (hist[i]) top.push_back({hist[i], i});
            std::sort(top.rbegin(), top.rend());
            for (size_t i = 0; i < top.size() && i < 40; i++) std::fprintf(stderr, "%-12s %6.2f%%\n", mnemonic_name(top[i].second), 100.0 * double(top[i].first) / double(m.steps));
        }
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "world_validate: %s\n", e.what()); return 2; }
}
