// Every vehicle type of the original beyond the motocross bikes and the ATVs (_re/notes/vehicles.md, _re/names/84_vehicles.tsv): the
// "machines", read at run time from the player's install and the running game.
//   which vehicles        VehicleModel.VehicleTypeId -> VehicleType.FactoryName, the index VehicleFactory_CreateVehicle 00542b90 builds
//                         by (names at 0x9ce220): 3 "TrophyTruck" RavenVehicleTrophyTruck 0x93cee4 (Pro Lite Truck 14, Pro 2 Truck 15;
//                         Pro 4 / Trophy have no VehicleModel), 5 "Buggy" RavenVehicleBuggy 0x93d2e4 (Super Buggy 12), 7 "Rhino"
//                         RavenVehicleUTV 0x93d4d4 (UTV 11; ORV 10 has no model); 2 MonsterTruck / 4 SandRail / 6 GolfCart build nothing
//   a machine's parts     only the GraphicKits part (11) names packages: its model package is the chassis (UTVs/UTV01: UTV01 on
//                         UTV01_Hierarchy), its material package the kit, which also holds the four wheel models (UTV_OBJ_FRT_LFT_Wheel_03
//                         ...: each hangs on the hierarchy node its first node names, OBJ_FRT_LFT_Wheel ...); VirtualEngine / Suspension /
//                         Tires (25..27) are tuning values without packages
//   its materials         as the running game's vehicles have them (ObjShadeTests.BindingRule over the vehicles' own models): every
//                         chassis binding takes the FIRST kit material whose key (MaterialKey: FUN_00543fc0) prefixes the binding's key
//                         (FUN_0053d8b0's comparison); none: the chassis package's own material of that name
//   its simulation        RavenSimulationTrophyTruck 0x93c93c / Buggy 0x93d24c / UTV 0x93d43c with 5 bodies: the chassis, then front left,
//                         front right, rear left, rear right wheel (no rider body: the driver's RiderAnimation rides the chassis);
//                         reflex_world_vehicle type = the factory index (3, 5, 7)
//   node visibility       the model's node record +0xc bit 0x10 hides a node from the renderer (007f2740 skips it): the damage panels
//                         DMG_Roof, DMG_Hood, DMG_*_Door, DMG_*_Fender start hidden (the damage part table at 0x9c6440: OBJ_ / DMG_ / NDE_
//                         names per panel) and the game swaps them as the vehicle is damaged; NodeFlags reads the running game's flags
// ReflexVehicleSpecs.Bike / ReflexBikeModel.Build take the machine branch through BikeSpec.machine; the machines' materials run their
// own ported programs (Reflex/Vehicle, ReflexObjectShading); the run-time constants the game drives (the Super Buggy's belt scroll and
// flywheel blur) come from the vehicle's override list (CollectConstants, ReflexObjectShading.ApplyVehicleConstants).
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class ReflexVehicles
    {
        /// VehicleFactory_CreateVehicle's factory names, by index (0x9ce220).
        public static readonly string[] Factories = { "MotocrossBike", "Atv", "MonsterTruck", "TrophyTruck", "SandRail", "Buggy", "GolfCart", "Rhino" };
        public const int FactoryMotocross = 0, FactoryAtv = 1, FactoryTrophyTruck = 3, FactoryBuggy = 5, FactoryUtv = 7;
        // VehicleTypes with models (MXTables_Default): UTV 11 ("Rhino"), Super Buggy 12 ("Buggy"), Pro Lite Truck 14, Pro 2 Truck 15 ("TrophyTruck")
        public const int TypeUtv = 11, TypeSuperBuggy = 12, TypeProLite = 14, TypePro2 = 15;
        // their Single classes (VehicleClassType): UTV 22, Super Buggy 19, Pro Lite 21, Pro 2 27
        public static readonly int[] MachineTypes = { TypeUtv, TypeSuperBuggy, TypeProLite, TypePro2 };

        public static int FactoryIndex(string factoryName) => Array.IndexOf(Factories, factoryName ?? "");
        /// The factory index of a VehicleModelId's type (-1: unknown).
        public static int FactoryOfModel(int vehicleModelId, ReflexGameTables t = null) => FactoryIndex(ReflexAtv.FactoryOfType(ReflexAtv.TypeOfModel(vehicleModelId, t), t));
        public static int FactoryOfType(int vehicleTypeId, ReflexGameTables t = null) => FactoryIndex(ReflexAtv.FactoryOfType(vehicleTypeId, t));
        /// A truck, buggy or UTV (the four-wheelers with a seated driver).
        public static bool IsMachineFactory(int factory) => factory >= 2;
        public static bool IsMachineModel(int vehicleModelId, ReflexGameTables t = null) => IsMachineFactory(FactoryOfModel(vehicleModelId, t));
        /// A vehicle type the port draws: a motocross bike, an ATV or a machine of a factory class the game builds (3, 5, 7).
        public static bool Drawable(int vehicleTypeId, ReflexGameTables t = null)
        {
            int f = FactoryOfType(vehicleTypeId, t);
            return f == FactoryMotocross || f == FactoryAtv || f == FactoryTrophyTruck || f == FactoryBuggy || f == FactoryUtv;
        }

        /// A machine's graphic kit (ReflexVehicleSpecs.Bike): the chassis' hierarchy, the kit's own models (the wheels) hung on the hierarchy
        /// with the kit as their material package, no motocross tyre set.
        internal static void Finish(BikeSpec s, string hierarchy, (string db, string pkg)? kit)
        {
            s.machine = true; s.hierarchyModel = hierarchy;
            s.tireDb = s.tirePkg = s.frontModel = s.rearModel = null;
            if (kit != null) { s.partModels.Add(kit.Value); s.partMaterials.Add(kit); }
        }

        /// FUN_00543fc0: a material name's override key: digits dropped, a '_' after a '_' dropped, cut before the second '_' (the third
        /// after an "HR_" prefix). "Pro201_Body_Black" -> "Pro_Body", "UTV01_FrameCC" -> "UTV_FrameCC", "ProTruck_Wheel01" -> "ProTruck_Wheel".
        public static string MaterialKey(string name)
        {
            if (string.IsNullOrEmpty(name)) return "";
            int stop = name.Length >= 3 && name[0] == 'H' && name[1] == 'R' && name[2] == '_' ? 3 : 2, underscores = 0;
            var o = new System.Text.StringBuilder(name.Length);
            foreach (char c in name)
            {
                if (c >= '0' && c <= '9') continue;
                if (c == '_' && o.Length > 0 && o[o.Length - 1] == '_') continue;
                if (c == '_' && ++underscores == stop) break;
                o.Append(c);
            }
            return o.ToString();
        }

        /// A chassis / kit-wheel binding's material as the running game's vehicles have it (each vehicle's own model V+0x1328, read back by
        /// ObjShadeTests.BindingRule, 12-machine races: Super Buggy 672 / 672, UTV 654 / 654, Pro Lite 792 / 792, Pro 2 840 / 840): the FIRST material of
        /// the kit, in package order, whose key prefixes the binding's key (_strnicmp over the material key's length, the comparison of
        /// VehicleLoader_OverrideMaterials FUN_0053d8b0); none: the chassis package's own material of the binding's name. (The shared model
        /// assets' bindings, which FUN_0053d8b0 rewrites, keep the last match - SB_Tire_14 - but the vehicles draw the first - SB_Tire_03.)
        internal static VehicleMaterialDef Resolve(VehicleCatalog cat, string binding, (string db, string pkg) own, (string db, string pkg)? kit)
        {
            string b = MaterialKey(binding); string pick = null;
            if (kit != null)
                foreach (string n in cat.MaterialNames(kit.Value.db, kit.Value.pkg))
                {
                    string k = MaterialKey(n);
                    if (k.Length > 0 && b.Length >= k.Length && string.Compare(b, 0, k, 0, k.Length, StringComparison.OrdinalIgnoreCase) == 0) { pick = n; break; }
                }
            if (pick != null) return cat.Material(kit.Value.db, kit.Value.pkg, pick);
            return cat.Material(own.db, own.pkg, binding);
        }

        /// The model's node names whose node the game hides at start (the damage part table's DMG_ panels, record +0xc bit 0x10): what the
        /// garage preview shows (a running world's own flags replace it: NodeFlags).
        public static bool HiddenAtStart(string nodeName) => nodeName != null && nodeName.StartsWith("DMG_", StringComparison.OrdinalIgnoreCase);
        public const byte NodeHidden = 0x10;

        /// The node flags (record +0xc low byte) of a vehicle's model [V+0x1328] (hierarchy at model +0x10: records +8, count +0xc; 16-byte
        /// records), through reflex_world_read; bit 0x10 hidden. Returns the node count (0: none / unreadable).
        public static int NodeFlags(IntPtr world, uint vehicle, byte[] flags, byte[] scratch)
        {
            if (world == IntPtr.Zero || vehicle == 0 || flags == null || scratch == null || scratch.Length < 16) return 0;
            CollectConstants(world, vehicle, scratch);   // (the same thread, the same frame: the run-time shader constants)
            uint U(uint a) => Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(world, a, scratch, 4) == 0 ? BitConverter.ToUInt32(scratch, 0) : 0u;
            uint model = U(vehicle + 0x1328); if (model == 0) return 0;
            uint records = U(model + 0x18), count = U(model + 0x1c);
            if (records == 0 || count == 0 || count > 64 || scratch.Length < count * 16) return 0;
            if (Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(world, records, scratch, count * 16) != 0) return 0;
            int n = (int)Math.Min(count, (uint)flags.Length);
            for (int i = 0; i < n; i++) flags[i] = scratch[i * 16 + 12];
            return n;
        }

        // ------------------------------------------------------------------------------------------- run-time shader constants

        /// A vehicle's shader-constant override list (the game binds it to the vehicle's model and rider): V+0x6c0, 64-byte names, the
        /// count in the byte at +0x940, the value pointers at +0x440 (+0x400 a size word). Every vehicle lists its mud record's three
        /// constants (0xa96d90 table: ReflexMud); RavenVehicleBuggy's constructor (004a2c62..004a2cf3) adds gTextureOffset -> V+0x15a0 (the
        /// belts' scroll) and usr_fade_value -> V+0x15b0 (the flywheels' blur), which its update drives. The other entries are kept here.
        sealed class VehicleConstantList { public IntPtr world; public string[] names; public uint[] pointers; public Vector4[] values; }
        static readonly Dictionary<uint, VehicleConstantList> vehicleConstants = new Dictionary<uint, VehicleConstantList>();

        static void CollectConstants(IntPtr world, uint vehicle, byte[] scratch)
        {
            VehicleConstantList c;
            lock (vehicleConstants) vehicleConstants.TryGetValue(vehicle, out c);
            if (c == null || c.world != world)   // (another world may reuse the address)
            {
                var names = new List<string>(); var ptrs = new List<uint>();
                if (Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(world, vehicle + 0x6c0 + 0x940, scratch, 4) == 0)
                {
                    int n = Math.Min(scratch[0], (byte)16);
                    for (int i = 0; i < n; i++)
                    {
                        if (Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(world, vehicle + 0x6c0 + (uint)(64 * i), scratch, 16) != 0) continue;
                        int e = Array.IndexOf(scratch, (byte)0, 0, 16); string name = System.Text.Encoding.ASCII.GetString(scratch, 0, e < 0 ? 16 : e);
                        if (name.StartsWith("usr_mud_", StringComparison.Ordinal) || name.Length == 0) continue;   // ReflexMud's
                        if (Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(world, vehicle + 0x6c0 + 0x440 + (uint)(4 * i), scratch, 4) != 0) continue;
                        names.Add(name); ptrs.Add(BitConverter.ToUInt32(scratch, 0));
                    }
                }
                c = new VehicleConstantList { world = world, names = names.ToArray(), pointers = ptrs.ToArray(), values = new Vector4[names.Count] };
                lock (vehicleConstants) vehicleConstants[vehicle] = c;
            }
            for (int i = 0; i < c.pointers.Length; i++)
                if (Reflex.Port.OriginalWorld.ReflexWorldNative.reflex_world_read(world, c.pointers[i], scratch, 16) == 0)
                {
                    var v = new Vector4(BitConverter.ToSingle(scratch, 0), BitConverter.ToSingle(scratch, 4), BitConverter.ToSingle(scratch, 8), BitConverter.ToSingle(scratch, 12));
                    lock (vehicleConstants) c.values[i] = v;
                }
        }

        /// The last collected run-time constants of a vehicle (names and values; false: none collected).
        public static bool TryConstants(uint vehicle, out string[] names, out Vector4[] values)
        {
            names = null; values = null;
            lock (vehicleConstants)
            {
                if (!vehicleConstants.TryGetValue(vehicle, out var c)) return false;
                names = c.names; values = (Vector4[])c.values.Clone(); return true;
            }
        }
        /// Forgets a world's vehicles (its destruction).
        public static void ForgetConstants(IntPtr world) { lock (vehicleConstants) foreach (var k in vehicleConstants.Where(kv => kv.Value.world == world).Select(kv => kv.Key).ToList()) vehicleConstants.Remove(k); }
    }
}
