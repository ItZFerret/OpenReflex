// Software Direct3D 9 rasterizer (swr.h).
#include "swr.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace swr {
namespace {
// ------------------------------------------------------------------------------------------------ half / formats
float half_to_float(uint16_t h) {
    uint32_t s = uint32_t(h >> 15) << 31, e = (h >> 10) & 31, m = h & 1023, f;
    if (e == 0) { if (!m) f = s; else { float v = std::ldexp(float(m), -24); return s ? -v : v; } }
    else if (e == 31) f = s | 0x7f800000u | (m << 13);
    else f = s | ((e + 112) << 23) | (m << 13);
    float r; std::memcpy(&r, &f, 4); return r;
}
uint16_t float_to_half(float v) {
    uint32_t x; std::memcpy(&x, &v, 4);
    uint32_t s = (x >> 16) & 0x8000; int e = int((x >> 23) & 255) - 127 + 15; uint32_t m = x & 0x7fffff;
    if (((x >> 23) & 255) == 255) return uint16_t(s | 0x7c00 | (m ? 0x200 : 0));
    if (e >= 31) return uint16_t(s | 0x7c00);
    if (e <= 0) { if (e < -10) return uint16_t(s); m |= 0x800000; uint32_t sh = uint32_t(14 - e); uint32_t hm = m >> sh; uint32_t rem = m & ((1u << sh) - 1), half = 1u << (sh - 1); if (rem > half || (rem == half && (hm & 1))) hm++; return uint16_t(s | hm); }
    uint32_t hm = m >> 13, rem = m & 0x1fff; uint32_t r = s | (uint32_t(e) << 10) | hm; if (rem > 0x1000 || (rem == 0x1000 && (hm & 1))) r++; return uint16_t(r);
}
bool is_dxt(uint32_t f, uint32_t* block) {
    if (f == 0x31545844u || f == 0x31495441u) { *block = 8; return true; }
    if (f == 0x32545844u || f == 0x33545844u || f == 0x34545844u || f == 0x35545844u || f == 0x32495441u) { *block = 16; return true; }
    return false;
}
void dxt_color(const uint8_t* b, uint32_t px, float out[4], bool dxt1) {
    uint16_t c0 = uint16_t(b[0] | (b[1] << 8)), c1 = uint16_t(b[2] | (b[3] << 8));
    uint32_t bits = uint32_t(b[4]) | (uint32_t(b[5]) << 8) | (uint32_t(b[6]) << 16) | (uint32_t(b[7]) << 24);
    uint32_t code = (bits >> (2 * px)) & 3;
    auto ex = [](uint16_t c, float* rgb) { uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, bl = c & 31; rgb[0] = float((r << 3) | (r >> 2)) / 255.f; rgb[1] = float((g << 2) | (g >> 4)) / 255.f; rgb[2] = float((bl << 3) | (bl >> 2)) / 255.f; };
    float a[3], c[3]; ex(c0, a); ex(c1, c); out[3] = 1;
    if (!dxt1 || c0 > c1) {
        if (code == 0) { for (int i = 0; i < 3; i++) out[i] = a[i]; }
        else if (code == 1) { for (int i = 0; i < 3; i++) out[i] = c[i]; }
        else if (code == 2) { for (int i = 0; i < 3; i++) out[i] = (2 * a[i] + c[i]) / 3; }
        else { for (int i = 0; i < 3; i++) out[i] = (a[i] + 2 * c[i]) / 3; }
    } else {
        if (code == 0) { for (int i = 0; i < 3; i++) out[i] = a[i]; }
        else if (code == 1) { for (int i = 0; i < 3; i++) out[i] = c[i]; }
        else if (code == 2) { for (int i = 0; i < 3; i++) out[i] = (a[i] + c[i]) / 2; }
        else { out[0] = out[1] = out[2] = 0; out[3] = 0; }
    }
}
float bc4(const uint8_t* b, uint32_t px) {
    float a0 = b[0] / 255.f, a1 = b[1] / 255.f;
    uint64_t bits = 0; for (int i = 0; i < 6; i++) bits |= uint64_t(b[2 + i]) << (8 * i);
    uint32_t code = uint32_t(bits >> (3 * px)) & 7;
    if (code == 0) return a0; if (code == 1) return a1;
    if (b[0] > b[1]) return ((8 - code) * a0 + (code - 1) * a1) / 7;
    if (code == 6) return 0; if (code == 7) return 1;
    return ((6 - code) * a0 + (code - 1) * a1) / 5;
}
}  // namespace

bool format_supported(uint32_t f) {
    uint32_t blk; if (is_dxt(f, &blk)) return true;
    switch (f) { case 20: case 21: case 22: case 23: case 25: case 26: case 28: case 31: case 32: case 34: case 35: case 36: case 50: case 51: case 60: case 63: case 111: case 112: case 113: case 114: case 115: case 116: case 75: case 77: case 80: return true; }
    return false;
}

std::array<float, 4> load_texel(const Image& img, uint32_t x, uint32_t y) {
    std::array<float, 4> o{0, 0, 0, 1};
    uint32_t blk;
    if (img.bytes.empty()) return o;
    if (is_dxt(img.format, &blk)) {
        size_t off = size_t(y / 4) * img.pitch + size_t(x / 4) * blk; if (off + blk > img.bytes.size()) return o;
        const uint8_t* b = &img.bytes[off]; uint32_t px = (y & 3) * 4 + (x & 3);
        switch (img.format) {
            case 0x31545844u: dxt_color(b, px, o.data(), true); break;
            case 0x32545844u: case 0x33545844u: { dxt_color(b + 8, px, o.data(), false); uint32_t a = (b[px / 2] >> (4 * (px & 1))) & 15; o[3] = float(a * 17) / 255.f; break; }
            case 0x34545844u: case 0x35545844u: { dxt_color(b + 8, px, o.data(), false); o[3] = bc4(b, px); break; }
            case 0x31495441u: { float v = bc4(b, px); o = {v, v, v, v}; break; }   // ATI1: replicated (driver dependent)
            case 0x32495441u: { o[0] = bc4(b + 8, px); o[1] = bc4(b, px); o[2] = 1; o[3] = 1; break; }   // ATI2: x from the second block
        }
        return o;
    }
    size_t row = size_t(y) * img.pitch;
    auto p8 = [&](size_t i) { return img.bytes[row + i]; };
    auto rd16 = [&](size_t i) { uint16_t v; std::memcpy(&v, &img.bytes[row + i], 2); return v; };
    auto rd32 = [&](size_t i) { uint32_t v; std::memcpy(&v, &img.bytes[row + i], 4); return v; };
    auto rdf = [&](size_t i) { float v; std::memcpy(&v, &img.bytes[row + i], 4); return v; };
    switch (img.format) {
        case 21: case 22: { uint32_t v = rd32(4 * x); o = {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, img.format == 21 ? (v >> 24) / 255.f : 1.f}; break; }
        case 32: case 33: { uint32_t v = rd32(4 * x); o = {(v & 255) / 255.f, ((v >> 8) & 255) / 255.f, ((v >> 16) & 255) / 255.f, img.format == 32 ? (v >> 24) / 255.f : 1.f}; break; }
        case 20: { o = {p8(3 * x + 2) / 255.f, p8(3 * x + 1) / 255.f, p8(3 * x) / 255.f, 1}; break; }
        case 23: { uint16_t v = rd16(2 * x); o = {((v >> 11) & 31) / 31.f, ((v >> 5) & 63) / 63.f, (v & 31) / 31.f, 1}; break; }
        case 25: { uint16_t v = rd16(2 * x); o = {((v >> 10) & 31) / 31.f, ((v >> 5) & 31) / 31.f, (v & 31) / 31.f, float(v >> 15)}; break; }
        case 26: { uint16_t v = rd16(2 * x); o = {((v >> 8) & 15) / 15.f, ((v >> 4) & 15) / 15.f, (v & 15) / 15.f, (v >> 12) / 15.f}; break; }
        case 28: { o = {0, 0, 0, p8(x) / 255.f}; break; }
        case 50: { float l = p8(x) / 255.f; o = {l, l, l, 1}; break; }
        case 51: { float l = p8(2 * x) / 255.f; o = {l, l, l, p8(2 * x + 1) / 255.f}; break; }
        case 31: { uint32_t v = rd32(4 * x); o = {(v & 1023) / 1023.f, ((v >> 10) & 1023) / 1023.f, ((v >> 20) & 1023) / 1023.f, (v >> 30) / 3.f}; break; }
        case 35: { uint32_t v = rd32(4 * x); o = {((v >> 20) & 1023) / 1023.f, ((v >> 10) & 1023) / 1023.f, (v & 1023) / 1023.f, (v >> 30) / 3.f}; break; }
        case 34: { o = {rd16(4 * x) / 65535.f, rd16(4 * x + 2) / 65535.f, 1, 1}; break; }
        case 36: { o = {rd16(8 * x) / 65535.f, rd16(8 * x + 2) / 65535.f, rd16(8 * x + 4) / 65535.f, rd16(8 * x + 6) / 65535.f}; break; }
        case 60: { o = {float(int8_t(p8(2 * x))) / 127.f, float(int8_t(p8(2 * x + 1))) / 127.f, 1, 1}; break; }
        case 63: { o = {float(int8_t(p8(4 * x))) / 127.f, float(int8_t(p8(4 * x + 1))) / 127.f, float(int8_t(p8(4 * x + 2))) / 127.f, float(int8_t(p8(4 * x + 3))) / 127.f}; break; }
        case 111: { o = {half_to_float(rd16(2 * x)), 1, 1, 1}; break; }
        case 112: { o = {half_to_float(rd16(4 * x)), half_to_float(rd16(4 * x + 2)), 1, 1}; break; }
        case 113: { o = {half_to_float(rd16(8 * x)), half_to_float(rd16(8 * x + 2)), half_to_float(rd16(8 * x + 4)), half_to_float(rd16(8 * x + 6))}; break; }
        case 114: { o = {rdf(4 * x), 1, 1, 1}; break; }
        case 115: { o = {rdf(8 * x), rdf(8 * x + 4), 1, 1}; break; }
        case 116: { o = {rdf(16 * x), rdf(16 * x + 4), rdf(16 * x + 8), rdf(16 * x + 12)}; break; }
        default: break;
    }
    return o;
}
void store_texel(Image& img, uint32_t x, uint32_t y, const float* c) {
    if (x >= img.w || y >= img.h) return;
    size_t row = size_t(y) * img.pitch;
    auto u8 = [](float v) { v = std::min(1.f, std::max(0.f, v)); if (!(v == v)) v = 0; return uint32_t(v * 255.f + 0.5f); };
    auto u16 = [](float v) { v = std::min(1.f, std::max(0.f, v)); if (!(v == v)) v = 0; return uint16_t(v * 65535.f + 0.5f); };
    auto w16 = [&](size_t i, uint16_t v) { std::memcpy(&img.bytes[row + i], &v, 2); };
    auto w32 = [&](size_t i, uint32_t v) { std::memcpy(&img.bytes[row + i], &v, 4); };
    auto wf = [&](size_t i, float v) { std::memcpy(&img.bytes[row + i], &v, 4); };
    switch (img.format) {
        case 21: case 22: w32(4 * x, (u8(c[3]) << 24) | (u8(c[0]) << 16) | (u8(c[1]) << 8) | u8(c[2])); break;
        case 32: case 33: w32(4 * x, (u8(c[3]) << 24) | (u8(c[2]) << 16) | (u8(c[1]) << 8) | u8(c[0])); break;
        case 23: { uint32_t r = uint32_t(std::min(1.f, std::max(0.f, c[0])) * 31 + .5f), g = uint32_t(std::min(1.f, std::max(0.f, c[1])) * 63 + .5f), b = uint32_t(std::min(1.f, std::max(0.f, c[2])) * 31 + .5f); w16(2 * x, uint16_t((r << 11) | (g << 5) | b)); break; }
        case 28: img.bytes[row + x] = uint8_t(u8(c[3])); break;
        case 50: img.bytes[row + x] = uint8_t(u8(c[0])); break;
        case 34: w16(4 * x, u16(c[0])); w16(4 * x + 2, u16(c[1])); break;
        case 36: for (int i = 0; i < 4; i++) w16(8 * x + 2 * i, u16(c[i])); break;
        case 111: w16(2 * x, float_to_half(c[0])); break;
        case 112: w16(4 * x, float_to_half(c[0])); w16(4 * x + 2, float_to_half(c[1])); break;
        case 113: for (int i = 0; i < 4; i++) w16(8 * x + 2 * i, float_to_half(c[i])); break;
        case 114: wf(4 * x, c[0]); break;
        case 115: wf(8 * x, c[0]); wf(8 * x + 4, c[1]); break;
        case 116: for (int i = 0; i < 4; i++) wf(16 * x + 4 * i, c[i]); break;
        case 31: { auto q = [](float v) { return uint32_t(std::min(1.f, std::max(0.f, v)) * 1023 + .5f); }; w32(4 * x, q(c[0]) | (q(c[1]) << 10) | (q(c[2]) << 20) | (uint32_t(std::min(1.f, std::max(0.f, c[3])) * 3 + .5f) << 30)); break; }
        case 35: { auto q = [](float v) { return uint32_t(std::min(1.f, std::max(0.f, v)) * 1023 + .5f); }; w32(4 * x, q(c[2]) | (q(c[1]) << 10) | (q(c[0]) << 20) | (uint32_t(std::min(1.f, std::max(0.f, c[3])) * 3 + .5f) << 30)); break; }
        default: break;
    }
}
static bool unorm_target(uint32_t f) { return !(f == 111 || f == 112 || f == 113 || f == 114 || f == 115 || f == 116); }

// ------------------------------------------------------------------------------------------------ shader decoding
enum Op { NOP = 0, MOV, ADD, SUB, MAD, MUL, RCP, RSQ, DP3, DP4, MIN, MAX, SLT, SGE, EXP, LOG, LIT, DST, LRP, FRC, M4x4, M4x3, M3x4, M3x3, M3x2,
    CALL, CALLNZ, LOOP, RET, ENDLOOP, LABEL, DCL, POW, CRS, SGN, ABS, NRM, SINCOS, REP, ENDREP, IF, IFC, ELSE, ENDIF, BREAK, BREAKC, MOVA, DEFB, DEFI,
    TEXCOORD = 64, TEXKILL, TEX, EXPP = 78, LOGP, CND, DEF, CMP = 88, BEM, DP2ADD, DSX, DSY, TEXLDD, SETP, TEXLDL, BREAKP };
static bool has_dst(uint32_t op) {
    switch (op) { case NOP: case CALL: case CALLNZ: case LOOP: case RET: case ENDLOOP: case LABEL: case REP: case ENDREP: case IF: case IFC: case ELSE: case ENDIF: case BREAK: case BREAKC: case BREAKP: return false; }
    return true;
}
static const char* op_name(uint32_t op) {
    static const char* n[] = {"nop", "mov", "add", "sub", "mad", "mul", "rcp", "rsq", "dp3", "dp4", "min", "max", "slt", "sge", "exp", "log", "lit", "dst", "lrp", "frc", "m4x4", "m4x3", "m3x4", "m3x3", "m3x2",
        "call", "callnz", "loop", "ret", "endloop", "label", "dcl", "pow", "crs", "sgn", "abs", "nrm", "sincos", "rep", "endrep", "if", "ifc", "else", "endif", "break", "breakc", "mova", "defb", "defi"};
    if (op < sizeof n / sizeof n[0]) return n[op];
    switch (op) { case TEXCOORD: return "texcoord"; case TEXKILL: return "texkill"; case TEX: return "texld"; case EXPP: return "expp"; case LOGP: return "logp"; case CND: return "cnd"; case DEF: return "def";
        case CMP: return "cmp"; case BEM: return "bem"; case DP2ADD: return "dp2add"; case DSX: return "dsx"; case DSY: return "dsy"; case TEXLDD: return "texldd"; case SETP: return "setp"; case TEXLDL: return "texldl"; case BREAKP: return "breakp"; }
    return "?";
}
static uint32_t reg_type(uint32_t t) { return ((t >> 28) & 7) | ((t >> 8) & 0x18); }

bool Shader::decode() {
    code.clear(); dcls.clear(); defs.clear(); defis.clear(); defbs.clear();
    if (tokens.empty()) { error = "empty"; return false; }
    size_t p = 1;
    while (p < tokens.size()) {
        uint32_t t = tokens[p];
        if (t == 0xffff) break;
        uint32_t op = t & 0xffff;
        if (op == 0xfffe) { p += 1 + ((t >> 16) & 0x7fff); continue; }
        uint32_t len = (t >> 24) & 15;
        if (major < 2) { error = "shader model 1"; return false; }
        size_t start = p + 1, end = start + len;
        if (end > tokens.size()) { error = "truncated"; return false; }
        if (op == DCL) {
            Dcl d{}; uint32_t u = tokens[start], r = tokens[start + 1];
            d.reg_type = reg_type(r); d.reg = r & 0x7ff; d.mask = (r >> 16) & 15; d.usage = u & 31; d.index = (u >> 16) & 15; d.sampler_type = (u >> 27) & 15;
            dcls.push_back(d); p = end; continue;
        }
        if (op == DEF) { std::array<float, 4> v; std::memcpy(v.data(), &tokens[start + 1], 16); defs.push_back({tokens[start] & 0x7ff, v}); p = end; continue; }
        if (op == DEFI) { std::array<int32_t, 4> v; std::memcpy(v.data(), &tokens[start + 1], 16); defis.push_back({tokens[start] & 0x7ff, v}); p = end; continue; }
        if (op == DEFB) { defbs.push_back({tokens[start] & 0x7ff, tokens[start + 1]}); p = end; continue; }
        Ins in{}; in.op = op; in.ctrl = (t >> 16) & 0xff; in.pred = (t >> 28) & 1; in.pos = uint32_t(p);
        size_t q = start;
        if (has_dst(op)) { in.dst = tokens[q++]; in.ndst = 1; if ((in.dst >> 13) & 1) in.dst_rel = tokens[q++]; }
        if (in.pred) { uint32_t pt = tokens[q++]; in.pred_not = ((pt >> 24) & 15) == 13; in.pred_swz = (pt >> 16) & 0xff; }
        while (q < end && in.nsrc < 4) { uint32_t s = tokens[q++]; in.src[in.nsrc] = s; in.src_rel[in.nsrc] = 0; if ((s >> 13) & 1) in.src_rel[in.nsrc] = tokens[q++]; in.nsrc++; }
        code.push_back(in); p = end;
    }
    // match control flow
    jump.assign(code.size(), -1);
    std::vector<int> st;
    for (int i = 0; i < int(code.size()); i++) {
        uint32_t op = code[i].op;
        if (op == IF || op == IFC || op == LOOP || op == REP) st.push_back(i);
        else if (op == ELSE) { if (st.empty()) { error = "else"; return false; } jump[st.back()] = i; st.back() = i; }
        else if (op == ENDIF || op == ENDLOOP || op == ENDREP) { if (st.empty()) { error = "end"; return false; } jump[st.back()] = i; jump[i] = st.back(); st.pop_back(); }
    }
    return true;
}

static std::string reg_str(uint32_t t, bool ps, uint32_t rel) {
    uint32_t ty = reg_type(t), n = t & 0x7ff; char b[48];
    const char* nm = "?";
    switch (ty) { case 0: nm = "r"; break; case 1: nm = "v"; break; case 2: nm = "c"; break; case 3: nm = ps ? "t" : "a"; break; case 4: nm = n == 0 ? "oPos" : n == 1 ? "oFog" : "oPts"; break;
        case 5: nm = "oD"; break; case 6: nm = "o"; break; case 7: nm = "i"; break; case 8: nm = "oC"; break; case 9: nm = "oDepth"; break; case 10: nm = "s"; break; case 14: nm = "b"; break;
        case 15: nm = "aL"; break; case 17: nm = n == 0 ? "vPos" : "vFace"; break; case 18: nm = "l"; break; case 19: nm = "p"; break; }
    if (ty == 4 || ty == 9 || ty == 15 || ty == 17) std::snprintf(b, sizeof b, "%s", nm);
    else if ((t >> 13) & 1) { uint32_t rt = reg_type(rel); static const char xyzw[] = "xyzw"; std::snprintf(b, sizeof b, "%s[%s.%c + %u]", nm, rt == 15 ? "aL" : "a0", xyzw[(rel >> 16) & 3], n); }
    else std::snprintf(b, sizeof b, "%s%u", nm, n);
    return b;
}
std::string disassemble(const Shader& s) {
    std::string o; char b[256];
    std::snprintf(b, sizeof b, "%s_%u_%u\n", s.ps ? "ps" : "vs", s.major, s.minor); o += b;
    static const char* usage[] = {"position", "blendweight", "blendindices", "normal", "psize", "texcoord", "tangent", "binormal", "tessfactor", "positiont", "color", "fog", "depth", "sample"};
    for (auto& d : s.dcls) {
        if (d.reg_type == 10) std::snprintf(b, sizeof b, "dcl_%s s%u\n", d.sampler_type == 3 ? "cube" : d.sampler_type == 4 ? "volume" : "2d", d.reg);
        else std::snprintf(b, sizeof b, "dcl_%s%u %s.%x\n", d.usage < 14 ? usage[d.usage] : "?", d.index, reg_str((d.reg_type & 7) << 28 | ((d.reg_type & 0x18) << 8) | d.reg, s.ps, 0).c_str(), d.mask);
        o += b;
    }
    for (auto& d : s.defs) { std::snprintf(b, sizeof b, "def c%u, %.9g, %.9g, %.9g, %.9g\n", d.first, d.second[0], d.second[1], d.second[2], d.second[3]); o += b; }
    for (auto& d : s.defis) { std::snprintf(b, sizeof b, "defi i%u, %d, %d, %d, %d\n", d.first, d.second[0], d.second[1], d.second[2], d.second[3]); o += b; }
    for (auto& d : s.defbs) { std::snprintf(b, sizeof b, "defb b%u, %u\n", d.first, d.second); o += b; }
    static const char xyzw[] = "xyzw";
    static const char* cmp[] = {"", "_gt", "_eq", "_ge", "_lt", "_ne", "_le", ""};
    for (auto& in : s.code) {
        std::string line;
        std::snprintf(b, sizeof b, "%04x  ", in.pos * 4); line += b;
        if (in.pred) { line += in.pred_not ? "(!p0) " : "(p0) "; }
        line += op_name(in.op);
        if (in.op == IFC || in.op == BREAKC || in.op == SETP) line += cmp[in.ctrl & 7];
        if (in.op == TEX && in.ctrl == 1) line += "p"; if (in.op == TEX && in.ctrl == 2) line += "b";
        if (in.ndst) { uint32_t mod = (in.dst >> 20) & 15; if (mod & 1) line += "_sat"; if (mod & 2) line += "_pp"; if (mod & 4) line += "_centroid"; }
        line += " ";
        bool first = true;
        if (in.ndst) { line += reg_str(in.dst, s.ps, in.dst_rel); uint32_t mk = (in.dst >> 16) & 15; if (mk != 15) { line += "."; for (int i = 0; i < 4; i++) if (mk & (1 << i)) line += xyzw[i]; } first = false; }
        for (int i = 0; i < in.nsrc; i++) {
            uint32_t t = in.src[i], mod = (t >> 24) & 15, sw = (t >> 16) & 0xff;
            line += first ? "" : ", "; first = false;
            std::string r = reg_str(t, s.ps, in.src_rel[i]);
            if (mod == 1 || mod == 12) line += "-";
            if (mod == 11 || mod == 12) r += "_abs";
            line += r;
            if (sw != 0xe4) { line += "."; char c[5] = {xyzw[sw & 3], xyzw[(sw >> 2) & 3], xyzw[(sw >> 4) & 3], xyzw[(sw >> 6) & 3], 0}; if (c[0] == c[1] && c[1] == c[2] && c[2] == c[3]) c[1] = 0; line += c; }
            if (mod != 0 && mod != 1 && mod != 11 && mod != 12) { std::snprintf(b, sizeof b, "<mod%u>", mod); line += b; }
        }
        o += line + "\n";
    }
    return o;
}

// ------------------------------------------------------------------------------------------------ execution
namespace {
constexpr int L = 4;   // lanes (a pixel quad or 4 vertices)
struct V4 { float v[4]; };
struct Ctx {
    const State* st; const Shader* sh; bool ps;
    V4 r[32][L]; V4 in[16][L]; V4 out[16][L]; V4 rast[3][L]; V4 oc[4][L]; float odepth[L]; V4 misc[2][L];
    int32_t a0[4][L]; int32_t aL[L]; bool p[4][L];
    const float (*cf)[4]; const int32_t (*ci)[4]; const uint32_t* cb;
    float local_f[256][4]; bool has_local[256] = {};
    int32_t local_i[16][4]; bool has_local_i[16] = {};
    uint32_t local_b[16]; bool has_local_b[16] = {};
    uint32_t kill = 0;   // lanes discarded
    uint32_t active_lanes = 0xf;
    int lod_explicit = 0;
};

inline float wrap_coord(float c, uint32_t mode, bool& border) {
    switch (mode) {
        case 1: return c - std::floor(c);
        case 2: { float t = c - 2 * std::floor(c / 2); return t > 1 ? 2 - t : t; }
        case 3: return std::min(1.f, std::max(0.f, c));
        case 4: if (c < 0 || c > 1) border = true; return c;
        case 5: { float a = std::fabs(c); return std::min(1.f, a); }
    }
    return c - std::floor(c);
}
int address_texel(int i, int n, uint32_t mode, bool& border) {
    switch (mode) {
        case 1: { int r = i % n; return r < 0 ? r + n : r; }
        case 2: { int p = 2 * n; int r = i % p; if (r < 0) r += p; return r >= n ? p - 1 - r : r; }
        case 3: return std::clamp(i, 0, n - 1);
        case 4: if (i < 0 || i >= n) border = true; return std::clamp(i, 0, n - 1);
        case 5: { int a = i < 0 ? -i - 1 : i; return std::min(a, n - 1); }
    }
    int r = i % n; return r < 0 ? r + n : r;
}
std::array<float, 4> fetch(const Image& im, int x, int y, const Sampler& s, int axu, int axv) {
    bool border = false;
    int xx = address_texel(x, int(im.w), s.s[axu], border), yy = address_texel(y, int(im.h), s.s[axv], border);
    if (border) { uint32_t c = s.s[4]; return {((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, (c >> 24) / 255.f}; }
    return load_texel(im, uint32_t(xx), uint32_t(yy));
}
std::array<float, 4> sample_level(const Image& im, float u, float v, const Sampler& s, bool linear) {
    if (!linear) return fetch(im, int(std::floor(u * float(im.w))), int(std::floor(v * float(im.h))), s, 1, 2);
    float fx = u * float(im.w) - 0.5f, fy = v * float(im.h) - 0.5f; float x0 = std::floor(fx), y0 = std::floor(fy); float ax = fx - x0, ay = fy - y0;
    int ix = int(x0), iy = int(y0);
    auto a = fetch(im, ix, iy, s, 1, 2), b = fetch(im, ix + 1, iy, s, 1, 2), c = fetch(im, ix, iy + 1, s, 1, 2), d = fetch(im, ix + 1, iy + 1, s, 1, 2);
    std::array<float, 4> o; for (int k = 0; k < 4; k++) o[k] = (a[k] * (1 - ax) + b[k] * ax) * (1 - ay) + (c[k] * (1 - ax) + d[k] * ax) * ay;
    return o;
}
void cube_face(const float* d, int& face, float& u, float& v) {
    float ax = std::fabs(d[0]), ay = std::fabs(d[1]), az = std::fabs(d[2]), sc, tc, ma;
    if (ax >= ay && ax >= az) { ma = ax; if (d[0] >= 0) { face = 0; sc = -d[2]; tc = -d[1]; } else { face = 1; sc = d[2]; tc = -d[1]; } }
    else if (ay >= az) { ma = ay; if (d[1] >= 0) { face = 2; sc = d[0]; tc = d[2]; } else { face = 3; sc = d[0]; tc = -d[2]; } }
    else { ma = az; if (d[2] >= 0) { face = 4; sc = d[0]; tc = -d[1]; } else { face = 5; sc = -d[0]; tc = -d[1]; } }
    if (ma == 0) ma = 1;
    u = (sc / ma + 1) * 0.5f; v = (tc / ma + 1) * 0.5f;
}
// lod: level-of-detail value (already biased); mag when lod <= 0
std::array<float, 4> sample_tex(const Texture* t, const Sampler& s, const float* coord, float lod, float aniso_ratio, const float* major_axis) {
    if (!t || t->level.empty() || !t->level[0]) return {0, 0, 0, 1};
    float u = coord[0], v = coord[1]; int face = 0;
    if (t->kind == 2) cube_face(coord, face, u, v);
    uint32_t levels = std::max(1u, t->levels);
    float maxlvl = float(levels - 1);
    float minlod = float(std::min<uint32_t>(s.s[9], levels - 1));
    bool mag = lod <= 0;
    uint32_t filt = mag ? s.s[5] : s.s[6];
    bool linear = filt >= 2;
    uint32_t mip = s.s[7];
    lod = std::max(lod, 0.f);
    auto lvl_img = [&](int l) -> const Image* { size_t i = size_t(face) * levels + size_t(l); return i < t->level.size() ? t->level[i] : nullptr; };
    auto at_level = [&](float lu, float lv, int l) -> std::array<float, 4> { const Image* im = lvl_img(l); if (!im) return {0, 0, 0, 1}; return sample_level(*im, lu, lv, s, linear); };
    auto filtered = [&](float lu, float lv) -> std::array<float, 4> {
        if (mip == 0) return at_level(lu, lv, int(minlod));
        float l = std::min(std::max(lod, minlod), maxlvl);
        if (mip == 1) return at_level(lu, lv, int(std::floor(l + 0.5f)));
        int l0 = int(std::floor(l)); float f = l - float(l0); int l1 = std::min(l0 + 1, int(maxlvl));
        auto a = at_level(lu, lv, l0); if (f <= 0 || l1 == l0) return a;
        auto b = at_level(lu, lv, l1); for (int k = 0; k < 4; k++) a[k] += (b[k] - a[k]) * f; return a;
    };
    if (aniso_ratio > 1.0f && !mag && filt == 3 && major_axis && t->kind == 1) {
        int n = std::min(int(std::ceil(aniso_ratio)), int(std::max(1u, s.s[10])));
        std::array<float, 4> acc{0, 0, 0, 0};
        for (int i = 0; i < n; i++) { float o = (float(i) + 0.5f) / float(n) - 0.5f; auto c = filtered(u + major_axis[0] * o, v + major_axis[1] * o); for (int k = 0; k < 4; k++) acc[k] += c[k]; }
        for (int k = 0; k < 4; k++) acc[k] /= float(n);
        return acc;
    }
    return filtered(u, v);
}

inline float swz(const V4& v, uint32_t sw, int c) { return v.v[(sw >> (2 * c)) & 3]; }

struct Exec {
    Ctx& c;
    explicit Exec(Ctx& cc) : c(cc) {}
    const float* constf(uint32_t n) { if (n < 256 && c.has_local[n]) return c.local_f[n]; static const float z[4] = {0, 0, 0, 0}; return n < (c.ps ? 224u : 256u) ? c.cf[n] : z; }
    V4 read(uint32_t t, uint32_t rel, int lane) {
        uint32_t ty = reg_type(t), n = t & 0x7ff; V4 v{{0, 0, 0, 0}};
        if ((t >> 13) & 1) {
            uint32_t rt = reg_type(rel), comp = (rel >> 16) & 3;
            int off = rt == 15 ? c.aL[lane] : c.a0[comp][lane];
            int idx = int(n) + off; n = uint32_t(std::max(0, idx));
        }
        switch (ty) {
            case 0: v = c.r[n & 31][lane]; break;
            case 1: v = c.in[n & 15][lane]; break;
            case 2: { const float* f = constf(n); std::memcpy(v.v, f, 16); break; }
            case 3: if (!c.ps) { for (int k = 0; k < 4; k++) v.v[k] = float(c.a0[k][lane]); } else v = c.in[n & 15][lane]; break;
            case 7: { const int32_t* f = (n < 16 && c.has_local_i[n]) ? c.local_i[n] : c.ci[n & 15]; for (int k = 0; k < 4; k++) v.v[k] = float(f[k]); break; }
            case 14: { uint32_t bv = (n < 16 && c.has_local_b[n]) ? c.local_b[n] : c.cb[n & 15]; v.v[0] = v.v[1] = v.v[2] = v.v[3] = bv ? 1.f : 0.f; break; }
            case 15: for (int k = 0; k < 4; k++) v.v[k] = float(c.aL[lane]); break;
            case 17: v = c.misc[n & 1][lane]; break;
            case 19: for (int k = 0; k < 4; k++) v.v[k] = c.p[k][lane] ? 1.f : 0.f; break;
            case 6: v = c.out[n & 15][lane]; break;
            case 4: v = c.rast[n % 3][lane]; break;
            case 8: v = c.oc[n & 3][lane]; break;
            default: break;
        }
        return v;
    }
    V4 src(const Shader::Ins& in, int i, int lane) {
        uint32_t t = in.src[i]; V4 r = read(t, in.src_rel[i], lane); uint32_t sw = (t >> 16) & 0xff, mod = (t >> 24) & 15;
        V4 o; for (int k = 0; k < 4; k++) o.v[k] = swz(r, sw, k);
        switch (mod) {
            case 1: for (float& x : o.v) x = -x; break;
            case 2: for (float& x : o.v) x = x - 0.5f; break;
            case 3: for (float& x : o.v) x = -(x - 0.5f); break;
            case 4: for (float& x : o.v) x = 2 * (x - 0.5f); break;
            case 5: for (float& x : o.v) x = -2 * (x - 0.5f); break;
            case 6: for (float& x : o.v) x = 1 - x; break;
            case 7: for (float& x : o.v) x = 2 * x; break;
            case 8: for (float& x : o.v) x = -2 * x; break;
            case 11: for (float& x : o.v) x = std::fabs(x); break;
            case 12: for (float& x : o.v) x = -std::fabs(x); break;
            case 13: for (float& x : o.v) x = x != 0 ? 0.f : 1.f; break;
            default: break;
        }
        return o;
    }
    void write(const Shader::Ins& in, int lane, V4 val) {
        uint32_t t = in.dst, ty = reg_type(t), n = t & 0x7ff, mask = (t >> 16) & 15, mod = (t >> 20) & 15;
        if ((t >> 13) & 1) { uint32_t rt = reg_type(in.dst_rel); int off = rt == 15 ? c.aL[lane] : c.a0[(in.dst_rel >> 16) & 3][lane]; n = uint32_t(std::max(0, int(n) + off)); }
        if (mod & 1) for (float& x : val.v) x = std::min(1.f, std::max(0.f, x));
        if (in.pred) { for (int k = 0; k < 4; k++) { bool pv = c.p[(in.pred_swz >> (2 * k)) & 3][lane]; if (pv == in.pred_not) mask &= ~(1u << k); } }
        V4* d = nullptr;
        switch (ty) {
            case 0: d = &c.r[n & 31][lane]; break;
            case 6: d = &c.out[n & 15][lane]; break;
            case 5: d = &c.out[(n & 1) + 14][lane]; break;   // oD0/oD1 (vs_2_x)
            case 4: d = &c.rast[n % 3][lane]; break;
            case 8: d = &c.oc[n & 3][lane]; break;
            case 9: c.odepth[lane] = val.v[0]; return;
            case 3: if (!c.ps) { for (int k = 0; k < 4; k++) if (mask & (1 << k)) c.a0[k][lane] = int32_t(std::floor(val.v[k] + 0.5f)); return; } d = &c.r[n & 31][lane]; break;
            case 19: for (int k = 0; k < 4; k++) if (mask & (1 << k)) c.p[k][lane] = val.v[k] != 0; return;
            default: return;
        }
        for (int k = 0; k < 4; k++) if (mask & (1 << k)) d->v[k] = val.v[k];
    }
    static float dp(const V4& a, const V4& b, int n) { float s = 0; for (int k = 0; k < n; k++) s += a.v[k] * b.v[k]; return s; }
    static bool compare(uint32_t ctrl, float a, float b) {
        switch (ctrl & 7) { case 1: return a > b; case 2: return a == b; case 3: return a >= b; case 4: return a < b; case 5: return a != b; case 6: return a <= b; }
        return false;
    }
    // texture sample for all active lanes (derivatives across the quad for pixel shaders)
    void texsample(const Shader::Ins& in, uint32_t mask) {
        uint32_t sreg = in.src[1] & 0x7ff;
        int sidx = c.ps ? int(sreg) : int(17 + sreg);
        const Texture* t = sidx < 21 ? c.st->tex[sidx] : nullptr; const Sampler& sm = c.st->samp[std::min(sidx, 20)];
        V4 co[L]; for (int l = 0; l < L; l++) co[l] = src(in, 0, l);
        bool proj = in.op == TEX && in.ctrl == 1, bias = in.op == TEX && in.ctrl == 2;
        if (proj) for (int l = 0; l < L; l++) { float w = co[l].v[3]; if (w != 0) for (int k = 0; k < 3; k++) co[l].v[k] /= w; }
        int tw = 1, th = 1, td = 1;
        if (t && !t->level.empty() && t->level[0]) { tw = int(t->level[0]->w); th = int(t->level[0]->h); td = int(t->level[0]->d); }
        float mb = 0; std::memcpy(&mb, &sm.s[8], 4);
        // gradients
        float lod_q = 0, ratio = 1; float axis[2] = {0, 0};
        if (in.op == TEXLDL) {}
        else if (c.ps || in.op == TEXLDD) {
            float dx[3], dy[3];
            if (in.op == TEXLDD) { V4 gx = src(in, 2, 0), gy = src(in, 3, 0); for (int k = 0; k < 3; k++) { dx[k] = gx.v[k]; dy[k] = gy.v[k]; } }
            else for (int k = 0; k < 3; k++) { dx[k] = co[1].v[k] - co[0].v[k]; dy[k] = co[2].v[k] - co[0].v[k]; }
            float sx = float(tw), sy = float(th);
            if (t && t->kind == 2) {   // cube: face-space gradients of the first lane's face
                int f0; float u0, v0, u1, v1, u2, v2; int f1, f2;
                cube_face(co[0].v, f0, u0, v0); cube_face(co[1].v, f1, u1, v1); cube_face(co[2].v, f2, u2, v2);
                dx[0] = u1 - u0; dx[1] = v1 - v0; dy[0] = u2 - u0; dy[1] = v2 - v0; dx[2] = dy[2] = 0;
            }
            float lx = std::sqrt(dx[0] * dx[0] * sx * sx + dx[1] * dx[1] * sy * sy + (t && t->kind == 3 ? dx[2] * dx[2] * td * td : 0.f));
            float ly = std::sqrt(dy[0] * dy[0] * sx * sx + dy[1] * dy[1] * sy * sy + (t && t->kind == 3 ? dy[2] * dy[2] * td * td : 0.f));
            float mx = std::max(lx, ly), mn = std::min(lx, ly);
            uint32_t minf = sm.s[6];
            if (minf == 3 && mn > 0) {
                ratio = std::min(mx / mn, float(std::max(1u, sm.s[10])));
                lod_q = std::log2(std::max(mx / ratio, 1e-30f));
                if (lx >= ly) { axis[0] = dx[0]; axis[1] = dx[1]; } else { axis[0] = dy[0]; axis[1] = dy[1]; }
            } else lod_q = std::log2(std::max(mx, 1e-30f));
        }
        for (int l = 0; l < L; l++) {
            if (!(mask & (1u << l))) continue;
            float lod = lod_q + mb;
            if (in.op == TEXLDL) lod = co[l].v[3] + mb;
            if (bias) lod += co[l].v[3];
            auto r = sample_tex(t, sm, co[l].v, lod, ratio, ratio > 1 ? axis : nullptr);
            V4 v; std::memcpy(v.v, r.data(), 16);
            uint32_t sw = (in.src[1] >> 16) & 0xff; V4 o; for (int k = 0; k < 4; k++) o.v[k] = swz(v, sw, k);
            write(in, l, o);
        }
    }
    void run() {
        const Shader& s = *c.sh;
        struct Frame { int kind; uint32_t saved, cond, broken; int start; int32_t count, step; int32_t saved_aL[L]; };
        std::vector<Frame> st; st.reserve(16);
        uint32_t mask = c.active_lanes;
        auto loop_broken = [&]() -> uint32_t { for (int i = int(st.size()) - 1; i >= 0; i--) if (st[i].kind == 1) return st[i].broken; return 0; };
        std::vector<int> calls;
        int n = int(s.code.size());
        for (int pc = 0; pc < n; pc++) {
            const Shader::Ins& in = s.code[pc];
            uint32_t op = in.op;
            switch (op) {
                case IF: {
                    V4 b = src(in, 0, 0); bool cond = b.v[0] != 0;   // static boolean: uniform
                    uint32_t cm = cond ? 0xf : 0;
                    st.push_back({0, mask, cm, 0, pc, 0, 0, {}}); mask &= cm;
                    if (!mask) { int j = s.jump[pc]; pc = j; if (s.code[j].op == ELSE) { mask = st.back().saved & ~st.back().cond & ~loop_broken(); if (!mask) { pc = s.jump[j]; mask = st.back().saved & ~loop_broken(); st.pop_back(); } } else { mask = st.back().saved & ~loop_broken(); st.pop_back(); } }
                    continue;
                }
                case IFC: {
                    uint32_t cm = 0; for (int l = 0; l < L; l++) { V4 a = src(in, 0, l), b = src(in, 1, l); if (compare(in.ctrl, a.v[0], b.v[0])) cm |= 1u << l; }
                    st.push_back({0, mask, cm, 0, pc, 0, 0, {}}); mask &= cm;
                    if (!mask) { int j = s.jump[pc]; pc = j; if (s.code[j].op == ELSE) { mask = st.back().saved & ~st.back().cond & ~loop_broken(); if (!mask) { pc = s.jump[j]; mask = st.back().saved & ~loop_broken(); st.pop_back(); } } else { mask = st.back().saved & ~loop_broken(); st.pop_back(); } }
                    continue;
                }
                case ELSE: { Frame& f = st.back(); mask = f.saved & ~f.cond & ~loop_broken(); if (!mask) { pc = s.jump[pc]; mask = f.saved & ~loop_broken(); st.pop_back(); } continue; }
                case ENDIF: { mask = st.back().saved; st.pop_back(); mask &= ~loop_broken(); continue; }
                case LOOP: case REP: {
                    V4 iv = src(in, op == LOOP ? 1 : 0, 0);
                    Frame f{1, mask, 0, 0, pc, int32_t(iv.v[0]), op == LOOP ? int32_t(iv.v[2]) : 0, {}};
                    for (int l = 0; l < L; l++) f.saved_aL[l] = c.aL[l];
                    if (op == LOOP) for (int l = 0; l < L; l++) c.aL[l] = int32_t(iv.v[1]);
                    if (f.count <= 0) { pc = s.jump[pc]; for (int l = 0; l < L; l++) c.aL[l] = f.saved_aL[l]; continue; }
                    st.push_back(f); continue;
                }
                case ENDLOOP: case ENDREP: {
                    Frame& f = st.back(); f.count--;
                    if (op == ENDLOOP) for (int l = 0; l < L; l++) c.aL[l] += f.step;
                    uint32_t live = f.saved & ~f.broken;
                    if (f.count > 0 && live) { mask = live; pc = f.start; continue; }
                    mask = f.saved; if (op == ENDLOOP) for (int l = 0; l < L; l++) c.aL[l] = f.saved_aL[l];
                    st.pop_back(); mask &= ~loop_broken(); continue;
                }
                case BREAK: case BREAKC: case BREAKP: {
                    uint32_t bm = 0;
                    if (op == BREAK) bm = mask;
                    else if (op == BREAKC) { for (int l = 0; l < L; l++) if (mask & (1u << l)) { V4 a = src(in, 0, l), b = src(in, 1, l); if (compare(in.ctrl, a.v[0], b.v[0])) bm |= 1u << l; } }
                    else { for (int l = 0; l < L; l++) if (mask & (1u << l)) { uint32_t t = in.src[0]; int comp = (t >> 16) & 3; bool pv = c.p[comp][l]; if (((t >> 24) & 15) == 13) pv = !pv; if (pv) bm |= 1u << l; } }
                    for (int i = int(st.size()) - 1; i >= 0; i--) if (st[i].kind == 1) { st[i].broken |= bm; break; }
                    mask &= ~bm;
                    if (!mask) {   // skip to the loop end
                        int depth = 0; int j = pc + 1;
                        for (; j < n; j++) { uint32_t o = s.code[j].op; if (o == LOOP || o == REP) depth++; else if (o == ENDLOOP || o == ENDREP) { if (!depth) break; depth--; } }
                        while (!st.empty() && st.back().kind != 1) st.pop_back();
                        pc = j - 1; mask = 0;
                    }
                    continue;
                }
                case CALL: case CALLNZ: {
                    if (op == CALLNZ) { V4 b = src(in, 1, 0); if (b.v[0] == 0) continue; }
                    uint32_t lbl = in.src[0] & 0x7ff; int target = -1;
                    for (int j = 0; j < n; j++) if (s.code[j].op == LABEL && (s.code[j].src[0] & 0x7ff) == lbl) { target = j; break; }
                    if (target >= 0) { calls.push_back(pc); pc = target; }
                    continue;
                }
                case RET: { if (calls.empty()) return; pc = calls.back(); calls.pop_back(); continue; }
                case LABEL: continue;
                case NOP: continue;
                default: break;
            }
            if (!mask) continue;
            if (op == TEX || op == TEXLDL || op == TEXLDD) { texsample(in, mask); continue; }
            if (op == DSX || op == DSY) {
                V4 a[L]; for (int l = 0; l < L; l++) a[l] = src(in, 0, l);
                for (int l = 0; l < L; l++) if (mask & (1u << l)) { V4 o; for (int k = 0; k < 4; k++) o.v[k] = op == DSX ? (a[(l | 1)].v[k] - a[(l & ~1)].v[k]) : (a[(l | 2)].v[k] - a[(l & ~2)].v[k]); write(in, l, o); }
                continue;
            }
            for (int l = 0; l < L; l++) {
                if (!(mask & (1u << l))) continue;
                V4 o{{0, 0, 0, 0}};
                switch (op) {
                    case MOV: o = src(in, 0, l); break;
                    case MOVA: { V4 a = src(in, 0, l); for (int k = 0; k < 4; k++) if ((in.dst >> (16 + k)) & 1) c.a0[k][l] = int32_t(std::floor(a.v[k] + 0.5f)); continue; }
                    case ADD: { V4 a = src(in, 0, l), b = src(in, 1, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] + b.v[k]; break; }
                    case SUB: { V4 a = src(in, 0, l), b = src(in, 1, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] - b.v[k]; break; }
                    case MUL: { V4 a = src(in, 0, l), b = src(in, 1, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] * b.v[k]; break; }
                    case MAD: { V4 a = src(in, 0, l), b = src(in, 1, l), d = src(in, 2, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] * b.v[k] + d.v[k]; break; }
                    case LRP: { V4 a = src(in, 0, l), b = src(in, 1, l), d = src(in, 2, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] * (b.v[k] - d.v[k]) + d.v[k]; break; }
                    case DP3: { V4 a = src(in, 0, l), b = src(in, 1, l); float d = dp(a, b, 3); o = {{d, d, d, d}}; break; }
                    case DP4: { V4 a = src(in, 0, l), b = src(in, 1, l); float d = dp(a, b, 4); o = {{d, d, d, d}}; break; }
                    case DP2ADD: { V4 a = src(in, 0, l), b = src(in, 1, l), d = src(in, 2, l); float x = a.v[0] * b.v[0] + a.v[1] * b.v[1] + d.v[0]; o = {{x, x, x, x}}; break; }
                    case MIN: { V4 a = src(in, 0, l), b = src(in, 1, l); for (int k = 0; k < 4; k++) o.v[k] = std::isnan(a.v[k]) ? b.v[k] : std::isnan(b.v[k]) ? a.v[k] : a.v[k] < b.v[k] ? a.v[k] : b.v[k]; break; }
                    case MAX: { V4 a = src(in, 0, l), b = src(in, 1, l); for (int k = 0; k < 4; k++) o.v[k] = std::isnan(a.v[k]) ? b.v[k] : std::isnan(b.v[k]) ? a.v[k] : a.v[k] >= b.v[k] ? a.v[k] : b.v[k]; break; }
                    case SLT: { V4 a = src(in, 0, l), b = src(in, 1, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] < b.v[k] ? 1.f : 0.f; break; }
                    case SGE: { V4 a = src(in, 0, l), b = src(in, 1, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] >= b.v[k] ? 1.f : 0.f; break; }
                    case RCP: { V4 a = src(in, 0, l); float x = a.v[0]; float r = x == 0 ? INFINITY : 1.f / x; o = {{r, r, r, r}}; break; }
                    case RSQ: { V4 a = src(in, 0, l); float x = std::fabs(a.v[0]); float r = x == 0 ? INFINITY : 1.f / std::sqrt(x); o = {{r, r, r, r}}; break; }
                    case EXP: case EXPP: { V4 a = src(in, 0, l); float r = std::exp2(a.v[0]); o = {{r, r, r, r}}; break; }
                    case LOG: case LOGP: { V4 a = src(in, 0, l); float x = std::fabs(a.v[0]); float r = x == 0 ? -INFINITY : std::log2(x); o = {{r, r, r, r}}; break; }
                    case POW: { V4 a = src(in, 0, l), b = src(in, 1, l); float r = std::exp2(std::log2(std::fabs(a.v[0])) * b.v[0]); o = {{r, r, r, r}}; break; }   // as the hardware: exp(log|x| * y)
                    case FRC: { V4 a = src(in, 0, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] - std::floor(a.v[k]); break; }
                    case ABS: { V4 a = src(in, 0, l); for (int k = 0; k < 4; k++) o.v[k] = std::fabs(a.v[k]); break; }
                    case SGN: { V4 a = src(in, 0, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] > 0 ? 1.f : a.v[k] < 0 ? -1.f : 0.f; break; }
                    case NRM: { V4 a = src(in, 0, l); float d = dp(a, a, 3); float r = d == 0 ? 0 : 1.f / std::sqrt(d); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] * r; break; }
                    case CRS: { V4 a = src(in, 0, l), b = src(in, 1, l); o.v[0] = a.v[1] * b.v[2] - a.v[2] * b.v[1]; o.v[1] = a.v[2] * b.v[0] - a.v[0] * b.v[2]; o.v[2] = a.v[0] * b.v[1] - a.v[1] * b.v[0]; break; }
                    case SINCOS: { V4 a = src(in, 0, l); float x = a.v[0]; o.v[0] = std::cos(x); o.v[1] = std::sin(x); o.v[2] = 0; o.v[3] = 0; break; }
                    case CMP: { V4 a = src(in, 0, l), b = src(in, 1, l), d = src(in, 2, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] >= 0 ? b.v[k] : d.v[k]; break; }
                    case CND: { V4 a = src(in, 0, l), b = src(in, 1, l), d = src(in, 2, l); for (int k = 0; k < 4; k++) o.v[k] = a.v[k] > 0.5f ? b.v[k] : d.v[k]; break; }
                    case LIT: { V4 a = src(in, 0, l); o.v[0] = 1; o.v[1] = std::max(0.f, a.v[0]); float pw = std::min(127.9961f, std::max(-127.9961f, a.v[3])); o.v[2] = a.v[0] > 0 && a.v[1] > 0 ? std::pow(a.v[1], pw) : 0; o.v[3] = 1; break; }
                    case DST: { V4 a = src(in, 0, l), b = src(in, 1, l); o.v[0] = 1; o.v[1] = a.v[1] * b.v[1]; o.v[2] = a.v[2]; o.v[3] = b.v[3]; break; }
                    case M4x4: case M4x3: case M3x4: case M3x3: case M3x2: {
                        int rows = op == M4x4 || op == M3x4 ? 4 : op == M3x2 ? 2 : 3, cols = op == M4x4 || op == M4x3 ? 4 : 3;
                        V4 a = src(in, 0, l);
                        for (int r = 0; r < rows; r++) { Shader::Ins t = in; t.src[1] = (in.src[1] & ~0x7ffu) | ((in.src[1] & 0x7ff) + uint32_t(r)); V4 b = src(t, 1, l); o.v[r] = dp(a, b, cols); }
                        break;
                    }
                    case SETP: { V4 a = src(in, 0, l), b = src(in, 1, l); uint32_t mk = (in.dst >> 16) & 15; for (int k = 0; k < 4; k++) if (mk & (1 << k)) c.p[k][l] = compare(in.ctrl, a.v[k], b.v[k]); continue; }
                    case TEXKILL: {
                        uint32_t t = in.dst, mk = (t >> 16) & 15; V4 a = read(t & ~(0xffu << 16) & ~(15u << 24), 0, l);
                        for (int k = 0; k < 4; k++) if ((mk & (1 << k)) && a.v[k] < 0) c.kill |= 1u << l;
                        continue;
                    }
                    case BEM: case TEXCOORD: default: continue;
                }
                write(in, l, o);
            }
        }
    }
};

// ------------------------------------------------------------------------------------------------ vertices
struct Vtx { float pos[4]; float att[12][4]; };
float read_elem(const uint8_t* p, uint8_t type, int comp, float def) {
    auto f = [&](int i) { float v; std::memcpy(&v, p + 4 * i, 4); return v; };
    auto s16 = [&](int i) { int16_t v; std::memcpy(&v, p + 2 * i, 2); return v; };
    auto u16 = [&](int i) { uint16_t v; std::memcpy(&v, p + 2 * i, 2); return v; };
    switch (type) {
        case 0: return comp < 1 ? f(comp) : comp == 3 ? 1.f : 0.f;
        case 1: return comp < 2 ? f(comp) : comp == 3 ? 1.f : 0.f;
        case 2: return comp < 3 ? f(comp) : 1.f;
        case 3: return f(comp);
        case 4: { uint32_t v; std::memcpy(&v, p, 4); int sh[4] = {16, 8, 0, 24}; return float((v >> sh[comp]) & 255) / 255.f; }   // D3DCOLOR: bgra -> rgba
        case 5: return float(p[comp]);                         // UBYTE4
        case 6: return comp < 2 ? float(s16(comp)) : comp == 3 ? 1.f : 0.f;   // SHORT2
        case 7: return float(s16(comp));                        // SHORT4
        case 8: return float(p[comp]) / 255.f;                  // UBYTE4N
        case 9: return comp < 2 ? std::max(-1.f, float(s16(comp)) / 32767.f) : comp == 3 ? 1.f : 0.f;   // SHORT2N
        case 10: return std::max(-1.f, float(s16(comp)) / 32767.f);   // SHORT4N
        case 11: return comp < 2 ? float(u16(comp)) / 65535.f : comp == 3 ? 1.f : 0.f;   // USHORT2N
        case 12: return float(u16(comp)) / 65535.f;             // USHORT4N
        case 13: { uint32_t v; std::memcpy(&v, p, 4); if (comp == 3) return 1.f; return float((v >> (10 * comp)) & 1023); }   // UDEC3
        case 14: { uint32_t v; std::memcpy(&v, p, 4); if (comp == 3) return 1.f; int32_t x = int32_t(v << (22 - 10 * comp)) >> 22; return std::max(-1.f, float(x) / 511.f); }   // DEC3N
        case 15: return comp < 2 ? half_to_float(u16(comp)) : comp == 3 ? 1.f : 0.f;   // FLOAT16_2
        case 16: return half_to_float(u16(comp));               // FLOAT16_4
    }
    return def;
}
int elem_size(uint8_t type) { static const int s[] = {4, 8, 12, 16, 4, 4, 4, 8, 4, 4, 8, 4, 8, 4, 4, 4, 8}; return type < 17 ? s[type] : 0; }

}  // namespace

// ------------------------------------------------------------------------------------------------ raster
namespace {
struct RVtx { float x, y, z, w, rw; float att[12][4]; };   // screen position, 1/w; attributes pre-multiplied by 1/w
uint32_t ps_input_map(const Shader& vs, const Shader& ps, int map[16]) {   // ps input register -> vs output register
    uint32_t used = 0;
    for (int i = 0; i < 16; i++) map[i] = -1;
    for (auto& d : ps.dcls) {
        if (d.reg_type != 1) continue;
        if (ps.major < 3) { map[d.reg] = d.reg; used |= 1u << d.reg; continue; }
        for (auto& o : vs.dcls) if (o.reg_type == 6 && o.usage == d.usage && o.index == d.index) { map[d.reg] = int(o.reg); used |= 1u << d.reg; }
    }
    if (ps.major < 3) {   // ps_2_x: t# texcoord (reg type 3) and v# colours from vs_2_x oT#/oD#
        for (auto& d : ps.dcls) { if (d.reg_type == 3) map[8 + (d.reg & 7)] = int(d.reg); }
    }
    return used;
}
bool blend_ok(uint32_t f) { return f >= 1 && f <= 15; }
void blend_factor(uint32_t f, const float* s, const float* d, const float* bf, float* out, bool alpha_only) {
    (void)alpha_only;
    switch (f) {
        case 1: out[0] = out[1] = out[2] = out[3] = 0; break;
        case 2: out[0] = out[1] = out[2] = out[3] = 1; break;
        case 3: for (int k = 0; k < 4; k++) out[k] = s[k]; break;
        case 4: for (int k = 0; k < 4; k++) out[k] = 1 - s[k]; break;
        case 5: for (int k = 0; k < 4; k++) out[k] = s[3]; break;
        case 6: for (int k = 0; k < 4; k++) out[k] = 1 - s[3]; break;
        case 7: for (int k = 0; k < 4; k++) out[k] = d[3]; break;
        case 8: for (int k = 0; k < 4; k++) out[k] = 1 - d[3]; break;
        case 9: for (int k = 0; k < 4; k++) out[k] = d[k]; break;
        case 10: for (int k = 0; k < 4; k++) out[k] = 1 - d[k]; break;
        case 11: { float a = std::min(s[3], 1 - d[3]); out[0] = out[1] = out[2] = a; out[3] = 1; break; }
        case 12: for (int k = 0; k < 4; k++) out[k] = s[3]; break;
        case 13: for (int k = 0; k < 4; k++) out[k] = 1 - s[3]; break;
        case 14: for (int k = 0; k < 4; k++) out[k] = bf[k]; break;
        case 15: for (int k = 0; k < 4; k++) out[k] = 1 - bf[k]; break;
        default: out[0] = out[1] = out[2] = out[3] = 1;
    }
}
bool cmpf(uint32_t func, float a, float b) {   // a: incoming, b: stored
    switch (func) { case 1: return false; case 2: return a < b; case 3: return a == b; case 4: return a <= b; case 5: return a > b; case 6: return a != b; case 7: return a >= b; case 8: return true; }
    return true;
}
bool cmpu(uint32_t func, uint32_t ref, uint32_t val) {
    switch (func) { case 1: return false; case 2: return ref < val; case 3: return ref == val; case 4: return ref <= val; case 5: return ref > val; case 6: return ref != val; case 7: return ref >= val; case 8: return true; }
    return true;
}
uint8_t stencil_op(uint32_t op, uint8_t v, uint8_t ref) {
    switch (op) { case 1: return v; case 2: return 0; case 3: return ref; case 4: return v == 255 ? 255 : uint8_t(v + 1); case 5: return v == 0 ? 0 : uint8_t(v - 1); case 6: return uint8_t(~v); case 7: return uint8_t(v + 1); case 8: return uint8_t(v - 1); }
    return v;
}

struct Raster {
    State& s; Stats& st; const Shader* ps; int map[16]; uint32_t used;
    float bf[4];
    bool writes_depth = false;
    Ctx* ctx;
    Raster(State& ss, Stats& t) : s(ss), st(t), ps(ss.ps) {
        used = ps_input_map(*s.vs, *s.ps, map);
        uint32_t b = s.rs[193]; bf[0] = ((b >> 16) & 255) / 255.f; bf[1] = ((b >> 8) & 255) / 255.f; bf[2] = (b & 255) / 255.f; bf[3] = (b >> 24) / 255.f;
        ctx = new Ctx(); ctx->st = &s; ctx->sh = s.ps; ctx->ps = true; ctx->cf = s.psf; ctx->ci = s.psi; ctx->cb = s.psb;
        for (auto& d : ps->defs) if (d.first < 256) { std::memcpy(ctx->local_f[d.first], d.second.data(), 16); ctx->has_local[d.first] = true; }
        for (auto& d : ps->defis) if (d.first < 16) { std::memcpy(ctx->local_i[d.first], d.second.data(), 16); ctx->has_local_i[d.first] = true; }
        for (auto& d : ps->defbs) if (d.first < 16) { ctx->local_b[d.first] = d.second; ctx->has_local_b[d.first] = true; }
        for (auto& in : ps->code) if (in.ndst && reg_type(in.dst) == 9) writes_depth = true;
    }
    ~Raster() { delete ctx; }
    void triangle(const RVtx& a, const RVtx& b, const RVtx& c, bool front) {
        // bounding box over the viewport and scissor
        int x0 = int(s.vp[0]), y0 = int(s.vp[1]), x1 = int(s.vp[0] + s.vp[2]), y1 = int(s.vp[1] + s.vp[3]);
        if (s.rs[174]) { x0 = std::max(x0, s.scissor[0]); y0 = std::max(y0, s.scissor[1]); x1 = std::min(x1, s.scissor[2]); y1 = std::min(y1, s.scissor[3]); }
        Image* rt0 = s.rt[0];
        uint32_t tw = rt0 ? rt0->w : (s.depth ? s.depth->w : 0), th = rt0 ? rt0->h : (s.depth ? s.depth->h : 0);
        x1 = std::min(x1, int(tw)); y1 = std::min(y1, int(th)); x0 = std::max(x0, 0); y0 = std::max(y0, 0);
        float minx = std::min({a.x, b.x, c.x}), maxx = std::max({a.x, b.x, c.x}), miny = std::min({a.y, b.y, c.y}), maxy = std::max({a.y, b.y, c.y});
        int bx0 = std::max(x0, int(std::ceil(minx))) & ~1, by0 = std::max(y0, int(std::ceil(miny))) & ~1;
        int bx1 = std::min(x1 - 1, int(std::floor(maxx))), by1 = std::min(y1 - 1, int(std::floor(maxy)));
        if (bx1 < bx0 - 1 || by1 < by0 - 1) return;
        double area = double(b.x - a.x) * (c.y - a.y) - double(b.y - a.y) * (c.x - a.x);
        if (area == 0) return;
        // edge functions (orient so that inside >= 0)
        auto edge = [](const RVtx& p, const RVtx& q, double x, double y) { return double(q.x - p.x) * (y - p.y) - double(q.y - p.y) * (x - p.x); };
        bool ccw = area < 0;   // screen y down
        const RVtx* v0 = &a; const RVtx* v1 = ccw ? &c : &b; const RVtx* v2 = ccw ? &b : &c;
        double ar = std::fabs(area);
        auto topleft = [](const RVtx& p, const RVtx& q) { double dy = q.y - p.y, dx = q.x - p.x; return (dy == 0 && dx < 0) || dy > 0; };
        (void)topleft;
        bool tl0 = [&]{ double dy = v2->y - v1->y, dx = v2->x - v1->x; return (dy == 0 && dx > 0) || dy < 0; }();
        bool tl1 = [&]{ double dy = v0->y - v2->y, dx = v0->x - v2->x; return (dy == 0 && dx > 0) || dy < 0; }();
        bool tl2 = [&]{ double dy = v1->y - v0->y, dx = v1->x - v0->x; return (dy == 0 && dx > 0) || dy < 0; }();
        // depth slope for the bias
        float dzdx = 0, dzdy = 0;
        { double e = area; dzdx = float(((b.z - a.z) * (c.y - a.y) - (c.z - a.z) * (b.y - a.y)) / e); dzdy = float(((c.z - a.z) * (b.x - a.x) - (b.z - a.z) * (c.x - a.x)) / e); }
        float bias = 0, slope = 0; std::memcpy(&bias, &s.rs[195], 4); std::memcpy(&slope, &s.rs[175], 4);
        float zoff = bias + slope * std::max(std::fabs(dzdx), std::fabs(dzdy));
        int ninputs = 16;
        for (int qy = by0; qy <= by1; qy += 2)
            for (int qx = bx0; qx <= bx1; qx += 2) {
                uint32_t cover = 0; double w[4][3];
                for (int l = 0; l < 4; l++) {
                    double px = qx + (l & 1), py = qy + (l >> 1);
                    double e0 = edge(*v1, *v2, px, py), e1 = edge(*v2, *v0, px, py), e2 = edge(*v0, *v1, px, py);
                    w[l][0] = e0 / ar; w[l][1] = e1 / ar; w[l][2] = e2 / ar;
                    bool in = (e0 > 0 || (e0 == 0 && tl0)) && (e1 > 0 || (e1 == 0 && tl1)) && (e2 > 0 || (e2 == 0 && tl2));
                    if (in && px >= x0 && px < x1 && py >= y0 && py < y1) cover |= 1u << l;
                }
                if (!cover) continue;
                // barycentrics are for v0, v1, v2 (v1/v2 swapped when ccw)
                const RVtx* vv[3] = {v0, v1, v2};
                float z[4];
                for (int l = 0; l < 4; l++) {
                    double rw = w[l][0] * vv[0]->rw + w[l][1] * vv[1]->rw + w[l][2] * vv[2]->rw;
                    z[l] = float(w[l][0] * vv[0]->z + w[l][1] * vv[1]->z + w[l][2] * vv[2]->z) + zoff;
                    double iw = rw != 0 ? 1.0 / rw : 0;
                    for (int r = 0; r < ninputs; r++) {
                        if (!(used & (1u << r))) continue;
                        int o = map[r]; if (o < 0 || o >= 12) continue;
                        for (int k = 0; k < 4; k++) ctx->in[r][l].v[k] = float((w[l][0] * vv[0]->att[o][k] + w[l][1] * vv[1]->att[o][k] + w[l][2] * vv[2]->att[o][k]) * iw);
                    }
                    ctx->misc[0][l] = {{float(qx + (l & 1)), float(qy + (l >> 1)), 0, 0}};
                    float f = front ? 1.f : -1.f; ctx->misc[1][l] = {{f, f, f, f}};
                    ctx->odepth[l] = z[l];
                }
                shade_quad(qx, qy, cover, z);
            }
    }
    void shade_quad(int qx, int qy, uint32_t cover, const float* z) {
        Ctx& c = *ctx;
        // early depth/stencil rejection is a pure optimisation only when the shader neither kills nor writes depth; keep late test
        for (int i = 0; i < 4; i++) for (int l = 0; l < 4; l++) c.oc[i][l] = {{0, 0, 0, 0}};
        c.kill = 0; c.active_lanes = 0xf;
        Exec e(c); e.run();
        st.shaded += uint64_t(__builtin_popcount(cover));
        for (int l = 0; l < 4; l++) {
            if (!(cover & (1u << l)) || (c.kill & (1u << l))) continue;
            int px = qx + (l & 1), py = qy + (l >> 1);
            float* col = c.oc[0][l].v;
            if (s.rt[0] && unorm_target(s.rt[0]->format)) for (int k = 0; k < 4; k++) col[k] = std::min(1.f, std::max(0.f, col[k]));
            if (s.rs[15]) {   // alpha test
                float a = std::min(1.f, std::max(0.f, col[3]));
                uint32_t a8 = uint32_t(a * 255.f + 0.5f);
                if (!cmpu(s.rs[25], a8, s.rs[24] & 255)) continue;
            }
            float zz = writes_depth ? c.odepth[l] : z[l];
            zz = std::min(1.f, std::max(0.f, zz));
            size_t di = 0;
            if (s.depth) {
                if (uint32_t(px) >= s.depth->w || uint32_t(py) >= s.depth->h) continue;
                di = size_t(py) * s.depth->w + size_t(px);
            }
            // stencil
            bool ztest_pass = true;
            if (s.depth && s.rs[7]) { float dz; std::memcpy(&dz, &s.depth->bytes[4 * di], 4); ztest_pass = cmpf(s.rs[23], zz, dz); }
            if (s.depth && s.rs[52] && s.stencil) {
                uint8_t& sv = (*s.stencil)[di]; uint32_t ref = s.rs[57] & 255, msk = s.rs[58] & 255, wm = s.rs[59] & 255;
                bool front = c.misc[1][l].v[0] > 0;
                bool ccwside = s.rs[185] && !front;
                uint32_t fn = ccwside ? s.rs[189] : s.rs[56], fail = ccwside ? s.rs[186] : s.rs[53], zfail = ccwside ? s.rs[187] : s.rs[54], pass = ccwside ? s.rs[188] : s.rs[55];
                bool sp = cmpu(fn, ref & msk, sv & msk);
                uint8_t nv = !sp ? stencil_op(fail, sv, uint8_t(ref)) : !ztest_pass ? stencil_op(zfail, sv, uint8_t(ref)) : stencil_op(pass, sv, uint8_t(ref));
                sv = uint8_t((sv & ~wm) | (nv & wm));
                if (!sp) continue;
            }
            if (!ztest_pass) continue;
            if (s.depth && s.rs[7] && s.rs[14]) std::memcpy(&s.depth->bytes[4 * di], &zz, 4);
            st.pixels++;
            for (int i = 0; i < 4; i++) {
                Image* im = s.rt[i]; if (!im) continue;
                if (uint32_t(px) >= im->w || uint32_t(py) >= im->h) continue;
                uint32_t wmask = i == 0 ? s.rs[168] : s.rs[189 + i];
                if (!(wmask & 15)) continue;
                float sc[4]; std::memcpy(sc, c.oc[i][l].v, 16);
                if (std::isnan(sc[0]) || std::isnan(sc[1]) || std::isnan(sc[2]) || std::isnan(sc[3])) st.nan_pixels++;
                if (unorm_target(im->format)) for (float& x : sc) x = std::min(1.f, std::max(0.f, x));
                auto dc = load_texel(*im, uint32_t(px), uint32_t(py));
                float out[4];
                if (s.rs[27]) {
                    float fs[4], fd[4];
                    uint32_t sb = s.rs[19], db = s.rs[20], op = s.rs[171];
                    if (sb == 12) { sb = 5; db = 6; } else if (sb == 13) { sb = 6; db = 5; }
                    blend_factor(sb, sc, dc.data(), bf, fs, false); blend_factor(db, sc, dc.data(), bf, fd, false);
                    float fsa[4], fda[4]; uint32_t opa = op;
                    if (s.rs[206]) { blend_factor(s.rs[207], sc, dc.data(), bf, fsa, true); blend_factor(s.rs[208], sc, dc.data(), bf, fda, true); opa = s.rs[209]; fs[3] = fsa[3]; fd[3] = fda[3]; }
                    for (int k = 0; k < 4; k++) {
                        uint32_t o = k == 3 ? opa : op; float a = sc[k] * fs[k], b2 = dc[k] * fd[k];
                        out[k] = o == 2 ? a - b2 : o == 3 ? b2 - a : o == 4 ? std::min(sc[k], dc[k]) : o == 5 ? std::max(sc[k], dc[k]) : a + b2;
                    }
                } else std::memcpy(out, sc, 16);
                for (int k = 0; k < 4; k++) { int ch = k; if (!(wmask & (1u << ch))) out[k] = dc[k]; }
                store_texel(*im, uint32_t(px), uint32_t(py), out);
                if (i == 0 && st.id_buffer && im == st.id_target) (*st.id_buffer)[size_t(py) * im->w + size_t(px)] = st.id;
                if (i == 0 && px == st.trace_x && py == st.trace_y) { char b[200]; std::snprintf(b, sizeof b, "src %g %g %g %g -> %g %g %g %g z %g", sc[0], sc[1], sc[2], sc[3], out[0], out[1], out[2], out[3], zz); st.trace.push_back(b); }
            }
        }
    }
};

// clip a polygon (clip-space positions + attributes) against z >= 0, z <= w and w > eps
struct CV { float p[4]; float att[12][4]; };
void clip_poly(std::vector<CV>& poly) {
    auto clip = [&](auto dist) {
        std::vector<CV> out; size_t n = poly.size(); if (!n) return;
        for (size_t i = 0; i < n; i++) {
            const CV& a = poly[i]; const CV& b = poly[(i + 1) % n];
            float da = dist(a), db = dist(b);
            if (da >= 0) out.push_back(a);
            if ((da >= 0) != (db >= 0)) {
                float t = da / (da - db); CV c;
                for (int k = 0; k < 4; k++) c.p[k] = a.p[k] + (b.p[k] - a.p[k]) * t;
                for (int r = 0; r < 12; r++) for (int k = 0; k < 4; k++) c.att[r][k] = a.att[r][k] + (b.att[r][k] - a.att[r][k]) * t;
                out.push_back(c);
            }
        }
        poly.swap(out);
    };
    clip([](const CV& v) { return v.p[2]; });
    clip([](const CV& v) { return v.p[3] - v.p[2]; });
    clip([](const CV& v) { return v.p[3] - 1e-6f; });
    // guard band: x and y within +-w * 64 (the viewport bounding box clamps the rest)
    clip([](const CV& v) { return 64 * v.p[3] - v.p[0]; });
    clip([](const CV& v) { return 64 * v.p[3] + v.p[0]; });
    clip([](const CV& v) { return 64 * v.p[3] - v.p[1]; });
    clip([](const CV& v) { return 64 * v.p[3] + v.p[1]; });
}
}  // namespace

void clear(State& s, uint32_t flags, uint32_t color, float z, uint32_t stencil, const std::vector<std::array<int32_t, 4>>& rects) {
    float c[4] = {((color >> 16) & 255) / 255.f, ((color >> 8) & 255) / 255.f, (color & 255) / 255.f, (color >> 24) / 255.f};
    int32_t vx0 = int32_t(s.vp[0]), vy0 = int32_t(s.vp[1]), vx1 = int32_t(s.vp[0] + s.vp[2]), vy1 = int32_t(s.vp[1] + s.vp[3]);
    if (s.rs[174]) { vx0 = std::max(vx0, s.scissor[0]); vy0 = std::max(vy0, s.scissor[1]); vx1 = std::min(vx1, s.scissor[2]); vy1 = std::min(vy1, s.scissor[3]); }
    std::vector<std::array<int32_t, 4>> rs = rects; if (rs.empty()) rs.push_back({vx0, vy0, vx1, vy1});
    for (auto r : rs) {
        r[0] = std::max(r[0], vx0); r[1] = std::max(r[1], vy0); r[2] = std::min(r[2], vx1); r[3] = std::min(r[3], vy1);
        if (flags & 1) for (int i = 0; i < 4; i++) { Image* im = s.rt[i]; if (!im) continue; for (int y = std::max(0, r[1]); y < std::min(int(im->h), r[3]); y++) for (int x = std::max(0, r[0]); x < std::min(int(im->w), r[2]); x++) store_texel(*im, uint32_t(x), uint32_t(y), c); }
        if (s.depth) for (int y = std::max(0, r[1]); y < std::min(int(s.depth->h), r[3]); y++) for (int x = std::max(0, r[0]); x < std::min(int(s.depth->w), r[2]); x++) {
            size_t i = size_t(y) * s.depth->w + size_t(x);
            if (flags & 2) std::memcpy(&s.depth->bytes[4 * i], &z, 4);
            if ((flags & 4) && s.stencil) (*s.stencil)[i] = uint8_t(stencil);
        }
    }
}

bool draw(State& s, uint32_t prim, uint32_t start, uint32_t prim_count, bool indexed, int32_t base_vertex, uint32_t min_index, uint32_t num_vertices, Stats& st) {
    (void)min_index; (void)num_vertices;
    st.draws++;
    if (!s.vs || !s.ps || s.vs->code.empty() || s.ps->code.empty()) { st.last_error = "no shader"; return false; }
    if (s.rs[8] != 3 && s.rs[8] != 0) { /* wireframe/point fill: rasterized solid */ }
    // vertex shader inputs: declaration element per vs input register
    struct In { int reg; const VertexElement* e; };
    std::vector<In> ins;
    for (auto& d : s.vs->dcls) if (d.reg_type == 1) for (auto& e : s.decl) if (e.usage == d.usage && e.index == d.index) { ins.push_back({int(d.reg), &e}); break; }
    // outputs: position register
    int pos_out = -1; for (auto& d : s.vs->dcls) if (d.reg_type == 6 && d.usage == 0 && d.index == 0) pos_out = int(d.reg);
    // index list
    uint32_t nidx = prim == 4 ? 3 * prim_count : prim == 5 || prim == 6 ? prim_count + 2 : prim == 2 ? 2 * prim_count : prim == 3 ? prim_count + 1 : prim_count;
    std::vector<uint32_t> idx(nidx);
    for (uint32_t i = 0; i < nidx; i++) {
        if (indexed) {
            if (s.index_format == 102) { size_t o = 4 * size_t(start + i); uint32_t v = 0; if (s.indices && o + 4 <= s.index_size) std::memcpy(&v, s.indices + o, 4); idx[i] = uint32_t(int32_t(v) + base_vertex); }
            else { size_t o = 2 * size_t(start + i); uint16_t v = 0; if (s.indices && o + 2 <= s.index_size) std::memcpy(&v, s.indices + o, 2); idx[i] = uint32_t(int32_t(v) + base_vertex); }
        } else idx[i] = start + i;
    }
    // instancing (stream frequency): stream 0 indexed data with count, others instance data
    uint32_t instances = 1; for (int i = 0; i < 16; i++) if (s.streams[i].freq & 0x40000000u) instances = s.streams[i].freq & 0xffffff;
    if (!(s.streams[0].freq & 0x40000000u)) instances = 1;
    // run the vertex shader once per unique vertex (4 at a time)
    std::vector<uint32_t> uniq(idx); std::sort(uniq.begin(), uniq.end()); uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    Ctx* vc = new Ctx(); vc->st = &s; vc->sh = s.vs; vc->ps = false; vc->cf = s.vsf; vc->ci = s.vsi; vc->cb = s.vsb;
    for (auto& d : s.vs->defs) if (d.first < 256) { std::memcpy(vc->local_f[d.first], d.second.data(), 16); vc->has_local[d.first] = true; }
    for (auto& d : s.vs->defis) if (d.first < 16) { std::memcpy(vc->local_i[d.first], d.second.data(), 16); vc->has_local_i[d.first] = true; }
    for (auto& d : s.vs->defbs) if (d.first < 16) { vc->local_b[d.first] = d.second; vc->has_local_b[d.first] = true; }
    Raster rast(s, st);
    for (uint32_t inst = 0; inst < instances; inst++) {
        std::map<uint32_t, CV> out;
        for (size_t i0 = 0; i0 < uniq.size(); i0 += 4) {
            int n = int(std::min<size_t>(4, uniq.size() - i0));
            std::memset(vc->in, 0, sizeof vc->in); std::memset(vc->out, 0, sizeof vc->out); std::memset(vc->rast, 0, sizeof vc->rast); std::memset(vc->r, 0, sizeof vc->r);
            std::memset(vc->a0, 0, sizeof vc->a0); std::memset(vc->aL, 0, sizeof vc->aL);
            for (int l = 0; l < n; l++) {
                uint32_t vi = uniq[i0 + size_t(l)];
                for (auto& in : ins) {
                    const auto& sm = s.streams[in.e->stream];
                    uint32_t elem_index = (s.streams[in.e->stream].freq & 0x80000000u) ? inst : vi;
                    size_t off = size_t(sm.offset) + size_t(elem_index) * sm.stride + in.e->offset;
                    if (!sm.data || off + size_t(elem_size(in.e->type)) > sm.size) continue;
                    for (int k = 0; k < 4; k++) vc->in[in.reg][l].v[k] = read_elem(sm.data + off, in.e->type, k, 0);
                }
            }
            vc->active_lanes = (1u << n) - 1;
            Exec e(*vc); e.run();
            st.vertices += uint64_t(n);
            for (int l = 0; l < n; l++) {
                CV v; std::memset(&v, 0, sizeof v);
                if (pos_out >= 0) std::memcpy(v.p, vc->out[pos_out][l].v, 16); else std::memcpy(v.p, vc->rast[0][l].v, 16);
                for (int r = 0; r < 12; r++) std::memcpy(v.att[r], vc->out[r][l].v, 16);
                out[uniq[i0 + size_t(l)]] = v;
            }
        }
        // assemble triangles
        auto tri = [&](uint32_t ia, uint32_t ib, uint32_t ic) {
            std::vector<CV> poly = {out[ia], out[ib], out[ic]};
            clip_poly(poly);
            if (poly.size() < 3) return;
            std::vector<RVtx> rv(poly.size());
            for (size_t i = 0; i < poly.size(); i++) {
                const CV& c = poly[i]; RVtx& r = rv[i]; float rw = 1.f / c.p[3];
                r.x = float(s.vp[0]) + (1 + c.p[0] * rw) * float(s.vp[2]) * 0.5f;
                r.y = float(s.vp[1]) + (1 - c.p[1] * rw) * float(s.vp[3]) * 0.5f;
                r.z = s.vp_minz + c.p[2] * rw * (s.vp_maxz - s.vp_minz); r.w = c.p[3]; r.rw = rw;
                for (int a = 0; a < 12; a++) for (int k = 0; k < 4; k++) r.att[a][k] = c.att[a][k] * rw;
            }
            // winding of the unclipped triangle in screen space (clockwise = front)
            double area = double(rv[1].x - rv[0].x) * (rv[2].y - rv[0].y) - double(rv[1].y - rv[0].y) * (rv[2].x - rv[0].x);
            bool cw = area > 0;   // y down: positive cross = clockwise on screen
            uint32_t cull = s.rs[22];
            if (cull == 2 && cw) return;
            if (cull == 3 && !cw) return;
            for (size_t i = 1; i + 1 < rv.size(); i++) { rast.triangle(rv[0], rv[i], rv[i + 1], cw); st.prims++; }
        };
        if (prim == 4) for (uint32_t i = 0; i + 2 < nidx; i += 3) tri(idx[i], idx[i + 1], idx[i + 2]);
        else if (prim == 5) for (uint32_t i = 0; i + 2 < nidx; i++) { if (i & 1) tri(idx[i + 1], idx[i], idx[i + 2]); else tri(idx[i], idx[i + 1], idx[i + 2]); }
        else if (prim == 6) for (uint32_t i = 1; i + 1 < nidx; i++) tri(idx[0], idx[i], idx[i + 1]);
        else { st.last_error = "lines/points not rasterized"; }
    }
    delete vc;
    return true;
}
}  // namespace swr
