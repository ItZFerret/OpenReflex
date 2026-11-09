// Specialized instruction handlers, chosen once at decode for the most frequent operand
// shapes. Each one has exactly the semantics of the generic path in Machine::execute (flags
// included); anything unusual (watched stores, other sizes) is not specialized.
struct Fast {
    using M = Machine;
    static uint32_t generic(M& m, const Insn& in) { return m.execute(in); }
    // --- integer moves (32-bit) ---
    static uint32_t mov_rr(M& m, const Insn& in) { m.r[in.o[0].ridx] = m.r[in.o[1].ridx]; return in.next; }
    static uint32_t mov_ri(M& m, const Insn& in) { m.r[in.o[0].ridx] = in.o[1].imm; return in.next; }
    static uint32_t mov_rm(M& m, const Insn& in) { m.r[in.o[0].ridx] = uint32_t(m.mem.read(m.ea(in.o[1]), 4)); return in.next; }
    static uint32_t mov_mr(M& m, const Insn& in) {
        if (m.watch_address) return m.execute(in);
        m.mem.write(m.ea(in.o[0]), 4, m.r[in.o[1].ridx]); return in.next; }
    static uint32_t mov_mi(M& m, const Insn& in) {
        if (m.watch_address) return m.execute(in);
        m.mem.write(m.ea(in.o[0]), 4, in.o[1].imm); return in.next; }
    static uint32_t lea(M& m, const Insn& in) { m.r[in.o[0].ridx] = m.ea(in.o[1]); return in.next; }
    static uint32_t movzx_r_m8(M& m, const Insn& in) { m.r[in.o[0].ridx] = uint32_t(m.mem.read(m.ea(in.o[1]), 1)); return in.next; }
    static uint32_t movzx_r_m16(M& m, const Insn& in) { m.r[in.o[0].ridx] = uint32_t(m.mem.read(m.ea(in.o[1]), 2)); return in.next; }
    // --- stack / control ---
    static uint32_t push_r(M& m, const Insn& in) { m.push(m.r[in.o[0].ridx]); return in.next; }
    static uint32_t push_i(M& m, const Insn& in) { m.push(in.o[0].imm); return in.next; }
    static uint32_t pop_r(M& m, const Insn& in) { uint32_t v = m.pop(); m.r[in.o[0].ridx] = v; return in.next; }
    static uint32_t call_rel(M& m, const Insn& in) { m.push(in.next); return in.o[0].imm; }
    static uint32_t ret0(M& m, const Insn&) { return m.pop(); }
    static uint32_t ret_imm(M& m, const Insn& in) { uint32_t v = m.pop(); m.r[4] += in.o[0].imm; return v; }
    static uint32_t jmp_rel(M&, const Insn& in) { return in.o[0].imm; }
    static uint32_t jcc(M& m, const Insn& in) { return m.cond(in.mn) ? in.o[0].imm : in.next; }
    // --- integer ALU (32-bit, flags as set_add / set_logic) ---
    static uint32_t cmp_ri(M& m, const Insn& in) { m.set_add(m.r[in.o[0].ridx], in.o[1].imm, 0, 4, true); return in.next; }
    static uint32_t cmp_rr(M& m, const Insn& in) { m.set_add(m.r[in.o[0].ridx], m.r[in.o[1].ridx], 0, 4, true); return in.next; }
    static uint32_t cmp_rm(M& m, const Insn& in) { m.set_add(m.r[in.o[0].ridx], m.mem.read(m.ea(in.o[1]), 4), 0, 4, true); return in.next; }
    static uint32_t cmp_mi(M& m, const Insn& in) { m.set_add(m.mem.read(m.ea(in.o[0]), 4), in.o[1].imm, 0, 4, true); return in.next; }
    static uint32_t cmp_mr(M& m, const Insn& in) { m.set_add(m.mem.read(m.ea(in.o[0]), 4), m.r[in.o[1].ridx], 0, 4, true); return in.next; }
    static uint32_t add_ri(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d = uint32_t(m.set_add(d, in.o[1].imm, 0, 4, false)); return in.next; }
    static uint32_t add_rr(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d = uint32_t(m.set_add(d, m.r[in.o[1].ridx], 0, 4, false)); return in.next; }
    static uint32_t sub_ri(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d = uint32_t(m.set_add(d, in.o[1].imm, 0, 4, true)); return in.next; }
    static uint32_t sub_rr(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d = uint32_t(m.set_add(d, m.r[in.o[1].ridx], 0, 4, true)); return in.next; }
    static uint32_t inc_r(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; uint32_t cf = m.flags & 1; d = uint32_t(m.set_add(d, 1, 0, 4, false)); m.flags = (m.flags & ~1u) | cf; return in.next; }
    static uint32_t dec_r(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; uint32_t cf = m.flags & 1; d = uint32_t(m.set_add(d, 1, 0, 4, true)); m.flags = (m.flags & ~1u) | cf; return in.next; }
    static uint32_t test_rr(M& m, const Insn& in) { m.set_logic(m.r[in.o[0].ridx] & m.r[in.o[1].ridx], 4); return in.next; }
    static uint32_t test_ri(M& m, const Insn& in) { m.set_logic(m.r[in.o[0].ridx] & in.o[1].imm, 4); return in.next; }
    static uint32_t and_ri(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d &= in.o[1].imm; m.set_logic(d, 4); return in.next; }
    static uint32_t and_rr(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d &= m.r[in.o[1].ridx]; m.set_logic(d, 4); return in.next; }
    static uint32_t or_rr(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d |= m.r[in.o[1].ridx]; m.set_logic(d, 4); return in.next; }
    static uint32_t xor_rr(M& m, const Insn& in) { auto& d = m.r[in.o[0].ridx]; d ^= m.r[in.o[1].ridx]; m.set_logic(d, 4); return in.next; }
    // --- SSE moves ---
    static uint32_t movaps_xx(M& m, const Insn& in) { m.xmm[in.o[0].ridx] = m.xmm[in.o[1].ridx]; return in.next; }
    static uint32_t movaps_xm(M& m, const Insn& in) { uint32_t a = m.ea(in.o[1]); Xmm v; v.lo = m.mem.read(a, 8); v.hi = m.mem.read(a + 8, 8); m.xmm[in.o[0].ridx] = v; return in.next; }
    static uint32_t movaps_mx(M& m, const Insn& in) {
        if (m.watch_address) return m.execute(in);
        uint32_t a = m.ea(in.o[0]); const Xmm& v = m.xmm[in.o[1].ridx]; m.mem.write(a, 8, v.lo); m.mem.write(a + 8, 8, v.hi); return in.next; }
    static uint32_t movss_xm(M& m, const Insn& in) { auto& x = m.xmm[in.o[0].ridx]; x.lo = m.mem.read(m.ea(in.o[1]), 4); x.hi = 0; return in.next; }
    static uint32_t movss_mx(M& m, const Insn& in) {
        if (m.watch_address) return m.execute(in);
        m.mem.write(m.ea(in.o[0]), 4, m.xmm[in.o[1].ridx].lo & 0xffffffffull); return in.next; }
    static uint32_t movss_xx(M& m, const Insn& in) { auto& x = m.xmm[in.o[0].ridx]; x.lo = (x.lo & ~0xffffffffull) | (m.xmm[in.o[1].ridx].lo & 0xffffffffull); return in.next; }
    // --- SSE arithmetic (same exact paths as the generic ARITH cases) ---
    static inline Xmm src128(M& m, const Operand& o) {
        if (o.type == 1) return m.xmm[o.ridx];
        uint32_t a = m.ea(o); Xmm v; v.lo = m.mem.read(a, 8); v.hi = m.mem.read(a + 8, 8); return v;
    }
    template <int OP> static uint32_t arith_ps(M& m, const Insn& in) {
        Xmm& a = m.xmm[in.o[0].ridx]; Xmm b = src128(m, in.o[1]);
        if (!ps_fast(OP, a, b, m.mxcsr)) for (int i = 0; i < 4; i++) set_lane32(a, i, sse32(OP, lane32(a, i), lane32(b, i)));
        return in.next;
    }
    template <int OP> static uint32_t arith_ss(M& m, const Insn& in) {
        Xmm& a = m.xmm[in.o[0].ridx];
        uint32_t b = in.o[1].type == 1 ? uint32_t(m.xmm[in.o[1].ridx].lo) : uint32_t(m.mem.read(m.ea(in.o[1]), 4));
        set_lane32(a, 0, sse32(OP, lane32(a, 0), b));
        return in.next;
    }
    static uint32_t shufps(M& m, const Insn& in) {
        Xmm a = m.xmm[in.o[0].ridx], b = src128(m, in.o[1]), res; unsigned imm = in.o[2].imm;
        set_lane32(res, 0, lane32(a, imm & 3)); set_lane32(res, 1, lane32(a, (imm >> 2) & 3));
        set_lane32(res, 2, lane32(b, (imm >> 4) & 3)); set_lane32(res, 3, lane32(b, (imm >> 6) & 3));
        m.xmm[in.o[0].ridx] = res; return in.next;
    }
    static uint32_t andps(M& m, const Insn& in) { Xmm& a = m.xmm[in.o[0].ridx]; Xmm b = src128(m, in.o[1]); a.lo &= b.lo; a.hi &= b.hi; return in.next; }
    static uint32_t andnps(M& m, const Insn& in) { Xmm& a = m.xmm[in.o[0].ridx]; Xmm b = src128(m, in.o[1]); a.lo = ~a.lo & b.lo; a.hi = ~a.hi & b.hi; return in.next; }
    static uint32_t orps(M& m, const Insn& in) { Xmm& a = m.xmm[in.o[0].ridx]; Xmm b = src128(m, in.o[1]); a.lo |= b.lo; a.hi |= b.hi; return in.next; }
    static uint32_t xorps(M& m, const Insn& in) { Xmm& a = m.xmm[in.o[0].ridx]; Xmm b = src128(m, in.o[1]); a.lo ^= b.lo; a.hi ^= b.hi; return in.next; }
};

// The specialized handler for a decoded instruction, or nullptr.
static FastFn select_fast(const Insn& in) {
    const Operand* o = in.o;
    auto R = [&](int i) { return o[i].type == 1 && o[i].rclass == 1 && o[i].size == 4; };
    auto I = [&](int i) { return o[i].type == 2; };
    auto Mm = [&](int i, int size) { return o[i].type == 3 && o[i].size == size; };
    auto X = [&](int i) { return o[i].type == 1 && o[i].rclass == 5; };
    switch (in.mn) {
    case M_mov:
        if (in.n != 2) break;
        if (R(0) && R(1)) return Fast::mov_rr;
        if (R(0) && I(1)) return Fast::mov_ri;
        if (R(0) && Mm(1, 4)) return Fast::mov_rm;
        if (Mm(0, 4) && R(1)) return Fast::mov_mr;
        if (Mm(0, 4) && I(1)) return Fast::mov_mi;
        break;
    case M_lea: if (R(0) && o[1].type == 3) return Fast::lea; break;
    case M_movzx: if (R(0) && Mm(1, 1)) return Fast::movzx_r_m8; if (R(0) && Mm(1, 2)) return Fast::movzx_r_m16; break;
    case M_push: if (R(0)) return Fast::push_r; if (I(0) && o[0].size == 4) return Fast::push_i; break;
    case M_pop: if (R(0)) return Fast::pop_r; break;
    case M_call: if (I(0)) return Fast::call_rel; break;
    case M_ret: return in.n ? Fast::ret_imm : Fast::ret0;
    case M_jmp: if (I(0)) return Fast::jmp_rel; break;
    case M_jo: case M_jno: case M_jb: case M_jae: case M_je: case M_jne: case M_jbe: case M_ja:
    case M_js: case M_jns: case M_jp: case M_jnp: case M_jl: case M_jge: case M_jle: case M_jg:
        if (I(0)) return Fast::jcc; break;
    case M_cmp:
        if (R(0) && I(1)) return Fast::cmp_ri; if (R(0) && R(1)) return Fast::cmp_rr; if (R(0) && Mm(1, 4)) return Fast::cmp_rm;
        if (Mm(0, 4) && I(1)) return Fast::cmp_mi; if (Mm(0, 4) && R(1)) return Fast::cmp_mr; break;
    case M_add: if (R(0) && I(1)) return Fast::add_ri; if (R(0) && R(1)) return Fast::add_rr; break;
    case M_sub: if (R(0) && I(1)) return Fast::sub_ri; if (R(0) && R(1)) return Fast::sub_rr; break;
    case M_inc: if (R(0)) return Fast::inc_r; break;
    case M_dec: if (R(0)) return Fast::dec_r; break;
    case M_test: if (R(0) && R(1)) return Fast::test_rr; if (R(0) && I(1)) return Fast::test_ri; break;
    case M_and: if (R(0) && I(1)) return Fast::and_ri; if (R(0) && R(1)) return Fast::and_rr; break;
    case M_or: if (R(0) && R(1)) return Fast::or_rr; break;
    case M_xor: if (R(0) && R(1)) return Fast::xor_rr; break;
    case M_movaps: case M_movups: case M_movdqa: case M_movdqu: case M_movapd: case M_movupd:
        if (X(0) && X(1)) return Fast::movaps_xx; if (X(0) && o[1].type == 3) return Fast::movaps_xm; if (o[0].type == 3 && X(1)) return Fast::movaps_mx; break;
    case M_movss:
        if (X(0) && o[1].type == 3) return Fast::movss_xm; if (o[0].type == 3 && X(1)) return Fast::movss_mx; if (X(0) && X(1)) return Fast::movss_xx; break;
    case M_mulps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::arith_ps<OP_MUL>; break;
    case M_addps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::arith_ps<OP_ADD>; break;
    case M_subps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::arith_ps<OP_SUB>; break;
    case M_mulss: if (X(0) && (X(1) || o[1].type == 3)) return Fast::arith_ss<OP_MUL>; break;
    case M_addss: if (X(0) && (X(1) || o[1].type == 3)) return Fast::arith_ss<OP_ADD>; break;
    case M_subss: if (X(0) && (X(1) || o[1].type == 3)) return Fast::arith_ss<OP_SUB>; break;
    case M_shufps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::shufps; break;
    case M_andps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::andps; break;
    case M_andnps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::andnps; break;
    case M_orps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::orps; break;
    case M_xorps: if (X(0) && (X(1) || o[1].type == 3)) return Fast::xorps; break;
    default: break;
    }
    return nullptr;
}
