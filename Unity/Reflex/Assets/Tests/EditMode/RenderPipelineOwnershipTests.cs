using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.RxCore.Tests
{
    public class RenderPipelineOwnershipTests
    {
        [Test]
        public void OtherPipelineCannotClearRawHdrStateOfRenderingCamera()
        {
            // Rendering lives in Assembly-CSharp. Inactive objects avoid loading content or creating a renderer:
            // this exercises the event ownership boundary, not GPU rendering or the original tone-map math.
            var type = AppDomain.CurrentDomain.GetAssemblies()
                .Select(a => a.GetType("Reflex.Port.Rendering.ReflexRenderPipeline"))
                .FirstOrDefault(t => t != null);
            Assert.IsNotNull(type);
            var begin = type.GetMethod("OnBegin", BindingFlags.Instance | BindingFlags.NonPublic);
            var end = type.GetMethod("OnEnd", BindingFlags.Instance | BindingFlags.NonPublic);
            var cameraField = type.GetField("cam", BindingFlags.Instance | BindingFlags.NonPublic);
            Assert.IsNotNull(begin); Assert.IsNotNull(end); Assert.IsNotNull(cameraField);
            int raw = Shader.PropertyToID("_RfxRawPipeline");
            float saved = Shader.GetGlobalFloat(raw);
            var first = new GameObject("HDR owner test A"); first.SetActive(false);
            var second = new GameObject("HDR owner test B"); second.SetActive(false);
            try
            {
                var cameraA = first.AddComponent<Camera>();
                var cameraB = second.AddComponent<Camera>();
                var pipelineA = first.AddComponent(type);
                var pipelineB = second.AddComponent(type);
                cameraField.SetValue(pipelineA, cameraA);
                cameraField.SetValue(pipelineB, cameraB);
                foreach (var pair in new[] { (owner: pipelineA, other: pipelineB, camera: cameraA),
                                            (owner: pipelineB, other: pipelineA, camera: cameraB) })
                {
                    // The owner's begin has set raw HDR; the later non-owner callback must leave it intact.
                    Shader.SetGlobalFloat(raw, 1f);
                    begin.Invoke(pair.other, new object[] { default(ScriptableRenderContext), pair.camera });
                    Assert.AreEqual(1f, Shader.GetGlobalFloat(raw), "Non-owner begin must preserve raw HDR");
                    end.Invoke(pair.other, new object[] { default(ScriptableRenderContext), pair.camera });
                    Assert.AreEqual(1f, Shader.GetGlobalFloat(raw), "Non-owner end must preserve raw HDR");
                    end.Invoke(pair.owner, new object[] { default(ScriptableRenderContext), pair.camera });
                    Assert.AreEqual(0f, Shader.GetGlobalFloat(raw), "Owner end must restore preview output");
                }
            }
            finally
            {
                UnityEngine.Object.DestroyImmediate(first);
                UnityEngine.Object.DestroyImmediate(second);
                Shader.SetGlobalFloat(raw, saved);
            }
        }
    }
}
