#!/usr/bin/env python3
"""boot_threads.py <trace> [functions.tsv] -- what the guest threads do during a boot (native/game notes/loading.md).

The trace comes from reflex_boot with REFLEX_BOOT_THREADS=<trace> (boot.cpp, observation only):
  T tid start param steps creator            thread created
  R tid instructions reason at ra1..ra4 [w]  a thread's run ended: S time slice (preempted), B blocked on handles, C blocked on a
                                             critical section, L sleep, Y yield (Sleep(0) / SwitchToThread), F frame stop, X exit;
                                             at = pc (S) or the blocking call's return address
  E tid handle:name ra                       SetEvent
  F frame steps now game-state               Engine::Frame entered
  P steps text                               start_event milestones
  Z tid ra count                             zero-timeout waits that timed out (polls); tid | 0x80000000: failed
                                             InterlockedCompareExchange, | 0x40000000: failed TryEnterCriticalSection
  W tid jump ra                              baton: every thread waits; virtual time jumps (100 ns) to tid's timeout (ra: its wait)
  V tid ahead first ra                       baton: a lonely yield (the others all wait; +10 us); ahead: to the earliest timeout
Prints per thread (by start address): instructions per phase, run endings, where it blocks, where it is preempted, what it signals,
and the polls (spin-waits that only end because another thread runs: they rely on preemption)."""
import bisect, collections, os, sys

trace = sys.argv[1]
ftsv = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '../../../_re/out/functions.tsv')
starts, names = [], []
for line in open(ftsv):
    f = line.rstrip('\n').split('\t')
    if f[0] == 'entry': continue
    starts.append(int(f[0], 16)); names.append(f[1])

def fn(a):
    i = bisect.bisect_right(starts, a) - 1
    if a < 0x401000 or a >= 0x916000 or i < 0: return '%08x' % a
    return '%s+%x' % (names[i], a - starts[i])

THREAD_KIND = {0x00000000: 'main (WinMain, Engine::Frame)', 0x00881430: 'never resumed (suspended)', 0x007df370: 'render-sync worker',
               0x007d22a0: 'scene ticker', 0x0084c760: 'Database async loader', 0x00806f90: 'FMOD command queue',
               0x005a7f10: 'saved-game manager', 0x0084f320: 'engine task thread', 0x00760400: 'game init thread',
               0x00881900: 'engine job pool', 0x007a5a70: 'ModelGroup worker', 0x0061cdb0: 'rider animation worker',
               0x00662ec0: 'forest system worker'}
start_of, created = {}, {}
runs = collections.defaultdict(lambda: collections.Counter())
instr = collections.defaultdict(lambda: collections.Counter())    # tid -> phase -> instructions
blocks = collections.defaultdict(collections.Counter)             # tid -> (reason, site, waits) -> count
slices = collections.defaultdict(collections.Counter)             # tid -> preempted function -> count
signals = collections.defaultdict(collections.Counter)            # tid -> handle -> count
polls = []
jumps = collections.Counter(); jump_time = collections.Counter(); lonely = collections.Counter()   # baton: time jumps, lonely yields
phase, phases = 'boot', ['boot']
for line in open(trace):
    f = line.split()
    if not f: continue
    k = f[0]
    if k == 'T': start_of[f[1]] = int(f[2], 16); created[f[1]] = int(f[4])
    elif k == 'P':
        t = ' '.join(f[2:])
        nxt = {'front end': 'front end', 'StartEvent': 'loading', 'world running': 'running'}
        for key, ph in nxt.items():
            if t.startswith(key) and ph != phase: phase = ph; phases.append(ph)
    elif k == 'F':
        if phase == 'boot' and int(f[4]) == 2 and 'front end' not in phases: phase = 'front end'; phases.append(phase)
    elif k == 'R':
        tid, n, why, at = f[1], int(f[2]), f[3], int(f[4], 16)
        instr[tid][phase] += n; runs[tid][why] += 1
        ras = [x for x in f[5:9] if x != '-']
        if why == 'S':
            site = fn(at) if 0x401000 <= at < 0x916000 else fn(int(ras[0], 16)) + ' (in a call)' if ras else '%08x' % at
            slices[tid][site] += 1
        elif why in 'BCLY':
            w = ' '.join(x for x in f[5:] if not (len(x) == 8 and all(c in '0123456789abcdef' for c in x)) and x != '-')
            blocks[tid][(why, fn(at), w)] += 1
    elif k == 'E': signals[f[1]][f[2]] += 1
    elif k == 'Z': polls.append((int(f[3]), int(f[1], 16), int(f[2], 16)))
    elif k == 'W': jumps[(f[1], int(f[3], 16))] += 1; jump_time[(f[1], int(f[3], 16))] += int(f[2])
    elif k == 'V': lonely[(f[1], phase)] += 1

order = sorted(instr, key=lambda t: int(t, 16))
total = sum(sum(c.values()) for c in instr.values())
print('phases: %s; %.2f G instructions in all' % (' -> '.join(phases), total / 1e9))
print('%-6s %-9s %-32s %s  %s' % ('tid', 'start', 'kind', '  '.join('%10s' % p[:10] for p in phases), 'run ends'))
for t in order:
    s = start_of.get(t, 0)
    print('%-6s %08x  %-32s %s  %s' % (t, s, THREAD_KIND.get(s, fn(s)), '  '.join('%10.1fM' % (instr[t][p] / 1e6) for p in phases),
                                        ' '.join('%s%d' % (r, c) for r, c in sorted(runs[t].items()))))
print('\nwhere each thread blocks (reason site waits: count; B handle wait, C critical section, L sleep, Y yield)')
for t in order:
    s = start_of.get(t, 0)
    for (why, site, w), c in blocks[t].most_common(6):
        print('  %-6s %-24s %s %-40s %-50s %6d' % (t, THREAD_KIND.get(s, '%08x' % s)[:24], why, site, w[:50], c))
print('\nwhere threads are preempted (time-slice ends: count)')
for t in order:
    s = start_of.get(t, 0)
    for site, c in slices[t].most_common(8):
        print('  %-6s %-24s %-60s %6d' % (t, THREAD_KIND.get(s, '%08x' % s)[:24], site, c))
print('\nsignals (SetEvent: count)')
for t in order:
    for h, c in signals[t].most_common(5):
        print('  %-6s %-24s %-40s %6d' % (t, THREAD_KIND.get(start_of.get(t, 0), '')[:24], h, c))
print('\npolls: zero-timeout waits that timed out / failed compare-exchange / failed TryEnterCriticalSection')
for c, tid, ra in sorted(polls, reverse=True)[:20]:
    kind = 'CAS' if tid & 0x80000000 else 'TryEnterCS' if tid & 0x40000000 else 'wait(0)'
    t = '%x' % (tid & 0x3fffffff)
    print('  %-6s %-24s %-10s %-44s %10d' % (t, THREAD_KIND.get(start_of.get(t, 0), '')[:24], kind, fn(ra), c))
if jumps or lonely:
    print('\nbaton: virtual time jumps to a timeout (every thread waits): thread, its wait, count, total ms')
    for (t, ra), c in jumps.most_common(12):
        print('  %-6s %-24s %-44s %8d %10.1f' % (t, THREAD_KIND.get(start_of.get(t, 0), '')[:24], fn(ra), c, jump_time[(t, ra)] / 1e4))
    print('baton: lonely yields (+10 us each): thread, phase, count')
    for (t, ph), c in lonely.most_common(12):
        print('  %-6s %-24s %-12s %8d' % (t, THREAD_KIND.get(start_of.get(t, 0), '')[:24], ph, c))
