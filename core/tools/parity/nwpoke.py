#!/usr/bin/env python3
# nwpoke.py <oracle pair> <product slot address> <offset> <first product frame> <last product frame> [oracle word address] -- w-parity4w:
# PROBE_POKE entries "f:<slot>:<word>" that give the product, at the start of each frame, the dead-stack word the original's 0081c590
# (the disc against a cylinder, CylinderVsCylinder) reads as its first Newton step (its frame slot S + 0x60, never written before the
# read) at the first such call of a frame. In the original the last writer before that call is the previous frame's rendering
# (Model_RenderInstance 007f5680, main thread, [ESP + 0x2e0] at 0022f110 on the paired NAT_Bedford ride): code the product does not run,
# so the word is the render's leftover, not physics (a wheel-to-wheel contact found or missed on its value: NAT_Bedford k 973).
# The oracle pair needs PAIR_WORDS=<oracle word address> (default 22f110); <offset> = the oracle's frame number minus the product's
# (345 on NAT_Bedford / PX_Laguna); the product's slot address comes from the scratch Newton logger (rx_PairNewtonLog's product twin).
import sys
pair, slot, off, f0, f1 = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
want = sys.argv[6] if len(sys.argv) > 6 else '22f110'
d = {}
for l in open(pair):
    if l.startswith('W '):
        t = l.split(); d[int(t[1])] = t[2]
print(','.join('%d:%s:%s' % (f, slot, d[f + off]) for f in range(f0, f1 + 1) if f + off in d))
