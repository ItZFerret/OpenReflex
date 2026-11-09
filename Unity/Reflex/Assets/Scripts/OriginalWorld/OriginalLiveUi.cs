using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The game's own in-game UI, mirrored live (native/drafts/world_runtime/modes.cpp, _re/notes/modes.md). For the game modes
    // (Freestyle, Section Race, the Free Ride challenges) the running game's UI page graphs run as in the shipping flow: after
    // reflex_world_ui_activate the InGamePageGraph routes to the activity's own HUD page ("Goto<Activity>SinglePlayer" from the
    // hub), and the activity's pages and popups follow by themselves (RulesPopup, EventIntro, Pre / PostChallengePopup, the
    // post-event pages), driven by the game's MXUI*Component classes and page Lua, with the keyboard the game reads (Enter = accept).
    // Each frame reflex_world_ui_frame reports what is shown: the current page of every visible graph, its layers in order, and
    // every component's state bits (Visible), text, colour, Position, scale, plus the composite animations started / stopped since
    // the last call. This component builds the same layers from the install's MXUI database (ReflexUiData / OriginalUiTree, by
    // the layer's own package) and applies that state; the animations play from the layers' own keyframes when the game starts
    // them. The port's own HUD (OriginalHud, National) stays off while this runs.
    // WP-3: the arcade races run it too (OriginalHud.LiveRace: SinglePlayerNational, SinglePlayerSX_NAT, SinglePlayerOmnicross,
    // SinglePlayerPowercross, SinglePlayerWaypoint, SinglePlayerFreeride with every component the game drives), keeping the port's
    // post-race pages; OriginalHud's hand-built National HUD is their fallback when this is Unavailable. The UI sounds the game's UI
    // plays (reflex_world_ui_sounds) go to OriginalHud.UiSound; Collect reuses its frames and strings (no garbage per frame).
    [DisallowMultipleComponent]
    public sealed class OriginalLiveUi : MonoBehaviour
    {
        // ---------------------------------------------------------------------------------------------- native
        const string Lib = ReflexWorldNative.Library;
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_activate__fn(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string route, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_activate__fn> reflex_world_ui_activate__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_activate__fn>("reflex_world_ui_activate");
        static int reflex_world_ui_activate(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string route, byte[] output, int size) => reflex_world_ui_activate__p.Get()(world, route, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_event__fn(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_event__fn> reflex_world_ui_event__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_event__fn>("reflex_world_ui_event");
        static int reflex_world_ui_event(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string name) => reflex_world_ui_event__p.Get()(world, name);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_frame__fn(IntPtr world, byte[] layers, uint maxLayers, byte[] nodes, uint maxNodes, byte[] anims, uint maxAnims, byte[] text, uint textSize, int[] counts);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_frame__fn> reflex_world_ui_frame__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_frame__fn>("reflex_world_ui_frame");
        static int reflex_world_ui_frame(IntPtr world, byte[] layers, uint maxLayers, byte[] nodes, uint maxNodes, byte[] anims, uint maxAnims, byte[] text, uint textSize, int[] counts) => reflex_world_ui_frame__p.Get()(world, layers, maxLayers, nodes, maxNodes, anims, maxAnims, text, textSize, counts);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_mode__fn(IntPtr world, byte[] state);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mode__fn> reflex_world_mode__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mode__fn>("reflex_world_mode");
        static int reflex_world_mode(IntPtr world, byte[] state) => reflex_world_mode__p.Get()(world, state);   // ReflexModeState, parsed below (no marshalled strings per frame)
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_requests__fn(IntPtr world, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_requests__fn> reflex_world_ui_requests__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_requests__fn>("reflex_world_ui_requests");
        static int reflex_world_ui_requests(IntPtr world, byte[] output, int size) => reflex_world_ui_requests__p.Get()(world, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_mode_objects__fn(IntPtr world, byte[] output, uint max);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mode_objects__fn> reflex_world_mode_objects__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_mode_objects__fn>("reflex_world_mode_objects");
        static int reflex_world_mode_objects(IntPtr world, byte[] output, uint max) => reflex_world_mode_objects__p.Get()(world, output, max);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_ui_sounds__fn(IntPtr world, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_sounds__fn> reflex_world_ui_sounds__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_ui_sounds__fn>("reflex_world_ui_sounds");
        static int reflex_world_ui_sounds(IntPtr world, byte[] output, int size) => reflex_world_ui_sounds__p.Get()(world, output, size);   // WP-3: UISound_Play / PlayIfIdle / Stop (OriginalHud.UiSound)
        const int ObjectSize = 104;   // ReflexModeObject: object, vtable, kind 8, entity id 12, drawn 16, hit 20, axis 24, reserved 28, uv 32, matrix 40
        const int ModeSize = 112;     // ReflexModeState: valid 0, kind 4, state 8, sub 12, activity 16, vtable 20, clock 24, countdown 28, rules 32, area 36, track 40, area 48, name 80

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct ReflexModeState
        {
            public int valid, kind, state, subState;
            public uint activity, vtable;
            public float clock, countdown;
            public int rulesHold, areaActive, areaTrack, reserved;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string area;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string name;
        }
        // reflex_world.h ReflexUiNode (parent 0, layer 4, object 8, vtable 12, state 16, position 20, scale 28, size 36, color 44, text 48,
        // name 52) / ReflexUiLayer (first 0, nodes 4, object 8, state 12, graph 16, page 48, name 96) / ReflexUiAnim (node 0, state 4, time 8,
        // length 12, name 16)
        const int NodeSize = 100, LayerSize = 144, AnimSize = 64;
        const int MaxLayers = 64, MaxNodes = 8192, MaxAnims = 512, MaxText = 1 << 18;

        public sealed class NodeSnap { public int parent; public uint obj, vtable, state; public Vector2 pos, scale; public uint color; public string text; public string name; }
        public sealed class LayerSnap { public string graph, page, name; public uint obj, state; public readonly List<NodeSnap> nodes = new List<NodeSnap>(); }
        public sealed class AnimSnap { public int layer, node, state; public float time, length; public string name; }
        public sealed class Frame { public readonly List<LayerSnap> layers = new List<LayerSnap>(); public readonly List<AnimSnap> anims = new List<AnimSnap>(); public ReflexModeState mode; public bool hasMode; public readonly List<string> requests = new List<string>();
            public readonly List<(int kind, int entityId, bool shown, int axis)> objects = new List<(int, int, bool, int)>();
            /// WP-3: the UI sounds the game played since the last frame ("play" / "loop" / "stop", UIRegistry name; OriginalHud.UiSound).
            public readonly List<(string kind, string name)> sounds = new List<(string, string)>();
            // the snapshots of an earlier use (Collect reuses frames: no garbage per frame)
            internal readonly List<LayerSnap> spareLayers = new List<LayerSnap>(); internal readonly List<NodeSnap> spareNodes = new List<NodeSnap>(); internal readonly List<AnimSnap> spareAnims = new List<AnimSnap>();
            internal void Clear()
            {
                foreach (var l in layers) { spareNodes.AddRange(l.nodes); l.nodes.Clear(); spareLayers.Add(l); }
                layers.Clear(); spareAnims.AddRange(anims); anims.Clear(); requests.Clear(); objects.Clear(); sounds.Clear(); hasMode = false; mode = default;
            }
            internal LayerSnap NewLayer() { int n = spareLayers.Count; if (n == 0) return new LayerSnap(); var l = spareLayers[n - 1]; spareLayers.RemoveAt(n - 1); return l; }
            internal NodeSnap NewNode() { int n = spareNodes.Count; if (n == 0) return new NodeSnap(); var x = spareNodes[n - 1]; spareNodes.RemoveAt(n - 1); return x; }
            internal AnimSnap NewAnim() { int n = spareAnims.Count; if (n == 0) return new AnimSnap(); var x = spareAnims[n - 1]; spareAnims.RemoveAt(n - 1); return x; }
        }

        sealed class Buffers
        {
            public byte[] layers = new byte[MaxLayers * LayerSize], nodes = new byte[MaxNodes * NodeSize], anims = new byte[MaxAnims * AnimSize], text = new byte[MaxText]; public int[] counts = new int[4];
            public int[] nodeLayer = new int[MaxNodes], nodeLocal = new int[MaxNodes];
            public byte[] scroll = new byte[8];   // WP-T: a UIScrollingTextComponent's crawl offset
            public byte[] mode = new byte[ModeSize], objects = new byte[64 * ObjectSize], requests = new byte[1024], sounds = new byte[2048];
            public readonly Stack<Frame> free = new Stack<Frame>();
            public bool soundsMissing;
        }
        static readonly Buffers buffers = new Buffers();

        // The strings of the snapshots, interned by their UTF-8 bytes (names, page names and texts repeat frame after frame): a frame that
        // changes nothing allocates nothing. Bounded (a race timer makes a new text every frame).
        static class Strings
        {
            sealed class Entry { public byte[] bytes; public string s; public Entry next; }
            static readonly Dictionary<uint, Entry> table = new Dictionary<uint, Entry>();
            public static string Get(byte[] b, int at, int n)
            {
                if (n <= 0) return "";
                uint h = 2166136261; for (int i = 0; i < n; i++) h = (h ^ b[at + i]) * 16777619;
                table.TryGetValue(h, out var head);
                for (var e = head; e != null; e = e.next)
                {
                    if (e.bytes.Length != n) continue;
                    int k = 0; while (k < n && e.bytes[k] == b[at + k]) k++;
                    if (k == n) return e.s;
                }
                if (table.Count > 8192) { table.Clear(); head = null; }
                var bytes = new byte[n]; Buffer.BlockCopy(b, at, bytes, 0, n);
                var ne = new Entry { bytes = bytes, s = Encoding.UTF8.GetString(b, at, n), next = head };
                table[h] = ne; return ne.s;
            }
        }
        static string CStr(byte[] b, int at, int max) { int n = 0; while (n < max && b[at + n] != 0) n++; return Strings.Get(b, at, n); }

        /// A frame Collect returned, back to the pool (its snapshots are reused).
        public static void Recycle(Frame f) { if (f == null) return; lock (buffers) { if (buffers.free.Count < 4) buffers.free.Push(f); } }

        /// Measured (WP-3): ui_frame + the snapshot parse on the simulation thread, the mirror's Apply + Tick on the main thread
        /// (milliseconds), totals since the last Reset. (Their GC: HudTests.Bench / the play test's A/B; Unity's Mono does not implement
        /// GC.GetAllocatedBytesForCurrentThread.)
        public static class Stats
        {
            public static double collectMs, applyMs, collectMax, applyMax; public static int collects, applies;
            public static void Reset() { collectMs = applyMs = collectMax = applyMax = 0; collects = applies = 0; }
        }

        /// One frame of the game's UI (on the thread that runs the world's frames, between them). `objects`: the challenges' collectables
        /// too (reflex_world_mode_objects scans the heap until it finds some: only for the events that have them).
        public static Frame Collect(IntPtr world, bool objects = true, bool sounds = false)
        {
            long t0 = System.Diagnostics.Stopwatch.GetTimestamp();
            var B = buffers; Frame f;
            lock (B)
            {
                f = B.free.Count > 0 ? B.free.Pop() : new Frame(); f.Clear();
                if (reflex_world_ui_frame(world, B.layers, MaxLayers, B.nodes, MaxNodes, B.anims, MaxAnims, B.text, MaxText, B.counts) != 0) return f;
                int nl = B.counts[0], nn = B.counts[1], na = B.counts[2], nt = B.counts[3];
                var nodeLayer = B.nodeLayer; var nodeLocal = B.nodeLocal;
                for (int l = 0; l < nl; l++)
                {
                    int o = l * LayerSize;
                    int first = BitConverter.ToInt32(B.layers, o), count = BitConverter.ToInt32(B.layers, o + 4);
                    var ls = f.NewLayer();
                    ls.obj = BitConverter.ToUInt32(B.layers, o + 8); ls.state = BitConverter.ToUInt32(B.layers, o + 12);
                    ls.graph = CStr(B.layers, o + 16, 32); ls.page = CStr(B.layers, o + 48, 48); ls.name = CStr(B.layers, o + 96, 48);
                    for (int i = first; i < first + count && i < nn; i++)
                    {
                        int q = i * NodeSize;
                        int parent = BitConverter.ToInt32(B.nodes, q), text = BitConverter.ToInt32(B.nodes, q + 48);
                        var n = f.NewNode();
                        n.parent = parent >= 0 ? parent - first : -1; n.obj = BitConverter.ToUInt32(B.nodes, q + 8); n.vtable = BitConverter.ToUInt32(B.nodes, q + 12); n.state = BitConverter.ToUInt32(B.nodes, q + 16);
                        n.pos = new Vector2(BitConverter.ToSingle(B.nodes, q + 20), BitConverter.ToSingle(B.nodes, q + 24));
                        // WP-T: a UIScrollingTextComponent (vtable 00950bc4) draws its text at its crawl offset (+0x390 x, +0x394 y; vf11 005cd0c0 eases
                        // it between +0x39c and +0x3a0 over TimeToScroll): PreTutorialPopup's description crawls up through its TextMaskContainer
                        if (n.vtable == 0x950bc4 && ReflexWorldNative.reflex_world_read(world, n.obj + 0x390, B.scroll, 8) == 0) n.pos += new Vector2(BitConverter.ToSingle(B.scroll, 0), BitConverter.ToSingle(B.scroll, 4));
                        n.scale = new Vector2(BitConverter.ToSingle(B.nodes, q + 28), BitConverter.ToSingle(B.nodes, q + 32));
                        n.color = BitConverter.ToUInt32(B.nodes, q + 44); n.text = text >= 0 && text < nt ? CStr(B.text, text, nt - text) : null; n.name = CStr(B.nodes, q + 52, 48);
                        ls.nodes.Add(n);
                        nodeLayer[i] = l; nodeLocal[i] = i - first;
                    }
                    f.layers.Add(ls);
                }
                for (int a = 0; a < na; a++)
                {
                    int o = a * AnimSize, node = BitConverter.ToInt32(B.anims, o);
                    if (node < 0 || node >= nn) continue;
                    var x = f.NewAnim();
                    x.layer = nodeLayer[node]; x.node = nodeLocal[node]; x.state = BitConverter.ToInt32(B.anims, o + 4); x.time = BitConverter.ToSingle(B.anims, o + 8); x.length = BitConverter.ToSingle(B.anims, o + 12); x.name = CStr(B.anims, o + 16, 48);
                    f.anims.Add(x);
                }
                var mb = B.mode;
                if (reflex_world_mode(world, mb) == 0)
                {
                    f.hasMode = true;
                    f.mode = new ReflexModeState
                    {
                        valid = BitConverter.ToInt32(mb, 0), kind = BitConverter.ToInt32(mb, 4), state = BitConverter.ToInt32(mb, 8), subState = BitConverter.ToInt32(mb, 12),
                        activity = BitConverter.ToUInt32(mb, 16), vtable = BitConverter.ToUInt32(mb, 20), clock = BitConverter.ToSingle(mb, 24), countdown = BitConverter.ToSingle(mb, 28),
                        rulesHold = BitConverter.ToInt32(mb, 32), areaActive = BitConverter.ToInt32(mb, 36), areaTrack = BitConverter.ToInt32(mb, 40), reserved = BitConverter.ToInt32(mb, 44),
                        area = CStr(mb, 48, 32), name = CStr(mb, 80, 32),
                    };
                }
                if (objects)
                {
                    var ob = B.objects; int on = reflex_world_mode_objects(world, ob, 64);
                    for (int i = 0; i < on; i++) { int o = i * ObjectSize; f.objects.Add((BitConverter.ToInt32(ob, o + 8), BitConverter.ToInt32(ob, o + 12), BitConverter.ToInt32(ob, o + 16) != 0 && BitConverter.ToInt32(ob, o + 20) == 0, BitConverter.ToInt32(ob, o + 24))); }
                }
                var rq = B.requests; int rn = reflex_world_ui_requests(world, rq, rq.Length);
                for (int at = 0, i = 0; rn > 0 && i <= rn; i++) if (i == rn || rq[i] == (byte)'\n') { if (i > at) f.requests.Add(Strings.Get(rq, at, i - at)); at = i + 1; }
                if (sounds && !B.soundsMissing)   // "play RankChangeUp\n" ...
                {
                    int sn = 0;
                    try { sn = reflex_world_ui_sounds(world, B.sounds, B.sounds.Length); } catch (EntryPointNotFoundException) { B.soundsMissing = true; }
                    for (int at = 0, i = 0; sn > 0 && i <= sn; i++)
                        if (i == sn || B.sounds[i] == (byte)'\n')
                        {
                            int sp = at; while (sp < i && B.sounds[sp] != (byte)' ') sp++;
                            if (sp < i) f.sounds.Add((Strings.Get(B.sounds, at, sp - at), Strings.Get(B.sounds, sp + 1, i - sp - 1)));
                            at = i + 1;
                        }
                }
                double ms = (System.Diagnostics.Stopwatch.GetTimestamp() - t0) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
                Stats.collectMs += ms; if (ms > Stats.collectMax) Stats.collectMax = ms; Stats.collects++;
            }
            return f;
        }

        // ---------------------------------------------------------------------------------------------- which events
        /// The events whose worlds run the game's UI live: the modes (ReflexModes.IsMode) and the arcade races (OriginalHud.LiveRace,
        /// WP-3: National, Supercross, Terracross, Powercross, Waypoint, Free Ride, UTV). Their worlds are booted exactly as before (the
        /// UI goes in-game at run time; the bike state is unchanged by it).
        public static bool IsLive(ReflexEvents.Event e) => ReflexModes.IsMode(e) || OriginalHud.LiveRace(e) || ReflexTutorials.IsTutorial(e);   // WP-T: the MotoSkills world

        /// The scene's world runs the game's UI: added by ReflexEventSelect for a live event.
        public static OriginalLiveUi Attach(OriginalWorldRuntime runtime, ReflexEvents.Event e)
        {
            if (!runtime || !IsLive(e)) return null;
            var ui = runtime.GetComponent<OriginalLiveUi>() ?? runtime.gameObject.AddComponent<OriginalLiveUi>();
            ui.eventKey = e.key; ui.challenge = ReflexModes.ChallengeOf(e);
            // a race keeps the port's post-race pages (OriginalHud / OriginalPostRace): the game's own post-event pages are not shown and
            // their exits not followed; the hand-built HUD is the fallback when the live UI is unavailable
            // a MOTOCAREER series of a mode (Freestyle, Section Race: WP-V2) is a race too: its rules / post pages are OriginalCareerRace's
            // (the game's post-event pages and their exits would leave the level before the series event completes and saves)
            ui.race = !ReflexModes.IsMode(e) || ReflexCareer.Current is ReflexCareer.Race cr && cr.eventKey == e.key;
            ui.objects = !ui.race || e.spec.type == "FreeRide";   // the collectables: the challenges, Free Ride's Hit Discovery spots
            if (ReflexTutorials.IsTutorial(e)) { ui.race = false; ui.objects = false; OriginalTutorial.Attach(runtime, ui); }   // WP-T: the game's pages decide
            return ui;
        }

        // ---------------------------------------------------------------------------------------------- state
        public string eventKey, challenge;
        /// A race (not a mode): the port's post-race pages, OriginalHud's fallback.
        public bool race; bool objects = true;
        OriginalWorldRuntime runtime;
        IntPtr activatedWorld; bool collecting, newWorld;
        readonly object gate = new object();
        Frame pending, frame; string route; bool activationFailed, unavailable, hudLayerFailed;
        Canvas canvas; RectTransform root;
        readonly Dictionary<(string, string, string), LayerView> views = new Dictionary<(string, string, string), LayerView>();
        readonly List<(string name, float t)> sentEvents = new List<(string, float)>();
        Action<IntPtr> collectCall;
        public static OriginalLiveUi Active { get; private set; }
        /// Tests: the game's UI does not go in-game (as with an older plugin), so a race falls back to the hand-built HUD.
        public static bool SimulateUnavailable;

        /// The route the hub took ("GotoFreestyleSinglePlayer" ...), null before activation.
        public string Route => route;
        public Frame Last => frame;
        public bool Running => frame != null;
        /// The game's UI could not run in this world (no InGamePageGraph / activity, an older plugin, the HUD layer not built from the
        /// install): a race then shows the port's hand-built HUD (OriginalHud).
        public bool Unavailable => unavailable;
        /// The page shown by a graph ("InGamePageGraph.pag" ...), null when none.
        public string Page(string graph) => frame?.layers.FirstOrDefault(l => l.graph == graph)?.page;
        public IEnumerable<string> Pages => frame == null ? Enumerable.Empty<string>() : frame.layers.Select(l => l.page).Distinct();
        public bool Shows(string page) => frame != null && frame.layers.Any(l => l.page == page);
        /// The text of the first visible node of that name in a layer of that name (tests).
        /// Every layer built so far: its name, live node count and the live nodes the install's layer has no node for (tests).
        public IEnumerable<(string layer, int nodes, int unmapped, bool failed)> LayerStats => views.Values.Where(v => v != null).Select(v => (v.name, v.mappedTotal, v.unmapped, v.failed));
        public string Text(string layer, string node) => frame?.layers.Where(l => l.name == layer).SelectMany(l => l.nodes).FirstOrDefault(n => n.name == node && n.text != null && (n.state & 0x10) != 0)?.text;

        void Awake() { runtime = GetComponent<OriginalWorldRuntime>(); Active = this; collectCall = CollectBetweenFrames; }
        void OnDestroy() { if (Active == this) Active = null; UnityEngine.Rendering.RenderPipelineManager.beginCameraRendering -= FaceCamera; }

        /// WP-T: the world's own activation instead of reflex_world_ui_activate (between frames; its answer is the Route, null: failed).
        public Func<IntPtr, string> activate;
        /// WP-T: the built node (the install's layer) of a shown layer of that name, by node name; null when not built.
        public OriginalUiTree.Node BuiltNode(string layer, string node)
        {
            foreach (var v in views.Values)
                if (v != null && v.active && v.name == layer && v.tree != null)
                    foreach (var n in v.tree.byPath.Values) if (n.name == node) return n;
            return null;
        }

        /// Posts a UI event as App.SendUIEvent (between frames).
        public void SendUIEvent(string name)
        {
            var sim = runtime ? runtime.Simulation : null;
            if (sim == null) return;
            sentEvents.Add((name, Time.unscaledTime));
            sim.BetweenFrames(w => { try { reflex_world_ui_event(w, name); } catch (Exception e) { Debug.LogWarning("[LiveUi] " + e.Message); } });
        }

        // HUDVisibility (Pause > Options, ReflexOriginalHud): UIDashboard::vf3 (005e1e30) reads the UI setting when the HUD page starts and
        // shows the dashboard (vtable 00951f14, the HUD layer's "... Race Hud" container) only for 0. The world gets the player's choice before
        // the HUD page starts; a change during the race applies to the dashboard at once (in the game it applies when the pause page gives
        // the HUD page back, which re-runs vf3).
        const uint DashboardVtable = 0x951f14;
        volatile bool hudNow = true; bool hudUsed = true; int dashboardForce = -1;
        static int HudSetting(bool visible) => visible ? 0 : 1;

        // between two frames on the simulation thread: a new world (create / restart) first goes in-game, then the frame's UI
        void CollectBetweenFrames(IntPtr w)
        {
            if (w != activatedWorld)
            {
                string r = null; bool missing = false;
                try
                {
                    if (activate != null) r = SimulateUnavailable ? null : activate(w);   // WP-T: a world whose UI is in game already (OriginalTutorial)
                    else { var b = new byte[64]; int n = SimulateUnavailable ? -1 : reflex_world_ui_activate(w, null, b, b.Length); if (n > 0) r = CStr(b, 0, Math.Min(n, 63)); }
                }
                catch (EntryPointNotFoundException) { missing = true; }
                catch (Exception e) { Debug.LogWarning("[LiveUi] activation: " + e.Message); }
                bool hud = hudNow;
                if (r != null)   // before the HUD page starts (EventIntro first); always: the world's profile may carry another value
                    try { OriginalInputNativeSetting(w, "HUDVisibility", HudSetting(hud)); } catch (Exception) { }
                lock (gate) { route = r; activatedWorld = w; newWorld = true; activationFailed = r == null || missing; hudUsed = hud; }
                Debug.Log("[LiveUi] the game's UI runs in-game: " + (r ?? (missing ? "(not activated: the plugin has no live UI)" : "(not activated)")));
                if (r == null) { lock (gate) collecting = false; return; }
            }
            bool off; lock (gate) off = activationFailed;
            if (off) { lock (gate) collecting = false; return; }
            Frame f = null;
            try { f = Collect(w, objects, true); }
            catch (EntryPointNotFoundException) { lock (gate) activationFailed = true; }
            catch (Exception e) { Debug.LogWarning("[LiveUi] " + e.Message); }
            lock (gate) { if (pending != null) Recycle(pending); pending = f; collecting = false; }
        }
        static void OriginalInputNativeSetting(IntPtr w, string name, int value) => ReflexInputNative.reflex_world_ui_setting(w, name, value, -1, 1, out _);

        void Update()
        {
            if (!runtime) return;
            var sim = runtime.Simulation;
            if (!runtime.Running || sim == null)
            {
                // The open core supplies race/HUD state but has no legacy UI worker.
                // Let OriginalHud draw its fallback instead of hiding both canvases.
                unavailable = sim == null;
                if (canvas) canvas.enabled = false;
                collecting = false;
                return;
            }
            bool hudOn = runtime.OriginalHudVisible; hudNow = hudOn;
            bool request; lock (gate) { request = !collecting; if (request) collecting = true; }
            if (request) sim.BetweenFrames(collectCall);   // one call per frame
            long t0 = System.Diagnostics.Stopwatch.GetTimestamp();
            bool reset, failed, used; lock (gate) { reset = newWorld; newWorld = false; failed = activationFailed; used = hudUsed; }
            if (reset)
            {
                if (frame != null) Recycle(frame);
                frame = null; leaving = false; dashboardForce = -1; foreach (var v in views.Values) v?.Reset();
                if (route != null) OriginalHud.UiSound(this, "play", "LevelLoaded");   // the InGame Loading Page's end of loading (005f5d20: progress 1)
            }
            unavailable = failed || hudLayerFailed;
            Frame got; lock (gate) { got = pending; pending = null; }
            if (got != null && !unavailable)
            {
                if (frame != null) Recycle(frame);
                frame = got;
                dashboardForce = hudOn != used ? (hudOn ? 1 : 0) : -1;
                Apply(got); if (objects) ApplyObjects(got); HandlePages(got);
                foreach (var (kind, name) in got.sounds) OriginalHud.UiSound(this, kind, name);
            }
            else if (got != null) Recycle(got);
            float dt = runtime.paused ? 0f : Time.unscaledDeltaTime;
            foreach (var v in views.Values) if (v.active) v.tree.Tick(dt);
            // a race in Post: the port's post-race pages (OriginalHud) replace the game's post-event pages
            bool portPost = race && frame != null && frame.hasMode && frame.mode.state == 4;
            if (canvas) canvas.enabled = frame != null && !unavailable && !portPost && !(runtime.pauseMenu && runtime.pauseMenu.IsOpen);
            HandleKeys();
            if (got != null)
            {
                double ms = (System.Diagnostics.Stopwatch.GetTimestamp() - t0) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
                Stats.applyMs += ms; if (ms > Stats.applyMax) Stats.applyMax = ms; Stats.applies++;
            }
        }

        // ---------------------------------------------------------------------------------------------- pages the port acts on
        // CONTINUE / QUIT of the post-event menus and EXIT of a challenge leave the level (the TransitionPageGraph's loading pages:
        // "Main Menu Loading Page", "Quitting Page", the AAA loading of the activity switch): the port goes back to its front end
        // (or to the Free Ride of the challenge's location) instead of the game's own unload / load.
        bool leaving;
        void HandlePages(Frame f)
        {
            if (leaving || f.requests.Count == 0) return;
            string r = f.requests[0];
            // a race in Post: the port's post-race pages decide (the game's PostEventMenu runs unseen); before (the game's own pause page,
            // reached with the pad's Start: QUIT) the port follows the game as for the modes
            if (race && f.hasMode && f.mode.state == 4) { leaving = false; Debug.Log("[LiveUi] a race's post-event page asks to leave (the port's post-race pages decide): " + r); return; }
            leaving = true;
            Debug.Log("[LiveUi] the game leaves the level: " + r);
            LastRequest = r;
            if (r == "RealWorldSwitchToFreeRide")   // PostChallengePopup EXIT: the level's Free Ride (the game's AAA loading page)
            {
                string back = ReflexModes.FreeRideOf(eventKey);
                if (back != null) { runtime.StopSimulation(); ReflexEventSelect.Launch(back, 0, false, SceneManager.GetActiveScene().name); return; }
            }
            if (r == "GotoVehicleLoadingPage" && OriginalQuickView.Reload(runtime, eventKey)) return;   // the pause page's VEHICLE LIST (WP-5)
            OriginalPauseMenu.QuitToFrontEnd();   // CONTINUE / QUIT / the career exits: the port's front end
        }
        public static string LastRequest;

        // PreChallengePopup: its accept trigger sits on ButtonText, which its list container (BottomContainer) never focuses here (the
        // first child keeps the list focus): Enter / A sends what PreChallengePopup:OnAcceptKeypress sends.
        void HandleKeys()
        {
            if (frame == null) return;
            var kb = Keyboard.current; var gp = Gamepad.current;
            bool accept = kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame) || gp != null && gp.buttonSouth.wasPressedThisFrame;
            // In Free Ride an activation area ("StartActivity" trigger: the AAA notification shows) plus the pause key starts its challenge
            // (FUN_00598e40: EventTypeActivation, the AAA loading page): here the challenge's world (ReflexModes "<activity>/<level>").
            bool pause = kb != null && kb.escapeKey.wasPressedThisFrame || gp != null && gp.startButton.wasPressedThisFrame;
            if (pause && frame.hasMode && frame.mode.name == "FreeRide" && frame.mode.areaActive != 0 && !string.IsNullOrEmpty(frame.mode.area) && frame.mode.area != "FreeRide")
            {
                var level = ReflexModes.Find(eventKey)?.location ?? ReflexEvents.Find(eventKey)?.location;
                var ch = ReflexModes.Challenges.FirstOrDefault(e => e.location == level && string.Equals(ReflexModes.ChallengeOf(e), frame.mode.area, StringComparison.OrdinalIgnoreCase));
                if (ch != null) { Debug.Log("[LiveUi] activation area: " + ch.key); LastRequest = "Activate " + ch.key; runtime.StopSimulation(); ReflexEventSelect.Launch(ch.key, 0, false, SceneManager.GetActiveScene().name); return; }
            }
            if (!accept) return;
            var top = frame.layers.LastOrDefault(l => l.graph == "PopupPageStack.pag");
            if (top != null && top.page == "PreChallengePopup") { FrontEndSoundsPlay("Accept"); SendUIEvent("HidePreChallengePopup"); }
        }
        static void FrontEndSoundsPlay(string name) { }

        // ---------------------------------------------------------------------------------------------- the challenges' collectables
        // Target Practice's targets are scenery instances of the challenge's section (ReflexScenery, "<name> [<model>] #<entity id>"); the
        // game hides one when it is hit (model instance +0xa2, the target's +0x140): shown as the game draws them.
        // "Axis To Align" (+0x154; MXTargetPracticeCollectable::vf5 0040e5d0) is drawn by vf0_o150 0040e880 from the camera's world matrix
        // each render: 0 = the camera's rotation (screen-aligned, the entity's row lengths kept; every shipped target), 1..3 = turned about
        // the entity's own axis (not used by the shipped targets: drawn as authored here).
        readonly Dictionary<int, GameObject> objectViews = new Dictionary<int, GameObject>(); float objectsLookedUp = -10;
        readonly List<Transform> facing = new List<Transform>(); bool facingHooked;
        public int ObjectsShown => frame == null ? 0 : frame.objects.Count(o => o.shown);
        public int ObjectViews => objectViews.Count;
        void ApplyObjects(Frame f)
        {
            if (f.objects.Count == 0) return;
            int targets = 0; for (int i = 0; i < f.objects.Count; i++) if (f.objects[i].kind == 1) targets++;
            if (objectViews.Count < targets && Time.unscaledTime - objectsLookedUp > 2)
            {
                objectsLookedUp = Time.unscaledTime;
                var ids = new HashSet<int>(f.objects.Select(o => o.entityId));
                var level = FindAnyObjectByType<OriginalLevel>();
                if (level && level.Content != null)
                    foreach (var inst in level.Content.instances) if (inst.transform && ids.Contains(inst.entityId) && !objectViews.ContainsKey(inst.entityId))
                    {
                        objectViews[inst.entityId] = inst.transform.gameObject;
                        if (f.objects.Any(o => o.entityId == inst.entityId && o.kind == 1 && o.axis == 0)) facing.Add(inst.transform);
                        // GL_TargetPractice's Template_AnimChevron_NoFadeCtrl (the diffuse's alpha over a scrolled gradient; the generic
                        // scenery port outputs alpha 1): the diffuse texture's alpha
                        if (inst.transform.GetComponent<MeshRenderer>() is MeshRenderer mr) { var mpb = new MaterialPropertyBlock(); mr.GetPropertyBlock(mpb); mpb.SetFloat("_AlphaSource", 1); mr.SetPropertyBlock(mpb); }
                    }
                if (facing.Count > 0 && !facingHooked) { facingHooked = true; UnityEngine.Rendering.RenderPipelineManager.beginCameraRendering += FaceCamera; }
            }
            foreach (var (kind, id, shown, axis) in f.objects) if (objectViews.TryGetValue(id, out var go) && go && go.activeSelf != shown) go.SetActive(shown);
        }
        void FaceCamera(UnityEngine.Rendering.ScriptableRenderContext ctx, Camera cam)
        {
            if (!runtime || cam != runtime.chaseCamera) return;
            var q = cam.transform.rotation;
            foreach (var t in facing) if (t && t.gameObject.activeInHierarchy) t.rotation = q;
        }

        // ---------------------------------------------------------------------------------------------- drawing
        // Layers the port leaves out: debug overlays, the PC cursor (the port's own), network / voice / PS3 overlays (offline).
        static readonly HashSet<string> Skipped = new HashSet<string>(StringComparer.Ordinal)
        { "FPSOverlay", "DebugProgressComplete.lyr", "CursorOverlay.lyr", "PS3InviteOverlay.lyr", "NetworkOverlay", "VoiceChatOverlay.lyr", "PublisherVideoLayer" };

        void EnsureCanvas()
        {
            if (canvas) return;
            var cgo = new GameObject("Original UI (live)", typeof(Canvas), typeof(CanvasScaler));
            cgo.transform.SetParent(transform, false);
            canvas = cgo.GetComponent<Canvas>(); canvas.renderMode = RenderMode.ScreenSpaceOverlay; canvas.sortingOrder = 12;
            var sc = cgo.GetComponent<CanvasScaler>(); sc.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
            sc.referenceResolution = new Vector2(1280, 720); sc.screenMatchMode = CanvasScaler.ScreenMatchMode.Expand;
            var rgo = new GameObject("1280x720", typeof(RectTransform)); rgo.transform.SetParent(cgo.transform, false);
            root = (RectTransform)rgo.transform; root.anchorMin = root.anchorMax = root.pivot = new Vector2(0.5f, 0.5f); root.sizeDelta = new Vector2(1280, 720);
        }

        void Apply(Frame f)
        {
            EnsureCanvas();
            foreach (var v in views.Values) v.active = false;
            int order = 0;
            for (int li = 0; li < f.layers.Count; li++)
            {
                var l = f.layers[li];
                if (Skipped.Contains(l.name) || l.nodes.Count == 0) continue;
                var key = (l.graph, l.page, l.name);
                if (!views.TryGetValue(key, out var v))
                {
                    views[key] = v = LayerView.Create(l.name, root);
                    // a race's HUD page whose layer the install does not build: the hand-built HUD instead (OriginalHud)
                    if (v.failed && race && l.page.EndsWith("Single Player Page", StringComparison.Ordinal)) hudLayerFailed = true;
                }
                if (v == null || v.failed) continue;
                v.active = true; if (v.rt.GetSiblingIndex() != order) v.rt.SetSiblingIndex(order); order++;
                v.Apply(l, dashboardForce);
            }
            foreach (var v in views.Values) if (v != null && v.rt && v.rt.gameObject.activeSelf != v.active) v.rt.gameObject.SetActive(v.active);
            foreach (var a in f.anims)
            {
                if (a.layer < 0 || a.layer >= f.layers.Count) continue;
                var l = f.layers[a.layer];
                if (!views.TryGetValue((l.graph, l.page, l.name), out var v) || v == null || v.failed) continue;
                v.Animate(a);
            }
        }

        sealed class LayerView
        {
            public string name; public RectTransform rt; public OriginalUiTree tree; public bool active, failed;
            /// live nodes without a built node of the install's layer (matched by name and order among siblings)
            public int unmapped; public int mappedTotal => mappedCount;
            OriginalUiTree.Node[] map; uint mappedObj; int mappedCount = -1;
            uint[] lastColor; Vector2[] lastPos, lastScale; string[] lastText; bool[] lastVisible;

            public static LayerView Create(string layerName, RectTransform parent)
            {
                var v = new LayerView { name = layerName };
                var go = new GameObject(layerName, typeof(RectTransform)); go.transform.SetParent(parent, false);
                v.rt = (RectTransform)go.transform; v.rt.anchorMin = v.rt.anchorMax = new Vector2(0.5f, 0.5f); v.rt.pivot = new Vector2(0.5f, 0.5f); v.rt.sizeDelta = new Vector2(1280, 720);
                // children are laid out from the parent's top-left (OriginalUiTree): a full 1280x720 box
                try
                {
                    var (pkg, asset) = LayerPackages.Find(layerName);
                    if (pkg == null) throw new InvalidOperationException("no MXUI layer named " + layerName);
                    v.tree = new OriginalUiTree(OriginalUiAssets.Texture, OriginalUiAssets.Font);
                    var doc = OriginalUiAssets.Data.BuildLayerOwn(pkg, asset);
                    foreach (var e in OriginalUi.Arr(doc, "elements")) v.tree.Build((Dictionary<string, object>)e, v.rt, null);
                    v.tree.AddAnimations(OriginalUi.Arr(doc, "layerAnimations"));
                    v.tree.AddComponentAnimations(OriginalUi.Obj(doc, "layerJson"));
                    foreach (var r in v.tree.roots) OriginalHud.Complete(r);   // WP-3: icon geometry, mask containers
                    ReflexKeyMapGlyphs.BlankKeyMapImages(v.tree, OriginalUi.Obj(doc, "layerJson"));   // WP-C: KeyMap images are UIEmpty on the PC (OriginalInput.cs)
                }
                catch (Exception e) { v.failed = true; Debug.LogWarning("[LiveUi] layer " + layerName + " not built: " + e.Message); }
                return v;
            }

            public void Reset() { mappedObj = 0; mappedCount = -1; }

            // live node i (0 = the layer) -> the built node: children matched by name and occurrence among their siblings, in order
            void Map(LayerSnap l)
            {
                map = new OriginalUiTree.Node[l.nodes.Count];
                var childLists = new Dictionary<int, List<OriginalUiTree.Node>>();
                var used = new Dictionary<int, Dictionary<string, int>>();
                for (int i = 1; i < l.nodes.Count; i++)
                {
                    var n = l.nodes[i]; int p = n.parent;
                    List<OriginalUiTree.Node> sibs;
                    if (p == 0) sibs = tree.roots;
                    else if (p > 0 && p < i && map[p] != null) sibs = map[p].children;
                    else continue;
                    if (!used.TryGetValue(p, out var u)) used[p] = u = new Dictionary<string, int>();
                    u.TryGetValue(n.name, out int k); u[n.name] = k + 1;
                    int seen = 0;
                    foreach (var s in sibs) if (s.name == n.name) { if (seen == k) { map[i] = s; break; } seen++; }
                }
                mappedObj = l.obj; mappedCount = l.nodes.Count;
                unmapped = 0; for (int i = 1; i < map.Length; i++) if (map[i] == null) unmapped++;
                lastColor = new uint[map.Length]; lastPos = new Vector2[map.Length]; lastScale = new Vector2[map.Length]; lastText = new string[map.Length]; lastVisible = new bool[map.Length];
                for (int i = 0; i < map.Length; i++) { lastPos[i] = new Vector2(float.NaN, 0); lastScale[i] = new Vector2(float.NaN, 0); lastColor[i] = 0xdeadbeef; }
            }

            bool[] chainFocus;
            public void Apply(LayerSnap l, int dashboardForce = -1)
            {
                if (l.obj != mappedObj || l.nodes.Count != mappedCount) Map(l);
                if (!rt.gameObject.activeSelf) rt.gameObject.SetActive(true);
                bool layerVisible = (l.state & 0x10) != 0;
                var ls = layerVisible ? Vector3.one : Vector3.zero; if (rt.localScale != ls) rt.localScale = ls;
                // a component draws Focused only when its containers up to the layer are focused too (WP-5: the pause list's row keeps its
                // Focused bit while the StuntPreview list has the focus, and the original draws it unfocused)
                if (chainFocus == null || chainFocus.Length < l.nodes.Count) chainFocus = new bool[Math.Max(64, l.nodes.Count)];
                chainFocus[0] = (l.state & 1) != 0;
                for (int i = 1; i < l.nodes.Count; i++) { int pp = l.nodes[i].parent; chainFocus[i] = (l.nodes[i].state & 1) != 0 && (pp >= 0 && pp < i ? chainFocus[pp] : chainFocus[0]); }
                for (int i = 1; i < map.Length; i++)
                {
                    var t = map[i]; if (t == null) continue;
                    var n = l.nodes[i];
                    bool vis = (n.state & 0x10) != 0;
                    if (dashboardForce >= 0 && n.vtable == DashboardVtable) vis = dashboardForce == 1;   // HUDVisibility changed during the race
                    if (vis != lastVisible[i] || t.rect.gameObject.activeSelf != vis) { OriginalUiTree.SetVisible(t, vis); lastVisible[i] = vis; }
                    if (n.text != null && n.text != lastText[i]) { OriginalUiTree.SetText(t, n.text); lastText[i] = n.text; }
                    uint ec = t.graphic ? OriginalEventIntro.StateColor(t, chainFocus[i] ? n.state : n.state & ~1u, n.color) : n.color;   // UIColorState: the state's colour (WP-5)
                    if (t.graphic && ec != lastColor[i])
                    {
                        uint c = ec;
                        OriginalUiTree.SetColor(t, new Color(((c >> 16) & 255) / 255f, ((c >> 8) & 255) / 255f, (c & 255) / 255f, (c >> 24) / 255f));
                        lastColor[i] = c;
                    }
                    if (t.graphic is RawImage ri && OriginalEventIntro.CodeImage(n.obj, out var ctex, out var cuv) && (ri.texture != ctex || ri.uvRect != cuv)) { ri.texture = ctex; ri.uvRect = cuv; }   // SetMaterial / SetIconId by the page Lua (WP-5)
                    if (n.pos != lastPos[i])   // Position (set by the layer and by code): the pivot point in the parent, y down
                    {
                        var bp = new Vector2(n.pos.x, -n.pos.y);
                        t.rect.anchoredPosition += bp - t.basePos; t.basePos = bp; lastPos[i] = n.pos;
                    }
                    if (n.scale != lastScale[i] && n.scale.x > 0 && n.scale.y > 0)
                    {
                        var s = new Vector3(n.scale.x, n.scale.y, 1);
                        var cur = t.rect.localScale; var b = t.baseScale;
                        t.rect.localScale = new Vector3(b.x != 0 ? cur.x / b.x * s.x : s.x, b.y != 0 ? cur.y / b.y * s.y : s.y, 1);
                        t.baseScale = s; lastScale[i] = n.scale;
                    }
                }
            }

            public void Animate(AnimSnap a)
            {
                if (map == null || a.node < 0 || a.node >= map.Length) return;
                var scope = a.node == 0 ? null : map[a.node];
                if (a.node != 0 && scope == null) return;
                if (a.state == 0) tree.Play(a.name, scope);
                else if (a.state == 3) tree.Finish(a.name, scope);
            }
        }

        // ---------------------------------------------------------------------------------------------- layer -> package
        /// The MXUI package of a layer the game shows, by its root component's name (the uicmpnt asset is usually "<name>" or
        /// "<name>.lyr"; a few are named otherwise, e.g. PostEventOverlay.lyr's root "PostEvent_ActivityDashboard"): the game's
        /// language packages in the order the in-game page graph loads them (the Hud<Mode>SinglePlayer packages, InGame, Shared).
        public static class LayerPackages
        {
            static readonly Dictionary<string, (string, string)> found = new Dictionary<string, (string, string)>(StringComparer.Ordinal);
            static List<string> order;
            static readonly Dictionary<string, Dictionary<string, string>> rootNames = new Dictionary<string, Dictionary<string, string>>();

            static List<string> Order()
            {
                if (order != null) return order;
                var data = OriginalUiAssets.Data; string lang = "." + data.language;
                var db = ReflexInstall.Packages.Db(ReflexUiData.DatabaseName);
                var all = db != null ? db.packages.Keys.ToList() : new List<string>();
                var mine = all.Where(p => p.EndsWith(lang, StringComparison.Ordinal)).ToList();
                order = mine.Where(p => p.StartsWith("Hud", StringComparison.Ordinal) && p.Contains("SinglePlayer")).OrderBy(p => p, StringComparer.Ordinal).ToList();
                foreach (string p in new[] { "InGame" + lang, "Shared" + lang, "BaseLocalized" + lang }) if (all.Contains(p)) order.Add(p);
                order.AddRange(mine.Where(p => !order.Contains(p)).OrderBy(p => p, StringComparer.Ordinal));
                if (all.Contains("Base")) order.Add("Base");
                return order;
            }

            public static (string package, string asset) Find(string rootName)
            {
                lock (found)
                {
                    if (found.TryGetValue(rootName, out var r)) return r;
                    var data = OriginalUiAssets.Data;
                    foreach (string p in Order())
                        foreach (string a in new[] { rootName, rootName + ".lyr" })
                            if (data.Has(p, "uicmpnt", a)) return found[rootName] = (p, a);
                    foreach (string p in Order())
                    {
                        if (!rootNames.TryGetValue(p, out var names))
                        {
                            rootNames[p] = names = new Dictionary<string, string>(StringComparer.Ordinal);
                            foreach (string a in data.Names(p, "uicmpnt"))
                                try { var l = data.Layer(p, a); if (l.root != null && l.root.TryGetValue("name", out var nm) && nm is string s && !names.ContainsKey(s)) names[s] = a; }
                                catch (Exception) { }
                        }
                        if (names.TryGetValue(rootName, out string asset)) return found[rootName] = (p, asset);
                    }
                    return found[rootName] = (null, null);
                }
            }
        }
    }
}
