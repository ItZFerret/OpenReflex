// loop_anim.cpp -- oracle adapters for core/game/loop_anim.c (group "rx_loop"). LTCG register conventions (engine.md "Traps");
// the registers each one's callers read after it (plan/live.tsv) are set as the original leaves them.
#include "oracle.h"
#include "anim_oracle.h"
extern "C" {
#include "../game/loop_anim.h"
}
using namespace reflex_port;
static float xmm_lane0(reflex_vm::Machine& m, int i) { return rx_oracle::f32(uint32_t(m.xmm[i].lo)); }

// EAX = skeleton, EDX = pose; callers read ECX = the pose's end, EDX = the pose with its low byte cleared (xor dl, dl; only when
// there are bones), xmm1 (untouched)
ANIM_PORT(0x0082e250, SkeletonPose_Reset, "rx_loop", OUT_ECX | OUT_EDX) {
    const uint32_t skel = m.r[0], pose = m.r[2], n = uint32_t(m.mem.read(skel + 0x234, 4));
    rx_skeleton_pose_reset(skel, pose);
    m.r[1] = pose + 0x30 * n;
    if (n != 0) m.r[2] = pose & 0xffffff00u;
    m.r[0] = n != 0 ? pose + 0x30 * n + 0x18 : n;
    return m.ret(0);
}
// EDI = skeleton -> EAX = pose; callers read ECX (the reset's end pointer when a slot was free)
ANIM_PORT(0x0082e2d0, SkeletonPosePool_Acquire, "rx_loop", OUT_EAX) {
    m.r[0] = rx_skeleton_pose_acquire(m.r[7]);
    return m.ret(0);
}
// stack (skeleton, out); ECX = bone, EDX = pose, EDI = out vector -> EAX = EDI; ret 8
ANIM_PORT(0x0082efb0, SkeletonPose_BoneToModel, "rx_loop", OUT_EAX) {
    m.r[0] = rx_skeleton_bone_to_model(m.arg(0), m.arg(1), m.r[1], m.r[2], m.r[7]);
    return m.ret(8);
}
// ECX = key set, xmm0 = time, stack out; ret 4; callers read ECX (the set) and EDX (out)
ANIM_PORT(0x0082b930, AnimKeys_Locate, "rx_loop", OUT_ECX | OUT_EDX) {
    const uint32_t out = m.arg(0);
    rx_animkeys_locate(m.r[1], out, xmm_lane0(m, 0));
    m.r[2] = out;
    return m.ret(4);
}
ANIM_PORT(0x0082b9e0, AnimKeys_SampleRotation, "rx_loop", 0) { rx_animkeys_sample_rotation(m.r[1], m.r[2], m.r[6]); return m.ret(0); }
// stack key set; ECX = track, EDX = out, EDI = located time; ret 4; callers read ECX: the track (static key) or, animated, the
// first key's address
ANIM_PORT(0x0082bac0, AnimKeys_SampleTranslation, "rx_loop", OUT_ECX) {
    const uint32_t ks = m.arg(0), track = m.r[1];
    rx_animkeys_sample_translation(ks, track, m.r[2], m.r[7]);
    if (track == m.mem.read(ks + 4, 4)) {
        const float f = float(int32_t(m.mem.read(ks + 0xc, 4))) * rx_oracle::f32(uint32_t(m.mem.read(m.r[7], 4)));
        m.r[1] = uint32_t(m.mem.read(ks + 0x3c, 4)) + 16u * uint32_t(rx_cvtt(f));
    }
    return m.ret(4);
}
ANIM_PORT(0x0082ff20, Skeleton_Animate, "rx_loop", 0) { rx_skeleton_animate(m.r[1], m.r[7]); return m.ret(0); }
// stack bone map; ECX = pose, ESI = key set, xmm0 = time; ret 4; callers read EAX / EDX (the last translation key's address
// arithmetic: EAX = the bone's byte index, EDX = 3 x index x 16)
ANIM_PORT(0x00840040, Motion_Sample, "rx_loop", OUT_EAX | OUT_EDX) {
    const uint32_t map = m.arg(0), ks = m.r[6];
    rx_motion_sample(map, m.r[1], ks, xmm_lane0(m, 0));
    m.r[0] = uint32_t(m.mem.read(map + uint32_t(m.mem.read(ks + 4, 4)), 1));
    m.r[2] = m.r[0] * 0x30;
    return m.ret(4);
}
// stack skeleton; ECX = bone, EAX = pose, EDI = out; ret 4 (live.tsv's ECX / xmm1: the callers reload both)
ANIM_PORT(0x0082e610, SkeletonPose_BoneToModelScalar, "rx_loop", 0) {
    rx_skeleton_bone_to_model_scalar(m.arg(0), m.r[1], m.r[0], m.r[7]);
    return m.ret(4);
}
// 0061cdb0 (worker index): blocks at 0061cddd (WaitForSingleObject(go)) with ESP = S - 0xc (S = ((entry - 4) & ~15) - 0xa0),
// EBP = entry - 4, EBX = the WaitForSingleObject pointer, ESI = the head node when a job ran (else the index), EDI = the job's
// skeleton when a job ran (else unchanged). A quit returns 0 (registers as on entry).
RX_PORT_RAW(0x0061cdb0, RiderAnimation_WorkerThread, "rx_loop", OUT_EAX) {
    AnimBind ab_(m);
    const uint32_t entry = m.r[4], i = m.arg(0), r3 = m.r[3], r5 = m.r[5], r6 = m.r[6], r7 = m.r[7];
    const uint32_t job = uint32_t(m.mem.read(0x00a9408c + 4 * i, 4));
    const bool ran = job && m.mem.read(job + 0x5bb5c, 4) != 0;
    const uint32_t skel = ran ? uint32_t(m.mem.read(job + 0x144, 4)) : r7;
    uint32_t quit = 0;
    const uint32_t node = rx_rider_anim_worker_run(i, &quit);
    if (quit) { m.r[3] = r3; m.r[5] = r5; m.r[6] = r6; m.r[7] = r7; m.r[0] = 0; return m.ret(0); }
    const uint32_t S = ((entry - 4) & ~15u) - 0xa0, go = uint32_t(m.mem.read(0x00af6c08 + 4 * i, 4));
    m.r[5] = entry - 4; m.r[3] = uint32_t(m.mem.read(0x00916008, 4));
    m.r[6] = ran ? node : i; m.r[7] = skel;
    m.mem.write(S - 0xc, 4, 0x0061cddfu); m.mem.write(S - 0x8, 4, go); m.mem.write(S - 0x4, 4, 0xffffffffu);
    m.r[4] = S - 0xc; m.r[0] = i; m.r[1] = go;   /* EAX = the index, ECX = the go event (the push operands) */
    return 0;
}
