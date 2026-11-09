using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // Drives the original rider skeleton from per-frame bone world matrices produced by the
    // game's own animation code (supplied by the native plugin later). Until pose data is
    // applied, the capsule proxy stays visible and the skinned mesh stays hidden.
    //
    // Recovered runtime convention (see _re/trace/rider_skinning_check.py and _re/formats/RIDER_FORMAT.md):
    //   all matrices are row-major D3D 4x4, row vectors (v * M), 16 floats per bone, translation in 12..14.
    //   world[i]   = local[i] * world[parent[i]]   (node 69 = entity root = bike body-0 render pose)
    //   palette[i] = inverseBind[i] * world[i]     (what a skinning vertex shader consumes)
    //   skinned v  = sum_k w_k * (v_bind * palette[bone_k])
    // The rider built at run time by Content/ReflexRiderModel (from the player's install) uses bones[i] == hierarchy node i (i = 0..68) and
    // bindposes == inverse(bind world[i]) in Unity's convention (= transpose(inverseBind[i])), so setting the bone transforms to world[i] reproduces the game's skinning.
    public sealed class OriginalRider : MonoBehaviour
    {
        public const int OriginalBoneCount = 69;

        public SkinnedMeshRenderer skinnedMesh;
        public Transform[] bones;
        public GameObject proxy;
        // All skinned part renderers (jersey, boots, chest protector ...). skinnedMesh stays the primary one.
        public SkinnedMeshRenderer[] skinnedMeshes;
        // Rigid parts attached to bone transforms (helmet on node 35, goggles on node 36).
        public Renderer[] rigidMeshes;
        // Hierarchy parents of MXRider_Hierarchy (node 0's parent is -1: it hangs off the entity root node 69).
        public int[] boneParents;
        // Bind-pose world matrices of MXRider_Hierarchy, OriginalBoneCount * 16 floats, row-major row-vector.
        public float[] bindWorldMatrices;

        // True once ApplyBoneMatrices has successfully applied a pose.
        public bool HasOriginalPose { get; private set; }
        // True when a mesh and bones are assigned so original poses can be shown at all.
        public bool CanUseOriginalPose => (skinnedMesh != null || (skinnedMeshes != null && skinnedMeshes.Length > 0)) && bones != null && bones.Length > 0;

        float[] scratchWorld;

        void OnEnable() { SetProxyVisible(true); }

        // matrices: boneCount * 16 floats, each a native-world-space bone transform, row-major (D3D),
        // translation in elements 12..14. Native axes equal Unity axes (left-handed), so no handedness flip.
        // This is exactly the game's WORLD buffer (RiderCharacter +0x22c) rows 0..68.
        public bool ApplyBoneMatrices(float[] matrices, int boneCount)
        {
            if (!CanUseOriginalPose || matrices == null || boneCount <= 0 || boneCount > bones.Length || matrices.Length < boneCount * 16) return false;
            for (int i = 0; i < boneCount; i++)
            {
                if (!bones[i]) continue;
                int o = i * 16;
                for (int k = 0; k < 16; k++) if ((k & 3) != 3 && (float.IsNaN(matrices[o + k]) || float.IsInfinity(matrices[o + k]))) return false;
            }
            for (int i = 0; i < boneCount; i++)
            {
                if (!bones[i]) continue;
                int o = i * 16;
                var right = new Vector3(matrices[o + 0], matrices[o + 1], matrices[o + 2]);
                var up = new Vector3(matrices[o + 4], matrices[o + 5], matrices[o + 6]);
                var fwd = new Vector3(matrices[o + 8], matrices[o + 9], matrices[o + 10]);
                var pos = new Vector3(matrices[o + 12], matrices[o + 13], matrices[o + 14]);
                if (fwd.sqrMagnitude < 1e-12f || up.sqrMagnitude < 1e-12f) continue;
                bones[i].SetPositionAndRotation(pos, Quaternion.LookRotation(fwd, up));
            }
            HasOriginalPose = true;
            SetProxyVisible(false);
            return true;
        }

        // palette: count * 16 floats, palette[i] = inverseBind[i] * world[i] (row-major, row vectors), i.e. the skinning matrix
        // the game's renderer forms from the node set's +0x234 (inverse bind) and +0x22c (world) buffers.
        // The bone world matrix is recovered as world[i] = bindWorld[i] * palette[i] (bindWorld * inverseBind == identity).
        public bool ApplySkinningPalette(float[] matrices, int count)
        {
            if (matrices == null || count <= 0 || count > OriginalBoneCount || matrices.Length < count * 16) return false;
            if (bindWorldMatrices == null || bindWorldMatrices.Length < OriginalBoneCount * 16) return false;
            EnsureScratch();
            for (int i = 0; i < count; i++) MultiplyRowMajor(bindWorldMatrices, i * 16, matrices, i * 16, scratchWorld, i * 16);
            return ApplyBoneMatrices(scratchWorld, count);
        }

        // local: count * 16 floats = the game's LOCAL buffer (RiderCharacter +0x228) rows 0..count-1, as written by RiderCharacter::vf19.
        // rootWorld: 16 floats, the entity root (node 69) matrix = bike body-0 render pose (row-major, row vectors).
        // Computes world[i] = local[i] * world[parent[i]] and applies it.
        public bool ApplyLocalPose(float[] local, int count, float[] rootWorld)
        {
            if (local == null || rootWorld == null || rootWorld.Length < 16 || count <= 0 || count > OriginalBoneCount || local.Length < count * 16) return false;
            if (boneParents == null || boneParents.Length < count) return false;
            EnsureScratch();
            for (int i = 0; i < count; i++)
            {
                int p = boneParents[i];
                if (p < 0) MultiplyRowMajor(local, i * 16, rootWorld, 0, scratchWorld, i * 16);
                else if (p < i) MultiplyRowMajor(local, i * 16, scratchWorld, p * 16, scratchWorld, i * 16);
                else return false;
            }
            return ApplyBoneMatrices(scratchWorld, count);
        }

        public void ClearOriginalPose() { HasOriginalPose = false; SetProxyVisible(true); }

        void EnsureScratch() { if (scratchWorld == null || scratchWorld.Length < OriginalBoneCount * 16) scratchWorld = new float[OriginalBoneCount * 16]; }

        // out = a * b for row-major 4x4 matrices stored in flat arrays (out may alias neither input row range in practice).
        static void MultiplyRowMajor(float[] a, int ao, float[] b, int bo, float[] result, int ro)
        {
            float r00 = 0, r01 = 0, r02 = 0, r03 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0, r20 = 0, r21 = 0, r22 = 0, r23 = 0, r30 = 0, r31 = 0, r32 = 0, r33 = 0;
            for (int k = 0; k < 4; k++)
            {
                float b0 = b[bo + k * 4], b1 = b[bo + k * 4 + 1], b2 = b[bo + k * 4 + 2], b3 = b[bo + k * 4 + 3];
                float a0 = a[ao + k], a1 = a[ao + 4 + k], a2 = a[ao + 8 + k], a3 = a[ao + 12 + k];
                r00 += a0 * b0; r01 += a0 * b1; r02 += a0 * b2; r03 += a0 * b3;
                r10 += a1 * b0; r11 += a1 * b1; r12 += a1 * b2; r13 += a1 * b3;
                r20 += a2 * b0; r21 += a2 * b1; r22 += a2 * b2; r23 += a2 * b3;
                r30 += a3 * b0; r31 += a3 * b1; r32 += a3 * b2; r33 += a3 * b3;
            }
            result[ro] = r00; result[ro + 1] = r01; result[ro + 2] = r02; result[ro + 3] = r03;
            result[ro + 4] = r10; result[ro + 5] = r11; result[ro + 6] = r12; result[ro + 7] = r13;
            result[ro + 8] = r20; result[ro + 9] = r21; result[ro + 10] = r22; result[ro + 11] = r23;
            result[ro + 12] = r30; result[ro + 13] = r31; result[ro + 14] = r32; result[ro + 15] = r33;
        }

        void SetProxyVisible(bool proxyVisible)
        {
            if (proxy) proxy.SetActive(proxyVisible);
            if (skinnedMesh) skinnedMesh.enabled = !proxyVisible;
            if (skinnedMeshes != null) foreach (var r in skinnedMeshes) if (r) r.enabled = !proxyVisible;
            if (rigidMeshes != null) foreach (var r in rigidMeshes) if (r) r.enabled = !proxyVisible;
        }
    }
}
