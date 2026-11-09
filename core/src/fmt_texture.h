/* fmt_texture.h -- texture resources (type "texture", magic 0x77DDCC00 version 4), as far as the core needs them: the R32F
 * height maps and the DXT masks. Rendering textures stay with Unity's C# readers (ReflexTex).
 *
 * Layout (little endian): 14 u32 { 0x77DDCC00, 4, 0, 1, levels, 0,0,0,0,0, levels, width, height, D3D format } (words 4..9 differ in
 * the environment / reflection maps; the real mip count is word 10),
 * then levels * { u32 byte size, u32 pitch }, then the mip chain, level 0 first, each level's rows top to bottom (blocks for
 * the DXT formats: pitch = blocks per row * block bytes, rows = block rows). Formats: 114 R32F, 21 A8R8G8B8, 22 X8R8G8B8,
 * 112 G16R16F, 113 A16B16G16R16F, 'DXT1' (8-byte blocks), 'DXT3' / 'DXT5' (16-byte blocks). */
#ifndef FMT_TEXTURE_H
#define FMT_TEXTURE_H

#include "fmt_common.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FMT_TEX_A8R8G8B8 = 21, FMT_TEX_X8R8G8B8 = 22, FMT_TEX_G16R16F = 112, FMT_TEX_A16B16G16R16F = 113, FMT_TEX_R32F = 114,
    FMT_TEX_DXT1 = 0x31545844, FMT_TEX_DXT3 = 0x33545844, FMT_TEX_DXT5 = 0x35545844
};

typedef struct fmt_tex_mip { int width, height; uint32_t bytes, pitch; size_t offset; } fmt_tex_mip;

/* A parsed header; `data` is the caller's buffer (not copied, must outlive the struct). */
typedef struct fmt_texture {
    const uint8_t* data; size_t size;
    int width, height, levels;
    uint32_t format;
    uint32_t header[6];            /* words 4..9 as stored: [0] is the mip count in the usual files (the cube-like reflection maps carry other values) */
    fmt_tex_mip mips[16];          /* of face 0 */
    int faces;                     /* 1, or 6 for the cube-like environment maps: face f repeats the table + chain at +f * face_stride */
    size_t face_stride;            /* bytes of one face: levels * 8 table bytes + the chain */
} fmt_texture;

int  fmt_texture_parse(const uint8_t* data, size_t size, fmt_texture* out);        /* 0 / -1 (unknown format, bad sizes ...) */
const char* fmt_texture_format_name(uint32_t format);
/* Copies mip `level` of an R32F texture (width * height floats, row 0 first). */
int  fmt_texture_read_r32f(const fmt_texture* t, int level, float* dst);
/* Decodes a mip to RGBA8, rows top to bottom (the file's order; dst = width * height * 4). Handles A8R8G8B8 / X8R8G8B8 and
 * DXT1 / DXT3 / DXT5. rounding 0: the two interpolated DXT colours are floor((2a+b)/3), floor((a+2b)/3), floor((a+b)/2);
 * rounding 1: (2a+b+1)/3 ... (the variant that matched the live terrain-deformation capture). DXT1 index 3 of a 3-colour
 * block is transparent black. */
int  fmt_texture_decode_rgba8(const fmt_texture* t, int level, int rounding, uint8_t* dst);

#ifdef __cplusplus
}
#endif
#endif
