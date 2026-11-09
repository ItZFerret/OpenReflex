using System;
using System.Runtime.InteropServices;
using System.Text;
namespace Reflex.Port.RxCore
{
    // What the original's vehicle sound layer reads from the simulation every frame (core/include/rx_vehicle_audio.h, 5664 bytes, version 2 (w-rc-core: 32 riders; 3360 bytes in version 1), was version
    // 1): per rider the telemetry snapshot fields of the vehicle's vf52 and the words VehicleEngineAudio_Update tests, plus a ring of the
    // sound-layer cues gameplay code issued (the race intro's {EnableStartLineState} / {DisableStartLineState}, "Pre Exited", scenario
    // sounds). Read into a scratch buffer on the simulation thread and decoded into preallocated arrays (no marshalling per frame).
    public sealed class RxVehicleAudioRider
    {
        public const uint Valid = 1, HasPlayer = 2, LocalHuman = 4, ManualGearbox = 8, HumanSlot = 16, AudioSlot = 32, StartLine = 64;
        public const int ClassNone = 0, ClassMx = 1, ClassAtv = 2, ClassUtv = 3, ClassBuggy = 4, ClassTruck = 5;
        public int slot; public uint flags; public int vehicleClass, vehicleKind, pad, engineSize; public uint vehicle;
        public float px, py, pz, vx, vy, vz;
        public float speed, rpm, maxRpm, idleRpm, throttle, control2;
        public int gear; public float load;
        public bool clutch, airborne, belowHalf; public float water; public bool underwater, drowned, wrecked;
        public readonly bool[] wheelAir = new bool[4];
        /// The vehicle sound manager's slot 0 .. 11 (00680b10's first free slot; w-sfx), -1 none: the engine emitter's +0, its AIFX for non-local riders.
        public int audioSlot = -1;
        /// The core's start line of the slot's engine emitter (w-rng; valid with StartLine): the state (4 StartLine, 2 Rev, -1 another),
        /// a serial that changes at every StartLine / Rev entry and every rev replay, and a car's rev target rpm. The core draws the game's
        /// random generator for them (the time to the next rev, the revs in a row, the target), so the host follows instead of drawing.
        public int startLineState = -1; public uint startLineSerial; public float startLineTarget;
        public void CopyFrom(RxVehicleAudioRider o)
        {
            slot = o.slot; flags = o.flags; vehicleClass = o.vehicleClass; vehicleKind = o.vehicleKind; pad = o.pad; engineSize = o.engineSize; vehicle = o.vehicle;
            px = o.px; py = o.py; pz = o.pz; vx = o.vx; vy = o.vy; vz = o.vz; speed = o.speed; rpm = o.rpm; maxRpm = o.maxRpm; idleRpm = o.idleRpm;
            throttle = o.throttle; control2 = o.control2; gear = o.gear; load = o.load; clutch = o.clutch; airborne = o.airborne; belowHalf = o.belowHalf;
            water = o.water; underwater = o.underwater; drowned = o.drowned; wrecked = o.wrecked; Array.Copy(o.wheelAir, wheelAir, 4); audioSlot = o.audioSlot;
            startLineState = o.startLineState; startLineSerial = o.startLineSerial; startLineTarget = o.startLineTarget;
        }
    }
    public sealed class RxVehicleAudioCue { public uint serial; public int kind; public string name = ""; }

    public sealed class RxVehicleAudioState
    {
        // version 2 (w-rc-core): riders 16 .. 31 follow the cues (riders_ext); 32 riders in all
        public const int Size = 5664, Riders = 32, RidersV1 = 16, V1Size = 3360, Version = 2, Cues = 16, RiderBytes = 144, CueBytes = 64, CueName = 56;
        public const int CueStartLineEnable = 1, CueStartLineDisable = 2, CuePreExited = 3, CueScenarioSound = 4, CueMusicStart = 5, CueMusicStop = 6, CueOther = 7;
        public bool valid; public int riderCount; public uint cueSerial;
        public readonly RxVehicleAudioRider[] riders = new RxVehicleAudioRider[Riders];
        public readonly RxVehicleAudioCue[] cues = new RxVehicleAudioCue[Cues];
        public RxVehicleAudioState() { for (int i = 0; i < Riders; i++) riders[i] = new RxVehicleAudioRider(); for (int i = 0; i < Cues; i++) cues[i] = new RxVehicleAudioCue(); }
        public void Clear() { valid = false; riderCount = 0; }
        public void CopyFrom(RxVehicleAudioState o)
        {
            valid = o.valid; riderCount = o.riderCount; cueSerial = o.cueSerial;
            for (int i = 0; i < Riders; i++) riders[i].CopyFrom(o.riders[i]);
            for (int i = 0; i < Cues; i++) { cues[i].serial = o.cues[i].serial; cues[i].kind = o.cues[i].kind; cues[i].name = o.cues[i].name; }
        }

        static int I(byte[] b, int o) => BitConverter.ToInt32(b, o);
        static uint U(byte[] b, int o) => BitConverter.ToUInt32(b, o);
        static float F(byte[] b, int o) => BitConverter.ToSingle(b, o);
        /// Decodes the native layout (little endian).
        public void Decode(byte[] b)
        {
            valid = U(b, 8) != 0; riderCount = Math.Max(0, Math.Min(Riders, I(b, 12))); cueSerial = U(b, 16);
            for (int i = 0; i < Riders; i++)
            {
                int o = i < RidersV1 ? 32 + i * RiderBytes : V1Size + (i - RidersV1) * RiderBytes; var r = riders[i];
                r.slot = I(b, o); r.flags = U(b, o + 4); r.vehicleClass = I(b, o + 8); r.vehicleKind = I(b, o + 12); r.pad = I(b, o + 16); r.engineSize = I(b, o + 20); r.vehicle = U(b, o + 24);
                r.px = F(b, o + 28); r.py = F(b, o + 32); r.pz = F(b, o + 36); r.vx = F(b, o + 40); r.vy = F(b, o + 44); r.vz = F(b, o + 48);
                r.speed = F(b, o + 52); r.rpm = F(b, o + 56); r.maxRpm = F(b, o + 60); r.idleRpm = F(b, o + 64); r.throttle = F(b, o + 68); r.control2 = F(b, o + 72);
                r.gear = I(b, o + 76); r.load = F(b, o + 80); r.clutch = U(b, o + 84) != 0; r.airborne = U(b, o + 88) != 0; r.belowHalf = U(b, o + 92) != 0;
                r.water = F(b, o + 96); r.underwater = U(b, o + 100) != 0; r.drowned = U(b, o + 104) != 0; r.wrecked = U(b, o + 108) != 0;
                for (int k = 0; k < 4; k++) r.wheelAir[k] = U(b, o + 112 + 4 * k) != 0;
                r.audioSlot = (r.flags & RxVehicleAudioRider.AudioSlot) != 0 ? I(b, o + 128) : -1;
                bool sl = (r.flags & RxVehicleAudioRider.StartLine) != 0;
                r.startLineState = sl ? I(b, o + 132) : -1; r.startLineSerial = sl ? U(b, o + 136) : 0u; r.startLineTarget = sl ? F(b, o + 140) : 0f;
            }
            int c0 = 32 + RidersV1 * RiderBytes;
            for (int i = 0; i < Cues; i++)
            {
                int o = c0 + i * CueBytes; var c = cues[i];
                uint serial = U(b, o);
                if (serial == c.serial && serial != 0) continue;           // unchanged entry: keep the decoded name
                c.serial = serial; c.kind = I(b, o + 4);
                int n = 0; while (n < CueName && b[o + 8 + n] != 0) n++;
                c.name = n == 0 ? "" : Encoding.UTF8.GetString(b, o + 8, n);
            }
        }
    }

    /// The host's inputs to the core's vehicle sound layer (w-rng): the session pointer is the one BetweenFrames hands out.
    public static class RxVehicleAudioInput
    {
        /// rx_session_vehicle_audio_finished (0 = taken; throws EntryPointNotFoundException on an older core).
        public static int Finished(IntPtr session, int audioSlot) => RxVehicleAudioNative.Finished(session, audioSlot);
    }

    internal static class RxVehicleAudioNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, byte[] value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_vehicle_audio");
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FinishedFn(IntPtr session, int audioSlot);
        static readonly RxNativeLoader.Fn<FinishedFn> finished = new RxNativeLoader.Fn<FinishedFn>("rx_session_vehicle_audio_finished");
        /// rx_session_vehicle_audio_finished: the rev one-shot of the engine emitter in sound slot `audioSlot` played to its end (w-rng). 0 = taken.
        internal static int Finished(IntPtr session, int audioSlot) => finished.Get()(session, audioSlot);
        /// rx_session_vehicle_audio into `scratch` (RxVehicleAudioState.Size bytes) and decoded into `into`. 0 = read.
        internal static int Read(IntPtr session, byte[] scratch, RxVehicleAudioState into)
        {
            const uint size = RxVehicleAudioState.Size;
            scratch[0] = unchecked((byte)size); scratch[1] = unchecked((byte)(size >> 8)); scratch[2] = 0; scratch[3] = 0;
            scratch[4] = (byte)RxVehicleAudioState.Version; scratch[5] = 0; scratch[6] = 0; scratch[7] = 0;
            int rc = read.Get()(session, scratch);
            if (rc == 0) into.Decode(scratch); else into.Clear();
            return rc;
        }
    }
}
