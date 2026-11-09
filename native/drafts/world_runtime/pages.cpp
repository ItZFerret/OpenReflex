// The in-race pages (WP-5): see pages.h, _re/notes/in-race-pages.md.
#include "pages.h"
#include "vm.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace reflex_vm {
namespace pages {
namespace {
constexpr uint32_t SCRATCH = 0xee500000, SCRATCH_SIZE = 0x10000;   // guest strings of the commands (mapped on first use)
constexpr uint32_t UI_LUA = 0xd94938;                              // the UI scripts' Lua wrapper
constexpr uint32_t UI_MANAGER = 0xa93fcc;                          // [ ] = the UI property set (UIProperty_Get*)

uint32_t rd(Machine& m, uint32_t a) { return a >= 0x10000 && m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; }
std::string cstr(Machine& m, uint32_t p, size_t max) {
    std::string o;
    for (size_t i = 0; p >= 0x10000 && i < max && m.mem.mapped(p + uint32_t(i)); i++) { char c = char(m.mem.read(p + uint32_t(i), 1)); if (!c) break; o += c; }
    return o;
}

struct Scratch {   // bump allocator over the scratch pages; ESP restored after each call
    Machine& m; uint32_t at = SCRATCH;
    explicit Scratch(Machine& mm) : m(mm) { if (!m.mem.mapped(SCRATCH)) m.mem.map(SCRATCH, nullptr, SCRATCH_SIZE); }
    uint32_t alloc(size_t n) {
        if (at + n + 16 > SCRATCH + SCRATCH_SIZE) throw std::runtime_error("pages: scratch full");
        uint32_t a = at; at += uint32_t((n + 15) & ~size_t(15)); return a;
    }
    uint32_t str(const std::string& s) { uint32_t a = alloc(s.size() + 1); m.mem.write_block(a, s.c_str(), s.size() + 1); return a; }
    uint32_t call(uint32_t fn, std::vector<uint32_t> args, int64_t ecx = -1, int64_t eax = -1) {
        uint32_t esp = m.r[4]; uint32_t r = m.call_function(fn, args.data(), int(args.size()), ecx, eax); m.r[4] = esp; return r;
    }
    // UIProperty_GetString 005dc060: EAX key, ESI size, EDI buffer, stack(ui), ret 4
    std::string ui_string(const std::string& key, uint32_t size = 0x40) {
        uint32_t buf = alloc(size); std::vector<uint8_t> z(size, 0); m.mem.write_block(buf, z.data(), size);
        uint32_t k = str(key); m.r[6] = size; m.r[7] = buf;
        bool found = (call(0x5dc060, {rd(m, UI_MANAGER)}, -1, k) & 0xff) != 0;
        return found ? cstr(m, buf, size) : std::string();
    }
    // UIProperty_GetInt 005dc0c0: EAX key, stack(ui, int* out)
    int ui_int(const std::string& key, int fallback) {
        uint32_t out = alloc(4), k = str(key); m.mem.write(out, 4, uint32_t(fallback));
        call(0x5dc0c0, {rd(m, UI_MANAGER), out}, -1, k);
        return int32_t(rd(m, out));
    }
    // as the engine runs a script asset (LuaAssetIO -> 00744df0): FUN_00744920 (ECX chunk name, stack(wrapper, buffer, size), ret 0xc:
    // luaL_loadbuffer 0077a4b0, the function stored as the global "<name>_Chunk"), then FUN_00744e70 (cdecl(wrapper, "<name>_Chunk", ""))
    void lua(const std::string& name, const std::string& src) {
        call(0x744920, {UI_LUA, str(src), uint32_t(src.size())}, str(name));
        call(0x744e70, {UI_LUA, str(name + "_Chunk"), str("")});
    }
};

// ---- the localized UI voice
std::mutex voice_lock;
std::unordered_map<const Machine*, std::deque<std::string>> voice_queue, image_queue;
void voice_hooks(Machine& mc) {
    const Machine* key = &mc;
    // UILocSound_Play 0068dd40: thiscall (ECX the manager [0xd94ad4]), stack(database, package, event path, priority, owner name), ret 0x14
    mc.natives[0x68dd40] = [key](Machine& m) -> uint32_t {
        uint32_t sp = m.r[4];
        std::string line = "play " + cstr(m, rd(m, sp + 4), 64) + "|" + cstr(m, rd(m, sp + 8), 64) + "|" + cstr(m, rd(m, sp + 12), 260) + "|" + cstr(m, rd(m, sp + 20), 64);
        { std::lock_guard<std::mutex> g(voice_lock); auto& q = voice_queue[key]; if (q.size() < 64) q.push_back(line); }
        return RUN_ORIGINAL;
    };
    // UILocSound_Stop 0068de20: cdecl(manager, owner name or NULL = every request)
    mc.natives[0x68de20] = [key](Machine& m) -> uint32_t {
        std::string line = "stop " + cstr(m, rd(m, m.r[4] + 8), 64);
        { std::lock_guard<std::mutex> g(voice_lock); auto& q = voice_queue[key]; if (q.size() < 64) q.push_back(line); }
        return RUN_ORIGINAL;
    };
}

std::string jesc(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += c; } else if (uint8_t(c) < 0x20) o += ' '; else o += c; }
    return o;
}
}  // namespace

std::string voice(Machine& m) {
    std::lock_guard<std::mutex> g(voice_lock);
    auto& q = voice_queue[&m];
    if (!m.natives.count(0x68dd40)) { q.clear(); voice_hooks(m); }
    std::string r;
    while (!q.empty()) { r += q.front(); r += '\n'; q.pop_front(); }
    return r;
}

void forget(const Machine* m) { std::lock_guard<std::mutex> g(voice_lock); voice_queue.erase(m); }

std::string world_command(Machine& m, const std::string& cmd, std::string* error) {
    size_t sp = cmd.find(' ');
    std::string verb = cmd.substr(0, sp), rest = sp == std::string::npos ? "" : cmd.substr(sp + 1);
    Scratch sc(m);
    if (verb == "ui") return sc.ui_string(rest, 0x400);
    if (verb == "ui_int") return std::to_string(sc.ui_int(rest, -1));
    if (verb == "lua") {
        // "lua <source>": the source runs as lua_ui's; its answer is what it passes to App.DebugPrint( "REFLEXANSWER:" .. answer ) (any
        // length: a UI variable keeps 31 characters). App.DebugPrint (binding 007130a0) does nothing in the shipping build; while the
        // command runs a native there reads the Lua string argument (the call's first TValue at [[esp+4]+0xc], type 4 = string, TString
        // chars at +0x10, length at +0xc) and runs the original.
        static int serial2 = 0;
        std::string name = "ReflexPagesA" + std::to_string(++serial2), answer; bool had = m.natives.count(0x7130a0) != 0, got = false;
        Native old_native; if (had) old_native = m.natives[0x7130a0];
        m.natives[0x7130a0] = [&answer, &got](Machine& mm) -> uint32_t {
            uint32_t L = rd(mm, mm.r[4] + 4), tv = L ? rd(mm, L + 0xc) : 0;
            if (tv && rd(mm, tv + 4) == 4) {
                uint32_t ts = rd(mm, tv), len = ts ? rd(mm, ts + 0xc) : 0;
                if (ts && len >= 13 && len < (1u << 20)) {
                    std::string sv(len, '\0'); for (uint32_t i = 0; i < len; i++) sv[i] = char(mm.mem.read(ts + 0x10 + i, 1));
                    if (sv.compare(0, 13, "REFLEXANSWER:") == 0) { answer = sv.substr(13); got = true; }
                }
            }
            return RUN_ORIGINAL;
        };
        try { sc.lua(name, rest); sc.lua(name + "R", name + "_Chunk = nil; " + name + "R_Chunk = nil"); }
        catch (...) { if (had) m.natives[0x7130a0] = old_native; else m.natives.erase(0x7130a0); throw; }
        if (had) m.natives[0x7130a0] = old_native; else m.natives.erase(0x7130a0);
        if (!got && error) *error = "the Lua gave no answer (an error in the chunk?)";
        return answer;
    }
    if (verb == "lua_ui") {
        size_t s2 = rest.find(' '); std::string key = rest.substr(0, s2), src = s2 == std::string::npos ? "" : rest.substr(s2 + 1);
        if (key.empty() || src.empty()) { if (error) *error = "lua_ui <key> <source>"; return ""; }
        static int serial = 0;
        std::string name = "ReflexPages" + std::to_string(++serial);
        sc.lua(name, src);
        std::string answer = sc.ui_string(key, 0x8000);
        sc.lua(name + "R", "App.RemoveUIVariable( \"" + key + "\" ); " + name + "_Chunk = nil; " + name + "R_Chunk = nil");
        return answer;
    }
    if (verb == "state") {
        // the race's page state: race state A+0x21c, rules hold A+0xff068, the allow-pause flag of the UI manager ([0xa93fcc]+0x28c:
        // App.AllowPause, read by MXUIManager_OnPauseRequest 00598e40; EventIntro / EventOutro:Present set it FALSE, the HUD pages TRUE),
        // and for a race activity (MXActivityRace vtable 00943554) its post sub-state A+0x13e640 (MXActivityRace_UpdatePost 00569a90:
        // 0 before, 1 the post scenarios play (the Event Outro page; "Post Scenarios Start"), 2 after ("Post Scenarios End" -> GotoPostEvent)).
        uint32_t G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0, vt = A ? rd(m, A) : 0;
        int post = vt == 0x943554 ? int(rd(m, A + 0x13e640)) : -1;
        char b[256];
        std::snprintf(b, sizeof b, "{\"race\":%d,\"rulesHold\":%u,\"allowPause\":%d,\"post\":%d,\"skip\":%u,\"vtable\":%u}", A ? int(rd(m, A + 0x21c)) : -1,
                      A ? rd(m, A + 0xff068) : 0u, int(rd(m, rd(m, UI_MANAGER) + 0x28c)), post, rd(m, 0xa7f748), vt);
        return b;
    }
    if (verb == "skip_scenario") {   // App.SkipToNextScenario (binding 00736d60): g_SkipToNextScenario [0xa7f748] = 1
        m.mem.write(0xa7f748, 4, 1);
        return "{}";
    }
    if (verb == "ui_images") {
        // The images the page Lua sets by code, which the live mirror's component state does not carry: UIImageComponent:SetMaterial( name,
        // b ) (binding 00723140 -> UIImage_SetMaterialByName 0070f720: EAX the name, EDI the component; the material looked up by 005c9d50
        // and set for its six colour states through vf 0x94) and MXUIStreamableImageIcon:SetIconId( id ) (binding 00730f30 ->
        // UIStreamableIcon_SetIconId 005b7750, cdecl(component, id)): the StuntPreview gesture images, medal icons, the Free Ride map, ...
        // The first call installs the hooks (they record and run the original); then "<object>|<material>" / "<object>|#<icon id>" lines
        // of the calls since the last one.
        std::lock_guard<std::mutex> g(voice_lock);
        auto& q = image_queue[&m];
        if (!m.natives.count(0x70f720)) {
            q.clear();
            const Machine* key = &m;
            m.natives[0x70f720] = [key](Machine& mm) -> uint32_t {
                char b[96]; std::snprintf(b, sizeof b, "%08x|", mm.r[7]);
                std::string line = b + cstr(mm, mm.r[0], 80);
                { std::lock_guard<std::mutex> g2(voice_lock); auto& qq = image_queue[key]; if (qq.size() < 4096) qq.push_back(line); }
                return RUN_ORIGINAL;
            };
            m.natives[0x5b7750] = [key](Machine& mm) -> uint32_t {
                char b[64]; std::snprintf(b, sizeof b, "%08x|#%d", rd(mm, mm.r[4] + 4), int32_t(rd(mm, mm.r[4] + 8)));
                { std::lock_guard<std::mutex> g2(voice_lock); auto& qq = image_queue[key]; if (qq.size() < 4096) qq.push_back(b); }
                return RUN_ORIGINAL;
            };
        }
        std::string r;
        while (!q.empty()) { r += q.front(); r += '\n'; q.pop_front(); }
        return r;
    }
    if (verb == "com_stubs") {
        // The boot's COM stand-ins (boot.cpp: memory-backed D3D resources, host code at 0xed000000+, one native per method) keep their
        // vtables in the world image (the boot heap at 0x10000000) but the world runtime has no natives there. A UI material clone
        // (XenonMaterialAsset copy 007b16f0: the textures' AddRef) - the pause page's StuntPreview gesture images, any SetMaterial of a
        // page - then runs into unmapped code. IUnknown AddRef / Release (slots 1 / 2, stdcall(this)) of every such vtable become
        // reference-count stubs (the world draws nothing; the boot's own count is not used after the image). Returns the count.
        int n = 0;
        auto host = [&](uint32_t a) { return a >= 0xed000000u && a < 0xed100000u; };
        for (uint32_t v = 0x10000000; v < 0x10020000; v += 4) {
            if (!m.mem.mapped(v) || !m.mem.mapped(v + 8)) { v = (v | 0xfff); continue; }
            uint32_t q = rd(m, v), a = rd(m, v + 4), r = rd(m, v + 8);
            if (!host(q) || a != q + 4 || r != q + 8 || (v > 0x10000000 && host(rd(m, v - 4)) && rd(m, v - 4) + 4 == q)) continue;
            for (uint32_t t : {a, r}) if (!m.natives.count(t)) { m.natives[t] = [](Machine& mm) -> uint32_t { return mm.ret(4, 1); }; n++; }
        }
        return std::to_string(n);
    }
    if (verb == "hold") {
        // "hold <v>": the activity's rules hold A+0xff068 (MXActivityRaceStub::vf25 005681e0 leaves Pre alone while it is set: no intro
        // scenario, no countdown) set to v; the answer is the value before. The port's Gate Selection page holds the race this way while
        // it shows (in the game the page comes before the race activity exists).
        uint32_t G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0;
        if (!A) { if (error) *error = "no activity"; return ""; }
        uint32_t before = rd(m, A + 0xff068); m.mem.write(A + 0xff068, 4, uint32_t(std::atoi(rest.c_str())));
        return std::to_string(before);
    }
    if (verb == "gate_camera") {
        // GateSelection's App.SetCameraToGate( gate, 25, 7, device, 0 ) (binding 00736930 -> GateSelection_SetCameraToGate 0073ed70, cdecl(gate,
        // float distance, float height, device, float seconds)): from the TO_StartingGate model's "GateStart_%02d" node it places the
        // "UI_StartGate_Camera" entity, whose matrix it hands to the entity's vf 0x40 at 0073f874 (ECX the entity, stack the matrix). The
        // world renders nothing, so that matrix (row vectors: right, up, forward, position) is the answer: 16 floats, or "" when the level
        // has no start gate camera.
        int gate = std::atoi(rest.c_str());
        std::vector<float> mtx; bool had = m.natives.count(0x73f874) != 0;
        Native old_native; if (had) old_native = m.natives[0x73f874];
        m.natives[0x73f874] = [&mtx](Machine& mm) -> uint32_t {
            uint32_t p = rd(mm, mm.r[4]); mtx.assign(16, 0.f);
            for (int i = 0; i < 16; i++) { uint32_t v = rd(mm, p + 4 * uint32_t(i)); std::memcpy(&mtx[size_t(i)], &v, 4); }
            return RUN_ORIGINAL;
        };
        float d = 25.f, h = 7.f, t = 0.f; uint32_t db, hb, tb; std::memcpy(&db, &d, 4); std::memcpy(&hb, &h, 4); std::memcpy(&tb, &t, 4);
        try { sc.call(0x73ed70, {uint32_t(gate), db, hb, 0u, tb}); }
        catch (...) { if (had) m.natives[0x73f874] = old_native; else m.natives.erase(0x73f874); throw; }
        if (had) m.natives[0x73f874] = old_native; else m.natives.erase(0x73f874);
        std::string r;
        for (size_t i = 0; i < mtx.size(); i++) { char b[32]; std::snprintf(b, sizeof b, "%s%.4f", i ? " " : "", double(mtx[i])); r += b; }
        return r;
    }
    if (verb == "gates") {
        // GateSelection:Start: MXSeriesManager::ProcessGateSelection (MXSeriesEvent_ProcessGateSelection 005a9a30, ECX the series event:
        // the riders before the local player pick, the AI with the game's generator, until GateSelectionDeviceIndex is a local device);
        // OnGateSelectionDeviceIndexChanged: App.IsSeriesGateAvailable( i ) (0073ecc0, ESI gate) for the 16 gates, the focus on the first.
        constexpr uint32_t SERIES_MANAGER = 0x9e2188;   // career.h
        uint32_t ev = rd(m, SERIES_MANAGER + 0x18c);
        std::string status = sc.ui_string("SeriesEventStatus");
        std::string j = "{\"status\":\"" + jesc(status) + "\"";
        if (!ev || status != "GateSelection") return j + ",\"device\":-1}";
        if (rest != "noprocess") sc.call(0x5a9a30, {}, ev);
        std::string dev = sc.ui_string("GateSelectionDeviceIndex");
        j += ",\"device\":" + std::string(dev.empty() ? "-1" : dev) + ",\"available\":[";
        int first = -1;
        for (int i = 0; i < 16; i++) { m.r[6] = uint32_t(i); bool ok = (sc.call(0x73ecc0, {}) & 0xff) != 0; if (ok && first < 0) first = i; j += (i ? "," : "") + std::string(ok ? "1" : "0"); }
        return j + "],\"first\":" + std::to_string(first) + "}";
    }
    if (error) *error = "unknown pages command: " + cmd;
    return "";
}
// WP-T: a line from another hook (tutorial.cpp: StopTutorialAudio), queued as the voice hooks queue theirs
void voice_note(Machine& m, const std::string& line) { std::lock_guard<std::mutex> g(voice_lock); auto& q = voice_queue[&m]; if (q.size() < 64) q.push_back(line); }
}  // namespace pages
}  // namespace reflex_vm
