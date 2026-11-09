using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;

namespace Reflex.Port.OriginalWorldEditor
{
    // The rider is no longer a prefab with Mesh / Material / Texture assets under Assets/ReflexRecovered/Rider: Content/ReflexRiderModel
    // builds it at run time from the player's own Reflex install (the MXRider_Hierarchy skeleton, the Answer-red gear, the original
    // skin weights). This class only validates it.
    //
    // Matrix convention: the original data is row-major D3D (row vectors). A Unity (column-vector) matrix is the transpose,
    // so Unity bindpose[i] = inverse(bind world[i]) and bone.localToWorld = transpose(world[i]).
    public static class RiderAssetBuilder
    {
        // Headless validation: build the rider, drive it with the bind-pose palette (identity-equivalent) and with the
        // bind world matrices, and check that baked vertices equal the stored bind positions.
        [MenuItem("Reflex/Validate recovered rider")]
        public static void Validate()
        {
            var model = ReflexRiderModel.Build();
            try
            {
                var rider = model.rider;
                if (!rider.CanUseOriginalPose) throw new InvalidDataException("Rider has no renderers/bones.");
                var identityPalette = new float[OriginalRider.OriginalBoneCount * 16];
                for (int i = 0; i < OriginalRider.OriginalBoneCount; i++) { identityPalette[i * 16] = identityPalette[i * 16 + 5] = identityPalette[i * 16 + 10] = identityPalette[i * 16 + 15] = 1; }
                if (!rider.ApplySkinningPalette(identityPalette, OriginalRider.OriginalBoneCount)) throw new InvalidDataException("ApplySkinningPalette rejected the bind palette.");
                var bake = new Mesh();
                float worst = 0; int checkedVertices = 0;
                foreach (var smr in rider.skinnedMeshes)
                {
                    smr.BakeMesh(bake, true);
                    var baked = bake.vertices; var original = smr.sharedMesh.vertices;
                    for (int v = 0; v < original.Length; v++) { worst = Mathf.Max(worst, (baked[v] - original[v]).magnitude); checkedVertices++; }
                }
                UnityEngine.Object.DestroyImmediate(bake);
                Debug.Log("Reflex run-time rider validation: bind palette reproduces bind vertices, worst error " + worst.ToString("G4") + " over " + checkedVertices + " vertices");
                if (worst > 0.01f) throw new InvalidDataException("Bind pose is not reproduced: " + worst);
            }
            finally { model.Destroy(); ReflexBikeModel.ReleaseTextures(); }
        }

        // -executeMethod entry point for batch mode
        public static void BuildAndValidate() => Validate();
    }
}
