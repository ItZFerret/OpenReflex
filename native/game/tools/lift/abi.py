# live-in registers of a function (the inputs of its calling convention) and its stack cleanup
from lift import Lifter, ENTS
from capstone import x86 as X
FULL = {}
for r in ["eax","ecx","edx","ebx","esi","edi","ebp"]: FULL[r] = r
for a, b in [("ax","eax"),("al","eax"),("ah","eax"),("cx","ecx"),("cl","ecx"),("ch","ecx"),("dx","edx"),("dl","edx"),("dh","edx"),
             ("bx","ebx"),("bl","ebx"),("bh","ebx"),("si","esi"),("di","edi"),("bp","ebp")]: FULL[a] = b
for k in range(8): FULL["xmm%d" % k] = "xmm%d" % k
VOL = {"eax", "ecx", "edx"} | {"xmm%d" % k for k in range(8)}
def ud(L, i):
    m = i.mnemonic
    try: rd, wr = i.regs_access()
    except Exception: rd, wr = [], []
    use = {FULL[i.reg_name(r)] for r in rd if i.reg_name(r) in FULL}
    dfn = {FULL[i.reg_name(r)] for r in wr if i.reg_name(r) in FULL}
    ops = i.operands
    if m in ("xorps", "pxor", "xor", "sub") and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG and ops[0].reg == ops[1].reg:
        use = set()
    if m == "push" and ops[0].type == X.X86_OP_REG and i.reg_name(ops[0].reg) in ("ebx", "esi", "edi", "ebp"): use = set()
    if m in ("movss", "movsd") and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG: use.add(FULL[i.reg_name(ops[0].reg)])
    if m in ("movss", "movsd") and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_MEM: use.discard(FULL[i.reg_name(ops[0].reg)])
    if m == "call": use = set(); dfn = set(VOL)
    if m == "ret": use = set()
    return use, dfn
def abi(entry):
    L = Lifter(entry); ins = L.ins
    succ = {}
    for a, i in ins.items():
        s = []
        if i.mnemonic not in ("jmp", "ret"): s.append(a + i.size)
        if i.mnemonic.startswith("j") and i.operands[0].type == X.X86_OP_IMM: s.append(i.operands[0].imm)
        if a in L.tables: s += L.tables[a][1]
        succ[a] = [t for t in s if t in ins]
    U = {a: ud(L, i) for a, i in ins.items()}
    live = {a: set() for a in ins}
    changed = True
    while changed:
        changed = False
        for a in sorted(ins, reverse=True):
            out = set()
            for t in succ[a]: out |= live[t]
            u, d = U[a]
            new = u | (out - d)
            if new != live[a]: live[a] = new; changed = True
    rets = sorted({i.operands[0].imm if i.operands else 0 for i in ins.values() if i.mnemonic == "ret"})
    order = ["ecx", "eax", "edx", "ebx", "esi", "edi", "ebp"] + ["xmm%d" % k for k in range(8)]
    return [r for r in order if r in live[entry]], rets
if __name__ == "__main__":
    for l in open("../../bike_list.tsv"):
        a = int(l.split("\t")[0], 16); r, rets = abi(a)
        print("%08x %-38s in: %-30s ret %s" % (a, ENTS.get(a), " ".join(r), rets))
