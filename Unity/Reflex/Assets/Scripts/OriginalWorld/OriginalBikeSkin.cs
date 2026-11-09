using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // A skinned surface of a bike mod's chassis (surface flags 0x40003: per-vertex blend indices into the surface's bone palette, UBYTE4N
    // weights) deformed over the nodes its vertices are weighted to, as the game's skinned draw does: each vertex is the weighted sum of
    // its bones' liveWorld(node) * inverse(bindWorld(node)). The mods put cables, brake lines, the rear shock and fork boots in such
    // surfaces and bind the surface to any node (VenusF: frame, swingarm and fork vertices in a surface bound to OBJ_UPR_Forks), so the
    // rigid placement by the binding node that the original bikes' skinned fenders get would steer the rear shock with the bars.
    //
    // The bones are empty transforms beside the parts (ReflexBikeModel.Build), posed by OriginalBikePose like every part (and recorded /
    // played back by the replay with them); the mesh is model space at the bind pose. Each LateUpdate (after the pose, the replay's
    // playback and the presentation) the vertices are re-blended in this part's own space when any bone moved against it.
    [DisallowMultipleComponent, DefaultExecutionOrder(31000)]
    public sealed class OriginalBikeSkin : MonoBehaviour
    {
        Transform[] bones;
        int[] index;          // 4 per vertex: bone slot (-1 none)
        float[] weight;       // 4 per vertex, normalised
        Vector3[] basePos, baseNrm, pos, nrm;
        Vector4[] baseTan, tan;
        Mesh mesh;
        Matrix4x4[] live, last;

        /// The bone slots used by the mesh's vertices (bones[k]) and per vertex up to four (slot, weight) pairs; the mesh holds the bind pose.
        public void Bind(Mesh m, Transform[] boneTransforms, int[] slots, float[] weights)
        {
            mesh = m; bones = boneTransforms; index = slots; weight = weights;
            basePos = m.vertices; baseNrm = m.normals; var t = m.tangents; baseTan = t != null && t.Length == basePos.Length ? t : null;
            pos = new Vector3[basePos.Length]; nrm = new Vector3[basePos.Length]; tan = baseTan != null ? new Vector4[basePos.Length] : null;
            live = new Matrix4x4[bones.Length]; last = new Matrix4x4[bones.Length];
            for (int i = 0; i < last.Length; i++) last[i] = Matrix4x4.identity;   // the bind pose: the mesh as built
            for (int v = 0; v < basePos.Length; v++)
            {
                float s = 0; for (int k = 0; k < 4; k++) if (index[v * 4 + k] >= 0) s += weight[v * 4 + k];
                if (s > 0) for (int k = 0; k < 4; k++) weight[v * 4 + k] /= s;
            }
            mesh.MarkDynamic();
        }

        public int BoneCount => bones?.Length ?? 0;
        public Transform Bone(int i) => bones[i];

        void LateUpdate() => Deform();

        /// Re-blends the vertices when a bone moved against this part (false: nothing changed).
        public bool Deform()
        {
            if (mesh == null || bones == null) return false;
            var inv = transform.worldToLocalMatrix;
            bool moved = false;
            for (int i = 0; i < bones.Length; i++)
            {
                live[i] = bones[i] ? inv * bones[i].localToWorldMatrix : last[i];
                if (!moved && live[i] != last[i]) moved = true;
            }
            if (!moved) return false;
            for (int i = 0; i < bones.Length; i++) last[i] = live[i];
            for (int v = 0; v < basePos.Length; v++)
            {
                int o = v * 4;
                Vector3 p = Vector3.zero, n = Vector3.zero, t = Vector3.zero, bp = basePos[v], bn = baseNrm[v], bt = baseTan != null ? (Vector3)baseTan[v] : Vector3.zero;
                bool any = false;
                for (int k = 0; k < 4; k++)
                {
                    int j = index[o + k]; if (j < 0) continue;
                    float w = weight[o + k]; any = true;
                    p += w * live[j].MultiplyPoint3x4(bp); n += w * live[j].MultiplyVector(bn);
                    if (tan != null) t += w * live[j].MultiplyVector(bt);
                }
                if (!any) { p = bp; n = bn; t = bt; }   // (a vertex with no weight stays as built)
                pos[v] = p;
                float l = n.sqrMagnitude; nrm[v] = l > 1e-12f ? n / Mathf.Sqrt(l) : bn;
                if (tan != null) { float tl = t.sqrMagnitude; if (tl > 1e-12f) t /= Mathf.Sqrt(tl); tan[v] = new Vector4(t.x, t.y, t.z, baseTan[v].w); }
            }
            mesh.SetVertices(pos); mesh.SetNormals(nrm); if (tan != null) mesh.SetTangents(tan);
            mesh.RecalculateBounds();
            return true;
        }
    }
}
