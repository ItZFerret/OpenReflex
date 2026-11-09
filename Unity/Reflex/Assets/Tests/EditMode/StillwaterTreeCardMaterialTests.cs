using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class StillwaterTreeCardMaterialTests
    {
        static Type Find(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        [TestCase("NAT_ST_BB_Trees_01")]
        [TestCase("NAT_ST_BB_Trees_02")]
        public void OriginalTreeCardUsesTextureAlphaAndLiteralNormalStrength(string program)
        {
            var content = Find("Reflex.Port.Content.ReflexLevelContent");
            var lmType = content.GetNestedType("LevelMaterial", BindingFlags.NonPublic);
            var lm = Activator.CreateInstance(lmType, true);
            lmType.GetField("name").SetValue(lm, program);
            lmType.GetField("pixelShader").SetValue(lm, program);
            lmType.GetField("alphaTest").SetValue(lm, true);
            lmType.GetField("alphaFunc").SetValue(lm, 5);
            lmType.GetField("alphaRef").SetValue(lm, 75);
            var names = (List<string>)lmType.GetField("textures").GetValue(lm);
            var states = (List<Dictionary<int,float>>)lmType.GetField("samplerStates").GetValue(lm);
            lmType.GetField("userSamplers").SetValue(lm, new[]{"s_sampler0","s_sampler1"});
            var textures = new Dictionary<string,Texture2D>(); Material material = null;
            try
            {
                foreach (var name in new[]{"tree-diffuse-alpha","tree-normal"})
                { names.Add(name); states.Add(new Dictionary<int,float>()); textures[name] = new Texture2D(1,1); }
                var shadersType = content.GetNestedType("SceneryShaders");
                var shaders = Activator.CreateInstance(shadersType);
                var shader = Shader.Find("Reflex/Original Scenery"); Assert.IsNotNull(shader);
                shadersType.GetField("scenery").SetValue(shaders, shader);
                Func<string,Dictionary<int,float>,Texture2D> load = (name, state) => textures[name];
                material = (Material)Find("Reflex.Port.Content.ReflexSceneryMaterial").GetMethod("Create", BindingFlags.Public|BindingFlags.Static)
                    .Invoke(null, new object[]{null,lm,shaders,load,null});
                Assert.AreSame(textures["tree-diffuse-alpha"], material.GetTexture("_BaseMap"));
                Assert.AreSame(textures["tree-normal"], material.GetTexture("_NormalMap"));
                Assert.AreEqual(1, material.GetFloat("_AlphaSource"));
                Assert.AreEqual(1, material.GetFloat("_AlphaTest"));
                Assert.AreEqual(75f / 255f, material.GetFloat("_Cutoff"));
                Assert.AreEqual(0, material.GetFloat("_AlphaFuncGE"), "Original GREATER rejects alpha75 and below");
                Assert.AreEqual(1, material.GetFloat("_NormalMode"));
                Assert.AreEqual(3, material.GetFloat("_NormalAmount"));
                Assert.AreEqual(1, material.GetFloat("_AmbientSrc"));
                Assert.AreEqual(0, material.GetFloat("_UseDynShadow"));
                Assert.AreEqual(0, material.GetFloat("_UseVolShadow"));
                Assert.AreEqual(0, material.GetFloat("_ShadowLit"));
                Assert.AreEqual(0, material.GetFloat("_TwoSided"));
            }
            finally { if(material) UnityEngine.Object.DestroyImmediate(material); foreach(var t in textures.Values) UnityEngine.Object.DestroyImmediate(t); }
        }
    }
}
