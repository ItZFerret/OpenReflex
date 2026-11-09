// Persistence of arcade races, highlights and Steam achievements through the game's own code (persist.h; _re/notes/persistence.md,
// _re/names/86_persist.tsv).
#include "persist.h"
#include "boot.h"
#include "career.h"
#include "vm.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <sys/stat.h>
#include <vector>

namespace reflex_vm {
namespace career { std::string table_json(Machine& m, uint32_t t); }   // career.cpp
namespace {
// The Steam page of career.cpp (0xee100000: remote storage +0x000, user +0x100, friends +0x200, persona +0x800, scratch +0xa00) and its
// host code range (0xee200000 + 0x1000 * interface + 16 * slot); ISteamUserStats is interface 3, its object at +0x300 (vtable +0x310).
// A second page holds this file's scratch (strings and the callback parameter). Neither is ever part of a world image (>= 0xe0000000).
constexpr uint32_t STEAM_PAGE = 0xee100000, STEAM_CODE = 0xee200000, STEAM_STATS = STEAM_PAGE + 0x300, STATS_EMPTY = STEAM_PAGE + 0x3f0;
constexpr uint32_t SCRATCH = 0xee102000;
constexpr uint32_t PROFILE_MANAGER = 0xd92ee0, SAVED_GAMES = 0xdce458, RECORDS = 0xa946c0, GAME = 0xa946b8;
constexpr int ACHIEVEMENTS = 48;

uint32_t rd(Machine& m, uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0; }
std::string cstr(Machine& m, uint32_t p, size_t max = 256) {
    std::string s; if (!p || !m.mem.mapped(p)) return s;
    for (size_t i = 0; i < max; i++) { char c = char(m.mem.read(p + uint32_t(i), 1)); if (!c) break; s += c; }
    return s;
}
std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) { char b[1024]; va_list a; va_start(a, f); std::vsnprintf(b, sizeof b, f, a); va_end(a); return b; }
std::string jesc(const std::string& s) {
    std::string o; for (unsigned char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += char(c); } else if (c < 0x20) o += fmt("\\u%04x", c); else o += char(c); }
    return o;
}
void map_page(Machine& m, uint32_t a) { if (!m.mem.mapped(a)) m.mem.map(a, nullptr, 4096); }
void mkdirs(const std::string& path) { for (size_t i = 1; i <= path.size(); i++) if (i == path.size() || path[i] == '/') mkdir(path.substr(0, i).c_str(), 0755); }
bool read_text(const std::string& p, std::string& out) {
    FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false;
    char b[4096]; size_t n; out.clear(); while ((n = std::fread(b, 1, sizeof b, f)) > 0) out.append(b, n); std::fclose(f); return true;
}
bool write_text(const std::string& p, const std::string& s) {   // atomic, as the remote storage stand-in
    std::string tmp = p + ".tmp"; FILE* f = std::fopen(tmp.c_str(), "wb"); if (!f) return false;
    bool ok = std::fwrite(s.data(), 1, s.size(), f) == s.size(); ok = std::fclose(f) == 0 && ok;
    if (ok) ok = std::rename(tmp.c_str(), p.c_str()) == 0; else std::remove(tmp.c_str());
    return ok;
}

// Guest scratch for strings and small structs, reset per command.
struct Scratch {
    Machine& m; uint32_t at = SCRATCH;
    explicit Scratch(Machine& mm) : m(mm) { map_page(m, SCRATCH); }
    uint32_t bytes(const void* p, size_t n) { uint32_t a = at; m.mem.write_block(a, p, n); at += uint32_t((n + 15) & ~size_t(15)); return a; }
    uint32_t str(const std::string& s) { return bytes(s.c_str(), s.size() + 1); }
};

// ---- The Steam user's stats, shared by every machine of the process (profile session, race worlds) and kept in one file per save.
struct Achievement { bool achieved = false; uint32_t time = 0; };
struct Stat { char type = 'i'; uint32_t bits = 0; };
struct Store {
    std::mutex mu; std::string path;
    std::map<std::string, Achievement> achievements; std::map<std::string, Stat> stats;
    void load() {   // UserGameStats_55140.txt: "achievement <name> <0|1> <unix time>" / "stat <name> <i|f> <value>"
        achievements.clear(); stats.clear();
        std::string t; if (!read_text(path, t)) return;
        size_t at = 0;
        while (at < t.size()) {
            size_t e = t.find('\n', at); std::string l = t.substr(at, e == std::string::npos ? std::string::npos : e - at); at = e == std::string::npos ? t.size() : e + 1;
            char kind[16] = {}, name[128] = {}, a[32] = {}, b[32] = {};
            if (std::sscanf(l.c_str(), "%15s %127s %31s %31s", kind, name, a, b) < 3) continue;
            if (!std::strcmp(kind, "achievement")) { Achievement& x = achievements[name]; x.achieved = std::atoi(a) != 0; x.time = uint32_t(std::strtoul(b, nullptr, 10)); }
            else if (!std::strcmp(kind, "stat")) {
                Stat& s = stats[name]; s.type = a[0] == 'f' ? 'f' : 'i';
                if (s.type == 'f') { float v = std::strtof(b, nullptr); std::memcpy(&s.bits, &v, 4); } else s.bits = uint32_t(int32_t(std::strtol(b, nullptr, 10)));
            }
        }
    }
    bool save() {
        std::string t = "# MX vs ATV Reflex (Steam app 55140): the Steam user's achievements and stats as SteamUserStats stores them (libreflexworld persist.cpp)\n";
        for (auto& [n, a] : achievements) t += fmt("achievement %s %d %u\n", n.c_str(), a.achieved ? 1 : 0, a.time);
        for (auto& [n, s] : stats) { float f; std::memcpy(&f, &s.bits, 4); t += s.type == 'f' ? fmt("stat %s f %.9g\n", n.c_str(), double(f)) : fmt("stat %s i %d\n", n.c_str(), int32_t(s.bits)); }
        size_t slash = path.rfind('/'); if (slash != std::string::npos) mkdirs(path.substr(0, slash));
        return write_text(path, t);
    }
};
std::mutex stores_mu;
std::map<std::string, std::shared_ptr<Store>> stores;
std::map<const Machine*, std::shared_ptr<Store>> machine_store;
std::map<const Machine*, int> attached;   // world_command "attach" 1 / "attach_steam" 2
std::shared_ptr<Store> store_for(const std::string& path) {
    std::lock_guard<std::mutex> g(stores_mu);
    auto& s = stores[path]; if (!s) { s = std::make_shared<Store>(); s->path = path; std::lock_guard<std::mutex> g2(s->mu); s->load(); }
    return s;
}
std::shared_ptr<Store> store_of(const Machine& m) { std::lock_guard<std::mutex> g(stores_mu); auto it = machine_store.find(&m); return it == machine_store.end() ? nullptr : it->second; }

// The game's achievement names (its own table: what the Steam app's schema holds for it).
std::vector<std::string> schema(Machine& m) {
    std::vector<std::string> n;
    for (int i = 0; i < ACHIEVEMENTS; i++) n.push_back(cstr(m, rd(m, persist::ACHIEVEMENT_NAMES + 12u * uint32_t(i)), 64));
    return n;
}

// A call into the game between frames with the stack pointer kept (cdecl callees leave their arguments).
uint32_t callf(Machine& m, uint32_t fn, const uint32_t* args, int n, int64_t ecx = -1, int64_t eax = -1) { uint32_t sp = m.r[4]; uint32_t r = m.call_function(fn, args, n, ecx, eax); m.r[4] = sp; return r; }
Native answer(uint32_t cleanup, uint32_t value = 0) { return [cleanup, value](Machine& m) { m.r[2] = 0; return m.ret(cleanup, value); }; }
}  // namespace

void install_user_stats(Machine& m, const std::string& folder) {
    std::string dir = folder; while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
    auto st = store_for(dir + "/UserGameStats_" + std::to_string(persist::STEAM_APP_ID) + ".txt");
    { std::lock_guard<std::mutex> g(stores_mu); machine_store[&m] = st; }
    map_page(m, STEAM_PAGE);
    m.mem.write_block(STATS_EMPTY, nullptr, 16);
    auto names = std::make_shared<std::vector<std::string>>(schema(m));
    auto known = [names](const std::string& n) { return !n.empty() && std::find(names->begin(), names->end(), n) != names->end(); };
    auto stat_name = [](const std::string& n) { return !n.empty() && n.size() < 100 && n.find_first_of(" \t\r\n") == std::string::npos; };
    std::vector<Native> s(43);
    s[0] = answer(0, 1);                                                    // bool RequestCurrentStats(): the stats are here
    auto get_stat = [st, stat_name](char type) {                            // bool GetStat(name, float* / int32*)
        return [st, stat_name, type](Machine& m) {
            std::string n = cstr(m, m.arg(0)); uint32_t out = m.arg(1); bool ok = stat_name(n);
            if (ok) { std::lock_guard<std::mutex> g(st->mu); auto it = st->stats.find(n); uint32_t v = it == st->stats.end() ? 0 : it->second.bits;
                      if (it != st->stats.end() && it->second.type != type) { float f; if (type == 'f') { f = float(int32_t(v)); std::memcpy(&v, &f, 4); } else { std::memcpy(&f, &v, 4); v = uint32_t(int32_t(f)); } }
                      if (out) m.mem.write(out, 4, v); }
            return m.ret(8, ok ? 1 : 0);
        };
    };
    auto set_stat = [st, stat_name](char type) {                            // bool SetStat(name, float / int32)
        return [st, stat_name, type](Machine& m) {
            std::string n = cstr(m, m.arg(0)); bool ok = stat_name(n);
            if (ok) { std::lock_guard<std::mutex> g(st->mu); Stat& x = st->stats[n]; x.type = type; x.bits = m.arg(1); }
            return m.ret(8, ok ? 1 : 0);
        };
    };
    s[1] = get_stat('f'); s[2] = get_stat('i'); s[3] = set_stat('f'); s[4] = set_stat('i');   // MSVC lists the overloads in reverse
    s[5] = answer(16, 0);                                                   // UpdateAvgRateStat(name, float, double): no such stat
    s[6] = [st, known](Machine& m) {                                        // bool GetAchievement(name, bool* achieved)
        std::string n = cstr(m, m.arg(0)); uint32_t out = m.arg(1); bool ok = known(n);
        if (ok && out) { std::lock_guard<std::mutex> g(st->mu); auto it = st->achievements.find(n); m.mem.write(out, 1, it != st->achievements.end() && it->second.achieved ? 1 : 0); }
        return m.ret(8, ok ? 1 : 0);
    };
    s[7] = [st, known](Machine& m) {                                        // bool SetAchievement(name)
        std::string n = cstr(m, m.arg(0)); bool ok = known(n);
        if (ok) { std::lock_guard<std::mutex> g(st->mu); Achievement& a = st->achievements[n]; if (!a.achieved) { a.achieved = true; a.time = uint32_t(std::time(nullptr)); }
                  if (std::getenv("REFLEX_CAREER_TRACE")) std::fprintf(stderr, "[persist] SetAchievement %s\n", n.c_str()); }
        return m.ret(4, ok ? 1 : 0);
    };
    s[8] = [st, known](Machine& m) {                                        // bool ClearAchievement(name)
        std::string n = cstr(m, m.arg(0)); bool ok = known(n);
        if (ok) { std::lock_guard<std::mutex> g(st->mu); st->achievements[n] = Achievement(); }
        return m.ret(4, ok ? 1 : 0);
    };
    s[9] = [st, known](Machine& m) {                                        // bool GetAchievementAndUnlockTime(name, bool*, uint32* time)
        std::string n = cstr(m, m.arg(0)); bool ok = known(n);
        if (ok) { std::lock_guard<std::mutex> g(st->mu); auto it = st->achievements.find(n); bool a = it != st->achievements.end() && it->second.achieved;
                  if (m.arg(1)) m.mem.write(m.arg(1), 1, a ? 1 : 0); if (m.arg(2)) m.mem.write(m.arg(2), 4, a ? it->second.time : 0); }
        return m.ret(12, ok ? 1 : 0);
    };
    s[10] = [st](Machine& m) { bool ok; { std::lock_guard<std::mutex> g(st->mu); ok = st->save(); } return m.ret(0, ok ? 1 : 0); };   // bool StoreStats()
    s[11] = answer(4, 0);                                                   // int GetAchievementIcon(name): none
    s[12] = answer(8, STATS_EMPTY);                                         // const char* GetAchievementDisplayAttribute(name, key): ""
    s[13] = answer(12, 1);                                                  // IndicateAchievementProgress(name, cur, max)
    s[14] = answer(0, uint32_t(names->size()));                             // uint32 GetNumAchievements()
    s[15] = [](Machine& m) { uint32_t i = m.arg(0); return m.ret(4, i < uint32_t(ACHIEVEMENTS) ? rd(m, persist::ACHIEVEMENT_NAMES + 12u * i) : 0u); };   // GetAchievementName(i)
    s[16] = answer(8, 0);                                                   // SteamAPICall_t RequestUserStats(CSteamID): invalid (online)
    s[17] = answer(16); s[18] = answer(16); s[19] = answer(16); s[20] = answer(20);   // GetUserStat x2, GetUserAchievement(AndUnlockTime): other users
    s[21] = [st](Machine& m) {                                              // bool ResetAllStats(bool achievementsToo)
        bool all = (m.arg(0) & 0xff) != 0;
        { std::lock_guard<std::mutex> g(st->mu); st->stats.clear(); if (all) st->achievements.clear(); st->save(); }
        return m.ret(4, 1);
    };
    // the online leaderboards (MX07Leaderboards.xml, PCStats): no Steam servers, every call returns an invalid handle / nothing
    s[22] = answer(12); s[23] = answer(4); s[24] = answer(8, STATS_EMPTY); s[25] = answer(8); s[26] = answer(8); s[27] = answer(8);
    s[28] = answer(20); s[29] = answer(16); s[30] = answer(24); s[31] = answer(24); s[32] = answer(16); s[33] = answer(0); s[34] = answer(0);
    s[35] = answer(16, 0xffffffffu); s[36] = answer(20, 0xffffffffu); s[37] = answer(8); s[38] = answer(4); s[39] = answer(8); s[40] = answer(8); s[41] = answer(12); s[42] = answer(12);
    uint32_t vtable = STEAM_STATS + 0x10;
    m.mem.write(STEAM_STATS, 4, vtable);
    for (size_t i = 0; i < s.size(); i++) { uint32_t code = STEAM_CODE + 0x3000u + 16u * uint32_t(i); m.mem.write(vtable + 4 * uint32_t(i), 4, code); m.natives[code] = s[i]; }
    m.imports["steam_api.dll!SteamUserStats"] = [](Machine& m) { return m.ret(0, STEAM_STATS); };
}

namespace persist {
namespace {
uint32_t achievements_object(Machine& m) { uint32_t t = rd(m, HIGHLIGHT_TRACKING); return t ? rd(m, t + 4) : 0; }
}  // namespace

void stats_received(Machine& m) {
    uint32_t o = achievements_object(m); if (!o) return;
    // 006d8780 (logged on): +4 = SteamUtils()->GetAppID(); RequestCurrentStats. Steam answers with UserStatsReceived_t {m_nGameID (u64),
    // m_eResult = k_EResultOK, m_steamIDUser}, run by the registered CCallback (+0xc) as (object->*0x6d8750)(param): ECX object, ret 4.
    m.mem.write(o + 4, 4, STEAM_APP_ID);
    Scratch sc(m); uint32_t p[5] = {STEAM_APP_ID, 0, 1, 1, 0x01100001}; uint32_t param = sc.bytes(p, sizeof p);
    callf(m, 0x6d8750, &param, 1, o);
}
void stats_received(Boot& b) {
    Machine& m = b.machine(); uint32_t o = achievements_object(m); if (!o) return;
    m.mem.write(o + 4, 4, STEAM_APP_ID);
    Scratch sc(m); uint32_t p[5] = {STEAM_APP_ID, 0, 1, 1, 0x01100001}; uint32_t param = sc.bytes(p, sizeof p);
    Boot::GuestCall c; c.fn = 0x6d8750; c.ecx = o; c.args = {param}; b.call(c);
}

void forget(const Machine& m) { std::lock_guard<std::mutex> g(stores_mu); machine_store.erase(&m); attached.erase(&m); }

void flush_stats(Machine& m) {
    uint32_t o = achievements_object(m); if (!o || !rd(m, o)) return;
    auto st = store_of(m);
    if (st) { std::lock_guard<std::mutex> g(st->mu); st->save(); }   // SteamUserStats()->StoreStats() (slot 10 of the stand-in)
    m.mem.write(o, 4, 0);
}

namespace {
uint32_t find_table(Machine& m, uint32_t db, const char* name) {   // a table of a database (static 00ae0910, controller 00ae10f0 + 0x1f4 * c)
    uint32_t count = rd(m, db + 0x1e8);
    for (uint32_t i = 0; i < count && i < 200; i++) { uint32_t t = rd(m, db + 0x38 + 4 * i); if (!t) continue; char n[64] = {}; m.mem.read_bytes(t + 0x3c, n, 63); if (!std::strcmp(n, name)) return t; }
    return 0;
}
// The cells of an int / string column (a column: name +0, type +0x2c (1 int, 4 string), storage +0x40 with rows at +0xc..+0x10).
std::vector<int32_t> ints(Machine& m, uint32_t t, const char* column) {
    std::vector<int32_t> v;
    for (uint32_t p = rd(m, t + 0x10); t && p < rd(m, t + 0x14); p += 4) {
        uint32_t c = rd(m, p); char cn[33] = {}; m.mem.read_bytes(c, cn, 32); if (std::strcmp(cn, column) || rd(m, c + 0x2c) != 1) continue;
        uint32_t h = rd(m, c + 0x40); for (uint32_t a = rd(m, h + 0xc); a < rd(m, h + 0x10); a += 4) v.push_back(int32_t(rd(m, a)));
    }
    return v;
}
std::vector<std::string> strings(Machine& m, uint32_t t, const char* column) {
    std::vector<std::string> v;
    for (uint32_t p = rd(m, t + 0x10); t && p < rd(m, t + 0x14); p += 4) {
        uint32_t c = rd(m, p); char cn[33] = {}; m.mem.read_bytes(c, cn, 32); if (std::strcmp(cn, column) || rd(m, c + 0x2c) != 4) continue;
        uint32_t h = rd(m, c + 0x40);
        for (uint32_t s = rd(m, h + 0xc); s + 28 <= rd(m, h + 0x10); s += 28) { uint32_t len = rd(m, s + 20), cap = rd(m, s + 24), data = cap >= 16 ? rd(m, s + 4) : s + 4; v.push_back(cstr(m, data, std::min<uint32_t>(len, 256))); }
    }
    return v;
}
}  // namespace

std::string achievements_json(Machine& m) {
    auto st = store_of(m);
    uint32_t hl = find_table(m, 0xae0910, "Highlight"), phl = find_table(m, 0xae10f0, "PlayerHighlight");
    auto ids = ints(m, hl, "HighlightId"), goals = ints(m, hl, "Goal"); auto enums = strings(m, hl, "Enum");
    auto pids = ints(m, phl, "HighlightId"), progress = ints(m, phl, "GoalProgress"), order = ints(m, phl, "UnlockedOrder");
    auto names = schema(m);
    std::string j = "{\"steam\":" + std::string(st ? "1" : "0") + ",\"file\":\"" + jesc(st ? st->path : "") + "\"";
    uint32_t o = achievements_object(m);
    j += fmt(",\"statsReceived\":%u,\"achievements\":[", o ? rd(m, o + 8) : 0u);
    std::unique_lock<std::mutex> g; if (st) g = std::unique_lock<std::mutex>(st->mu);
    int unlocked = 0, achieved = 0;
    for (int i = 0; i < ACHIEVEMENTS; i++) {
        int id = i + 1, goal = -1, prog = 0, ord = -1; std::string en;
        for (size_t k = 0; k < ids.size(); k++) if (ids[k] == id) { goal = k < goals.size() ? goals[k] : -1; en = k < enums.size() ? enums[k] : ""; }
        for (size_t k = 0; k < pids.size(); k++) if (pids[k] == id) { prog = k < progress.size() ? progress[k] : 0; ord = k < order.size() ? order[k] : -1; }
        Achievement a; if (st) { auto it = st->achievements.find(names[size_t(i)]); if (it != st->achievements.end()) a = it->second; }
        bool u = goal > 0 && prog >= goal; unlocked += u; achieved += a.achieved;
        j += fmt("%s{\"id\":%d,\"enum\":\"%s\",\"goal\":%d,\"progress\":%d,\"unlockedOrder\":%d,\"unlocked\":%d,\"name\":\"%s\",\"achieved\":%d,\"unlockTime\":%u}",
                 i ? "," : "", id, jesc(en).c_str(), goal, prog, ord, int(u), jesc(names[size_t(i)]).c_str(), int(a.achieved), a.time);
    }
    j += fmt("],\"unlocked\":%d,\"achieved\":%d,\"stats\":{", unlocked, achieved);
    bool first = true;
    if (st) for (auto& [n, s] : st->stats) { float f; std::memcpy(&f, &s.bits, 4); j += (first ? "\"" : ",\"") + jesc(n) + "\":" + (s.type == 'f' ? fmt("%.9g", double(f)) : fmt("%d", int32_t(s.bits))); first = false; }
    return j + "}}";
}

namespace {
// The race's activity (the local player's, P+0x94 of slot 0) and its local human drivers' players (D+0x74: P+4 local, P+0x10 == 0 human,
// P+0x1f0 the records slot of the records manager [00a946c0], 0x18 bytes each: +0 PlayerRecords, +4 the race's working row, +0xc
// EventTypeLocationId, +0x10 VehicleTypeId, +0x14 holeshot won).
struct Local { uint32_t driver, player; int slot; };
uint32_t activity_of(Machine& m) { uint32_t G = rd(m, GAME); return G ? rd(m, G + 0x7460 + 0x94) : 0; }
std::vector<Local> local_humans(Machine& m, uint32_t A) {
    std::vector<Local> v; if (!A) return v;
    uint32_t list = rd(m, A + 0x268), n = rd(m, A + 0x26c);
    for (uint32_t i = 0; list && i < n && i < 64; i++) {
        uint32_t d = rd(m, list + 4 * i), p = d ? rd(m, d + 0x74) : 0;
        if (p && rd(m, p + 4) && !rd(m, p + 0x10) && int32_t(rd(m, p + 0x1f0)) >= 0) v.push_back({d, p, int32_t(rd(m, p + 0x1f0))});
    }
    return v;
}
// The activity's driver-joined records call (0055c0b6..0055c0e2): FUN_00764170(EAX slot; stack (records, P+0x1b0 vehicle row, A+0x23c (the
// series' 005b6a30 in tour / championship), A+0x220, [A+0x234] + 0x20030), ret 0x14) for local humans when A+0x238 != 4. It finds the
// race's EventTypeLocations row (EventTypeId, the level file, TrackId) and VehicleTypeId and adds the race's working PlayerRecords row,
// copied from the stored row for them (00763600) or initialised (00763a50).
int join_records(Machine& m, std::string* log) {
    uint32_t A = activity_of(m), mgr = rd(m, RECORDS); int n = 0;
    if (!A || !mgr || rd(m, A + 0x238) == 4) return 0;
    uint32_t owner = rd(m, 0x9e2188 + 0x190);
    for (auto& l : local_humans(m, A)) {
        uint32_t third = rd(m, A + 0x23c);
        if (owner == 1 || owner == 2) { uint32_t mgr2 = 0x9e2188; third = callf(m, 0x5b6a30, &mgr2, 1); }
        uint32_t args[5] = {mgr, rd(m, l.player + 0x1b0), third, rd(m, A + 0x220), rd(m, A + 0x234) + 0x20030};
        callf(m, 0x764170, args, 5, -1, uint32_t(l.slot));
        uint32_t rec = mgr + 0x18u * uint32_t(l.slot);
        if (log) *log += fmt("records joined: slot %d, working row %d, EventTypeLocationId %d, VehicleTypeId %d\n", l.slot, int32_t(rd(m, rec + 4)), int32_t(rd(m, rec + 0xc)), int32_t(rd(m, rec + 0x10)));
        n++;
    }
    return n;
}
// App.RequestSaveGame(0) (SavedGameManager_RequestSave 005a6910: EAX manager, stack (controller), ret 4) and the saved-game worker's
// write of the document it queued (SavedGameManager_Process 005a76c0: SavedGameManager_WriteSaveGame 005a6690, ECX document, through
// SteamCloudOutputStream -> FileWrite "savegame.bxml"; then the document's scalar deleting destructor). -1: nothing queued (no table
// changed, or no storage), else WriteSaveGame's result (0 = written).
int save_now(Machine& m) {
    uint32_t slot = SAVED_GAMES + 0x10; uint32_t zero = 0;
    callf(m, 0x5a6910, &zero, 1, -1, SAVED_GAMES);
    uint32_t doc = rd(m, slot + 8);
    if (!doc) { m.mem.write(SAVED_GAMES + 0x1d8, 4, 0); return -1; }
    m.mem.write(slot + 8, 4, 0);
    int r = int(callf(m, 0x5a6690, nullptr, 0, doc));
    uint32_t one = 1; callf(m, rd(m, rd(m, doc)), &one, 1, doc);
    m.mem.write(SAVED_GAMES + 0x1d8, 4, 0);
    return r;
}
std::string records_json(Machine& m) { uint32_t t = find_table(m, 0xae10f0, "PlayerRecords"); return t ? career::table_json(m, t) : "null"; }
std::string highlight_json(Machine& m) { uint32_t t = find_table(m, 0xae10f0, "PlayerHighlight"); return t ? career::table_json(m, t) : "null"; }
bool save_file_ok(const std::string& path) {   // PCStorageDevice_QueryDirectories' test: the file and its 0x27720006 trailer
    FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return false;
    uint32_t t = 0; bool ok = std::fseek(f, -4, SEEK_END) == 0 && std::fread(&t, 4, 1, f) == 1 && t == 0x27720006u; std::fclose(f); return ok;
}

// The profile signed in with storage and the save loaded, in a running world (the steps of career::sign_in that the boot ran with frames,
// made between frames): MXProfile_SignIn (006166a0: EAX profile, EDI storage directory) with the PCStorageDirectory "SaveGame" that
// PCStorageDevice_QueryDirectories makes (alloc 0x150, PCStorageDirectory_Construct 006d4b70: CL 1, EAX device, stack (object, name),
// ret 8). With a save: SignIn restores the default tables and queues the load; the load is the worker's read (a 0x460 document,
// FUN_00824e70 ESI object stack (0); SavedGameManager_ReadSaveGame 005a67a0 stack (document) -> 0 ok) handed to the main thread
// (slot +0xc = manager +0x1c) and applied there (005a6620 stack (manager): SavedGameManager_ApplyLoaded, the document destroyed). Without one
// (the device's new "SaveGame", MXProfile_SignInNewDirectory's path): IsGameLoaded (+0x1c) = 1, nothing to load.
std::string attach(Machine& m, const std::string& save, bool load, std::string* error) {
    install_remote_storage(m, save + "/remote");
    install_user_stats(m, save + "/stats");
    stats_received(m);
    { std::lock_guard<std::mutex> g(stores_mu); attached[&m] = load ? 1 : 2; }
    std::string log, j = "{";
    uint32_t profile = 0, mgr = rd(m, PROFILE_MANAGER); if (mgr) profile = rd(m, mgr + 4);
    if (!load || !profile) {
        if (load && error) *error = "no profile in this world";
        return j + fmt("\"steam\":1,\"statsReceived\":%u}", rd(m, achievements_object(m) + 8));
    }
    j += fmt("\"before\":{\"signedIn\":%u,\"saveEnabled\":%u,\"storage\":%u,\"gameLoaded\":%u,\"restore\":%u}", rd(m, profile + 4), rd(m, profile + 8), rd(m, profile + 0x2c) ? 1u : 0u, rd(m, profile + 0x1c), rd(m, profile + 0x10));
    bool has_save = save_file_ok(save + "/remote/savegame.bxml");
    int read = -1, applied = 0;
    if (!rd(m, profile + 0x2c)) {
        Scratch sc(m);
        uint32_t size = 0x150, dir = callf(m, 0x78fbe0, &size, 1);   // ReflexAlignedAlloc (cdecl)
        if (!dir) { if (error) *error = "no memory for the storage directory"; return j + "}"; }
        uint32_t storage = rd(m, profile + 0x28), device = storage ? rd(m, storage + 4) : 0;
        uint32_t args[2] = {dir, sc.str("SaveGame")};
        m.r[1] = 1; callf(m, 0x6d4b70, args, 2, -1, device);
        if (!has_save) m.mem.write(profile + 0x1c, 4, 1);
        m.r[7] = dir; callf(m, 0x6166a0, nullptr, 0, -1, profile);
        m.mem.write(SAVED_GAMES + 0x14, 4, 0);   // controller 0's queued load request (slot +4): made here, the worker thread does not run in a world
    }
    if (has_save) {
        uint32_t size = 0x460, doc = callf(m, 0x78fbe0, &size, 1);
        if (doc) {
            uint32_t zero = 0; m.r[6] = doc; callf(m, 0x824e70, &zero, 1);
            read = int(callf(m, 0x5a67a0, &doc, 1));
            if (read == 0) { m.mem.write(SAVED_GAMES + 0x1c, 4, doc); uint32_t sg = SAVED_GAMES; callf(m, 0x5a6620, &sg, 1); applied = 1; }   // slot +0xc: the loaded document
            else { uint32_t one = 1; callf(m, rd(m, rd(m, doc)), &one, 1, doc); }
        }
    }
    // Without a save the boot's tables are already the fresh profile's, with the race's working row: nothing to join again.
    int joined = applied ? join_records(m, &log) : 0;
    j += fmt(",\"save\":%d,\"read\":%d,\"applied\":%d,\"recordsJoined\":%d", int(has_save), read, applied, joined);
    j += fmt(",\"after\":{\"signedIn\":%u,\"saveEnabled\":%u,\"storage\":%u,\"gameLoaded\":%u}", rd(m, profile + 4), rd(m, profile + 8), rd(m, profile + 0x2c) ? 1u : 0u, rd(m, profile + 0x1c));
    j += ",\"log\":\"" + jesc(log) + "\"}";
    return j;
}
}  // namespace

int commit_records(Machine& m) {
    // PlayerLeft (0055c1d0, 0055c556..0055c580): local human, A+0x238 != 4, records manager set -> FUN_00764560 (stack (records, slot),
    // ret 8): the working row into the stored row for (EventTypeLocationId, VehicleTypeId) (added when missing), the working row deleted
    // (004ece50), then 007647f0 (the holeshot highlight 0x25 when +0x14).
    uint32_t A = activity_of(m), mgr = rd(m, RECORDS); int n = 0;
    if (!A || !mgr || rd(m, A + 0x238) == 4) return 0;
    for (auto& l : local_humans(m, A)) {
        if (int32_t(rd(m, mgr + 0x18u * uint32_t(l.slot) + 4)) == -1) continue;   // already left
        uint32_t args[2] = {mgr, uint32_t(l.slot)}; callf(m, 0x764560, args, 2); n++;
    }
    return n;
}

std::string world_command(Machine& m, const std::string& save_dir, const std::string& cmd, std::string* error) {
    std::string save = save_dir; while (save.size() > 1 && save.back() == '/') save.pop_back();
    if (cmd == "attach" || cmd == "attach_steam") return attach(m, save, cmd == "attach", error);
    if (cmd == "achievements") return achievements_json(m);
    if (cmd == "records") return "{\"records\":" + records_json(m) + ",\"highlights\":" + highlight_json(m) + "}";
    if (cmd == "leave" || cmd == "restart") {
        int mode; { std::lock_guard<std::mutex> g(stores_mu); auto it = attached.find(&m); mode = it == attached.end() ? 0 : it->second; }
        if (!mode) { if (error) *error = "not attached"; return ""; }
        if (mode == 2) { flush_stats(m); return "{\"command\":\"" + cmd + "\",\"steamOnly\":1}"; }   // a career world: career::finish_event saves
        uint32_t A = activity_of(m);
        int race = A ? int(rd(m, A + 0x21c)) : -1, first = -2, reset = 0;
        if (cmd == "leave") first = save_now(m);   // PostEventOverlay:OnPostStateComplete / PauseMenu:OnQuitEventPopupResult: App.RequestSaveGame
        else {
            // App.RestartActivity -> Activity::vf18 (00557de0): for each local human, FUN_00764710 (ECX records, EAX slot): the working row
            // reset to the stored row (00763600) or initialised (00763a50), the holeshot flag cleared
            uint32_t mgr = rd(m, RECORDS);
            if (mgr) for (auto& l : local_humans(m, A)) { callf(m, 0x764710, nullptr, 0, mgr, uint32_t(l.slot)); reset++; }
        }
        int committed = commit_records(m);   // the activity's teardown (SwitchToFrontEnd / the restarted race's world: PlayerLeft)
        int second = save_now(m);            // the profile carried on: the next save (the front end's, or the restarted race's)
        flush_stats(m);
        return fmt("{\"command\":\"%s\",\"raceState\":%d,\"saved\":[%d,%d],\"reset\":%d,\"committed\":%d", cmd.c_str(), race, first, second, reset, committed)
             + ",\"records\":" + records_json(m) + ",\"achievements\":" + achievements_json(m) + "}";
    }
    if (error) *error = "unknown persistence command: " + cmd;
    return "";
}
}  // namespace persist
}  // namespace reflex_vm
