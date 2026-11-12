#!/bin/sh
# tests/untamed_convert/build/run_gates.sh <scratch dir> [tree|verify|orcheck|race|all] [short names ...]
# The reference converter's own acceptance gates on THIS port's output (the release-form folder UxBuildTrack writes), without writing
# into the reference: ux_verify.py --release --no-renders, ux_orcheck.py and the headless AI race ux_racetest.py (11 AI, 3 laps, through
# the installed Open Reflex core rxcore.dll, no window; docs/headless-race.md of the reference).
#   tree     <scratch>/gates/untamed = a scratch copy of what the gates read of the reference (tools/, templates/, extract/, research/
#            sx-slot/tools, downloads/openreflex-src/repo, the race tool's stock baselines work/headless-race/baseline/*.json; about
#            2.3 GB, copied once), with ux_racetest.py's work folder pointed at the copy,
#            then "build.dll gates": every level built by the C# builder into gates/untamed/release/<short>-cs, its stage outputs into
#            gates/untamed/work/<Level> (what the verifier reads of the stages) and build_info.json + package + level into
#            gates/untamed/build/<short>-cs (where a release verify takes the rollout record from)
#   verify / orcheck / race   run the gate on every (or the named) track; logs in <scratch>/gate-logs/, the RESULT lines are printed
# The tools are copied only when the copy does not exist: the results in core/plan/UNTAMED_CONVERT.md are those of the reference's tools
# of 2026-11-11 14:06 (ux_build_track.py 161ea48c, ux_verify.py ca86a57d, ux_verify_engine.py c9f652a7: after its dirt-look rounds; the
# verifier's V7.10 judges the ground look and W12 the shore colour of the tracks this port writes).
# Environment: UX_DISC_ISO or UX_DISC_FOLDER, UX_REFLEX or UX_CLIENT_LIB (as run.sh), UX_PY_ROOT (read only)
# and UX_PYTHON (run with -I -B and UX_ROOT = the copy): both required. The race needs the installed
# Open Reflex client (its rxcore.dll and "game files"; the tool copies the dll and never writes into the install).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
WHAT=${1:-all}; [ $# -gt 0 ] && shift
PY=${UX_PYTHON:?set UX_PYTHON to the reference converter Python}
REF=${UX_PY_ROOT:?set UX_PY_ROOT to the reference converter untamed folder}
G="$S/gates/untamed"; L="$S/gate-logs"
ALL="norton capitalcity columbus denton harrispark foothills weststone jefferson wilford bridgeport fairview lincoln"
TRACKS=${*:-$ALL}
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$L"
if [ "$WHAT" = tree ] || [ "$WHAT" = all ]; then
  mkdir -p "$G/research/sx-slot" "$G/downloads/openreflex-src" "$G/work" "$G/build" "$G/release"
  [ -d "$G/tools" ] || { cp -r "$REF/tools" "$G/"; rm -rf "$G/tools/__pycache__"; }
  [ -d "$G/templates" ] || cp -r "$REF/templates" "$G/"
  [ -d "$G/extract" ] || cp -r "$REF/extract" "$G/"
  [ -d "$G/research/sx-slot/tools" ] || cp -r "$REF/research/sx-slot/tools" "$G/research/sx-slot/"
  [ -d "$G/downloads/openreflex-src/repo" ] || cp -r "$REF/downloads/openreflex-src/repo" "$G/downloads/openreflex-src/"
  # the race tool judges a track against the stock baselines it measured (work/headless-race/baseline/<LEVEL>.json: the off-track and
  # crash rates of stock levels; without them a converted track's off-track resets are a WARN instead of a note)
  if [ ! -d "$G/work/headless-race/baseline" ]; then
    mkdir -p "$G/work/headless-race/baseline"
    cp "$REF"/work/headless-race/baseline/*.json "$G/work/headless-race/baseline/"
  fi
  # the race tool's work folder is a constant inside the reference tree: the COPY works below UX_ROOT
  "$PY" -I -B - "$G/tools/ux_racetest.py" <<'EOF'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
import re
s2 = re.sub(r'WORK = r"[^"\n]*\\work\\headless-race"', 'WORK = os.path.join(os.environ["UX_ROOT"], "work", "headless-race")', s, count=1)
if s2 != s:
    s = s2
    open(p, 'w', encoding='utf-8', newline='\n').write(s)
assert 'os.environ["UX_ROOT"]' in s, 'ux_racetest.py: the WORK constant was not found'
EOF
  sh "$HERE/../build.sh" "$S" build > /dev/null
  list=$(echo $TRACKS | tr ' ' ',')
  DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/build.dll" gates --scratch "$S" --gate-root "$G" --levels "$list"
fi
export UX_ROOT="$G"
cd "$G"
gate() {   # gate <name> <track> <command ...>: runs it, keeps the log, prints the track's RESULT line
  n=$1; t=$2; shift 2
  if "$@" > "$L/$t-$n.log" 2>&1; then rc=0; else rc=$?; fi
  printf '%-8s %-12s exit %d  %s\n' "$n" "$t" "$rc" "$(grep -a '^RESULT' "$L/$t-$n.log" | tail -1 | cut -c1-260)"
}
for t in $TRACKS; do
  [ -d "$G/release/$t-cs" ] || { echo "no $G/release/$t-cs (run: run_gates.sh <scratch> tree)"; continue; }
  if [ "$WHAT" = verify ] || [ "$WHAT" = all ]; then gate verify "$t" "$PY" -I -B tools/ux_verify.py "release/$t-cs" --release --no-renders --jobs 4 --report-dir "$G/work/verify/$t-cs"; fi
  if [ "$WHAT" = orcheck ] || [ "$WHAT" = all ]; then gate orcheck "$t" "$PY" -I -B tools/ux_orcheck.py "release/$t-cs"; fi
  if [ "$WHAT" = race ] || [ "$WHAT" = all ]; then mkdir -p "$G/work/headless-race/runs"; gate race "$t" "$PY" -I -B tools/ux_racetest.py "release/$t-cs" --json "$G/work/headless-race/runs/cs-$t.json"; fi
done
