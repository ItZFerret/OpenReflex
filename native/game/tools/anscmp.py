#!/usr/bin/env python3
"""anscmp.py <dirA> <dirB>: the reflex_client answers of two runs (out.txt + ans/<n>.txt), command by command, with the run folders' own
paths normalised; setup commands (boot_ports, world_port, port_report, canonical) are skipped."""
import os, re, sys
SKIP = ('boot_ports', 'world_port', 'port_report', 'profile canonical', 'profile memhash', 'world memhash')
def load(d):
    out, cmd, n = [], None, 0
    for l in open(os.path.join(d, 'out.txt'), errors='replace'):
        if l.startswith('> '): cmd = l[2:].rstrip('\n')
        elif l.startswith('= '):
            n += 1
            if cmd and not cmd.startswith(SKIP):
                p = os.path.join(d, 'ans', '%d.txt' % n)
                t = open(p, 'rb').read().decode('latin-1') if os.path.exists(p) else l
                out.append((cmd, t.replace(os.path.abspath(d), '<RUN>')))
            cmd = None
        elif l.startswith('! '): out.append((cmd, 'ERROR ' + l[2:].replace(os.path.abspath(d), '<RUN>'))); cmd = None
    return out
A, B = load(sys.argv[1]), load(sys.argv[2]); bad = 0
for i, (a, b) in enumerate(zip(A, B)):
    if a != b: bad += 1; print('differs: %s\n  A %s\n  B %s' % (a[0], a[1][:200], b[1][:200])) if bad <= 5 else None
if len(A) != len(B): bad += 1; print('answer counts %d / %d' % (len(A), len(B)))
mem = [[l for l in open(os.path.join(d, 'out.txt')) if 'memory hash' in l or re.match(r'= \w+ \d+ [0-9a-f]{16} \d+ pages', l)] for d in sys.argv[1:3]]
print('answers: %s (%d compared)%s' % ('identical' if not bad else 'DIFFERENT', len(A), '; memory hashes ' + ('identical' if mem[0] == mem[1] else 'DIFFERENT') if mem[0] else ''))
sys.exit(1 if bad else 0)
