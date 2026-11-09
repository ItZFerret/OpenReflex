# patch_displog.py <core/game dir> <out src dir> -- w-parity (development only): patch_pairlog.py plus a copy of phys_contact.c whose
# dispatcher kernel (Collision_ContactDispatch 0085ddd0 and its observed forms) calls parity_dispatch_hook at its entry and exit.
import sys, os
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'patch_pairlog.py')).read())
f = 'phys_contact.c'; s = open(os.path.join(g, f)).read()
old = "static void contact_dispatch_kernel(rx_addr M,rx_addr A,rx_addr QA,rx_addr PA,rx_addr B,rx_addr QB,rx_addr PB,uint32_t flag,int observe,rx_v4*live){\n"
assert s.count(old) == 1
s = s.replace(old, "static void contact_dispatch_kernel_impl(rx_addr M,rx_addr A,rx_addr QA,rx_addr PA,rx_addr B,rx_addr QB,rx_addr PB,uint32_t flag,int observe,rx_v4*live);\n"
 "void parity_dispatch_hook(int phase, rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap, unsigned long* tok, uint32_t* before) __attribute__((weak));\n"
 + old + "  unsigned long tok = 0; uint32_t before = 0;\n  if (parity_dispatch_hook) parity_dispatch_hook(0, M, A, QA, PA, B, QB, PB, flag, &tok, &before);\n"
 "  contact_dispatch_kernel_impl(M,A,QA,PA,B,QB,PB,flag,observe,live);\n  if (parity_dispatch_hook) parity_dispatch_hook(1, M, A, QA, PA, B, QB, PB, flag, &tok, &before);\n}\n"
 + old.replace("contact_dispatch_kernel(", "contact_dispatch_kernel_impl("))
open(os.path.join(o, f), 'w').write('#line 1 "%s"\n' % f + s)
