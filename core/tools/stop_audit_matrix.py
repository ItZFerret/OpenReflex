#!/usr/bin/env python3
"""stop_audit_matrix.py <rxcore dir with stop_audit_run> <install root> <out.tsv> [-j N] [--frames N] [--filter regex] [--resume]
                     [--life N] [--no-teardown] [--career]
--life N     the whole life of every event (needs the development hooks: the "strict" library of stop_audit_trace.sh's neighbour, i.e. the
             product objects + stop_audit_trace_hooks.c, where rx_call_original still aborts): the exhibition replay presenter is installed
             (STOP_AUDIT_REPLAY=1, the driver acts as the renderer), N frames after the activity enters RACING it is forced into WindDown
             (N < 0: a natural finish), the run continues 900 frames past Post, the live activity is torn down (STOP_AUDIT_TEARDOWN=1) and the
             session destroyed; the TSV row adds the LIFE line (racing / wind / post / forced frame) and ENDED / noend
--no-teardown  with --life: leave out the activity teardown
--career     run the Free Ride rows (free ride and its challenges) with the UI property InCareerMode (rx_session_event_in_career_mode) too:
             the extra rows are keyed ".../career"
The dynamic half of core/plan/STOP_AUDIT.md: runs the product session (stop_audit_run.c, librxcore) once per event the Unity shell
can offer -- every EventTypeLocations row of the install's MXTables_Default (+ the DLC levels) x every vehicle class of the event type
-- holding the throttle key after the load, and records whether the product stops (an act_unreached / unported function / fault) and
the first rxcore line it printed. Needs the table dump (tables.json) made from MXTables_Default.xml (see the comment in tables()).
Read-only on the install."""
import json, os, re, subprocess, sys, concurrent.futures as cf

def tables(path):
    import xml.etree.ElementTree as ET
    T = {}
    for tb in ET.parse(path).getroot().iter("Table"):
        T[tb.find("Name").text] = [{k: (int(v[5:]) if v.startswith("_int:") else v) for k, v in r.attrib.items()} for r in tb.find("Rows")]
    return T

DLC = [("National", 10, "NAT_DLC05_Dalton", 98), ("National", 10, "NAT_DLC06_DaltonB", 99), ("Supercross", 8, "SX_DLC01_FreightYard", 100),
       ("Supercross", 8, "SX_DLC02_FreightYardB", 101), ("Freestyle", 7, "FMX_DLC03_FunFactory", 102), ("Freestyle", 7, "FMX_DLC04_FunFactoryB", 103)]
RUN = {"National", "Supercross", "WaypointRace", "FreeRide", "Terracross", "Powercross", "UTVMoto", "Freestyle", "SectionRace"}
CHALLENGE_TRACKS = {(a, l): 3 for a in ("HillClimb", "Trailblazer") for l in ("FR_Alpine", "FR_Cheyenne")}   # activity areas of the FR levels (QUEUE.md w-challenge: product.sh)
ACT = {"HillClimb": "HillClimb", "TrailBlazer": "Trailblazer", "TargetPractice": "TargetPractice"}

def jobs(xml):
    T = tables(xml)
    ET = {r["EventTypeId"]: r for r in T["EventType"]}; EL = {r["EventLocationId"]: r for r in T["EventLocation"]}
    cls = {}
    for r in T["EventTypeVehicleClasses"]: cls.setdefault(r["EventTypeId"], []).append(r["VehicleClassId"])
    dur = {r["EventTypeId"]: (r["ActivityDurationType"], r["Min"]) for r in T["EventTypeDurations"]}
    rows = [(ET[r["EventTypeId"]]["ActivityName"], r["EventTypeId"], EL[r["EventLocationId"]]["Filename"], r["EventLocationId"], r["TrackId"]) for r in T["EventTypeLocations"]]
    rows += [(a, t, l, i, 0) for a, t, l, i in DLC]
    out = []
    for act, tid, lvl, lid, trk in rows:
        if act in ACT:   # a Free Ride challenge: EventType FreeRide + EventTypeActivation (the table row is track 0; the levels' activation areas
            # name more: the 17 shipped rows are HillClimb / Trailblazer tracks 0-2 on FR_Alpine and FR_Cheyenne, track 0 on FR_Gibson, TargetPractice track 0)
            for t in range(CHALLENGE_TRACKS.get((ACT[act], lvl), 1)):
                out.append(dict(act="FreeRide", tid=9, lvl=lvl, lid=lid, trk=t, cls=-1, dur=0, dt="", riders=1, ai=0, activation=ACT[act], key=f"{ACT[act]}/{lvl}/{t}"))
            continue
        if act not in RUN: continue
        if act == "FreeRide":
            out.append(dict(act=act, tid=tid, lvl=lvl, lid=lid, trk=trk, cls=-1, dur=0, dt="", riders=1, ai=0, activation="-", key=f"{act}/{lvl}")); continue
        dt, dmin = dur.get(tid, ("Laps", 3))
        for c in cls.get(tid, [0]):
            for riders, ai in ((12, 95),) if act != "WaypointRace" else ((1, 0),):
                out.append(dict(act=act, tid=tid, lvl=lvl, lid=lid, trk=trk, cls=c, dur=3 if dt == "Laps" else dmin, dt=dt, riders=riders, ai=ai, activation="-", key=f"{act}/{lvl}/{trk}/class{c}/r{riders}"))
    return out

def run(j, exe, install, frames, life=None, teardown=True):
    args = [exe, install, j["act"], str(j["tid"]), j["lvl"], str(j["lid"]), str(j["trk"]), str(j["cls"]), str(j["dur"]), j["dt"] or "", str(j["riders"]), str(j["ai"]),
            j["activation"], "3000", str(frames), "17"]   # 17 = DirectInput scan code of W (throttle)
    rc = -1
    env = dict(os.environ)
    if j.get("career"): env["STOP_AUDIT_IN_CAREER_MODE"] = "1"
    if life is not None:
        env["STOP_AUDIT_REPLAY"] = "1"; env["STOP_AUDIT_LIFE"] = str(life)
        if teardown: env["STOP_AUDIT_TEARDOWN"] = "1"
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=1500, errors="replace", env=env)
        text = p.stdout + p.stderr; rc = p.returncode
    except subprocess.TimeoutExpired:
        text = 'RESULT stop state=-1 frames=0 game_time=0 error="timeout"'
    res = next((l for l in text.splitlines() if l.startswith("RESULT")), "RESULT stop state=-1 error=\"no result (exit %d)\"" % rc)
    why = next((l for l in text.splitlines() if "unreached" in l or "not in the reference boot" in l or "unported" in l or "rxcore:" in l), "")
    unp = sorted(set(re.findall(r"RXTRACE-UNPORTED fn=([0-9a-f]+) ex=(\d)", text)))   # the development library logs and goes on
    if unp: why = (why + " " if why else "") + "unported=" + ",".join(f"{a}{'x' if e == '1' else ''}" for a, e in unp)
    if life is not None:
        lf = next((l for l in text.splitlines() if l.startswith("LIFE")), "LIFE none")
        td = next((l for l in text.splitlines() if l.startswith("TEARDOWN")), "TEARDOWN none")
        ended = "ENDED" if any(l.strip() == "ENDED" for l in text.splitlines()) else "noend"
        pod = next((l for l in text.splitlines() if l.startswith("PODIUM summary")), "PODIUM none")   # the post-race presentation (w-podium)
        why = f"{why}\t{lf}\t{td}\t{ended}\texit={rc}\t{pod}"
        if os.environ.get("STOP_AUDIT_LOGDIR"):   # every run's whole output, by key
            with open(os.path.join(os.environ["STOP_AUDIT_LOGDIR"], j["key"].replace("/", "_") + ".log"), "w") as f: f.write(text)
    return j["key"], res, why

if __name__ == "__main__":
    a = sys.argv[1:]; exe = os.path.join(a[0], "stop_audit_run"); install = a[1]; out = a[2]
    n = int(a[a.index("-j") + 1]) if "-j" in a else 3; frames = int(a[a.index("--frames") + 1]) if "--frames" in a else 1300
    flt = a[a.index("--filter") + 1] if "--filter" in a else ""
    xml = os.environ.get("STOP_AUDIT_TABLES", os.path.join(os.path.dirname(install.rstrip("/")), "_re/formats/out/MXTables_Default.xml"))
    J = jobs(xml)
    if "--career" in a:   # the Free Ride rows (free ride and its challenges) once more with InCareerMode
        J += [dict(j, career=True, key=j["key"] + "/career") for j in J if j["act"] == "FreeRide"]
    J = [j for j in J if re.search(flt, j["key"])]
    life = int(a[a.index("--life") + 1]) if "--life" in a else None
    done = set(l.split("\t")[0] for l in open(out)) if os.path.exists(out) and "--resume" in a else set()
    J = [j for j in J if j["key"] not in done]
    print(len(J), "runs", file=sys.stderr)
    with open(out, "a" if "--resume" in a else "w") as f, cf.ThreadPoolExecutor(n) as ex:
        for key, res, why in ex.map(lambda j: run(j, exe, install, frames, life, "--no-teardown" not in a), J):
            f.write(f"{key}\t{res}\t{why}\n"); f.flush()
