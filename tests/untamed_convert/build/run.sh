#!/bin/sh
# tests/untamed_convert/build/run.sh <scratch dir> [lzx|options|build|determinism|variants|cancel|all] [--levels norton,foothills ...] [--packing lzx|stored|raw]
#                                    [--threads N] [--reuse] [--core-blobs] [--tag X]
# Builds the "build" part (../build.sh) and runs the track builder (UxBuildTrack + UxPackage + UxLzxEncoder, with S1-S8 from the disc)
# against the reference release of each track that the port mirrors (norton-v7 ... lincoln-v7), headless. See Program.cs for what each
# mode compares. A newer release of the reference than those is reported as a SKIP: INCOMPLETE.
#   build        (in 'all') every level into <scratch>/out<tag>/<short>, work stores <scratch>/work<tag>/<Level>
#   determinism  (not in 'all') each named level three times: twice with all threads, once with one thread
#   variants     (not in 'all') the older option values against the older releases: every named level with --dirt-look untamed
#                --ec-water-shore untamed against norton-v6 ... lincoln-v5 (the releases before the dirt-look rounds), and Fairview /
#                Lincoln also with the old water look and no spray against fairview-v4 / lincoln-v4 and with the level cube as the
#                water's reflection against fairview-v6 / lincoln-v6
#   cancel       (not in 'all') a build cancelled at four points ends within seconds with an OperationCanceledException
#   --core-blobs also writes <scratch>/coreblobs/<short>.bin for core_lzx_test.sh (the C core's resource reader on this build's bytes)
# Inputs (environment; a missing disc or library makes the result INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
#   UX_PY_ROOT      the reference converter, read only: release/<short>-v<k>, build/<short>-v<k>,
#                   work/combined/commands.json
# The Python gates on this port's output (ux_verify, ux_orcheck, the headless race): run_gates.sh. Per-track time and memory: one
# process per level, e.g.  for t in norton denton; do sh run.sh <scratch> build --levels $t --no-compare; done
# expected_sha256.txt (UX_BUILD_EXPECTED): the sha256 of the four .dx9 files of every level as this port writes them; every lzx build is
# compared with it (the files must not depend on the machine or the platform); --write-expected <file> writes the list.
# Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" build > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
export UX_BUILD_EXPECTED=${UX_BUILD_EXPECTED:-$HERE/expected_sha256.txt}
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/build.dll" "$@" --scratch "$S"
