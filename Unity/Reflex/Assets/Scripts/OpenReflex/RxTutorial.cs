using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Reflex.Port.RxCore
{
    // The MotoSkills tutorials (core/include/rx_tutorial.h, w-tutorials): the original rides the six lessons in the front end's own world UI_Compound
    // (the MXUIWorld every session builds) with its FreeRide activity and the TutorialManager of that world (MXUIWorld + 0x22180, ported:
    // core/game/fx_tutorial_manager.c). rx_session_tutorial_start instead of rx_session_event_load (MainMenu:OnTutorialKeypress, the Tutorial Load
    // Page); the pages' calls (App.EnableTutorial, App.DisableTutorial, App.PauseActiveWorld, RetryTutorial, EnableTutorialMode( FALSE )) and the
    // popup stack the manager asks about are the host's; rx_session_tutorial reads what the manager decided and handed the UI.
    public sealed class RxTutorialState
    {
        public const int Size = 588, Events = 32, Key = 64;
        public int active, spawned, mode, managerOff, lesson, lessonState, step, passed, wrecked, boundary, good, bad, valid, currentTutorial, completed;
        public float stepTimer, leanTime, holdTime, wreckTimer;
        public int leanFlags, worldPaused;
        public uint eventSerial, voiceSerial, stopSerial, frames;
        public readonly byte[] events = new byte[Events];
        public string description = "", activation = "", hudLabel = "", completion = "", failed = "", voice = "";
        public bool Valid;

        public const int EventPrePopup = 1, EventPostPopup = 2, EventFailedPopup = 3, EventShowHud = 4;

        internal void Parse(byte[] b)
        {
            int I(int w) => BitConverter.ToInt32(b, 4 * w);
            float F(int w) => BitConverter.ToSingle(b, 4 * w);
            active = I(2); spawned = I(3); mode = I(4); managerOff = I(5); lesson = I(6); lessonState = I(7); step = I(8);
            passed = I(9); wrecked = I(10); boundary = I(11); good = I(12); bad = I(13); valid = I(14); currentTutorial = I(15); completed = I(16);
            stepTimer = F(17); leanTime = F(18); holdTime = F(19); wreckTimer = F(20); leanFlags = I(21); worldPaused = I(22);
            eventSerial = (uint)I(23);
            Buffer.BlockCopy(b, 96, events, 0, Events);
            int o = 96 + Events;
            description = S(b, o, Key); o += Key; activation = S(b, o, Key); o += Key; hudLabel = S(b, o, Key); o += Key;
            completion = S(b, o, Key); o += Key; failed = S(b, o, Key); o += Key;
            voiceSerial = BitConverter.ToUInt32(b, o); stopSerial = BitConverter.ToUInt32(b, o + 4); o += 8;
            voice = S(b, o, 2 * Key); o += 2 * Key;
            frames = BitConverter.ToUInt32(b, o);
            Valid = true;
        }
        static string S(byte[] b, int o, int n) { int e = Array.IndexOf<byte>(b, 0, o, n); return Encoding.ASCII.GetString(b, o, (e < 0 ? o + n : e) - o); }

        /// The kind of UI event number n (0..eventSerial-1) while it is still in the ring.
        public int EventAt(uint n) => n + Events >= eventSerial && n < eventSerial ? events[n % Events] : 0;
        public RxTutorialState Clone() => (RxTutorialState)MemberwiseClone();
    }

    /// One lesson object (rx_tutorial_object): a TUT<n> model / scrolling texture of the UI scene and its draw flag (ShowLessonObjects).
    public struct RxTutorialObject { public int entityId; public bool shown; public string name; }

    internal static class RxTutorialNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int StartFn(IntPtr session, ref RxEventSpec spec);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int IntFn(IntPtr session, int value);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int VoidFn(IntPtr session);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, byte[] state);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ObjectsFn(IntPtr session, byte[] objects, uint max);
        static readonly RxNativeLoader.Fn<StartFn> start = new RxNativeLoader.Fn<StartFn>("rx_session_tutorial_start");
        static readonly RxNativeLoader.Fn<IntFn> pause = new RxNativeLoader.Fn<IntFn>("rx_session_tutorial_pause");
        static readonly RxNativeLoader.Fn<IntFn> lesson = new RxNativeLoader.Fn<IntFn>("rx_session_tutorial_lesson");
        static readonly RxNativeLoader.Fn<VoidFn> disable = new RxNativeLoader.Fn<VoidFn>("rx_session_tutorial_disable");
        static readonly RxNativeLoader.Fn<VoidFn> retry = new RxNativeLoader.Fn<VoidFn>("rx_session_tutorial_retry");
        static readonly RxNativeLoader.Fn<IntFn> popup = new RxNativeLoader.Fn<IntFn>("rx_session_tutorial_popup");
        static readonly RxNativeLoader.Fn<IntFn> suspend = new RxNativeLoader.Fn<IntFn>("rx_session_tutorial_suspend_input");
        static readonly RxNativeLoader.Fn<VoidFn> end = new RxNativeLoader.Fn<VoidFn>("rx_session_tutorial_end");
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_tutorial");
        static readonly RxNativeLoader.Fn<ObjectsFn> objects = new RxNativeLoader.Fn<ObjectsFn>("rx_session_tutorial_objects");

        internal static int Start(IntPtr s, ref RxEventSpec spec) => start.Get()(s, ref spec);
        internal static int Pause(IntPtr s, bool paused) => pause.Get()(s, paused ? 1 : 0);
        internal static int Lesson(IntPtr s, int n) => lesson.Get()(s, n);
        internal static int Disable(IntPtr s) => disable.Get()(s);
        internal static int Retry(IntPtr s) => retry.Get()(s);
        internal static int Popup(IntPtr s, bool up) => popup.Get()(s, up ? 1 : 0);
        /// App.SetUIVariableInt( "SuspendPlayerInput", value ) where the original's pages set it (w-tutcore: Player_SetInputSuspended in the core).
        internal static int SuspendInput(IntPtr s, bool suspended) => suspend.Get()(s, suspended ? 1 : 0);
        internal static int End(IntPtr s) => end.Get()(s);

        [ThreadStatic] static byte[] buf;
        internal static int Read(IntPtr s, RxTutorialState into)
        {
            var b = buf ?? (buf = new byte[RxTutorialState.Size]);
            BitConverter.GetBytes((uint)RxTutorialState.Size).CopyTo(b, 0);
            BitConverter.GetBytes(1u).CopyTo(b, 4);
            int r = read.Get()(s, b);
            if (r == 0) into.Parse(b);
            return r;
        }
        [ThreadStatic] static byte[] objBuf;
        internal static int Objects(IntPtr s, RxTutorialObject[] into)
        {
            const int one = 72;
            var b = objBuf ?? (objBuf = new byte[one * 64]);
            int n = objects.Get()(s, b, (uint)Math.Min(into.Length, 64));
            for (int i = 0; i < n; i++)
            {
                int o = i * one, e = Array.IndexOf<byte>(b, 0, o + 8, 64);
                into[i] = new RxTutorialObject { entityId = BitConverter.ToInt32(b, o), shown = BitConverter.ToInt32(b, o + 4) != 0,
                    name = Encoding.ASCII.GetString(b, o + 8, (e < 0 ? o + 72 : e) - (o + 8)) };
            }
            return n;
        }
    }
}
