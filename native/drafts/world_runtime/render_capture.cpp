// Render capture (render_capture.h): Direct3D 9 state shadow over the boot's recording device, call log, per-draw
// state records and the software rasterizer (swr.h) feeding.
#include "render_capture.h"
#include "swr.h"
#include "vm.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <sys/stat.h>
#include <zlib.h>

namespace reflex_vm {
namespace {
uint64_t fnv(const void* p, size_t n) { const uint8_t* b = static_cast<const uint8_t*>(p); uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
std::string hex64(uint64_t v) { char b[24]; std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v); return b; }
float f32(uint32_t v) { float f; std::memcpy(&f, &v, 4); return f; }

void write_png(const std::string& path, uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba) {
    std::vector<uint8_t> raw; raw.reserve(size_t(h) * (w * 4 + 1));
    for (uint32_t y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgba.begin() + size_t(y) * w * 4, rgba.begin() + size_t(y + 1) * w * 4); }
    uLongf zn = compressBound(uLong(raw.size())); std::vector<uint8_t> z(zn); compress2(z.data(), &zn, raw.data(), uLong(raw.size()), 6); z.resize(zn);
    FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
    auto be = [](std::vector<uint8_t>& v, uint32_t x) { v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16)); v.push_back(uint8_t(x >> 8)); v.push_back(uint8_t(x)); };
    auto chunk = [&](const char* t, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> c; be(c, uint32_t(data.size())); c.insert(c.end(), t, t + 4); c.insert(c.end(), data.begin(), data.end());
        be(c, uint32_t(crc32(0, c.data() + 4, uInt(c.size() - 4)))); std::fwrite(c.data(), 1, c.size(), f);
    };
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr; be(ihdr, w); be(ihdr, h); ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk("IHDR", ihdr); chunk("IDAT", z); chunk("IEND", {}); std::fclose(f);
}

// CTAB (the blob after the 'CTAB' fourcc): name -> (register set, index, count, class, default value offset)
struct CtabEntry { std::string name; uint16_t set, index, count, cls = 0; uint32_t def = 0; };
std::vector<CtabEntry> ctab_blob(const uint8_t* b, size_t n, std::string* target = nullptr) {
    std::vector<CtabEntry> out;
    auto rd = [&](size_t o) { uint32_t v = 0; if (o + 4 <= n) std::memcpy(&v, b + o, 4); return v; };
    auto str = [&](size_t o) { std::string t; for (size_t k = o; k < n && b[k]; k++) t += char(b[k]); return t; };
    uint32_t count = rd(12), info = rd(16);
    if (target) *target = str(rd(24));
    for (uint32_t i = 0; i < count && info + 20 * i + 20 <= n; i++) {
        CtabEntry e; e.name = str(rd(info + 20 * i));
        uint32_t w1 = rd(info + 20 * i + 4), w2 = rd(info + 20 * i + 8), ty = rd(info + 20 * i + 12);
        e.set = uint16_t(w1); e.index = uint16_t(w1 >> 16); e.count = uint16_t(w2); e.def = rd(info + 20 * i + 16);
        if (ty + 2 <= n) std::memcpy(&e.cls, b + ty, 2);
        out.push_back(e);
    }
    return out;
}
std::vector<CtabEntry> ctab_of(const std::vector<uint32_t>& t) {
    for (size_t p = 1; p < t.size();) {
        uint32_t tok = t[p];
        if (tok == 0xffff) break;
        if ((tok & 0xffff) == 0xfffe) {
            uint32_t len = (tok >> 16) & 0x7fff;
            if (p + 1 < t.size() && t[p + 1] == 0x42415443u) return ctab_blob(reinterpret_cast<const uint8_t*>(&t[p + 2]), 4 * size_t(len - 1));
            p += len + 1; continue;
        }
        break;
    }
    return {};
}
const char* RS_NAMES(uint32_t r) {
    switch (r) {
        case 7: return "ZENABLE"; case 8: return "FILLMODE"; case 14: return "ZWRITE"; case 15: return "ALPHATEST"; case 19: return "SRCBLEND"; case 20: return "DESTBLEND";
        case 22: return "CULL"; case 23: return "ZFUNC"; case 24: return "ALPHAREF"; case 25: return "ALPHAFUNC"; case 27: return "BLEND"; case 28: return "FOG";
        case 52: return "STENCIL"; case 53: return "STENCILFAIL"; case 54: return "STENCILZFAIL"; case 55: return "STENCILPASS"; case 56: return "STENCILFUNC"; case 57: return "STENCILREF";
        case 58: return "STENCILMASK"; case 59: return "STENCILWRITEMASK"; case 168: return "COLORWRITE"; case 171: return "BLENDOP"; case 174: return "SCISSOR";
        case 175: return "SLOPEBIAS"; case 185: return "TWOSIDEDSTENCIL"; case 194: return "SRGBWRITE"; case 195: return "DEPTHBIAS"; case 206: return "SEPALPHA";
        case 207: return "SRCBLENDALPHA"; case 208: return "DESTBLENDALPHA"; case 209: return "BLENDOPALPHA"; case 193: return "BLENDFACTOR"; case 161: return "MSAA";
        case 156: return "POINTSPRITE"; case 154: return "POINTSIZE"; case 186: return "CCWSTENCILFAIL"; case 187: return "CCWSTENCILZFAIL"; case 188: return "CCWSTENCILPASS"; case 189: return "CCWSTENCILFUNC";
        case 190: return "COLORWRITE1"; case 191: return "COLORWRITE2"; case 192: return "COLORWRITE3"; case 136: return "CLIPPING"; case 152: return "CLIPPLANES";
    }
    return nullptr;
}
// The game compiles five small HLSL programs at run time (D3DXCompileShader; sources in MXReflex.exe at 0x56e3c0..0x56f078);
// the boot's stand-in returns a program without instructions. Hand-assembled equivalents (same operations as the HLSL).
namespace sm {
uint32_t D(uint32_t type, uint32_t reg, uint32_t mask = 15) { return 0x80000000u | ((type & 7) << 28) | ((type & 0x18) << 8) | (mask << 16) | reg; }
uint32_t S(uint32_t type, uint32_t reg, uint32_t swz = 0xe4) { return 0x80000000u | ((type & 7) << 28) | ((type & 0x18) << 8) | (swz << 16) | reg; }
uint32_t I(uint32_t op, uint32_t len) { return op | (len << 24); }
uint32_t F(float f) { uint32_t v; std::memcpy(&v, &f, 4); return v; }
std::vector<uint32_t> compiled_program(const std::vector<CtabEntry>& ct, bool ps) {
    auto has = [&](const char* n) { for (auto& e : ct) if (e.name == n) return true; return false; };
    const uint32_t XX = 0x00, YY = 0x55;
    if (ps && (has("fullscreen_texture") || has("screen_texture")))   // return tex2D(s0, Tex)
        return {0xffff0300u, I(31, 2), 0x80000005u, D(1, 0, 3), I(31, 2), 0x80000000u | (2u << 27), D(10, 0), I(66, 3), D(0, 0), S(1, 0), S(10, 0), I(1, 2), D(8, 0), S(0, 0), 0xffffu};
    if (!ps && has("g_HalfPixelOffset"))   // OutPos = (inPos + g_HalfPixelOffset.xy, 0, 1); OutTex = inPos * (.5, -.5) + .5
        return {0xfffe0300u, I(81, 5), D(2, 0), F(0.5f), F(-0.5f), F(0), F(1), I(31, 2), 0x80000000u, D(1, 0), I(31, 2), 0x80000000u, D(6, 0), I(31, 2), 0x80000005u, D(6, 1, 3),
                I(2, 3), D(6, 0, 3), S(1, 0), S(2, 200), I(1, 2), D(6, 0, 12), S(2, 0), I(4, 4), D(6, 1, 3), S(1, 0), S(2, 0), S(2, 0, XX), 0xffffu};
    if (!ps && has("g_TextureCoordScale"))   // ProjPos = (ObjPos, 0, 1); Tex0 = (ObjPos * (.5, -.5) + .5) * g_TextureCoordScale
        return {0xfffe0300u, I(81, 5), D(2, 1), F(0.5f), F(-0.5f), F(0), F(1), I(31, 2), 0x80000000u, D(1, 0), I(31, 2), 0x80000000u, D(6, 0), I(31, 2), 0x80000005u, D(6, 1, 3),
                I(1, 2), D(6, 0, 3), S(1, 0), I(1, 2), D(6, 0, 12), S(2, 1), I(4, 4), D(0, 0, 3), S(1, 0), S(2, 1), S(2, 1, XX), I(5, 3), D(6, 1, 3), S(0, 0), S(2, 0), 0xffffu};
    if (!ps && has("g_OverlayColor1"))   // ProjPos = (ObjPos, 0, 1); Tex0 as above; OverlayColor = lerp(c0, c1, Tex0.y)
        return {0xfffe0300u, I(81, 5), D(2, 2), F(0.5f), F(-0.5f), F(0), F(1), I(31, 2), 0x80000000u, D(1, 0), I(31, 2), 0x80000000u, D(6, 0), I(31, 2), 0x80000005u, D(6, 1, 3),
                I(31, 2), 0x80010005u, D(6, 2, 7), I(1, 2), D(6, 0, 3), S(1, 0), I(1, 2), D(6, 0, 12), S(2, 2), I(4, 4), D(0, 0, 3), S(1, 0), S(2, 2), S(2, 2, XX), I(1, 2), D(6, 1, 3), S(0, 0),
                I(18, 4), D(6, 2, 7), S(0, 0, YY), S(2, 1), S(2, 0), 0xffffu};
    return {};
}
}  // namespace sm
}  // namespace

struct RenderCaptureImpl {
    Boot& b; std::string dir;
    bool converge_adaptation = true; uint64_t nan_seen = 0; std::vector<uint32_t> id_buffer; uint32_t id_w = 0, id_h = 0;
    int raster = 0, capture_left = 0, frame_no = 0, captured = 0, compiled = 0;
    FILE* calls = nullptr; FILE* draws = nullptr;
    uint64_t seq = 0, draw_no = 0, frame_draw = 0;
    // shadow (object pointers as the game set them)
    uint32_t rs[256] = {}, samp[21][14] = {}, tex[21] = {}, vs = 0, ps = 0, decl = 0, fvf = 0, ib = 0, rt[4] = {}, dsurf = 0;
    struct Stream { uint32_t vb = 0, offset = 0, stride = 0, freq = 1; } streams[16];
    float vsf[256][4] = {}, psf[224][4] = {}; int32_t vsi[16][4] = {}, psi[16][4] = {}; uint32_t vsb[16] = {}, psb[16] = {};
    uint32_t vp[6] = {0, 0, 1280, 720, 0, 0x3f800000}; int32_t scissor[4] = {0, 0, 1280, 720};
    std::vector<uint32_t> args; std::string pending_label;
    std::map<std::string, uint64_t> counts;   // iface.method -> calls (whole run)
    std::map<uint32_t, std::string> labels;   // object -> asset name candidates seen on the creating call's stack
    // caches
    std::map<uint32_t, std::unique_ptr<swr::Shader>> shaders; std::map<uint32_t, uint64_t> shader_hash;
    std::map<uint32_t, std::vector<swr::VertexElement>> decls;
    struct Buf { uint32_t version = ~0u; std::vector<uint8_t> data; };
    std::map<uint32_t, Buf> bufs;
    struct Img { swr::Image img; uint32_t res_version = ~0u; bool rendered = false, dirty = false; uint32_t addr = 0; };
    std::map<std::pair<uint32_t, uint32_t>, std::unique_ptr<Img>> images;   // (container or surface, level index) -> image
    std::map<uint32_t, std::unique_ptr<swr::Texture>> textures;
    struct Depth { swr::Image img; std::vector<uint8_t> stencil; };
    std::map<uint32_t, std::unique_ptr<Depth>> depths;
    std::set<uint64_t> written_shaders;
    swr::Stats stats;
    std::string state_log;

    RenderCaptureImpl(Boot& boot, const std::string& d) : b(boot), dir(d) {
        if (const char* t = std::getenv("REFLEX_PIXEL_TRACE")) std::sscanf(t, "%d,%d", &stats.trace_x, &stats.trace_y);
        mkdir(dir.c_str(), 0755); mkdir((dir + "/shaders").c_str(), 0755); mkdir((dir + "/rt").c_str(), 0755);
        reset_defaults();
    }
    void reset_defaults() {
        std::memset(rs, 0, sizeof rs);
        auto R = [&](uint32_t k, uint32_t v) { rs[k] = v; };
        R(7, 1); R(8, 3); R(9, 2); R(14, 1); R(16, 1); R(19, 2); R(20, 1); R(22, 3); R(23, 4); R(25, 8); R(53, 1); R(54, 1); R(55, 1); R(56, 8);
        R(58, 0xffffffffu); R(59, 0xffffffffu); R(60, 0xffffffffu); R(136, 1); R(137, 1); R(141, 1); R(142, 1); R(161, 1); R(162, 0xffffffffu); R(154, 0x3f800000u);
        R(155, 0x3f800000u); R(166, 0x42800000u); R(168, 0xf); R(171, 1); R(186, 1); R(187, 1); R(188, 1); R(189, 8); R(190, 0xf); R(191, 0xf); R(192, 0xf);
        R(193, 0xffffffffu); R(207, 2); R(208, 1); R(209, 1);
        for (auto& s : samp) { uint32_t def[14] = {0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0}; std::memcpy(s, def, sizeof def); }
    }
    static int sampler_index(uint32_t s) { return s < 16 ? int(s) : (s >= 256 && s <= 260) ? int(16 + s - 256) : -1; }
    Machine& m() { return b.machine(); }

    // --------------------------------------------------------------- resources
    swr::Shader* shader(uint32_t o) {
        if (!o) return nullptr;
        auto it = shaders.find(o); if (it != shaders.end()) return it->second.get();
        Boot::D3DResource r; if (!b.d3d_resource(o, r) || r.bytes.size() < 8) return nullptr;
        auto sh = std::make_unique<swr::Shader>(); sh->tokens.resize(r.bytes.size() / 4); std::memcpy(sh->tokens.data(), r.bytes.data(), r.bytes.size());
        sh->ps = (sh->tokens[0] >> 16) == 0xffff; sh->major = (sh->tokens[0] >> 8) & 0xff; sh->minor = sh->tokens[0] & 0xff;
        if (!sh->decode()) std::fprintf(stderr, "shader %08x decode: %s\n", o, sh->error.c_str());
        uint64_t h = fnv(r.bytes.data(), r.bytes.size()); shader_hash[o] = h;
        if (written_shaders.insert(h).second) {
            std::string base = dir + "/shaders/" + hex64(h) + (sh->ps ? ".ps" : ".vs");
            FILE* f = std::fopen((base + ".bin").c_str(), "wb"); if (f) { std::fwrite(r.bytes.data(), 1, r.bytes.size(), f); std::fclose(f); }
            f = std::fopen((base + ".asm").c_str(), "w"); if (f) { auto l = labels.find(o); if (l != labels.end()) std::fprintf(f, "// created with %s\n", l->second.c_str()); std::fputs(swr::disassemble(*sh).c_str(), f); std::fclose(f); }
        }
        auto* p = sh.get(); shaders[o] = std::move(sh); return p;
    }
    std::vector<swr::VertexElement>* declaration(uint32_t o) {
        if (!o) return nullptr;
        auto it = decls.find(o); if (it != decls.end()) return &it->second;
        Boot::D3DResource r; if (!b.d3d_resource(o, r)) return nullptr;
        std::vector<swr::VertexElement> v;
        for (size_t i = 0; i + 8 <= r.bytes.size(); i += 8) {
            swr::VertexElement e; std::memcpy(&e.stream, &r.bytes[i], 2); std::memcpy(&e.offset, &r.bytes[i + 2], 2); e.type = r.bytes[i + 4]; e.method = r.bytes[i + 5]; e.usage = r.bytes[i + 6]; e.index = r.bytes[i + 7];
            if (e.stream == 0xff) break; v.push_back(e);
        }
        return &(decls[o] = v);
    }
    Buf* buffer(uint32_t o) {
        if (!o) return nullptr;
        Boot::D3DResource r; if (!b.d3d_resource(o, r)) return nullptr;
        Buf& bf = bufs[o];
        if (bf.version != r.version || bf.data.size() != r.size) { bf.data.resize(r.size); m().mem.read_block(r.data, bf.data.data(), r.size); bf.version = r.version; }
        return &bf;
    }
    // image of a surface object (a texture level or a standalone surface)
    Img* image_of_surface(uint32_t o) {
        Boot::D3DResource r; if (!b.d3d_resource(o, r)) return nullptr;
        if (r.container) { Boot::D3DResource c; b.d3d_resource(r.container, c); return image(r.container, c, r.level); }
        return image(o, r, 0);
    }
    Img* image(uint32_t owner, const Boot::D3DResource& r, uint32_t idx) {
        auto& slot = images[{owner, idx}];
        if (!slot) {
            slot = std::make_unique<Img>(); slot->img.format = r.format; slot->img.w = r.level_w[idx]; slot->img.h = r.level_h[idx]; slot->img.pitch = r.level_pitch[idx];
            slot->img.d = r.kind == 3 ? std::max(1u, r.depth >> (idx % std::max(1u, r.levels))) : 1; slot->addr = r.level_addr[idx];
            slot->img.bytes.resize(r.level_bytes[idx]);
        }
        Img& im = *slot;
        if (!im.rendered && im.res_version != r.version) { m().mem.read_block(im.addr, im.img.bytes.data(), im.img.bytes.size()); im.res_version = r.version; im.img.version++; }
        return &im;
    }
    swr::Texture* texture(uint32_t o) {
        if (!o) return nullptr;
        Boot::D3DResource r; if (!b.d3d_resource(o, r)) return nullptr;
        if (r.kind != 1 && r.kind != 2 && r.kind != 3) return nullptr;
        auto& t = textures[o]; if (!t) { t = std::make_unique<swr::Texture>(); t->kind = r.kind; t->levels = r.levels; }
        t->level.resize(r.level_addr.size());
        for (uint32_t i = 0; i < r.level_addr.size(); i++) t->level[i] = &image(o, r, i)->img;
        return t.get();
    }
    void flush(Img& im) { if (im.dirty) { m().mem.write_block(im.addr, im.img.bytes.data(), im.img.bytes.size()); im.dirty = false; } }
    void flush_object(uint32_t o) {   // the game is about to read o's memory
        Boot::D3DResource r; if (!b.d3d_resource(o, r)) return;
        uint32_t owner = r.container ? r.container : o;
        for (auto& [k, im] : images) if (k.first == owner) flush(*im);
    }
    Depth* depth_of(uint32_t o) {
        if (!o) return nullptr;
        auto& d = depths[o];
        if (!d) { Boot::D3DResource r; if (!b.d3d_resource(o, r)) return nullptr; d = std::make_unique<Depth>(); d->img.format = 114; d->img.w = r.level_w.empty() ? r.width : r.level_w[0]; d->img.h = r.level_h.empty() ? r.height : r.level_h[0];
            d->img.pitch = d->img.w * 4; d->img.bytes.assign(size_t(d->img.pitch) * d->img.h, 0); d->stencil.assign(size_t(d->img.w) * d->img.h, 0); float one = 1; for (size_t i = 0; i < size_t(d->img.w) * d->img.h; i++) std::memcpy(&d->img.bytes[4 * i], &one, 4); }
        return d.get();
    }

    // --------------------------------------------------------------- dumps
    void dump_image(const swr::Image& im, const std::string& name) {
        std::vector<uint8_t> rgba(size_t(im.w) * im.h * 4);
        bool fl = im.format == 114 || im.format == 111 || im.format == 112 || im.format == 113 || im.format == 115 || im.format == 116;
        float lo = 0, hi = 1;
        if (fl && (im.format == 114 || im.format == 111)) {   // single channel float: normalise to the range
            lo = 1e30f; hi = -1e30f;
            for (uint32_t y = 0; y < im.h; y += 4) for (uint32_t x = 0; x < im.w; x += 4) { float v = swr::load_texel(im, x, y)[0]; if (std::isfinite(v)) { lo = std::min(lo, v); hi = std::max(hi, v); } }
            if (hi <= lo) hi = lo + 1;
        }
        for (uint32_t y = 0; y < im.h; y++) for (uint32_t x = 0; x < im.w; x++) {
            auto t = swr::load_texel(im, x, y); uint8_t* p = &rgba[(size_t(y) * im.w + x) * 4];
            for (int c = 0; c < 4; c++) { float v = (t[c] - lo) / (hi - lo); if (!(v >= 0)) v = 0; if (v > 1) v = 1; p[c] = uint8_t(v * 255.0f + 0.5f); }
            if (fl && (im.format == 114 || im.format == 111)) { p[1] = p[2] = p[0]; p[3] = 255; }
            if (im.format == 22 || im.format == 23) p[3] = 255;
        }
        write_png(dir + "/rt/" + name + ".png", im.w, im.h, rgba);
        FILE* f = std::fopen((dir + "/rt/" + name + ".raw").c_str(), "wb");
        if (f) { uint32_t hd[4] = {im.format, im.w, im.h, im.pitch}; std::fwrite(hd, 4, 4, f); std::fwrite(im.bytes.data(), 1, im.bytes.size(), f); std::fclose(f); }
    }

    // --------------------------------------------------------------- the hook
    void log_line(const std::string& s) { if (calls) { std::fputs(s.c_str(), calls); std::fputc('\n', calls); } }
    std::string stack_names() {   // asset-like strings referenced from the caller's stack (labels of created objects)
        Machine& mm = m(); std::string out; std::set<std::string> seen;
        for (uint32_t a = mm.r[4], i = 0; i < 0x300 && seen.size() < 4; i++, a += 4) {
            if (!mm.mem.mapped(a)) break;
            uint32_t p = uint32_t(mm.mem.read(a, 4)); if (p < 0x10000 || !mm.mem.mapped(p) || !mm.mem.mapped(p + 63)) continue;
            std::string t; for (uint32_t k = 0; k < 64; k++) { uint8_t c = uint8_t(mm.mem.read(p + k, 1)); if (!c) break; if (c < 32 || c > 126) { t.clear(); break; } t += char(c); }
            for (char& ch : t) if (ch == '\\' || ch == '"') ch = '/';
            if (t.size() >= 5 && t.size() < 64 && (t.find('_') != std::string::npos || t.find('.') != std::string::npos) && std::isalpha((unsigned char)t[0]) && seen.insert(t).second) out += (out.empty() ? "" : "|") + t;
        }
        return out;
    }
    void hook(Machine& mm, const std::string& iface, uint32_t self, const std::string& mt, bool after) {
        if (!after) {
            counts[iface + "." + mt]++;
            args.assign(14, 0); for (int i = 1; i < 14; i++) args[i] = mm.mem.mapped(mm.r[4] + 4 + 4 * i) ? mm.arg(i) : 0;
            if (iface == "Device" && (mt == "CreateTexture" || mt == "CreateCubeTexture" || mt == "CreateVolumeTexture" || mt == "CreateVertexShader" || mt == "CreatePixelShader" || mt == "CreateRenderTarget" || mt == "CreateVertexBuffer"))
                pending_label = stack_names();
            if (iface == "Device") pre_device(mt);
            else if (iface == "StateBlock") { if (capture_left) log_line("StateBlock." + mt + " (not modelled)"); counts["!stateblock"]++; }
            else if (iface == "ConstantTable" && mt.rfind("Set", 0) == 0) constant_table_set(self, mt);
            else if ((iface == "Surface" || iface == "Texture") && mt == "LockRect") flush_object(self);
            else if (iface == "SwapChain" && mt == "Present") present();
            return;
        }
        if (iface == "Device") post_device(mt);
        (void)self;
    }
    // ID3DXConstantTable::Set* (the boot's stand-in drops them): the register writes D3DX makes through the device
    void constant_table_set(uint32_t self, const std::string& mt) {
        Boot::D3DResource r; if (!b.d3d_resource(self, r) || r.kind != 16) { counts["!consttable-unknown"]++; return; }
        std::string target; auto ct = ctab_blob(r.bytes.data(), r.bytes.size(), &target);
        bool ps = target.rfind("ps", 0) == 0;
        Machine& mm = m(); auto& a = args;
        auto entry = [&](uint32_t h) -> const CtabEntry* {
            if (h >= 0x40000000u && h < 0x40000000u + ct.size()) return &ct[h - 0x40000000u];
            std::string n; for (uint32_t p = h; p && mm.mem.mapped(p) && n.size() < 128; p++) { char c = char(mm.mem.read(p, 1)); if (!c) break; n += c; }
            for (auto& e : ct) if (e.name == n) return &e;
            return nullptr;
        };
        auto setf = [&](uint32_t reg, const float* v) { if (ps ? reg < 224 : reg < 256) std::memcpy(ps ? psf[reg] : vsf[reg], v, 16); };
        if (mt == "SetDefaults") {
            for (auto& e : ct) if (e.def && e.set == 2) for (uint32_t i = 0; i < e.count; i++) { float v[4] = {}; if (e.def + 16 * (i + 1) <= r.bytes.size()) std::memcpy(v, r.bytes.data() + e.def + 16 * i, 16); setf(e.index + i, v); }
            counts["!consttable-defaults"]++; return;
        }
        const CtabEntry* e = entry(a[2]); if (!e) { counts["!consttable-nohandle"]++; return; }
        counts["!consttable-" + mt]++;
        if (capture_left) { char l[160]; std::snprintf(l, sizeof l, "  ConstantTable.%s %s %s set %u reg %u n %u", mt.c_str(), ps ? "ps" : "vs", e->name.c_str(), e->set, e->index, e->count); log_line(l); }
        if (mt == "SetFloat" || mt == "SetInt" || mt == "SetBool") {
            float f = mt == "SetFloat" ? f32(a[3]) : float(int32_t(a[3]));
            if (mt == "SetBool") f = a[3] ? 1.f : 0.f;
            if (e->set == 2) { float v[4] = {f, 0, 0, 0}; setf(e->index, v); }
            else if (e->set == 0 && e->index < 16) (ps ? psb : vsb)[e->index] = a[3] ? 1 : 0;
            else if (e->set == 1 && e->index < 16) { int32_t* d = ps ? psi[e->index] : vsi[e->index]; d[0] = int32_t(a[3]); }
            return;
        }
        if (mt == "SetVector" || mt == "SetVectorArray" || mt == "SetFloatArray" || mt == "SetValue") {
            uint32_t n = mt == "SetVector" ? 1 : mt == "SetVectorArray" ? a[4] : mt == "SetFloatArray" ? (a[4] + 3) / 4 : a[4] / 16;
            n = std::min<uint32_t>(n, e->count);
            for (uint32_t i = 0; i < n; i++) {
                float v[4] = {}; uint32_t bytes = mt == "SetFloatArray" ? std::min<uint32_t>(16, 4 * (a[4] - 4 * i)) : 16;
                mm.mem.read_bytes(a[3] + 16 * i, v, bytes);
                if (e->set == 2) setf(e->index + i, v);
                else if (e->set == 0 && e->index + i < 16) (ps ? psb : vsb)[e->index + i] = v[0] != 0;
            }
            return;
        }
        if (mt == "SetMatrix" || mt == "SetMatrixTranspose" || mt == "SetMatrixArray") {
            float mtx[16]; mm.mem.read_bytes(a[3], mtx, 64);
            bool columns = e->cls == 3; if (mt == "SetMatrixTranspose") columns = !columns;
            for (uint32_t i = 0; i < e->count && i < 4; i++) { float v[4]; for (int k = 0; k < 4; k++) v[k] = columns ? mtx[4 * k + i] : mtx[4 * i + k]; setf(e->index + i, v); }
            return;
        }
        counts["!consttable-unhandled-" + mt]++;
    }
    uint32_t outobj(int argi) { return args[argi] && m().mem.mapped(args[argi]) ? uint32_t(m().mem.read(args[argi], 4)) : 0; }
    void post_device(const std::string& mt) {
        int out = mt == "CreateTexture" || mt == "CreateRenderTarget" || mt == "CreateDepthStencilSurface" ? 7 : mt == "CreateVolumeTexture" ? 8 : mt == "CreateCubeTexture" ? 6 :
                  mt == "CreateVertexBuffer" || mt == "CreateIndexBuffer" || mt == "CreateOffscreenPlainSurface" ? 5 : mt == "CreateVertexShader" || mt == "CreatePixelShader" || mt == "CreateVertexDeclaration" ? 2 : 0;
        if (out) {
            uint32_t o = outobj(out);
            if (o && !pending_label.empty()) labels[o] = pending_label;
            if (o && (mt == "CreateVertexShader" || mt == "CreatePixelShader") && args[1] > 0x100) {   // shader resource entry: [kind][name length][name][size][bytecode]
                Machine& mm = m(); uint32_t p = args[1];
                if (mm.mem.mapped(p - 64)) {
                    uint32_t q = p - 6; std::string nm;
                    while (q > p - 260) { uint8_t c = uint8_t(mm.mem.read(q, 1)); if (!(std::isalnum(c) || c == '_')) break; nm.insert(nm.begin(), char(c)); q--; }
                    if (!nm.empty() && uint32_t(mm.mem.read(q - 3, 4)) == nm.size() + 1) labels[o] = nm + " @ " + labels[o];
                }
            }
            if (o && (mt == "CreateVertexShader" || mt == "CreatePixelShader")) {   // run-time compiled programs: the hand-assembled equivalent
                Boot::D3DResource r;
                if (b.d3d_resource(o, r) && r.bytes.size() > 8 && std::string(reinterpret_cast<const char*>(r.bytes.data()), r.bytes.size()).find("Reflex boot (stand-in") != std::string::npos) {
                    std::vector<uint32_t> tk(r.bytes.size() / 4); std::memcpy(tk.data(), r.bytes.data(), tk.size() * 4);
                    auto prog = sm::compiled_program(ctab_of(tk), mt == "CreatePixelShader");
                    if (!prog.empty()) {
                        auto sh = std::make_unique<swr::Shader>(); sh->tokens = prog; sh->ps = mt == "CreatePixelShader"; sh->major = 3; sh->minor = 0;
                        if (!sh->decode()) std::fprintf(stderr, "compiled program decode: %s\n", sh->error.c_str());
                        uint64_t h = fnv(prog.data(), prog.size() * 4); shader_hash[o] = h; labels[o] = "D3DXCompileShader";
                        if (FILE* f = std::fopen((dir + "/shaders/" + hex64(h) + (sh->ps ? ".ps" : ".vs") + ".asm").c_str(), "w")) { std::fputs("// run-time compiled (hand-assembled from the exe's HLSL)\n", f); std::fputs(swr::disassemble(*sh).c_str(), f); std::fclose(f); }
                        written_shaders.insert(h); shaders[o] = std::move(sh); counts["!compiled-program"]++;
                    } else counts["!compiled-unknown"]++;
                }
            }
            if (capture_left) { char b2[160]; std::snprintf(b2, sizeof b2, "  -> %08x %s", o, pending_label.c_str()); log_line(b2); }
        }
        pending_label.clear();
        if (mt == "UpdateSurface" || mt == "UpdateTexture") copy_update(mt);
    }
    void copy_update(const std::string& mt) {
        // the recording device does not copy; the rasterizer's view of the destination is the source's contents
        Boot::D3DResource s, d; if (!b.d3d_resource(args[1], s) || !b.d3d_resource(mt == "UpdateSurface" ? args[3] : args[2], d)) return;
        counts["!update-copied"]++;
        auto lvls = [&](const Boot::D3DResource& r, uint32_t o, std::vector<Img*>& v) {
            if (r.kind == 4) { if (Img* im = image_of_surface(o)) v.push_back(im); return; }
            for (uint32_t i = 0; i < r.level_addr.size(); i++) v.push_back(image(o, r, i));
        };
        std::vector<Img*> sv, dv; lvls(s, args[1], sv); lvls(d, mt == "UpdateSurface" ? args[3] : args[2], dv);
        size_t skip = sv.size() > dv.size() ? sv.size() - dv.size() : 0;
        for (size_t i = 0; i < dv.size() && i + skip < sv.size(); i++) if (sv[i + skip]->img.bytes.size() == dv[i]->img.bytes.size()) { dv[i]->img.bytes = sv[i + skip]->img.bytes; dv[i]->rendered = true; dv[i]->dirty = true; dv[i]->img.version++; }
    }
    void set_rt(uint32_t i, uint32_t o) {
        if (capture_left && rt[i] != o && rt[i]) pass_end(i);
        rt[i] = o;
        if (i == 0 && o) {   // D3D9: SetRenderTarget(0) resets the viewport and the scissor rect to the target
            Boot::D3DResource r; if (b.d3d_resource(o, r)) { uint32_t w = r.level_w.empty() ? r.width : r.level_w[0], h = r.level_h.empty() ? r.height : r.level_h[0];
                if (r.container) { Boot::D3DResource c; b.d3d_resource(r.container, c); w = c.level_w[r.level]; h = c.level_h[r.level]; }
                vp[0] = 0; vp[1] = 0; vp[2] = w; vp[3] = h; vp[4] = 0; vp[5] = 0x3f800000; scissor[0] = 0; scissor[1] = 0; scissor[2] = int32_t(w); scissor[3] = int32_t(h); }
        }
    }
    uint64_t pass_no = 0; uint64_t pass_draws = 0;
    void pass_end(uint32_t i) {
        if (!pass_draws) return;
        if (Img* im = image_of_surface(rt[i])) { char n[64]; std::snprintf(n, sizeof n, "f%d_p%03llu_rt%u_%08x", frame_no, (unsigned long long)pass_no, i, rt[i]); dump_image(im->img, n); }
        if (i == 0) { pass_no++; pass_draws = 0; }
    }
    std::vector<float> floats(uint32_t p, uint32_t n) { std::vector<float> v(n); if (p) m().mem.read_bytes(p, v.data(), 4 * n); return v; }
    void pre_device(const std::string& mt) {
        auto& a = args; Machine& mm = m();
        if (capture_left) {
            uint32_t ret = uint32_t(mm.mem.read(mm.r[4], 4));
            char line[256]; std::snprintf(line, sizeof line, "%llu %s %08x %08x %08x %08x %08x %08x <%08x", (unsigned long long)seq, mt.c_str(), a[1], a[2], a[3], a[4], a[5], a[6], ret); log_line(line);
            if (mt == "SetVertexShaderConstantF" || mt == "SetPixelShaderConstantF") {
                std::string l = "   "; char b2[64];
                for (uint32_t i = 0; i < a[3] && i < 8; i++) { float v[4]; mm.mem.read_bytes(a[2] + 16 * i, v, 16); std::snprintf(b2, sizeof b2, " [%g %g %g %g]", v[0], v[1], v[2], v[3]); l += b2; }
                log_line(l);
            }
        }
        seq++;
        if (mt == "SetRenderState") { if (a[1] < 256) rs[a[1]] = a[2]; return; }
        if (mt == "SetSamplerState") { int si = sampler_index(a[1]); if (si >= 0 && a[2] < 14) samp[si][a[2]] = a[3]; return; }
        if (mt == "SetTexture") { int si = sampler_index(a[1]); if (si >= 0) tex[si] = a[2]; return; }
        if (mt == "SetVertexShader") { vs = a[1]; return; }
        if (mt == "SetPixelShader") { ps = a[1]; return; }
        if (mt == "SetVertexDeclaration") { decl = a[1]; return; }
        if (mt == "SetFVF") { fvf = a[1]; decl = 0; return; }
        if (mt == "SetIndices") { ib = a[1]; return; }
        if (mt == "SetStreamSource") { if (a[1] < 16) streams[a[1]] = {a[2], a[3], a[4], streams[a[1]].freq}; return; }
        if (mt == "SetStreamSourceFreq") { if (a[1] < 16) streams[a[1]].freq = a[2]; return; }
        if (mt == "SetViewport") { mm.mem.read_bytes(a[1], vp, 24); return; }
        if (mt == "SetScissorRect") { mm.mem.read_bytes(a[1], scissor, 16); return; }
        if (mt == "SetRenderTarget") { if (a[1] < 4) set_rt(a[1], a[2]); return; }
        if (mt == "SetDepthStencilSurface") { dsurf = a[1]; return; }
        if (mt == "SetVertexShaderConstantF" || mt == "SetPixelShaderConstantF") {
            bool v = mt[3] == 'V'; uint32_t start = a[1], n = a[3], lim = v ? 256 : 224;
            for (uint32_t i = 0; i < n && start + i < lim; i++) mm.mem.read_bytes(a[2] + 16 * i, v ? vsf[start + i] : psf[start + i], 16);
            return;
        }
        if (mt == "SetVertexShaderConstantI" || mt == "SetPixelShaderConstantI") { bool v = mt[3] == 'V'; for (uint32_t i = 0; i < a[3] && a[1] + i < 16; i++) mm.mem.read_bytes(a[2] + 16 * i, v ? vsi[a[1] + i] : psi[a[1] + i], 16); return; }
        if (mt == "SetVertexShaderConstantB" || mt == "SetPixelShaderConstantB") { bool v = mt[3] == 'V'; for (uint32_t i = 0; i < a[3] && a[1] + i < 16; i++) (v ? vsb : psb)[a[1] + i] = uint32_t(mm.mem.read(a[2] + 4 * i, 4)); return; }
        if (mt == "Clear") { do_clear(); return; }
        if (mt == "DrawPrimitive" || mt == "DrawIndexedPrimitive" || mt == "DrawPrimitiveUP" || mt == "DrawIndexedPrimitiveUP") { do_draw(mt); return; }
        if (mt == "StretchRect") { do_stretch(); return; }
        if (mt == "ColorFill") { if (Img* im = image_of_surface(a[1])) { float c[4] = {((a[3] >> 16) & 255) / 255.f, ((a[3] >> 8) & 255) / 255.f, (a[3] & 255) / 255.f, (a[3] >> 24) / 255.f};
            for (uint32_t y = 0; y < im->img.h; y++) for (uint32_t x = 0; x < im->img.w; x++) swr::store_texel(im->img, x, y, c); im->rendered = im->dirty = true; im->img.version++; } return; }
        if (mt == "GetRenderTargetData" || mt == "GetFrontBufferData") { flush_object(a[1]); return; }
        if (mt == "Present") { present(); return; }
        if (mt == "BeginStateBlock" || mt == "CreateStateBlock") { counts["!stateblock"]++; return; }
    }
    swr::State build_state(bool& ok) {
        swr::State s; ok = true;
        std::memcpy(s.rs, rs, sizeof rs);
        for (int i = 0; i < 21; i++) { std::memcpy(s.samp[i].s, samp[i], sizeof samp[i]); s.tex[i] = texture(tex[i]); }
        s.vs = shader(vs); s.ps = shader(ps);
        std::memcpy(s.vsf, vsf, sizeof vsf); std::memcpy(s.psf, psf, sizeof psf); std::memcpy(s.vsi, vsi, sizeof vsi); std::memcpy(s.psi, psi, sizeof psi);
        std::memcpy(s.vsb, vsb, sizeof vsb); std::memcpy(s.psb, psb, sizeof psb);
        if (auto* d = declaration(decl)) s.decl = *d; else ok = false;
        for (int i = 0; i < 16; i++) if (streams[i].vb) { if (Buf* bf = buffer(streams[i].vb)) { s.streams[i].data = bf->data.data(); s.streams[i].size = bf->data.size(); } s.streams[i].offset = streams[i].offset; s.streams[i].stride = streams[i].stride; s.streams[i].freq = streams[i].freq; }
        if (ib) { Boot::D3DResource r; if (Buf* bf = buffer(ib)) { s.indices = bf->data.data(); s.index_size = bf->data.size(); } if (b.d3d_resource(ib, r)) s.index_format = r.format; }
        for (int i = 0; i < 4; i++) if (rt[i]) { if (Img* im = image_of_surface(rt[i])) { s.rt[i] = &im->img; } }
        if (Depth* d = depth_of(dsurf)) { s.depth = &d->img; s.stencil = &d->stencil; }
        s.vp[0] = vp[0]; s.vp[1] = vp[1]; s.vp[2] = vp[2]; s.vp[3] = vp[3]; s.vp_minz = f32(vp[4]); s.vp_maxz = f32(vp[5]);
        std::memcpy(s.scissor, scissor, 16);
        if (!s.vs || !s.ps) ok = false;
        return s;
    }
    void mark_rendered() { for (int i = 0; i < 4; i++) if (rt[i]) if (Img* im = image_of_surface(rt[i])) { im->rendered = true; im->dirty = true; im->img.version++; } }
    bool backbuffer_target() { Boot::D3DResource r; return rt[0] && b.d3d_resource(rt[0], r) && r.label == "backbuffer"; }
    void do_clear() {
        bool rast = raster == 2 || (raster == 1 && !backbuffer_target());
        if (!rast) return;
        bool ok; swr::State s = build_state(ok);
        std::vector<std::array<int32_t, 4>> rects;
        for (uint32_t i = 0; i < args[1] && args[2]; i++) { std::array<int32_t, 4> r; m().mem.read_bytes(args[2] + 16 * i, r.data(), 16); rects.push_back(r); }
        swr::clear(s, args[3], args[4], f32(args[5]), args[6], rects);
        if (args[3] & 1) mark_rendered();
    }
    void do_stretch() {
        Img* src = image_of_surface(args[1]); Img* dst = image_of_surface(args[3]);
        if (!src || !dst) { Depth* sd = depth_of(args[1]); Depth* dd = depth_of(args[3]); if (sd && dd && sd->img.bytes.size() == dd->img.bytes.size()) { dd->img.bytes = sd->img.bytes; dd->stencil = sd->stencil; } return; }
        int32_t sr[4] = {0, 0, int32_t(src->img.w), int32_t(src->img.h)}, dr[4] = {0, 0, int32_t(dst->img.w), int32_t(dst->img.h)};
        if (args[2]) m().mem.read_bytes(args[2], sr, 16);
        if (args[4]) m().mem.read_bytes(args[4], dr, 16);
        bool linear = args[5] == 2;
        for (int32_t y = dr[1]; y < dr[3]; y++) for (int32_t x = dr[0]; x < dr[2]; x++) {
            float u = sr[0] + (x - dr[0] + 0.5f) * float(sr[2] - sr[0]) / float(dr[2] - dr[0]), v = sr[1] + (y - dr[1] + 0.5f) * float(sr[3] - sr[1]) / float(dr[3] - dr[1]);
            std::array<float, 4> c{};
            if (linear) {
                float fx = u - 0.5f, fy = v - 0.5f; int x0 = int(std::floor(fx)), y0 = int(std::floor(fy)); float ax = fx - x0, ay = fy - y0;
                auto L = [&](int xx, int yy) { xx = std::clamp(xx, 0, int(src->img.w) - 1); yy = std::clamp(yy, 0, int(src->img.h) - 1); return swr::load_texel(src->img, uint32_t(xx), uint32_t(yy)); };
                auto a0 = L(x0, y0), a1 = L(x0 + 1, y0), b0 = L(x0, y0 + 1), b1 = L(x0 + 1, y0 + 1);
                for (int k = 0; k < 4; k++) c[k] = (a0[k] * (1 - ax) + a1[k] * ax) * (1 - ay) + (b0[k] * (1 - ax) + b1[k] * ax) * ay;
            } else c = swr::load_texel(src->img, uint32_t(std::clamp(int(u), 0, int(src->img.w) - 1)), uint32_t(std::clamp(int(v), 0, int(src->img.h) - 1)));
            swr::store_texel(dst->img, uint32_t(x), uint32_t(y), c.data());
        }
        dst->rendered = dst->dirty = true; dst->img.version++;
        if (capture_left) { char n[64]; std::snprintf(n, sizeof n, "f%d_s%03llu_stretch_%08x", frame_no, (unsigned long long)seq, args[3]); dump_image(dst->img, n); }
    }
    void present() {
        if (capture_left) {
            pass_end(0);
            Boot::D3DResource r; uint32_t bb = 0;
            for (auto& [k, im] : images) { if (b.d3d_resource(k.first, r) && r.label == "backbuffer") { bb = k.first; char n[64]; std::snprintf(n, sizeof n, "f%d_backbuffer", frame_no); dump_image(im->img, n); } }
            (void)bb;
            if (!id_buffer.empty()) { char n[96]; std::snprintf(n, sizeof n, "/rt/f%d_ids.raw", frame_no); if (FILE* f = std::fopen((dir + n).c_str(), "wb")) { uint32_t hd[4] = {0, id_w, id_h, id_w * 4}; std::fwrite(hd, 4, 4, f); std::fwrite(id_buffer.data(), 4, id_buffer.size(), f); std::fclose(f); } id_buffer.clear(); }
            std::fprintf(stderr, "capture frame %d: %llu draws, %llu prims, %llu pixels shaded %s\n", frame_no, (unsigned long long)frame_draw, (unsigned long long)stats.prims,
                         (unsigned long long)stats.shaded, stats.last_error.c_str());
            capture_left--; captured++;
            if (!capture_left) { if (calls) std::fclose(calls), calls = nullptr; if (draws) std::fclose(draws), draws = nullptr; }
        }
        frame_no++; frame_draw = 0; particles_seen = false;
        if (capture_left) open_logs();
    }
    void open_logs() {
        if (calls) return;
        char n[64]; std::snprintf(n, sizeof n, "/calls_f%d.txt", frame_no); calls = std::fopen((dir + n).c_str(), "w");
        std::snprintf(n, sizeof n, "/draws_f%d.jsonl", frame_no); draws = std::fopen((dir + n).c_str(), "w");
        pass_no = 0; pass_draws = 0;
    }
    std::string json_draw(const std::string& mt) {
        std::string j; char t[256];
        auto& a = args;
        std::snprintf(t, sizeof t, "{\"draw\":%llu,\"seq\":%llu,\"call\":\"%s\",\"args\":[%u,%d,%u,%u,%u,%u]", (unsigned long long)frame_draw, (unsigned long long)seq, mt.c_str(), a[1], int32_t(a[2]), a[3], a[4], a[5], a[6]); j += t;
        shader(vs); shader(ps);
        std::snprintf(t, sizeof t, ",\"vs\":\"%s\",\"ps\":\"%s\"", vs ? hex64(shader_hash[vs]).c_str() : "", ps ? hex64(shader_hash[ps]).c_str() : ""); j += t;
        auto lab = [&](uint32_t o) { auto it = labels.find(o); return it == labels.end() ? std::string() : it->second; };
        j += ",\"rt\":[";
        for (int i = 0; i < 4; i++) { Boot::D3DResource r; if (!rt[i] || !b.d3d_resource(rt[i], r)) continue; uint32_t own = r.container ? r.container : rt[i]; Boot::D3DResource c = r; if (r.container) b.d3d_resource(r.container, c);
            std::snprintf(t, sizeof t, "%s{\"i\":%d,\"obj\":\"%08x\",\"fmt\":%u,\"w\":%u,\"h\":%u,\"label\":\"%s\"}", i ? "," : "", i, own, c.format, c.level_w.empty() ? c.width : c.level_w[r.container ? r.level : 0], c.level_h.empty() ? c.height : c.level_h[r.container ? r.level : 0], r.label.empty() ? lab(own).c_str() : r.label.c_str()); j += t; }
        j += "]";
        { Boot::D3DResource r; if (dsurf && b.d3d_resource(dsurf, r)) { std::snprintf(t, sizeof t, ",\"ds\":{\"obj\":\"%08x\",\"fmt\":%u,\"w\":%u,\"h\":%u}", dsurf, r.format, r.width, r.height); j += t; } }
        std::snprintf(t, sizeof t, ",\"vp\":[%u,%u,%u,%u,%g,%g],\"scissor\":[%d,%d,%d,%d]", vp[0], vp[1], vp[2], vp[3], f32(vp[4]), f32(vp[5]), scissor[0], scissor[1], scissor[2], scissor[3]); j += t;
        j += ",\"rs\":{";
        bool first = true;
        for (uint32_t k : {7u, 8u, 14u, 15u, 19u, 20u, 22u, 23u, 24u, 25u, 27u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u, 168u, 171u, 174u, 175u, 185u, 194u, 195u, 206u, 207u, 208u, 209u, 193u, 136u, 152u, 190u}) {
            std::snprintf(t, sizeof t, "%s\"%s\":%u", first ? "" : ",", RS_NAMES(k), rs[k]); j += t; first = false; }
        j += "}";
        j += ",\"tex\":[";
        first = true;
        for (int i = 0; i < 21; i++) {
            if (!tex[i]) continue; Boot::D3DResource r; if (!b.d3d_resource(tex[i], r)) continue;
            std::snprintf(t, sizeof t, "%s{\"s\":%d,\"obj\":\"%08x\",\"kind\":%d,\"fmt\":%u,\"w\":%u,\"h\":%u,\"levels\":%u,\"usage\":%u,\"label\":\"%s\",\"ss\":[%u,%u,%u,%u,%u,%u,%u,%g,%u,%u,%u]}", first ? "" : ",", i, tex[i], r.kind, r.format, r.width, r.height, r.levels, r.usage,
                          lab(tex[i]).c_str(), samp[i][1], samp[i][2], samp[i][3], samp[i][4], samp[i][5], samp[i][6], samp[i][7], f32(samp[i][8]), samp[i][9], samp[i][10], samp[i][11]); j += t; first = false;
        }
        j += "]";
        // constants named by the shaders' CTAB
        for (int which = 0; which < 2; which++) {
            swr::Shader* sh = shader(which ? ps : vs); if (!sh) continue;
            j += which ? ",\"psc\":{" : ",\"vsc\":{";
            first = true;
            for (auto& e : ctab_of(sh->tokens)) {
                std::snprintf(t, sizeof t, "%s\"%s\":{\"set\":%u,\"reg\":%u,\"n\":%u,\"v\":[", first ? "" : ",", e.name.c_str(), e.set, e.index, e.count); j += t; first = false;
                for (uint32_t r = 0; r < e.count && r < 16; r++) {
                    uint32_t ri = e.index + r;
                    if (e.set == 2) { const float* f = which ? (ri < 224 ? psf[ri] : nullptr) : (ri < 256 ? vsf[ri] : nullptr); if (!f) continue; std::snprintf(t, sizeof t, "%s[%.9g,%.9g,%.9g,%.9g]", r ? "," : "", f[0], f[1], f[2], f[3]); }
                    else if (e.set == 1) { const int32_t* f = which ? psi[ri & 15] : vsi[ri & 15]; std::snprintf(t, sizeof t, "%s[%d,%d,%d,%d]", r ? "," : "", f[0], f[1], f[2], f[3]); }
                    else if (e.set == 0) std::snprintf(t, sizeof t, "%s%u", r ? "," : "", (which ? psb : vsb)[ri & 15]);
                    else std::snprintf(t, sizeof t, "%s%u", r ? "," : "", ri);
                    j += t;
                }
                j += "]}";
            }
            j += "}";
        }
        j += ",\"streams\":[";
        first = true;
        for (int i = 0; i < 16; i++) if (streams[i].vb) { std::snprintf(t, sizeof t, "%s[%d,\"%08x\",%u,%u,%u]", first ? "" : ",", i, streams[i].vb, streams[i].offset, streams[i].stride, streams[i].freq); j += t; first = false; }
        j += "],\"decl\":[";
        if (auto* d = declaration(decl)) { first = true; for (auto& e : *d) { std::snprintf(t, sizeof t, "%s[%u,%u,%u,%u,%u]", first ? "" : ",", e.stream, e.offset, e.type, e.usage, e.index); j += t; first = false; } }
        j += "]}";
        return j;
    }
    // REFLEX_STATIC_ONLY: reference images of the static world only (what the Unity comparison renders): no draws into the
    // dynamic shadow maps, no characters / crowds / riders / vehicles / particles / tyre tracks
    bool static_only = std::getenv("REFLEX_STATIC_ONLY") != nullptr;
    bool dynamic_draw() {
        for (int i = 0; i < 1 && rt[0]; i++) { Boot::D3DResource r; if (b.d3d_resource(rt[0], r)) { Boot::D3DResource c = r; if (r.container) b.d3d_resource(r.container, c);
            if (c.format == 114 && c.width == 768 && dsurf) return true; } }   // the dynamic shadow maps (R32F 768 x 640 / 384 with depth)
        static const char* keys[] = {"Crowd", "Body", "Rider", "Jersey", "Helmet", "Goggle", "Lens", "Chestprt", "Boots", "Skin_FNL", "Template_ATV", "Template_MX",
                                     "Template_Vehicle", "Template_tire", "MarsF", "Pyro", "Particle", "HFT_Track", "Gloves", "Pants", "Character"};
        for (uint32_t o : {vs, ps}) { auto it = labels.find(o); if (it == labels.end()) continue; for (auto k : keys) if (it->second.find(k) != std::string::npos) return true; }
        if (!vs || !ps) return false;
        if (swr::Shader* v = shader(vs))   // by the vertex program's constants: particles, billboard crowds, skinned characters
            for (auto& e : ctab_of(v->tokens))
                if (e.name == "g_NoLightLuminance" || e.name == "colorTable" || e.name == "emptyChairRatio" || e.name == "gBoneMatrixVectors" || e.name == "g_BoneMatrixVectors") return true;
        return false;
    }
    // REFLEX_OBJ_DUMP (object shading, _re/notes/object-shading.md): every dynamic draw (dynamic_draw) into the HDR scene target is
    // written to obj/f<frame>_d<draw>.bin with its index list, vertex declaration, the referenced vertex bytes of every stream, all
    // shader constants and the render states (the replay test renders the same geometry with the port's shaders); blended draws
    // also get the target before the draw (rt/f<frame>_pre<draw>.raw).
    bool obj_dump = std::getenv("REFLEX_OBJ_DUMP") != nullptr, particles_seen = false; std::set<uint32_t> cubes_dumped, textures_dumped;
    void dump_obj(const swr::State& s, uint32_t call, uint32_t prim, uint32_t start, uint32_t prim_count, bool indexed, int32_t base_vertex) {
        if (!capture_left || !obj_dump || !s.rt[0] || s.rt[0]->format != 113 || s.rt[0]->w < 1000 || !dynamic_draw()) return;
        mkdir((dir + "/obj").c_str(), 0755);
        char n[96]; std::snprintf(n, sizeof n, "/obj/f%d_d%04llu.bin", frame_no, (unsigned long long)frame_draw);
        FILE* f = std::fopen((dir + n).c_str(), "wb"); if (!f) return;
        auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
        uint32_t nidx = prim == 4 ? 3 * prim_count : prim == 5 || prim == 6 ? prim_count + 2 : prim == 2 ? 2 * prim_count : prim == 3 ? prim_count + 1 : prim_count;
        std::vector<uint32_t> idx(nidx); uint32_t maxi = 0;
        for (uint32_t i = 0; i < nidx; i++) {
            uint32_t v = start + i;
            if (indexed) { v = 0; if (s.index_format == 102) { size_t o = 4 * size_t(start + i); if (s.indices && o + 4 <= s.index_size) std::memcpy(&v, s.indices + o, 4); }
                           else { size_t o = 2 * size_t(start + i); uint16_t x = 0; if (s.indices && o + 2 <= s.index_size) std::memcpy(&x, s.indices + o, 2); v = x; }
                           v = uint32_t(int32_t(v) + base_vertex); }
            idx[i] = v; maxi = std::max(maxi, v);
        }
        uint32_t instances = 1; for (int i = 0; i < 16; i++) if (s.streams[i].freq & 0x40000000u) instances = s.streams[i].freq & 0xffffff;
        if (!(s.streams[0].freq & 0x40000000u)) instances = 1;
        w32(0x444f5852u); w32(1); w32(uint32_t(frame_draw)); w32(call); w32(prim); w32(prim_count); w32(instances);
        w32(nidx); std::fwrite(idx.data(), 4, nidx, f);
        w32(uint32_t(s.decl.size())); for (auto& e : s.decl) { std::fwrite(&e.stream, 2, 1, f); std::fwrite(&e.offset, 2, 1, f); std::fwrite(&e.type, 1, 1, f); std::fwrite(&e.method, 1, 1, f); std::fwrite(&e.usage, 1, 1, f); std::fwrite(&e.index, 1, 1, f); }
        for (int i = 0; i < 16; i++) {
            const auto& sm = s.streams[i];
            uint32_t count = (sm.freq & 0x80000000u) ? instances : maxi + 1;
            size_t want = sm.data ? std::min(sm.size > sm.offset ? sm.size - sm.offset : 0, size_t(count) * sm.stride) : 0;
            w32(sm.data ? 1 : 0); w32(sm.stride); w32(sm.freq); w32(uint32_t(want)); if (want) std::fwrite(sm.data + sm.offset, 1, want, f);
        }
        std::fwrite(s.vsf, 4, 256 * 4, f); std::fwrite(s.psf, 4, 224 * 4, f); std::fwrite(s.vsi, 4, 64, f); std::fwrite(s.psi, 4, 64, f); std::fwrite(s.vsb, 4, 16, f); std::fwrite(s.psb, 4, 16, f);
        std::fwrite(s.rs, 4, 256, f);
        for (int i = 0; i < 21; i++) w32(tex[i]);
        uint64_t hv = shader_hash[vs], hp = shader_hash[ps]; std::fwrite(&hv, 8, 1, f); std::fwrite(&hp, 8, 1, f);
        for (int i = 0; i < 21; i++) {   // per sampler: format, size, levels and the FNV-1a of level 0's bytes (finds the texture asset)
            const swr::Texture* t = s.tex[i]; const swr::Image* l0 = t && !t->level.empty() ? t->level[0] : nullptr;
            w32(l0 ? l0->format : 0); w32(l0 ? l0->w : 0); w32(l0 ? l0->h : 0); w32(t ? t->levels : 0); w32(t ? uint32_t(t->kind) : 0);
            uint64_t h = l0 ? fnv(l0->bytes.data(), l0->bytes.size()) : 0; std::fwrite(&h, 8, 1, f);
        }
        std::fclose(f);
        if (s.rs[27]) { std::snprintf(n, sizeof n, "f%d_pre%04llu", frame_no, (unsigned long long)frame_draw); dump_image(*s.rt[0], n); }
        for (int i = 0; i < 12; i++) {   // the material textures as the device holds them, every level (rt/tex_<object>.raw): run-time ones (plates) too
            const swr::Texture* t = s.tex[i]; if (!t || t->kind != 1 || !textures_dumped.insert(tex[i]).second) continue;
            std::snprintf(n, sizeof n, "/rt/tex_%08x.raw", tex[i]);
            if (FILE* g = std::fopen((dir + n).c_str(), "wb")) {
                uint32_t hd[2] = {t->level.empty() || !t->level[0] ? 0u : t->level[0]->format, uint32_t(t->level.size())}; std::fwrite(hd, 4, 2, g);
                for (auto* l : t->level) { uint32_t lh[4] = {l ? l->w : 0, l ? l->h : 0, l ? l->pitch : 0, l ? uint32_t(l->bytes.size()) : 0}; std::fwrite(lh, 4, 4, g); if (l) std::fwrite(l->bytes.data(), 1, l->bytes.size(), g); }
                std::fclose(g);
            }
        }
        for (int i = 0; i < 21; i++) {   // cube textures as the device holds them: level 0 of the six faces (rt/cube_<object>.raw)
            const swr::Texture* t = s.tex[i]; if (!t || t->kind != 2 || !cubes_dumped.insert(tex[i]).second) continue;
            std::snprintf(n, sizeof n, "/rt/cube_%08x.raw", tex[i]);
            if (FILE* g = std::fopen((dir + n).c_str(), "wb")) {
                const swr::Image* l0 = t->level.empty() ? nullptr : t->level[0];
                uint32_t hd[4] = {l0 ? l0->format : 0, l0 ? l0->w : 0, l0 ? l0->pitch : 0, t->levels}; std::fwrite(hd, 4, 4, g);
                for (uint32_t face = 0; face < 6; face++) { size_t k = size_t(face) * t->levels; if (k < t->level.size() && t->level[k]) std::fwrite(t->level[k]->bytes.data(), 1, t->level[k]->bytes.size(), g); }
                std::fclose(g);
            }
        }
    }
    void do_draw(const std::string& mt) {
        draw_no++; frame_draw++; pass_draws++;
        auto& a = args;
        if (capture_left && draws) { std::string j = json_draw(mt); std::fputs(j.c_str(), draws); std::fputc('\n', draws); }
        bool rast = raster == 2 || (raster == 1 && !backbuffer_target());
        if (rast && static_only && dynamic_draw()) { counts["!static-only-skipped"]++; rast = false; }
        if (!rast) { if (mt == "DrawPrimitiveUP" || mt == "DrawIndexedPrimitiveUP") { streams[0] = {}; ib = 0; } return; }
        bool ok; swr::State s = build_state(ok);
        if (capture_left && obj_dump && !particles_seen && ok && s.rt[0] && s.rt[0]->format == 113 && s.rt[0]->w >= 1000 && s.vs) {   // REFLEX_OBJ_DUMP: the scene and
            bool particle = false; for (auto& e : ctab_of(s.vs->tokens)) if (e.name == "g_NoLightLuminance") particle = true;   // its id buffer before the particles
            if (particle) {
                particles_seen = true;
                char n[96]; std::snprintf(n, sizeof n, "f%d_preparticles", frame_no); dump_image(*s.rt[0], n);
                if (s.depth) { std::snprintf(n, sizeof n, "/rt/f%d_depth_preparticles.raw", frame_no);   // the scene depth (z / w) the soft particles read
                    if (FILE* f = std::fopen((dir + n).c_str(), "wb")) { uint32_t hd[4] = {114, s.depth->w, s.depth->h, s.depth->w * 4}; std::fwrite(hd, 4, 4, f); std::fwrite(s.depth->bytes.data(), 1, s.depth->bytes.size(), f); std::fclose(f); } }
                std::snprintf(n, sizeof n, "/rt/f%d_ids_preparticles.raw", frame_no);
                if (FILE* f = std::fopen((dir + n).c_str(), "wb")) { uint32_t hd[4] = {0, id_w, id_h, id_w * 4}; std::fwrite(hd, 4, 4, f); std::fwrite(id_buffer.data(), 4, id_buffer.size(), f); std::fclose(f); }
            }
        }
        if (capture_left && s.rt[0] && s.rt[0]->format == 113 && s.rt[0]->w >= 1000) {   // id buffer of the HDR scene target: last draw per pixel
            if (id_buffer.size() != size_t(s.rt[0]->w) * s.rt[0]->h) id_buffer.assign(size_t(s.rt[0]->w) * s.rt[0]->h, 0);
            stats.id_target = s.rt[0]; stats.id_buffer = &id_buffer; stats.id = uint32_t(frame_draw); id_w = s.rt[0]->w; id_h = s.rt[0]->h;
        } else { stats.id_buffer = nullptr; }
        std::vector<uint8_t> upv, upi;
        auto vcount = [](uint32_t prim, uint32_t n) { return prim == 1 ? n : prim == 2 ? 2 * n : prim == 3 ? n + 1 : prim == 4 ? 3 * n : n + 2; };
        if (ok) {
            if (mt == "DrawPrimitive") {
                dump_obj(s, 0, a[1], a[2], a[3], false, 0);
                swr::draw(s, a[1], a[2], a[3], false, 0, 0, 0, stats);
                // eye adaptation (ps with g_fElapsedTime / g_firstTime: adapted += (current - adapted) * rate, per frame): the reference
                // uses its steady state for the frame's luminance, iterating the game's own shader with its output fed back as s1
                bool adapt = false; for (auto& e : ctab_of(s.ps->tokens)) if (e.name == "g_fElapsedTime") adapt = true;
                if (adapt && converge_adaptation && s.tex[1] && s.rt[0] && !s.tex[1]->level.empty()) {
                    for (int it = 0; it < 400; it++) { s.tex[1]->level[0]->bytes = s.rt[0]->bytes; s.tex[1]->level[0]->version++; swr::draw(s, a[1], a[2], a[3], false, 0, 0, 0, stats); }
                    float v = swr::load_texel(*s.rt[0], 0, 0)[0]; char l[96]; std::snprintf(l, sizeof l, "adapted luminance (steady state) %.6f", v); log_line(l); std::fprintf(stderr, "%s\n", l);
                }
            }
            else if (mt == "DrawIndexedPrimitive") { dump_obj(s, 1, a[1], a[5], a[6], true, int32_t(a[2])); swr::draw(s, a[1], a[5], a[6], true, int32_t(a[2]), a[3], a[4], stats); }
            else if (mt == "DrawPrimitiveUP") {
                uint32_t n = vcount(a[1], a[2]); upv.resize(size_t(n) * a[4]); m().mem.read_bytes(a[3], upv.data(), upv.size());
                s.streams[0] = {upv.data(), upv.size(), 0, a[4], 1}; dump_obj(s, 2, a[1], 0, a[2], false, 0); swr::draw(s, a[1], 0, a[2], false, 0, 0, 0, stats);
            } else {
                uint32_t n = vcount(a[1], a[4]), isz = a[6] == 102 ? 4 : 2; upi.resize(size_t(n) * isz); m().mem.read_bytes(a[5], upi.data(), upi.size());
                upv.resize(size_t(a[2] + a[3]) * a[8]); m().mem.read_bytes(a[7], upv.data(), upv.size());
                s.streams[0] = {upv.data(), upv.size(), 0, a[8], 1}; s.indices = upi.data(); s.index_size = upi.size(); s.index_format = a[6];
                dump_obj(s, 3, a[1], 0, a[4], true, 0); swr::draw(s, a[1], 0, a[4], true, 0, a[2], a[3], stats);
            }
            mark_rendered();
            if (!stats.trace.empty()) { for (auto& t : stats.trace) { char l[300]; std::snprintf(l, sizeof l, "TRACE draw %llu vs %s ps %s: %s", (unsigned long long)frame_draw, hex64(shader_hash[vs]).c_str(), hex64(shader_hash[ps]).c_str(), t.c_str()); log_line(l); } stats.trace.clear(); }
            if (stats.nan_pixels != nan_seen && capture_left) { char l[160]; std::snprintf(l, sizeof l, "NaN: draw %llu wrote %llu NaN pixels (vs %s ps %s)", (unsigned long long)frame_draw, (unsigned long long)(stats.nan_pixels - nan_seen), hex64(shader_hash[vs]).c_str(), hex64(shader_hash[ps]).c_str()); log_line(l); std::fprintf(stderr, "%s\n", l); }
            nan_seen = stats.nan_pixels;
            if (capture_left && s.ps && s.rt[0]) {   // the tonemapped world image (before the UI overlays): ps with g_KeyValue
                bool tm = false; for (auto& e : ctab_of(s.ps->tokens)) if (e.name == "g_KeyValue") tm = true;
                if (tm) { char n[64]; std::snprintf(n, sizeof n, "f%d_tonemapped", frame_no); dump_image(*s.rt[0], n); }
            }
            if (const char* da = std::getenv("REFLEX_DUMP_DRAWS")) if (capture_left) {   // "a-b": RT0 after each of these draws
                int lo = 0, hi = -1; std::sscanf(da, "%d-%d", &lo, &hi);
                if (int(frame_draw) >= lo && int(frame_draw) <= hi && s.rt[0]) { char n[64]; std::snprintf(n, sizeof n, "f%d_d%04llu", frame_no, (unsigned long long)frame_draw); dump_image(*s.rt[0], n); }
            }
        } else counts["!draw-skipped"]++;
        if (mt == "DrawPrimitiveUP" || mt == "DrawIndexedPrimitiveUP") { streams[0] = {}; ib = 0; }
    }
};

RenderCapture::RenderCapture(Boot& boot, const std::string& dir) : d(new RenderCaptureImpl(boot, dir)) {
    RenderCaptureImpl* p = d.get();
    boot.set_d3d_hook([p](Machine& m, const std::string& iface, uint32_t self, const std::string& mt, bool after) { p->hook(m, iface, self, mt, after); });
}
RenderCapture::~RenderCapture() { if (d->calls) std::fclose(d->calls); if (d->draws) std::fclose(d->draws); }
void RenderCapture::set_raster(int mode) { d->raster = mode; }
void RenderCapture::after_start() {
    // D3DXCompileShader: keep the HLSL source the game compiles at run time (the boot's stand-in returns an empty program)
    RenderCaptureImpl* p = d.get(); Machine& m = p->b.machine();
    auto it = m.imports.find("d3dx9_43.dll!D3DXCompileShader"); if (it == m.imports.end()) return;
    Native orig = it->second;
    it->second = [p, orig](Machine& m) -> uint32_t {
        uint32_t src = m.arg(0), len = m.arg(1); std::string text(len, '\0'); m.mem.read_bytes(src, text.data(), len);
        std::string entry; for (uint32_t a = m.arg(4); a && entry.size() < 64; a++) { char c = char(m.mem.read(a, 1)); if (!c) break; entry += c; }
        std::string prof; for (uint32_t a = m.arg(5); a && prof.size() < 16; a++) { char c = char(m.mem.read(a, 1)); if (!c) break; prof += c; }
        char n[96]; std::snprintf(n, sizeof n, "/shaders/compiled_%02d_%s_%s.hlsl", p->compiled++, entry.c_str(), prof.c_str());
        if (FILE* f = std::fopen((p->dir + n).c_str(), "w")) { std::fwrite(text.data(), 1, text.size(), f); std::fclose(f); }
        return orig(m);
    };
}
void RenderCapture::capture_frames(int frames) { d->capture_left = frames; d->open_logs(); }
bool RenderCapture::capturing() const { return d->capture_left > 0; }
std::string RenderCapture::summary() const {
    std::string s; char b[160];
    for (auto& [k, n] : d->counts) { std::snprintf(b, sizeof b, "%-48s %llu\n", k.c_str(), (unsigned long long)n); s += b; }
    std::snprintf(b, sizeof b, "draws %llu, rasterized prims %llu, pixels %llu\n", (unsigned long long)d->draw_no, (unsigned long long)d->stats.prims, (unsigned long long)d->stats.shaded); s += b;
    return s;
}
}  // namespace reflex_vm
