#!/usr/bin/env python3
"""vtrace_run.py <traced build dir> <out dir> <set a|b|c> [-j N]   (w-vtlog; build with tools/vtrace_build.sh)
Runs a list of stop_audit_run events on the traced library, one RX_FN_TRACE_OUT file (<out>/<name>.txt) and one log per run, in parallel.
  a  the AI classes of the event types: National / Supercross / Powercross / Terracross / Waypoint / Freestyle / Section Race / Free Ride, MX, ATV,
     UTV, super buggy, Pro Lite, Pro 2 (12 riders, AI 95), 4,500 frames, fuzzed pad
  b  the player's own vehicle from a garage profile (ATV / UTV / super buggy / Pro Lite / Pro 2: <out>/prof or $VT_PROFILES, written by this script:
     rx_profile_cells with PlayerRider.LastEquippedVehicle = AIVehicle 44 / 45 / 46 / 47 / 48), 4,500 frames
  c  whole events with the replay host (Freestyle, Section Race to Post; 1-lap National / Supercross / Powercross / Terracross; Waypoint) and the
     Free Ride challenges (HillClimb / Trailblazer / TargetPractice) with InCareerMode
env: STOP_AUDIT_TABLES (MXTables_Default.xml, as stop_audit_matrix.py), VT_INSTALL (the install root, default <repo>/game files/Archive)."""
import sys, os, subprocess, re, json, concurrent.futures as cf, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import stop_audit_matrix as m
V = os.path.abspath(sys.argv[1]) if len(sys.argv) > 3 else "."          # the traced build dir (stop_audit_run, librxcore.dylib)
OUT = os.path.abspath(sys.argv[2]) if len(sys.argv) > 3 else "."
INST = os.environ.get("VT_INSTALL", os.path.join(HERE, "..", "..", "game files", "Archive"))
J = {j["key"]: j for j in m.jobs(os.environ["STOP_AUDIT_TABLES"])}
def pick(prefix, cls=None, nth=0):
    ks = [k for k in J if k.startswith(prefix) and (cls is None or J[k]["cls"] == cls)]
    return ks[nth] if len(ks) > nth else None
FR = 4500
SETS = {
 "a": [  # (name, job key or dict, frames, extra env, keys)
  ("nat_mx",    pick("National/NAT_Concord/", 2), FR, {}, "fuzz:7"),
  ("nat_atv",   pick("National/NAT_Concord/", 1), FR, {}, "fuzz:8"),
  ("nat_mxatv", pick("National/NAT_Concord/", 10), FR, {}, "fuzz:9"),
  ("nat_moto",  pick("National/NAT_Concord/", 15), FR, {}, "fuzz:10"),
  ("pc_utv",    pick("Powercross/PX_Laguna/", 22), FR, {}, "fuzz:11"),
  ("pc_buggy",  pick("Powercross/PX_Laguna/", 19), FR, {}, "fuzz:12"),
  ("pc_truck",  pick("Powercross/PX_Laguna/", 21), FR, {}, "fuzz:13"),
  ("pc_pro2",   pick("Powercross/PX_Laguna/", 27), FR, {}, "fuzz:14"),
  ("pc_mixed",  pick("Powercross/PX_Laguna/", 23), FR, {}, "fuzz:15"),
  ("sx_mx",     pick("Supercross/", 2), FR, {}, "fuzz:16"),
  ("sx_mxatv",  pick("Supercross/", 10), FR, {}, "fuzz:17"),
  ("fmx_mx",    pick("Freestyle/", 25), FR, {}, "fuzz:18"),
  ("fmx_atv",   pick("Freestyle/", 24), FR, {}, "fuzz:19"),
  ("fmx_mix",   pick("Freestyle/", 26), FR, {}, "fuzz:20"),
  ("tx_atv",    pick("Terracross/", 1), FR, {}, "fuzz:21"),
  ("tx_utv",    pick("Terracross/", 22), FR, {}, "fuzz:22"),
  ("tx_pro2",   pick("Terracross/", 27), FR, {}, "fuzz:23"),
  ("tx_unl",    pick("Terracross/", 17), FR, {}, "fuzz:24"),
  ("wp_mx",     pick("WaypointRace/", 2), FR, {}, "fuzz:25"),
  ("wp_utv",    pick("WaypointRace/", 22), FR, {}, "fuzz:26"),
  ("sr_mx",     pick("SectionRace/", 2), FR, {}, "fuzz:27"),
  ("sr_atv",    pick("SectionRace/", 1), FR, {}, "fuzz:28"),
  ("fr_mx",     pick("FreeRide/", None), FR, {}, "fuzz:29"),
 ],
}
def run(name, key, frames, env, keys, outdir, over=None):
    j = dict(J[key], **(over or {}))
    args = [V + "/stop_audit_run", INST, j["act"], str(j["tid"]), j["lvl"], str(j["lid"]), str(j["trk"]), str(j["cls"]), str(j["dur"]), j["dt"] or "", str(j["riders"]), str(j["ai"]), j["activation"], "3000", str(frames), keys]
    e = dict(os.environ, RX_FN_TRACE_OUT="%s/%s.txt" % (outdir, name), **env)
    t = time.time()
    p = subprocess.run(args, capture_output=True, text=True, errors="replace", env=e, timeout=3000)
    open("%s/%s.log" % (outdir, name), "w").write(p.stdout + p.stderr)
    res = next((l for l in (p.stdout + p.stderr).splitlines() if l.startswith("RESULT")), "no RESULT rc=%d" % p.returncode)
    loc = next((l for l in (p.stdout + p.stderr).splitlines() if l.startswith("LOCAL vehicle")), "")
    return name, key, res, loc[:60], int(time.time() - t)

def make_profile(path, last_vehicle):
    """a garage profile (rx_profile_cell rows: table[32] column[32] text[128] row type value, as stop_audit_run's STOP_AUDIT_PROFILE reads) whose pro
    rider rides AIVehicle `last_vehicle` (PlayerRider.LastEquippedVehicle: 44 ATV, 45 UTV, 46 super buggy, 47 Pro Lite truck, 48 Pro 2 truck)"""
    import struct
    R = [("PlayerDatabaseVersion", 71), ("SkinRiderGearId", 321), ("Number", "998"), ("AccountBalance", 0), ("RiderHomeId", -1), ("RiderTrailerId", -1),
         ("RiderFontId", -1), ("VehicleFontId", -1), ("RiderInterfaceId", -1), ("Name", "RIDER"), ("NameStatus", 1), ("LastEquippedVehicle", last_vehicle),
         ("PercentComplete", 0), ("TotalEarnings", 0), ("MilesDriven", 0), ("ControllerPresetId", 1), ("LastRiderId", 1), ("LastRiderGearSetId", 3),
         ("TourVisited", 0), ("ChampionshipVisited", 0), ("ControllerSensor", 0), ("DownloadedPackages", 0), ("CareerVisited", 0), ("MotocardSetup", 0),
         ("TutorialsEntered", 0), ("TutorialsCompleted", 0)]
    cells = [("PlayerRider", c, "", 0, 0, v) if isinstance(v, int) else ("PlayerRider", c, v, 0, 1, 0) for c, v in R]
    cells += [("PlayerRiderGear", "RiderGearId", "", i, 0, g) for i, g in enumerate((486, 507, 448, 414, 438, 321))]
    cells += [("PlayerVehicle", "PlayerVehicleId", "", 0, 0, 0), ("PlayerVehicle", "VehicleModelId", "", 0, 0, 11), ("PlayerVehicle", "Favorite", "", 0, 0, 1)]
    cells += [("PlayerVehicleParts", "PlayerVehicleId", "", i, 0, 0) for i in range(7)]
    cells += [("PlayerVehicleParts", "VehiclePartId", "", i, 0, p) for i, p in enumerate((663, 625, 615, 228, 11, 627, 851))]
    cells += [("PlayerVehicleTuning", "PlayerVehicleId", "", 0, 0, 0)] + [("PlayerVehicleTuning", c, "", 0, 0, v) for c, v in
              (("Engine1", 0), ("Engine2", 0), ("Suspension1", 0), ("Suspension2", 0), ("Tire1", 0), ("Tire2", 0), ("Braking", 50), ("Steering", 50))]
    with open(path, "wb") as f:
        for t, c, tx, row, ty, v in cells:
            f.write(t.encode().ljust(32, b"\0") + c.encode().ljust(32, b"\0") + tx.encode().ljust(128, b"\0") + struct.pack("<iii", row, ty, v))


PROFDIR = os.environ.get("VT_PROFILES", os.path.join(OUT, "prof"))
P = PROFDIR + "/"
SETS["b"] = [
  ("p_atv_nat",    pick("National/NAT_Concord/", 2), FR, {"STOP_AUDIT_PROFILE": P + "atv.bin"}, "fuzz:31"),
  ("p_atv_tx",     pick("Terracross/", 1), FR, {"STOP_AUDIT_PROFILE": P + "atv.bin"}, "fuzz:32"),
  ("p_atv_sx",     pick("Supercross/", 1), FR, {"STOP_AUDIT_PROFILE": P + "atv.bin"}, "fuzz:33"),
  ("p_utv_pc",     pick("Powercross/PX_Laguna/", 22), FR, {"STOP_AUDIT_PROFILE": P + "utv.bin"}, "fuzz:34"),
  ("p_utv_tx",     pick("Terracross/", 22), FR, {"STOP_AUDIT_PROFILE": P + "utv.bin"}, "fuzz:35"),
  ("p_utv_wp",     pick("WaypointRace/", 22), FR, {"STOP_AUDIT_PROFILE": P + "utv.bin"}, "fuzz:36"),
  ("p_utv_fr",     pick("FreeRide/", None), FR, {"STOP_AUDIT_PROFILE": P + "utv.bin"}, "fuzz:37"),
  ("p_buggy_pc",   pick("Powercross/PX_Laguna/", 19), FR, {"STOP_AUDIT_PROFILE": P + "buggy.bin"}, "fuzz:38"),
  ("p_buggy_fr",   pick("FreeRide/", None), FR, {"STOP_AUDIT_PROFILE": P + "buggy.bin"}, "fuzz:39"),
  ("p_truck_pc",   pick("Powercross/PX_Laguna/", 21), FR, {"STOP_AUDIT_PROFILE": P + "truck.bin"}, "fuzz:40"),
  ("p_truck_wp",   pick("WaypointRace/", 21), FR, {"STOP_AUDIT_PROFILE": P + "truck.bin"}, "fuzz:41"),
  ("p_pro2_pc",    pick("Powercross/PX_Laguna/", 27), FR, {"STOP_AUDIT_PROFILE": P + "pro2.bin"}, "fuzz:42"),
  ("p_pro2_fr",    pick("FreeRide/", None), FR, {"STOP_AUDIT_PROFILE": P + "pro2.bin"}, "fuzz:43"),
  ("p_pro2_mix",   pick("Powercross/PX_Laguna/", 23), FR, {"STOP_AUDIT_PROFILE": P + "pro2.bin"}, "fuzz:44"),
]
CH = [k for k in J if k.startswith(("HillClimb", "Trailblazer", "TargetPractice"))]
LONG = 14000
SETS["c"] = [
  ("l_fmx_mx",   pick("Freestyle/", 25), 9000, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:51"),
  ("l_fmx_atv",  pick("Freestyle/", 24), 9000, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:52"),
  ("l_sr_mx",    pick("SectionRace/", 2), 9000, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:53"),
  ("l_nat_1lap", pick("National/NAT_Concord/", 2), LONG, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:54", {"dur": 1}),
  ("l_sx_1lap",  pick("Supercross/", 2), LONG, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:55", {"dur": 1}),
  ("l_pc_utv1",  pick("Powercross/PX_Laguna/", 22), LONG, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:56", {"dur": 1}),
  ("l_wp_mx",    pick("WaypointRace/", 2), LONG, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:57"),
  ("l_tx_atv",   pick("Terracross/", 1), LONG, {"STOP_AUDIT_REPLAY": "1"}, "fuzz:58", {"dur": 1}),
] + [("c_" + k.replace("/", "_"), k, 4500, {"STOP_AUDIT_IN_CAREER_MODE": "1", "STOP_AUDIT_REPLAY": "1"}, "fuzz:%d" % (60 + i)) for i, k in enumerate(CH[:9])]
if __name__ == "__main__":
    sets = sys.argv[3]; outdir = OUT; n = int(sys.argv[sys.argv.index("-j") + 1]) if "-j" in sys.argv else 8
    if sets == "b":
        os.makedirs(PROFDIR, exist_ok=True)
        for nm, vid in (("atv", 44), ("utv", 45), ("buggy", 46), ("truck", 47), ("pro2", 48)):
            make_profile(os.path.join(PROFDIR, nm + ".bin"), vid)
    jobs = [x for x in SETS[sets] if x[1]]
    with cf.ThreadPoolExecutor(n) as ex:
        for r in ex.map(lambda x: run(*x[:5], outdir, x[5] if len(x) > 5 else None), jobs):
            print("\t".join(map(str, r)), flush=True)
