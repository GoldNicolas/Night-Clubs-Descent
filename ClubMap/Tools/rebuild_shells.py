"""Rebuild only the shells (floor, walls, ceiling, coves, trims, door frames) of every room with the current room_shell,
leaving furniture, lights and dressing untouched. Run with include=[cm_kit.py, this file], then rebuild_shells().
Keep these parameters in sync with each room's build script."""
import unreal

SHELLS = {
    "DanceHall": (0, 0, 8, 8, {"S": [(3, 4)], "W": [(5, 6)], "E": [(3, 4)]}, {}),
    "Entry":     (800, -1200, 4, 3, {"S": [(1, 2)], "N": [(1, 2)]}, {"trim": "Neon_Magenta", "portal": "Neon_Cyan"}),
    "LongBar":   (-800, 1200, 2, 5, {"E": [(2, 3)], "N": [(1, 1)]}, {"trim": "Neon_Gold", "portal": "Neon_Magenta"}),
    "Billiards": (3200, 800, 4, 3, {"W": [(1, 2)], "N": [(0, 1)], "E": [(1, 2)]}, {"trim": "Neon_Cyan", "portal": "Neon_Magenta"}),
    "VIP":       (3200, 2000, 4, 5, {"S": [(0, 1)], "W": [(3, 3)]}, {"trim": "Neon_Gold", "portal": "Neon_Magenta"}),
    "SportsBar": (4800, 400, 6, 5, {"W": [(2, 3)]}, {"wall_mat": "Wall_Panel", "floor_mat": "Wood_Floor", "trim": "Neon_Orange", "portal": "Neon_Magenta"}),
    "Corridor":  (-800, 3200, 10, 1, {"N": [(1, 1), (3, 3), (5, 5), (7, 7), (9, 9)], "S": [(1, 1)], "E": [(0, 0)]},
                  {"wall_mat": "Wall_Concrete", "floor_mat": "Floor_Concrete", "trim": "Speaker_Black", "portal": "Speaker_Black"}),
    "PrivateA":  (-800, 3600, 2, 2, {"S": [(1, 1)]}, {"wall_mat": "Velvet_Red", "floor_mat": "Rug_Charcoal", "trim": "Neon_Gold", "portal": "Neon_Gold"}),
    "PrivateB":  (0, 3600, 2, 2, {"S": [(1, 1)]}, {"wall_mat": "Velvet_Teal", "floor_mat": "Rug_Charcoal", "trim": "Neon_Gold", "portal": "Neon_Gold"}),
    "RestroomM": (800, 3600, 2, 3, {"S": [(1, 1)]}, {"wall_mat": "Tile_Black", "floor_mat": "Tile_Black", "trim": "Neon_Cyan", "portal": "Neon_Cyan"}),
    "RestroomW": (1600, 3600, 2, 3, {"S": [(1, 1)]}, {"wall_mat": "Tile_Black", "floor_mat": "Tile_Black", "trim": "Neon_Magenta", "portal": "Neon_Magenta"}),
    "Storage":   (2400, 3600, 2, 3, {"S": [(1, 1)]}, {"wall_mat": "Wall_Concrete", "floor_mat": "Floor_Concrete", "trim": "Speaker_Black", "portal": "Speaker_Black"}),
}


def rebuild_shells(names=None):
    out = {}
    for name, (x0, y0, tw, td, doors, kw) in SHELLS.items():
        if names and name not in names:
            continue
        gone = 0
        for a in _EAS.get_all_level_actors():
            if str(a.get_folder_path()) == "Rooms/%s/Shell" % name:
                _EAS.destroy_actor(a)
                gone += 1
        room_shell(name, x0, y0, tw, td, doors=doors, **kw)
        out[name] = gone
    return out
