// RavenPhysics mechanism (mechanism group): the per-node passes of the articulated-body step (00431d60 calls them in this order:
// 0085c6d0 node slot 2, 0085cb70 preparation, 0085ca40 forces, 0085c700 node slot 3, 0085cf80 solve, 0085d270 velocities,
// ..., 0085d100 poses, 0085d300 node slot 6).
// Mechanism M (= simulation + 0xd0): +00 gravity vec4, +1c node count (signed), per node i (stride a0) linear body at +20 and
// angular body at +660: pose +00, velocity +10, mass / inertia +20, inverse +30, resolved force +40, applied force +50,
// impulse +60, acceleration +70 (offsets within each body); node objects (constraints) at +ca4 + 4 * (i - 1) for i >= 1.
#include "mechanism_kernels.h"

using namespace mech;
using namespace x86;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

namespace {
constexpr uint32_t kNode = 0xa0;
constexpr uint32_t kGlobalZero = 0x00d61670;   // runtime vec4 the step uses to clear accumulators (read every time)

// 0085c6d0 / 0085c700 / 0085d300 / 0085d330: for every non-root node, its virtual slot `slot` (count re-read each iteration).
uint32_t node_slot_loop(reflex_vm::Machine& m, int slot) {
    Guest g{m}; uint32_t M = m.arg(0);
    for (int32_t i = 1; i < g.i32(M + 0x1c); i++) {
        uint32_t node = g.u32(M + 0xca4 + 4 * (i - 1));
        vcall(g, node, slot);
    }
    return m.ret(4);
}
}

REFLEX_PORT_FN(0x0085c6d0, Mechanism_NodesSlot2, "mechanism", 0) { return node_slot_loop(m, 2); }
REFLEX_PORT_FN(0x0085c700, Mechanism_NodesSlot3, "mechanism", 0) { return node_slot_loop(m, 3); }
REFLEX_PORT_FN(0x0085d300, Mechanism_NodesSlot6, "mechanism", 0) { return node_slot_loop(m, 6); }
REFLEX_PORT_FN(0x0085d330, Mechanism_NodesSlot7, "mechanism", 0) { return node_slot_loop(m, 7); }

// 0085c9d0 (EDX = M): clear every node's linear +50/+60/+40... accumulators: linear +70, +60, +50 and angular +6b0, +6a0, +690
// (offsets from M), each from the runtime vec4 at 00d61670. ECX = count / EAX = last pointer as the loop leaves them.
REFLEX_PORT_FN(0x0085c9d0, Mechanism_ClearForces, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; uint32_t M = m.r[2], i = 0, p = m.r[0];
    if (g.i32(M + 0x1c) > 0) {
        p = M + 0x70;
        do {
            st(g, p + 0x10, ld(g, kGlobalZero));
            st(g, p, ld(g, kGlobalZero));
            st(g, p + 0x650, ld(g, kGlobalZero));
            st(g, p + 0x640, ld(g, kGlobalZero));
            st(g, p - 0x10, ld(g, kGlobalZero));
            st(g, p + 0x630, ld(g, kGlobalZero));
            i++; p += kNode;
        } while (int32_t(i) < g.i32(M + 0x1c));
    }
    m.r[1] = i; m.r[0] = p; return m.ret(0);
}

// 0085c500 (EDX = M, ESI = vec4): every node's linear pose += vec (a translation of the whole mechanism).
REFLEX_PORT_FN(0x0085c500, Mechanism_Translate, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; uint32_t M = m.r[2], v = m.r[6], i = 0, p = m.r[0];
    if (g.i32(M + 0x1c) > 0) {
        p = M + 0x20;
        do { V4 a = ld(g, p), b = ld(g, v); st(g, p, add(a, b)); i++; p += kNode; } while (int32_t(i) < g.i32(M + 0x1c));
    }
    m.r[1] = i; m.r[0] = p; return m.ret(0);
}

// 0085c4b0 (EDX = M, ESI = vec4): every node's linear velocity = vec; linear +90, angular +670 / +6d0 cleared (00d61670).
REFLEX_PORT_FN(0x0085c4b0, Mechanism_SetVelocity, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; uint32_t M = m.r[2], v = m.r[6], i = 0, p = m.r[0];
    if (g.i32(M + 0x1c) > 0) {
        p = M + 0x90;
        do {
            st(g, p - 0x60, ld(g, v));
            st(g, p, ld(g, kGlobalZero));
            st(g, p + 0x5e0, ld(g, kGlobalZero));
            st(g, p + 0x640, ld(g, kGlobalZero));
            i++; p += kNode;
        } while (int32_t(i) < g.i32(M + 0x1c));
    }
    m.r[1] = i; m.r[0] = p; return m.ret(0);
}

// 0085c730 (ECX = M, EDX = vec4, arg = node index): node's linear velocity = vec. EAX = index * a0.
REFLEX_PORT_FN(0x0085c730, Mechanism_SetNodeLinearVelocity, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t off = m.arg(0) * kNode;
    st(g, off + m.r[1] + 0x30, ld(g, m.r[2]));
    m.r[0] = off; return m.ret(4);
}

// 0085c750 (ECX = M, EDX = vec4, arg = node index): node's angular velocity = vec. EAX = index * a0.
REFLEX_PORT_FN(0x0085c750, Mechanism_SetNodeAngularVelocity, "mechanism", OUT_EAX) {
    Guest g{m}; uint32_t off = m.arg(0) * kNode;
    st(g, off + m.r[1] + 0x670, ld(g, m.r[2]));
    m.r[0] = off; return m.ret(4);
}

// 0085ca40 (EDI = M, arg = dt): force composition. Per node: linear resolved = mass * gravity + applied; angular resolved =
// applied - omega x (I omega) (through the exe's skew matrix 00857ff0 and 3x3 product 00854490 on stack locals); then both
// resolved += impulse * (1 / dt).
REFLEX_PORT_FN(0x0085ca40, Mechanism_ComposeForces, "mechanism", 0) {
    Guest g{m}; uint32_t M = m.r[7]; float dt = f32_arg(m, 0);
    float inv = div(g.f32(0x00931f24), dt);
    V4 inv4 = V4::splat(inv);
    {
    Frame f(m, 0x60);
    uint32_t T = f.at(0x00), Iw = f.at(0x40), W = f.at(0x50);   // Matrix3x3 local, I*omega, omega x (I*omega)
    g.u32(T + 4, 3); g.u32(T + 8, 3); g.u32(T, 0x0093ca04);
    for (int32_t i = 0; i < g.i32(M + 0x1c); i++) {
        uint32_t p = M + 0x70 + i * kNode;
        V4 x1 = ld(g, M), x0 = ld(g, p - 0x30);
        x0 = mul(x0, x1);
        x1 = ld(g, p);
        st(g, p - 0x10, add(x0, x1));
        st(g, p + 0x630, ld(g, p + 0x640));
        x0 = ld(g, p + 0x610); x1 = ld(g, p + 0x600);
        st(g, Iw, mul(x0, x1));
        Matrix3x3_Skew(g, p + 0x600, T);
        Mat3_MulVec(g, W, Iw, T);
        x0 = ld(g, p + 0x630); x1 = ld(g, W);
        st(g, p + 0x630, sub(x0, x1));
    }
    }
    for (int32_t i = 0; i < g.i32(M + 0x1c); i++) {
        uint32_t p = M + 0x60 + i * kNode;
        V4 x0 = ld(g, p + 0x20), x1 = ld(g, p);
        st(g, p, add(mul(x0, inv4), x1));
        x0 = ld(g, p + 0x660); x1 = ld(g, p + 0x640);
        st(g, p + 0x640, add(mul(x0, inv4), x1));
    }
    return m.ret(4);
}

// 0085d270 Mechanism_IntegrateVelocities (EDX = M, arg = dt): per node linear accel = resolved * inverse mass, angular accel =
// resolved * inverse inertia, velocity += accel * dt (angular: dt * accel). ECX = count, EAX = last pointer.
REFLEX_PORT_FN(0x0085d270, Mechanism_IntegrateVelocities, "mechanism", OUT_EAX | OUT_ECX) {
    Guest g{m}; uint32_t M = m.r[2], i = 0, p = m.r[0];
    V4 dt = V4::splat(f32_arg(m, 0));
    if (g.i32(M + 0x1c) > 0) {
        p = M + 0x50;
        do {
            V4 x2 = ld(g, p), x1 = ld(g, p + 0x10);
            st(g, p + 0x40, mul(x1, x2));
            x2 = ld(g, p + 0x640); x1 = ld(g, p + 0x650);
            st(g, p + 0x680, mul(x1, x2));
            x1 = ld(g, p + 0x40); x2 = ld(g, p - 0x20);
            st(g, p - 0x20, add(mul(x1, dt), x2));
            x2 = ld(g, p + 0x680);
            x1 = mul(dt, x2);
            x2 = ld(g, p + 0x620);
            st(g, p + 0x620, add(x1, x2));
            i++; p += kNode;
        } while (int32_t(i) < g.i32(M + 0x1c));
    }
    m.r[1] = i; m.r[0] = p; return m.ret(4);
}

// 0085d100 Mechanism_IntegratePoses (ECX = M, XMM6 lane 0 = dt): positions += velocity * dt; quaternions += (q * omega) * dt/2
// with omega.w from the runtime vec4 00d61660 (lane 3), the Hamilton product in the exe's order with its sign vectors
// (0093a1f0 / 0093a200 / 0093a210); then every quaternion normalized (packed sqrt / div, zero norm -> 0, infinite norm^2 -> the
// NaN vec4 at 0092dc30). EDX = count / EAX = last pointer of the second loop.
REFLEX_PORT_FN(0x0085d100, Mechanism_IntegratePoses, "mechanism", OUT_EAX | OUT_EDX) {
    Guest g{m}; uint32_t M = m.r[1];
    float dt = g.xmm(6).f[0];
    V4 dt4 = V4::splat(dt), half = V4::splat(mul(dt, g.f32(0x009322cc)));
    uint32_t i = 0, p = m.r[0];
    if (g.i32(M + 0x1c) > 0) {
        p = M + 0x20;
        do {
            V4 v = ld(g, p + 0x10), x = ld(g, p);
            st(g, p, add(mul(v, dt4), x));
            V4 G = ld(g, 0x00d61660), w = ld(g, p + 0x650);
            V4 omega = {{w.f[0], w.f[1], w.f[2], G.f[3]}};
            V4 q = ld(g, p + 0x640);
            V4 rev = shuf(omega, omega, 0x1b);                       // (w, z, y, x)
            V4 tx = mul(bcast(q, 0), rev);
            V4 r2 = shuf(rev, rev, 0xb1);                            // (z, w, x, y)
            V4 r3 = shuf(r2, r2, 0x1b);                              // (y, x, w, z)
            V4 ty = mul(bcast(q, 1), r2);
            V4 tz = mul(r3, bcast(q, 2));
            tz = mul(tz, ld(g, 0x0093a210));
            ty = mul(ty, ld(g, 0x0093a200));
            tz = add(tz, ty);
            V4 s1 = ld(g, 0x0093a1f0);
            V4 tw = mul(bcast(q, 3), omega);
            tx = mul(tx, s1);
            tw = add(tw, tx);
            tz = add(tz, tw);
            tz = mul(tz, half);
            st(g, p + 0x640, add(tz, q));
            i++; p += kNode;
        } while (int32_t(i) < g.i32(M + 0x1c));
    }
    i = 0;
    if (g.i32(M + 0x1c) > 0) {
        p = M + 0x660;
        do {
            V4 q = ld(g, p);
            V4 s = mul(q, q);
            float n2 = add(add(s.f[3], s.f[1]), add(s.f[2], s.f[0]));
            V4 inf = ld(g, 0x0092dc20);
            float n = x86::sqrt(n2);
            V4 nan = ld(g, 0x0092dc30), out;
            for (int k = 0; k < 4; k++) {
                bool finite = cmp_pred(n2, inf.f[k], 4), nonzero = cmp_pred(flt(0), n, 4);
                uint32_t d = bits(div(q.f[k], n));
                out.f[k] = flt(((nonzero ? d : 0u) & (finite ? ~0u : 0u)) | (finite ? 0u : bits(nan.f[k])));
            }
            st(g, p, out);
            i++; p += kNode;
        } while (int32_t(i) < g.i32(M + 0x1c));
    }
    m.r[2] = i; m.r[0] = p; return m.ret(0);
}
