using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
namespace Reflex.Port.Content
{
    // Move the source sky domes with the final view, including replay/photo cameras.
    public sealed class ExperimentalMtxView : MonoBehaviour
    {
        public readonly List<Transform> skies = new List<Transform>();
        void OnEnable() => RenderPipelineManager.beginCameraRendering += BeforeCamera;
        void OnDisable() => RenderPipelineManager.beginCameraRendering -= BeforeCamera;
        void BeforeCamera(ScriptableRenderContext context, Camera camera)
        {
            if(!camera || camera.cameraType != CameraType.Game) return;
            foreach(var sky in skies) if(sky) sky.position=camera.transform.position;
        }
    }
}
