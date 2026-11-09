// reflex_client: drives libreflexworld through its C API (reflex_world.h) exactly as the Unity client does, from a script, so the
// client's boots (reflex_world_build, reflex_profile_open), profile commands and between-frame world commands can be checked
// (ports none vs all: answers, save files and memory hashes) and covered (REFLEX_CLIENT_COVERAGE=<prefix>, boot.h).
//   reflex_client <game_dir> <crt_dir> <save_dir> <script>
// Script lines ('#' comments; every answer is printed as "= <fnv64> <length> <first 160 bytes>"):
//   boot_ports <filter> <mode>              reflex_world_boot_ports (0 original, 1 natives, 2 verify)
//   build <prefix> <type,typeid,location,locid,track,class,durtype,dur,activity> [riders ai]
//   testdrive <prefix> [vehicle_owner vehicle_id]   the garage's TEST DRIVE (REFLEX_TESTDRIVE_SET), booted from <save_dir>/remote
//   series <prefix> <spec9> <series_id> [series_vehicle_type]   a MOTOCAREER series event (REFLEX_CAREER_SET)
//   profile_open | profile <command> | profile_close
//   world_open <prefix> | world_port <filter> <mode> | frames <n> [drive seed] | world_close
//   world <api> [command]   api: career persist remap garage tutorial pages ui_activate ui_event career_finish ui_frame ui_requests
//                           ui_sounds ui_voice mode mode_objects ui_setting(<name> <value> <set>) race memhash
//   keys <frames> <hex scan>...   frames with these DirectInput keys held;  peek <hex> [<hex off>...]   a pointer chain's values
//   file_hash <path>        FNV-1a 64 of a file (a save written by the game)
//   port_report             reflex_world_port_report (the ports called so far)
#include "reflex_world.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
uint64_t fnv(const void* p, size_t n) { uint64_t h = 1469598103934665603ull; auto* b = static_cast<const uint8_t*>(p); for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 1099511628211ull; return h; }
void answer(const std::string& r) {
    static int serial = 0; serial++;
    if (const char* d = std::getenv("REFLEX_CLIENT_ANSWERS")) {   // every answer in full: <dir>/<serial>.txt
        if (FILE* f = std::fopen((std::string(d) + "/" + std::to_string(serial) + ".txt").c_str(), "wb")) { std::fwrite(r.data(), 1, r.size(), f); std::fclose(f); } }
    std::string head = r.substr(0, 160); for (char& c : head) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    std::printf("= %016llx %zu %s\n", (unsigned long long)fnv(r.data(), r.size()), r.size(), head.c_str()); std::fflush(stdout);
}
void fail(const std::string& what) { std::printf("! %s\n", what.c_str()); std::fflush(stdout); }
template <class F> std::string text_call(F f) {   // buffer-returning calls: retry with -n + 16 as the client does
    std::vector<char> b(1 << 16);
    int n = f(b.data(), int32_t(b.size()));
    if (n < -1) { b.assign(size_t(-n + 16), 0); n = f(b.data(), int32_t(b.size())); }
    if (n < 0) return std::string("\x01") + std::to_string(n);
    return std::string(b.data(), std::strlen(b.data()));
}
bool spec9(const std::string& s, ReflexEventSpec& e) {
    std::vector<std::string> f; std::string cur;
    for (size_t i = 0; i <= s.size(); i++) { if (i == s.size() || s[i] == ',') { f.push_back(cur); cur.clear(); } else cur += s[i]; }
    if (f.size() != 9) return false;
    std::memset(&e, 0, sizeof e);
    std::snprintf(e.type, sizeof e.type, "%s", f[0].c_str()); std::snprintf(e.location, sizeof e.location, "%s", f[2].c_str());
    std::snprintf(e.duration_type, sizeof e.duration_type, "%s", f[6].c_str());
    e.type_id = std::atoi(f[1].c_str()); e.location_id = std::atoi(f[3].c_str()); e.track_id = std::atoi(f[4].c_str());
    e.vehicle_class = std::atoi(f[5].c_str()); e.duration = std::atoi(f[7].c_str()); e.activity = std::atoi(f[8].c_str());
    e.riders = 1; e.ai = 0;
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 5) { std::fprintf(stderr, "usage: %s game_dir crt_dir save_dir script\n", argv[0]); return 2; }
    std::string game = argv[1], crt = argv[2], save = argv[3];
    std::ifstream in(argv[4]); if (!in) { std::fprintf(stderr, "no script %s\n", argv[4]); return 2; }
    void* profile = nullptr; void* world = nullptr; uint32_t frames_run = 0; int bad = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::printf("> %s\n", line.c_str()); std::fflush(stdout);
        std::istringstream ls(line); std::string verb; ls >> verb;
        std::string rest; std::getline(ls, rest); if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
        if (verb == "boot_ports") { std::istringstream a(rest); std::string f; int mode = 1; a >> f >> mode; reflex_world_boot_ports(f.c_str(), mode); continue; }
        if (verb == "build" || verb == "testdrive" || verb == "series") {
            std::istringstream a(rest); std::string prefix, sp; a >> prefix;
            ReflexEventSpec e;
            if (verb == "testdrive") {
                spec9("National,10,NAT_Concord,50,0,3,Laps,2,0", e);
                e.garage = REFLEX_GARAGE_SET; e.rider_owner = -1; e.vehicle_owner = -1; e.vehicle_type = -1; e.test_drive = REFLEX_TESTDRIVE_SET;
                e.modes = 0; e.career = 0; a >> e.vehicle_owner >> e.vehicle_id;
            } else {
                a >> sp; if (!spec9(sp, e)) { fail("bad spec " + sp); bad = 1; continue; }
                if (verb == "build") { int r = 0, ai = 0; if (a >> r >> ai) { e.riders = r; e.ai = ai; } }
                else { e.garage = REFLEX_GARAGE_SET; e.rider_owner = e.vehicle_owner = e.vehicle_type = -1; e.career = REFLEX_CAREER_SET;
                       e.series_vehicle_type = -1; a >> e.series_id >> e.series_vehicle_type; }
            }
            int r = reflex_world_build(game.c_str(), crt.c_str(), save.c_str(), &e, prefix.c_str());
            if (r) { fail(std::string("build: ") + reflex_world_build_error()); bad = 1; } else answer("built " + prefix);
            continue;
        }
        if (verb == "profile_open") {
            profile = reflex_profile_open(game.c_str(), crt.c_str(), save.c_str());
            if (!profile) { fail(std::string("profile_open: ") + reflex_profile_error(nullptr)); bad = 1; } else answer("open");
            continue;
        }
        if (verb == "profile") {
            if (!profile) { fail("no profile"); bad = 1; continue; }
            std::string r = text_call([&](char* o, int32_t n) { return reflex_profile_command(profile, rest.c_str(), o, n); });
            if (!r.empty() && r[0] == '\x01') { fail("profile " + rest + ": " + reflex_profile_error(profile)); bad = 1; } else answer(r);
            continue;
        }
        if (verb == "profile_close") { if (profile) reflex_profile_close(profile); profile = nullptr; continue; }
        if (verb == "world_open") {
            world = reflex_world_create((game + "/MXReflex.exe").c_str(), (crt + "/msvcr90.dll").c_str(), (rest + "-world.bin").c_str(), (rest + "-deform.bin").c_str());
            if (!world) { fail(std::string("world_open: ") + reflex_world_error(nullptr)); bad = 1; } else answer("open");
            frames_run = 0; continue;
        }
        if (verb == "world_close") { if (world) reflex_world_destroy(world); world = nullptr; continue; }
        if (verb == "port_report") { answer(text_call([&](char* o, int32_t n) { int k = reflex_world_port_report(o, n); return k >= n ? -k - 1 : k; })); continue; }
        if (verb == "file_hash") {
            std::ifstream f(rest, std::ios::binary); if (!f) { fail("no file " + rest); bad = 1; continue; }
            std::string d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            std::printf("= %016llx %zu %s\n", (unsigned long long)fnv(d.data(), d.size()), d.size(), rest.c_str()); continue;
        }
        if (!world) { fail("no world"); bad = 1; continue; }
        if (verb == "world_port") { std::istringstream a(rest); std::string f; int mode = 1; a >> f >> mode; answer(std::to_string(reflex_world_port(world, f.c_str(), mode))); continue; }
        if (verb == "frames") {   // the REFLEX_DRIVE rider of the CLI's world test (boot_main.cpp) after the gate drop
            std::istringstream a(rest); uint32_t n = 0, drive = 0; a >> n >> drive;
            static uint32_t seed = 0, until = 0; static uint8_t held[8] = {}; static const uint8_t pool[8] = {0x11, 0x1f, 0x1e, 0x20, 0xc8, 0xd0, 0xcb, 0xcd};
            if (drive && !seed) seed = drive * 2654435761u + 1;
            uint8_t keys[256] = {};
            for (uint32_t i = 0; i < n; i++, frames_run++) {
                ReflexRaceState rs{}; int race = reflex_world_race(world, &rs) == 0 && rs.valid ? rs.state : -1;
                keys[0x11] = race >= 2 ? 0x80 : 0;
                if (drive) {
                    bool go = race >= 2 || (race < 0 && frames_run >= 120);
                    if (go && frames_run >= until) {
                        auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
                        until = frames_run + 20 + rnd() % 70;
                        held[0] = rnd() % 10 < 8; held[1] = !held[0] && rnd() % 3 == 0;
                        uint32_t st = rnd() % 3; held[2] = st == 1; held[3] = st == 2;
                        uint32_t ln = rnd() % 5; held[4] = ln == 1; held[5] = ln == 2; held[6] = ln == 3; held[7] = ln == 4;
                    }
                    for (int k = 0; k < 8; k++) keys[pool[k]] = go && held[k] ? 0x80 : 0;
                }
                if (reflex_world_frame(world, 1.0f / 60.0f, keys, nullptr, 0)) { fail(std::string("frame: ") + reflex_world_error(world)); bad = 1; break; }
            }
            ReflexRaceState rs{}; reflex_world_race(world, &rs);
            answer("frames " + std::to_string(frames_run) + " race " + std::to_string(rs.state) + " laps " + std::to_string(rs.laps_completed));
            continue;
        }
        if (verb == "keys") {   // keys <frames> <hex scan code>...: the keys held down for <frames> frames (then released)
            std::istringstream a(rest); uint32_t n = 0; a >> n; uint8_t keys[256] = {}; std::string k;
            while (a >> k) keys[std::strtoul(k.c_str(), nullptr, 16) & 0xff] = 0x80;
            for (uint32_t i = 0; i < n; i++, frames_run++) if (reflex_world_frame(world, 1.0f / 60.0f, keys, nullptr, 0)) { fail(std::string("frame: ") + reflex_world_error(world)); bad = 1; break; }
            answer("frames " + std::to_string(frames_run)); continue;
        }
        if (verb == "peek") {   // peek <hex> [<hex offset>...]: [a], then [[a] + off] ... (a pointer chain), every value printed
            std::istringstream a(rest); std::string x; uint32_t v = 0; bool first = true; std::string r;
            while (a >> x) { uint32_t o = uint32_t(std::strtoul(x.c_str(), nullptr, 16)), at = first ? o : v + o; first = false;
                uint8_t b[4] = {}; v = reflex_world_read(world, at, b, 4) == 0 ? uint32_t(b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24) : 0;
                char t[24]; std::snprintf(t, sizeof t, "%s%08x", r.empty() ? "" : " ", v); r += t; }
            answer(r); continue;
        }
        if (verb != "world") { fail("unknown verb " + verb); bad = 1; continue; }
        std::istringstream a(rest); std::string api; a >> api; std::string cmd; std::getline(a, cmd); if (!cmd.empty() && cmd[0] == ' ') cmd.erase(0, 1);
        std::string r;
        const std::string remote = save + "/remote";
        if (api == "career") r = text_call([&](char* o, int32_t n) { return reflex_world_career(world, cmd.c_str(), o, n); });
        else if (api == "persist") r = text_call([&](char* o, int32_t n) { return reflex_world_persist(world, save.c_str(), cmd.c_str(), o, n); });
        else if (api == "remap") r = text_call([&](char* o, int32_t n) { return reflex_world_remap(world, cmd.c_str(), o, n); });
        else if (api == "garage") r = text_call([&](char* o, int32_t n) { return reflex_world_garage(world, cmd.c_str(), o, n); });
        else if (api == "tutorial") r = text_call([&](char* o, int32_t n) { return reflex_world_tutorial(world, cmd.c_str(), o, n); });
        else if (api == "pages") r = text_call([&](char* o, int32_t n) { return reflex_world_pages(world, cmd.c_str(), o, n); });
        else if (api == "career_finish") r = text_call([&](char* o, int32_t n) { return reflex_world_career_finish(world, remote.c_str(), o, n); });
        else if (api == "ui_activate") r = text_call([&](char* o, int32_t n) { int k = reflex_world_ui_activate(world, cmd.empty() ? nullptr : cmd.c_str(), o, n); return k >= n ? -k - 1 : k; });
        else if (api == "ui_requests") r = text_call([&](char* o, int32_t n) { return reflex_world_ui_requests(world, o, n); });
        else if (api == "ui_sounds") r = text_call([&](char* o, int32_t n) { return reflex_world_ui_sounds(world, o, n); });
        else if (api == "ui_voice") r = text_call([&](char* o, int32_t n) { return reflex_world_ui_voice(world, o, n); });
        else if (api == "ui_event") r = std::to_string(reflex_world_ui_event(world, cmd.c_str()));
        else if (api == "ui_setting") {
            std::istringstream b(cmd); std::string name; int value = 0, set = 0; b >> name >> value >> set; int32_t before = 0;
            int k = reflex_world_ui_setting(world, name.c_str(), value, -1, set, &before); r = std::to_string(k) + " " + std::to_string(before);
        } else if (api == "ui_frame") {
            std::vector<ReflexUiLayer> L(64); std::vector<ReflexUiNode> N(4096); std::vector<ReflexUiAnim> A(512); std::vector<char> T(1 << 18); int32_t c[4] = {};
            int k = reflex_world_ui_frame(world, L.data(), 64, N.data(), 4096, A.data(), 512, T.data(), uint32_t(T.size()), c);
            r = std::to_string(k) + " layers " + std::to_string(c[0]) + " nodes " + std::to_string(c[1]) + " anims " + std::to_string(c[2]) + " text " + std::to_string(c[3]);
            uint64_t h = fnv(L.data(), sizeof(ReflexUiLayer) * size_t(c[0])) ^ fnv(N.data(), sizeof(ReflexUiNode) * size_t(c[1])) ^ fnv(A.data(), sizeof(ReflexUiAnim) * size_t(c[2])) ^ fnv(T.data(), size_t(c[3]));
            char b[32]; std::snprintf(b, sizeof b, " h %016llx", (unsigned long long)h); r += b;
        } else if (api == "mode") { std::vector<uint8_t> b(4096, 0); int k = reflex_world_mode(world, reinterpret_cast<ReflexModeState*>(b.data())); r = std::to_string(k) + " " + std::to_string(fnv(b.data(), sizeof(ReflexModeState))); }
        else if (api == "mode_objects") { std::vector<ReflexModeObject> v(64); int k = reflex_world_mode_objects(world, v.data(), 64); r = std::to_string(k) + " " + std::to_string(k > 0 ? fnv(v.data(), sizeof(ReflexModeObject) * size_t(k)) : 0); }
        else if (api == "race") { ReflexRaceState rs{}; int k = reflex_world_race(world, &rs); r = std::to_string(k) + " state " + std::to_string(rs.state) + " place " + std::to_string(rs.place) + " laps " + std::to_string(rs.laps_completed); }
        else if (api == "memhash") {   // as the CLI's REFLEX_MEMHASH: every resident page but the world runtime's stacks
            uint64_t h = 0; uint32_t pages = 0; reflex_world_memory_hash(world, &h, &pages);
            char b[64]; std::snprintf(b, sizeof b, "memory hash %016llx over %u pages", (unsigned long long)h, pages); r = b;
        } else { fail("unknown world api " + api); bad = 1; continue; }
        if (!r.empty() && r[0] == '\x01') { fail(api + " " + cmd + ": " + r.substr(1) + " " + reflex_world_error(world)); bad = 1; } else answer(r);
    }
    if (world) reflex_world_destroy(world);
    if (profile) reflex_profile_close(profile);
    return bad;
}
