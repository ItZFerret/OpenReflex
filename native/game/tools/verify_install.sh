#!/bin/bash
# verify_install.sh [noinstall] -- builds libreflexworld (with every native/game port) and the CLI, then requires, on the five
# verify worlds (Unity's default world settings, the REFLEX_DRIVE rider, 1500-1800 frames): the reference hashes without ports AND
# with every port on (REFLEX_PORT=all); the plugin's own world build (reflex_client: reflex_world_build, baton scheduling) of the
# default event with the original code byte-identical to the reference image, and its build with every port in the boot racing to the
# reference hashes; installs the plugin into the Unity project (atomic rename; restart Unity to load it) only if all hold.
# References (2026-10-18, notes/loading.md section 9): baton worlds built by the plugin after the CPUID fix (scratchpad boot/worlds).
S=${REFLEX_TESTDATA:-$(cd "$(dirname "$0")/../../.." && pwd)/testdata}   # test inputs: boot/worlds, crt, events (see testdata/README.md)
G="/Users/payton/Documents/Development/REFLEX/game files/Archive"
W=/Users/payton/Documents/Development/REFLEX/native/drafts/world_runtime
cd $W || exit 1
./build.sh 2>&1 | grep -E " error|built"; [ ${PIPESTATUS[0]} -eq 0 ] || { echo BUILD-FAILED; exit 1; }
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; mkdir -p $T/save
ok=1
# world:frames:drive:state hash:memory hash (references of the original code on the baton worlds, CPUID fixed, 2026-10-18 evening;
# until then exact-slice worlds and CPUID without SSE2: 14fbb46b422c34d8:f1a02da0f3546f3b fc428ffca95782b6:62b9d1986dc7b8ba
# 9a5002d41e4ece6b:9c6ce38c09cfa013 210a0eafb60f5e69:9895014866da6382 339c9eb6fa778d96:28623e7b63b760c9, default world bj/ci-world.bin)
REF=1800:1:a936214c3ad9ccf1:aff6eee02af59604
for row in National_NAT_Concord:$REF National_NAT_Concord_ai11:1500:2:fd1d4b45b48e5a5e:527c8a1386afeec3 \
           FreeRide_FR_Alpine:1500:3:1ef28933aa2c03c7:e77600c0ba4fa124 SX_Springfield_ai11:1500:4:fe8060ee3ca40a7b:5f6b6c29e7e0024d \
           Terracross_TRX_Verde_ai11:1500:5:0f1c6fa1301d6af3:77f774138f2cb6f5; do
  IFS=: read w n d sh mh <<< "$row"
  for p in none all; do
    out=$(env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=$d REFLEX_MEMHASH=1 REFLEX_PORT=$p \
          REFLEX_WORLD_TEST=$S/boot/worlds/$w REFLEX_AFTER=$n perl -e 'alarm 1800; exec @ARGV' $W/reflex_boot "$G" $S/crt $T/save $n $T/log.txt 2>&1)
    ms=$(echo "$out" | grep "ms/frame" | tail -1 | awk '{print $1}')
    if echo "$out" | grep -q "state hash $sh" && echo "$out" | grep -q "memory hash $mh"; then echo "$w ports=$p: ok ($ms ms/frame)"; else echo "$w ports=$p: MISMATCH"; echo "$out" | grep -E "hash|STOPPED"; ok=0; fi
  done
done
# the plugin's world build of the default event (as Unity builds it: reflex_world_build), the original code (boot ports 0) and every port (1)
mkdir -p $T/b0 $T/b1
printf 'boot_ports all 0\nbuild %s/b0/c National,10,NAT_Concord,50,0,3,Laps,2,0\n' $T > $T/b0.txt; printf 'boot_ports all 1\nbuild %s/b1/c National,10,NAT_Concord,50,0,3,Laps,2,0\n' $T > $T/b1.txt
perl -e 'alarm 900; exec @ARGV' $W/reflex_client "$G" $S/crt $T/b0 $T/b0.txt > $T/b0/out.txt 2>&1 & perl -e 'alarm 900; exec @ARGV' $W/reflex_client "$G" $S/crt $T/b1 $T/b1.txt > $T/b1/out.txt 2>&1 & wait
cmp -s $T/b0/c-world.bin $S/boot/worlds/National_NAT_Concord-world.bin && echo DEFAULT-WORLD-IDENTICAL || { echo DEFAULT-WORLD-DIFFERS; grep "^!" $T/b0/out.txt; ok=0; }
IFS=: read n d sh mh <<< "$REF"
out=$(env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=$d REFLEX_MEMHASH=1 REFLEX_PORT=all \
      REFLEX_WORLD_TEST=$T/b1/c REFLEX_AFTER=$n perl -e 'alarm 1800; exec @ARGV' $W/reflex_boot "$G" $S/crt $T/save $n $T/log.txt 2>&1)
if echo "$out" | grep -q "state hash $sh" && echo "$out" | grep -q "memory hash $mh"; then echo "PORTED-BUILD-RACES-IDENTICAL"; else echo "PORTED-BUILD-RACE-MISMATCH"; echo "$out" | grep -E "hash|STOPPED"; grep "^!" $T/b1/out.txt; ok=0; fi
[ $ok -eq 1 ] || { echo VERIFY-FAILED; exit 1; }
[ "$1" == noinstall ] || bash $W/install_unity.sh
