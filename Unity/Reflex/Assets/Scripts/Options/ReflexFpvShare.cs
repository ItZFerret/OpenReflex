// CAMERA SHARING: the Dynamic FPV section, registered with the settings in use (THE REGISTRATION POINT for the FPV camera's settings: a share code,
// a FIRST-PERSON SETUP and the setups' summaries carry exactly what is registered here). Parameters, versions and upgrades: ReflexFpvShare.Params.cs.
// Read takes the ReflexFpv.* keys in use (ReflexSettings.Onboard*: an unset key reads as its default); Apply writes every key and reloads a running
// FPV camera (ReflexClientLive.FpvChanged).
using System;
using System.Collections.Generic;

namespace Reflex.Port.Options
{
    public static partial class ReflexFpvShare
    {
        const string P = "ReflexFpv.";

        /// The "fpv" section (ReflexCameraShare's start-up).
        public static void RegisterDefault()
        {
            var sec = new CameraShareSection(CameraShare.FpvId, Version, "DYNAMIC FPV", Params()) { Upgrade = Upgrade };
            sec.Read = () =>
            {
                ReflexSettings.MigrateFpvConfig();
                var d = new Dictionary<string, double>(StringComparer.Ordinal);
                foreach (var p in sec.Params)
                {
                    switch (p.Key)
                    {   // (the three shift gains fall back to the old single strength, as the options rows read them)
                        case "shiftLong": d[p.Key] = ReflexSettings.OnboardShiftLong; break;
                        case "shiftLat": d[p.Key] = ReflexSettings.OnboardShiftLat; break;
                        case "shiftVert": d[p.Key] = ReflexSettings.OnboardShiftVert; break;
                        default: d[p.Key] = p.Clean(p.Kind == CameraShareKind.Number ? ReflexSave.GetFloat(P + p.Key, (float)p.Default) : ReflexSave.GetInt(P + p.Key, (int)p.Default)); break;
                    }
                }
                return d;
            };
            sec.Apply = values =>
            {
                ReflexSettings.MigrateFpvConfig();   // (first: the rewrite's settings never overwrite a setup taken later)
                foreach (var p in sec.Params)
                {
                    double v = values != null && values.TryGetValue(p.Key, out double x) ? p.Clean(x) : p.Clean(p.Default);
                    if (p.Kind == CameraShareKind.Number) ReflexSave.SetFloat(P + p.Key, (float)v); else ReflexSave.SetInt(P + p.Key, (int)v);
                }
                // (HEAD SHIFT STRENGTH: the three gains' mean, the old single key the gains fall back to)
                ReflexSave.SetFloat(P + "shiftStrength", (ReflexSettings.OnboardShiftLong + ReflexSettings.OnboardShiftLat + ReflexSettings.OnboardShiftVert) / 3f);
                ReflexClientLive.FpvChanged();   // (a running FPV camera reads them again: live; saved)
            };
            CameraShare.Register(sec);
        }
    }
}
