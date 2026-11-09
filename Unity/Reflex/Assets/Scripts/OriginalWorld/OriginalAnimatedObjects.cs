using System;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // Scenery objects the original game moves at run time (e.g. the national start gate, DropGate,
    // _re/notes/start-gate.md): each is located once in the running original world by its vtable and
    // entity id, then its world matrix (+matrixOffset, row vectors, native axes = Unity axes) is read
    // every frame on the simulation thread and applied to the exported Unity object.
    [DisallowMultipleComponent]
    public sealed class OriginalAnimatedObjects : MonoBehaviour
    {
        [Serializable]
        public struct Entry
        {
            public Transform target;
            public uint vtable;          // e.g. DropGate 0x95fc24
            public uint idOffset;        // dword identifying the instance (entity id at +0x14)
            public uint id;
            public uint matrixOffset;    // world matrix (DropGate +0x30)
        }

        public Entry[] entries = new Entry[0];

        // matrices: entries.Length * 16 floats; valid[i] false when the object was not found.
        public void Apply(float[] matrices, bool[] valid)
        {
            if (matrices == null || valid == null) return;
            for (int i = 0; i < entries.Length && i < valid.Length; i++)
            {
                if (!valid[i] || !entries[i].target) continue;
                int o = i * 16;
                var x = new Vector3(matrices[o], matrices[o + 1], matrices[o + 2]);
                var y = new Vector3(matrices[o + 4], matrices[o + 5], matrices[o + 6]);
                var z = new Vector3(matrices[o + 8], matrices[o + 9], matrices[o + 10]);
                var p = new Vector3(matrices[o + 12], matrices[o + 13], matrices[o + 14]);
                if (!(z.sqrMagnitude > 1e-10f) || !(y.sqrMagnitude > 1e-10f) || float.IsNaN(p.x) || float.IsInfinity(p.x)) continue;
                entries[i].target.SetPositionAndRotation(p, Quaternion.LookRotation(z.normalized, y.normalized));
            }
        }
    }
}
