// MOTOCAREER and its persistence through the game's own code (career.h; _re/notes/career.md, _re/names/80_career.tsv).
#include "career.h"
#include "persist.h"
#include "remap.h"
#include "reflex_world.h"
#include "vm.h"
#include <memory>
#include <mutex>
#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace reflex_vm {
namespace {
// Steam interface objects (never part of a world image: boot images leave out pages >= 0xe0000000; the world runtime installs
// them again). One page: ISteamRemoteStorage object +0x000 (vtable +0x010), ISteamUser +0x100 (vtable +0x110), ISteamFriends
// +0x200 (vtable +0x210), the persona name +0x800. Methods are host code at STEAM_CODE + 0x1000 * interface + 16 * slot.
constexpr uint32_t STEAM_PAGE = 0xee100000, STEAM_CODE = 0xee200000;
constexpr uint32_t STEAM_STORAGE = STEAM_PAGE, STEAM_USER = STEAM_PAGE + 0x100, STEAM_FRIENDS = STEAM_PAGE + 0x200, STEAM_PERSONA = STEAM_PAGE + 0x800;
constexpr uint64_t OFFLINE_STEAM_ID = 0x0110000100000001ull;   // individual account 1, public universe (the user is not logged on)

void steam_page(Machine& m) { if (!m.mem.mapped(STEAM_PAGE)) m.mem.map(STEAM_PAGE, nullptr, 4096); }
std::string guest_cstr(Machine& m, uint32_t p, size_t max = 512) {
    std::string s; if (!p) return s;
    for (size_t i = 0; i < max; i++) { char c = char(m.mem.read(p + uint32_t(i), 1)); if (!c) break; s += c; }
    return s;
}
// one interface: object -> vtable of `slots` host methods, each (machine) -> next pc with the method's own stdcall cleanup
void steam_interface(Machine& m, uint32_t object, int index, const std::vector<Native>& slots) {
    steam_page(m);
    uint32_t vtable = object + 0x10;
    m.mem.write(object, 4, vtable);
    for (size_t i = 0; i < slots.size(); i++) {
        uint32_t code = STEAM_CODE + 0x1000u * uint32_t(index) + 16u * uint32_t(i);
        m.mem.write(vtable + 4 * uint32_t(i), 4, code);
        m.natives[code] = slots[i];
    }
}
Native answer(uint32_t cleanup, uint32_t value = 0) { return [cleanup, value](Machine& m) { m.r[2] = 0; return m.ret(cleanup, value); }; }
bool safe_name(const std::string& n) { return !n.empty() && n.size() < 200 && n.find('/') == std::string::npos && n.find('\\') == std::string::npos && n != "." && n != ".."; }
void mkdirs(const std::string& path) { for (size_t i = 1; i <= path.size(); i++) if (i == path.size() || path[i] == '/') { std::string d = path.substr(0, i); mkdir(d.c_str(), 0755); } }
bool read_file(const std::string& p, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(p.c_str(), "rb"); if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    out.resize(size_t(std::max(0L, n))); bool ok = out.empty() || std::fread(out.data(), 1, out.size(), f) == out.size(); std::fclose(f); return ok;
}
bool write_file(const std::string& p, const uint8_t* data, size_t n) {   // atomic: a crash never leaves a half save
    std::string tmp = p + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb"); if (!f) return false;
    bool ok = std::fwrite(data, 1, n, f) == n; ok = std::fclose(f) == 0 && ok;
    if (ok) ok = std::rename(tmp.c_str(), p.c_str()) == 0; else std::remove(tmp.c_str());
    return ok;
}
std::string jesc(const std::string& s) {
    std::string o; for (unsigned char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += char(c); } else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += char(c); }
    return o;
}
}  // namespace

void install_remote_storage(Machine& m, const std::string& folder) {
    std::string dir = folder; while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
    auto path = [dir](Machine& m, uint32_t name) -> std::string { std::string n = guest_cstr(m, name); return safe_name(n) ? dir + "/" + n : std::string(); };
    auto size_of = [](const std::string& p) -> int64_t { struct stat st; return !p.empty() && stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode) ? int64_t(st.st_size) : -1; };
    std::vector<Native> s(18);
    s[0] = [path, dir](Machine& m) {   // bool FileWrite(const char* file, const void* data, int32 size)
        std::string p = path(m, m.arg(0)); uint32_t data = m.arg(1); int32_t n = int32_t(m.arg(2)); bool ok = false;
        if (!p.empty() && n >= 0 && n <= (100 << 20)) { std::vector<uint8_t> b(static_cast<size_t>(n)); if (n) m.mem.read_bytes(data, b.data(), b.size()); mkdirs(dir); ok = write_file(p, b.data(), b.size()); }
        return m.ret(12, ok ? 1 : 0);
    };
    s[1] = [path](Machine& m) {        // int32 FileRead(const char* file, void* data, int32 size): bytes read
        std::string p = path(m, m.arg(0)); uint32_t data = m.arg(1); int32_t n = int32_t(m.arg(2)); std::vector<uint8_t> b; int32_t got = 0;
        if (!p.empty() && n > 0 && read_file(p, b)) { got = int32_t(std::min<size_t>(b.size(), size_t(n))); m.mem.write_block(data, b.data(), size_t(got)); }
        return m.ret(12, uint32_t(got));
    };
    s[2] = answer(4);                  // FileForget
    s[3] = [path](Machine& m) { std::string p = path(m, m.arg(0)); return m.ret(4, !p.empty() && std::remove(p.c_str()) == 0 ? 1 : 0); };   // FileDelete
    s[4] = answer(4);                  // SteamAPICall_t FileShare(file): k_uAPICallInvalid
    s[5] = answer(8);                  // SetSyncPlatforms(file, platforms)
    s[6] = answer(4);                  // FileWriteStreamOpen(file): invalid handle
    s[7] = answer(16);                 // FileWriteStreamWriteChunk(handle u64, data, size)
    s[8] = answer(8);                  // FileWriteStreamClose(handle u64)
    s[9] = answer(8);                  // FileWriteStreamCancel(handle u64)
    s[10] = [path, size_of](Machine& m) { return m.ret(4, size_of(path(m, m.arg(0))) >= 0 ? 1 : 0); };   // bool FileExists(file)
    s[11] = [path, size_of](Machine& m) { return m.ret(4, size_of(path(m, m.arg(0))) >= 0 ? 1 : 0); };   // bool FilePersisted(file)
    s[12] = [path, size_of](Machine& m) { int64_t n = size_of(path(m, m.arg(0))); return m.ret(4, uint32_t(n < 0 ? 0 : n)); };   // int32 GetFileSize(file)
    s[13] = [path](Machine& m) { struct stat st; std::string p = path(m, m.arg(0)); int64_t t = !p.empty() && stat(p.c_str(), &st) == 0 ? int64_t(st.st_mtime) : 0; m.r[2] = uint32_t(uint64_t(t) >> 32); m.r[0] = uint32_t(t); uint32_t pc = m.ret(4); m.r[2] = uint32_t(uint64_t(t) >> 32); return pc; };   // int64 GetFileTimestamp
    s[14] = answer(4, 0xffffffffu);    // GetSyncPlatforms(file): all
    s[15] = [dir](Machine& m) { struct stat st; return m.ret(0, stat((dir + "/savegame.bxml").c_str(), &st) == 0 ? 1 : 0); };   // int32 GetFileCount()
    s[16] = [dir](Machine& m) {        // const char* GetFileNameAndSize(int i, int32* size)
        struct stat st; bool has = stat((dir + "/savegame.bxml").c_str(), &st) == 0 && m.arg(0) == 0;
        if (m.arg(1)) m.mem.write(m.arg(1), 4, has ? uint32_t(st.st_size) : 0);
        uint32_t name = STEAM_PAGE + 0x900; const char* n = has ? "savegame.bxml" : ""; m.mem.write_block(name, n, std::strlen(n) + 1);
        return m.ret(8, name);
    };
    s[17] = [](Machine& m) {           // bool GetQuota(int32* total, int32* available): Steam Cloud's per-game quota
        if (m.arg(0)) m.mem.write(m.arg(0), 4, 100u << 20); if (m.arg(1)) m.mem.write(m.arg(1), 4, 100u << 20);
        return m.ret(8, 1);
    };
    steam_interface(m, STEAM_STORAGE, 0, s);
    m.imports["steam_api.dll!SteamRemoteStorage"] = [](Machine& m) { return m.ret(0, STEAM_STORAGE); };
    // the same Steam user's stats and achievements (persist.h): <save>/stats next to <save>/remote
    size_t slash = dir.rfind('/'); install_user_stats(m, (slash == std::string::npos ? std::string(".") : dir.substr(0, slash)) + "/stats");
}

void install_steam_user(Machine& m, const std::string& persona) {
    steam_page(m);
    std::string p = persona.substr(0, 63); m.mem.write_block(STEAM_PERSONA, nullptr, 64); m.mem.write_block(STEAM_PERSONA, p.c_str(), p.size());
    auto steam_id = [](Machine& m, uint32_t out) { if (out) { m.mem.write(out, 4, uint32_t(OFFLINE_STEAM_ID)); m.mem.write(out + 4, 4, uint32_t(OFFLINE_STEAM_ID >> 32)); } };
    // ISteamUser016 (stdcall cleanup bytes per slot; CSteamID / CGameID by value are 8 bytes; a returned CSteamID is a hidden pointer)
    std::vector<Native> u = {
        answer(0, 1),                                                   // 0 GetHSteamUser
        answer(0, 0),                                                   // 1 BLoggedOn: not connected to the Steam servers
        [steam_id](Machine& m) { steam_id(m, m.arg(0)); return m.ret(4, m.arg(0)); },   // 2 GetSteamID
        answer(28), answer(8), answer(16),                              // 3 InitiateGameConnection, 4 TerminateGameConnection, 5 TrackAppUsageEvent
        answer(8), answer(0), answer(0),                                // 6 GetUserDataFolder (false), 7 Start/8 StopVoiceRecording
        answer(12, 2), answer(36, 2), answer(24, 2), answer(0, 11025),  // 9 GetAvailableVoice / 10 GetVoice / 11 DecompressVoice: not recording; 12 sample rate
        answer(12), answer(16, 1), answer(8), answer(4),                // 13 GetAuthSessionTicket, 14 BeginAuthSession (invalid ticket), 15 End, 16 Cancel
        answer(12, 2), answer(0), answer(16), answer(8), answer(12),    // 17 UserHasLicenseForApp (no auth), 18 BIsBehindNAT, 19 AdvertiseGame, 20/21 encrypted ticket
    };
    steam_interface(m, STEAM_USER, 1, u);
    // ISteamFriends013, slots 0..40 (the game uses 0 GetPersonaName; 22 / 26 overlays only online)
    std::vector<Native> f = {
        answer(0, STEAM_PERSONA), answer(4), answer(0, 0), answer(4, 0),                                   // 0 GetPersonaName, 1 Set, 2 GetPersonaState offline, 3 GetFriendCount
        [](Machine& m) { if (m.arg(0)) m.mem.write_block(m.arg(0), nullptr, 8); return m.ret(12, m.arg(0)); },   // 4 GetFriendByIndex
        answer(8), answer(8), answer(8, STEAM_PERSONA + 63), answer(12), answer(12, STEAM_PERSONA + 63), answer(12),   // 5..10
        answer(0), [](Machine& m) { if (m.arg(0)) m.mem.write_block(m.arg(0), nullptr, 8); return m.ret(8, m.arg(0)); },  // 11 GetClanCount, 12 GetClanByIndex
        answer(8, STEAM_PERSONA + 63), answer(8, STEAM_PERSONA + 63), answer(20), answer(8), answer(8),    // 13..17
        [](Machine& m) { if (m.arg(0)) m.mem.write_block(m.arg(0), nullptr, 8); return m.ret(16, m.arg(0)); },  // 18 GetFriendFromSourceByIndex
        answer(16), answer(12), answer(4), answer(12), answer(4), answer(8), answer(8), answer(8),        // 19..26
        answer(8), answer(8), answer(8), answer(12), answer(8),                                            // 27..31
        [](Machine& m) { if (m.arg(0)) m.mem.write_block(m.arg(0), nullptr, 8); return m.ret(12, m.arg(0)); },  // 32 GetClanOwner
        answer(8), [](Machine& m) { if (m.arg(0)) m.mem.write_block(m.arg(0), nullptr, 8); return m.ret(16, m.arg(0)); },  // 33, 34
        answer(0), answer(8), answer(0), answer(12, STEAM_PERSONA + 63), answer(8), answer(16, STEAM_PERSONA + 63),   // 35 GetUserRestrictions .. 40
    };
    steam_interface(m, STEAM_FRIENDS, 2, f);
    m.imports["steam_api.dll!SteamUser"] = [](Machine& m) { return m.ret(0, STEAM_USER); };
    m.imports["steam_api.dll!SteamFriends"] = [](Machine& m) { return m.ret(0, STEAM_FRIENDS); };
}

namespace career {
namespace {
uint32_t rd(Machine& m, uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0; }
uint32_t profile_of(Machine& m, int c = 0) { uint32_t mgr = rd(m, PROFILE_MANAGER); return mgr ? rd(m, mgr + 4 + 4 * uint32_t(c)) : 0; }
int game_state(Machine& m) { uint32_t g = rd(m, 0xd728d8); return g ? int(rd(m, g + 0x18)) : -1; }
void note(std::string* log, const std::string& s) { if (log) *log += s + "\n"; if (std::getenv("REFLEX_CAREER_TRACE")) { std::fprintf(stderr, "[career] %s\n", s.c_str()); std::fflush(stderr); } }
std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) { char b[512]; va_list a; va_start(a, f); std::vsnprintf(b, sizeof b, f, a); va_end(a); return b; }

// The UI manager and its property setters (profile-bootstrap.md): 005dbf50 ESI=ui EDI=key stack(value) ret 4; 005dbfb0 ECX=ui stack(key, value) ret 8.
uint32_t ui_of(Boot& b) { uint32_t g = b.read32(0xd728d8); return g ? b.read32(g + 0x745c) : 0; }
void ui_string(Boot& b, const char* key, const std::string& v) { Boot::GuestCall c; c.fn = 0x5dbf50; c.esi = ui_of(b); c.edi = b.guest_string(key); c.args = {b.guest_string(v)}; b.call(c); }
void ui_int(Boot& b, const char* key, int v) { Boot::GuestCall c; c.fn = 0x5dbfb0; c.ecx = ui_of(b); c.args = {b.guest_string(key), uint32_t(v)}; b.call(c); }
uint32_t call(Boot& b, uint32_t fn, std::vector<uint32_t> args = {}, uint32_t eax = 0, uint32_t ecx = 0, uint32_t esi = 0, uint32_t edi = 0) {
    Boot::GuestCall c; c.fn = fn; c.args = std::move(args); c.eax = eax; c.ecx = ecx; c.esi = esi; c.edi = edi; return b.call(c);
}
void frames(Boot& b, uint32_t n) { for (uint32_t i = 0; i < n; i++) b.run_frames(1, 0); }

// MXSeriesManager_EnableTourMode (the SWIG binding 00735da0 inlines it): series owner +0x190 = Tour, UI SeriesOwner = "Tour".
void enable_tour_mode(Boot& b) { b.machine().mem.write(SERIES_MANAGER + 0x190, 4, 1); ui_string(b, "SeriesOwner", "Tour"); }

// The front end reached as Boot::start_event does: state 2, 30 settling frames, and the window's focus (WM_SETFOCUS, 00880dc0).
bool front_end(Boot& b, std::string* log, uint32_t max_frames = 3000) {
    Machine& m = b.machine();
    for (uint32_t f = 0; game_state(m) != 2; f++) { if (f > max_frames) { note(log, "front end not reached"); return false; } frames(b, 1); }
    frames(b, 30);
    b.settle_front_end(3000);   // baton scheduling: until 5 s virtual, as Boot::start_event (the front end's timed steps)
    if (!b.read32(0xa9596c)) call(b, 0x880dc0, {0x10010, 7, 0, 0});
    return ui_of(b) && profile_of(m);
}
}  // namespace

std::string persona(const std::string& save_dir) {
    std::vector<uint8_t> b; std::string p;
    if (read_file(save_dir + "/steam_persona.txt", b)) for (uint8_t c : b) { if (c == '\n' || c == '\r' || p.size() >= 31) break; if (c >= 0x20) p += char(c); }
    return p.empty() ? "Player" : p;
}

bool sign_in(Boot& b, std::string* log) {
    Machine& m = b.machine();
    if (!front_end(b, log)) return false;
    uint32_t profile = profile_of(m);
    // PCSignInManager::vf2 (006e0f30): the offline Steam user signs controller 0 in (NetLoginStatus 1 -> 00616ed0 -> 00616740)
    for (uint32_t f = 0; f < 120 && !b.read32(profile + 4); f++) frames(b, 1);
    note(log, fmt("Steam sign-in: profile %08x signed in %u (sign-in manager state %u)", profile, b.read32(profile + 4), b.read32(b.read32(0xda6cac) + 0x34)));
    if (!b.read32(profile + 4)) { call(b, 0x6166a0, {}, profile, 0, 0, 0); note(log, "no Steam sign-in: MXProfile_SignIn(no storage)"); }
    // Title:OnStartPressed (PC): RequestNewStorageDevice( TRUE, FALSE ); SetPrimaryPlayerDeviceIndex
    call(b, 0x616560, {1, 0}, profile);
    call(b, 0x743030, {}, 0);
    // PCStorage::vf7 -> DeviceAcquired -> 00617110 -> MXProfile_SignIn(SaveGame) -> load queued; the worker loads (or fails) and
    // the profile's "Game Load(ed / Failed)" handler sets +0x1c (IsGameLoaded)
    uint32_t f = 0;
    for (; f < 900 && !(b.read32(profile + 0x2c) && b.read32(profile + 0x1c)); f++) frames(b, 1);
    note(log, fmt("storage sign-in after %u frames: directory %08x game loaded %u save enabled %u", f, b.read32(profile + 0x2c), b.read32(profile + 0x1c), b.read32(profile + 8)));
    frames(b, 2);
    return b.read32(profile + 0x2c) && b.read32(profile + 0x1c);
}

bool save(Boot& b, std::string* log, uint32_t max_frames) {
    Machine& m = b.machine();
    uint32_t slot = SAVED_GAMES + 0x10;   // controller 0's slot (+0x18 pending save document)
    // App.RequestSaveGame(0): 00742de0 -> SavedGameManager_RequestSave 005a6910 (EAX = manager, stack(controller), ret 4)
    call(b, 0x5a6910, {0}, SAVED_GAMES);
    bool queued = b.read32(slot + 0x8) != 0;
    uint32_t f = 0;
    for (; f < max_frames && (b.read32(slot + 0x8) || b.read32(SAVED_GAMES + 0x1d8)); f++) frames(b, 1);
    note(log, fmt("save requested (%s), written after %u frames", queued ? "queued" : "nothing to save", f));
    (void)m; return queued && f < max_frames;
}

// A table as JSON {"rows":n,"columns":{name:[values]}} (int, float and string columns); "" when it does not look like a table.
// A table: name +0x3c, columns +0x10..+0x14; a column: name +0, type +0x2c (1 int, 2 float, 4 string), storage +0x40 whose
// +0xc..+0x10 range holds the rows.
std::string table_json(Machine& m, uint32_t t) {
    static const uint32_t strides[] = {16, 4, 4, 1, 28, 24, 8};
    uint32_t cb = rd(m, t + 0x10), ce = rd(m, t + 0x14); if (ce < cb || ce - cb > 4 * 200) return "";
    std::vector<std::pair<std::string, uint32_t>> cols; uint32_t rows = 0;
    for (uint32_t p = cb; p < ce; p += 4) {
        uint32_t c = rd(m, p); if (!c) continue; char cn[33] = {}; m.mem.read_bytes(c, cn, 32);
        uint32_t ty = rd(m, c + 0x2c), h = rd(m, c + 0x40); if (ty >= 7 || !h) continue;
        uint32_t n = (rd(m, h + 0x10) - rd(m, h + 0xc)) / strides[ty]; if (cols.empty()) rows = n;
        cols.push_back({cn, c});
    }
    std::string j = "{\"rows\":" + std::to_string(rows) + ",\"columns\":{";
    bool first_col = true;
    for (auto& [cn, c] : cols) {
        uint32_t ty = rd(m, c + 0x2c), h = rd(m, c + 0x40), base = rd(m, h + 0xc), n = (rd(m, h + 0x10) - base) / strides[ty];
        if (ty != 1 && ty != 2 && ty != 4) continue;
        j += std::string(first_col ? "" : ",") + "\"" + jesc(cn) + "\":["; first_col = false;
        for (uint32_t r = 0; r < n && r < 4096; r++) {
            if (r) j += ",";
            if (ty == 1) j += std::to_string(int32_t(rd(m, base + 4 * r)));
            else if (ty == 2) { uint32_t v = rd(m, base + 4 * r); float fv; std::memcpy(&fv, &v, 4); j += std::isfinite(fv) ? fmt("%.9g", double(fv)) : std::string("0"); }
            else { uint32_t s = base + 28 * r, len = rd(m, s + 20), cap = rd(m, s + 24), data = cap >= 16 ? rd(m, s + 4) : s + 4; std::string str(std::min<uint32_t>(len, 1024), '\0'); if (!str.empty()) m.mem.read_bytes(data, &str[0], str.size()); j += "\"" + jesc(str) + "\""; }
        }
        j += "]";
    }
    return j + "}}";
}

std::string profile_json(Machine& m, bool with_logic, Boot* b) {
    std::string j = "{";
    uint32_t profile = profile_of(m);
    j += fmt("\"profile\":{\"signedIn\":%u,\"saveEnabled\":%u,\"noStorage\":%u,\"storage\":%u,\"gameLoaded\":%u},", rd(m, profile + 4), rd(m, profile + 8), rd(m, profile + 0xc), rd(m, profile + 0x2c) ? 1u : 0u, rd(m, profile + 0x1c));
    j += fmt("\"series\":{\"owner\":%u},", rd(m, SERIES_MANAGER + 0x190));
    // the Player* tables of controller 0 (database 00ae10f0: tables +0x38, count +0x1e8; a table: name +0x3c, columns +0x10..+0x14;
    // a column: name +0, type +0x2c (1 int, 2 float, 4 string), storage +0x40 whose +0xc..+0x10 range holds the rows)
    uint32_t db = 0xae10f0, count = rd(m, db + 0x1e8);
    j += "\"tables\":{";
    bool first_table = true;
    for (uint32_t i = 0; i < count && i < 120; i++) {
        uint32_t t = rd(m, db + 0x38 + 4 * i); if (!t) continue;
        std::string tj = table_json(m, t); if (tj.empty()) continue;
        char name[64] = {}; m.mem.read_bytes(t + 0x3c, name, 63);
        j += std::string(first_table ? "" : ",") + "\"" + jesc(name) + "\":" + tj; first_table = false;
    }
    j += "}";
    if (with_logic && b) {
        // the game's answers for the career pages (MXTableHelper: [0xd72a34]): IsLocked 004e96e0 (ECX helper, stack(device, product), ret 8),
        // IsHidden 004e9f50 (EAX product), IsStoreProductNew 004e9c50 (EAX device, stack(product), ret 4), IsStoreProductDLC 004e9bf0
        // (stack(product), ret 4) for every StoreProduct row of the static database; GetPlayerEventStatus 004eaaf0 (EAX device, ECX name);
        // CalculateCareerCompletion 00743870 (cdecl(device), float in xmm0)
        uint32_t helper = rd(m, 0xd72a34);
        std::vector<int32_t> products;
        uint32_t sdb = 0xae0910, scount = rd(m, sdb + 0x1e8);
        for (uint32_t i = 0; i < scount && i < 200; i++) {
            uint32_t t = rd(m, sdb + 0x38 + 4 * i); if (!t) continue; char name[64] = {}; m.mem.read_bytes(t + 0x3c, name, 63);
            if (std::strcmp(name, "StoreProducts")) continue;
            for (uint32_t p = rd(m, t + 0x10); p < rd(m, t + 0x14); p += 4) {
                uint32_t c = rd(m, p); char cn[33] = {}; m.mem.read_bytes(c, cn, 32); if (std::strcmp(cn, "StoreProductId") || rd(m, c + 0x2c) != 1) continue;
                uint32_t h = rd(m, c + 0x40); for (uint32_t a = rd(m, h + 0xc); a < rd(m, h + 0x10); a += 4) products.push_back(int32_t(rd(m, a)));
            }
        }
        j += ",\"storeProducts\":{";
        for (size_t i = 0; i < products.size(); i++) {
            uint32_t sp = uint32_t(products[i]);
            uint32_t locked = call(*b, 0x4e96e0, {0, sp}, 0, helper), hidden = call(*b, 0x4e9f50, {}, sp), fresh = call(*b, 0x4e9c50, {sp}, 0), dlc = call(*b, 0x4e9bf0, {sp});
            j += fmt("%s\"%d\":[%u,%u,%u,%u]", i ? "," : "", products[i], locked & 0xff, hidden & 0xff, fresh & 0xff, dlc & 0xff);
        }
        j += "}";
        j += ",\"events\":{";
        const char* events[] = {"CareerWelcome", "CareerCompleted", "TutorialsEntered"};
        for (size_t i = 0; i < sizeof events / sizeof *events; i++) j += fmt("%s\"%s\":%d", i ? "," : "", events[i], int32_t(call(*b, 0x4eaaf0, {}, 0, b->guest_string(events[i]))));
        j += "}";
        // App.GetUISetting( name, device ): UISetting_Index 00740c30, UISettings_Get 005b9f50 (ECX settings, EAX index, ESI device)
        j += ",\"uiSettings\":{";
        const char* settings[] = {"CareerDifficulty"};
        for (size_t i = 0; i < sizeof settings / sizeof *settings; i++) {
            uint32_t index = call(*b, 0x740c30, {b->guest_string(settings[i])});
            int32_t value = index <= 15 ? int32_t(call(*b, 0x5b9f50, {}, index, b->read32(0xd92efc), 0)) : -1;
            j += fmt("%s\"%s\":%d", i ? "," : "", settings[i], value);
        }
        j += "}";
        call(*b, 0x743870, {0}); float completion; std::memcpy(&completion, &b->machine().xmm[0], 4);
        j += fmt(",\"careerCompletion\":%.6g", double(completion));
    }
    return j + "}";
}

// ---- A race's outcome, carried from the world that ran it into a boot that replays the series up to its next race (Heat / Moto).
// The world image is the boot's memory at the race's start, so the activity, its drivers and their order are at the same addresses
// in a boot replaying the same save and choices: what the race changed and the series' results read is copied (career.md).
namespace {
constexpr uint32_t OUTCOME_MAGIC = 0x4354554f;   // 'OUTC'
const uint32_t OUTCOME_FIELDS[] = {0x70, 0x78, 0xf1c, 0x134, 0x10c4, 0x10e0, 0x1128, 0x10cc, 0x10b4, 0x10b8};   // finished, place, laps, race clock, best lap, last lap, ..., sort key, gaps
template <class Rd> bool outcome_drivers(Rd rd32, uint32_t& A, uint32_t& list, uint32_t& n) {
    uint32_t G = rd32(0xa946b8); A = G ? rd32(G + 0x7460 + 0x94) : 0; if (!A) return false;
    list = rd32(A + 0x268); n = rd32(A + 0x26c); return list && n && n <= 64;
}
}  // namespace

std::vector<uint32_t> race_outcome(Machine& m, uint32_t flags) {
    auto r = [&](uint32_t a) { return rd(m, a); };
    uint32_t A, list, n; std::vector<uint32_t> o;
    if (!outcome_drivers(r, A, list, n)) return o;
    o = {OUTCOME_MAGIC, 1, n, flags};
    for (uint32_t i = 0; i < n; i++) { uint32_t d = r(A + 0x2d0 + 4 * i), k = 0; while (k < n && r(list + 4 * k) != d) k++; o.push_back(k); }
    for (uint32_t k = 0; k < n; k++) for (uint32_t f : OUTCOME_FIELDS) o.push_back(r(r(list + 4 * k) + f));
    return o;
}

bool read_outcomes(const std::string& path, int count, std::vector<std::vector<uint32_t>>& out) {
    std::vector<uint8_t> bytes; if (!read_file(path, bytes) || bytes.size() % 4) return false;
    std::vector<uint32_t> w(bytes.size() / 4); std::memcpy(w.data(), bytes.data(), bytes.size());
    const size_t F = sizeof OUTCOME_FIELDS / sizeof *OUTCOME_FIELDS; size_t at = 0;
    for (int i = 0; i < count; i++) {
        if (at + 4 > w.size() || w[at] != OUTCOME_MAGIC || w[at + 2] == 0 || w[at + 2] > 64) return false;
        size_t len = 4 + w[at + 2] + w[at + 2] * F; if (at + len > w.size()) return false;
        out.emplace_back(w.begin() + long(at), w.begin() + long(at + len)); at += len;
    }
    return true;
}

bool read_gates(const std::string& path, std::vector<int>& out) {
    std::vector<uint8_t> bytes; if (!read_file(path, bytes) || bytes.size() % 4 || bytes.size() > 4 * 64) return false;
    out.resize(bytes.size() / 4); std::memcpy(out.data(), bytes.data(), bytes.size());
    return true;
}

namespace {
bool apply_outcome(Boot& b, const uint32_t* o, size_t words, std::string* log) {
    auto r = [&](uint32_t a) { return b.read32(a); };
    uint32_t A, list, n; const size_t F = sizeof OUTCOME_FIELDS / sizeof *OUTCOME_FIELDS;
    if (words < 4 || o[0] != OUTCOME_MAGIC || !outcome_drivers(r, A, list, n) || o[2] != n || words < 4 + n + n * F) { note(log, "career: the race outcome does not fit this race"); return false; }
    Machine& m = b.machine();
    for (uint32_t i = 0; i < n; i++) m.mem.write(A + 0x2d0 + 4 * i, 4, r(list + 4 * (o[4 + i] % n)));
    for (uint32_t k = 0; k < n; k++) { uint32_t d = r(list + 4 * k); for (size_t f = 0; f < F; f++) m.mem.write(d + OUTCOME_FIELDS[f], 4, o[4 + n + k * F + f]); }
    note(log, fmt("career: race outcome applied (%u riders, winner driver %u)", n, o[4]));
    return true;
}
}  // namespace

namespace {
// The loading (states 3..11) and its confirmation (state 6: Enter), as Boot::start_event; true once the world runs.
bool run_loading(Boot& b, uint32_t until_frame, std::string* log) {
    Machine& m = b.machine();
    int last = -1; uint32_t at6 = 0; uint8_t keys[256] = {};
    while (uint32_t(b.frames()) < until_frame) {
        int st = game_state(m);
        if (st != last) { note(log, fmt("state %d at frame %llu", st, (unsigned long long)b.frames())); last = st; }
        if (st == 6) { at6++; int off = int(at6) - 30; keys[0x1c] = (off >= 0 && off / 120 < 3 && off % 120 < 2) ? 0x80 : 0; b.set_keys(keys); }
        else if (at6) { keys[0x1c] = 0; b.set_keys(keys); }
        uint32_t g = b.read32(0xd728d8);
        if (st == 11 || (at6 && st != 6 && b.read32(g + 0x20c8))) { frames(b, 60); note(log, fmt("world running: state %d", game_state(m))); return true; }
        frames(b, 1);
    }
    note(log, "career: the event did not start in time"); return false;
}
// A UI property of the booted game (UIProperty_GetString 005dc060: EAX key, ESI size, EDI buffer, stack(ui), ret 4).
std::string boot_ui_string(Boot& b, const char* key) {
    uint32_t buf = b.guest_string(std::string(0x3f, ' '));
    Boot::GuestCall c; c.fn = 0x5dc060; c.eax = b.guest_string(key); c.esi = 0x30; c.edi = buf; c.args = {b.read32(0xa93fcc)};
    if (!b.call(c)) return "";
    return guest_cstr(b.machine(), buf, 0x30);
}
// Heat / Moto: the gate selection. The booted game stays on its loading page (no in-game HUD pages are made before the world image
// is written), so the GateSelection page's calls are made here, as gate_selection does in a world: ProcessGateSelection
// (005a9a30), then the local player's gate (the page's default focus: the first available, App.IsSeriesGateAvailable 0073ecc0) with
// AssignPlayerGate (005a9e00) and AdvanceGateSelection (005a9ce0); the race's activity then loads (the level's scene again).
// WP-5: `choices` / `index` - the player's gate at this gate selection (the series' index-th; Boot::EventSpec series_gates, -1 or missing: the
// default focus); the log line carries the selection's index, the available gates (bit i: App.IsSeriesGateAvailable( i )) and the default,
// which the client's Gate Selection page reads (OriginalGateSelection).
bool gate_selection_boot(Boot& b, uint32_t until_frame, std::string* log, const std::vector<int>* choices = nullptr, int* index = nullptr) {
    if (boot_ui_string(b, "SeriesEventStatus") != "GateSelection") return true;
    int k = index ? (*index)++ : 0, choice = choices && k < int(choices->size()) ? (*choices)[size_t(k)] : -1;
    uint32_t ev = b.read32(SERIES_MANAGER + 0x18c);
    call(b, 0x5a9a30, {}, 0, ev);
    std::string dev = boot_ui_string(b, "GateSelectionDeviceIndex");
    int gate = -1, first = -1; uint32_t mask = 0;
    if (!dev.empty() && dev[0] != '-') {
        for (int i = 0; i < 16; i++) if (call(b, 0x73ecc0, {}, 0, 0, uint32_t(i)) & 0xff) { mask |= 1u << i; if (first < 0) first = i; if (gate < 0 || i == choice) gate = i; }
        if (gate >= 0) call(b, 0x5a9e00, {uint32_t(std::atoi(dev.c_str())), uint32_t(gate)}, 0, ev);
    }
    call(b, 0x5a9ce0, {}, 0, ev);
    note(log, fmt("gate selection: device %s gate %d; status %s", dev.c_str(), gate, boot_ui_string(b, "SeriesEventStatus").c_str()));
    note(log, fmt("gate selection %d: available %04x default %d choice %d gate %d", k, mask, first, choice, gate));
    // the race's activity: the level reloads in game state 2 (the scene, then the activity's assets); done when the local player's
    // activity exists (P+0x94) and a second has passed, or through the loading states when the game goes there
    while (uint32_t(b.frames()) < until_frame) {
        frames(b, 1);
        int st = game_state(b.machine());
        if (st >= 3 && st <= 11) return run_loading(b, until_frame, log);
        uint32_t G = b.read32(0xa946b8), A = G ? b.read32(G + 0x7460 + 0x94) : 0;
        if (A && st == 2) { frames(b, 60); note(log, fmt("race activity %08x ready at frame %llu (race state %d)", A, (unsigned long long)b.frames(), int(b.read32(A + 0x21c)))); return true; }
    }
    note(log, "career: the race's activity did not start in time"); return false;
}
}  // namespace

namespace {
// HandleAdvanceNextSubEvent's other route (Heat): UIRoutingMessage "Select Activity Area" and GotoHub, which the in-game page graph
// takes to the "MXUIAAALoading Page" (MXUIAAALoadingPage::vf4 005a8030, series branch; not instantiated in the boot): FUN_00805120(1, 1),
// clear "Activity List Real World" (004ecdb0), FUN_0084ea00 (render thread / job pool idle), FUN_005a8af0(current event, 1) (the next
// sub event's players and activity), FUN_0075c430 (ESI = game) and FUN_0075a380 (ECX 1, EAX 0x3e: game state 9, "AAA Creation").
void aaa_loading(Boot& b, std::string* log) {
    call(b, 0x805120, {1, 1});
    call(b, 0x4ecdb0, {0xae1b18});
    // (FUN_0084ea00 waits for the render thread and the job pool to be idle: between frames they are)
    call(b, 0x5a8af0, {b.read32(SERIES_MANAGER + 0x18c), 1});
    { Boot::GuestCall c; c.fn = 0x75c430; c.esi = b.read32(0xa946b8); b.call_in_frame(c); }   // waits for the loader threads
    call(b, 0x75a380, {}, 0x3e, 1);
    note(log, fmt("career: Select Activity Area (MXUIAAALoadingPage): game state %d", game_state(b.machine())));
}
}  // namespace

bool start_series_event(Boot& b, const Boot::EventSpec& e, uint32_t max_frames, std::string* log) {
    Machine& m = b.machine();
    if (!sign_in(b, log)) { note(log, "career: the profile did not sign in with storage"); return false; }
    uint32_t start_frames = uint32_t(b.frames());
    // EnsureLocalPlayers / SetPrimaryPlayerDeviceIndex as Boot::start_event (controller 0)
    call(b, 0x59e030, {0, uint32_t(e.activity)}); frames(b, 1);
    call(b, 0x743030, {}, 0);
    // MainMenu:CareerMode
    ui_string(b, "PlayerMode", "SinglePlayer");
    enable_tour_mode(b);
    ui_int(b, "InCareerMode", 1);
    frames(b, 1);
    call(b, 0x59e030, {0, uint32_t(e.activity)}); frames(b, 1);
    // RaceAndRideEvents:Start (EnableTourMode) and OnAcceptSeries: progress = the series manager (GetMXSeriesProgress 00735fe0 returns it)
    enable_tour_mode(b);
    call(b, 0x5b0620, {}, SERIES_MANAGER);                                                   // progress:Reset( )
    call(b, 0x5b0ad0, {0, uint32_t(e.series_id), 1}, 0, 0, SERIES_MANAGER);                  // progress:LoadSeries( device, seriesId, GetMXSeriesOwner("tour") = 1 )
    if (e.series_vehicle_type >= 0) call(b, 0x5b1270, {SERIES_MANAGER, uint32_t(e.series_vehicle_type)});   // progress:SetVehicleTypeId( type )
    // CareerPracticePopup (Heat / Moto): progress:SetSkipPractice( TRUE / FALSE ) (the SWIG binding 00734fe0 writes progress +0x74)
    if (e.series_skip_practice >= 0) m.mem.write(SERIES_MANAGER + 0x74, 4, uint32_t(e.series_skip_practice != 0));
    note(log, fmt("career: series %d loaded (vehicle type %d, skip practice %d)", e.series_id, e.series_vehicle_type, e.series_skip_practice));
    frames(b, 1);
    // StartRacePopup:End (tour): CommitAndInitializeSeries, StartEvent, then GotoLoadingPage
    call(b, 0x5b4160, {SERIES_MANAGER});
    call(b, 0x5b7210, {SERIES_MANAGER});
    const uint32_t videos_at_loading = b.bink_opens();
    call(b, 0x740ed0, {}, b.guest_string("GotoLoadingPage"));
    note(log, "career: CommitAndInitializeSeries, StartEvent, GotoLoadingPage");
    if (!run_loading(b, start_frames + max_frames, log)) return false;
    // baton scheduling: the loading page's logo video is opened as Boot::start_event waits for it (the world image is past it)
    if (uint32_t n = b.wait_logo_video(videos_at_loading)) note(log, fmt("career: logo video after %u more frames", n));
    int gate_k = 0;
    if (!gate_selection_boot(b, start_frames + max_frames, log, &e.series_gates, &gate_k)) return false;
    // Heat / Moto: the races already run (their outcomes, from the worlds that ran them): each one's results and the next sub event,
    // as PostEventOverlay:OnPostStateComplete does (HeatOrLCQ -> MXSeriesManager::HandleAdvanceNextSubEvent), then its loading and
    // gate selection.
    size_t at = 0;
    for (int i = 0; i < int(e.series_outcomes.size()); i++) {
        const auto& oc = e.series_outcomes[size_t(i)];
        if (!apply_outcome(b, oc.data(), oc.size(), log)) return false;
        uint32_t ev = b.read32(SERIES_MANAGER + 0x18c), G = b.read32(0xa946b8), A = G ? b.read32(G + 0x7460 + 0x94) : 0;
        if (oc.size() > 3 && (oc[3] & 1)) { std::string rule = boot_ui_string(b, "RulesType"); if (!rule.empty()) call(b, 0x4eabd0, {1}, 0, b.guest_string("Rule_" + rule)); }   // RulesPopup:OnToggleKeypress
        if (!b.read32(ev + 0x30)) { b.machine().mem.write(ev + 0x14, 4, A); call(b, 0x592b20, {}, 0, ev); }      // post_results
        std::string status = boot_ui_string(b, "SeriesEventStatus");
        note(log, fmt("career: sub event %d results: %s", i + 1, status.c_str()));
        // HeatOrLCQ: HandleAdvanceNextSubEvent; GateSelection (after a practice / qualifier: the event's vf13 goes straight to the
        // gate pick of its first race in the same activity area): the gate selection below
        if (status != "HeatOrLCQ" && status != "GateSelection") { note(log, "career: the series does not continue after this race"); return false; }
        if (status == "HeatOrLCQ") {
            ui_string(b, "UIRoutingMessage", "");
            call(b, 0x5aad40, {}, 0, ev);                                                                           // next_sub_event
            if (boot_ui_string(b, "UIRoutingMessage") == "Select Activity Area") aaa_loading(b, log);
        }
        bool ready = false, reloaded = false; std::string last_status; int last_st = -2;
        while (!ready && uint32_t(b.frames()) < start_frames + max_frames) {
            frames(b, 1);
            int st = game_state(m);
            if (std::getenv("REFLEX_CAREER_TRACE")) {
                std::string ss = boot_ui_string(b, "SeriesEventStatus");
                uint32_t A3 = b.read32(b.read32(0xa946b8) + 0x7460 + 0x94);
                if (ss != last_status || st != last_st || b.frames() % 120 == 0) { note(log, fmt("  frame %llu: game state %d, SeriesEventStatus %s, RulesType %s, activity %08x race state %d hold %u drivers %u d0 finished %u", (unsigned long long)b.frames(), st, ss.c_str(), boot_ui_string(b, "RulesType").c_str(), A3, int(b.read32(A3 + 0x21c)), b.read32(A3 + 0xff068), b.read32(A3 + 0x26c), b.read32(b.read32(b.read32(A3 + 0x268)) + 0x70))); last_status = ss; last_st = st; }
            }
            if (st == 9 || st == 10) reloaded = true;   // the activity area's reload (MXUIAAALoadingPage)
            else if (st >= 3 && st <= 11) { if (!run_loading(b, start_frames + max_frames, log)) return false; reloaded = true; }
            if (boot_ui_string(b, "SeriesEventStatus") == "GateSelection") { if (!gate_selection_boot(b, start_frames + max_frames, log, &e.series_gates, &gate_k)) return false; ready = true; break; }
            uint32_t A2 = b.read32(b.read32(0xa946b8) + 0x7460 + 0x94), n2 = A2 ? b.read32(A2 + 0x26c) : 0;
            if (A2 && st == 2 && (A2 != A || reloaded) && n2 && !b.read32(b.read32(b.read32(A2 + 0x268)) + 0x70)) {
                frames(b, 60);
                if (boot_ui_string(b, "SeriesEventStatus") == "GateSelection") continue;
                ready = true; note(log, fmt("career: sub event %d's activity %08x ready (race state %d)", i + 2, A2, int(b.read32(A2 + 0x21c))));
            }
        }
        if (!ready) { note(log, "career: the next sub event did not start in time"); return false; }
        (void)at;
    }
    return true;
}

bool command(Boot& b, const std::string& cmd, std::string* out) {
    // "event_status <name> <value>"    MXTableHelper:SetPlayerEventStatus( device, name, value ) (004eabd0: EAX device, ECX name, stack(value))
    // "rider_int <column> <value>"      PlayerRider row 0 cell (playerRiderTable:setCell_int( 0, column, value ))
    // "store_not_new <product>"         MXTableHelper:SetStoreProductNotNew( device, product ) (004e9cd0: EAX device, stack(product), ret 4)
    // "save"                            App.RequestSaveGame( device )
    // "json"                            profile_json with the game's answers
    // "set_int <table> <row> <column> <value>"  a Player* table cell of controller 0 (table:setCell_int: Table_SetCellInt 004f6e60, ESI table,
    //                                   EAX column, stack(row, &value), ret 8; it bumps the table's revision +0x68, which the save requires)
    // "purchase_asset <asset>"          MXTableHelper:PurchaseStoreAsset( device, asset ) (004e8920: ECX helper, stack(device, asset), ret 8)
    char a[128] = {}, t[64] = {}; int v = 0, row = 0;
    if (std::sscanf(cmd.c_str(), "event_status %127s %d", a, &v) == 2) { call(b, 0x4eabd0, {uint32_t(v)}, 0, b.guest_string(a)); return true; }
    if (std::sscanf(cmd.c_str(), "set_int %63s %d %127s %d", t, &row, a, &v) == 4) {
        uint32_t table = call(b, 0x402220, {b.guest_string(t)}, 0xae10f0);   // Database_FindTable (EAX database, stack(name), ret 4)
        if (!table) { if (out) *out = std::string("no table ") + t; return false; }
        uint32_t value = b.guest_string("1234"); b.machine().mem.write(value, 4, uint32_t(v));
        call(b, 0x4f6e60, {uint32_t(row), value}, b.guest_string(a), 0, table);
        return true;
    }
    if (std::sscanf(cmd.c_str(), "purchase_asset %d", &v) == 1) { call(b, 0x4e8920, {0, uint32_t(v)}, 0, b.read32(0xd72a34)); return true; }
    if (std::sscanf(cmd.c_str(), "store_not_new %d", &v) == 1) { call(b, 0x4e9cd0, {uint32_t(v)}, 0); return true; }
    // "ui_setting <name> <value>"       App.SetUISetting( name, value, device ): index = UISetting_Index 00740c30 (cdecl(name), 16 names at
    //                                   009dd028), UISettings_Set 005b9fa0 (EAX settings [0xd92efc], EDX value, ESI index, EDI device)
    // "save_ui_settings"                App.SaveUISettings( device ): UISettings_Save 005b7e70 (stack(settings + device * 72 + 4), ret 4)
    if (std::sscanf(cmd.c_str(), "ui_setting %127s %d", a, &v) == 2) {
        uint32_t index = call(b, 0x740c30, {b.guest_string(a)});
        if (index > 15) { if (out) *out = std::string("no UI setting ") + a; return false; }
        Boot::GuestCall c; c.fn = 0x5b9fa0; c.eax = b.read32(0xd92efc); c.edx = uint32_t(v); c.esi = index; c.edi = 0; b.call(c);
        return true;
    }
    if (cmd == "save_ui_settings") { call(b, 0x5b7e70, {b.read32(0xd92efc) + 4}); return true; }
    // "rider_string <column> <text>"   PlayerRider row 0: playerRiderTable:setCell_string( 0, column, text ) (the SWIG binding's
    //                                  setter 0070eae0: EDX text, EDI table, stack(row, column); bumps the revision +0x68)
    // "validate_jersey_name <text>"    App.ValidateJerseyName (00742f00 cdecl(text): the text without punctuation) and
    //                                  App.IsStringSafe of the result (00743060, EDI text) -> "<safe> <validated>"
    // "profile_name_valid <text>"      App.IsProfileNameValid( device, text ) (00616f70: ECX text, stack(profile manager, device))
    {
        size_t sp = cmd.find(' ');
        std::string verb = cmd.substr(0, sp), rest = sp == std::string::npos ? "" : cmd.substr(sp + 1);
        if (verb == "rider_string") {
            size_t s2 = rest.find(' '); std::string column = rest.substr(0, s2), text = s2 == std::string::npos ? "" : rest.substr(s2 + 1);
            uint32_t table = call(b, 0x402220, {b.guest_string("PlayerRider")}, 0xae10f0);
            if (!table || column.empty()) { if (out) *out = "no PlayerRider table / column"; return false; }
            Boot::GuestCall c; c.fn = 0x70eae0; c.edx = b.guest_string(text); c.edi = table; c.args = {0, b.guest_string(column)}; b.call(c);
            return true;
        }
        if (verb == "validate_jersey_name") {
            uint32_t v = call(b, 0x742f00, {b.guest_string(rest)});
            std::string validated = guest_cstr(b.machine(), v ? v : 0xda7028, 128);
            uint32_t safe = call(b, 0x743060, {}, 0, 0, 0, b.guest_string(validated)) & 0xff;
            if (out) *out = std::to_string(safe) + " " + validated;
            return true;
        }
        // "favorite_vehicle <playerVehicleId>"  MXTableHelper:SetPlayerFavoriteVehicle( device, id, TRUE ) (004e7e60: EAX device,
        //                                  stack(id, favorite)): PlayerVehicle.Favorite and PlayerRider.LastEquippedVehicle
        if (verb == "favorite_vehicle") {
            call(b, 0x4e7e60, {uint32_t(std::atoi(rest.c_str())), 1}, 0);
            return true;
        }
        // "lua <source>"                  the source run in the game's Lua state (the UI scripts' state, wrapper 00d94938) as the engine
        //                                  runs a script asset (LuaAssetIO -> 00744df0): FUN_00744920 (ECX chunk name, stack(wrapper,
        //                                  buffer, size), ret 0xc: luaL_loadbuffer 0077a4b0, the function stored as the global
        //                                  "<name>_Chunk"), then FUN_00744e70 (cdecl(wrapper, "<name>_Chunk", "")) calls it.
        if (verb == "lua") {
            static int serial = 0;
            std::string name = "ReflexCareer" + std::to_string(++serial);
            uint32_t buf = b.guest_string(rest);
            call(b, 0x744920, {0xd94938, buf, uint32_t(rest.size())}, 0, b.guest_string(name));
            call(b, 0x744e70, {0xd94938, b.guest_string(name + "_Chunk"), b.guest_string("")});
            return true;
        }
        // "lua_ui <key> <source>"          as "lua", then the UI variable <key> as the answer (App.GetUIVariableString: UIProperty_GetString
        //                                  005dc060, EAX key, ESI size, EDI buffer, stack(ui manager [00a93fcc]); 32 KB): what the source
        //                                  leaves there with App.SetUIVariableString (App.ProcessCheatEntryString's result, the trick
        //                                  library's App.GetStuntDescriptorTierContainer rows, ...); the variable and both chunks' globals are removed after.
        if (verb == "lua_ui") {
            size_t s2 = rest.find(' '); std::string key = rest.substr(0, s2), src = s2 == std::string::npos ? "" : rest.substr(s2 + 1);
            if (key.empty() || src.empty()) { if (out) *out = "lua_ui <key> <source>"; return false; }
            static int serial_ui = 0;
            std::string name = "ReflexCareerUi" + std::to_string(++serial_ui);
            call(b, 0x744920, {0xd94938, b.guest_string(src), uint32_t(src.size())}, 0, b.guest_string(name));
            call(b, 0x744e70, {0xd94938, b.guest_string(name + "_Chunk"), b.guest_string("")});
            const uint32_t size = 0x8000;
            uint32_t buf = b.guest_string(std::string(size - 1, ' '));
            Boot::GuestCall c; c.fn = 0x5dc060; c.eax = b.guest_string(key); c.esi = size; c.edi = buf; c.args = {b.read32(0xa93fcc)};
            bool found = (b.call(c) & 0xff) != 0;
            if (out) *out = found ? guest_cstr(b.machine(), buf, size) : "";
            std::string rm = "App.RemoveUIVariable( \"" + key + "\" ); " + name + "_Chunk = nil; " + name + "R_Chunk = nil";
            call(b, 0x744920, {0xd94938, b.guest_string(rm), uint32_t(rm.size())}, 0, b.guest_string(name + "R"));
            call(b, 0x744e70, {0xd94938, b.guest_string(name + "R_Chunk"), b.guest_string("")});
            return true;
        }
        if (verb == "profile_name_valid") {
            uint32_t ok = call(b, 0x616f70, {b.read32(PROFILE_MANAGER), 0}, 0, b.guest_string(rest)) & 0xff;
            if (out) *out = std::to_string(ok);
            return true;
        }
    }
    // "achievements"                   the 48 highlights / Steam achievements and the stats (persist.h achievements_json)
    if (cmd == "achievements") { if (out) *out = persist::achievements_json(b.machine()); return true; }
    // "remap ..."                      the ControllerRemapping table, key / pad names, alreadyMapped, KeyMap glyphs (remap.h, WP-C)
    if (cmd.rfind("remap", 0) == 0) return remap::profile_command(b, cmd, out);
    // "canonical <file>"               diagnostics (native/game tools/world_compare.py): the session's heap as a layout-free dump (Boot::write_canonical)
    // "memhash"                        diagnostics: FNV-1a over every resident guest page below 0x7f000000 (the boot's heap, images and globals;
    //                                  the thread stacks, dead between frames, at 0x12000000.. are included: compare canonical dumps for layout-free checks)
    if (cmd.rfind("canonical ", 0) == 0) { std::string info; b.write_canonical(cmd.substr(10), false, &info); if (out) *out = info; return true; }
    if (cmd == "memhash") {
        Machine& m = b.machine(); uint64_t h = 1469598103934665603ull; uint32_t pages = 0;
        for (uint64_t pg = 0; pg < (0x7f000000ull >> 12); pg++) { const uint8_t* q = m.mem.peek(uint32_t(pg << 12)); if (!q) continue; pages++;
            for (int i = 0; i < 4096; i += 8) { uint64_t v; std::memcpy(&v, q + i, 8); h = (h ^ v) * 1099511628211ull; } }
        if (out) *out = fmt("%016llx %u pages", (unsigned long long)h, pages);
        return true;
    }
    if (cmd == "save") return save(b, out);
    if (cmd == "json") { if (out) *out = profile_json(b.machine(), true, &b); return true; }
    if (out) *out = "unknown command: " + cmd;
    return false;
}

namespace {
// World-mode guest scratch for strings (the Steam page, never part of an image).
struct Scratch {
    Machine& m; uint32_t at = STEAM_PAGE + 0xa00;
    explicit Scratch(Machine& mm) : m(mm) { steam_page(m); }
    uint32_t str(const char* s) { uint32_t a = at; size_t n = std::strlen(s) + 1; m.mem.write_block(a, s, n); at += uint32_t((n + 15) & ~size_t(15)); return a; }
    std::string ui_string(const char* key) {   // UIProperty_GetString 005dc060 (EAX key, ESI size, EDI buffer, stack(ui), ret 4)
        uint32_t buf = at; at += 0x40; m.mem.write_block(buf, nullptr, 0x40);
        uint32_t ui = rd(m, 0xa93fcc), k = str(key); m.r[6] = 0x30; m.r[7] = buf; m.call_function(0x5dc060, &ui, 1, -1, k);
        return guest_cstr(m, buf, 0x30);
    }
};
}  // namespace

std::string post_results(Machine& m) {
    uint32_t ev = rd(m, SERIES_MANAGER + 0x18c), G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0;
    if (!ev || !A) return "";
    // MXUIPostEventOverlay's "Show Activity Results" handler (005f42e0, tour branch): the series event's activity (+0x14) and
    // MXSeriesManager::AdvancePostEvent, whose handler (00592b20) is the event's vf13 (standings, points, SeriesEventStatus). The in-game
    // HUD pages are not instantiated in the world (the client draws its own), so their listener is reproduced; once per activity (+0x30).
    if (!rd(m, ev + 0x30)) { m.mem.write(ev + 0x14, 4, A); m.call_function(0x592b20, nullptr, 0, ev); }
    return Scratch(m).ui_string("SeriesEventStatus");
}

std::string world_ui_string(Machine& m, const char* key) { return Scratch(m).ui_string(key); }

int gate_selection(Machine& m, int choice) {
    // The in-game GateSelection page (GateSelection.lua; not instantiated in the world): Start sends
    // MXSeriesManager::ProcessGateSelection (handler MXSeriesEvent_ProcessGateSelection 005a9a30, ECX = the series event: the riders
    // pick in order, the AI at random with the game's own generator, until a local player's turn sets GateSelectionDeviceIndex);
    // OnAccept on an available gate (App.IsSeriesGateAvailable = 0073ecc0, ESI gate) sends AssignPlayerGate (005a9e00, ECX event,
    // stack (device, gate), ret 8) and AdvanceGateSelection (005a9ce0, ECX event: the remaining picks, then the race).
    // choice < 0: the page's default focus, the first available gate. Returns the gate taken (-1: no gate selection pending).
    uint32_t ev = rd(m, SERIES_MANAGER + 0x18c);
    if (!ev || world_ui_string(m, "SeriesEventStatus") != "GateSelection") return -1;
    m.call_function(0x5a9a30, nullptr, 0, ev);
    std::string dev = world_ui_string(m, "GateSelectionDeviceIndex");
    if (dev.empty() || dev[0] == '-') return -1;
    int gate = -1;
    for (int i = 0; i < 16; i++) {
        m.r[6] = uint32_t(i); bool ok = (m.call_function(0x73ecc0, nullptr, 0) & 0xff) != 0;
        if (ok && (gate < 0 || i == choice)) gate = i;
    }
    if (gate < 0) return -1;
    uint32_t args[2] = {uint32_t(std::atoi(dev.c_str())), uint32_t(gate)};
    m.call_function(0x5a9e00, args, 2, ev);
    m.call_function(0x5a9ce0, nullptr, 0, ev);
    return gate;
}

void next_sub_event(Machine& m) {
    // PostEventOverlay:OnPostStateComplete with SeriesEventStatus "HeatOrLCQ": App.SendEvent( "UI", "MXSeriesManager::HandleAdvanceNextSubEvent" )
    // -> the handler MXSeriesEvent_OnAdvanceNextSubEvent (005aad40, ECX = the series event): requested player states, then the next
    // sub event's activity (the event's vf9 / vf12 -> MXSeriesEvent_SetupActivity 005a9290).
    uint32_t ev = rd(m, SERIES_MANAGER + 0x18c);
    if (ev) m.call_function(0x5aad40, nullptr, 0, ev);
}

namespace {
// UIProperty_GetInt 005dc0c0 (EAX key, stack(ui, int* out)).
int world_ui_int(Machine& m, const char* key, int fallback) {
    Scratch sc(m); uint32_t out = sc.str("    "), k = sc.str(key); m.mem.write(out, 4, uint32_t(fallback));
    uint32_t args[2] = {rd(m, 0xa93fcc), out}; m.call_function(0x5dc0c0, args, 2, -1, k);
    return int32_t(rd(m, out));
}
std::string utf16(Machine& m, uint32_t p, size_t max = 256) {   // a guest wide string as UTF-8
    std::string o;
    for (size_t i = 0; p && i < max; i++) {
        uint32_t c = uint32_t(m.mem.read(p + 2 * uint32_t(i), 2)); if (!c) break;
        if (c < 0x80) o += char(c); else if (c < 0x800) { o += char(0xc0 | (c >> 6)); o += char(0x80 | (c & 0x3f)); }
        else { o += char(0xe0 | (c >> 12)); o += char(0x80 | ((c >> 6) & 0x3f)); o += char(0x80 | (c & 0x3f)); }
    }
    return o;
}
// App.FormatCredits( value ) (005dc430: ECX value, EDX size, stack(buffer), cdecl)
std::string format_credits(Machine& m, int32_t v) {
    Scratch sc(m); uint32_t buf = sc.at; sc.at += 0x40; m.mem.write_block(buf, nullptr, 0x40);
    m.r[2] = 0x40; m.call_function(0x5dc430, &buf, 1, uint32_t(v));
    return guest_cstr(m, buf, 0x40);
}
uint32_t static_table(Machine& m, const char* name) {   // a table of the static database 00ae0910
    uint32_t db = 0xae0910, count = rd(m, db + 0x1e8);
    for (uint32_t i = 0; i < count && i < 200; i++) { uint32_t t = rd(m, db + 0x38 + 4 * i); if (!t) continue; char n[64] = {}; m.mem.read_bytes(t + 0x3c, n, 63); if (!std::strcmp(n, name)) return t; }
    return 0;
}
std::vector<int32_t> int_column(Machine& m, uint32_t t, const char* column) {
    std::vector<int32_t> v;
    for (uint32_t p = rd(m, t + 0x10); t && p < rd(m, t + 0x14); p += 4) {
        uint32_t c = rd(m, p); char cn[33] = {}; m.mem.read_bytes(c, cn, 32); if (std::strcmp(cn, column) || rd(m, c + 0x2c) != 1) continue;
        uint32_t h = rd(m, c + 0x40); for (uint32_t a = rd(m, h + 0xc); a < rd(m, h + 0x10); a += 4) v.push_back(int32_t(rd(m, a)));
    }
    return v;
}
// progress:GetRewards( ) (the MXSeriesProgress rewards at +0x78, SWIG 00735700) as PostEventRewards.lua shows them: GetPlace +0xf0,
// GetBasePrizeMoney +0xf4, GetBonusMoney +0xf8, GetTotalPrizeMoney (their sum), GetUnlockTable (+4: StoreProductId, titles from
// MXTableHelper:TitleLocalizationString 004e9f30) and GetPurchaseTable (+0x78: StoreAssetId); and CareerFrontEndLoadingPage:DoneLoading's
// reward vehicle: the first purchase reward not yet purchased (IsStoreAssetPurchased 004e88a0: EAX device, stack(asset)) that is a
// VehicleModel row's StoreAssetId.
std::string rewards_json(Machine& m, int page) {
    uint32_t R = SERIES_MANAGER + 0x78;
    int32_t place = int32_t(rd(m, R + 0xf0)), base = int32_t(rd(m, R + 0xf4)), bonus = int32_t(rd(m, R + 0xf8)), total = base + bonus;
    std::string j = fmt("{\"page\":%d,\"place\":%d,\"base\":%d,\"bonus\":%d,\"total\":%d", page, place, base, bonus, total);
    j += ",\"baseText\":\"" + jesc(format_credits(m, base)) + "\",\"bonusText\":\"" + jesc(format_credits(m, bonus)) + "\",\"totalText\":\"" + jesc(format_credits(m, total)) + "\"";
    j += ",\"unlocks\":[";
    auto unlocks = int_column(m, R + 4, "StoreProductId");
    for (size_t i = 0; i < unlocks.size(); i++) j += std::string(i ? "," : "") + fmt("{\"product\":%d,\"title\":\"", unlocks[i]) + jesc(utf16(m, m.call_function(0x4e9f30, nullptr, 0, -1, uint32_t(unlocks[i])))) + "\"}";
    j += "],\"purchases\":[";
    auto assets = int_column(m, R + 0x78, "StoreAssetId");
    int32_t vehicle_asset = -1, vehicle_model = -1;
    uint32_t vm = static_table(m, "VehicleModel"); auto vm_assets = int_column(m, vm, "StoreAssetId"), vm_ids = int_column(m, vm, "VehicleModelId");
    for (size_t i = 0; i < assets.size(); i++) {
        uint32_t a = uint32_t(assets[i]); bool owned = (m.call_function(0x4e88a0, &a, 1, -1, 0) & 0xff) != 0;
        j += fmt("%s{\"asset\":%d,\"purchased\":%d}", i ? "," : "", assets[i], int(owned));
        if (!owned && vehicle_model < 0)
            for (size_t k = 0; k < vm_assets.size() && k < vm_ids.size(); k++) if (vm_assets[k] == assets[i]) { vehicle_asset = assets[i]; vehicle_model = vm_ids[k]; break; }
    }
    j += fmt("],\"rewardVehicleAsset\":%d,\"rewardVehicleModel\":%d}", vehicle_asset, vehicle_model);
    return j;
}
}  // namespace

std::string world_command(Machine& m, const std::string& cmd, std::string* error) {
    uint32_t mgr = SERIES_MANAGER, ev = rd(m, mgr + 0x18c), G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0;
    static const char* formats[] = {"Moto", "Heat", "SingleEvent", "Freestyle", "Overtake", "Eliminator", "SpeednStyle", "Outrun", "Faceoff", "SectionRace"};   // MXSeriesFormat names, 0x009d2380 (WP-V2)
    if (cmd == "status") {
        uint32_t format = rd(m, mgr + 0x10);
        return fmt("{\"owner\":%u,\"active\":%u,\"series\":%d,\"format\":\"%s\",\"event\":%u,\"raceState\":%d,\"rulesHold\":%u,\"allowRestart\":%d",
                   rd(m, mgr + 0x190), rd(m, mgr + 0x188), int(rd(m, mgr + 0x20)), format < 10 ? formats[format] : "?", ev ? 1u : 0u,
                   A ? int(rd(m, A + 0x21c)) : -1, A ? rd(m, A + 0xff068) : 0u, world_ui_int(m, "SeriesAllowRestart", 0))
             + ",\"rulesType\":\"" + jesc(world_ui_string(m, "RulesType")) + "\",\"status\":\"" + jesc(world_ui_string(m, "SeriesEventStatus")) + "\"}";
    }
    int v = 0;
    if (std::sscanf(cmd.c_str(), "rules_done %d", &v) == 1) {
        // RulesPopup: OnToggleKeypress ("don't show again": MXTableHelper:SetPlayerEventStatus( device, "Rule_" .. RulesType, TRUE ),
        // 004eabd0: EAX device, ECX name, stack(value), ret 4), then End: App.PreActivityRulesPageDone (00742a00: the activity's rules
        // hold +0xff068 = 0; the race leaves Pre).
        Scratch sc(m);
        if (v) { std::string rule = "Rule_" + world_ui_string(m, "RulesType"); uint32_t one = 1; m.call_function(0x4eabd0, &one, 1, sc.str(rule.c_str()), 0); }
        m.call_function(0x742a00, nullptr, 0);
        return "{}";
    }
    if (cmd == "results" || cmd == "moto_results") {
        // PostEventResults:Start: App.PopulateResultsTable (00742c40) or, with ShowMotoResults (the PostEventMotoResults page),
        // App.PopulateSeriesMotoResultsTable (00742ce0); the page shows the table "Activity Results" (00ae18c0: Index, Name, Column0..2;
        // row 0 the column titles). The series' standings are processed first (post_results).
        std::string status = post_results(m);
        m.call_function(cmd == "results" ? 0x742c40 : 0x742ce0, nullptr, 0);
        return "{\"status\":\"" + jesc(status) + "\",\"table\":" + table_json(m, 0xae18c0) + "}";
    }
    if (std::sscanf(cmd.c_str(), "outcome %d", &v) == 1) {
        auto o = race_outcome(m, uint32_t(v));
        if (o.empty()) { if (error) *error = "no race outcome (no activity)"; return ""; }
        std::string h; for (uint32_t w : o) h += fmt("%08x", w);
        return h;
    }
    if (error) *error = "unknown world command: " + cmd;
    return "";
}

std::string finish_event(Machine& m, const std::string& remote, std::string* error) {
    install_remote_storage(m, remote);
    persist::stats_received(m);   // the Steam user's stats (CompleteSeries unlocks the series highlights / achievements)
    uint32_t mgr = SERIES_MANAGER, ev = rd(m, mgr + 0x18c);
    uint32_t G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0;
    static const char* formats[] = {"Moto", "Heat", "SingleEvent", "Freestyle", "Overtake", "Eliminator", "SpeednStyle", "Outrun", "Faceoff", "SectionRace"};   // MXSeriesFormat names, 0x009d2380 (WP-V2)
    uint32_t format = rd(m, mgr + 0x10);
    std::string j = fmt("{\"owner\":%u,\"active\":%u,\"series\":%d,\"format\":\"%s\"", rd(m, mgr + 0x190), rd(m, mgr + 0x188), int(rd(m, mgr + 0x20)),
                        format < 10 ? formats[format] : "?");
    if (rd(m, mgr + 0x190) != 1 || !ev) { if (error) *error = "not a career (tour) event"; return j + "}"; }
    if (!A || rd(m, A + 0x21c) < 4) { if (error) *error = "the race is not over (race state Post)"; return j + "}"; }
    // 1. The results (post_results).
    std::string status = post_results(m);
    j += ",\"status\":\"" + jesc(status) + "\"";
    // 2. PostEventOverlay:OnPostStateComplete (tour): Moto / Heat with SeriesEventStatus "HeatOrLCQ" go on to the next sub event in the same
    //    world (not here); otherwise unless "Failed", seriesManager:CompleteSeriesEvent( ) (005b41b0, ESI = manager); then SwitchToFrontEnd.
    if (status == "HeatOrLCQ" || status == "GateSelection") { if (error) *error = "the series continues with its next race (a world built by career::start_series_event with this race's outcome)"; return j + "}"; }
    if (status != "Failed") { m.r[6] = mgr; m.call_function(0x5b41b0, nullptr, 0); }
    //    CompleteSeriesEvent sends GotoRewardsPage (PostEventRewards: progress:GetRewards( )) when the manager has rewards (+0x174 /
    //    +0x178 / +0x17c), else GotoCareerRaceAndRidePage; read here before CompleteSeries applies them.
    int page = status != "Failed" && (rd(m, mgr + 0x174) || rd(m, mgr + 0x178) || rd(m, mgr + 0x17c)) ? 1 : 0;
    j += ",\"rewards\":" + rewards_json(m, page);
    // 3. CareerFrontEndLoadingPage:DoneLoading (no reward vehicle; a vehicle reward goes through RewardVehiclePresentation first, which
    //    ends the same way): seriesManager:CompleteSeries( ) (005b4310, EAX = manager: PlayerSeriesStatus, unlocks, the save request
    //    005a6910), seriesManager:EnableTourMode( ).
    //    The activity was left first (SwitchToFrontEnd: PlayerLeft commits the race's PlayerRecords working row, persist.h).
    int committed = persist::commit_records(m);
    m.call_function(0x5b4310, nullptr, 0, -1, mgr);
    m.mem.write(mgr + 0x190, 4, 1);
    // 4. The saved-game worker's write of that request (SavedGameManager_Process 005a76c0: take the slot's document, write it with
    //    SavedGameManager_WriteSaveGame 005a6690 (ECX = document) through SteamCloudOutputStream -> FileWrite, destroy it).
    uint32_t slot = SAVED_GAMES + 0x10, doc = rd(m, slot + 0x8);
    int saved = -1;
    if (doc) {
        m.mem.write(slot + 0x8, 4, 0);
        saved = int(m.call_function(0x5a6690, nullptr, 0, doc));
        uint32_t vt = rd(m, doc); uint32_t one = 1; m.call_function(rd(m, vt), &one, 1, doc);   // scalar deleting destructor
        m.mem.write(SAVED_GAMES + 0x1d8, 4, 0);
    } else if (error) *error = "the game did not request a save (no profile storage or nothing changed)";
    persist::flush_stats(m);   // MXGame::Update's StoreStats of the achievements set above
    j += fmt(",\"saved\":%d,\"recordsCommitted\":%d,\"profile\":", saved, committed) + profile_json(m, false) + "}";
    return j;
}
}  // namespace career
}  // namespace reflex_vm

// ---------------------------------------------------------------------------------------------------- C API (reflex_world.h)
namespace {
struct ProfileSession { std::unique_ptr<reflex_vm::Boot> boot; std::string error, log, stopped; std::mutex lock; };
std::string profile_open_error;
int copy_out(const std::string& r, char* out, int32_t size) {
    if (!out || size <= int32_t(r.size())) return -int(r.size()) - 1;
    std::memcpy(out, r.c_str(), r.size() + 1); return int(r.size());
}
}  // namespace

extern "C" {
void* reflex_profile_open(const char* game_dir, const char* crt_dir, const char* save_dir) {
    auto* p = new ProfileSession;
    try {
        reflex_vm::BootConfig c; c.game_dir = game_dir; c.crt_dir = crt_dir; c.save_dir = save_dir; c.jit = !std::getenv("REFLEX_NO_JIT");
        c.remote_storage = std::string(save_dir) + "/remote"; c.steam_persona = reflex_vm::career::persona(save_dir);
        c.log_path = std::string(save_dir) + "/profile-boot.log";
        reflex_vm::client_boot_config(c);   // baton scheduling and the client's ports (boot.h, reflex_world_boot_ports)
        p->boot = std::make_unique<reflex_vm::Boot>(c);
        reflex_vm::install_client_boot_ports(*p->boot, c);
        p->boot->start();
        if (!reflex_vm::career::sign_in(*p->boot, &p->log)) throw reflex_vm::VmError("the profile did not sign in:\n" + p->log);
        reflex_vm::persist::stats_received(*p->boot);   // the Steam user's stats for MXHighlightTracking (persist.h)
        reflex_vm::client_coverage_write(p->boot->machine(), "profile-open");   // REFLEX_CLIENT_COVERAGE (boot.h): boot and sign-in; then the commands
        profile_open_error.clear();
        return p;
    } catch (const std::exception& e) { profile_open_error = e.what(); if (p->boot) reflex_vm::client_coverage_forget(p->boot->machine()); delete p; return nullptr; }
}
int reflex_profile_command(void* profile, const char* command, char* out, int32_t size) {
    auto* p = static_cast<ProfileSession*>(profile);
    if (!p || !command) return -1;
    std::lock_guard<std::mutex> g(p->lock);
    if (!p->stopped.empty()) { p->error = "the profile session stopped: " + p->stopped; return -1; }   // reopen it
    try {
        std::string r;
        bool ok = reflex_vm::career::command(*p->boot, command, &r);
        reflex_vm::persist::flush_stats(p->boot->machine());   // MXGame::Update's StoreStats for achievements a command unlocked
        if (!ok) { p->error = r.empty() ? std::string("command failed: ") + command : r; return -1; }
        return copy_out(r, out, size);
    } catch (const std::exception& e) { p->error = p->stopped = e.what(); return -1; }   // the booted game is not usable after a fault
}
const char* reflex_profile_error(void* profile) { return profile ? static_cast<ProfileSession*>(profile)->error.c_str() : profile_open_error.c_str(); }
void reflex_profile_close(void* profile) {
    auto* p = static_cast<ProfileSession*>(profile);
    if (p && p->boot) { reflex_vm::client_coverage_write(p->boot->machine(), "profile-commands"); reflex_vm::client_coverage_forget(p->boot->machine()); reflex_vm::persist::forget(p->boot->machine()); }
    delete p;
}
}  // extern "C"
