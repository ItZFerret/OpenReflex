using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The Gate Selection page's 3D markers (GateSelection_ShowGates 0073f8a0 / SetCameraToGate 0073ed70 / FUN_00740320 EnableGate, _re/notes/in-race-pages.md):
    // a numbered card over every start gate (the level's UI_StartCard_NN entities, one shared model with a material per number), an "X" card in front
    // of the gates the riders ahead took (UI_StartCard_X_NN) and the arrow over the focused gate (UI_StartCard_Arrow). The level builds them
    // (ReflexGateCards.cs) where the scene stores them; the game hides them (+0xa2 = 0) until the page shows them, so they start hidden here and
    // go where the page's code puts them (row-vector matrices: right, up (0, 1, 0), forward = the gate node's forward, position).
    public sealed class OriginalGateCards
    {
        readonly Transform[] cards = new Transform[16], xs = new Transform[16];
        Transform arrow;
        public int CardCount => cards.Count(c => c);
        public bool HasArrow => arrow;
        /// Its transforms still exist (a level loaded again makes new ones: the page then finds the new set).
        public bool Alive => arrow || cards.Any(c => c);

        /// Finds the level's card entities and hides them. Null when the level has none.
        public static OriginalGateCards Attach(OriginalLevel level)
        {
            if (!level || level.Content == null) return null;
            OriginalGateCards g = null;
            foreach (var i in level.Content.instances)
            {
                if (i.entityType != ReflexLevelContent.GateCardType || !i.transform) continue;
                g ??= new OriginalGateCards();
                string n = i.name ?? "";
                if (n.StartsWith("UI_StartCard_X_", StringComparison.Ordinal) && int.TryParse(n.Substring(15), out int xi) && xi >= 1 && xi <= 16) g.xs[xi - 1] = i.transform;
                else if (n == "UI_StartCard_Arrow") g.arrow = i.transform;
                else if (n.StartsWith("UI_StartCard_", StringComparison.Ordinal) && int.TryParse(n.Substring(13), out int ci) && ci >= 1 && ci <= 16) g.cards[ci - 1] = i.transform;
                i.transform.gameObject.SetActive(false);
            }
            return g;
        }

        // the page's matrix: right = up x forward... as Vec3_Cross( forward, (0, 1, 0) ) hands it, up (0, 1, 0), forward the normalised direction
        static void Place(Transform t, Vector3 position, Vector3 forward)
        {
            if (!t) return;
            if (forward.sqrMagnitude < 1e-12f) forward = Vector3.forward;
            t.SetPositionAndRotation(position, Quaternion.LookRotation(forward.normalized, Vector3.up));
            t.localScale = Vector3.one;
        }

        /// ShowGates + EnableGate: every card above its gate (5 up), the X card in front of the gates that are not available, the arrow hidden until
        /// Focus. position / forward: the gate node's world frame (rx_start_gates).
        public void Show(RxStartGatesView gates, ICollection<int> available)
        {
            for (int i = 0; i < 16; i++)
            {
                if (!gates.Present(i)) { if (cards[i]) cards[i].gameObject.SetActive(false); if (xs[i]) xs[i].gameObject.SetActive(false); continue; }
                Vector3 p = gates.Position(i), d = gates.Forward(i);
                if (cards[i]) { Place(cards[i], p + Vector3.up * 5f, d); cards[i].gameObject.SetActive(true); }
                if (xs[i])
                {
                    // 0073f8a0: the X stands 0.25 along the direction and 5 up; it faces (the direction x 10.25, 5 up)
                    Vector3 xp = p + d * 0.25f + Vector3.up * 5f;
                    Place(xs[i], xp, new Vector3(d.x * 10.25f, 5f + d.y * 10.25f, d.z * 10.25f));
                    xs[i].gameObject.SetActive(available == null || !available.Contains(i));
                }
            }
            if (arrow) arrow.gameObject.SetActive(false);
        }

        /// SetCameraToGate's arrow: on the focused gate, half a foot up.
        public void Focus(RxStartGatesView gates, int gate)
        {
            if (!arrow) return;
            if (!gates.Present(gate)) { arrow.gameObject.SetActive(false); return; }
            Place(arrow, gates.Position(gate) + Vector3.up * 0.5f, gates.Forward(gate));
            arrow.gameObject.SetActive(true);
        }

        public void Hide()
        {
            foreach (var t in cards) if (t) t.gameObject.SetActive(false);
            foreach (var t in xs) if (t) t.gameObject.SetActive(false);
            if (arrow) arrow.gameObject.SetActive(false);
        }
    }

    /// A copy of rx_start_gates for the managed side.
    public sealed class RxStartGatesView
    {
        public readonly bool Valid, HasCamera;
        public readonly float Fov, NearPlane, FarPlane;
        public readonly Matrix4x4 GateMatrix;
        readonly uint mask;
        readonly Vector3[] localPosition = new Vector3[16], localForward = new Vector3[16], position = new Vector3[16], forward = new Vector3[16];
        public bool Present(int gate) => gate >= 0 && gate < 16 && (mask & (1u << gate)) != 0;
        public Vector3 LocalPosition(int g) => localPosition[g];
        public Vector3 LocalForward(int g) => localForward[g];
        public Vector3 Position(int g) => position[g];
        public Vector3 Forward(int g) => forward[g];

        public unsafe RxStartGatesView(RxCore.RxStartGates r)
        {
            Valid = r.valid != 0; HasCamera = r.has_camera != 0; Fov = r.fov; NearPlane = r.near_plane; FarPlane = r.far_plane; mask = r.present_mask;
            var m = new Matrix4x4();
            for (int k = 0; k < 16; k++) m[k / 4, k % 4] = r.gate_matrix[k];   // (row, column): the D3D rows are the first index
            GateMatrix = m;
            for (int i = 0; i < 16; i++)
            {
                float* g = r.gates + 12 * i;
                localPosition[i] = new Vector3(g[0], g[1], g[2]); localForward[i] = new Vector3(g[3], g[4], g[5]);
                position[i] = new Vector3(g[6], g[7], g[8]); forward[i] = new Vector3(g[9], g[10], g[11]);
            }
        }

        /// The game's row-vector transform of a point by the gate matrix: x * row0 + y * row1 + z * row2 + row3.
        public Vector3 Place(Vector3 p)
        {
            var m = GateMatrix;   // m[row, column] with the D3D rows as rows
            return new Vector3(
                p.x * m[0, 0] + p.y * m[1, 0] + p.z * m[2, 0] + m[3, 0],
                p.x * m[0, 1] + p.y * m[1, 1] + p.z * m[2, 1] + m[3, 1],
                p.x * m[0, 2] + p.y * m[1, 2] + p.z * m[2, 2] + m[3, 2]);
        }
    }
}
