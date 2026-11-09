// Run-time readers for the game's FMOD Ex data (ports of _re/formats/fev_parse.py and fsb_export.py):
//   * FEV1 v0x3a event projects ("sound" assets: a 0x88-byte wrapper + the FEV1 image),
//   * FSB3/FSB4 wave banks ("soundbnk" assets: a wrapper + the FSB), their sample headers and the codecs:
//     MPEG Layer III (every shipped bank; FMOD pads frames to 16 bytes, the padding is stripped to a clean MP3 stream),
//     PCM8/PCM16 and FMOD IMA ADPCM (not used by the shipped banks, covered by a synthetic test).
// Nothing here touches the Unity API, so it can run on any thread. Semantics: _re/notes/game-audio.md, engine-audio.md.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Reflex.Port.Content
{
    public static class ReflexFmod
    {
        // ================================================================================================ FEV
        public sealed class FevReader
        {
            public readonly byte[] d; public int p;
            public FevReader(byte[] data, int pos = 0) { d = data; p = pos; }
            void Need(int n) { if (p < 0 || n < 0 || p > d.Length - n) throw new InvalidDataException("FEV: read past the end"); }
            public uint U32() { Need(4); uint v = BitConverter.ToUInt32(d, p); p += 4; return v; }
            public int I32() { Need(4); int v = BitConverter.ToInt32(d, p); p += 4; return v; }
            public float F32() { Need(4); float v = BitConverter.ToSingle(d, p); p += 4; return v; }
            public ushort U16() { Need(2); ushort v = BitConverter.ToUInt16(d, p); p += 2; return v; }
            public short I16() { Need(2); short v = BitConverter.ToInt16(d, p); p += 2; return v; }
            public byte[] Raw(int n) { Need(n); var v = new byte[n]; Buffer.BlockCopy(d, p, v, 0, n); p += n; return v; }
            public string S()
            {
                uint n = U32(); Need((int)Math.Min(n, int.MaxValue));
                int e = (int)n; while (e > 0 && d[p + e - 1] == 0) e--;      // rstrip(b"\0"), latin-1
                string s = Encoding.GetEncoding(28591).GetString(d, p, e); p += (int)n; return s;
            }
            public int Count() { uint n = U32(); if (n > 1_000_000) throw new InvalidDataException("FEV: implausible count " + n); return (int)n; }
        }

        public const uint FevVersion = 0x003a0000;
        public static readonly string[] PlayModes = { "sequential", "random", "random_no_repeat", "sequential_event_restart", "shuffle", "programmer", "shuffle_global" };

        public sealed class Point { public float x, y; public uint shape; }
        public sealed class Envelope
        {
            public int dspEffectIndex; public string name; public uint dspParamIndex, flags, flags2; public Point[] points;
            public uint controlParam; public bool logScale;
            // fev_parse.ENV_FLAGS: 0x8 volume, 0x10 pitch, 0x20 pan, 0x40 time_offset, 0x80 surround_pan, 0x100 3d_speaker_spread,
            // 0x200 reverb_level, 0x400 3d_pan_level, 0x800 reverb_balance, 0x1000 spawn_intensity. No bit (only 0x4) = a DSP parameter.
            static readonly (uint bit, string name)[] Bits = { (0x8, "volume"), (0x10, "pitch"), (0x20, "pan"), (0x40, "time_offset"), (0x80, "surround_pan"),
                (0x100, "3d_speaker_spread"), (0x200, "reverb_level"), (0x400, "3d_pan_level"), (0x800, "reverb_balance"), (0x1000, "spawn_intensity") };
            /// audio_export.norm_envelope: spawn_intensity > volume > pitch > first other bit > "dsp".
            public string Kind
            {
                get
                {
                    if ((flags & 0x1000) != 0) return "spawn_intensity";
                    if ((flags & 0x8) != 0) return "volume";
                    if ((flags & 0x10) != 0) return "pitch";
                    foreach (var b in Bits) if ((flags & b.bit) != 0) return b.name;
                    return "dsp";
                }
            }
        }
        public sealed class SoundInstance
        {
            public ushort soundDef; public float start, length; public uint autopitchEnabled, loopWord; public int autopitchParamOrUnk34;
            public uint unk38, unk28, unk2c, unk30; public float volume, fadeIn, fadeOut; public uint fadeInCurve, fadeOutCurve;
            public bool Loop => loopWord == 0;     // LOOP_WORD: 0 loop, 1 oneshot, 2 loop_and_cancel
        }
        public sealed class Layer { public ushort flags; public short priority; public ushort controlParam; public SoundInstance[] instances; public Envelope[] envelopes; }
        public sealed class Parameter { public string name; public float initial, min, max; public uint flags, unk1c, unk2c; public float[] keyOffsets; }
        public sealed class Event
        {
            public uint type; public string name; public float volume, pitch, pitchRandomization, volumeRandomization;
            public uint priority, maxPlaybacks, mode; public float minDistance, maxDistance; public uint fadeInMs, fadeOutMs;
            /// The word after the 3D cone (fev_parse "rolloff"): 1 / 3 / 4 in the shipped data, read as FMOD Designer's max-playbacks
            /// behaviour (1 steal oldest, 2 steal newest, 3 steal quietest, 4 just fail, 5 just fail if quietest): 3 on every
            /// drive-on / collision / crowd event (max 2..12), 1 on the one-shot VO (max 1), as that reading predicts.
            public uint maxPlaybacksBehavior;
            public Layer[] layers; public Parameter[] parameters; public string[] categories;
        }
        public sealed class Group { public string name; public Group[] groups; public Event[] events; }
        public sealed class SoundDefBlock
        {
            public uint playMode, spawnMin, spawnMax, f04; public float f10, f14, f18, f1c, f20, f24, f28, f2c; public uint x1, x2, u30;
            public string PlayModeName => PlayModes.Length > (playMode & 7) ? PlayModes[playMode & 7] : "?";
        }
        public sealed class WaveEntry { public uint type, weight; public string kind, wave, bank; public uint index, lengthMs; }
        public sealed class SoundDef { public string name; public uint block; public WaveEntry[] entries; }
        public sealed class WaveBankRef { public uint flags, maxStreams; public string name; }
        /// A reverb definition of the project (the FEV's tail: u32 count, then per definition a name and 33 dwords; _re/notes/audio-port.md 9.1,
        /// EventSystemI::load): what EventSystem::getReverbPreset(name) (Audio_GetReverbPreset 00805610) hands the game. Room / RoomHF / RoomLF /
        /// Reflections / Reverb in mB, the delays and the decay in seconds (the FMOD_REVERB_PROPERTIES fields of the same names).
        public sealed class ReverbDef
        {
            public string name;
            public int room, roomHF, roomLF, reflections, reverb;
            public float decayTime, decayHFRatio, reflectionsDelay, reverbDelay, diffusion, density, hfReference, lfReference;
        }
        public sealed class Fev
        {
            public string name; public WaveBankRef[] waveBanks; public Group[] groups; public SoundDefBlock[] blocks; public SoundDef[] soundDefs;
            public ReverbDef[] reverbs = new ReverbDef[0];
            /// every event with its "Group/Sub" path (audio_export.walk_events), in file order.
            public IEnumerable<(string path, Event ev)> Events()
            {
                var list = new List<(string, Event)>();
                void Rec(Group g, string path)
                {
                    foreach (var e in g.events) list.Add((path, e));
                    foreach (var s in g.groups) Rec(s, path + "/" + s.name);
                }
                foreach (var g in groups) Rec(g, g.name);
                return list;
            }
        }

        static void Prop(FevReader r) { r.S(); uint t = r.U32(); if (t == 0 || t == 1) r.Raw(4); else if (t == 2) r.S(); }
        static void Category(FevReader r) { r.S(); r.F32(); r.F32(); r.U32(); r.U32(); int n = r.Count(); for (int i = 0; i < n; i++) Category(r); }

        static Envelope ReadEnvelope(FevReader r)
        {
            var e = new Envelope { dspEffectIndex = r.I32(), name = r.S(), dspParamIndex = r.U32(), flags = r.U32(), flags2 = r.U32() };
            e.points = new Point[r.Count()];
            for (int i = 0; i < e.points.Length; i++) e.points[i] = new Point { x = r.F32(), y = r.F32(), shape = r.U32() };
            var a = r.Raw(4); var b = r.Raw(4);
            e.controlParam = BitConverter.ToUInt32(a, 0); e.logScale = BitConverter.ToUInt32(b, 0) != 0;
            return e;
        }

        static SoundInstance ReadInstance(FevReader r) => new SoundInstance
        {
            soundDef = r.U16(), start = r.F32(), length = r.F32(), autopitchEnabled = r.U32(), loopWord = r.U32(), autopitchParamOrUnk34 = r.I32(),
            unk38 = r.U32(), unk28 = r.U32(), unk2c = r.U32(), unk30 = r.U32(), volume = r.F32(), fadeIn = r.F32(), fadeOut = r.F32(),
            fadeInCurve = r.U32(), fadeOutCurve = r.U32()
        };

        static Event ReadEvent(FevReader r)
        {
            var e = new Event { type = r.U32(), name = r.S() };
            r.U32(); r.U16(); r.U16(); r.Raw(8);                                                   // guid
            e.volume = r.F32(); e.pitch = r.F32(); e.pitchRandomization = r.F32(); e.volumeRandomization = r.F32();
            e.priority = r.U32(); e.maxPlaybacks = r.U32(); r.U32(); e.mode = r.U32();
            e.minDistance = r.F32(); e.maxDistance = r.F32(); r.U32();                              // flags2
            for (int i = 0; i < 8; i++) r.F32();                                                    // speaker levels
            r.F32(); r.F32(); r.F32(); e.maxPlaybacksBehavior = r.U32(); r.F32();                   // cone x3, max-playbacks behaviour, unk104
            r.F32(); r.F32(); r.F32(); e.fadeInMs = r.U32(); e.fadeOutMs = r.U32();                 // unk8c unk90 unk110 unka4 unka6
            r.F32(); r.F32(); r.F32(); r.U32();                                                     // unk98 unk9c unk114 unkf4
            if (e.type == 0x10)         // "simple" event: one sound instance, no layers/params/envelopes
            {
                r.U32();
                e.layers = new[] { new Layer { flags = 0, priority = -1, controlParam = 0, instances = new[] { ReadInstance(r) }, envelopes = new Envelope[0] } };
                e.parameters = new Parameter[0];
                e.categories = new string[r.Count()]; for (int i = 0; i < e.categories.Length; i++) e.categories[i] = r.S();
                return e;
            }
            e.layers = new Layer[r.Count()];
            for (int i = 0; i < e.layers.Length; i++)
            {
                var l = new Layer { flags = r.U16(), priority = r.I16(), controlParam = r.U16() };
                int nsi = r.U16(), nenv = r.U16();
                l.instances = new SoundInstance[nsi]; for (int k = 0; k < nsi; k++) l.instances[k] = ReadInstance(r);
                l.envelopes = new Envelope[nenv]; for (int k = 0; k < nenv; k++) l.envelopes[k] = ReadEnvelope(r);
                e.layers[i] = l;
            }
            e.parameters = new Parameter[r.Count()];
            for (int i = 0; i < e.parameters.Length; i++)
            {
                var p = new Parameter { name = r.S(), initial = r.F32(), min = r.F32(), max = r.F32(), flags = r.U32(), unk1c = r.U32(), unk2c = r.U32() };
                p.keyOffsets = new float[r.Count()]; for (int k = 0; k < p.keyOffsets.Length; k++) p.keyOffsets[k] = r.F32();
                e.parameters[i] = p;
            }
            int np = r.Count(); for (int i = 0; i < np; i++) Prop(r);
            e.categories = new string[r.Count()]; for (int i = 0; i < e.categories.Length; i++) e.categories[i] = r.S();
            return e;
        }

        static Group ReadGroup(FevReader r)
        {
            var g = new Group { name = r.S() };
            int np = r.Count(); for (int i = 0; i < np; i++) Prop(r);
            int nsub = r.Count(), nev = r.Count();
            g.groups = new Group[nsub]; for (int i = 0; i < nsub; i++) g.groups[i] = ReadGroup(r);
            g.events = new Event[nev]; for (int i = 0; i < nev; i++) g.events[i] = ReadEvent(r);
            return g;
        }

        /// A "sound" asset (wrapper + FEV1) or a bare FEV1 image; null when it is not a FEV1 v0x3a file (the shipped
        /// CROWD_IND_MED/SML and Editor_Default are v0x2d and are not read, as in fev_parse).
        public static Fev ParseFev(byte[] asset)
        {
            int i = IndexOf(asset, 0, Math.Min(asset.Length, 0x100), (byte)'F', (byte)'E', (byte)'V', (byte)'1');
            if (i < 0) return null;
            var r = new FevReader(asset, i + 4);
            if (r.U32() != FevVersion) return null;
            r.U32(); r.U32();                          // pool size, skipped
            var f = new Fev { name = r.S() };
            f.waveBanks = new WaveBankRef[r.Count()];
            for (int k = 0; k < f.waveBanks.Length; k++) f.waveBanks[k] = new WaveBankRef { flags = r.U32(), maxStreams = r.U32(), name = r.S() };
            Category(r);
            f.groups = new Group[r.Count()]; for (int k = 0; k < f.groups.Length; k++) f.groups[k] = ReadGroup(r);
            f.blocks = new SoundDefBlock[r.Count()];
            for (int k = 0; k < f.blocks.Length; k++)
            {
                var b = new SoundDefBlock { playMode = r.U32(), spawnMin = r.U32(), spawnMax = r.U32(), f04 = r.U32(), f10 = r.F32(), x1 = r.U32(), f14 = r.F32(), f18 = r.F32(), f1c = r.F32(), f20 = r.F32() };
                b.x2 = r.U32(); b.f24 = r.F32(); b.f28 = r.F32(); b.f2c = r.F32(); b.u30 = r.U32();
                f.blocks[k] = b;
            }
            f.soundDefs = new SoundDef[r.Count()];
            for (int k = 0; k < f.soundDefs.Length; k++)
            {
                var sd = new SoundDef { name = r.S(), block = r.U32() };
                sd.entries = new WaveEntry[r.Count()];
                for (int n = 0; n < sd.entries.Length; n++)
                {
                    var x = new WaveEntry { type = r.U32(), weight = r.U32() };
                    switch (x.type)
                    {
                        case 0: x.kind = "wave"; x.wave = r.S(); x.bank = r.S(); x.index = r.U32(); x.lengthMs = r.U32(); break;
                        case 1: x.kind = "oscillator"; r.U32(); r.U32(); break;
                        case 2: x.kind = "null"; break;
                        case 3: x.kind = "programmer"; break;
                        default: throw new InvalidDataException("FEV: unknown sound entry type " + x.type);
                    }
                    sd.entries[n] = x;
                }
                f.soundDefs[k] = sd;
            }
            // the tail: the reverb definitions (Default: FirstPerson, WreckAvoidance_disabled; SX_FD_B: Reverb); a short or odd tail leaves none
            try
            {
                int nrev = r.Count();
                var list = new List<ReverbDef>();
                for (int k = 0; k < nrev; k++)
                {
                    string rn = r.S(); var v = new uint[33];
                    for (int n = 0; n < v.Length; n++) v[n] = r.U32();
                    float F(int i) => BitConverter.ToSingle(BitConverter.GetBytes(v[i]), 0);
                    int I(int i) => unchecked((int)v[i]);
                    // Room RoomHF RoomLF DecayTime DecayHFRatio Reflections ReflectionsDelay Reverb ReverbDelay Diffusion Density HFReference [RoomRolloff] LFReference
                    // (checked against FMOD_PRESET_GENERIC / _CONCERTHALL / _UNDERWATER: the values of the three shipped definitions)
                    list.Add(new ReverbDef { name = rn, room = I(0), roomHF = I(1), roomLF = I(2), decayTime = F(3), decayHFRatio = F(4), reflections = I(5), reflectionsDelay = F(6),
                                             reverb = I(7), reverbDelay = F(8), diffusion = F(9), density = F(10), hfReference = F(11), lfReference = F(13) });
                }
                f.reverbs = list.ToArray();
            }
            catch (Exception) { f.reverbs = new ReverbDef[0]; }
            return f;
        }

        static int IndexOf(byte[] b, int from, int to, params byte[] needle)
        {
            for (int i = from; i + needle.Length <= to; i++)
            {
                int k = 0; while (k < needle.Length && b[i + k] == needle[k]) k++;
                if (k == needle.Length) return i;
            }
            return -1;
        }

        // ================================================================================================ FSB
        public const uint M_LOOP_OFF = 0x1, M_LOOP_NORMAL = 0x2, M_LOOP_BIDI = 0x4, M_8BITS = 0x8, M_16BITS = 0x10, M_MONO = 0x20, M_STEREO = 0x40,
            M_UNSIGNED = 0x80, M_SIGNED = 0x100, M_DELTA = 0x200, M_IMAADPCM = 0x400000;

        public sealed class Sample
        {
            public int index; public string name; public int lengthSamples, lengthBytes, loopStart, loopEnd; public uint mode; public int frequency;
            public int defaultVolume, defaultPan, defaultPriority, channels, dataOffset; public float minDistance, maxDistance;
            public string Stem => Stem(name);
        }
        public sealed class Fsb
        {
            public byte[] blob; public string version; public uint fsbVersion, headerMode; public int dataSize; public Sample[] samples;
            /// the first bytes of a sample (enough for CodecOf's header probes)
            public byte[] Head(Sample s) { var r = new byte[Math.Min(s.lengthBytes, 68)]; Buffer.BlockCopy(blob, s.dataOffset, r, 0, r.Length); return r; }
            public byte[] Raw(Sample s) { var r = new byte[s.lengthBytes]; Buffer.BlockCopy(blob, s.dataOffset, r, 0, r.Length); return r; }
        }

        /// pathlib.PurePosixPath(n).stem
        public static string Stem(string n)
        {
            int s = n.LastIndexOf('/'); if (s >= 0) n = n.Substring(s + 1);
            int i = n.LastIndexOf('.');
            return i > 0 && i < n.Length - 1 ? n.Substring(0, i) : n;
        }

        /// A "soundbnk" asset (wrapper + FSB3/FSB4) or a bare FSB.
        public static Fsb ParseFsb(byte[] asset)
        {
            int at = IndexOf(asset, 0, Math.Min(asset.Length, 0x100), (byte)'F', (byte)'S', (byte)'B');
            if (at < 0) throw new InvalidDataException("FSB: magic not found");
            byte[] blob = asset;
            if (at != 0) { blob = new byte[asset.Length - at]; Buffer.BlockCopy(asset, at, blob, 0, blob.Length); }
            if (blob.Length < 0x30 || blob[3] != (byte)'3' && blob[3] != (byte)'4') throw new InvalidDataException("not FSB3/FSB4");
            bool fsb4 = blob[3] == (byte)'4';
            int n = (int)BitConverter.ToUInt32(blob, 4), shdr = (int)BitConverter.ToUInt32(blob, 8), dsz = (int)BitConverter.ToUInt32(blob, 12);
            uint ver = BitConverter.ToUInt32(blob, 16), hmode = BitConverter.ToUInt32(blob, 20);
            int bas = fsb4 ? 0x30 : 0x18;
            if ((hmode & 0x2) != 0) throw new InvalidDataException("FSB basic-header banks are not supported (none in the game)");
            int dataOff = bas + shdr;
            if (n < 0 || n > 100000 || dataOff + (long)dsz > blob.Length) throw new InvalidDataException("truncated FSB");
            var smp = new Sample[n]; int p = bas, off = 0;
            for (int i = 0; i < n; i++)
            {
                int size = BitConverter.ToUInt16(blob, p);
                int e = p + 2; while (e < p + 32 && blob[e] != 0) e++;
                var s = new Sample
                {
                    index = i, name = Encoding.GetEncoding(28591).GetString(blob, p + 2, e - p - 2),
                    lengthSamples = (int)BitConverter.ToUInt32(blob, p + 32), lengthBytes = (int)BitConverter.ToUInt32(blob, p + 36),
                    loopStart = (int)BitConverter.ToUInt32(blob, p + 40), loopEnd = (int)BitConverter.ToUInt32(blob, p + 44),
                    mode = BitConverter.ToUInt32(blob, p + 48), frequency = (int)BitConverter.ToUInt32(blob, p + 52),
                    defaultVolume = BitConverter.ToUInt16(blob, p + 56), defaultPan = BitConverter.ToInt16(blob, p + 58),
                    defaultPriority = BitConverter.ToUInt16(blob, p + 60)
                };
                int nch = BitConverter.ToUInt16(blob, p + 62);
                s.channels = nch != 0 ? nch : (s.mode & M_STEREO) != 0 ? 2 : 1;
                s.dataOffset = dataOff + off;
                if (size >= 80) { s.minDistance = BitConverter.ToSingle(blob, p + 64); s.maxDistance = BitConverter.ToSingle(blob, p + 68); }
                if (s.lengthBytes < 0 || s.dataOffset + (long)s.lengthBytes > blob.Length) throw new InvalidDataException("FSB sample outside the bank");
                smp[i] = s; off += s.lengthBytes; p += size;
            }
            if (off != dsz) throw new InvalidDataException("FSB sample sizes (" + off + ") do not add up to the data size (" + dsz + ")");
            return new Fsb { blob = blob, version = fsb4 ? "FSB4" : "FSB3", fsbVersion = ver, headerMode = hmode, dataSize = dsz, samples = smp };
        }

        // ------------------------------------------------------------------------------------------ MPEG frame cleaning
        static readonly int[][] BitrateV1 = {
            null,
            new[] { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
            new[] { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },
            new[] { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 } };
        static readonly int[][] BitrateV2 = {
            null,
            new[] { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 },
            new[] { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
            new[] { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 } };
        static readonly int[][] SampleRates = { null, new[] { 44100, 48000, 32000 }, new[] { 22050, 24000, 16000 }, new[] { 11025, 12000, 8000 } };

        /// fsb_export.mpeg_header: (frame bytes, samples per frame, rate, channels) of the frame header at b[i], or false.
        public static bool MpegHeader(byte[] b, int i, out int size, out int spf, out int rate, out int channels)
        {
            size = spf = rate = channels = 0;
            if (i + 4 > b.Length || b[i] != 0xFF || (b[i + 1] & 0xE0) != 0xE0) return false;
            int vbits = (b[i + 1] >> 3) & 3, lbits = (b[i + 1] >> 1) & 3;
            if (vbits == 1 || lbits == 0) return false;
            int version = vbits == 3 ? 1 : vbits == 2 ? 2 : 3;         // MPEG1, MPEG2, MPEG2.5
            int layer = 4 - lbits;
            int bi = b[i + 2] >> 4, si = (b[i + 2] >> 2) & 3, pad = (b[i + 2] >> 1) & 1;
            if (bi == 0 || bi == 15 || si == 3) return false;
            int br = (version == 1 ? BitrateV1 : BitrateV2)[layer][bi] * 1000;
            rate = SampleRates[version][si];
            channels = (b[i + 3] >> 6) == 3 ? 1 : 2;
            if (layer == 1) { size = (12 * br / rate + pad) * 4; spf = 384; return true; }
            spf = layer == 2 || version == 1 ? 1152 : 576;
            size = spf / 8 * br / rate + pad;
            return true;
        }

        /// fsb_export.clean_mpeg: strips FMOD's inter-frame padding; frames/samples/rate/channels of the stream.
        public static byte[] CleanMpeg(byte[] raw, out int frames, out int samples, out int rate, out int channels)
        {
            var o = new MemoryStream(raw.Length); int p = 0; frames = samples = rate = channels = 0;
            while (p < raw.Length)
            {
                if (!MpegHeader(raw, p, out int size, out int spf, out int r, out int ch)) { p++; continue; }
                rate = r; channels = ch;
                if (p + size > raw.Length) break;
                o.Write(raw, p, size); frames++; samples += spf; p += size;
            }
            return o.ToArray();
        }

        /// "mpeg", "ima_adpcm", "pcm" or "unknown" (fsb_export.codec_of).
        public static string CodecOf(Sample s, byte[] raw)
        {
            if ((s.mode & M_IMAADPCM) != 0) return "ima_adpcm";
            if (MpegHeader(raw, 0, out _, out _, out _, out _) || ((s.mode & M_DELTA) != 0 && (s.mode & 0x2000) == 0 && (s.mode & (M_8BITS | M_16BITS)) == 0)) return "mpeg";
            for (int i = 0; i < Math.Min(raw.Length, 64); i++) if (MpegHeader(raw, i, out _, out _, out _, out _)) return "mpeg";
            if ((s.mode & (M_8BITS | M_16BITS)) != 0) return "pcm";
            return "unknown";
        }

        // ------------------------------------------------------------------------------------------ PCM / IMA ADPCM
        static readonly int[] ImaStep = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88,
            97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
            724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
            4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
            18500, 20350, 22385, 24623, 27086, 29794, 32767 };
        static readonly int[] ImaIdx = { -1, -1, -1, -1, 2, 4, 6, 8 };

        static void ImaNibble(ref int pred, ref int idx, int nib)
        {
            int step = ImaStep[idx], diff = step >> 3;
            if ((nib & 4) != 0) diff += step;
            if ((nib & 2) != 0) diff += step >> 1;
            if ((nib & 1) != 0) diff += step >> 2;
            pred += (nib & 8) != 0 ? -diff : diff;
            pred = Math.Max(-32768, Math.Min(32767, pred));
            idx = Math.Max(0, Math.Min(88, idx + ImaIdx[nib & 7]));
        }

        /// FMOD IMA ADPCM: per channel 36-byte blocks (int16 predictor, u8 index, u8 0, 32 nibble bytes) = 64 samples; the
        /// blocks of the channels alternate. The header predictor is output sample 0, then 63 nibbles. Interleaved PCM16.
        public static short[] DecodeIma(byte[] raw, int channels, int nsamples)
        {
            int nb = raw.Length / (36 * channels);
            var pcm = new short[channels][];
            for (int c = 0; c < channels; c++) pcm[c] = new short[nb * 64];
            for (int b = 0; b < nb; b++)
                for (int c = 0; c < channels; c++)
                {
                    int o = (b * channels + c) * 36, pred = BitConverter.ToInt16(raw, o), idx = raw[o + 2], n = 0;
                    var dst = pcm[c]; int w = b * 64;
                    dst[w + n++] = (short)pred;
                    for (int k = 4; k < 36 && n < 64; k++)
                        foreach (int nib in new[] { raw[o + k] & 15, raw[o + k] >> 4 })
                        {
                            if (n >= 64) break;
                            ImaNibble(ref pred, ref idx, nib); dst[w + n++] = (short)pred;
                        }
                }
            int len = nb * 64; if (nsamples < len) len = nsamples;
            var o2 = new short[len * channels];
            for (int i = 0; i < len; i++) for (int c = 0; c < channels; c++) o2[i * channels + c] = pcm[c][i];
            return o2;
        }

        /// Test helper (fsb_export.encode_ima_block): 64 samples -> a 36-byte FMOD IMA block.
        public static byte[] EncodeImaBlock(short[] s64, int idx = 0)
        {
            var o = new byte[36]; o[0] = (byte)(s64[0] & 255); o[1] = (byte)((s64[0] >> 8) & 255); o[2] = (byte)idx;
            int pred = s64[0]; var nibs = new int[64];
            for (int i = 0; i < 64; i++)
            {
                int s = i + 1 < 64 ? s64[i + 1] : s64[63], step = ImaStep[idx], d = s - pred, nib = 0;
                if (d < 0) { nib = 8; d = -d; }
                if (d >= step) { nib |= 4; d -= step; }
                if (d >= step >> 1) { nib |= 2; d -= step >> 1; }
                if (d >= step >> 2) nib |= 1;
                ImaNibble(ref pred, ref idx, nib); nibs[i] = nib;
            }
            for (int i = 0; i < 64; i += 2) o[4 + i / 2] = (byte)(nibs[i] | (nibs[i + 1] << 4));
            return o;
        }

        /// fsb_export.decode_pcm: interleaved PCM16 from 16-bit (optionally delta coded) or 8-bit data.
        public static short[] DecodePcm(byte[] raw, uint mode)
        {
            if ((mode & M_16BITS) != 0)
            {
                var o = new short[raw.Length / 2]; int acc = 0;
                for (int i = 0; i < o.Length; i++)
                {
                    short v = BitConverter.ToInt16(raw, i * 2);
                    if ((mode & M_DELTA) != 0) { acc = (acc + v) & 0xFFFF; v = (short)acc; }
                    o[i] = v;
                }
                return o;
            }
            if ((mode & M_8BITS) != 0)
            {
                var o = new short[raw.Length]; bool unsigned = (mode & M_UNSIGNED) != 0 || (mode & M_SIGNED) == 0;
                for (int i = 0; i < raw.Length; i++) o[i] = (short)((unsigned ? raw[i] - 128 : (sbyte)raw[i]) << 8);
                return o;
            }
            throw new InvalidDataException("unknown PCM layout");
        }
    }
}
