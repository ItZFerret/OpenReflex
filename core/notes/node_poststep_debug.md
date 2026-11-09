# Mechanism node visual-debug callbacks

00859b30, MainBodyNode vf4 00859e40, ConstraintSlidingRotor vf4 0085b180, ConstraintFixedBody vf4 0085bed0. Files node_poststep_debug.c/.h, oracle group rx_NodePoststepDebug. Hand-read original decompile and disassembly; no lifted source.

These are dormant debug evaluations, not force or constraint solving. All three callbacks gate on node+4; they call 00859b30 only while enabled. That helper transforms three runtime basis axes by the quaternion at [node+34]+90, scales by9322a4, and adds [node+30]+90; its784-byte body has no calls and all stores are ESP-relative inside its b0 frame. Its EAX=node, EDX=[node+34], x87 unchanged.

MainBody callback has no frame; disabled EAX/EDX pass through. SlidingRotor preserves its aligned ac/one-push frame and common-helper call. Its region0085b19f..0085b58f is exclusively SSE debug arithmetic; every write is to ESP+00..a0, no further calls occur, and no scratch pointer escapes. The decompiler eliminates that entire region. It is intentionally omitted, retaining EAX=[node+2c], EDX=[node+30] when enabled and incoming registers when disabled. This is a proof of no observable math effect, not a replacement for real simulation math.

FixedBody retains its9c/one-push frame, cookie at steadyESP+9c XOR ESP+4, common-helper call, offset evaluation and actual matrix-kernel calls. Matrix3x3 at ESP+50 uses vtable93ca04 and3x3 dimensions. Quaternion source [node+34]+90. The scaled basis vectors live at ESP+20; Mat3_MulVec outputs ESP+40, +30, +10. All kernels receive their original stack addresses and calling ESP. Cookie check occurs after POP ESI (ESP+4). Enabled outputs EAX=ESP+50, EDX=ESP+20; disabled EAX=cookie-mixed frame pointer, EDX incoming. All own code and called math use SSE; x87 remains unchanged.

Adapters declare OUT_EAX|OUT_EDX|OUT_FPU, with incoming register arguments for gated callbacks. Syntax-only C check passed; heavy build/oracle checks owned by lead. Shared vehicle_poststep_probe owner notified to cover actual node classes and direct positive/zero gates.


## Lead verification, 2026-10-21

Accepted reference evidence and current native boundary are recorded in `HANDOFF/SESSION_2026-10-21_VEHICLE_POSES.md` and `builds/sol_vehicle_pose`. The 600-frame UTV/SuperBuggy/ProLite/Pro2 comparisons match, including whole-memory hashes. Restored probes cover articulation and post-step paths absent from natural recordings; the fixed-body node is verified separately on the real MX rider node. Native playability is a separate acceptance and is not implied by these method comparisons.
