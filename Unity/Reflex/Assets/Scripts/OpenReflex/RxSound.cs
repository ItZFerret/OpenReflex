using System;
using System.Runtime.InteropServices;
using System.Text;
namespace Reflex.Port.RxCore
{
    // The rest of the original's game sound as the core reports it (core/include/rx_sound.h, 23812 bytes, version 1; w-sfx): the level
    // volume mix, per rider the sound manager snapshot's wheel groups (drive-on / skid / foley / tyre hits), the physics contact events
    // the AudioCollisionManager gets, the crowd (momentum, reactions), the activity audio cues and the music player. Read into a scratch
    // buffer on the simulation thread and decoded into preallocated objects (ring entries decode only when their serial changed).
    public sealed class RxSoundWheel
    {
        public const uint Air = 1, Water = 2, WaterEntered = 4;
        public float px, py, pz, nx, ny, nz, sideSlip2, rollSlip2, suspensionForce, angularVelocity, depth;
        public uint flags; public int zone, contactId; public uint contact; public int contactClass, contactSound, contactMaterial;
        public bool InAir => (flags & Air) != 0;
        public void CopyFrom(RxSoundWheel o)
        {
            px = o.px; py = o.py; pz = o.pz; nx = o.nx; ny = o.ny; nz = o.nz; sideSlip2 = o.sideSlip2; rollSlip2 = o.rollSlip2;
            suspensionForce = o.suspensionForce; angularVelocity = o.angularVelocity; depth = o.depth; flags = o.flags; zone = o.zone; contactId = o.contactId;
            contact = o.contact; contactClass = o.contactClass; contactSound = o.contactSound; contactMaterial = o.contactMaterial;
        }
    }
    public sealed class RxSoundRider
    {
        public int slot; public uint vehicle; public int wheelCount, vehicleClass, soundId, material, soundFeature0; public float vx, vy, vz;
        public readonly RxSoundWheel[] wheels = { new RxSoundWheel(), new RxSoundWheel(), new RxSoundWheel(), new RxSoundWheel() };
        public void CopyFrom(RxSoundRider o)
        {
            slot = o.slot; vehicle = o.vehicle; wheelCount = o.wheelCount; vehicleClass = o.vehicleClass; soundId = o.soundId; material = o.material; soundFeature0 = o.soundFeature0;
            vx = o.vx; vy = o.vy; vz = o.vz; for (int i = 0; i < 4; i++) wheels[i].CopyFrom(o.wheels[i]);
        }
    }
    public sealed class RxSoundContact
    {
        public const int ClassRider = 0x13, ClassTerrain = 0x10, ClassWater = 0xf, ClassForest = 0x100f, ClassCollision = 5, ClassPhysics = 0x1029, ClassVehicle = 0x1082;
        public const uint Soft = 1, RiderA = 2, RiderB = 4, VocalA = 8, VocalB = 16, RagdollA = 32, RagdollB = 64, SlideB = 128;
        public uint serial, frame, a, b; public int classA, classB, soundA, soundB, materialA, materialB; public uint flags;
        public float strengthA, strengthB; public int soundOverride, featureA, featureB; public float px, py, pz;
        public uint vocalA, vocalB; public float slideSpeed; public int slideKind, zone, objectMaterial;
        public void CopyFrom(RxSoundContact o)
        {
            serial = o.serial; frame = o.frame; a = o.a; b = o.b; classA = o.classA; classB = o.classB; soundA = o.soundA; soundB = o.soundB;
            materialA = o.materialA; materialB = o.materialB; flags = o.flags; strengthA = o.strengthA; strengthB = o.strengthB; soundOverride = o.soundOverride;
            featureA = o.featureA; featureB = o.featureB; px = o.px; py = o.py; pz = o.pz; vocalA = o.vocalA; vocalB = o.vocalB;
            slideSpeed = o.slideSpeed; slideKind = o.slideKind; zone = o.zone; objectMaterial = o.objectMaterial;
        }
    }
    public sealed class RxSoundCue { public uint serial; public int index; public string path = ""; }

    public sealed class RxSoundState
    {
        // version 2 (w-rc-core): riders 16 .. 31 at the end (riders_ext, from V1Size); 32 riders in all
        public const int Size = 29320, V1Size = 23944, Version = 2, RidersV1 = 16, Channels = 16, Riders = 32, Contacts = 128, Reactions = 16, Cues = 16, CuePath = 88, Playlist = 32, Name = 32, RiderSounds = 8;
        const int RiderBytes = 336, WheelBytes = 72, ContactBytes = 128, CueBytes = 96;
        const int OffMix = 32, OffRiders = OffMix + 4 * Channels, OffContacts = OffRiders + RidersV1 * RiderBytes, OffCrowd = OffContacts + Contacts * ContactBytes;
        const int OffCues = OffCrowd + 164, OffMusic = OffCues + Cues * CueBytes, OffRiderSounds = OffMusic + 256;
        public bool valid; public int riderCount, channelCount; public uint contactSerial, cueSerial, frame;
        public readonly float[] channelMix = new float[Channels];
        public readonly RxSoundRider[] riders = new RxSoundRider[Riders];
        public readonly RxSoundContact[] contacts = new RxSoundContact[Contacts];
        // crowd
        public bool crowdValid; public int crowdType, crowdSize, crowdSizeRequested, crowdState; public float crowdMomentum; public uint reactionSerial;
        public readonly uint[] reactionSerials = new uint[Reactions]; public readonly int[] reactionKinds = new int[Reactions];
        public readonly RxSoundCue[] cues = new RxSoundCue[Cues];
        // music
        public bool musicValid; public int musicMode, musicKind, musicRequested, musicCurrent, scoreRequested, scoreCurrent, musicPosition, musicCount;
        public uint musicFlags, musicSerial; public float musicDelay, musicTransition, musicStartDelay;
        public readonly int[] playlist = new int[Playlist]; public string musicTrack = "", musicScore = "";
        // the rider sound slots gameplay code started (006817c0)
        public uint riderSoundSerial; public readonly uint[] riderSoundSerials = new uint[RiderSounds], riderSoundVehicles = new uint[RiderSounds]; public readonly int[] riderSoundSlots = new int[RiderSounds];

        public RxSoundState()
        {
            for (int i = 0; i < Riders; i++) riders[i] = new RxSoundRider();
            for (int i = 0; i < Contacts; i++) contacts[i] = new RxSoundContact();
            for (int i = 0; i < Cues; i++) cues[i] = new RxSoundCue();
            for (int i = 0; i < Channels; i++) channelMix[i] = 1f;
        }
        /// The first-person camera is the local player's view (rx_session_first_person_view: entity class 0x1002 with its pose valid; the camera sound
        /// effect's active flag and the foley emitter's test, 0068bfb0). firstPersonKnown false: the core has no such export (an older rxcore); the host approximates.
        public bool firstPersonKnown, firstPersonView;
        public void Clear() { valid = false; riderCount = 0; firstPersonKnown = firstPersonView = false; }
        public void CopyFrom(RxSoundState o)
        {
            firstPersonKnown = o.firstPersonKnown; firstPersonView = o.firstPersonView;
            valid = o.valid; riderCount = o.riderCount; channelCount = o.channelCount; contactSerial = o.contactSerial; cueSerial = o.cueSerial; frame = o.frame;
            Array.Copy(o.channelMix, channelMix, Channels);
            for (int i = 0; i < Riders; i++) riders[i].CopyFrom(o.riders[i]);
            for (int i = 0; i < Contacts; i++) if (contacts[i].serial != o.contacts[i].serial) contacts[i].CopyFrom(o.contacts[i]);
            crowdValid = o.crowdValid; crowdType = o.crowdType; crowdSize = o.crowdSize; crowdSizeRequested = o.crowdSizeRequested; crowdState = o.crowdState;
            crowdMomentum = o.crowdMomentum; reactionSerial = o.reactionSerial;
            Array.Copy(o.reactionSerials, reactionSerials, Reactions); Array.Copy(o.reactionKinds, reactionKinds, Reactions);
            for (int i = 0; i < Cues; i++) { cues[i].serial = o.cues[i].serial; cues[i].index = o.cues[i].index; cues[i].path = o.cues[i].path; }
            musicValid = o.musicValid; musicMode = o.musicMode; musicKind = o.musicKind; musicRequested = o.musicRequested; musicCurrent = o.musicCurrent;
            scoreRequested = o.scoreRequested; scoreCurrent = o.scoreCurrent; musicPosition = o.musicPosition; musicCount = o.musicCount;
            musicFlags = o.musicFlags; musicSerial = o.musicSerial; musicDelay = o.musicDelay; musicTransition = o.musicTransition; musicStartDelay = o.musicStartDelay;
            Array.Copy(o.playlist, playlist, Playlist); musicTrack = o.musicTrack; musicScore = o.musicScore;
            riderSoundSerial = o.riderSoundSerial; Array.Copy(o.riderSoundSerials, riderSoundSerials, RiderSounds); Array.Copy(o.riderSoundVehicles, riderSoundVehicles, RiderSounds); Array.Copy(o.riderSoundSlots, riderSoundSlots, RiderSounds);
        }

        static int I(byte[] b, int o) => BitConverter.ToInt32(b, o);
        static uint U(byte[] b, int o) => BitConverter.ToUInt32(b, o);
        static float F(byte[] b, int o) => BitConverter.ToSingle(b, o);
        static string S(byte[] b, int o, int max, string old)
        {
            int n = 0; while (n < max && b[o + n] != 0) n++;
            if (n == 0) return "";
            if (old != null && old.Length == n) { bool same = true; for (int i = 0; i < n && same; i++) same = old[i] == (char)b[o + i]; if (same) return old; }
            return Encoding.ASCII.GetString(b, o, n);
        }
        /// Decodes the native layout (little endian).
        public void Decode(byte[] b)
        {
            valid = U(b, 8) != 0; riderCount = Math.Max(0, Math.Min(Riders, I(b, 12))); channelCount = Math.Max(0, Math.Min(Channels, I(b, 16)));
            contactSerial = U(b, 20); cueSerial = U(b, 24); frame = U(b, 28);
            for (int k = 0; k < Channels; k++) channelMix[k] = F(b, OffMix + 4 * k);
            for (int i = 0; i < riderCount; i++)
            {
                int o = i < RidersV1 ? OffRiders + i * RiderBytes : V1Size + (i - RidersV1) * RiderBytes; var r = riders[i];
                r.slot = I(b, o); r.vehicle = U(b, o + 4); r.wheelCount = Math.Max(0, Math.Min(4, I(b, o + 8))); r.vehicleClass = I(b, o + 12);
                r.soundId = I(b, o + 16); r.material = I(b, o + 20); r.vx = F(b, o + 24); r.vy = F(b, o + 28); r.vz = F(b, o + 32); r.soundFeature0 = I(b, o + 36);
                for (int k = 0; k < 4; k++)
                {
                    int w = o + 48 + k * WheelBytes; var d = r.wheels[k];
                    d.px = F(b, w); d.py = F(b, w + 4); d.pz = F(b, w + 8); d.nx = F(b, w + 12); d.ny = F(b, w + 16); d.nz = F(b, w + 20);
                    d.sideSlip2 = F(b, w + 24); d.rollSlip2 = F(b, w + 28); d.suspensionForce = F(b, w + 32); d.angularVelocity = F(b, w + 36);
                    d.flags = U(b, w + 40); d.depth = F(b, w + 44); d.zone = I(b, w + 48); d.contactId = I(b, w + 52); d.contact = U(b, w + 56);
                    d.contactClass = I(b, w + 60); d.contactSound = I(b, w + 64); d.contactMaterial = I(b, w + 68);
                }
            }
            for (int i = 0; i < Contacts; i++)
            {
                int o = OffContacts + i * ContactBytes; var c = contacts[i];
                uint serial = U(b, o);
                if (serial == c.serial) continue;
                c.serial = serial; c.frame = U(b, o + 4); c.a = U(b, o + 8); c.b = U(b, o + 12); c.classA = I(b, o + 16); c.classB = I(b, o + 20);
                c.soundA = I(b, o + 24); c.soundB = I(b, o + 28); c.materialA = I(b, o + 32); c.materialB = I(b, o + 36); c.flags = U(b, o + 40);
                c.strengthA = F(b, o + 44); c.strengthB = F(b, o + 48); c.soundOverride = I(b, o + 52); c.featureA = I(b, o + 56); c.featureB = I(b, o + 60);
                c.px = F(b, o + 64); c.py = F(b, o + 68); c.pz = F(b, o + 72); c.vocalA = U(b, o + 76); c.vocalB = U(b, o + 80);
                c.slideSpeed = F(b, o + 84); c.slideKind = I(b, o + 88); c.zone = I(b, o + 92); c.objectMaterial = I(b, o + 96);
            }
            int cr = OffCrowd;
            crowdValid = U(b, cr) != 0; crowdType = I(b, cr + 4); crowdSize = I(b, cr + 8); crowdSizeRequested = I(b, cr + 12); crowdMomentum = F(b, cr + 16);
            crowdState = I(b, cr + 20); reactionSerial = U(b, cr + 24);
            for (int k = 0; k < Reactions; k++) { reactionSerials[k] = U(b, cr + 36 + 8 * k); reactionKinds[k] = I(b, cr + 40 + 8 * k); }
            for (int i = 0; i < Cues; i++)
            {
                int o = OffCues + i * CueBytes; var c = cues[i];
                uint serial = U(b, o);
                if (serial == c.serial && serial != 0) continue;
                c.serial = serial; c.index = I(b, o + 4); c.path = S(b, o + 8, CuePath, c.path);
            }
            int m = OffMusic;
            musicValid = U(b, m) != 0; musicMode = I(b, m + 4); musicFlags = U(b, m + 8); musicKind = I(b, m + 12); musicDelay = F(b, m + 16);
            musicRequested = I(b, m + 20); musicCurrent = I(b, m + 24); scoreRequested = I(b, m + 28); scoreCurrent = I(b, m + 32);
            musicPosition = I(b, m + 36); musicCount = I(b, m + 40); musicTransition = F(b, m + 44); musicStartDelay = F(b, m + 48); musicSerial = U(b, m + 52);
            for (int k = 0; k < Playlist; k++) playlist[k] = I(b, m + 64 + 4 * k);
            musicTrack = S(b, m + 192, Name, musicTrack); musicScore = S(b, m + 224, Name, musicScore);
            riderSoundSerial = U(b, OffRiderSounds);
            for (int k = 0; k < RiderSounds; k++) { int o = OffRiderSounds + 4 + 16 * k; riderSoundSerials[k] = U(b, o); riderSoundVehicles[k] = U(b, o + 4); riderSoundSlots[k] = I(b, o + 8); }
        }
    }

    internal static class RxSoundNative
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ReadFn(IntPtr session, byte[] value);
        static readonly RxNativeLoader.Fn<ReadFn> read = new RxNativeLoader.Fn<ReadFn>("rx_session_sound");
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int ViewFn(IntPtr session);
        static readonly RxNativeLoader.Fn<ViewFn> firstPerson = new RxNativeLoader.Fn<ViewFn>("rx_session_first_person_view");
        static bool hasFirstPerson = true; static int firstPersonGeneration = -1;
        /// rx_session_sound into `scratch` (RxSoundState.Size bytes) and decoded into `into`. 0 = read.
        internal static int Read(IntPtr session, byte[] scratch, RxSoundState into)
        {
            const uint size = RxSoundState.Size;
            scratch[0] = unchecked((byte)size); scratch[1] = unchecked((byte)(size >> 8)); scratch[2] = 0; scratch[3] = 0;
            scratch[4] = (byte)RxSoundState.Version; scratch[5] = 0; scratch[6] = 0; scratch[7] = 0;
            int rc = read.Get()(session, scratch);
            if (rc == 0) into.Decode(scratch); else into.Clear();
            into.firstPersonKnown = false; into.firstPersonView = false;
            if (rc == 0)
            {
                if (firstPersonGeneration != RxNativeLoader.Generation) { firstPersonGeneration = RxNativeLoader.Generation; hasFirstPerson = true; }
                if (hasFirstPerson)
                    try { into.firstPersonView = firstPerson.Get()(session) != 0; into.firstPersonKnown = true; }
                    catch (EntryPointNotFoundException) { hasFirstPerson = false; }
            }
            return rc;
        }
    }
}
