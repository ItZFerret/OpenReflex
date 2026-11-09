/* rx_d3d.h -- the core's minimal Direct3D 9 resource layer (core/notes/construction.md, "D3D resource layer").
 *
 * Gameplay code (terrain deformation, tyre tracks, the height map copy, the engine's GPU resource registry) creates, locks and
 * copies D3D9 resources whose CPU-visible contents the race reads (the terrain's deformation surface at terrain + 0x240 is locked
 * by the height queries every frame). The core has no renderer (Unity draws), so this layer gives those objects game addresses,
 * COM vtables and memory: nothing is drawn, every resource is backed by arena memory, contents are what the original's calls
 * produce (Clear of an R32F target with 0x80808080 gives 128/255 = 0x3f008081 in every texel, GetRenderTargetData copies, LockRect
 * returns the level's bits and pitch). It mirrors the semantics of the boot's recording device (native/drafts/world_runtime/
 * boot_devices.inc), which the original world images were made with.
 *
 * Objects: arena blocks (rx_heap) of 0x40 bytes, +0 the vtable, the layer's state after it (layout below; game code only uses
 * the vtable). Vtables live at fixed addresses in a reserved range after the executable image (RX_D3D_VT_*), their entries are
 * method ids RX_D3D_FN(iface, slot) registered in the function registry (rx_fn.h) with ONE signature for every method:
 *     uint32_t method(const uint32_t* a)      a[0] = this, a[1..] = the stack arguments; returns the HRESULT / count (EAX)
 * so a virtual call is rx_d3d_call(obj, slot, n, args) (n = arguments including this). In the oracle build (RX_ORACLE) the objects
 * are the emulator's device stand-ins: rx_d3d_call finds no core method and calls the original entry (stdcall), so transliterated
 * game code runs unchanged there.
 *
 * Slots (d3d9.h order): IUnknown 0 QueryInterface, 1 AddRef, 2 Release; resources 3 GetDevice .. 10 GetType; textures 11 SetLOD ..
 * 16 GenerateMipSubLevels, 17 GetLevelDesc, 18 GetSurfaceLevel, 19 LockRect, 20 UnlockRect, 21 AddDirtyRect; surfaces 11
 * GetContainer, 12 GetDesc, 13 LockRect, 14 UnlockRect; buffers 11 Lock, 12 Unlock, 13 GetDesc; device (119 slots) 23 CreateTexture,
 * 26 CreateVertexBuffer, 27 CreateIndexBuffer, 28 CreateRenderTarget, 29 CreateDepthStencilSurface, 32 GetRenderTargetData,
 * 25 CreateCubeTexture, 36 CreateOffscreenPlainSurface, 37 SetRenderTarget, 38 GetRenderTarget, 39 SetDepthStencilSurface, 40 GetDepthStencilSurface,
 * 43 Clear, 86 CreateVertexDeclaration, 91 CreateVertexShader, 106 CreatePixelShader; vertex declarations 3 GetDevice, 4 GetDeclaration;
 * cube textures 11..16 as textures, 17 GetLevelDesc(level), 18 GetCubeMapSurface(face, level), 19 LockRect(face, level), 20 UnlockRect(face, level),
 * 21 AddDirtyRect(face); vertex / pixel shaders 3 GetDevice, 4 GetFunction; ID3DXConstantTable (D3DXGetShaderConstantTable's result) 3 GetBufferPointer, 4 GetBufferSize,
 * 5 GetDesc, 6 GetConstantDesc, 7 GetSamplerIndex, 8 GetConstant, 9 GetConstantByName, 10 GetConstantElement, 11 SetDefaults, 12.. Set*; other device Set* / Begin / End / Draw* are accepted and ignored (S_OK), other Get* fail loudly (not needed yet).
 *
 * Object state (offsets into the 0x40 block): +4 references, +8 kind (RX_D3D_*), +0xc width, +0x10 height, +0x14 levels, +0x18
 * format, +0x1c usage, +0x20 pool, +0x24 the level table (levels x 0x18: bits, pitch, bytes, width, height, surface object), +0x28
 * container (a texture's level surface), +0x2c level, +0x30 size (buffers), +0x34 FVF / index format, +0x38 bits (buffers).
 * Cube texture: as a texture, +0x30 the number of level entries (6 * levels, face after face), +0xc width = height = the edge.
 * Vertex declaration: +0x30 element count (with the end element), +0x38 the elements (8 bytes each).
 * Vertex / pixel shader: +0x30 the token stream's byte length, +0x38 a copy of the tokens (CreateVertexShader / CreatePixelShader read the
 * stream up to its end token, skipping comment blocks and the operands of SM2+ instructions: boot_devices.inc shader_bytes).
 * Constant table: +0x30 the CTAB blob's byte length, +0x38 the blob (the comment block's words after the 'CTAB' tag).
 * Device: +0x10 render targets [4], +0x20 depth-stencil surface, +0x24 back buffer, +0x28 depth buffer. */
#pragma once
#include "rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif

#define RX_D3D_VT_BASE   0x00e40000u              /* reserved range 0x00e40000..0x00e47fff (after .reloc, image end 0x00e3b000) */
#define RX_D3D_VT_DEVICE (RX_D3D_VT_BASE + 0x0000)
#define RX_D3D_VT_TEXTURE (RX_D3D_VT_BASE + 0x0400)
#define RX_D3D_VT_SURFACE (RX_D3D_VT_BASE + 0x0500)
#define RX_D3D_VT_VB     (RX_D3D_VT_BASE + 0x0600)
#define RX_D3D_VT_IB     (RX_D3D_VT_BASE + 0x0700)
#define RX_D3D_VT_DECL   (RX_D3D_VT_BASE + 0x0800)
#define RX_D3D_VT_VS     (RX_D3D_VT_BASE + 0x0900)
#define RX_D3D_VT_PS     (RX_D3D_VT_BASE + 0x0a00)
#define RX_D3D_VT_CTAB   (RX_D3D_VT_BASE + 0x0b00)
#define RX_D3D_VT_CUBE   (RX_D3D_VT_BASE + 0x0c00)
#define RX_D3D_FN(iface, slot) (RX_D3D_VT_BASE + 0x1000u + 0x400u * (uint32_t)(iface) + 4u * (uint32_t)(slot))   /* method ids */

enum { RX_D3D_DEVICE = 1, RX_D3D_TEXTURE = 2, RX_D3D_SURFACE = 3, RX_D3D_VB = 4, RX_D3D_IB = 5, RX_D3D_DECL = 6, RX_D3D_VS = 7, RX_D3D_PS = 8,
       RX_D3D_CTAB = 9, RX_D3D_CUBE = 10 };
enum { RX_D3DFMT_A8R8G8B8 = 21, RX_D3DFMT_X8R8G8B8 = 22, RX_D3DFMT_R32F = 114, RX_D3DPOOL_DEFAULT = 0, RX_D3DPOOL_MANAGED = 1,
       RX_D3DPOOL_SYSTEMMEM = 2 };

/* product: the vtables and method ids (once); the device object */
void     rx_d3d_init(void);
rx_addr  rx_d3d_device_create(void);
/* a virtual call of a D3D object: slot of its vtable, n 32-bit arguments (a[0] = this); EAX */
uint32_t rx_d3d_call(rx_addr obj, uint32_t slot, int n, const uint32_t* a);

/* convenience forms of the device / resource calls (each one rx_d3d_call) */
uint32_t rx_d3d_create_texture(rx_addr dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool,
                               rx_addr out);                                    /* slot 23 */
uint32_t rx_d3d_create_offscreen(rx_addr dev, uint32_t w, uint32_t h, uint32_t format, uint32_t pool, rx_addr out);   /* slot 36 */
uint32_t rx_d3d_create_vb(rx_addr dev, uint32_t length, uint32_t usage, uint32_t fvf, uint32_t pool, rx_addr out);     /* slot 26 */
uint32_t rx_d3d_create_ib(rx_addr dev, uint32_t length, uint32_t usage, uint32_t format, uint32_t pool, rx_addr out);  /* slot 27 */
/* D3DX9 stand-in: D3DXGetShaderConstantTable(pFunction, &table) (the import the shader assets call): the CTAB comment of the token stream
 * as a constant table object (refcount 1; 0 and D3DERR_NOTAVAILABLE when the stream has no CTAB). Oracle build: the original's import. */
uint32_t rx_d3dx_get_shader_constant_table(rx_addr function, rx_addr out);
/* host-side view of a locked level for tools / other layers (product only): bits address and pitch of a texture level or a
 * surface (level ignored); 0 when not a core resource */
rx_addr  rx_d3d_bits(rx_addr obj, uint32_t level, uint32_t* pitch);
/* GetRenderTargetData dirty rectangle (rx_d3d.c "with a dirty rectangle"): the deformation pass announces the texels it wrote for the system
 * memory surface its targets are read back into; the host export takes the rectangle of that surface that changed since its last look
 * (1 + rect[4] = x0, z0, x1, z1, empty when x1 <= x0; 0 = unknown: look at everything). */
void     rx_d3d_track_note(rx_addr dst_obj, int32_t x0, int32_t z0, int32_t x1, int32_t z1);
int      rx_d3d_track_take(rx_addr bits, int32_t* rect);
void     rx_d3d_track_stats(uint64_t* part, uint64_t* full, uint64_t* bytes_part, uint32_t* epoch);   /* tools / tests */

#ifdef __cplusplus
}
#endif
