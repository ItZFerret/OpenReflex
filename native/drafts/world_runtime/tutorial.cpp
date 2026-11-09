// The MotoSkills tutorials (WP-T): see tutorial.h, _re/notes/tutorials.md.
#include "tutorial.h"
#include "modes.h"
#include "pages.h"
#include "career.h"
#include "vm.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

namespace reflex_vm {
namespace tutorial {
namespace {
constexpr uint32_t GAME = 0xa946b8;                 // G (MXGame)
constexpr uint32_t UI_MANAGER = 0xa93fcc;           // MXUIManager (also [G+0x745c])
constexpr uint32_t PAGE_STACK_VT = 0x9516b4;

std::string cname(Boot& b, uint32_t c) { char n[48] = {}; if (c) b.machine().mem.read_bytes(c + 0x10, n, 47); return n; }
std::vector<uint32_t> graphs(Boot& b) {   // the UI manager's UIPageGraphContainer ([G+0x745c]+0xd8) children (modes.cpp page_graphs)
    std::vector<uint32_t> r; uint32_t G = b.read32(GAME), ui = G ? b.read32(G + 0x745c) : 0, c = ui ? b.read32(ui + 0xd8) : 0;
    if (!c) return r;
    for (uint32_t p = b.read32(c + 0xcc), e = b.read32(c + 0xd0); p && p < e && r.size() < 32; p += 4) r.push_back(b.read32(p));
    return r;
}
uint32_t graph(Boot& b, const char* name) { for (uint32_t g : graphs(b)) if (cname(b, g) == name) return g; return 0; }
// the top of the popup stack (UIPageStack: std::list at +0xf0, head node +0x104, node {next, prev, page @+8})
std::string top_popup(Boot& b) {
    uint32_t g = graph(b, "PopupPageStack.pag"); if (!g || b.read32(g) != PAGE_STACK_VT) return "";
    uint32_t head = b.read32(g + 0x104), top = 0;
    for (uint32_t n = b.read32(head), k = 0; n && n != head && k < 16; n = b.read32(n), k++) top = b.read32(n + 8);
    return top ? cname(b, top) : "";
}
void lua(Boot& b, const std::string& src) {   // the UI scripts' Lua state (wrapper 00d94938): FUN_00744920 loads the chunk, FUN_00744e70 calls it
    static int serial = 0; std::string name = "ReflexTutorial" + std::to_string(++serial);
    Boot::GuestCall c; c.fn = 0x744920; c.ecx = b.guest_string(name); c.args = {0xd94938, b.guest_string(src), uint32_t(src.size())}; b.call(c);
    Boot::GuestCall d; d.fn = 0x744e70; d.args = {0xd94938, b.guest_string(name + "_Chunk"), b.guest_string("")}; b.call(d);
    { std::string rm = name + "_Chunk = nil"; Boot::GuestCall e; e.fn = 0x744920; e.ecx = b.guest_string(name + "R");
        e.args = {0xd94938, b.guest_string(rm), uint32_t(rm.size())}; b.call(e);
        Boot::GuestCall f; f.fn = 0x744e70; f.args = {0xd94938, b.guest_string(name + "R_Chunk"), b.guest_string("")}; b.call(f); }
}
// UIComponent_SetStateBit 005c9950: EAX on, ESI component, stack bit
void set_state(Boot& b, uint32_t comp, int bit, bool on) { Boot::GuestCall s; s.fn = 0x5c9950; s.eax = on ? 1 : 0; s.esi = comp; s.args = {uint32_t(bit)}; b.call(s); }
}  // namespace

bool start(Boot& b, const Boot::EventSpec& e, uint32_t max_frames, std::string* out) {
    auto note = [&](const std::string& x) { if (out) *out += x + "\n"; };
    auto state = [&]() { uint32_t g = b.read32(0xd728d8); return g ? int(b.read32(g + 0x18)) : -1; };
    uint32_t frames = 0; char t[256];
    auto frame = [&](uint32_t n = 1) { for (uint32_t i = 0; i < n; i++) { b.run_frames(1, 0); frames++; if (b.progress) b.progress(frames, state()); } };
    while (state() != 2) { if (frames > max_frames) { note("front end not reached"); return false; } frame(); }
    frame(30);   // the front end settles (profile-login UI event)
    // baton scheduling: the front end's timed steps need 5 s virtual (Boot::settle_front_end; without them FrontEndPageGraph is not made yet)
    if (b.baton()) while (b.now() < 50000000ull && frames <= max_frames) frame();
    // the front end as Boot::start_event has it: logical focus (WM_SETFOCUS through the window procedure 00880dc0), the no-storage sign-in
    // (MXProfile_SignIn 006166a0), EnsureLocalPlayers (0059e030), SetPrimaryPlayerDeviceIndex(0) (00743030), the title page's exclusive
    // device 0 (FUN_0059a910: 009c4ee0, its count 00a94070, UI ExclusiveDeviceIndex; MainMenu's Lua looks the player's tables up with it)
    if (!b.read32(0xa9596c)) { Boot::GuestCall f; f.fn = 0x880dc0; f.args = {0x10010, 7, 0, 0}; b.call(f); }
    uint32_t manager = b.read32(0xd92ee0), profile = manager ? b.read32(manager + 4) : 0, ui = b.read32(UI_MANAGER);
    if (!profile || !ui) { note("no profile / UI manager"); return false; }
    Boot::GuestCall signin; signin.fn = 0x6166a0; signin.eax = profile; signin.edi = 0;
    std::snprintf(t, sizeof t, "front end after %u frames; sign-in -> %u", frames, b.call(signin)); note(t); frame();
    Boot::GuestCall ensure; ensure.fn = 0x59e030; ensure.args = {0, uint32_t(e.activity)}; b.call(ensure); frame();
    Boot::GuestCall primary; primary.fn = 0x743030; primary.eax = 0; b.call(primary);
    b.machine().mem.write(0x9c4ee0, 4, 0); b.machine().mem.write(0xa94070, 4, b.read32(0xa94070) + 1);
    { Boot::GuestCall s; s.fn = 0x5dbfb0; s.ecx = ui; s.args = {b.guest_string("ExclusiveDeviceIndex"), 0}; b.call(s); }
    frame(2);
    // the Main Menu Page, where MOTOSKILLS is pressed: the start-up graphs (the boot never left the publisher / legal pages) lose focus and
    // visibility, FrontEndPageGraph goes to it (UIPageGraph::vf50 005ddcd0: page, edge 0, 1, 1) with focus; MainMenu:Start / Present run
    for (uint32_t g : graphs(b)) {
        std::string n = cname(b, g);
        if (n == "FrontEndIntroPageGraph.pag" || n == "IntroPageGraph.pag") for (int bit : {0, 4}) if (b.read32(g + 0xb4) & (1u << bit)) set_state(b, g, bit, false);
    }
    uint32_t fe = graph(b, "FrontEndPageGraph.pag");
    if (!fe) { note("no FrontEndPageGraph"); return false; }
    { Boot::GuestCall go; go.fn = b.read32(b.read32(fe) + 50 * 4); go.ecx = fe; go.args = {b.guest_string("Main Menu Page"), 0, 1, 1}; b.call(go); }
    set_state(b, fe, 4, true); set_state(b, fe, 0, true);
    frame(10);
    std::snprintf(t, sizeof t, "Main Menu Page: %s, popup '%s'", cname(b, b.read32(fe + 0xe8)).c_str(), top_popup(b).c_str()); note(t);
    // MainMenu:Present shows TutorialFirstTimeEntryPopup while PlayerRider.TutorialsEntered is FALSE (the boot's default profile): the
    // client shows that popup itself before it starts a tutorial world, so it is put away here (its End marks the default profile only)
    if (top_popup(b) == "TutorialFirstTimeEntryPopup") { lua(b, "App.SendUIEvent( \"HideTutorialFirstTimeEntryPopup\" )"); frame(5); note("TutorialFirstTimeEntryPopup hidden"); }
    // MOTOSKILLS: the main menu's own handler (MainMenu.lua OnTutorialKeypress: PlayerMode, the equipped vehicle kept in UI
    // TutorialOwnerVehicleId / TutorialVehicleOwner, ChangeVehicle( row, 2 ) MX Lite, SyncDatabaseWithManagers, EnableTutorialMode( TRUE ),
    // GotoLoadingPage, PendingHUBPopup ShowTutorialIntroPopup)
    lua(b, "MainMenu:OnTutorialKeypress( )");
    note("MainMenu:OnTutorialKeypress");
    uint32_t ingame = graph(b, "InGamePageGraph.pag"), G = b.read32(GAME);
    int last_page = -1;
    while (frames < max_frames) {
        frame();
        uint32_t page = ingame ? b.read32(ingame + 0xe8) : 0, V = b.read32(G + 0x7460 + 0x1ac);
        std::string pn = page ? cname(b, page) : "";
        int now = pn == "Freeride Single Player Page" ? 1 : 0;
        if (now != last_page) { std::snprintf(t, sizeof t, "frame %u: InGame page '%s', vehicle %08x, TutorialMode %u", frames, pn.c_str(), V, b.read32(ui + 0x298)); note(t); last_page = now; }
        if (now && V && (b.read32(ingame + 0xb4) & 0x11) == 0x11) {
            // the Freeride page (Tutorial Load Page's RoutedGotoInGameHUB): the world image is written here; the hub's pending popup
            // (TutorialIntroductionPopup) starts in the world's first frame
            // the loads the page and the vehicle started finish first (a worker's database request in the world's first frame stops it):
            // 60 frames as Boot::start_event runs after its world is up; the hub's TutorialIntroductionPopup is up by then
            frame(60);
            std::snprintf(t, sizeof t, "tutorial world running: frame %u, UI world %08x, TutorialManager %08x", frames, b.read32(G + 0x20c8), b.read32(b.read32(G + 0x20c8) + 0x22180)); note(t);
            return true;
        }
    }
    note("the tutorial did not start in time"); return false;
}

// ------------------------------------------------------------------------------------------------ the world
namespace {
std::string vehicle_note;
std::map<const Machine*, uint32_t> save_frames;
constexpr uint32_t SAVED_GAMES = 0xdce458;   // SavedGameManager (career.md, persist.cpp)
uint32_t rd(Machine& m, uint32_t a) { return a >= 0x10000 && m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; }
std::string cstr(Machine& m, uint32_t p, size_t max) {
    std::string o; for (size_t i = 0; p >= 0x10000 && i < max && m.mem.mapped(p + uint32_t(i)); i++) { char c = char(m.mem.read(p + uint32_t(i), 1)); if (!c) break; o += c; } return o;
}
std::string jesc(const std::string& s) { std::string o; for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += uint8_t(c) < 0x20 ? ' ' : c; } return o; }
// the popup on top of the PopupPageStack, as ui_frame sees the page graphs
std::string top_popup(Machine& m) {
    std::vector<UiLayer> L; std::vector<UiNode> N; std::vector<UiAnim> A; std::string T;
    ui_frame(m, L, N, A, T);
    std::string top; for (auto& l : L) if (!std::strcmp(l.graph, "PopupPageStack.pag")) top = l.page;
    return top;
}
}  // namespace

std::string world_command(Machine& m, const std::string& cmd, std::string* error) {
    uint32_t G = rd(m, GAME), W = G ? rd(m, G + 0x20c8) : 0, TM = W ? rd(m, W + 0x22180) : 0, ui = rd(m, UI_MANAGER);
    if (cmd == "attach") {
        ui_attach(m);
        pages::voice(m);   // installs the localized voice hooks (UILocSound_Play / Stop)
        { std::string e; pages::world_command(m, "com_stubs", &e); }   // the boot's COM resources' AddRef / Release (PreTutorialPopup's SetMaterial)
        // StopTutorialAudio (binding 0072a0b0) and the manager's own stops (00752740, 007527c0, 00752920): 0080c880 cdecl(owner) stops the
        // owner's playing requests of the localized sound list [0xa957d4]; UILocSound_Stop's hook does not see it
        m.natives[0x80c880] = [](Machine& mm) -> uint32_t { pages::voice_note(mm, "stop " + cstr(mm, rd(mm, mm.r[4] + 4), 64)); return RUN_ORIGINAL; };
        // UILocSound_Play (0068dd40, thiscall manager, stack(database, package, event path, priority, owner), ret 0x14): recorded as the
        // voice hook records it, the original not run: the request's package (AudioLocalizedTutorial "Audio") would be loaded through the
        // database loader, which a world does not service (its SetEvent from the manager's update 0068dea0 stops the world); the client plays
        // the voice itself
        m.natives[0x68dd40] = [](Machine& mm) -> uint32_t {
            uint32_t sp = mm.r[4];
            pages::voice_note(mm, "play " + cstr(mm, rd(mm, sp + 4), 64) + "|" + cstr(mm, rd(mm, sp + 8), 64) + "|" + cstr(mm, rd(mm, sp + 12), 260) + "|" + cstr(mm, rd(mm, sp + 20), 64));
            return mm.ret(0x14);
        };
        // Bink (binkw32.dll), as the boot has it (boot_devices.inc: movies are not played; BinkOpen 0): PreTutorialPopup's "Tutorial Video"
        // (UIVideoComponent ChangeVideoPath "Videos\\Tutorial<n>", Play) opens no video in the world; the client plays that file itself
        // through the install's binkw32.dll (bink.cpp) from the component's path
        m.imports["binkw32.dll!_BinkOpen@8"] = [](Machine& mm) { return mm.ret(8, 0); };
        for (auto [f, n] : std::vector<std::pair<const char*, uint32_t>>{{"_BinkSetSoundOnOff@8", 8}, {"_BinkWait@4", 4}, {"_BinkNextFrame@4", 4}, {"_BinkGetFrameBuffersInfo@8", 8},
                                                                        {"_BinkShouldSkip@4", 4}, {"_BinkPause@8", 8}, {"_BinkSetVolume@12", 12}, {"_BinkClose@4", 4},
                                                                        {"_BinkDoFrame@4", 4}, {"_BinkGoto@12", 12}, {"_BinkRegisterFrameBuffers@8", 8}, {"_BinkGetRects@8", 8}}) {
            std::string k = std::string("binkw32.dll!") + f;
            if (!m.imports.count(k)) { uint32_t b = n; m.imports[k] = [b](Machine& mm) { return mm.ret(b, 0); }; }
        }
        // MainMenu:OnTutorialKeypress's vehicle part again, on the player's tables (the client's persistence attach loaded the save after the
        // boot ran it on the default profile): the vehicle the front end had equipped (EnsureLocalPlayers: PlayerRider.LastEquippedVehicle,
        // owner Player1 - what GetEquippedVehicle answers there) kept in UI TutorialOwnerVehicleId / TutorialVehicleOwner for
        // TutorialPauseMenu:OnQuit / TutorialConclusionPopup:End to equip again, and ChangeVehicle( row, 2 ) (the player's MX Lite)
        {
            std::string err, v = pages::world_command(m, "lua_ui ReflexTutorialVehicle local d = App.GetExclusiveDeviceIndex( ); local h = App.GetMXTableHelper( ); "
                "local row = h:GetPlayerTableRow( d ):rowIndex( ); local last = App.LookupPlayerTable( \"PlayerRider\", d ):cell_int( 0, \"LastEquippedVehicle\" ); "
                "App.RemoveUIVariable( \"TutorialOwnerVehicleId\" ); App.RemoveUIVariable( \"TutorialVehicleOwner\" ); "
                "if ( last ~= -1 ) then App.SetUIVariableInt( \"TutorialOwnerVehicleId\", last ); App.SetUIVariableInt( \"TutorialVehicleOwner\", 0 ); end "
                "App.ChangeVehicle( row, 2 ); App.SetUIVariableString( \"ReflexTutorialVehicle\", last .. \" \" .. App.LookupPlayerTable( \"PlayerRider\", d ):cell_int( 0, \"LastEquippedVehicle\" ) )", &err);
            vehicle_note = v;
        }
        std::string r = "attached";
        if (top_popup(m) == "TutorialIntroductionPopup") {   // its Start's request was made in the boot (before the image was written)
            pages::voice_note(m, "play AudioLocalizedTutorial|Audio|TUTORIAL_INTRO/VO/INTRO|Tutorial Introduction Popup");
            r += " intro";
        }
        r += " vehicle " + vehicle_note;
        return r;
    }
    if (cmd == "state") {
        char b[512];
        uint32_t vid = 0; std::string path; int vstate = -1;
        {   // the shown PreTutorialPopup's "Tutorial Video" (the popup stack's page, as the live mirror sees it)
            std::vector<UiLayer> L; std::vector<UiNode> N; std::vector<UiAnim> A; std::string T;
            ui_frame(m, L, N, A, T);
            for (auto& l : L) if (!std::strcmp(l.page, "PreTutorialPopup"))
                for (int k = l.first_node; k < l.first_node + l.nodes; k++) if (!std::strcmp(N[size_t(k)].name, "Tutorial Video")) vid = N[size_t(k)].object;
            if (vid) { path = cstr(m, vid + 0x270, 0x30); vstate = int(rd(m, vid + 0x2ac)); }
        }
        std::string err, current = pages::world_command(m, "ui_int CurrentTutorial", &err);
        std::snprintf(b, sizeof b, "{\"mode\":%d,\"manager\":%u,\"off\":%d,\"tutorial\":%d,\"state\":%d,\"step\":%d,\"success\":%d,\"current\":%s,\"video\":{\"object\":%u,\"path\":\"%s\",\"state\":%d},\"music\":%d}",
                      int(rd(m, ui + 0x298)), TM, TM ? int(rd(m, TM)) : -1, TM ? int(rd(m, TM + 4)) : -1, TM ? int(rd(m, TM + 8)) : -1, TM ? int(rd(m, TM + 0x7c)) : -1,
                      TM ? int(rd(m, TM + 0x64)) : -1, current.empty() ? "-1" : current.c_str(), vid, jesc(path).c_str(), vstate, G ? int(rd(m, G + 0x15c6c + 8)) : -1);
        return b;
    }
    if (cmd == "progress") {
        return pages::world_command(m, "lua_ui ReflexTutorialProgress local t = App.LookupPlayerTable( \"PlayerRider\", App.GetExclusiveDeviceIndex( ) ); "
                                       "App.SetUIVariableString( \"ReflexTutorialProgress\", t:cell_int( 0, \"TutorialsCompleted\" ) .. \" \" .. t:cell_int( 0, \"TutorialsEntered\" ) )", error);
    }
    if (cmd == "frame") {
        // The saved-game worker (SavedGameManager_ThreadProc 005a7f10: SavedGameManager_Process 005a76c0 every 1000 ms; never woken in a world).
        // A save the page Lua requested (App.RequestSaveGame: TutorialPauseMenu:Start after a lesson, :OnQuit, TutorialConclusionPopup:End;
        // SavedGameManager_RequestSave 005a6910 queues the document in the manager 00dce458's controller slot +0x18 and sends "Game Save
        // Notify": the Save Popup) is written by the worker's own pass once 60 frames (its 1000 ms period, also the popup's minimum time
        // 009cf5b0) went by: "Game Save", WriteSaveGame through the Steam Cloud stand-in, "Game Saved" (the popup hides). Its Sleep for the
        // minimum display time has nothing left to wait for (the time already passed).
        std::string o;
        if (rd(m, SAVED_GAMES + 0x18)) {
            uint32_t& n = save_frames[&m];
            if (++n >= 60) {
                n = 0;
                // for the pass only: Sleep returns at once, and the storage device's name (PCStorageDevice::vf1 006d4680, for the "Game Save"
                // message) reads SteamFriends()->GetPersonaName, so the offline Steam user's interfaces stand in (career.h install_steam_user;
                // left installed, the world's sign-in manager would sign in again from them)
                const char* keep[] = {"kernel32.dll!Sleep", "steam_api.dll!SteamUser", "steam_api.dll!SteamFriends"};
                std::vector<std::pair<bool, Native>> was;
                for (const char* k : keep) { auto it = m.imports.find(k); was.push_back(it == m.imports.end() ? std::make_pair(false, Native()) : std::make_pair(true, it->second)); }
                auto restore = [&]() { for (size_t i = 0; i < 3; i++) { if (was[i].first) m.imports[keep[i]] = was[i].second; else m.imports.erase(keep[i]); } };
                m.imports["kernel32.dll!Sleep"] = [](Machine& mm) { return mm.ret(4, 0); };
                install_steam_user(m, "Player");
                uint32_t a = SAVED_GAMES, esp = m.r[4];
                try { m.call_function(0x5a76c0, &a, 1); } catch (...) { restore(); m.r[4] = esp; throw; }
                restore(); m.r[4] = esp;
                o += "Saved\n";
            }
        } else save_frames[&m] = 0;
        return o;
    }
    if (cmd == "objects") {
        // The lessons' own scene objects ("TUT0<n>_...", "TUT03_Trajectory_<step>_..."), as TutorialManager_ShowLessonObjects (00751c40)
        // finds them: the UI world's scene ([0xa95844]+0x1c) entities of types 4 and 0x1078 (Scene_EntitiesOfType 007d2920: EAX scene,
        // stack(type, list, max 0x32, 0)), each shown (+0xa2 = 1) only for the running lesson (and step). Lines "<entity id> <shown> <name>".
        uint32_t core = rd(m, 0xa95844), scene = core ? rd(m, core + 0x1c) : 0;
        if (!scene) { if (error) *error = "no UI world scene"; return ""; }
        constexpr uint32_t LIST = 0xee600000;   // a scratch page of our own (never part of an image)
        if (!m.mem.mapped(LIST)) m.mem.map(LIST, nullptr, 4096);
        std::string o;
        for (uint32_t type : {4u, 0x1078u}) {
            m.mem.write_block(LIST, nullptr, 0x100);
            uint32_t a[4] = {type, LIST, 0x32, 0}, esp = m.r[4];
            int n = int(m.call_function(0x7d2920, a, 4, -1, scene)); m.r[4] = esp;
            for (int i = 0; i < n && i < 0x32; i++) {
                uint32_t e = rd(m, LIST + 4 * uint32_t(i)); if (!e) continue;
                std::string name = cstr(m, rd(m, e + 0x18), 96);
                if (name.compare(0, 3, "TUT") != 0) continue;   // 0096d2ac "TUT", then the lesson number (atoi)
                o += std::to_string(rd(m, e + 0x14)) + " " + std::to_string(m.mem.read(e + 0xa2, 1) & 0xff) + " " + name + "\n";
            }
        }
        return o;
    }
    if (cmd == "test_pass") {
        // tests only: the running lesson's own outcome as its check reports a pass (+0x64 = 1, then 007520a0: the completion
        // VO, TutorialsCompleted (004e8460), the highlights, ShowPostTutorialPopup, the next lesson)
        if (!TM || rd(m, TM + 8) != 2) { if (error) *error = "no lesson running"; return ""; }
        m.mem.write(TM + 0x64, 4, 1); uint32_t a = TM, esp = m.r[4]; m.call_function(0x7520a0, &a, 1); m.r[4] = esp;
        return "passed " + std::to_string(rd(m, TM + 4));
    }
    if (error) *error = "unknown tutorial command: " + cmd;
    return "";
}

namespace {
struct ScriptLine { uint32_t frame; std::string verb, rest; };
std::vector<ScriptLine> script;
uint8_t script_keys[256];
struct Tap { uint32_t dik, until; };
std::vector<Tap> taps;
uint32_t finish_until = 0, finish_next = 0;   // "finish": retry a failed lesson / judge it passed, until this frame
// "drive <ride frames> [<pass 0|1>]": the reactive lesson driver: the game's own pages answered as a player would (CONTINUE, the pause menu's
// next locked-free lesson, RETRY once per lesson), each lesson ridden with its key pattern for <ride frames>, then (pass 1) judged passed if
// it still runs (the verification stimulus "pass"); until the TutorialConclusionPopup is accepted
struct Drive { bool on = false; uint32_t ride0 = 600, ride = 600, quiet = 0, lesson_start = 0, retries = 0; int lesson = -1; bool pass = true, done = false; int menu_step = -1; uint32_t menu_at = 0; };
Drive drive;
}  // namespace
void script_load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) { std::printf("REFLEX_TUT_SCRIPT: cannot read %s\n", path.c_str()); return; }
    char line[4096];
    while (std::fgets(line, sizeof line, f)) {
        std::string s(line); while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        char* e = nullptr; uint32_t fr = uint32_t(std::strtoul(s.c_str(), &e, 10));
        std::string rest = e; size_t p = rest.find_first_not_of(' '); rest = p == std::string::npos ? "" : rest.substr(p);
        size_t sp = rest.find(' ');
        script.push_back({fr, rest.substr(0, sp), sp == std::string::npos ? "" : rest.substr(sp + 1)});
    }
    std::fclose(f);
    std::memset(script_keys, 0, sizeof script_keys);
}
void script_frame(Boot& b, uint32_t frame) {
    for (auto& l : script) {
        if (l.frame != frame) continue;
        if (l.verb == "lua") { lua(b, l.rest); std::printf("tut %u: lua %s\n", frame, l.rest.c_str()); }
        else if (l.verb == "keys") {
            std::memset(script_keys, 0, sizeof script_keys);
            for (const char* p = l.rest.c_str(); *p;) { char* e = nullptr; uint32_t k = uint32_t(std::strtoul(p, &e, 16)); if (e == p) break; script_keys[k & 255] = 0x80; p = *e ? e + 1 : e; }
            std::printf("tut %u: keys %s\n", frame, l.rest.c_str());
        } else if (l.verb == "tap") {
            char* e = nullptr; uint32_t k = uint32_t(std::strtoul(l.rest.c_str(), &e, 16)); uint32_t n = *e ? uint32_t(std::atoi(e)) : 3;
            taps.push_back({k & 255, frame + (n ? n : 3)}); std::printf("tut %u: tap %02x\n", frame, k);
        } else if (l.verb == "mgr") {   // the manager's lesson words and the local vehicle's body-0 position (sim +0x120)
            Machine& m = b.machine();
            uint32_t G = b.read32(0xa946b8), W = G ? b.read32(G + 0x20c8) : 0, TM = W ? b.read32(W + 0x22180) : 0;
            auto f = [&](uint32_t a) { uint32_t v = b.read32(a); float x; std::memcpy(&x, &v, 4); return x; };
            uint32_t V = G ? b.read32(G + 0x7460 + 0x1ac) : 0, S = V ? b.read32(V + 0x234) : 0, P = G + 0x7460;
            std::printf("tut %u: mgr lesson %d state %d step %d pass %d bad %d good %d wreck %d bound %d land %d t84 %.2f t88 %.2f lean %.2f/%.2f air %d knock %d nobound %d | pos %.1f %.1f %.1f reset %.2f wreckt %.2f trick '%s'\n", frame,
                        int(b.read32(TM + 4)), int(b.read32(TM + 8)), int(b.read32(TM + 0x7c)), int(b.read32(TM + 0x64)), int(b.read32(TM + 0x68)), int(b.read32(TM + 0x6c)),
                        int(b.read32(TM + 0x70)), int(b.read32(TM + 0x74)), int(b.read32(TM + 0x80)), f(TM + 0x84), f(TM + 0x88), f(TM + 0x8c), f(TM + 0x90),
                        int(b.read32(TM + 0x98)), int(b.read32(TM + 0x9c)), int(b.read32(TM + 0x18)), S ? f(S + 0xf0) : 0, S ? f(S + 0xf4) : 0, S ? f(S + 0xf8) : 0,
                        f(P + 0x116c), f(P + 0xb38), b.read32(P + 0xb74) ? cstr(m, b.read32(P + 0xb74), 40).c_str() : "");
        } else if (l.verb == "drive") {
            drive.on = true; char* e = nullptr; drive.ride = uint32_t(std::strtoul(l.rest.c_str(), &e, 10)); if (!drive.ride) drive.ride = 600; drive.ride0 = drive.ride;
            if (e && *e) drive.pass = std::atoi(e) != 0;
        } else if (l.verb == "finish") { finish_until = frame + uint32_t(std::atoi(l.rest.c_str())); finish_next = frame;
        } else if (l.verb == "pass") {   // verification stimulus: the running lesson judged as passed (+0x64 = 1, TutorialManager_Outcome 007520a0)
            uint32_t G = b.read32(0xa946b8), W = G ? b.read32(G + 0x20c8) : 0, TM = W ? b.read32(W + 0x22180) : 0;
            if (TM && b.read32(TM + 8) == 2 && b.read32(TM) == 0) {
                b.machine().mem.write(TM + 0x64, 4, 1);
                Boot::GuestCall c; c.fn = 0x7520a0; c.args = {TM}; b.call(c);
                std::printf("tut %u: pass (lesson now %u)\n", frame, b.read32(TM + 4));
            } else std::printf("tut %u: pass: no lesson running\n", frame);
        } else if (l.verb == "triggers") {   // the UI scene's MXTriggers (0x1028): name, translation (+0x220), axes length (+0x230), matrix axes
            Machine& m = b.machine();
            uint32_t core = b.read32(0xa95844), scene = core ? b.read32(core + 0x1c) : 0;
            constexpr uint32_t LIST = 0xee600000;
            if (!m.mem.mapped(LIST)) m.mem.map(LIST, nullptr, 4096);
            m.mem.write_block(LIST, nullptr, 0x100);
            uint32_t a[4] = {0x1028, LIST, 0x32, 0}, esp = m.r[4];
            int n = int(m.call_function(0x7d2920, a, 4, -1, scene)); m.r[4] = esp;
            auto f = [&](uint32_t x) { uint32_t v = b.read32(x); float y; std::memcpy(&y, &v, 4); return y; };
            for (int i = 0; i < n && i < 0x32; i++) {
                uint32_t e = b.read32(LIST + 4 * uint32_t(i));
                std::printf("trigger %s pos %.1f %.1f %.1f | x %.1f %.1f %.1f | y %.1f %.1f %.1f | z %.1f %.1f %.1f\n", cstr(m, b.read32(e + 0x18), 64).c_str(), f(e + 0x60), f(e + 0x64), f(e + 0x68),
                            f(e + 0x30), f(e + 0x34), f(e + 0x38), f(e + 0x40), f(e + 0x44), f(e + 0x48), f(e + 0x50), f(e + 0x54), f(e + 0x58));
            }
        } else if (l.verb == "state" || l.verb == "objects") {
            std::string err; std::string r = world_command(b.machine(), l.verb, &err);
            std::printf("tut %u: %s %s popup '%s'%s\n", frame, l.verb.c_str(), r.c_str(), top_popup(b).c_str(), err.c_str());
        }
    }
    if (drive.on && !drive.done && frame >= drive.quiet) {
        std::string top = top_popup(b);
        uint32_t G = b.read32(0xa946b8), W = G ? b.read32(G + 0x20c8) : 0, TM = W ? b.read32(W + 0x22180) : 0;
        uint32_t ingame = graph(b, "InGamePageGraph.pag"), page = ingame ? b.read32(ingame + 0xe8) : 0;
        std::string pn = page ? cname(b, page) : "";
        auto tap = [&](uint32_t k, const char* what) { taps.push_back({k, frame + 3}); drive.quiet = frame + 40; std::printf("tut %u: drive: %s\n", frame, what); };
        if (top == "TutorialIntroductionPopup" || top == "PreTutorialPopup" || top == "PostTutorialPopup") {
            std::memset(script_keys, 0, sizeof script_keys);
            if (top == "PreTutorialPopup") {
                const int now = TM ? int(b.read32(TM + 4)) : -1;
                if (now != drive.lesson) { drive.retries = 0; drive.ride = drive.ride0; }
                drive.lesson = now; drive.lesson_start = 0;
            }
            tap(0x1c, top.c_str());
        } else if (top == "TutorialConclusionPopup") { tap(0x1c, top.c_str()); drive.done = true; }
        else if (top == "FailedTutorialPopup") {
            std::memset(script_keys, 0, sizeof script_keys);
            drive.retries++; drive.lesson_start = 0;
            if (drive.retries > 1) drive.ride = 30;   // after one retry: judged passed at once
            tap(0x1c, "FailedTutorialPopup RETRY");
        } else if (top.empty() && pn == "Tutorial Pause Menu Page") {
            // the pause menu: the first lesson not completed (TutorialsCompleted, the menu's own unlock count) is the next
            std::string err, v = pages::world_command(b.machine(), "lua_ui ReflexTutorialNext App.SetUIVariableInt( \"ReflexTutorialNext\", App.LookupPlayerTable( \"PlayerRider\", App.GetExclusiveDeviceIndex( ) ):cell_int( 0, \"TutorialsCompleted\" ) )", &err);
            int next = std::atoi(v.c_str()); if (next > 5) next = 5;
            if (drive.menu_step < 0) { drive.menu_step = 0; drive.menu_at = frame + 20; }
            if (frame >= drive.menu_at) {
                if (drive.menu_step < next) { taps.push_back({0xd0, frame + 3}); drive.menu_step++; drive.menu_at = frame + 12; std::printf("tut %u: drive: menu down (%d / %d)\n", frame, drive.menu_step, next); }
                else { tap(0x1c, "menu accept"); drive.menu_step = -1; }
            }
        } else if (top.empty() && TM && b.read32(TM) == 0 && b.read32(TM + 8) == 2) {
            if (!drive.lesson_start) drive.lesson_start = frame;
            const uint32_t t = frame - drive.lesson_start;
            const int L = int(b.read32(TM + 4));
            std::memset(script_keys, 0, sizeof script_keys);
            auto hold = [&](std::initializer_list<int> ks) { for (int k : ks) script_keys[k] = 0x80; };
            switch (L) {   // the lessons' key patterns (W throttle, arrows lean, A / D steer, space trick modifier)
            case 0: if (t < 240) { static const int seq[4] = {0xc8, 0xd0, 0xcb, 0xcd}; script_keys[seq[t / 60]] = 0x80; } break;
            case 1: {   // REFLEX_TUT_L2="throttle on,off,lean on,off,lean from" (frames)
                static int pat[5] = {1, 0, 1, 0, 60}; static bool rd = false;
                if (!rd) { rd = true; if (const char* e = std::getenv("REFLEX_TUT_L2")) std::sscanf(e, "%d,%d,%d,%d,%d", &pat[0], &pat[1], &pat[2], &pat[3], &pat[4]); }
                if (pat[1] == 0 || int(t) % (pat[0] + pat[1]) < pat[0]) hold({0x11});
                if (int(t) >= pat[4] && (pat[3] == 0 || (int(t) - pat[4]) % (pat[2] + pat[3]) < pat[2])) hold({0xd0});
                break; }
            case 2: hold({0x11}); if (t > 100 && t < 260) hold({0x1e, 0xcb}); if (t > 300 && t < 420) hold({0x20, 0xcd}); break;
            case 3: hold({0x11}); if ((t / 40) % 3 == 2) hold({0xd0}); break;
            case 4: hold({0x11}); if (t > 200 && t < 260) hold({0xcb}); break;
            default: hold({0x11}); if (t > 150 && t < 220) hold({0x39, 0xc8}); break;
            }
            if (t >= drive.ride) {
                std::memset(script_keys, 0, sizeof script_keys);
                if (drive.pass) {
                    if (L == 3) {   // TAKE-OFFS AND LANDINGS: a good jump as its check counts one (+0x6c, the step +0x7c on)
                        b.machine().mem.write(TM + 0x6c, 4, b.read32(TM + 0x6c) + 1);
                        b.machine().mem.write(TM + 0x7c, 4, b.read32(TM + 0x7c) + 1);
                    }
                    b.machine().mem.write(TM + 0x64, 4, 1);
                    Boot::GuestCall c; c.fn = 0x7520a0; c.args = {TM}; b.call(c);
                    std::printf("tut %u: drive: lesson %d judged passed (stimulus)\n", frame, L);
                }
                drive.lesson_start = 0; drive.quiet = frame + 30;
            }
        }
    }
    if (frame < finish_until && frame >= finish_next) {
        std::string top = top_popup(b);
        uint32_t G = b.read32(0xa946b8), W = G ? b.read32(G + 0x20c8) : 0, TM = W ? b.read32(W + 0x22180) : 0;
        if (top == "FailedTutorialPopup") { taps.push_back({0x1c, frame + 3}); finish_next = frame + 60; std::printf("tut %u: finish: RETRY\n", frame); }
        else if (top.empty() && TM && b.read32(TM + 8) == 2 && b.read32(TM) == 0) {
            b.machine().mem.write(TM + 0x64, 4, 1);
            Boot::GuestCall c; c.fn = 0x7520a0; c.args = {TM}; b.call(c);
            std::printf("tut %u: finish: pass (lesson now %u)\n", frame, b.read32(TM + 4));
            finish_until = 0;
        }
    }
    uint8_t k[256]; std::memcpy(k, script_keys, 256);
    for (auto& t : taps) if (frame < t.until) k[t.dik] = 0x80;
    b.set_keys(k);
}
}  // namespace tutorial
}  // namespace reflex_vm
