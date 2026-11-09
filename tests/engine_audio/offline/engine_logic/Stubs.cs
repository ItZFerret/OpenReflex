// Minimal stand-ins so Unity/Reflex/Assets/Scripts/OriginalWorld/OriginalEngineAudio.cs (the original engine's state machine) runs as a
// plain .NET program: the UnityEngine maths, the content loaders (the state table is injected instead) and a recording OriginalAudio.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace UnityEngine
{
    public struct Vector3
    {
        public float x, y, z;
        public Vector3(float x, float y, float z) { this.x = x; this.y = y; this.z = z; }
        public static Vector3 zero => new Vector3(0, 0, 0);
    }
    public static class Mathf
    {
        public static float Clamp(float v, float a, float b) => v < a ? a : v > b ? b : v;
        public static float Clamp01(float v) => v < 0 ? 0 : v > 1 ? 1 : v;
        public static float Max(float a, float b) => a > b ? a : b;
        public static float Min(float a, float b) => a < b ? a : b;
        public static float Sqrt(float v) => (float)System.Math.Sqrt(v);
        public static float Exp(float v) => (float)System.Math.Exp(v);
        public static float Abs(float v) => v < 0 ? -v : v;
    }
    public static class Debug { public static void LogWarning(object o) { Console.Error.WriteLine("warn: " + o); } public static void Log(object o) { } }
}

namespace Reflex.Port.Content
{
    public sealed class BxNode
    {
        public string tag; public object value; public Dictionary<string, object> attributes = new Dictionary<string, object>(); public List<BxNode> children = new List<BxNode>();
        public string S(string n) => null;
        public IEnumerable<BxNode> All() { yield break; }
    }
    public static class Rfx { public static BxNode Bxml(object o) => new BxNode(); }
    public sealed class PackageStub { public object Read(string a, string b, string c, string d) => null; }
    public static class ReflexInstall
    {
        public static string Root = ".";
        public static PackageStub Packages = new PackageStub();
        public static string PathOf(string name) => System.IO.Path.Combine(Root, name);
    }
}

namespace Reflex.Port.OriginalWorld
{
    /// A recording stand-in for OriginalAudio: every host command is passed to Sink (the harness decides what plays and when it ends).
    public sealed class OriginalAudio
    {
        public const uint HostWrapperBase = 0xF0000000u;
        public event Action<uint, uint> HostFinished;
        public Action<int, uint, uint, string, string, float, UnityEngine.Vector3, bool> Sink;
        public UnityEngine.Vector3 Listener;
        public static implicit operator bool(OriginalAudio a) => !ReferenceEquals(a, null);
        public void Host(int sub, uint wrapper, uint serial, string path, string param, float value, UnityEngine.Vector3 position, bool hasPosition) => Sink?.Invoke(sub, wrapper, serial, path, param, value, position, hasPosition);
        public void Finish(uint wrapper, uint serial) => HostFinished?.Invoke(wrapper, serial);
        public bool TryEventParameterRange(string path, string name, out float min, out float max) { min = max = 0; return false; }
    }
}
