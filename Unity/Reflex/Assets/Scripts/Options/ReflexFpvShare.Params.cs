// CAMERA SHARING, the Dynamic FPV section (the "fpv" section of a camera config: CameraShareCodec.cs): its parameters and its upgrades. No UnityEngine
// in this file (tests/camshare/codec_test.sh compiles it with the codec); reading and applying the settings in use: ReflexFpvShare.cs.
//   version 1 (w-camshare, the 7a9c99ff camera)   the ReflexFpv.* keys of the old camera, keyed by those names
//   version 2 (w-fpvrewrite .. w-fpvfix)          the rewrite's FpvConfig fields (lookDown, horizonLock, headMotion, shake, ... 40 keys)
//   version 3 (w-fpvrevert)                       the old camera again: version 1's keys + headBob, headSmooth, viewSmooth (w-fpvrevert's)
// Reading an older code: version 1's keys are version 3's (the three new ones take their defaults). Version 2 is taken best-effort, as a save's
// Fpv.Config is (ReflexSettings.MigrateFpvConfig): a value the sender changed from the rewrite's default translates (mode, fov, free, offX / offY /
// offZ, trueSmooth by name; lookDown -> pitch = (lookDown - 15) / 20; horizonLock -> tilt = 1 - horizonLock); a value at the rewrite's default, and
// every key of the rewrite's neck model, is left out (the restored camera's default). New codes are written as version 3: a build of version 1 or 2
// reads them key by key (the shared names: version 1 every key but the three new ones, version 2 mode / fov / free / the offsets / trueSmooth).
using System;
using System.Collections.Generic;

namespace Reflex.Port.Options
{
    public static partial class ReflexFpvShare
    {
        public const int Version = 3;

        /// The FPV settings: key (= the ReflexFpv.* save key), label, default and range (the FIRST-PERSON VIEW rows' own; the defaults are
        /// OriginalOnboardCamera's Default* constants).
        public static CameraShareParam[] Params() => new[]
        {
            CameraShareParam.Choice("mode", "HEAD MODE", 1, "CHASSIS", "DYNAMIC", "TRUE"),
            CameraShareParam.Number("fov", "FIELD OF VIEW", 116, 40, 140, "°"),
            CameraShareParam.Number("tilt", "TILT", 0, 0, 1),
            CameraShareParam.Number("pitch", "PITCH", 0.31, -1, 1),
            CameraShareParam.Switch("leanPitch", "LEAN PITCH", true),
            CameraShareParam.Choice("ca", "CORNER ANTICIPATION", 0, "OFF", "TRACK", "LEAN"),
            CameraShareParam.Number("caScale", "CORNER SCALE", 0.5, 0, 1),
            CameraShareParam.Number("leanHead", "LEAN HEADING", 35.2, 0, 60),
            CameraShareParam.Switch("free", "FREE LOOK", false),
            CameraShareParam.Switch("shift", "HEAD SHIFT", false),
            CameraShareParam.Number("shiftLong", "SHIFT LONG", 1, 0, 3),
            CameraShareParam.Number("shiftLat", "SHIFT LAT", 1, 0, 3),
            CameraShareParam.Number("shiftVert", "SHIFT VERT", 1, 0, 3),
            CameraShareParam.Number("trueSmooth", "TRUE SMOOTHING", 0.35, 0, 1),
            CameraShareParam.Number("trueRoll", "TRUE HEAD ROLL", 1, 0, 1),
            CameraShareParam.Number("trueYaw", "TRUE YAW SMOOTH", 0, 0, 1),
            CameraShareParam.Number("truePitch", "TRUE PITCH SMOOTH", 0, 0, 1),
            CameraShareParam.Number("offX", "EYE RIGHT", 0, -OffsetMax, OffsetMax, " FT"),
            CameraShareParam.Number("offY", "EYE UP", 0, -OffsetMax, OffsetMax, " FT"),
            CameraShareParam.Number("offZ", "EYE FORWARD", 0, -OffsetMax, OffsetMax, " FT"),
            CameraShareParam.Number("headBob", "HEAD BOB", DefaultHeadBob, 0, 1),
            CameraShareParam.Number("headSmooth", "HEAD SMOOTHING", DefaultHeadSmooth, 0, 0.25, " S"),
            CameraShareParam.Number("viewSmooth", "VIEW SMOOTHING", DefaultViewSmooth, 0, 0.25, " S"),
        };
        public const double OffsetMax = 2;
        /// (= OriginalOnboardCamera.DefaultHeadBob / DefaultHeadSmoothing / DefaultViewSmoothing)
        public const double DefaultHeadBob = 1, DefaultHeadSmooth = 0.05, DefaultViewSmooth = 0.03;

        /// The rewrite's (version 2) defaults of the keys that translate: a value still at one was not the sender's choice.
        static readonly Dictionary<string, double> V2Defaults = new Dictionary<string, double>(StringComparer.Ordinal)
        {
            { "mode", 1 }, { "fov", 116 }, { "free", 0 }, { "offX", 0 }, { "offY", 0 }, { "offZ", 0 }, { "trueSmooth", 0.35 }, { "lookDown", 20 }, { "horizonLock", 0.7 },
        };

        /// Older codes in this version's keys (CameraShareSection.Upgrade; also tests/camshare).
        public static Dictionary<string, double> Upgrade(int from, Dictionary<string, double> v)
        {
            if (v == null || from >= Version || from <= 1) return v;   // (version 1's keys are this version's)
            var o = new Dictionary<string, double>(StringComparer.Ordinal);
            bool Changed(string k, out double x) => v.TryGetValue(k, out x) && !double.IsNaN(x) && !double.IsInfinity(x) && Math.Abs(x - V2Defaults[k]) > 1e-4;
            foreach (var k in new[] { "mode", "fov", "free", "offX", "offY", "offZ", "trueSmooth" }) if (Changed(k, out double x)) o[k] = x;
            if (Changed("lookDown", out double ld)) o["pitch"] = Math.Max(-1, Math.Min(1, (ld - 15) / 20));
            if (Changed("horizonLock", out double hl)) o["tilt"] = Math.Max(0, Math.Min(1, 1 - hl));
            return o;
        }
    }
}
