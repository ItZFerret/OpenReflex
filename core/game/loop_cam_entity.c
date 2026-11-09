/* loop_cam_entity.c -- the Camera base (vtable 0095a2d8) and the MXCameraEntity code the camera classes share: the basis builder
 * (an orthonormal right / up / forward from a forward and an up vector), the copies of a Camera, the vector getters, the look-at
 * pose, the entity's per-frame velocity (vf19), its render camera record (vf31) and the copy into the render camera.
 *
 * Layout (game-camera.md): a Camera has +0x10 position, +0x20 right, +0x30 up, +0x40 forward, +0x50 fov, +0x54 aspect, +0x58 /
 * +0x5c near / far, +0x68 perspective, +0x70 a second 0x60-byte block, +0xd0..+0xdc cache-valid flags, +0xe0 / +0x120 / +0x160 the
 * cached matrices, +0x1a0 the render position, +0x1b0 / +0x1c0 / +0x1d0 the render right / up / forward. A camera entity embeds a
 * Camera at +0x90 and keeps its own pose at +0x40 (up) / +0x50 (forward) / +0x60 (position), +0x270 a 4x4 matrix copy, +0x2a0
 * the previous position, +0x2b0 "pose set", +0x2c0 the velocity, +0x2f4 "pose valid".
 *
 * The original copies most floats with x87 `fld m32; fstp m32` (a signalling NaN comes out quiet): cam_x87_copy. */
#include "loop_cam.h"
#include "loop_cam_entity.h"
#include "loop_util.h"

/* Normalize the vector at c + off (x first, the stored components scaled by 1 / sqrt(x*x + y*y + z*z) when that is > 0). The
 * squares are summed as ((x * x) + (y * y)) + (z * z); SqrtF 0042c230 is sqrtss. */
static void cam_normalize3(rx_addr c, uint32_t off) {
    const float x = F32(c, off), y = F32(c, off + 4), z = F32(c, off + 8);
    const float l = ((x * x) + (y * y)) + (z * z);
    if (l > 0.0f /* 0x92e12c */) {
        const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(l);
        SF32(c, off + 0, x * inv);
        SF32(c, off + 4, F32(c, off + 4) * inv);
        SF32(c, off + 8, F32(c, off + 8) * inv);
    }
}

/* The basis builder of 00790ce0 / 00790f40 at rows right `r`, up `u`, forward `f` (offsets of c): forward = (fx, fy, fz)
 * normalized; up = (ux, uy, uz); right = (f.z * u.y - f.y * u.z, f.x * u.z - u.x * f.z, u.x * f.y - f.x * u.y) normalized;
 * up = (r.z * f.y - r.y * f.z, r.x * f.z - f.x * r.z, f.x * r.y - r.x * f.y). */
static void cam_basis(rx_addr c, uint32_t r, uint32_t u, uint32_t f, float fx, float fy, float fz, float ux, float uy, float uz) {
    SF32(c, f + 0, fx);
    SF32(c, f + 4, fy);
    SF32(c, f + 8, fz);
    cam_normalize3(c, f);
    SF32(c, u + 0, ux);
    SF32(c, u + 4, uy);
    SF32(c, u + 8, uz);
    {
        const float f0 = F32(c, f), f1 = F32(c, f + 4), f2 = F32(c, f + 8);
        const float u0 = F32(c, u), u1 = F32(c, u + 4), u2 = uz;
        const float r0 = (f2 * u1) - (f1 * u2);
        const float r2 = (u0 * f1) - (f0 * u1);
        const float r1 = (f0 * u2) - (u0 * f2);
        SF32(c, r + 8, r2);
        SF32(c, r + 0, r0);
        SF32(c, r + 4, r1);
    }
    cam_normalize3(c, r);
    {
        const float r0 = F32(c, r), r1 = F32(c, r + 4), r2 = F32(c, r + 8);
        const float f0 = F32(c, f), f1 = F32(c, f + 4), f2 = F32(c, f + 8);
        SF32(c, u + 0, (r2 * f1) - (r1 * f2));
        SF32(c, u + 4, (r0 * f2) - (f0 * r2));
        SF32(c, u + 8, (f0 * r1) - (r0 * f1));
    }
}

/* 00790ce0 (LTCG: ESI = camera, xmm0 = fwd.x; stack fwd.y, fwd.z, up.x, up.y, up.z; ret 0x14): the camera's basis (right +0x20,
 * up +0x30, forward +0x40) from a forward and an up vector; the cache flags +0xd4 / +0xd0 / +0xdc cleared. */
void rx_cam_set_basis(rx_addr c, float fx, float fy, float fz, float ux, float uy, float uz) {
    cam_basis(c, 0x20, 0x30, 0x40, fx, fy, fz, ux, uy, uz);
    SU32(c, 0xd4, 0);
    SU32(c, 0xd0, 0);
    SU32(c, 0xdc, 0);
}
RX_FUNCTION(0x00790ce0, rx_cam_set_basis)

/* 00790f40 (LTCG: ESI = camera, xmm0 = fwd.x; stack fwd.y, fwd.z, up.x, up.y, up.z; ret 0x14): the render basis (right +0x1b0,
 * up +0x1c0, forward +0x1d0); no flags. */
void rx_cam_set_render_basis(rx_addr c, float fx, float fy, float fz, float ux, float uy, float uz) {
    cam_basis(c, 0x1b0, 0x1c0, 0x1d0, fx, fy, fz, ux, uy, uz);
}
RX_FUNCTION(0x00790f40, rx_cam_set_render_basis)

/* 007911b0 (LTCG: ESI = camera (passed through to 00790f40), ECX = forward vec3, EAX = up vec3): the render basis from two
 * stored vectors; fwd.x goes in xmm0 (movss), the other five through x87 copies onto the stack. */
void rx_cam_set_render_basis_from(rx_addr c, rx_addr up, rx_addr fwd) {
    rx_cam_set_render_basis(c, F32(fwd, 0), cam_x87_f32(fwd + 4), cam_x87_f32(fwd + 8),
                            cam_x87_f32(up), cam_x87_f32(up + 4), cam_x87_f32(up + 8));
}
RX_FUNCTION(0x007911b0, rx_cam_set_render_basis_from)

/* 007911e0 (LTCG: EAX = camera, xmm0 = fov): the fov +0x50; the cache flags +0xd4 / +0xdc / +0xd8 cleared. */
void rx_cam_set_fov(rx_addr c, float fov) {
    SU32(c, 0xd4, 0);
    SU32(c, 0xdc, 0);
    SF32(c, 0x50, fov);
    SU32(c, 0xd8, 0);
}
RX_FUNCTION(0x007911e0, rx_cam_set_fov)

/* 00792330 / 007923a0 / 00792380 (LTCG: ECX = camera, EAX = out vec3): the position +0x10 / right +0x30 / forward +0x40. */
void rx_cam_get_position(rx_addr c, rx_addr out) { cam_x87_copy3(out, c + 0x10); }
RX_FUNCTION(0x00792330, rx_cam_get_position)
void rx_cam_get_right(rx_addr c, rx_addr out) { cam_x87_copy3(out, c + 0x30); }
RX_FUNCTION(0x007923a0, rx_cam_get_right)
void rx_cam_get_forward(rx_addr c, rx_addr out) { cam_x87_copy3(out, c + 0x40); }
RX_FUNCTION(0x00792380, rx_cam_get_forward)

/* 00792350 Camera::vf0 (thiscall; stack out vec3; ret 4): the render position +0x1a0. */
void rx_camera_vf0(rx_addr c, rx_addr out) { cam_x87_copy3(out, c + 0x1a0); }
RX_FUNCTION(0x00792350, rx_camera_vf0)

/* 00790cb0 (LTCG: ECX = camera, EAX = vec3): the render position +0x1a0 (x through x87, y / z movss). */
void rx_cam_set_render_position(rx_addr c, rx_addr v) {
    const uint32_t y = U32(v, 4), z = U32(v, 8);
    cam_x87_copy(c + 0x1a0, v);
    SU32(c, 0x1a4, y);
    SU32(c, 0x1a8, z);
}
RX_FUNCTION(0x00790cb0, rx_cam_set_render_position)

/* 0063ddd0 (LTCG: ECX = source, EAX = destination): 24 floats +0x00..+0x5c (x87 copies). */
void rx_camera_copy_block(rx_addr src, rx_addr dst) {
    for (uint32_t o = 0; o < 0x60; o += 4) cam_x87_copy(dst + o, src + o);
}
RX_FUNCTION(0x0063ddd0, rx_camera_copy_block)

/* 0063dc30 (stdcall out, source; ret 8) -> EAX = out: a Camera copied: the vtable 0095a2d8, position / right / up / forward
 * (xyz only), +0x50..+0x64 (x87), +0x68 (integer), the block +0x70 (0063ddd0), the flags +0xd0..+0xdc, the three cached matrices
 * (rep movsd), the render position / right / up / forward (xyz, x87). */
rx_addr rx_camera_copy(rx_addr out, rx_addr src) {
    static const uint16_t head[] = {0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30, 0x34, 0x38, 0x40, 0x44, 0x48,
                                    0x50, 0x54, 0x58, 0x5c, 0x60, 0x64};
    static const uint16_t tail[] = {0x1a0, 0x1a4, 0x1a8, 0x1b0, 0x1b4, 0x1b8, 0x1c0, 0x1c4, 0x1c8, 0x1d0, 0x1d4, 0x1d8};
    SU32(out, 0, 0x0095a2d8);
    for (unsigned i = 0; i < sizeof head / sizeof head[0]; i++) cam_x87_copy(out + head[i], src + head[i]);
    SU32(out, 0x68, U32(src, 0x68));
    rx_camera_copy_block(src + 0x70, out + 0x70);
    for (uint32_t o = 0xd0; o <= 0xdc; o += 4) SU32(out, o, U32(src, o));
    for (uint32_t o = 0xe0; o < 0x1a0; o += 4) SU32(out, o, U32(src, o));
    for (unsigned i = 0; i < sizeof tail / sizeof tail[0]; i++) cam_x87_copy(out + tail[i], src + tail[i]);
    return out;
}
RX_FUNCTION(0x0063dc30, rx_camera_copy)

/* 00790b30 (LTCG: EDI = source camera, ESI = destination camera): the camera's pose copied into the destination: position,
 * forward, up, right (x87), the render position through the source's vf0 (into destination +0x1a0), the render forward / up /
 * right, +0x50..+0x64 (x87), +0x68; the destination's cache flags +0xd4 / +0xd0 / +0xdc / +0xd8 cleared. */
void rx_camera_assign(rx_addr src, rx_addr dst) {
    static const uint16_t a[] = {0x10, 0x14, 0x18, 0x40, 0x44, 0x48, 0x30, 0x34, 0x38, 0x20, 0x24, 0x28};
    static const uint16_t b[] = {0x1d0, 0x1d4, 0x1d8, 0x1c0, 0x1c4, 0x1c8, 0x1b0, 0x1b4, 0x1b8, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64};
    for (unsigned i = 0; i < sizeof a / sizeof a[0]; i++) cam_x87_copy(dst + a[i], src + a[i]);
    loop_tc1(loop_vslot(src, 0), src, dst + 0x1a0);
    for (unsigned i = 0; i < sizeof b / sizeof b[0]; i++) cam_x87_copy(dst + b[i], src + b[i]);
    SU32(dst, 0x68, U32(src, 0x68));
    SU32(dst, 0xd4, 0);
    SU32(dst, 0xd0, 0);
    SU32(dst, 0xdc, 0);
    SU32(dst, 0xd8, 0);
}
RX_FUNCTION(0x00790b30, rx_camera_assign)

/* 0063dba0 (stdcall entity, view; ret 8): the entity's camera (+0x90) is copied into a stack Camera (0063dc30), its aspect
 * replaced by the view's render camera's (view +0 -> +0x54) and its matrix flags cleared, then assigned to the view's render
 * camera (00790b30). When the view has a status record (+0x68): {byte +0x2fc, -, x87 +0x300, +0x304}. Frame: push ebp; and esp,
 * -16; sub esp, 0x1e4; push ebx / esi / edi: the stack Camera at the aligned base - 0x1e0. */
void rx_cam_entity_to_view(rx_addr e, rx_addr view) {
    const rx_addr sp0 = rx_sp();
    const rx_addr A = (sp0 - 4u) & ~0xfu;
    const rx_addr C = A - 0x1e0u;
    rx_set_sp(A - 0x1f0u);
    rx_camera_copy(C, e + 0x90);
    rx_addr rc = U32(view, 0);
    SF32(C, 0x54, F32(rc, 0x54));
    SU32(C, 0xd4, 0);
    SU32(C, 0xdc, 0);
    SU32(C, 0xd8, 0);
    rx_camera_assign(C, rc);
    rx_addr st = U32(view, 0x68);
    if (st != 0) {
        SU32(st, 0, U8(e, 0x2fc));
        const uint32_t v = U32(e, 0x304);
        SU32(st, 0xc, v);
        cam_x87_copy(st + 8, e + 0x300);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0063dba0, rx_cam_entity_to_view)

/* 0063de70 MXCameraEntity::vf29: "pose valid" +0x2f4 cleared. */
void rx_cam_entity_invalidate(rx_addr e) { SU32(e, 0x2f4, 0); }
RX_FUNCTION(0x0063de70, rx_cam_entity_invalidate)

/* 0063de80 MXCameraEntity::vf31: the embedded Camera's render basis from the entity's forward (+0x50, xmm0 movss; +0x54 / +0x58
 * x87) and up (+0x40 / +0x44 / +0x48), its render position (+0x90 + 0x1a0) from the entity's position +0x60 (x87, movss, movss). */
void rx_cam_entity_record(rx_addr e) {
    rx_cam_set_render_basis(e + 0x90, F32(e, 0x50), cam_x87_f32(e + 0x54), cam_x87_f32(e + 0x58),
                            cam_x87_f32(e + 0x40), cam_x87_f32(e + 0x44), cam_x87_f32(e + 0x48));
    const uint32_t y = U32(e, 0x64), z = U32(e, 0x68);
    cam_x87_copy(e + 0x230, e + 0x60);
    SU32(e, 0x234, y);
    SU32(e, 0x238, z);
}
RX_FUNCTION(0x0063de80, rx_cam_entity_record)

/* 0063dee0 (stdcall entity, pose matrix; ret 8): sets the entity's pose. "pose set" +0x2b0 = 1; the matrix vf14 returns (slot
 * 0x38) is copied to +0x270 (16 dwords); vf16 (slot 0x40) takes the pose; the embedded Camera's basis from the pose's forward
 * (+0x20) and up (+0x10) rows (00790ce0) and its position from the translation row +0x30; its cache flags cleared; vf31 (0x7c). */
void rx_cam_entity_set_pose(rx_addr e, rx_addr p) {
    SU8(e, 0x2b0, 1);
    rx_addr m = loop_tc0(loop_vslot(e, 0x38), e);
    for (uint32_t o = 0; o < 0x40; o += 4) SU32(e, 0x270 + o, U32(m, o));
    loop_tc1(loop_vslot(e, 0x40), e, p);
    const rx_addr c = e + 0x90;
    rx_cam_set_basis(c, F32(p, 0x20), cam_x87_f32(p + 0x24), cam_x87_f32(p + 0x28),
                     cam_x87_f32(p + 0x10), cam_x87_f32(p + 0x14), cam_x87_f32(p + 0x18));
    const uint32_t y = U32(p, 0x34), z = U32(p, 0x38);
    cam_x87_copy(c + 0x10, p + 0x30);
    SU32(c, 0x14, y);
    SU32(c, 0x18, z);
    SU32(c, 0xd4, 0);
    SU32(c, 0xd0, 0);
    SU32(c, 0xdc, 0);
    loop_tc0(loop_vslot(e, 0x7c), e);
}
RX_FUNCTION(0x0063dee0, rx_cam_entity_set_pose)

/* 0063df90 (LTCG: ECX = from vec3, EAX = to vec3; stack entity; ret 4): the entity looks from `from` at `to`. The direction
 * d = to - from is normalized (length^2 = (dz * dz + dx * dx) + dy * dy); right = (0, 1, 0) x d with its zero products kept, up =
 * d x right; the pose matrix {right, 0; up, 0; d, 0; from, 1} is built on the stack (aligned base - 0x40) and set (0063dee0). */
void rx_cam_entity_look_at(rx_addr from, rx_addr to, rx_addr e) {
    const rx_addr sp0 = rx_sp();
    const rx_addr A = (sp0 - 4u) & ~0xfu;
    const rx_addr E = A - 0x70u, M = E + 0x30u;
    rx_set_sp(E);
    const float px = F32(from, 0), py = F32(from, 4), pz = F32(from, 8);
    float x = F32(to, 0) - px, y = F32(to, 4) - py, z = F32(to, 8) - pz;
    const float l = ((z * z) + (x * x)) + (y * y);
    SF32(E, 4, px); SF32(E, 8, py); SF32(E, 0xc, pz);
    SF32(E, 0x10, x); SF32(E, 0x14, y); SF32(E, 0x18, z);
    SF32(E, 0, l);
    if (l > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l);
        y = inv * y;
        x = x * inv;
        z = z * inv;
    }
    const float y0 = y * 0.0f, x0 = x * 0.0f, z0 = z * 0.0f;
    const float r1 = x0 - z0;
    const float r0 = z - y0;
    const float r2 = y0 - x;
    const float u0 = (y * r2) - (z * r1);
    const float u1 = (z * r0) - (r2 * x);
    const float u2 = (r1 * x) - (y * r0);
    SF32(E, 0x28, r2);
    SF32(E, 0x10, u0);
    SF32(E, 0x14, u1);
    SF32(M, 0x00, r0); SF32(M, 0x04, r1); SF32(M, 0x08, r2); SF32(M, 0x0c, 0.0f);
    SF32(M, 0x10, u0); SF32(M, 0x14, u1); SF32(M, 0x18, u2); SF32(M, 0x1c, 0.0f);
    SF32(M, 0x20, x);  SF32(M, 0x24, y);  SF32(M, 0x28, z);  SF32(M, 0x2c, 0.0f);
    SF32(M, 0x30, px); SF32(M, 0x34, py); SF32(M, 0x38, pz); SF32(M, 0x3c, 1.0f);
    rx_set_sp(E - 8u);
    rx_cam_entity_set_pose(e, M);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0063df90, rx_cam_entity_look_at)

/* 0063e310 MXCameraEntity::vf19 (thiscall dt; ret 4): once the pose is set (+0x2b0) and |dt| >= 0x977814, the velocity +0x2c0 =
 * (1 / dt) * (the position of the matrix vf14 returns (+0x30) - the previous position +0x2a0); otherwise the velocity is zero. */
void rx_cam_entity_vf19(rx_addr e, float dt) {
    if (U8(e, 0x2b0) == 0 || 1e-9f /* 0x977814 */ > fabsf(dt)) {
        SF32(e, 0x2c0, 0.0f);
        SF32(e, 0x2c4, 0.0f);
        SF32(e, 0x2c8, 0.0f);
        return;
    }
    const float ox = F32(e, 0x2a0), oy = F32(e, 0x2a4), oz = F32(e, 0x2a8);
    rx_addr m = loop_tc0(loop_vslot(e, 0x38), e);
    const float dx = F32(m, 0x30) - ox, dy = F32(m, 0x34) - oy, dz = F32(m, 0x38) - oz;
    const float inv = 1.0f /* 0x931f24 */ / dt;
    SF32(e, 0x2c0, inv * dx);
    SF32(e, 0x2c4, inv * dy);
    SF32(e, 0x2c8, inv * dz);
}
RX_FUNCTION(0x0063e310, rx_cam_entity_vf19)
