# patch_pairlog.py <core/game dir> <out src dir> -- w-parity (development only, core/tools/parity/scratch_build.sh): a copy of
# phys_coll_pair.c whose CollisionPair_CompositeVsComposite 008732d0 calls parity_pair_hook (parity_hook.c) at its entry and exit.
import sys, os
g, o = sys.argv[1], sys.argv[2]
f = 'phys_coll_pair.c'; s = open(os.path.join(g, f)).read()
old = "uint32_t phys_pair_composite_vs_composite(rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap) {\n"
assert s.count(old) == 1
s = s.replace(old, "static uint32_t composite_impl(rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap);\n"
 "void parity_pair_hook(int phase, rx_addr esp, rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap) __attribute__((weak));\n"
 "uint32_t phys_pair_composite_vs_composite(rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap) {\n"
 "  if (parity_pair_hook) parity_pair_hook(0, rx_sp(), M, A, qA, pA, B, qB, pB, swap);\n"
 "  uint32_t r = composite_impl(M, A, qA, pA, B, qB, pB, swap);\n"
 "  if (parity_pair_hook) parity_pair_hook(1, rx_sp(), M, A, qA, pA, B, qB, pB, swap);\n  return r;\n}\n"
 "static uint32_t composite_impl(rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap) {\n")
open(os.path.join(o, f), 'w').write('#line 1 "%s"\n' % f + s)
