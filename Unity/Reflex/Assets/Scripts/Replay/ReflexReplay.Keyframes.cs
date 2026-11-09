// The replay's KEYFRAMES camera (ReflexReplay, after Open Unleashed's MxaReplay.Keyframes): camera keys set along the replay's
// timeline, flown through as it plays.
//   ADD     K / R3 (the UI's ADD KEY) at the moment shown, from the camera's view now (any camera: fly the FREE camera to the shot, or
//           take the director's), position, direction and lens; a key at the same moment (within 0.1 s) is replaced
//   DELETE  Shift+K / Delete / R3 on a key (the UI's DELETE KEY): the key at the moment shown (within 0.25 s)
//   CLEAR   the UI's CLEAR: every key
//   AIM     VIEW flies the keys' own directions; RIDER keeps the keys' path and aims at the followed rider
// Between keys the eye follows a smooth curve (Catmull-Rom, its tangents scaled by the keys' times: no overshoot on uneven spacing),
// the direction and the lens ease between the two keys around the moment; before the first key / after the last it holds that key.
// The keys live in the replay's clock: a key that falls off the start of the recording goes with it; a new race clears them.
using System.Collections.Generic;
using UnityEngine;

namespace Reflex.Port.Replay
{
    public sealed partial class ReflexReplay
    {
        public struct CamKey { public float t; public Vector3 pos; public Quaternion rot; public float fov; }

        readonly List<CamKey> keys = new List<CamKey>();
        public IReadOnlyList<CamKey> Keys => keys;
        /// AIM: false = each key's own direction, true = always at the followed rider.
        public bool KeyAimRider { get; set; }
        /// The last key action, for the UI's note ("KEY 3 ADDED" ...); cleared by the UI.
        public string KeyNote { get; set; }

        /// The key at moment t (within tol seconds), -1 when none.
        public int KeyAt(float t, float tol)
        {
            int best = -1; float bd = tol;
            for (int i = 0; i < keys.Count; i++) { float d = Mathf.Abs(keys[i].t - t); if (d <= bd) { bd = d; best = i; } }
            return best;
        }

        /// A key at the moment shown from the camera's view now (replacing one within 0.1 s). Returns its index.
        public int AddKey()
        {
            if (!camera) return -1;
            var k = new CamKey { t = PlayTime, pos = camera.transform.position, rot = camera.transform.rotation, fov = camera.fieldOfView };
            int at = KeyAt(PlayTime, 0.1f);
            if (at >= 0) { keys[at] = k; KeyNote = $"KEY {at + 1} REPLACED"; return at; }
            int i = 0; while (i < keys.Count && keys[i].t < k.t) i++;
            keys.Insert(i, k);
            KeyNote = keys.Count == 1 ? "KEY 1 ADDED  ·  ADD ANOTHER AT A LATER MOMENT" : $"KEY {i + 1} OF {keys.Count} ADDED  ·  CAMERA: KEYFRAMES TO PLAY";
            return i;
        }

        /// Deletes the key at the moment shown (within 0.25 s). False when there is none.
        public bool DeleteKey()
        {
            int at = KeyAt(PlayTime, 0.25f);
            if (at < 0) { KeyNote = "NO KEY HERE"; return false; }
            keys.RemoveAt(at);
            KeyNote = $"KEY {at + 1} DELETED";
            return true;
        }

        /// R3: deletes the key here, else adds one.
        public void ToggleKey() { if (KeyAt(PlayTime, 0.25f) >= 0) DeleteKey(); else AddKey(); }

        public void ClearKeys() { if (keys.Count > 0) KeyNote = "KEYS CLEARED"; keys.Clear(); }

        /// (tests) the free camera at pos looking at look.
        internal void FreeCameraAt(Vector3 pos, Vector3 look)
        {
            SetCamera(Cam.Free);
            freePos = pos;
            var e = Quaternion.LookRotation(look - pos, Vector3.up).eulerAngles;
            freeYaw = e.y; freePitch = e.x > 180f ? e.x - 360f : e.x;
            CameraCut();
        }

        /// The keys outside the recording (its start moved on) go.
        void PruneKeys()
        {
            if (Buffer.Count == 0) return;
            keys.RemoveAll(k => k.t < Buffer.StartTime - 0.01f || k.t > Buffer.EndTime + 0.01f);
        }

        /// The keyframed camera at moment t: the eye on the curve, the direction and lens eased between the keys around it.
        bool KeyframeView(float t, Vector3 subject, out Vector3 eye, out Quaternion rot, out float fov)
        {
            eye = default; rot = Quaternion.identity; fov = 55f;
            int n = keys.Count;
            if (n == 0) return false;
            if (n == 1 || t <= keys[0].t) { var k = keys[0]; eye = k.pos; rot = k.rot; fov = k.fov; }
            else if (t >= keys[n - 1].t) { var k = keys[n - 1]; eye = k.pos; rot = k.rot; fov = k.fov; }
            else
            {
                int i = 0; while (i < n - 2 && keys[i + 1].t < t) i++;
                CamKey a = keys[i], b = keys[i + 1];
                float h = Mathf.Max(1e-4f, b.t - a.t), u = Mathf.Clamp01((t - a.t) / h);
                // the tangents: central differences in time (the end keys: one-sided), times the segment's length
                Vector3 Tan(int j)
                {
                    int lo = Mathf.Max(0, j - 1), hi = Mathf.Min(n - 1, j + 1);
                    float dt = Mathf.Max(1e-4f, keys[hi].t - keys[lo].t);
                    return (keys[hi].pos - keys[lo].pos) / dt;
                }
                Vector3 m0 = Tan(i) * h, m1 = Tan(i + 1) * h;
                float u2 = u * u, u3 = u2 * u;
                eye = (2 * u3 - 3 * u2 + 1) * a.pos + (u3 - 2 * u2 + u) * m0 + (-2 * u3 + 3 * u2) * b.pos + (u3 - u2) * m1;
                float s = u * u * (3f - 2f * u);   // the direction and the lens: eased in and out of each key
                rot = Quaternion.Slerp(a.rot, b.rot, s);
                fov = Mathf.Lerp(a.fov, b.fov, s);
            }
            if (KeyAimRider && (subject - eye).sqrMagnitude > 0.01f) rot = Quaternion.LookRotation(subject - eye, Vector3.up);
            return true;
        }
    }
}
