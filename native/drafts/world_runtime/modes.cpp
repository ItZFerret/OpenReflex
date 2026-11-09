// Game modes beyond the races (modes.h; _re/notes/modes.md, _re/names/83_modes.tsv).
#include "modes.h"
#include "vm.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

namespace reflex_vm {
namespace {
uint32_t rd(Memory& m, uint32_t a) { return a >= 0x10000 && m.mapped(a) ? uint32_t(m.read(a, 4)) : 0u; }
float rf(Memory& m, uint32_t a) { uint32_t v = rd(m, a); float f; std::memcpy(&f, &v, 4); return f; }
// UI class vtables live in .rdata 0x94d000..0x958000 (MXUIManager 0x94db7c, UIComponent 0x950114 ... MXUIOutOfBoundsComponent 0x957374)
bool ui_vtable(uint32_t vt) { return vt >= 0x94d000 && vt < 0x958000; }
// a UITextComponent (or subclass): its vf16 / vf36 are UITextComponent's (005cc5d0 / 005cc8c0, in 40 classes each)
bool text_class(Memory& m, uint32_t vt) { return rd(m, vt + 16 * 4) == 0x5cc5d0 || rd(m, vt + 36 * 4) == 0x5cc8c0; }
std::string comp_name(Memory& m, uint32_t c) { char n[48] = {}; if (c) m.read_bytes(c + 0x10, n, 47); for (char& ch : n) if (ch && (ch < 32 || ch > 126)) ch = '?'; return n; }
void utf8(std::string& o, uint32_t ch) {
    if (ch < 0x80) o += char(ch); else if (ch < 0x800) { o += char(0xc0 | (ch >> 6)); o += char(0x80 | (ch & 0x3f)); }
    else { o += char(0xe0 | (ch >> 12)); o += char(0x80 | ((ch >> 6) & 0x3f)); o += char(0x80 | (ch & 0x3f)); }
}
constexpr uint32_t UI_SCRATCH = 0xee380000;   // a page of our own for the strings passed to the game's UI functions
constexpr uint32_t ANIM_SET_STATE = 0x5c5ea0; // CompositeAnimation SetState (ECX record, stack state, target)
constexpr uint32_t PAGE_GRAPH_VT = 0x950494, PAGE_STACK_VT = 0x9516b4, PAGE_GRAPH_CONTAINER_VT = 0x95056c;

struct Mirror {
    bool active = false;
    std::vector<std::pair<uint32_t, uint32_t>> anim_log;   // (record, new state) since the last ui_frame
    std::vector<std::string> requests;                     // the UI events that leave the level, held back for the client (ui_requests)
    std::map<std::string, std::vector<uint32_t>> roots;    // ui_collect's cache
    std::map<std::string, uint64_t> scanned_at; uint64_t calls = 0;
};
std::mutex lock_;
std::map<const Machine*, Mirror> mirrors;
Mirror& mirror(const Machine& m) { return mirrors[&m]; }

uint32_t scratch_str(Memory& m, uint32_t& at, const std::string& s) {
    if (!m.mapped(UI_SCRATCH)) m.map(UI_SCRATCH, nullptr, 4096);
    uint32_t a = at; m.write_block(a, s.c_str(), s.size() + 1); at += uint32_t((s.size() + 16) & ~size_t(15)); return a;
}
// the page graphs of the UI manager's container (MXUIManager [G+0x745c] +0xd8: UIPageGraphContainer; children = UIPageGraph)
std::vector<uint32_t> page_graphs(Memory& m) {
    std::vector<uint32_t> r; uint32_t G = rd(m, 0xa946b8), ui = G ? rd(m, G + 0x745c) : 0, c = ui ? rd(m, ui + 0xd8) : 0;
    if (!c || rd(m, c) != PAGE_GRAPH_CONTAINER_VT) return r;
    for (uint32_t p = rd(m, c + 0xcc), e = rd(m, c + 0xd0); p && p < e && r.size() < 32; p += 4) { uint32_t g = rd(m, p), vt = rd(m, g); if (vt == PAGE_GRAPH_VT || vt == PAGE_STACK_VT) r.push_back(g); }
    return r;
}
// FUN_005c9950: a component's state bit on/off (EAX on, ESI component, stack bit; OnStateChanged and the state triggers run)
void set_state(Machine& mc, uint32_t comp, int bit, bool on) { mc.r[6] = comp; uint32_t b = uint32_t(bit); mc.call_function(0x5c9950, &b, 1, -1, on ? 1 : 0); }

// The UI events with which the pages leave the level (PostEventMenu:Continue / Quit, the quit and restart popups' results, the
// career / challenge exits): their pages unload the world and load the front end through the database loader, which a world image
// does not service. App.SendUIEvent (00740ed0, EAX name) holds them back; the client does what they mean (ui_requests).
bool leaves_level(const std::string& e) {
    static const char* names[] = {"GotoFrontEndMainMenuPage", "GotoQuittingPage", "GotoCareerRaceAndRidePage", "GotoFrontEndPageGraph",
                                  "GotoVehicleLoadingPage", "GotoGarageVehiclePage", "GotoRewardsPage"};
    for (const char* n : names) if (e == n) return true;
    return false;
}
std::string guest_cstr(Memory& m, uint32_t p) { std::string o; for (uint32_t i = 0; p && i < 96; i++) { uint8_t c = uint8_t(m.read(p + i, 1)); if (!c) break; o += char(c); } return o; }

void install_hook(Machine& mc) {
    Machine* key = &mc;
    mc.natives[0x740ed0] = [key](Machine& m) -> uint32_t {
        std::string e = guest_cstr(m.mem, m.r[0]);
        if (const char* tf = std::getenv("REFLEX_UI_TRACE")) { if (FILE* f = std::fopen(tf, "a")) { std::fprintf(f, "ui event %s\n", e.c_str()); std::fclose(f); } }   // diagnostics
        if (!leaves_level(e)) return RUN_ORIGINAL;
        { std::lock_guard<std::mutex> g(lock_); auto& r = mirror(*key).requests; if (r.size() < 64) r.push_back(e); }
        return m.ret(0);   // held back (cdecl, no stack arguments)
    };
    // App.RealWorldSwitchToFreeRide (Lua binding 0072e780 -> MXUIManager 0059b8e0; PostChallengePopup EXIT): the challenge's activity
    // gives way to the level's Free Ride through the AAA loading page. Held back as "RealWorldSwitchToFreeRide"; returns 0 results.
    mc.natives[0x72e780] = [key](Machine& m) -> uint32_t {
        { std::lock_guard<std::mutex> g(lock_); auto& r = mirror(*key).requests; if (r.size() < 64) r.push_back("RealWorldSwitchToFreeRide"); }
        return m.ret(0, 0);
    };
    mc.natives[ANIM_SET_STATE] = [key](Machine& m) -> uint32_t {
        uint32_t rec = m.r[1], st = uint32_t(m.mem.read(m.r[4] + 4, 4));
        if (st != uint32_t(m.mem.read(rec + 0x38, 4))) {   // SetState returns at once when the state does not change
            std::lock_guard<std::mutex> g(lock_); auto& log = mirror(*key).anim_log;
            if (log.size() < 4096) log.push_back({rec, st});
        }
        return RUN_ORIGINAL;
    };
}

// one component tree, depth first, appended to nodes (parent indices absolute); records -> node for the animation events
void walk(Memory& m, uint32_t root, int32_t layer, std::vector<UiNode>& out, std::string& text, std::vector<std::pair<uint32_t, int32_t>>* recs) {
    struct Item { uint32_t c; int32_t parent; };
    std::vector<Item> stack{{root, -1}};
    while (!stack.empty() && out.size() < 16384) {
        Item it = stack.back(); stack.pop_back();
        uint32_t c = it.c, vt = rd(m, c);
        if (!ui_vtable(vt)) continue;
        UiNode n{}; n.parent = it.parent; n.layer = layer; n.object = c; n.vtable = vt; n.state = rd(m, c + 0xb4) & 0xff;
        m.read_bytes(c + 0x40, n.position, 8); m.read_bytes(c + 0x60, n.scale, 8); m.read_bytes(c + 0x88, n.size, 8);
        n.color = rd(m, c + 0xd8);
        std::string nm = comp_name(m, c); std::memcpy(n.name, nm.c_str(), std::min<size_t>(nm.size(), 47));
        n.text = -1;
        if (text_class(m, vt)) {
            n.text = int32_t(text.size());
            // the text renderer at +0x100 holds the string drawn (wchar_t* +0x220, length +0x204: localized keys and dynamic strings alike);
            // the dynamic string buffer +0x368 (capacity +0x36c) when the renderer has none
            uint32_t s = rd(m, c + 0x220), len = rd(m, c + 0x204);
            if (!s || !len || len > 1024) { s = rd(m, c + 0x368); len = rd(m, c + 0x36c); }
            for (uint32_t i = 0; s && i < len && i < 1024; i++) { uint32_t ch = uint32_t(m.read(s + 2 * i, 2)); if (!ch) break; utf8(text, ch); }
            text += '\0';
        }
        int32_t self = int32_t(out.size()); out.push_back(n);
        if (recs) { uint32_t na = rd(m, c + 0x9c), ar = rd(m, c + 0xa0); for (uint32_t k = 0; ar && k < na && k < 64; k++) recs->push_back({ar + 0x4c * k, self}); }
        uint32_t b = rd(m, c + 0xcc), e = rd(m, c + 0xd0);
        if (b && e > b && e - b < 4 * 1024)
            for (uint32_t p = e; p > b; p -= 4) { uint32_t k = rd(m, p - 4); if (k && rd(m, k + 0x90) == c) stack.push_back({k, self}); }
    }
}
}  // namespace

std::string ui_requests(Machine& m) {
    std::lock_guard<std::mutex> g(lock_); auto& r = mirror(m).requests; std::string o;
    for (auto& e : r) { o += e; o += '\n'; }
    r.clear(); return o;
}
bool ui_active(const Machine& m) { std::lock_guard<std::mutex> g(lock_); auto it = mirrors.find(&m); return it != mirrors.end() && it->second.active; }
void ui_forget(const Machine* m) { std::lock_guard<std::mutex> g(lock_); mirrors.erase(m); }

void ui_event(Machine& mc, const std::string& name) {
    uint32_t at = UI_SCRATCH + 0x800;
    mc.call_function(0x740ed0, nullptr, 0, -1, scratch_str(mc.mem, at, name));
}

std::string ui_activate(Machine& mc, const std::string& event) {
    Memory& m = mc.mem;
    uint32_t G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0;
    auto graphs = page_graphs(m);
    uint32_t ingame = 0;
    for (uint32_t g : graphs) if (comp_name(m, g) == "InGamePageGraph.pag") ingame = g;
    if (!ingame || !A) return "";
    install_hook(mc);
    uint32_t at = UI_SCRATCH;
    // InGamePageGraph "Fullscreen HUB" (UIPageGraph::vf50 005ddcd0: page name, edge, 1, 1), focused as the edge leaves it
    uint32_t hub_name = scratch_str(m, at, "Fullscreen HUB"), args[4] = {hub_name, 0, 1, 1};
    mc.call_function(rd(m, rd(m, ingame) + 50 * 4), args, 4, ingame);
    for (uint32_t g : graphs) {   // the start-up graphs the boot never left: their pages stop running
        std::string n = comp_name(m, g);
        if (g != ingame && (n == "FrontEndIntroPageGraph.pag" || n == "IntroPageGraph.pag" || n == "FrontEndPageGraph.pag")) {
            if (rd(m, g + 0xb4) & 1) set_state(mc, g, 0, false);
            if (rd(m, g + 0xb4) & 0x10) set_state(mc, g, 4, false);
        }
    }
    set_state(mc, ingame, 0, true);
    if (uint32_t page = rd(m, ingame + 0xe8)) set_state(mc, page, 0, true);
    // the hub's route (PlayerJoined 005e2d20): FUN_005e3000(buf, 0x30, kind = 00556280(EAX activity), activity +0x238, player mode 0075ec80(),
    // "Goto") = "Goto" + the activity's UI name + "SinglePlayer"
    std::string route = event;
    if (route.empty()) {
        uint32_t buf = at; at += 0x40; m.write_block(buf, nullptr, 0x40);
        uint32_t kind = mc.call_function(0x556280, nullptr, 0, -1, A), mode = mc.call_function(0x75ec80, nullptr, 0);
        uint32_t a2[6] = {buf, 0x30, kind, rd(m, A + 0x238), mode, 0x95003c};
        mc.call_function(0x5e3000, a2, 6);
        char b[0x31] = {}; m.read_bytes(buf, b, 0x30); route = b;
    }
    if (route.empty()) return "";
    ui_event(mc, route);
    { std::lock_guard<std::mutex> g(lock_); auto& mr = mirror(mc); mr.active = true; mr.anim_log.clear(); }
    return route;
}

void ui_frame(Machine& mc, std::vector<UiLayer>& layers, std::vector<UiNode>& nodes, std::vector<UiAnim>& anims, std::string& text) {
    Memory& m = mc.mem; layers.clear(); nodes.clear(); anims.clear(); text.clear();
    std::vector<std::pair<uint32_t, int32_t>> recs;
    for (uint32_t g : page_graphs(m)) {
        uint32_t gs = rd(m, g + 0xb4);
        if (!(gs & 0x10)) continue;
        // a graph shows its current page (+0xe8); a UIPageStack every visible page of its std::list at +0xf0 (head node +0x14 = +0x104,
        // node {next, prev, page}), bottom first (UIPageStack::vf12 005de7c0)
        std::vector<uint32_t> pages;
        if (rd(m, g) == PAGE_STACK_VT) { uint32_t head = rd(m, g + 0x104); for (uint32_t n = rd(m, head), k = 0; n && n != head && k < 32; n = rd(m, n), k++) { uint32_t pg = rd(m, n + 8); if (pg && (rd(m, pg + 0xb4) & 0x10)) pages.push_back(pg); } }
        else if (uint32_t pg = rd(m, g + 0xe8)) pages.push_back(pg);
        std::string gname = comp_name(m, g);
        for (uint32_t page : pages) {
        if (!ui_vtable(rd(m, page))) continue;
        std::string pname = comp_name(m, page);
        for (uint32_t p = rd(m, page + 0xcc), e = rd(m, page + 0xd0); p && p < e && layers.size() < 64; p += 4) {
            uint32_t L = rd(m, p); if (!L || !ui_vtable(rd(m, L))) continue;
            UiLayer l{}; l.object = L; l.state = rd(m, L + 0xb4) & 0xff; l.first_node = int32_t(nodes.size());
            std::strncpy(l.graph, gname.c_str(), 31); std::strncpy(l.page, pname.c_str(), 47);
            std::string ln = comp_name(m, L); std::strncpy(l.name, ln.c_str(), 47);
            walk(m, L, int32_t(layers.size()), nodes, text, &recs);
            l.nodes = int32_t(nodes.size()) - l.first_node;
            layers.push_back(l);
        }
        }
    }
    std::vector<std::pair<uint32_t, uint32_t>> log;
    { std::lock_guard<std::mutex> g(lock_); auto& mr = mirror(mc); log.swap(mr.anim_log); }
    std::sort(recs.begin(), recs.end());
    for (auto& [rec, st] : log) {
        auto it = std::lower_bound(recs.begin(), recs.end(), std::make_pair(rec, int32_t(-0x7fffffff)));
        UiAnim a{}; a.node = it != recs.end() && it->first == rec ? it->second : -1; a.state = int32_t(st);
        a.time = rf(m, rec + 0x3c); a.length = rf(m, rec + 0x40);
        char nm[48] = {}; m.read_bytes(rec, nm, 47); std::memcpy(a.name, nm, 48);
        if (a.node >= 0) anims.push_back(a);
    }
}

int ui_collect(Machine& mc, const std::string& root, std::vector<UiNode>& out, std::string& text) {
    Memory& m = mc.mem; out.clear(); text.clear();
    if (root.empty() || root.size() > 46) return 0;
    std::vector<uint32_t> cands;
    {
        std::lock_guard<std::mutex> g(lock_); Mirror& mr = mirror(mc); mr.calls++;
        auto& v = mr.roots[root];
        v.erase(std::remove_if(v.begin(), v.end(), [&](uint32_t c) { return !ui_vtable(rd(m, c)) || comp_name(m, c) != root; }), v.end());
        auto sa = mr.scanned_at.find(root);
        if (v.empty() && (sa == mr.scanned_at.end() || mr.calls - sa->second >= 120)) {   // the instances: resident objects with a UI vtable named `root`
            mr.scanned_at[root] = mr.calls; size_t n = root.size() + 1;
            for (uint64_t pg = 0x10; pg < 0xe0000; pg++) {
                const uint8_t* p = m.peek(uint32_t(pg << 12)); if (!p) continue;
                for (size_t i = 0x10; i + n <= 4096; i += 4) {
                    if (p[i] != uint8_t(root[0]) || std::memcmp(p + i, root.c_str(), n)) continue;
                    uint32_t vt; std::memcpy(&vt, p + i - 0x10, 4);
                    if (ui_vtable(vt)) v.push_back(uint32_t(pg << 12) + uint32_t(i) - 0x10);
                }
            }
        }
        cands = v;
    }
    uint32_t best = 0;
    for (uint32_t c : cands) if (!best || (rd(m, c + 0xb4) & 0x10)) best = c;
    if (best) walk(m, best, 0, out, text, nullptr);
    return int(out.size());
}

// ---------------------------------------------------------------------------------------------------- the activity
namespace {
struct ActivityClass { uint32_t vtable; const char* name; };
// RTTI (_re/names/00_rtti_labels.tsv): the activities of the local player's slot
const ActivityClass ACTIVITIES[] = {
    {0x943554, "Race"}, {0x943654, "RaceChampIntro"}, {0x943754, "RaceChampPractice"}, {0x943854, "RaceChampHeat"}, {0x943954, "RaceChampMainEvent"},
    {0x943a54, "RaceChampLCQ"}, {0x94329c, "FreeRide"}, {0x943c14, "WaypointRace"}, {0x943d14, "WaypointRaceChampPractice"},
    {0x943e14, "WaypointRaceChampMainEvent"}, {0x943f14, "WaypointRaceChampIntro"}, {0x9442cc, "Freestyle"}, {0x9443cc, "FreestyleChampIntro"},
    {0x9444cc, "FreestyleChampPractice"}, {0x9445cc, "FreestyleChampMainEvent"}, {0x944a74, "SectionRace"}, {0x9450cc, "Challenge"},
    {0x9451dc, "HillClimbChallenge"}, {0x9452ec, "BigAirChallenge"}, {0x9453fc, "PathfinderChallenge"}, {0x945514, "StepUpChallenge"},
    {0x945624, "TrailBlazerChallenge"}, {0x945734, "TargetPracticeChallenge"}, {0x9446cc, "Tag"}, {0x944884, "Snake"}, {0x94497c, "Graffiti"},
    {0x944b74, "PokerRace"}, {0x944c74, "Infection"}, {0x944e24, "EggHunt"}, {0x944fd4, "Capture"},
};
}  // namespace

bool mode_state(Memory& m, ModeState& o) {
    std::memset(&o, 0, sizeof o); o.kind = -1;
    uint32_t G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0;
    if (!A) return false;
    o.valid = 1; o.activity = A; o.vtable = rd(m, A);
    for (size_t i = 0; i < sizeof ACTIVITIES / sizeof ACTIVITIES[0]; i++)
        if (ACTIVITIES[i].vtable == o.vtable) { o.kind = int32_t(i); std::strncpy(o.name, ACTIVITIES[i].name, 31); }
    o.state = int32_t(rd(m, A + 0x21c)); o.sub_state = int32_t(rd(m, A + 0x238));
    o.clock = rf(m, A + 0x294); o.countdown = rf(m, A + 0x29c);
    o.rules_hold = int32_t(rd(m, A + 0xff068));
    // the activation area the player is in (the trigger handler FUN_005a3b60, "StartActivity" triggers of "<level>FreeRide": MXPlayer +0xa0 = 1,
    // +0xa4 the activity, +0x1a8 the track; FUN_005a3d00 resets them on leaving), what the AAA notification shows and the pause key starts
    uint32_t P = G + 0x7460;
    o.area_active = int32_t(rd(m, P + 0xa0)); o.area_track = int32_t(rd(m, P + 0x1a8));
    m.read_bytes(P + 0xa4, o.area, 31); o.area[31] = 0; for (char& c : o.area) if (c && (c < 32 || c > 126)) { o.area[0] = 0; break; }
    return true;
}

namespace {
constexpr uint32_t COLLECTABLE_VTS[3] = {0x938e04, 0x938ebc, 0x938f7c};
struct ObjectCache { std::vector<uint32_t> objects; uint64_t calls = 0, scanned = 0; bool any = false; };
std::map<const Machine*, ObjectCache> object_caches;
}  // namespace

int mode_objects(Machine& mc, std::vector<ModeObject>& out) {
    Memory& m = mc.mem; out.clear();
    std::vector<uint32_t> objs;
    {
        std::lock_guard<std::mutex> g(lock_); auto& c = object_caches[&mc]; c.calls++;
        c.objects.erase(std::remove_if(c.objects.begin(), c.objects.end(), [&](uint32_t o) { uint32_t v = rd(m, o); return v != COLLECTABLE_VTS[0] && v != COLLECTABLE_VTS[1] && v != COLLECTABLE_VTS[2]; }), c.objects.end());
        if (!c.any || (c.objects.empty() && c.calls - c.scanned >= 120)) {   // heap objects (the level's sections are loaded): one scan, again rarely
            c.any = true; c.scanned = c.calls; c.objects.clear();
            for (uint64_t pg = 0x10000; pg < 0x40000; pg++) {
                uint32_t a = uint32_t(pg << 12);
                if (!m.mapped(a)) continue;
                const uint8_t* p = m.page(a); if (!p) continue;   // pages of the image not touched yet are loaded
                for (int i = 0; i < 4096; i += 4) { uint32_t v; std::memcpy(&v, p + i, 4);
                    if ((v == COLLECTABLE_VTS[0] || v == COLLECTABLE_VTS[1] || v == COLLECTABLE_VTS[2]) && rd(m, uint32_t(pg << 12) + i + 0x90) == v + 0x78)
                        c.objects.push_back(uint32_t(pg << 12) + uint32_t(i)); }   // the IRenderable vtable at +0x90 is vftable + 0x78 (vftable_o90)
            }
        }
        objs = c.objects;
    }
    for (uint32_t o : objs) {
        ModeObject x{}; x.object = o; x.vtable = rd(m, o);
        x.kind = x.vtable == COLLECTABLE_VTS[1] ? 1 : x.vtable == COLLECTABLE_VTS[2] ? 2 : 0;
        x.entity_id = int32_t(rd(m, o + 0x14)); x.drawn = int32_t(m.read(o + 0xa2, 1));
        m.read_bytes(o + 0x30, x.matrix, 64);
        if (x.kind == 1) { x.hit = int32_t(rd(m, o + 0x140)); x.axis = int32_t(rd(m, o + 0x154)); m.read_bytes(o + 0xab0, x.uv_offset, 8); }
        out.push_back(x);
    }
    return int(out.size());
}
// WP-T (tutorial.h): the hooks of ui_activate without its navigation (the UI of a tutorial world is in game already)
void ui_attach(Machine& mc) {
    install_hook(mc);
    std::lock_guard<std::mutex> g(lock_); auto& mr = mirror(mc); mr.active = true; mr.anim_log.clear();
}
}  // namespace reflex_vm
