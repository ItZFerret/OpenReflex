# rtpoke.py <product pair> <oracle pair> [frame] -- w-parity2: the riders' pre-race residue as PROBE_POKE entries ("frame:addr:word").
# Both pair files need PAIR_RTAIL lines ("RT <frame> <slot> <R> <words>", core/tools/gridpair_line.inc) of the same aligned frame
# (product 1011 / oracle 1355 = k 150). For every rider slot, the words of its RiderAnimation's lag filters (+0x5b964 .. +0x5bb00:
# value / tau / k of the locomotion's lags), the nod timer / start (+0x5b514 / +0x5b518) and the 11 noise wobble phases (+0x5b300 +
# 0x30 i) that differ, pointer pairs left out, written in the product after <frame> (default 1011).
# Why: the riders exist from the loading page on and run their locomotion update (RiderAnimation::vf19 mode 0) until the activity
# switches them to the gate pose (mode 1). The original waits on the loading page for the player's accept (the paired boot: 30
# frames), the product does not, so its riders run fewer such frames (local: 17 vs 57) and the AI riders' first frame takes a
# different dt (the load time): the lags enter the race with other values (+0x5ba4c, the steering layer weight: 0.4067 vs 0.4953),
# and the first locomotion frame (k 190) blends a different steering pose. Player timing, like the grid zone (w-gridpair).
# With PAIR_VTIMERS lines of the same frames ("VT <frame> <slot>:<V> <V+0x1558> <V+0x155c> ...") also every vehicle's two shake
# timers (004674e0, the rider-attached nodes' noise: random at construction; they shake the model's nodes, whose subtree bounds
# the AI's BehaviorSeperate target pick reads through 00533510).
import sys
def load(p):
    d = {}
    for l in open(p):
        if l.startswith('RT '):
            t = l.split(); d[int(t[2])] = (int(t[3], 16), t[4:])
    return d
a, b = load(sys.argv[1]), load(sys.argv[2]); fr = sys.argv[3] if len(sys.argv) > 3 else '1011'
isp = lambda w: 0x10000000 <= int(w, 16) < 0x30000000
out = []
for k in sorted(a):
    if k not in b: continue
    for i, (x, y) in enumerate(zip(a[k][1], b[k][1])):
        o = 0x5b300 + 4 * i
        if x != y and not (isp(x) and isp(y)) and (0x5b964 <= o < 0x5bb00 or o in (0x5b514, 0x5b518) or (o < 0x5b510 and (o - 0x5b300) % 0x30 == 0)): out.append('%s:%x:%s' % (fr, a[k][0] + o, y))
def rtframe(p):
    for l in open(p):
        if l.startswith('RT '): return l.split()[1]
def vt(p, want):
    for l in open(p):
        if l.startswith('VT ') and l.split()[1] == want:
            t = l.split(); d = {}
            for i in range(2, len(t), 3):
                k, v = t[i].split(':'); d[int(k)] = (int(v, 16), t[i + 1], t[i + 2])
            return d
    return {}
pa, ob = vt(sys.argv[1], rtframe(sys.argv[1])), vt(sys.argv[2], rtframe(sys.argv[2]))
for k, (V, t1, t2) in sorted(pa.items()):
    if k not in ob: continue
    if t1 != ob[k][1]: out.append('%s:%x:%s' % (fr, V + 0x1558, ob[k][1]))
    if t2 != ob[k][2]: out.append('%s:%x:%s' % (fr, V + 0x155c, ob[k][2]))
print(','.join(out))
