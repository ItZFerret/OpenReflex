// Rider animation pose capture in the 0x0042xxxx range (RiderAnimation::vf19's helper 00426a50).
#include "math_util.h"

using reflex_game::Guest;
using namespace x86;

namespace {
// Original call with the guest's ESP lowered to `esp` (the original's ESP at the call, below the frame's locals) and EBP set;
// ECX / EAX as given (< 0: unchanged).
uint32_t call_at(reflex_vm::Machine& m, uint32_t esp, uint32_t ebp, uint32_t address, std::initializer_list<uint32_t> args,
                 int64_t ecx = -1, int64_t eax = -1) {
    uint32_t saved_esp = m.r[4], saved_ebp = m.r[5];
    m.r[4] = esp; m.r[5] = ebp;
    uint32_t r = Guest{m}.call(address, args, ecx, eax);
    m.r[4] = saved_esp; m.r[5] = saved_ebp;
    return r;
}
}  // namespace

// 00426a50 (EDI = the rider animation object, RiderAnimation::vf19's only call): evaluates the skeleton's world nodes and stores
// per bone the world quaternion (Matrix3_ToQuaternion via 00401740) and, for bones 1..count-1, a reference point (the global
// vec4 at 0xd61670 minus a per-bone table entry) transformed by the bone's world matrix.
//   bone 0: +0x400 = world position (x, y, z, z), +0xe80 = quaternion
//   bone i: +0x480 + 0x80 (i - 1) = transformed point (w lane: bone 0's z, the original's stale stack lane), +0xef0 + 0x70 (i - 1)
//           = quaternion; node index from +0xa540 + 4 i, table chain +0x380 + 0x1974 + 4 (i - 1) -> +0x3d6c -> +0x3d64 -> +0x1ee0.
// The world matrix is copied to the original's stack slots (00401740 reads it there, its result lands at the frame's +0x80).
REFLEX_PORT_FN(0x00426a50, Fn_00426a50, "math", 0) {
    Guest g{m};
    const uint32_t entry_esp = m.r[4], ebx0 = m.r[3], esi0 = m.r[6], edi = m.r[7];
    const uint32_t ebp1 = entry_esp - 4;
    const uint32_t B = (ebp1 & ~15u) - 0xa8 - 8;            // ESP after push ebx / push esi
    const uint32_t mat = B + 0x40, quat = B + 0x80;
    auto node_of = [&](uint32_t index) { return (index << 4) + g.u32(g.u32(g.u32(edi + 0x37c) + 0x240) + 8); };
    // Node_EvaluateWorld, the world matrix to the stack, Matrix3_ToQuaternion into the stack (ESI = destination)
    auto bone_pose = [&](uint32_t node, uint32_t ebx) {
        m.r[3] = ebx; m.r[6] = node;
        call_at(m, B, ebp1, 0x007db890, {0}, node, g.u32(edi + 0x37c));
        uint32_t model = g.u32(node), world = (uint32_t(g.u8(node + 0xb)) << 6) + g.u32(model + 0x1c);
        for (uint32_t k = 0; k < 64; k += 4) g.u32(mat + k, g.u32(world + k));
        m.r[2] = model; m.r[6] = quat;
        call_at(m, B, ebp1, 0x00401740, {mat}, -1, mat);
    };
    auto f = [&](uint32_t row, uint32_t col) { return g.f32(mat + 16 * row + 4 * col); };
    auto copy16 = [&](uint32_t dst, uint32_t src) { for (uint32_t k = 0; k < 16; k += 4) g.u32(dst + k, g.u32(src + k)); };

    bone_pose(node_of(g.u32(edi + 0xa540)), ebx0);
    const uint32_t w_lane = g.u32(mat + 0x38);               // row 3 z
    g.u32(edi + 0x400, g.u32(mat + 0x30)); g.u32(edi + 0x404, g.u32(mat + 0x34)); g.u32(edi + 0x408, w_lane); g.u32(edi + 0x40c, w_lane);
    copy16(edi + 0xe80, quat);

    uint32_t eax = 1, ecx = m.r[1], edx = m.r[2];
    if (g.i32(edi + 0x3fc) > 1) {
        uint32_t counter = 0x1974, dst = edi + 0x480, dst2 = edi + 0xef0, ebx = edi + 0xa544;
        do {
            bone_pose(node_of(g.u32(ebx)), ebx);
            uint32_t e = g.u32(counter + edi + 0x380) * 0x230;
            e = g.u32(e + edi + 0x3d6c) * 0x230;
            e = g.u32(e + edi + 0x3d64) * 0x170;
            V4 d = sub(g.v4(0xd61670), g.v4(e + edi + 0x1ee0));
            float x = add(add(add(mul(d.f[1], f(1, 0)), mul(f(0, 0), d.f[0])), mul(f(2, 0), d.f[2])), f(3, 0));
            float y = add(add(add(mul(f(1, 1), d.f[1]), mul(f(0, 1), d.f[0])), mul(f(2, 1), d.f[2])), f(3, 1));
            float z = add(add(add(mul(f(1, 2), d.f[1]), mul(f(0, 2), d.f[0])), mul(f(2, 2), d.f[2])), f(3, 2));
            g.f32(dst, x); g.f32(dst + 4, y); g.f32(dst + 8, z); g.u32(dst + 12, w_lane);
            copy16(dst2, quat);
            eax++; ebx += 4; counter += 4; dst += 0x80; dst2 += 0x70;
            ecx = counter; edx = dst;
        } while (int32_t(eax) < g.i32(edi + 0x3fc));
    }
    m.r[0] = eax; m.r[1] = ecx; m.r[2] = edx; m.r[3] = ebx0; m.r[6] = esi0;
    return m.ret(0);
}
