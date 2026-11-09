using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Reflex.Port.RxCore
{
    // The structs of core/include/rx_api.h, field for field. Field names are the C names, so RxApiLayout can look every one up by name
    // and compare Marshal.OffsetOf with the C offsetof (builds/unity/layout.txt). C arrays are fixed buffers (blittable: no
    // marshalling copies, no array to allocate); read them with the Copy helpers or the pointers in an unsafe context.
    // Change nothing here without the matching change in rx_api.h and a version bump (RxApiNative.ExpectedApiVersion).

    public static unsafe class RxFixed
    {
        public static void Copy(float* src, float[] dst, int count, int dstOffset = 0)
        {
            for (int i = 0; i < count; i++) dst[dstOffset + i] = src[i];
        }

        public static string GetUtf8(byte* p, int capacity)
        {
            int n = 0;
            while (n < capacity && p[n] != 0) n++;
            return Encoding.UTF8.GetString(p, n);
        }

        // Truncates to capacity - 1 bytes (always NUL terminated, like the C side's snprintf).
        public static void PutUtf8(byte* p, int capacity, string value)
        {
            for (int i = 0; i < capacity; i++) p[i] = 0;
            if (string.IsNullOrEmpty(value)) return;
            var bytes = Encoding.UTF8.GetBytes(value);
            int n = Math.Min(bytes.Length, capacity - 1);
            for (int i = 0; i < n; i++) p[i] = bytes[i];
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxProfileCell
    {
        public fixed byte table[32], column[32], text[128];
        public int row, type, value;
        static void Check(string s, int capacity) { if (s == null || s.IndexOf('\0') >= 0 || Encoding.UTF8.GetByteCount(s) >= capacity) throw new ArgumentException("Invalid or oversized profile text"); }
        public string Table { set { Check(value,32); fixed(byte* p=table) RxFixed.PutUtf8(p,32,value); } }
        public string Column { set { Check(value,32); fixed(byte* p=column) RxFixed.PutUtf8(p,32,value); } }
        public string Text { set { Check(value,128); fixed(byte* p=text) RxFixed.PutUtf8(p,128,value); } }
    }

    // rx_event_spec: the event's UI property block. Fill it from RxApiNative.EventDefault (the reference National / NAT_Concord / 12 riders).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxEventSpec
    {
        public fixed byte event_type[32];
        public int event_type_id;
        public fixed byte event_activation[32];
        public fixed byte location[64];
        public int location_id;
        public int track_id;
        public int duration;
        public fixed byte duration_type[16];
        public int riders;
        public int ai_difficulty;
        public int vehicle_class;
        public fixed byte player_mode[16];
        public fixed ushort player_name[68];   // UTF-16

        public string EventType { get { fixed (byte* p = event_type) return RxFixed.GetUtf8(p, 32); } set { fixed (byte* p = event_type) RxFixed.PutUtf8(p, 32, value); } }
        public string EventActivation { get { fixed (byte* p = event_activation) return RxFixed.GetUtf8(p, 32); } set { fixed (byte* p = event_activation) RxFixed.PutUtf8(p, 32, value); } }
        public string Location { get { fixed (byte* p = location) return RxFixed.GetUtf8(p, 64); } set { fixed (byte* p = location) RxFixed.PutUtf8(p, 64, value); } }
        public string DurationType { get { fixed (byte* p = duration_type) return RxFixed.GetUtf8(p, 16); } set { fixed (byte* p = duration_type) RxFixed.PutUtf8(p, 16, value); } }
        public string PlayerMode { get { fixed (byte* p = player_mode) return RxFixed.GetUtf8(p, 16); } set { fixed (byte* p = player_mode) RxFixed.PutUtf8(p, 16, value); } }
        public string PlayerName
        {
            get { fixed (ushort* p = player_name) { int n = 0; while (n < 68 && p[n] != 0) n++; return new string((char*)p, 0, n); } }
            set
            {
                fixed (ushort* p = player_name)
                {
                    for (int i = 0; i < 68; i++) p[i] = 0;
                    if (value == null) return;
                    for (int i = 0; i < value.Length && i < 67; i++) p[i] = value[i];
                }
            }
        }
    }

    // rx_pad: one gamepad, the old plugin's ReflexPad (12 bytes).
    [StructLayout(LayoutKind.Sequential)]
    public struct RxPad
    {
        public ushort buttons;
        public byte left_trigger, right_trigger;
        public short lx, ly, rx, ry;
    }

    // rx_bike_state: four body vec4s per pose field (frame, rider, front wheel, rear wheel).
    // Positions are xyz + padding; orientations are xyzw quaternions, not transform matrices.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxBikeState
    {
        public fixed float position[16];
        public fixed float orientation[16];
        public fixed float render_position[16];
        public fixed float render_orientation[16];
        public fixed float velocity[4];
        public float speed;
        public int gear;
        public fixed float inputs[8];   // throttle, reverse, front brake, rear brake, steer x/y, lean x/y
        public uint frames;
        public uint reserved;

        public void CopyPosition(float[] dst) { fixed (float* p = position) RxFixed.Copy(p, dst, 16); }
        public void CopyOrientation(float[] dst) { fixed (float* p = orientation) RxFixed.Copy(p, dst, 16); }
        public void CopyRenderPosition(float[] dst) { fixed (float* p = render_position) RxFixed.Copy(p, dst, 16); }
        public void CopyRenderOrientation(float[] dst) { fixed (float* p = render_orientation) RxFixed.Copy(p, dst, 16); }
        public void CopyVelocity(float[] dst) { fixed (float* p = velocity) RxFixed.Copy(p, dst, 4); }
        public void CopyInputs(float[] dst) { fixed (float* p = inputs) RxFixed.Copy(p, dst, 8); }
    }

    // Additive API; does not alter the 320-byte bike-state ABI.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxCameraBindings
    {
        public int valid, keyboard_scan, pad_input, right_stick_rides, driving_camera, look_back_pad_input;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxOnboardState
    {
        public int valid, rider_active, wrecked, trick_active, contacts_valid, front_grounded, rear_grounded;
        public int animation_mode, stunt_state;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxCamera
    {
        public fixed float position[3];
        public fixed float right[3];
        public fixed float up[3];
        public fixed float forward[3];
        public float fov, near_plane, far_plane;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxRaceState
    {
        public int valid, state, num_laps, laps_completed, place, riders;
        public float lap_time, last_lap, best_lap, race_time, lap_progress, start_timer, reset_timer;
        /// The activity's own start countdown as the original's BeginningCountdownComponent shows it (3, 2, 1); -1 none (not in Countdown, or the start board drives the start).
        public int countdown_step;
    }

    [Flags]
    public enum RxEventUiKnown : uint
    {
        ActivityType = 1, RaceState = 2, RulesHold = 4, PostState = 8,
        SkipRequested = 16, AllowPause = 32, PauseOwner = 64
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxEventUiState
    {
        public RxEventUiKnown known;
        public int activity_type, race_state, rules_hold, post_state, skip_requested;
        public int native_allow_pause, native_pause_owner;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxDriverResult
    {
        public int slot, local, place, laps_completed, finished;
        public float race_time, best_lap, last_lap;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxHudState
    {
        public int valid, activity_state, on_track, wrong_way;
        public float speed, time_behind, time_ahead, hold_clock, hold_until, penalty;
    }

    /// LAP LOG (port addition, rx_api.h rx_lap_record, core/game/practice.h): one line crossing of the local human that the race counted.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxLapRecord
    {
        public uint serial;        // 1, 2, ... per session (the gate lap is 1); laps_completed stops at 97, this does not
        public int flying;         // 1: the lap began crossing the line; 0: the first lap, from the gate
        public float time;         // the raw lap clock at the crossing (s, whole frames)
        public float exact;        // the time from sub-frame crossing to sub-frame crossing (s)
        public float penalty;      // the off-track rule's penalty at the crossing (s)
        public float off_track;    // s of the lap off the track
        public float race_time;
        public uint resets;        // the rider's resets in the lap (crash, manual, out of bounds, off-track)
        public int wrong_way;      // the wrong-way indicator showed
        public uint mods;          // OR over the lap: 1 / 2 / 4 PHYSICS STUDIO (config, restored, taint), 8 air helpers not original, 16 map scale
        public uint markers;       // (w-lb-markers) rides back to the SPAWN MARKER in the lap (PRACTICE): not counted, timed again from the line
        public const uint ModPhysConfig = 1, ModPhysRestored = 2, ModPhysTaint = 4, ModAirHelpers = 8, ModMapScale = 16;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxSlot
    {
        public int slot, type, local, rider_row, vehicle_row;
        public uint vehicle, rider;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxAppearance
    {
        public int rider_owner, owner_rider_id, gear_set_id, vehicle_owner, owner_vehicle_id, vehicle_model_id;
        public int part_count, gear_count;
        public fixed int part_ids[17], gear_ids[16];
        public fixed byte name[64], plate_name[16], plate_number[16];
        public int bike_dark, on_jersey, invert;
        public string Name { get { fixed (byte* p = name) return RxFixed.GetUtf8(p, 64); } }
        public string PlateName { get { fixed (byte* p = plate_name) return RxFixed.GetUtf8(p, 16); } }
        public string PlateNumber { get { fixed (byte* p = plate_number) return RxFixed.GetUtf8(p, 16); } }
        public int[] PartIds { get { var a = new int[Math.Max(0, Math.Min(17, part_count))]; fixed (int* p = part_ids) for (int i = 0; i < a.Length; i++) a[i] = p[i]; return a; } }
        public int[] GearIds { get { var a = new int[Math.Max(0, Math.Min(16, gear_count))]; fixed (int* p = gear_ids) for (int i = 0; i < a.Length; i++) a[i] = p[i]; return a; } }
    }

    // rx_vehicle_info: a slot's vehicle (the old ReflexVehicleInfo). render_position/orientation hold up to six bodies.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxVehicleInfo
    {
        public int slot, type, bodies, model_nodes;
        public uint vehicle, sim;
        public int gear;
        public float rpm, speed;
        public fixed float render_position[24];
        public fixed float render_orientation[24];
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxTerrainInfo
    {
        public uint samples, stride;
        public float spacing, min_x, min_z, max_x, max_z, gain;
        public uint deform_version;
        public int dirty_x0, dirty_z0, dirty_x1, dirty_z1;
    }

    /// rx_terrain_rect (rx_session_terrain_tiles): one rectangle [x0,x1) x [z0,z1) of whole 32 x 32 tiles of the changed terrain grid.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxTerrainRect
    {
        public int x0, z0, x1, z1;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxParticleInfo
    {
        public fixed byte texture[64];
        public fixed float luminance[4];
        public fixed float luminance_delta[4];
        public fixed float fade[4];
        public int soft, normal, additive, live;
        public float near_plane, far_plane;
        public fixed float reserved[2];

        public string Texture { get { fixed (byte* p = texture) return RxFixed.GetUtf8(p, 64); } }
    }

    // rx_particle: one live particle as the original renderer instances it (the old ReflexParticle, 64 bytes).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxParticle
    {
        public fixed float position[3];
        public fixed float size[2];
        public fixed float pivot[2];
        public float scale, rotation;
        public uint color, color1;
        public fixed float uv[4];
        public uint flags;   // bit 0: additive manager
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxAmbientInfo
    {
        public int entities, matrices, objects, reserved0;
        public float fade_start, lod_divisor;
        public fixed float reserved[2];
    }

    // rx_ambient_entity: one ambient animated entity (the old plugin's ReflexAmbientEntity, 304 bytes). `bones` matrices start at
    // `first_matrix` in the matrices array of the same call; `matrix` is the entity's own world matrix.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxAmbientEntity
    {
        public uint object_id;
        public int entity_id, kind;
        public uint flags;
        public int bones, first_matrix, source;
        public float lod_distance;
        public fixed float param[4];
        public fixed float matrix[16];
        public fixed byte model[64];
        public fixed byte texture[64];
        public fixed byte texture_package[64];

        public string Model { get { fixed (byte* p = model) return RxFixed.GetUtf8(p, 64); } }
        public string Texture { get { fixed (byte* p = texture) return RxFixed.GetUtf8(p, 64); } }
        public string TexturePackage { get { fixed (byte* p = texture_package) return RxFixed.GetUtf8(p, 64); } }
        public void CopyMatrix(float[] dst, int offset = 0) { fixed (float* p = matrix) RxFixed.Copy(p, dst, 16, offset); }
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxProp
    {
        public uint vtable;
        public int entity_id, kind;
        public uint flags;
        public fixed float matrix[16];
        public void CopyMatrix(float[] dst) { fixed (float* p = matrix) RxFixed.Copy(p, dst, 16, 0); }
    }

    // rx_scene_object: one live scene entity (props, gates, bales, boards, ...): class vtable (the old find_object's), entity id,
    // its world matrix (+0x30), name and model name.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxSceneObject
    {
        public uint vtable;
        public int entity_id, flags;
        public fixed float matrix[16];
        public fixed byte name[64];
        public fixed byte model[64];

        public string Name { get { fixed (byte* p = name) return RxFixed.GetUtf8(p, 64); } }
        public string Model { get { fixed (byte* p = model) return RxFixed.GetUtf8(p, 64); } }
    }

    // rx_gate_api.h (w-p2scene): the career Gate Selection page's 3D camera inputs (rx_start_gates, 880 bytes): the start gate entity's
    // matrix and each GateStart_NN node's frame in the model's space (local_*) and the world (position / forward), 12 floats per gate.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxStartGates
    {
        public uint size, version;
        public int valid, gate_count, has_camera;
        public float fov, near_plane, far_plane;
        public fixed float gate_matrix[16];
        public uint present_mask;
        public fixed uint reserved[3];
        public fixed float gates[16 * 12];   // per gate: local_position[3], local_forward[3], position[3], forward[3]
        public const uint Version = 1;
        public static RxStartGates Create() { var g = new RxStartGates(); g.size = (uint)sizeof(RxStartGates); g.version = Version; return g; }
    }

    // rx_podium_api.h (w-podium): the post-race presentation's cast. rx_podium_info (32 bytes), rx_podium_rider (160), rx_podium_slot (16).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxPodiumInfo
    {
        public int riders, matrices, scenario, step, post_state, slots;
        public fixed int reserved[2];
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxPodiumRider
    {
        public uint obj;
        public int slot, visible, bones, first_matrix, placed;
        public float draw_distance;   // the renderer culls the character beyond this distance from the camera (0: never)
        public int reserved;
        public fixed float root[16];
        public fixed float mud_amount[4];
        public fixed float mud_color[4];
        public fixed float mud_wetness[4];
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RxPodiumSlot
    {
        public int slot, rider_drawn, vehicle_drawn, reserved;
    }

    // rx_crowd_api.h v1: one ACTIVE camera-flash record of a stands crowd section (BillboardCrowdEntity). 32 bytes.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxCrowdFlash
    {
        public uint object_id;        // the section's rx_ambient_entity.object_id
        public int entity_id;         // the section's scene entity id
        public int binding;           // LOD-0 binding (= flash record index) whose seat mesh carries the flash
        public uint seat;             // seat number (0 .. 499): drawn on seat % (vertices of that binding / 4)
        public float age, length;     // seconds since the start / how long it lasts
        public float empty_chair_ratio;   // the vertex program's c66 of this section
        public float reserved;
    }

    // rx_podium_api.h v3 (w-frscene): a scenario script's sound effect (rx_scenario_sound, 144 bytes): the FMOD event path SoundEffect
    // 0055d610 hands the sound engine ([FORMAT] / [LOC] / [PLATFORM] unresolved), 3D at `position` when has_position.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxScenarioSound
    {
        public fixed byte eventName[128];
        public fixed float position[3];
        public int has_position;
        public string Event { get { fixed (byte* p = eventName) return RxFixed.GetUtf8(p, 128); } }
    }

    // rx_podium_api.h v3 (w-frscene): a static scenery model as the game draws it (rx_scenario_model, 160 bytes): the ModelEntity's /
    // ScrollingTextureEntity's drawn flag (+0xa2) and instance matrix (+0xc0) a scenario script hides / shows (the parked hovercraft,
    // Gibson's dropped box), and a scrolling texture's gTextureOffset (+0xaa0).
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxScenarioModel
    {
        public int entity_id;     // the scene entity id as first seen (ModelPosition re-adds an entity under a new id)
        public int current_id, drawn, kind;   // kind 0 ModelEntity, 1 ScrollingTextureEntity
        public fixed float matrix[16];
        public fixed float texture_offset[2];
        public fixed int reserved[2];
        public fixed byte name[64];
        public string Name { get { fixed (byte* p = name) return RxFixed.GetUtf8(p, 64); } }
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxMudState
    {
        public int slot, record;
        public uint vehicle, rider;
        public fixed float amount[4];
        public fixed float wetness[4];
        public fixed float color[4];
    }

    // rx_track_info: the live TerrainTracks ribbon export (rx_session_tracks). One vertex is 6 floats: world x, z, side,
    // V (32*|w|), intensity, material type; 3 vertices per triangle. The game keeps three rings (30/10/5 chunks) x 24 strips.
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxTrackInfo
    {
        public uint version;
        public int triangles, vertices, strips, chunks, slots, sets;
        public uint vertex_stride, vertex_format, index_format;
        public float half_x, half_z, centre_x, centre_z;
        public int material_count, reserved0;
        public fixed byte material[6 * 24];

        public string MaterialName(int index)
        {
            if (index < 0 || index >= 6) return "";
            fixed (byte* p = material) return RxFixed.GetUtf8(p + index * 24, 24);
        }
    }

    // rx_track_ribbon: one allocated strip with geometry; first_vertex / vertices index the rx_session_tracks triangle list.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxTrackRibbon
    {
        public int set, slot, type, first_vertex, vertices, triangles;
        public uint flags, reserved;
    }

    /// rx_session_state
    public enum RxSessionState
    {
        Loading = 0,
        Countdown = 1,
        Racing = 2,
        Finished = 3,
        Error = 4,
    }
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct RxRemapTable
    {
        public fixed int keyboard[19];
        public fixed int pad[19];
        public fixed int scan[256];
    }
}
