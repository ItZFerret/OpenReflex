using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using NUnit.Framework;
using UnityEngine;

namespace Reflex.Port.RxCore.Tests
{
    public class RenneMaterialTests
    {
        static Type Find(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        [TestCase("NAT_RE_A_EOW")]
        [TestCase("NAT_RE_B_EOW")]
        public void OriginalAnonymousSamplerRolesDoNotDisplayOverlayAsColour(string program)
        {
            var content = Find("Reflex.Port.Content.ReflexLevelContent");
            var lmType = content.GetNestedType("LevelMaterial", BindingFlags.NonPublic);
            var lm = Activator.CreateInstance(lmType, true);
            lmType.GetField("name").SetValue(lm, program);
            lmType.GetField("pixelShader").SetValue(lm, program);
            var names = (List<string>)lmType.GetField("textures").GetValue(lm);
            var states = (List<Dictionary<int,float>>)lmType.GetField("samplerStates").GetValue(lm);
            lmType.GetField("userSamplers").SetValue(lm, new[]{"s_sampler0","s_sampler1","s_sampler2","s_sampler3"});
            var textures = new Dictionary<string,Texture2D>(); Material material = null;
            try
            {
                foreach (var name in new[]{"overlay-control","light-dirt","track-dirt","normal"})
                { names.Add(name); states.Add(new Dictionary<int,float>()); textures[name] = new Texture2D(1,1); }
                var shadersType = content.GetNestedType("SceneryShaders");
                var shaders = Activator.CreateInstance(shadersType);
                var shader = Shader.Find("Reflex/Original Scenery"); Assert.IsNotNull(shader);
                shadersType.GetField("scenery").SetValue(shaders, shader);
                Func<string,Dictionary<int,float>,Texture2D> load = (name, state) => textures[name];
                material = (Material)Find("Reflex.Port.Content.ReflexSceneryMaterial").GetMethod("Create", BindingFlags.Public|BindingFlags.Static)
                    .Invoke(null, new object[]{null,lm,shaders,load,null});
                Assert.AreEqual(14, material.GetFloat("_Mode"));
                Assert.AreSame(textures["overlay-control"], material.GetTexture("_ComboMap"));
                Assert.AreSame(textures["light-dirt"], material.GetTexture("_BaseMap"));
                Assert.AreSame(textures["track-dirt"], material.GetTexture("_DetailMap"));
                Assert.AreSame(textures["normal"], material.GetTexture("_NormalMap"));
                Assert.AreEqual(1,material.GetFloat("_AmbientSrc"));
                Assert.AreEqual(0,material.GetFloat("_UseDynShadow"));
                Assert.AreEqual(0,material.GetFloat("_UseVolShadow"));
            }
            finally { if(material) UnityEngine.Object.DestroyImmediate(material); foreach(var t in textures.Values) UnityEngine.Object.DestroyImmediate(t); }
        }
    }
}
