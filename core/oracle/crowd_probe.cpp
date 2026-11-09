// crowd_probe.cpp -- construction check of the BillboardCrowdEntity (core/game/build_scene_crowd.c) against the ORIGINAL's live objects.
// The vf5 of the class reads the RSData through the core's fmt readers (rs_*), which a baton boot cannot Verify per call; this probe checks
// it on the live objects instead: in a baton boot of a level that holds crowd sections it finds every live
// BillboardCrowdEntity the original built (vtable 0x00960654 / 0x009606cc), opens the same level through the core's fmt readers
// (RX_CROWD_ROOT = the install root, RX_CROWD_LEVEL = "SX_Kingston"), builds a fresh entity of the same BillboardCrowdRSData id with the core's
// factory + vf5 (rx_scene_entity_from_rsdata) in the same guest memory, and compares the two objects word by word, the flash records and the
// state the constructor left in the shared render event. Differences are printed per entity offset (count over all entities + one example),
// the run ends after the probe (the fresh entities stay in the image).
//   builds/w-npcs/cprobe.sh <level> <event spec>: REFLEX_PORT=rx_CrowdProbeHook REFLEX_EVENT=1 REFLEX_EVENT_SPEC=.. RX_CROWD_PROBE=1 RX_CROWD_ROOT=<root> RX_CROWD_LEVEL=<level>
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
extern "C" {
#include "../src/fmt_db.h"
#include "../src/fmt_scene.h"
#include "../game/build_scene.h"
}
using namespace reflex_port;

RX_PORT(0x0084ed20, CrowdProbeHook, "thunk", 0) {
    if (!std::getenv("RX_CROWD_PROBE")) return reflex_vm::RUN_ORIGINAL;
    // the probe runs inside a baton BOOT (REFLEX_EVENT=1: the boot's Direct3D stand-ins answer the shader AddRef of vf5; a world image has no
    // device): at the first Engine::Update (0084ed20) after the crowd count [0x00a942f8] has been non-zero for RX_CROWD_FRAME (default 30) updates
    static const uint32_t at = std::getenv("RX_CROWD_FRAME") ? uint32_t(std::atoi(std::getenv("RX_CROWD_FRAME"))) : 30;
    static uint32_t frame = 0;
    if (rx_rd32(0x00a942f8) == 0) return reflex_vm::RUN_ORIGINAL;
    if (++frame != at) return reflex_vm::RUN_ORIGINAL;
    auto rd = [&](uint32_t a) { return rx_rd32(a); };
    auto mapped = [&](uint32_t a) { return m.mem.mapped(a) && m.mem.mapped(a + 3); };
    std::vector<uint32_t> live;
    for (uint32_t pg = 0; pg < (1u << 20); pg++) if (m.mem.mapped(pg << 12))
        for (uint32_t o = 0; o < 0x1000; o += 16) { const uint32_t a = (pg << 12) + o; if (rd(a) == 0x00960654 && mapped(a + 0x90) && rd(a + 0x90) == 0x009606cc) live.push_back(a); }
    std::fprintf(stderr, "[crowd] %zu live BillboardCrowd entities; shared event %08x, count %u\n", live.size(), rd(0x00a942f4), rd(0x00a942f8));
    const char* root = std::getenv("RX_CROWD_ROOT"); const char* level = std::getenv("RX_CROWD_LEVEL");
    fmt_install* in = root ? fmt_install_open(root) : nullptr;
    fmt_level* lv = in && level ? fmt_level_open(in, level) : nullptr;
    fmt_scene* sc = lv ? fmt_level_scene(lv) : nullptr;
    if (!sc) { std::fprintf(stderr, "[crowd] cannot open the level (%s)\n", root ? root : "no RX_CROWD_ROOT"); std::exit(2); }
    std::map<uint32_t, const fmt_entity*> byid;
    for (int s = 0; s < fmt_scene_subscene_count(sc); s++) {
        int n = 0; const fmt_entity* es = fmt_scene_entities(sc, s, &n);
        for (int i = 0; i < n; i++) if (es[i].ns && std::strstr(es[i].ns, "BillboardCrowdRSData")) byid[es[i].id] = &es[i];
    }
    std::map<uint32_t, std::pair<unsigned, std::string>> diffs;   // offset -> (entities differing, an example)
    unsigned compared = 0, flash_bad = 0, event_before = rd(0x00a942f4);
    const uint32_t count_before = rd(0x00a942f8);
    for (uint32_t E : live) {
        auto it = byid.find(rd(E + 0x14));
        if (it == byid.end()) { std::fprintf(stderr, "[crowd] entity %08x id %u has no BillboardCrowdRSData in the level\n", E, rd(E + 0x14)); continue; }
        const uint32_t N = rx_scene_entity_from_rsdata(rd(E + 0x10), it->second, 0);
        if (!N) { std::fprintf(stderr, "[crowd] the core built nothing for id %u\n", rd(E + 0x14)); continue; }
        compared++;
        for (uint32_t o = 0; o < 0x190; o += 4) {
            if (rd(E + o) == rd(N + o)) continue;
            char b[160]; std::snprintf(b, sizeof b, "id %u: orig %08x core %08x", rd(E + 0x14), rd(E + o), rd(N + o));
            auto& d = diffs[o]; if (!d.first++) d.second = b;
        }
        // the flash records (the vector at +0x170: records of 16 bytes between +0xc and +0x10)
        const uint32_t ob = rd(E + 0x17c), oe = rd(E + 0x180), nb = rd(N + 0x17c), ne = rd(N + 0x180);
        if (oe - ob != ne - nb) { flash_bad++; std::fprintf(stderr, "[crowd] id %u: flash vector %u / %u bytes\n", rd(E + 0x14), oe - ob, ne - nb); continue; }
        for (uint32_t o = 0; o < oe - ob; o += 4) if (rd(ob + o) != rd(nb + o)) { flash_bad++; std::fprintf(stderr, "[crowd] id %u: flash word +%x orig %08x core %08x\n", rd(E + 0x14), o, rd(ob + o), rd(nb + o)); }
    }
    std::fprintf(stderr, "[crowd] compared %u entities (the core's count %u -> %u, event %08x -> %08x), flash record differences %u\n", compared, count_before, rd(0x00a942f8), event_before, rd(0x00a942f4), flash_bad);
    for (auto& d : diffs) std::fprintf(stderr, "[crowd] +%03x differs in %u entities, e.g. %s\n", d.first, d.second.first, d.second.second.c_str());
    std::fprintf(stderr, "[crowd] done\n");
    std::exit(0);
}
