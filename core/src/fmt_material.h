/* fmt_material.h -- material resources (type "material"): BXML (0x3EA raw or 0x103EA)
 *   <MaterialEditorMaterial numPasses><Pass><Shaders asset="HFTerrain_.."/>  or  <Shaders><VertexShader asset entryPoint
 *   skinningEntryPoint/><PixelShader asset entryPoint/></Shaders><Textures numTextures><Texture textureName><XenonSamplerStates
 *   numStates>(bytes)</..></Texture>..</Textures><Parameters><Parameter name type>(float / vec4)</Parameter>..</Parameters>
 *   <XenonStates numStates>(bytes)</XenonStates></Pass></MaterialEditorMaterial>.
 * The state blobs are numStates pairs of u32 (key, value): sampler states use D3DSAMPLERSTATETYPE keys (1 ADDRESSU, 2 ADDRESSV,
 * 3 ADDRESSW, 5 MAGFILTER, 6 MINFILTER, 7 MIPFILTER, 8 MIPMAPLODBIAS = float bits), the pass states D3DRENDERSTATETYPE keys
 * (7 ZENABLE, 14 ZWRITEENABLE, 15 ALPHATESTENABLE, 22 CULLMODE, 23 ZFUNC, 24 ALPHAREF, 27 ALPHABLENDENABLE). Only the first
 * pass is read (every shipped material has one). The texture order is the sampler order of the pixel shader. */
#ifndef FMT_MATERIAL_H
#define FMT_MATERIAL_H

#include "fmt_bxml.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FMT_MAT_MAX_STATES 16

typedef struct fmt_state_pair { uint32_t key, value; } fmt_state_pair;

typedef struct fmt_material_texture {
    const char* name;
    fmt_state_pair sampler[FMT_MAT_MAX_STATES]; int nsampler;
} fmt_material_texture;

typedef struct fmt_material_param {
    const char* name;
    const char* type;              /* "Color", "Constant" ... */
    float value[4]; int ncomp;     /* 1 (float) or 4 (vec4) */
} fmt_material_param;

typedef struct fmt_material {
    int num_passes;
    const char* shader;            /* <Shaders asset=..> (terrain materials), else the pixel shader asset */
    const char* vertex_shader; const char* vertex_entry; const char* skinning_entry;
    const char* pixel_shader;  const char* pixel_entry;
    fmt_material_texture* textures; int ntextures;
    fmt_material_param* params; int nparams;
    fmt_state_pair states[FMT_MAT_MAX_STATES]; int nstates;
    fmt_bxml* doc;                 /* owns the strings above */
} fmt_material;

fmt_material* fmt_material_parse(const uint8_t* data, size_t size);
void          fmt_material_free(fmt_material* m);
const fmt_material_param* fmt_material_param_find(const fmt_material* m, const char* name);
int           fmt_state_get(const fmt_state_pair* pairs, int n, uint32_t key, uint32_t* value);   /* 1 if present */

#ifdef __cplusplus
}
#endif
#endif
