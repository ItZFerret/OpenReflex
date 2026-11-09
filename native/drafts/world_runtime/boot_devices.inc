// Device stand-ins of the boot process (included by boot.cpp inside namespace reflex_vm).
// Direct3D 9: a recording device. Every resource is memory-backed (Lock returns emulated
// memory, contents kept for the client), shaders and declarations keep their bytecode;
// nothing is drawn. DirectInput: the keyboard reads the host's key states.

namespace {
// D3D9 interface method tables: name, stdcall bytes incl. this.
using Methods = std::vector<std::pair<const char*, uint32_t>>;
const Methods IUNKNOWN = {{"QueryInterface", 12}, {"AddRef", 4}, {"Release", 4}};
Methods with(const Methods& base, const Methods& more) { Methods m = base; m.insert(m.end(), more.begin(), more.end()); return m; }
const Methods D3D9 = with(IUNKNOWN, {{"RegisterSoftwareDevice", 8}, {"GetAdapterCount", 4}, {"GetAdapterIdentifier", 16}, {"GetAdapterModeCount", 12},
    {"EnumAdapterModes", 20}, {"GetAdapterDisplayMode", 12}, {"CheckDeviceType", 24}, {"CheckDeviceFormat", 28}, {"CheckDeviceMultiSampleType", 28},
    {"CheckDepthStencilMatch", 24}, {"CheckDeviceFormatConversion", 20}, {"GetDeviceCaps", 16}, {"GetAdapterMonitor", 8}, {"CreateDevice", 28}});
const Methods RESOURCE = with(IUNKNOWN, {{"GetDevice", 8}, {"SetPrivateData", 20}, {"GetPrivateData", 16}, {"FreePrivateData", 8}, {"SetPriority", 8},
    {"GetPriority", 4}, {"PreLoad", 4}, {"GetType", 4}});
const Methods BASETEX = with(RESOURCE, {{"SetLOD", 8}, {"GetLOD", 4}, {"GetLevelCount", 4}, {"SetAutoGenFilterType", 8}, {"GetAutoGenFilterType", 4}, {"GenerateMipSubLevels", 4}});
const Methods TEXTURE = with(BASETEX, {{"GetLevelDesc", 12}, {"GetSurfaceLevel", 12}, {"LockRect", 20}, {"UnlockRect", 8}, {"AddDirtyRect", 8}});
const Methods CUBETEX = with(BASETEX, {{"GetLevelDesc", 12}, {"GetCubeMapSurface", 16}, {"LockRect", 24}, {"UnlockRect", 12}, {"AddDirtyRect", 12}});
const Methods VOLTEX = with(BASETEX, {{"GetLevelDesc", 12}, {"GetVolumeLevel", 12}, {"LockBox", 20}, {"UnlockBox", 8}, {"AddDirtyBox", 8}});
const Methods SURFACE = with(RESOURCE, {{"GetContainer", 12}, {"GetDesc", 8}, {"LockRect", 16}, {"UnlockRect", 4}, {"GetDC", 8}, {"ReleaseDC", 8}});
const Methods BUFFER = with(RESOURCE, {{"Lock", 20}, {"Unlock", 4}, {"GetDesc", 8}});
const Methods SHADER = with(IUNKNOWN, {{"GetDevice", 8}, {"GetFunction", 12}});
const Methods DECL = with(IUNKNOWN, {{"GetDevice", 8}, {"GetDeclaration", 12}});
const Methods QUERY = with(IUNKNOWN, {{"GetDevice", 8}, {"GetType", 4}, {"GetDataSize", 4}, {"Issue", 8}, {"GetData", 16}});
const Methods STATEBLOCK = with(IUNKNOWN, {{"GetDevice", 8}, {"Capture", 4}, {"Apply", 4}});
const Methods SWAPCHAIN = with(IUNKNOWN, {{"Present", 24}, {"GetFrontBufferData", 8}, {"GetBackBuffer", 16}, {"GetRasterStatus", 8}, {"GetDisplayMode", 8},
    {"GetDevice", 8}, {"GetPresentParameters", 8}});
const Methods DINPUT = with(IUNKNOWN, {{"CreateDevice", 16}, {"EnumDevices", 20}, {"GetDeviceStatus", 8}, {"RunControlPanel", 12}, {"Initialize", 12},
    {"FindDevice", 16}, {"EnumDevicesBySemantics", 24}, {"ConfigureDevices", 20}});
const Methods DIDEVICE = with(IUNKNOWN, {{"GetCapabilities", 8}, {"EnumObjects", 16}, {"GetProperty", 12}, {"SetProperty", 12}, {"Acquire", 4}, {"Unacquire", 4},
    {"GetDeviceState", 12}, {"GetDeviceData", 20}, {"SetDataFormat", 8}, {"SetEventNotification", 8}, {"SetCooperativeLevel", 12}, {"GetObjectInfo", 16},
    {"GetDeviceInfo", 8}, {"RunControlPanel", 12}, {"Initialize", 16}, {"CreateEffect", 20}, {"EnumEffects", 16}, {"GetEffectInfo", 12},
    {"GetForceFeedbackState", 8}, {"SendForceFeedbackCommand", 8}, {"EnumCreatedEffectObjects", 16}, {"Escape", 8}, {"Poll", 4}, {"SendDeviceData", 20},
    {"EnumEffectsInFile", 20}, {"WriteEffectToFile", 20}, {"BuildActionMap", 16}, {"SetActionMap", 16}, {"GetImageInfo", 8}});
const Methods CONSTTABLE = with(IUNKNOWN, {{"GetBufferPointer", 4}, {"GetBufferSize", 4}, {"GetDesc", 8}, {"GetConstantDesc", 16}, {"GetSamplerIndex", 8},
    {"GetConstant", 12}, {"GetConstantByName", 12}, {"GetConstantElement", 12}, {"SetDefaults", 8}, {"SetValue", 20}, {"SetBool", 16}, {"SetBoolArray", 20},
    {"SetInt", 16}, {"SetIntArray", 20}, {"SetFloat", 16}, {"SetFloatArray", 20}, {"SetVector", 16}, {"SetVectorArray", 20}, {"SetMatrix", 16},
    {"SetMatrixArray", 20}, {"SetMatrixPointerArray", 20}, {"SetMatrixTranspose", 16}, {"SetMatrixTransposeArray", 20}, {"SetMatrixTransposePointerArray", 20}});

uint32_t format_bits(uint32_t f) {
    switch (f) {
        case 20: return 24; case 21: case 22: case 31: case 32: case 33: case 34: case 35: case 63: case 64: case 67: case 71: case 75: case 77: case 79: case 114: return 32;
        case 23: case 24: case 25: case 26: case 29: case 30: case 51: case 60: case 80: case 81: case 111: case 70: case 73: return 16;
        case 28: case 50: case 41: case 52: return 8;
        case 36: case 112 + 1: case 115: case 110: return 64;
        case 116: return 128;
        case 112: return 32;
        default: return 32;
    }
}
bool format_dxt(uint32_t f, uint32_t* block) {
    if (f == 0x31545844u) { *block = 8; return true; }                                        // DXT1
    if (f == 0x32545844u || f == 0x33545844u || f == 0x34545844u || f == 0x35545844u) { *block = 16; return true; }   // DXT2..5
    if (f == 0x31495441u) { *block = 8; return true; }                                        // ATI1
    if (f == 0x32495441u) { *block = 16; return true; }                                       // ATI2
    return false;
}
}  // namespace

struct Iface { std::string name; Methods methods; uint32_t vtable = 0; };
struct Res {
    enum Kind { Device, Texture, Cube, Volume, Surface, VB, IB, VS, PS, Decl, Query, StateBlock, SwapChain, D3D, DInput, DIDevice, ConstTable, Generic } kind = Generic;
    uint32_t refs = 1;
    uint32_t width = 0, height = 0, depth = 1, levels = 1, format = 0, usage = 0, pool = 0, fvf = 0, size = 0, type = 0;
    std::vector<uint32_t> level_addr, level_pitch, level_bytes, level_w, level_h;   // per level (cube: 6 * levels)
    std::vector<uint32_t> surfaces;                                                  // surface objects of levels (lazy)
    uint32_t data = 0;                                                               // buffers / surfaces
    uint32_t container = 0, level = 0, face = 0;
    std::vector<uint8_t> bytes;                                                      // shader bytecode / declaration
    uint32_t version = 0;                                                            // bumped on unlock
    std::string label;
};


void Boot::Impl::install_devices() {
    Impl* s = this;
    struct DevState {
        std::map<uint32_t, std::shared_ptr<Res>> res;   // object address -> resource
        std::map<std::string, std::unique_ptr<Iface>> ifaces;
        uint32_t device = 0, backbuffer = 0, depth = 0, swapchain = 0, d3d = 0;
        std::map<uint64_t, uint32_t> state;              // generic device state: (method << 32 | key) -> value
        std::vector<uint8_t> caps;
        uint64_t presents = 0, draws = 0;
        uint32_t viewport[6] = {0, 0, 1280, 720, 0, 0x3f800000};
        uint32_t render_targets[4] = {}, textures[32] = {}, ds = 0;
        uint64_t epoch = 0;                              // native/game Verify: the undo epoch of the last scalar snapshot
    };
    auto ds = std::make_shared<DevState>();
    // native/game Verify inside the boot (boot.cpp, notes/loading.md): the device's scalars once per undo epoch, a resource's mutable
    // fields before each change, created resources and remembered render state undone with the guest memory
    auto touch_ds = [s, ds]() {
        if (!s->logging() || ds->epoch == s->undo_epoch) return;
        ds->epoch = s->undo_epoch;
        struct Sc { uint32_t device, backbuffer, depth, swapchain, d3d; uint64_t presents, draws; uint32_t viewport[6], render_targets[4], textures[32], ds; };
        Sc v{ds->device, ds->backbuffer, ds->depth, ds->swapchain, ds->d3d, ds->presents, ds->draws, {}, {}, {}, ds->ds};
        std::memcpy(v.viewport, ds->viewport, sizeof v.viewport); std::memcpy(v.render_targets, ds->render_targets, sizeof v.render_targets); std::memcpy(v.textures, ds->textures, sizeof v.textures);
        s->on_undo([ds, v] { ds->device = v.device; ds->backbuffer = v.backbuffer; ds->depth = v.depth; ds->swapchain = v.swapchain; ds->d3d = v.d3d; ds->presents = v.presents; ds->draws = v.draws;
            std::memcpy(ds->viewport, v.viewport, sizeof v.viewport); std::memcpy(ds->render_targets, v.render_targets, sizeof v.render_targets); std::memcpy(ds->textures, v.textures, sizeof v.textures); ds->ds = v.ds; });
    };
    auto touch_res = [s, ds](uint32_t o) {
        if (!s->logging()) return;
        auto it = ds->res.find(o); if (it == ds->res.end()) return;
        std::shared_ptr<Res> r = it->second; uint32_t refs = r->refs, version = r->version; std::vector<uint32_t> surfaces = r->surfaces;
        s->on_undo([r, refs, version, surfaces] { r->refs = refs; r->version = version; r->surfaces = surfaces; });
    };
    // One vtable of natives per interface; each native dispatches to `call(self, method)`.
    using Call = std::function<uint32_t(Machine&, uint32_t self, const std::string& method, uint32_t bytes)>;
    auto iface = [s, ds, touch_ds](const std::string& name, const Methods& methods, Call call) -> Iface* {
        auto it = ds->ifaces.find(name); if (it != ds->ifaces.end()) return it->second.get();
        auto f = std::make_unique<Iface>(); f->name = name; f->methods = methods;
        std::vector<uint32_t> vt;
        for (auto& [mname, bytes] : methods) {
            std::string mn = mname; uint32_t b = bytes;
            vt.push_back(s->native([s, call, mn, b, name, touch_ds](Machine& m) -> uint32_t {
                touch_ds();
                if (!s->d3d_hook) return call(m, m.arg(0), mn, b);
                uint32_t self = m.arg(0); s->d3d_hook(m, name, self, mn, false);   // RENDER CAPTURE (render_capture.cpp)
                uint32_t next = call(m, self, mn, b); s->d3d_hook(m, name, self, mn, true); return next; }));
        }
        f->vtable = s->halloc(s->process_heap, uint32_t(vt.size() * 4), true);
        s->m.mem.write_block(f->vtable, vt.data(), vt.size() * 4);
        Iface* p = f.get(); ds->ifaces[name] = std::move(f); return p;
    };
    auto make = [s, ds](Iface* f, std::shared_ptr<Res> r) -> uint32_t {
        uint32_t o = s->halloc(s->process_heap, 32, true);
        s->m.mem.write(o, 4, f->vtable); ds->res[o] = std::move(r); s->on_undo([ds, o] { ds->res.erase(o); }); return o;
    };
    auto unknown = [ds, touch_res](Machine& m, uint32_t self, const std::string& method, uint32_t bytes) -> uint32_t {
        auto it = ds->res.find(self); Res* r = it == ds->res.end() ? nullptr : it->second.get();
        if (r && (method == "AddRef" || method == "Release" || method == "QueryInterface")) touch_res(self);
        if (method == "AddRef") { if (r) r->refs++; return m.ret(4, r ? r->refs : 1); }
        if (method == "Release") { uint32_t n = r && r->refs ? --r->refs : 0; return m.ret(4, n); }
        if (method == "QueryInterface") { m.mem.write(m.arg(2), 4, self); if (r) r->refs++; return m.ret(12, 0); }
        if (method == "GetDevice") { m.mem.write(m.arg(1), 4, ds->device); return m.ret(8, 0); }
        if (method == "SetPrivateData" || method == "FreePrivateData" || method == "PreLoad") return m.ret(bytes, 0);
        if (method == "GetPrivateData") return m.ret(bytes, 0x88760866u);   // D3DERR_NOTFOUND
        if (method == "SetPriority" || method == "GetPriority") return m.ret(bytes, 0);
        return RUN_ORIGINAL;
    };
    // ---------------------------------------------------------------- resources
    Iface* surface_if = nullptr; Iface* texture_if = nullptr; Iface* cube_if = nullptr; Iface* vol_if = nullptr;
    Iface* vb_if = nullptr; Iface* ib_if = nullptr; Iface* vs_if = nullptr; Iface* ps_if = nullptr; Iface* decl_if = nullptr;
    Iface* query_if = nullptr; Iface* sb_if = nullptr; Iface* sc_if = nullptr;
    auto alloc_data = [s](uint32_t bytes) { return s->halloc(DEVICE_HEAP, std::max<uint32_t>(bytes, 16), false); };
    auto lock_rect = [s](Machine& m, Res& r, uint32_t idx, uint32_t out, uint32_t rect) {
        uint32_t addr = r.level_addr[idx], pitch = r.level_pitch[idx];
        if (rect) {
            int32_t x = int32_t(m.mem.read(rect, 4)), y = int32_t(m.mem.read(rect + 4, 4)); uint32_t blk;
            if (format_dxt(r.format, &blk)) addr += uint32_t(y / 4) * pitch + uint32_t(x / 4) * blk;
            else addr += uint32_t(y) * pitch + uint32_t(x) * format_bits(r.format) / 8;
        }
        m.mem.write(out, 4, pitch); m.mem.write(out + 4, 4, addr); (void)s;
    };
    auto surface_desc = [](Machine& m, const Res& r, uint32_t idx, uint32_t out) {
        uint32_t v[8] = {r.format, 1 /*SURFACE*/, r.usage, r.pool, 0, 0, r.level_w.empty() ? r.width : r.level_w[idx], r.level_h.empty() ? r.height : r.level_h[idx]};
        m.mem.write_block(out, v, 32);
    };
    std::function<uint32_t(uint32_t, uint32_t, uint32_t)> surface_of;   // (container, level index, face) -> surface object
    Call surface_call = [s, ds, unknown, lock_rect, surface_desc, touch_res](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        Res& r = *ds->res.at(self);
        Res* c = r.container ? ds->res.at(r.container).get() : nullptr;
        if (mt == "GetType") return m.ret(4, 1);
        if (mt == "GetDesc") { if (c) surface_desc(m, *c, r.level, m.arg(1)); else surface_desc(m, r, 0, m.arg(1)); return m.ret(8, 0); }
        if (mt == "LockRect") { if (c) lock_rect(m, *c, r.level, m.arg(1), m.arg(2)); else lock_rect(m, r, 0, m.arg(1), m.arg(2)); return m.ret(16, 0); }
        if (mt == "UnlockRect") { touch_res(c ? r.container : self); (c ? c : &r)->version++; return m.ret(4, 0); }
        if (mt == "GetContainer") { m.mem.write(m.arg(2), 4, r.container ? r.container : ds->device); return m.ret(12, 0); }
        if (mt == "GetDC") return m.ret(8, 0x8876086cu);
        if (mt == "ReleaseDC") return m.ret(8, 0);
        (void)s; throw VmError("IDirect3DSurface9::" + mt);
    };
    surface_if = iface("Surface", SURFACE, surface_call);
    surface_of = [s, ds, surface_if, make, touch_res](uint32_t container, uint32_t idx, uint32_t face) -> uint32_t {
        touch_res(container);
        Res& c = *ds->res.at(container);
        if (c.surfaces.size() < c.level_addr.size()) c.surfaces.resize(c.level_addr.size(), 0);
        if (!c.surfaces[idx]) {
            auto r = std::make_shared<Res>(); r->kind = Res::Surface; r->container = container; r->level = idx; r->face = face; r->refs = 1;
            c.surfaces[idx] = make(surface_if, r);
        } else { touch_res(c.surfaces[idx]); ds->res.at(c.surfaces[idx])->refs++; }
        (void)s; return c.surfaces[idx];
    };
    auto standalone_surface = [s, ds, surface_if, make, alloc_data](uint32_t w, uint32_t h, uint32_t fmt, uint32_t usage, uint32_t pool, const std::string& label) {
        auto r = std::make_shared<Res>(); r->kind = Res::Surface; r->width = w; r->height = h; r->format = fmt; r->usage = usage; r->pool = pool; r->label = label;
        uint32_t blk, pitch, bytes;
        if (format_dxt(fmt, &blk)) { pitch = std::max(1u, (w + 3) / 4) * blk; bytes = pitch * std::max(1u, (h + 3) / 4); }
        else { pitch = std::max(1u, w * format_bits(fmt) / 8); pitch = (pitch + 3) & ~3u; bytes = pitch * h; }
        r->level_addr = {alloc_data(bytes)}; r->level_pitch = {pitch}; r->level_bytes = {bytes}; r->level_w = {w}; r->level_h = {h};
        (void)s; return make(surface_if, r);
    };
    Call texture_call = [s, ds, unknown, lock_rect, surface_desc, surface_of, touch_res](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        Res& r = *ds->res.at(self);
        bool cube = r.kind == Res::Cube;
        if (mt == "GetType") return m.ret(4, cube ? 5 : r.kind == Res::Volume ? 4 : 3);
        if (mt == "GetLevelCount") return m.ret(4, r.levels);
        if (mt == "SetLOD" || mt == "GetLOD") return m.ret(bytes, 0);
        if (mt == "SetAutoGenFilterType" || mt == "GenerateMipSubLevels") return m.ret(bytes, 0);
        if (mt == "GetAutoGenFilterType") return m.ret(4, 2);
        if (mt == "GetLevelDesc") { surface_desc(m, r, m.arg(1), m.arg(2)); return m.ret(12, 0); }
        if (mt == "GetSurfaceLevel") { m.mem.write(m.arg(2), 4, surface_of(self, m.arg(1), 0)); return m.ret(12, 0); }
        if (mt == "GetCubeMapSurface") { uint32_t idx = m.arg(1) * r.levels + m.arg(2); m.mem.write(m.arg(3), 4, surface_of(self, idx, m.arg(1))); return m.ret(16, 0); }
        if (mt == "LockRect") {
            if (cube) { lock_rect(m, r, m.arg(1) * r.levels + m.arg(2), m.arg(3), m.arg(4)); return m.ret(24, 0); }
            lock_rect(m, r, m.arg(1), m.arg(2), m.arg(3)); return m.ret(20, 0);
        }
        if (mt == "UnlockRect") { touch_res(self); r.version++; return m.ret(bytes, 0); }
        if (mt == "AddDirtyRect" || mt == "AddDirtyBox") return m.ret(bytes, 0);
        if (mt == "LockBox") {
            uint32_t lvl = m.arg(1), out = m.arg(2);
            m.mem.write(out, 4, r.level_pitch[lvl]); m.mem.write(out + 4, 4, r.level_pitch[lvl] * r.level_h[lvl]); m.mem.write(out + 8, 4, r.level_addr[lvl]);
            return m.ret(20, 0);
        }
        if (mt == "UnlockBox") { touch_res(self); r.version++; return m.ret(8, 0); }
        (void)s; throw VmError("IDirect3DBaseTexture9::" + mt);
    };
    texture_if = iface("Texture", TEXTURE, texture_call);
    cube_if = iface("CubeTexture", CUBETEX, texture_call);
    vol_if = iface("VolumeTexture", VOLTEX, texture_call);
    Call buffer_call = [ds, unknown, touch_res](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        Res& r = *ds->res.at(self);
        if (mt == "GetType") return m.ret(4, r.kind == Res::VB ? 6 : 7);
        if (mt == "Lock") { m.mem.write(m.arg(3), 4, r.data + m.arg(1)); return m.ret(20, 0); }
        if (mt == "Unlock") { touch_res(self); r.version++; return m.ret(4, 0); }
        if (mt == "GetDesc") {
            // D3DVERTEXBUFFER_DESC {Format, Type, Usage, Pool, Size, FVF} (24 bytes); D3DINDEXBUFFER_DESC has no FVF (20 bytes)
            bool vb = r.kind == Res::VB;
            uint32_t v[6] = {vb ? 100u /*VERTEXDATA*/ : r.format, uint32_t(vb ? 6 : 7), r.usage, r.pool, r.size, r.fvf};
            m.mem.write_block(m.arg(1), v, vb ? 24 : 20); return m.ret(8, 0);
        }
        throw VmError("IDirect3D{Vertex,Index}Buffer9::" + mt);
    };
    vb_if = iface("VertexBuffer", BUFFER, buffer_call);
    ib_if = iface("IndexBuffer", BUFFER, buffer_call);
    Call shader_call = [ds, unknown](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        Res& r = *ds->res.at(self);
        if (mt == "GetFunction" || mt == "GetDeclaration") {
            uint32_t data = m.arg(1), sizep = m.arg(2);
            uint32_t n = mt == "GetDeclaration" ? uint32_t(r.bytes.size() / 8) : uint32_t(r.bytes.size());
            if (data) m.mem.write_block(data, r.bytes.data(), r.bytes.size());
            m.mem.write(sizep, 4, n); return m.ret(12, 0);
        }
        throw VmError("shader/declaration::" + mt);
    };
    vs_if = iface("VertexShader", SHADER, shader_call);
    ps_if = iface("PixelShader", SHADER, shader_call);
    decl_if = iface("VertexDeclaration", DECL, shader_call);
    Call query_call = [ds, unknown](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        Res& r = *ds->res.at(self);
        if (mt == "GetType") return m.ret(4, r.type);
        if (mt == "GetDataSize") return m.ret(4, r.type == 8 ? 4 : r.type == 9 ? 4 : 4);
        if (mt == "Issue") return m.ret(8, 0);
        if (mt == "GetData") {
            uint32_t out = m.arg(1), size = m.arg(2);
            if (out && size >= 4) m.mem.write(out, 4, r.type == 9 ? 1000u : 1u);   // occlusion: visible pixels; event: done
            return m.ret(16, 0);
        }
        throw VmError("IDirect3DQuery9::" + mt);
    };
    query_if = iface("Query", QUERY, query_call);
    Call sb_call = [ds, unknown](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        (void)ds; return m.ret(bytes, 0);
    };
    sb_if = iface("StateBlock", STATEBLOCK, sb_call);
    Call sc_call = [s, ds, unknown, touch_res](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        if (mt == "Present") { ds->presents++; return m.ret(24, 0); }
        if (mt == "GetBackBuffer") { m.mem.write(m.arg(3), 4, ds->backbuffer); touch_res(ds->backbuffer); ds->res.at(ds->backbuffer)->refs++; return m.ret(16, 0); }
        if (mt == "GetRasterStatus") { m.mem.write(m.arg(1), 4, 1); m.mem.write(m.arg(1) + 4, 4, 0); return m.ret(8, 0); }
        if (mt == "GetDisplayMode") { uint32_t v[4] = {s->cfg.width, s->cfg.height, 60, 22}; m.mem.write_block(m.arg(1), v, 16); return m.ret(8, 0); }
        if (mt == "GetPresentParameters") { m.mem.write_block(m.arg(1), nullptr, 56); return m.ret(8, 0); }
        return m.ret(bytes, 0);
    };
    sc_if = iface("SwapChain", SWAPCHAIN, sc_call);

    // ---------------------------------------------------------------- the device
    auto layout_texture = [](Boot::Impl* s, Res& r, int faces) {
        uint32_t blk = 0; bool dxt = format_dxt(r.format, &blk); uint32_t bits = format_bits(r.format);
        for (int f = 0; f < faces; f++)
            for (uint32_t l = 0; l < r.levels; l++) {
                uint32_t w = std::max(1u, r.width >> l), h = std::max(1u, r.height >> l), d = std::max(1u, r.depth >> l), pitch, rows;
                if (dxt) { pitch = std::max(1u, (w + 3) / 4) * blk; rows = std::max(1u, (h + 3) / 4); }
                else { pitch = std::max(1u, w * bits / 8); pitch = (pitch + 3) & ~3u; rows = h; }
                uint32_t bytes = pitch * rows * d;
                r.level_addr.push_back(s->halloc(DEVICE_HEAP, std::max<uint32_t>(bytes, 16), false)); r.level_pitch.push_back(pitch); r.level_bytes.push_back(bytes);
                r.level_w.push_back(w); r.level_h.push_back(h);
            }
    };
    auto shader_bytes = [](Machine& m, uint32_t p) {   // SM tokens up to and including the end token
        std::vector<uint8_t> b; uint32_t n = 0;
        // comments (CTAB, DBUG) and SM2+ instruction operands may hold 0x0000ffff: skip them by their lengths (RENDER CAPTURE fix)
        bool sm2 = ((uint32_t(m.mem.read(p, 4)) >> 8) & 0xff) >= 2;
        for (n = 1;;) { uint32_t t = uint32_t(m.mem.read(p + 4 * n, 4)); if (t == 0x0000ffffu) { n++; break; } if (n > (1u << 20)) throw VmError("shader without end token");
            n += (t & 0xffff) == 0xfffe ? 1 + ((t >> 16) & 0x7fff) : sm2 ? 1 + ((t >> 24) & 15) : 1; }
        b.resize(4 * n); m.mem.read_block(p, b.data(), b.size()); return b;
    };
    auto device_call = [=](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        auto texture = [&](Res::Kind kind, uint32_t w, uint32_t h, uint32_t d, uint32_t levels, uint32_t usage, uint32_t fmt, uint32_t pool, uint32_t out) {
            auto r = std::make_shared<Res>(); r->kind = kind; r->width = w; r->height = h; r->depth = d; r->usage = usage; r->format = fmt; r->pool = pool;
            uint32_t full = 1; for (uint32_t x = std::max({w, h, d}); x > 1; x >>= 1) full++;
            r->levels = levels == 0 || levels > full ? full : levels;
            if (usage & 0x400) r->levels = levels == 0 ? full : levels;   // AUTOGENMIPMAP
            layout_texture(s, *r, kind == Res::Cube ? 6 : 1);
            if (w >= 1024) s->say("CreateTexture %ux%u levels %u format %08x usage %x pool %u", w, h, r->levels, fmt, usage, pool);
            m.mem.write(out, 4, make(kind == Res::Cube ? cube_if : kind == Res::Volume ? vol_if : texture_if, r));
        };
        if (mt == "TestCooperativeLevel") return m.ret(4, 0);
        if (mt == "GetAvailableTextureMem") return m.ret(4, 512u << 20);
        if (mt == "EvictManagedResources") return m.ret(4, 0);
        if (mt == "GetDirect3D") { m.mem.write(m.arg(1), 4, ds->d3d); return m.ret(8, 0); }
        if (mt == "GetDeviceCaps") { m.mem.write_block(m.arg(1), ds->caps.data(), ds->caps.size()); return m.ret(8, 0); }
        if (mt == "GetDisplayMode") { uint32_t v[4] = {s->cfg.width, s->cfg.height, 60, 22}; m.mem.write_block(m.arg(2), v, 16); return m.ret(12, 0); }
        if (mt == "GetCreationParameters") { uint32_t v[4] = {0, 1, 0x10010, 0x40}; m.mem.write_block(m.arg(1), v, 16); return m.ret(8, 0); }
        if (mt == "GetSwapChain") { m.mem.write(m.arg(2), 4, ds->swapchain); return m.ret(12, 0); }
        if (mt == "GetNumberOfSwapChains") return m.ret(4, 1);
        if (mt == "Reset") return m.ret(8, 0);
        if (mt == "Present") { ds->presents++; return m.ret(20, 0); }
        if (mt == "GetBackBuffer") { m.mem.write(m.arg(4), 4, ds->backbuffer); touch_res(ds->backbuffer); ds->res.at(ds->backbuffer)->refs++; return m.ret(20, 0); }
        if (mt == "GetRasterStatus") { m.mem.write(m.arg(2), 4, 1); m.mem.write(m.arg(2) + 4, 4, 0); return m.ret(12, 0); }
        if (mt == "CreateTexture") { texture(Res::Texture, m.arg(1), m.arg(2), 1, m.arg(3), m.arg(4), m.arg(5), m.arg(6), m.arg(7)); return m.ret(36, 0); }
        if (mt == "CreateVolumeTexture") { texture(Res::Volume, m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6), m.arg(7), m.arg(8)); return m.ret(40, 0); }
        if (mt == "CreateCubeTexture") { texture(Res::Cube, m.arg(1), m.arg(1), 1, m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6)); return m.ret(32, 0); }
        if (mt == "CreateVertexBuffer" || mt == "CreateIndexBuffer") {
            auto r = std::make_shared<Res>(); bool vb = mt == "CreateVertexBuffer";
            r->kind = vb ? Res::VB : Res::IB; r->size = m.arg(1); r->usage = m.arg(2); if (vb) r->fvf = m.arg(3); else r->format = m.arg(3); r->pool = m.arg(4);
            r->data = alloc_data(r->size);
            m.mem.write(m.arg(5), 4, make(vb ? vb_if : ib_if, r)); return m.ret(28, 0);
        }
        if (mt == "CreateRenderTarget" || mt == "CreateDepthStencilSurface" || mt == "CreateOffscreenPlainSurface") {
            uint32_t w = m.arg(1), h = m.arg(2), fmt = m.arg(3), out = mt == "CreateOffscreenPlainSurface" ? m.arg(5) : m.arg(7);
            m.mem.write(out, 4, standalone_surface(w, h, fmt, mt == "CreateRenderTarget" ? 1 : mt == "CreateDepthStencilSurface" ? 2 : 0, mt == "CreateOffscreenPlainSurface" ? m.arg(4) : 0, mt));
            return m.ret(bytes, 0);
        }
        if (mt == "CreateVertexShader" || mt == "CreatePixelShader") {
            auto r = std::make_shared<Res>(); r->kind = mt == "CreateVertexShader" ? Res::VS : Res::PS; r->bytes = shader_bytes(m, m.arg(1));
            m.mem.write(m.arg(2), 4, make(r->kind == Res::VS ? vs_if : ps_if, r)); return m.ret(12, 0);
        }
        if (mt == "CreateVertexDeclaration") {
            auto r = std::make_shared<Res>(); r->kind = Res::Decl; uint32_t p = m.arg(1), n = 0;
            while (m.mem.read(p + 8 * n, 2) != 0xff && n < 64) n++; n++;
            r->bytes.resize(8 * n); m.mem.read_block(p, r->bytes.data(), r->bytes.size());
            m.mem.write(m.arg(2), 4, make(decl_if, r)); return m.ret(12, 0);
        }
        if (mt == "CreateQuery") {
            if (!m.arg(2)) return m.ret(12, 0);   // support check
            auto r = std::make_shared<Res>(); r->kind = Res::Query; r->type = m.arg(1);
            m.mem.write(m.arg(2), 4, make(query_if, r)); return m.ret(12, 0);
        }
        if (mt == "CreateStateBlock") { auto r = std::make_shared<Res>(); r->kind = Res::StateBlock; m.mem.write(m.arg(2), 4, make(sb_if, r)); return m.ret(12, 0); }
        if (mt == "EndStateBlock") { auto r = std::make_shared<Res>(); r->kind = Res::StateBlock; m.mem.write(m.arg(1), 4, make(sb_if, r)); return m.ret(8, 0); }
        if (mt == "CreateAdditionalSwapChain") { m.mem.write(m.arg(2), 4, ds->swapchain); return m.ret(12, 0); }
        if (mt == "GetRenderTarget") { uint32_t i = m.arg(1); uint32_t rt = i < 4 ? ds->render_targets[i] : 0; if (!rt && i == 0) rt = ds->backbuffer; m.mem.write(m.arg(2), 4, rt); if (rt) { touch_res(rt); ds->res.at(rt)->refs++; } return m.ret(12, rt ? 0 : 0x8876086cu); }
        if (mt == "SetRenderTarget") { uint32_t i = m.arg(1); if (i < 4) ds->render_targets[i] = m.arg(2); return m.ret(12, 0); }
        if (mt == "GetDepthStencilSurface") { uint32_t d = ds->ds ? ds->ds : ds->depth; m.mem.write(m.arg(1), 4, d); if (d) { touch_res(d); ds->res.at(d)->refs++; } return m.ret(8, d ? 0 : 0x8876086cu); }
        if (mt == "SetDepthStencilSurface") { ds->ds = m.arg(1); return m.ret(8, 0); }
        if (mt == "GetViewport") { m.mem.write_block(m.arg(1), ds->viewport, 24); return m.ret(8, 0); }
        if (mt == "SetViewport") { m.mem.read_block(m.arg(1), ds->viewport, 24); return m.ret(8, 0); }
        if (mt == "SetTexture") { uint32_t st = m.arg(1); if (st < 32) ds->textures[st] = m.arg(2); return m.ret(12, 0); }
        if (mt == "GetTexture") { uint32_t st = m.arg(1), t = st < 32 ? ds->textures[st] : 0; m.mem.write(m.arg(2), 4, t); if (t && ds->res.count(t)) { touch_res(t); ds->res.at(t)->refs++; } return m.ret(12, 0); }
        // Clear of render target 0 and GetRenderTargetData: the CPU-visible results the game reads
        // back (the terrain deformation surfaces start as Clear(0x80808080) copied to sysmem).
        auto surface_memory = [ds](uint32_t o, uint32_t& addr, uint32_t& pitch, uint32_t& bytes, uint32_t& fmt, uint32_t& w, uint32_t& h) -> bool {
            auto it = ds->res.find(o); if (it == ds->res.end()) return false;
            Res* r = it->second.get(); uint32_t idx = 0;
            if (r->container) { idx = r->level; r = ds->res.at(r->container).get(); }
            if (idx >= r->level_addr.size()) return false;
            addr = r->level_addr[idx]; pitch = r->level_pitch[idx]; bytes = r->level_bytes[idx]; fmt = r->format; w = r->level_w[idx]; h = r->level_h[idx];
            return true;
        };
        if (mt == "Clear") {
            uint32_t flags = m.arg(3), color = m.arg(4), addr, pitch, n, fmt, w, h;
            if ((flags & 1) && m.arg(1) == 0 && surface_memory(ds->render_targets[0], addr, pitch, n, fmt, w, h)) {
                uint32_t v = 0; bool fill = true;
                if (fmt == 114) { float f = float((color >> 16) & 0xff) / 255.0f; std::memcpy(&v, &f, 4); }        // R32F: red
                else if (fmt == 21 || fmt == 22) v = color;
                else fill = false;
                if (fill) { std::vector<uint32_t> row(pitch / 4, v); for (uint32_t y = 0; y < h; y++) m.mem.write_block(addr + y * pitch, row.data(), pitch); }
            }
            return m.ret(28, 0);
        }
        if (mt == "GetRenderTargetData") {
            uint32_t sa, sp, sn, sf, sw, sh, da, dp, dn, df, dw, dh;
            if (surface_memory(m.arg(1), sa, sp, sn, sf, sw, sh) && surface_memory(m.arg(2), da, dp, dn, df, dw, dh) && sf == df && sp == dp) {
                std::vector<uint8_t> tmp(std::min(sn, dn)); m.mem.read_block(sa, tmp.data(), tmp.size()); m.mem.write_block(da, tmp.data(), tmp.size());
            }
            return m.ret(12, 0);
        }
        if (mt.rfind("Draw", 0) == 0) { ds->draws++; return m.ret(bytes, 0); }
        if (mt.rfind("Set", 0) == 0) {   // remember (method, first argument) -> second argument for the matching getter
            uint64_t key = (uint64_t(std::hash<std::string>()(mt.substr(3))) << 32) | m.arg(1);
            if (s->logging()) { auto it = ds->state.find(key); bool had = it != ds->state.end(); uint32_t was = had ? it->second : 0;
                s->on_undo([ds, key, had, was] { if (had) ds->state[key] = was; else ds->state.erase(key); }); }
            ds->state[key] = bytes >= 12 ? m.arg(2) : m.arg(1);
            return m.ret(bytes, 0);
        }
        if (mt == "GetRenderState" || mt == "GetSamplerState" || mt == "GetTextureStageState") {
            uint64_t key = (uint64_t(std::hash<std::string>()(mt.substr(3))) << 32) | m.arg(1);
            if (mt != "GetRenderState") key = (uint64_t(std::hash<std::string>()(mt.substr(3))) << 32) | m.arg(1);
            auto it = ds->state.find(key);
            m.mem.write(m.arg(mt == "GetRenderState" ? 2 : 3), 4, it == ds->state.end() ? 0 : it->second); return m.ret(bytes, 0);
        }
        if (mt.rfind("Get", 0) == 0) { s->say("D3D device %s (zeroed)", mt.c_str()); return m.ret(bytes, 0); }
        if (mt == "BeginScene" || mt == "EndScene" || mt == "Clear" || mt == "StretchRect" || mt == "UpdateSurface" || mt == "UpdateTexture" ||
            mt == "GetRenderTargetData" || mt == "ColorFill" || mt == "BeginStateBlock" || mt == "ProcessVertices" || mt == "ShowCursor" ||
            mt == "SetCursorProperties" || mt == "SetCursorPosition" || mt == "SetDialogBoxMode" || mt == "SetGammaRamp" || mt == "DeletePatch" ||
            mt == "ValidateDevice" || mt == "SetSoftwareVertexProcessing")
            return m.ret(bytes, 0);
        throw VmError("IDirect3DDevice9::" + mt);
    };
    Methods device_methods; for (auto& e : D3D9_DEVICE_METHODS) device_methods.push_back({e.name, e.bytes});
    Iface* device_if = iface("Device", device_methods, device_call);
    texture_info = [ds](uint32_t o, TexInfo& t) -> bool {
        auto it = ds->res.find(o); if (it == ds->res.end()) return false;
        Res* r = it->second.get();
        if (r->container) { Res* c = ds->res.at(r->container).get(); t.format = c->format; t.addr = {c->level_addr[r->level]}; t.pitch = {c->level_pitch[r->level]}; t.w = {c->level_w[r->level]}; t.h = {c->level_h[r->level]}; }
        else { if (r->level_addr.empty()) return false; t.format = r->format; t.addr = r->level_addr; t.pitch = r->level_pitch; t.w = r->level_w; t.h = r->level_h; }
        t.width = t.w[0]; t.height = t.h[0]; return true;
    };
    d3d_resource = [ds](uint32_t o, Boot::D3DResource& t) -> bool {   // RENDER CAPTURE (render_capture.cpp)
        auto it = ds->res.find(o); if (it == ds->res.end()) return false;
        const Res& r = *it->second;
        t.kind = int(r.kind); t.width = r.width; t.height = r.height; t.depth = r.depth; t.levels = r.levels; t.format = r.format; t.usage = r.usage; t.pool = r.pool;
        t.fvf = r.fvf; t.size = r.size; t.data = r.data; t.container = r.container; t.level = r.level; t.face = r.face; t.version = r.version;
        t.level_addr = r.level_addr; t.level_pitch = r.level_pitch; t.level_bytes = r.level_bytes; t.level_w = r.level_w; t.level_h = r.level_h; t.bytes = r.bytes; t.label = r.label;
        if (o == ds->backbuffer) t.label = "backbuffer"; else if (o == ds->depth) t.label = "depth";
        return true;
    };
    // D3DCAPS9 of a shader model 3 adapter (see boot_caps.inc)
    ds->caps.assign(304, 0);
    {
        auto put = [&](uint32_t off, uint32_t v) { std::memcpy(ds->caps.data() + off, &v, 4); };
        auto putf = [&](uint32_t off, float v) { std::memcpy(ds->caps.data() + off, &v, 4); };
#include "boot_caps.inc"
    }
    Call d3d_call = [=](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        if (mt == "GetAdapterCount") return m.ret(4, 1);
        if (mt == "GetAdapterIdentifier") {
            std::vector<uint8_t> v(1100, 0);
            std::strcpy(reinterpret_cast<char*>(v.data()), "nvd3dum.dll"); std::strcpy(reinterpret_cast<char*>(v.data() + 512), "NVIDIA GeForce 8800 GT");
            std::strcpy(reinterpret_cast<char*>(v.data() + 1024), "\\\\.\\DISPLAY1");
            uint32_t vend = 0x10de, dev = 0x0611; std::memcpy(v.data() + 1064, &vend, 4); std::memcpy(v.data() + 1068, &dev, 4);
            uint64_t ver = 0x0008000f000b0a0cull; std::memcpy(v.data() + 1056, &ver, 8);
            m.mem.write_block(m.arg(3), v.data(), v.size()); return m.ret(16, 0);
        }
        if (mt == "GetAdapterModeCount") return m.ret(12, 4);
        if (mt == "EnumAdapterModes") {
            static const uint32_t modes[4][2] = {{800, 600}, {1024, 768}, {1280, 720}, {1920, 1080}}; uint32_t i = std::min<uint32_t>(m.arg(3), 3);
            uint32_t v[4] = {modes[i][0], modes[i][1], 60, m.arg(2)}; m.mem.write_block(m.arg(4), v, 16); return m.ret(20, 0);
        }
        if (mt == "GetAdapterDisplayMode") { uint32_t v[4] = {1920, 1080, 60, 22}; m.mem.write_block(m.arg(2), v, 16); return m.ret(12, 0); }
        if (mt == "CheckDeviceType" || mt == "CheckDepthStencilMatch" || mt == "CheckDeviceFormatConversion") return m.ret(bytes, 0);
        if (mt == "CheckDeviceFormat") {
            uint32_t fmt = m.arg(6);
            s->say("CheckDeviceFormat usage %x rtype %u format %x", m.arg(4), m.arg(5), fmt);
            bool fourcc_unusual = fmt == 0x5a544e49u /*INTZ*/ || fmt == 0x5a574152u /*RAWZ*/ || fmt == 0x4c4c554eu /*NULL*/ || fmt == 0x34324644u /*DF24*/ || fmt == 0x36314644u /*DF16*/ || fmt == 0x4f4c4552u;
            return m.ret(28, fourcc_unusual ? 0x8876086au : 0);
        }
        if (mt == "CheckDeviceMultiSampleType") { if (m.arg(6)) m.mem.write(m.arg(6), 4, 1); return m.ret(28, m.arg(5) <= 1 ? 0 : 0x8876086au); }
        if (mt == "GetDeviceCaps") { m.mem.write_block(m.arg(3), ds->caps.data(), ds->caps.size()); return m.ret(16, 0); }
        if (mt == "GetAdapterMonitor") return m.ret(8, 0x10020);
        if (mt == "CreateDevice") {
            uint32_t pp = m.arg(5), out = m.arg(6);
            uint32_t bw = uint32_t(m.mem.read(pp, 4)), bh = uint32_t(m.mem.read(pp + 4, 4)), bf = uint32_t(m.mem.read(pp + 8, 4));
            uint32_t autods = uint32_t(m.mem.read(pp + 40, 4)), dfmt = uint32_t(m.mem.read(pp + 44, 4));
            if (!bw) bw = s->cfg.width; if (!bh) bh = s->cfg.height; if (!bf) bf = 22;
            s->say("CreateDevice %ux%u format %u autodepth %u (%u) behavior %x", bw, bh, bf, autods, dfmt, m.arg(4));
            if (!ds->device) {
                auto dev = std::make_shared<Res>(); dev->kind = Res::Device; ds->device = make(device_if, dev);
                ds->backbuffer = standalone_surface(bw, bh, bf, 1, 0, "backbuffer");
                ds->depth = autods ? standalone_surface(bw, bh, dfmt, 2, 0, "depth") : 0;
                auto sc = std::make_shared<Res>(); sc->kind = Res::SwapChain; ds->swapchain = make(sc_if, sc);
                ds->render_targets[0] = ds->backbuffer; ds->ds = ds->depth;
                ds->viewport[2] = bw; ds->viewport[3] = bh;
            }
            m.mem.write(out, 4, ds->device); return m.ret(28, 0);
        }
        throw VmError("IDirect3D9::" + mt);
    };
    Iface* d3d_if = iface("Direct3D9", D3D9, d3d_call);
    imp("d3d9.dll!Direct3DCreate9", [s, ds, d3d_if, make, touch_ds](Machine& m) {
        s->say("Direct3DCreate9 %u", m.arg(0)); touch_ds();
        if (!ds->d3d) { auto r = std::make_shared<Res>(); r->kind = Res::D3D; ds->d3d = make(d3d_if, r); }
        return m.ret(4, ds->d3d);
    });
    // ---------------------------------------------------------------- D3DX
    // ID3DXConstantTable over the shader's CTAB comment block.
    struct Ctab { std::vector<uint8_t> blob; uint32_t blob_addr = 0, refs = 1; };
    auto ctabs = std::make_shared<std::map<uint32_t, Ctab>>();
    auto ctab_call = [s, ctabs](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        Ctab& c = ctabs->at(self);
        auto rd32 = [&](uint32_t off) { uint32_t v = 0; if (off + 4 <= c.blob.size()) std::memcpy(&v, c.blob.data() + off, 4); return v; };
        // CTAB: Size, Creator, Version, Constants, ConstantInfo, Flags, Target; D3DXSHADER_CONSTANTINFO: Name, RegisterSet(2), RegisterIndex(2), RegisterCount(2), Reserved(2), TypeInfo, DefaultValue
        uint32_t count = rd32(12), info = rd32(16);
        auto handle_of = [&](uint32_t i) { return 0x40000000u + i; };   // D3DXHANDLE: a cookie
        auto index_of_name = [&](const std::string& n) -> int64_t { for (uint32_t i = 0; i < count; i++) { uint32_t no = rd32(info + 20 * i); std::string nm(reinterpret_cast<const char*>(c.blob.data() + no)); if (nm == n) return i; } return -1; };
        if (mt == "AddRef" || mt == "Release") { uint32_t was = c.refs; s->on_undo([ctabs, self, was] { auto it = ctabs->find(self); if (it != ctabs->end()) it->second.refs = was; }); }
        if (mt == "AddRef") return m.ret(4, ++c.refs);
        if (mt == "Release") return m.ret(4, c.refs ? --c.refs : 0);
        if (mt == "QueryInterface") return m.ret(12, 0x80004002u);
        if (mt == "GetBufferPointer") return m.ret(4, c.blob_addr);
        if (mt == "GetBufferSize") return m.ret(4, uint32_t(c.blob.size()));
        if (mt == "GetDesc") {   // D3DXCONSTANTTABLE_DESC: Creator, Version, Constants
            uint32_t v[3] = {c.blob_addr + rd32(4), rd32(8), count}; m.mem.write_block(m.arg(1), v, 12); return m.ret(8, 0);
        }
        if (mt == "GetConstant") {   // (parent handle, index)
            if (m.arg(1)) throw VmError("ID3DXConstantTable::GetConstant with a parent");
            uint32_t i = m.arg(2); return m.ret(12, i < count ? handle_of(i) : 0);
        }
        if (mt == "GetConstantByName") {
            if (m.arg(1)) throw VmError("ID3DXConstantTable::GetConstantByName with a parent");
            int64_t i = index_of_name(s->cstr(m.arg(2))); return m.ret(12, i < 0 ? 0 : handle_of(uint32_t(i)));
        }
        if (mt == "GetConstantDesc" || mt == "GetSamplerIndex") {
            uint32_t h = m.arg(1); uint32_t i;
            if (h >= 0x40000000u && h < 0x40000000u + count) i = h - 0x40000000u;
            else { int64_t k = index_of_name(s->cstr(h)); if (k < 0) return m.ret(bytes, mt == "GetSamplerIndex" ? 0xffffffffu : 0x8876086cu); i = uint32_t(k); }
            uint32_t e = info + 20 * i, type = rd32(e + 12);
            uint16_t set, reg, regs; std::memcpy(&set, c.blob.data() + e + 4, 2); std::memcpy(&reg, c.blob.data() + e + 6, 2); std::memcpy(&regs, c.blob.data() + e + 8, 2);
            if (mt == "GetSamplerIndex") return m.ret(8, set == 3 ? reg : 0xffffffffu);
            // D3DXCONSTANT_DESC: Name, RegisterSet, RegisterIndex, RegisterCount, Class, Type, Rows, Columns, Elements, StructMembers, Bytes, DefaultValue
            uint16_t cls, ty, rows, cols, elems, members; std::memcpy(&cls, c.blob.data() + type, 2); std::memcpy(&ty, c.blob.data() + type + 2, 2);
            std::memcpy(&rows, c.blob.data() + type + 4, 2); std::memcpy(&cols, c.blob.data() + type + 6, 2); std::memcpy(&elems, c.blob.data() + type + 8, 2); std::memcpy(&members, c.blob.data() + type + 10, 2);
            uint32_t dv = rd32(e + 16);
            uint32_t v[12] = {c.blob_addr + rd32(e), set, reg, regs, cls, ty, rows, cols, elems, members, 4u * rows * cols * std::max<uint32_t>(1, elems), dv ? c.blob_addr + dv : 0};
            uint32_t out = m.arg(2), cnt = m.arg(3);
            if (out) m.mem.write_block(out, v, 48);
            if (cnt) m.mem.write(cnt, 4, 1);
            return m.ret(16, 0);
        }
        if (mt.rfind("Set", 0) == 0) return m.ret(bytes, 0);
        throw VmError("ID3DXConstantTable::" + mt);
    };
    Iface* ct_if = iface("ConstantTable", CONSTTABLE, ctab_call);
    d3dx_constant_table = [ctabs](uint32_t o, Boot::D3DResource& t) -> bool {   // RENDER CAPTURE: the table's CTAB (kind 16)
        auto it = ctabs->find(o); if (it == ctabs->end()) return false; t.kind = 16; t.bytes = it->second.blob; return true; };
    imp("d3dx9_43.dll!D3DXGetShaderConstantTable", [s, ctabs, ct_if](Machine& m) {
        uint32_t fn = m.arg(0), out = m.arg(1);
        // find the CTAB comment: comment token 0xFFFE with length in bits 16..30, then 'CTAB'
        for (uint32_t p = fn + 4, k = 0; k < 100000; k++) {
            uint32_t t = uint32_t(m.mem.read(p, 4));
            if (t == 0x0000ffffu) break;
            if ((t & 0xffff) == 0xfffe) {
                uint32_t len = (t >> 16) & 0x7fff;
                if (m.mem.read(p + 4, 4) == 0x42415443u) {
                    Ctab c; c.blob.resize(4 * (len - 1)); m.mem.read_block(p + 8, c.blob.data(), c.blob.size());
                    c.blob_addr = s->halloc(s->process_heap, uint32_t(c.blob.size()), false); m.mem.write_block(c.blob_addr, c.blob.data(), c.blob.size());
                    uint32_t o = s->halloc(s->process_heap, 16, true); m.mem.write(o, 4, ct_if->vtable); (*ctabs)[o] = std::move(c); s->on_undo([ctabs, o] { ctabs->erase(o); });
                    m.mem.write(out, 4, o); return m.ret(8, 0);
                }
                p += 4 * (len + 1); continue;
            }
            p += 4;   // instruction tokens: scan token by token (the CTAB comes first in practice)
        }
        m.mem.write(out, 4, 0); return m.ret(8, 0x8876086cu);
    });
    imp("d3dx9_43.dll!D3DXFloat32To16Array", [](Machine& m) {
        uint32_t out = m.arg(0), in = m.arg(1), n = m.arg(2);
        for (uint32_t i = 0; i < n; i++) {
            uint32_t f = uint32_t(m.mem.read(in + 4 * i, 4)); uint32_t sign = (f >> 16) & 0x8000; int32_t e = int32_t((f >> 23) & 0xff) - 127 + 15; uint32_t mant = f & 0x7fffff; uint16_t h;
            if (((f >> 23) & 0xff) == 0xff) h = uint16_t(sign | 0x7c00 | (mant ? 0x200 : 0));
            else if (e >= 31) h = uint16_t(sign | 0x7bff);                                     // D3DX clamps to the largest finite half
            else if (e <= 0) { if (e < -10) h = uint16_t(sign); else { mant |= 0x800000; uint32_t shift = uint32_t(14 - e); uint32_t v = mant >> shift; uint32_t rem = mant & ((1u << shift) - 1), half = 1u << (shift - 1); if (rem > half || (rem == half && (v & 1))) v++; h = uint16_t(sign | v); } }
            else { uint32_t v = (uint32_t(e) << 10) | (mant >> 13); uint32_t rem = mant & 0x1fff; if (rem > 0x1000 || (rem == 0x1000 && (v & 1))) v++; h = uint16_t(sign | v); }
            m.mem.write(out + 2 * i, 2, h);
        }
        return m.ret(12, out);
    });
    imp("d3dx9_43.dll!D3DXLoadSurfaceFromSurface", [s](Machine& m) { s->say("D3DXLoadSurfaceFromSurface (not copied)"); return m.ret(32, 0); });
    // D3DXCompileShader: the game compiles a few small HLSL helpers at run time (overlay blits).
    // Nothing is drawn here, so the result is a valid shader token stream (version, CTAB built
    // from the source's register-bound globals, end) for CreateVertex/PixelShader and the
    // constant table queries; the source is kept in the log.
    Methods buffer_methods = with(IUNKNOWN, {{"GetBufferPointer", 4}, {"GetBufferSize", 4}});
    auto buffers = std::make_shared<std::map<uint32_t, std::pair<uint32_t, uint32_t>>>();   // ID3DXBuffer -> (data, size)
    Iface* buf_if = iface("D3DXBuffer", buffer_methods, [buffers](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        auto& b = buffers->at(self);
        if (mt == "GetBufferPointer") return m.ret(4, b.first);
        if (mt == "GetBufferSize") return m.ret(4, b.second);
        if (mt == "QueryInterface") return m.ret(12, 0x80004002u);
        return m.ret(bytes, 1);
    });
    imp("d3dx9_43.dll!D3DXCompileShader", [s, buffers, buf_if](Machine& m) -> uint32_t {
        std::string src = s->cstr(m.arg(0), m.arg(1)), entry = s->cstr(m.arg(4)), profile = s->cstr(m.arg(5));
        uint32_t out_shader = m.arg(7), out_errors = m.arg(8), out_table = m.arg(9);
        s->say("D3DXCompileShader %s %s (%zu bytes of source)", entry.c_str(), profile.c_str(), src.size());
        // globals "<type> <name> : register(<set><n>)"
        struct C { std::string name, type; uint16_t set, reg, count, cls, ty, rows, cols; };
        std::vector<C> cs;
        for (size_t at = 0; (at = src.find("register", at)) != std::string::npos; at += 8) {
            size_t open = src.find('(', at), close = src.find(')', at); if (open == std::string::npos || close == std::string::npos) break;
            std::string r = src.substr(open + 1, close - open - 1); r.erase(std::remove(r.begin(), r.end(), ' '), r.end());
            size_t colon = src.rfind(':', at), semi = src.rfind(';', colon), brace = src.rfind('}', colon); size_t start = std::max(semi == std::string::npos ? 0 : semi + 1, brace == std::string::npos ? 0 : brace + 1);
            std::string decl = src.substr(start, colon - start); std::vector<std::string> w; std::string tok;
            for (char c : decl) { if (std::isspace((unsigned char)c)) { if (!tok.empty()) w.push_back(tok), tok.clear(); } else tok += c; } if (!tok.empty()) w.push_back(tok);
            if (w.size() < 2 || r.empty()) continue;
            C c; c.name = w.back(); c.type = w[w.size() - 2]; c.reg = uint16_t(std::atoi(r.c_str() + 1));
            c.set = r[0] == 's' ? 3 : r[0] == 'b' ? 0 : r[0] == 'i' ? 1 : 2;
            std::string t = c.type; c.rows = 1; c.cols = 1; c.cls = 0; c.ty = 3; c.count = 1;
            if (t.rfind("float", 0) == 0) { size_t x = t.find('x'); if (x != std::string::npos) { c.rows = uint16_t(t[5] - '0'); c.cols = uint16_t(t[x + 1] - '0'); c.cls = 2; c.count = c.rows; } else if (t.size() > 5) { c.cols = uint16_t(t[5] - '0'); c.cls = 1; } }
            else if (t.rfind("sampler", 0) == 0) { c.cls = 4; c.ty = t == "samplerCUBE" ? 14 : t == "sampler3D" ? 13 : 12; }
            else if (t == "bool") { c.ty = 1; } else if (t.rfind("int", 0) == 0) { c.ty = 2; }
            cs.push_back(c);
        }
        // CTAB blob
        std::vector<uint8_t> blob(28 + 20 * cs.size() + 16 * cs.size(), 0);
        auto put32 = [&](size_t off, uint32_t v) { std::memcpy(blob.data() + off, &v, 4); };
        auto put16 = [&](size_t off, uint16_t v) { std::memcpy(blob.data() + off, &v, 2); };
        auto str = [&](const std::string& x) { uint32_t off = uint32_t(blob.size()); blob.insert(blob.end(), x.begin(), x.end()); blob.push_back(0); while (blob.size() % 4) blob.push_back(0); return off; };
        bool vs = profile.rfind("vs", 0) == 0; uint32_t version = (vs ? 0xfffe0000u : 0xffff0000u) | (uint32_t(profile[3] - '0') << 8) | uint32_t(profile.size() > 5 && profile[5] != 'x' ? profile[5] - '0' : 0);
        put32(0, 28); put32(8, version); put32(12, uint32_t(cs.size())); put32(16, 28); put32(20, 0);
        for (size_t i = 0; i < cs.size(); i++) {
            size_t e = 28 + 20 * i, ti = 28 + 20 * cs.size() + 16 * i;
            put16(e + 4, cs[i].set); put16(e + 6, cs[i].reg); put16(e + 8, cs[i].count); put32(e + 12, uint32_t(ti));
            put16(ti, cs[i].cls); put16(ti + 2, cs[i].ty); put16(ti + 4, cs[i].rows); put16(ti + 6, cs[i].cols); put16(ti + 8, 1); put16(ti + 10, 0);
        }
        for (size_t i = 0; i < cs.size(); i++) put32(28 + 20 * i, str(cs[i].name));
        put32(4, str("Reflex boot (stand-in for D3DX9 Shader Compiler)")); put32(24, str(profile));
        // token stream: version, comment('CTAB' + blob), end
        std::vector<uint32_t> tokens = {version, 0xfffeu | (uint32_t(1 + blob.size() / 4) << 16), 0x42415443u};
        for (size_t i = 0; i < blob.size(); i += 4) { uint32_t v; std::memcpy(&v, blob.data() + i, 4); tokens.push_back(v); }
        tokens.push_back(0x0000ffffu);
        uint32_t data = s->halloc(s->process_heap, uint32_t(tokens.size() * 4), false); m.mem.write_block(data, tokens.data(), tokens.size() * 4);
        uint32_t b = s->halloc(s->process_heap, 16, true); m.mem.write(b, 4, buf_if->vtable); (*buffers)[b] = {data, uint32_t(tokens.size() * 4)}; s->on_undo([buffers, b] { buffers->erase(b); });
        if (out_shader) m.mem.write(out_shader, 4, b);
        if (out_errors) m.mem.write(out_errors, 4, 0);
        if (out_table) {   // ID3DXConstantTable via the same path as D3DXGetShaderConstantTable
            uint32_t args[2] = {data, out_table};
            uint32_t sp = m.r[4]; m.push(args[1]); m.push(args[0]); m.push(0);
            s->m.imports.at("d3dx9_43.dll!D3DXGetShaderConstantTable")(m);
            m.r[4] = sp;
        }
        return m.ret(40, 0);
    });
    // ---------------------------------------------------------------- DirectInput 8
    struct DiDev { bool keyboard = false, mouse = false; };
    auto didevs = std::make_shared<std::map<uint32_t, DiDev>>();
    auto didev_call = [s, ds, didevs, unknown](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        DiDev d; { auto it = didevs->find(self); if (it != didevs->end()) d = it->second; }   // (read only: no entry made)
        if (mt == "GetDeviceState") {
            uint32_t size = m.arg(1), out = m.arg(2);
            if (d.keyboard) { std::vector<uint8_t> k(size, 0); std::memcpy(k.data(), s->keys, std::min<uint32_t>(size, 256)); m.mem.write_block(out, k.data(), size); }
            else m.mem.write_block(out, nullptr, size);
            return m.ret(12, 0);
        }
        if (mt == "GetDeviceData") { if (m.arg(3)) m.mem.write(m.arg(3), 4, 0); return m.ret(20, 0); }
        if (mt == "GetCapabilities") { uint32_t p = m.arg(1); uint32_t size = uint32_t(m.mem.read(p, 4)); m.mem.write_block(p + 4, nullptr, size - 4); m.mem.write(p + 4, 4, 1 /*ATTACHED*/); m.mem.write(p + 8, 4, d.keyboard ? 0x13u : 0x12u); return m.ret(8, 0); }
        if (mt == "EnumObjects" || mt == "EnumEffects" || mt == "EnumCreatedEffectObjects") return m.ret(bytes, 0);
        if (mt == "GetDeviceInfo") { uint32_t p = m.arg(1); uint32_t size = uint32_t(m.mem.read(p, 4)); m.mem.write_block(p + 4, nullptr, size - 4); m.mem.write(p + 36, 4, d.keyboard ? 0x13u : 0x12u); return m.ret(8, 0); }
        if (mt == "GetProperty") return m.ret(12, 0x80004001u);
        (void)ds; return m.ret(bytes, 0);   // SetDataFormat, SetCooperativeLevel, Acquire, Unacquire, SetProperty, Poll, ...
    };
    Iface* didev_if = iface("DirectInputDevice8", DIDEVICE, didev_call);
    auto di_call = [s, ds, didevs, didev_if, make, unknown](Machine& m, uint32_t self, const std::string& mt, uint32_t bytes) -> uint32_t {
        uint32_t r0 = unknown(m, self, mt, bytes); if (r0 != RUN_ORIGINAL) return r0;
        if (mt == "CreateDevice") {
            uint32_t g = uint32_t(m.mem.read(m.arg(1), 4));
            auto r = std::make_shared<Res>(); r->kind = Res::DIDevice; uint32_t o = make(didev_if, r);
            DiDev& d = (*didevs)[o]; d.keyboard = g == 0x6f1d2b61u; d.mouse = g == 0x6f1d2b60u; s->on_undo([didevs, o] { didevs->erase(o); });
            s->say("DirectInput CreateDevice %08x -> %s", g, d.keyboard ? "keyboard" : d.mouse ? "mouse" : "?");
            m.mem.write(m.arg(2), 4, o); return m.ret(16, 0);
        }
        if (mt == "EnumDevices") return m.ret(20, 0);   // no joysticks (XInput pads come through XInput)
        if (mt == "GetDeviceStatus") return m.ret(8, 1);   // DI_NOTATTACHED
        (void)ds; return m.ret(bytes, 0);
    };
    Iface* di_if = iface("DirectInput8", DINPUT, di_call);
    imp("dinput8.dll!DirectInput8Create", [s, di_if, make](Machine& m) {
        auto r = std::make_shared<Res>(); r->kind = Res::DInput; m.mem.write(m.arg(3), 4, make(di_if, r)); s->say("DirectInput8Create"); return m.ret(20, 0);
    });
    // ---------------------------------------------------------------- XInput: no pads (host pads arrive later)
    imp("xinput1_3.dll!#2", [](Machine& m) { return m.ret(8, 1167); });
    imp("xinput1_3.dll!#3", [](Machine& m) { return m.ret(8, 1167); });
    imp("xinput1_3.dll!#4", [](Machine& m) { return m.ret(12, 1167); });
    imp("xinput1_3.dll!#8", [](Machine& m) { return m.ret(12, 1167); });
    // ---------------------------------------------------------------- Steam: no client
    imp("steam_api.dll!SteamAPI_RestartAppIfNecessary", [](Machine& m) { return m.ret(0, 0); });
    imp("steam_api.dll!SteamAPI_Init", [s](Machine& m) { s->say("SteamAPI_Init -> false"); return m.ret(0, 0); });
    imp("steam_api.dll!SteamAPI_Shutdown", [](Machine& m) { return m.ret(0, 0); });
    for (const char* f : {"SteamUser", "SteamUserStats", "SteamFriends", "SteamUtils", "SteamRemoteStorage", "SteamNetworking", "SteamMatchmaking"})
        imp(std::string("steam_api.dll!") + f, [](Machine& m) { return m.ret(0, 0); });
    for (const char* f : {"SteamAPI_RegisterCallback", "SteamAPI_UnregisterCallback", "SteamAPI_RegisterCallResult", "SteamAPI_UnregisterCallResult", "SteamAPI_RunCallbacks"})
        imp(std::string("steam_api.dll!") + f, [](Machine& m) { return m.ret(0, 0); });
    // ---------------------------------------------------------------- Bink: movies are not played
    uint32_t bink_sound = native([](Machine& m) { return m.ret(4, 1); });
    imp("binkw32.dll!_BinkOpen@8", [s](Machine& m) {
        std::string f = s->cstr(m.arg(0));
        if (lower(f).find(".bik") != std::string::npos) { s->bink_file_opens++; s->on_undo([s] { s->bink_file_opens--; }); }   // (not the in-memory BIKi opens)
        s->say("BinkOpen %s -> 0 (movies not played) at %.2f s virtual, frame %llu", f.c_str(), s->now_ / 1e7, (unsigned long long)s->frames); return m.ret(8, 0);
    });
    imp("binkw32.dll!_BinkOpenDirectSound@4", [bink_sound](Machine& m) { return m.ret(4, bink_sound); });
    imp("binkw32.dll!_BinkSetSoundSystem@8", [](Machine& m) { return m.ret(8, 1); });
    for (auto [f, b] : std::vector<std::pair<const char*, uint32_t>>{{"_BinkSetSoundOnOff@8", 8}, {"_BinkWait@4", 4}, {"_BinkNextFrame@4", 4}, {"_BinkGetFrameBuffersInfo@8", 8},
                                                                     {"_BinkShouldSkip@4", 4}, {"_BinkPause@8", 8}, {"_BinkSetVolume@12", 12}, {"_BinkClose@4", 4},
                                                                     {"_BinkDoFrame@4", 4}, {"_BinkGoto@12", 12}, {"_BinkRegisterFrameBuffers@8", 8}, {"_BinkGetRects@8", 8}})
        imp(std::string("binkw32.dll!") + f, [b](Machine& m) { return m.ret(b, 0); });
    // ---------------------------------------------------------------- Winsock: no network
    imp("ws2_32.dll!#115", [](Machine& m) { uint32_t p = m.arg(1); m.mem.write_block(p, nullptr, 400); m.mem.write(p, 2, 0x0202); m.mem.write(p + 2, 2, 0x0202); return m.ret(8, 0); });
    imp("ws2_32.dll!#116", [](Machine& m) { return m.ret(0, 0); });
    imp("ws2_32.dll!#9", [](Machine& m) { uint32_t v = m.arg(0) & 0xffff; return m.ret(4, ((v & 0xff) << 8) | (v >> 8)); });   // htons
    imp("ws2_32.dll!#15", [](Machine& m) { uint32_t v = m.arg(0) & 0xffff; return m.ret(4, ((v & 0xff) << 8) | (v >> 8)); });  // ntohs
    imp("ws2_32.dll!#8", [](Machine& m) { return m.ret(4, __builtin_bswap32(m.arg(0))); });    // htonl
    imp("ws2_32.dll!#14", [](Machine& m) { return m.ret(4, __builtin_bswap32(m.arg(0))); });   // ntohl
    imp("ws2_32.dll!#23", [](Machine& m) { return m.ret(12, 0xffffffffu); });                  // socket -> INVALID_SOCKET
    imp("ws2_32.dll!#111", [](Machine& m) { return m.ret(0, 10050); });                        // WSAGetLastError: WSAENETDOWN
    imp("ws2_32.dll!#3", [](Machine& m) { return m.ret(4, 0); });
    imp("ws2_32.dll!#52", [](Machine& m) { return m.ret(4, 0); });                             // gethostbyname
    imp("ws2_32.dll!#11", [](Machine& m) { return m.ret(4, 0xffffffffu); });                   // inet_addr
    imp("ws2_32.dll!WSASocketA", [](Machine& m) { return m.ret(24, 0xffffffffu); });
    imp("ws2_32.dll!WSACreateEvent", [s](Machine& m) { auto o = std::make_shared<Obj>(); o->kind = Obj::Event; o->manual = true; return m.ret(0, s->add_handle(o)); });
    imp("ws2_32.dll!WSACloseEvent", [](Machine& m) { return m.ret(4, 1); });
    imp("ws2_32.dll!WSAResetEvent", [](Machine& m) { return m.ret(4, 1); });
    // ---------------------------------------------------------------- FMOD Ex / Event: no audio device
    imp("fmod_eventl.dll!_FMOD_EventSystem_Create@4", [s](Machine& m) { m.mem.write(m.arg(0), 4, 0xfa000000u); s->say("FMOD_EventSystem_Create"); return m.ret(4, 0); });
    imp("fmodexl.dll!FMOD_Memory_Initialize", [](Machine& m) { return m.ret(24, 0); });   // F_API = stdcall
    imp("fmodexl.dll!FMOD_Debug_SetLevel", [](Machine& m) { return m.ret(4, 0x52); });   // FMOD_ERR_UNSUPPORTED in release builds
    imp("fmodexl.dll!?getVersion@System@FMOD@@QAG?AW4FMOD_RESULT@@PAI@Z", [](Machine& m) { m.mem.write(m.arg(1), 4, 0x00042208u); return m.ret(8, 0); });
    imp("fmodexl.dll!?getMasterChannelGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVChannelGroup@2@@Z", [](Machine& m) { m.mem.write(m.arg(1), 4, 0xfb0f0000u); return m.ret(8, 0); });
    imp("fmodexl.dll!?getMasterSoundGroup@System@FMOD@@QAG?AW4FMOD_RESULT@@PAPAVSoundGroup@2@@Z", [](Machine& m) { m.mem.write(m.arg(1), 4, 0xfb0f0010u); return m.ret(8, 0); });
    imp("fmodexl.dll!?setSoftwareChannels@System@FMOD@@QAG?AW4FMOD_RESULT@@H@Z", [](Machine& m) { return m.ret(8, 0); });
    imp("fmodexl.dll!?setAdvancedSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@PAUFMOD_ADVANCEDSETTINGS@@@Z", [](Machine& m) { return m.ret(8, 0); });
    imp("fmodexl.dll!?get3DSettings@System@FMOD@@QAG?AW4FMOD_RESULT@@PAM00@Z", [](Machine& m) { for (int i = 1; i <= 3; i++) if (m.arg(i)) m.mem.write(m.arg(i), 4, 0x3f800000u); return m.ret(16, 0); });
    {
        Machine& m = this->m;
        constexpr uint32_t FMOD_EVENT_SYSTEM_GLOBAL = 0xa957f4;
#include "fmod_stand_ins.inc"
    }
    {   // native/game Verify inside the boot: fmod_stand_ins.inc's state (props, playing, handles) once per undo epoch
        auto epoch = std::make_shared<uint64_t>(0);
        auto touch = [this, epoch]() { if (logging() && *epoch != undo_epoch && fmod_snapshot) { *epoch = undo_epoch; on_undo(fmod_snapshot()); } };
        for (auto& [n, f] : m.imports) if (n.rfind("fmod_eventl.dll!", 0) == 0 || n.rfind("fmodexl.dll!", 0) == 0) { Native inner = f; f = [touch, inner](Machine& mm) { touch(); return inner(mm); }; }
        for (auto& [a, f] : m.natives) if (a >= 0xfa002000u && a < 0xfa003000u) { Native inner = f; f = [touch, inner](Machine& mm) { touch(); return inner(mm); }; }
    }
    // FMOD Event objects with their real interfaces (fmod_event_slots.inc): every getter that
    // returns an object hands out a new object of the right class (last argument), counts are 0,
    // infos are zero with empty names, user data is kept; everything else succeeds (FMOD_OK).
    {
        struct FmodObj { std::string cls; uint32_t user = 0; };
        auto objs = std::make_shared<std::map<uint32_t, FmodObj>>();
        auto vtables = std::make_shared<std::map<std::string, uint32_t>>();
        uint32_t empty = halloc(process_heap, 16, true);
        auto make_obj = std::make_shared<std::function<uint32_t(const std::string&)>>();
        auto class_of = [](const std::string& method) -> std::string {
            if (method.find("Group") != std::string::npos && method.find("Num") == std::string::npos) return "EventGroup";
            if (method.find("Project") != std::string::npos && method.find("Num") == std::string::npos) return "EventProject";
            if (method == "Load") return "EventProject";
            if (method.find("Category") != std::string::npos && method.find("Num") == std::string::npos) return "EventCategory";
            if (method.rfind("GetEvent", 0) == 0) return "Event";
            if (method.rfind("GetParameter", 0) == 0 && method != "GetParameterValue" && method != "GetParameters") return "EventParameter";
            if (method == "GetMusicSystem") return "MusicSystem";
            if (method == "CreateReverb") return "EventReverb";
            if (method == "GetSystemObject") return "System";
            if (method == "GetChannelGroup") return "ChannelGroup";
            return "";
        };
        auto call = [this, objs, empty, make_obj, class_of](Machine& m, const std::string& cls, const std::string& method, uint32_t bytes) -> uint32_t {
            uint32_t self = m.arg(0), nargs = bytes / 4;
            std::string made = class_of(method);
            if (!made.empty() && (method.rfind("Get", 0) == 0 || method == "Load" || method == "CreateReverb")) {
                uint32_t out = m.arg(int(nargs - 1));
                if (out) m.mem.write(out, 4, (*make_obj)(made));
                say("FMOD %s::%s -> %s", cls.c_str(), method.c_str(), made.c_str());
                return m.ret(bytes, 0);
            }
            if (method.rfind("GetNum", 0) == 0) { if (m.arg(1)) m.mem.write(m.arg(1), 4, 0); return m.ret(bytes, 0); }
            if (method == "SetUserData") {
                if (logging()) { auto it = objs->find(self); bool had = it != objs->end(); FmodObj was = had ? it->second : FmodObj{};
                    on_undo([objs, self, had, was] { if (had) (*objs)[self] = was; else objs->erase(self); }); }
                (*objs)[self].user = m.arg(1); return m.ret(bytes, 0);
            }
            if (method == "GetUserData") { auto it = objs->find(self); if (m.arg(1)) m.mem.write(m.arg(1), 4, it != objs->end() ? it->second.user : 0u); return m.ret(bytes, 0); }
            if (method == "GetInfo") {   // (index*, name**) for groups/categories; a struct for projects/systems
                if (cls == "EventGroup" || cls == "EventCategory") { if (m.arg(1)) m.mem.write(m.arg(1), 4, 0); if (m.arg(2)) m.mem.write(m.arg(2), 4, empty); }
                else if (m.arg(1)) m.mem.write_block(m.arg(1), nullptr, cls == "EventProject" ? 280 : 64);
                return m.ret(bytes, 0);
            }
            if (method == "GetReverbPreset" || method == "GetReverbPresetByIndex") {   // (name|index, FMOD_REVERB_PROPERTIES*, int* index)
                if (m.arg(2)) m.mem.write_block(m.arg(2), nullptr, 108);
                if (m.arg(3)) m.mem.write(m.arg(3), 4, 0);
                return m.ret(bytes, 0);
            }
            if (method.rfind("Get", 0) == 0) { uint32_t out = nargs > 1 ? m.arg(int(nargs - 1)) : 0; if (out >= 0x10000) m.mem.write(out, 4, 0); return m.ret(bytes, 0); }
            return m.ret(bytes, 0);
        };
        std::map<std::string, std::vector<uint32_t>> tables;
        for (auto& e : FMOD_EVENT_SLOTS) {
            auto& t = tables[e.cls]; if (t.size() < e.slot / 4 + 1) t.resize(e.slot / 4 + 1, 0);
            std::string cls = e.cls, method = e.method; uint32_t bytes = e.bytes;
            t[e.slot / 4] = native([call, cls, method, bytes](Machine& m) { return call(m, cls, method, bytes); });
        }
        for (auto& [cls, t] : tables) {
            if (cls == "EventSystem") continue;
            for (auto& a : t) if (!a) a = native([cls](Machine&) -> uint32_t { throw VmError("FMOD " + cls + ": unknown slot"); });
            uint32_t vt = halloc(process_heap, uint32_t(4 * t.size()), true); m.mem.write_block(vt, t.data(), 4 * t.size()); (*vtables)[cls] = vt;
        }
        // the EventSystem's slots (stand-in object 0xfa000000) take the same generic path
        for (auto& e : FMOD_EVENT_SLOTS) if (std::string(e.cls) == "EventSystem") {
            std::string method = e.method; uint32_t bytes = e.bytes;
            if (method == "Create") continue;
            m.natives[0xfa002000u + e.slot] = [call, method, bytes](Machine& m) { return call(m, "EventSystem", method, bytes); };
        }
        *make_obj = [this, objs, vtables](const std::string& cls) -> uint32_t {
            uint32_t o = halloc(process_heap, 32, true);
            auto it = vtables->find(cls);
            m.mem.write(o, 4, it != vtables->end() ? it->second : 0xfa003000u);   // classes without virtuals (Event, System): the trap table
            (*objs)[o] = {cls, 0}; on_undo([objs, o] { objs->erase(o); });
            return o;
        };
    }
}

