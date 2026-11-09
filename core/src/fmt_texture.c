/* fmt_texture.c -- see fmt_texture.h. Reference: texture_decode.py (parse_texture, decode_dxt1), bike_asset_export.py
 * (texture_dds), terrain_deform_reference.py (Dxt1 rounding), RfxData.cs (ParseTexture). */
#include "fmt_texture.h"

const char* fmt_texture_format_name(uint32_t f)
{
    switch (f) {
    case FMT_TEX_A8R8G8B8: return "A8R8G8B8";
    case FMT_TEX_X8R8G8B8: return "X8R8G8B8";
    case FMT_TEX_G16R16F: return "G16R16F";
    case FMT_TEX_A16B16G16R16F: return "A16B16G16R16F";
    case FMT_TEX_R32F: return "R32F";
    case FMT_TEX_DXT1: return "DXT1";
    case FMT_TEX_DXT3: return "DXT3";
    case FMT_TEX_DXT5: return "DXT5";
    default: return "unknown";
    }
}

static int block_bytes(uint32_t f) { return f == FMT_TEX_DXT1 ? 8 : (f == FMT_TEX_DXT3 || f == FMT_TEX_DXT5) ? 16 : 0; }
static int pixel_bytes(uint32_t f)
{
    switch (f) {
    case FMT_TEX_A8R8G8B8: case FMT_TEX_X8R8G8B8: case FMT_TEX_G16R16F: case FMT_TEX_R32F: return 4;
    case FMT_TEX_A16B16G16R16F: return 8;
    default: return 0;
    }
}

int fmt_texture_parse(const uint8_t* data, size_t size, fmt_texture* t)
{
    memset(t, 0, sizeof *t);
    if (size < 56 || size > ((size_t)512 << 20)) return fmt_fail("texture: resource size %llu outside bounds", (unsigned long long)size);
    if (fmt_u32(data) != 0x77DDCC00u || fmt_u32(data + 4) != 4 || fmt_u32(data + 8) != 0 || fmt_u32(data + 12) != 1)
        return fmt_fail("texture: unsupported magic / version / image count");
    uint32_t levels = fmt_u32(data + 40), w = fmt_u32(data + 44), h = fmt_u32(data + 48), fmtid = fmt_u32(data + 52);
    if (levels < 1 || levels > 15) return fmt_fail("texture: bad mip count");
    if (w < 1 || h < 1 || w > 16384 || h > 16384) return fmt_fail("texture: bad size %ux%u", w, h);
    int bb = block_bytes(fmtid), pb = pixel_bytes(fmtid);
    if (!bb && !pb) return fmt_fail("texture: unsupported format 0x%08x", fmtid);
    size_t off = 56 + 8 * (size_t)levels;
    if (off > size) return fmt_fail("texture: truncated mip table");
    for (int k = 0; k < 6; k++) t->header[k] = fmt_u32(data + 16 + 4 * (size_t)k);
    t->data = data; t->size = size; t->width = (int)w; t->height = (int)h; t->levels = (int)levels; t->format = fmtid;
    for (uint32_t l = 0; l < levels; l++) {
        uint32_t lw = w >> l ? w >> l : 1, lh = h >> l ? h >> l : 1;
        uint32_t bsz = fmt_u32(data + 56 + 8 * (size_t)l), pitch = fmt_u32(data + 60 + 8 * (size_t)l);
        uint32_t want_pitch, rows;
        if (bb) { want_pitch = ((lw + 3) / 4) * (uint32_t)bb; rows = (lh + 3) / 4; }
        else { want_pitch = lw * (uint32_t)pb; rows = lh; }
        if (pitch != want_pitch || bsz != want_pitch * rows) return fmt_fail("texture: mip %u size / pitch (%u, %u) differ from the dimensions", l, bsz, pitch);
        if (off + bsz > size) return fmt_fail("texture: truncated mip %u", l);
        t->mips[l].width = (int)lw; t->mips[l].height = (int)lh; t->mips[l].bytes = bsz; t->mips[l].pitch = pitch; t->mips[l].offset = off;
        off += bsz;
    }
    t->faces = 1;
    t->face_stride = off - 56;                             /* one face: its mip table and its chain */
    if (off != size) {                                     /* the cube-like environment maps: more faces follow, each with its own table */
        size_t extra = size - off;
        if (t->face_stride == 0 || extra % t->face_stride != 0 || extra / t->face_stride > 5)
            return fmt_fail("texture: %llu unexpected trailing bytes", (unsigned long long)extra);
        t->faces = 1 + (int)(extra / t->face_stride);
    }
    return 0;
}

int fmt_texture_read_r32f(const fmt_texture* t, int level, float* dst)
{
    if (t->format != FMT_TEX_R32F || level < 0 || level >= t->levels) return fmt_fail("texture: not an R32F level");
    memcpy(dst, t->data + t->mips[level].offset, t->mips[level].bytes);     /* little-endian host, like the file */
    return 0;
}

static void rgb565(uint16_t c, int out[3])
{
    int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
    out[0] = (r << 3) | (r >> 2);
    out[1] = (g << 2) | (g >> 4);
    out[2] = (b << 3) | (b >> 2);
}

static void dxt_color_block(const uint8_t* b, int four_color_only, int rounding, uint8_t dst[16][4], int has_alpha_transparent)
{
    uint16_t c0 = fmt_u16(b), c1 = fmt_u16(b + 2);
    uint32_t idx = fmt_u32(b + 4);
    int a[3], bb[3], pal[4][4];
    rgb565(c0, a); rgb565(c1, bb);
    int r = rounding ? 1 : 0;
    for (int k = 0; k < 3; k++) { pal[0][k] = a[k]; pal[1][k] = bb[k]; }
    pal[0][3] = pal[1][3] = 255;
    if (four_color_only || c0 > c1) {
        for (int k = 0; k < 3; k++) { pal[2][k] = (2 * a[k] + bb[k] + r) / 3; pal[3][k] = (a[k] + 2 * bb[k] + r) / 3; }
        pal[2][3] = pal[3][3] = 255;
    } else {
        for (int k = 0; k < 3; k++) { pal[2][k] = (a[k] + bb[k] + r) / 2; pal[3][k] = 0; }
        pal[2][3] = 255; pal[3][3] = has_alpha_transparent ? 0 : 255;
    }
    for (int i = 0; i < 16; i++) {
        int p = (int)((idx >> (2 * i)) & 3);
        dst[i][0] = (uint8_t)pal[p][0]; dst[i][1] = (uint8_t)pal[p][1]; dst[i][2] = (uint8_t)pal[p][2]; dst[i][3] = (uint8_t)pal[p][3];
    }
}

int fmt_texture_decode_rgba8(const fmt_texture* t, int level, int rounding, uint8_t* dst)
{
    if (level < 0 || level >= t->levels) return fmt_fail("texture: bad mip level %d", level);
    const fmt_tex_mip* m = &t->mips[level];
    const uint8_t* src = t->data + m->offset;
    int w = m->width, h = m->height;
    if (t->format == FMT_TEX_A8R8G8B8 || t->format == FMT_TEX_X8R8G8B8) {
        for (int i = 0; i < w * h; i++) {
            dst[4 * i] = src[4 * i + 2]; dst[4 * i + 1] = src[4 * i + 1]; dst[4 * i + 2] = src[4 * i];
            dst[4 * i + 3] = t->format == FMT_TEX_X8R8G8B8 ? 255 : src[4 * i + 3];
        }
        return 0;
    }
    if (!block_bytes(t->format)) return fmt_fail("texture: cannot decode %s to RGBA8", fmt_texture_format_name(t->format));
    int bw = (w + 3) / 4, bh = (h + 3) / 4, bs = block_bytes(t->format);
    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            const uint8_t* b = src + ((size_t)by * bw + bx) * bs;
            uint8_t px[16][4];
            if (t->format == FMT_TEX_DXT1) dxt_color_block(b, 0, rounding, px, 1);
            else {
                dxt_color_block(b + 8, 1, rounding, px, 0);
                if (t->format == FMT_TEX_DXT3) {
                    for (int i = 0; i < 16; i++) px[i][3] = (uint8_t)(((b[i >> 1] >> ((i & 1) * 4)) & 15) * 17);
                } else {
                    int a0 = b[0], a1 = b[1], table[8];
                    table[0] = a0; table[1] = a1;
                    if (a0 > a1) for (int i = 1; i < 7; i++) table[i + 1] = ((7 - i) * a0 + i * a1) / 7;
                    else { for (int i = 1; i < 5; i++) table[i + 1] = ((5 - i) * a0 + i * a1) / 5; table[6] = 0; table[7] = 255; }
                    uint64_t bits = 0;
                    for (int i = 0; i < 6; i++) bits |= (uint64_t)b[2 + i] << (8 * i);
                    for (int i = 0; i < 16; i++) px[i][3] = (uint8_t)table[(bits >> (3 * i)) & 7];
                }
            }
            for (int i = 0; i < 16; i++) {
                int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                if (x < w && y < h) memcpy(dst + ((size_t)y * w + x) * 4, px[i], 4);
            }
        }
    }
    return 0;
}
