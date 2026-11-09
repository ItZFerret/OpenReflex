#!/bin/bash
# pair.sh <side: product|oracle> <out dir> [VAR=value ...] -- w-parity: one side of the w-rendersync turn pair (Concord, 12 riders,
# ruts off, clocks aligned) with the current addresses: the local sim is at 14c0f120 on main 0982b75 (the wheel-load poke S+0x8870 /
# S+0x89c0; check `PROBE_PAIR_SIMDUMP=9`'s S line after a product change). N frames (2400), PB / RB as gridpair.sh. Compare with
# core/tools/gridpair_cmp.py <out>/pair-product.txt <oracle out>/pair-oracle.txt (the oracle side does not depend on the core unless
# PORTS is set: one reference run serves many product runs).
# Other events (w-parity4): SPEC=<the oracle's type,typeid,location,locid,track,class,durtype,dur,activity> for the oracle side and
# PROBE_EVENT=<type,typeid,location,locid,class,riders,ai> for the product, e.g. SX_Springfield: SPEC=Supercross,8,SX_Springfield,78,0,2,
# Laps,3,0 / PROBE_EVENT=Supercross,8,SX_Springfield,78,2,12,95. The wheel-load poke holds (the front end's sim); RTPOKE is per event:
# regenerate it with rtpoke.py from PAIR_RTAIL=<k 150's frame> PAIR_VTIMERS=1 runs of both sides (the frame argument = the product's).
# EVENT=1 (w-lcgdraws): also pin the event's start, so the game LCG's per-frame draw counts can be compared from the event's first frame:
# both sides reseed at the start of that frame and of the intro's (activity state 5) frames (PAIR_RESEED_EVENT=1, gridpair_line.inc),
# the product's engine clock is the oracle's from the event's first frame (EVENT_CLOCK = <product frame of the last loading frame,
# MXGame state 0xb>:<the oracle's gt word on its own last loading frame>) and the local rider / vehicle residue the front end leaves
# (EVENT_RTPOKE: rtpoke.py with the product's last loading frame from PAIR_RTAIL=<that frame> PAIR_VTIMERS=1 runs of both sides; the AI
# riders are made after it) is the oracle's; RTPOKE (k 150) is left out. The defaults (product frame 8, oracle 352) are the front end's,
# the same for every event (Concord and SX_Springfield give the same frames, clock and residue); regenerate them after a product change
# moves the heap.
# PAIR_RESEED_EVENT=only instead of 1 (as a VAR=value argument on both sides): that pin alone, no later reseed.
# POISON=<word> (w-aidrift, hex): both sides' main-thread stack below the frame filled with the word at every frame start (product
# PROBE_STACK_POISON before each frame, oracle RX_PAIR_STACK_POISON at Engine::Update's entry): the dead stack the original's render pass
# (Engine::Render, which the product does not have) leaves for the next frame is pinned, as RESEED pins the seeds. Without it the
# EVENT=1 PAIR_RESEED_EVENT=only ride parts at k 774: a CylinderVsCylinder Newton step reads that residue (core/notes/phys.md).
# Four-wheel locals (w-parity4w: the trace line has their L / R fields, gridpair_line.inc pair_fw). The local rides an AI rider's vehicle on
# both sides: the oracle REFLEX_GARAGE=-1,0,0,4,<AIVehicle>,-1 (boot.cpp EquipVehicle after EnsureLocalPlayers), the product
# PROBE_EQUIP=4,<AIVehicle> (the same call before the load); 44 = an ATV, 45 = a UTV. E.g. National NAT_Bedford ATV: SPEC=National,10,
# NAT_Bedford,48,0,1,Laps,3,0 / PROBE_EVENT=National,10,NAT_Bedford,48,1,12,95; Powercross PX_Laguna UTV: SPEC=Powercross,16,PX_Laguna,58,
# 0,22,Laps,3,0 / PROBE_EVENT=Powercross,16,PX_Laguna,58,22,12,95 (SPEC is an environment variable of pair.sh, not a VAR=value argument).
# With EVENT=1 also: GRID=1 (both sides: the grid pass's draws pinned, PAIR_RESEED_GRID; Bedford's loading-page draws gave the two locals
# zones 12 / 8); the original's loading takes one frame more on these events (its last loading frame is 353, not 352): EVENT_CLOCK=
# 8:40bd5f1c and, on the product side, PROBE_CLOCK0=57552000 (one frame of 166,700 ticks later: the millisecond clock's phase, whose
# 16 / 17 ms activity dt decides the frame a scripted event fires: with the Concord value the start line's {EnableStartLineState} came one
# frame early); EVENT_RTPOKE = simpoke.py of PROBE_PAIR_SIMDUMP=8 / RX_PAIR_SIMDUMP=353 runs with PAIR_RSIZE=5bb80 PAIR_SIMDUMP_NODES=1
# (--kinds S,R,V,M1L,M1W,M1B,M5L,M5W,M5B,RHL,RHW,RHB): the front-end vehicle's simulation, rider, vehicle and node tables.
# Three more product-side alignments for these rides (w-parity4w round 2): PROBE_NODEFORM_SURF=1 (the deformation surfaces restored every
# frame: the activity's Pre (rx_world_set_terrain_wetness) sets the gain back to 2 inside a frame, the product's CPU stamps of that frame
# moved the vertices under the wheels by an ulp; the oracle never deforms); PROBE_PART_SOFT=<frame>:<slot>.<part>,... (UTV / buggy /
# truck damage parts the original makes "soft" from an uninitialised entity byte, PAIR_DMG's "sh <masks>" 0x14 instead of 0x11: on
# PX_Laguna 18:0.0,18:0.1,18:1.0,18:1.1,18:1.2); the CylinderVsCylinder Newton slot the original's previous-frame rendering leaves
# (nwpoke.py from a PAIR_WORDS=22f110 oracle run, appended to EVENT_RTPOKE: NAT_Bedford's wheel-to-wheel contacts); the mud clock
# [0xa89bc4] (MudRecord_Update 00408c50 runs when it passes 0.1 s; it ticks from boot, through the original's front end) from the
# oracle's last loading frame: 9:a89bc4:<the oracle's frame 354 value, PAIR_MUD=1's "clk"> appended to EVENT_RTPOKE (3d0d26a7 on
# NAT_Bedford: without it the local's mud came 3 frames early and the idle-rider gesture 0054cec0 drew once more at event k 10875).
# w-parity5 (full-length rides, N=40000 on both sides: 39,992 aligned frames from the event start; the turn-scripted local never
# finishes, so the race state stays 2): use POISON=7fc0dead for every ride (MX and four-wheel). It supersedes nwpoke.py (which pinned only
# the first CylinderVsCylinder slot of a frame: with it the Concord MX ride still parted at k 3963 and NAT_Bedford at event frame
# 17878, both on a Newton step reading render residue). Buggy / trophy truck: Powercross PX_Laguna class 19 (AIVehicle 46, the super
# buggy) / class 21 (47, the Pro Lite truck), aligned as the UTV (EVENT_RTPOKE = their own simpoke.py + the mud clock poke) with
# PROBE_PART_SOFT 18:0.0,18:0.1,18:1.0,18:1.1,18:1.2 (buggy) / 18:0.0,18:0.1,18:1.0,18:1.1,18:1.4 (truck: PAIR_DMG=1 oracle run, the
# parts whose entity +0x15d is set). The product reads long poke lists from PROBE_POKE_FILE=<file> (same entries as PROBE_POKE).
set -e
R=$(cd "$(dirname "$0")/../../.." && pwd); side=${1:?side}; D=${2:?out}; shift 2
# w-parity2: RTPOKE = the riders' pre-race residue (lag filters, nod timer, noise wobble phases) and the vehicles' shake timers, written
# after product frame 1011 (k 150): core/tools/parity/rtpoke.py (regenerate after a product change moves the heap); RTPOKE= leaves it out.
# main 06b17369's addresses.
RTPOKE=${RTPOKE-1011:14c781b0:426ba1e0,1011:14c781e0:426c3b7a,1011:14c78210:426cf3cc,1011:14c78240:426d1cc1,1011:14c78270:426dd513,1011:14c782a0:426e1cc1,1011:14c782d0:426c8328,1011:14c78300:426d1284,1011:14c78330:426eb65b,1011:14c78360:4272f3cc,1011:14c78390:4270d201,1011:14c783c4:3f2ef7e0,1011:14c783c8:3f37e800,1011:14c78834:3e7ffd98,1011:14c78848:3a9d07e0,1011:14c78884:3f333183,1011:14c788fc:3efd958c,1011:2786ece0:424de1ab,1011:2786ed10:424e7b45,1011:2786ed40:424f3397,1011:2786ed70:424f5c8c,1011:2786eda0:425014de,1011:2786edd0:42505c8c,1011:2786ee00:424ec2f3,1011:2786ee30:424f524f,1011:2786ee60:4250f626,1011:2786ee90:42553397,1011:2786eec0:425311cc,1011:2786f40c:3f800000,1011:2786f420:3f800000,1011:2786f42c:3ea6c217,1011:2786f434:3dccd74a,1011:27953ec0:4088f755,1011:27953ef0:408dc422,1011:27953f20:409386b1,1011:27953f50:4094ce5f,1011:27953f80:409a90ef,1011:27953fb0:409cce5f,1011:27953fe0:40900192,1011:27954010:40947c74,1011:27954040:40a19b2c,1011:27954070:40c386b1,1011:279540a0:40b2785b,1011:279545ec:3f800000,1011:27954600:3f800000,1011:2795460c:3e9cd717,1011:27954614:3dccd74a,1011:27a34db0:42855378,1011:27a34de0:4285a045,1011:27a34e10:4285fc6e,1011:27a34e40:428610e9,1011:27a34e70:42866d12,1011:27a34ea0:428690e9,1011:27a34ed0:4285c41c,1011:27a34f00:42860bca,1011:27a34f30:4286ddb5,1011:27a34f60:4288fc6e,1011:27a34f90:4287eb88,1011:27a354dc:3f800000,1011:27a354f0:3f800000,1011:27a354fc:3e91d1eb,1011:27a35504:3dccd74a,1011:27b17060:4138b780,1011:27b17090:413b1de6,1011:27b170c0:413dff2e,1011:27b170f0:413ea305,1011:27b17120:4141844d,1011:27b17150:4142a305,1011:27b17180:413c3c9f,1011:27b171b0:413e7a0f,1011:27b171e0:4145096c,1011:27b17210:4155ff2e,1011:27b17240:414d7803,1011:27bfb6e0:4291d2ad,1011:27bfb710:42921f7a,1011:27bfb740:42927ba3,1011:27bfb770:4292901e,1011:27bfb7a0:4292ec47,1011:27bfb7d0:4293101e,1011:27bfb800:42924351,1011:27bfb830:42928aff,1011:27bfb860:42935cea,1011:27bfb890:42957ba3,1011:27bfb8c0:42946abd,1011:27cdc6c0:4148ebc5,1011:27cdc6f0:414b522b,1011:27cdc720:414e3373,1011:27cdc750:414ed74a,1011:27cdc780:4151b892,1011:27cdc7b0:4152d74a,1011:27cdc7e0:414c70e4,1011:27cdc810:414eae54,1011:27cdc840:41553db1,1011:27cdc870:41663373,1011:27cdc8a0:415dac48,1011:27dbd7d0:40caee00,1011:27dbd800:40cfbacd,1011:27dbd830:40d57d5c,1011:27dbd860:40d6c50a,1011:27dbd890:40dc879a,1011:27dbd8c0:40dec50a,1011:27dbd8f0:40d1f83d,1011:27dbd920:40d6731f,1011:27dbd950:40e391d7,1011:27dbd980:4102beae,1011:27dbd9b0:40f46f06,1011:27e9eb50:4220abbf,1011:27e9eb80:42214559,1011:27e9ebb0:4221fdab,1011:27e9ebe0:422226a0,1011:27e9ec10:4222def2,1011:27e9ec40:422326a0,1011:27e9ec70:42218d07,1011:27e9eca0:42221c63,1011:27e9ecd0:4223c03a,1011:27e9ed00:4227fdab,1011:27e9ed30:4225dbe0,1011:27f7fc60:4286ac52,1011:27f7fc90:4286f91f,1011:27f7fcc0:42875548,1011:27f7fcf0:428769c3,1011:27f7fd20:4287c5ec,1011:27f7fd50:4287e9c3,1011:27f7fd80:42871cf6,1011:27f7fdb0:428764a4,1011:27f7fde0:4288368f,1011:27f7fe10:428a5548,1011:27f7fe40:42894462,1011:28060dc0:42a554a2,1011:28060df0:42a5a16f,1011:28060e20:42a5fd98,1011:28060e50:42a61213,1011:28060e80:42a66e3c,1011:28060eb0:42a69213,1011:28060ee0:42a5c546,1011:28060f10:42a60cf4,1011:28060f40:42a6dedf,1011:28060f70:42a8fd98,1011:28060fa0:42a7ecb2,1011:281445a0:41fb6710,1011:281445d0:41fc9a43,1011:28144600:41fe0ae7,1011:28144630:41fe5cd3,1011:28144660:41ffcd76,1011:28144690:42002e59,1011:281446c0:41fd299f,1011:281446f0:41fe4858,1011:28144720:4200c7f7,1011:28144750:4205056b,1011:28144780:4202e3a0,1011:14c0a8d8:43633943,1011:14c0a8dc:43577259,1011:278028e8:4342879a,1011:278028ec:435c2a3c,1011:278e2b98:437c789e,1011:278e2b9c:43776ec6,1011:279c6b88:435014b6,1011:279c6b8c:435ba427,1011:27aa8e38:43811d4b,1011:27aa8e3c:434c7cab,1011:27b8d168:436208b7,1011:27b8d16c:4359faf3,1011:27c6e498:43809865,1011:27c6e49c:433f96dc,1011:27d4f5a8:435c2255,1011:27d4f5ac:435df267,1011:27e30808:435cbe19,1011:27e3080c:43485cca,1011:27f118e8:435420df,1011:27f118ec:4368453b,1011:27ff2b48:432d66f3,1011:27ff2b4c:4332b4c7,1011:280d3d18:434f77af,1011:280d3d1c:43819e6f}
EVENT_RTPOKE=${EVENT_RTPOKE-8:14c781b0:426ba1e0,8:14c781e0:426c3b7a,8:14c78210:426cf3cc,8:14c78240:426d1cc1,8:14c78270:426dd513,8:14c782a0:426e1cc1,8:14c782d0:426c8328,8:14c78300:426d1284,8:14c78330:426eb65b,8:14c78360:4272f3cc,8:14c78390:4270d201,8:14c783c4:3f444e4c,8:14c7881c:3a7b185b,8:14c78820:bb781f3f,8:14c7882c:3abc86c0,8:14c78834:3e7ff117,8:14c78848:3bc859f4,8:14c78884:3f3328c3,8:14c788a0:bd726be0,8:14c788a4:3f0ac00a,8:14c788a8:c0730b52,8:14c788ac:00000000,8:14c788b8:be981e68,8:14c788dc:3c888f87,8:14c788f0:3d088f87,8:14c788fc:3efdce70,8:14c78940:3efe0fc2,8:14c78944:3efe00aa,8:14c78980:be0b152f,8:14c78984:bab35a86,8:14c78988:3f7da0a0,8:14c78990:3ba97f3a,8:14c78994:3f7ffefc,8:14c78998:3b08fcf8,8:14c789a0:bf7d9fce,8:14c789a4:3bb13aab,8:14c789a8:be0b12c6,8:14c0a8d8:426ec083,8:14c0a8dc:423fa523}
CLOCK=18:40c22c2c; EV=
if [ -n "$EVENT" ]; then RTPOKE=$EVENT_RTPOKE; CLOCK=${EVENT_CLOCK:-8:40bcd68c}; EV=PAIR_RESEED_EVENT=1; fi
GR=; [ -n "$GRID" ] && GR=PAIR_RESEED_GRID=1   # w-parity4w: the grid pass's draws pinned too (gridpair_line.inc)
POKE=${POKE:-9:14c17990:4302ee94,9:14c17ae0:4330b314,2:9dfa88:1,2:9dfa8c:0,2:9dfaa8:168c${RTPOKE:+,$RTPOKE}}
if [ "$side" = product ]; then
  RESEED=1 CLOCK_AT=$CLOCK bash "$R/core/tools/gridpair.sh" "$D" product "${N:-2400}" RX_WHIP_MODE=turn PROBE_DT=0.01667 PROBE_NODEFORM=1 PROBE_CLOCK0=57385300 PROBE_POKE=$POKE $EV $GR ${POISON:+PROBE_STACK_POISON=$POISON} "$@"
else
  RESEED=1 CLOCK_AT=18:40c22c2c bash "$R/core/tools/gridpair.sh" "$D" oracle "${N:-2400}" RX_WHIP_MODE=turn PROBE_DT=0.01667 PROBE_NODEFORM=1 RX_PAIR_NODEFORM=1 PROBE_CLOCK0=57385300 REFLEX_MS_FROM_FRAME=1 $EV $GR ${POISON:+RX_PAIR_STACK_POISON=$POISON} "$@"
  if grep -q "^P 1400 .* L cls " "$D/pair-oracle.txt"; then   # w-parity4w: a four-wheel local (its input block's steer, S+0x2c)
    grep -q "^P .* L cls [0-9a-f]* off [0-9]* in [0-9a-f]* [0-9a-f]* [0-9a-f]* [0-9a-f]* 3eaf2b54" "$D/pair-oracle.txt" || echo "WARNING: the turn script did not engage (no in .. 3eaf2b54)"
  else grep -q "^P 1400 .*c 3eaf2b54" "$D/pair-oracle.txt" || echo "WARNING: the turn script did not engage (P 1400 has no c 3eaf2b54)"; fi
fi
