#!/bin/sh
# showcase_fixture.sh <scratch dir> <stock install (holding Database)> <Downloads dir> -- the installs the menus showcase's `importflow` page imports
# (tests/menus_showcase/fixture/UiShowcaseImportFlow.cs), made of APFS clones (cp -c: no extra space) of the stock install; the stock install and
# the downloads are only read:
#   <scratch>/src-modded/MX vs ATV Reflex   the whole install + community packs dropped into Database the original way (Distractions gear,
#                                           BARX kit, the Hurrencane Hills beta-slot track, one 2017 SX DLC track slot), a damaged MXTables_DLC008
#   <scratch>/src-small/MX vs ATV Reflex    the tables, UI and a few small databases and loose files (an import of a few seconds)
#   <scratch>/dest                          where the "another drive" import goes
# Then: tests/menus_showcase/run.py <Showcase.app> --pages importflow --batch --env REFLEX_IMPORTFLOW_FIXTURE=<scratch>
set -eu
S=${1:?scratch}; G=${2:?stock install}; DL=${3:?downloads}
rm -rf "$S/src-modded" "$S/src-small" "$S/dest"; mkdir -p "$S/src-modded" "$S/src-small/MX vs ATV Reflex/Database" "$S/dest"
cp -cR "$G" "$S/src-modded/MX vs ATV Reflex"
D="$S/src-modded/MX vs ATV Reflex/Database"
for z in Distractions.Gear.zip BARXSuzuki.zip 1061-hurrencane-hills.zip; do unzip -q -o -j "$DL/$z" -x "__MACOSX/*" -d "$D"; done
unzip -q -o -j "$DL/DLC0046 (2).zip" "DLC0046/MXTables_DLC0046.*" "DLC0046/MXUI_DLC0046.*" "DLC0046/SX_Rd17_2017_LasVegas.*" -d "$D"
head -c 60 "$D/MXTables_DLC010.dx9.database" > "$D/MXTables_DLC008.dx9.database"; head -c 16 /dev/zero > "$D/MXTables_DLC008.dx9.package"
M="$S/src-small/MX vs ATV Reflex"
for b in MXTables_Default MXTables_DLC003 MXTables_DLC004 MXTables_DLC005 MXUI MXUIPatch MXUI_DLC001 MXUI_DLC003 MXUI_DLC004 MXUI_DLC005 UIScene UI_Compound UI_CompoundExtras UIAudio \
         Rider HR_Rider AIData CameraTweaks MX_Wheels MX_Forks Rider_Goggles_Models HR_Rider_Goggles_Models; do
  for e in database package; do cp -c "$G/Database/$b.dx9.$e" "$M/Database/"; done
done
for f in "$G"/*.xml "$G"/*.bxml "$G"/MXReflex.exe; do cp -c "$f" "$M/"; done
cp -cR "$G/Physics" "$M/Physics"
echo "fixture ready: $S"
