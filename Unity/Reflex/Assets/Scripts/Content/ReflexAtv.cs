// ATVs (_re/notes/atv.md, _re/names/78_atv.tsv), read at run time from the player's install and the running game:
//   which vehicles are ATVs   VehicleModel.VehicleTypeId -> VehicleType.FactoryName "Atv" (the factory name the game builds a
//                             RavenVehicleATV 0x93d0ec for; "MotocrossBike" a RavenVehicleMotocross 0x93cd84), and its node
//                             hierarchy VehicleModel.VehicleHierarchyId -> VehicleHierarchies.AssetName (ATV_Hierarchy: 50 nodes,
//                             the running game's vehicle model [V+0x1328] in that order, which reflex_world_bike_pose(_slot) returns)
//   an ATV's parts            GraphicKits (11): the chassis (ATVs/<chassis>: ATV_<chassis> on that package's ATV_Hierarchy) and the
//                             kit's materials; every other part with a model package (Tires 8 SWP_*_Tire, Wheels 3 OBJ_*_Wheel,
//                             A-Arms 1, Suspensions 15 shocks, Handlebars 7, Bumpers 9, NerfBars 16, Exhausts 10, Grilles 21, ...)
//                             hangs each of its models on the hierarchy node its first node names; material-only parts
//                             (FrameMaterials 23, SwingarmMaterials 22, the wheels' colours) join the material override's pool
//   its simulation            RavenSimulationATV 0x93d054 with 6 bodies: frame, rider, front left / right, rear left / right wheel
//                             (reflex_world_vehicle; ReflexBikeState holds the first four)
// ReflexVehicleSpecs.Bike / ReflexBikeModel.Build take the ATV branch through BikeSpec.atv.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class ReflexAtv
    {
        public const int VehicleTypeAtv = 4;         // VehicleType "STR_VEHICLE_ATV" (FactoryName "Atv")
        public const int VehicleClassAtv = 1;        // VehicleClass "STR_VEHICLECLASS_ATV" (Format "Single", VehicleClassType: type 4 only)
        public const string Factory = "Atv";

        static Dictionary<string, List<Dictionary<string, object>>> installTables;
        /// The install's MXTables_Default (static data; the running game's registry is asked first).
        static Dictionary<string, List<Dictionary<string, object>>> Install =>
            installTables ??= Rfx.Tables(ReflexInstall.Packages.Read("MXTables_Default", "MXTables_Default", "MXTables_Default", "tbldata"));
        static List<Dictionary<string, object>> Rows(string table) => Install.TryGetValue(table, out var r) ? r : new List<Dictionary<string, object>>();
        static Dictionary<string, object> Unique(string table, string col, int v) => Rows(table).FirstOrDefault(r => Rfx.Int(r, col, -1) == v);

        /// VehicleType.FactoryName of a VehicleTypeId ("MotocrossBike", "Atv", "TrophyTruck" ...).
        public static string FactoryOfType(int vehicleTypeId, ReflexGameTables t = null)
        {
            string f = t?.Str("VehicleType", "FactoryName", t.Find("VehicleType", "VehicleTypeId", vehicleTypeId));
            return f ?? Rfx.Str(Unique("VehicleType", "VehicleTypeId", vehicleTypeId), "FactoryName");
        }
        public static int TypeOfModel(int vehicleModelId, ReflexGameTables t = null)
        {
            int r = t != null ? t.Find("VehicleModel", "VehicleModelId", vehicleModelId) : -1;
            int v = r >= 0 ? t.Int("VehicleModel", "VehicleTypeId", r) : -1;
            return v >= 0 ? v : Rfx.Int(Unique("VehicleModel", "VehicleModelId", vehicleModelId), "VehicleTypeId", -1);
        }
        public static bool IsAtvModel(int vehicleModelId, ReflexGameTables t = null) => FactoryOfType(TypeOfModel(vehicleModelId, t), t) == Factory;

        /// VehicleModel.VehicleHierarchyId -> VehicleHierarchies.AssetName ("ATV_Hierarchy"); null when the tables have none.
        public static string HierarchyOf(int vehicleModelId, ReflexGameTables t = null)
        {
            int r = t != null ? t.Find("VehicleModel", "VehicleModelId", vehicleModelId) : -1;
            int h = r >= 0 ? t.Int("VehicleModel", "VehicleHierarchyId", r) : -1;
            if (h < 0) h = Rfx.Int(Unique("VehicleModel", "VehicleModelId", vehicleModelId), "VehicleHierarchyId", -1);
            if (h < 0) return null;
            string n = t?.Str("VehicleHierarchies", "AssetName", t.Find("VehicleHierarchies", "VehicleHierarchyId", h));
            return n ?? Rfx.Str(Unique("VehicleHierarchies", "VehicleHierarchyId", h), "AssetName");
        }

        /// An ATV's part (ReflexVehicleSpecs.Bike, every part type but the graphic kit): a model package hangs its models on the
        /// hierarchy (each on the node its first node names), with its material package beside it; a material package alone joins the pool.
        internal static void AddPart(BikeSpec s, (string db, string pkg)? model, (string db, string pkg)? mat, List<(string db, string pkg)> mats)
        {
            if (model != null) { s.partModels.Add(model.Value); s.partMaterials.Add(mat); }
            if (mat != null) mats.Add(mat.Value);
        }
        /// After the parts: the ATV's hierarchy, and no motocross tyre set (its tyres are part models).
        internal static void Finish(BikeSpec s, string hierarchy)
        {
            s.atv = true; s.hierarchyModel = hierarchy ?? "ATV_Hierarchy";
            s.tireDb = s.tirePkg = s.frontModel = s.rearModel = null;
        }

        /// What VehicleSelect offers first for a vehicle type when the player's own rider has none of it (no PlayerVehicle of the type:
        /// the default profile owns only its MX Lite): the first PlayerSelectable AIRider by DisplayName (VehicleSelect_SortRiders)
        /// with an AIRiderVehicle of the type, on his first such AIVehicle, in his DefaultRiderGearSetId; VehicleClass the event's
        /// Single class holding only that type (LocationSelect_VehicleClassSort: fewest types first). Null: nobody has one.
        public static ReflexWorldFiles.Garage DefaultGarage(int eventTypeId, int vehicleTypeId = VehicleTypeAtv)
        {
            var riders = Rows("AIRider").Where(r => Rfx.Int(r, "PlayerSelectable", 0) != 0)
                         .OrderBy(r => Rfx.Str(r, "DisplayName") ?? "", StringComparer.Ordinal).ToList();
            foreach (var r in riders)
            {
                int id = Rfx.Int(r, "AIRiderId", -1);
                var v = Rows("AIRiderVehicle").FirstOrDefault(x => Rfx.Int(x, "AIRiderId", -1) == id && Rfx.Int(x, "VehicleTypeId", -1) == vehicleTypeId);
                if (v == null) continue;
                return new ReflexWorldFiles.Garage
                {
                    riderOwner = ReflexWorldFiles.Garage.AI, riderId = id, riderGearSet = Rfx.Int(r, "DefaultRiderGearSetId", -1),
                    vehicleOwner = ReflexWorldFiles.Garage.AI, vehicleId = Rfx.Int(v, "AIVehicleId", -1), vehicleTypeId = vehicleTypeId,
                    vehicleClass = ClassFor(eventTypeId, vehicleTypeId),
                };
            }
            return null;
        }
        /// LocationSelect:BuildVehicleClassList's first entry: the event type's classes containing the vehicle type, fewest types first.
        public static int ClassFor(int eventTypeId, int vehicleTypeId)
        {
            var o = new List<(int cls, int types)>();
            foreach (int cls in Rows("EventTypeVehicleClasses").Where(r => Rfx.Int(r, "EventTypeId", -1) == eventTypeId).Select(r => Rfx.Int(r, "VehicleClassId", -1)).OrderBy(x => x))
            {
                var types = Rows("VehicleClassType").Where(r => Rfx.Int(r, "VehicleClassId", -1) == cls).Select(r => Rfx.Int(r, "VehicleTypeId", -1)).ToList();
                if (types.Contains(vehicleTypeId)) o.Add((cls, types.Count));
            }
            return o.Count == 0 ? -1 : o.Select((c, i) => (c, i)).OrderBy(x => x.c.types).ThenBy(x => x.i).First().c.cls;
        }
        /// The VehicleTypeIds of a VehicleClass (VehicleClassType).
        public static int[] TypesOfClass(int vehicleClassId) => Rows("VehicleClassType").Where(r => Rfx.Int(r, "VehicleClassId", -1) == vehicleClassId).Select(r => Rfx.Int(r, "VehicleTypeId", -1)).ToArray();
    }

    /// reflex_world_vehicle (reflex_world.h ReflexVehicleInfo): a slot's vehicle class and the six bodies of an ATV's simulation.
    [StructLayout(LayoutKind.Sequential)]
    public struct ReflexVehicleInfo
    {
        public int slot, type, bodies, modelNodes;   // type: 0 motocross bike, 1 ATV, -1 other
        public uint vehicle, sim;
        public int gear;
        public float rpm, speed;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 24)] public float[] renderPosition;      // 6 x (x, y, z, w)
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 24)] public float[] renderOrientation;   // 6 quaternions (x, y, z, w)
        public bool Atv => type == 1;
        public Vector3 Position(int body) => new Vector3(renderPosition[body * 4], renderPosition[body * 4 + 1], renderPosition[body * 4 + 2]);
        /// The front / rear axle (an ATV: between its two front / rear wheels; a bike: its front / rear wheel).
        /// A truck, buggy or UTV (factory 3, 5, 7: ReflexVehicles): five bodies, the chassis then front left / right, rear left / right wheel.
        public bool Machine => type >= 2 && bodies == 5;
        /// Four wheels (an ATV or a machine): Front / Rear are axle midpoints.
        public bool FourWheels => (Atv && bodies >= 6) || Machine;
        public Vector3 Front => Atv && bodies >= 6 ? (Position(2) + Position(3)) * 0.5f : Machine ? (Position(1) + Position(2)) * 0.5f : Position(2);
        public Vector3 Rear => Atv && bodies >= 6 ? (Position(4) + Position(5)) * 0.5f : Machine ? (Position(3) + Position(4)) * 0.5f : Position(3);
    }

    public static class ReflexAtvNative
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_vehicle__fn(IntPtr world, int slot, ref ReflexVehicleInfo info);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_vehicle__fn> reflex_world_vehicle__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_vehicle__fn>("reflex_world_vehicle");
        public static int reflex_world_vehicle(IntPtr world, int slot, ref ReflexVehicleInfo info) => reflex_world_vehicle__p.Get()(world, slot, ref info);
        public const int InfoSize = 4 * 4 + 2 * 4 + 4 + 2 * 4 + 2 * 24 * 4;   // 228
        /// The local player's (slot -1) or a slot's vehicle; false without the export (an older plugin) or a vehicle.
        public static bool Read(IntPtr world, int slot, ref ReflexVehicleInfo info)
        {
            if (info.renderPosition == null) { info.renderPosition = new float[24]; info.renderOrientation = new float[24]; }
            try { return reflex_world_vehicle(world, slot, ref info) == 0; }
            catch (EntryPointNotFoundException) { return false; }
        }
    }
}
