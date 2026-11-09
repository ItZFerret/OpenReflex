"""Build the isolated playable-test runtime; never replaces canonical reflexcore."""
import json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
base=Path(__file__).resolve().parent
source_line=re.search(r'add_library\(reflexcore SHARED ([^)]+)\)',(ROOT/'native/CMakeLists.txt').read_text()).group(1)
sources=[ROOT/'native'/x for x in source_line.split()]
sources += [ROOT/'native'/x for x in ('drafts/bike_runtime/bike_runtime.cpp','drafts/controls_terrain/controls_terrain_draft.cpp','drafts/controls_terrain/terrain_ray_bridge.cpp','drafts/slot12/slot12_draft.cpp','drafts/slot13/slot13_draft.cpp','drafts/bike_slot15/bike_slot15_callable.cpp','drafts/bike_step_bridges/bike_step_bridges.cpp','drafts/bike_core_bridge/bike_core_bridge.cpp','drafts/bike_runtime/poststep_impl.cpp')]
arch=sys.argv[1] if len(sys.argv)>1 else 'arm64'
output=base/f'libreflexbike-{arch}.dylib'
command=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch,'-dynamiclib','-DREFLEX_SLOT12_NO_MAIN','-DREFLEX_SLOT13_NO_MAIN','-DREFLEX_CORE_BUILD','-I'+str(ROOT/'native/include'),'-Wl,-install_name,@rpath/libreflexbike.dylib',*map(str,sources),'-o',str(output)]
(base/f'command-{arch}.json').write_text(json.dumps(command,indent=2)+'\n')
with (base/f'compile-{arch}.log').open('w') as log:result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
print(json.dumps(dict(success=result.returncode==0,output=str(output))))
raise SystemExit(result.returncode)
