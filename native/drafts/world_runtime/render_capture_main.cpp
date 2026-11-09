// rfx_capture: boots an event of MXReflex.exe in the interpreter (as reflex_boot) with the render capture installed, then
// logs and software-renders chosen frames of the original renderer (render_capture.h).
//   rfx_capture <game_dir> <crt_dir> <save_dir> <out_dir>
// env: REFLEX_EVENT_SPEC (as reflex_boot; default NAT_Concord), REFLEX_EVENT=alpine, REFLEX_RASTER (0 state only, 1 render-to-texture
// draws only, 2 all; default 1 before the capture, 2 during it), REFLEX_CAPTURE_AFTER (frames after the world runs, default 30),
// REFLEX_CAPTURE_FRAMES (default 1), REFLEX_KEYS_W (hold throttle after the gate drop), REFLEX_BOOT_JIT, REFLEX_GARAGE / REFLEX_RIDERS / REFLEX_AI (as reflex_boot), REFLEX_OBJ_DUMP (render_capture.cpp dump_obj).
#include "boot.h"
#include "render_capture.h"
#include "vm.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    if (argc < 5) { std::fprintf(stderr, "usage: %s game_dir crt_dir save_dir out_dir\n", argv[0]); return 2; }
    reflex_vm::BootConfig c; c.game_dir = argv[1]; c.crt_dir = argv[2]; c.save_dir = argv[3]; c.log_path = std::string(argv[4]) + "/boot.log";
    c.jit = std::getenv("REFLEX_BOOT_JIT") != nullptr;
    reflex_vm::Boot boot(c);
    reflex_vm::RenderCapture cap(boot, argv[4]);
    int pre = std::getenv("REFLEX_RASTER_PRE") ? std::atoi(std::getenv("REFLEX_RASTER_PRE")) : 1;
    int during = std::getenv("REFLEX_RASTER") ? std::atoi(std::getenv("REFLEX_RASTER")) : 2;
    cap.set_raster(pre);
    auto t0 = std::chrono::steady_clock::now();
    auto secs = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); };
    try {
        boot.start();
        cap.after_start();
        std::printf("started %.1fs\n", secs());
        reflex_vm::Boot::EventSpec e;
        if (const char* sp = std::getenv("REFLEX_EVENT_SPEC")) {
            char t[32] = {}, l[64] = {}, dt[16] = {};
            if (std::sscanf(sp, "%31[^,],%d,%63[^,],%d,%d,%d,%15[^,],%d,%d", t, &e.type_id, l, &e.location_id, &e.track_id, &e.vehicle_class, dt, &e.duration, &e.activity) == 9) { e.type = t; e.location = l; e.duration_type = dt; }
            else if (std::sscanf(sp, "%31[^,],%d,%63[^,],%d,%d,%d,,%d,%d", t, &e.type_id, l, &e.location_id, &e.track_id, &e.vehicle_class, &e.duration, &e.activity) == 8) { e.type = t; e.location = l; e.duration_type = ""; }
        }
        if (const char* g = std::getenv("REFLEX_GARAGE"))   // Vehicle Select (as reflex_boot): rider_owner,rider_id,gear_set,vehicle_owner,vehicle_id,vehicle_type
            std::sscanf(g, "%d,%d,%d,%d,%d,%d", &e.rider_owner, &e.rider_id, &e.rider_gear_set, &e.vehicle_owner, &e.vehicle_id, &e.vehicle_type);
        if (const char* rr = std::getenv("REFLEX_RIDERS")) { e.riders = std::atoi(rr); e.ai = std::getenv("REFLEX_AI") ? std::atoi(std::getenv("REFLEX_AI")) : 1; }
        if (std::getenv("REFLEX_EVENT") && std::string(std::getenv("REFLEX_EVENT")) == "alpine") { e.type = "FreeRide"; e.type_id = 9; e.location = "FR_Alpine"; e.location_id = 45; e.vehicle_class = -1; e.activity = 8; e.duration_type = ""; }
        std::string log;
        bool ok = boot.start_event(e, 20000, &log);
        std::printf("%s%s %.1fs\n", log.c_str(), ok ? "EVENT STARTED" : "EVENT FAILED", secs());
        if (!ok) return 1;
        int after = std::getenv("REFLEX_CAPTURE_AFTER") ? std::atoi(std::getenv("REFLEX_CAPTURE_AFTER")) : 30;
        uint8_t keys[256] = {};
        for (int i = 0; i < after; i++) {
            if (std::getenv("REFLEX_KEYS_W")) { uint32_t G = boot.read32(0xa946b8), A = G ? boot.read32(G + 0x7460 + 0x94) : 0; keys[0x11] = A && boot.read32(A + 0x21c) >= 2 ? 0x80 : 0; boot.set_keys(keys); }
            boot.run_frames(1);
        }
        std::printf("pre-capture frames done %.1fs\n", secs()); std::fflush(stdout);
        if (std::getenv("REFLEX_DUMP_MODELS")) {   // the loaded model assets' bindings as the game bound them (object-shading.md: the material rule)
            // ModelAssetHandle (vftable 0x972898): name inline at +4, model data at +0x44; data +0x118 LOD count, +0x130 LODs (0x1c each),
            // LOD +0x10 / +0x14 the bindings (16 bytes), binding +0xc the MaterialAssetHandle (vftable 0x95b4f4, name inline at +4)
            reflex_vm::Machine& mm = boot.machine();
            auto rd = [&](uint32_t a) -> uint32_t { return mm.mem.mapped(a) ? uint32_t(mm.mem.read(a, 4)) : 0u; };
            auto str = [&](uint32_t a) { std::string t; for (uint32_t i = 0; i < 128 && mm.mem.mapped(a + i); i++) { char c = char(mm.mem.read(a + i, 1)); if (!c) break; t += c; } return t; };
            std::string path = std::string(argv[4]) + "/models.txt"; FILE* f = std::fopen(path.c_str(), "w");
            int found = 0;
            for (uint32_t pg = 0x10; pg < 0x100000 && f; pg++) {
                const uint8_t* p = mm.mem.peek(pg << 12); if (!p) continue;
                for (uint32_t o = 0; o < 4096; o += 4) {
                    uint32_t v; std::memcpy(&v, p + o, 4); if (v != 0x972898) continue;
                    uint32_t h = (pg << 12) + o, data = rd(h + 0x44); if (!data) continue;
                    uint32_t lods = rd(data + 0x118), lb = rd(data + 0x130), le = rd(data + 0x134);
                    if (lods > 16 || le < lb || (le - lb) / 0x1c > 16) continue;
                    std::fprintf(f, "model %08x %s lods %u\n", h, str(h + 4).c_str(), lods); found++;
                    for (uint32_t l = 0; l < (le - lb) / 0x1c; l++) {
                        uint32_t bb = rd(lb + l * 0x1c + 0x10), be = rd(lb + l * 0x1c + 0x14);
                        for (uint32_t k = 0; be >= bb && k < (be - bb) / 16 && k < 256; k++) {
                            uint32_t mh = rd(bb + k * 16 + 0xc);
                            std::fprintf(f, "  L%u b%u %08x %08x %s\n", l, k, rd(bb + k * 16), mh, mh && rd(mh) == 0x95b4f4 ? str(mh + 4).c_str() : "?");
                        }
                    }
                }
            }
            if (f) std::fclose(f);
            std::printf("models dumped: %d\n", found); std::fflush(stdout);
        }
        if (std::getenv("REFLEX_FORCE_SOFT")) boot.machine().mem.write(0xc11d04, 4, 1);   // particles.cpp SOFT_PARTICLES: the NoLightPS path
        cap.set_raster(during);
        int frames = std::getenv("REFLEX_CAPTURE_FRAMES") ? std::atoi(std::getenv("REFLEX_CAPTURE_FRAMES")) : 1;
        cap.capture_frames(frames);
        for (int i = 0; i < 20 * frames && cap.capturing(); i++) boot.run_frames(1);
        std::printf("captured %.1fs\n%s", secs(), cap.summary().c_str());
    } catch (const std::exception& ex) {
        std::printf("STOPPED %.1fs frame %llu: %s\n%s\n%s", secs(), (unsigned long long)boot.frames(), ex.what(), boot.status().c_str(), cap.summary().c_str());
        return 1;
    }
    return 0;
}
