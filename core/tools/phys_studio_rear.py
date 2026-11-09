#!/usr/bin/env python3
"""phys_studio_rear.py <trace> -- the rear wheel's statistics of a bike_parity_probe turn trace, as the w-physparity rear-grip tables
(~/Desktop/Reflex Review/Physics Feel/parity, rear.py): from the first steering step on, the steps with the rear on the ground, the share
with its lateral impulse at the limit (rear W+c0 == 0: sliding sideways), rolling at the limit (W+c4 == 0), per steering phase."""
import sys, struct
def f(h): return struct.unpack('<f', struct.pack('<I', int(h, 16)))[0]
st = {}; started = False
for line in open(sys.argv[1]):
    r = line.split()
    if 'rear' not in r or 'c' not in r: continue
    i = r.index('rear')
    if not started:
        if abs(f(r[r.index('c') + 1])) <= 0.05: continue
        started = True
    active, lat, roll = r[i + 1], r[i + 2], r[i + 3]
    if active == '0': continue
    steer = f(r[r.index('c') + 1]); ph = 'turn' if abs(steer) > 0.05 else 'straight'
    s = st.setdefault(ph, [0, 0, 0, 0.0]); s[0] += 1; s[1] += lat == '0'; s[2] += roll == '0'; s[3] += abs(f(r[i + 6]))
for ph, (n, lat, roll, v) in sorted(st.items()):
    print(f'{ph:9s} rear active steps {n:6d}  lateral at limit {100*lat/n:5.1f}%  rolling at limit {100*roll/n:5.1f}%  mean |lateral speed| {v/n:.2f} ft/s')
