// FMOD Ex / FMOD Event stand-ins (audio is not reproduced; the physics never reads it).
// Every call succeeds (FMOD_OK); setters are remembered per object so the matching
// getters read them back; events report PLAYING between start() and stop(); created
// objects get unique handles. Stack sizes are the stdcall argument bytes incl. `this`.
{
    struct FmodState { std::map<std::pair<uint32_t, std::string>, uint32_t> props; std::map<uint32_t, uint32_t> playing; uint32_t next_handle = 0xfb000000; };
    auto fm = std::make_shared<FmodState>();
    s->fmod_snapshot = [fm]() { auto copy = std::make_shared<FmodState>(*fm); return std::function<void()>([fm, copy]() { *fm = *copy; }); };
    auto key = [](const std::string& full) { return full.substr(full.find('!') + 1); };
    (void)key;
    struct Spec { const char* name; uint32_t bytes; int kind; const char* prop; };
    // kind: 0 setter (remember arg1 as prop), 1 no-op, 2 bool/int getter of prop (out = arg1),
    //       3 new handle into arg(index given by prop[0]-'0'), 4 event start, 5 event stop, 6 event getState,
    //       7 zero-fill outputs: prop = list of (arg index,size) pairs as "i:size,i:size"
    static const Spec specs[] = {
        {"fmod_eventl.dll!?setVolume@Event@FMOD@@QAG?AW4FMOD_RESULT@@M@Z", 8, 0, "volume"},
        {"fmod_eventl.dll!?setPaused@Event@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z", 8, 0, "paused"},
        {"fmod_eventl.dll!?getPaused@Event@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z", 8, 2, "paused"},
        {"fmod_eventl.dll!?setMute@Event@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z", 8, 0, "mute"},
        {"fmod_eventl.dll!?getMute@Event@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z", 8, 2, "mute"},
        {"fmod_eventl.dll!?set3DOcclusion@Event@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z", 12, 1, ""},
        {"fmod_eventl.dll!?set3DAttributes@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@00@Z", 16, 1, ""},
        {"fmod_eventl.dll!?setReverbProperties@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_REVERB_CHANNELPROPERTIES@@@Z", 8, 1, ""},
        {"fmod_eventl.dll!?getReverbProperties@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_CHANNELPROPERTIES@@@Z", 8, 7, "1:24"},
        {"fmod_eventl.dll!?setPropertyByIndex@Event@FMOD@@QAG?AW4FMOD_RESULT@@HPAX_N@Z", 16, 1, ""},
        {"fmod_eventl.dll!?getPropertyByIndex@Event@FMOD@@QAG?AW4FMOD_RESULT@@HPAX_N@Z", 16, 7, "2:4"},
        {"fmod_eventl.dll!?getNumParameters@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z", 8, 7, "1:4"},
        {"fmod_eventl.dll!?getParameterByIndex@Event@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAVEventParameter@2@@Z", 12, 3, "2"},
        {"fmod_eventl.dll!?getParameter@Event@FMOD@@QAG?AW4FMOD_RESULT@@PBDPAPAVEventParameter@2@@Z", 12, 3, "2"},
        {"fmod_eventl.dll!?setCallback@Event@FMOD@@QAG?AW4FMOD_RESULT@@P6G?AW43@PAUFMOD_EVENT@@W4FMOD_EVENT_CALLBACKTYPE@@PAX22@Z2@Z", 12, 1, ""},
        {"fmod_eventl.dll!?getInfo@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAHPAPAD@Z", 12, 7, "1:4,2:4"},
        {"fmod_eventl.dll!?getRange@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@PAM0@Z", 12, 7, "1:4,2:4"},
        {"fmod_eventl.dll!?setValue@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@M@Z", 8, 0, "value"},
        {"fmod_eventl.dll!?keyOff@EventParameter@FMOD@@QAG?AW4FMOD_RESULT@@XZ", 4, 1, ""},
        {"fmod_eventl.dll!?start@Event@FMOD@@QAG?AW4FMOD_RESULT@@XZ", 4, 4, ""},
        {"fmod_eventl.dll!?stop@Event@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z", 8, 5, ""},
        {"fmod_eventl.dll!?getInfo@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAHPAPADPAUFMOD_EVENT_INFO@@@Z", 16, 7, "1:4,2:4"},
        {"fmod_eventl.dll!?getState@Event@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z", 8, 6, ""},
        {"fmodexl.dll!?set3DNumListeners@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z", 8, 1, ""},
        {"fmodexl.dll!?set3DListenerAttributes@System@FMOD@@QAG?AW4FMOD_RESULT@@HPBUFMOD_VECTOR@@000@Z", 24, 1, ""},
        {"fmodexl.dll!?createSound@System@FMOD@@QAG?AW4FMOD_RESULT@@PBDIPAUFMOD_CREATESOUNDEXINFO@@PAPAVSound@2@@Z", 20, 3, "4"},
        {"fmodexl.dll!?playSound@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_CHANNELINDEX@@PAVSound@2@_NPAPAVChannel@2@@Z", 20, 3, "4"},
        {"fmodexl.dll!?setLoopCount@Sound@FMOD@@QAG?AW4FMOD_RESULT@@H@Z", 8, 1, ""},
        {"fmodexl.dll!?getNumSubSounds@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAH@Z", 8, 7, "1:4"},
        {"fmodexl.dll!?set3DMinMaxDistance@Sound@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z", 12, 1, ""},
        {"fmodexl.dll!?release@Sound@FMOD@@QAG?AW4FMOD_RESULT@@XZ", 4, 1, ""},
        {"fmodexl.dll!?getSubSound@Sound@FMOD@@QAG?AW4FMOD_RESULT@@HPAPAV12@@Z", 12, 3, "2"},
        {"fmodexl.dll!?getOpenState@Sound@FMOD@@QAG?AW4FMOD_RESULT@@PAW4FMOD_OPENSTATE@@PAIPA_N@Z", 16, 7, "1:4,2:4,3:1"},
        {"fmodexl.dll!?stop@Channel@FMOD@@QAG?AW4FMOD_RESULT@@XZ", 4, 5, ""},
        {"fmodexl.dll!?setPaused@Channel@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z", 8, 0, "paused"},
        {"fmodexl.dll!?setVolume@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z", 8, 0, "volume"},
        {"fmodexl.dll!?setFrequency@Channel@FMOD@@QAG?AW4FMOD_RESULT@@M@Z", 8, 0, "frequency"},
        {"fmodexl.dll!?set3DAttributes@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@0@Z", 12, 1, ""},
        {"fmodexl.dll!?set3DMinMaxDistance@Channel@FMOD@@QAG?AW4FMOD_RESULT@@MM@Z", 12, 1, ""},
        {"fmodexl.dll!?isPlaying@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z", 8, 8, ""},
        {"fmodexl.dll!?isVirtual@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PA_N@Z", 8, 7, "1:1"},
        {"fmodexl.dll!?setVolume@SoundGroup@FMOD@@QAG?AW4FMOD_RESULT@@M@Z", 8, 0, "volume"},
        {"fmodexl.dll!?setMute@ChannelGroup@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z", 8, 0, "mute"},
        {"fmodexl.dll!?release@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@XZ", 4, 1, ""},
        {"fmodexl.dll!?set3DAttributes@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_VECTOR@@MM@Z", 16, 1, ""},
        {"fmodexl.dll!?setProperties@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@PBUFMOD_REVERB_PROPERTIES@@@Z", 8, 1, ""},
        {"fmodexl.dll!?setActive@Reverb@FMOD@@QAG?AW4FMOD_RESULT@@_N@Z", 8, 0, "active"},
        {"fmodexl.dll!?setReverbAmbientProperties@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_REVERB_PROPERTIES@@@Z", 8, 1, ""},
        {"fmodexl.dll!?createReverb@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVReverb@2@@Z", 8, 3, "1"},
        {"fmodexl.dll!?set3DSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@MMM@Z", 16, 1, ""},
    };
    // Fake FMOD objects. Every handle the stand-ins hand out (0xfb000000 + 16 n) is a
    // readable object whose vtable traps with the slot number; the EventSystem
    // stand-in (EVENT_SYSTEM_OBJECT) implements every slot with its exact stdcall
    // cleanup (from the fmod_eventL.dll C wrappers).
    constexpr uint32_t HANDLES = 0xfb000000, EVENT_SYSTEM_OBJECT = 0xfa000000, EVENT_SYSTEM_VTABLE = 0xfa001000,
                       EVENT_SYSTEM_CODE = 0xfa002000, UNKNOWN_VTABLE = 0xfa003000, UNKNOWN_CODE = 0xfa004000;
    {
        std::vector<uint32_t> table(0x100000 / 4);
        for (size_t i = 0; i < table.size(); i += 4) table[i] = UNKNOWN_VTABLE;
        m.mem.map(HANDLES, reinterpret_cast<uint8_t*>(table.data()), 0x100000, true);
        std::vector<uint32_t> unknown(256); for (uint32_t i = 0; i < 256; i++) unknown[i] = UNKNOWN_CODE + 4 * i;
        m.mem.map(UNKNOWN_VTABLE, reinterpret_cast<uint8_t*>(unknown.data()), 1024, true);
        for (uint32_t i = 0; i < 256; i++) m.natives[UNKNOWN_CODE + 4 * i] = [i](Machine& m) -> uint32_t {
            char b[128]; std::snprintf(b, sizeof b, "unmodelled FMOD virtual call: object %08x slot +0x%x (return %08x)", m.arg(0) ? 0u : 0u, 4 * i, uint32_t(m.mem.read(m.r[4], 4)));
            (void)m; throw VmError(b);
        };
        uint32_t object[4] = {EVENT_SYSTEM_VTABLE, 0, 0, 0};
        m.mem.map(EVENT_SYSTEM_OBJECT, reinterpret_cast<uint8_t*>(object), 16, true);
        std::vector<uint32_t> vt(64, 0);
        for (auto& e : EVENT_SYSTEM_SLOTS) {
            uint32_t code = EVENT_SYSTEM_CODE + e.slot; vt[e.slot / 4] = code;
            std::string nm = e.name; uint32_t bytes = e.bytes;
            m.natives[code] = [fm, nm, bytes](Machine& m) -> uint32_t {
                // object-returning getters: the output pointer is the last argument
                if (nm.find("GetSystemObject") != std::string::npos || nm.find("GetMusicSystem") != std::string::npos ||
                    nm.find("GetProject") != std::string::npos || nm.find("GetCategory") != std::string::npos ||
                    nm.find("GetMusicCategory") != std::string::npos || nm.find("GetGroup") != std::string::npos ||
                    nm.find("GetEvent") != std::string::npos || nm.find("CreateReverb") != std::string::npos) {
                    uint32_t out = m.arg(bytes / 4 - 1);
                    if (out) { m.mem.write(out, 4, fm->next_handle); fm->next_handle += 16; }
                }
                return m.ret(bytes, 0);
            };
        }
        m.mem.map(EVENT_SYSTEM_VTABLE, reinterpret_cast<uint8_t*>(vt.data()), 256, true);
        m.mem.write(FMOD_EVENT_SYSTEM_GLOBAL, 4, EVENT_SYSTEM_OBJECT);
    }
    for (const Spec& sp : specs) {
        Spec spec = sp; std::string name = spec.name;
        m.imports[name] = [fm, spec, name](Machine& m) -> uint32_t {
            uint32_t self = m.arg(0);
            switch (spec.kind) {
                case 0: fm->props[{self, spec.prop}] = m.arg(1); break;
                case 1: break;
                case 2: m.mem.write(m.arg(1), 1, fm->props.count({self, spec.prop}) ? (fm->props[{self, spec.prop}] & 0xff) : 0); break;
                case 3: { uint32_t out = m.arg(spec.prop[0] - '0'); m.mem.write(out, 4, fm->next_handle); fm->next_handle += 16; break; }
                case 4: fm->playing[self] = 1; break;
                case 5: fm->playing.erase(self); break;
                case 6: m.mem.write(m.arg(1), 4, fm->playing.count(self) ? 0x19u : 0x1u); break;   // READY | PLAYING | CHANNELSACTIVE, or READY
                case 7: {
                    for (const char* p = spec.prop; *p;) {
                        int idx = *p - '0'; p += 2; int size = std::atoi(p); while (*p && *p != ',') p++; if (*p == ',') p++;
                        std::vector<uint8_t> z(size, 0); uint32_t out = m.arg(idx); if (out) m.mem.write_bytes(out, z.data(), z.size());
                    }
                    break; }
                case 8: m.mem.write(m.arg(1), 1, fm->playing.count(self) ? 1 : 0); break;
            }
            return m.ret(spec.bytes, 0);   // FMOD_OK
        };
    }
}
