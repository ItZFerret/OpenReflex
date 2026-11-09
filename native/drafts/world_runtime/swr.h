// Software Direct3D 9 rasterizer for the render capture (render_capture.cpp): executes the game's own shader model 3
// bytecode (vs_3_0 / ps_3_0, also 2.x) per vertex and per 2x2 pixel quad, with D3D9 rasterization rules (pixel centres
// at integer coordinates, top-left fill), depth / stencil, alpha test, blending and write masks into memory images of the
// recording device's render targets. Reference renderer for the Unity port, not used at run time.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace swr {

// One mip level / cube face of a texture or a render target, in its D3D format and memory layout.
struct Image {
    uint32_t format = 0, w = 0, h = 0, d = 1, pitch = 0;
    std::vector<uint8_t> bytes;
    uint64_t version = 0;          // bumped on every write by the rasterizer
};
struct Texture {
    int kind = 1;                  // 1 2D, 2 cube, 3 volume (Boot::D3DResource kind)
    uint32_t levels = 1;
    std::vector<Image*> level;     // cube: face * levels + level
};

struct Sampler { uint32_t s[14] = {0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0}; };   // D3DSAMPLERSTATETYPE 1..13 (index = type)

struct Shader {
    bool ps = false; uint32_t major = 3, minor = 0;
    std::vector<uint32_t> tokens;
    // decoded
    struct Dcl { uint32_t reg_type, reg, mask, usage, index, sampler_type; };
    std::vector<Dcl> dcls;
    std::vector<std::pair<uint32_t, std::array<float, 4>>> defs;
    std::vector<std::pair<uint32_t, std::array<int32_t, 4>>> defis;
    std::vector<std::pair<uint32_t, uint32_t>> defbs;
    struct Ins { uint32_t op, ctrl; bool pred, pred_not; uint32_t pred_swz; int ndst, nsrc; uint32_t dst; uint32_t dst_rel; uint32_t src[4], src_rel[4]; uint32_t pos; };
    std::vector<Ins> code;
    std::vector<int> jump;         // matching else/endif/endloop/endrep per instruction
    std::string error;
    bool decode();
};

struct VertexElement { uint16_t stream, offset; uint8_t type, method, usage, index; };

// Device state of one draw.
struct State {
    uint32_t rs[256] = {};
    Sampler samp[21];              // 0..15 pixel, 16..20 = D3DDMAPSAMPLER, D3DVERTEXTEXTURESAMPLER0..3
    Texture* tex[21] = {};
    const Shader* vs = nullptr; const Shader* ps = nullptr;
    float vsf[256][4] = {}, psf[224][4] = {};
    int32_t vsi[16][4] = {}, psi[16][4] = {};
    uint32_t vsb[16] = {}, psb[16] = {};
    std::vector<VertexElement> decl;
    struct Stream { const uint8_t* data = nullptr; size_t size = 0; uint32_t offset = 0, stride = 0, freq = 1; } streams[16];
    const uint8_t* indices = nullptr; size_t index_size = 0; uint32_t index_format = 101;   // D3DFMT_INDEX16
    Image* rt[4] = {}; Image* depth = nullptr; std::vector<uint8_t>* stencil = nullptr;   // depth: R32F-like float image
    uint32_t vp[4] = {}; float vp_minz = 0, vp_maxz = 1;
    int32_t scissor[4] = {};
};

struct Stats { uint64_t draws = 0, prims = 0, pixels = 0, shaded = 0, vertices = 0, nan_pixels = 0; std::string last_error;
    int trace_x = -1, trace_y = -1; std::vector<std::string> trace;
    const Image* id_target = nullptr; std::vector<uint32_t>* id_buffer = nullptr; uint32_t id = 0; };   // id buffer: last draw id per pixel of id_target   // trace: writes to pixel (trace_x, trace_y) of render target 0

// prim: D3DPRIMITIVETYPE; vertices are fetched for indices base + index (indexed) or start + i.
bool draw(State& s, uint32_t prim, uint32_t start, uint32_t prim_count, bool indexed, int32_t base_vertex, uint32_t min_index, uint32_t num_vertices, Stats& st);
void clear(State& s, uint32_t flags, uint32_t color, float z, uint32_t stencil, const std::vector<std::array<int32_t, 4>>& rects);
// Texel fetch as the sampler would return it (for dumps): rgba of image texel x, y.
std::array<float, 4> load_texel(const Image& img, uint32_t x, uint32_t y);
void store_texel(Image& img, uint32_t x, uint32_t y, const float* rgba);
std::string disassemble(const Shader& s);
bool format_supported(uint32_t format);
}  // namespace swr
