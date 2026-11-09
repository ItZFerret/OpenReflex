// The camera share codec's tests outside Unity (w-camshare): Unity/Reflex/Assets/Scripts/Options/CameraShareCodec.cs alone, on the SDK's dotnet.
// Run: tests/camshare/codec_test.sh <outdir>. Prints one line per check and "CODEC PASS" / "CODEC FAIL n".
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using Reflex.Port.Options;

static class CameraShareCodecTests
{
    static int s_fail, s_ok;
    static void Check(bool ok, string what) { if (ok) s_ok++; else s_fail++; Console.WriteLine((ok ? "ok   " : "FAIL ") + what); }

    // the chase section as the client registers it (ReflexCameraShare.ChaseSection: the presets' ranges)
    static CameraShareSection Chase() => new CameraShareSection(CameraShare.ChaseId, 1, "CAMERA", new[]
    {
        CameraShareParam.Choice("base", "BASE VIEW", 1, "VISUAL", "GAME PLAY", "USER"),
        CameraShareParam.Choice("fovMode", "FIELD OF VIEW", 0, "SCALE", "FIXED"),
        CameraShareParam.Whole("fovScale", "FOV SCALE", 100, 50, 200, "%"),
        CameraShareParam.Whole("fovDegrees", "FOV DEGREES", 85, 40, 140, "°"),
        CameraShareParam.Number("distance", "DISTANCE", 0, -10, 30, " FT"),
        CameraShareParam.Number("height", "HEIGHT", 0, -6, 20, " FT"),
        CameraShareParam.Number("side", "SIDE", 0, -12, 12, " FT"),
        CameraShareParam.Number("pitch", "PITCH", 0, -25, 25, "°"),
        CameraShareParam.Number("posLag", "POSITION LAG", 0, 0, 1, " S"),
        CameraShareParam.Number("rotLag", "ROTATION LAG", 0, 0, 1, " S"),
        CameraShareParam.Number("roll", "ROLL WITH BIKE", 0, 0, 1),
        CameraShareParam.Number("shake", "SHAKE", 1, 0, 2),
        CameraShareParam.Number("shakeSpeed", "SHAKE: SPEED", 0, 0, 1),
        CameraShareParam.Number("shakeLanding", "SHAKE: LANDINGS", 0, 0, 1),
        CameraShareParam.Number("shakeRough", "SHAKE: ROUGH GROUND", 0, 0, 1),
    });

    // today's FPV section (ReflexFpvShare.Params), version 1
    static CameraShareParam[] FpvV1Params() => new[]
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
        CameraShareParam.Number("offX", "EYE RIGHT", 0, -2, 2, " FT"),
        CameraShareParam.Number("offY", "EYE UP", 0, -2, 2, " FT"),
        CameraShareParam.Number("offZ", "EYE FORWARD", 0, -2, 2, " FT"),
    };
    static CameraShareSection FpvV1() => new CameraShareSection(CameraShare.FpvId, 1, "DYNAMIC FPV", FpvV1Params());

    // a rewritten FPV (what w-fpvrewrite might register): version 2, "pitch" (0..1 of 20 degrees + 15) became "pitchDeg", "tilt" dropped, "nearClip" new
    static CameraShareSection FpvV2() => new CameraShareSection(CameraShare.FpvId, 2, "DYNAMIC FPV", FpvV1Params().Where(p => p.Key != "pitch" && p.Key != "tilt")
        .Concat(new[] { CameraShareParam.Number("pitchDeg", "PITCH", 21.2, -5, 35, "°"), CameraShareParam.Number("nearClip", "NEAR CLIP", 0.1, 0.02, 0.5) }))
    {
        Upgrade = (from, v) => { var d = new Dictionary<string, double>(v); if (from < 2 && d.TryGetValue("pitch", out double p)) { d["pitchDeg"] = p * 20 + 15; d.Remove("pitch"); } return d; },
    };

    static string Code(string json)
    {
        byte[] raw = Encoding.UTF8.GetBytes(json);
        using var ms = new MemoryStream();
        using (var z = new DeflateStream(ms, CompressionLevel.Optimal, true)) z.Write(raw, 0, raw.Length);
        return CameraConfig.SharePrefix + Convert.ToBase64String(ms.ToArray()).TrimEnd('=').Replace('+', '-').Replace('/', '_');
    }
    static string CodeOfBytes(byte[] raw)
    {
        using var ms = new MemoryStream();
        using (var z = new DeflateStream(ms, CompressionLevel.Optimal, true)) z.Write(raw, 0, raw.Length);
        return CameraConfig.SharePrefix + Convert.ToBase64String(ms.ToArray()).TrimEnd('=').Replace('+', '-').Replace('/', '_');
    }

    static CameraConfig Random(Random r, bool chase, bool fpv)
    {
        string Odd(int n) { var sb = new StringBuilder(); string pool = "ABCxyz09 _-\"\\/'é漢\t\n{}[]:,ORCAM1"; for (int i = 0; i < n; i++) sb.Append(pool[r.Next(pool.Length)]); return sb.ToString(); }
        var c = new CameraConfig { name = Odd(r.Next(1, 30)), author = Odd(r.Next(0, 20)), description = Odd(r.Next(0, 200)), game = "0.9.test" };
        foreach (var (on, id) in new[] { (chase, CameraShare.ChaseId), (fpv, CameraShare.FpvId) })
        {
            if (!on) continue;
            var sec = CameraShare.Section(id);
            var v = new Dictionary<string, double>();
            foreach (var p in sec.Params) v[p.Key] = p.Min + r.NextDouble() * (p.Max - p.Min);
            c.Put(id, v);
        }
        return c;
    }

    static bool Same(CameraConfig a, CameraConfig b, out string why)
    {
        why = "";
        if (b == null) { why = "null"; return false; }
        if (a.name != b.name || a.author != b.author || a.description != b.description || a.game != b.game) { why = "text: '" + a.name + "' / '" + b.name + "'"; return false; }
        if (a.Sections.Count != b.Sections.Count) { why = "sections " + a.Sections.Count + " / " + b.Sections.Count; return false; }
        foreach (var s in a.Sections)
        {
            var t = b.Section(s.Id);
            if (t == null || t.Version != s.Version || t.Values.Count != s.Values.Count) { why = "section " + s.Id; return false; }
            foreach (var kv in s.Values)
                if (!t.Values.TryGetValue(kv.Key, out double v) || BitConverter.DoubleToInt64Bits(v) != BitConverter.DoubleToInt64Bits(kv.Value)) { why = s.Id + "." + kv.Key + " " + kv.Value.ToString("R") + " / " + v.ToString("R"); return false; }
        }
        return true;
    }

    static CameraConfig Read(string text, out string err) => CameraConfig.FromShareText(text, out err);
    static bool Refused(string text, string what, string expect = null)
    {
        var c = Read(text, out string err);
        bool ok = c == null && err != null && (expect == null || err.IndexOf(expect, StringComparison.OrdinalIgnoreCase) >= 0);
        Check(ok, "refused: " + what + " (" + (err ?? "ACCEPTED") + ")");
        return ok;
    }

    static int Main()
    {
        // ------------------------------------------------------------------ the registry
        CameraShare.Register(Chase());
        Check(CameraShare.RegisterDefault(FpvV1()), "RegisterDefault fills an empty id");
        var owner = FpvV1(); owner.Label = "OWNER";
        Check(!CameraShare.RegisterDefault(FpvV1()) && CameraShare.Section("fpv").Label == "DYNAMIC FPV", "RegisterDefault never replaces");
        CameraShare.Register(owner);
        Check(CameraShare.Section("fpv") == owner, "Register replaces (the FPV code's own set wins)");
        Check(!CameraShare.RegisterDefault(FpvV1()) && CameraShare.Section("fpv") == owner, "a default registered after the owner does not replace it");
        CameraShare.Register(FpvV1());

        // ------------------------------------------------------------------ round trips
        var rnd = new Random(20261103);
        int trips = 0; string firstWhy = null;
        for (int i = 0; i < 300; i++)
        {
            var c = Random(rnd, i % 3 != 1, i % 3 != 0);
            c.name = CameraConfig.CleanText(c.name, CameraConfig.MaxName); if (c.name.Length == 0) c.name = "Imported camera";
            c.author = CameraConfig.CleanText(c.author, CameraConfig.MaxAuthor); c.description = CameraConfig.CleanText(c.description, CameraConfig.MaxNotes, true);
            var a = CameraConfig.FromJson(c.ToJson());
            var b = CameraConfig.FromJson(c.ToJson(false));
            var d = Read(c.ShareCode(), out _);
            var e = Read("hey, try my camera: " + c.ShareCode() + " (thanks!)", out _);
            if (Same(c, a, out var w1) && Same(c, b, out var w2) && Same(c, d, out var w3) && Same(c, e, out var w4)) trips++;
            else firstWhy ??= "#" + i + " " + (Same(c, a, out w1) ? Same(c, b, out w2) ? Same(c, d, out w3) ? "chat: " + (Same(c, e, out w4) ? "" : w4) : "code: " + w3 : "compact: " + w2 : "json: " + w1);
        }
        Check(trips == 300, "300 random configs round-trip bit for bit through JSON, compact JSON, the share code and a code inside a chat line (" + trips + (firstWhy != null ? "; " + firstWhy : "") + ")");
        var sample = Random(new Random(7), true, true); sample.name = "CLOSE CHASE"; sample.author = "ITSWILLYFERRET"; sample.description = "";
        string sampleCode = sample.ShareCode();
        Console.WriteLine("     sample code (chase + fpv): " + sampleCode.Length + " characters");
        Check(sampleCode.Length < 1200, "a two-section code stays short (" + sampleCode.Length + " characters)");
        var only = new CameraConfig { name = "MY FPV" }; only.Put("fpv", FpvV1().Defaults());
        Check(CameraConfig.SameValues(Read(only.ShareCode(), out _).Section("fpv").Values, FpvV1().Defaults()), "the FPV defaults round-trip");
        Check(Read("orcam1:" + sampleCode.Substring(CameraConfig.SharePrefix.Length), out _) != null, "the prefix is found in lower case too");
        Check(Read(sampleCode + "==", out _) != null, "base64 padding is accepted");
        Check(Read(sample.ToJson(), out _) != null, "a config's JSON pasted as text is accepted");

        // ------------------------------------------------------------------ versioning
        Refused("{\"format\":\"open-reflex-camera\",\"version\":2,\"sections\":{\"chase\":{\"version\":1,\"values\":{}}}}", "a newer envelope", "newer");
        var nv = Read(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"N\",\"sections\":{\"fpv\":{\"version\":7,\"values\":{\"fov\":100,\"warp\":3,\"free\":true}}}}"), out string nerr);
        Check(nv != null && nv.Section("fpv").Newer && nv.Section("fpv").Get("fov", 0) == 100 && nv.Section("fpv").Get("free", 0) == 1 && nv.Section("fpv").Unknown == 1 && nv.OtherVersion,
              "a newer section version: read key by key, flagged newer, its unknown key ignored (" + nerr + ")");
        var miss = Read(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"M\",\"sections\":{\"chase\":{\"version\":1,\"values\":{\"distance\":5,\"zoom\":9,\"drone\":{\"x\":1}}}}}"), out _);
        var ms = miss?.Section("chase");
        Check(ms != null && ms.Get("distance", 0) == 5 && ms.Get("shake", -1) == 1 && ms.Get("fovScale", -1) == 100 && ms.Get("base", -1) == 1 && ms.Missing == 14 && ms.Unknown == 2 && !ms.Values.ContainsKey("zoom"),
              "missing keys take their defaults (14), unknown keys are ignored (2: a number and an object)");
        var unk = Read(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"U\",\"sections\":{\"drone\":{\"version\":1,\"values\":{\"x\":1}},\"chase\":{\"values\":{\"side\":2}}}}"), out _);
        Check(unk != null && unk.UnknownSections == 1 && unk.Sections.Count == 1 && unk.Section("chase").Get("side", 0) == 2, "an unknown section is ignored (counted); a section without a version reads as version 1");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"sections\":{\"drone\":{\"version\":1,\"values\":{\"x\":1}}}}"), "only sections this build does not know", "does not have");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"sections\":{}}"), "no sections", "no camera settings");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"version\":1}"), "no sections at all", "no camera settings");

        // the FPV rewrite: a v1 code into a v2 section with an Upgrade, a v2 code into today's v1 section
        var v1Code = only.ShareCode();
        var v1Pitch = new CameraConfig { name = "OLD" }; var pv = FpvV1().Defaults(); pv["pitch"] = 0.5; pv["tilt"] = 0.4; v1Pitch.Put("fpv", pv); string oldCode = v1Pitch.ShareCode();
        CameraShare.Register(FpvV2());
        var up = Read(oldCode, out _)?.Section("fpv");
        Check(up != null && up.Version == 2 && Math.Abs(up.Get("pitchDeg", 0) - 25) < 1e-6 && !up.Values.ContainsKey("pitch") && !up.Values.ContainsKey("tilt") && up.Get("nearClip", 0) == (double)(float)0.1 && up.Missing == 1 && up.Unknown == 1,
              "a v1 FPV code read by a v2 section: Upgrade turned pitch 0.5 into pitchDeg 25, tilt ignored, nearClip at its default");
        var v2 = new CameraConfig { name = "NEW" }; var v2v = FpvV2().Defaults(); v2v["pitchDeg"] = 30; v2v["fov"] = 90; v2.Put("fpv", v2v); string newCode = v2.ShareCode();
        Check(Read(newCode, out _)?.Section("fpv").Get("pitchDeg", 0) == 30, "a v2 code round-trips in a v2 build");
        CameraShare.Register(FpvV1());
        var down = Read(newCode, out _)?.Section("fpv");
        Check(down != null && down.Newer && down.Get("fov", 0) == 90 && down.Get("pitch", 0) == (double)(float)0.31 && down.Unknown == 2, "a v2 code in today's v1 build: flagged newer, fov kept, pitch at its default, 2 unknown keys ignored");
        Check(Read(v1Code, out _) != null, "today's code still reads in today's build");

        // ------------------------------------------------------------------ values cleaned
        var dirty = Read(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"D\",\"sections\":{\"chase\":{\"version\":1,\"values\":{" +
                              "\"distance\":1e300,\"height\":-1e400,\"side\":\"7\",\"pitch\":null,\"posLag\":[1],\"base\":7,\"fovMode\":-3,\"fovScale\":149.6,\"fovDegrees\":true,\"roll\":0.5}}," +
                              "\"fpv\":{\"version\":1,\"values\":{\"free\":true,\"shift\":2,\"leanPitch\":false,\"ca\":1.5}}}}"), out string derr);
        var dc = dirty?.Section("chase"); var df = dirty?.Section("fpv");
        Check(dc != null && dc.Get("distance", 0) == 30 && dc.Get("height", 9) == 0 && dc.Get("side", 9) == 0 && dc.Get("pitch", 9) == 0 && dc.Get("posLag", 9) == 0 && dc.Get("base", 0) == 2 && dc.Get("fovMode", 9) == 0 &&
              dc.Get("fovScale", 0) == 150 && dc.Get("fovDegrees", 0) == 40 && dc.Get("roll", 0) == 0.5, "values clamped (1e300 -> 30, a choice 7 -> 2, -3 -> 0), rounded (149.6 -> 150), true as 1 (-> 40 min), -inf / text / null / arrays at the default (" + derr + ")");
        Check(df != null && df.Get("free", 0) == 1 && df.Get("shift", 0) == 1 && df.Get("leanPitch", 1) == 0 && df.Get("ca", 0) == 2, "switches and choices: true / false, 2 -> 1, 1.5 -> 2");
        var ctl = Read(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"A\\u0000B\\u001bC\\nD\",\"author\":\"" + new string('x', 500) + "\",\"description\":\"line 1\\nline 2\\u0007\",\"sections\":{\"chase\":{}}}"), out _);
        Check(ctl != null && ctl.name == "ABCD" && ctl.author.Length == CameraConfig.MaxAuthor && ctl.description == "line 1\nline 2", "text: control characters dropped (notes keep line breaks), the author clipped to " + CameraConfig.MaxAuthor);

        // ------------------------------------------------------------------ garbage
        Refused(null, "null"); Refused("", "empty"); Refused("   ", "spaces"); Refused("hello there", "chat without a code", "no camera share code");
        Refused("ORCAM1:", "the prefix alone", "empty"); Refused("ORCAM1:!!!!", "punctuation after the prefix", "empty");
        Refused("ORCAM1:A", "one base64 character", "cut short"); Refused("ORCAM1:abcd", "base64 that is not deflate", "damaged");
        Refused(CodeOfBytes(new byte[] { 0xff, 0xfe, 0xfd, 0x80, 0x81 }), "deflate of invalid UTF-8", "damaged");
        Refused(Code("this is not json"), "deflate of text", "not JSON"); Refused(Code("[1,2,3]"), "a JSON array", "not a camera config");
        Refused(Code("{\"format\":\"open-reflex-physics\",\"version\":1}"), "a physics config", "not an OPEN REFLEX camera config");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"sections\":{\"chase\":{\"values\":{\"distance\":1}}}} trailing"), "text after the JSON", "not JSON");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"name\":\"unterminated}"), "an unterminated string", "not JSON");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"name\":\"bad \\q escape\"}"), "a bad escape", "not JSON");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"x\":" + new string('[', 9) + new string(']', 9) + "}"), "nesting 10 deep", "nested too deeply");
        Refused(Code(new string('[', 60000)), "60,000 open brackets", "nested too deeply");
        Refused(Code(new string('[', 100000)), "100,000 open brackets (over the inflate limit)", "unpacks too large");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"sections\":{\"chase\":{\"values\":{\"distance\":NaN}}}}"), "NaN", "not JSON");
        Refused(Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"sections\":{\"chase\":{\"values\":{\"distance\":" + new string('9', 60) + "}}}}"), "a 60-digit number", "not JSON");
        Refused("{\"format\":\"open-reflex-camera\"", "a cut JSON", "not JSON");
        // truncated codes: every cut of a real code is refused or reads cleanly, never throws
        int cuts = 0, threw = 0;
        for (int n = CameraConfig.SharePrefix.Length; n < sampleCode.Length; n += 3)
        {
            try { var c = Read(sampleCode.Substring(0, n), out string e); if (c == null && e != null) cuts++; } catch (Exception) { threw++; }
        }
        Check(threw == 0 && cuts > 0, "every truncation of a code is refused without an exception (" + cuts + " refused, " + threw + " threw)");
        // random bytes and random mutations
        int fuzz = 0; threw = 0; var fr = new Random(99);
        for (int i = 0; i < 3000; i++)
        {
            string t;
            if (i % 3 == 0) { var b = new byte[fr.Next(0, 400)]; fr.NextBytes(b); t = CameraConfig.SharePrefix + Convert.ToBase64String(b).Replace('+', '-').Replace('/', '_'); }
            else if (i % 3 == 1) { var ch = sampleCode.ToCharArray(); for (int k = 0; k < 1 + fr.Next(4); k++) ch[CameraConfig.SharePrefix.Length + fr.Next(ch.Length - CameraConfig.SharePrefix.Length)] = "AZaz09-_"[fr.Next(8)]; t = new string(ch); }
            else { var b = new byte[fr.Next(0, 300)]; fr.NextBytes(b); t = CodeOfBytes(b); }
            try { Read(t, out _); fuzz++; } catch (Exception) { threw++; }
        }
        Check(threw == 0, "3000 random / mutated codes: no exception (" + fuzz + " handled)");

        // ------------------------------------------------------------------ oversized
        Refused(CameraConfig.SharePrefix + new string('A', CameraConfig.MaxCode + 1), "a code over " + CameraConfig.MaxCode + " characters", "too long");
        Refused(new string('x', 4 * CameraConfig.MaxCode + 1), "a paste over " + 4 * CameraConfig.MaxCode + " characters", "too long");
        string bomb = Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"" + new string(' ', 4 << 20) + "\"}");
        Console.WriteLine("     a 4 MB JSON deflates to a " + bomb.Length + "-character code");
        Refused(bomb, "a small code that inflates to 4 MB", bomb.Length > CameraConfig.MaxCode ? "too long" : "unpacks too large");
        string bomb2 = Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"" + new string(' ', 200 << 10) + "\"}");
        Refused(bomb2, "a " + bomb2.Length + "-character code that inflates to 200 KB", "unpacks too large");
        Refused("{\"format\":\"open-reflex-camera\",\"version\":1,\"description\":\"" + new string('x', CameraConfig.MaxJson) + "\",\"sections\":{\"chase\":{}}}", "a pasted JSON over " + CameraConfig.MaxJson + " bytes", "too ");
        var many = new StringBuilder("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"K\",\"sections\":{\"chase\":{\"version\":1,\"values\":{");
        for (int i = 0; i < 400; i++) many.Append("\"k").Append(i).Append("\":").Append(i).Append(',');
        many.Append("\"distance\":3}}}}");
        var mk = Read(Code(many.ToString()), out _)?.Section("chase");
        Check(mk != null && mk.Get("distance", 0) == 0 && mk.Unknown == 401 && mk.Missing == 15,
              "a section with 401 keys: only the first " + CameraConfig.MaxKeys + " are looked at (distance, the 401st, stays at its default; all 401 counted unknown)");
        var secs = new StringBuilder("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"S\",\"sections\":{");
        for (int i = 0; i < 40; i++) secs.Append("\"x").Append(i).Append("\":{},");
        secs.Append("\"chase\":{}}}");
        Refused(Code(secs.ToString()), "41 sections (the known one past the first " + CameraConfig.MaxSections + ")", "does not have");

        // ------------------------------------------------------------------ the client's FPV section (ReflexFpvShare.Params.cs, w-fpvrevert: version 3)
        var real = new CameraShareSection(CameraShare.FpvId, ReflexFpvShare.Version, "DYNAMIC FPV", ReflexFpvShare.Params()) { Upgrade = ReflexFpvShare.Upgrade };
        CameraShare.Register(real);
        Check(real.Version == 3 && real.Params.Count == 23 && FpvV1Params().All(p => real.Params.Any(q => q.Key == p.Key && q.Default == p.Default && q.Min == p.Min && q.Max == p.Max)),
              "the restored FPV section: version 3, version 1's 20 keys with their defaults and ranges + headBob / headSmooth / viewSmooth");
        var r1 = Read(oldCode, out _)?.Section("fpv");   // (a version 1 code: pitch 0.5, tilt 0.4)
        Check(r1 != null && r1.Version == 3 && !r1.Newer && Math.Abs(r1.Get("pitch", 0) - 0.5) < 1e-6 && Math.Abs(r1.Get("tilt", 0) - 0.4) < 1e-6 && r1.Get("headBob", 0) == 1 && r1.Missing == 3 && r1.Unknown == 0,
              "a version 1 FPV code in the restored section: every key as it was (pitch 0.5, tilt 0.4), the three new ones at their defaults");
        // a code of the rewrite (version 2): its keys as w-fpvrewrite / w-fpvfix wrote them (40; here the ones that matter, the rest at the rewrite's defaults)
        string RewriteCode(string values) => Code("{\"format\":\"open-reflex-camera\",\"version\":1,\"name\":\"RW\",\"sections\":{\"fpv\":{\"version\":2,\"values\":{" + values + "}}}}");
        var r2 = Read(RewriteCode("\"mode\":2,\"fov\":100,\"lookDown\":30,\"horizonLock\":0.4,\"headMotion\":2,\"shake\":0,\"free\":1,\"offY\":0.25,\"trueSmooth\":0.35,\"neckHz\":3"), out _)?.Section("fpv");
        Check(r2 != null && r2.Version == 3 && r2.Get("mode", 0) == 2 && r2.Get("fov", 0) == 100 && Math.Abs(r2.Get("pitch", 0) - 0.75) < 1e-6 && Math.Abs(r2.Get("tilt", 0) - 0.6) < 1e-6 && r2.Get("free", 0) == 1 &&
              Math.Abs(r2.Get("offY", 0) - 0.25) < 1e-6 && Math.Abs(r2.Get("trueSmooth", 0) - 0.35) < 1e-6 && r2.Get("leanPitch", 0) == 1 && r2.Get("headBob", 0) == 1 && !r2.Values.ContainsKey("lookDown") && !r2.Values.ContainsKey("headMotion"),
              "a version 2 (rewrite) code, best effort: mode / fov / free / offY kept, lookDown 30 -> pitch 0.75, horizonLock 0.4 -> tilt 0.6, the neck model's keys dropped, the rest at the restored defaults");
        var r2d = Read(RewriteCode("\"mode\":1,\"fov\":116,\"lookDown\":20,\"horizonLock\":0.7,\"headMotion\":1,\"shake\":1"), out _)?.Section("fpv");
        Check(r2d != null && CameraConfig.SameValues(r2d.Values, real.Defaults()), "a version 2 code at the rewrite's defaults reads as the restored camera's defaults (pitch 0.31, tilt 0: not 0.25 / 0.3)");
        var r2e = Read(RewriteCode("\"lookDown\":80,\"horizonLock\":-3"), out _)?.Section("fpv");
        Check(r2e != null && r2e.Get("pitch", 0) == 1 && r2e.Get("tilt", 0) == 1, "out of range rewrite values clamp (lookDown 80 -> pitch 1, horizonLock -3 -> tilt 1)");
        var v3 = new CameraConfig { name = "RESTORED" }; var v3v = real.Defaults(); v3v["tilt"] = 0.25; v3v["headBob"] = 0.5; v3v["viewSmooth"] = 0.04; v3.Put("fpv", v3v); string v3Code = v3.ShareCode();
        var r3 = Read(v3Code, out _)?.Section("fpv");
        Check(r3 != null && r3.Version == 3 && CameraConfig.SameValues(r3.Values, v3v) && r3.Missing == 0 && r3.Unknown == 0, "a version 3 code round-trips (tilt 0.25, head bob 0.5, view smoothing 0.04)");
        CameraShare.Register(FpvV1());
        var r3in1 = Read(v3Code, out _)?.Section("fpv");
        Check(r3in1 != null && r3in1.Newer && Math.Abs(r3in1.Get("tilt", 0) - 0.25) < 1e-6 && r3in1.Unknown == 3, "a version 3 code in a version 1 build (before the rewrite): flagged newer, every old key read (tilt 0.25), the 3 new ones unknown");

        // ------------------------------------------------------------------ Put / SameValues
        var put = new CameraConfig(); put.Put("chase", new Dictionary<string, double> { ["distance"] = 99, ["nope"] = 1 });
        Check(put.Section("chase").Get("distance", 0) == 30 && !put.Section("chase").Values.ContainsKey("nope") && put.Section("chase").Values.Count == 15, "Put completes, cleans and drops unregistered keys");
        Check(CameraConfig.SameValues(FpvV1().Defaults(), Read(only.ShareCode(), out _).Section("fpv").Values) && !CameraConfig.SameValues(FpvV1().Defaults(), pv), "SameValues");
        bool threwPut = false; try { new CameraConfig().Put("drone", null); } catch (InvalidOperationException) { threwPut = true; }
        Check(threwPut, "Put of an unregistered section throws");

        Console.WriteLine(s_fail == 0 ? "CODEC PASS (" + s_ok + " checks)" : "CODEC FAIL " + s_fail + " of " + (s_ok + s_fail));
        return s_fail == 0 ? 0 : 1;
    }
}
