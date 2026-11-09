using System;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.RxCore.Tests
{
    public class RTBumpOwnershipTests
    {
        [Test]
        public void ForeignCallbacksCannotClearRenderingCameraBumpState()
        {
            var type = AppDomain.CurrentDomain.GetAssemblies()
                .Select(a => a.GetType("Reflex.Port.Rendering.ReflexRTBump"))
                .FirstOrDefault(t => t != null);
            Assert.IsNotNull(type);
            const BindingFlags flags = BindingFlags.Instance | BindingFlags.NonPublic;
            var begin = type.GetMethod("OnBegin", flags);
            var end = type.GetMethod("OnEnd", flags);
            var disable = type.GetMethod("OnDisable", flags);
            var renderingCamera = type.GetField("renderingCamera", flags);
            var activeOwner = type.GetField("activeOwner", BindingFlags.Static | BindingFlags.NonPublic);
            int id = Shader.PropertyToID("_RfxRTBumpParams");
            var saved = Shader.GetGlobalVector(id);
            var savedOwner = activeOwner.GetValue(null);
            var first = new GameObject("RT_Bump owner A"); first.SetActive(false);
            var second = new GameObject("RT_Bump owner B"); second.SetActive(false);
            try
            {
                var cameraA = first.AddComponent<Camera>();
                var cameraB = second.AddComponent<Camera>();
                var bumpA = first.AddComponent(type);
                var bumpB = second.AddComponent(type);
                type.GetField("targetCamera").SetValue(bumpA, cameraA);
                type.GetField("targetCamera").SetValue(bumpB, cameraB);
                foreach (var pair in new[] { (owner: bumpA, other: bumpB, camera: cameraA),
                                            (owner: bumpB, other: bumpA, camera: cameraB) })
                {
                    // Simulate the completed owner's GPU binding; no renderer/content allocation is needed.
                    var valid = new Vector4(1, 1, 0, 0);
                    activeOwner.SetValue(null, pair.owner);
                    renderingCamera.SetValue(pair.owner, pair.camera);
                    Shader.SetGlobalVector(id, valid);
                    begin.Invoke(pair.other, new object[] { default(ScriptableRenderContext), pair.camera });
                    Assert.AreEqual(valid, Shader.GetGlobalVector(id), "Later foreign begin must preserve RT_Bump");
                    end.Invoke(pair.other, new object[] { default(ScriptableRenderContext), pair.camera });
                    Assert.AreEqual(valid, Shader.GetGlobalVector(id), "Foreign end must preserve RT_Bump");
                    disable.Invoke(pair.other, null);
                    Assert.AreEqual(valid, Shader.GetGlobalVector(id), "Disabling another owner must preserve RT_Bump");
                    end.Invoke(pair.owner, new object[] { default(ScriptableRenderContext), pair.camera });
                    Assert.AreEqual(Vector4.zero, Shader.GetGlobalVector(id), "Owner end must restore other-camera fallback");
                    disable.Invoke(pair.owner, null);
                    Assert.AreEqual(Vector4.zero, Shader.GetGlobalVector(id));
                }
            }
            finally
            {
                UnityEngine.Object.DestroyImmediate(first);
                UnityEngine.Object.DestroyImmediate(second);
                activeOwner.SetValue(null, savedOwner);
                Shader.SetGlobalVector(id, saved);
            }
        }
    }
}
