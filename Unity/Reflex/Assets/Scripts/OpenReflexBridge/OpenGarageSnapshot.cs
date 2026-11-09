using System;
using System.Collections.Generic;
using Reflex.Port.Content;
namespace Reflex.Port.RxCore.Bridge
{
    // Copies selected profile data before the worker starts. Never shares a mutable UI table with the core.
    public static class OpenGarageSnapshot
    {
        public static IPlayerSessionPersistence CapturePersistence(ReflexWorldFiles.Garage garage = null) => CoreGarageProfile.CapturePersistence(garage);
        static readonly string[] Tables = { "PlayerRider", "PlayerRiderGear", "PlayerVehicle", "PlayerVehicleParts", "PlayerVehicleTuning" };
        public static RxProfileCell[] Capture(ReflexWorldFiles.Garage garage)
        {
            var p = ReflexProfile.Capture();
            if (p == null) throw new InvalidOperationException(ReflexProfile.Error ?? "Garage profile unavailable");
            if (garage != null && garage.Equips)
            {
                var r = p.T("PlayerRider");
                void Set(string name, int value) { if (!r.columns.TryGetValue(name,out var cells) || cells.Count != 1) throw new InvalidOperationException("Missing profile selection " + name); cells[0] = (double)value; }
                if (garage.riderOwner >= 0)
                {
                    if (garage.riderOwner != 0 && garage.riderOwner != 4) throw new InvalidOperationException("Unsupported rider owner");
                    Set("LastRiderId", garage.riderOwner == 4 ? garage.riderId : 0);
                    Set("LastRiderGearSetId", garage.riderGearSet);
                }
                if (garage.vehicleOwner >= 0)
                {
                    if (garage.vehicleOwner != 0 && garage.vehicleOwner != 4) throw new InvalidOperationException("Unsupported vehicle owner");
                    if ((r.Int("LastRiderId",0) > 0) != (garage.vehicleOwner == 4)) throw new InvalidOperationException("Rider and vehicle ownership do not match");
                    Set("LastEquippedVehicle",garage.vehicleId);
                }
            }
            var result = new List<RxProfileCell>();
            foreach (string name in Tables)
            {
                var t = p.T(name);
                if (t.rows > 4096) throw new InvalidOperationException("Garage table exceeds 4096 rows: " + name);
                foreach (var col in t.columns)
                {
                    // Saved free-look camera floats are not part of the garage selection/tuning API.
                    if (name == "PlayerRider" && (col.Key == "UserCameraHeading" || col.Key == "UserCameraPitch")) continue;
                    for (int row=0; row<t.rows; row++)
                    {
                        if (row >= col.Value.Count || col.Value[row] == null) continue;
                        var c = new RxProfileCell { Table = name, Column = col.Key, row = row };
                        if (col.Value[row] is string text) { c.type=1; c.Text=text; }
                        else if (col.Value[row] is double n && !double.IsNaN(n) && n >= int.MinValue && n <= int.MaxValue && n == Math.Truncate(n)) c.value=(int)n;
                        else throw new InvalidOperationException("Invalid profile field " + name + "." + col.Key);
                        result.Add(c);
                    }
                }
            }
            if (result.Count > 65536) throw new InvalidOperationException("Garage snapshot exceeds 65536 cells");
            return result.ToArray();
        }
    }
}
